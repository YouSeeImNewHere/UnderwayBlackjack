#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <fstream>
#include <cstdio>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

// Cross-launch save so the menu can tell Start apart from Resume/Restart,
// and so Resume can actually restore the game it remembers (player count +
// each player's starting bankroll, chosen on SetupMenu) instead of just
// remembering that *a* game once existed. Extend this struct further later
// (in-round chip counts, etc.) the same way -- add fields that default
// sanely when missing from an older save.
//
// Native: a small text file under SDL_GetPrefPath (the OS's proper
// per-user app-data location, not the working directory, so it's found the
// same way whether launched from Visual Studio or a double-clicked exe).
// Web: a handful of localStorage keys, since Emscripten's virtual
// filesystem doesn't persist across page loads on its own.
struct SaveData
{
	bool gameStarted = false;
	// GameMode cast to/from int (see GameModeMenu.h) -- SaveData doesn't
	// include GameModeMenu.h itself to keep it a plain, dependency-light
	// data file; callers cast at the boundary (see mina.cpp).
	int gameModeIndex = 0;
	int numberOfPlayers = 1;
	int bankrolls[3] = {500, 500, 500};
	int initialBets[3] = {25, 25, 25};
	// Only meaningful for a GameMode with a side bet -- 0 otherwise.
	int sideBetSizes[3] = {0, 0, 0};

	void load(){
#ifdef __EMSCRIPTEN__
		gameStarted = EM_ASM_INT({
			return localStorage.getItem('underwayBlackjackSave') ? 1 : 0;
		}) != 0;

		if(!gameStarted)
			return;

		gameModeIndex = EM_ASM_INT({
			var v = localStorage.getItem('underwayBlackjackMode');
			return v ? parseInt(v) : 0;
		});
		numberOfPlayers = EM_ASM_INT({
			var v = localStorage.getItem('underwayBlackjackPlayers');
			return v ? parseInt(v) : 1;
		});
		for(int i = 0; i < 3; i++){
			bankrolls[i] = EM_ASM_INT({
				var v = localStorage.getItem('underwayBlackjackBankroll' + $0);
				return v ? parseInt(v) : 500;
			}, i);
			initialBets[i] = EM_ASM_INT({
				var v = localStorage.getItem('underwayBlackjackBet' + $0);
				return v ? parseInt(v) : 25;
			}, i);
			sideBetSizes[i] = EM_ASM_INT({
				var v = localStorage.getItem('underwayBlackjackSideBet' + $0);
				return v ? parseInt(v) : 0;
			}, i);
		}
#else
		std::ifstream in(path());
		gameStarted = in.good();

		if(!gameStarted)
			return;

		std::string line;
		while(std::getline(in, line)){
			size_t eq = line.find('=');
			if(eq == std::string::npos)
				continue;

			std::string key = line.substr(0, eq);
			std::string value = line.substr(eq + 1);

			if(key == "gameModeIndex") gameModeIndex = std::stoi(value);
			else if(key == "numberOfPlayers") numberOfPlayers = std::stoi(value);
			else if(key == "bankroll0") bankrolls[0] = std::stoi(value);
			else if(key == "bankroll1") bankrolls[1] = std::stoi(value);
			else if(key == "bankroll2") bankrolls[2] = std::stoi(value);
			else if(key == "bet0") initialBets[0] = std::stoi(value);
			else if(key == "bet1") initialBets[1] = std::stoi(value);
			else if(key == "bet2") initialBets[2] = std::stoi(value);
			else if(key == "sideBet0") sideBetSizes[0] = std::stoi(value);
			else if(key == "sideBet1") sideBetSizes[1] = std::stoi(value);
			else if(key == "sideBet2") sideBetSizes[2] = std::stoi(value);
		}
#endif
	}

	// Called once SetupMenu's GO is confirmed -- persists exactly what the
	// player configured (game mode from GameModeMenu, players/bankrolls/
	// side-bet size from SetupMenu) so Resume can reconstruct it on a
	// future launch.
	void saveGameConfig(int modeIndex, int players, const int bankrollValues[3], const int betValues[3], const int sideBetValues[3]){
		gameStarted = true;
		gameModeIndex = modeIndex;
		numberOfPlayers = players;
		for(int i = 0; i < 3; i++){
			bankrolls[i] = bankrollValues[i];
			initialBets[i] = betValues[i];
			sideBetSizes[i] = sideBetValues[i];
		}

#ifdef __EMSCRIPTEN__
		EM_ASM({
			localStorage.setItem('underwayBlackjackSave', '1');
			localStorage.setItem('underwayBlackjackMode', $0);
			localStorage.setItem('underwayBlackjackPlayers', $1);
			localStorage.setItem('underwayBlackjackBankroll0', $2);
			localStorage.setItem('underwayBlackjackBankroll1', $3);
			localStorage.setItem('underwayBlackjackBankroll2', $4);
			localStorage.setItem('underwayBlackjackBet0', $5);
			localStorage.setItem('underwayBlackjackBet1', $6);
			localStorage.setItem('underwayBlackjackBet2', $7);
			localStorage.setItem('underwayBlackjackSideBet0', $8);
			localStorage.setItem('underwayBlackjackSideBet1', $9);
			localStorage.setItem('underwayBlackjackSideBet2', $10);
		}, gameModeIndex, numberOfPlayers, bankrolls[0], bankrolls[1], bankrolls[2], initialBets[0], initialBets[1], initialBets[2], sideBetSizes[0], sideBetSizes[1], sideBetSizes[2]);
#else
		std::ofstream out(path());
		out << "gameStarted=1\n";
		out << "gameModeIndex=" << gameModeIndex << "\n";
		out << "numberOfPlayers=" << numberOfPlayers << "\n";
		out << "bankroll0=" << bankrolls[0] << "\n";
		out << "bankroll1=" << bankrolls[1] << "\n";
		out << "bankroll2=" << bankrolls[2] << "\n";
		out << "bet0=" << initialBets[0] << "\n";
		out << "bet1=" << initialBets[1] << "\n";
		out << "bet2=" << initialBets[2] << "\n";
		out << "sideBet0=" << sideBetSizes[0] << "\n";
		out << "sideBet1=" << sideBetSizes[1] << "\n";
		out << "sideBet2=" << sideBetSizes[2] << "\n";
#endif
	}

	// Wipes the save (Restart) so a future launch that never reaches
	// SetupMenu's GO (app closed mid-setup) starts from Start again,
	// instead of silently resuming the pre-restart game.
	void clear(){
		gameStarted = false;
#ifdef __EMSCRIPTEN__
		EM_ASM({
			localStorage.removeItem('underwayBlackjackSave');
			localStorage.removeItem('underwayBlackjackMode');
			localStorage.removeItem('underwayBlackjackPlayers');
			localStorage.removeItem('underwayBlackjackBankroll0');
			localStorage.removeItem('underwayBlackjackBankroll1');
			localStorage.removeItem('underwayBlackjackBankroll2');
			localStorage.removeItem('underwayBlackjackBet0');
			localStorage.removeItem('underwayBlackjackBet1');
			localStorage.removeItem('underwayBlackjackBet2');
			localStorage.removeItem('underwayBlackjackSideBet0');
			localStorage.removeItem('underwayBlackjackSideBet1');
			localStorage.removeItem('underwayBlackjackSideBet2');
		});
#else
		std::remove(path().c_str());
#endif
	}

private:
#ifndef __EMSCRIPTEN__
	std::string path(){
		char* pref = SDL_GetPrefPath("UnderwayBlackjack", "Save");
		std::string p = pref ? std::string(pref) + "save.txt" : "save.txt";
		if(pref)
			SDL_free(pref);
		return p;
	}
#endif
};
