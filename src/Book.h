#ifndef BOOK_INC
#define BOOK_INC

#include "Common.h"
#include "Board.h"
#include "MoveGen.h"
#include "MoveSort.h"
#include "Evaluation.h"
#include "Search.h"

struct Book_Position {
	uint64_t hash; // the hash for the position (all 64 bits so less collisions)
	uint64_t info; // this stores information about this position
	/*
	* bits 0-7: the "flag" for the move - good, bad, great, etc.
	* bits 8-31: the number of times this move has appeared in games (e.g. count number of times played in GM games)
	* bits 32-63: the learned value of the move (should be bitcasted to a float)
	*/
	
	void set_num(uint32_t to){
		// Note: Only 24 bits are used.
		to &= uint32_t(0xFFFFFF); // 0xFF = 2^8 (256), therefore 0xFFFFFF = 2^24
		info &= ~(uint64_t(0xFFFFFF) << 8); // clear num
		info |= uint64_t(to) << 8; // set num
	}
	
	uint32_t get_num(void){
		return uint32_t((info >> 8) & uint64_t(0xFFFFFF));
	}
	
	void set_learn(float to){
		info &= uint64_t(uint64_t(~uint32_t(0)) << 32);
		info |= static_cast<uint64_t>(to) << 32;
	}
	
	float get_learn(void){
		return static_cast<float>(uint32_t(info >> 32));
	}
}; // 128 bits or 16 bytes per position

// Therefore, 1 byte stores 2^(-4) positions, 1 kilobyte stores 2^6 positions, 1 MB stores 2^16 positions, 1 GB stores 2^26 positions, etc., etc.

/*
* Book Format:
* A set of positions with attributes which can then be searched or modified.
* Moves are not stored; rather, the result of the move and the subsequent hash
* are searched for in the book.
* The "flag" of the move, the "learned" value, and the number of times the
* move was played are the criteria for sorting through book moves.
*/














#endif // #ifndef BOOK_INC