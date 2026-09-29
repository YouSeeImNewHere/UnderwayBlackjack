#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <fstream>
#include <algorithm>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

// Lifetime play statistics, across every game and seat, shown on the STATS
// page (StatsMenu.h). Table updates it as hands settle and decisions are
// made (see Table::recordHand()/recordDecision()); mina.cpp saves it at
// the same moments it saves game progress. Kept separate from SaveData so
// Restart (which wipes the game) never wipes these.
//
// Stored like SaveData: a key=value text file under SDL_GetPrefPath on
// native builds, localStorage on the web. Unknown/missing keys load as 0.
struct Stats
{
	// Order matters: it's the on-disk key order and the web key indexes.
	enum Field{
		HandsPlayed, Wins, Losses, Pushes, Blackjacks, Busts, Surrenders,
		Doubles, Splits, NetWinnings, BiggestWin, CurrentStreak, BestStreak,
		JackpotHits, InsuranceTaken, DecisionsTotal, DecisionsCorrect,
		FieldCount
	};

	long long values[FieldCount] = {};

	long long get(Field f) const { return values[f]; }

	// One settled hand. net is what it won or lost in real money (stake
	// excluded: +25 for an even-money win on 25, -25 for a loss, 0 for a
	// push).
	void recordHand(long long net, bool win, bool push, bool blackjack, bool bust, bool surrender){
		values[HandsPlayed]++;
		values[NetWinnings] += net;
		if(blackjack) values[Blackjacks]++;
		if(bust) values[Busts]++;
		if(surrender) values[Surrenders]++;

		if(win){
			values[Wins]++;
			values[BiggestWin] = std::max(values[BiggestWin], net);
			values[CurrentStreak] = std::max(0LL, values[CurrentStreak]) + 1;
			values[BestStreak] = std::max(values[BestStreak], values[CurrentStreak]);
		} else if(push){
			values[Pushes]++;
		} else{
			values[Losses]++;
			values[CurrentStreak] = 0;
		}
	}

	void recordDecision(bool matchedChart){
		values[DecisionsTotal]++;
		if(matchedChart)
			values[DecisionsCorrect]++;
	}

	void bump(Field f){ values[f]++; }

	void reset(){
		for(long long& v : values)
			v = 0;
	}

	static const char* key(int f){
		static const char* KEYS[FieldCount] = {
			"hands", "wins", "losses", "pushes", "blackjacks", "busts", "surrenders",
			"doubles", "splits", "net", "biggestWin", "streak", "bestStreak",
			"jackpots", "insurance", "decisions", "decisionsCorrect"
		};
		return KEYS[f];
	}

	void load(){
#ifdef __EMSCRIPTEN__
		for(int f = 0; f < FieldCount; f++){
			values[f] = EM_ASM_INT({
				var v = localStorage.getItem('underwayBlackjackStat' + $0);
				return v === null ? 0 : parseInt(v);
			}, f);
		}
#else
		std::ifstream in(path());
		std::string line;
		while(std::getline(in, line)){
			size_t eq = line.find('=');
			if(eq == std::string::npos)
				continue;
			std::string k = line.substr(0, eq);
			for(int f = 0; f < FieldCount; f++){
				if(k == key(f)){
					try{ values[f] = std::stoll(line.substr(eq + 1)); } catch(...){}
					break;
				}
			}
		}
#endif
	}

	void save(){
#ifdef __EMSCRIPTEN__
		// localStorage stores strings; int range is plenty for a web session's stats.
		for(int f = 0; f < FieldCount; f++){
			EM_ASM({ localStorage.setItem('underwayBlackjackStat' + $0, $1); }, f, (int)values[f]);
		}
#else
		std::ofstream out(path());
		for(int f = 0; f < FieldCount; f++)
			out << key(f) << "=" << values[f] << "\n";
#endif
	}

private:
#ifndef __EMSCRIPTEN__
	std::string path(){
		char* pref = SDL_GetPrefPath("UnderwayBlackjack", "Save");
		std::string p = pref ? std::string(pref) + "stats.txt" : "stats.txt";
		if(pref)
			SDL_free(pref);
		return p;
	}
#endif
};
