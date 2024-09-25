#include "deck.h"
#include <stdio.h>
#include <stdlib.h>

struct deck deck_instance; 

int shuffle(){ 

	srand(time(NULL));
	deck_instance.top_card = 0; //Set top_card which serves as where we are in the deck rather than add/remove elements we can just ignore them

	char ranks[13] = {'2','3','4','5','6','7','8','9','T','J','K','Q','A'}; //Array of ranks
	char suits[4] = {'S','C','H','D'}; //Array of suits

	int count=0; //Number to go from 0 to 51 because our for loops run 52 times but i and j only go from 0-12 and 0-3;

	for (int i=0; i<13; i++){ //for each rank
		for (int j=0; j<4; j++){ //for each suit
			struct card temp; //make a temp card
			temp.suit = suits[j]; //set the cards suit
			temp.rank = ranks[i]; //set the cards rank
			deck_instance.list[count] = temp; //set the empty card struct within the card list to the temp card;
			count++; //increment count to go to index the next empty card struct
		}
	}
	for (int i=0; i<52; i++){

		int shift = rand()%51;
		struct card temp = deck_instance.list[i];
		deck_instance.list[i] = deck_instance.list[shift];
		deck_instance.list[shift] = temp;

	}
	return 1; //once finished return 1
}

int deal_player_cards(struct player* target){	//Deals cards from the deck uses top_card to represent where we are in the deck
    for (int i=0; i<7; i++){ //Starting hand is 7 cards
        add_card(target,&(deck_instance.list[deck_instance.top_card]));	  // add the card at list[top_card] to the target specified
        deck_instance.top_card++;	//increment top_card so we can add the next card in the deck, we use this instead of i because remember top_card represents where we are in the deck for the whole game, so when we want to draw cards we need to increment this to essentially ignore the cards before this index.
    }
}

// TO DO

struct card* next_card( ){ // I guess this is a function? read description, remember for the deck we arent actually removing we just adjust deck_instance.top_card
	struct card* temp = &deck_instance.list[deck_instance.top_card];
	deck_instance.top_card++;
	return temp;
}

size_t deck_size( ){	//havent tested
	return (51 - deck_instance.top_card); //top_card represent how far in we are in the deck so..
}
