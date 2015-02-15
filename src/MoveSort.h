#ifndef MSORT_INCLUDED
#define MSORT_INCLUDED

#include "Common.h"
#include "Bitboards.h"
#include "Board.h"
#include "MoveGen.h"
#include "Search.h"

template<typename T>
struct Stats {
	static const Value Max = Value(256);
	
	void clear(void){
		std::memset(table, 0, sizeof(table));
	}
	
	const T* operator[](Piece pc) const {
		// Returns an array given the moving piece
		// that contains the scores and can be
		// indexed by [square].
		return table[pc];
	}
	
	void register_update(Piece pc, Square to, Value v){
		// This increases the score of the given moving piece/dest. square
		// combo given the added score.
		if(abs(table[pc][to] + v) < Max){
			table[pc][to] += v;
		}
	}
private:
	T table[PIECE_NB][SQUARE_NB];
};

typedef Stats<Value> HistoryTable;

class MoveSorter {
	public:
		MoveSorter(const Board& pos, Depth d, const HistoryTable& hst, Search::Stack* ss); // for main search
		MoveSorter(const Board& pos, Depth d, const HistoryTable& hst, Square s); // for QS search
		
		Move next_move(void); // get the next move we should search
	private:
		void gen_next_stage(void);
		template<GenType> void score(void);
		
		const Board& pos;
		const HistoryTable& hst;
		Search::Stack* ss;
		Depth depth;
		int stage;
		Square recap_sq;
		// TODO: Killers
		ActMove *cur, *end, *end_quiets, *end_bad_captures;
		ActMove moves[MAX_MOVES];
};

#endif // #ifndef MSORT_INCLUDED