#ifndef BOOK_INC
#define BOOK_INC

#include "Common.h"
#include "Board.h"
#include "PGN.h"
#include <vector>

enum Book_Flag : int {
	BLUNDER, // ??
	BAD, // ?
	EQUAL, // = (equal position)
	GOOD, // !
	GREAT, // !!
	// After this are reserved for *possible* future use.
	BWIN1, // black won at least one game
	DR1, // at least one draw
	WWIN1 // white won at least one game
};

struct Book_Position {
	uint64_t hash; // the hash for the position (all 64 bits so less collisions)
	uint64_t info; // this stores information about this position
	/*
	* bits 0-7: the "flag" for the move - good, bad, great, etc.
	* bits 8-31: the number of times this move has appeared in games (e.g. count number of times played in GM games)
	* bits 32-63: the learned value of the move (should be bitcasted to a float)
	*/
	
	void add_flag(Book_Flag flag){
		info |= (1ULL << int(flag)); // since first 8 bits are for flag
	}
	
	void remove_flag(Book_Flag flag){
		info |= ~(1ULL << int(flag));
	}
	
	inline bool has_flag(Book_Flag flag) const {
		return (info & (1ULL << int(flag)));
	}
	
	void set_num(uint32_t to){
		// Note: Only 24 bits are used.
		to &= uint32_t(0xFFFFFF); // 0xFF = 2^8 (256), therefore 0xFFFFFF = 2^24
		info &= ~(uint64_t(0xFFFFFF) << 8); // clear num
		info |= uint64_t(to) << 8; // set num
	}
	
	inline uint32_t get_num(void) const {
		return uint32_t((info >> 8) & uint64_t(0xFFFFFF));
	}
	
	void set_learn(float to){
		info &= uint64_t(uint64_t(~uint32_t(0)) << 32);
		info |= static_cast<uint64_t>(to) << 32;
	}
	
	inline float get_learn(void) const {
		return static_cast<float>(uint32_t(info >> 32));
	}
}; // 128 bits or 16 bytes per position

struct Book_Move {
	Book_Position bpos; // the book position after playing a valid move 'move'
	Move move; // the move played to get to the specified position (requires context)
	int score; // score given by sorting
};

// Therefore, 1 byte stores 2^(-4) positions, 1 kilobyte stores 2^6 positions, 1 MB stores 2^16 positions, 1 GB stores 2^26 positions, etc., etc.

/*
* Book Format:
* A set of positions with attributes which can then be searched or modified.
* Moves are not stored; rather, the result of the move and the subsequent hash
* are searched for in the book.
* The "flag" of the move, the "learned" value, and the number of times the
* move was played are the criteria for sorting through book moves.
*/

struct Book_Skill {
	// This structure determines the sorting of book moves. //
	int variance; // 0 to 100 - determines deviation from the "optimal" sort (the higher, the more possibly weaker)
	int forgiveness; // 0 to 100 - determines how liable a move learned to be bad (e.g. learn < 0) is to possibly being "forgiven" (or basically, how much a negative learned value counts against the move)
};

class Book {
	private:
		std::string data; // the raw book data (std::string is binary-safe)
		unsigned int offset_of(uint64_t hash, bool& found); // get offset of given hash in string 'data'
	public:
		Book(std::string dt) : data(dt) { }
		~Book(void){ }
		
		static void init(void);
		friend Book& operator<<(Book& book, PGN_Game& game); // take the given game, process it, and add it to this book
		
		std::string& get_book(void){ return data; }
		void add_position(Book_Position pos); // add a new position to the book
		Book_Position get_position_at(unsigned int off); // get a position at a specified offset
		void update_position(uint64_t hash, Book_Position with); // replaces position that has the specified hash with the given position
		void remove_position(uint64_t hash); // remove the position with the given hash from the book
		
		void sort_results_by(std::vector<Book_Move>& results, Book_Skill by); // sort the given results by the specified "skill level"
		std::vector<Book_Move> results_for(Board& pos); // get all book moves for specified board position
};

#endif // #ifndef BOOK_INC