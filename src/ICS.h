#ifndef ICS_INC
#define ICS_INC

#include "Common.h"
#include "Search.h"

struct Socket {
	int fd;
	
	/* Ctor/Dtor */
	Socket(void);
	~Socket(void);
	
	/* Connections */
	int open_connection(std::string host, int port);
	int write(std::string msg);
	std::string read(int len);
	
	/* Operators */
	friend Socket& operator<<(Socket& sock, std::string msg);
};

struct ICS_Settings {
	// ICS Client Settings //
	bool allow_unrated; // allow unrated games
	bool allow_rated; // allow rated games
	std::vector<std::string> allowed_types; // allowed game types (e.g. "standard", "blitz", etc.)
};

enum ICS_Result {
	NO_START, // game was not started for some reason (e.g. play request got through too late, timed out waiting for game to start, etc.)
	LOST, // we lost
	DRAWN, // game drawn
	WON, // we won
	STOPPED, // stopped (e.g. aborted, etc.)
	UNKNOWN // could not parse result
};

struct ICS_Results {
	unsigned int won = 0, lost = 0, drawn = 0, unknown = 0;
};

struct ICS_SeekInfo {
	std::string name; // name of player seeking
	int rating; // rating, if given (else -1)
	int base; // base TC
	int inc; // increment TC
	bool rated; // whether this game is rated or not
	std::string type; // the type of game (e.g. blitz, standard, etc.)
	std::vector<std::string> specials; // modifiers (e.g. "[black"], "m", etc.)
	int game_num; // game number for playing (e.g. send "play N" to take player up on their offer)
};

struct ICS_GameInfo {
	std::string fen; // FEN
	std::string names[SIDE_NB]; // names of players by [side]
	int relation; // my relation to this game (isolated, observing, examiner, playing - my move, etc.)
	int base; // initial time (seconds)
	int inc; // increment (seconds)
	int mat_strength[SIDE_NB]; // material strength by side (if given/applicable)
	int time[SIDE_NB]; // time left in seconds by [side]
};

struct ICS_Game {
	ICS_Result result; // the result of the game
	std::vector<ICS_GameInfo> moves; // the move-by-move game
	
	ICS_Game(ICS_Result res, const std::vector<ICS_GameInfo> m) : result(res), moves(m) { }
};

class ICS {
	protected:
		Socket sock; // connection to ICS server
		bool logged_in; // if we are logged in or not
		ICS_Settings settings; // ICS settings
		std::string username; // username
	public:
		/* Ctor/Dtor */
		ICS(ICS_Settings s) : logged_in(false), settings(s) { }
		~ICS(void){ }
		
		/* Actions */
		Move get_best_move(const Board& pos, const Search::SearchLimits& limits, Search::BoardStateStack& states);
		void handle_game_result(ICS_Game res, ICS_SeekInfo si, ICS_Results& ret);
		virtual int parse_style(std::string line, ICS_GameInfo& ret) = 0; // parse the game information in the appropriate style (e.g. style12 for FICS)
		virtual int parse_seek(std::string line, ICS_SeekInfo& ret) = 0; // parse a seek request in the appropriate style
		virtual int try_login(std::string user, std::string pass) = 0; // try to login to the server with the specified credentials
		virtual ICS_Game do_game(std::string cont) = 0; // do a game and return the result with either a seek request in hand or a request to continue a game already started
		virtual ICS_Results listen(unsigned int seconds) = 0; // perform the game loop for the specified # of seconds (read input in loop, parse, process according to settings) - if seconds is 0, then infinite
};

class FICS : public ICS {
	public:
		FICS(ICS_Settings s) : ICS(s) { }
		int try_login(std::string user, std::string pass);
		int parse_style(std::string line, ICS_GameInfo& ret);
		int parse_seek(std::string line, ICS_SeekInfo& ret);
		ICS_Game do_game(std::string cont);
		ICS_Results listen(unsigned int seconds);
};









































#endif // #ifndef ICS_INC