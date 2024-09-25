#include <stdio.h>

#include "gofish.h"


//Remember deck_istance, user, computer declared in header/other c files so they are global variables.
//Should not need to adjust #includes
//I do not believe we will need to make anything in gofish.h atm its just linking stuff

//Remember to make, then ./gofish make will often tell you where errors are I didnt really find a need to the debugger app but you can run gdb ./gofish after make to enter it.

//All functions not in TO DO should be working as intended so notify me if you think something is wrong.

//Hand is the linked list each hand contains ones card called top each player has a pointer card_list to the first hand struct.

//#1 ISSUE !!! keeping track of what is an address and what a value 99% of problems are because of & -> and .

//&(item) - gets address of item useful when handing something to a function

//uno->dos - same as (*uno).dos  so treats uno as an address gets value at that address and then a value dos within that
//(dos could be address or object this affects what notation comes after, another -> or a .)

//. - similar to python object.attribute but wont work if its a pointer.attribute need the object at pointer.

//I removed most my code in main as it was just testing functions, main is just going to handle looping the gameplay and turns.

int turn = 0; //Boolean to track who's trun it is, 0 for user, 1 for computer
int p1_book_count = 0;
int p2_book_count = 0;

int main(int args, char* argv[]) {
	char new_rank;
	char play_again = "";
	
	shuffle();
	
	deal_player_cards(&user);
	deal_player_cards(&computer);
	//Start game
	while(1){
		// Print all starting information
		printf("\n");
		printf("Player 1's Hand - ");
		struct hand* iterator = user.card_list;
		while (iterator != NULL){
			printf("%c%c ",iterator->top.rank,iterator->top.suit);
			iterator=iterator->next;
		}
		printf("\n");
		printf("Player 1's Book - ");
		for (int i=0; i<7; i++){
			printf("%c ",user.book[i]);
		}
		printf("\n");
		
		printf("Player 2's Book - ");
		for (int i=0; i<7; i++){
			printf("%c ",computer.book[i]);
		}
		printf("\n");
	
		//Gameplay
		if (turn == 0){	//Player 1 turn
			printf("Player 1's Turn, enter a Rank: ");
			char input = user_play(&user);
			if (search(&computer, input) != 1){	//computer doesn't have card (go fish)
				printf("\t- Player 2 has no %c's\n",input);
				//Draw card
				struct card* new_card = next_card();
				new_rank = new_card->rank;
				add_card(&user, new_card);
				printf("\t- Go Fish, Player 1 draws %c%c\n",new_rank,new_card->suit);
				
				//Check if new card completes book
				if (check_add_book(&user) != 0){
					printf("\t- Player 1 books %c\n\t- Player 1 gets another turn\n",new_rank);
					p1_book_count++;
					turn = 0;
				}
				//Check if computer has rank of card that was picked up
				else if (search(&computer, new_rank) == 1){ 
					card_matches_p1(new_rank);
				}
				//Card doesn't match computers hand, end turn	
				else{ 
					printf("\t- Player 2's turn\n");
					turn = 1; //set turn indicator to player 2
				}
				
			}else{	//Computer has the card (don't draw)
				card_matches_p1(input);
			}
		}
		
		//Player 2 turn
		else if(turn == 1){
			char input_2 = computer_play(&computer);
			printf("Player 2's Turn, enter a Rank: %c\n",input_2);
			if (search(&user, input_2) != 1){	//user doesn't have card (go fish)
				printf("\t- Player 1 has no %c's\n",input_2);
				//Draw card
				struct card* new_card_2 = next_card();
				new_rank = new_card_2->rank;
				add_card(&computer, new_card_2);
				printf("\t- Go Fish, Player 2 draws a card\n");
				
				//Check if new card completes book
				if (check_add_book(&user) != 0){
					printf("\t- Player 2 books %c\n\t- Player 2 gets another turn\n",new_rank);
					p2_book_count++;
					turn = 1;
				}
				//check if user has rank of card that was picked up
				else if (search(&user, new_rank) == 1){ 
					card_matches_p2(new_rank);
				}
				//Card doesn't match users hand, end turn	
				else{ 
					printf("\t- Player 1's turn");
					turn = 0; //set turn indicator to player 1
				}
				
			}else{	//User has the card (don't draw)
				card_matches_p2(input_2);
			}
		}
		//Check if game is over
		if (game_over(&user) == 1){
			printf("Player 1 Wins! %i-%i\nDo you want to play again [Y/N]: ",p1_book_count,p2_book_count);
			scanf(" %c",&play_again);
			while (play_again != "Y" || play_again != "N"){
				printf("Please only enter Y or N");
				scanf(" %c",&play_again);
			}
		}
		if (game_over(&computer) == 1){
			printf("Player 2 Wins! %i-%i\nDo you want to play again [Y/N]: ",p2_book_count,p1_book_count);
			scanf(" %c",&play_again);
			while (play_again != "Y" || play_again != "N"){
				printf("Please only enter Y or N");
				scanf(" %c",&play_again);
			}
		}
		
		if (play_again == "Y"){
			reset_player(&user);
			reset_player(&computer);
			
			shuffle();
			deal_player_cards(&user);
			deal_player_cards(&computer);
		}else if(play_again == "N"){
			break;
		}
		
	}
	
}


void card_matches_p1(char rank){
	//print player 2's cards, then player 1's
	//transfer cards from comp to user
	//check books for user
	//Set turn indicator
	printf("\t- Player 2 has ");
	print_matching_rank(&computer, rank);
	
	printf("\t- Player 1 has ");
	print_matching_rank(&user, rank);
	
	transfer_cards(&computer, &user, rank);
	if (check_add_book(&user) != 0){
		printf("\t- Player 1 books %c\n\t- Player 1 gets another turn\n",rank);
		p1_book_count++;
	}else{
		printf("\t- Player 2's turn\n");
		turn = 1;
	}
	
}

void card_matches_p2(char rank){
	//print player 1's cards, then player 2's
	//transfer cards from user to comp
	//check books for comp
	//Set turn indicator
	printf("\t- Player 1 has ");
	print_matching_rank(&user, rank);
	
	printf("\t- Player 2 has ");
	print_matching_rank(&computer, rank);
	
	transfer_cards(&user, &computer, rank);
	if (check_add_book(&computer) != 0){
		printf("\t- Player 2 books %c\n\t- Player 2 gets another turn\n",rank);
		p2_book_count++;
	}else{
		printf("\t- Player 1's turn\n");
		turn = 0;
	}
	
}

void print_matching_rank(struct player* target, char rank){
	struct hand* iterator = target->card_list;
	while (iterator != NULL){
		if (iterator->top.rank == rank){
			printf("%c%c ",iterator->top.rank,iterator->top.suit);
		}
		iterator=iterator->next;
	}
	printf("\n");
}
