#pragma once
#include "Game.h"
#include "Card.h"
#include "DigitFont.h"
#include "StrategyChart.h"
#include "ChartsMenu.h"
#include "Trainer.h"
#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <array>

// The home page's TRAINING screens -- practice for a real casino, away
// from the full game:
//   TrainingMenu    the hub
//   DrillMenu       strategy flash cards, missed hands coming back more
//   ReviewMenu      the chart coloured by how well each cell is known,
//                   and the most recent mistakes
//   CountDrillMenu  Hi-Lo running count against the clock
//   TrueCountMenu   estimating decks left from the discard tray, and
//                   turning a running count into a true count
//   BankrollMenu    what a session costs and how much money to bring
// Each has draw()/handlePoint() (true = BACK) like every other menu; the
// drills also take keys (handleKey) and time (update).
namespace TrainingUI{
	inline const SDL_Color WHITE{255, 255, 255, 255};
	inline const SDL_Color GOLD{230, 210, 140, 255};
	inline const SDL_Color DIM{160, 180, 160, 255};
	inline const SDL_Color GREEN{110, 230, 110, 255};
	inline const SDL_Color RED{245, 110, 90, 255};
	inline const SDL_Color BG{10, 30, 15, 255};

	inline void clear(SDLState& state){
		SDL_SetRenderDrawColor(state.renderer, BG.r, BG.g, BG.b, BG.a);
		SDL_RenderFillRect(state.renderer, nullptr);
	}

	// Centered on x (default: the screen), shrunk to fit maxW.
	inline void text(SDLState& state, const std::string& s, float y, float pixel, SDL_Color color, float cx = 720.0f, float maxW = 1400.0f){
		float w = DigitFont::textWidth(s, pixel);
		if(w > maxW && w > 0.0f){
			pixel *= maxW / w;
			w = DigitFont::textWidth(s, pixel);
		}
		DigitFont::drawText(state, s, cx - w / 2.0f, y, pixel, color);
	}

	inline void button(SDLState& state, const SDL_FRect& r, SDL_Color c, const std::string& label, float pixel = 5.0f){
		StrategyChart::drawButton(state, r, c, label, pixel);
	}

	inline bool hit(SDL_FPoint p, const SDL_FRect& r){ return SDL_PointInRectFloat(&p, &r); }

	inline bool toLogical(SDLState& state, float wx, float wy, SDL_FPoint& p){
		return SDL_RenderCoordinatesFromWindow(state.renderer, wx, wy, &p.x, &p.y);
	}

	inline const char* moveName(char m){
		switch(m){
			case 'H': return "HIT";
			case 'S': return "STAND";
			case 'D': return "DOUBLE";
			case 'P': return "SPLIT";
			case 'R': return "SURRENDER";
			default: return "?";
		}
	}

	// "HARD 16", "SOFT 18 A-7", "PAIR OF 8S".
	inline std::string handName(int section, int row){
		if(section == 0)
			return row == 0 ? "HARD 8 OR LESS" : "HARD " + std::to_string(row + 8);
		if(section == 1)
			return "SOFT " + std::to_string(row + 13) + " A-" + std::to_string(row + 2);
		static const char* PAIRS[10] = { "2S", "3S", "4S", "5S", "6S", "7S", "8S", "9S", "10S", "ACES" };
		return std::string("PAIR OF ") + PAIRS[row];
	}

	inline std::string dealerName(int col){
		static const char* UP[10] = { "2", "3", "4", "5", "6", "7", "8", "9", "10", "ACE" };
		return UP[col];
	}

	inline std::string signedNum(int v){ return v > 0 ? "+" + std::to_string(v) : std::to_string(v); }

	// A short name for a chart, for the mistakes list.
	inline const char* setShortName(int set){
		static const char* NAMES[StrategyChart::SET_COUNT] = {
			"SHOE H17", "SHOE S17", "2 DECK H17", "2 DECK S17", "1 DECK H17", "1 DECK S17",
			"SPANISH H17", "SPANISH S17", "FREE BET"
		};
		return NAMES[set];
	}

	// Hi-Lo: 2-6 +1, 7-9 0, 10s and aces -1.
	inline int hiLo(int value){
		if(value >= 2 && value <= 6) return 1;
		if(value >= 7 && value <= 9) return 0;
		return -1;
	}

	inline void drawCard(SDLState& state, Resources& res, int suit, int value, float x, float y){
		Card c(suit, value, true, SDL_FPoint{x, y}, 0.0f);
		c.draw(state, res);
	}

	// A - value + stepper, CHECK beside it (count and true count answers).
	struct Stepper{
		SDL_FRect minus{ .x = 470, .y = 470, .w = 70, .h = 60 };
		SDL_FRect plus{ .x = 760, .y = 470, .w = 70, .h = 60 };
		SDL_FRect check{ .x = 860, .y = 470, .w = 200, .h = 60 };
		void draw(SDLState& state, const std::string& value, const std::string& checkLabel = "CHECK"){
			button(state, minus, SDL_Color{70, 70, 80, 255}, "-", 6.0f);
			button(state, plus, SDL_Color{70, 70, 80, 255}, "+", 6.0f);
			text(state, value, 480.0f, 7.0f, SDL_Color{255, 225, 80, 255}, 650.0f);
			button(state, check, SDL_Color{60, 130, 70, 255}, checkLabel, 5.0f);
		}
	};
}

// ---------------------------------------------------------------------
class TrainingMenu
{
public:
	enum Choice{ NONE, DRILL, REVIEW, COUNT_SPEED, TRUE_COUNT, BANKROLL, BACK };

	void draw(SDLState& state, const Trainer& trainer){
		using namespace TrainingUI;
		clear(state);
		text(state, "TRAINING", 30.0f, 7.0f, WHITE);
		static const char* LABELS[5] = { "STRATEGY DRILL", "MISTAKES + ACCURACY", "COUNTING SPEED", "TRUE COUNT + DECKS", "BANKROLL PLANNER" };
		static const char* NOTES[5] = {
			"FLASH CARDS. HANDS YOU MISS COME BACK MORE",
			"WHICH CHART CELLS YOU KNOW, AND YOUR LAST MISTAKES",
			"KEEP THE HI-LO RUNNING COUNT AS CARDS FLASH BY",
			"ESTIMATE DECKS LEFT, TURN RUNNING COUNT INTO TRUE COUNT",
			"WHAT A SESSION COSTS, AND HOW MUCH MONEY TO BRING"
		};
		static const SDL_Color COLORS[5] = { {60, 130, 70, 255}, {150, 70, 70, 255}, {60, 90, 150, 255}, {50, 120, 130, 255}, {150, 120, 40, 255} };
		for(int i = 0; i < 5; i++){
			button(state, buttonRect(i), COLORS[i], LABELS[i], 4.5f);
			float np = std::min(3.0f, 780.0f / DigitFont::textWidth(NOTES[i], 1.0f));
			DigitFont::drawText(state, NOTES[i], 640.0f, buttonRect(i).y + 35.0f - 2.5f * np, np, DIM);
		}
		int drills = 0, right = 0;
		for(int s = 0; s < StrategyChart::SET_COUNT; s++){
			drills += trainer.totalAttempts(s);
			right += trainer.totalCorrect(s);
		}
		std::string summary = drills > 0
			? std::to_string(drills) + " STRATEGY DECISIONS SO FAR, " + std::to_string((right * 100 + drills / 2) / drills) + "% RIGHT"
			: "EVERY DECISION IN DRILLS AND REAL HANDS IS TRACKED HERE";
		text(state, summary, 560.0f, 3.4f, GOLD);
		button(state, backButton, SDL_Color{80, 80, 80, 255}, "BACK", 6.0f);
	}

	Choice handlePoint(SDLState& state, float wx, float wy){
		SDL_FPoint p;
		if(!TrainingUI::toLogical(state, wx, wy, p))
			return NONE;
		for(int i = 0; i < 5; i++)
			if(TrainingUI::hit(p, buttonRect(i)))
				return (Choice)(DRILL + i);
		if(TrainingUI::hit(p, backButton))
			return BACK;
		return NONE;
	}

	std::vector<SDL_FRect> focusRects(){
		std::vector<SDL_FRect> r;
		for(int i = 0; i < 5; i++)
			r.push_back(buttonRect(i));
		r.push_back(backButton);
		return r;
	}

private:
	SDL_FRect buttonRect(int i){ return SDL_FRect{ .x = 160, .y = 110.0f + i * 86.0f, .w = 450, .h = 70 }; }
	SDL_FRect backButton{ .x = 620, .y = 620, .w = 200, .h = 56 };
};

// ---------------------------------------------------------------------
class DrillMenu
{
public:
	bool dirty = false; // the trainer has new results to save

	void open(GameMode mode, bool dealerHitsSoft17, bool indexPlays, Trainer& t){
		trainer = &t;
		selector.open(mode, dealerHitsSoft17);
		selector.indexPlays = indexPlays;
		streak = 0;
		sessionRight = sessionTotal = 0;
		newQuestion();
	}

	void update(float dt){
		if(phase == CORRECT){
			timer -= dt;
			if(timer <= 0.0f)
				newQuestion();
		}
	}

	void draw(SDLState& state, Resources& res){
		using namespace TrainingUI;
		clear(state);
		StrategyChart::Rules rules = selector.rules();
		text(state, "STRATEGY DRILL", 16.0f, 6.0f, WHITE);
		text(state, StrategyChart::titleFor(rules), 58.0f, 3.0f, DIM);

		DigitFont::drawText(state, "DEALER", 650.0f - DigitFont::textWidth("DEALER", 4.5f), 150.0f, 4.5f, GOLD);
		drawCard(state, res, dealerSuit, dealerValue, 670.0f, 85.0f);
		if(counting)
			DigitFont::drawText(state, "TRUE COUNT " + signedNum(trueCount), 800.0f, 145.0f, 5.0f, SDL_Color{255, 225, 80, 255});

		DigitFont::drawText(state, "YOU", 595.0f - DigitFont::textWidth("YOU", 4.5f), 315.0f, 4.5f, GOLD);
		for(size_t i = 0; i < playerCards.size(); i++)
			drawCard(state, res, playerCards[i].first, playerCards[i].second, 615.0f + i * 110.0f, 245.0f);

		if(phase == CORRECT){
			text(state, "CORRECT - " + std::string(moveName(answer)), 400.0f, 6.0f, GREEN);
			if(!answerWhy.empty())
				text(state, answerWhy, 445.0f, 3.2f, GOLD);
		} else if(phase == WRONG){
			text(state, std::string("NO - THE PLAY IS ") + moveName(answer), 400.0f, 6.0f, RED);
			std::string detail = handName(section, row) + " VS " + dealerName(col);
			if(!answerWhy.empty())
				detail += " - " + answerWhy;
			text(state, detail + "  -  TAP OR ENTER FOR NEXT", 445.0f, 3.2f, GOLD);
		}

		for(int i = 0; i < 5; i++){
			bool enabled = moveEnabled(MOVES[i]);
			SDL_Color c = !enabled ? SDL_Color{45, 50, 48, 255}
				: (phase != ASKING && MOVES[i] == answer) ? SDL_Color{60, 150, 70, 255}
				: (phase == WRONG && MOVES[i] == chosen) ? SDL_Color{160, 60, 60, 255}
				: StrategyChart::colorFor(MOVES[i]);
			button(state, moveRect(i), c, moveName(MOVES[i]), 5.0f);
		}

		int known = trainer ? trainer->totalAttempts(rules.set) : 0;
		int knownRight = trainer ? trainer->totalCorrect(rules.set) : 0;
		std::string score = "STREAK " + std::to_string(streak) + "   BEST " + std::to_string(best)
			+ "   THIS SESSION " + std::to_string(sessionRight) + "/" + std::to_string(sessionTotal);
		if(known > 0)
			score += "   THIS CHART " + std::to_string((knownRight * 100 + known / 2) / known) + "% OF " + std::to_string(known);
		text(state, score, 550.0f, 3.2f, DIM);

		selector.draw(state, RULES_Y, true);
		button(state, backButton, SDL_Color{80, 80, 80, 255}, "BACK", 6.0f);
	}

	bool handlePoint(SDLState& state, float wx, float wy){
		SDL_FPoint p;
		if(!TrainingUI::toLogical(state, wx, wy, p))
			return false;
		if(TrainingUI::hit(p, backButton))
			return true;
		if(selector.handle(p, RULES_Y, true)){
			newQuestion();
			return false;
		}
		if(phase == ASKING){
			for(int i = 0; i < 5; i++)
				if(TrainingUI::hit(p, moveRect(i)))
					choose(MOVES[i]);
		} else if(p.y < RULES_Y){
			newQuestion();
		}
		return false;
	}

	void handleKey(SDL_Scancode sc){
		if(phase != ASKING){
			if(sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_SPACE || sc == SDL_SCANCODE_KP_ENTER)
				newQuestion();
			return;
		}
		switch(sc){
			case SDL_SCANCODE_H: choose('H'); break;
			case SDL_SCANCODE_S: choose('S'); break;
			case SDL_SCANCODE_D: choose('D'); break;
			case SDL_SCANCODE_P: choose('P'); break;
			case SDL_SCANCODE_R: choose('R'); break;
			default: break;
		}
	}

	// Letter keys answer; arrows and Enter still walk the buttons.
	static bool takesKey(SDL_Scancode sc){
		return sc == SDL_SCANCODE_H || sc == SDL_SCANCODE_S || sc == SDL_SCANCODE_D || sc == SDL_SCANCODE_P || sc == SDL_SCANCODE_R;
	}
	bool waitingForNext() const{ return phase != ASKING; }

	std::vector<SDL_FRect> focusRects(){
		std::vector<SDL_FRect> r;
		for(int i = 0; i < 5; i++)
			r.push_back(moveRect(i));
		selector.addRects(r, RULES_Y, true);
		r.push_back(backButton);
		return r;
	}

	// For tests: the current question's answer.
	char currentAnswer() const{ return answer; }

private:
	static constexpr char MOVES[5] = { 'H', 'S', 'D', 'P', 'R' };
	static constexpr float RULES_Y = 585.0f;
	enum Phase{ ASKING, CORRECT, WRONG };

	RuleSelector selector;
	Trainer* trainer = nullptr;
	std::mt19937 rng{ std::random_device{}() };

	int section = 0, row = 0, col = 0;
	std::vector<std::pair<int, int>> playerCards; // (suit, value)
	int dealerSuit = 0, dealerValue = 10;
	bool counting = false;
	int trueCount = 0;
	char answer = 'S', chosen = 'S';
	std::string answerWhy;
	Phase phase = ASKING;
	float timer = 0.0f;
	int streak = 0, best = 0, sessionRight = 0, sessionTotal = 0;

	SDL_FRect moveRect(int i){ return SDL_FRect{ .x = 192.0f + i * 214.0f, .y = 476, .w = 200, .h = 60 }; }
	SDL_FRect backButton{ .x = 620, .y = 645, .w = 200, .h = 56 };

	bool moveEnabled(char m){
		if(m == 'P') return section == 2;
		if(m == 'R') return selector.rules().surrender;
		return true;
	}

	int rnd(int n){ return (int)(rng() % (unsigned)n); }

	// A ten-value card: 10/J/Q/K, but Spanish 21 decks have no 10s.
	int tenValue(bool spanish){ return spanish ? 11 + rnd(3) : 10 + rnd(4); }

	void newQuestion(){
		StrategyChart::Rules rules = selector.rules();
		bool spanish = StrategyChart::isSpanish(rules.set);
		counting = selector.usesIndexPlays();
		phase = ASKING;
		answerWhy.clear();

		// With INDEX PLAYS, most hands are the ones the count changes.
		static const int INDEX_CELLS[][3] = {
			{0,8,8}, {0,8,7}, {0,7,8}, {0,7,7}, {0,7,9}, {0,6,8}, {0,5,0}, {0,5,1}, {0,4,0}, {0,4,1},
			{0,4,2}, {0,4,3}, {0,4,4}, {0,3,9}, {0,2,8}, {0,2,9}, {0,1,0}, {0,1,5}, {2,8,3}, {2,8,4}
		};
		if(counting && rnd(100) < 65){
			const int* c = INDEX_CELLS[rnd((int)(sizeof(INDEX_CELLS) / sizeof(INDEX_CELLS[0])))];
			section = c[0]; row = c[1]; col = c[2];
		} else{
			// Weighted by how well each cell is known (Trainer::drillWeight).
			std::vector<float> weights;
			std::vector<std::array<int, 3>> cells;
			for(int s = 0; s < 3; s++){
				int rows = s == 1 ? 8 : 10;
				for(int r = 0; r < rows; r++)
					for(int c = 0; c < 10; c++){
						cells.push_back({s, r, c});
						weights.push_back(trainer ? trainer->drillWeight(rules.set, s, r, c) : 1.0f);
					}
			}
			std::discrete_distribution<int> pick(weights.begin(), weights.end());
			const auto& c = cells[pick(rng)];
			section = c[0]; row = c[1]; col = c[2];
		}
		trueCount = counting ? -3 + rnd(10) : Trainer::NO_COUNT;

		// The cards.
		playerCards.clear();
		auto suit = [&](){ return rnd(4); };
		auto cardFor = [&](int v){ return v == 10 ? tenValue(spanish) : v; };
		if(section == 0){
			int total = row == 0 ? (rules.set == StrategyChart::SINGLE_DECK_H17 || rules.set == StrategyChart::SINGLE_DECK_S17 ? 8 : 5 + rnd(4)) : row + 8;
			std::vector<std::pair<int, int>> combos;
			for(int a = 2; a <= 10; a++){
				int b = total - a;
				if(b > a && b <= 10)
					combos.push_back({a, b});
			}
			auto ab = combos[rnd((int)combos.size())];
			playerCards = { {suit(), cardFor(ab.first)}, {suit(), cardFor(ab.second)} };
		} else if(section == 1){
			playerCards = { {suit(), 1}, {suit(), row + 2} };
		} else{
			int v = row == 9 ? 1 : row + 2;
			playerCards = { {suit(), cardFor(v)}, {suit(), cardFor(v)} };
		}
		if(rnd(2))
			std::swap(playerCards[0], playerCards[1]);
		dealerSuit = suit();
		dealerValue = col == 9 ? 1 : cardFor(col + 2);

		// The answer: the chart's code for these rules...
		char code = StrategyChart::code(rules.set, section, row, col);
		answer = StrategyChart::resolve(code, rules.das, rules.surrender);
		// ...Spanish 21's 6-7-8 / suited 7-7-7 plays...
		char bonus = StrategyChart::bonusRule(rules.set, section, row, col);
		if(bonus != ' '){
			int a = std::min(playerCards[0].second, 10), b = std::min(playerCards[1].second, 10);
			bool sameSuit = playerCards[0].first == playerCards[1].first;
			bool in678 = a >= 6 && a <= 8 && b >= 6 && b <= 8 && a != b;
			bool hitIt = bonus == '7' ? (a == 7 && b == 7 && sameSuit)
				: in678 && (bonus == 'A' || (bonus == 'S' && sameSuit) || (bonus == 'K' && sameSuit && playerCards[0].first == 0));
			if(hitIt){
				answer = 'H';
				answerWhy = bonus == '7' ? "HIT FOR THE SUITED 7-7-7 BONUS" : "HIT - A 6-7-8 BONUS IS POSSIBLE";
			}
		}
		// ...and the count's index plays.
		if(counting){
			int hardTotal = 0;
			for(auto& c : playerCards)
				hardTotal += std::min(c.second, 10);
			std::string note;
			char m = IndexPlays::move(section == 2 && row == 8, section == 1, hardTotal, col, trueCount,
				selector.dealerHitsSoft17(), rules.surrender, true, answer, &note);
			if(IndexPlays::isDeviation(m, answer)){
				answer = m;
				answerWhy = "INDEX: " + note;
			}
		}
		if(!moveEnabled(answer))
			answer = 'H';
	}

	void choose(char m){
		if(!moveEnabled(m))
			return;
		chosen = m;
		bool ok = m == answer;
		sessionTotal++;
		if(ok){
			sessionRight++;
			streak++;
			best = std::max(best, streak);
			phase = CORRECT;
			timer = answerWhy.empty() ? 0.6f : 1.6f;
		} else{
			streak = 0;
			phase = WRONG;
		}
		if(trainer){
			trainer->record(selector.rules().set, section, row, col, ok, m, answer, 2, counting ? trueCount : Trainer::NO_COUNT, true);
			dirty = true;
		}
	}
};

// ---------------------------------------------------------------------
class ReviewMenu
{
public:
	void open(GameMode mode, bool dealerHitsSoft17, const Trainer& t){
		trainer = &t;
		selector.open(mode, dealerHitsSoft17);
		showingMistakes = false;
	}

	void draw(SDLState& state){
		using namespace TrainingUI;
		StrategyChart::Rules rules = selector.rules();
		if(showingMistakes)
			drawMistakes(state);
		else{
			StrategyChart::Tint tint = [&](int s, int r, int c, SDL_Color& color){
				float a = trainer->accuracy(rules.set, s, r, c);
				color = a < 0.0f ? SDL_Color{40, 46, 44, 255}
					: a >= 0.9f ? SDL_Color{50, 140, 70, 255}
					: a >= 0.7f ? SDL_Color{170, 140, 40, 255}
					: SDL_Color{165, 50, 50, 255};
				return true;
			};
			StrategyChart::drawChart(state, rules, false, 0, 0, 0, true, &tint);
			drawAccuracyNotes(state, rules);
		}
		selector.draw(state, RULES_Y, false);
		button(state, backButton, SDL_Color{80, 80, 80, 255}, "BACK", 6.0f);
		button(state, pageButton, SDL_Color{60, 90, 150, 255}, showingMistakes ? "ACCURACY" : "MISTAKES", 5.0f);
	}

	bool handlePoint(SDLState& state, float wx, float wy){
		SDL_FPoint p;
		if(!TrainingUI::toLogical(state, wx, wy, p))
			return false;
		if(TrainingUI::hit(p, backButton))
			return true;
		if(TrainingUI::hit(p, pageButton))
			showingMistakes = !showingMistakes;
		selector.handle(p, RULES_Y, false);
		return false;
	}

	std::vector<SDL_FRect> focusRects(){
		std::vector<SDL_FRect> r;
		selector.addRects(r, RULES_Y, false);
		r.push_back(backButton);
		r.push_back(pageButton);
		return r;
	}

private:
	static constexpr float RULES_Y = 585.0f;
	RuleSelector selector;
	const Trainer* trainer = nullptr;
	bool showingMistakes = false;
	SDL_FRect backButton{ .x = 470, .y = 645, .w = 200, .h = 56 };
	SDL_FRect pageButton{ .x = 690, .y = 645, .w = 280, .h = 56 };

	void drawAccuracyNotes(SDLState& state, const StrategyChart::Rules& rules){
		using namespace TrainingUI;
		// Legend.
		struct Key{ SDL_Color c; const char* label; };
		static const Key KEYS[4] = {
			{ {50, 140, 70, 255}, "90%+" }, { {170, 140, 40, 255}, "70-89%" },
			{ {165, 50, 50, 255}, "UNDER 70%" }, { {40, 46, 44, 255}, "NOT PLAYED YET" }
		};
		float x = 300.0f, y = 512.0f;
		for(const Key& k : KEYS){
			SDL_FRect sw{ .x = x, .y = y, .w = 30, .h = 24 };
			SDL_SetRenderDrawColor(state.renderer, k.c.r, k.c.g, k.c.b, 255);
			SDL_RenderFillRect(state.renderer, &sw);
			SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
			SDL_RenderRect(state.renderer, &sw);
			DigitFont::drawText(state, k.label, x + 40.0f, y + 4.0f, 3.2f, WHITE);
			x += 70.0f + DigitFont::textWidth(k.label, 3.2f) + 30.0f;
		}

		int n = trainer->totalAttempts(rules.set), right = trainer->totalCorrect(rules.set);
		std::string line = n == 0 ? "NO DECISIONS ON THIS CHART YET - PLAY OR DRILL IT"
			: "THIS CHART: " + std::to_string((right * 100 + n / 2) / n) + "% RIGHT OVER " + std::to_string(n) + " DECISIONS";
		// The 3 weakest cells played at least 3 times.
		struct Weak{ float a; int s, r, c; };
		std::vector<Weak> weak;
		for(int s = 0; s < 3; s++){
			int rows = s == 1 ? 8 : 10;
			for(int r = 0; r < rows; r++)
				for(int c = 0; c < 10; c++){
					int row = Trainer::cellRow(s, r);
					if(trainer->attempts[rules.set][row][c] >= 3 && trainer->accuracy(rules.set, s, r, c) < 0.9f)
						weak.push_back({ trainer->accuracy(rules.set, s, r, c), s, r, c });
				}
		}
		std::sort(weak.begin(), weak.end(), [](const Weak& a, const Weak& b){ return a.a < b.a; });
		if(!weak.empty()){
			line += "   WEAKEST:";
			for(size_t i = 0; i < weak.size() && i < 3; i++)
				line += " " + handName(weak[i].s, weak[i].r) + " VS " + dealerName(weak[i].c) + (i + 1 < std::min<size_t>(3, weak.size()) ? "," : "");
		}
		text(state, line, 550.0f, 3.0f, GOLD);
	}

	void drawMistakes(SDLState& state){
		using namespace TrainingUI;
		clear(state);
		text(state, "RECENT MISTAKES", 20.0f, 6.0f, WHITE);
		if(trainer->mistakes.empty()){
			text(state, "NONE YET - EVERY MISSED DECISION, IN DRILLS OR REAL HANDS, SHOWS UP HERE", 300.0f, 3.6f, GOLD);
			return;
		}
		static const float X[6] = { 40.0f, 520.0f, 660.0f, 880.0f, 1100.0f, 1230.0f };
		static const char* HEAD[6] = { "YOUR HAND", "VS", "YOU PLAYED", "THE PLAY", "COUNT", "CHART" };
		for(int i = 0; i < 6; i++)
			DigitFont::drawText(state, HEAD[i], X[i], 80.0f, 3.2f, GOLD);
		float y = 112.0f;
		for(size_t i = 0; i < trainer->mistakes.size() && i < 13; i++){
			const Trainer::Mistake& m = trainer->mistakes[i];
			std::string hand = handName(m.section, m.row);
			if(m.cards > 2)
				hand += " - " + std::to_string(m.cards) + " CARDS";
			if(!m.fromDrill)
				hand += " - GAME";
			DigitFont::drawText(state, hand, X[0], y, 3.0f, WHITE);
			DigitFont::drawText(state, dealerName(m.col), X[1], y, 3.0f, WHITE);
			DigitFont::drawText(state, moveName(m.chosen), X[2], y, 3.0f, RED);
			DigitFont::drawText(state, moveName(m.correct), X[3], y, 3.0f, GREEN);
			DigitFont::drawText(state, m.trueCount == Trainer::NO_COUNT ? "-" : signedNum(m.trueCount), X[4], y, 3.0f, WHITE);
			DigitFont::drawText(state, setShortName(m.set), X[5], y, 2.6f, DIM);
			y += 34.0f;
		}
	}
};

// ---------------------------------------------------------------------
class CountDrillMenu
{
public:
	bool dirty = false;

	void open(Trainer& t){
		trainer = &t;
		phase = SETUP;
	}

	void update(float dt){
		if(phase != RUNNING)
			return;
		timer += dt;
		if(timer >= interval()){
			timer = 0.0f;
			shown++;
			if(shown >= cardCount()){
				phase = ANSWER;
				guess = 0;
			}
		}
	}

	void draw(SDLState& state, Resources& res){
		using namespace TrainingUI;
		clear(state);
		text(state, "COUNTING SPEED", 20.0f, 6.0f, WHITE);
		text(state, "HI-LO: 2-6 COUNT +1    7-9 COUNT 0    10S AND ACES COUNT -1", 70.0f, 3.4f, GOLD);

		if(phase == SETUP || phase == RESULT){
			button(state, cardsButton, SDL_Color{60, 90, 150, 255}, "CARDS: " + std::to_string(cardCount()), 4.5f);
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.2f SEC PER CARD", interval());
			button(state, speedButton, SDL_Color{60, 90, 150, 255}, buf, 4.5f);
			button(state, startButton, SDL_Color{60, 130, 70, 255}, phase == RESULT ? "AGAIN" : "START", 6.0f);
			if(phase == RESULT){
				text(state, resultLine, 200.0f, 6.0f, resultOk ? GREEN : RED);
				text(state, resultDetail, 260.0f, 3.6f, GOLD);
			} else{
				text(state, "WATCH THE CARDS, KEEP THE COUNT, THEN ENTER IT", 220.0f, 3.6f, DIM);
			}
			std::string stats = "RIGHT " + std::to_string(trainer->speedRight) + " OF " + std::to_string(trainer->speedDrills);
			if(trainer->bestDeckSeconds > 0)
				stats += "    BEST FULL DECK " + std::to_string(trainer->bestDeckSeconds) + " SEC";
			text(state, stats, 560.0f, 3.6f, DIM);
			text(state, "A CASINO PACE IS ABOUT 0.5 SEC PER CARD - A FULL DECK IN UNDER 30 SEC", 600.0f, 3.0f, DIM);
		} else if(phase == RUNNING){
			const auto& c = deck[shown];
			drawCard(state, res, c.first, c.second, 670.0f, 200.0f);
			text(state, "CARD " + std::to_string(shown + 1) + " OF " + std::to_string(cardCount()), 380.0f, 4.0f, DIM);
		} else{
			text(state, "WHAT IS THE RUNNING COUNT?", 300.0f, 5.0f, WHITE);
			stepper.draw(state, signedNum(guess));
		}
		button(state, backButton, SDL_Color{80, 80, 80, 255}, "BACK", 6.0f);
	}

	bool handlePoint(SDLState& state, float wx, float wy){
		SDL_FPoint p;
		if(!TrainingUI::toLogical(state, wx, wy, p))
			return false;
		if(TrainingUI::hit(p, backButton)){
			phase = SETUP;
			return true;
		}
		if(phase == SETUP || phase == RESULT){
			if(TrainingUI::hit(p, cardsButton)) cardsIndex = (cardsIndex + 1) % 3;
			if(TrainingUI::hit(p, speedButton)) speedIndex = (speedIndex + 1) % 5;
			if(TrainingUI::hit(p, startButton)) start();
		} else if(phase == ANSWER){
			if(TrainingUI::hit(p, stepper.minus)) guess--;
			if(TrainingUI::hit(p, stepper.plus)) guess++;
			if(TrainingUI::hit(p, stepper.check)) check();
		}
		return false;
	}

	void handleKey(SDL_Scancode sc){
		bool enter = sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_SPACE || sc == SDL_SCANCODE_KP_ENTER;
		if(phase == ANSWER){
			if(sc == SDL_SCANCODE_LEFT || sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_MINUS || sc == SDL_SCANCODE_KP_MINUS) guess--;
			else if(sc == SDL_SCANCODE_RIGHT || sc == SDL_SCANCODE_UP || sc == SDL_SCANCODE_EQUALS || sc == SDL_SCANCODE_KP_PLUS) guess++;
			else if(enter) check();
		} else if((phase == SETUP || phase == RESULT) && enter){
			start();
		}
	}

	// The keys this screen takes for itself while answering.
	bool takesKey(SDL_Scancode sc) const{
		if(phase == ANSWER)
			return true;
		return phase == RUNNING;
	}

	std::vector<SDL_FRect> focusRects(){
		if(phase == ANSWER || phase == RUNNING)
			return { backButton };
		return { cardsButton, speedButton, startButton, backButton };
	}

private:
	enum Phase{ SETUP, RUNNING, ANSWER, RESULT };
	Phase phase = SETUP;
	Trainer* trainer = nullptr;
	std::mt19937 rng{ std::random_device{}() };
	std::vector<std::pair<int, int>> deck;
	int cardsIndex = 2, speedIndex = 1;
	int shown = 0, guess = 0, count = 0;
	float timer = 0.0f;
	std::string resultLine, resultDetail;
	bool resultOk = false;
	TrainingUI::Stepper stepper;

	SDL_FRect cardsButton{ .x = 330, .y = 330, .w = 240, .h = 64 };
	SDL_FRect speedButton{ .x = 590, .y = 330, .w = 300, .h = 64 };
	SDL_FRect startButton{ .x = 910, .y = 330, .w = 200, .h = 64 };
	SDL_FRect backButton{ .x = 620, .y = 640, .w = 200, .h = 56 };

	int cardCount() const{ static const int N[3] = { 13, 26, 52 }; return N[cardsIndex]; }
	float interval() const{ static const float S[5] = { 1.0f, 0.75f, 0.5f, 0.35f, 0.25f }; return S[speedIndex]; }

	void start(){
		deck.clear();
		for(int s = 0; s < 4; s++)
			for(int v = 1; v <= 13; v++)
				deck.push_back({s, v});
		std::shuffle(deck.begin(), deck.end(), rng);
		count = 0;
		for(int i = 0; i < cardCount(); i++)
			count += TrainingUI::hiLo(std::min(deck[i].second, 10));
		shown = 0;
		timer = 0.0f;
		phase = RUNNING;
	}

	void check(){
		resultOk = guess == count;
		int seconds = (int)std::lround(cardCount() * interval());
		trainer->speedDrills++;
		if(resultOk){
			trainer->speedRight++;
			if(cardCount() == 52 && (trainer->bestDeckSeconds == 0 || seconds < trainer->bestDeckSeconds))
				trainer->bestDeckSeconds = seconds;
		}
		dirty = true;
		resultLine = resultOk ? "CORRECT!" : "THE COUNT WAS " + TrainingUI::signedNum(count);
		resultDetail = std::to_string(cardCount()) + " CARDS IN " + std::to_string(seconds) + " SEC" + (resultOk ? "" : " - YOU SAID " + TrainingUI::signedNum(guess));
		if(cardCount() == 52)
			resultDetail += "   A FULL DECK ALWAYS COUNTS TO 0";
		phase = RESULT;
	}
};

// ---------------------------------------------------------------------
class TrueCountMenu
{
public:
	bool dirty = false;

	void open(Trainer& t){
		trainer = &t;
		newQuestion();
	}

	void draw(SDLState& state){
		using namespace TrainingUI;
		clear(state);
		text(state, "TRUE COUNT + DECKS", 20.0f, 6.0f, WHITE);
		if(estimating){
			text(state, std::to_string(shoeDecks) + " DECK SHOE - HOW MANY DECKS ARE LEFT TO PLAY?", 72.0f, 3.8f, GOLD);
			drawTray(state);
			stepper.draw(state, half(guessHalves), answered ? "NEXT" : "CHECK");
		} else{
			text(state, "RUNNING COUNT " + signedNum(runningCount) + " WITH " + half(decksLeftHalves) + " DECKS LEFT", 150.0f, 5.0f, GOLD);
			text(state, "WHAT IS THE TRUE COUNT? ROUND DOWN", 230.0f, 4.0f, DIM);
			stepper.draw(state, signedNum(guess), answered ? "NEXT" : "CHECK");
		}
		if(answered){
			text(state, resultLine, 395.0f, 5.5f, resultOk ? GREEN : RED);
			text(state, resultDetail, 545.0f, 3.4f, GOLD);
		}
		std::string stats = "DECKS RIGHT " + std::to_string(trainer->deckEstimatesRight) + " OF " + std::to_string(trainer->deckEstimates)
			+ "    TRUE COUNT RIGHT " + std::to_string(trainer->trueCountRight) + " OF " + std::to_string(trainer->trueCountQuestions);
		text(state, stats, 600.0f, 3.2f, DIM);
		button(state, backButton, SDL_Color{80, 80, 80, 255}, "BACK", 6.0f);
	}

	bool handlePoint(SDLState& state, float wx, float wy){
		SDL_FPoint p;
		if(!TrainingUI::toLogical(state, wx, wy, p))
			return false;
		if(TrainingUI::hit(p, backButton))
			return true;
		if(TrainingUI::hit(p, stepper.check)){
			answered ? newQuestion() : check();
			return false;
		}
		if(!answered){
			if(TrainingUI::hit(p, stepper.minus)) nudge(-1);
			if(TrainingUI::hit(p, stepper.plus)) nudge(1);
		}
		return false;
	}

	void handleKey(SDL_Scancode sc){
		if(sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_SPACE || sc == SDL_SCANCODE_KP_ENTER){
			answered ? newQuestion() : check();
			return;
		}
		if(answered)
			return;
		if(sc == SDL_SCANCODE_LEFT || sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_MINUS || sc == SDL_SCANCODE_KP_MINUS) nudge(-1);
		else if(sc == SDL_SCANCODE_RIGHT || sc == SDL_SCANCODE_UP || sc == SDL_SCANCODE_EQUALS || sc == SDL_SCANCODE_KP_PLUS) nudge(1);
	}

	std::vector<SDL_FRect> focusRects(){ return { stepper.minus, stepper.plus, stepper.check, backButton }; }

private:
	Trainer* trainer = nullptr;
	std::mt19937 rng{ std::random_device{}() };
	bool estimating = true, answered = false, resultOk = false;
	int shoeDecks = 6, discardQuarters = 8, guessHalves = 6;
	int runningCount = 0, decksLeftHalves = 6, guess = 0;
	std::string resultLine, resultDetail;
	TrainingUI::Stepper stepper;
	SDL_FRect backButton{ .x = 620, .y = 640, .w = 200, .h = 56 };

	static std::string half(int halves){
		return std::to_string(halves / 2) + (halves % 2 ? ".5" : "");
	}

	void nudge(int d){
		if(estimating) guessHalves = std::clamp(guessHalves + d, 0, shoeDecks * 2);
		else guess = std::clamp(guess + d, -30, 30);
	}

	void newQuestion(){
		answered = false;
		estimating = rng() % 2 == 0;
		if(estimating){
			shoeDecks = rng() % 3 == 0 ? 8 : 6;
			discardQuarters = 2 + (int)(rng() % (unsigned)(shoeDecks * 4 - 5));
			guessHalves = shoeDecks;
		} else{
			do{ runningCount = -12 + (int)(rng() % 28); } while(runningCount == 0);
			decksLeftHalves = 1 + (int)(rng() % 11);
			guess = 0;
		}
	}

	void check(){
		answered = true;
		dirty = true;
		if(estimating){
			float left = shoeDecks - discardQuarters / 4.0f;
			resultOk = std::fabs(guessHalves / 2.0f - left) <= 0.5f;
			trainer->deckEstimates++;
			if(resultOk) trainer->deckEstimatesRight++;
			char buf[96];
			std::snprintf(buf, sizeof(buf), "%.2f DECKS LEFT", left);
			resultLine = (resultOk ? "CLOSE ENOUGH - " : "NO - ") + std::string(buf);
			resultDetail = "WITHIN HALF A DECK IS GOOD. COMPARE THE TRAY TO THE 1 DECK STACK";
		} else{
			float exact = runningCount / (decksLeftHalves / 2.0f);
			int answer = IndexPlays::floorCount(exact);
			resultOk = guess == answer;
			trainer->trueCountQuestions++;
			if(resultOk) trainer->trueCountRight++;
			char buf[96];
			std::snprintf(buf, sizeof(buf), "%s OVER %s IS %+.1f, ROUND DOWN TO ", TrainingUI::signedNum(runningCount).c_str(), half(decksLeftHalves).c_str(), exact);
			resultLine = resultOk ? "CORRECT!" : "TRUE COUNT " + TrainingUI::signedNum(answer);
			resultDetail = buf + TrainingUI::signedNum(answer);
		}
	}

	// The discard tray, filled to how much has been played, with a 1-deck
	// stack beside it to judge against.
	void drawTray(SDLState& state){
		const float trayX = 560.0f, trayY = 110.0f, trayH = 300.0f, trayW = 120.0f;
		float perDeck = trayH / shoeDecks;
		auto stack = [&](float x, float y, float h){
			for(float yy = y; yy < y + h; yy += 3.0f){
				SDL_SetRenderDrawColor(state.renderer, ((int)(yy / 3) % 2) ? 235 : 200, ((int)(yy / 3) % 2) ? 235 : 200, ((int)(yy / 3) % 2) ? 230 : 196, 255);
				SDL_FRect line{ .x = x, .y = yy, .w = trayW - 10.0f, .h = std::min(3.0f, y + h - yy) };
				SDL_RenderFillRect(state.renderer, &line);
			}
		};
		SDL_FRect tray{ .x = trayX - 5.0f, .y = trayY - 5.0f, .w = trayW, .h = trayH + 10.0f };
		SDL_SetRenderDrawColor(state.renderer, 30, 30, 30, 255);
		SDL_RenderFillRect(state.renderer, &tray);
		float played = discardQuarters / 4.0f * perDeck;
		stack(trayX, trayY + trayH - played, played);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &tray);
		TrainingUI::text(state, "DISCARDS", trayY + trayH + 14.0f, 3.2f, TrainingUI::DIM, trayX + trayW / 2.0f - 5.0f);

		float refX = 760.0f;
		stack(refX, trayY + trayH - perDeck, perDeck);
		TrainingUI::text(state, "1 DECK", trayY + trayH + 14.0f, 3.2f, TrainingUI::DIM, refX + trayW / 2.0f - 5.0f);
	}
};

// ---------------------------------------------------------------------
class BankrollMenu
{
public:
	// House edge (main bet, perfect basic strategy) and standard deviation
	// per hand, in bets -- measured with this game's own engine, playing
	// the charts, over 3 million rounds each.
	struct Preset{ const char* name; float edge; float sd; };
	static const Preset* presets(){
		static const Preset P[4] = {
			{ "BLACKJACK 6-8 DECKS", 0.0055f, 1.15f },
			{ "BLACKJACK 2 DECKS", 0.0055f, 1.15f },
			{ "SPANISH 21 / PLAYERS EDGE", 0.0065f, 1.16f },
			{ "FREE BET BLACKJACK", 0.0110f, 1.07f },
		};
		return P;
	}
	static constexpr int PRESET_COUNT = 4;

	void draw(SDLState& state){
		using namespace TrainingUI;
		clear(state);
		text(state, "BANKROLL PLANNER", 20.0f, 6.0f, WHITE);
		const Preset& p = presets()[game];
		button(state, gameButton, SDL_Color{150, 120, 40, 255}, std::string("GAME: ") + p.name, 4.0f);
		drawStepper(state, 0, "BET", "$" + std::to_string(BETS[betIndex]));
		drawStepper(state, 1, "HANDS PER HOUR", std::to_string(handsPerHour));
		drawStepper(state, 2, "HOURS", std::to_string(hours));

		long hands = (long)handsPerHour * hours;
		double bet = BETS[betIndex];
		double mean = -p.edge * bet * hands;
		double sd = p.sd * bet * std::sqrt((double)hands);
		auto money = [](double v){
			long r = std::lround(std::fabs(v));
			return std::string(v < -0.5 ? "-$" : "$") + std::to_string(r);
		};
		auto plusMoney = [&](double v){ return v >= 0.5 ? "+" + money(v) : money(v); };
		float y = 330.0f;
		auto row = [&](const std::string& label, const std::string& value, SDL_Color c){
			DigitFont::drawText(state, label, 180.0f, y, 3.6f, GOLD);
			DigitFont::drawText(state, value, 1260.0f - DigitFont::textWidth(value, 4.2f), y - 2.0f, 4.2f, c);
			y += 40.0f;
		};
		row("YOUR TOTAL ACTION, " + std::to_string(hands) + " HANDS", "$" + std::to_string((long)(bet * hands)), WHITE);
		row("EXPECTED RESULT", plusMoney(mean), RED);
		row("2 IN 3 SESSIONS END BETWEEN", plusMoney(mean - sd) + " AND " + plusMoney(mean + sd), WHITE);
		row("1 IN 20 SESSIONS LOSE AT LEAST", money(-(mean - 1.645 * sd)), RED);
		row("BRING THIS TO BE 95% SURE NOT TO GO BROKE", money(bankrollFor(0.05, mean, sd, hands)), GREEN);
		row("BRING THIS TO BE 99% SURE", money(bankrollFor(0.01, mean, sd, hands)), GREEN);
		text(state, "ASSUMES PERFECT BASIC STRATEGY ON THE MAIN BET. SIDE BETS COST FAR MORE", 590.0f, 3.0f, DIM);
		button(state, backButton, SDL_Color{80, 80, 80, 255}, "BACK", 6.0f);
	}

	bool handlePoint(SDLState& state, float wx, float wy){
		SDL_FPoint p;
		if(!TrainingUI::toLogical(state, wx, wy, p))
			return false;
		if(TrainingUI::hit(p, backButton))
			return true;
		if(TrainingUI::hit(p, gameButton))
			game = (game + 1) % PRESET_COUNT;
		for(int i = 0; i < 3; i++){
			if(TrainingUI::hit(p, minusRect(i))) step(i, -1);
			if(TrainingUI::hit(p, plusRect(i))) step(i, 1);
		}
		return false;
	}

	std::vector<SDL_FRect> focusRects(){
		std::vector<SDL_FRect> r{ gameButton };
		for(int i = 0; i < 3; i++){
			r.push_back(minusRect(i));
			r.push_back(plusRect(i));
		}
		r.push_back(backButton);
		return r;
	}

	// The money to start with so the chance of losing all of it at some
	// point in the session is `risk`: the session as a random walk with a
	// drift (Brownian motion), whose chance of ever falling `b` below the
	// start within the session has a closed form.
	static double bankrollFor(double risk, double mean, double sd, long hands){
		if(sd <= 0.0)
			return std::max(0.0, -mean);
		auto phi = [](double x){ return 0.5 * std::erfc(-x / std::sqrt(2.0)); };
		double mu = mean / hands, var = sd * sd / hands;
		auto ruin = [&](double b){
			double t = (double)hands;
			double s = std::sqrt(var * t);
			return phi((-b - mu * t) / s) + std::exp(-2.0 * mu * b / var) * phi((-b + mu * t) / s);
		};
		double lo = 0.0, hi = std::fabs(mean) + 10.0 * sd;
		for(int i = 0; i < 80; i++){
			double mid = (lo + hi) / 2.0;
			(ruin(mid) > risk ? lo : hi) = mid;
		}
		return hi;
	}

private:
	static constexpr int BETS[10] = { 5, 10, 15, 25, 50, 75, 100, 200, 300, 500 };
	int game = 0, betIndex = 3, handsPerHour = 70, hours = 3;

	SDL_FRect gameButton{ .x = 420, .y = 85, .w = 600, .h = 56 };
	SDL_FRect backButton{ .x = 620, .y = 640, .w = 200, .h = 56 };

	SDL_FRect minusRect(int i){ return SDL_FRect{ .x = 760, .y = 160.0f + i * 54.0f, .w = 60, .h = 46 }; }
	SDL_FRect plusRect(int i){ return SDL_FRect{ .x = 1000, .y = 160.0f + i * 54.0f, .w = 60, .h = 46 }; }

	void drawStepper(SDLState& state, int i, const std::string& label, const std::string& value){
		using namespace TrainingUI;
		DigitFont::drawText(state, label, 380.0f, minusRect(i).y + 12.0f, 4.0f, WHITE);
		button(state, minusRect(i), SDL_Color{70, 70, 80, 255}, "-", 5.0f);
		button(state, plusRect(i), SDL_Color{70, 70, 80, 255}, "+", 5.0f);
		text(state, value, minusRect(i).y + 10.0f, 5.0f, SDL_Color{255, 225, 80, 255}, 910.0f);
	}

	void step(int i, int d){
		if(i == 0) betIndex = std::clamp(betIndex + d, 0, 9);
		else if(i == 1) handsPerHour = std::clamp(handsPerHour + 10 * d, 30, 150);
		else hours = std::clamp(hours + d, 1, 24);
	}
};
