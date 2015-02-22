#ifndef ICS_INC
#define ICS_INC

#include "Common.h"

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
};

struct ICS_Results {
	unsigned int won = 0, lost = 0, drawn = 0;
};

struct ICS_GameInfo {
	// <12> rnbqkb-r ppp-pppp -----n-- ---p---- ---P---- --N--N-- PPP-PPPP R-BQKB-R B -1 1 1 1 1 1 265 firebolting GuestYRFZ -1 3 2 39 39 160 180 3 N/b1-c3 (0:21) Nc3 0 1 0
	std::string fen;
	std::string names[SIDE_NB]; // names of players by [side]
	int relation; // my relation to this game (isolated, observing, examiner, playing - my move, etc.)
	int base; // initial time (seconds)
	int inc; // increment (seconds)
	int mat_strength[SIDE_NB]; // material strength by side (if given/applicable)
	int time[SIDE_NB]; // time left in seconds by [side]
};

class ICS {
	protected:
		Socket sock; // connection to ICS server
		bool logged_in; // if we are logged in or not
		ICS_Settings settings; // ICS settings
	public:
		/* Ctor/Dtor */
		ICS(ICS_Settings s) : logged_in(false), settings(s) { }
		~ICS(void){ }
		
		/* Actions */
		virtual ICS_GameInfo parse_style(std::string line) = 0; // parse the game information in the appropriate style (e.g. style12 for FICS)
		virtual int try_login(std::string user, std::string pass) = 0; // try to login to the server with the specified credentials
		virtual ICS_Results listen(unsigned int seconds) = 0; // perform the game loop for the specified # of seconds (read input in loop, parse, process according to settings)
};

class FICS : public ICS {
	public:
		FICS(ICS_Settings s) : ICS(s) { }
		int try_login(std::string user, std::string pass);
		ICS_GameInfo parse_style(std::string line);
		ICS_Results listen(unsigned int seconds);
};









































#endif // #ifndef ICS_INC