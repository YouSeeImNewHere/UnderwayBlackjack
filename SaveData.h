#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <fstream>
#include <cstdio>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

// Cross-launch save so the menu can tell Start apart from Resume/Restart,
// and so Resume puts the player back where they left off: the game mode,
// player count, each seat's bankroll *as of the last finished round*, their
// bets and side bets, and the Game Options (dealer speed, face-down
// doubles, hide hands). Fields missing from an older save fall back to
// sane defaults, so a save from a previous version still loads.
//
// Written three times over a game's life: once when SetupMenu's GO starts
// it (saveGameConfig()), whenever Game Options are applied (saveOptions()),
// and at the start of every betting phase once the last round's payouts
// have landed (saveProgress(), from mina.cpp). A round in progress when the
// app closes is simply not counted -- Resume goes back to the betting
// phase before it.
//
// Native: a small text file under SDL_GetPrefPath (the OS's proper
// per-user app-data location, not the working directory, so it's found the
// same way whether launched from Visual Studio or a double-clicked exe).
// Web: localStorage keys, since Emscripten's virtual filesystem doesn't
// persist across page loads on its own.
struct SaveData
{
	bool gameStarted = false;
	// GameMode cast to/from int (see GameModeMenu.h) -- SaveData doesn't
	// include GameModeMenu.h itself to keep it a plain, dependency-light
	// data file; callers cast at the boundary (see mina.cpp).
	int gameModeIndex = 0;
	int numberOfPlayers = 1;
	// Each seat's buy-in as chosen on SetupMenu (what a re-buy adds).
	int bankrolls[5] = {500, 500, 500, 500, 500};
	int initialBets[5] = {25, 25, 25, 25, 25};
	// Only meaningful for a GameMode with a side bet -- 0 otherwise.
	int sideBetSizes[5] = {0, 0, 0, 0, 0};

	// Live progress (saveProgress()). hasProgress is false for a save made
	// before any round finished, or by an older version -- Resume then
	// starts everyone from their buy-in, same as before.
	bool hasProgress = false;
	int currentBankrolls[5] = {500, 500, 500, 500, 500};
	// Everything each seat has bought in with so far (first buy-in plus
	// every re-buy) -- Person::getInitialBankroll()'s break-even point.
	int totalBuyIns[5] = {500, 500, 500, 500, 500};

	// GameOptionsMenu's choices (saveOptions()). dealerSpeed is the
	// slider's position, 0 (slowest) to 100 (fastest). Saved under a new
	// key, dealerSpeedPct: the old "dealerSpeed" key held the 3 presets
	// (0 slow, 1 normal, 2 fast) and is converted by speedFromPreset().
	bool hasOptions = false;
	int dealerSpeed = DEFAULT_DEALER_SPEED;
	static constexpr int DEFAULT_DEALER_SPEED = 40;
	bool faceDownDoubles = false;
	bool hideInactiveHands = false;
	bool soundEffects = true;

	// HOW TO PLAY opens by itself until it's been closed once.
	bool tutorialSeen = false;

	void load(){
#ifdef __EMSCRIPTEN__
		gameStarted = EM_ASM_INT({
			return localStorage.getItem('underwayBlackjackSave') ? 1 : 0;
		}) != 0;

		gameModeIndex = webGet(0, 0);
		numberOfPlayers = webGet(1, 1);
		hasProgress = webGet(2, 0) != 0;
		hasOptions = webGet(3, 0) != 0;
		dealerSpeed = webGet(8, -1);
		if(dealerSpeed < 0)
			dealerSpeed = speedFromPreset(webGet(4, 1));
		faceDownDoubles = webGet(5, 0) != 0;
		hideInactiveHands = webGet(6, 0) != 0;
		tutorialSeen = webGet(7, 0) != 0;
		soundEffects = webGet(9, 1) != 0;
		for(int i = 0; i < 5; i++){
			bankrolls[i] = webGetSeat(0, i, 500);
			initialBets[i] = webGetSeat(1, i, 25);
			sideBetSizes[i] = webGetSeat(2, i, 0);
			currentBankrolls[i] = webGetSeat(3, i, bankrolls[i]);
			totalBuyIns[i] = webGetSeat(4, i, bankrolls[i]);
		}
#else
		// The file can exist with no game in it (just Game Options) --
		// gameStarted comes from its own key.
		std::ifstream in(path());
		gameStarted = false;
		if(!in.good())
			return;

		bool sawCurrent[5] = {false, false, false, false, false};
		int legacyPreset = -1, speedPct = -1;
		bool sawBuyIns[5] = {false, false, false, false, false};

		std::string line;
		while(std::getline(in, line)){
			size_t eq = line.find('=');
			if(eq == std::string::npos)
				continue;

			std::string key = line.substr(0, eq);
			int value = 0;
			try{ value = std::stoi(line.substr(eq + 1)); } catch(...){ continue; }

			// Seat keys end in a single digit 0-4.
			int seat = -1;
			std::string base = key;
			if(!key.empty() && key.back() >= '0' && key.back() <= '4'){
				seat = key.back() - '0';
				base = key.substr(0, key.size() - 1);
			}

			if(key == "gameStarted") gameStarted = value != 0;
			else if(key == "gameModeIndex") gameModeIndex = value;
			else if(key == "numberOfPlayers") numberOfPlayers = value;
			else if(key == "hasProgress") hasProgress = value != 0;
			else if(key == "hasOptions") hasOptions = value != 0;
			else if(key == "dealerSpeedPct") speedPct = value;
			else if(key == "dealerSpeed") legacyPreset = value;
			else if(key == "faceDownDoubles") faceDownDoubles = value != 0;
			else if(key == "hideInactiveHands") hideInactiveHands = value != 0;
			else if(key == "tutorialSeen") tutorialSeen = value != 0;
			else if(key == "soundEffects") soundEffects = value != 0;
			else if(seat >= 0 && base == "bankroll") bankrolls[seat] = value;
			else if(seat >= 0 && base == "bet") initialBets[seat] = value;
			else if(seat >= 0 && base == "sideBet") sideBetSizes[seat] = value;
			else if(seat >= 0 && base == "current"){ currentBankrolls[seat] = value; sawCurrent[seat] = true; }
			else if(seat >= 0 && base == "buyIns"){ totalBuyIns[seat] = value; sawBuyIns[seat] = true; }
		}

		for(int i = 0; i < 5; i++){
			if(!sawCurrent[i]) currentBankrolls[i] = bankrolls[i];
			if(!sawBuyIns[i]) totalBuyIns[i] = bankrolls[i];
		}
		dealerSpeed = speedPct >= 0 ? speedPct : speedFromPreset(legacyPreset);
#endif
		if(numberOfPlayers < 1 || numberOfPlayers > 5)
			numberOfPlayers = 1;
		if(dealerSpeed < 0 || dealerSpeed > 100)
			dealerSpeed = DEFAULT_DEALER_SPEED;
	}

	// Called once SetupMenu's GO is confirmed -- persists exactly what the
	// player configured (game mode from GameModeMenu, players/bankrolls/
	// side-bet size from SetupMenu) so Resume can reconstruct it on a
	// future launch. Starts a fresh game, so any old progress is dropped.
	void saveGameConfig(int modeIndex, int players, const int bankrollValues[5], const int betValues[5], const int sideBetValues[5]){
		gameStarted = true;
		gameModeIndex = modeIndex;
		numberOfPlayers = players;
		hasProgress = false;
		for(int i = 0; i < 5; i++){
			bankrolls[i] = bankrollValues[i];
			initialBets[i] = betValues[i];
			sideBetSizes[i] = sideBetValues[i];
			currentBankrolls[i] = bankrollValues[i];
			totalBuyIns[i] = bankrollValues[i];
		}
		write();
	}

	// Every betting phase, once the previous round's payouts have landed:
	// each seat's live bankroll, everything they've bought in with, and
	// the bets/side bets currently set.
	void saveProgress(const int current[5], const int buyIns[5], const int bets[5], const int sideBets[5]){
		if(!gameStarted)
			return;
		hasProgress = true;
		for(int i = 0; i < 5; i++){
			currentBankrolls[i] = current[i];
			totalBuyIns[i] = buyIns[i];
			initialBets[i] = bets[i];
			sideBetSizes[i] = sideBets[i];
		}
		write();
	}

	// Whenever Game Options are applied (pre-game or from the pause menu).
	// Kept even with no game saved, so the next new game starts from the
	// player's last choices too.
	void saveOptions(int speed, bool faceDown, bool hideHands, bool effects){
		hasOptions = true;
		dealerSpeed = speed;
		faceDownDoubles = faceDown;
		hideInactiveHands = hideHands;
		soundEffects = effects;
		write();
	}

	// The old SLOW/NORMAL/FAST presets as slider positions -- the same
	// speeds they gave (see GameOptionsMenu::dealerSpeedFactor()).
	static int speedFromPreset(int preset){
		switch(preset){
			case 0: return 6;
			case 2: return 85;
			default: return DEFAULT_DEALER_SPEED;
		}
	}

	void saveTutorialSeen(){
		tutorialSeen = true;
		write();
	}

	// Wipes the game (Restart) so a future launch that never reaches
	// SetupMenu's GO (app closed mid-setup) starts from Start again,
	// instead of silently resuming the pre-restart game. Game Options are
	// kept -- they're preferences, not part of the game being abandoned.
	void clear(){
		gameStarted = false;
		hasProgress = false;
		write();
	}

private:
	void write(){
#ifdef __EMSCRIPTEN__
		EM_ASM({
			if($0) localStorage.setItem('underwayBlackjackSave', '1');
			else localStorage.removeItem('underwayBlackjackSave');
			localStorage.setItem('underwayBlackjackMode', $1);
			localStorage.setItem('underwayBlackjackPlayers', $2);
			localStorage.setItem('underwayBlackjackHasProgress', $3);
			localStorage.setItem('underwayBlackjackHasOptions', $4);
			localStorage.setItem('underwayBlackjackDealerSpeedPct', $5);
			localStorage.setItem('underwayBlackjackFaceDownDoubles', $6);
			localStorage.setItem('underwayBlackjackHideHands', $7);
			localStorage.setItem('underwayBlackjackTutorialSeen', $8);
			localStorage.setItem('underwayBlackjackSoundEffects', $9);
		}, gameStarted ? 1 : 0, gameModeIndex, numberOfPlayers, hasProgress ? 1 : 0,
		   hasOptions ? 1 : 0, dealerSpeed, faceDownDoubles ? 1 : 0, hideInactiveHands ? 1 : 0,
		   tutorialSeen ? 1 : 0, soundEffects ? 1 : 0);
		for(int i = 0; i < 5; i++){
			EM_ASM({
				localStorage.setItem('underwayBlackjackBankroll' + $0, $1);
				localStorage.setItem('underwayBlackjackBet' + $0, $2);
				localStorage.setItem('underwayBlackjackSideBet' + $0, $3);
				localStorage.setItem('underwayBlackjackCurrent' + $0, $4);
				localStorage.setItem('underwayBlackjackBuyIns' + $0, $5);
			}, i, bankrolls[i], initialBets[i], sideBetSizes[i], currentBankrolls[i], totalBuyIns[i]);
		}
#else
		std::ofstream out(path());
		if(gameStarted)
			out << "gameStarted=1\n";
		out << "gameModeIndex=" << gameModeIndex << "\n";
		out << "numberOfPlayers=" << numberOfPlayers << "\n";
		out << "hasProgress=" << (hasProgress ? 1 : 0) << "\n";
		out << "hasOptions=" << (hasOptions ? 1 : 0) << "\n";
		out << "dealerSpeedPct=" << dealerSpeed << "\n";
		out << "faceDownDoubles=" << (faceDownDoubles ? 1 : 0) << "\n";
		out << "hideInactiveHands=" << (hideInactiveHands ? 1 : 0) << "\n";
		out << "tutorialSeen=" << (tutorialSeen ? 1 : 0) << "\n";
		out << "soundEffects=" << (soundEffects ? 1 : 0) << "\n";
		for(int i = 0; i < 5; i++){
			out << "bankroll" << i << "=" << bankrolls[i] << "\n";
			out << "bet" << i << "=" << initialBets[i] << "\n";
			out << "sideBet" << i << "=" << sideBetSizes[i] << "\n";
			out << "current" << i << "=" << currentBankrolls[i] << "\n";
			out << "buyIns" << i << "=" << totalBuyIns[i] << "\n";
		}
#endif
	}

#ifdef __EMSCRIPTEN__
	// Game-wide values by index (see write() for the key order); missing
	// keys fall back to def. The key lists are single '|'-separated strings
	// because EM_ASM is a macro: a bare comma in the JS (like in an array
	// literal) would split its argument and break the build.
	static int webGet(int which, int def){
		return EM_ASM_INT({
			var keys = 'underwayBlackjackMode|underwayBlackjackPlayers|underwayBlackjackHasProgress|underwayBlackjackHasOptions|underwayBlackjackDealerSpeed|underwayBlackjackFaceDownDoubles|underwayBlackjackHideHands|underwayBlackjackTutorialSeen|underwayBlackjackDealerSpeedPct|underwayBlackjackSoundEffects'.split('|');
			var v = localStorage.getItem(keys[$0]);
			return v === null ? $1 : parseInt(v);
		}, which, def);
	}

	// Per-seat values: 0 bankroll, 1 bet, 2 sideBet, 3 current, 4 buyIns.
	static int webGetSeat(int which, int seat, int def){
		return EM_ASM_INT({
			var keys = 'underwayBlackjackBankroll|underwayBlackjackBet|underwayBlackjackSideBet|underwayBlackjackCurrent|underwayBlackjackBuyIns'.split('|');
			var v = localStorage.getItem(keys[$0] + $1);
			return v === null ? $2 : parseInt(v);
		}, which, seat, def);
	}
#else
	std::string path(){
		char* pref = SDL_GetPrefPath("UnderwayBlackjack", "Save");
		std::string p = pref ? std::string(pref) + "save.txt" : "save.txt";
		if(pref)
			SDL_free(pref);
		return p;
	}
#endif
};
