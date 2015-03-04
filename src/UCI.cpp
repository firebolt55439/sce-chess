#include "Common.h"
#include "Bitboards.h"
#include "Board.h"
#include "MoveGen.h"
#include "Evaluation.h"
#include "Search.h"
#include "Threads.h"
#include "UCI.h"
#include <fstream>
#include <ostream>

namespace {
	Board MainBoard;
	Search::BoardStateStack BSS;
}

std::string ENGINE_VERSION = "v0.1";

void UCI::init(void){
	// TODO: Initialize options, etc.
	std::cout << "Sumer's Chess Engine, " << ENGINE_VERSION << " by Sumer Kohli" << std::endl;
}

std::string UCI::value(Value v){
	std::stringstream ss;
	if(abs(v) < VAL_MATE_IN_MAX_PLY){
		ss << "cp " << (v * 100 / PawnValueEg);
	} else {
		ss << "mate " << ((v > 0 ? (VAL_MATE - v + 1) : (-VAL_MATE -v)) / 2); // since getting mated means negative mate score (e.g. "mate -2" means "mated in 2")
	}
	return ss.str();
}

std::string UCI::move(Move m){
	// This yields the move's algebraic *coordinate* notation. //
	// TODO: Implement Moves::format<true> for SAN
	return Moves::format<false>(m);
}

void handle_go(std::istringstream& ss){
	std::string tok;
	Search::SearchLimits limits;
	std::memset(&limits, 0, sizeof(limits));
	while(ss >> tok){
		if(tok == "ponder"){
			// OK, start searching in "pondering" mode, and if we get a "ponderhit", it means
			// we can just switch the ponder flag to off and the search will run like a normal
			// search.
			limits.ponder = true;
		} else if(tok == "wtime") ss >> limits.time[WHITE];
		else if(tok == "btime") ss >> limits.time[BLACK];
		else if(tok == "winc") ss >> limits.inc[WHITE];
		else if(tok == "binc") ss >> limits.inc[BLACK];
		else if(tok == "movestogo") ss >> limits.movestogo;
		else if(tok == "depth") ss >> limits.depth;
		else if(tok == "nodes") ss >> limits.nodes; // TODO: Finish implementing this, plus NPS stuff
		else if(tok == "mate") ss >> limits.mate;
		else if(tok == "movetime") ss >> limits.movetime;
		else if(tok == "infinite") limits.infinite = true;
		else if(tok == "searchmoves"){
			while(ss >> tok){
				Move m = Moves::parse<false>(tok, MainBoard);
				if(m != MOVE_NONE) limits.SearchMoves.push_back(m);
			}
		}
	}
	Threads.start_searching(MainBoard, limits, BSS);
}

void handle_position(std::istringstream& ss){
	// "position [fen  | startpos ]  moves  ... "
	// Note: 'ss' should have already consumed "position".
	std::string tok, fen = "";
	ss >> tok;
	// position startpos moves d2d4 d7d5 g1f3 b8c6
	if(tok == "fen"){
		while(ss >> tok && (tok != "moves")){
			fen += tok + " ";
		}
	} else if(tok == "startpos"){
		fen = StartFEN;
		ss >> tok; // consume "moves"
	} else {
		return; // invalid token
	}
	MainBoard.init_from(fen);
	Move m;
	BSS = Search::BoardStateStack(new std::stack<BoardState>());
	while(ss >> tok && ((m = Moves::parse<false>(tok, MainBoard)) != MOVE_NONE)){
		BSS->push(BoardState());
		MainBoard.do_move(m, BSS->top());
	}
}

void UCI::loop(int argc, char** argv){
	std::string inp = "", tok;
	MainBoard.init_from(StartFEN);
	while(tok != "quit"){
		inp.clear(); // clear flags
		tok.clear();
		std::getline(std::cin, inp);
		std::istringstream ss(inp);
		ss >> std::skipws >> tok;
		if(tok == "uci"){
			std::cout << "id name SCE 0.1" << std::endl;
			std::cout << "id author Sumer Kohli" << std::endl;
			// TODO: Send more options to GUI for customizing and handle 'setoption'
			std::cout << std::endl;
			std::cout << "option name Ponder type check default true" << std::endl; // declare our ability to ponder for polyglot
			std::cout << "uciok" << std::endl;
		} else if(tok == "isready"){
			std::cout << "readyok" << std::endl;
		} else if(tok == "ucinewgame"){
			// TODO: Clear TT, etc.
			MainBoard.init_from(StartFEN);
			BSS.release(); // release ownership and free memory
		} else if(tok == "position"){
			handle_position(ss);
		} else if(tok == "disp"){
			std::cerr << MainBoard << std::endl;
		} else if(tok == "go"){
			handle_go(ss);
		} else if((tok == "quit") || (tok == "stop") || (tok == "ponderhit" && Search::Signals.stop_on_ponder_hit)){
			Search::Signals.stop = true;
			Threads.main_thread->notify_one(); // wake it up just in case
		} else if(tok == "ponderhit"){
			Search::Limits.ponder = false; // alright, we got a free headstart on the search
		}
	}
}
























































