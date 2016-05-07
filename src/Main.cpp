#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <unistd.h>
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
#include "PGN.h"
#include "Book.h"
#include <sstream>
#include <fstream>

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

void Warn(std::string of){
	std::cerr << BOLDYELLOW << "Warning: " << RESET << of << std::endl;
}

void Error(std::string msg){
	std::cerr << BOLDRED << "Error: " << RESET << msg << std::endl;
	::exit(1);
}

std::string ReadEntireFile(std::ifstream& ifp){
	std::string ret;
	ifp.seekg(0, std::ios::end);
	ret.reserve(ifp.tellg());
	ifp.seekg(0, std::ios::beg);
	ret.assign((std::istreambuf_iterator<char>(ifp)), std::istreambuf_iterator<char>());
	return ret;
}

void InitCrit(void){
	Bitboards::init();
	Board::init();
	Moves::init();
	Eval::init();
	Pawns::init();
	Search::init();
	Threads.init();
	EndgameN::init();
	// Not critical, per se, but useful.
	PGN::init();
	Annotate::init();
}

/*__attribute__((weak))*/ int main(int argc, char** argv){
	// Initialize Everything //
	InitCrit(); // critical sections first
	UCI::init(); // this only kibitzes
	// Note: Book::init() is called on a case-by-case basis (e.g. used for UCI use, in ICS, but not for annotations)
	// Command Line Arguments //
	CommandLineArgs args(argc, argv);
	if(args.contains("--help") || args.contains("-h")){
		puts("\t-ics\t\tLaunch the ICS client");
		puts("\t-icsunrated/-icsrated\t\tSet ICS rated option");
		puts("\t-icstime\t\tSet how long to listen for a game");
		puts("\t-annotate FNAME\tAnnotate the given PGN game file (use -out ONAME to specify output file, -anntime To specify time per move for annotation)");
		puts("\t-read FNAME\tRead the specified PGN game file (use -create ONAME to create a book from the file)");
		puts("\t-readbook FNAME\tRead the specified book file and launch an interactive console");
	} else if(args.contains("-ics")){
		Book::init();
		// ICS (if/a) //
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
		ics.listen(sec);
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
	} else if(args.contains("-annotate")){
		std::string inf = args.value("-annotate");
		if(!inf.length()){
			Error("Option '-annotate' requires an input filename.");
		} else {
			std::string outf = "out.pgn";
			if(!args.contains("-out")){
				Warn("No output file given, assuming '" + outf + "'.");
			} else {
				outf = args.value("-out");
			}
			Annotator_Options ap;
			ap.time_per = 1000; // 1 second per move by default
			if(args.contains("-anntime")){
				ap.time_per = atoi(args.value("-anntime").c_str());
			}
			Annotate::annotate_file(inf, outf, ap);
		}
	} else if(args.contains("-read")){
		std::string inf = args.value("-read");
		if(!inf.length()){
			Error("Option '-read' requires an input filename.");
		} else {
			std::ifstream ifp(inf);
			if(!ifp.is_open()){
				Error("Could not open input file '" + inf + "' for reading.");
			}
			std::string data = ReadEntireFile(ifp);
			ifp.close();
			PGN_Reader reader;
			reader.init(data);
			reader.read_all();
			bool write_out = (args.contains("-out") && args.value("-out").length());
			std::ofstream ofp;
			const std::string outf = (write_out ? args.value("-out") : "");
			if(write_out){
				ofp.open(outf);
				if(!ofp.is_open()){
					Error("Could not open output file '" + outf + "' for writing.");
				}
			}
			bool create_book = (args.contains("-create") && args.value("-create").length());
			if(create_book){
				const std::string nam = args.value("-create");
				std::ofstream bfp(nam);
				if(!bfp.is_open()){
					Error("Could not open book file '" + nam + "' for writing.");
				}
				std::cout << "Press [enter] to create the book.\n";
				getchar();
				Book book("");
				for(unsigned int i = 0, e = reader.games_num(); i < e; i++){
					PGN_Game on = reader.get_game(i);
					printf("\r%c[0K\r", char(0x1B)); // ANSI escape sequence Esc[0K to clear the line from cursor onwards
					printf("%u/%u - %.2f%% ", (i + 1), e, (double(i + 1) / e) * 100.0);
					std::cout.flush();
					book << on;
				}
				printf("\n");
				bfp << book.get_book();
				bfp.close();
			}
			std::cout << "Press [enter] to view formatted PGN output for all games read.\n";
			if(write_out) std::cout << "(Writing PGN output to file '" + outf + "').\n";
			getchar();
			PGN_Writer writer;
			std::stringstream ss;
			for(unsigned int i = 0, e = reader.games_num(); i < e; i++){
				PGN_Game on = reader.get_game(i);
				writer.clear();
				writer.init(on);
				ss << "\n" + writer.formatted() + "\n";
			}
			std::cout << ss.str();
			if(write_out){
				ofp << ss.str();
				ofp.close();
			}
		}
	} else if(args.contains("-readbook")){
		const std::string val = args.value("-readbook");
		std::ifstream ifp(val);
		if(!ifp.is_open()){
			Error("Could not open input book '" + val + "' for reading.");
		}
		std::cout << "Reading book...\n";
		std::string data = ReadEntireFile(ifp);
		std::cout << "Read book!\n";
		Book book(data);
		ifp.close();
		std::cout << "Book contains " << book.get_book().size() / sizeof(Book_Position) << " positions.\n";
		std::string str;
		Board pos;
		pos.init_from(StartFEN);
		std::cout << "Type 'exit' or 'quit' to quit." << std::endl;
		std::cout << "You can type:\n";
		std::cout << "1. 'info' to see all moves for the current position.\n";
		std::cout << "2. 'fen' followed by an FEN position (or just 'fen' to see the current FEN).\n";
		std::cout << "3. 'num' to see the number of positions in the book.\n";
		std::cout << "4. 'moves' followed by a series of moves from the last given FEN position.\n";
		std::cout << "5. 'disp' to see the board position.\n";
		std::cout << "Book functions (all will be prefixed by 'book' - e.g. 'book variance 0')\n";
		std::cout << "6. [variance/forgiveness] (optional: new value from 0 - 100) - if no new value is given, it displays the requested value.\n";
		std::cout << "(more to come)\n";
		Book_Skill skill;
		skill.variance = skill.forgiveness = 0;
		while(true){
			printf("> ");
			std::getline(std::cin, str);
			if(str == "exit" || str == "quit") break;
			if(str == "num"){
				printf("There are %lu positions in this book.\n", book.get_book().size() / sizeof(Book_Position));
			} else if(str.find("moves") == 0){
				std::istringstream ss(str);
				ss >> std::skipws;
				ss >> str;
				while(ss >> str){
					Move move = Moves::parse<false>(str, pos);
					if(!MoveList<LEGAL>(pos).contains(move)){
						Warn("Invalid move: " + str);
					} else {
						BoardState st;
						pos.do_move(move, st);
						pos.init_from(pos.fen());
					}
				}
			} else if(str.find("fen") == 0){
				if(str.length() < 5){
					printf("FEN: %s\n", pos.fen().c_str());
				} else {
					pos.init_from(str.substr(4));
				}
			} else if(str == "info"){
				auto res = book.results_for(pos);
				book.sort_results_by(res, skill);
				for(Book_Move& on : res){
					std::cout << "Move: " << Moves::format<false>(on.move) << " (" << Moves::format<true>(on.move, pos) << ")" << std::endl;
					printf("Count: %u\n", on.bpos.get_num());
					printf("Learn: %f\n\n", on.bpos.get_learn());
				}
			} else if(str == "disp"){
				std::cout << pos;
			} else if(str.find("book") == 0){
				std::istringstream ss(str);
				ss >> std::skipws;
				ss >> str; // "book"
				ss >> str;
				if(str == "variance" || str == "forgiveness"){
					int n;
					if(!(ss >> n)){
						if(str == "variance") printf("Variance: %d\n", skill.variance);
						else printf("Forgiveness: %d\n", skill.forgiveness);
					} else {
						if((n < 0) || (n > 100)){
							Warn("Invalid value for book parameter - must be in range of 0 to 100.");
						} else {
							if(str == "variance") skill.variance = n;
							else skill.forgiveness = n;
						}
					}
				}
			}
		}
	} else {
		Book::init();
		// Start the UCI Loop //
		UCI::loop(argc, argv);
	}
	return 0;
}











































