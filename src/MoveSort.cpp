#include "Common.h"
#include "Bitboards.h"
#include "Board.h"
#include "MoveGen.h"
#include "MoveSort.h"

enum {
	// Stages //
	REGULAR, // regular search (at d > 0)
		CAPTURES_S1, // winning captures
		QUIETS_S1, // quiets with positive history value
		QUIETS_S2, // quiets with negative history value
		BAD_CAPTURES_S1, // bad captures
	EVASION, // regular evasions
		EVASIONS_S1, // evasions itself
	STOP // when we are done
	// TODO: More stuff here...
};

void stable_insertion_sort(ActMove* begin, ActMove* end){
	ActMove tmp, *p, *q;
	for(p = begin + 1; p < end; p++){
		tmp = *p;
		for(q = p; (q != begin && *(q - 1) < tmp); q--){
			*q = *(q - 1);
		}
		*q = tmp;
	}
}

inline bool has_positive_score(const ActMove& move){
	return move.value > VAL_ZERO;
}

inline ActMove* pick_best(ActMove* begin, ActMove* end){
	// Picks the best move, brings it to the front, and returns it. //
	std::swap(*begin, *std::max_element(begin, end));
	return begin; // since it is now swapped
}

MoveSorter::MoveSorter(const Board& p, Depth d, const HistoryTable& ht, Search::Stack* s) : pos(p), hst(ht), ss(s), depth(d) {
	assert(d > DEPTH_ZERO); // have not implemented QS yet
	cur = end = moves; // reset current and end
	end_bad_captures = moves + MAX_MOVES - 1; // the end of the array
	if(pos.checkers()) stage = EVASION;
	else stage = REGULAR;
}

template<>
void MoveSorter::score<CAPTURES>(void){
	// This uses MVV/LVA ordering for captures. //
	// TODO: Move bad captures to end using SEE
	// TOOD: Killer moves
	// TOOD: TT/Hash moves
	for(ActMove* it = cur; it != end; it++){
		Move m = it->move;
		it->value = PieceValue[MG][type_of(pos.at(to_sq(m)))] - Value(type_of(pos.moved_piece(m)));
		if(type_of(m) == ENPASSANT){
			it->value += PieceValue[MG][PAWN];
		} else if(type_of(m) == PROMOTION){
			it->value -= PieceValue[MG][PAWN] - PieceValue[MG][promotion_type(m)]; // add promotion value, subtract pawn value
		}
	}	
}

template<>
void MoveSorter::score<NON_CAPTURES>(void){
	// For non-captures/quiets, we use the history value for sorting. //
	for(ActMove* it = cur; it != end; it++){
		Move m = it->move;
		it->value = hst[pos.moved_piece(m)][to_sq(m)];
	}
}

template<>
void MoveSorter::score<EVASIONS>(void){
	Move m;
	Value see;
	for(ActMove* it = cur; it != end; it++){
		m = it->move;
		if((see = pos.see_sign(m)) < VAL_ZERO){
			it->value = see - HistoryTable::Max; // move losing captures to bottom
		} else if(pos.is_capture(m)){
			it->value = PieceValue[MG][type_of(pos.at(to_sq(m)))] - Value(type_of(pos.moved_piece(m))) + HistoryTable::Max; // move winning captures to top
		} else {
			it->value = hst[pos.moved_piece(m)][to_sq(m)]; // otherwise use history value
		}
	}
}

void MoveSorter::gen_next_stage(void){
	cur = moves; // start from the beginning
	switch(++stage){
		case CAPTURES_S1:
			end = generate_moves<CAPTURES>(pos, moves);
			score<CAPTURES>();
			return;
		case QUIETS_S1:
			end_quiets = end = generate_moves<NON_CAPTURES>(pos, moves);
			score<NON_CAPTURES>();
			end = std::partition(cur, end, has_positive_score); // for first quiet stage, only positive history value quiets
			stable_insertion_sort(cur, end);
			return;
		case QUIETS_S2:
			cur = end; // since we partitioned above
			end = end_quiets; // restore proper end
			if(depth >= (3 * ONE_PLY)){ // don't waste too much effort on leaves/low depths
				stable_insertion_sort(cur, end);
			}
			return;
		case BAD_CAPTURES_S1:
			// When we moved the losing captures to the end, it was
			// when iterating from [best --> worst] by MVV/LVA
			// ordering, so if we now pick them in reverse order
			// from the end, we should get MVV/LVA ordering
			// again (double neg. = positive)
			cur = moves + MAX_MOVES - 1;
			end = end_bad_captures; // should be <= cur now since we will pick in reverse order
			return;
		case EVASIONS_S1:
			end = generate_moves<EVASIONS>(pos, moves);
			score<EVASIONS>();
			return;
		case EVASION:
			// If we are at one of these, then we completed all stages of a previous cycle. //
			stage = STOP;
			++cur; // otherwise the while loop of (cur == end) will never terminate in next_move()
			return;
		case STOP:
			end = cur + 1; // so the while loop will stop looping and this won't get called again and we won't end up at (STOP + 1) and have to abort
			return;
		default:
			assert(false);
	}
}

Move MoveSorter::next_move(void){
	Move m;
	while(true){
		while(cur == end) gen_next_stage();
		switch(stage){
			case STOP:
				return MOVE_NONE;
			case CAPTURES_S1:
				m = pick_best(cur++, end)->move; // pick_best also moves it to the beginning, so this works
				if(pos.see_sign(m) >= VAL_ZERO){ // only winning/equal captures right now
					return m;
				}
				(end_bad_captures--)->move = m; // move to end for bad captures - deal with later
				break;
			case QUIETS_S1: case QUIETS_S2:
				m = (cur++)->move;
				// TODO: Check against TT move, killers, etc.
				return m;
			case BAD_CAPTURES_S1:
				return (cur--)->move;
			case EVASIONS_S1:
				m = pick_best(cur++, end)->move;
				// TODO: Check against TT move
				return m;
			default:
				assert(false);
		}
	}
}





































