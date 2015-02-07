#ifndef COM_INC
#define COM_INC

#include <iostream>
#include <cstdio>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <climits>

#define RESET   "\033[0m"
#define BOLDCOLOR    "\033[1m" 		 /* Bold */
#define BLACKCOLOR   "\033[30m"      /* Black */
#define REDCOLOR     "\033[31m"      /* Red */
#define GREENCOLOR   "\033[32m"      /* Green */
#define YELLOWCOLOR  "\033[33m"      /* Yellow */
#define BLUECOLOR    "\033[34m"      /* Blue */
#define MAGENTACOLOR "\033[35m"      /* Magenta */
#define CYANCOLOR    "\033[36m"      /* Cyan */
#define WHITECOLOR   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */

#ifdef _MSC_VER
#	error "Windows not supported!"
#endif

#if UINTPTR_MAX != 0xffffffffffffffff
#	error "Only 64-bit systems are supported!"
#endif

#include <inttypes.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>

typedef uint64_t Bitboard;
typedef uint64_t Key;

static const int MAX_MOVES = 256;
static const int MAX_PLY = 128;

// Moves are encoded in 16 bits.
// bits 0 - 5: origin square
// bits 6 - 11: destination square
// bits 12 - 13: promotion piece type (from 0 = knight to 3 = queen)
// bits 14 - 15: move flag (none, e.p., castling, promotion)
// Note: For castling, origin = king square, destination = rook square.

enum Move {
	MOVE_NONE,
	MOVE_NULL = 65
};

enum MoveType {
	NORMAL,
	ENPASSANT = 1 << 14,
	CASTLING = 2 << 14,
	PROMOTION = 3 << 14
};

enum PieceType {
	NO_PIECE_TYPE,
	PAWN,
	KNIGHT,
	BISHOP,
	ROOK,
	QUEEN,
	KING,
	ALL_PIECES = 0,
	PIECE_TYPE_NB = 8
};

enum Piece {
	NO_PIECE,
	W_PAWN = 1, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
	B_PAWN = 9, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
	PIECE_NB = 16
};

enum Side {
	WHITE, BLACK, SIDE_NB = 2
};

enum CastlingSide {
	KING_SIDE,
	QUEEN_SIDE,
	CASTLING_SIDE_NB = 2
};

enum CastlingRight {
	NO_CASTLING,
	WHITE_OO,
	WHITE_OOO = WHITE_OO << 1,
	BLACK_OO = WHITE_OO << 2,
	BLACK_OOO = WHITE_OO << 3,
	ANY_CASTLING = WHITE_OO | WHITE_OOO | BLACK_OO | BLACK_OOO,
	CASTLING_RIGHT_NB = 16
};

enum Phase {
	PHASE_MIDGAME = 0,
	PHASE_ENDGAME = 128,
	MG = 0, EG = 1, PHASE_NB = 2
};

enum Bound {
	BOUND_NONE,
	BOUND_UPPER,
	BOUND_LOWER,
	BOUND_EXACT = BOUND_UPPER | BOUND_LOWER
};

enum Value {
	VAL_ZERO = 0,
	VAL_DRAW = 0,
	VAL_KNOWN_WIN = 50000,
	VAL_MATE = 100000,
	VAL_INF = VAL_MATE + 1,
	
	VAL_MATE_IN_MAX_PLY = VAL_MATE - MAX_PLY,
	VAL_MATED_IN_MAX_PLY = -VAL_MATE + MAX_PLY,
	
	VAL_INT_MIN = INT_MIN,
	VAL_INT_MAX = INT_MAX,
	
	PawnValueMg   = 198,   PawnValueEg   = 258,
	KnightValueMg = 817,   KnightValueEg = 846,
	BishopValueMg = 836,   BishopValueEg = 857,
	RookValueMg   = 1270,  RookValueEg   = 1278,
	QueenValueMg  = 2521,  QueenValueEg  = 2558,
	MidgameLimit  = 15581, EndgameLimit  = 3998
};

enum Depth {
	ONE_PLY = 1,
	DEPTH_ZERO = 0,
	DEPTH_MAX = MAX_PLY
};

enum Square {
	SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
	SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
	SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
	SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
	SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
	SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
	SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
	SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
	SQ_NONE,

	SQUARE_NB = 64,

	DELTA_N = 8,
	DELTA_E = 1,
	DELTA_S = -8,
	DELTA_W = -1,

	DELTA_NN = DELTA_N + DELTA_N,
	DELTA_NE = DELTA_N + DELTA_E,
	DELTA_SE = DELTA_S + DELTA_E,
	DELTA_SS = DELTA_S + DELTA_S,
	DELTA_SW = DELTA_S + DELTA_W,
	DELTA_NW = DELTA_N + DELTA_W
};

enum File {
	FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, FILE_NB
};

enum Rank {
	RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_NB
};

enum Score {
	SCORE_ZERO,
	SCORE_INT_MIN = INT_MIN,
	SCORE_INT_MAX = INT_MAX
};

inline Score make_score(Value mg, Value eg){
	return Score((eg << 16) + mg);
}

inline Value mg_value(Score sc){
	union { uint16_t u; uint16_t s; } mg = { uint16_t(unsigned(sc + 0x8000) >> 16) };
	return Value(mg.u);
}

inline Value eg_value(Score sc){
	union { uint16_t u; uint16_t s; } eg = { uint16_t(unsigned(sc + 0x8000) >> 16) };
	return Value(eg.s);
}

#define ENABLE_BASE_OPERATORS_ON(T)                             \
inline T operator+(T d1, T d2){ return T(int(d1) + int(d2)); } \
inline T operator-(T d1, T d2){ return T(int(d1) - int(d2)); } \
inline T operator*(int i, T d){ return T(i * int(d)); }        \
inline T operator*(T d, int i){ return T(int(d) * i); }        \
inline T operator-(T d){ return T(-int(d)); }                  \
inline T& operator+=(T& d1, T d2){ return d1 = d1 + d2; }      \
inline T& operator-=(T& d1, T d2){ return d1 = d1 - d2; }      \
inline T& operator*=(T& d, int i){ return d = T(int(d) * i); }

#define ENABLE_FULL_OPERATORS_ON(T)                             \
ENABLE_BASE_OPERATORS_ON(T)                                     \
inline T& operator++(T& d){ return d = T(int(d) + 1); }        \
inline T& operator--(T& d){ return d = T(int(d) - 1); }        \
inline T operator/(T d, int i){ return T(int(d) / i); }        \
inline int operator/(T d1, T d2){ return int(d1) / int(d2); }  \
inline T& operator/=(T& d, int i){ return d = T(int(d) / i); }

ENABLE_FULL_OPERATORS_ON(Value)
ENABLE_FULL_OPERATORS_ON(PieceType)
ENABLE_FULL_OPERATORS_ON(Piece)
ENABLE_FULL_OPERATORS_ON(Side)
ENABLE_FULL_OPERATORS_ON(Depth)
ENABLE_FULL_OPERATORS_ON(Square)
ENABLE_FULL_OPERATORS_ON(File)
ENABLE_FULL_OPERATORS_ON(Rank)
ENABLE_BASE_OPERATORS_ON(Score)

#undef ENABLE_FULL_OPERATORS_ON
#undef ENABLE_BASE_OPERATORS_ON

inline Value operator+(Value v, int i){ 
	return Value(int(v) + i); 
}

inline Value operator-(Value v, int i){ 
	return Value(int(v) - i); 
}

inline Value& operator+=(Value& v, int i){ 
	return v = v + i; 
}

inline Value& operator-=(Value& v, int i){ 
	return v = v - i; 
}

inline Score operator*(Score, Score);

inline Score operator/(Score s, int i){
	return make_score(mg_value(s) / i, eg_value(s) / i);
}

extern Value PieceValue[PHASE_NB][PIECE_NB];

inline Value piece_val_of(Phase ph, Piece pt){
	return PieceValue[ph][pt];
}

inline Side operator~(Side c){
	return Side(c ^ BLACK); // flip a side
}

inline Square operator~(Square s){
	return Square(s ^ SQ_A8); // vertical flip of square
}

inline CastlingRight operator|(Side c, CastlingSide s){
	return CastlingRight(WHITE_OO << ((s == QUEEN_SIDE) + 2 * c));
}

inline Value mate_in(int ply){
	return VAL_MATE - ply;
}

inline Value mated_in(int ply){
	return -VAL_MATE + ply; // basically negative mate_in(ply)
}

inline Square make_square(Rank r, File f){
	return Square((r << 3) | f); // aka (rank * 8) + file
}

inline Piece make_piece(Side c, PieceType pt){
	return Piece((c << 3) | pt);
}

inline PieceType type_of(Piece pc)  {
	return PieceType(pc & 7);
}

inline Side side_of(Piece pc){
	assert(pc != NO_PIECE);
	return Side(pc >> 3);
}

inline bool is_ok(Square s){
	return (s >= SQ_A1 && s <= SQ_H8);
}

inline File file_of(Square s){
	return File(s & 7);
}

inline Rank rank_of(Square s){
	return Rank(s >> 3);
}

inline Square relative_square(Side c, Square s){
	return Square(s ^ (c * 56));
}

inline Rank relative_rank(Side c, Rank r){
	return Rank(r ^ (c * 7));
}

inline Rank relative_rank(Side c, Square s){
	return relative_rank(c, rank_of(s));
}

inline bool opposite_colors(Square s1, Square s2){
	int s = int(s1) ^ int(s2);
	return ((s >> 3) ^ s) & 1;
}

inline Square pawn_push(Side c){
	return (c == WHITE ? DELTA_N : DELTA_S);
}

inline Square from_sq(Move m){
	return Square(m & 0x3F);
}

inline Square to_sq(Move m){
	return Square((m >> 6) & 0x3F);
}

inline MoveType type_of(Move m){
	return MoveType(m & (3 << 14)); // since 3 = 11 in binary
}

inline PieceType promotion_type(Move m){
	return PieceType(((m >> 12) & 3) + 2);
}

inline Move make_move(Square from, Square to){
	return Move(from | (to << 6));
}

template<MoveType T>
inline Move make(Square from, Square to, PieceType pt = KNIGHT){
	return Move(from | (to << 6) | T | ((pt - KNIGHT) << 12));
}

inline bool is_ok(Move m){
	return (from_sq(m) != to_sq(m)); // checks for MOVE_NULL and MOVE_NONE primarily
}

inline char rank_char_of(const Square& sq){
	return char(rank_of(sq) + '1');
}

inline char file_char_of(const Square& sq){
	return char(file_of(sq) + 'a');
}

inline std::ostream& operator<<(std::ostream& ss, const Square& sq){
	ss << file_char_of(sq) << rank_char_of(sq);
	return ss;
}

/* This is a pseudo-random number generator described in http://vigna.di.unimi.it/ftp/papers/xorshift.pdf */
class RNG {
	uint64_t s;

	uint64_t rand64(void){
		s ^= s >> 12, s ^= s << 25, s ^= s >> 27;
		return (s * 2685821657736338717LL);
	}

public:
	RNG(uint64_t seed) : s(seed){
		assert(seed);
	}

	template<typename T> 
	T rand(void){
		return T(rand64());
	}

	template<typename T> 
	T sparse_rand(void){
		// A "quick-n-dirty" version
		return T(rand64() & rand64() & rand64()); 
	}
};

enum BitCountType {
	CNT_64,
	CNT_64_MAX15,
	CNT_32,
	CNT_32_MAX15,
	CNT_HW_POPCNT
};

const BitCountType Full = CNT_HW_POPCNT;
const BitCountType Max15 = CNT_HW_POPCNT;

template<BitCountType> inline int popcount(Bitboard); // this is a bitcount of all set bits
// Note: Template specialization is used here just to determine the most optimized bitcount
// at compile time rather than run time.

template<>
inline int popcount<CNT_64>(Bitboard b){
	b -=  (b >> 1) & 0x5555555555555555ULL;
	b  = ((b >> 2) & 0x3333333333333333ULL) + (b & 0x3333333333333333ULL);
	b  = ((b >> 4) + b) & 0x0F0F0F0F0F0F0F0FULL;
	return (b * 0x0101010101010101ULL) >> 56;
}

template<>
inline int popcount<CNT_64_MAX15>(Bitboard b){
	b -=  (b >> 1) & 0x5555555555555555ULL;
	b  = ((b >> 2) & 0x3333333333333333ULL) + (b & 0x3333333333333333ULL);
	return (b * 0x1111111111111111ULL) >> 60;
}

template<>
inline int popcount<CNT_32>(Bitboard b){
	unsigned w = unsigned(b >> 32), v = unsigned(b);
	v -=  (v >> 1) & 0x55555555; // 0-2 in 2 bits
	w -=  (w >> 1) & 0x55555555;
	v  = ((v >> 2) & 0x33333333) + (v & 0x33333333); // 0-4 in 4 bits
	w  = ((w >> 2) & 0x33333333) + (w & 0x33333333);
	v  = ((v >> 4) + v + (w >> 4) + w) & 0x0F0F0F0F;
	return (v * 0x01010101) >> 24;
}

template<>
inline int popcount<CNT_32_MAX15>(Bitboard b){
	unsigned w = unsigned(b >> 32), v = unsigned(b);
	v -=  (v >> 1) & 0x55555555; // 0-2 in 2 bits
	w -=  (w >> 1) & 0x55555555;
	v  = ((v >> 2) & 0x33333333) + (v & 0x33333333); // 0-4 in 4 bits
	w  = ((w >> 2) & 0x33333333) + (w & 0x33333333);
	return ((v + w) * 0x11111111) >> 28;
}

template<>
inline int popcount<CNT_HW_POPCNT>(Bitboard b){
	return __builtin_popcountll(b);
}

#endif // #ifndef COM_INC