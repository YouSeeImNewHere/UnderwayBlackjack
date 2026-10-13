#pragma once
#include "Game.h"
#include "DigitFont.h"
#include "UI.h"
#include "StrategyChart.h"
#include "Trainer.h"
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
		// Changeable rules in the choice colour, fixed ones dark; every
		// label at one size, the largest that fits all of them.
		SDL_Color on = UI::NAV, fixed = UI::PANEL;
		int n = withIndexButton ? 6 : 5;
		std::string labels[6] = {
			GAMES[game],
			std::string("DECKS: ") + (game == STANDARD ? DECKS[decks] : game == SPANISH ? "6-8" : "6"),
			std::string("SOFT 17: ") + (dealerHitsSoft17() ? "HITS" : "STANDS"),
			std::string("DAS: ") + (game != STANDARD || das ? "YES" : "NO"),
			std::string("SURRENDER: ") + (game != FREE_BET && surrender ? "YES" : "NO"),
			std::string("INDEX: ") + (usesIndexPlays() ? "ON" : "OFF") };
		SDL_Color fills[6] = { on, game == STANDARD ? on : fixed, game != FREE_BET ? on : fixed,
			game == STANDARD ? on : fixed, game != FREE_BET ? on : fixed,
			game == STANDARD ? (indexPlays ? UI::PRIMARY : on) : fixed };
		float size = grid ? UI::BODY : UI::SMALL;
		for(int k = 0; k < n; k++)
			size = std::min(size, UI::fit(labels[k], size, button(k, y, n).w - 16.0f));
		for(int k = 0; k < n; k++)
			UI::button(state, button(k, y, n), fills[k], labels[k], size);
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
		// Each slot fits its longest value at the small size (SPANISH 21,
		// DECKS: 4-8, SOFT 17: STANDS, DAS: YES, SURRENDER: YES,
		// INDEX: OFF), so nothing shifts when a rule changes.
		static const char* LONGEST[6] = { "SPANISH 21", "DECKS: 4-8", "SOFT 17: STANDS", "DAS: YES", "SURRENDER: YES", "INDEX: OFF" };
		float W[6];
		for(int k = 0; k < 6; k++)
			W[k] = UI::width(LONGEST[k], UI::SMALL) + 22.0f;
		float gap = 10.0f, total = (n - 1) * gap;
		for(int k = 0; k < n; k++) total += W[k];
		float x = (1440.0f - total) / 2.0f;
		for(int k = 0; k < i; k++)
			x += W[k] + gap;
		return SDL_FRect{ .x = x, .y = y, .w = W[i], .h = 46.0f };
	}
};

// The home page's CHARTS screen: any basic-strategy chart, for whatever
// rules the table you're about to sit at has (RuleSelector under it).
// With INDEX PLAYS on (standard games), the count's deviations show on the
// cells and a DEVIATIONS button lists them. handlePoint() returns true for
// BACK.
class ChartsMenu
{
public:
	// Starts on the chart for the game last set up.
	void open(GameMode mode, bool dealerHitsSoft17, bool indexPlays = false){
		selector.open(mode, dealerHitsSoft17);
		selector.indexPlays = indexPlays;
		listing = false;
		detailSection = -1;
	}

	void draw(SDLState& state){
		StrategyChart::Rules rules = selector.rules();
		bool deviations = selector.usesIndexPlays();
		if(deviations && listing){
			StrategyChart::drawDeviationList(state, "INDEX PLAYS - " + StrategyChart::titleFor(rules), IndexPlays::deviationLines(rules), IndexPlays::rampLine());
		} else{
			std::vector<IndexPlays::Deviation> devs = deviations ? IndexPlays::deviations(rules) : std::vector<IndexPlays::Deviation>{};
			StrategyChart::Marks marks = [&](int section, int row, int col, std::string& when, char& move){
				for(const IndexPlays::Deviation& d : devs)
					if(d.section == section && d.row == row && d.col == col){
						when = IndexPlays::markWhen(d);
						move = d.move;
						return true;
					}
				return false;
			};
			StrategyChart::drawChart(state, rules, false, 0, 0, 0, true, nullptr, deviations ? &marks : nullptr);
			selector.draw(state, RULES_Y, true);
			if(detailSection >= 0)
				StrategyChart::drawCellDetail(state, rules, detailSection, detailRow, detailCol, deviations ? &marks : nullptr);
		}
		StrategyChart::drawButton(state, backRect(), UI::SECONDARY, "BACK");
		if(deviations)
			StrategyChart::drawButton(state, listButton, UI::NAV, listing ? "CHART" : "DEVIATIONS", 5.0f);
	}

	bool handlePoint(SDLState& state, float windowX, float windowY){
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			return false;
		SDL_FPoint p{x, y};
		// A tapped cell's explanation: any tap closes it.
		if(detailSection >= 0){
			detailSection = -1;
			return false;
		}
		if(!listing && StrategyChart::cellAt(p, detailSection, detailRow, detailCol))
			return false;
		detailSection = -1;
		SDL_FRect back = backRect();
		if(SDL_PointInRectFloat(&p, &back))
			return true;
		if(selector.usesIndexPlays() && SDL_PointInRectFloat(&p, &listButton)){
			listing = !listing;
			return false;
		}
		if(!listing)
			selector.handle(p, RULES_Y, true);
		if(!selector.usesIndexPlays())
			listing = false;
		return false;
	}

	std::vector<SDL_FRect> focusRects(){
		std::vector<SDL_FRect> rects;
		if(!listing)
			selector.addRects(rects, RULES_Y, true);
		rects.push_back(backRect());
		if(selector.usesIndexPlays())
			rects.push_back(listButton);
		return rects;
	}

private:
	static constexpr float RULES_Y = 580.0f;
	RuleSelector selector;
	bool listing = false;
	int detailSection = -1, detailRow = 0, detailCol = 0;
	SDL_FRect listButton{ .x = 730, .y = 640, .w = 260, .h = 56 };
	SDL_FRect backRect(){
		return selector.usesIndexPlays() ? SDL_FRect{ .x = 510, .y = 640, .w = 200, .h = 56 } : SDL_FRect{ .x = 620, .y = 640, .w = 200, .h = 56 };
	}
};
