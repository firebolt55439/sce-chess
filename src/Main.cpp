#include <iostream>
#include <cstdio>
#include "Common.h"
#include "Bitboards.h"
#include "Position.h"

int main(int argc, char** argv){
	Bitboards::init();
	Position::init();
	Position pos;
	pos.init_from("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	// Board //
	std::cout << pos;
	// Tests //
	std::cout << popcount<Full>(attacks_bb<BISHOP>(SQ_D4, Bitboard(0) | SQ_A6 | SQ_C5)) << std::endl;
	std::cout << Bitboards::pretty(attacks_bb<BISHOP>(SQ_D4, Bitboard(0) | SQ_A6 | SQ_C5)) << std::endl;
	for(Piece pt = W_PAWN; pt <= W_KING; pt++){
		std::cout << Bitboards::pretty(pos.attacks_from(pt, SQ_D4)) << std::endl;
	}
	return 0;
}