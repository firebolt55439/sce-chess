#ifndef POS_INCLUDED
#define POS_INCLUDED

#include "Common.h"
#include "Bitboards.h"
#include <cstddef>
#include <string>

class Position;

struct CheckInfo {
	explicit CheckInfo(const Position& pos); // explicit means only this can be used (no defaults)
	
	Bitboard lined; // for discovered checks, pieces in line with king blocked by only one piece
	Bitboard pinned; // any absolutely pinned pieces
	Bitboard kattk[PIECE_TYPE_NB]; // uses king as superpiece, and finds attacks it can do by [piece type emulated]
	Square ksq; // the square of the king
};

struct PosState {
	// This state allows a position to be completely stored and later restored. //
	// Note: Stores stuff like last captured piece, e.p. square, checkers/blockers, etc.
	Key key; // current Zobrist hash
	Value npMaterial[SIDE_NB]; // static value of non-pawn material by side
	int fifty_ct; // halfmove count
	int plies; // game ply count
	int castling; // castling rights mask
	Square epsq; // e.p. square
	Bitboard checkers; // all checkers
	PieceType captured_type; // last piece type captured (useful for SEE-style analysis)
	PosState* prev; // previous (for linked list)
};

class Position {
	private:
		Bitboard check_blockers(Side c, Side kingC) const; // get all pieces blocking check (e.g. discovered check candidates)
		void put_piece(Square s, Side c, PieceType pt); // put a piece
		void remove_piece(Square s, Side c, PieceType pt); // remove a piece
		void move_piece(Square from, Square to, Side c, PieceType pt); // move a piece
		void clear(void); // clear a position
		void set_castling_right(Side c, Square rfrom);
		template<bool Do> void do_castling(Square from, Square& to, Square& rfrom, Square& rto); // can also be used to find information about the castling
		void set_state(PosState* si); // set a state
		
  		Piece board[SQUARE_NB]; // piece-by-square
  		Bitboard byType[PIECE_TYPE_NB]; // piece-by-type
  		Bitboard bySide[SIDE_NB]; // piece-by-side
  		int pCount[SIDE_NB][PIECE_TYPE_NB]; // count of pieces by [side][piece type]
  		Square pList[SIDE_NB][PIECE_TYPE_NB][16]; // get list of square of piece if/a by [side][piece type] (returns a pointer to a Square[16] array)
  		int index[SQUARE_NB]; // used for indexing pList and such
  		int castlingMask[SQUARE_NB]; // castling rights by square (has moved, etc.)
  		Square castlingRookSq[CASTLING_RIGHT_NB];
  		Bitboard castlingPath[CASTLING_RIGHT_NB];
  		PosState orig_state; // the original state of this position (since PosState is a linked list)
  		uint64_t nodes; // node count (for perft, etc. purposes)
  		int ply; // ply this position was searched at
  		Side to_move; // side to move
  		PosState* st; // current state
	public:
		friend std::ostream& operator<<(std::ostream&, const Position&);
		static void init(void);
		
		Position(void){ clear(); }
		Position(const Position& pos);
		Position& operator=(const Position& pos);
		
		void init_from(const std::string& fen); // init from FEN
		const std::string fen(void) const; // get FEN
		
  		Bitboard pieces(void) const; // all pieces
  		Bitboard pieces(PieceType pt) const; // piece-by-type
  		Bitboard pieces(PieceType pt1, PieceType pt2) const; // piece-by-2-types
  		Bitboard pieces(Side c) const; // pieces-by-side
  		Bitboard pieces(Side c, PieceType pt) const; // by side and type
  		Bitboard pieces(Side c, PieceType pt1, PieceType pt2) const; // by side or 2 types
  		Piece at(Square sq) const; // get piece by square
  		Square king_sq(Side s) const; // king square by side
  		Square ep_sq(void) const; // get e.p. square if/a
  		bool empty(Square s) const; // test if square is occupied or not
  		template<PieceType Pt> int count(Side c) const; // count pieces by side and type
  		template<PieceType Pt> const Square* list(Side c) const; // list pieces by side and type
  		
  		int can_castle(Side c) const; // returns masked int of all possible castles for side
  		int can_castle(CastlingRight cr) const; // returns mask of castles given rights
  		bool castling_impeded(CastlingRight cr) const; // if castling is impeded
  		Square castling_rook_square(CastlingRight cr) const; // get rook square
  		
  		Bitboard checkers(void) const; // checkers
  		Bitboard lined(void) const; // discovered check candidates
  		Bitboard pinned(Side c) const; // pinned pieces
  		
  		// TODO: SEE, hash keys, etc.
  		Bitboard attackers_to(Square s) const; // get a bitboard of all attackers to a specific square (all sides included)
  		Bitboard attackers_to(Square s, Bitboard occupied) const; // called by above
  		Bitboard attacks_from(Piece pc, Square s) const; // get attacks from a certain square with a certain piece on it
  		template<PieceType Pt> Bitboard attacks_from(Square s) const; // templated version
  		template<PieceType Pt> Bitboard attacks_from(Square s, Side c) const; // attacks from a specific side and piece type (only for pawns)
  		
  		bool legal(Move m, Bitboard pinned) const; // check if move is legal
  		bool pseudo_legal(const Move m) const; // check if move is pseudo-legal
  		bool capture(Move m) const; // s.e.
  		bool capture_or_promotion(Move m) const; // s.e.
  		bool gives_check(Move m, const CheckInfo& ci) const; // s.e.
  		bool advanced_pawn_push(Move m) const; // if pawn is being pushed further than rank 4
  		Piece moved_piece(Move m) const; // s.e.
  		PieceType captured_piece_type(void) const; // s.e. (for the *just* captured piece type from PosState)
  		
  		void do_move(Move m, PosState& st); // s.e.
  		void do_move(Move m, PosState& st, const CheckInfo& ci, bool moveIsCheck); // called by above
  		void undo_move(Move m); // s.e.
  		void do_null_move(PosState& st); // s.e.
  		void undo_null_move(void); // s.e.
  		
  		Side side_to_move(void) const; // side to move
  		Phase game_phase(void) const; // game phase
  		int get_ply(void) const; // game ply
  		void set_nodes(uint64_t to);
  		uint64_t get_nodes(void) const;
  		bool is_draw(void) const; // test if is draw
  		Score psq_score(void) const; // get piece-square table score for position
  		Value np_material(Side c) const; // non-pawn material by side
  		
  		void flip(void); // flip sides completely (for rooting out evaluation symmetry bugs)
};

inline Side Position::side_to_move(void) const {
	return to_move;
}

inline int Position::get_ply(void) const {
	return ply;
}

inline void Position::set_nodes(uint64_t to){
	nodes = to;
}

inline uint64_t Position::get_nodes(void) const {
	return nodes;
}

inline Bitboard Position::pieces(void) const {
	return byType[ALL_PIECES];
}

inline Bitboard Position::pieces(PieceType pt) const {
	return byType[pt];
}

inline Bitboard Position::pieces(PieceType pt1, PieceType pt2) const {
	return byType[pt1] | byType[pt2];
}

inline Bitboard Position::pieces(Side c) const {
	return bySide[c];
}

inline Bitboard Position::pieces(Side c, PieceType pt) const {
	return byType[pt] & bySide[c];
}

inline Bitboard Position::pieces(Side c, PieceType pt1, PieceType pt2) const {
	return pieces(pt1, pt2) & bySide[c];
}

template<PieceType Pt> inline int Position::count(Side c) const {
	return pCount[c][Pt];
}

template<PieceType Pt> inline const Square* Position::list(Side c) const {
	return pList[c][Pt];
}

inline Piece Position::at(Square sq) const {
	return board[sq];
}

inline bool Position::empty(Square sq) const {
	return (board[sq] != NO_PIECE);
}

inline Square Position::king_sq(Side c) const {
	return pList[c][KING][0];
}

inline Square Position::ep_sq(void) const {
	return st->epsq;
}

inline Bitboard Position::attackers_to(Square s) const {
	return attackers_to(s, byType[ALL_PIECES]);
}

template<PieceType Pt>
inline Bitboard Position::attacks_from(Square s) const {
	return (Pt == BISHOP || Pt == ROOK) ? (attacks_bb<Pt>(s, byType[ALL_PIECES])) : 
			((Pt == QUEEN) ? (attacks_bb<ROOK>(s, byType[ALL_PIECES]) | attacks_bb<BISHOP>(s, byType[ALL_PIECES])) : 
			StepAttacksBB[Pt][s]);
}

template<>
inline Bitboard Position::attacks_from<PAWN>(Square s, Side c) const {
	return StepAttacksBB[make_piece(c, PAWN)][s];
}

inline Bitboard Position::attacks_from(Piece pc, Square s) const {
	return attacks_bb(pc, s, byType[ALL_PIECES]);
}

inline Bitboard Position::checkers(void) const {
	return st->checkers;
}

inline Bitboard Position::lined(void) const {
	return check_blockers(to_move, ~to_move);
}

inline Bitboard Position::pinned(Side c) const {
	return check_blockers(c, c); // it does make sense
}

inline bool Position::capture_or_promotion(Move m) const {
	assert(is_ok(m));
	return (type_of(m) != NORMAL) ? (type_of(m) != CASTLING) : (!empty(to_sq(m)));
}

inline bool Position::capture(Move m) const {
	// Castling is encoded as "king to rook"
	assert(is_ok(m));
	return (!empty(to_sq(m)) && (type_of(m) != CASTLING)) || (type_of(m) == ENPASSANT);
}

inline PieceType Position::captured_piece_type(void) const {
	return st->captured_type;
}

inline int Position::can_castle(CastlingRight cr) const {
	return (st->castling & cr);
}

inline int Position::can_castle(Side c) const {
	return (st->castling & ((WHITE_OO | WHITE_OOO) << (2 * c))); // based on enum CastlingRights (all basically WHITE_OO << n)
}

inline bool Position::castling_impeded(CastlingRight cr) const {
	return byType[ALL_PIECES] & castlingPath[cr]; // checks the castling path (e.g. for WHITE_OO, BLACK_OOO, etc.)
}

inline Square Position::castling_rook_square(CastlingRight cr) const {
	return castlingRookSq[cr];
}

inline Piece Position::moved_piece(Move m) const {
	return board[from_sq(m)];
}



















#endif // #ifndef POS_INCLUDED