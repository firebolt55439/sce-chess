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
#include "Annotate.h"

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
	Annotate::init();
	// ICS (if/a) //
	if(argc > 1 && std::string(argv[1]) == "-ics"){
		ICS_Settings s;
		s.allow_unrated = false;
		s.allow_rated = true;
		s.allowed_types.push_back("blitz");
		FICS ics(s);
		if(ics.try_login("firebolting", "alvqqn")){
			printf("Login failed.\n");
			return 1;
		} else {
			printf("Logged in!\n");
		}
		printf("Listening-\n");
		auto res = ics.listen(6000);
		/*
				  rating     RD      win    loss    draw   total   best
		Blitz      1519     97.0      37      16       0      53   1519 (22-Oct-2014)
		Standard   1722    204.9       2       2       0       4
		Lightning  1247    265.0       0       2       0       2
		*/
		/*
					  rating     RD      win    loss    draw   total   best
		Blitz      1565     66.1      53      21       1      75   1573 (25-Feb-2015)
		Standard   1722    205.0       2       2       0       4
		Lightning  1247    265.1       0       2       0       2
		*/
		/*
				  rating     RD      win    loss    draw   total   best
		Blitz      1752     47.6      91      32       2     125   1752 (27-Feb-2015)
		Standard   1722    205.4       2       2       0       4
		Lightning  1247    265.4       0       2       0       2
		*/
		printf("Done listening.\n");
	}
	// And start the UCI Loop //
	UCI::loop(argc, argv);
	return 0;
}











































