#ifndef BOARD_INC
#define BOARD_INC

#include "Common.h"
#include "Bitboards.h"

class Board;

struct CheckInfo {
	explicit CheckInfo(const Board& pos);
	Bitboard kattk[PIECE_TYPE_NB]; // if king was a superpiece, this stores its attacks
	Square ksq; // king square of side to move
};

struct BoardState {
	Key key; // Zobrist hash key for board
	Key pawn_key; // Zobrist hash key for only pawns
	Key material_key; // Zobrist hash key for only material
	int ply; // fullmove ct but starts from 0
	int fifty_ct; // halfmove ct
	int castling; // castling rights mask
	Square epsq; // e.p. square if/a (or SQ_NONE)
	Bitboard checkers; // everything giving check
	Bitboard pinned; // pinned pieces
	Bitboard lined; // everything in line that can give check if a piece is removed
	BoardState* prev; // previous board state
	PieceType capd; // captured piece type (for undoing move)
};

extern const std::string PieceChar;

class Board {
	private:
		Side to_move; // side to move
		Piece board[SQUARE_NB]; // piece-by-square
		Bitboard byType[PIECE_TYPE_NB]; // piece-by-type (incl. ALL_PIECES)
		Bitboard bySide[SIDE_NB]; // piece-by-side
		int pCount[SIDE_NB][PIECE_TYPE_NB]; // piece-count by side and piece type
		BoardState orig_st; // original state
		BoardState* st; // current state
		
		void clear(void);
		Bitboard check_blockers(Side ofC, Side kingC) const; // get pieces of side 'ofC' blocking check on king of side 'kingC'
		void put_piece(PieceType pt, Side c, Square sq); // s.e.
		void move_piece(PieceType pt, Side c, Square from, Square to); // s.e.
		void remove_piece(PieceType pt, Side c, Square sq); // s.e.
		void update_state(BoardState* st); // update a state by filling in required info and such (e.g. key, checkers, etc.)
	public:
		const char* get_representation(void);
		friend std::ostream& operator<<(std::ostream&, const Board& board);
		
		Board(void){ clear(); }
		Board(const Board& brd) = delete; // none of this
		Board& operator=(const Board& brd);
		
		static void init(void); // set up Zobrist keys, etc.
		
		void init_from(const std::string& fen); // init from FEN
		void init_from(const char* fen); // init from FEN (const char* overload)
		std::string fen(void) const; // get FEN
		bool is_draw(void) const; // check if the position is drawn (aside from stalemate)
		
		// Pieces //
		Side side_to_move(void) const; // side to move
		Piece at(Square sq) const; // piece at square
		Bitboard all(void) const; // occupancy
		bool empty(Square sq) const; // test if square is empty
		int count(Side c, PieceType pt) const; // count pieces of color
		Bitboard pieces(Side c) const; // piece-by-side
		Bitboard pieces(PieceType pt) const; // piece-by-type
		Bitboard pieces(PieceType pt1, PieceType pt2) const; // 2 piece-by-types meshed together
		Bitboard pieces(Side c, PieceType pt) const; // piece by side and type
		Bitboard pieces(Side c, PieceType pt1, PieceType pt2) const; // pieces by side and 2 types
		Square king_sq(Side c) const; // get king square for side
		Square ep_sq(void) const; // get e.p. square if/a
		
		// Castling //
		int get_castling_rights(void) const; // get castling rights mask
		void set_castling_rights(CastlingRight cr); // set castling rights
		bool can_castle(void) const; // if anyone can castle
		bool can_castle(CastlingRight cr) const; // if the specified type of castle is allowed
		Bitboard castling_path(CastlingRight cr) const; // the castling path for the specified type of castle
		Bitboard king_castling_path(CastlingRight cr) const; // the squares the *king* moves for the specified type of castle (e.g. detect castling through check, etc.)
		Square castling_rook_sq(CastlingRight cr) const; // the rook square for the specified type of castle
		
		// Checking //
		Bitboard checkers(void) const; // get pieces giving check
		Bitboard pinned(Side c) const; // get absolutely pinned pieces by side
		Bitboard lined(Side c) const; // get pieces of *ours* that, if we remove, will give *them* check
		
		// Attacks //
		Bitboard attackers_to(Square sq, Bitboard occ) const; // get all attacks to square (side-ind.)
		Bitboard attacks_from(Piece pc, Square s) const; // get attacks from square by piece
		template<PieceType Pt> Bitboard attacks_from(Square sq) const; // get attacks from square by piece type (side-ind.)
		template<PieceType Pt> Bitboard attacks_from(Square sq, Side c) const; // get attacks from square by piece type and side (side-ind.)
		
		// SEE
		Value see(Move m) const; // SEE a move
		Value see_sign(Move m) const; // SEE the sign of the move (pos./neg.)
		
		// Moves //
		Piece moved_piece(Move m) const; // piece moved
		bool is_capture(Move m) const; // check if move is a capture
		bool legal(Move m, Bitboard pinned) const; // check if a move is legal
		bool gives_check(Move m, CheckInfo& ci) const; // check if a move gives check
		void do_move(Move m, BoardState& new_st); // do a move and get a new state (as well as updating current state with prev. link)
		void undo_move(Move m); // undo a move
		
		// Hashes //
		Key key(void) const; // Board hash
		Key pawn_key(void) const; // Pawn hash
		Key material_key(void) const; // Material hash
		
		// Other //
		int get_ply(void){ return st->ply; }
		void set_ply(int to){ st->ply = to; }
};

inline Side Board::side_to_move(void) const {
	return to_move;
}

inline Piece Board::at(Square sq) const {
	return board[sq];
}

inline Bitboard Board::all(void) const {
	return byType[ALL_PIECES];
}

inline bool Board::empty(Square sq) const {
	return !(byType[ALL_PIECES] & sq);
}

inline int Board::count(Side c, PieceType pt) const {
	return pCount[c][pt];
}

inline Bitboard Board::pieces(Side c) const {
	return bySide[c];
}

inline Bitboard Board::pieces(PieceType pt) const {
	return byType[pt];
}

inline Bitboard Board::pieces(PieceType pt1, PieceType pt2) const {
	return byType[pt1] | byType[pt2];
}

inline Bitboard Board::pieces(Side c, PieceType pt) const {
	return byType[pt] & bySide[c];
}

inline Bitboard Board::pieces(Side c, PieceType pt1, PieceType pt2) const {
	return bySide[c] & (byType[pt1] | byType[pt2]);
}

inline Square Board::king_sq(Side c) const {
	return lsb(byType[KING] & bySide[c]);
}

inline Square Board::ep_sq(void) const {
	return st->epsq;
}

inline int Board::get_castling_rights(void) const {
	return st->castling;
}

inline void Board::set_castling_rights(CastlingRight cr){
	st->castling = cr;
}

inline bool Board::can_castle(void) const {
	return (st->castling & ANY_CASTLING);
}

inline bool Board::can_castle(CastlingRight cr) const {
	return (st->castling & cr);
}

inline Bitboard Board::checkers(void) const {
	return st->checkers;
}

inline Bitboard Board::pinned(Side c) const {
	// TODO: For 'pinned' and 'lined', consider saving once and returning cached value
	return check_blockers(to_move, to_move); // pieces of ours pinned against our own king
}

inline Bitboard Board::lined(Side c) const {
	return check_blockers(c, ~c); // get discovered check candidates AGAINST *their* king
}

template<PieceType Pt>
inline Bitboard Board::attacks_from(Square sq) const {
	assert(Pt != PAWN); // should not be used for pawns
	return (Pt == ROOK || Pt == BISHOP) ? (attacks_bb<Pt>(sq, byType[ALL_PIECES])) : ((Pt == QUEEN) ? (attacks_from<BISHOP>(sq) | attacks_from<ROOK>(sq)) : (StepAttacksBB[Pt][sq]));
}

template<>
inline Bitboard Board::attacks_from<PAWN>(Square sq, Side c) const {
	return StepAttacksBB[make_piece(c, PAWN)][sq];
}

inline Bitboard Board::attacks_from(Piece pc, Square s) const {
	return attacks_bb(pc, s, byType[ALL_PIECES]);
}

inline bool Board::is_capture(Move m) const {
	return (type_of(m) != CASTLING) && (!empty(to_sq(m)) || (type_of(m) == ENPASSANT));
}

inline Key Board::key(void) const {
	return st->key;
}

inline Key Board::pawn_key(void) const {
	return st->pawn_key;
}

inline Key Board::material_key(void) const {
	return st->material_key;
}

inline Piece Board::moved_piece(Move m) const {
	return board[from_sq(m)];
}




















#endif // #ifndef BOARD_INC