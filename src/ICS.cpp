#include "Common.h"
#include "Bitboards.h"
#include "Board.h"
#include "MoveGen.h"
#include "Threads.h"
#include "Search.h"
#include "ICS.h"
#include "UCI.h"
#include "Annotate.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

Socket::Socket(void){
	fd = socket(AF_INET, SOCK_STREAM, 0);
	assert(fd >= 0);
}

Socket::~Socket(void){
	close(fd);
}

int Socket::open_connection(std::string host, int port){
	struct hostent* server = gethostbyname(host.c_str());
	if(server == NULL){
		return 1; // DNS lookup failed
	}
	struct sockaddr_in serv_addr;
	bzero((char*) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char*) server->h_addr, (char*) &serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(port);
	if(connect(fd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0){
		return 2; // could not connect
	}
	return 0;
}

int Socket::write(std::string msg){
	// MTU = around 1500
	char buf[1452];
	unsigned int ct, j;
	for(unsigned int i = 0; i < msg.length(); i += 1450){
		buf[0] = '\0';
		const unsigned int max = ((i + 1450) >= msg.length()) ? msg.length() : (i + 1450);
		for(j = i, ct = 0; j < max; j++, ct++){
			buf[ct] = msg[j];
		}
		buf[ct] = '\0';
		assert(ct == strlen(buf));
		int n = ::write(fd, buf, ct);
		if(n < 0){
			return 1; // error writing to socket
		}
	}
	return 0;
}

std::string Socket::read(int len){
	char* buf = new char[len + 1];
	buf[0] = '\0';
	int n = ::read(fd, buf, len);
	if(n < 0){
		return "";
	}
	std::string ret(buf);
	delete[] buf;
	return ret;
}

Socket& operator<<(Socket& sock, std::string msg){
	if(int r = sock.write(msg)){
		printf("Error writing to socket (code #%d)!\n", r);
		::exit(1);
	}
	return sock;
}

Move ICS::get_best_move(const Board& pos, const Search::SearchLimits& limits, Search::BoardStateStack& states){
	Threads.start_searching(pos, limits, states);
	while(Threads.main_thread->thinking){
		usleep(TimerThread::PollEvery * 4); // poll the main thread approx. every 20 ms
	}
	return Search::RootMoves[0].pv[0];
}

/*
enum PGN_Req_Tag {
	Event,
	Site,
	Date,
	Round,
	White,
	Black,
	Result // Note: this is technically required but only here for reading purposes later - is computed by program
}; // a required PGN tag

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
	Annotator, // *
	Mode, // *
	PlayCount // *
}; // an extra/supplemental PGN tag
*/

void ICS::handle_game_result(ICS_Game rest, ICS_SeekInfo si, ICS_Results& ret){
	// Handle Result //
	ICS_Result res = rest.result;
	PGN_Result pgnr = Unknown;
	std::string rstr = "(invalid)";
	if(res == DRAWN){ 
		++ret.drawn;
		rstr = "Drawn";
		pgnr = Draw;
	} else if(res == LOST){ 
		++ret.lost;
		rstr = "Lost";
	} else if(res == WON){ 
		++ret.won;
		rstr = "Won";
	} else if(res == UNKNOWN){ 
		++ret.unknown;
		rstr = "Unknown";
		pgnr = Unknown;
	} else if(res == STOPPED){
		rstr = "Stopped";
		pgnr = Stopped;
	} else if(res == NO_START){
		rstr = "Not Started";
		pgnr = Stopped;
	}
	printf("%sGame result: %s%s\n", BOLDCYAN, rstr.c_str(), RESET);
	printf("%sGame Record (W-L-D-U): %u-%u-%u-%u%s\n", BOLDCYAN, ret.won, ret.lost, ret.drawn, ret.unknown, RESET);
	// Create PGN //
	auto& moves = rest.moves;
	PGN_Writer writ;
	PGN_Options opt;
	opt.add(Event, "An ICS Game");
	opt.add(Site, "Online (ICS Website)");
	opt.add(Round, "1"); // TODO
	// White, Black, White/Black Elo, White/Black Type, TimeControl, FEN, Mode, etc.
	Board pos;
	if(moves.size()){
		const auto& gi = moves[0];
		std::stringstream tmp;
		pos.init_from(gi.fen);
		bool are_white = (pos.side_to_move() == WHITE && moves[0].relation == 1) || (pos.side_to_move() != WHITE && moves[0].relation == -1); // TODO: Simplify this condition somehow
		if(res == LOST || res == WON){ // TODO: Simplify this condition as well
			if(res == WON){
				if(are_white) pgnr = WhiteWin;
				else pgnr = BlackWin;
			} else {
				if(are_white) pgnr = BlackWin;
				else pgnr = WhiteWin;
			}
		}
		opt.add(White, (are_white ? username : gi.names[int(are_white)]));
		opt.add(Black, (!are_white ? username : gi.names[int(are_white)]));
		if(si.game_num != -1){
			if(si.rating != -1){
				tmp.str("");
				tmp << si.rating;
				opt.add((are_white ? BlackElo : WhiteElo), tmp.str());
			}
		}
		// TODO: Block computer opponent seeks on ICS (with '(C)' in their names)
		opt.add(WhiteType, (are_white ? "Computer": "ICS Player"));
		opt.add(BlackType, (!are_white ? "Computer": "ICS Player"));
		// ('?', '-' (none), '40/9000' (9000 seconds for 40 moves), '5+2', '*10' (total 10)
		tmp.str("");
		tmp << gi.base << '+' << gi.inc;
		opt.add(TimeControl, tmp.str());
		opt.add(FEN, StartFEN); // TODO
		// TODO
	}
	writ.init(opt, pgnr);
	std::vector<Move> conv;
	BoardState st;
	for(auto& on : moves){
		on.fen = strip_fen(on.fen);
	}
	for(unsigned int i = 0; i < (moves.size() - 1); i++){
		const auto& on = moves[i].fen;
		const auto& next = moves[i + 1].fen;
		pos.init_from(on);
		Move m = MOVE_NONE;
		for(MoveList<LEGAL> it(pos); *it; it++){
			pos.do_move(*it, st);
			const std::string cur = strip_fen(pos.fen());
			if(cur == next){
				m = *it;
				break;
			}
			pos.undo_move(*it);
		}
		if(m == MOVE_NONE){
			std::cerr << "Cannot deduce move from FEN '" << on << "' to FEN '" << next << "'.\n";
			break; // stop conversion if cannot deduce move
		}
		conv.push_back(m);
	}
	// Dump move arr //
	printf("const std::vector<Move> move_arr = {\n");
	for(unsigned int i = 0; i < conv.size(); i++){
		Move& m = conv[i];
		std::cout << '\t';
		std::string from = square_str_of(from_sq(m));
		std::string to = square_str_of(to_sq(m));
		std::transform(from.begin(), from.end(), from.begin(), ::toupper);
		std::transform(to.begin(), to.end(), to.begin(), ::toupper);
		if(type_of(m) == NORMAL){
			std::cout << "make_move(SQ_" << from << ", SQ_" << to << ")";
		} else if(type_of(m) == ENPASSANT || type_of(m) == CASTLING){
			std::cout << "make<" << (type_of(m) == ENPASSANT ? "ENPASSANT" : "CASTLING") << ">(SQ_" << from << ", SQ_" << to << ")";
		} else {
			assert(type_of(m) == PROMOTION);
			std::string name = "";
			PieceType p = promotion_type(m);
			if(p == KNIGHT) name = "KNIGHT";
			else if(p == BISHOP) name = "BISHOP";
			else if(p == ROOK) name = "ROOK";
			else if(p == QUEEN) name = "QUEEN";
			std::cout << "make<PROMOTION>(SQ_" << from << ", SQ_" << to << ", " << name << ")";
		}
		if((i + 1) < conv.size()) std::cout << ',';
		std::cout << '\n';
	}
	printf("};\n");
	PGN_Move move;
	for(const Move& on : conv){
		move.enc = on;
		move.annot = ""; // TODO: Annotate
		writ.write(move);
	}
	std::cout << "\n" + writ.formatted() + "\n";
}
	
int FICS::try_login(std::string user, std::string pass){
	assert(user.length() >= 3);
	printf("Connecting to FICS...\n");
	if(int r = sock.open_connection("freechess.org", 5000)){
		printf("Could not open connection to FICS (code %d).\n", r);
		return r;
	}
	printf("Connected! Entering login credentials...\n");
	sock << user << "\n";
	sock << pass << "\n";
	std::string str = "";
	printf("Waiting for login confirmation...\n");
	size_t i;
	while(!logged_in){
		str = sock.read(256);
		if((i = str.find("Starting FICS session as")) != std::string::npos){
			printf("Login successful!\n");
			logged_in = true;
			sock << "set style 12\n";
			this->username = user;
			return 0;
		}
		if(str.find("is not a registered name") != std::string::npos){
			printf("Invalid username provided.\n");
			return 1; // bad username
		}
		if(str.find("Invalid password!") != std::string::npos){
			printf("Incorrect password provided.\n");
			return 2; // bad password
		}
	}
	return 0;
}

volatile bool listen_time_up = false;

void listen_alarm_handler(int){
	listen_time_up = true;
	signal(SIGALRM, SIG_IGN);
}

int FICS::parse_style(std::string line, ICS_GameInfo& ret){
	// <12> rnbqkb-r ppp-pppp -----n-- ---p---- ---P---- --N--N-- PPP-PPPP R-BQKB-R B -1 1 1 1 1 1 265 firebolting GuestYRFZ -1 3 2 39 39 160 180 3 N/b1-c3 (0:21) Nc3 0 1 0
	std::istringstream ss(line);
	ss >> std::skipws;
	std::string tok;
	if(!(ss >> tok)) return 1;
	if(tok != "<12>") return 2; // no style12 heading found
	std::stringstream fen;
	// rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
	for(Rank r = RANK_8; r >= RANK_1; r--){
		if(!(ss >> tok)) return 1;
		unsigned int dash_ct = 0; // dash counter
		for(unsigned int i = 0; i < tok.length(); i++){
			char on = tok[i];
			if(on == '-'){
				++dash_ct;
			} else {
				if(!isalpha(on)) return 3; // invalid char for piece-rank specification
				if(dash_ct){
					fen << dash_ct;
					dash_ct = 0;
				}
				fen << on;
			}
		}
		if(dash_ct){
			fen << dash_ct;
		}
		if(r > RANK_1) fen << "/";
	}
	fen << " ";
	// STM //
	if(!(ss >> tok)) return 1;
	if(tok.length() != 1) return 4; // bad STM descriptor
	fen << char(tolower(tok[0]));
	Side to_move = (tok[0] == 'W') ? WHITE : BLACK;
	fen << " ";
	// E.p. File (1) //
	if(!(ss >> tok)) return 1;
	int ep_file = atoi(tok.c_str());
	// Castling Rights //
	int cr = NO_CASTLING;
	// WHITE_OO
	if(!(ss >> tok)) return 1;
	int n = atoi(tok.c_str());
	if(n == 1) cr |= WHITE_OO;
	else if(n) return 6;
	// WHITE_OOO
	if(!(ss >> tok)) return 1;
	n = atoi(tok.c_str());
	if(n == 1) cr |= WHITE_OOO;
	else if(n) return 6;
	// BLACK_OO
	if(!(ss >> tok)) return 1;
	n = atoi(tok.c_str());
	if(n == 1) cr |= BLACK_OO;
	else if(n) return 6;
	// BLACK_OOO
	if(!(ss >> tok)) return 1;
	n = atoi(tok.c_str());
	if(n == 1) cr |= BLACK_OOO;
	else if(n) return 6;
	// Castling Rights to FEN
	if(!(cr & ANY_CASTLING)){
		fen << '-';
	} else {
		if(cr & WHITE_OO) fen << 'K';
		if(cr & WHITE_OOO) fen << 'Q';
		if(cr & BLACK_OO) fen << 'k';
		if(cr & BLACK_OOO) fen << 'q';
	}
	fen << ' ';
	// E.p. File (2) //
	if(ep_file == -1) fen << '-';
	else {
		if(ep_file < 0 || ep_file > 7) return 5; // bad e.p. file
		Square ep_sq = make_square(relative_rank(to_move, RANK_6), File(ep_file));
		fen << ep_sq;
	}
	fen << ' ';
	// Halfmove counter //
	if(!(ss >> tok)) return 1;
	int fifty_ct = atoi(tok.c_str());
	fen << fifty_ct << ' ';
	// Game # //
	if(!(ss >> tok)) return 1;
	// White + Black Name //
	if(!(ss >> tok)) return 1;
	ret.names[WHITE] = tok;
	if(!(ss >> tok)) return 1;
	ret.names[BLACK] = tok;
	// My Relation //
	if(!(ss >> tok)) return 1;
	ret.relation = atoi(tok.c_str());
	// Initial TC //
	if(!(ss >> tok)) return 1;
	ret.base = atoi(tok.c_str());
	if(!(ss >> tok)) return 1;
	ret.inc = atoi(tok.c_str());
	// White/Black Material Strength //
	if(!(ss >> tok)) return 1;
	ret.mat_strength[WHITE] = atoi(tok.c_str());
	if(!(ss >> tok)) return 1;
	ret.mat_strength[BLACK] = atoi(tok.c_str());
	// White/Black Remaining Time //
	if(!(ss >> tok)) return 1;
	ret.time[WHITE] = atoi(tok.c_str());
	if(!(ss >> tok)) return 1;
	ret.time[BLACK] = atoi(tok.c_str());
	// Fullmove Counter //
	if(!(ss >> tok)) return 1;
	int fullmove_ct = atoi(tok.c_str());
	fen << fullmove_ct;
	ret.fen = fen.str();
	// Note: Everything else is unnecessary.
	// TODO: Bughouse? Crazyhouse?
	return 0;
}

int FICS::parse_seek(std::string str, ICS_SeekInfo& ret){
	ret.specials.clear(); // clean up any previous specials
	std::string tok;
	std::istringstream ss(str);
	if(!(ss >> tok)) return 1;
	ret.name = tok;
	// Must be the rating then. //
	if(!(ss >> tok)) return 1;
	if(tok[0] != '(' || tok.back() != ')') return -1; // definitely not a seek request
	tok = tok.substr(1);
	tok.pop_back();
	ret.rating = (tok != "++++" && tok != "----") ? atoi(tok.c_str()) : -1; // opponent's rating (if given)
	// "Seeking" //
	printf("1_Tok: |%s|\n", tok.c_str());
	if(!(ss >> tok)) return 1;
	if(tok != "seeking") return 2; // not seeking apparently
	// Time Controls (e.g. 15 0) //
	printf("2_Tok: |%s|\n", tok.c_str());
	if(!(ss >> tok)) return 1;
	ret.base = atoi(tok.c_str());
	if(!(ss >> tok)) return 1;
	ret.inc = atoi(tok.c_str());
	// "Rated"/"Unrated" //
	printf("3_Tok: |%s|\n", tok.c_str());
	if(!(ss >> tok)) return 1;
	if(tok == "rated") ret.rated = true;
	else if(tok == "unrated") ret.rated = false;
	else return 3; // invalid value
	// Type of Game (e.g. standard, blitz, etc.) //
	printf("4_Tok: |%s|\n", tok.c_str());
	if(!(ss >> tok)) return 1;
	ret.type = tok;
	// Special Modifiers (e.g. "[black]", "m", etc.) //
	printf("5_Tok: |%s|\n", tok.c_str());
	if(!(ss >> tok)) return 1;
	bool ok_special = true;
	while(tok[0] == '[' || tok.length() == 1){
		ret.specials.push_back(tok);
		if(!(ss >> tok)){ 
			ok_special = false;
			break;
		}
	}
	if(!ok_special) return 4; // invalid special modifier
	// Now the '("play #" to respond)' part. //
	printf("6_Tok: |%s|\n", tok.c_str());
	if(tok[0] != '(') return 5;
	if(tok.find("play") == std::string::npos) return 6;
	if(!(ss >> tok)) return 1;
	ret.game_num = atoi(tok.c_str());
	printf("Seek string: \n|%s|\n", str.c_str());
	return 0;
}

ICS_Game FICS::do_game(std::string cont){
	// {Game 286 (firebolting vs. Luciopwm) Game aborted on move 1} *
	// {Game 67 (MAd vs. Sandstrom) Game adjourned by mutual agreement} *
	// {Game 222 (firebolting vs. GuestFGQQ) firebolting resigns} 0-1
	// {Game 222 (firebolting vs. GuestFGQQ) Creating unrated standard match.}
	// That seek is not available.
	// Note: Listening time limit is ignored when doing a game.
	Search::BoardStateStack BSS;
	Board board;
	std::vector<ICS_GameInfo> game_stack;
	ICS_GameInfo gi;
	std::string u1, u2;
	bool in_game = (cont.length() ? true : false);
	int counter = 0;
	bool first_run = true;
	while(true){
		std::string str_orig, str;
		if(!first_run || !cont.length()) str_orig = sock.read(1024);
		else str_orig = cont; // to make it "resume" basically
		std::stringstream str_ss(str_orig);
		first_run = false;
		while(std::getline(str_ss, str)){
			std::istringstream ss(str);
			std::string tok;
			ss >> std::skipws;
			if(!(ss >> tok)){
				continue; // if no input to be had
			} else if(tok == "fics%"){ // just a prompt
				continue;
			}
			if(str.find("That seek is not available") != std::string::npos){
				printf("%sSeek was not available.%s\n", BOLDCYAN, RESET);
				return ICS_Game(NO_START, game_stack); // seek was not available
			}
			if(tok == "<12>"){
				in_game = true;
				// The game state has been updated.
				printf("Style12: |%s|\n", str.c_str());
				if(int r = parse_style(str, gi)){
					printf("Could not parse Style12 with error code %d.\n", r);
				} else {
					game_stack.push_back(gi);
					printf("FEN |%s|, [%s vs. %s], relation %d, TC %d+%d, [%d vs. %d], have [%d vs. %d] left\n", gi.fen.c_str(), gi.names[WHITE].c_str(), 
						gi.names[BLACK].c_str(), gi.relation, gi.base, gi.inc, gi.mat_strength[WHITE], gi.mat_strength[BLACK], gi.time[WHITE], gi.time[BLACK]);
					BSS = Search::BoardStateStack(new std::stack<BoardState>());
					Search::SearchLimits limits;
					std::memset(&limits, 0, sizeof(limits));
					limits.inc[BLACK] = limits.inc[WHITE] = gi.inc * 1000; // converting from milliseconds to seconds here
					limits.time[BLACK] = gi.time[BLACK] * 1000;
					limits.time[WHITE] = gi.time[WHITE] * 1000;
					board.init_from(gi.fen);
					std::cout << board;
					if(gi.relation == -1){
						printf("%sOpponent's move.%s\n", BOLDCYAN, RESET);
					} else if(gi.relation == 1){
						printf("%sIt is our move.%s\n", BOLDCYAN, RESET);
						Move best = get_best_move(board, limits, BSS);
						std::string move_str = UCI::move(best);
						// Note: FICS does not handle "e7e8q" - you have to send "promote [piece (e.g. "q")]"
						if(type_of(best) == PROMOTION){
							printf("Is promotion - setting promotion piece to %c.\n", move_str.back());
							std::string send = "promote ";
							send.push_back(move_str.back());
							sock << send + "\n";
							move_str.pop_back(); // remove the trailing 'q', for example
						}
						printf("%sSending move %s.%s\n", BOLDCYAN, move_str.c_str(), RESET);
						sock << move_str + "\n";
					}
				}
			}
			if(tok == "{Game"){
				printf("Game result/init string: |%s|\n", str.c_str());
				in_game = true; // if this is a "creating game" string
				// Got our game result!
				if(!(ss >> tok)) return ICS_Game(UNKNOWN, game_stack); // eat the game number
				if(!(ss >> tok)) return ICS_Game(UNKNOWN, game_stack); // get the "(username" part
				if(!tok.length()) return ICS_Game(UNKNOWN, game_stack);
				tok = tok.substr(1); // get rid of leading '('
				u1 = tok; // first username
				if(!(ss >> tok)) return ICS_Game(UNKNOWN, game_stack); // eat "vs."
				if(!(ss >> tok)) return ICS_Game(UNKNOWN, game_stack); // get "username)"
				tok.pop_back(); // get rid of trailing ')'
				u2 = tok;
				Side us;
				if(u1 == username) us = WHITE;
				else if(u2 == username) us = BLACK;
				else {
					printf("%sERROR: Game result does not have our username!%s\n", BOLDRED, RESET);
					return ICS_Game(UNKNOWN, game_stack);
				}
				bool is_init = false; // if this is just a "creating game" string
				while(true){
					if(!(ss >> tok)) return ICS_Game(UNKNOWN, game_stack);
					if(tok == "Creating"){
						is_init = true;
						break;
					}
					if(tok.back() == '}') break; // great, next one is the result!
				}
				if(!is_init){
					if(!(ss >> tok)) return ICS_Game(UNKNOWN, game_stack); // get the result now
					if(tok == "*") return ICS_Game(STOPPED, game_stack); // was aborted, adjourned, etc.
					Side winner = WHITE; // fix bogus uninitialized warning
					if(tok == "0-1") winner = BLACK;
					else if(tok == "1-0") winner = WHITE;
					else if(tok == "0.5-0.5") return ICS_Game(DRAWN, game_stack);
					if(us == winner) return ICS_Game(WON, game_stack);
					else return ICS_Game(LOST, game_stack);
				}
			}
			if(!in_game){
				++counter;
				if(counter > 125){
					printf("%sGame timed out.%s\n", BOLDCYAN, RESET);
					// OK, no update on the game... unknown result.
					sock << "abort\n"; // just in case
					return ICS_Game(NO_START, game_stack); // timed out
				}
			}
		}
	}
}

ICS_Results FICS::listen(unsigned int seconds){
	assert(logged_in);
	// Note: If 'seconds' is 0, then this will loop indefinitely (until terminated).
	// GuestGYBL (++++) seeking 15 0 unrated standard ("play 5" to respond)
	// GuestGFDN (++++) seeking 5 5 unrated blitz [black] ("play 22" to respond)
	// cookiemaster (1238) seeking 3 12 rated blitz ("play 49" to respond)
	// oxothnk (1434) seeking 3 0 rated blitz m ("play 79" to respond)
	listen_time_up = false;
	if(seconds != 0){ 
		alarm(int(seconds));
		signal(SIGALRM, listen_alarm_handler);
	}
	ICS_Results ret;
	ICS_GameInfo gi;
	ICS_SeekInfo si_tmp;
	std::vector<ICS_SeekInfo> seeks;
	while(!listen_time_up){
		seeks.clear();
		std::string str_orig = sock.read(1024), str;
		std::stringstream str_ss(str_orig);
		while(std::getline(str_ss, str)){
			std::istringstream ss(str);
			std::string tok;
			ss >> std::skipws;
			if(!(ss >> tok)){
				continue; // if no input to be had
			}
			if(tok == "fics%"){ // just a prompt
				continue;
			}
			if(str.find("says") != std::string::npos){
				printf("%sChat: |%s|%s\n", BOLDCYAN, str.c_str(), RESET);
			}
			if(str.find("tells") != std::string::npos){
				printf("%sTell: |%s|%s\n", BOLDCYAN, str.c_str(), RESET);
			}
			if(tok == "<12>"){
				ICS_Game res = do_game(str); // str = continuation string
				si_tmp.game_num = -1; // mark seek as invalid
				handle_game_result(res, si_tmp, ret);
			}
			if(!parse_seek(str, si_tmp)){
				seeks.push_back(si_tmp);
			}
		}
		for(const ICS_SeekInfo& si : seeks){
			printf("|%s| with a rating of %d was seeking a %d+%d %s %s game with game num. %d and modifiers [", si.name.c_str(), si.rating, si.base, si.inc, 
				(si.rated ? "rated" : "unrated"), si.type.c_str(), si.game_num);
			for(unsigned int i = 0; i < si.specials.size(); i++){
				const std::string& on = si.specials[i];
				std::cout << '"' << on << '"';
				if((i + 1) < si.specials.size()) std::cout << ", ";
			}
			std::cout << "]\n";
			if(si.rated == settings.allow_rated || !si.rated == settings.allow_unrated){
				if(std::find(settings.allowed_types.begin(), settings.allowed_types.end(), si.type) != settings.allowed_types.end()){
					// We can play this game! //
					std::stringstream ss;
					ss << "play " << si.game_num << std::endl;
					sock << ss.str();
					printf("%sSent gameplay request (%s).%s\n", BOLDCYAN, ss.str().substr(0, ss.str().length() - 1).c_str(), RESET);
					ICS_Game res = do_game(""); // "" = continuation string
					handle_game_result(res, si, ret);
				}
			}
		}
	}
	printf("%sFinal Game Record (W-L-D-U): %u-%u-%u-%u%s\n", BOLDCYAN, ret.won, ret.lost, ret.drawn, ret.unknown, RESET);
	return ret;
}






































