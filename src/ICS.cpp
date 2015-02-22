#include "Common.h"
#include "Bitboards.h"
#include "Board.h"
#include "MoveGen.h"
#include "Search.h"
#include "ICS.h"
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
	
int FICS::try_login(std::string user, std::string pass){
	assert(user.length() >= 3);
	if(int r = sock.open_connection("freechess.org", 5000)){
		printf("Could not open connection to FICS (code %d).\n", r);
		return r;
	}
	sock << user << "\n";
	sock << pass << "\n";
	std::string str = "";
	size_t i;
	while(!logged_in){
		str = sock.read(256);
		if((i = str.find("Starting FICS session as")) != std::string::npos){
			logged_in = true;
			return 0;
		}
		if(str.find("is not a registered name") != std::string::npos){
			return 1; // bad username
		}
		if(str.find("Invalid password!") != std::string::npos){
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

ICS_GameInfo FICS::parse_style(std::string line){
	return ICS_GameInfo();
}

ICS_Results FICS::listen(unsigned int seconds){
	// GuestGYBL (++++) seeking 15 0 unrated standard ("play 5" to respond)
	// GuestGFDN (++++) seeking 5 5 unrated blitz [black] ("play 22" to respond)
	// cookiemaster (1238) seeking 3 12 rated blitz ("play 49" to respond)
	// oxothnk (1434) seeking 3 0 rated blitz m ("play 79" to respond)
	listen_time_up = false;
	signal(SIGALRM, listen_alarm_handler);
	ICS_Results ret;
	while(!listen_time_up){
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
			// TODO: Proceed under assumption then that it is a name
			std::string name = tok;
			if(!(ss >> tok)) continue;
			if(tok[0] == '(' && tok.back() == ')'){ // must be a seek request then
				printf("Seek request: |\n%s|\n", str.c_str());
				// Must be the rating then. //
				tok = tok.substr(1);
				tok.pop_back();
				int rating = (tok != "++++") ? atoi(tok.c_str()) : -1; // opponent's rating (if given)
				// "Seeking" //
				printf("1_Tok: |%s|\n", tok.c_str());
				if(!(ss >> tok)) continue;
				if(tok != "seeking") continue;
				// Time Controls (e.g. 15 0) //
				printf("2_Tok: |%s|\n", tok.c_str());
				if(!(ss >> tok)) continue;
				int base = atoi(tok.c_str());
				if(!(ss >> tok)) continue;
				int inc = atoi(tok.c_str());
				// "Rated"/"Unrated" //
				printf("3_Tok: |%s|\n", tok.c_str());
				if(!(ss >> tok)) continue;
				bool rated;
				if(tok == "rated") rated = true;
				else if(tok == "unrated") rated = false;
				else continue;
				// Type of Game (e.g. standard, blitz, etc.) //
				printf("4_Tok: |%s|\n", tok.c_str());
				if(!(ss >> tok)) continue;
				std::string type = tok;
				if((tok != "standard") && (tok != "blitz")){
					continue; // can't play variants
				}
				// Special Modifiers (e.g. "[black]", "m", etc.) //
				printf("5_Tok: |%s|\n", tok.c_str());
				if(!(ss >> tok)) continue;
				bool ok_special = true;
				while(tok[0] == '[' || tok.length() == 1){
					if(!(ss >> tok)) ok_special = false;
				}
				if(!ok_special) continue;
				// Now the '("play #" to respond)' part. //
				printf("6_Tok: |%s|\n", tok.c_str());
				if(tok[0] != '(') continue;
				if(tok.find("play") == std::string::npos) continue;
				if(!(ss >> tok)) continue;
				int game_num = atoi(tok.c_str());
				// Dump //
				printf("%s with a rating of %d was seeking a %d+%d %s game with game num. %d.\n", name.c_str(), rating, base, inc, (rated ? "rated" : "unrated"), game_num);
				getchar();
			}
		}
	}
	return ret;
}






































