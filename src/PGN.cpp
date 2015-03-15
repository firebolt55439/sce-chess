#include "Common.h"
#include "PGN.h"
#include "Board.h"
#include "MoveGen.h"
#include "MoveSort.h"
#include "UCI.h"
#include <iomanip>
#include <fstream>
#include <sstream>

std::string req_tags[] =
{
	"Event",
	"Site",
	"Date",
	"Round",
	"White",
	"Black",
	"Result" // computed
}; // required tag strings

std::string ext_tags[] =
{
	"WhiteTitle", "BlackTitle",
	"WhiteElo", "BlackElo", // *
	"WhiteUSCF", "BlackUSCF",
	"WhiteNA", "BlackNA",
	"WhiteType", "BlackType", // *
	"EventDate",
	"EventSponsor",
	"Section",
	"Stage",
	"Board",
	"Opening", // *
	"Variation", // *
	"SubVariation", // *
	"ECO",
	"NIC",
	"Time", // *
	"UTCTime",
	"UTCDate",
	"TimeControl", // * ('?', '-' (none), '40/9000' (9000 seconds for 40 moves), '5+2', '*10' (total 10)
	"SetUp", // * 
	"FEN", // * alternative starting FEN
	"Termination", // *
	"Annotator", // *
	"Mode", // *
	"PlyCount" // *
}; // extra/supplemental PGN tag strings

const std::string test_game = "[Event \"\"] \n[Site \"Niksic\"] \n[Date \"1978.??.??\"] \n[Round \"1\"] \n[White \"Velimirovic,D\"] \n[Black \"Timman,J\"] \n[Result \"0-1\"] \n[BlackElo \"2585\"] \n[WhiteElo \"2520\"] \n\n1. a3 g6 2. g3 Bg7 3. Bg2 e5 4. d3 Ne7 5. c3 d5 6. Nd2 O-O 7. h4 h6 8. \ne4 Be6 9. Ngf3 Nd7 10. O-O a5 11. exd5 Bxd5 12. Re1 Nc6 13. Qc2 Nc5 14. \nh5 g5 15. Ne4 Nb3 16. Rb1 f5 17. Ned2 Nxd2 18. Bxd2 a4 19. Be3 e4 20. Nd2 \nexd3 21. Qxd3 Bxg2 22. Qxd8 Raxd8 23. Kxg2 f4 24. gxf4 gxf4 25. Bd4 Nxd4 \n26. cxd4 Rxd4 27. Nf3 Rd3 28. Rbc1 c6 29. Rc4 Bxb2 30. Rb4 Bc3 0-1 ";

void PGN::init(void){
	/*
	std::string inp = test_game;
	while(true){
		PGN_Reader reader;
		reader.init(inp);
		reader.read_all();
		if(reader.games_num()){
			PGN_Writer writer;
			writer.init(reader.get_game(0));
			std::cout << "\n" + writer.formatted() + "\n";
			inp = writer.formatted();
		} else {
			printf("Failed.\n");
			break;
		}
		getchar();
	}
	*/
}

/* PGN Writer Implementation */

std::string PGN_Options::formatted(void){
	// Extract PGN Writer output as PGN text. //
	std::stringstream ss;
	for(int i = 0; i < 7; i++){
		ss << '[' << req_tags[i] << " \"" << builtin[i] << "\"]\n";
	}
	for(const std::pair<PGN_Ext_Tag, std::string>& on : addl){
		ss << '[' << ext_tags[std::get<0>(on)] << " \"" << std::get<1>(on) << "\"]\n";
	}
	return ss.str();
}

void PGN_Writer::clear(void){
	// Clear PGN Writer to "factory state". //
	opts = PGN_Options();
	res = Unknown;
	num = 0;
	line_len = 0;
	ss.str(""); // clear stringstream
}

void PGN_Writer::init(PGN_Options op, PGN_Result rt){
	// Initialize PGN Writer with given options and result. //
	bss = Search::BoardStateStack(new std::stack<BoardState>());
	board.init_from(StartFEN);
	opts = op;
	res = rt;
	// Add PGN Result //
	std::string rstr = "(invalid)";
	if(res == WhiteWin) rstr = "1-0";
	else if(res == BlackWin) rstr = "0-1";
	else if(res == Draw) rstr = "1/2-1/2";
	else if(res == Stopped) rstr = "*";
	opts.builtin[6] = rstr; // compute result string
	// Add Annotator //
	opts.addl[AnnotatorStr] = "SCE " + ENGINE_VERSION;
	// Init from custom FEN if needed. //
	if(opts.addl.find(FEN) != opts.addl.end()){
		board.init_from(opts.addl[FEN]);
	}
	// Add time if needed. //
	if(!opts.builtin[Date].length()){
		char buf[80];
		time_t now = time(NULL);
		struct tm time_struct = *localtime(&now);
		strftime(buf, sizeof(buf), "%B %d, %Y", &time_struct);
		opts.add(Date, std::string(buf));
	}
	// Save //
	assert(!num && !line_len && "clear() should be called before init()");
	ss << opts.formatted() + "\n";
}

void PGN_Writer::init(PGN_Game game){
	init(game.opts, game.res);
	for(const PGN_Move& on : game.moves){
		write(on);
	}
}

void PGN_Writer::write(PGN_Move move){
	// Write given PGN move. //
	std::stringstream cur;
	if(!(num % 2)){
		// White to move.
		if(num) cur << ' ';
		cur << (num / 2 + 1) << '.';
	}
	cur << ' ' << Moves::format<true>(move.enc, board);
	if(move.comment.size()){
		for(const int& on : move.comment){
			cur << " $" << on;
		}
	}
	line_len += int(cur.str().length());
	ss << cur.str();
	if(line_len > 80){
		ss << "\n";
		line_len = 0;
	}
	cur.str(" ");
	if(move.annot.length()){
		std::string& on = move.annot;
		cur << " {";
		while((line_len + on.length()) > 80){
			int left = 80 - line_len; // how many characters we can still write in this line
			if(left){
				while(!isspace(on[left - 1]) && (left < on.length())) ++left; // we don't want to cut off something in the middle though (e.g. -1.55)
				// Add 'left' chars from string into line. //
				std::string part = on.substr(0, left);
				on = (left >= on.length() ? "" : on.substr(left));
				cur << part + "\n";
			} else cur << "\n";
			line_len = 0;
		}
		if(on.length()){
			line_len += on.length();
			cur << on;
		}
		cur << '}';
		ss << cur.str();
	}
	// And update the board, etc. //
	bss->push(BoardState());
	board.do_move(move.enc, bss->top());
	++num;
}

std::string PGN_Writer::formatted(void){
	ss << " " + opts.builtin[6];
	return ss.str();
}

/* PGN Parser/Reader Implementation */

void PGN_Reader::init(std::string inp){
	clear();
	ss = std::istringstream(inp);
	ss >> std::skipws;
}

void PGN_Reader::clear(void){
	reset();
	ss.str(""); // clear internal buffer
	games.clear(); // clear games
}

void PGN_Reader::reset(void){
	// Reset board and BSS. //
	bss.release(); // release current BSS
	bss = Search::BoardStateStack(new std::stack<BoardState>()); // re-init. BSS
	board.init_from(StartFEN); // reset board
}

void PGN_Reader::read_all(void){
	int r;
	std::string tok;
	std::istringstream ss2; // for avoiding spurious error messages when we have parsed all games and there is only whitespace/EOF left
	unsigned int suc = 0, tot = 0;
	while(true){
		ss2.str(ss.str());
		ss2 >> std::skipws;
		ss2.seekg(ss.tellg());
		if(!(ss2 >> tok)) break; // no more input
		++tot;
		if((r = parse())){
			printf("Failed with error code %d.\n", r);
			// Now skip to the next game.
			while(ss >> tok){
				if(tok == "[Event"){
					ss >> std::noskipws;
					while(ss.peek() != '[') ss.unget();
					ss >> std::skipws;
					break;
				}
			}
		} else ++suc;
		reset();
	}
	printf("Successfully read %u out of %u games.\n", suc, tot);
}

int PGN_Reader::parse(void){
	/*
	[Event ""] 
	[Site "Niksic"] 
	[Date "1978.??.??"] 
	[Round "1"] 
	[White "Velimirovic,D"] 
	[Black "Timman,J"] 
	[Result "0-1"] 
	[BlackElo "2585"] 
	[WhiteElo "2520"] 

	1. a3 g6 2. g3 Bg7 3. Bg2 e5 4. d3 Ne7 5. c3 d5 6. Nd2 O-O 7. h4 h6 8. 
	e4 Be6 9. Ngf3 Nd7 10. O-O a5 11. exd5 Bxd5 12. Re1 Nc6 13. Qc2 Nc5 14. 
	h5 g5 15. Ne4 Nb3 16. Rb1 f5 17. Ned2 Nxd2 18. Bxd2 a4 19. Be3 e4 20. Nd2 
	exd3 21. Qxd3 Bxg2 22. Qxd8 Raxd8 23. Kxg2 f4 24. gxf4 gxf4 25. Bd4 Nxd4 
	26. cxd4 Rxd4 27. Nf3 Rd3 28. Rbc1 c6 29. Rc4 Bxb2 30. Rb4 Bc3 0-1 
	*/
	std::string tok;
	// First, parse tags. //
	uint8_t tags_parsed = 0; // only for the seven tag roster - when is 11111110 in binary (254) from right = 2^0, then done
	PGN_Options opts;
	bool keep_going = false;
	while((tags_parsed != uint8_t(254)) || keep_going){
		if(!(ss >> tok)) return 1; // ran out of input
		if(tok.length() < 2) return 2; // invalid tag - is something else
		if(tok[0] != '[') return 2;
		std::string tag_name = tok.substr(1);
		int got = -1;
		bool got_req = false;
		for(unsigned int i = 0; i < 7; i++){ // first check 7-tag-roster
			if(tag_name == req_tags[i]){
				tags_parsed |= uint8_t(1U << (i + 1));
				got = int(i);
				got_req = true;
				break;
			}
		}
		if(got == -1){
			for(unsigned int i = 0; i < 30; i++){
				if(tag_name == ext_tags[i]){
					got = int(i);
					break;
				}
			}
		}
		if(got != -1){ // if it is a valid tag (UPDATE: no longer warning)
			if(!(ss >> tok)) return 1;
			if(tok.length() < 3) return 4; // invalid tag value
			std::string val = tok.substr(1); // get rid of the leading '"'
			if(val.find(']') == std::string::npos){
				ss >> std::noskipws;
				char c;
				while((c = ss.get()) != ']'){
					if(c == EOF) return 4; // invalid tag value
					val.push_back(c);
				}
				ss >> std::skipws;
			} else val.pop_back(); // need to get rid of the ']' as well
			val.pop_back(); // get rid of the trailing '"'
			if(got_req){
				PGN_Req_Tag tag = (PGN_Req_Tag) got;
				opts.add(tag, val);
			} else {
				PGN_Ext_Tag tag = (PGN_Ext_Tag) got;
				opts.add(tag, val);
				if(tag == FEN){
					board.init_from(val);
				}
			}
		}
		if(tags_parsed == uint8_t(254)){
			ss >> std::noskipws;
			while(ss.get() != '\n') ;
			if(ss.peek() == '[') keep_going = true;
			else keep_going = false;
			ss >> std::skipws;
		}
	}
	// Now, let's parse the game moves. //
	unsigned int counter = 0;
	std::vector<PGN_Move> moves;
	PGN_Result res = Unknown;
	while(true){
		++counter;
		// 1. a3 g6
		if(!(ss >> tok)) return 1;
		if(!tok.length()) return -1; // should never happen
		if(tok.back() == '.'){
			// It should be a move number (e.g. '12.' or '2...')
			while(tok.back() == '.') tok.pop_back();
			for(int i = 0; i < tok.length(); i++) if(!isdigit(tok[i])) return 5; // invalid move number
		} else if(tok[0] == '{'){
			// It should be an annotation (e.g. '{<text that can be broken>}')
			std::string annot = (tok.length() > 1) ? tok.substr(1) : "";
			if(tok.back() == '}'){
				annot.pop_back();
			} else {
				ss >> std::noskipws;
				char c;
				while((c = ss.get()) != '}'){
					if(c == EOF) return 8; // annotation ran over EOF
					annot.push_back(c);
				}
				ss >> std::skipws;
			}
			if(moves.size()) moves.back().annot = annot; // update last move's annotation
		} else if(tok[0] == '('){
			// It must be a recursive annotation variation (RAV) - which we will ignore (and skip).
			if(tok.back() != ')'){
				ss >> std::noskipws;
				char c;
				while((c = ss.get()) != ')'){
					if(c == EOF) return 9; // RAV ran over EOF
				}
				ss >> std::skipws;
			}
		} else {
			// Otherwise, we assume it's a move.
			if(tok[0] == '1' || tok[0] == '0' || tok == "*"){
				// It should be a result, which means we are done (e.g. "0-1", "1/2-1/2", etc.)
				if(tok != "0-1" && tok != "1-0" && tok != "1/2-1/2" && tok != "*") return 6; // invalid result token
				if(tok == "0-1") res = BlackWin;
				else if(tok == "1/2-1/2") res = Draw;
				else if(tok == "1-0") res = WhiteWin;
				else if(tok == "*") res = Stopped;
				else assert(false);
				break;
			}
			Move move = Moves::parse</*isAlgebraic=*/true>(tok, board);
			bool ill = false;
			if(move == MOVE_NONE || (ill = !MoveList<LEGAL>(board).contains(move))){
				printf("%s move |%s| (#%lu), conversion failed.\n", (ill ? "Illegal" : "Invalid"), tok.c_str(), moves.size());
				std::cout << board;
				for(PGN_Move on : moves){
					std::cout << Moves::format<false>(on.enc);
					if(type_of(on.enc) == ENPASSANT) std::cout << "(e.p.)";
					std::cout << ' ';
				}
				std::cout << std::endl;
				// Conversion failed, stop.
				return 7; // invalid/incorrectly formed move - could not parse
			}
			bss->push(BoardState());
			board.do_move(move, bss->top());
			PGN_Move m;
			m.enc = move;
			moves.push_back(m);
		}
	}
	PGN_Game ret;
	ret.opts = opts;
	ret.res = res;
	ret.moves = moves;
	games.push_back(ret);
	return 0;
}















































