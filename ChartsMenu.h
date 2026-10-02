#pragma once
#include "Game.h"
#include "DigitFont.h"
#include "StrategyChart.h"
#include <string>
#include <vector>

// The home page's CHARTS screen: any basic-strategy chart, for whatever
// rules the table you're about to sit at has. The buttons under the
// chart pick the game (standard, Spanish 21, Free Bet), the number of
// decks, whether the dealer hits soft 17, double after split, and
// surrender; each tap steps that rule. Rules that don't vary for a game
// (Spanish 21's deck count, Free Bet's soft 17) are shown greyed.
// handlePoint() returns true for BACK.
class ChartsMenu
{
public:
	// Starts on the chart for the game last set up.
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

	void draw(SDLState& state){
		StrategyChart::drawChart(state, rules(), false, 0, 0, 0, true);

		static const char* GAMES[3] = { "STANDARD", "SPANISH 21", "FREE BET" };
		static const char* DECKS[3] = { "1", "2", "4-8" };
		SDL_Color on{60, 90, 150, 255}, fixed{55, 60, 58, 255};
		StrategyChart::drawButton(state, button(0), SDL_Color{150, 120, 40, 255}, std::string("GAME: ") + GAMES[game], 3.6f);
		StrategyChart::drawButton(state, button(1), game == STANDARD ? on : fixed,
			std::string("DECKS: ") + (game == STANDARD ? DECKS[decks] : game == SPANISH ? "6-8" : "6"), 3.6f);
		StrategyChart::drawButton(state, button(2), game != FREE_BET ? on : fixed,
			std::string("SOFT 17: ") + (game == FREE_BET || hitsSoft17 ? "HITS" : "STANDS"), 3.6f);
		StrategyChart::drawButton(state, button(3), game == STANDARD ? on : fixed,
			std::string("DOUBLE AFTER SPLIT: ") + (game != STANDARD || das ? "YES" : "NO"), 3.6f);
		StrategyChart::drawButton(state, button(4), game != FREE_BET ? on : fixed,
			std::string("SURRENDER: ") + (game != FREE_BET && surrender ? "YES" : "NO"), 3.6f);
		StrategyChart::drawButton(state, backButton, SDL_Color{80, 80, 80, 255}, "BACK");
	}

	bool handlePoint(SDLState& state, float windowX, float windowY){
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			return false;
		SDL_FPoint p{x, y};
		if(SDL_PointInRectFloat(&p, &backButton))
			return true;

		for(int i = 0; i < 5; i++){
			SDL_FRect b = button(i);
			if(!SDL_PointInRectFloat(&p, &b))
				continue;
			switch(i){
				case 0: game = (game + 1) % 3; break;
				case 1: if(game == STANDARD) decks = (decks + 1) % 3; break;
				case 2: if(game != FREE_BET) hitsSoft17 = !hitsSoft17; break;
				case 3: if(game == STANDARD) das = !das; break;
				case 4: if(game != FREE_BET) surrender = !surrender; break;
			}
		}
		return false;
	}

	std::vector<SDL_FRect> focusRects(){
		std::vector<SDL_FRect> rects;
		for(int i = 0; i < 5; i++)
			rects.push_back(button(i));
		rects.push_back(backButton);
		return rects;
	}

private:
	enum{ STANDARD, SPANISH, FREE_BET };
	enum{ ONE, TWO, SHOE };
	int game = STANDARD;
	int decks = SHOE;
	bool hitsSoft17 = true;
	bool das = true;
	bool surrender = true;

	// The 5 rule buttons in a row under the chart; BACK under them.
	SDL_FRect button(int i){
		static const float W[5] = { 250.0f, 190.0f, 260.0f, 360.0f, 260.0f };
		float gap = 14.0f, total = 4 * gap;
		for(float w : W) total += w;
		float x = (1440.0f - total) / 2.0f;
		for(int k = 0; k < i; k++)
			x += W[k] + gap;
		return SDL_FRect{ .x = x, .y = 578.0f, .w = W[i], .h = 48.0f };
	}

	SDL_FRect backButton{ .x = 620, .y = 640, .w = 200, .h = 56 };
};
