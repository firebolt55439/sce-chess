#ifndef ENDG_INC
#define ENDG_INC

#include "Common.h"
#include "Evaluation.h"
#include <map>

enum EndgameType {
	NONE,
	KPK,
	KRK,
	KQK
	// TODO: More
};

struct EndgameBase {
	//virtual ~EndgameBase(void){ }
	virtual Side strong_side(void) const = 0;
	virtual Value operator()(const Board&) const = 0;
};

template<EndgameType E>
struct Endgame : public EndgameBase {
	private:
		Side strongSide, weakSide;
	
	public:
		explicit Endgame(Side c) : strongSide(c), weakSide(~c) { }
		
		Side strong_side(void) const {
			return strongSide;
		}
		
		Value operator()(const Board&) const;
};
	

class Endgames {
	private:
		std::map<Key, EndgameBase*> key_map;
		template<EndgameType E> void add(std::string code);
	public:
		Endgames(void){ }
		void init(void);
		//~Endgames(void);
		
		EndgameBase* probe(Key key, EndgameBase* ret){
			return (ret = (key_map.count(key) ? key_map[key] : NULL));
		}
};

namespace EndgameN {
	void init(void);
	Value probe(const Board& pos, bool& found);
	extern Endgames EndGames;
	
	inline Endgames& get_endgames(void){
		return EndGames;
	}
}

#endif // #ifndef ENDG_INC