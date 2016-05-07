#include "Common.h"
#include "Board.h"
#include "MoveGen.h"
#include "UCI.h"
#include "Annotate.h"
#include <iomanip>
#include <fstream>
#include <sstream>

std::string strip_fen(std::string fen){
	while(fen.length() && isspace(fen.back())) fen.pop_back();
	// Strips fullmove + halfmove count from FEN and returns it. //
	while(isdigit(fen.back())) fen.pop_back(); // pop fullmove counter
	assert(fen.back() == ' ');
	fen.pop_back(); // pop space
	while(isdigit(fen.back())) fen.pop_back(); // pop halfmove counter
	assert(fen.back() == ' ');
	fen.pop_back(); // remove trailing space
	return fen;
}

void Annotate::init(void){
	/*
	PGN_Move move;
	PGN_Writer writ;
	PGN_Options opt;
	opt.add(Site, "Home - iMac");
	writ.init(opt, Stopped);
	for(const Move& on : test_game){
		move.enc = on;
		writ.write(move);
	}
	std::cout << "\n" + writ.formatted() + "\n";
	*/
	// TODO
}

void Annotator::init(PGN_Options op, Annotator_Options a_opts, PGN_Result rt){
	this->ap = a_opts;
	PGN_Writer::init(op, rt);
}

void Annotator::clear(void){
	PGN_Writer::clear();
	this->ap = Annotator_Options();
	this->ap.time_per = -1; // mark as just cleared/not init'ed yet
}

// Annotator Constants //
// TODO: Allow command-line, UCI customizability
static const int UNCLEAR_MAX = 20; // unclear at 0.2 pawns or less
static const int LIKELY_MAX = 60; // likely win at between 0.2 pawns and 0.6 pawns
static const int UPPER_MAX = 110; // upper hand at between 0.6 pawns and 1.1 pawns
// And anything greater than UPPER_MAX is a decisive advantage.

static const int MISSED_MARGIN = 25; // margin for a missed good move
static const int BLUNDER_MARGIN = 105; // we blundered if we missed a move with a score at least this much greater

// Annotator Methods //

Ann_Advantage Annotator::get_advantage(Value eval, const Board& pos){
	int cp = int(eval); // eval is already in centipawns
	int ret = BLACK_DECISIVE - 1; // off the edge
	// When computing the advantage, assume it is white to move.
	// Then, if it is black to move, just negate the return
	// value.
	if(cp < -UPPER_MAX){
		ret = BLACK_DECISIVE;
	} else if(cp < -LIKELY_MAX){
		ret = BLACK_UPPER;
	} else if(cp < -UNCLEAR_MAX){
		ret = BLACK_LIKELY;
	} else if((cp > -UNCLEAR_MAX) && (cp < UNCLEAR_MAX)){
		ret = UNCLEAR;
	} else if(cp < LIKELY_MAX){
		ret = WHITE_LIKELY;
	} else if(cp < UPPER_MAX){
		ret = WHITE_UPPER;
	} else {
		assert(cp >= UPPER_MAX);
		ret = WHITE_DECISIVE;
	}
	if(pos.side_to_move() == BLACK){
		ret = -ret;
	}
	return Ann_Advantage(ret);
}

void Annotator::search_for(int msec, Search::BoardStateStack& states){
	// Note: This assumes the board and limits (except time) are already set up.
	Search::Limits.movetime = msec;
	Threads.start_searching(board, Search::Limits, states);
	while(Threads.main_thread->thinking){
		usleep(TimerThread::PollEvery * 4); // poll the main thread approx. every 20 ms
	}
}

PGN_Move Annotator::annotate(Move move){
	PGN_Move ret;
	ret.enc = move;
	std::string& annot = ret.annot;
	annot = ""; // clear previous annotation, if any
	auto& comm = ret.comment;
	// Then, search the current position and record the best move and its score. //
	Search::BoardStateStack BSS = Search::BoardStateStack(new std::stack<BoardState>());
	Search::SearchLimits limits;
	std::memset(&limits, 0, sizeof(limits));
	/*
	std::vector<Move> SearchMoves; // which moves to search, if/a
	int time[SIDE_NB], inc[SIDE_NB]; // time and increment by side (for asymmetric time controls/handicap)
	int movestogo; // moves to go until clock reset
	int depth; // only search to a specified depth (e.g. "go depth 6")
	int movetime; // if/a
	int mate; // for mate searches
	int infinite; // if we are doing an infinite search (e.g. "go infinite")
	int ponder; // if we are pondering
	int64_t nodes; // stop at a certain number of nodes
	*/
	Search::Limits = limits;
	search_for(ap.time_per, BSS);
	printf("A1\n");
	Search::RootMove best_line = Search::LastBest, other_best_line(MOVE_NONE);
	Move best = Search::LastBest.pv[0];
	Value best_score = Value(Search::LastBest.score * 100 / PawnValueEg), other_best_score = best_score; // best scores, in centipawns
	Ann_Advantage cur_adv = get_advantage(best_score, board);
	printf("A2\n");
	// Calculate the advantage after the given move. //
	BSS = Search::BoardStateStack(new std::stack<BoardState>());
	printf("A3\n");
	Ann_Advantage next_adv = cur_adv;
	// If the best move and played move differ, search the played move. //
	printf("A5\n");
	if(best != move){
		printf("A5B\n");
		std::memset(&limits, 0, sizeof(limits));
		limits.SearchMoves.clear();
		limits.SearchMoves.push_back(move);
		Search::Limits = limits;
		search_for(ap.time_per, BSS);
		other_best_score = Value(Search::LastBest.score * 100 / PawnValueEg);
		other_best_line = Search::LastBest;
		next_adv = get_advantage(other_best_score, board); // hard-coded black/white values, so no need to flip sides/negate this
		printf("A5B1\n");
	}
	// Now, classify the move played based on the two scores and annotate it accordingly. //
	printf("A6\n");
	int diff = next_adv - cur_adv; // calculates the advantage diff. from this move to the next as if white to move
	printf("Next: |%d|, cur: |%d|\n", next_adv, cur_adv);
	printf("(S) Next: |%d|, cur: |%d|\n", best_score, other_best_score);
	if(board.side_to_move() == BLACK){
		diff *= (-1); // since black increases negatively and white positively, with equal being 0
	}
	printf("A7\n");
	if(move == best){
		printf("A7B-Best Move\n");
		// Special case: user played the engine's best choice.
		// TODO: Figure out exactly how good the user had to play to match the engine's choice.
		if(diff == 1){
			// The choice the user made pushed the user over one threshold.
			comm.emplace(1); // !
		} else if(diff > 1){
			// Note: If the best move resulted in the user having to give up advantage, it
			// must have been the result of a *previous* blunder.
			comm.emplace(3); // !!
		}
	} else {
		printf("A7B-Other Move\n");
		printf("Diff: |%d|\n", diff);
		bool kibitz_score = false; // show score of move just played
		bool kibitz_current_line = false; // show the line of moves for the actual move played
		bool kibitz_line = false; // show best line
		if(diff >= 2){
			printf("A7B-!!\n");
			comm.emplace(3); // !!
		} else if(diff <= -2){
			kibitz_current_line = true;
			kibitz_line = true;
			printf("A7B-??\n");
			comm.emplace(4); // ??
		} else {
			printf("A7B-Other\n");
			// If diff is between -1 and 1 incl., then we take into account the possible scores.
			kibitz_score = true;
			int score_diff = other_best_score - best_score;
			printf("SD: |%d|\n", score_diff);
			bool missed = (score_diff < -MISSED_MARGIN); // if we actually missed the better move
			if(missed){
				kibitz_line = true;
			}
			if(score_diff < -BLUNDER_MARGIN){
				kibitz_current_line = kibitz_line = true;
				// Wow, we blundered!
				comm.emplace(4); // ??
			} else {
				if(diff == 1){
					if(missed){
						comm.emplace(5); // !?
					} else {
						comm.emplace(1); // !
					}
				} else if(diff == -1){
					kibitz_current_line = true;
					comm.emplace(2); // ?
				} else if(!diff){
					if(missed){
						comm.emplace(6); // ?!
					}
				}
			}
		}
		std::stringstream ann;
		char buf[32];
		ann << std::setprecision(2) << std::showpoint;
		if(kibitz_score){ // kibitzes score in pawns
			BSS = Search::BoardStateStack(new std::stack<BoardState>());
			sprintf(buf, "%.2f", double(double(other_best_score) / 100.0)); // current line's score
			ann << std::string(buf);
			if(kibitz_current_line){ // kibitz current line
				for(const Move& on : other_best_line.pv){
					assert(on != MOVE_NONE);
					ann << ' ' << Moves::format<true>(on, board);
					BSS->push(BoardState());
					board.do_move(on, BSS->top());
				}
				for(auto it = other_best_line.pv.rbegin(); it != other_best_line.pv.rend(); it++){
					board.undo_move(*it);
					BSS->pop();
				}
			}
			if(kibitz_line){ // kibitz best line + score
				sprintf(buf, "%.2f", double(double(best_score) / 100.0));
				ann << " / " << std::string(buf); // show the better line's score as well
				for(const Move& on : best_line.pv){
					ann << ' ' << Moves::format<true>(on, board);
					BSS->push(BoardState());
					board.do_move(on, BSS->top());
				}
				for(auto it = best_line.pv.rbegin(); it != best_line.pv.rend(); it++){
					board.undo_move(*it);
					BSS->pop();
				}
			}
		}
		annot = ann.str();
	}
	printf("A8\n");
	BSS.release();
	printf("A9\n");
	printf("Annotation: |%s|, Comment(s): |", annot.c_str());
	for(const auto& on : comm) std::cout << ' ' << on;
	std::cout << "|\n";
	//getchar();
	return ret;
}

void Annotate::annotate_file(std::string inf, std::string outf, Annotator_Options ap){
	std::ifstream ifp(inf);
	if(!ifp.is_open()){
		Error("Could not open input file '" + inf + "' for reading.");
	}
	std::string inp = ReadEntireFile(ifp);
	std::ofstream ofp(outf, std::ofstream::app);
	if(!ofp.is_open()){
		Error("Could not open output file '" + outf + "' for writing.");
	}
	PGN_Reader reader;
	reader.init(inp);
	reader.read_all();
	printf("Annotating all games...\n");
	Annotator annt;
	for(unsigned int i = 0, e = reader.games_num(); i < e; i++){
		PGN_Game on = reader.get_game(i);
		annt.clear();
		annt.init(on.opts, ap, on.res);
		if(on.opts.addl.find(FEN) != on.opts.addl.end()){
			annt.init_from(on.opts.addl[FEN]);
		} else annt.init_from(StartFEN);
		for(const PGN_Move& m : on.moves){
			std::cout << "On move " << Moves::format<false>(m.enc) << "...\n";
			annt.write_annot(m.enc);
		}
		// And write the output to the output file. //
		ofp << "\n" + annt.formatted() + "\n";
	}
	printf("Annotated all games and wrote them to output file!\n");
	ofp.close();
}




































