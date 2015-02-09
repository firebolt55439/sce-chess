#include "Common.h"
#include "Bitboards.h"
#include "Board.h"
#include "MoveGen.h"
#include "Evaluation.h"
#include "Search.h"

Value search(Value alpha, Value beta, Depth depth, Depth ply, Board& pos){
	if(depth == DEPTH_ZERO){
		// TODO: Quiescent search
		return Eval::evaluate(pos);
	}
	Value score(VAL_ZERO);
	MoveList<LEGAL> it(pos);
	if(!it.size()){
		if(pos.checkers()) return mated_in(ply);
		else return Value(0); // draw by stalemate
	}
	for(; *it; it++){
		BoardState st;
		pos.do_move(*it, st);
		score = -search(-beta, -alpha, (depth - ONE_PLY), (ply + ONE_PLY), pos);
		pos.undo_move(*it);
		if(score >= beta){
			return beta; // have now pruned the rest of this node's children
		}
		if(score > alpha){
			alpha = score; // raise bound
		}
	}
	return alpha;
}

Move Search::search_root(Board& pos){
	Value alpha = -VAL_INF, beta = VAL_INF;
	const Depth max_depth = Depth(3);
	Move best_move = MOVE_NONE;
	for(MoveList<LEGAL> it(pos); *it; it++){
		// TODO: Iterative deepening, multiPV, etc.
		BoardState st;
		pos.do_move(*it, st);
		Value score = -search(-beta, -alpha, max_depth, Depth(0), pos);
		pos.undo_move(*it);
		std::cout << MAGENTACOLOR << "Move " << Moves::format<false>(*it) << " had score " << int(score) << ".\n" << RESET << std::endl;
		if(score >= beta){
			break; // resign?
		}
		if(score > alpha){
			std::cout << "New best move.\n";
			alpha = score;
			best_move = *it;
		}
	}
	return best_move;
}
















































