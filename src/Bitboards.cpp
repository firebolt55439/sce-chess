#include "Common.h"
#include "Bitboards.h"
#include <cstring>
#include <algorithm>

int SquareDistance[SQUARE_NB][SQUARE_NB];

Bitboard RookMasks [SQUARE_NB];
Bitboard RookMagics[SQUARE_NB];
Bitboard* RookAttacks[SQUARE_NB];
unsigned RookShifts[SQUARE_NB];

Bitboard BishopMasks [SQUARE_NB];
Bitboard BishopMagics[SQUARE_NB];
Bitboard* BishopAttacks[SQUARE_NB];
unsigned BishopShifts[SQUARE_NB];

Bitboard SquareBB[SQUARE_NB];
Bitboard FileBB[FILE_NB];
Bitboard RankBB[RANK_NB];
Bitboard AdjacentFilesBB[FILE_NB];
Bitboard InFrontBB[SIDE_NB][RANK_NB];
Bitboard StepAttacksBB[PIECE_NB][SQUARE_NB];
Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
Bitboard LineBB[SQUARE_NB][SQUARE_NB];
Bitboard DistanceRingBB[SQUARE_NB][8];
Bitboard ForwardBB[SIDE_NB][SQUARE_NB];
Bitboard PassedPawnMask[SIDE_NB][SQUARE_NB];
Bitboard PawnAttackSpan[SIDE_NB][SQUARE_NB];
Bitboard PseudoAttacks[PIECE_TYPE_NB][SQUARE_NB];

/* Magic Bitboard Tables */
Bitboard RookTable[0x19000]; // for rook attacks
Bitboard BishopTable[0x1480]; // for bishop attacks

typedef unsigned (Fn)(Square, Bitboard);
void init_magics(Bitboard table[], Bitboard* attacks[], Bitboard magics[], Bitboard masks[], unsigned shifts[], Square deltas[], Fn index);

std::string Bitboards::pretty(const Bitboard& bb){
	std::stringstream ss;
	for(Rank r = RANK_8; r >= RANK_1; r--){
		for(File f = FILE_A; f <= FILE_H; f++){
			Square s = make_square(r, f);
			ss << ((bb & s) ? '1' : '0') << " ";
		}
		ss << "\n";
	}
	return ss.str();
}

void Bitboards::init(void){
	/* SquareBB */
	for(Square s = SQ_A1; s <= SQ_H8; s++){
		SquareBB[s] = 1ULL << s;
	}
    /* FileBB */
	for(File f = FILE_A; f <= FILE_H; f++){
		FileBB[f] = (f != FILE_A) ? (FileBB[f - 1] << 1) : FileABB;
	}
	/* RankBB */
	for(Rank r = RANK_1; r <= RANK_8; r++){
		RankBB[r] = (r != RANK_1) ? (RankBB[r - 1] << 8) : Rank1BB;
	}
	/* AdjacentFilesBB */
	for(File f = FILE_A; f <= FILE_H; f++){
		AdjacentFilesBB[f] = ((f - 1) < FILE_A ? 0ULL : FileBB[f - 1]) | ((f + 1) > FILE_H ? 0ULL : FileBB[f + 1]);
	}
	/* InFrontBB */
	InFrontBB[BLACK][RANK_1] = 0ULL;
	for(Rank r = RANK_1; r < RANK_8; r++){
		InFrontBB[BLACK][r + 1] = InFrontBB[BLACK][r] | RankBB[r];
		InFrontBB[WHITE][r] = ~InFrontBB[BLACK][r + 1];
	}
	/* ForwardBB, PawnAttackSpan, PassedPawnMask */
	for(Side c = WHITE; c <= BLACK; c++){
		for(Square s = SQ_A1; s <= SQ_H8; s++){
			ForwardBB[c][s] = InFrontBB[c][rank_of(s)] & FileBB[file_of(s)];
			PawnAttackSpan[c][s] = InFrontBB[c][rank_of(s)] & AdjacentFilesBB[file_of(s)];
			PassedPawnMask[c][s] = ForwardBB[c][s] | PawnAttackSpan[c][s];
		}
	}
	/* SquareDistance, DistanceRingBB */
	std::memset(&DistanceRingBB[0][0], 0, (8 * int(SQUARE_NB) * sizeof(Bitboard)));
	for(Square i = SQ_A1; i <= SQ_H8; i++){
		for(Square j = SQ_A1; j <= SQ_H8; j++){
			SquareDistance[i][j] = std::max(distance<Rank>(i, j), distance<File>(i, j));
			DistanceRingBB[i][SquareDistance[i][j] - 1] |= i; // overloaded for adding in a square
		}
	}
	/* StepAttacksBB */
	int steps[][9] = { {}, { 7, 9 }, { 17, 15, 10, 6, -6, -10, -15, -17 }, 
					   {}, {}, {}, { 9, 7, -7, -9, 8, 1, -1, -8 } }; // steps by piece type
	std::memset(&StepAttacksBB[0][0], 0, (sizeof(Bitboard) * int(PIECE_NB) * int(SQUARE_NB)));
	for(Side c = WHITE; c <= BLACK; c++){
		for(PieceType pt = PAWN; pt <= KING; pt++){
			for(Square s = SQ_A1; s <= SQ_H8; s++){
				for(int i = 0; (steps[pt][i]); i++){
					Square to = Square(s + ((c == WHITE) ? steps[pt][i] : (-steps[pt][i])));
					if(is_ok(to) && (distance(s, to) < 3)){
						StepAttacksBB[make_piece(c, pt)][s] |= to;
					}
				}
			}
		}
	}
	/* Magic Bitboards */
	Square RookDeltas[] = { DELTA_N,  DELTA_E,  DELTA_S,  DELTA_W  }; // rook steps
	Square BishopDeltas[] = { DELTA_NE, DELTA_SE, DELTA_SW, DELTA_NW }; // bishop steps
	init_magics(RookTable, RookAttacks, RookMagics, RookMasks, RookShifts, RookDeltas, magic_index<ROOK>); // init rook magics
	init_magics(BishopTable, BishopAttacks, BishopMagics, BishopMasks, BishopShifts, BishopDeltas, magic_index<BISHOP>); // init bishop magics
	/* Init a Psuedo Attacks table for reachability testing using magic bitboards. */
	/* PsuedoAttacks, LineBB, BetweenBB */
	for(Square i = SQ_A1; i <= SQ_H8; i++){
		PseudoAttacks[QUEEN][i] = PseudoAttacks[BISHOP][i] = attacks_bb<BISHOP>(i, 0);
		PseudoAttacks[QUEEN][i] = PseudoAttacks[ROOK][i] = attacks_bb<ROOK>(i, 0);
		for(Square j = SQ_A1; j <= SQ_H8; j++){
			Piece pc = (PseudoAttacks[BISHOP][i] & j) ? W_BISHOP :
					   (PseudoAttacks[ROOK][i] & j) ? W_ROOK : NO_PIECE;
			if(pc == NO_PIECE) continue;
			LineBB[i][j] = (attacks_bb(pc, i, 0) & attacks_bb(pc, j, 0)) | i | j; // used for testing alignment
			BetweenBB[i][j] = attacks_bb(pc, i, SquareBB[j]) & attacks_bb(pc, j, SquareBB[i]); // between squares if on same row, col, or diagonal, else 0
		}
	}
}

Bitboard sliding_attack(Square deltas[], Square sq, Bitboard occupied){
	/* This function should only be used for initialization purposes due to slowness. */
	Bitboard ret = 0ULL;
	for(int i = 0; i < 4; i++){
		// 0 - 3 since only first 4 delta types are used. //
		for(Square s = sq + deltas[i]; (is_ok(s) && (distance(s, s - deltas[i]) == 1)); s += deltas[i]){
			// Just a regular 0x88-style sliding attack generator. //
			ret |= s;
			if(occupied & s) break; // we have hit a piece, so the ray ends
		}
	}
	return ret;
}

void init_magics(Bitboard table[], Bitboard* attacks[], Bitboard magics[], Bitboard masks[], unsigned shifts[], Square deltas[], Fn index){
	/* "Fancy" magic bitboards. */
	int seeds[][RANK_NB] = { { 8977, 44560, 54343, 38998, 5731, 95205, 104912, 17020 },
						   {  728, 10316, 55013, 32803, 12281, 15100, 16645, 255 } }; // magic seeds
	Bitboard occupancy[4096], reference[4096], edges, b;
	int i, size;
	attacks[SQ_A1] = table; // beginning of attack table
	for(Square s = SQ_A1; s <= SQ_H8; s++){
		// Don't have to worry about the edges of the board for the occupancy variations. //
		edges = ((Rank1BB | Rank8BB) & ~rank_bb(s)) | ((FileABB | FileHBB) & ~file_bb(s));
		// Finding the magic shift index. //
		masks[s]  = sliding_attack(deltas, s, 0) & ~edges;
		shifts[s] = 64 - popcount<Max15>(masks[s]);
		// Carry-Rippler technique for traversing subsets of a set is used here. //
		b = size = 0;
		do {
			occupancy[size] = b;
			reference[size] = sliding_attack(deltas, s, b);
			++size;
			b = (b - masks[s]) & masks[s];
		} while (b);
		// Find offset of next square in table //
		if(s < SQ_H8) attacks[s + 1] = attacks[s] + size;
		RNG rng(seeds[1][rank_of(s)]); // 1 = Is64Bit
		// Find a magic that passes the test, and in the process
		// build an attack table.
		do {
			do {
				magics[s] = rng.sparse_rand<Bitboard>();
			} while (popcount<Max15>((magics[s] * masks[s]) >> 56) < 6);
			std::memset(attacks[s], 0, size * sizeof(Bitboard));
			// Now verify the magic. //
			for(i = 0; i < size; i++){
				Bitboard& attack = attacks[s][index(s, occupancy[i])];
				if(attack && attack != reference[i]) break; // we are done (did this before, and it follows a distinct order which repeats)
				assert(reference[i]);
				attack = reference[i];
			}
		} while (i < size);
	}
}