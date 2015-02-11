#ifndef PAWNS_INCLUDED
#define PAWNS_INCLUDED

#include "Common.h"
#include "Bitboards.h"
#include "Board.h"

namespace Pawns {
	struct PawnEntry {
		// This contains information about a pawn structure. //
		Key key; // no hash collisions allowed
		Score score; // the final score
		Bitboard passedPawns[SIDE_NB]; // passed pawns by side
		Bitboard pawnAttks[SIDE_NB]; // pawn attacks by side
		Square kingSqs[SIDE_NB]; // king squares by side
		Score kingSafety[SIDE_NB]; // king safety score by side
		int castlingRights[SIDE_NB]; // castling rights by side
		int semiopenFiles[SIDE_NB]; // semi-open files by side
		
		Score pawn_score(void){
			return score;
		}
		// TODO: Finish
		// TODO: King Safety
		// TODO: Use in Evaluation
	};
	
	void init(void);
	PawnEntry* probe(const Board& pos);
}

#endif // #ifndef PAWNS_INCLUDED