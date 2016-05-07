#ifndef TMAN_INC
#define TMAN_INC

#include "Common.h"
#include "Bitboards.h"
#include "Board.h"
#include "Search.h"

class TimeManager {
	public:
		const int MinThinkingTime = 20; // in milliseconds
		// This is our time manager, which decides how much time to allocate per move
		// given the limits from the GUI;
		void init(const Search::SearchLimits& limits, Side us, int ply);
	
		int available_time(void) const {
			return int(optimal_search_time * 0.71 * 1.2); // 1.2 acts like our "PV instability" factor here
		}
	
		int maximum_time(void) const {
			return max_search_time;
		}
	
	private:
		int optimal_search_time; // what we strive for
		int max_search_time; // how much we can "push it"
		// TODO: PV instability factor
};

#endif // #ifndef TMAN_INC