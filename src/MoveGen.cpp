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
std::string Moves::format<false>(Move m, Board& pos){
	return Moves::format<false>(m);
}

template<> 
std::string Moves::format<true>(Move m, Board& pos){
	std::string ret = "";
	Square from = from_sq(m), to = to_sq(m);
	Square sqs[3], others[2];
	if(type_of(m) == CASTLING){
		// NOTE: PGN uses the uppercase letter while FIDE SAN uses the digit zero.
		if(to < from) ret = "O-O-O"; // uppercase letter
		else ret = "O-O";
	} else {
		Piece pc = pos.moved_piece(m);
		if(pc == NO_PIECE) return "(invalid)";
		Side us = side_of(pc);
		const PieceType pt = type_of(pc);
		if(pt == PAWN){
			Square capsq = (type_of(m) == ENPASSANT) ? (to - pawn_push(pos.side_to_move())) : to;
			if(pos.empty(capsq)){ // if not a capture (or e.p.)
				// e.g. 'b4'
				ret += file_char_of(to);
				ret += rank_char_of(to);
			} else {
				// e.g. 'bxc3'
				ret += file_char_of(from);
				ret += 'x';
				ret += file_char_of(to);
				ret += rank_char_of(to);
			}
			if(type_of(m) == PROMOTION){
				ret += '=';
				ret += char(toupper(PieceChar[promotion_type(m)]));
			}
		} else {
			ret += char(toupper(PieceChar[pc]));
			Bitboard b = pos.attacks_from(pc, to) & pos.pieces(us, pt);
			if(more_than_one(b)){ // TODO: Simplify logic
				// OK, we have an ambiguous move.
				sqs[0] = pop_lsb(&b);
				sqs[1] = pop_lsb(&b);
				bool three = (b);
				sqs[2] = (three ? pop_lsb(&b) : SQ_NONE);
				assert(!b);
				assert(sqs[0] == from || sqs[1] == from || sqs[2] == from);
				assert(sqs[0] != sqs[1] && sqs[0] != sqs[2] && sqs[1] != sqs[2]);
				if(!three){
					// Then file/rank depending on where they differ.
					if(file_of(sqs[0]) != file_of(sqs[1])){
						ret += 'a' + file_of(from);
					} else {
						ret += '1' + rank_of(from);
					}
				} else {
					for(unsigned int i = 0, ct = 0; i < 3; i++){
						if(sqs[i] != from) others[ct++] = sqs[i];
					}
					bool file_conflict = (file_of(others[0]) == file_of(from) || file_of(others[1]) == file_of(from));
					bool rank_conflict = (rank_of(others[0]) == rank_of(from) || rank_of(others[1]) == rank_of(from));
					if(file_conflict && rank_conflict){
						ret += square_str_of(from); // have to put in entire square now
					} else if(file_conflict){
						ret += '1' + rank_of(from);
					} else if(rank_conflict){
						ret += 'a' + file_of(from);
					}
				}
			}
			if(!pos.empty(to)) ret += 'x';
			ret += file_char_of(to);
			ret += rank_char_of(to);
		}
	}
	// Add '+', '#' as needed. //
	BoardState st;
	pos.do_move(m, st);
	if(pos.checkers()){
		if(MoveList<LEGAL>(pos).size()) ret.push_back('+');
		else ret.push_back('#');
	}
	pos.undo_move(m);
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

template<>
Move Moves::parse<true>(std::string move, const Board& pos){
	if(move.length() < 2) return MOVE_NONE; // all SAN moves are at least 2 chars in length
	for(unsigned int i = 0; i < move.length(); i++) if(isspace(move[i])) return MOVE_NONE; // cannot be any spaces
	if(move.back() == '+' || move.back() == '#') move.pop_back(); // get rid of any basically annotation symbols (they are not very useful)
	const Side us = pos.side_to_move();
	if(move.length() == 2){
		// Must be a pawn advance (e.g. e3), and can be a double pawn advance.
		Rank r = Rank(move[1] - '1');
		File f = File(move[0] - 'a');
		Square to = make_square(r, f);
		Square sq = to - pawn_push(us);
		if(relative_rank(us, r) != RANK_4){ // must be a single pawn push
			return make_move(sq, to);
		}
		if(pos.empty(sq)) return make_move(sq - pawn_push(us), to);
		else return make_move(sq, to);
	} else if(islower(move[0]) && (move[1] == 'x')){
		// Must be a pawn capture (e.g. exd3), and can be an en passant capture or a promotion as well.
		File from = File(move[0] - 'a');
		Rank r = Rank(move[3] - '1');
		File f = File(move[2] - 'a');
		Square cap = make_square(r, f);
		Square fr = make_square(rank_of(cap - pawn_push(us)), from);
		if((cap == pos.ep_sq()) && (pos.attacks_from<PAWN>(cap, ~us) & pos.pieces(us, PAWN) & file_bb(from))){
			assert(pos.empty(cap));
			return make<ENPASSANT>(fr, cap);
		}
		if(!(pos.pieces(us, PAWN) & fr)) return MOVE_NONE; // or something went REALLY wrong
		if(move.find('=') != std::string::npos){
			// It must be a capture promotion (e.g. bxc8=Q)
			PieceType prom = NO_PIECE_TYPE;
			char l = move.back();
			if(l == 'N') prom = KNIGHT;
			else if(l == 'B') prom = BISHOP;
			else if(l == 'R') prom = ROOK;
			else if(l == 'Q') prom = QUEEN;
			else return MOVE_NONE; // invalid promotion piece char
			return make<PROMOTION>(fr, cap, prom);
		} else return make_move(fr, cap);
	} else if(isupper(move[0]) && (PieceChar.find(move[0]) != std::string::npos)){
		// Must be a piece move (like Nb3, Ra3d3, Qxc3, etc.)
		PieceType pt = PieceType(PieceChar.find(move[0]));
		if(move.length() == 3 || (move.length() == 4 && move[1] == 'x')){
			// Nice, simple case: something like Nb3 or Kxd2.
			unsigned int base = (move.length() == 3 ? 1 : 2); // base index
			Rank r = Rank(move[base + 1] - '1');
			File f = File(move[base] - 'a');
			Square to = make_square(r, f);
			Bitboard b = pos.attacks_from(make_piece(us, pt), to) & pos.pieces(us, pt);
			if(!b) return MOVE_NONE; // invalid
			if(more_than_one(b)){
				// This is the tiresome part of PGN parsing: no ambiguity resolving when there are
				// multiple pieces that can move to one square and one is absolutely pinned.
				// Time to use a legal move generator...
				for(MoveList<LEGAL> it(pos); *it; it++){
					PieceType moved = type_of(pos.moved_piece(*it));
					if(moved == pt && to_sq(*it) == to){
						return *it;
					}
				}
			}
			return make_move(lsb(b), to);
		} else {
			if(move[1] == 'x') return MOVE_NONE; // b/c length was not 4, or would have been caught by above branch
			if(move.length() < 4) return MOVE_NONE; // has to be at least 4 chars at this point
			File f = File(-1);
			Rank r = Rank(-1);
			Square from = SQ_NONE;
			unsigned int base = 2; // in "Naxa3", would be at 'x'
			if(isalpha(move[1])){
				// This should be the file of departure.
				f = File(move[1] - 'a');
				if(isdigit(move[2])){
					base = 3;
					// Then the entire square was given.
					r = Rank(move[2] - '1');
					from = make_square(r, f);
				}
			} else if(isdigit(move[1])){
				r = Rank(move[1] - '1');
			}
			if(move[base] == 'x'){
				++base;
			}
			if((base + 2) < move.length()) return MOVE_NONE; // not enough chars for to square
			Square to = make_square(Rank(move[base + 1] - '1'), File(move[base] - 'a'));
			if(from != SQ_NONE){
				return make_move(from, to); // literally the only easy case... thanks PGN standard
			}
			for(MoveList<LEGAL> it(pos); *it; it++){
				// Thanks to PGN standard, need to do this pretty much always...
				if(type_of(pos.moved_piece(*it)) == pt && to_sq(*it) == to){
					// Could be the move.
					if(int(r) != -1){ // if rank was given
						if(rank_of(from_sq(*it)) == r) return *it;
					} else { // if file was given
						assert(int(f) != -1);
						if(file_of(from_sq(*it)) == f) return *it;
					}
				}
			}
		}		
	} else if(islower(move[0]) && (move.find('=') != std::string::npos)){
		// Must be a pawn promotion by advancing then, since we already ruled out all pawn captures - e.p. included, and regular/double pushes
		Rank r = Rank(move[1] - '1');
		File f = File(move[0] - 'a');
		Square to = make_square(r, f);
		Square sq = to - pawn_push(us);
		char l = move.back();
		if(PieceChar.find(l) == std::string::npos) return MOVE_NONE; // invalid promotion piece type
		PieceType prom = PieceType(PieceChar.find(l));
		return make<PROMOTION>(sq, to, prom);
	} else if((move[0] == '0' || move[0] == 'O') && (move.find('-') != std::string::npos)){
		// Must be castling - either SAN or PGN format (O-O, 0-0, O-O-O, or 0-0-0)
		Square ksq = make_square(relative_rank(us, RANK_1), FILE_E); // where the king should be
		Square rsq = SQ_NONE;
		if(move == "O-O" || move == "0-0"){
			// King-side castling.
			rsq = make_square(relative_rank(us, RANK_1), FILE_H);
		} else if(move == "O-O-O" || move == "0-0-0"){
			// Queen-side castling.
			rsq = make_square(relative_rank(us, RANK_1), FILE_A);
		} else return MOVE_NONE;
		return make<CASTLING>(ksq, rsq);
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
			possibs &= ~pos.all(); // *quiet* checks
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



































































