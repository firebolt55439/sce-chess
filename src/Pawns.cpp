#include "Common.h"
#include "Bitboards.h"
#include "Board.h"
#include "MoveGen.h"
#include "Evaluation.h"
#include "Pawns.h"

#define S(mg, eg) make_score(mg, eg)

HashTable<Pawns::PawnEntry, 16384> PawnHashTable;

// Doubled Pawn Penalty by [file] //
const Score Doubled[FILE_NB] = {
	S(13, 43), S(20, 48), S(23, 48), S(23, 48),
	S(23, 48), S(23, 48), S(20, 48), S(13, 43) 
};

// Isolated pawn penalty by [opposed][file] //
const Score Isolated[2][FILE_NB] = {
	{ 
		S(37, 45), S(54, 52), S(60, 52), S(60, 52),
		S(60, 52), S(60, 52), S(54, 52), S(37, 45)
	},
	{ 
		S(25, 30), S(36, 35), S(40, 35), S(40, 35),
		S(40, 35), S(40, 35), S(36, 35), S(25, 30) 
	} 
};

// Backward pawn penalty by [opposed][file] //
const Score Backward[2][FILE_NB] = {
	{ 
		S(30, 42), S(43, 46), S(49, 46), S(49, 46),
		S(49, 46), S(49, 46), S(43, 46), S(30, 42)
	},
	{ 
		S(20, 28), S(29, 31), S(33, 31), S(33, 31),
		S(33, 31), S(33, 31), S(29, 31), S(20, 28)
	} 
};

// Connected pawn bonus by [opposed][phalanxed][rank] //
Score Connected[2][2][RANK_NB];

// Lever bonus by [rank] //
const Score Lever[RANK_NB] = {
	S( 0, 0), S( 0, 0), S(0, 0), S(0, 0),
	S(20,20), S(40,40), S(0, 0), S(0, 0) 
};

// Unsupported pawn penalty //
const Score UnsupportedPawnPenalty = S(20, 10);

void Pawns::init(void){
	static const int Seed[RANK_NB] = {
		0, 6, 15, 10, 57, 75, 135, 258
	}; // seed for bonuses based on pawn connectivity
	for(int opposed = 0; opposed <= 1; opposed++){
		for(int phalanx = 0; phalanx <= 1; phalanx++){
			for(Rank r = RANK_2; r < RANK_8; r++){
				int bonus = Seed[r] + (phalanx ? (Seed[r + 1] - Seed[r]) / 2 : 0);
				Connected[opposed][phalanx][r] = make_score(bonus / 2, bonus >> opposed); // '>> opposed' is an optimization for 'divide by 2 if opposed is one'
			}
		}
	}
}

template<Side Us> Score evaluate(const Board& pos, Pawns::PawnEntry* e);

Pawns::PawnEntry* Pawns::probe(const Board& pos){
	Key pawnKey = pos.pawn_key();
	Pawns::PawnEntry* ent = PawnHashTable[pawnKey];
	if(ent->key == pawnKey) return ent;
	ent->key = pawnKey;
	ent->score = evaluate<WHITE>(pos, ent) - evaluate<BLACK>(pos, ent);
	return ent;
}

template<Side Us>
Score evaluate(const Board& pos, Pawns::PawnEntry* e){
	const Side Them = (Us == WHITE ? BLACK : WHITE);
    const Square Up = (Us == WHITE ? DELTA_N : DELTA_S);
    const Square Right = (Us == WHITE ? DELTA_NE : DELTA_SW);
    const Square Left = (Us == WHITE ? DELTA_NW : DELTA_SE);
	Score score = SCORE_ZERO;
	Bitboard b; // just a temporary for calculations
	Bitboard our_pawns = pos.pieces(Us, PAWN);
	Bitboard their_pawns = pos.pieces(Them, PAWN);
	e->passedPawns[Us] = 0;
	e->pawnAttks[Us] = shift_bb<Right>(our_pawns) | shift_bb<Left>(our_pawns);
	e->kingSqs[Us] = SQ_NONE; // hashing only changes for pawns, so the king square is passed as a parameter to the king safety part of the pawns evaluation
	e->kingSafety[Us] = SCORE_ZERO;
	e->semiopenFiles[Us] = 0xFF; // we mark semiopen files as just one bit set per file, so at the beginning, all files are initially considered semi-open
	for(Bitboard pawns = our_pawns; pawns; ){
		Square s = pop_lsb(&pawns);
		File f = file_of(s);
		e->semiopenFiles[Us] &= ~(1ULL << f); // semi-open files have an opposing pawn, but no own pawns, so this file cannot be semi-open
		Bitboard prev_rank = rank_bb(s - pawn_push(Us));
		Bitboard connected = our_pawns & adjacent_files_bb(f) & (rank_bb(s) | prev_rank); // connected pawns - have pawns on same/prev rank on adjacent files
		bool phalanx = connected & rank_bb(s); // phalanx is connected on same rank (e.g. P P P)
		bool unsupported = !(our_pawns & adjacent_files_bb(f) & prev_rank); // no pawn support from own side
		bool isolated = !(our_pawns & adjacent_files_bb(f)); // no own pawns on adjacent files at all
		Bitboard doubled = our_pawns & forward_bb(Us, s); // multiple pawns on same file
		bool opposed = their_pawns & forward_bb(Us, s); // if we are opposed
		bool passed = !(their_pawns & passed_pawn_mask(Us, s)); // if we are "passed" (naively)
		bool lever = StepAttacksBB[make_piece(Us, PAWN)][s] & their_pawns; // if we are a lever - e.g. attacking their pawns
		bool backward;
		if(passed || isolated || connected || lever || (our_pawns & pawn_attack_span(Them, s))){
			backward = false;
		} else {
			// The stop square of a backwards pawn is not protected but is attacked by an opposing
			// sentry, meaning that this pawn really has no hope to be promoted.
			b = pawn_attack_span(Us, s) & (their_pawns | our_pawns); // now we have all pawns ahead of on adjacent files
			b = pawn_attack_span(Us, s) & rank_bb(backmost_sq(Us, b)); // and we get the rank of the closest pawn ahead of us on an adjacent file
			backward = (b | shift_bb<Up>(b)) & their_pawns; // if we find an enemy pawn waiting to take us, this pawn is backwards
		}
		assert(opposed || passed || (pawn_attack_span(Us, s) & their_pawns)); // one of these *has* to be true
		if(passed && !doubled){
			// We only care about the frontmost passed pawn since anything
			// ahead of a passed pawn is an impedance.
			e->passedPawns[Us] |= s;
		}
		if(isolated){
			// This is an isolated pawn, meaning no friendly pawns on adjacent
			// files to help it out. The penalty is pretty severe for this, since
			// this pawn should barely be counted by material value, considering
			// it is extremely easy to pick off with a rook or such, and will
			// require expensive material support to stay alive.
			score -= Isolated[opposed][f];
		}
		if(unsupported && !isolated){
			// This pawn could *potentially* be supported by adjacent file pawns,
			// but they are not, so penalty.
			score -= UnsupportedPawnPenalty;
		}
		if(doubled){
			// We penalize doubled pawns, but reduce it based on the rank distance between
			// this pawn and the furthest advanced pawn on this file.
			score -= Doubled[f] / distance<Rank>(s, frontmost_sq(Us, doubled));
		}
		if(backward){
			// Backward pawns incur a penalty, even more if opposed.
			score -= Backward[opposed][f];
		}
		if(connected){
			score += Connected[opposed][phalanx][relative_rank(Us, s)];
		}
		if(lever){
			score += Lever[rank_of(s)];
		}
	}
	return score;
}
























































	