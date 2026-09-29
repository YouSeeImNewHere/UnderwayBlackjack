#pragma once
#include "Game.h"
#include "DigitFont.h"
#include "Stats.h"
#include <string>
#include <vector>

// Lifetime stats page (Stats.h), opened from the main menu and the pause
// menu. Same shape as GesturesMenu/KeyboardMenu: draw() plus handlePoint()
// returning true for BACK. RESET wipes the stats, but only on a second
// press -- the first turns it into "SURE?" -- so a stray tap can't.
class StatsMenu
{
public:
	void draw(SDLState& state, Resources& res, const Stats& stats){
		SDL_SetRenderDrawColor(state.renderer, 10, 30, 15, 255);
		SDL_RenderFillRect(state.renderer, nullptr);

		float titlePixel = 7.0f;
		std::string title = "STATS";
		float titleW = DigitFont::textWidth(title, titlePixel);
		DigitFont::drawText(state, title, (1440.0f - titleW) / 2.0f, 40.0f, titlePixel, SDL_Color{255, 255, 255, 255});

		long long hands = stats.get(Stats::HandsPlayed);
		long long wins = stats.get(Stats::Wins), losses = stats.get(Stats::Losses);
		long long decisions = stats.get(Stats::DecisionsTotal);

		std::vector<std::pair<std::string, std::string>> left{
			{"HANDS PLAYED", num(hands)},
			{"WINS", num(wins)},
			{"LOSSES", num(losses)},
			{"PUSHES", num(stats.get(Stats::Pushes))},
			{"WIN RATE", wins + losses > 0 ? percent(wins, wins + losses) : "-"},
			{"BLACKJACKS", num(stats.get(Stats::Blackjacks))},
			{"BUSTS", num(stats.get(Stats::Busts))},
			{"SURRENDERS", num(stats.get(Stats::Surrenders))},
			{"DOUBLES", num(stats.get(Stats::Doubles))},
		};
		std::vector<std::pair<std::string, std::string>> right{
			{"SPLITS", num(stats.get(Stats::Splits))},
			{"NET WINNINGS", signedNum(stats.get(Stats::NetWinnings))},
			{"BIGGEST WIN", num(stats.get(Stats::BiggestWin))},
			{"WIN STREAK", num(stats.get(Stats::CurrentStreak))},
			{"BEST STREAK", num(stats.get(Stats::BestStreak))},
			{"RARE SIDE BETS", num(stats.get(Stats::JackpotHits))},
			{"INSURANCE TAKEN", num(stats.get(Stats::InsuranceTaken))},
			{"CHART MOVES", decisions > 0 ? percent(stats.get(Stats::DecisionsCorrect), decisions) : "-"},
			{" OF DECISIONS", num(decisions)},
		};

		drawColumn(state, left, 150.0f);
		drawColumn(state, right, 780.0f);

		drawButton(state, backButton, SDL_Color{80, 80, 80, 255}, "BACK");
		drawButton(state, resetButton, confirmingReset ? SDL_Color{170, 50, 50, 255} : SDL_Color{110, 60, 60, 255},
			confirmingReset ? "SURE?" : "RESET");
	}

	// Returns true for BACK. RESET is handled here (first press arms it,
	// second wipes stats -- the caller saves them afterward).
	bool handlePoint(SDLState& state, float windowX, float windowY, Stats& stats, bool& statsChanged){
		statsChanged = false;
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			return false;

		SDL_FPoint p{x, y};
		if(SDL_PointInRectFloat(&p, &backButton)){
			confirmingReset = false;
			return true;
		}
		if(SDL_PointInRectFloat(&p, &resetButton)){
			if(confirmingReset){
				stats.reset();
				statsChanged = true;
				confirmingReset = false;
			} else{
				confirmingReset = true;
			}
		}
		return false;
	}

	// Leaving the page disarms RESET.
	void onLeave(){ confirmingReset = false; }

	std::vector<SDL_FRect> focusRects(){
		return { backButton, resetButton };
	}

private:
	bool confirmingReset = false;

	SDL_FRect backButton{ .x = 500, .y = 630, .w = 200, .h = 56 };
	SDL_FRect resetButton{ .x = 740, .y = 630, .w = 200, .h = 56 };

	static std::string num(long long v){ return std::to_string(v); }
	static std::string signedNum(long long v){ return v > 0 ? "+" + std::to_string(v) : std::to_string(v); }
	static std::string percent(long long part, long long whole){
		return std::to_string((int)((part * 100 + whole / 2) / whole)) + "%";
	}

	void drawColumn(SDLState& state, const std::vector<std::pair<std::string, std::string>>& rows, float x){
		float y = 130.0f;
		for(const auto& [label, value] : rows){
			DigitFont::drawText(state, label, x, y, 4.0f, SDL_Color{200, 180, 100, 255});
			float vW = DigitFont::textWidth(value, 5.0f);
			DigitFont::drawText(state, value, x + 500.0f - vW, y - 2.0f, 5.0f, SDL_Color{235, 235, 235, 255});
			y += 52.0f;
		}
	}

	void drawButton(SDLState& state, const SDL_FRect& rect, SDL_Color color, const std::string& label){
		SDL_SetRenderDrawColor(state.renderer, color.r, color.g, color.b, color.a);
		SDL_RenderFillRect(state.renderer, &rect);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &rect);

		float pixel = 6.0f;
		float w = DigitFont::textWidth(label, pixel);
		DigitFont::drawText(state, label, rect.x + (rect.w - w) / 2.0f, rect.y + (rect.h - 5 * pixel) / 2.0f, pixel, SDL_Color{255, 255, 255, 255});
	}
};
