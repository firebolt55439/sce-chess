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

int main(int argc, char** argv){
	Bitboards::init();
	Board::init();
	Moves::init();
	Eval::init();
	Pawns::init();
	Search::init();
	Threads.init();
	Board pos;
	printf("Init'ing...\n");
	pos.init_from("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	Side us = BLACK;
	std::vector<Move> move_stack;
	while(true){
		std::cout << pos;
		MoveList<LEGAL> it(pos);
		if(!it.size()){
			if(pos.checkers()){
				if(pos.side_to_move() == us) std::cout << BOLDCYAN << "You win!";
				else std::cout << BOLDRED << "You lose!";
				std::cout << RESET << "\n";
			} else {
				std::cout << BOLDCYAN << "Draw by stalemate!\n" << RESET;
			}
			break;
		}
		if(pos.side_to_move() == us){
			// Our turn //
			Search::SearchLimits limits;
			std::memset(&limits, 0, sizeof(limits));
			limits.movetime = 2000; // in msec
			Search::BoardStateStack board_stack_ptr(Search::BoardStateStack(new std::stack<BoardState>()));
			Board tmp;
			tmp.init_from("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
			for(const Move& m : move_stack){
				board_stack_ptr->push(BoardState());
				printf("DM...\n");
				std::cout << UCI::move(m) << std::endl;
				tmp.do_move(m, board_stack_ptr->top());
				printf("DN!\n");
			}
			Threads.start_searching(tmp, limits, board_stack_ptr); // this tells the main thread to search
			sleep(10); // TODO: Fix, less hacky
			Move m = Search::RootMoves[0].pv[0];
			if(m != MOVE_NONE){
				assert(pos.legal(m, pos.pinned(us)));
				std::cout << "Computer move: " << Moves::format<false>(m) << std::endl;
				BoardState* st = new BoardState();
				pos.do_move(m, *st);
				move_stack.push_back(m);
			} else {
				std::cout << BOLDRED << "Computer resigns.\n" << RESET;
				break;
			}
		} else {
			// TODO: Test ponder here
			// Their turn //
			Move theirs = MOVE_NONE;
			while(true){
				std::cout << "Please enter your move: ";
				std::string inp;
				std::getline(std::cin, inp);
				Move m = Moves::parse_coord(inp, pos);
				if(m != MOVE_NONE){
					theirs = m;
					break;
				} else {
					std::cerr << REDCOLOR << "Invalid move entered! Please try again.\n\n" << RESET;
				}
			}
			BoardState* st = new BoardState();
			pos.do_move(theirs, *st);
			move_stack.push_back(theirs);
		}
	}
	return 0;
}












































