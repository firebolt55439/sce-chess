#include "Common.h"
#include "Bitboards.h"
#include "Board.h"
#include "MoveGen.h"
#include "Evaluation.h"
#include "Pawns.h"

#define S(mg, eg) make_score(mg, eg)
#define SS(g) make_score(g, g)

enum {
	Mobility, PawnStructure, PassedPawns, Space, KingSafety
};

const struct Weight { int mg, eg; } Weights[] = {
	{289, 344}, {233, 201}, {221, 273}, {46, 0}, {321, 0}
}; // weights by [above enum from Mobility to KingSafety]

Score PSQTable[SIDE_NB][PIECE_TYPE_NB][SQUARE_NB]; // used for piece-square table scores

struct EvalInfo {
	// This is a structure that collects information computed by evaluation
	// so effort is not wasted on repeat collections of data.
	// TODO: Material (hash) tables
	Bitboard attackedBy[SIDE_NB][PIECE_TYPE_NB]; // attacked by [side][piece type (or ALL_PIECES)]
	Bitboard kingRing[SIDE_NB]; // the squares which we are watching for attacks against each side's king
	int kingAttkCount[SIDE_NB]; // the number of pieces of the given color attacking a square in the enemy's king ring
	int kingAttkWeight[SIDE_NB]; // we "weight" each king attacker (e.g. queen attack > bishop attack)
	int kingAdjCount[SIDE_NB]; // the number of attacks to squares directly next to the king of the given side
	Bitboard pinned[SIDE_NB]; // pinned pieces by side
	Pawns::PawnEntry* pe; // pawn entry from hash table
};

// Material Imbalance and Values //
//                            none  pawn knight bishop rook queen
const int LinearMaterial[6] = { 0, -162, -1122, -183,  249, -154 };

const int QuadraticOurs[][PIECE_TYPE_NB] = {
	//            OUR PIECES
	// none pawn knight bishop rook queen
	{  0                               }, // None
	{  0,    2                         }, // Pawn
	{  0,  271,  -4                    }, // Knight      OUR PIECES
	{  0,  105,   4,    0              }, // Bishop
	{  0,   -2,  46,   100,  -141      }, // Rook
	{  0,   25, 129,   142,  -137,   0 }  // Queen
};

const int QuadraticTheirs[][PIECE_TYPE_NB] = {
	//           THEIR PIECES
	// none pawn knight bishop rook queen
	{   0                               }, // None
	{   0,    0                         }, // Pawn
	{   0,   62,   0                    }, // Knight      OUR PIECES
	{   0,   64,  39,     0             }, // Bishop
	{   0,   40,  23,   -22,    0       }, // Rook
	{   0,  105, -39,   141,  274,    0 }  // Queen
};

// King Safety, Threats //
const int KingAttackWeights[] = { 0, 0, 6, 2, 5, 5 }; // by [piece type], excl. king, rook == queen
Score KingDanger[512];

const Score ThreatenedByPawn[PIECE_TYPE_NB - 1] = {
	S(0, 0), S(0, 0), S(87, 118), S(84, 122), S(114, 203), S(121, 217)
}; // for penalties when a piece of type [PieceType] is threatened by an enemy pawn (excl. the king)

// Evaluation Bonuses/Penalties //
const Score RookOnPawn = S(7, 27); // for rooks picking off pawns

const Score MobilityBonus[][32] = {
	// Mobility Bonus by [piece type][number of available squares]
	{}, // NO_PIECE_TYPE
	{}, // Pawns (handled separately)
	{ 
		S(-65,-50), S(-42,-30), S(-9,-10), S( 3,  0), S(15, 10), S(27, 20), // Knights
		S( 37, 28), S( 42, 31), S(44, 33) 
	},
	{ 
		S(-52,-47), S(-28,-23), S( 6,  1), S(20, 15), S(34, 29), S(48, 43), // Bishops
		S( 60, 55), S( 68, 63), S(74, 68), S(77, 72), S(80, 75), S(82, 77),
		S( 84, 79), S( 86, 81) 
	},
	{ 
		S(-47,-53), S(-31,-26), S(-5,  0), S( 1, 16), S( 7, 32), S(13, 48), // Rooks
		S( 18, 64), S( 22, 80), S(26, 96), S(29,109), S(31,115), S(33,119),
		S( 35,122), S( 36,123), S(37,124) 
	},
	{ 
		S(-42,-40), S(-28,-23), S(-5, -7), S( 0,  0), S( 6, 10), S(11, 19), // Queens
		S( 13, 29), S( 18, 38), S(20, 40), S(21, 41), S(22, 41), S(22, 41),
		S( 22, 41), S( 23, 41), S(24, 41), S(25, 41), S(25, 41), S(25, 41),
		S( 25, 41), S( 25, 41), S(25, 41), S(25, 41), S(25, 41), S(25, 41),
		S( 25, 41), S( 25, 41), S(25, 41), S(25, 41) 
	}
};

// Check Danger Constants for King Danger //
// Note: Only safe checks are counted.
// Contact Checks
const int QueenContactCheck = 92;
const int RookContactCheck = 68;
// Distance Checks
const int QueenCheck = 50;
const int RookCheck = 36;
const int BishopCheck = 7;
const int KnightCheck = 14;

Score apply_weight(Score s, const Weight& w){
	// Weighs score 's' by weight 'w'.
	return make_score(mg_value(s) * w.mg / 256, eg_value(s) * w.eg / 256);
}

inline std::string score_str(Score s){
	std::stringstream ss;
	ss << "(" << mg_value(s) << ", " << eg_value(s) << ")";
	return ss.str();
}

void Eval::init(void){
	// Init KingDanger Array //
	const double MaxSlope = 7.5; // maximum instantaneous change b/w two points (e.g. derivative at a point)
	const double Peak = 1280.0; // max amount
	double t = 0.0;
	KingDanger[0] = SCORE_ZERO;
	// f(i) = min(Peak, min((1/40)*i^2, f(i - 1))) for i = 1 ... 400
	for(int i = 1; i < 400; i++){ // essentially simulating graphing a function
		// Note: King safety weight goes from 321 weighted to 0 by extreme endgame.
		t = std::min(Peak, std::min(0.025 * i * i, t + MaxSlope));
		KingDanger[i] = apply_weight(make_score(int(t), 0), Weights[KingSafety]);
	}
	// Init PSQTable //
	for(PieceType pt = PAWN; pt <= KING; pt++){
		Score v = make_score(PieceValue[MG][pt], PieceValue[EG][pt]);
		for(Square s = SQ_A1; s <= SQ_H8; s++){
			PSQTable[WHITE][pt][s] = v + Eval::PieceSquareTable[pt][s];
			PSQTable[BLACK][pt][~s] = -v - Eval::PieceSquareTable[pt][s];
		}
	}
}

template<Side Us>
void init_eval_info(const Board& pos, EvalInfo& ei){
	// Note: We decide here whether to initialize the other person's king safety table or not. //
	const Side Them = (Us == WHITE) ? BLACK : WHITE;
	ei.pinned[Us] = pos.pinned(Us);
	Bitboard king_zone = ei.attackedBy[Them][KING] = pos.attacks_from<KING>(pos.king_sq(Them)); 
	ei.attackedBy[Us][ALL_PIECES] = ei.attackedBy[Us][PAWN] = ei.pe->pawnAttks[Us];
	// TODO: Only init king safety tables if we have over a certain material threshold
	ei.kingRing[Them] = king_zone | shift_bb<Us == WHITE ? DELTA_S : DELTA_N>(king_zone); // their king ring has their king zone plus their zone "pawn pushed"
	king_zone &= ei.attackedBy[Us][PAWN]; // now find anything controlled by a pawn of ours
	ei.kingAttkCount[Us] = (king_zone ? popcount<Max15>(king_zone) : 0); // init our king attk counter with pawn attacks
	ei.kingAttkWeight[Us] = ei.kingAdjCount[Us] = 0; // zero out the other king safety tables
}

template<Side Us, bool Verbose>
int material_imbalance(const int pcount[][SIDE_NB]){
	const Side Them = (Us == WHITE ? BLACK : WHITE);
	int bonus = 0;
	//printf("Side %d.\n", int(Us));
	for(PieceType i = PAWN; i <= QUEEN; i++){
		if(!pcount[i][Us]) continue;
		//printf("Checking piece %c...\n", PieceChar[make_piece(WHITE, i)]);
		int v = LinearMaterial[i]; // scale factor, or rather, value of this piece type given the current material
		for(PieceType j = PAWN; j <= QUEEN; j++){
			//printf("Against %c.\n", PieceChar[make_piece(BLACK, j)]);
			v += (QuadraticOurs[i][j] * pcount[j][Us]) + (QuadraticTheirs[i][j] * pcount[j][Them]);
			//printf("V - Now: %d\n", v);
		}
		bonus += pcount[i][Us] * v;
		//printf("Bonus is now %d.\n", bonus);
	}
	int16_t cast = int16_t(bonus);
	return int(cast);
}

Score psq_score(const Board& pos){
	Score score = SCORE_ZERO;
	Bitboard pcs = pos.all();
	while(pcs){
		Square s = pop_lsb(&pcs);
		Piece pc = pos.at(s);
		score += PSQTable[side_of(pc)][type_of(pc)][s];
	}
	return score;
}

template<PieceType Pt, Side Us, bool Verbose>
Score evaluate_pieces(const Board& pos, EvalInfo& ei, Score* mobility, Bitboard* mobility_area){
	// This contains rules to evaluate each piece by. //
	// Note: Pawns and kings are evaluated separately.
	// TODO: Mobility
	Bitboard b; // a temporary basically used for keeping track of attacked squares, threats, etc.
	assert(Pt > PAWN && Pt < KING);
	const PieceType NextPt = (Us == WHITE) ? Pt : PieceType(Pt + 1);
	const Side Them = (Us == WHITE ? BLACK : WHITE);
	const Square ksq = pos.king_sq(Us);
	Score score = SCORE_ZERO;
	ei.attackedBy[Us][Pt] = 0;
	Bitboard pcs = pos.pieces(Us, Pt);
	while(pcs){
		Square s = pop_lsb(&pcs);
		b = (Pt == BISHOP) ? attacks_bb<BISHOP>(s, pos.all() ^ pos.pieces(Us, QUEEN)) :
			(Pt == ROOK) ? attacks_bb<ROOK>(s, pos.all() ^ pos.pieces(Us, ROOK, QUEEN)) :
			pos.attacks_from<Pt>(s); // this way, we can count supported pieces for sliders (not for bishop due to slim possiblity of same-colored bishops)
		if(ei.pinned[Us] & s){
			// OK, this is a pinned piece. //
			b &= LineBB[ksq][s]; // therefore, can only attack places without giving check (or it is not really an attack)
		}
		ei.attackedBy[Us][ALL_PIECES] |= ei.attackedBy[Us][Pt] |= b;
		if(b & ei.kingRing[Them]){
			// Yay, we can attack some of the enemy king's stuff! //
			ei.kingAttkCount[Us]++;
			ei.kingAttkWeight[Us] += KingAttackWeights[Pt];
			Bitboard bb = b & ei.attackedBy[Them][KING];
			if(bb) ei.kingAdjCount[Them] += popcount<Max15>(bb); // count adjacent attacks
		}
		if(Pt == QUEEN){
			// The Queen is the most valuable piece on the board, so we won't consider
			// cases where we blunder into actual attacks (not by pinned pieces) by
			// less valuable pieces (e.g. knights, bishops, rooks, etc.)
			b &= ~(ei.attackedBy[Them][KNIGHT] | ei.attackedBy[Them][BISHOP] | ei.attackedBy[Them][ROOK]);
		}
		int mob = (Pt != QUEEN) ? (popcount<Max15>(b & mobility_area[Us])) : (popcount<Full>(b & mobility_area[Us])); // since the queen can possibly go to more than 15 squares
		mobility[Us] += MobilityBonus[Pt][mob];
		if(ei.attackedBy[Them][PAWN] & s){
			// Penalty if attacked by pawn. //
			score -= ThreatenedByPawn[Pt];
		}
		if((Pt == BISHOP) || (Pt == KNIGHT)){
			// TODO: Outpost evaluation
			// TODO: Possibly bonus if behind pawn? Supported by pawn? Supporting pawn?
		}
		if(Pt == ROOK){
			// Give a bonus to positions where the rook goes way ahead and starts picking
			// off pawns in the same rank (or file for that matter).
			if(relative_rank(Us, s) >= RANK_5){
				Bitboard bb = pos.pieces(Them, PAWN) & PseudoAttacks[ROOK][s];
				if(bb) score += RookOnPawn * popcount<Max15>(bb);
			}
			// TODO: Open, semi-open files
			// TODO: Penalty for being trapped by king, esp. if king cannot castle
		}
	}
	return score - evaluate_pieces<NextPt, Them, Verbose>(pos, ei, mobility, mobility_area);
}

template<>
Score evaluate_pieces<KING, WHITE, true>(const Board&, EvalInfo&, Score*, Bitboard*){
	return SCORE_ZERO; // stop here
}

template<>
Score evaluate_pieces<KING, WHITE, false>(const Board&, EvalInfo&, Score*, Bitboard*){
	return SCORE_ZERO; // stop here
}

template<Side Us, bool Verbose>
Score evaluate_king(const Board& pos, EvalInfo& ei){
	const Side Them = (Us == WHITE ? BLACK : WHITE);
	const Square ksq = pos.king_sq(Us);
	// TODO: King shelter, enemy pawn storm, etc. (in pawn hash table)
	Score score = SCORE_ZERO;
	Bitboard undefended = ei.attackedBy[Us][KING] & ei.attackedBy[Them][ALL_PIECES]
		& ~(ei.attackedBy[Us][PAWN] | ei.attackedBy[Us][KNIGHT] | ei.attackedBy[Us][BISHOP] | 
			ei.attackedBy[Us][ROOK] | ei.attackedBy[Us][QUEEN]); // undefended squares adjacent to king (attacked with king as only defender)
	int attack_danger = 0;
	// Safe Queen Contact Checks //
	Bitboard b = undefended & ei.attackedBy[Them][QUEEN] & ~pos.pieces(Them);
	if(b){
		b &= ei.attackedBy[Them][PAWN] | ei.attackedBy[Them][KNIGHT] | ei.attackedBy[Them][BISHOP] | ei.attackedBy[Them][ROOK]; // has to be supported by something
		if(b){
			attack_danger += QueenContactCheck * popcount<Max15>(b);
		}
	}
	// Safe Rook Contact Checks //
	b = undefended & ei.attackedBy[Them][ROOK] & ~pos.pieces(Them); // now for rook contact checks
	b &= PseudoAttacks[ROOK][ksq]; // make sure it actually gives check (rather than going to a "corner" of the zone)
	if(b){
		b &= ei.attackedBy[Them][PAWN] | ei.attackedBy[Them][KNIGHT] | ei.attackedBy[Them][BISHOP] | ei.attackedBy[Them][QUEEN]; // has to be supported by something
		if(b){
			attack_danger += RookContactCheck * popcount<Max15>(b);
		}
	}
	// Now find any safe (slider) checks from a distance //
	Bitboard safe = ~(ei.attackedBy[Us][ALL_PIECES] | pos.pieces(Them)); // this contains all safe squares they can attack
	Bitboard b1 = pos.attacks_from<ROOK>(ksq) & safe;
	Bitboard b2 = pos.attacks_from<BISHOP>(ksq) & safe;
	if(Verbose) printf("Attack danger after contacts (side %d): %d\n", int(Us), attack_danger);
	// Queen Safe Checks //
	b = (b1 | b2) & ei.attackedBy[Them][QUEEN];
	if(b){
		attack_danger += QueenCheck * popcount<Max15>(b);
	}
	// Rook Safe Checks //
	b = b1 & ei.attackedBy[Them][ROOK];
	if(b){
		attack_danger += RookCheck * popcount<Max15>(b);
	}
	// Bishop Safe Checks //
	b = b2 & ei.attackedBy[Them][BISHOP];
	if(b){
		attack_danger += BishopCheck * popcount<Max15>(b);
	}
	// Knight Safe Checks //
	b = pos.attacks_from<KNIGHT>(ksq) & safe & ei.attackedBy[Them][KNIGHT];
	if(b){
		attack_danger += KnightCheck * popcount<Max15>(b);
	}
	// And return the final score. //
	if(Verbose) printf("Attack danger (side %d): %d\n", int(Us), attack_danger);
	score -= KingDanger[std::max(std::min(attack_danger, 399), 0)];
	if(Verbose) printf("King score (side %d): %s - %s - %d\n", int(Us), score_str(score).c_str(), score_str(KingDanger[std::max(std::min(attack_danger, 399), 0)]).c_str(), std::max(std::min(attack_danger, 399), 0));
	return score;
}

template<Side Us, bool Verbose>
Score evaluate_passed_pawns(const Board& pos, EvalInfo& ei){
	const Side Them = (Us == WHITE ? BLACK : WHITE);
	const Square ksq_us = pos.king_sq(Us), ksq_them = pos.king_sq(Them);
	Score score = SCORE_ZERO;
	Bitboard b = ei.pe->passedPawns[Us];
	Bitboard defended, unsafe, to_queen;
	while(b){
		Square s = pop_lsb(&b);
		int r = relative_rank(Us, s) - RANK_2;
		int rr = r * (r - 1);
		Value mbonus = Value(17 * rr), ebonus = Value(7 * (rr + r + 1));
		if(rr){ // means that it has advanced at least 2 ranks (or rr would be 0)
			// First, we want to maximize distance away from their king and minimize
			// distance away from ours - much more if we are far advanced.
			ebonus += (5 * rr * distance(ksq_them, s)) - (2 * rr * distance(ksq_us, s));
			Square next_sq = s + pawn_push(Us);
			if(relative_rank(Us, next_sq) != RANK_8){
				// OK, we can't promote in one move, so let's consider one
				// more square.
				ebonus -= distance(ksq_us, next_sq + pawn_push(Us)) * rr;
			}
			if(pos.empty(next_sq)){
				// OK, we can move up one.
				defended = unsafe = to_queen = forward_bb(Us, s);
				Bitboard bb = forward_bb(Them, s) & pos.pieces(ROOK, QUEEN) & pos.attacks_from<ROOK>(s); // any rooks or queens that are on the file and are attacking this pawn
				if(!(pos.pieces(Us) & bb)){ 
					// We have no defenders on this file.
					defended &= ei.attackedBy[Us][ALL_PIECES]; // only what squares are protected by us
				}
				if(!(pos.pieces(Them) & bb)){
					unsafe &= ei.attackedBy[Them][ALL_PIECES] | pos.pieces(Them); // unsafe squares are attacked by them (or are occupied)
				}
				int mult = (!unsafe ? 15 : (!(unsafe & next_sq) ? 9 : 0)); // a bonus multiplier - esp. high if our file is open, somewhat high if we can advance, otherwise no bonus
				if(defended == to_queen){
					// Wow, we literally have EVERYTHING defended in front of the pawn. //
					mult += 6;
				} else if(defended & next_sq){
					// At least we can move up more. //
					mult += 4;
				}
				mbonus += mult * rr;
				ebonus += mult * rr;
			} else if(pos.pieces(Us) & next_sq){
				// So our own piece is blocking the square. //
				// We give it a bonus b/c it could mean that
				// we are extremely close to promoting and just
				// want to make sure the pawn isn't going to
				// be taken.
				mbonus += (rr * 3) + (r * 2) + 3;
				ebonus += rr + (r * 2);
			}
		}
		if(pos.count(Us, PAWN) > pos.count(Them, PAWN)){
			ebonus += (ebonus / 4); // in the endgame, we'll have more opportunities, so this is the opportunity bonus
									// and also so we can split their attention and force them to make a choice
		}
		score += make_score(mbonus, ebonus);
	}
	return apply_weight(score, Weights[PassedPawns]);
}

template<bool Verbose>
Value do_evaluate(const Board& pos){
	// Returns score relative to side to move (e.g. -200 for black to move is +200 for white to move). //
	// Note: All helper functions should return score relative to white. //
	Score score = SCORE_ZERO, mobility_score[SIDE_NB] = { SCORE_ZERO, SCORE_ZERO };
	EvalInfo ei;
	Value nonPawnMaterial[SIDE_NB];
	Phase game_phase;
	// TODO: Endgames
	// Init Eval Info //
	ei.pe = Pawns::probe(pos);
	init_eval_info<WHITE>(pos, ei);
	init_eval_info<BLACK>(pos, ei);
	ei.attackedBy[WHITE][ALL_PIECES] |= ei.attackedBy[WHITE][KING];
	ei.attackedBy[BLACK][ALL_PIECES] |= ei.attackedBy[BLACK][KING];
	for(Side c = WHITE; c <= BLACK; c++){
		nonPawnMaterial[c] = Value(0);
		for(PieceType pt = KNIGHT; pt < KING; pt++){
			nonPawnMaterial[c] += (pos.count(c, pt) * PieceValue[MG][pt]);
		}
		if(Verbose) printf("side %d: %d\n", int(c), int(nonPawnMaterial[c]));
	}
	Value total_npm = nonPawnMaterial[BLACK] + nonPawnMaterial[WHITE];
	total_npm = std::max(EndgameLimit, std::min(MidgameLimit, total_npm));
	game_phase = Phase(((total_npm - EndgameLimit) * PHASE_MIDGAME) / (MidgameLimit - EndgameLimit)); // scale phase b/w midgame and endgame, truncate decimal point
	if(Verbose) printf("Game phase: %d\n", game_phase);
	// TODO: Mobility area, space evaluation
	// TODO: Threats, etc.
	// Material Factoring //
	const int pcounts[PIECE_TYPE_NB - 1][SIDE_NB] = {
		{}, // NO_PIECE_TYPE
		{ pos.count(WHITE, PAWN), pos.count(BLACK, PAWN) },
		{ pos.count(WHITE, KNIGHT), pos.count(BLACK, KNIGHT) },
		{ pos.count(WHITE, BISHOP), pos.count(BLACK, BISHOP) },
		{ pos.count(WHITE, ROOK), pos.count(BLACK, ROOK) },
		{ pos.count(WHITE, QUEEN), pos.count(BLACK, QUEEN) }
	};
	Value imb = Value((material_imbalance<WHITE, Verbose>(pcounts) - material_imbalance<BLACK, Verbose>(pcounts)) / 16);
	score += SS(imb);
	if(Verbose) printf("Material Imbalance: %d\n", imb);
	// Piece-Square Tables //
	score += psq_score(pos); // piece-square tables
	if(Verbose) printf("+PSQT Score: %s\n", score_str(score).c_str());
	// Pawns //
	score += apply_weight(ei.pe->pawn_score(), Weights[PawnStructure]);
	if(Verbose) printf("+Pawns Score: %s\n", score_str(score).c_str());
	// Piece-Specific Evaluation and Mobility //
	Bitboard mobility_area[SIDE_NB] = { ~(ei.attackedBy[BLACK][PAWN] | pos.pieces(WHITE, PAWN, KING)), ~(ei.attackedBy[WHITE][PAWN] | pos.pieces(BLACK, PAWN, KING)) }; // what we are considering as "mobile" squares
	score += evaluate_pieces<KNIGHT, WHITE, Verbose>(pos, ei, mobility_score, mobility_area);
	if(Verbose) printf("+Pieces: %s\n", score_str(score).c_str());
	score += apply_weight(mobility_score[WHITE] - mobility_score[BLACK], Weights[Mobility]);
	if(Verbose) printf("+Mobility: %s\n", score_str(score).c_str());
	// King Safety //
	score += evaluate_king<WHITE, Verbose>(pos, ei) - evaluate_king<BLACK, Verbose>(pos, ei);
	if(Verbose) printf("+King: %s\n", score_str(score).c_str());
	// Passed Pawns //
	score += evaluate_passed_pawns<WHITE, Verbose>(pos, ei) - evaluate_passed_pawns<BLACK, Verbose>(pos, ei);
	if(Verbose) printf("+Passed Pawns: %s\n", score_str(score).c_str());
	// Return //
	// TODO: ScaleFactor's and specialized evaluation functions
	ScaleFactor scale_factor = SCALE_FACTOR_NORMAL; // TODO
	Value final_score = (mg_value(score) * int(game_phase)) + (eg_value(score) * int(PHASE_MIDGAME - game_phase) * scale_factor / SCALE_FACTOR_NORMAL);
	final_score /= int(PHASE_MIDGAME);
	if(Verbose) printf("Final score: %d\n", final_score);
	// TODO: Tempo
	assert(abs(final_score) < VAL_INF);
	return (pos.side_to_move() == WHITE) ? (final_score) : (-final_score);
}

Value Eval::evaluate(const Board& pos){
	return do_evaluate<false>(pos);
}

Value Eval::evaluate_verbose(const Board& pos){
	return do_evaluate<true>(pos);
}



















































