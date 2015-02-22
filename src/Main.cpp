#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "Common.h"
#include "Bitboards.h"
#include "Board.h"
#include "MoveGen.h"
#include "Search.h"
#include "Evaluation.h"
#include "Pawns.h"
#include "Threads.h"
#include "UCI.h"
#include "Endgame.h"
#include "ICS.h"

int main(int argc, char** argv){
	// Initialize Everything //
	Bitboards::init();
	Board::init();
	Moves::init();
	Eval::init();
	Pawns::init();
	Search::init();
	Threads.init();
	UCI::init();
	EndgameN::init();
	// 
	ICS_Settings s;
	s.allow_unrated = true;
	s.allow_rated = false;
	FICS ics(s);
	if(ics.try_login("firebolting", "alvqqn")){
		printf("failed login\n");
		return 1;
	} else {
		printf("logged in!\n");
	}
	printf("listening-\n");
	auto res = ics.listen(500);
	printf("done listening.\n");
	// And start the UCI Loop //
	UCI::loop(argc, argv);
	return 0;
}











































