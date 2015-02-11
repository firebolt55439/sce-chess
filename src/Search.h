#ifndef SEARCH_INC
#define SEARCH_INC

#include "Common.h"
#include "Bitboards.h"
#include "Board.h"
#include <memory>
#include <stack>
#include <ctime>

namespace Search {
	struct Stack {
		Move* pv; // the principal variation
		int ply; // ply at
		Move cur_move; // move just played
		Depth reduction; // reduction, if/a
		// TODO: Killers, TT move, static eval, etc.
		bool skip_early_pruning; // whether we should skip early pruning or not (for stuff like ProbCut, etc.)
	};
	
	struct RootMove {
		// Keep track of a move at a root, store stuff like PV, score, etc. //
		Value score; // current score
		Value prev_score; // previous score
		std::vector<Move> pv; // the PV
		
		RootMove(Move m) : score(-VAL_INF), prev_score(-VAL_INF), pv(1, m) { } // the std::vector fill constructor is called
		
		bool operator<(const RootMove& m) const {
			// We want an ascending sort here. //
			return score > m.score;
		}
		
		bool operator==(const RootMove& m) const {
			return pv[0] == m.pv[0]; // since the first move in the PV is the actual root move
		}
		
		void insert_pv_in_tt(Board& pos){ // for re-inserting the PV in the TT
			// TODO
			return;
		}
		
		Move extract_ponder_from_tt(Board& pos){ // for extracting the ponder move, PV, etc. from the TT
			// TODO
			return MOVE_NONE;
		}
	};
	
	typedef std::vector<RootMove> RootMoveVector; // that logic
	
	struct SearchLimits {
		std::vector<Move> SearchMoves; // which moves to search, if/a
		int time[SIDE_NB], inc[SIDE_NB]; // time and increment by side (for asymmetric time controls/handicap)
		int movestogo; // moves to go until clock reset
		int depth; // only search to a specified depth (e.g. "go depth 6")
		int movetime; // if/a
		int mate; // for mate searches
		int infinite; // if we are doing an infinite search (e.g. "go infinite")
		int ponder; // if we are pondering
		int64_t nodes; // stop at a certain number of nodes
		
		SearchLimits(void){
			std::memset(this, 0, sizeof(SearchLimits));
		}
		
		bool use_time_manager(void){
			// If we should use our own time manager or not. //
			return !(mate | movetime | depth | nodes | infinite);
		}
	};
	
	struct SearchSignals {
		bool stop; // whether to stop/terminate search
		bool stop_on_ponder_hit; // stop if we get a "ponder hit"
	};
	
	typedef std::unique_ptr<std::stack<BoardState> > BoardStateStack;
	
	extern volatile SearchSignals Signals;
	extern SearchLimits Limits;
	extern RootMoveVector RootMoves;
	extern Board RootPos;
	extern int64_t SearchTime; // the start of the search time, in milliseconds
	extern BoardStateStack SetupStates;
	
	void init(void);
	void think(void);
	void check_time_limit(void); // for TimerThread
	
	template<bool Root> uint64_t perft(Board& pos, Depth depth);
}

namespace UCI {
	std::string value(Value v);
	std::string move(Move m);
}

#endif // #ifndef SEARCH_INC