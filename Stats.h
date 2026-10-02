#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <fstream>
#include <algorithm>
#include "GameModeMenu.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

// Lifetime play statistics, shown on the STATS page (StatsMenu.h): one set
// for all games combined, and one per kind of game (Standard, Lucky
// Ladies, Player's Edge, Lucky Stiff, Free Bet). Table records into both
// the combined set and whichever game is being played (setGame(), from
// Table::configureGameMode()) as hands settle and decisions are made;
// mina.cpp saves it at the same moments it saves game progress. Kept
// separate from SaveData so Restart (which wipes the game) never wipes
// these.
//
// Stored like SaveData: a key=value text file under SDL_GetPrefPath on
// native builds, localStorage on the web. Unknown/missing keys load as 0.
// The combined set keeps its original key names, so stats saved before
// the per-game sets existed still load.
struct Stats
{
	// Order matters: it's the on-disk key order and the web key indexes.
	enum Field{
		HandsPlayed, Wins, Losses, Pushes, Blackjacks, Busts, Surrenders,
		Doubles, Splits, NetWinnings, BiggestWin, CurrentStreak, BestStreak,
		JackpotHits, InsuranceTaken, DecisionsTotal, DecisionsCorrect,
		// Per side bet (SideBet order), SIDE_BET_FIELDS each -- see
		// recordSideBet(). Only ever append new fields after these.
		LuckyLadiesBets, LuckyLadiesWagered, LuckyLadiesHits, LuckyLadiesWon, LuckyLadiesLost,
		MatchUpBets, MatchUpWagered, MatchUpHits, MatchUpWon, MatchUpLost,
		MatchDownBets, MatchDownWagered, MatchDownHits, MatchDownWon, MatchDownLost,
		LuckyStiffBets, LuckyStiffWagered, LuckyStiffHits, LuckyStiffWon, LuckyStiffLost,
		Push22Bets, Push22Wagered, Push22Hits, Push22Won, Push22Lost,
		PotOfGoldBets, PotOfGoldWagered, PotOfGoldHits, PotOfGoldWon, PotOfGoldLost,
		CountQuizzes, CountQuizCorrect,
		FieldCount
	};

	enum SideBet{ LuckyLadies, MatchUp, MatchDown, LuckyStiff, Push22, PotOfGold, SideBetCount };

	// Offsets within one side bet's block of fields.
	enum SideBetField{ SideBets, SideWagered, SideHits, SideWon, SideLost, SIDE_BET_FIELDS };

	// Which set of stats: everything, or one kind of game.
	enum Scope{ AllGames, StandardGame, LuckyLadiesGame, PlayersEdgeGame, LuckyStiffGame, FreeBetGame, ScopeCount };

	static Field sideBetField(SideBet bet, SideBetField field){
		return (Field)(LuckyLadiesBets + bet * SIDE_BET_FIELDS + field);
	}

	static Scope scopeFor(GameMode mode){
		if(hasLuckyLadies(mode)) return LuckyLadiesGame;
		if(isPlayersEdge(mode)) return PlayersEdgeGame;
		if(hasLuckyStiff(mode)) return LuckyStiffGame;
		if(isFreeBet(mode)) return FreeBetGame;
		return StandardGame;
	}

	static const char* scopeName(int scope){
		static const char* NAMES[ScopeCount] = { "ALL GAMES", "STANDARD", "LUCKY LADIES", "PLAYERS EDGE", "LUCKY STIFF", "FREE BET" };
		return NAMES[scope];
	}

	long long values[ScopeCount][FieldCount] = {};

	// The game now being played -- everything recorded also goes into its set.
	void setGame(GameMode mode){ current = scopeFor(mode); }

	long long get(Field f) const { return values[AllGames][f]; }
	long long get(int scope, Field f) const { return values[scope][f]; }
	long long get(SideBet bet, SideBetField field) const { return values[AllGames][sideBetField(bet, field)]; }
	long long get(int scope, SideBet bet, SideBetField field) const { return values[scope][sideBetField(bet, field)]; }

	// One settled hand. net is what it won or lost in real money (stake
	// excluded: +25 for an even-money win on 25, -25 for a loss, 0 for a
	// push).
	void recordHand(long long net, bool win, bool push, bool blackjack, bool bust, bool surrender){
		forEachScope([&](long long* v){
			v[HandsPlayed]++;
			v[NetWinnings] += net;
			if(blackjack) v[Blackjacks]++;
			if(bust) v[Busts]++;
			if(surrender) v[Surrenders]++;

			if(win){
				v[Wins]++;
				v[BiggestWin] = std::max(v[BiggestWin], net);
				v[CurrentStreak] = std::max(0LL, v[CurrentStreak]) + 1;
				v[BestStreak] = std::max(v[BestStreak], v[CurrentStreak]);
			} else if(push){
				v[Pushes]++;
			} else{
				v[Losses]++;
				v[CurrentStreak] = 0;
			}
		});
	}

	// One settled side bet. credit is everything paid back, stake included
	// (0 on a loss, the stake alone on a push). Won is the profit on hits,
	// Lost the stakes lost; both also count toward NetWinnings.
	void recordSideBet(SideBet bet, long long wager, long long credit){
		forEachScope([&](long long* v){
			v[sideBetField(bet, SideBets)]++;
			v[sideBetField(bet, SideWagered)] += wager;
			if(credit > wager){
				v[sideBetField(bet, SideHits)]++;
				v[sideBetField(bet, SideWon)] += credit - wager;
			} else if(credit < wager){
				v[sideBetField(bet, SideLost)] += wager - credit;
			}
			v[NetWinnings] += credit - wager;
		});
	}

	// Money won or lost outside a hand or side bet (insurance, Envy Bonus).
	void addNet(long long amount){
		forEachScope([&](long long* v){ v[NetWinnings] += amount; });
	}

	void recordDecision(bool matchedChart){
		forEachScope([&](long long* v){
			v[DecisionsTotal]++;
			if(matchedChart)
				v[DecisionsCorrect]++;
		});
	}

	void bump(Field f){
		forEachScope([&](long long* v){ v[f]++; });
	}

	void reset(){
		for(auto& scope : values)
			for(long long& v : scope)
				v = 0;
	}

	static const char* key(int f){
		static const char* KEYS[FieldCount] = {
			"hands", "wins", "losses", "pushes", "blackjacks", "busts", "surrenders",
			"doubles", "splits", "net", "biggestWin", "streak", "bestStreak",
			"jackpots", "insurance", "decisions", "decisionsCorrect",
			"llBets", "llWagered", "llHits", "llWon", "llLost",
			"muBets", "muWagered", "muHits", "muWon", "muLost",
			"mdBets", "mdWagered", "mdHits", "mdWon", "mdLost",
			"lsBets", "lsWagered", "lsHits", "lsWon", "lsLost",
			"p22Bets", "p22Wagered", "p22Hits", "p22Won", "p22Lost",
			"pogBets", "pogWagered", "pogHits", "pogWon", "pogLost",
			"countQuizzes", "countQuizCorrect"
		};
		return KEYS[f];
	}

	// A per-game key: "std.hands", "pe.net", ...; the combined set has none.
	static std::string scopedKey(int scope, int f){
		static const char* PREFIX[ScopeCount] = { "", "std.", "ll.", "pe.", "ls.", "fb." };
		return std::string(PREFIX[scope]) + key(f);
	}

	void load(){
#ifdef __EMSCRIPTEN__
		// Web keys are numbered: the combined set from 0 (as before), each
		// game's set WEB_STRIDE further on, so adding fields never moves
		// another set's keys.
		for(int s = 0; s < ScopeCount; s++)
			for(int f = 0; f < FieldCount; f++){
				values[s][f] = EM_ASM_INT({
					var v = localStorage.getItem('underwayBlackjackStat' + $0);
					return v === null ? 0 : parseInt(v);
				}, s * WEB_STRIDE + f);
			}
#else
		std::ifstream in(path());
		std::string line;
		while(std::getline(in, line)){
			size_t eq = line.find('=');
			if(eq == std::string::npos)
				continue;
			std::string k = line.substr(0, eq);
			bool found = false;
			for(int s = 0; s < ScopeCount && !found; s++){
				for(int f = 0; f < FieldCount; f++){
					if(k == scopedKey(s, f)){
						try{ values[s][f] = std::stoll(line.substr(eq + 1)); } catch(...){}
						found = true;
						break;
					}
				}
			}
		}
#endif
	}

	void save(){
#ifdef __EMSCRIPTEN__
		// localStorage stores strings; int range is plenty for a web session's stats.
		for(int s = 0; s < ScopeCount; s++)
			for(int f = 0; f < FieldCount; f++){
				EM_ASM({ localStorage.setItem('underwayBlackjackStat' + $0, $1); }, s * WEB_STRIDE + f, (int)values[s][f]);
			}
#else
		std::ofstream out(path());
		for(int s = 0; s < ScopeCount; s++)
			for(int f = 0; f < FieldCount; f++)
				out << scopedKey(s, f) << "=" << values[s][f] << "\n";
#endif
	}

private:
	static constexpr int WEB_STRIDE = 200;
	Scope current = StandardGame;

	template<typename Fn>
	void forEachScope(Fn fn){
		fn(values[AllGames]);
		fn(values[current]);
	}

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
