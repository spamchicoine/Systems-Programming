#ifndef GOFISH_H
#define GOFISH_H

#include "deck.h"
#include "player.h"

/*
   Define any prototype functions
   for gofish.h here.
*/

void card_matches_p1(char rank);

void card_matches_p2(char rank);

void print_matching_rank(struct player* target, char rank);

#endif
