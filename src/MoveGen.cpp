#include "Common.h"
#include "Bitboards.h"
#include "Board.h"
#include "MoveGen.h"

void Moves::init(void){
	// TODO: Init tables for scoring, history tables, etc.
}

template<> 
std::string Moves::format<false>(Move m){
	std::string ret = "";
	Square from = from_sq(m), to = to_sq(m);
	// From //
	ret += file_char_of(from);
	ret += rank_char_of(from);
	if(type_of(m) == CASTLING){
		// We will display castling as only the king movement, though it
		// is encoded as king --> castling rook.
		int file_dist = distance<File>(from, to); // file distance b/w king and rook
		int dist = int(to) - int(from);
		int avg = dist / file_dist; // the delta basically
		ret += file_char_of(Square(from + (2 * avg)));
		ret += rank_char_of(Square(from + (2 * avg)));
		return ret;
	}
	// To //
	ret += file_char_of(to);
	ret += rank_char_of(to);
	if(type_of(m) == PROMOTION) ret += char(tolower(PieceChar[promotion_type(m)]));
	return ret;
}

template<>
Move Moves::parse<false>(std::string move, const Board& pos){
	if(move.length() < 4 || move.length() > 5) return MOVE_NONE;
	Rank r1, r2;
	File f1, f2;
	Square from, to;
	PieceType prom = NO_PIECE_TYPE;
	f1 = File(move[0] - 'a'), f2 = File(move[2] - 'a');
	r1 = Rank(move[1] - '1'), r2 = Rank(move[3] - '1');
	from = make_square(r1, f1), to = make_square(r2, f2);
	Move m = make_move(from, to);
	if(move.length() == 5){
		char pr = move[4];
		if(pr == 'b') prom = BISHOP;
		else if(pr == 'n') prom = KNIGHT;
		else if(pr == 'r') prom = ROOK;
		else if(pr == 'q') prom = QUEEN;
		m = make<PROMOTION>(from, to, prom);
	}
	for(MoveList<LEGAL> it(pos); *it; it++){
		if(type_of(*it) == CASTLING){
			Square f = from_sq(*it), t = to_sq(*it);
			// For castling, the user might enter "e1g1" or such. //
			Square kto = (t < f) ? (f - Square(2)) : (f + Square(2));
			if((f == from) && (to == kto)){
				return *it;
			}
		} else if(type_of(*it) == NORMAL || type_of(*it) == PROMOTION){
			if(*it == m) return *it;
		} else if(type_of(*it) == ENPASSANT){
			if(from_sq(m) == from_sq(*it) && to_sq(m) == to_sq(*it)){
				return *it;
			}
		}
	}
	return MOVE_NONE;
}

// inline Move make_move(Square from, Square to)
// template<MoveType T> inline Move make(Square from, Square to, PieceType pt = KNIGHT)

template<GenType T>
ActMove* generate_sliders_knight(const Board& pos, ActMove* list, Side us){
	// This generates all slider moves and knight moves as specified by GenType. //
	Bitboard attks[PIECE_TYPE_NB - 1];
	if(T == QUIET_CHECKS){
		const Square ksq = pos.king_sq(~us); // their king
		const Bitboard exc = ~pos.all(); // since they are *quiet* checks
		attks[ROOK] = pos.attacks_from<ROOK>(ksq) & exc;
		attks[BISHOP] = pos.attacks_from<BISHOP>(ksq) & exc;
		attks[KNIGHT] = pos.attacks_from<KNIGHT>(ksq) & exc;
		attks[QUEEN] = attks[ROOK] | attks[BISHOP];
	}
	const Square ksq = pos.king_sq(us);
	Bitboard pcs = pos.pieces(us, KNIGHT, BISHOP) | pos.pieces(us, ROOK, QUEEN);
	const Square chksq = (T == EVASIONS) ? lsb(pos.checkers()) : SQ_NONE;
	while(pcs){
		Square from = pop_lsb(&pcs);
		Piece on = pos.at(from);
		PieceType pt = type_of(on);
		Bitboard possibs = pos.attacks_from(on, from) & (~pos.pieces(us)) & ~pos.pieces(KING); // don't want to take our own pieces
		if(T == CAPTURES){
			possibs &= pos.pieces(~us); // have to capture
		} else if(T == NON_CAPTURES){
			possibs &= ~pos.all(); // can't capture anything
		} else if(T == QUIET_CHECKS){
			possibs &= attks[pt]; // only the squares that it gives check on
		}
		while(possibs){
			Square to = pop_lsb(&possibs);
			if((T != EVASIONS) || (to == chksq) || (between_bb(ksq, chksq) & to)){
				(list++)->move = make_move(from, to);
			}
		}
	}
	return list;
}

template<GenType T>
ActMove* generate_king(const Board& pos, ActMove* list, Side us){
	// This generates all king moves (other than castling). //
	assert(T != QUIET_CHECKS);
	Square from = pos.king_sq(us);
	Bitboard possibs = pos.attacks_from<KING>(from) & (~pos.pieces(us));
	if(T == EVASIONS){
		Bitboard chks = pos.checkers();
		while(chks){ // just quickly and cheaply rule out spots under check
			possibs &= ~(between_bb(from, pop_lsb(&chks)));
		}
	} else if(T == CAPTURES){
		possibs &= pos.pieces(~us); // have to capture
	} else if(T == NON_CAPTURES){
		possibs &= ~pos.all(); // no captures, period
	}
	while(possibs){
		Square to = pop_lsb(&possibs);
		(list++)->move = make_move(from, to);
	}
	return list;
}

template<GenType T>
ActMove* generate_castles(const Board& pos, ActMove* list, Side us){
	assert((T != EVASIONS) && (T != CAPTURES)); // a castling move cannot be an evasion since we cannot castle under check and cannot capture anything
	const Square ksq = pos.king_sq(us), tksq = (T == QUIET_CHECKS) ? pos.king_sq(~us) : SQ_NONE;
	for(CastlingRight on = WHITE_OO; on <= BLACK_OOO; on = CastlingRight(on << 1)){
		if(pos.can_castle(on)){
			Bitboard path = pos.castling_path(on);
			if(!(path & pos.all())){ // check if nothing between rook and king
				path = pos.king_castling_path(on); // now make sure we are not castling through/into check
				bool passed = true;
				while(path){
					if((pos.attackers_to(pop_lsb(&path), pos.all()) & pos.pieces(~us))){ // found a check - no longer legal
						passed = false;
						break;
					}
				}
				if(passed){
					const Square rfrom = pos.castling_rook_sq(on);
					const Square kto = (rfrom < ksq) ? (ksq - Square(2)) : (ksq + Square(2));
					if((T != QUIET_CHECKS) || (pos.attacks_from<ROOK>(tksq) & ((ksq + kto) / 2))){
						(list++)->move = make<CASTLING>(ksq, rfrom); // castling is encoded as "king captures rook"
					}
				}
			}
		}
	}
	return list;
}

template<GenType T>
ActMove* generate_pawns(const Board& pos, ActMove* list, Side us){
	// This generates all pawn moves and knight moves as specified by GenType. //
	// Note: There should not be a double-check.
	Bitboard pcs = pos.pieces(us, PAWN);
	const Square delta = pawn_push(us);
	const Square ep = pos.ep_sq();
	const Square ksq = pos.king_sq(us), tksq = pos.king_sq(~us);
	const Square chksq = (T == EVASIONS) ? lsb(pos.checkers()) : SQ_NONE;
	const Bitboard ep_mask = (ep != SQ_NONE) ? SquareBB[ep] : 0;
	while(pcs){
		Square from = pop_lsb(&pcs);
		const Bitboard orig = SquareBB[from];
		// First, generate pawn captures and en passants. //
		Bitboard possibs = 0;
		if((T != NON_CAPTURES) && (T != QUIET_CHECKS)){ // since these are always captures
			possibs |= pos.attacks_from<PAWN>(from, us) & (pos.pieces(~us) | ep_mask) & (~pos.pieces(KING)); // can only be a capture or an e.p.
		}
		// Then, generate pawn pushes (single and double). //
		if(T != CAPTURES){ // since pawn pushes cannot be captures
			Bitboard push = shift_bb(orig, delta) & ~pos.all(); // single push
			if(relative_rank(us, from) == RANK_2){ // double push
				push |= shift_bb(push, delta) & ~pos.all(); // shifts 'push' so if no single push was possible, neither will any double push be
			}
			assert(!(push & pos.all()));
			possibs |= push;
		}
		if(T == QUIET_CHECKS){
			possibs &= pos.attacks_from<PAWN>(tksq, ~us) | (pos.attacks_from<KNIGHT>(tksq) & rank_bb(relative_rank(us, RANK_8))); // have to give them check (via direct or knight underpromotion)
		}
		// And, fill up the move list. //
		while(possibs){
			Square to = pop_lsb(&possibs);
			Square to_cap = (to == ep) ? (to - delta) : (to);
			if((T != EVASIONS) || (between_bb(ksq, chksq) & to) || (to_cap == chksq)){ // if we are blocking/taking the checking piece (note: knight checker is handled since between_bb(...) would be 0)
				if(relative_rank(us, to) <= RANK_7){
					if(to != ep){
						// Pawn (double) push/capture.
						(list++)->move = make_move(from, to);
					} else {
						// En passant.
						if(relative_rank(us, to) != RANK_6){
							std::cout << "Rank: " << int(relative_rank(us, to)) << std::endl;
							assert(relative_rank(us, to) == RANK_6);
						}
						(list++)->move = make<ENPASSANT>(from, to);
					}
				} else {
					// OK, it looks like we got a pawn promoted!
					// Note: It is impossible for a promotion to be an e.p.
					(list++)->move = make<PROMOTION>(from, to, KNIGHT);
					(list++)->move = make<PROMOTION>(from, to, BISHOP);
					(list++)->move = make<PROMOTION>(from, to, ROOK);
					(list++)->move = make<PROMOTION>(from, to, QUEEN);
				}
			}
		}
	}
	return list;
}

template<GenType T> ActMove* generate_evasions(const Board& pos, ActMove* list, Side us);

template<>
ActMove* generate_evasions<EVASIONS>(const Board& pos, ActMove* list, Side us){
	const Bitboard chk = pos.checkers();
	if(more_than_one(chk)){
		// Double check - king *has* to move
		list = generate_king<EVASIONS>(pos, list, us);
		return list;
	}
	list = generate_sliders_knight<EVASIONS>(pos, list, us);
	list = generate_king<EVASIONS>(pos, list, us);
	list = generate_pawns<EVASIONS>(pos, list, us);
	// Castles are not allowed under check anyway.
	return list;
}
	

// TODO: Perft, evasions, captures, quiets, etc.

template<>
ActMove* generate_moves<NON_EVASIONS>(const Board& pos, ActMove* list){
	Side us = pos.side_to_move();
	list = generate_sliders_knight<NON_EVASIONS>(pos, list, us);
	list = generate_king<NON_EVASIONS>(pos, list, us);
	list = generate_pawns<NON_EVASIONS>(pos, list, us);
	list = generate_castles<NON_EVASIONS>(pos, list, us);
	return list;
}

template<>
ActMove* generate_moves<EVASIONS>(const Board& pos, ActMove* list){
	Side us = pos.side_to_move();
	list = generate_evasions<EVASIONS>(pos, list, us);
	return list;
}

template<>
ActMove* generate_moves<CAPTURES>(const Board& pos, ActMove* list){
	Side us = pos.side_to_move();
	list = generate_sliders_knight<CAPTURES>(pos, list, us);
	list = generate_king<CAPTURES>(pos, list, us);
	list = generate_pawns<CAPTURES>(pos, list, us);
	return list;
}

template<>
ActMove* generate_moves<NON_CAPTURES>(const Board& pos, ActMove* list){
	Side us = pos.side_to_move();
	list = generate_sliders_knight<NON_CAPTURES>(pos, list, us);
	list = generate_king<NON_CAPTURES>(pos, list, us);
	list = generate_pawns<NON_CAPTURES>(pos, list, us);
	list = generate_castles<NON_CAPTURES>(pos, list, us);
	return list;
}

template<>
ActMove* generate_moves<QUIET_CHECKS>(const Board& pos, ActMove* list){
	Side us = pos.side_to_move();
	list = generate_sliders_knight<QUIET_CHECKS>(pos, list, us);
	list = generate_pawns<QUIET_CHECKS>(pos, list, us);
	list = generate_castles<QUIET_CHECKS>(pos, list, us);
	return list;
}

template<>
ActMove* generate_moves<LEGAL>(const Board& pos, ActMove* list){
	// This generates all *legal* moves, which are verified to be legal and not just pseudo-legal. //
	Bitboard pinned = pos.pinned(pos.side_to_move());
	const Square ksq = pos.king_sq(pos.side_to_move());
	ActMove* on = list; // start of list
	list = (pos.checkers() ? generate_moves<EVASIONS>(pos, list) : generate_moves<NON_EVASIONS>(pos, list)); // now points to end of list
	while(on != list){ // as long as we aren't at the end
		if((pinned || (from_sq(on->move) == ksq) || (type_of(on->move) == ENPASSANT)) && !pos.legal(on->move, pinned)){
			on->move = (--list)->move; // take it from the end and overwrite current invalid one without incrementing so we visit this
		} else ++on;
	}
	return list;
}



































































