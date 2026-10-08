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

	inline void drawCard(SDLState& state, Resources& res, int suit, int value, float x, float y, float scale = 1.0f){
		Card c(suit, value, true, SDL_FPoint{x, y}, 0.0f);
		c.drawScaled(state, res, scale);
	}

	// The drills (hub, strategy drill, review, counting speed, true count)
	// are portrait screens -- 720 x 1440, held in one hand, with every
	// answer button in the bottom half where a thumb reaches.
	constexpr float PW = 720.0f, PH = 1440.0f, CX = 360.0f, MAXW = 680.0f;

	// Set by mina.cpp: true on phones and tablets (portrait screens), false
	// on a desktop or laptop, where these screens keep their landscape
	// 1440 x 720 layout.
	inline bool portrait = false;

	inline void ptext(SDLState& state, const std::string& s, float y, float pixel, SDL_Color color){
		text(state, s, y, pixel, color, CX, MAXW);
	}

	// BACK: top-left in portrait, bottom-center in landscape.
	inline SDL_FRect backRect(){
		return portrait ? SDL_FRect{ .x = 16, .y = 16, .w = 150, .h = 70 } : SDL_FRect{ .x = 620, .y = 640, .w = 200, .h = 56 };
	}

	// - value + and CHECK. Portrait: big buttons for a thumb, CHECK across
	// the bottom; landscape: one row mid-screen.
	struct Stepper{
		SDL_FRect minus, plus, check;
		void layout(){
			if(portrait){
				minus = SDL_FRect{ .x = 20, .y = 1090, .w = 220, .h = 150 };
				plus = SDL_FRect{ .x = 480, .y = 1090, .w = 220, .h = 150 };
				check = SDL_FRect{ .x = 20, .y = 1260, .w = 680, .h = 140 };
			} else{
				minus = SDL_FRect{ .x = 470, .y = 470, .w = 70, .h = 60 };
				plus = SDL_FRect{ .x = 760, .y = 470, .w = 70, .h = 60 };
				check = SDL_FRect{ .x = 860, .y = 470, .w = 200, .h = 60 };
			}
		}
		void draw(SDLState& state, const std::string& value, const std::string& checkLabel = "CHECK"){
			layout();
			if(portrait){
				button(state, minus, SDL_Color{70, 70, 80, 255}, "-", 12.0f);
				button(state, plus, SDL_Color{70, 70, 80, 255}, "+", 12.0f);
				text(state, value, 1140.0f, 10.0f, SDL_Color{255, 225, 80, 255}, CX, 220.0f);
				button(state, check, SDL_Color{60, 130, 70, 255}, checkLabel, 9.0f);
			} else{
				button(state, minus, SDL_Color{70, 70, 80, 255}, "-", 6.0f);
				button(state, plus, SDL_Color{70, 70, 80, 255}, "+", 6.0f);
				text(state, value, 480.0f, 7.0f, SDL_Color{255, 225, 80, 255}, 650.0f);
				button(state, check, SDL_Color{60, 130, 70, 255}, checkLabel, 5.0f);
			}
		}
		bool hitMinus(SDL_FPoint p){ layout(); return hit(p, minus); }
		bool hitPlus(SDL_FPoint p){ layout(); return hit(p, plus); }
		bool hitCheck(SDL_FPoint p){ layout(); return hit(p, check); }
		std::vector<SDL_FRect> rects(){ layout(); return { minus, plus, check }; }
	};
}

// ---------------------------------------------------------------------
class TrainingMenu
{
public:
	enum Choice{ NONE, DRILL, REVIEW, COUNT_SPEED, TRUE_COUNT, BANKROLL, ODDS, BACK };
	static constexpr int COUNT = 6;

	void draw(SDLState& state, const Trainer& trainer){
		using namespace TrainingUI;
		clear(state);
		if(portrait)
			ptext(state, "TRAINING", 40.0f, 8.0f, WHITE);
		else
			text(state, "TRAINING", 30.0f, 7.0f, WHITE);
		static const char* LABELS[COUNT] = { "STRATEGY DRILL", "MISTAKES + ACCURACY", "COUNTING SPEED", "TRUE COUNT + DECKS", "BANKROLL PLANNER", "GAME ODDS" };
		static const char* NOTES[COUNT] = {
			"FLASH CARDS. HANDS YOU MISS COME BACK MORE",
			"WHICH CHART CELLS YOU KNOW, AND YOUR LAST MISTAKES",
			"KEEP THE HI-LO RUNNING COUNT AS CARDS FLASH BY",
			"ESTIMATE DECKS LEFT, TURN RUNNING COUNT INTO TRUE COUNT",
			"WHAT A SESSION COSTS, AND HOW MUCH MONEY TO BRING",
			"EVERY GAME AND SIDE BET RANKED - WHERE YOUR MONEY LASTS LONGEST"
		};
		static const SDL_Color COLORS[COUNT] = { {60, 130, 70, 255}, {150, 70, 70, 255}, {60, 90, 150, 255}, {50, 120, 130, 255}, {150, 120, 40, 255}, {120, 70, 130, 255} };
		for(int i = 0; i < COUNT && !portrait; i++){
			button(state, buttonRect(i), COLORS[i], LABELS[i], 4.5f);
			float np = std::min(3.0f, 780.0f / DigitFont::textWidth(NOTES[i], 1.0f));
			DigitFont::drawText(state, NOTES[i], 640.0f, buttonRect(i).y + 35.0f - 2.5f * np, np, DIM);
		}
		for(int i = 0; i < COUNT && portrait; i++){
			// The label up top in the button, its note under it.
			SDL_FRect b = buttonRect(i);
			SDL_SetRenderDrawColor(state.renderer, COLORS[i].r, COLORS[i].g, COLORS[i].b, 255);
			SDL_RenderFillRect(state.renderer, &b);
			SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
			SDL_RenderRect(state.renderer, &b);
			text(state, LABELS[i], b.y + 26.0f, 6.0f, WHITE, CX, 600.0f);
			text(state, NOTES[i], b.y + 86.0f, 2.8f, SDL_Color{225, 235, 225, 255}, CX, 600.0f);
		}
		int drills = 0, right = 0;
		for(int s = 0; s < StrategyChart::SET_COUNT; s++){
			drills += trainer.totalAttempts(s);
			right += trainer.totalCorrect(s);
		}
		std::string summary = drills > 0
			? std::to_string(drills) + " STRATEGY DECISIONS SO FAR, " + std::to_string((right * 100 + drills / 2) / drills) + "% RIGHT"
			: "EVERY DECISION IN DRILLS AND REAL HANDS IS TRACKED HERE";
		if(portrait){
			ptext(state, summary, 1120.0f, 3.0f, GOLD);
			ptext(state, "BANKROLL AND GAME ODDS TURN THE SCREEN SIDEWAYS", 1170.0f, 2.6f, DIM);
		} else{
			text(state, summary, 578.0f, 3.4f, GOLD);
		}
		button(state, backButton(), SDL_Color{80, 80, 80, 255}, "BACK", portrait ? 8.0f : 6.0f);
	}

	Choice handlePoint(SDLState& state, float wx, float wy){
		SDL_FPoint p;
		if(!TrainingUI::toLogical(state, wx, wy, p))
			return NONE;
		for(int i = 0; i < COUNT; i++)
			if(TrainingUI::hit(p, buttonRect(i)))
				return (Choice)(DRILL + i);
		if(TrainingUI::hit(p, backButton()))
			return BACK;
		return NONE;
	}

	std::vector<SDL_FRect> focusRects(){
		std::vector<SDL_FRect> r;
		for(int i = 0; i < COUNT; i++)
			r.push_back(buttonRect(i));
		r.push_back(backButton());
		return r;
	}

private:
	SDL_FRect buttonRect(int i){
		if(TrainingUI::portrait)
			return SDL_FRect{ .x = 30, .y = 140.0f + i * 158.0f, .w = 660, .h = 140 };
		return SDL_FRect{ .x = 160, .y = 92.0f + i * 78.0f, .w = 450, .h = 64 };
	}
	SDL_FRect backButton(){
		if(TrainingUI::portrait)
			return SDL_FRect{ .x = 30, .y = 1240, .w = 660, .h = 130 };
		return SDL_FRect{ .x = 620, .y = 630, .w = 200, .h = 56 };
	}
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
		selector.grid = true;
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
		selector.grid = portrait;
		if(!portrait){
			drawWide(state, res);
			return;
		}
		StrategyChart::Rules rules = selector.rules();
		DigitFont::drawText(state, "DRILL", 200.0f, 34.0f, 6.0f, WHITE);
		DigitFont::drawText(state, std::to_string(sessionRight) + "/" + std::to_string(sessionTotal), 560.0f, 34.0f, 5.0f, DIM);
		ptext(state, StrategyChart::titleFor(rules), 104.0f, 2.6f, DIM);
		selector.draw(state, rulesY(), true);

		ptext(state, "DEALER", 340.0f, 4.0f, GOLD);
		drawCard(state, res, dealerSuit, dealerValue, CX - 70.0f, 375.0f, 1.4f);
		if(counting)
			ptext(state, "TRUE COUNT " + signedNum(trueCount), 585.0f, 5.0f, SDL_Color{255, 225, 80, 255});
		for(size_t i = 0; i < playerCards.size(); i++)
			drawCard(state, res, playerCards[i].first, playerCards[i].second, CX - 150.0f + i * 160.0f, 630.0f, 1.4f);

		if(phase == CORRECT){
			ptext(state, "CORRECT - " + std::string(moveName(answer)), 845.0f, 6.0f, GREEN);
			if(!answerWhy.empty())
				ptext(state, answerWhy, 900.0f, 2.8f, GOLD);
		} else if(phase == WRONG){
			ptext(state, std::string("THE PLAY IS ") + moveName(answer), 845.0f, 6.0f, RED);
			std::string detail = handName(section, row) + " VS " + dealerName(col);
			if(!answerWhy.empty())
				detail += " - " + answerWhy;
			ptext(state, detail, 900.0f, 2.8f, GOLD);
			ptext(state, "TAP ANYWHERE FOR THE NEXT HAND", 935.0f, 2.6f, DIM);
		} else{
			int known = trainer ? trainer->totalAttempts(rules.set) : 0;
			int knownRight = trainer ? trainer->totalCorrect(rules.set) : 0;
			std::string score = "STREAK " + std::to_string(streak) + "   BEST " + std::to_string(best);
			if(known > 0)
				score += "   THIS CHART " + std::to_string((knownRight * 100 + known / 2) / known) + "%";
			ptext(state, score, 900.0f, 3.0f, DIM);
		}

		for(int i = 0; i < 5; i++){
			bool enabled = moveEnabled(MOVES[i]);
			SDL_Color c = !enabled ? SDL_Color{45, 50, 48, 255}
				: (phase != ASKING && MOVES[i] == answer) ? SDL_Color{60, 150, 70, 255}
				: (phase == WRONG && MOVES[i] == chosen) ? SDL_Color{160, 60, 60, 255}
				: StrategyChart::colorFor(MOVES[i]);
			button(state, moveRect(i), c, moveName(MOVES[i]), 8.0f);
		}
		button(state, backButton(), SDL_Color{80, 80, 80, 255}, "BACK", 6.0f);
	}

	bool handlePoint(SDLState& state, float wx, float wy){
		SDL_FPoint p;
		if(!TrainingUI::toLogical(state, wx, wy, p))
			return false;
		if(TrainingUI::hit(p, backButton()))
			return true;
		if(selector.handle(p, rulesY(), true)){
			newQuestion();
			return false;
		}
		if(phase == ASKING){
			for(int i = 0; i < 5; i++)
				if(TrainingUI::hit(p, moveRect(i)))
					choose(MOVES[i]);
		} else if(TrainingUI::portrait ? p.y > rulesY() + 190.0f : p.y < rulesY()){
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
		selector.addRects(r, rulesY(), true);
		r.push_back(backButton());
		return r;
	}

	// For tests: the current question's answer.
	char currentAnswer() const{ return answer; }

private:
	static constexpr char MOVES[5] = { 'H', 'S', 'D', 'P', 'R' };
	float rulesY() const{ return TrainingUI::portrait ? 130.0f : 585.0f; }
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

	// HIT / STAND, DOUBLE / SPLIT, then SURRENDER across the bottom.
	SDL_FRect moveRect(int i){
		if(!TrainingUI::portrait)
			return SDL_FRect{ .x = 192.0f + i * 214.0f, .y = 476, .w = 200, .h = 60 };
		if(i == 4)
			return SDL_FRect{ .x = 20, .y = 1286, .w = 680, .h = 124 };
		return SDL_FRect{ .x = 20.0f + (i % 2) * 350.0f, .y = 990.0f + (i / 2) * 148.0f, .w = 330, .h = 134 };
	}
	SDL_FRect backButton(){ return TrainingUI::backRect(); }

	// The landscape (desktop) layout.
	void drawWide(SDLState& state, Resources& res){
		using namespace TrainingUI;
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

		selector.draw(state, rulesY(), true);
		button(state, backButton(), SDL_Color{80, 80, 80, 255}, "BACK", 6.0f);
	}

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
		selector.grid = portrait;
		if(!portrait){
			drawWide(state);
			return;
		}
		clear(state);
		if(showingMistakes){
			DigitFont::drawText(state, "MISTAKES", 200.0f, 34.0f, 6.0f, WHITE);
			drawMistakes(state);
		} else{
			DigitFont::drawText(state, "ACCURACY", 200.0f, 34.0f, 6.0f, WHITE);
			StrategyChart::Rules rules = selector.rules();
			selector.draw(state, rulesY(), false);
			static const char* TABS[3] = { "HARD", "SOFT", "PAIRS" };
			for(int i = 0; i < 3; i++)
				button(state, tabRect(i), i == section ? SDL_Color{60, 90, 150, 255} : SDL_Color{50, 55, 60, 255}, TABS[i], 5.0f);
			drawTable(state, rules);
			drawAccuracyNotes(state, rules);
		}
		button(state, backButton(), SDL_Color{80, 80, 80, 255}, "BACK", 6.0f);
		button(state, pageButton(), SDL_Color{60, 90, 150, 255}, showingMistakes ? "SHOW ACCURACY" : "SHOW MISTAKES", 7.0f);
	}

	bool handlePoint(SDLState& state, float wx, float wy){
		SDL_FPoint p;
		if(!TrainingUI::toLogical(state, wx, wy, p))
			return false;
		if(TrainingUI::hit(p, backButton()))
			return true;
		if(TrainingUI::hit(p, pageButton()))
			showingMistakes = !showingMistakes;
		else if(!TrainingUI::portrait)
			selector.handle(p, rulesY(), false);
		else if(!showingMistakes){
			selector.handle(p, rulesY(), false);
			for(int i = 0; i < 3; i++)
				if(TrainingUI::hit(p, tabRect(i)))
					section = i;
		}
		return false;
	}

	std::vector<SDL_FRect> focusRects(){
		selector.grid = TrainingUI::portrait;
		std::vector<SDL_FRect> r;
		if(!TrainingUI::portrait || !showingMistakes)
			selector.addRects(r, rulesY(), false);
		if(TrainingUI::portrait && !showingMistakes)
			for(int i = 0; i < 3; i++)
				r.push_back(tabRect(i));
		r.push_back(pageButton());
		r.push_back(backButton());
		return r;
	}

private:
	static constexpr float TABLE_Y = 380.0f;
	static constexpr float CELL = 56.0f, STRIDE = 61.0f;
	RuleSelector selector;
	const Trainer* trainer = nullptr;
	bool showingMistakes = false;
	int section = 0;
	SDL_FRect backButton(){ return TrainingUI::backRect(); }
	float rulesY() const{ return TrainingUI::portrait ? 110.0f : 585.0f; }
	SDL_FRect pageButton() const{
		return TrainingUI::portrait ? SDL_FRect{ .x = 20, .y = 1290, .w = 680, .h = 120 } : SDL_FRect{ .x = 840, .y = 640, .w = 280, .h = 56 };
	}

	SDL_FRect tabRect(int i){ return SDL_FRect{ .x = 20.0f + i * 230.0f, .y = 300, .w = 220, .h = 64 }; }

	static SDL_Color accuracyColor(float a){
		return a < 0.0f ? SDL_Color{40, 46, 44, 255}
			: a >= 0.9f ? SDL_Color{50, 140, 70, 255}
			: a >= 0.7f ? SDL_Color{170, 140, 40, 255}
			: SDL_Color{165, 50, 50, 255};
	}

	static void cell(SDLState& state, const SDL_FRect& r, SDL_Color c, const std::string& label, float pixel){
		SDL_SetRenderDrawColor(state.renderer, c.r, c.g, c.b, 255);
		SDL_RenderFillRect(state.renderer, &r);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &r);
		float w = DigitFont::textWidth(label, pixel);
		if(w > r.w - 8.0f){
			pixel *= (r.w - 8.0f) / w;
			w = DigitFont::textWidth(label, pixel);
		}
		DigitFont::drawText(state, label, r.x + (r.w - w) / 2.0f, r.y + (r.h - 5 * pixel) / 2.0f, pixel, SDL_Color{255, 255, 255, 255});
	}

	// One section of the chart (hard / soft / pairs), each cell coloured by
	// how often it's been played right, highest total at the top.
	void drawTable(SDLState& state, const StrategyChart::Rules& rules){
		float x0 = 360.0f - 11.0f * STRIDE / 2.0f;
		SDL_Color head{40, 60, 45, 255};
		for(int c = 0; c < 10; c++)
			cell(state, SDL_FRect{ x0 + STRIDE * (c + 1), TABLE_Y, CELL, CELL - 8.0f }, head, StrategyChart::dealerColLabel(c), 3.6f);
		int rows = StrategyChart::rowCount(section);
		for(int r = 0; r < rows; r++){
			float y = TABLE_Y + STRIDE * (rows - r) - 8.0f;
			cell(state, SDL_FRect{ x0, y, CELL, CELL }, head, StrategyChart::rowLabel(section, r), 3.6f);
			for(int c = 0; c < 10; c++){
				SDL_Color color = accuracyColor(trainer->accuracy(rules.set, section, r, c));
				cell(state, SDL_FRect{ x0 + STRIDE * (c + 1), y, CELL, CELL }, color, StrategyChart::cellLabel(rules, section, r, c), 3.6f);
			}
		}
	}

	void drawAccuracyNotes(SDLState& state, const StrategyChart::Rules& rules){
		using namespace TrainingUI;
		struct Key{ SDL_Color c; const char* label; };
		static const Key KEYS[4] = {
			{ {50, 140, 70, 255}, "90%+" }, { {170, 140, 40, 255}, "70-89%" },
			{ {165, 50, 50, 255}, "UNDER 70%" }, { {40, 46, 44, 255}, "NOT YET" }
		};
		float y = TABLE_Y + STRIDE * 11.0f + 10.0f;
		for(int i = 0; i < 4; i++){
			float x = 30.0f + (i % 2) * 340.0f, yy = y + (i / 2) * 40.0f;
			SDL_FRect sw{ .x = x, .y = yy, .w = 34, .h = 28 };
			SDL_SetRenderDrawColor(state.renderer, KEYS[i].c.r, KEYS[i].c.g, KEYS[i].c.b, 255);
			SDL_RenderFillRect(state.renderer, &sw);
			SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
			SDL_RenderRect(state.renderer, &sw);
			DigitFont::drawText(state, KEYS[i].label, x + 46.0f, yy + 5.0f, 3.4f, WHITE);
		}
		y += 92.0f;

		int n = trainer->totalAttempts(rules.set), right = trainer->totalCorrect(rules.set);
		ptext(state, n == 0 ? "NO DECISIONS ON THIS CHART YET"
			: "THIS CHART: " + std::to_string((right * 100 + n / 2) / n) + "% RIGHT OF " + std::to_string(n), y, 3.4f, GOLD);
		// The 3 weakest cells.
		std::vector<Weak> weak = weakCells(rules);
		for(size_t i = 0; i < weak.size() && i < 3; i++)
			ptext(state, "WEAK: " + handName(weak[i].s, weak[i].r) + " VS " + dealerName(weak[i].c) + "  "
				+ std::to_string((int)std::lround(weak[i].a * 100.0f)) + "%", y + 40.0f + i * 34.0f, 3.0f, RED);
	}

	void drawMistakes(SDLState& state){
		using namespace TrainingUI;
		if(trainer->mistakes.empty()){
			ptext(state, "NONE YET", 400.0f, 6.0f, GOLD);
			ptext(state, "EVERY MISSED DECISION, IN DRILLS OR", 480.0f, 3.2f, DIM);
			ptext(state, "REAL HANDS, SHOWS UP HERE", 520.0f, 3.2f, DIM);
			return;
		}
		float y = 120.0f;
		for(size_t i = 0; i < trainer->mistakes.size() && i < 14; i++){
			const Trainer::Mistake& m = trainer->mistakes[i];
			std::string hand = handName(m.section, m.row) + " VS " + dealerName(m.col);
			if(m.cards > 2)
				hand += ", " + std::to_string(m.cards) + " CARDS";
			DigitFont::drawText(state, hand, 30.0f, y, 3.6f, WHITE);
			std::string detail = std::string("YOU ") + moveName(m.chosen) + ", PLAY " + moveName(m.correct);
			DigitFont::drawText(state, detail, 30.0f, y + 30.0f, 2.8f, GREEN);
			std::string where = std::string(setShortName(m.set)) + (m.fromDrill ? "" : " GAME")
				+ (m.trueCount == Trainer::NO_COUNT ? "" : " TC " + signedNum(m.trueCount));
			DigitFont::drawText(state, where, 690.0f - DigitFont::textWidth(where, 2.4f), y + 32.0f, 2.4f, DIM);
			y += 80.0f;
		}
	}
	// Landscape (desktop): the whole chart at once, tinted by accuracy.
	void drawWide(SDLState& state){
		using namespace TrainingUI;
		StrategyChart::Rules rules = selector.rules();
		if(showingMistakes)
			drawMistakesWide(state);
		else{
			StrategyChart::Tint tint = [&](int s, int r, int c, SDL_Color& color){
				color = accuracyColor(trainer->accuracy(rules.set, s, r, c));
				return true;
			};
			StrategyChart::drawChart(state, rules, false, 0, 0, 0, true, &tint);
			drawAccuracyNotesWide(state, rules);
		}
		selector.draw(state, rulesY(), false);
		button(state, backButton(), SDL_Color{80, 80, 80, 255}, "BACK", 6.0f);
		button(state, pageButton(), SDL_Color{60, 90, 150, 255}, showingMistakes ? "ACCURACY" : "MISTAKES", 5.0f);
	}

	// The cells played at least 3 times and under 90%, weakest first.
	struct Weak{ float a; int s, r, c; };
	std::vector<Weak> weakCells(const StrategyChart::Rules& rules){
		std::vector<Weak> weak;
		for(int s = 0; s < 3; s++)
			for(int r = 0; r < StrategyChart::rowCount(s); r++)
				for(int c = 0; c < 10; c++){
					int row = Trainer::cellRow(s, r);
					if(trainer->attempts[rules.set][row][c] >= 3 && trainer->accuracy(rules.set, s, r, c) < 0.9f)
						weak.push_back({ trainer->accuracy(rules.set, s, r, c), s, r, c });
				}
		std::sort(weak.begin(), weak.end(), [](const Weak& a, const Weak& b){ return a.a < b.a; });
		return weak;
	}

	void drawAccuracyNotesWide(SDLState& state, const StrategyChart::Rules& rules){
		using namespace TrainingUI;
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
		std::vector<Weak> weak = weakCells(rules);
		if(!weak.empty()){
			line += "   WEAKEST:";
			size_t shown = std::min<size_t>(3, weak.size());
			for(size_t i = 0; i < shown; i++)
				line += " " + handName(weak[i].s, weak[i].r) + " VS " + dealerName(weak[i].c) + (i + 1 < shown ? "," : "");
		}
		text(state, line, 550.0f, 3.0f, GOLD);
	}

	void drawMistakesWide(SDLState& state){
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
		if(!portrait){
			drawWide(state, res);
			return;
		}
		DigitFont::drawText(state, "COUNT SPEED", 190.0f, 34.0f, 5.5f, WHITE);
		ptext(state, "HI-LO:  2-6 +1   7-9 0   10-A -1", 120.0f, 4.0f, GOLD);

		if(phase == SETUP || phase == RESULT){
			button(state, cardsButton(), SDL_Color{60, 90, 150, 255}, "CARDS: " + std::to_string(cardCount()), 6.0f);
			button(state, speedButton(), SDL_Color{60, 90, 150, 255}, speedLabel(), 6.0f);
			button(state, startButton(), SDL_Color{60, 130, 70, 255}, phase == RESULT ? "AGAIN" : "START", 11.0f);
			if(phase == RESULT){
				ptext(state, resultLine, 560.0f, 7.0f, resultOk ? GREEN : RED);
				ptext(state, resultDetail, 640.0f, 3.2f, GOLD);
			} else{
				ptext(state, "WATCH THE CARDS, KEEP THE COUNT,", 580.0f, 3.6f, DIM);
				ptext(state, "THEN ENTER IT", 625.0f, 3.6f, DIM);
			}
			ptext(state, statsLine(), 760.0f, 3.6f, DIM);
			ptext(state, "CASINO PACE: ABOUT 0.5 SEC A CARD,", 840.0f, 3.0f, DIM);
			ptext(state, "A FULL DECK IN UNDER 30 SEC", 880.0f, 3.0f, DIM);
		} else if(phase == RUNNING){
			const auto& c = deck[shown];
			drawCard(state, res, c.first, c.second, CX - 125.0f, 320.0f, 2.5f);
			ptext(state, "CARD " + std::to_string(shown + 1) + " OF " + std::to_string(cardCount()), 720.0f, 5.0f, DIM);
		} else{
			ptext(state, "RUNNING COUNT?", 560.0f, 8.0f, WHITE);
			stepper.draw(state, signedNum(guess));
		}
		button(state, backButton(), SDL_Color{80, 80, 80, 255}, "BACK", 6.0f);
	}

	bool handlePoint(SDLState& state, float wx, float wy){
		SDL_FPoint p;
		if(!TrainingUI::toLogical(state, wx, wy, p))
			return false;
		if(TrainingUI::hit(p, backButton())){
			phase = SETUP;
			return true;
		}
		if(phase == SETUP || phase == RESULT){
			if(TrainingUI::hit(p, cardsButton())) cardsIndex = (cardsIndex + 1) % 3;
			if(TrainingUI::hit(p, speedButton())) speedIndex = (speedIndex + 1) % 5;
			if(TrainingUI::hit(p, startButton())) start();
		} else if(phase == ANSWER){
			if(stepper.hitMinus(p)) guess--;
			if(stepper.hitPlus(p)) guess++;
			if(stepper.hitCheck(p)) check();
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
			return { backButton() };
		return { cardsButton(), speedButton(), startButton(), backButton() };
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

	SDL_FRect cardsButton() const{
		return TrainingUI::portrait ? SDL_FRect{ .x = 20, .y = 200, .w = 680, .h = 120 } : SDL_FRect{ .x = 330, .y = 330, .w = 240, .h = 64 };
	}
	SDL_FRect speedButton() const{
		return TrainingUI::portrait ? SDL_FRect{ .x = 20, .y = 340, .w = 680, .h = 120 } : SDL_FRect{ .x = 590, .y = 330, .w = 300, .h = 64 };
	}
	SDL_FRect startButton() const{
		return TrainingUI::portrait ? SDL_FRect{ .x = 20, .y = 1200, .w = 680, .h = 210 } : SDL_FRect{ .x = 910, .y = 330, .w = 200, .h = 64 };
	}
	SDL_FRect backButton(){ return TrainingUI::backRect(); }

	std::string speedLabel() const{
		char buf[32];
		std::snprintf(buf, sizeof(buf), "%.2f SEC PER CARD", interval());
		return buf;
	}

	std::string statsLine() const{
		std::string stats = "RIGHT " + std::to_string(trainer->speedRight) + " OF " + std::to_string(trainer->speedDrills);
		if(trainer->bestDeckSeconds > 0)
			stats += "    BEST FULL DECK " + std::to_string(trainer->bestDeckSeconds) + " SEC";
		return stats;
	}

	// Landscape (desktop): settings in one row, the card mid-screen.
	void drawWide(SDLState& state, Resources& res){
		using namespace TrainingUI;
		text(state, "COUNTING SPEED", 20.0f, 6.0f, WHITE);
		text(state, "HI-LO: 2-6 COUNT +1    7-9 COUNT 0    10S AND ACES COUNT -1", 70.0f, 3.4f, GOLD);
		if(phase == SETUP || phase == RESULT){
			button(state, cardsButton(), SDL_Color{60, 90, 150, 255}, "CARDS: " + std::to_string(cardCount()), 4.5f);
			button(state, speedButton(), SDL_Color{60, 90, 150, 255}, speedLabel(), 4.5f);
			button(state, startButton(), SDL_Color{60, 130, 70, 255}, phase == RESULT ? "AGAIN" : "START", 6.0f);
			if(phase == RESULT){
				text(state, resultLine, 200.0f, 6.0f, resultOk ? GREEN : RED);
				text(state, resultDetail, 260.0f, 3.6f, GOLD);
			} else{
				text(state, "WATCH THE CARDS, KEEP THE COUNT, THEN ENTER IT", 220.0f, 3.6f, DIM);
			}
			text(state, statsLine(), 560.0f, 3.6f, DIM);
			text(state, "A CASINO PACE IS ABOUT 0.5 SEC PER CARD - A FULL DECK IN UNDER 30 SEC", 600.0f, 3.0f, DIM);
		} else if(phase == RUNNING){
			const auto& c = deck[shown];
			drawCard(state, res, c.first, c.second, 670.0f, 200.0f);
			text(state, "CARD " + std::to_string(shown + 1) + " OF " + std::to_string(cardCount()), 380.0f, 4.0f, DIM);
		} else{
			text(state, "WHAT IS THE RUNNING COUNT?", 300.0f, 5.0f, WHITE);
			stepper.draw(state, signedNum(guess));
		}
		button(state, backButton(), SDL_Color{80, 80, 80, 255}, "BACK", 6.0f);
	}

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
		if(!portrait){
			drawWide(state);
			return;
		}
		DigitFont::drawText(state, "TRUE COUNT", 200.0f, 34.0f, 5.5f, WHITE);
		if(estimating){
			ptext(state, std::to_string(shoeDecks) + " DECK SHOE", 120.0f, 5.0f, GOLD);
			ptext(state, "HOW MANY DECKS ARE LEFT?", 170.0f, 4.0f, GOLD);
			drawTray(state);
			stepper.draw(state, half(guessHalves), answered ? "NEXT" : "CHECK");
		} else{
			ptext(state, "RUNNING COUNT", 200.0f, 4.0f, DIM);
			ptext(state, signedNum(runningCount), 250.0f, 12.0f, GOLD);
			ptext(state, "DECKS LEFT", 380.0f, 4.0f, DIM);
			ptext(state, half(decksLeftHalves), 430.0f, 12.0f, GOLD);
			ptext(state, "TRUE COUNT? ROUND DOWN", 580.0f, 4.0f, WHITE);
			stepper.draw(state, signedNum(guess), answered ? "NEXT" : "CHECK");
		}
		if(answered){
			ptext(state, resultLine, 920.0f, 5.0f, resultOk ? GREEN : RED);
			ptext(state, resultDetail, 980.0f, 2.8f, GOLD);
		}
		ptext(state, statsLine(), 1035.0f, 2.8f, DIM);
		button(state, backButton(), SDL_Color{80, 80, 80, 255}, "BACK", 6.0f);
	}

	bool handlePoint(SDLState& state, float wx, float wy){
		SDL_FPoint p;
		if(!TrainingUI::toLogical(state, wx, wy, p))
			return false;
		if(TrainingUI::hit(p, backButton()))
			return true;
		if(stepper.hitCheck(p)){
			answered ? newQuestion() : check();
			return false;
		}
		if(!answered){
			if(stepper.hitMinus(p)) nudge(-1);
			if(stepper.hitPlus(p)) nudge(1);
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

	std::vector<SDL_FRect> focusRects(){ auto r = stepper.rects(); r.push_back(backButton()); return r; }

private:
	Trainer* trainer = nullptr;
	std::mt19937 rng{ std::random_device{}() };
	bool estimating = true, answered = false, resultOk = false;
	int shoeDecks = 6, discardQuarters = 8, guessHalves = 6;
	int runningCount = 0, decksLeftHalves = 6, guess = 0;
	std::string resultLine, resultDetail;
	TrainingUI::Stepper stepper;
	SDL_FRect backButton(){ return TrainingUI::backRect(); }

	std::string statsLine() const{
		return "DECKS RIGHT " + std::to_string(trainer->deckEstimatesRight) + " OF " + std::to_string(trainer->deckEstimates)
			+ "    TRUE COUNT RIGHT " + std::to_string(trainer->trueCountRight) + " OF " + std::to_string(trainer->trueCountQuestions);
	}

	// Landscape (desktop): the question on top, the stepper in one row.
	void drawWide(SDLState& state){
		using namespace TrainingUI;
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
		// The discard tray fills the middle when estimating decks.
		if(answered){
			text(state, resultLine, estimating ? 545.0f : 395.0f, 5.5f, resultOk ? GREEN : RED);
			text(state, resultDetail, estimating ? 585.0f : 545.0f, 3.4f, GOLD);
		}
		text(state, statsLine(), estimating ? 612.0f : 600.0f, 3.2f, DIM);
		button(state, backButton(), SDL_Color{80, 80, 80, 255}, "BACK", 6.0f);
	}

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
		bool tall = TrainingUI::portrait;
		const float trayX = tall ? 190.0f : 560.0f, trayY = tall ? 230.0f : 110.0f;
		const float trayH = tall ? 620.0f : 300.0f, trayW = tall ? 150.0f : 120.0f;
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

		float refX = tall ? 400.0f : 760.0f;
		stack(refX, trayY + trayH - perDeck, perDeck);
		TrainingUI::text(state, "1 DECK", trayY + trayH + 14.0f, 3.2f, TrainingUI::DIM, refX + trayW / 2.0f - 5.0f);
	}
};

// ---------------------------------------------------------------------
// House edges and per-hand standard deviations measured with this game's
// own engine playing the published charts: main bets over 3-4 million
// rounds per game (dealer-hits: the average of 3 runs, each good to about
// +-0.05%), side bets over 20 million rounds each, flat betting.
namespace HouseEdge{
	inline constexpr float EDGE_6D_H17 = 0.0055f, EDGE_6D_S17 = 0.0032f;
	inline constexpr float EDGE_2D_H17 = 0.0055f, EDGE_2D_S17 = 0.0039f;
	inline constexpr float EDGE_8D_H17 = 0.0060f, EDGE_8D_S17 = 0.0028f;
	inline constexpr float EDGE_PE2_H17 = 0.0057f, EDGE_PE2_S17 = 0.0013f;
	inline constexpr float EDGE_PE6_H17 = 0.0068f, EDGE_PE6_S17 = 0.0033f;
	inline constexpr float EDGE_FREE_BET = 0.0109f;

	struct SideBet{ const char* name; float edge; float sd; };
	enum{ LL_2D, LL_6D, MATCH_UP_2D, MATCH_DOWN_2D, MATCH_UP_6D, MATCH_DOWN_6D, LUCKY_STIFF, PUSH_22, POT_OF_GOLD, SIDE_COUNT };
	inline const SideBet& sideBet(int i){
		static const SideBet S[SIDE_COUNT] = {
			{ "LUCKY LADIES", 0.256f, 4.57f }, { "LUCKY LADIES", 0.250f, 4.98f },
			{ "MATCH UP", 0.163f, 2.18f }, { "MATCH DOWN", 0.163f, 2.18f },
			{ "MATCH UP", 0.031f, 2.45f }, { "MATCH DOWN", 0.030f, 2.45f },
			{ "LUCKY STIFF", 0.107f, 2.25f }, { "PUSH 22", 0.059f, 4.20f }, { "POT OF GOLD", 0.087f, 3.97f },
		};
		return S[i];
	}
}

// ---------------------------------------------------------------------
// BANKROLL PLANNER: what a session at a chosen game, bet, side bet, pace
// and length is expected to cost, how widely it swings, and how much money
// to bring. The main bet and the side bet are treated as independent.
class BankrollMenu
{
public:
	struct Game{ const char* name; float edgeH17, edgeS17, sd; std::vector<int> sides; };
	static const std::vector<Game>& games(){
		using namespace HouseEdge;
		static const std::vector<Game> G = {
			{ "BLACKJACK 6 DECKS", EDGE_6D_H17, EDGE_6D_S17, 1.15f, { LL_6D } },
			{ "BLACKJACK 2 DECKS", EDGE_2D_H17, EDGE_2D_S17, 1.15f, { LL_2D } },
			{ "LUCKY STIFF 8 DECKS", EDGE_8D_H17, EDGE_8D_S17, 1.15f, { LUCKY_STIFF } },
			{ "PLAYERS EDGE 2 DECKS", EDGE_PE2_H17, EDGE_PE2_S17, 1.16f, { MATCH_UP_2D, MATCH_DOWN_2D } },
			{ "PLAYERS EDGE 6 DECKS", EDGE_PE6_H17, EDGE_PE6_S17, 1.16f, { MATCH_UP_6D, MATCH_DOWN_6D } },
			{ "FREE BET 6 DECKS", EDGE_FREE_BET, EDGE_FREE_BET, 1.07f, { PUSH_22, POT_OF_GOLD } },
		};
		return G;
	}

	void draw(SDLState& state){
		using namespace TrainingUI;
		clear(state);
		text(state, "BANKROLL PLANNER", 20.0f, 6.0f, WHITE);
		text(state, "PERFECT BASIC STRATEGY ON THE MAIN BET, FLAT BETTING, SIDE BET EVERY HAND", 60.0f, 2.8f, DIM);
		const Game& g = games()[game];
		bool freeBet = g.edgeH17 == g.edgeS17;
		SDL_Color on{60, 90, 150, 255}, fixed{55, 60, 58, 255};
		button(state, gameButton, SDL_Color{150, 120, 40, 255}, std::string("GAME: ") + g.name, 4.0f);
		button(state, soft17Button, freeBet ? fixed : on, std::string("SOFT 17: ") + (freeBet || hitsSoft17 ? "HITS" : "STANDS"), 4.0f);
		std::string sideLabel = "SIDE BET: NONE";
		if(side > 0){
			const HouseEdge::SideBet& sb = HouseEdge::sideBet(g.sides[side - 1]);
			char edge[16];
			std::snprintf(edge, sizeof(edge), "%.1f%%", sb.edge * 100.0f);
			sideLabel = std::string("SIDE BET: ") + sb.name + " - " + edge + " EDGE";
		}
		button(state, sideButton, SDL_Color{120, 70, 140, 255}, sideLabel, 4.0f);
		drawStepper(state, 0, "BET", "$" + std::to_string(BETS[betIndex]));
		drawStepper(state, 1, "SIDE BET", side > 0 ? "$" + std::to_string(SIDE_BETS[sideIndex]) : "-");
		drawStepper(state, 2, "HANDS PER HOUR", std::to_string(handsPerHour));
		drawStepper(state, 3, "HOURS", std::to_string(hours));

		long hands = (long)handsPerHour * hours;
		double bet = BETS[betIndex], sideBet = side > 0 ? SIDE_BETS[sideIndex] : 0.0;
		double edge = freeBet || hitsSoft17 ? g.edgeH17 : g.edgeS17;
		double mainMean = -edge * bet * hands, sideMean = 0.0, var = g.sd * g.sd * bet * bet * hands;
		if(side > 0){
			const HouseEdge::SideBet& sb = HouseEdge::sideBet(g.sides[side - 1]);
			sideMean = -sb.edge * sideBet * hands;
			var += sb.sd * sb.sd * sideBet * sideBet * hands;
		}
		double mean = mainMean + sideMean, sd = std::sqrt(var);
		auto money = [](double v){
			long r = std::lround(std::fabs(v));
			return std::string(v < -0.5 ? "-$" : "$") + std::to_string(r);
		};
		auto plusMoney = [&](double v){ return v >= 0.5 ? "+" + money(v) : money(v); };
		float y = 422.0f;
		auto row = [&](const std::string& label, const std::string& value, SDL_Color c){
			DigitFont::drawText(state, label, 180.0f, y, 3.4f, GOLD);
			DigitFont::drawText(state, value, 1260.0f - DigitFont::textWidth(value, 4.0f), y - 2.0f, 4.0f, c);
			y += 30.0f;
		};
		row("YOUR TOTAL ACTION, " + std::to_string(hands) + " HANDS", "$" + std::to_string((long)((bet + sideBet) * hands)), WHITE);
		row("EXPECTED RESULT", plusMoney(mean), RED);
		if(side > 0)
			row("OF THAT, MAIN BET / SIDE BET", plusMoney(mainMean) + " / " + plusMoney(sideMean), RED);
		row("2 IN 3 SESSIONS END BETWEEN", plusMoney(mean - sd) + " AND " + plusMoney(mean + sd), WHITE);
		row("1 IN 20 SESSIONS LOSE AT LEAST", money(-(mean - 1.645 * sd)), RED);
		row("BRING THIS TO BE 95% SURE NOT TO GO BROKE", money(bankrollFor(0.05, mean, sd, hands)), GREEN);
		row("BRING THIS TO BE 99% SURE", money(bankrollFor(0.01, mean, sd, hands)), GREEN);
		button(state, backButton, SDL_Color{80, 80, 80, 255}, "BACK", 6.0f);
	}

	bool handlePoint(SDLState& state, float wx, float wy){
		SDL_FPoint p;
		if(!TrainingUI::toLogical(state, wx, wy, p))
			return false;
		if(TrainingUI::hit(p, backButton))
			return true;
		if(TrainingUI::hit(p, gameButton)){
			game = (game + 1) % (int)games().size();
			side = 0;
		}
		if(TrainingUI::hit(p, soft17Button))
			hitsSoft17 = !hitsSoft17;
		if(TrainingUI::hit(p, sideButton))
			side = (side + 1) % ((int)games()[game].sides.size() + 1);
		for(int i = 0; i < 4; i++){
			if(TrainingUI::hit(p, minusRect(i))) step(i, -1);
			if(TrainingUI::hit(p, plusRect(i))) step(i, 1);
		}
		return false;
	}

	std::vector<SDL_FRect> focusRects(){
		std::vector<SDL_FRect> r{ gameButton, soft17Button, sideButton };
		for(int i = 0; i < 4; i++){
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
	static constexpr int SIDE_BETS[8] = { 1, 2, 5, 10, 15, 25, 50, 100 };
	int game = 0, side = 0, betIndex = 3, sideIndex = 2, handsPerHour = 70, hours = 3;
	bool hitsSoft17 = true;

	SDL_FRect gameButton{ .x = 300, .y = 84, .w = 560, .h = 50 };
	SDL_FRect soft17Button{ .x = 880, .y = 84, .w = 260, .h = 50 };
	SDL_FRect sideButton{ .x = 300, .y = 144, .w = 840, .h = 50 };
	SDL_FRect backButton{ .x = 620, .y = 640, .w = 200, .h = 56 };

	SDL_FRect minusRect(int i){ return SDL_FRect{ .x = 760, .y = 208.0f + i * 50.0f, .w = 60, .h = 44 }; }
	SDL_FRect plusRect(int i){ return SDL_FRect{ .x = 1000, .y = 208.0f + i * 50.0f, .w = 60, .h = 44 }; }

	void drawStepper(SDLState& state, int i, const std::string& label, const std::string& value){
		using namespace TrainingUI;
		DigitFont::drawText(state, label, 380.0f, minusRect(i).y + 12.0f, 4.0f, WHITE);
		button(state, minusRect(i), SDL_Color{70, 70, 80, 255}, "-", 5.0f);
		button(state, plusRect(i), SDL_Color{70, 70, 80, 255}, "+", 5.0f);
		text(state, value, minusRect(i).y + 10.0f, 5.0f, SDL_Color{255, 225, 80, 255}, 910.0f);
	}

	void step(int i, int d){
		if(i == 0) betIndex = std::clamp(betIndex + d, 0, 9);
		else if(i == 1) sideIndex = std::clamp(sideIndex + d, 0, 7);
		else if(i == 2) handsPerHour = std::clamp(handsPerHour + 10 * d, 30, 150);
		else hours = std::clamp(hours + d, 1, 24);
	}
};

// ---------------------------------------------------------------------
// GAME ODDS: every game in the app (and its soft-17 variant) and every
// side bet, ranked by house edge, with what each costs per hour at a
// chosen bet -- where a bankroll lasts longest. The numbers were measured
// with this game's own engine playing the published charts (3-4 million
// rounds per game) and, for Lucky Ladies and Match the Dealer, exact math.
class OddsMenu
{
public:
	// frequency: the share of hands the bet is offered on (insurance only
	// against an ace), for its cost per hour.
	struct Row{ const char* name; const char* rules; float edge; float sd; float frequency = 1.0f; };

	static const std::vector<Row>& mainGames(){
		static const std::vector<Row> rows = sortedByEdge({
			{ "BLACKJACK 6 DECKS", "DEALER STANDS SOFT 17 - LUCKY LADIES 6D TOO", HouseEdge::EDGE_6D_S17, 1.15f },
			{ "BLACKJACK 6 DECKS", "DEALER HITS SOFT 17 - LUCKY LADIES 6D TOO", HouseEdge::EDGE_6D_H17, 1.15f },
			{ "BLACKJACK 2 DECKS", "DEALER STANDS SOFT 17 - LUCKY LADIES 2D TOO", HouseEdge::EDGE_2D_S17, 1.15f },
			{ "BLACKJACK 2 DECKS", "DEALER HITS SOFT 17 - LUCKY LADIES 2D TOO", HouseEdge::EDGE_2D_H17, 1.15f },
			{ "LUCKY STIFF 8 DECKS", "DEALER STANDS SOFT 17", HouseEdge::EDGE_8D_S17, 1.15f },
			{ "LUCKY STIFF 8 DECKS", "DEALER HITS SOFT 17", HouseEdge::EDGE_8D_H17, 1.15f },
			{ "PLAYERS EDGE 2 DECKS", "DEALER STANDS SOFT 17", HouseEdge::EDGE_PE2_S17, 1.16f },
			{ "PLAYERS EDGE 2 DECKS", "DEALER HITS SOFT 17", HouseEdge::EDGE_PE2_H17, 1.16f },
			{ "PLAYERS EDGE 6 DECKS", "DEALER STANDS SOFT 17", HouseEdge::EDGE_PE6_S17, 1.16f },
			{ "PLAYERS EDGE 6 DECKS", "DEALER HITS SOFT 17", HouseEdge::EDGE_PE6_H17, 1.16f },
			{ "FREE BET 6 DECKS", "DEALER ALWAYS HITS SOFT 17", HouseEdge::EDGE_FREE_BET, 1.07f },
		});
		return rows;
	}

	static const std::vector<Row>& sideBets(){
		static const std::vector<Row> rows = sortedByEdge({
			{ "MATCH UP OR DOWN 6 DECKS", "COUNT DOES NOT HELP - NEVER PAYS, BUT CHEAPEST", 0.031f, 2.45f },
			{ "PUSH 22", "NEVER PAYS - LEAST BAD AT LOW COUNTS", 0.059f, 4.20f },
			{ "INSURANCE, NOT COUNTING", "PAYS AT TRUE COUNT +3 OR MORE - VS AN ACE ONLY", 0.074f, 0.0f, 0.5f / 13.0f },
			{ "POT OF GOLD", "PAYS AT TRUE COUNT -4 OR LOWER, ABOUT +9%", 0.087f, 3.97f },
			{ "LUCKY STIFF", "NEVER PAYS - WORSE AT HIGH COUNTS", 0.107f, 2.25f },
			{ "MATCH UP OR DOWN 2 DECKS", "COUNT DOES NOT HELP - SKIP IT", 0.163f, 2.18f },
			{ "LUCKY LADIES 6 DECKS", "PAYS AT TRUE COUNT +8 OR MORE, ABOUT +11%", 0.250f, 4.98f },
			{ "LUCKY LADIES 2 DECKS", "PAYS AT TRUE COUNT +7 OR MORE, +3 TO +24%", 0.256f, 4.57f },
		});
		return rows;
	}

	void draw(SDLState& state){
		using namespace TrainingUI;
		clear(state);
		text(state, showingSideBets ? "GAME ODDS - SIDE BETS" : "GAME ODDS - MAIN GAMES", 18.0f, 6.0f, WHITE);
		const std::vector<Row>& rows = showingSideBets ? sideBets() : mainGames();
		int bet = BETS[betIndex];
		float hands = 70.0f;

		std::string intro = showingSideBets
			? "WHAT EACH SIDE BET KEEPS OF EVERY DOLLAR, AND ITS COST PER HOUR AT $" + std::to_string(bet) + " A HAND"
			: "PERFECT BASIC STRATEGY, MAIN BET ONLY. COST PER HOUR AT $" + std::to_string(bet) + " A HAND, 70 HANDS AN HOUR";
		text(state, intro, 62.0f, 3.0f, DIM);

		float maxEdge = 0.0f;
		for(const Row& r : rows) maxEdge = std::max(maxEdge, r.edge);
		static const float X_NAME = 40.0f, X_BAR = 720.0f, BAR_W = 260.0f, X_EDGE = 1120.0f, X_COST = 1400.0f;
		DigitFont::drawText(state, "GAME", X_NAME, 92.0f, 3.0f, GOLD);
		DigitFont::drawText(state, "HOUSE EDGE", X_EDGE - DigitFont::textWidth("HOUSE EDGE", 3.0f), 92.0f, 3.0f, GOLD);
		DigitFont::drawText(state, "COST PER HOUR", X_COST - DigitFont::textWidth("COST PER HOUR", 3.0f), 92.0f, 3.0f, GOLD);

		float rowH = showingSideBets ? 52.0f : 40.0f;
		float y = 118.0f;
		for(size_t i = 0; i < rows.size(); i++){
			const Row& r = rows[i];
			// Rank, best first.
			SDL_Color c = rankColor((float)i / std::max<size_t>(1, rows.size() - 1));
			DigitFont::drawText(state, std::to_string(i + 1) + ". " + r.name, X_NAME, y, 3.0f, WHITE);
			float rulesPixel = std::min(2.2f, 2.2f * (X_BAR - X_NAME - 56.0f) / std::max(1.0f, DigitFont::textWidth(r.rules, 2.2f)));
			DigitFont::drawText(state, r.rules, X_NAME + 36.0f, y + 19.0f, rulesPixel, DIM);

			float w = BAR_W * r.edge / maxEdge;
			SDL_FRect bar{ .x = X_BAR, .y = y + 4.0f, .w = std::max(3.0f, w), .h = 18.0f };
			SDL_SetRenderDrawColor(state.renderer, c.r, c.g, c.b, 255);
			SDL_RenderFillRect(state.renderer, &bar);

			char edge[16];
			std::snprintf(edge, sizeof(edge), r.edge < 0.1f ? "%.2f%%" : "%.0f%%", r.edge * 100.0f);
			DigitFont::drawText(state, edge, X_EDGE - DigitFont::textWidth(edge, 3.6f), y + 2.0f, 3.6f, c);
			std::string cost = "$" + std::to_string((int)std::lround(r.edge * bet * hands * r.frequency)) + "/HR";
			DigitFont::drawText(state, cost, X_COST - DigitFont::textWidth(cost, 3.6f), y + 2.0f, 3.6f, WHITE);
			y += rowH;
		}

		std::string tip = showingSideBets
			? "FLAT BET, EVERY SIDE BET LOSES. COUNTERS: ONLY AT THE COUNTS SHOWN, OTHERWISE SKIP IT"
			: "LOWER IS BETTER. NO GAME BEATS THE HOUSE WITH BASIC STRATEGY ALONE - COUNTING CAN";
		text(state, tip, 572.0f, 3.0f, GOLD);

		button(state, minusButton, SDL_Color{70, 70, 80, 255}, "-", 5.0f);
		text(state, "$" + std::to_string(bet), 610.0f, 4.6f, SDL_Color{255, 225, 80, 255}, 330.0f);
		button(state, plusButton, SDL_Color{70, 70, 80, 255}, "+", 5.0f);
		button(state, pageButton, SDL_Color{60, 90, 150, 255}, showingSideBets ? "MAIN GAMES" : "SIDE BETS", 5.0f);
		button(state, backButton, SDL_Color{80, 80, 80, 255}, "BACK", 6.0f);
	}

	bool handlePoint(SDLState& state, float wx, float wy){
		SDL_FPoint p;
		if(!TrainingUI::toLogical(state, wx, wy, p))
			return false;
		if(TrainingUI::hit(p, backButton))
			return true;
		if(TrainingUI::hit(p, pageButton))
			showingSideBets = !showingSideBets;
		if(TrainingUI::hit(p, minusButton))
			betIndex = std::max(0, betIndex - 1);
		if(TrainingUI::hit(p, plusButton))
			betIndex = std::min(9, betIndex + 1);
		return false;
	}

	std::vector<SDL_FRect> focusRects(){ return { minusButton, plusButton, pageButton, backButton }; }

private:
	static constexpr int BETS[10] = { 5, 10, 15, 25, 50, 75, 100, 200, 300, 500 };
	int betIndex = 3;
	bool showingSideBets = false;

	SDL_FRect minusButton{ .x = 230, .y = 600, .w = 50, .h = 46 };
	SDL_FRect plusButton{ .x = 380, .y = 600, .w = 50, .h = 46 };
	SDL_FRect pageButton{ .x = 460, .y = 600, .w = 260, .h = 46 };
	SDL_FRect backButton{ .x = 760, .y = 595, .w = 200, .h = 56 };

	static std::vector<Row> sortedByEdge(std::vector<Row> rows){
		std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b){ return a.edge < b.edge; });
		return rows;
	}

	// Green for the best, through gold, to red for the worst.
	static SDL_Color rankColor(float t){
		if(t < 0.5f){
			float u = t / 0.5f;
			return SDL_Color{ (Uint8)(90 + u * 140), (Uint8)(210 - u * 20), (Uint8)(100 - u * 40), 255 };
		}
		float u = (t - 0.5f) / 0.5f;
		return SDL_Color{ (Uint8)(230 + u * 15), (Uint8)(190 - u * 90), (Uint8)(60 + u * 20), 255 };
	}
};
