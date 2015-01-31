#ifndef BBOARDS_INC
#define BBOARDS_INC

#include "Common.h"
#include <sstream>

namespace Bitboards {
	void init(void);
	
	std::string pretty(const Bitboard& bb);
}

// Constants //
const Bitboard DarkSquares = 0xAA55AA55AA55AA55ULL; // all the dark squares on a chessboard

const Bitboard FileABB = 0x0101010101010101ULL;
const Bitboard FileBBB = FileABB << 1;
const Bitboard FileCBB = FileABB << 2;
const Bitboard FileDBB = FileABB << 3;
const Bitboard FileEBB = FileABB << 4;
const Bitboard FileFBB = FileABB << 5;
const Bitboard FileGBB = FileABB << 6;
const Bitboard FileHBB = FileABB << 7;

const Bitboard Rank1BB = 0xFF;
const Bitboard Rank2BB = Rank1BB << (8 * 1);
const Bitboard Rank3BB = Rank1BB << (8 * 2);
const Bitboard Rank4BB = Rank1BB << (8 * 3);
const Bitboard Rank5BB = Rank1BB << (8 * 4);
const Bitboard Rank6BB = Rank1BB << (8 * 5);
const Bitboard Rank7BB = Rank1BB << (8 * 6);
const Bitboard Rank8BB = Rank1BB << (8 * 7);

/* Some useful arrays */
extern int SquareDistance[SQUARE_NB][SQUARE_NB];

// Rook Magics //
extern Bitboard RookMasks[SQUARE_NB];
extern Bitboard RookMagics[SQUARE_NB];
extern Bitboard* RookAttacks[SQUARE_NB];
extern unsigned RookShifts[SQUARE_NB]; // 'unsigned' = 'unsigned int' by default

// Bishop Magics //
extern Bitboard BishopMasks[SQUARE_NB];
extern Bitboard BishopMagics[SQUARE_NB];
extern Bitboard* BishopAttacks[SQUARE_NB];
extern unsigned BishopShifts[SQUARE_NB];

// Useful Arrays //
extern Bitboard SquareBB[SQUARE_NB]; // better than doing 1ULL << square every time for a square mask...
extern Bitboard FileBB[FILE_NB]; // self-explanatory
extern Bitboard RankBB[RANK_NB]; // s.e.
extern Bitboard AdjacentFilesBB[FILE_NB]; // s.e.
extern Bitboard InFrontBB[SIDE_NB][RANK_NB]; // all ranks ahead
extern Bitboard StepAttacksBB[PIECE_NB][SQUARE_NB]; // this gives everything that can be attacked by [piece][square] (but should not be used for sliders)
extern Bitboard BetweenBB[SQUARE_NB][SQUARE_NB]; // all squares between two squares if/a
extern Bitboard LineBB[SQUARE_NB][SQUARE_NB]; // all horizontal/vertical lines between two squares
extern Bitboard DistanceRingBB[SQUARE_NB][8];
extern Bitboard ForwardBB[SIDE_NB][SQUARE_NB]; // everything ahead of a square in a line
extern Bitboard PassedPawnMask[SIDE_NB][SQUARE_NB]; // is basically ForwardBB OR'd with PawnAttackSpan
extern Bitboard PawnAttackSpan[SIDE_NB][SQUARE_NB]; // self-explanatory
extern Bitboard PseudoAttacks[PIECE_TYPE_NB][SQUARE_NB]; // pseudo attacks (incredibly useful for evaluation)

/* Operators and More. */

inline Bitboard operator&(Bitboard b, Square s){
	return b & SquareBB[s]; // test if square is in bitboard
}

inline Bitboard operator|(Bitboard b, Square s){
	return b | SquareBB[s]; // add square to bitboard
}

inline Bitboard operator^(Bitboard b, Square s){
	return b ^ SquareBB[s]; // add/remove square from bitboard
}

inline Bitboard& operator|=(Bitboard& b, Square s){
	return b |= SquareBB[s]; // shortcut
}

inline Bitboard& operator^=(Bitboard& b, Square s){
	return b ^= SquareBB[s]; // shortcut
}

inline bool more_than_one(Bitboard b){ // tests if more than one bit is set
	return b & (b - 1); // b & (b - 1) clears the LSB, so if the cleared version is nonzero, then it had more than one bit set
}

/* Some rank and file retrievers. */
inline Bitboard rank_bb(Rank r){
	return RankBB[r];
}

inline Bitboard rank_bb(Square s){
	return RankBB[rank_of(s)];
}

inline Bitboard file_bb(File f){
	return FileBB[f];
}

inline Bitboard file_bb(Square s){
	return FileBB[file_of(s)];
}

template<Square Delta>
inline Bitboard shift_bb(Bitboard b){
	// This shifts the bitboard in a certain direction. //
	// Note: Mainly used to advance pawns and such.
	return  Delta == DELTA_N  ?  b               << 8 : Delta == DELTA_S  ?  b             >> 8
			: Delta == DELTA_NE ? (b & ~FileHBB) << 9 : Delta == DELTA_SE ? (b & ~FileHBB) >> 7
			: Delta == DELTA_NW ? (b & ~FileABB) << 7 : Delta == DELTA_SW ? (b & ~FileABB) >> 9
			: 0;
}

inline Bitboard adjacent_files_bb(File f){
	// Returns the BB of adjacent files to given one. //
	return AdjacentFilesBB[f];
}

inline Bitboard between_bb(Square s1, Square s2){
	// Returns the BB of all squares between, not inclusive, of
	// the given squares.
	// Note: 0ULL is returned if the given squares are not
	// on the same row, column, or diagonal.
	return BetweenBB[s1][s2];
}

// Note: "In front of" from a given side's POV is with respect to a pawn 
// advance.

inline Bitboard in_front_bb(Side c, Rank r){
	// Returns a BB of all ranks in front of the given one. //
	return InFrontBB[c][r];
}

inline Bitboard forward_bb(Side c, Square s){
	// Returns all the squares in the current file ahead of the current square
	// with a given side.
	return ForwardBB[c][s];
}

inline Bitboard pawn_attack_span(Side c, Square s){
	// Returns the two squares a pawn can attack of a given side from a certain
	// square.
	return PawnAttackSpan[c][s];
}

inline Bitboard passed_pawn_mask(Side c, Square s){
	// Returns its pawn attack span OR its forward_bb. //
	// Note: Primarily used for testing for passed pawn-ness.
	return PassedPawnMask[c][s];
}

inline Bitboard squares_of_color(Square s){
	// Returns a BB with all squares of the same color of the given one. //
	return (DarkSquares & s ? DarkSquares : ~DarkSquares);
}

inline bool aligned(Square s1, Square s2, Square s3){
	// Returns if the three given squares are aligned in any way. //
	return LineBB[s1][s2] & s3;
}

/* These distance functions return the file or rank distance, or whichever is greater. */
template<typename T> 
inline int distance(T x, T y){ 
	return (x < y ? y - x : x - y); // or std::abs(x - y) 
}

template<> 
inline int distance<Square>(Square x, Square y){ 
	return SquareDistance[x][y]; 
}

template<typename T1, typename T2> inline int distance(T2 x, T2 y);

template<> 
inline int distance<File>(Square x, Square y){ 
	return distance(file_of(x), file_of(y)); 
}

template<> 
inline int distance<Rank>(Square x, Square y){ 
	return distance(rank_of(x), rank_of(y)); 
}

template<PieceType Pt>
inline unsigned magic_index(Square s, Bitboard occupied){
	// This is a helper function for the magic bitboards index. //
	Bitboard* const Masks = Pt == ROOK ? RookMasks  : BishopMasks;
	Bitboard* const Magics = Pt == ROOK ? RookMagics : BishopMagics;
	unsigned* const Shifts = Pt == ROOK ? RookShifts : BishopShifts;
	return unsigned(((occupied & Masks[s]) * Magics[s]) >> Shifts[s]);
}

template<PieceType Pt>
inline Bitboard attacks_bb(Square s, Bitboard occupied){
	// This returns a BB of all the squares attacked by a specific piece type
	// on a square given the occupied squares BB.
	return (Pt == ROOK ? RookAttacks : BishopAttacks)[s][magic_index<Pt>(s, occupied)];
}

inline Bitboard attacks_bb(Piece pc, Square s, Bitboard occupied){
	// Overload for piece as an explicit parameter rather than piece type. //
	switch(type_of(pc)){
		case BISHOP: 
			return attacks_bb<BISHOP>(s, occupied);
		case ROOK: 
			return attacks_bb<ROOK>(s, occupied);
		case QUEEN: 
			return attacks_bb<BISHOP>(s, occupied) | attacks_bb<ROOK>(s, occupied);
		default: 
			return StepAttacksBB[pc][s];
	}
}

inline Square lsb(Bitboard b){
	Bitboard idx;
	__asm__("bsfq %1, %0": "=r"(idx): "rm"(b) );
	return (Square) idx;
}

inline Square msb(Bitboard b){
	Bitboard idx;
	__asm__("bsrq %1, %0": "=r"(idx): "rm"(b) );
	return (Square) idx;
}

inline Square pop_lsb(Bitboard* b){
	// This returns the LSB index of the specified bitboard but also clears
	// the bit. Useful for looping through a bitboard.
	const Square s = lsb(*b);
	*b &= *b - 1;
	return s;
}

/* These return the most/least advanced squares relative to a specified side. */
inline Square frontmost_sq(Side c, Bitboard b){ 
	return (c == WHITE ? msb(b) : lsb(b));
}

inline Square  backmost_sq(Side c, Bitboard b){ 
	return (c == WHITE ? lsb(b) : msb(b));
}

#endif // #ifndef BBOARDS_INCLUDED