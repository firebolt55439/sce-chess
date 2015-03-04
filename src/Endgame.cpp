#include "Common.h"
#include "Bitboards.h"
#include "Board.h"
#include "MoveGen.h"
#include "Evaluation.h"
#include "Pawns.h"
#include "Endgame.h"
#include <map>

const int PushToEdges[SQUARE_NB] = {
	100, 90, 80, 70, 70, 80, 90, 100,
	 90, 70, 60, 50, 50, 60, 70,  90,
	 80, 60, 40, 30, 30, 40, 60,  80,
	 70, 50, 30, 20, 20, 30, 50,  70,
	 70, 50, 30, 20, 20, 30, 50,  70,
	 80, 60, 40, 30, 30, 40, 60,  80,
	 90, 70, 60, 50, 50, 60, 70,  90,
	100, 90, 80, 70, 70, 80, 90, 100,
}; // used to push a piece to the edges of the board

namespace EndgameN {
	Endgames EndGames;
}

void EndgameN::init(void){
	// TODO: Init KPK tables, etc.
	EndgameN::get_endgames().init();
}

Key key_for_code(const std::string& code, Side c){
	// Creates an FEN string for the given code
	// and returns its material key.
	assert(code.length() && (code.length() < 8));
	assert(code[0] == 'K');
	std::string sides[2] = {
		code.substr(code.find('K', 1)), // 2nd/weaker side
		code.substr(0, code.find('K', 1)) // 1st/stronger side
	};
	std::transform(sides[c].begin(), sides[c].end(), sides[c].begin(), tolower); // make it all lower case
	std::string fen = sides[0] + char(8 - sides[0].length() + '0') + "/8/8/8/8/8/8/" + sides[1] + char(8 - sides[1].length() + '0') + " w - - 0 1";
	Board pos;
	pos.init_from(fen);
	return pos.material_key();
}

template<EndgameType E>
void Endgames::add(std::string code){
	key_map[key_for_code(code, WHITE)] = new Endgame<E>(WHITE);
	key_map[key_for_code(code, BLACK)] = new Endgame<E>(BLACK);
}

void Endgames::init(void){
	//add<KPK>("KPK"); // TODO
	add<KRK>("KRK");
	add<KQK>("KQK");
}

Value EndgameN::probe(const Board& pos, bool& found){
	EndgameBase* r = nullptr;
	r = EndGames.probe(pos.material_key(), r);
	if(!r){
		found = false;
		return VAL_ZERO;
	}
	found = true;
	return (*r)(pos);
}

template<>
Value Endgame<KPK>::operator()(const Board& pos) const {
	return VAL_ZERO;
}

template<>
Value Endgame<KRK>::operator()(const Board& pos) const {
	// 8/1KR5/8/1k6/8/8/8/8 w - - 0 1
	Side us = strongSide, them = weakSide;
	Value ret = VAL_KNOWN_WIN;
	const Square ksq = pos.king_sq(us), tksq = pos.king_sq(them);
	const Square rsq = lsb(pos.pieces(ROOK));
	const Rank kr = rank_of(ksq), tkr = rank_of(tksq), rr = rank_of(rsq);
	ret += (PushToEdges[tksq] / 4); // push the other king to the edges
	bool lined_kings = false;
	if(pos.side_to_move() == us){
		if(file_of(ksq) == file_of(tksq) && distance<Rank>(ksq, tksq) == 2) lined_kings = true;
		else if(kr == tkr && distance<File>(ksq, tksq) == 2) lined_kings = true;
		if(lined_kings){
			ret += 30; // bonus for lined up kings
		}
	}
	if(rr == tkr || file_of(rsq) == file_of(tksq)){
		ret += 15; // bonus for rook check
	}
	ret -= 10 * bool(pos.attacks_from<KING>(tksq) & rsq); // keep our rook out of other king's range
	return (pos.side_to_move() == us) ? (ret) : (-ret);
}

template<>
Value Endgame<KQK>::operator()(const Board& pos) const {
	// 8/1KQ5/8/8/8/3k4/8/8 w - - 0 1
	Side us = strongSide, them = weakSide;
	Value ret = VAL_KNOWN_WIN;
	const Square ksq = pos.king_sq(us), tksq = pos.king_sq(them);
	const Square qsq = lsb(pos.pieces(QUEEN));
	ret += (PushToEdges[tksq] / 4); // push other king to edges
	if(pos.attacks_from<KING>(ksq) & qsq){
		ret += 35; // queen proximity bonus
		if(pos.attacks_from<QUEEN>(qsq) & ksq){
			ret += 25; // queen attacking other king bonus
		}
	}
	return (pos.side_to_move() == us) ? (ret) : (-ret);
}

































