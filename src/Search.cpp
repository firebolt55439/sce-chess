#include "Common.h"
#include "Bitboards.h"
#include "Board.h"
#include "MoveGen.h"
#include "MoveSort.h"
#include "Evaluation.h"
#include "Search.h"
#include "Threads.h"
#include "TimeManager.h"
#include "UCI.h"
#include <cfloat>
#include <cmath>

// Externally Available Data //
namespace Search {
	volatile SearchSignals Signals;
	SearchLimits Limits;
	RootMoveVector RootMoves;
	Board RootPos;
	int64_t SearchTime; // the start of the search time, in milliseconds
	BoardStateStack SetupStates;
}

// Search //

using namespace Search; // since we are implementing its functions after all

namespace {
	enum NodeType {
		Root, // a root node (at the root of the tree)
		PV, // a PV node (score is within the alpha --> beta window)
		NonPV // a non-PV node
	};

	HistoryTable History; // history table for use with move ordering
	TimeManager TimeMgr; // our time manager
	Value DrawValue[SIDE_NB]; // draw value by side
	size_t PVIdx; // used for root nodes and PV lines
	int FutilityMoveCounts[2][16]; // futility move counts by [improving][depth]
	int8_t Reductions[2][2][64][64]; // reductions by [pv][improving][depth][move num.]
}

template<bool PvNode>
inline Depth reduction(bool improving, Depth d, unsigned int move_num){
	return Depth(Reductions[PvNode][improving][d][move_num]);
}

inline Value futility_margin(Depth d){
	return Value(200 * d);
}

template<NodeType NT>
Value search(Board& pos, Stack* ss, Value alpha, Value beta, Depth depth, bool cut_node);

template<NodeType NT, bool InCheck>
Value qsearch(Board& pos, Stack* ss, Value alpha, Value beta, Depth depth);

void search_loop(Board& pos); // main iterative deepening loop

void Search::init(void){
	// Reductions Array //
	for(int d = 1; d < 64; d++){
		for(int mc = 1; mc < 64; mc++){
			double pv_reduc = log(double(d)) * log(double(mc)) / 3.00; // PV node reduction
			double non_pv_reduc = 0.33 + log(double(d)) * log(double(mc)) / 2.25; // non-PV node reduction
			Reductions[1][0][d][mc] = Reductions[1][1][d][mc] = int8_t(pv_reduc >= 1.0 ? pv_reduc + 0.5 : 0);
			Reductions[0][0][d][mc] = Reductions[0][1][d][mc] = int8_t(non_pv_reduc >= 1.0 ? non_pv_reduc + 0.5 : 0);
			if(Reductions[0][0][d][mc] >= 2){
				Reductions[0][0][d][mc] += 1; // increase reduction for non-PV, non-improving nodes
			}
		}
	}
	// Move-Count Based Pruning Array //
	for(int d = 0; d < 16; d++){
		FutilityMoveCounts[0][d] = int(2.4 + 0.773 * pow(d + 0.00, 1.8)) + 3;
		FutilityMoveCounts[1][d] = int(2.9 + 1.045 * pow(d + 0.49, 1.8)) + 3;
	}
}

template<bool Root>
uint64_t Search::perft(Board& pos, Depth depth){
	assert(depth >= ONE_PLY);
	// depth = depth left
	BoardState st;
	uint64_t nodes = 0, tmp;
	const bool leaf = (depth == (2 * ONE_PLY)); // at that point, just use MoveList<LEGAL>.size()
	for(MoveList<LEGAL> it(pos); *it; it++){
		if(depth != ONE_PLY){
			pos.do_move(*it, st);
			tmp = (leaf ? MoveList<LEGAL>(pos).size() : perft<false>(pos, depth - ONE_PLY));
			if(Root){
				std::cout << Moves::format<false>(*it) << ": " << tmp << std::endl;
			}
			nodes += tmp;
			pos.undo_move(*it);
		} else {
			++nodes;
		}
	}
	return nodes;
}

template uint64_t Search::perft<true>(Board& pos, Depth depth); // explicit instantiation

std::string uci_pv(const Board& pos, Depth depth, Value alpha, Value beta){
	std::stringstream ss;
	int64_t elapsed = get_system_time_msec() - SearchTime;
	size_t PVLinesNum = 1; // TODO: Only 1 PV line for now
	// TODO: SelDepth
	for(size_t i = 0; i < PVLinesNum; i++){
		bool is_searched = (i <= PVIdx); // figure out if this one has been searched yet
		if(!is_searched && (depth == ONE_PLY)) continue;
		Depth d = (is_searched ? depth : (depth - ONE_PLY)); // if it hasn't been searched yet at this ply, then we use the previous ply search
		Value v = (is_searched ? RootMoves[i].score : RootMoves[i].prev_score);
		if(ss.rdbuf()->in_avail()){
			// rdbuf() returns pointer to internal buffer, in_avail()
			// returns chars available to read.
			ss << "\n"; // we have written stuff before - just a hack to avoid using a boolean variable
		}
		ss << "info depth " << d << " seldepth " << Depth(d + 1) /* TODO */
		   << " multipv " << (i + 1) << " score " << UCI::value(v);
		if(i == PVIdx){
			// OK, we are called right now? Then it might be to report a failed aspiration
			// window.
			if(v >= beta) ss << " lowerbound"; // failed high, so must be higher than beta
			else if(v <= alpha) ss << " upperbound"; // failed low, so must be lower than alpha
		}
		ss << " nodes " << uint64_t(0) /* TODO */ 
		   << " nps " << uint64_t(0) << " time " << elapsed << " pv";
		for(size_t j = 0; j < RootMoves[i].pv.size(); j++){
			ss << " " << UCI::move(RootMoves[i].pv[j]);
		}
	}
	return ss.str();
}		

void search_loop(Board& pos){
	Stack stack[MAX_PLY + 4], *ss = stack + 2; // for the fun (ss - 2) and (ss + 1)-type stuff
	std::memset(ss - 2, 0, 5 * sizeof(Stack)); // get the first few down
	Depth depth = DEPTH_ZERO; // what depth we are at right now
	Value best_val, alpha, beta, delta; // best = best value so far, alpha & beta are search window, delta is aspiration window delta
	best_val = alpha = delta = -VAL_INF;
	beta = VAL_INF;
	// TODO: TT.new_search();
	History.clear();
	while((++depth < DEPTH_MAX) && !Signals.stop && (!Limits.depth || (depth <= Limits.depth))){
		for(size_t i = 0; i < RootMoves.size(); i++){
			RootMoves[i].prev_score = RootMoves[i].score; // save scores from last iteration
		}
		size_t PVLinesNum = 1; // TODO
		PVLinesNum = std::min(PVLinesNum, RootMoves.size());
		for(PVIdx = 0; (PVIdx < PVLinesNum && !Signals.stop); PVIdx++){
			if(depth >= (5 * ONE_PLY)){
				delta = Value(16); // reset delta
				// And pick the most constraining alpha-beta combination based
				// on previous scores.
				alpha = std::max(RootMoves[PVIdx].prev_score - delta, -VAL_INF);
				beta = std::min(RootMoves[PVIdx].prev_score + delta, VAL_INF);
			}
			while(true){ // Aspiration window loop
				best_val = search<Root>(pos, ss, alpha, beta, depth, false); // false = isCutNode
				std::stable_sort(RootMoves.begin() + PVIdx, RootMoves.end()); // bring the new best move to the front
				for(size_t i = 0; i < RootMoves.size(); i++){
					RootMoves[i].insert_pv_in_tt(pos);
				}
				if(Signals.stop) break; // stop - no time or told to stop
				if(PVLinesNum == 1 && (best_val <= alpha || best_val >= beta) && (get_system_time_msec() - 3000 > SearchTime)){
					// Give UCI update when failing high/low (e.g. lowerbound/upperbound). //
					std::cout << uci_pv(pos, depth, alpha, beta) << std::endl;
				}
				if(best_val <= alpha){
					// Failed low. //
					// Note: Failing *LOW* at root is usually bad - means that we are in the midst of a window
					// that's overvalued - we might be losing a lot of stuff. Therefore, we need to let time
					// management know that we did fail low at root and get a better window.
					beta = (alpha + beta) / 2; // it seems like beta is working out, so let's push our luck a little
					alpha = std::max(best_val - delta, -VAL_INF); // decrease alpha
					Signals.failed_low_at_root = true; // we did after all
					Signals.stop_on_ponder_hit = false; // we need to complete this first
				} else if(best_val >= beta){
					// Failed high. //
					alpha = (alpha + beta) / 2; // push our luck with alpha
					beta = std::min(best_val + delta, VAL_INF); // increase beta
				} else {
					break;
				}
				delta += (delta / 2); // increase how quickly we increase our bounds exponentially (by a factor of 1.5)
				// TODO: Optimize above factor (2x is too quick, 1.5x may be a bit too slow)
				assert((alpha >= -VAL_INF) && (beta <= VAL_INF)); // just make sure
			}
			std::stable_sort(RootMoves.begin(), RootMoves.begin() + PVIdx + 1); // sort the lines that we have *already* searched so far
			if(Signals.stop){
				// TODO: Fully implement node count
				std::cout << "info nodes " << uint64_t(0) /* TODO */ << " time " << get_system_time_msec() - SearchTime << std::endl;
			} else if((PVIdx + 1 == PVLinesNum) || (get_system_time_msec() - 3000 > SearchTime)){
				// We display the PV for root only if we have just finished an entire root
				// PV line or it has already been more than 3 seconds since the 
				// search started.
				std::cout << uci_pv(pos, depth, alpha, beta) << std::endl;
			}
		}
		if(Limits.mate && (best_val >= VAL_MATE_IN_MAX_PLY) && ((VAL_MATE - best_val) <= (2 * Limits.mate))){
			Signals.stop = true; // we have completed the mate search and found the mate in the specified number of moves
		}
		if(Limits.use_time_manager() && !Signals.stop && !Signals.stop_on_ponder_hit){
			// Let's see if we even need another iteration (or have time for one). //
			if(RootMoves.size() == 1 || (get_system_time_msec() - SearchTime > TimeMgr.available_time())){
				// If we only have one move to search or we have exceeded optimal time, stop. //
				if(Limits.ponder) Signals.stop_on_ponder_hit = true;
				else Signals.stop = true;
			}
		}
	}
}

void update_pv(Move* pv, Move move, Move* child_pv){
	for(*pv++ = move; (child_pv && (*child_pv != MOVE_NONE)); ){
		*pv++ = *child_pv++; // copy PV into child PV
	}
	*pv = MOVE_NONE; // stop it right here
}

uint64_t failed_high_total, failed_high_first, failed_high_second; // for measuring move ordering

template<NodeType NT>
Value search(Board& pos, Stack* ss, Value alpha, Value beta, Depth depth, bool cut_node){
	const bool RootNode = (NT == Root);
	const bool PvNode = RootNode || (NT == PV);
	assert((-VAL_INF <= alpha) && (alpha < beta) && (beta <= VAL_INF));
	assert(PvNode || (alpha == (beta - 1))); // either this is a PV node or some zero window searches are being done
	BoardState st; // a BoardState we use for doing moves
	const bool in_check = pos.checkers();
	assert(depth > DEPTH_ZERO);
	const Value old_alpha = alpha;
	// Set Up Stack //
	Move pv[MAX_PLY + 1]; // PV used for children of PV nodes
	ss->ply = (ss-1)->ply + 1;
	assert(0 <= ss->ply && ss->ply < MAX_PLY);
	(ss+1)->skip_early_pruning = false;
	(ss+1)->reduction = DEPTH_ZERO;
	if(!RootNode){
		if(Signals.stop || pos.is_draw() || (ss->ply >= MAX_PLY)){
			return (ss->ply >= MAX_PLY && !in_check) ? Eval::evaluate(pos) : DrawValue[pos.side_to_move()];
		}
		// Mate Distance Pruning //
		alpha = std::max(alpha, mated_in(ss->ply));
		beta = std::min(mate_in(ss->ply + 1), beta);
		if(alpha >= beta) return alpha;
	}
	ss->static_eval = VAL_NONE;
	/*
	if(!in_check && !ss->skip_early_pruning){
		// Now let's try to prune as much as possible before entering the main move loop. //
		Value eval = ss->static_eval = ((ss - 1)->current_move != MOVE_NULL) ? Eval::evaluate(pos) : -(ss - 1)->static_eval; // TODO: Tempo add
		Side to_move = pos.side_to_move();
		const bool np_material = pos.pieces(to_move) & ~(pos.pieces(PAWN) | pos.pieces(KING)); // if the side to move has any non-pawn material or not
		// TODO: Razoring
		// Futility Pruning (Child Node) //
		if(!RootNode && (depth < 7 * ONE_PLY) && (eval - futility_margin(depth) >= beta) && (eval < VAL_KNOWN_WIN) && np_material){
			// OK, this node can't and likely won't do much - it is futile to search it.
			return eval - futility_margin(depth);
		}
		// Verified Null Move Pruning //
		if(!PvNode && (depth >= 2 * ONE_PLY) && (eval >= beta) && np_material){
			ss->current_move = MOVE_NULL; // a null move - literally
			// TODO
		}
		// ProbCut //
		if(!PvNode && (depth >= 5 * ONE_PLY) && (abs(beta) < VAL_MATE_IN_MAX_PLY)){
			// TODO: Implement in MoveSorter
		}
		// TODO: IID
	}
	*/
	// Main Move Loop //
	MoveSorter mi(pos, depth, History, ss);
	Move m = MOVE_NULL;
	Value score, best_score = -VAL_INF;
	const Bitboard pinned = pos.pinned(pos.side_to_move());
	const bool improving = (ss->static_eval >= (ss - 2)->static_eval) || (ss->static_eval == VAL_NONE) || ((ss - 2)->static_eval == VAL_NONE);
	unsigned int move_num = 0; // number of valid moves searched
	CheckInfo ci(pos);
	while((m = mi.next_move()) != MOVE_NONE){
		if(RootNode && !std::count(RootMoves.begin() + PVIdx, RootMoves.end(), m)){
			// At the root, the moves to search are already filled in by 
			// Threads.start_searching(), so we can check if this is
			// in that vector for a legality check.
			// Note: Also, this essentially implements the "searchmoves" option.
			continue;
		}
		++move_num;
		if(RootNode){
			Signals.first_root_move = (move_num == 1);
			if(get_system_time_msec() - 3000 > SearchTime){
				std::cout << "info depth " << (depth / ONE_PLY) << " currmove " << UCI::move(m) << " currmovenumber " << move_num << std::endl;
			}
		}
		if(PvNode){
			(ss + 1)->pv = NULL; // we haven't set its PV yet
		}
		Depth extension = DEPTH_ZERO;
		bool cap_or_prom = pos.is_capture(m) || (type_of(m) == PROMOTION);
		bool gives_check = (type_of(m) == NORMAL && !pos.lined(pos.side_to_move())) ? (ci.kattk[type_of(pos.at(from_sq(m)))] & to_sq(m)) : pos.gives_check(m, ci);
		bool dangerous = gives_check || (type_of(m) != NORMAL) || (type_of(pos.moved_piece(m)) == PAWN);
		// Checking Extension //
		if(gives_check && (pos.see_sign(m) >= VAL_ZERO)){
			extension = ONE_PLY;
		}
		Depth new_depth = depth - ONE_PLY + extension;
		Depth likely_depth = new_depth;
		// Shallow Pruning //
		if(!RootNode && !PvNode && move_num && !cap_or_prom && !in_check && !dangerous && (best_score > VAL_MATED_IN_MAX_PLY)){
			// OK, if it really won't hurt most likely, let's bite. //
			// TODO: Move count based pruning
			// TODO: Futility pruning for parent node
			likely_depth = new_depth - reduction<PvNode>(improving, depth, move_num); // TODO: improving = true for now
			// SEE-Based Pruning //
			if((likely_depth < 4 * ONE_PLY) && (pos.see_sign(m) < VAL_ZERO)){
				continue; // skip this move
			}
		}
		// TODO: Extensions, Reductions
		if(!RootNode && !pos.legal(m, pinned)){
			// If illegal, skip it (late so we might get a change to prune this). //
			--move_num;
			continue;
		}
		ss->current_move = m; // set the current move
		// Do Move //
		pos.do_move(m, st);
		bool do_full_depth_search;
		// LMR //
		if((depth >= 3 * ONE_PLY) && (move_num > 1) && !cap_or_prom){
			ss->reduction = reduction<PvNode>(improving, depth, move_num);
			// Increase reduction if it has a bad history value or it is a cut node. //
			if((!PvNode && cut_node) || (History[pos.at(to_sq(m))][to_sq(m)])){
				ss->reduction += ONE_PLY;
			}
			// If this evades a capture, don't reduce it as much. //
			if((ss->reduction != DEPTH_ZERO) && (type_of(m) == NORMAL) && (type_of(pos.at(to_sq(m))) != PAWN) && (pos.see(make_move(to_sq(m), from_sq(m))) < VAL_ZERO)){
				ss->reduction = std::max(DEPTH_ZERO, ss->reduction - ONE_PLY);
			}
			Depth d = std::max(new_depth - ss->reduction, ONE_PLY);
			score = -search<NonPV>(pos, (ss + 1), -(alpha + 1), -alpha, d, true); // true = cut node
			if(score > alpha && (ss->reduction >= 4 * ONE_PLY)){
				// Search again with a bigger depth if the move was reduced a huge amount and seems
				// promising.
				Depth d2 = std::max(new_depth - 2 * ONE_PLY, ONE_PLY);
				score = -search<NonPV>(pos, (ss + 1), -(alpha + 1), -alpha, d2, true);
			}
			do_full_depth_search = (score > alpha && ss->reduction != DEPTH_ZERO);
			ss->reduction = DEPTH_ZERO;
		} else {
			do_full_depth_search = !PvNode || (move_num > 1);
		}
		if(do_full_depth_search){
			// Full Depth Search //
			score = (new_depth < ONE_PLY) ? 
					(gives_check ? (-qsearch<NonPV, true>(pos, (ss + 1), -(alpha + 1), -alpha, DEPTH_ZERO))
								 : (-qsearch<NonPV, false>(pos, (ss + 1), -(alpha + 1), -alpha, DEPTH_ZERO)))
								 : (-search<NonPV>(pos, (ss + 1), -(alpha + 1), -alpha, new_depth, !cut_node)); // since the children of a cut-node cannot also be a cut-node (cut-nodes are only at odd plies)
		}
		if(PvNode && ((move_num == 1) || (score > alpha && (RootNode || score < beta)))){
			// PVS //
			// This is a PV node which is either the first move to search, or it failed low at root or 
			// was within the alpha-beta window.
			(ss + 1)->pv = pv; // setup next PV
			(ss + 1)->pv[0] = MOVE_NONE; // reset next first move
			score = (new_depth < ONE_PLY) ? 
					(gives_check ? (-qsearch<PV, true>(pos, (ss + 1), -beta, -alpha, DEPTH_ZERO))
								 : (-qsearch<PV, false>(pos, (ss + 1), -beta, -alpha, DEPTH_ZERO)))
								 : (-search<PV>(pos, (ss + 1), -beta, -alpha, new_depth, false)); // is_cut_node = false since we don't know if the next one is for sure
		}
		pos.undo_move(m);
		if(Signals.stop){
			// Before updating anything important, let's stop here if we are out of time. //
			return VAL_ZERO; // stopped
		}
		if(RootNode){
			// If we are at the root, we have to keep the RootMove stuff updated, including PV's. //
			RootMove& rm = *std::find(RootMoves.begin(), RootMoves.end(), m);
			if(move_num == 1 || score > alpha){ // either this is the first move or it is the new best move
				rm.score = score;
				rm.pv.resize(1);
				assert((ss + 1)->pv); // make sure it is not NULL
				for(Move* m = (ss + 1)->pv; *m != MOVE_NONE; m++){
					rm.pv.push_back(*m);
				}
			} else {
				rm.score = -VAL_INF; // we use a stable sort, so no worries about this - just to make sure it gets pushed to the end
			}
		}
		if(score > best_score){
			best_score = score;
			if(score > alpha){
				// We only update alpha if it is a PV node. //
				if(PvNode && !RootNode){
					// If this is a PV node not at the root, record its best move in the PV of its child as well. //
					update_pv(ss->pv, m, (ss + 1)->pv);
				}
				if(PvNode && (score < beta)){ // If we haven't failed high and it is a PV node
					alpha = score; // then update alpha
				} else {
					assert(score >= beta); // fail-high
					break;
				}
			}
		}
	}
	if(!move_num){
		// No valid moves at this position, so we must be in either checkmate or stalemate. //
		if(pos.checkers()) return mated_in(ss->ply);
		else return DrawValue[pos.side_to_move()];
	}
	if(best_score >= beta && !in_check && !pos.is_capture(m) && (type_of(m) != PROMOTION)){
		// TODO: Penalty for all quiet moves that didn't do anything
		const Value bonus = Value(int(depth) * int(depth));
		History.register_update(pos.moved_piece(m), to_sq(m), bonus);
		if(move_num == 1) ++failed_high_first;
		else if(move_num == 2) ++failed_high_second;
		++failed_high_total;
	}
	assert(best_score > -VAL_INF);
	return best_score;
}

template<NodeType NT, bool InCheck>
Value qsearch(Board& pos, Stack* ss, Value alpha, Value beta, Depth depth){
	// Note: 'depth' can be negative.
	const bool PvNode = (NT == PV);
	assert(NT != Root);
	assert(InCheck == bool(pos.checkers()));
	assert((-VAL_INF <= alpha) && (alpha < beta) && (beta <= VAL_INF));
	assert(PvNode || (alpha == beta - 1));
	assert(depth <= DEPTH_ZERO);
	Move pv[MAX_PLY + 1]; // for giving the PV to children
	Move best_move, m;
	BoardState st;
	Value best_score, old_alpha, score;
	if(PvNode){
		old_alpha = alpha; // for TT reference
		(ss + 1)->pv = pv; // give child a PV
		ss->pv[0] = MOVE_NONE; // reset our own PV (since we can't rely on search() to do it for us necessarily)
	}
	ss->current_move = best_move = MOVE_NONE;
	ss->ply = (ss - 1)->ply + 1;
	// Check for draws, going over maximum ply. //
	if(pos.is_draw() || (ss->ply >= MAX_PLY)){
		return (ss->ply >= MAX_PLY && !InCheck) ? (Eval::evaluate(pos)) : (DrawValue[pos.side_to_move()]);
	}
	assert((0 <= ss->ply) && (ss->ply < MAX_PLY));
	// TODO: TT Lookup
	// TODO: Futility base
	if(InCheck){
		ss->static_eval = VAL_NONE;
		best_score = -VAL_INF;
	} else {
		ss->static_eval = best_score = ((ss - 1)->current_move != MOVE_NULL) ? (Eval::evaluate(pos)) : (-(ss - 1)->static_eval); // TODO: Add Eval::Tempo
		// Stand pat if possible. //
		if(best_score >= beta){
			// TODO: Save to TT
			return best_score;
		}
		if(PvNode && (best_score > alpha)){
			alpha = best_score;
		}
	}
	MoveSorter mp(pos, depth, History, to_sq((ss - 1)->current_move)); // const Board& p, Depth d, const HistoryTable& ht, Square s
	CheckInfo ci(pos);
	const Bitboard pinned = pos.pinned(pos.side_to_move());
	while((m = mp.next_move()) != MOVE_NONE){
		assert(is_ok(m));
		bool gives_check = (type_of(m) == NORMAL && !pos.lined(pos.side_to_move())) ? (ci.kattk[type_of(pos.at(from_sq(m)))] & to_sq(m)) : pos.gives_check(m, ci);
		// TODO: Futility pruning with Futility Base
		bool is_prunable_evasion = InCheck && (best_score > VAL_MATED_IN_MAX_PLY) && !pos.is_capture(m) && !pos.can_castle(CastlingRight((WHITE_OO | WHITE_OOO) << (2 * pos.side_to_move())));
		if((!InCheck || is_prunable_evasion) && (type_of(m) != PROMOTION) && (pos.see_sign(m) < VAL_ZERO)){
			continue; // avoid losing captures (unless in check)
		}
		if(!pos.legal(m, pinned)){
			continue; // illegal move
		}
		ss->current_move = m; // set current move
		pos.do_move(m, st);
		score = gives_check ? (-qsearch<NT, true>(pos, (ss + 1), -beta, -alpha, (depth - ONE_PLY))) : (-qsearch<NT, false>(pos, (ss + 1), -beta, -alpha, (depth - ONE_PLY)));
		pos.undo_move(m);
		assert((score > -VAL_INF) && (score < VAL_INF));
		if(score > best_score){
			best_score = score;
			if(score > alpha){
				if(PvNode){
					update_pv(ss->pv, m, (ss + 1)->pv); // copy PV from child
				}
				if(PvNode && (score < beta)){
					alpha = score;
					best_move = m;
				} else {
					// Fail-high. //
					// TODO: TT Save
					return score;
				}
			}
		}
	}
	if(InCheck && (best_score == -VAL_INF)){
		// In check and no legal moves? Must be checkmate. //
		// TODO: Verify that this was not the result of pruning moves
		return mated_in(ss->ply);
	}
	// TODO: TT Save
	return best_score;
}

void Search::think(void){
	// This is the externally available way to launch a search. //
	// Note: The SearchLimits, SearchTime, etc. should already
	// be correctly initialized and set to the values provided
	// by the GUI.
	failed_high_total = failed_high_first = failed_high_second = 0;
	Side to_move = RootPos.side_to_move();
	TimeMgr.init(Limits, to_move, RootPos.get_ply());
	Value contempt = VAL_ZERO; // TODO: Base this on game phase
	DrawValue[to_move] = VAL_DRAW - contempt;
	DrawValue[~to_move] = VAL_DRAW + contempt;
	if(RootMoves.empty()){
		// Ummm... what? No moves available? We're in trouble...
		std::cout << "info depth 0 score " << UCI::value(RootPos.checkers() ? VAL_MATE : VAL_DRAW) << std::endl;
	} else {
		Threads.timer->run = true;
		Threads.timer->notify_one();
		search_loop(RootPos);
		Threads.timer->run = false; // stop the timer
	}
	if(!Signals.stop && (Limits.ponder || Limits.infinite)){
		Signals.stop_on_ponder_hit = true;
		Threads.main_thread->wait_for(Signals.stop);
	}
	std::cout << "bestmove " << UCI::move(RootMoves[0].pv[0]);
	if(RootMoves[0].pv.size() > 1 || RootMoves[0].extract_ponder_from_tt(RootPos)){
		std::cout << " ponder " << UCI::move(RootMoves[0].pv[1]);
	}
	std::cout << std::endl;
	printf("# Of %llu moves, %llu were on the first try and %llu on the second, so move ordering is %.3f%% (or tot. %.3f%%).\n", failed_high_total, failed_high_first, failed_high_second, double(failed_high_first) / double(failed_high_total) * 100.0, double(failed_high_first + failed_high_second) / double(failed_high_total) * 100.0);
}

void Search::check_time_limit(void){
	// This is called by the timer thread periodically to check
	// if we need to stop the search because time is up.
	int64_t now = get_system_time_msec();
	int64_t elapsed = now - SearchTime;
	if(Limits.ponder){
		// Only the GUI tells us to stop pondering. //
		return;
	}
	if(Limits.use_time_manager()){
		bool past_time = Signals.first_root_move && !Signals.failed_low_at_root && (elapsed > TimeMgr.available_time() * 75 / 100);
		if(past_time || (elapsed > (TimeMgr.maximum_time() - (2 * TimerThread::PollEvery)))){
			Signals.stop = true;
		}
	} else if(Limits.movetime && (elapsed >= Limits.movetime)){ // if specific amount of time for moving, and we have exhausted that
		Signals.stop = true; // then we are done
	} else if(Limits.nodes){
		assert(false && ("Have not implemented node counters yet!"));
	}
}











































