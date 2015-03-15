#ifndef ANN_INC
#define ANN_INC

#include "Common.h"
#include "Evaluation.h"
#include "Threads.h"
#include "Search.h"
#include "PGN.h"
#include "UCI.h"

struct Annotator_Options;

namespace Annotate {
	void init(void);
	void annotate_file(std::string infile, std::string outfile, Annotator_Options ap); // annotate a PGN file 'infile' and write output to 'outfile'
}

extern std::string strip_fen(std::string fen); // strip FEN of fullmove + halfmove counters

struct Annotator_Options {
	int time_per; // time per move for analyzing, in milliseconds
	// TODO: Blunder threshold, kibitz style, natural language, etc.
};

enum Ann_Advantage {
	BLACK_DECISIVE = -3, // black is poised to win
	BLACK_UPPER = -2, // black has the upper hand
	BLACK_LIKELY = -1, // black is likely to win
	UNCLEAR = 0, // it is unclear who will win (relatively equal)
	WHITE_LIKELY = 1,
	WHITE_UPPER = 2,
	WHITE_DECISIVE = 3
};

enum Ann_Move { // TODO: Use? Remove?
	// Move Types //
	Forced = -4, // a forced move (meaning the move was either best move by a huge margin or it was the only legal move in the position) // TODO: MultiPV
	// Annotation Move Types //
	Blunder = -3, // (??)
	Bad = -2, // (?)
	Dubious = -1, // (?!)
	OK = 0, // (Score/Depth)
	Interesting = 1, // (!?)
	Good = 2, // (!)
	Best = 3 // (!!) the best move in the position according to the engine
};

class Annotator : protected PGN_Writer {
	private:
		Annotator_Options ap; // annotator options
		
		void search_for(int msec, Search::BoardStateStack& states); // search for given number of milliseconds (assumes the limits have already been set up)
	public:
		Annotator(std::string fen){ board.init_from(fen); }
		Annotator(void){ board.init_from(StartFEN); }
		~Annotator(void){ }
		
		void init(PGN_Options op, Annotator_Options ap, PGN_Result rt); // initialize annotator
		void init_from(std::string fen){ board.init_from(fen); }
		void clear(void); // clear annotator for reuse
		void write(PGN_Move move){ PGN_Writer::write(move); }
		void write_annot(Move move){ PGN_Writer::write(annotate(move)); }
		
		Ann_Advantage get_advantage(Value eval, const Board& board); // returns the advantage type for the given board
		PGN_Move annotate(Move move); // annotates a move and returns the PGN_Move for writing
		std::string formatted(void){ return PGN_Writer::formatted(); }
};

#endif // #ifndef ANN_INC