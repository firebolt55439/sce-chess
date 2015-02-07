#ifndef MGEN_INCLUDED
#define MGEN_INCLUDED

#include "Common.h"
#include "Bitboards.h"
#include "Board.h"

namespace Moves {
	void init(void);
	template<bool Algebraic> std::string format(Move m);
}

struct ActMove {
	Move move; // the move itself
	Value score; // move score (used for move ordering)
};

enum GenType {
	NON_EVASIONS, // not check evasions
	EVASIONS, // check evasions
	LEGAL // driver
};

template<GenType>
ActMove* generate_moves(const Board& pos, ActMove* list); // returns end of list (meaning one *after* last move generated)

template<GenType T>
struct MoveList {
	// Holds pseudo-legal moves generated and provides useful methods. //
	private:
		ActMove moves[MAX_MOVES];
		ActMove *cur, *last; // (cur = what we are on, last = the last one)
	public:
		explicit MoveList(const Board& pos) : cur(moves), last(generate_moves<T>(pos, moves)) {
			last->move = MOVE_NONE;
		}
		
		void operator++(void){ ++cur; }
		void operator++(int){ ++cur; }
		
		Move operator*(void) const {
			return cur->move;
		}
		
		size_t size(void) const {
			return last - moves; // pointer arithmetic is overloaded to allow this
		}
		
		bool contains(Move m) const {
			for(ActMove* it(moves); it != last; it++){
				if(it->move == m) return true;
			}
			return false;
		}
};

#endif // #ifndef MGEN_INCLUDED