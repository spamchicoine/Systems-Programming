/* 
 * tsh - A tiny shell program with job control
 * 
 * Nathan Robinson
 * Sam Chicoine
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Sio Functions */
static size_t sio_strlen(char s[])
{
    int i = 0;

    while (s[i] != '\0')
        ++i;
    return i;
}
ssize_t sio_puts(char s[]) /* Put string */
{
    return write(STDOUT_FILENO, s, sio_strlen(s)); 
}
void sio_error(char s[]) /* Put error message and exit */
{
    sio_puts(s);
    _exit(1);
}
static void sio_reverse(char s[])
{
    int c, i, j;

    for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}
static void sio_ltoa(long v, char s[], int b) 
{
    int c, i = 0;
    int neg = v < 0;

    if (neg)
	v = -v;

    do {  
        s[i++] = ((c = (v % b)) < 10)  ?  c + '0' : c - 10 + 'a';
    } while ((v /= b) > 0);

    if (neg)
	s[i++] = '-';

    s[i] = '\0';
    sio_reverse(s);
}
ssize_t sio_putl(long v)
{
	char s[128];
    
	sio_ltoa(v, s, 10);
	return (sio_puts(s));
}

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
	    break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	    break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	    break;
	default:
            usage();
	}
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {

	/* Read command line */
	if (emit_prompt) {
	    printf("%s", prompt);
	    fflush(stdout);
	}
	if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin)) {
	    app_error("fgets error");
	}

	if (feof(stdin)) { /* End of file (ctrl-d) */
	    fflush(stdout);
	    exit(0);
	}

	/* Evaluate the command line */
//	printf("EVAL: %s\n", cmdline);
//	fflush(stdout);
	eval(cmdline);
	fflush(stdout);
	fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) 
{
	char *argv[MAXARGS];	// Declare array for arguements
	int bg;		// Determines if we run process in background if 1, 0 then foreground
	pid_t pid;	//Initialize pid
	sigset_t mask, prev_mask;	//Declare masks
	
	sigemptyset(&mask);		//Declare empty mask
 	sigaddset(&mask, SIGCHLD);	//Add SIGCHILD to mask
	
	bg = parseline(cmdline, argv);	//Parse command line to get arguments
	
	if (argv[0] == NULL) {return;}	//No arguements then return
	if(builtin_cmd(argv)){		//Call builtin_cmd to return true and run command if its built in
		return;
	}
	else{	
		sigprocmask(SIG_BLOCK, &mask, &prev_mask);	//Activate mask and save previous mask
			
		if((pid = fork()) == 0){	//Create child to run process
			sigprocmask(SIG_SETMASK, &prev_mask, NULL);	// Unblock SIGCHILD

			setpgid(0, 0);		//Change group pid

			if (execve(argv[0], argv, environ ) < 0) {		//Execute process
	 			printf("%s: Command not found.\n", argv[0]);
				exit(0);
	 		}
			return;
 		}
 		
 		// Parent
	 	if(!bg){	//Foreground job
			addjob(jobs, pid, FG, cmdline);	//Add job
			//printf("adding foreground -- %s\n",cmdline);
	 		sigprocmask(SIG_SETMASK, &prev_mask, NULL); //Unblock SIGCHILD
	 		waitfg(pid);	//Wait for foreground to terminate
	 	}
		else{
			addjob(jobs, pid, BG, cmdline);	//Add job
			//printf("adding background -- %s ",cmdline);
			printf("[%d] (%d) %s", pid2jid(pid), (int)pid, cmdline);

	 		sigprocmask(SIG_SETMASK, &prev_mask, NULL); //Unblock SIGCHILD
		}
	}
	return;
	
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv) 
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
	buf++;
	delim = strchr(buf, '\'');
    }
    else {
	delim = strchr(buf, ' ');
    }

    while (delim) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* ignore spaces */
	       buf++;

	if (*buf == '\'') {
	    buf++;
	    delim = strchr(buf, '\'');
	}
	else {
	    delim = strchr(buf, ' ');
	}
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* ignore blank line */
	return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0) {
	argv[--argc] = NULL;
    }
    return bg;
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) 
{
	if (strcmp(argv[0],"quit")==0){		//Exit shell
		exit(0);
	}
	
	else if (strcmp(argv[0],"bg")==0){	//Call do_bgfg
		do_bgfg(argv);
		return 1;
	}
	else if (strcmp(argv[0],"fg")==0){	//Call do_bgfg
		do_bgfg(argv);
		return 1;
	}
	else if (strcmp(argv[0],"jobs")==0){	//List jobs
		listjobs(jobs);
		return 1;
	}
  return 0;     /* not a builtin command */
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv)
{	
	struct job_t* job;	//Declare job to change
	if (argv[1] == NULL){
		if(strcmp(argv[0],"fg") == 0){
			printf("fg command requires PID or %%jobid argument\n");
		}else{
			printf("bg command requires PID or %%jobid argument\n");
		}
		return;
	}
	
	if (argv[1][0]=='%'){	//If JID format
			
		if (atoi(&argv[1][1]) == 0){		//atoi error returns 0, only works because we know JID/PID cannot be 0 they start at 1
			printf("%s: No such job\n",argv[1]);
			return;
		}
		
		if (getjobjid(jobs, atoi(&argv[1][1])) == NULL){	//getjobjid returns NULL if job of JID not found
			printf("%s: No such job\n",argv[1]);
			return;
		}
		
		job = getjobjid(jobs, atoi(&argv[1][1]));	//Get address of job using JID
	}
	else{	//If PID format
	
		if (atoi(argv[1]) == 0){	//atoi error
			if(strcmp(argv[0],"bg")==0){
				printf("bg: argument must be a PID or %%jobid\n");
			}else{
				printf("fg: argument must be a PID or %%jobid\n");
			}
			return;
		}
		
		if (getjobpid(jobs, (pid_t)atoi(argv[1])) == NULL){	//getjobpid returns NULL if job of PID not found
			printf("(%s): No such process\n",argv[1]);
			return;
		}
		
		job = getjobpid(jobs, (pid_t)atoi(argv[1]));	//Get address of job using PID
	}
	
	if (strcmp(argv[0], "bg") == 0){	//If bg command
		printf("[%d] (%d) %s",job->jid, job->pid, job->cmdline);
		if(job->state == ST){
			//printf("Sending SIGCONT -- argv[0] is %s -- incoming state is %d\n",argv[0],job->state);
			fflush(stdout);
			if (kill((job->pid), SIGCONT) < 0){	//Send signal to contiue process, check return for errors
				printf("kill error");
			}
		}
		job->state = BG;	//Change state of job at adresss to BG
	}
	else if (strcmp(argv[0], "fg") == 0){	//If fg command
		//printf("running fg command\n");
		fflush(stdout);
		if (job->state == ST){
			//printf("Sending SIGCONT -- argv[0] is %s -- incoming state is %d\n",argv[0],job->state);
			fflush(stdout);
			if (kill((job->pid), SIGCONT) < 0){	//Send signal to contiue process, check return for errors
				printf("kill error");
			}
		}
		//printf("changing job state\n");
		//fflush(stdout);
		job->state = FG;	//Change state of job at adresss to FG
		//printf("waiting\n");
		//fflush(stdout);
		waitfg(job->pid);	//Foreground job so wait for termination
		
	}
	return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
	//printf("before while loop");
	while(fgpid(jobs) != (pid_t)0){	//fgpid returns PID of fg job or 0 if there is no fg job, so until there is no fg job
		//printf("waiting");
		sleep(1);	//Sleep so its not checking as fast as it possibly can
	}
	//printf("outside while loop");
	return; //returns giving back the command line
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) 
{

	// Save old error, restore before exiting
	int olderrno = errno;
	// Temporarily block signals
	sigset_t mask, prev_mask;
	sigfillset(&mask);
	sigprocmask(SIG_SETMASK, &mask, &prev_mask);
	pid_t pid;
	int child_state;
	while ((pid = waitpid(-1, &child_state, WNOHANG || WUNTRACED)) > 0){
		if(WIFEXITED(child_state)){
			deletejob(jobs, pid);
		}
		// Job [jid] (pid) stopped by signal **
		if(WIFSTOPPED(child_state)){
			sio_puts("In child handler -- ");
			sio_puts("Job [");
			sio_putl(pid2jid(pid));
			sio_puts("] (");
			sio_putl((int)pid);
			sio_puts(") stopped by signal ");
			sio_putl(WSTOPSIG(child_state));
			sio_puts("\n");
			struct job_t* job = getjobpid(jobs, pid);
			job->state=ST;
		}
		// Job [jid] (pid) terminated by signal **
		if (WIFSIGNALED(child_state)){
			 sio_puts("Job [");
			 sio_putl(pid2jid(pid));
			 sio_puts("] (");
			 sio_putl((int)pid);
			 sio_puts(") terminated by signal ");
			 sio_putl(WTERMSIG(child_state));
			 sio_puts("\n");
			 deletejob(jobs, pid);
		}
	}
	// Reset error
	errno = olderrno;

	// unblock signals
	sigprocmask(SIG_SETMASK, &prev_mask, NULL);
	return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
	int olderrno = errno;
	// Temporarily block signals
	sigset_t mask, prev_mask;
	sigfillset(&mask);
	sigprocmask(SIG_SETMASK, &mask, &prev_mask);

	pid_t PID = fgpid(jobs);
	if(PID != 0){ 
		kill(-PID, SIGINT);
	}
	// Reset error
	errno = olderrno;

	// unblock signals
	sigprocmask(SIG_SETMASK, &prev_mask, NULL);
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
	printf("Running handler\n");
	fflush(stdout);
	int olderrno = errno;
	// Temporarily block signals
	sigset_t mask, prev_mask;
	sigfillset(&mask);
	sigprocmask(SIG_SETMASK, &mask, &prev_mask);

	pid_t PID = fgpid(jobs);
	if(PID != 0){ 
		struct job_t* job = getjobpid(jobs, PID);
		job->state=ST;
		//sio_puts("Sending SIGTSTP\n");
		//fflush(stdout);
		kill(-PID, SIGTSTP);
		sio_puts("Job [");
		sio_putl(pid2jid(PID));
		sio_puts("] (");
		sio_putl((int)PID);
		sio_puts(") stopped by signal ");
		sio_putl(sig);
		sio_puts("\n");
		fflush(stdout);
	}
	// Reset error
	errno = olderrno;

	// unblock signals
	sigprocmask(SIG_SETMASK, &prev_mask, NULL);
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid > max)
	    max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
{
    int i;
    
    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == 0) {
	    jobs[i].pid = pid;
	    jobs[i].state = state;
	    jobs[i].jid = nextjid++;
	    if (nextjid > MAXJOBS)
		nextjid = 1;
	    strcpy(jobs[i].cmdline, cmdline);
  	    if(verbose){
	        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
	}
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == pid) {
	    clearjob(&jobs[i]);
	    nextjid = maxjid(jobs)+1;
	    return 1;
	}
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;
    for (i = 0; i < MAXJOBS; i++){
			if (jobs[i].state == FG){
				return jobs[i].pid;
			}
		}
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid)
	    return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
    int i;

    if (jid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid == jid)
	    return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) 
{
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid != 0) {
	    printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
	    switch (jobs[i].state) {
		case BG: 
		    printf("Running ");
		    break;
		case FG: 
		    printf("Foreground ");
		    break;
		case ST: 
		    printf("Stopped ");
		    break;
	    default:
		    printf("listjobs: Internal error: job[%d].state=%d ", 
			   i, jobs[i].state);
	    }
	    printf("%s", jobs[i].cmdline);
	}
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}
