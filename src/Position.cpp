#include "Common.h"
#include "Bitboards.h"
#include "Position.h"
#include <sstream>
#include <fstream>

Value PieceValue[PHASE_NB][PIECE_NB] = {
	{ VAL_ZERO, PawnValueMg, KnightValueMg, BishopValueMg, RookValueMg, QueenValueMg },
	{ VAL_ZERO, PawnValueEg, KnightValueEg, BishopValueEg, RookValueEg, QueenValueEg }
};

const std::string PieceChar(" PNBRQK  pnbrqk");

CheckInfo::CheckInfo(const Position& pos){
	Side them = ~pos.side_to_move();
	ksq = pos.king_sq(them);
	pinned = pos.pinned(pos.side_to_move());
	lined = pos.lined();
	kattk[PAWN] = pos.attacks_from<PAWN>(ksq, them);
	kattk[KNIGHT] = pos.attacks_from<KNIGHT>(ksq);
	kattk[BISHOP] = pos.attacks_from<BISHOP>(ksq);
	kattk[ROOK] = pos.attacks_from<ROOK>(ksq);
	kattk[QUEEN] = kattk[BISHOP] | kattk[ROOK];
	kattk[KING] = 0; // forget king moving as a king (since two kings can NEVER be in contact)
}

Position& Position::operator=(const Position& pos){
	std::memcpy(this, &pos, sizeof(Position));
	orig_state = *st; // since 'st' is uncorrupted
	st = &orig_state; // and finish the loop
	nodes = 0;
	return *this;
}

void Position::init(void){
	// TOOD: Zobrist keys, psq, pvalue, etc.
}

Bitboard Position::attackers_to(Square s, Bitboard occ) const {
	// Note: Side-independent.
	Bitboard r = attacks_from<PAWN>(s, BLACK) & pieces(WHITE, PAWN);
	r |= attacks_from<PAWN>(s, WHITE) & pieces(BLACK, PAWN);
	r |= attacks_from<KNIGHT>(s) & pieces(KNIGHT);
	r |= attacks_bb<ROOK>(s, occ) & pieces(ROOK, QUEEN);
	r |= attacks_bb<BISHOP>(s, occ) & pieces(BISHOP, QUEEN);
	r |= attacks_from<KING>(s) & pieces(KING);
	return r;
}

void Position::set_state(PosState* si){
	si->key = 0;
	si->npMaterial[WHITE] = si->npMaterial[BLACK] = VAL_ZERO;
	si->checkers = attackers_to(king_sq(to_move)) & pieces(~to_move);
	// TODO: Key, psq, etc.
	for(Side c = WHITE; c <= BLACK; c++){
		for(PieceType pt = KNIGHT; pt <= QUEEN; pt++){
			si->npMaterial[c] += pCount[c][pt] * PieceValue[MG][pt];
		}
	}
}

void Position::put_piece(Square s, Side c, PieceType pt){
	board[s] = make_piece(c, pt);
	byType[ALL_PIECES] |= s;
	byType[pt] |= s;
	bySide[c] |= s;
	index[s] = pCount[c][pt]++;
	pList[c][pt][index[s]] = s;
	pCount[c][ALL_PIECES]++;
}

void Position::move_piece(Square from, Square to, Side c, PieceType pt){
	Bitboard from_to = SquareBB[from] ^ SquareBB[to];
	byType[ALL_PIECES] ^= from_to;
	byType[pt] ^= from_to;
	bySide[c] ^= from_to;
	board[from] = NO_PIECE;
	board[to] = make_piece(c, pt);
	index[to] = index[from];
	pList[c][pt][index[to]] = to;
}

void Position::remove_piece(Square s, Side c, PieceType pt){
	byType[ALL_PIECES] ^= s;
	byType[pt] ^= s;
	bySide[c] ^= s;
	Square lastsq = pList[c][pt][--pCount[c][pt]];
	index[lastsq] = index[s];
	pList[c][pt][index[lastsq]] = lastsq;
	pList[c][pt][pCount[c][pt]] = SQ_NONE;
	pCount[c][ALL_PIECES]--;
}

void Position::clear(void){
	std::memset(this, 0, sizeof(Position)); // hehe
	orig_state.epsq = SQ_NONE;
	st = &orig_state;
    for(int i = 0; i < PIECE_TYPE_NB; i++){
    	for(int j = 0; j < 16; j++){
    		pList[WHITE][i][j] = pList[BLACK][i][j] = SQ_NONE;
    	}
    }
}

std::ostream& operator<<(std::ostream& ss, const Position& pos){
	ss << "\n  ---------------------------------\n";
	for(Rank r = RANK_8; r >= RANK_1; r--){
		ss << int(r + 1) << " ";
		ss << "|";
		for(File f = FILE_A; f <= FILE_H; f++){
			Piece p = pos.at(make_square(r, f));
			if(p != NO_PIECE) ss << ((side_of(p) == WHITE) ? (BOLDCYAN) : (BOLDRED));
			ss << " " << char(toupper(PieceChar[p])) << RESET << " |";
		}
		ss << "\n  ---------------------------------\n";
	}
	ss << "    ";
	for(char on = 'a'; on <= 'h'; on++) ss << on << "   ";
	ss << "\n";
	ss << "\nFEN: " << pos.fen();
	ss << "\nNo. Of Checkers: " << popcount<Full>(pos.checkers()) << std::endl;
	return (ss << "\n");
}

void Position::set_castling_right(Side c, Square rfrom){
	Square kfrom = king_sq(c);
	CastlingSide cs = (kfrom < rfrom ? KING_SIDE : QUEEN_SIDE); // since queen-side castling is to left and king-side to right
	CastlingRight cr = (c | cs);
	st->castling |= cr;
	castlingMask[kfrom] |= cr;
	castlingMask[rfrom] |= cr;
	castlingRookSq[cr] = rfrom;
	Square kto = relative_square(c, (cs == KING_SIDE ? SQ_G1 : SQ_C1)); // get relative king dest. square
	Square rto = relative_square(c, (cs == KING_SIDE ? SQ_F1 : SQ_D1)); // and rook dest. sq
	for(Square s = std::min(rfrom, rto); s <= std::max(rfrom, rto); s++){ // add in rook squares to castling path
		if((s != kfrom) && (s != rfrom)){
			castlingPath[cr] |= s;
		}
	}
	for(Square s = std::min(kfrom, kto); s <= std::max(kfrom, kto); s++){ // and king squares
		if((s != kfrom) && (s != rfrom)){
			castlingPath[cr] |= s;
		}
	}
}

void Position::init_from(const std::string& fen){
	// rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
	clear();
	std::istringstream ss(fen); // easy tokenizing
	ss >> std::noskipws; // don't skip spaces
	char tok;
	size_t idx;
	Square sq = SQ_A8;
	// Piece Placement //
	while((ss >> tok) && !isspace(tok)){
		if(isdigit(tok)){
			sq += Square(tok - '0');
		} else if(tok == '/'){
			sq -= Square(16); // otherwise known as decrementing the rank
		} else if((idx = PieceChar.find(tok)) != std::string::npos){
			put_piece(sq++, side_of(Piece(idx)), type_of(Piece(idx)));
		}
	}
	// Side To Move //
	ss >> tok;
	to_move = (tok == 'w' ? WHITE : BLACK);
	ss >> tok; // skip space
	// Castling Rights //
	while((ss >> tok) && !isspace(tok)){
		Side c = (islower(tok) ? BLACK : WHITE);
		tok = char(toupper(tok));
		Square rsq; // the rook square
		if(tok == 'K'){
			for(rsq = relative_square(c, SQ_H1); type_of(at(rsq)) != ROOK; rsq--) ; // for king-side
		} else if(tok == 'Q'){
			for(rsq = relative_square(c, SQ_A1); type_of(at(rsq)) != ROOK; rsq++) ; // for queen-side
		} else {
			assert(false && ("Bad char in castling rights!"));
		}
		set_castling_right(c, rsq);
	}
	// E.p. Square //
	ss >> tok;
	if(tok != '-'){
		char file = tok;
		char rank;
		ss >> rank;
		File f = File(file - 'a');
		Rank r = Rank(rank - '1');
		st->epsq = make_square(r, f);
		assert(attackers_to(st->epsq) & pieces(to_move, PAWN)); // since it is the next person's turn, his pawn attacks should reach the e.p. person's orig. pawn place
	}
	ss >> tok;
	assert(isspace(tok));
	// Halfmove + Fullmove counters //
	ss >> std::skipws;
	ss >> ply >> st->fifty_ct;
	ply = std::max(ply - 1, 0); // ply counter starts from 0
	set_state(st);
}

const std::string Position::fen(void) const {
	// rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
	std::stringstream ss;
	for(Rank r = RANK_8; r >= RANK_1; r--){
		int seq = 0; // sequential
		for(File f = FILE_A; f <= FILE_H; f++){
			Square sq = make_square(r, f);
			if(byType[ALL_PIECES] & sq){
				if(seq){
					ss << seq;
					seq = 0;
				}
				ss << PieceChar[board[sq]];
			} else ++seq;
		}
		if(seq){
			ss << seq;
		}
		if(r != RANK_1) ss << "/";
	}
	ss << " ";
	ss << ((to_move == WHITE) ? 'w' : 'b');
	ss << " ";
	if(st->castling & WHITE_OO) ss << 'K';
	if(st->castling & WHITE_OOO) ss << 'Q';
	if(st->castling & BLACK_OO) ss << 'k';
	if(st->castling & BLACK_OOO) ss << 'q';
	if(st->castling & ANY_CASTLING) ss << " ";
	if(ep_sq() == SQ_NONE) ss << "-";
	else {
		ss << char(file_of(st->epsq) + 'a');
		ss << char(rank_of(st->epsq) + '1');
	}
	ss << " " << st->fifty_ct << " " << ply;
	return ss.str();
}

Bitboard Position::check_blockers(Side c, Side kingC) const {
	// Pieces of side 'c' blocking check on given king
	Bitboard r = 0;
	Square ksq = king_sq(kingC);
	Bitboard pos = ((pieces(ROOK, QUEEN) & PseudoAttacks[ROOK][ksq]) | (pieces(BISHOP, QUEEN) & PseudoAttacks[BISHOP][ksq])) & bySide[~kingC];
	while(pos){
		Bitboard inb = between_bb(ksq, pop_lsb(&pos)) & pieces();
		if(!more_than_one(inb)){
			r |= inb & pieces(c);
		}
	}
	return r;
}

bool Position::pseudo_legal(const Move m) const {
	// Check if a given move is pseudo-legal (can be used to verify TT moves, etc.) //
	Square from = from_sq(m);
	Square to = to_sq(m);
	Piece pc = moved_piece(m);
	Side us = to_move;
	// TODO: Check for castling, promotion, and e.p. pseudo-legality
	if((promotion_type(m) - 2) != NO_PIECE_TYPE) return false;
	if((pc == NO_PIECE) || (side_of(pc) != us)) return false;
	if(pieces(us) & to) return false; // no taking own pieces
	if(type_of(pc) == PAWN){
		if(rank_of(to) == relative_rank(us, RANK_8)) return false; // cannot retain pawn-ness at this point
		// TODO: Check pushes
	} else if(!(attacks_from(pc, from) & to)) return false; // if the piece cannot go back (and is not a pawn) to where it came from, then invalid
	if(checkers()){
		if(type_of(pc) != KING){
			if(more_than_one(checkers())) return false; // double check - king *has* to move
			if(!(between_bb(lsb(checkers()), king_sq(us))) || !(checkers() & to)) return false; // must either block then or capture offending piece
		} else if(attackers_to(to, pieces() ^ from) & pieces(~us)) return false; // if we can still be attacked after moving, then invalid
	}
	return true;
}

bool Position::legal(Move m, Bitboard pinned) const {
	assert(is_ok(m));
	assert(pinned == this->pinned(to_move));
	Side us = to_move;
	Square from = from_sq(m);
	Square to = to_sq(m);
	assert(side_of(moved_piece(m)) == us);
	assert(at(king_sq(us)) == make_piece(us, KING));
	if(type_of(m) == ENPASSANT){
		// Note: Most testing was done in move generator.
		Square ksq = king_sq(us);
		Square capsq = to - pawn_push(us);
		Bitboard occ = (pieces() ^ from ^ capsq) | to; // w/o moving piece and cap.'d pawn, but with the place moved to (as if e.p. move was actually done)
		assert(to == ep_sq());
		assert(moved_piece(m) == make_piece(us, PAWN));
		assert(at(capsq) == make_piece(~us, PAWN));
		assert(at(to) == NO_PIECE);
		return !(attacks_bb<ROOK>(ksq, occ) & pieces(~us, ROOK, QUEEN)) && !(attacks_bb<BISHOP>(ksq, occ) & pieces(~us, BISHOP, QUEEN));
	}
	if(type_of(at(from)) == KING){
		return (type_of(m) == CASTLING || !(attackers_to(to) & pieces(~us))); // make sure king move is castle or that it is not attacked
		// Note: Castling legality is checked during move generation (e.g. crossing check, etc.)
	}
	return (!pinned) || !(pinned & from) || aligned(from, to, king_sq(us)); // make sure either nothing is pinned, we aren't moving the pinned piece, or that we are moving it in a line relative to the king
}

bool Position::gives_check(Move m, const CheckInfo& ci) const {
	// Tests if a given move gives check. //
	assert(is_ok(m));
	assert(side_of(moved_piece(m)) == to_move);
	Square from = from_sq(m);
	Square to = to_sq(m);
	PieceType pt = type_of(at(from));
	if(ci.kattk[pt] & to) return true; // if the king were the same piece, the two pieces can see each other
	if(ci.lined && (ci.lined & from) && !aligned(from, to, ci.ksq)) return true; // discover check
	switch(type_of(m)){
		case NORMAL: return false;
		case PROMOTION: return attacks_bb(Piece(promotion_type(m)), to, pieces() ^ from) & ci.ksq; // check for uncovered check on king
		case ENPASSANT:
			if(pt == PAWN){ // mostly for scope
				// Rare, but possible to have an e.p. give check. //
				Square capsq = make_square(rank_of(from), file_of(to));
				Bitboard occ = (pieces() ^ from ^ capsq) | to; // as if the e.p. move had been performed
				return !(attacks_bb<ROOK>(ci.ksq, occ) & pieces(to_move, ROOK, QUEEN)) && !(attacks_bb<BISHOP>(ci.ksq, occ) & pieces(to_move, BISHOP, QUEEN));
			} else assert(false);
		case CASTLING:
			if(pt == KING){ // for scope
				// We are only thinking about *giving* check, and the rook is the only one capable of that. //
				Square kfrom = from;
				Square rfrom = to;
				Square kto = relative_square(to_move, (rfrom > kfrom ? SQ_G1 : SQ_C1));
				Square rto = relative_square(to_move, (rfrom > kfrom ? SQ_F1 : SQ_D1));
				return (PseudoAttacks[ROOK][rto] & ci.ksq) && (attacks_bb<ROOK>(rto, (pieces() ^ kfrom ^ rfrom) | rto | kto) & ci.ksq); // if the rook gives check after this is done
			} else assert(false);
		default:
			assert(false);
	}
	return false;
}

void Position::do_move(Move m, PosState& new_st){
	CheckInfo ci(*this);
	do_move(m, new_st, ci, gives_check(m, ci));
}

void Position::do_move(Move m, PosState& new_st, const CheckInfo& ci, bool givesCheck){
	assert(is_ok(m));
	assert(&new_st != st); // don't want to be overwriting data, etc.
	// TODO
}







































