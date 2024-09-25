#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "player.h"

struct player user;
struct player computer;

int add_card(struct player* target, struct card* new_card){
	struct hand* temp;	//pointer to hand object we are adding
	
	temp = (struct hand*)malloc(sizeof(struct hand)); 	//we are creating a new object so we make space for it (type)malloc(size)
	if (temp == NULL) {return -1;}	//check he had, not used unless malloc fails
	
	temp->top=*new_card; 	//set the card within the temp hand to the new_card handed to the funtion
	temp->next=target->card_list;	//adding to front, card_list is current front, set temp next pointer to current front
	target->card_list=temp;		//set the front pointer to the address of the card.
	
	target->hand_size++;	//increment variable keeping track of hand size

	return 1;
}

int remove_card(struct player* target, struct card* old_card){ //slightly more complicated function needs address of card to remove

	struct hand* iterator = target->card_list;	//iterator to loop through hand, "(type) iterator = address of first item in list"
	struct hand* previous = NULL;	//Need to keep track of the address of the previous item to properaly link after removal

	if (iterator == NULL) {return 0;}	//Check he had if the list is empty because card_list isn't defined by default until we add_card
	
	while (&(iterator->top) != old_card){	//Loops until the address of the card we are at matches the address of the card handed into the function
		previous = iterator;	//Next item so address of current hand becomes address of previous
		iterator = iterator->next;	//Next item so incremenet iterator
		if (iterator == NULL) {return 0;}	//End of hand list points to NULL so return 0 if we didn't find the card address specified
	}
	
	//Getting here means weve broken out of while loop so our iterator points to the hand with the card with the address specified.
		
	if (previous != NULL){	//Corner case handled in else, otherwise...
		previous->next = iterator->next;	//Sets the previous pointer to the next pointer "going around" the hand pointed to by iterator.
	}
	else{ //Corner case when we are removing the first item in the list
		target->card_list = iterator->next;	//In this case we just want to change the front of the list to the next hand in the list.
		
	}
	target->hand_size--;	//decrement variable keeping track of hand size
	
	free(iterator); //Free memory used by removed item

	return 1; //Returns 1 if succesful
}

char check_add_book(struct player* target){	//recalculates the number of each rank every call for a given address of a player
	char ranks[13] = {'2','3','4','5','6','7','8','9','T','J','K','Q','A'};
	int count[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; //corrosponding counter for each rank
	struct hand* iterator = target->card_list;

	while (iterator != NULL){	//for each card
		for (int i=0; i<13; i++){	//for each rank
			if (iterator->top.rank == ranks[i]){	//if the card rank matches the current rank we are checking
				count[i]++;	//increment rank
				if (count[i]>3){	//after increment check if that rank counter has hit 4
					
					target->book[target->bookpoint] = ranks[i];	//add the rank to a slot in players book.
					target->bookpoint++; //increment bookpoint so next time we set the next book slot.
					
					struct hand* rcpointer = target->card_list; // We need to remove those cards make a temp pointer to loop through hands

					while (rcpointer != NULL){	//loop through again to find cards of specified rank to remove
						if (rcpointer->top.rank == ranks[i]){	//If the current hands cards rank matches the rank to remove
						
						    remove_card(target,&(rcpointer->top)); //remove card, remove needs addresses of player/card
						    
						    rcpointer = target->card_list; //because removing may screw up iteration we start the loop over
						}
						if (rcpointer->top.rank != ranks[i]){	//only do this when we don't start the loop over
							rcpointer = rcpointer->next;	//increment pointer for next hand item
						}
					}
					return ranks[i]; //after we find 4 of rank and then remove the cards of that rank return the rank
				}
			}
		}
		iterator = iterator->next; // increment pointer for next hand item check and add to a counter.
	}
    return 0;
}

int search(struct player* target, char rank){	//simple function to just retorn one if a specified rank is found within a players hand list

	struct hand* iterator = target->card_list;	//pointer to iterator through hand list
	
	while (iterator != NULL){	//Loop through hand list until address is NULL
	
		if (iterator->top.rank == rank) {return 1;}	//If we find the rank we are looking for return 1
		iterator = iterator->next;	//increment iterator
	}
	return 0; //Looped through but didn't find and return 1 so return 0
}

int transfer_cards(struct player* src, struct player* dest, char rank){		// Function to move cards between two players struchts of specified rank
	
	int numt = 0;	//Number of cards we transfer
	struct hand* iterator = src->card_list;	 // src is soure player, dest is destination player, so were are looping through source for cards to transfer
	
	while (iterator != NULL){	//Loop until end of list
	
		if (iterator->top.rank == rank){	//Similar process to what occures in check book to repeatidly transfer cards rather than remove them
		
			add_card(dest, &(iterator->top));	//Once ranks match we add that card to destination
			remove_card(src,&(iterator->top));	//Then remove that card from source

			iterator = src->card_list;	//Similar to check book we start the loop over because iterator gets messed up
			
			numt++;	  //Increment the number of cards weve transfered for return
		}
		
		if (iterator->top.rank != rank){	//Only if this isnt the rank we are looking for and DONT transfer
			iterator = iterator->next;	//Increment iterator
		}
	}
	return numt;	//Return number of cards transfered
}

// TO DO (read descriptions in header files)

int game_over(struct player* target){	// (*target).book or target->book to get book array, I added a bookpoint same function as top_card but for the book array
	int count = 0;
	for (int i=0; i<7; i++){
		if (target->book[i] != 0){
			count++;
		}
	} 
	if (count == 7){
		return 1;
	}
	return 0;
	
}

int reset_player(struct player* target){	// remove_card frees the memory of the card specified (or i think it does hard to test)
	//Loop through array of cards and remove_card
	//Reset book array
	struct hand* iterator = computer.card_list;
	while (iterator != NULL){
		remove_card(target, &iterator->top);
		iterator=iterator->next;
	}
	memset(target->book, '\0', 7);
}

char computer_play(struct player* target){	// Remember this is just one turn
	char ranks[13] = {'2','3','4','5','6','7','8','9','T','J','K','Q','A'};
	char randRank = "";
	while (search(target, randRank) != 1){
		randRank = ranks[rand()%14]; 
	}
	return randRank;
}

char user_play(struct player* target){		//Same deal but use scanf to get user input then search in computer
	char input;
	scanf(" %c",&input);
	while(search(target, input) != 1){
		printf("Error - must have at least one card from rank to play\n");
		printf("Player 1's turn, enter a Rank: ");
		scanf(" %c",&input);
		printf("\n");
		
	}
	return input;
}
