#ifndef BOARD_INC
#define BOARD_INC

#include "Common.h"
#include "Bitboards.h"

struct BoardState {
	Key key; // hash key
	int ply; // fullmove ct
	int fifty_ct; // halfmove ct
	int castling; // castling rights mask
	Square epsq; // e.p. square if/a (or SQ_NONE)
	Bitboard checkers; // everything giving check
	Bitboard pinned; // pinned pieces
	Bitboard lined; // everything in line that can give check if a piece is removed
	BoardState* prev; // previous board state
};

class Board {
	private:
		Side side; // side to move
		Piece board[SQUARE_NB]; // piece-by-square
		Bitboard byType[PIECE_TYPE_NB]; // piece-by-type (incl. ALL_PIECES)
		Bitboard bySide[SIDE_NB]; // piece-by-side
		BoardState orig_st; // original state
		BoardState* st; // current state
		
		void clear(void);
		void put_piece(PieceType pt, Side c, Square sq);
		void move_piece(PieceType pt, Side c, Square sq);
		void remove_piece(PieceType pt, Side c, Square sq);
	public:
		friend std::ostream& operator<<(std::ostream&, const Board& board);
		
		Board(void){ clear(); }
		Board(const Board& brd) = delete; // none of this
		Board& operator=(const Board& brd);
		
		void init_from(const std::string& fen); // init from FEN
		std::string fen(void); // get FEN
		
		// Pieces //
		bool side(void) const; // side to move
		Piece at(Square sq) const; // piece at square
		Bitboard all(void) const; // occupancy
		bool empty(Square sq) const; // test if square is empty
		Bitboard pieces(PieceType pt) const; // piece-by-type
		Bitboard pieces(PieceType pt1, PieceType pt2) const; // 2 piece-by-types meshed together
		Square king_sq(Side c) const; // get king square for side
		Square ep_sq(void) const; // get e.p. square if/a
		
		// Castling //
		int get_castling_rights(void) const; // get castling rights mask
		void set_castling_rights(CastlingRight cr); // set castling rights
		bool can_castle(void) const; // if anyone can castle
		
		// Checking //
		Bitboard checkers(void) const; // get pieces giving check
		Bitboard pinned(Side c) const; // get absolutely pinned pieces by side
		Bitboard lined(void) const; // get candidates for discover check
		
		// Attacks //
		Bitboard attacks_to(Square sq) const; // get all attacks to square (side-ind.)
		template<PieceType pt> Bitboard attacks_from(Square sq); // get attacks from square by piece type
		template<PieceType pt> Bitboard attacks_from(Side c, Square sq); // get attacks from square by piece type and side
		
		// Moves //
		Piece moved_piece(Move m) const; // piece moved
		bool legal(Move m) const;
		void do_move(Move m, BoardState& new_st); // do a move and get a new state
		void undo_move(Move m); // undo a move
		
		// Hashes //
		Key key(void) const; // Zobrist hash
		
		// Other //
		int get_ply(void){ return st->ply; }
		void set_ply(int to){ st->ply = to; }
};


































#endif // #ifndef BOARD_INC