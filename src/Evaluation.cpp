#include "Common.h"
#include "Bitboards.h"
#include "Board.h"
#include "MoveGen.h"
#include "Evaluation.h"

#define S(mg, eg) make_score(mg, eg)

Score psq_score_of(const Board& pos){
	Score ret = 0;
	Bitboard pcs = pos.all();
	while(pcs){
		Square on = pop_lsb(&pcs);
		Piece pc = pos.at(on);
		Side us = color_of(pc);
		ret += (us == WHITE) ? Eval::PSQT[type_of(pc)][on] : (-Eval::PSQT[type_of(pc)][on]);
	}
	return ret
}

Value evaluate(const Board& pos){
	// Returns score relative to side to move (e.g. -200 for black to move is +200 for white to move). //
	// Note: All helper functions should return score relative to white. //
	Score ret = S(0, 0);
	// Material Factoring //
	for(PieceType pt = PAWN; pt < KING; pt++){
		ret += (pos.count(WHITE, pt) - pos.count(BLACK, pt)) * piece_val_of(MG, pt);
	}
	// TOOD: Imbalance
	// Piece-Square Table //
	ret += psq_score_of(pos);
	// Return //
	// TODO: Taper eval based on Phase
	return mg_value(ret);
}
	