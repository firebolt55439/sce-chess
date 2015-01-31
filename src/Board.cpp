#include "Common.h"
#include "Bitboards.h"
#include "Board.h"

void Board::clear(void){
	std::memset(this, 0, sizeof(Board));
	orig_st.epsq = SQ_NONE;
	st = &orig_st;
}