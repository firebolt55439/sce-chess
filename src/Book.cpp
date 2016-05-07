#include "Common.h"
#include "Board.h"
#include "Search.h"
#include "Evaluation.h"
#include "Threads.h"
#include "Book.h"
#include "UCI.h"
#include "MoveGen.h"
#include <stack>
#include <fstream>
#include <sstream>

void Book::init(void){
	// Search for an opening book 'book.sce' if there is one. //
	std::ifstream ifp("book.sce");
	if(ifp.is_open()){
		std::string data = ReadEntireFile(ifp);
		ifp.close();
		if(!(data.size() % sizeof(Book_Position))){ // if it's valid
			Search::EngineBook.get_book() = data;
			Book_Skill skill;
			skill.variance = 5; // should vary a *bit*
			skill.forgiveness = 0; // TODO: Allow UCI customizability of these options
			Search::EngineBookSkill = skill;
		}
	}
}

void Book::add_position(Book_Position pos){
	char bytes[sizeof(Book_Position)];
	memcpy(&bytes[0], &pos, sizeof(pos));
	for(int i = 0; i < sizeof(pos); i++) data.push_back(bytes[i]);
}

unsigned int Book::offset_of(uint64_t hash, bool& found){
	assert((int(data.size()) % int(sizeof(Book_Position))) == 0); // just make sure that there is no garbage
	Book_Position tmp;
	for(unsigned int i = 0; i < data.size(); i += sizeof(Book_Position)){
		memcpy(&tmp, &data[i], sizeof(Book_Position));
		if(tmp.hash == hash){
			found = true;
			return i;
		}
	}
	found = false;
	return 0;
}

Book_Position Book::get_position_at(unsigned int off){
	Book_Position ret;
	memcpy(&ret, &data[off], sizeof(Book_Position));
	return ret;
}

void Book::update_position(uint64_t hash, Book_Position with){
	bool found = false;
	unsigned int off = offset_of(hash, found);
	assert(found);
	memcpy(&data[off], &with, sizeof(Book_Position));
}

void Book::remove_position(uint64_t hash){
	bool found = false;
	unsigned int off = offset_of(hash, found);
	assert(found);
	data.erase(off, sizeof(Book_Position));
	assert(!(data.size() % sizeof(Book_Position)));
}

bool operator<(Book_Move a, Book_Move b){
	return a.score > b.score; // since std::sort does it in ascending order, and this returns whether 'a' should go *before* 'b'
}

void Book::sort_results_by(std::vector<Book_Move>& results, Book_Skill by){
	// Frequency is taken as a percentage P of the weight W out of
	// the total weight W_tot, then multiplied by 1000 and with the 
	// decimal point truncated. Therefore, the frequency will be
	// in the interval [0, 1000] - can only be 0 since the decimal
	// point is truncated obviously.
	// Therefore, P = W / W_tot, and freq = trunc(P * 1000)
	//
	// The magnitude of the learned score is capped at 6.0, and
	// the learned score is then multiplied by 100 and the
	// decimal point is truncated, leaving a range of [-600, 600].
	// Therefore, learn = trunc(10 * max(min(learn, 6.0), -6.0))
	// Considering forgiveness as 'forgiv' for a learn < 0:
	// learn = learn + 6 * forgiv
	// Which works since a negative learned score is now 
	// guaranteed to be in the interval [-600, 0).
	// 
	// The final score, considering the variance as 'var', is given by:
	// score = (freq + learn) + trunc(var * rand())
	// where rand() returns a value in the range of [0, 1]
	srand(314159); // fixed seed for debugging purposes (could be srand(time(NULL)) for better "randomness")
	unsigned int total_weight = 0;
	for(const Book_Move& on : results) total_weight += on.bpos.get_num();
	for(unsigned int i = 0, e = results.size(); i < e; i++){
		Book_Move& bmove = results[i];
		Book_Position& on = bmove.bpos;
		int freq = int((double(on.get_num()) / total_weight) * 1000.0);
		int learn = int(10 * std::max(std::min(double(on.get_learn()), 6.0), -6.0));
		if(learn < 0) learn = learn + 6 * by.forgiveness; // consider forgiveness
		bmove.score = freq + learn + int(by.variance * float(rand()) / RAND_MAX);
	}
	std::sort(results.begin(), results.end());
}

std::vector<Book_Move> Book::results_for(Board& pos){
	bool found = false;
	BoardState st;
	Book_Position tmp;
	std::vector<Book_Move> ret;
	Book_Move move;
	for(MoveList<LEGAL> it(pos); *it; it++){
		pos.do_move(*it, st);
		Key hash = pos.key();
		unsigned int off = offset_of(hash, found);
		if(found){
			memcpy(&tmp, &data[off], sizeof(Book_Position));
			move.bpos = tmp;
			move.move = *it;
			move.score = 0;
			ret.push_back(move);
		}
		pos.undo_move(*it);
	}
	return ret;
}

Book& operator<<(Book& book, PGN_Game& game){
	// TODO: Learning
	/*
	PGN_Options opts; // tags and options
	PGN_Result res; // result
	std::vector<PGN_Move> moves; // played moves on the board
	*/
	Board pos;
	Book_Position tmp;
	pos.init_from((game.opts.addl.find(FEN) != game.opts.addl.end()) ? game.opts.addl[FEN] : StartFEN);
	std::stack<BoardState> bss;
	bool found = false;
	for(const PGN_Move& pgn_move : game.moves){
		Move move = pgn_move.enc;
		bss.push(BoardState());
		pos.do_move(move, bss.top());
		Key hash = pos.key();
		unsigned int off = book.offset_of(hash, found);
		if(found){
			tmp = book.get_position_at(off);
			tmp.set_num(tmp.get_num() + 1);
			memcpy(&book.data[off], &tmp, sizeof(Book_Position)); // update it
		} else {
			tmp.hash = hash;
			tmp.info = 0ULL;
			tmp.set_num(1); // counter starts at 1
			book.add_position(tmp);
		}
	}
	return book;
}




















































































	