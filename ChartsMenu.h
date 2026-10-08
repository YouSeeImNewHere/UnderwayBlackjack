#pragma once
#include "Game.h"
#include "DigitFont.h"
#include "StrategyChart.h"
#include <string>
#include <vector>

// A row of buttons picking a strategy chart's rules: the game (standard,
// Spanish 21, Free Bet), the number of decks, whether the dealer hits soft
// 17, double after split, and surrender -- each tap steps that rule. Rules
// that don't vary for a game (Spanish 21's decks, Free Bet's soft 17) are
// greyed. Optionally a sixth button for INDEX PLAYS (counting deviations,
// standard games only). Used by CHARTS, the strategy drill and the review.
struct RuleSelector
{
	enum{ STANDARD, SPANISH, FREE_BET };
	enum{ ONE, TWO, SHOE };
	int game = STANDARD;
	int decks = SHOE;
	bool hitsSoft17 = true;
	bool das = true;
	bool surrender = true;
	bool indexPlays = false;
	// Portrait screens lay the buttons out 2 across instead of one row.
	bool grid = false;

	// Starts on the chart for a game.
	void open(GameMode mode, bool dealerHitsSoft17){
		StrategyChart::Rules r = StrategyChart::rulesFor(mode, dealerHitsSoft17);
		if(StrategyChart::isSpanish(r.set)) game = SPANISH;
		else if(r.set == StrategyChart::FREE_BET) game = FREE_BET;
		else game = STANDARD;
		decks = deckCountFor(mode) <= 2 ? TWO : SHOE;
		hitsSoft17 = dealerHitsSoft17;
		das = r.das;
		surrender = true;
	}

	StrategyChart::Rules rules() const{
		StrategyChart::Rules r;
		if(game == SPANISH){
			r.set = hitsSoft17 ? StrategyChart::SPANISH_H17 : StrategyChart::SPANISH_S17;
			r.das = true;
			r.surrender = surrender;
		} else if(game == FREE_BET){
			r.set = StrategyChart::FREE_BET;
			r.das = true;
			r.surrender = false;
		} else{
			static const int H17[3] = { StrategyChart::SINGLE_DECK_H17, StrategyChart::DOUBLE_DECK_H17, StrategyChart::SHOE_H17 };
			static const int S17[3] = { StrategyChart::SINGLE_DECK_S17, StrategyChart::DOUBLE_DECK_S17, StrategyChart::SHOE_S17 };
			r.set = hitsSoft17 ? H17[decks] : S17[decks];
			r.das = das;
			r.surrender = surrender;
		}
		return r;
	}

	bool usesIndexPlays() const{ return indexPlays && game == STANDARD; }
	bool dealerHitsSoft17() const{ return game == FREE_BET || hitsSoft17; }

	void draw(SDLState& state, float y, bool withIndexButton){
		static const char* GAMES[3] = { "STANDARD", "SPANISH 21", "FREE BET" };
		static const char* DECKS[3] = { "1", "2", "4-8" };
		SDL_Color on{60, 90, 150, 255}, fixed{55, 60, 58, 255};
		int n = withIndexButton ? 6 : 5;
		float p = 3.4f;
		StrategyChart::drawButton(state, button(0, y, n), SDL_Color{150, 120, 40, 255}, std::string("GAME: ") + GAMES[game], p);
		StrategyChart::drawButton(state, button(1, y, n), game == STANDARD ? on : fixed,
			std::string("DECKS: ") + (game == STANDARD ? DECKS[decks] : game == SPANISH ? "6-8" : "6"), p);
		StrategyChart::drawButton(state, button(2, y, n), game != FREE_BET ? on : fixed,
			std::string("SOFT 17: ") + (dealerHitsSoft17() ? "HITS" : "STANDS"), p);
		StrategyChart::drawButton(state, button(3, y, n), game == STANDARD ? on : fixed,
			std::string("DOUBLE AFTER SPLIT: ") + (game != STANDARD || das ? "YES" : "NO"), p);
		StrategyChart::drawButton(state, button(4, y, n), game != FREE_BET ? on : fixed,
			std::string("SURRENDER: ") + (game != FREE_BET && surrender ? "YES" : "NO"), p);
		if(withIndexButton)
			StrategyChart::drawButton(state, button(5, y, n), game == STANDARD ? (indexPlays ? SDL_Color{60, 130, 70, 255} : on) : fixed,
				std::string("INDEX PLAYS: ") + (usesIndexPlays() ? "ON" : "OFF"), p);
	}

	// True when a tap changed a rule.
	bool handle(SDL_FPoint p, float y, bool withIndexButton){
		int n = withIndexButton ? 6 : 5;
		for(int i = 0; i < n; i++){
			SDL_FRect b = button(i, y, n);
			if(!SDL_PointInRectFloat(&p, &b))
				continue;
			switch(i){
				case 0: game = (game + 1) % 3; break;
				case 1: if(game == STANDARD) decks = (decks + 1) % 3; break;
				case 2: if(game != FREE_BET) hitsSoft17 = !hitsSoft17; break;
				case 3: if(game == STANDARD) das = !das; break;
				case 4: if(game != FREE_BET) surrender = !surrender; break;
				case 5: if(game == STANDARD) indexPlays = !indexPlays; break;
			}
			return true;
		}
		return false;
	}

	void addRects(std::vector<SDL_FRect>& rects, float y, bool withIndexButton){
		int n = withIndexButton ? 6 : 5;
		for(int i = 0; i < n; i++)
			rects.push_back(button(i, y, n));
	}

	SDL_FRect button(int i, float y, int n){
		if(grid)
			return SDL_FRect{ .x = 20.0f + (i % 2) * 350.0f, .y = y + (i / 2) * 62.0f, .w = 330.0f, .h = 54.0f };
		static const float W5[5] = { 250.0f, 190.0f, 260.0f, 360.0f, 260.0f };
		static const float W6[6] = { 220.0f, 160.0f, 210.0f, 300.0f, 220.0f, 220.0f };
		const float* W = n == 6 ? W6 : W5;
		float gap = n == 6 ? 10.0f : 14.0f, total = (n - 1) * gap;
		for(int k = 0; k < n; k++) total += W[k];
		float x = (1440.0f - total) / 2.0f;
		for(int k = 0; k < i; k++)
			x += W[k] + gap;
		return SDL_FRect{ .x = x, .y = y, .w = W[i], .h = 46.0f };
	}
};

// The home page's CHARTS screen: any basic-strategy chart, for whatever
// rules the table you're about to sit at has (RuleSelector under it).
// handlePoint() returns true for BACK.
class ChartsMenu
{
public:
	// Starts on the chart for the game last set up.
	void open(GameMode mode, bool dealerHitsSoft17){
		selector.open(mode, dealerHitsSoft17);
	}

	void draw(SDLState& state){
		StrategyChart::drawChart(state, selector.rules(), false, 0, 0, 0, true);
		selector.draw(state, RULES_Y, false);
		StrategyChart::drawButton(state, backButton, SDL_Color{80, 80, 80, 255}, "BACK");
	}

	bool handlePoint(SDLState& state, float windowX, float windowY){
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			return false;
		SDL_FPoint p{x, y};
		if(SDL_PointInRectFloat(&p, &backButton))
			return true;
		selector.handle(p, RULES_Y, false);
		return false;
	}

	std::vector<SDL_FRect> focusRects(){
		std::vector<SDL_FRect> rects;
		selector.addRects(rects, RULES_Y, false);
		rects.push_back(backButton);
		return rects;
	}

private:
	static constexpr float RULES_Y = 580.0f;
	RuleSelector selector;
	SDL_FRect backButton{ .x = 620, .y = 640, .w = 200, .h = 56 };
};
