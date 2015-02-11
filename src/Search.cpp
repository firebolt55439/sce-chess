#include "Common.h"
#include "Bitboards.h"
#include "Board.h"
#include "MoveGen.h"
#include "MoveSort.h"
#include "Evaluation.h"
#include "Search.h"
#include "Threads.h"
#include "TimeManager.h"

// Externally Available Data //
namespace Search {
	volatile SearchSignals Signals;
	SearchLimits Limits;
	RootMoveVector RootMoves;
	Board RootPos;
	int64_t SearchTime; // the start of the search time, in milliseconds
	BoardStateStack SetupStates;
}

// UCI //

std::string UCI::value(Value v){
	std::stringstream ss;
	if(abs(v) < VAL_MATE_IN_MAX_PLY){
		ss << "cp " << (v * 100 / PawnValueEg);
	} else {
		ss << "mate " << ((v > 0 ? (VAL_MATE - v + 1) : (-VAL_MATE -v)) / 2); // since getting mated means negative mate score (e.g. "mate -2" means "mated in 2")
	}
	return ss.str();
}

std::string UCI::move(Move m){
	return Moves::format<false>(m);
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
}

template<NodeType NT>
Value search(Board& pos, Stack* ss, Value alpha, Value beta, Depth depth, bool cut_node);

template<NodeType NT, bool InCheck>
Value qsearch(Board& pos, Stack* ss, Value alpha, Value beta, Depth depth);

void search_loop(Board& pos); // main iterative deepening loop

void Search::init(void){
	// TODO: Init reduction array, move count pruning array, etc.
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
		for(size_t PVIdx = 0; (PVIdx < RootMoves.size() && !Signals.stop); PVIdx++){
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
				// TODO: Give upperbound/lowerbound update as UCI PV's
				if(best_val <= alpha){
					// Failed low. //
					beta = (alpha + beta) / 2; // it seems like beta is working out, so let's push our luck a little
					alpha = std::max(best_val - delta, -VAL_INF); // decrease alpha
					Signals.stop_on_ponder_hit = false; // we need to complete this first
				} else if(best_val >= beta){
					// Failed high. //
					alpha = (alpha + beta) / 2; // push our luck with alpha
					beta = std::min(best_val + delta, VAL_INF); // increase beta
				} else break;
				delta += (delta / 2); // increase how quickly we increase our bounds exponentially (by a factor of 1.5)
				assert((alpha >= -VAL_INF) && (beta <= VAL_INF)); // just make sure
			}
			std::stable_sort(RootMoves.begin(), RootMoves.begin() + PVIdx + 1); // sort what we have *already* searched so far
			// TODO: UCI update
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

template<NodeType NT>
Value search(Board& pos, Stack* ss, Value alpha, Value beta, Depth depth, bool cut_node){
	const bool RootNode = (NT == Root);
	const bool PvNode = RootNode || (NT == PV);
	assert((-VAL_INF <= alpha) && (alpha < beta) && (beta <= VAL_INF));
	assert(PvNode || (alpha == (beta - 1))); // either this is a PV node or some zero window searches are being done
	BoardState st; // a BoardState we use for doing moves
	const bool in_check = pos.checkers();
	assert(depth >= DEPTH_ZERO);
	if((depth == DEPTH_ZERO) || (ss->ply >= MAX_PLY)){
		// Recursive Base Case //
		return Eval::evaluate(pos);
	}
	const Value old_alpha = alpha;
	if(pos.is_draw()){
		// Position is Drawn //
		return DrawValue[pos.side_to_move()];
	}
	// Set Up Stack //
	ss->ply = (ss-1)->ply + 1;
	assert(0 <= ss->ply && ss->ply < MAX_PLY);
	(ss+1)->skip_early_pruning = false;
	(ss+1)->reduction = DEPTH_ZERO;
	// Mate Distance Pruning //
	alpha = std::max(alpha, mated_in(ss->ply));
	beta = std::min(mate_in(ss->ply + 1), beta);
	if(alpha >= beta) return alpha;
	// Main Move Loop //
	MoveSorter mi(pos, depth, History, ss);
	Move m = MOVE_NULL;
	Value score, best_score = -VAL_INF;
	const Bitboard pinned = pos.pinned(pos.side_to_move());
	unsigned int move_num = 0; // number of valid moves searched
	while((m = mi.next_move()) != MOVE_NONE){
		if(!pos.legal(m, pinned)){
			continue;
		}
		++move_num;
		pos.do_move(m, st);
		Depth new_depth = depth - ONE_PLY;
		// TODO: Extensions, Reductions
		score = -search<NonPV>(pos, (ss + 1), -(alpha + 1), -alpha, new_depth, !cut_node); // since the children of a cut-node cannot also be a cut-node
		pos.undo_move(m);
		if(score >= beta){
			// Fail-high, so update history table for use with move ordering. //
			// TODO: Penalty for all other moves that did not fail high.
			const Value bonus = Value(int(depth) * int(depth));
			History.register_update(pos.moved_piece(m), to_sq(m), bonus);
			return beta;
		}
		if(score > best_score){
			best_score = score;
			if(score > alpha){
				alpha = score;
			}
		}
	}
	if(!move_num){
		// No valid moves at this position, so we must be in either checkmate or stalemate. //
		if(pos.checkers()) return mated_in(ss->ply);
		else return DrawValue[pos.side_to_move()];
	}
	return best_score;
}

void Search::think(void){
	// This is the externally available way to launch a search. //
	// Note: The SearchLimits, SearchTime, etc. should already
	// be correctly initialized and set to the values provided
	// by the GUI.
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
		bool past_time = (elapsed > TimeMgr.available_time() * 75 / 100);
		if(past_time || (elapsed > (TimeMgr.maximum_time() - (2 * TimerThread::PollEvery)))){
			Signals.stop = true;
		}
	} else if(Limits.movetime && (elapsed >= Limits.movetime)){ // if specific amount of time for moving, and we have exhausted that
		Signals.stop = true; // then we are done
	} else if(Limits.nodes){
		assert(false && ("Have not implemented node counters yet!"));
	}
}











































