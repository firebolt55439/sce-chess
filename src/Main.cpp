#include <iostream>
#include <cstdio>
#include "Common.h"
#include "Bitboards.h"
#include "Board.h"
#include "MoveGen.h"

template<bool Root>
uint64_t perft(Board& pos, Depth depth){
	assert(depth >= ONE_PLY);
	// depth = depth left
	BoardState st;
	uint64_t nodes = 0, tmp;
	const bool leaf = (depth == (2 * ONE_PLY)); // at that point, just use MoveList<LEGAL>.size()
	for(MoveList<LEGAL> it(pos); *it; it++){
		if(depth != ONE_PLY){
			pos.do_move(*it, st);
			tmp = (leaf ? MoveList<LEGAL>(pos).size() : perft<false>(pos, depth - ONE_PLY));
			if(Root){
				std::cout << Moves::format<false>(*it) << ": " << tmp << std::endl;
			}
			nodes += tmp;
			pos.undo_move(*it);
		} else {
			++nodes;
		}
	}
	return nodes;
}

int main(int argc, char** argv){
	Bitboards::init();
	Board::init();
	Moves::init();
	Board pos;
	printf("Init'ing...\n");
	//pos.init_from("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	//pos.init_from("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -");
	//pos.init_from("rnbqkb1r/pp1p1ppp/2p5/4P3/2B5/8/PPP1NnPP/RNBQK2R w KQkq - 0 6");
	pos.init_from("r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1");
	printf("Init'ed!\n");
	std::cout << pos;
	MoveList<LEGAL> it(pos);
	printf("Generated %zu moves.\n", it.size());
	if(it.size()){
		BoardState st;
		pos.do_move(*it, st);
		std::cout << pos;
		pos.undo_move(*it);
		std::cout << pos;
	}
	for(; *it; it++){
		printf("Found move: %s\n", Moves::format<false>(*it).c_str());
	}
	getchar();
	//pos.init_from("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	//pos.init_from("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
	//pos.init_from("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
	//pos.init_from("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
	//pos.init_from("rnbqkb1r/pp1p1ppp/2p5/4P3/2B5/8/PPP1NnPP/RNBQK2R w KQkq - 0 6");
	pos.init_from("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10");
	for(int i = 1; i <= 7; i++){
		printf("Perft(%d) = %llu\n", i, perft<true>(pos, (ONE_PLY * i)));
		std::cout << pos;
	}
	getchar();
	//pos.init_from("8/2p5/3p4/KP5r/5p1k/8/4P1P1/1R6 b - - 0 1");
	//pos.init_from("8/2p5/3p4/KP1r4/5p1k/8/4P1P1/1R6 w - - 0 1");
	//pos.init_from("8/2p5/3p4/1P1r4/1K3p1k/8/4P1P1/1R6 b - - 0 1");
	pos.init_from("8/8/3p4/1Ppr4/1K3p1k/8/4P1P1/1R6 w - - 0 1");
	for(int i = 1; i <= 1; i++){
		printf("Perft(%d) = %llu\n", i, perft<true>(pos, (ONE_PLY * i)));
		std::cout << pos;
	}
	for(MoveList<LEGAL> it(pos); *it; it++){
		std::cout << "Found move " << Moves::format<false>(*it) << std::endl;
	}
	getchar();
	/*
	pos.init_from("r3k2r/p1ppqpb1/bn2pnp1/3PN3/Pp2P3/2N2Q1p/1PPBBPPP/R3K2R b KQkq a3 0 1");
	std::cout << pos;
	for(MoveList<LEGAL> it(pos); *it; it++){
		std::cout << "Found move " << Moves::format<false>(*it) << std::endl;
		if((from_sq(*it) == SQ_E1) && (to_sq(*it) == SQ_D1)){
			BoardState st;
			pos.do_move(*it, st);
			std::cout << pos;
			pos.undo_move(*it);
			std::cout << pos;
		}
	}
	getchar();
	*/
	for(MoveList<LEGAL> it(pos); *it; it++){
		if(type_of(pos.moved_piece(*it)) == ROOK){
			BoardState st;
			pos.do_move(*it, st);
			std::cout << pos;
			pos.undo_move(*it);
			std::cout << pos;
			break;
		}
	}
	/*
	getchar();
	pos.init_from("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/1R2K2R b Kkq - 1 2");
	std::cout << pos;
	*/
	return 0;
}




























