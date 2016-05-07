#include "Common.h"
#include "Bitboards.h"
#include "Board.h"
#include "MoveGen.h"
#include "Evaluation.h"

Value PieceValue[PHASE_NB][PIECE_TYPE_NB] = {
	{ VAL_ZERO, PawnValueMg, KnightValueMg, BishopValueMg, RookValueMg, QueenValueMg },
	{ VAL_ZERO, PawnValueEg, KnightValueEg, BishopValueEg, RookValueEg, QueenValueEg } 
}; // material value by piece

namespace Hashing {
	// For hashing purposes. //
	Key psq[SIDE_NB][PIECE_TYPE_NB][SQUARE_NB]; // hash keys by [side][piece type][square]
	Key enp[FILE_NB]; // en passant by [file]
	Key castling[CASTLING_RIGHT_NB]; // castling rights by [mask of castling rights]
	Key side; // xor'd in/out whenever the side to move changes
}

const std::string PieceChar(" PNBRQK  pnbrqk");

void Board::clear(void){
	std::memset(this, 0, sizeof(Board));
	orig_st.epsq = SQ_NONE;
	st = &orig_st;
}

void Board::init(void){
	RNG rng(148957);
	for(Side c = WHITE; c <= BLACK; c++){
		for(PieceType pt = PAWN; pt <= KING; pt++){
			for(Square s = SQ_A1; s <= SQ_H8; s++){
				Hashing::psq[c][pt][s] = rng.rand<Key>();
			}
		}
	}
	for(File f = FILE_A; f <= FILE_H; f++){
		Hashing::enp[f] = rng.rand<Key>();
	}
	for(CastlingRight cr = NO_CASTLING; cr <= ANY_CASTLING; cr++){
		Hashing::castling[cr] = rng.rand<Key>();
	}
	Hashing::side = rng.rand<Key>();
}

Board& Board::operator=(const Board& pos){
	std::memcpy(this, &pos, sizeof(Board));
	orig_st = *st;
	st = &orig_st;
	return *this;
}

Bitboard Board::attackers_to(Square s, Bitboard occ) const {
	return (attacks_from<PAWN>(s, BLACK) & pieces(WHITE, PAWN)) |
		   (attacks_from<PAWN>(s, WHITE) & pieces(BLACK, PAWN)) |
		   (attacks_from<KNIGHT>(s) & pieces(KNIGHT)) |
		   (attacks_bb<ROOK>(s, occ) & pieces(ROOK, QUEEN)) |
		   (attacks_bb<BISHOP>(s, occ) & pieces(BISHOP, QUEEN)) |
		   (attacks_from<KING>(s) & pieces(KING));
}

Bitboard Board::check_blockers(Side c, Side kc) const {
	Square ksq = king_sq(kc);
	Bitboard pinners = ((PseudoAttacks[ROOK][ksq] & pieces(ROOK, QUEEN)) | (PseudoAttacks[BISHOP][ksq] & pieces(BISHOP, QUEEN))) & pieces(~kc);
	Bitboard ret = 0, b;
	while(pinners){
		Square on = pop_lsb(&pinners);
		b = between_bb(on, ksq) & all(); // pieces blocking check
		if(!more_than_one(b)){ // otherwise its not useful
			ret |= b & pieces(c); // we get the ones of the specified color
		}
	}
	return ret;
}

void Board::put_piece(PieceType pt, Side c, Square sq){
	Piece p = make_piece(c, pt);
	board[sq] = p;
	byType[ALL_PIECES] |= sq;
	byType[pt] |= sq;
	bySide[c] |= sq;
	pCount[c][pt]++;
	pCount[c][ALL_PIECES]++;
}

void Board::move_piece(PieceType pt, Side c, Square from, Square to){
	Piece p = make_piece(c, pt);
	board[from] = NO_PIECE;
	board[to] = p;
	byType[pt] ^= SquareBB[from] | to; // remove 'from', add 'to'
	byType[ALL_PIECES] ^= SquareBB[from] | to; // same here
	bySide[c] ^= SquareBB[from] | to; // and same here
}

void Board::remove_piece(PieceType pt, Side c, Square sq){
	board[sq] = NO_PIECE;
	byType[ALL_PIECES] ^= sq; // remove 'sq'
	byType[pt] ^= sq; // same here
	bySide[c] ^= sq; // and here
	pCount[c][pt]--;
	pCount[c][ALL_PIECES]--;
}

CheckInfo::CheckInfo(const Board& pos){
	ksq = pos.king_sq(~pos.side_to_move());
	kattk[PAWN] = pos.attacks_from<PAWN>(ksq, ~pos.side_to_move());
	kattk[KNIGHT] = pos.attacks_from<KNIGHT>(ksq);
	kattk[BISHOP] = pos.attacks_from<BISHOP>(ksq);
	kattk[ROOK] = pos.attacks_from<ROOK>(ksq);
	kattk[QUEEN] = kattk[BISHOP] | kattk[ROOK];
	kattk[KING] = pos.attacks_from<KING>(ksq);
}

void Board::update_state(BoardState* st){
	// Should *only* be used when setting up the board. //
	assert(st != NULL);
	assert(st == &orig_st);
	st->key = st->pawn_key = st->material_key = 0;
	st->capd = NO_PIECE_TYPE;
	Square ksq = king_sq(to_move);
	st->checkers = attackers_to(ksq, byType[ALL_PIECES]) & pieces(~to_move);
	st->pinned = pinned(to_move);
	st->lined = lined(to_move);
	// And now for regenerating hash keys. //
	// First, Zobrist hash for entire board. //
	for(Bitboard b = all(); b; ){
		Square s = pop_lsb(&b);
		Piece pc = at(s);
		st->key ^= Hashing::psq[side_of(pc)][type_of(pc)][s];
	}
	if(ep_sq() != SQ_NONE) st->key ^= Hashing::enp[file_of(ep_sq())];
	st->key ^= Hashing::castling[st->castling];
	if(to_move == BLACK) st->key ^= Hashing::side;
	// Then, Zobrist hash for only pawns. //
	for(Bitboard b = pieces(PAWN); b; ){
		Square s = pop_lsb(&b);
		Piece pc = at(s);
		st->pawn_key ^= Hashing::psq[side_of(pc)][PAWN][s];
	}
	// And finally, Zobrist hash for material regardless of location. //
	for(Side c = WHITE; c <= BLACK; c++){
		for(PieceType pt = PAWN; pt <= KING; pt++){
			for(int i = 0; i < pCount[c][pt]; i++){
				// Since the number of pieces of one type can obviously never
				// exceed 64, we can do this.
				st->material_key ^= Hashing::psq[c][pt][i];
			}
		}
	}
}

const Bitboard castling_paths[9] = { // get castling paths for *individual* castling types (e.g. only WHITE_OOO set)
	0,
	0ULL | (1ULL << SQ_F1) | (1ULL << SQ_G1), /* WHITE_OO - 1 */
	0ULL | (1ULL << SQ_B1) | (1ULL << SQ_C1) | (1ULL << SQ_D1), /* WHITE_OOO - 2 */
	0,
	0ULL | (1ULL << SQ_F8) | (1ULL << SQ_G8), /* BLACK_OO - 4 */
	0, 0, 0,
	0ULL | (1ULL << SQ_B8) | (1ULL << SQ_C8) | (1ULL << SQ_D8) /* BLACK_OOO - 8 */
};

const Bitboard king_castling_paths[9] = { // squares the *king* passes through (incl. destination) for castling by type
	0,
	0ULL | (1ULL << SQ_F1) | (1ULL << SQ_G1), /* WHITE_OO - 1 */
	0ULL | (1ULL << SQ_C1) | (1ULL << SQ_D1), /* WHITE_OOO - 2 */
	0,
	0ULL | (1ULL << SQ_F8) | (1ULL << SQ_G8), /* BLACK_OO - 4 */
	0, 0, 0,
	0ULL | (1ULL << SQ_C8) | (1ULL << SQ_D8) /* BLACK_OOO - 8 */
};

const Square castling_rooks[9] = { // get castling rook square
	SQ_NONE,
	SQ_H1, // WHITE_OO
	SQ_A1, // WHITE_OOO
	SQ_NONE,
	SQ_H8, // BLACK_OO
	SQ_NONE, SQ_NONE, SQ_NONE,
	SQ_A8 // BLACK_OOO
};

Bitboard Board::castling_path(CastlingRight cr) const {
	assert(!more_than_one(Bitboard(cr)));
	return castling_paths[cr];
}

Bitboard Board::king_castling_path(CastlingRight cr) const {
	assert(!more_than_one(Bitboard(cr)));
	return king_castling_paths[cr];
}

Square Board::castling_rook_sq(CastlingRight cr) const {
	assert(!more_than_one(Bitboard(cr)));
	return castling_rooks[cr];
}

std::ostream& operator<<(std::ostream& ss, const Board& pos){
	ss << "\n  ---------------------------------\n";
	for(Rank r = RANK_8; r >= RANK_1; r--){
		ss << int(r + 1) << " |";
		for(File f = FILE_A; f <= FILE_H; f++){
			Square on = make_square(r, f);
			Piece at = pos.at(on);
			if(at != NO_PIECE) ss << ((side_of(at) == WHITE) ? BOLDCYAN : BOLDRED);
			ss << " " << char(toupper(PieceChar[at])) << RESET << " |";
		}
		ss << "\n  ---------------------------------\n";
	}
	ss << "    ";
	for(char on = 'a'; on <= 'h'; on++) ss << on << "   ";
	ss << "\n\n";
	ss << (pos.side_to_move() == WHITE ? "white" : "black") << " to move, e.p. square = ";
	if(pos.ep_sq() == SQ_NONE) ss << "-";
	else ss << pos.ep_sq();
	ss << "\n";
	// FEN //
	ss << "FEN: " << pos.fen() << std::endl;
	// Checkers //
	ss << "checkers: ";
	Bitboard ch = pos.checkers();
	if(!ch) ss << "none";
	else {
		while(ch) ss << pop_lsb(&ch) << " ";
	}
	ss << "\n";
	// Pinned Pieces //
	ss << "pinned: ";
	ch = pos.pinned(pos.side_to_move());
	if(!ch) ss << "none";
	else {
		while(ch) ss << pop_lsb(&ch) << " ";
	}
	ss << "\n";
	// Lined (D.C. Candidates) Pieces //
	ss << "lined: ";
	ch = pos.lined(pos.side_to_move());
	if(!ch) ss << "none";
	else {
		while(ch) ss << pop_lsb(&ch) << " ";
	}
	ss << "\n";
	// Evaluation //
	ss << "evaluation: " << Eval::to_cp(Eval::evaluate_verbose(pos)) << "\n";
	ss << "board hash: " << pos.key() << "\n";
	ss << "pawn hash: " << pos.pawn_key() << "\n";
	ss << "material hash: " << pos.material_key() << "\n";
	return (ss << "\n");
}

const char* Board::get_representation(void){
	std::stringstream ss;
	ss << *this;
	return strdup(ss.str().c_str());
}

void Board::init_from(const std::string& fen){
	// rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
	clear();
	std::istringstream ss(fen);
	ss >> std::noskipws;
	char tok;
	size_t idx;
	// Pieces //
	Square sq = SQ_A8;
	while((ss >> tok) && !isspace(tok)){
		if(isdigit(tok)){ 
			sq += Square(tok - '0');
		} else if(tok == '/'){
			// For example, after completing the back rank, we are at SQ_H8 + 1 (since it was incremented)
			// and we need to get to SQ_A7, so we need to get from 64 to 48, and as such, subtract 16.
			sq -= Square(16);
		} else if((idx = PieceChar.find(tok)) != std::string::npos){
			Piece p = Piece(idx);
			put_piece(type_of(p), side_of(p), sq);
			++sq;
		} else assert(false);
	}
	// STM //
	ss >> tok;
	to_move = (tok == 'w' ? WHITE : BLACK);
	ss >> tok; // skip space
	// Castling Rights //
	int cast = 0;
	while((ss >> tok) && !isspace(tok)){
		if(tok == 'K') cast |= WHITE_OO;
		else if(tok == 'Q') cast |= WHITE_OOO;
		else if(tok == 'k') cast |= BLACK_OO;
		else if(tok == 'q') cast |= BLACK_OOO;
		else if(tok == '-'){
			ss >> tok; // consume
			assert(isspace(tok));
			break;
		} else assert(false && ("Bad FEN castling string!"));
	}
	st->castling = cast;
	// E.p. Square //
	ss >> tok;
	if(tok != '-'){
		char file = tok;
		ss >> tok;
		char rank = tok;
		File f = File(file - 'a');
		Rank r = Rank(rank - '1');
		st->epsq = make_square(r, f);
	}
	ss >> std::skipws;
	// Halfmove, Ply counter //
	ss >> st->fifty_ct >> st->ply;
	st->ply = std::max(st->ply - 1, 0); // starts from 0, fix if bad FEN is given with counter = 0
	st->fifty_ct = std::max(std::min(st->fifty_ct, 99), 0); // fix halfmove counter
	// Update State //
	update_state(st);
	st->prev = NULL;
}

std::string Board::fen(void) const {
	// rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
	std::stringstream ss;
	// Pieces //
	for(Rank r = RANK_8; r >= RANK_1; r--){
		int seq = 0;
		for(File f = FILE_A; f <= FILE_H; f++){
			Square sq = make_square(r, f);
			Piece on = at(sq);
			if(on == NO_PIECE) ++seq;
			else {
				if(seq){
					ss << seq;
					seq = 0;
				}
				ss << PieceChar[on];
			}
		}
		if(seq) ss << seq;
		if(r > RANK_1) ss << "/";
	}
	// STM //
	ss << " " << (to_move == WHITE ? 'w' : 'b') << " ";
	// Castling Rights ///
	if(can_castle(WHITE_OO)) ss << 'K';
	if(can_castle(WHITE_OOO)) ss << 'Q';
	if(can_castle(BLACK_OO)) ss << 'k';
	if(can_castle(BLACK_OOO)) ss << 'q';
	if(can_castle()) ss << " "; // extra space if anyone can castle
	else ss << "- ";
	// E.p. Square //
	if(st->epsq == SQ_NONE) ss << "-";
	else ss << st->epsq;
	ss << " ";
	// Full/Halfmove Counters //
	ss << st->fifty_ct << " " << (st->ply + 1);
	return ss.str();
}

void Board::init_from(const char* s){
	init_from(s);
}

bool Board::is_draw(void) const {
	// Fifty-Move Rule //
	if(st->fifty_ct > 99 && (!checkers() || MoveList<LEGAL>(*this).size())){ 
		// Note: Using MoveList size is OK since it is unlikely enough that
		// the halfmove count is above 99 anyway, so it is a rare case.
		return true; // drawn by 50-move rule
	}
	// Draw by Repetition //
	BoardState* stp = st; // will be used to store previous states
	for(int i = 2, e = std::min(st->fifty_ct, st->ply); i <= e; i += 2){
		// Can only be the exact same during the same person's move, hence
		// the i += 2 above.
		if(!stp->prev) break;
		stp = stp->prev->prev; // find the one 1 full move ago
		if(!stp) break;
		if(stp->key == st->key){
			return true; // Drawn at first repetition
		}
	}
	// Draw by insufficient material //
	const Bitboard pcs = all();
	if(byType[PAWN] | byType[ROOK] | byType[QUEEN]){
		return false; // have mating material - early return
	}
	if(pcs == byType[KING]){
		return true; // King vs King
	}
	if((pCount[WHITE][ALL_PIECES] + pCount[BLACK][ALL_PIECES]) < 4){
		if((byType[BISHOP] | byType[KNIGHT]) & pcs){
			return true; // King and one minor piece
		}
	}
	return false;
}

bool Board::legal(Move m, Bitboard pinned) const {
	// Checks if a given move 'm' is legal. //
	// Note: Only checks for leaving king in
	// check, pseudo-legality is assumed.
	// TODO: Separate pseudo-legality checker.
	assert(is_ok(m));
	Side us = to_move;
	const Square from = from_sq(m), to = to_sq(m);
	Piece pc = moved_piece(m);
	PieceType pt = type_of(pc);
	if(type_of(m) == ENPASSANT){
		const Square ksq = king_sq(us);
		const Square ep_pawn = to - pawn_push(us); // find their pawn that did the double push
		assert(to == ep_sq());
		assert(relative_rank(us, to) == RANK_6);
		if(!empty(to)){
			std::cout << *this;
			assert(empty(to));
		}
		Bitboard occ = (all() ^ from ^ ep_pawn) | to; // as if we had did the move
		return !(attacks_bb<ROOK>(ksq, occ) & pieces(~us, ROOK, QUEEN)) && !(attacks_bb<BISHOP>(ksq, occ) & pieces(~us, BISHOP, QUEEN));
	}
	if(pt != KING){
		if(!pinned) return true; // no pinned pieces, king did not move, we're good
		if(!(pinned & from)) return true; // pinned piece(s) did not move
		if(aligned(from, to, king_sq(us))) return true; // just moved in a straight line from the king - did not reveal any checks
		return false; // revealed check
	}
	return (type_of(m) == CASTLING) || !(attackers_to(to, (all() ^ from ^ to)) & pieces(~us)); // castling is already legal when generated
}

bool Board::gives_check(Move m, CheckInfo& ci) const {
	const Square from = from_sq(m), to = to_sq(m);
	assert(is_ok(from));
	assert(is_ok(to));
	PieceType pt = type_of(moved_piece(m));
	if(ci.kattk[pt] & to){
		return true;
	}
	if(st->lined && (st->lined & from) && !aligned(from, to, ci.ksq)) return true; // discover check
	const MoveType type = type_of(m);
	if(type == NORMAL) return false;
	else if(type == PROMOTION){
		// OK, the promoted piece can give check since we have already ruled out any discover
		// or direct checks.
		return (attacks_bb(Piece(promotion_type(m)), to, all() ^ from) & ci.ksq);
	} else if(type == ENPASSANT){
		// So, the only undetected case would be if the captured pawn itself had been pinned. //
		Square capsq = to - pawn_push(to_move);
		Bitboard occ = (all() ^ from ^ capsq) | to;
		return (attacks_bb<ROOK>(ci.ksq, occ) & pieces(to_move, ROOK, QUEEN)) | (attacks_bb<BISHOP>(ci.ksq, occ) & pieces(to_move, BISHOP, QUEEN));
	} else if(type == CASTLING){
		// Only the rook can plausibly give check (king cannot since kings can't be in contact). //
		Square kto = (to < from) ? (from - Square(2)) : (from + Square(2));
		Square rto = (from + kto) / 2;
		return attacks_bb<ROOK>(ci.ksq, (all() ^ from ^ to) | rto | kto) & rto;
	} else assert(false);
}

void Board::do_move(Move m, BoardState& new_st){
	assert(is_ok(m));
	assert(st != &new_st); // or we'll have a problem
	// Save checking info //
	CheckInfo ci(*this);
	bool is_checking = gives_check(m, ci);
	const Bitboard our_lined = st->lined; // for *our* discover checks
	// Get the new state set up //
	Key key = st->key; // save current Zobrist key (so we can modify this, and eventually set the current one to this)
	const int orig_castling = st->castling; // save original castling rights (so we can XOR it out later and update the castling rights in the hash key)
	std::memcpy(&new_st, st, sizeof(BoardState));
	new_st.prev = st;
	if(new_st.epsq != SQ_NONE){
		key ^= Hashing::enp[file_of(new_st.epsq)];
		new_st.epsq = SQ_NONE; // reset e.p. square
	}
	new_st.capd = NO_PIECE_TYPE; // no piece type was captured
	new_st.key = 0; // reset Zobrist key
	st = &new_st;
	// Increment plies, etc. //
	++st->ply;
	++st->fifty_ct;
	key ^= Hashing::side; // we can do this now - it was coming anyway...
	bool should_reset_50 = false; // if we should set the halfmove counter to 0
	// Now do the move. //
	Side us = to_move, them = ~us;
	Square from = from_sq(m), to = to_sq(m);
	assert(is_ok(from));
	assert(is_ok(to));
	Piece pc = at(from);
	assert(side_of(pc) == us && (pCount[WHITE][ALL_PIECES] <= 16) && (pCount[BLACK][ALL_PIECES] <= 16));
	PieceType pt = type_of(pc);
	PieceType capd = (type_of(m) != ENPASSANT) ? type_of(at(to)) : PAWN;
	if((capd != NO_PIECE_TYPE) && (type_of(m) != CASTLING)){
		st->capd = capd;
		Square s = to;
		should_reset_50 = true;
		if(capd == PAWN){
			if(type_of(m) == ENPASSANT){
				s = to - pawn_push(us);
				assert(type_of(at(s)) == PAWN);
				assert(side_of(at(s)) == them);
			}
			st->pawn_key ^= Hashing::psq[them][PAWN][s];
		}
		remove_piece(capd, them, s);
		key ^= Hashing::psq[them][capd][s];
		st->material_key ^= Hashing::psq[them][capd][pCount[them][capd]]; // only pCount[...] instead of that minus 1 since remove_piece() above already decremented it by 1
	}
	if((type_of(m) != CASTLING) && st->castling && (capd == ROOK)){
		// If we captured a rook, update castling rights now. //
		Square rel_from = relative_square(them, to);
		int affected = 0;
		if(rel_from == SQ_A1) affected = WHITE_OOO;
		else if(rel_from == SQ_H1) affected = WHITE_OO;
		st->castling &= ~(affected << (2 * them)); // kill the side that lost its rook's rights
	}
	if(type_of(m) == NORMAL){
		// First, update castling rights if necessary. //
		if(((WHITE_OO | WHITE_OOO) << (2 * us)) & st->castling){
			int new_cast = st->castling;
			if(pt == KING){
				// If the king moves, no more castling for that side. Period. //
				new_cast &= ~((WHITE_OO | WHITE_OOO) << (2 * us)); // kill castling rights of side to move with maximum prejudice
			} else if(pt == ROOK){
				// Rook moves only affect one castling side. //
				Square rel_from = relative_square(us, from);
				int affected = 0;
				if(rel_from == SQ_A1) affected = WHITE_OOO;
				else if(rel_from == SQ_H1) affected = WHITE_OO;
				new_cast &= ~(affected << (2 * us)); // kill that sides rights
			}
			st->castling = new_cast;
		}
		// Then, move the piece. //
		move_piece(pt, us, from, to);
		key ^= Hashing::psq[us][pt][from] ^ Hashing::psq[us][pt][to]; // moved a piece
		// Now, handle pawn e.p. and halfmove reset //
		if(pt == PAWN){
			should_reset_50 = true;
			if(distance<Rank>(from, to) == 2){ // if it is a pawn double push
				st->epsq = (from + to) / 2; // the midpoint between from and to is the e.p. square
				key ^= Hashing::enp[file_of(st->epsq)]; // e.p. file
			}
		}
	} else if(type_of(m) == ENPASSANT){
		// Move our pawn //
		move_piece(pt, us, from, to);
		key ^= Hashing::psq[us][PAWN][from] ^ Hashing::psq[us][PAWN][to];
	} else if(type_of(m) == PROMOTION){
		// A promotion can be a capture, so handle that. //
		should_reset_50 = true; // the pawn moved, so we have to reset this
		// Now remove the moving pawn. //
		remove_piece(pt, us, from);
		// And put in the promoted piece. //
		PieceType prom = promotion_type(m);
		put_piece(prom, us, to);
		key ^= Hashing::psq[us][PAWN][from] ^ Hashing::psq[us][prom][to];
		st->material_key ^= Hashing::psq[us][PAWN][pCount[us][PAWN]] ^ Hashing::psq[us][prom][pCount[us][prom] - 1]; // the decrementing/incrementing is due to remove/put_piece() already being called above
	} else if(type_of(m) == CASTLING){
		assert(pt == KING);
		if(capd != ROOK){
			std::cout << *this << Moves::format<false>(m) << std::endl;
			assert(capd == ROOK);
		}
		Square kfrom = from, kto = SQ_NONE;
		Square rfrom = to, rto = SQ_NONE;
		// Find out where the king is going. //
		if(rfrom < kfrom) kto = kfrom - Square(2);
		else kto = kfrom + Square(2);
		// Find out where the rook is going. //
		rto = (kfrom + kto) / 2; // the rook always goes to the side of the king, in between the king's motion vector
		assert(is_ok(rto));
		// Now move the king //
		move_piece(KING, us, kfrom, kto);
		key ^= Hashing::psq[us][KING][kfrom] ^ Hashing::psq[us][KING][kto];
		move_piece(ROOK, us, rfrom, rto);
		key ^= Hashing::psq[us][ROOK][rfrom] ^ Hashing::psq[us][ROOK][rto];
		// And finish off castling rights. //
		st->castling &= ~((WHITE_OO | WHITE_OOO) << (2 * us));
	}
	if(pt == PAWN){
		st->pawn_key ^= Hashing::psq[us][PAWN][from];
		if(type_of(m) != PROMOTION) st->pawn_key ^= Hashing::psq[us][PAWN][to]; // since in promotion, the pawn "dissappears"
	}
	// Now, update the checkers, etc. bitboards. //
	st->checkers = 0;
	st->pinned = pinned(them);
	st->lined = lined(them);
	if(st->castling != orig_castling){
		key ^= Hashing::castling[orig_castling] ^ Hashing::castling[st->castling];
	}
	st->key = key; // update board hash key
	if(ci.ksq != king_sq(them)){
		std::cout << *this << Moves::format<false>(m) << std::endl;
		assert(ci.ksq == king_sq(them));
	}
	if(is_checking){
		if(type_of(m) == NORMAL){
			// We can optimize here. //
			if(ci.kattk[pt] & to){ // For direct checks
				st->checkers |= to;
			}
			if(our_lined && (our_lined & from)){ // For discover checks
				if(pt != ROOK) st->checkers |= attacks_from<ROOK>(ci.ksq) & pieces(us, ROOK, QUEEN); // since rooks can only move in certain ways for discover checks
				if(pt != BISHOP) st->checkers |= attacks_from<BISHOP>(ci.ksq) & pieces(us, BISHOP, QUEEN); // and same goes for bishops
			}
		} else {
			// Here we can't do as much optimization. //
			st->checkers = attackers_to(ci.ksq, byType[ALL_PIECES]) & pieces(us);
		}
	}
	to_move = ~to_move; // flip moving side
	if(should_reset_50) st->fifty_ct = 0;
	/*
	if(st->checkers != (attackers_to(king_sq(them), byType[ALL_PIECES]) & pieces(us))){
		std::cout << "Move " << Moves::format<false>(m) << " did not have correct checker bitboards updated." << std::endl;
		std::cout << *this;
		std::cout << Bitboards::pretty(st->checkers) << std::endl << Bitboards::pretty(attackers_to(king_sq(them), byType[ALL_PIECES]) & pieces(us)) << std::endl;
		assert(st->checkers == (attackers_to(king_sq(them), byType[ALL_PIECES]) & pieces(us)));
	}
	if(attackers_to(king_sq(us), byType[ALL_PIECES]) & pieces(~us)){
		std::cout << "Move " << Moves::format<false>(m) << " left own king in check and was validated." << std::endl;
		std::cout << *this;
		assert(st->prev->checkers);
		Bitboard chk = st->prev->checkers;
		while(chk){
			Square on = pop_lsb(&chk);
			std::cout << file_char_of(on) << rank_char_of(on) << std::endl;
		}
		assert(!(attackers_to(king_sq(us), byType[ALL_PIECES]) & pieces(~us))); // make sure we are not left in check
	}
	*/
}

void Board::undo_move(Move m){
	assert(is_ok(m));
	Square from = from_sq(m), to = to_sq(m);
	assert(is_ok(from));
	assert(is_ok(to));
	Side us = to_move, them = ~us;
	PieceType capd = st->capd;
	MoveType type = type_of(m);
	st = st->prev;
	if(type == NORMAL){
		// Let's first move the piece back. //
		PieceType moved_t = type_of(at(to));
		move_piece(moved_t, them, to, from);
		// Then, let's put the captured piece type if/a. //
		if(capd != NO_PIECE_TYPE){
			put_piece(capd, us, to);
		}
	} else if(type == CASTLING){
		// Note: We don't have to worry about castling rights and such since we saved our previous state.
		// Figure out what moved and how. //
		Square rfrom = to, rto = SQ_NONE; // rook path
		Square kfrom = from, kto = SQ_NONE; // king path
		// Find out where the king is going. //
		if(rfrom < kfrom) kto = kfrom - Square(2);
		else kto = kfrom + Square(2);
		assert(is_ok(kto));
		// Find out where the rook is going. //
		rto = (kfrom + kto) / 2;
		assert(is_ok(rto));
		// And, undo it. //
		move_piece(KING, them, kto, kfrom);
		move_piece(ROOK, them, rto, rfrom);
	} else if(type == ENPASSANT){
		// An e.p. cannot capture two pieces, we know that.
		// First, move the capturing pawn back. //
		move_piece(PAWN, them, to, from);
		// Then, put the captured pawn back. //
		put_piece(PAWN, us, (to + pawn_push(us)));
	} else if(type == PROMOTION){
		// First, get rid of the promoted piece. //
		PieceType prom_t = promotion_type(m);
		assert(prom_t == type_of(at(to)));
		remove_piece(prom_t, them, to);
		// Then, put the promoted pawn back. //
		put_piece(PAWN, them, from);
		// Finally, reverse any captures if/a. //
		if(capd != NO_PIECE_TYPE){
			put_piece(capd, us, to);
		}
	}
	to_move = ~to_move;
	assert((pCount[WHITE][ALL_PIECES] <= 16) && (pCount[BLACK][ALL_PIECES] <= 16));
}

template<int Pt>
PieceType min_attacker(const Bitboard* bb, const Square& to, const Bitboard& stm_attackers, Bitboard& occ, Bitboard& attackers){
	// Finds minimum attacker and updates bitboards accordingly. //
	Bitboard b = stm_attackers & bb[Pt]; // find anything we can capture with of the given piece type
	if(!b){
		// Nothing doing with this piece type - let's try the next. //
		return min_attacker<Pt + 1>(bb, to, stm_attackers, occ, attackers);
	}
	occ ^= b & ~(b - 1); // reset LSB - remove attacker just found from occupancy
	if((Pt == PAWN) || (Pt == BISHOP) || (Pt == QUEEN)){ // diagonal
		attackers |= attacks_bb<BISHOP>(to, occ) & (bb[BISHOP] | bb[QUEEN]); // add in anything behind that
	}
	if((Pt == ROOK) || (Pt == QUEEN)){ // vertical/horizontal
		attackers |= attacks_bb<ROOK>(to, occ) & (bb[ROOK] | bb[QUEEN]);
	}
	attackers &= occ; // only occupied squares - just make sure
	return PieceType(Pt);
}

template<>
PieceType min_attacker<KING>(const Bitboard* bb, const Square& to, const Bitboard& stm_attackers, Bitboard& occ, Bitboard& attackers){
	return KING; // we are done here anyway, so we don't have to update the bitboards
}
	
Value Board::see(Move m) const {
	Square from, to;
	Bitboard occ, attackers, stm_attackers; // stm = side to move
	Value swapList[32]; // a swap list tallying up the exchanges made
	int ind = 1; // index we are on in the swap list
	PieceType capd; // the piece type captured
	Side stm; // the side to move
	assert(is_ok(m));
	from = from_sq(m);
	to = to_sq(m);
	swapList[0] = PieceValue[MG][at(to)]; // the first piece taken
	stm = side_of(at(from)); // therefore, side to move is side of moving piece
	occ = all() ^ from; // occupancy is initially without the from square of the first move made
	if(type_of(m) == CASTLING){
		// Castling can never be SEE negative since it must be valid and 
		// the rook cannot end up under attack since otherwise the king
		// would have moved through check.
		return VAL_ZERO;
	} else if(type_of(m) == ENPASSANT){
		occ ^= to - pawn_push(stm); // get rid of the captured pawn
		swapList[0] = PieceValue[MG][PAWN]; // a pawn was taken first
	}
	attackers = attackers_to(to, occ) & occ; // get all valid attackers that can recapture
	stm = ~stm; // flip side to move
	stm_attackers = attackers & pieces(stm); // get all aggressors from side to recapture
	if(!stm_attackers){
		// If the opposing side cannot recapture, then we are done. //
		return swapList[0];
	}
	// OK, there are defenders of that square. Shame. //
	capd = type_of(at(from)); // because this piece is just about to be captured
	do {
		assert(ind < 32); // otherwise something is very, very wrong
		swapList[ind] = -swapList[ind - 1] + PieceValue[MG][capd]; // cumulative score, subtract other side's gains
		capd = min_attacker<PAWN>(byType, to, stm_attackers, occ, attackers);
		if(capd == KING){
			if(stm_attackers == attackers){
				++ind; // looks like the king can take it after all
			}
			break;
		}
		stm = ~stm;
		stm_attackers = attackers & pieces(stm);
		++ind;
	} while (stm_attackers); // as long as recapturing is possible
	// We have the swaplist now, so let's find the best possible score for the side that started the exchange
	// and return it.
	while(--ind){
		swapList[ind - 1] = std::min(-swapList[ind], swapList[ind - 1]); // since the exchange can be stopped at any point by either person, so they want the least-worst option
	}
	return swapList[0]; // everything else gets "condensed" into this final entry
}
	
Value Board::see_sign(Move m) const {
	assert(is_ok(m));
	if(PieceValue[MG][type_of(moved_piece(m))] <= PieceValue[MG][type_of(at(to_sq(m)))]){
		// We are taking a more/equally valuable piece with the same, so:
		// If we are taking a more valuable piece, then even if the
		// opponent recaptures, we are good.
		// If the same, then we will stop there.
		return VAL_KNOWN_WIN; // hehe
	}
	return see(m);
}

























































