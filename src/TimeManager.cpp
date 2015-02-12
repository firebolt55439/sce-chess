#include "Common.h"
#include "Bitboards.h"
#include "Board.h"
#include "MoveGen.h"
#include "Evaluation.h"
#include "TimeManager.h"
#include <cmath>
#include <cstdlib>
#include <cfloat>

static const int MoveHorizon = 50; // we only plan this many moves ahead (e.g. when dividing total time into moves)
const double MaxRatio = 7.0; // the highest ratio we can possibly, in the worst case scenario, step over
							 // reserved time by - and nearly 7x is a lot...
const double StealRatio = 0.33; // don't steal remaining moves' time by above this ratio however

enum {
	OptimalTime,
	MaxTime
};

double move_importance(int ply){
	// Basically, this formula approximates statistics based on how many
	// games are undecided after n halfmoves/plies.
	// That way, more time is spent on important moves, and less on less
	// important moves;
	// Note: This is a skew-logistic function, a type of probability
	// distribution.
	const double XScale = 9.3, XShift = 59.8;
	const double Skew = 0.172;
	// Function: y=10|_cdot_({1 + exp({|_frac_{{({x-59.8})};{9.3}}})})^{({-0.172})}
	// (Function multiplied by 10 to increase magnitude for easier viewing)
	// Goes "straight" at the beginning, but starts dipping rapidly, until it
	// approaches its asymptotic lower bound.
	return pow((1 + exp((ply - XShift) / XScale)), -Skew) + DBL_MIN; // +DBL_MIN simply makes sure it isn't zero
}

template<int T>
int remaining_time(int my_time, int mtg, int ply){
	// mtg = "moves to go", my_time = how much time we have
	const double max_ratio = (T == OptimalTime ? 1 : MaxRatio);
	const double steal_ratio = (T == OptimalTime ? 0 : StealRatio);
	double importance = move_importance(ply) * 0.8; // this move's importance (pared down a bit)
	double other_importance = 0.0; // other moves' importance
	for(int i = 1; i < mtg; i++){
		// Get the rest of the moves' importance. //
		other_importance += move_importance(ply + (2 * i)); // since a move = 2 halfmoves/plies
	}
	double option_1 = (max_ratio * importance) / (max_ratio * importance + other_importance); // pretty straightfoward actually - find fraction of total time to use based on importance
	double option_2 = (importance + steal_ratio * other_importance) / (importance + other_importance); // our importance plus how much we can steal over total importance of all moves, incl. current
	return int(my_time * std::min(option_1, option_2)); // we take whatever requires less time
}

void TimeManager::init(const Search::SearchLimits& limits, Side us, int ply){
	// TODO: Consider UCI options like minimum thinking time, move overhead, 
	// slow mover, etc.
	/*
	option name Write Debug Log type check default false
	option name Contempt type spin default 0 min -100 max 100
	option name Min Split Depth type spin default 0 min 0 max 12
	option name Threads type spin default 1 min 1 max 128
	option name Hash type spin default 16 min 1 max 65536
	option name Clear Hash type button
	option name Ponder type check default true
	option name MultiPV type spin default 1 min 1 max 500
	option name Skill Level type spin default 20 min 0 max 20
	option name Move Overhead type spin default 30 min 0 max 5000
	option name Minimum Thinking Time type spin default 20 min 0 max 5000
	option name Slow Mover type spin default 80 min 10 max 1000
	option name UCI_Chess960 type check default false
	*/
	const int MinThinkingTime = 20; // in milliseconds
	optimal_search_time = max_search_time = std::max(limits.time[us], MinThinkingTime); // initially, optimal = amount of time left for us on the clock, but at least 20 milliseconds
	const int MaxMTG = (limits.movestogo ? std::min(limits.movestogo, MoveHorizon) : MoveHorizon);
	for(int i = 0; i < MaxMTG; i++){
		// We now look into the future - yeah, for sure - and optimize the time
		// for this move.
		int my_time = limits.time[us] + (limits.inc[us] * (i - 1)) - (30 * (2 + std::min(i, 40))); // find how much time we "have" and also subtract the overhead costs of making moves, etc.
		my_time = std::max(my_time, 0); // can't be negative
		int pos_optimal = MinThinkingTime + remaining_time<OptimalTime>(my_time, i, ply);
		int pos_max = MinThinkingTime + remaining_time<MaxTime>(my_time, i, ply);
		optimal_search_time = std::min(pos_optimal, optimal_search_time); // we try to reduce time as much as possible
		max_search_time = std::min(pos_max, max_search_time);
	}
	if(limits.ponder){
		optimal_search_time += (optimal_search_time / 4); // if we are pondering, please think a bit more
	}
	optimal_search_time = std::min(optimal_search_time, max_search_time); // just want to make sure that optimal is <= max always
	//printf("optimal search time: %f seconds\n", optimal_search_time / 1000.0);
}







































