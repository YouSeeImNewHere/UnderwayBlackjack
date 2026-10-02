#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <deque>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include "StrategyChart.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

// Card counting's index plays: the Illustrious 18 and Fab 4 (Hi-Lo, true
// count), the deviations from basic strategy worth knowing. Standard
// blackjack only. Each returns the move at this true count, or 0 when
// the chart's own move stands. chartMove is the chart's move for the hand
// as things are (so a chart surrender isn't overridden by a stand index).
// why, when given, says which index applied.
namespace IndexPlays{
	// TC as counters use it: rounded down (+2.9 plays as +2, -0.5 as -1).
	inline int floorCount(float trueCount){ return (int)std::floor(trueCount + 1e-6f); }

	inline std::string signedNum(int v){ return v > 0 ? "+" + std::to_string(v) : std::to_string(v); }

	// col: 0..9 for dealer 2..10, A. hardTotal: the hand's hard total
	// (ignored for a pair of 10s). h17: the dealer hits soft 17.
	inline char move(bool pairOfTens, bool soft, int hardTotal, int col, int tc, bool h17,
			bool canSurrender, bool canDouble, char chartMove, std::string* why = nullptr){
		auto index = [&](char m, int idx, const char* note){
			if(why) *why = std::string(note) + " AT " + signedNum(idx) + " OR MORE";
			return m;
		};
		if(pairOfTens){
			if(col == 3 && tc >= 5) return index('P', 5, "SPLIT 10S VS 5");
			if(col == 4 && tc >= 4) return index('P', 4, "SPLIT 10S VS 6");
			return 0;
		}
		if(soft)
			return 0;
		switch(hardTotal){
			case 16:
				if(chartMove == 'R') return 0;
				if(col == 8) return tc >= 0 ? index('S', 0, "STAND 16 VS 10") : 'H';
				if(col == 7) return tc >= 5 ? index('S', 5, "STAND 16 VS 9") : 'H';
				return 0;
			case 15:
				if(col == 8){
					if(canSurrender) return tc >= 0 ? index('R', 0, "SURRENDER 15 VS 10") : 'H';
					return tc >= 4 ? index('S', 4, "STAND 15 VS 10") : 'H';
				}
				if(col == 7 && canSurrender && tc >= 2) return index('R', 2, "SURRENDER 15 VS 9");
				if(col == 9 && canSurrender){
					int idx = h17 ? -1 : 1;
					return tc >= idx ? index('R', idx, "SURRENDER 15 VS A") : 'H';
				}
				return 0;
			case 14:
				if(col == 8 && canSurrender && tc >= 3) return index('R', 3, "SURRENDER 14 VS 10");
				return 0;
			case 13:
				if(col == 0) return tc >= -1 ? 'S' : 'H';
				if(col == 1) return tc >= -2 ? 'S' : 'H';
				return 0;
			case 12:
				if(col == 0) return tc >= 3 ? index('S', 3, "STAND 12 VS 2") : 'H';
				if(col == 1) return tc >= 2 ? index('S', 2, "STAND 12 VS 3") : 'H';
				if(col == 2) return tc >= 0 ? 'S' : 'H';
				if(col == 3) return tc >= -2 ? 'S' : 'H';
				if(col == 4) return tc >= -1 ? 'S' : 'H';
				return 0;
			case 11:
				if(col == 9 && canDouble){
					int idx = h17 ? -1 : 1;
					return tc >= idx ? index('D', idx, "DOUBLE 11 VS A") : 'H';
				}
				return 0;
			case 10:
				if(col == 8 && canDouble && tc >= 4) return index('D', 4, "DOUBLE 10 VS 10");
				if(col == 9 && canDouble){
					int idx = h17 ? 3 : 4;
					if(tc >= idx) return index('D', idx, "DOUBLE 10 VS A");
				}
				return 0;
			case 9:
				if(col == 0 && canDouble) return tc >= 1 ? index('D', 1, "DOUBLE 9 VS 2") : 'H';
				if(col == 5 && canDouble && tc >= 3) return index('D', 3, "DOUBLE 9 VS 7");
				return 0;
		}
		return 0;
	}

	// The 'below the index' half of a stand/hit index that agrees with
	// the chart isn't a deviation; only report one when the move differs.
	inline bool isDeviation(char indexMove, char chartMove){
		return indexMove != 0 && indexMove != chartMove;
	}

	// Insurance pays at +3 or more.
	constexpr int INSURANCE_INDEX = 3;

	// A simple 1-8 betting ramp: 1 unit at +1 or less, then (TC - 1).
	inline int betUnits(int tc){ return std::clamp(tc - 1, 1, 8); }
}

// The TRAINING screens' memory: how often each chart cell has been played
// right (drills and real hands alike, per chart), the most recent
// mistakes, and the counting drills' results. Saved like Stats, separate
// from it so RESET there doesn't wipe this.
struct Trainer
{
	static constexpr int ROWS = 28; // 10 hard + 8 soft + 10 pairs
	static constexpr int MAX_MISTAKES = 40;
	static constexpr int NO_COUNT = -99;

	static int cellRow(int section, int row){ return section == 0 ? row : section == 1 ? 10 + row : 18 + row; }

	struct Mistake{
		int set = 0, section = 0, row = 0, col = 0, cards = 2;
		char chosen = 'H', correct = 'S';
		int trueCount = NO_COUNT;
		bool fromDrill = true;
	};

	int attempts[StrategyChart::SET_COUNT][ROWS][10] = {};
	int correct[StrategyChart::SET_COUNT][ROWS][10] = {};
	std::deque<Mistake> mistakes; // newest first

	// Counting drills.
	int speedDrills = 0, speedRight = 0;
	int bestDeckSeconds = 0;  // fastest correct full deck, 0 = none yet
	int trueCountQuestions = 0, trueCountRight = 0;
	int deckEstimates = 0, deckEstimatesRight = 0;

	void record(int set, int section, int row, int col, bool ok, char chosen, char correctMove, int cards, int trueCount, bool fromDrill){
		int r = cellRow(section, row);
		attempts[set][r][col]++;
		if(ok){
			correct[set][r][col]++;
			return;
		}
		mistakes.push_front(Mistake{ set, section, row, col, cards, chosen, correctMove, trueCount, fromDrill });
		while(mistakes.size() > MAX_MISTAKES)
			mistakes.pop_back();
	}

	// How well a cell is known: -1 never played, else 0..1.
	float accuracy(int set, int section, int row, int col) const{
		int r = cellRow(section, row);
		int a = attempts[set][r][col];
		return a == 0 ? -1.0f : (float)correct[set][r][col] / a;
	}

	int totalAttempts(int set) const{
		int n = 0;
		for(int r = 0; r < ROWS; r++)
			for(int c = 0; c < 10; c++)
				n += attempts[set][r][c];
		return n;
	}
	int totalCorrect(int set) const{
		int n = 0;
		for(int r = 0; r < ROWS; r++)
			for(int c = 0; c < 10; c++)
				n += correct[set][r][c];
		return n;
	}

	// How likely a drill is to pick a cell: unseen and missed cells come
	// up far more than ones already known (a simple spaced repetition).
	float drillWeight(int set, int section, int row, int col) const{
		int r = cellRow(section, row);
		int a = attempts[set][r][col];
		if(a == 0)
			return 3.0f;
		float missRate = 1.0f - (float)correct[set][r][col] / a;
		// Recent mistakes on this cell weigh in heavily.
		int recent = 0;
		for(size_t i = 0; i < mistakes.size() && i < 15; i++){
			const Mistake& m = mistakes[i];
			if(m.set == set && m.section == section && m.row == row && m.col == col)
				recent++;
		}
		return 0.4f + 6.0f * missRate + 4.0f * recent + (a < 3 ? 1.0f : 0.0f);
	}

	void reset(){
		*this = Trainer();
	}

	// ---- saving: one flat list of numbers ----
	std::vector<int> toInts() const{
		std::vector<int> v{ 1 /* version */ };
		for(int s = 0; s < StrategyChart::SET_COUNT; s++)
			for(int r = 0; r < ROWS; r++)
				for(int c = 0; c < 10; c++){
					v.push_back(attempts[s][r][c]);
					v.push_back(correct[s][r][c]);
				}
		v.push_back((int)mistakes.size());
		for(const Mistake& m : mistakes){
			v.insert(v.end(), { m.set, m.section, m.row, m.col, m.cards, m.chosen, m.correct, m.trueCount, m.fromDrill ? 1 : 0 });
		}
		v.insert(v.end(), { speedDrills, speedRight, bestDeckSeconds, trueCountQuestions, trueCountRight, deckEstimates, deckEstimatesRight });
		return v;
	}

	void fromInts(const std::vector<int>& v){
		size_t i = 0;
		auto next = [&](){ return i < v.size() ? v[i++] : 0; };
		if(next() != 1)
			return;
		for(int s = 0; s < StrategyChart::SET_COUNT; s++)
			for(int r = 0; r < ROWS; r++)
				for(int c = 0; c < 10; c++){
					attempts[s][r][c] = next();
					correct[s][r][c] = next();
				}
		int n = next();
		mistakes.clear();
		for(int k = 0; k < n && k < MAX_MISTAKES; k++){
			Mistake m;
			m.set = std::clamp(next(), 0, StrategyChart::SET_COUNT - 1);
			m.section = std::clamp(next(), 0, 2);
			m.row = std::clamp(next(), 0, 9);
			m.col = std::clamp(next(), 0, 9);
			m.cards = next();
			m.chosen = (char)next();
			m.correct = (char)next();
			m.trueCount = next();
			m.fromDrill = next() != 0;
			if(m.section == 1)
				m.row = std::min(m.row, 7);
			mistakes.push_back(m);
		}
		speedDrills = next(); speedRight = next(); bestDeckSeconds = next();
		trueCountQuestions = next(); trueCountRight = next();
		deckEstimates = next(); deckEstimatesRight = next();
	}

	void load(){
		std::vector<int> v;
#ifdef __EMSCRIPTEN__
		int n = EM_ASM_INT({
			var s = localStorage.getItem('underwayBlackjackTrainer');
			Module.blackjackTrainer = s ? s.split(',') : [];
			return Module.blackjackTrainer.length;
		});
		for(int k = 0; k < n; k++)
			v.push_back(EM_ASM_INT({ return parseInt(Module.blackjackTrainer[$0]) || 0; }, k));
#else
		std::ifstream in(path());
		int x;
		while(in >> x)
			v.push_back(x);
#endif
		if(!v.empty())
			fromInts(v);
	}

	void save() const{
		std::vector<int> v = toInts();
#ifdef __EMSCRIPTEN__
		// Built up in JS a number at a time (no string passing needed).
		EM_ASM({ Module.blackjackTrainer = []; });
		for(int x : v)
			EM_ASM({ Module.blackjackTrainer.push($0); }, x);
		EM_ASM({ localStorage.setItem('underwayBlackjackTrainer', Module.blackjackTrainer.join(',')); });
#else
		std::ofstream out(path());
		for(int x : v)
			out << x << ' ';
#endif
	}

private:
#ifndef __EMSCRIPTEN__
	static std::string path(){
		char* pref = SDL_GetPrefPath("UnderwayBlackjack", "Save");
		std::string p = pref ? std::string(pref) + "trainer.txt" : "trainer.txt";
		if(pref)
			SDL_free(pref);
		return p;
	}
#endif
};
