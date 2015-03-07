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

struct CommandLineArgs {
	std::vector<std::string> args;
	
	CommandLineArgs(int argc, char** argv){
		for(int i = 1; i < argc; i++){
			args.push_back(std::string(argv[i]));
		}
	}
	
	~CommandLineArgs(void){ }
	
	bool contains(std::string s){
		for(const std::string& on : args){
			if(s == on) return true;
		}
		return false;
	}
	
	std::string value(std::string s){
		for(unsigned int i = 0; i < args.size(); i++){
			const std::string& on = args[i];
			if(s == on){
				if((i + 1) < args.size()) return args[i + 1];
			}
		}
		return "";
	}
};

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
	// Command Line Arguments //
	CommandLineArgs args(argc, argv);
	// ICS (if/a) //
	if(args.contains("-ics")){
		ICS_Settings s;
		s.allow_unrated = (args.contains("-icsunrated"));
		s.allow_rated = (args.contains("-icsrated"));
		s.allowed_types.push_back("blitz"); // TODO: Add game types from command line
		FICS ics(s);
		if(ics.try_login("firebolting", "alvqqn")){
			printf("Login failed.\n");
			return 1;
		} else {
			printf("Logged in!\n");
		}
		int sec = 6000;
		if(args.contains("-icstime")){
			sec = atoi(args.value("-icstime").c_str());
		}
		printf("Listening for %d seconds-\n", sec);
		auto res = ics.listen(sec);
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











































