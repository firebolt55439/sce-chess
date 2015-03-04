#ifndef UCI_INC
#define UCI_INC

#include "Common.h"
#include "Bitboards.h"
#include "Board.h"
#include "Search.h"
#include "Threads.h"

extern std::string ENGINE_VERSION;

namespace UCI {
	void init(void); // init UCI stuff
	void loop(int argc, char** argv); // main UCI loop
	std::string value(Value v); // This returns a UCI-formatted value (e.g. "cp 5" or "mate -3")
	std::string move(Move m); // A wrapper around Moves::format<false>(Move m) for getting coordinate notation of a move
}

static const std::string StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

#endif // #ifndef UCI_INC