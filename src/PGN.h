#ifndef PGN_INC
#define PGN_INC

#include "Common.h"
#include "Board.h"
#include <map>
#include <set>
#include <stack>

namespace PGN {
	void init(void);
}

// A required PGN tag in the seven tag roster. //
enum PGN_Req_Tag {
	Event,
	Site,
	Date,
	Round,
	White,
	Black,
	Result // Note: this is technically required but only here for reading purposes later - is computed by program
};

// An optional PGN tag for further information about the game. //
enum PGN_Ext_Tag {
	WhiteTitle, BlackTitle,
	WhiteElo, BlackElo, // *
	WhiteUSCF, BlackUSCF,
	WhiteNA, BlackNA,
	WhiteType, BlackType, // *
	EventDate,
	EventSponsor,
	Section,
	Stage,
	BoardType,
	Opening, // *
	Variation, // *
	SubVariation, // *
	ECO,
	NIC,
	Time, // *
	UTCTime,
	UTCDate,
	TimeControl, // * ('?', '-' (none), '40/9000' (9000 seconds for 40 moves), '5+2', '*10' (total 10)
	SetUp, // * 
	FEN, // * alternative starting FEN
	Termination, // *
	AnnotatorStr, // *
	Mode, // *
	PlyCount // *
};

// The result of the game for PGN output. //
enum PGN_Result {
	Unknown, // not entered yet
	WhiteWin, // 1-0
	BlackWin, // 0-1
	Draw, // 1/2-1/2
	Stopped // '*'
}; // the result of the game for use with PGN's 

// This structure stores PGN tags and associated values. //
struct PGN_Options {
	std::string builtin[7]; // the required ones - Event, Site, Date, Round, White, Black, and Result
	std::map<PGN_Ext_Tag, std::string> addl; // additional ones
	
	PGN_Options(void){ }
	~PGN_Options(void){ }
	void add(PGN_Req_Tag tag, std::string val){ builtin[tag] = val; }
	void add(PGN_Ext_Tag tag, std::string val){ addl[tag] = val; }
	std::string formatted(void); // get PGN formatted options for registered options
};

// This stores a PGN move and its associated NAG(s) and annotation if/a. //
struct PGN_Move {
	Move enc; // 16-bit encoded move
	std::set<int> comment; // comment(s) as PGN NAG's
	std::string annot; // optional annotation (wrapped in '{' <annotation> '}')
};

// This structure stores a PGN game. //
struct PGN_Game {
	PGN_Options opts; // tags and options
	PGN_Result res; // result
	std::vector<PGN_Move> moves; // played moves on the board
};

// This class specifies an instance of a PGN writer to write a game. //
class PGN_Writer {
	// TODO: Allow greater interoperability with PGN_Game and PGN_Reader, and more writing options
	protected:
		Board board; // internal board
		std::stack<BoardState> bss; // internal BSS
		PGN_Options opts; // PGN options
		PGN_Result res; // result of game
		int num; // starts from 0 (if even, then white to move, else black to move)
		int line_len; // length of current line counter (for wrapping around)
		std::stringstream ss; // for writing to
	public:
		PGN_Writer(void){ clear(); }
		~PGN_Writer(void){ }
		
		void init(PGN_Options op, PGN_Result rt); // init's everything, puts options into stream with computed result, etc.
		void init(PGN_Game game); // initialize with a full PGN game given (writes all moves, etc.)
		void clear(void); // so a PGN_Writer instance can be reused - resets back to factory
		void write(PGN_Move move);
		
		std::string formatted(void); // get the final formatted PGN
		friend std::ostream& operator<<(std::ostream&, PGN_Writer&);
};

// This class specifies an instance of a PGN parser to read a game. //
class PGN_Reader {
	protected:
		// Internal Variables //
		Board board; // internal board
		std::stack<BoardState> bss; // internal BSS
		std::istringstream ss; // for reading from
		// Parsed Game(s) //
		std::vector<PGN_Game> games; // parsed games
	public:
		PGN_Reader(void){ clear(); }
		~PGN_Reader(void){ }
		
		void init(std::string inp); // initialize PGN reader with input file
		void clear(void); // reset to factory state basically
		void reset(void); // does not clear parser buffers but resets board, bss, etc.
		int parse(void); // attempt to parse PGN game at parser location and save it (returns nonzero value upon failure)
		void read_all(void); // keep parsing PGN games until first error (used for parsing large files)
		
		unsigned int games_num(void){ return games.size(); }
		PGN_Game get_game(unsigned int i){ assert(i < games.size()); return games[i]; }
		std::vector<PGN_Game>& get_game_vector(void){ return games; }
};

#endif // #ifndef PGN_INC