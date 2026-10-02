#pragma once
#include "Game.h"
#include "DigitFont.h"
#include "Stats.h"
#include <string>
#include <vector>

// Lifetime stats page (Stats.h), opened from the main menu and the pause
// menu. Same shape as GesturesMenu/KeyboardMenu: draw() plus handlePoint()
// returning true for BACK. The < and > beside the title step through all
// games combined and each kind of game (Stats::Scope); for each, three
// pages -- the main stats, a table of side bets, and a graph of net
// winnings round by round -- stepped through with the page button. RESET wipes all the stats, but only on a
// second press -- the first turns it into "SURE?" -- so a stray tap can't.
class StatsMenu
{
public:
	// Which set opens first: all games from the main menu, the game being
	// played from the pause menu.
	void open(int startScope){
		scope = startScope;
		page = MainPage;
		confirmingReset = false;
	}

	void draw(SDLState& state, Resources& res, const Stats& stats){
		SDL_SetRenderDrawColor(state.renderer, 10, 30, 15, 255);
		SDL_RenderFillRect(state.renderer, nullptr);

		float titlePixel = 6.0f;
		static const char* TITLES[PageCount] = { "STATS - ", "SIDE BETS - ", "GRAPH - " };
		std::string title = std::string(TITLES[page]) + Stats::scopeName(scope);
		float titleW = DigitFont::textWidth(title, titlePixel);
		DigitFont::drawText(state, title, (1440.0f - titleW) / 2.0f, 45.0f, titlePixel, SDL_Color{255, 255, 255, 255});
		drawButton(state, prevButton, SDL_Color{60, 90, 150, 255}, "<");
		drawButton(state, nextButton, SDL_Color{60, 90, 150, 255}, ">");

		if(page == SideBetsPage)
			drawSideBets(state, stats);
		else if(page == GraphPage)
			drawGraph(state, stats);
		else
			drawMain(state, stats);

		drawButton(state, backButton, SDL_Color{80, 80, 80, 255}, "BACK");
		drawButton(state, pageButton, SDL_Color{60, 90, 150, 255}, page == MainPage ? "SIDE BETS" : page == SideBetsPage ? "GRAPH" : "MAIN STATS");
		drawButton(state, resetButton, confirmingReset ? SDL_Color{170, 50, 50, 255} : SDL_Color{110, 60, 60, 255},
			confirmingReset ? "SURE?" : "RESET");
	}

	void drawMain(SDLState& state, const Stats& all){
		// Every line below reads this page's set of stats.
		struct View{
			const Stats& s; int scope;
			long long get(Stats::Field f) const { return s.get(scope, f); }
		} stats{all, scope};

		long long hands = stats.get(Stats::HandsPlayed);
		long long wins = stats.get(Stats::Wins), losses = stats.get(Stats::Losses);
		long long decisions = stats.get(Stats::DecisionsTotal);
		long long quizzes = stats.get(Stats::CountQuizzes);

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
			{"COUNT QUIZ", quizzes > 0 ? percent(stats.get(Stats::CountQuizCorrect), quizzes) : "-"},
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
			{" OF QUIZZES", num(quizzes)},
		};

		drawColumn(state, left, 150.0f);
		drawColumn(state, right, 780.0f);
	}

	// One row per side bet: how often and how much was bet, against how
	// often it hit and what it paid. WON is the profit on hits; NET is WON
	// minus the stakes lost.
	void drawSideBets(SDLState& state, const Stats& stats){
		static const char* NAMES[Stats::SideBetCount] = { "LUCKY LADIES", "MATCH UP", "MATCH DOWN", "LUCKY STIFF", "PUSH 22", "POT OF GOLD" };
		static const char* HEADERS[] = { "BETS", "WAGERED", "HITS", "HIT %", "WON", "NET" };
		const float nameX = 60.0f, firstColRight = 500.0f, colW = 170.0f;
		SDL_Color gold{200, 180, 100, 255}, white{235, 235, 235, 255};

		// Only the side bets this game actually has.
		std::vector<int> rows;
		for(int b = 0; b < Stats::SideBetCount; b++){
			bool belongs = scope == Stats::AllGames
				|| (scope == Stats::LuckyLadiesGame && b == Stats::LuckyLadies)
				|| (scope == Stats::PlayersEdgeGame && (b == Stats::MatchUp || b == Stats::MatchDown))
				|| (scope == Stats::LuckyStiffGame && b == Stats::LuckyStiff)
				|| (scope == Stats::FreeBetGame && (b == Stats::Push22 || b == Stats::PotOfGold));
			if(belongs)
				rows.push_back(b);
		}
		if(rows.empty()){
			std::string none = "THIS GAME HAS NO SIDE BETS";
			DigitFont::drawText(state, none, (1440.0f - DigitFont::textWidth(none, 5.0f)) / 2.0f, 320.0f, 5.0f, white);
			return;
		}

		float headerPixel = 3.5f;
		for(int c = 0; c < 6; c++){
			float w = DigitFont::textWidth(HEADERS[c], headerPixel);
			DigitFont::drawText(state, HEADERS[c], firstColRight + c * colW - w, 150.0f, headerPixel, gold);
		}

		float y = 215.0f;
		for(int b : rows){
			Stats::SideBet bet = (Stats::SideBet)b;
			long long bets = stats.get(scope, bet, Stats::SideBets);
			long long hits = stats.get(scope, bet, Stats::SideHits);
			long long won = stats.get(scope, bet, Stats::SideWon);
			std::string cells[6] = {
				num(bets),
				num(stats.get(scope, bet, Stats::SideWagered)),
				num(hits),
				bets > 0 ? percent(hits, bets) : "-",
				num(won),
				signedNum(won - stats.get(scope, bet, Stats::SideLost)),
			};

			DigitFont::drawText(state, NAMES[b], nameX, y, 4.0f, gold);
			for(int c = 0; c < 6; c++){
				float w = DigitFont::textWidth(cells[c], 5.0f);
				DigitFont::drawText(state, cells[c], firstColRight + c * colW - w, y - 2.0f, 5.0f, white);
			}
			y += rows.size() > 4 ? 56.0f : 80.0f;
		}

		std::string note = "WON IS PROFIT ON HITS. NET ALSO COUNTS IN NET WINNINGS.";
		float notePixel = 3.0f;
		DigitFont::drawText(state, note, (1440.0f - DigitFont::textWidth(note, notePixel)) / 2.0f, 560.0f, notePixel, SDL_Color{150, 170, 150, 255});
	}

	// Net winnings after each round, lifetime, as a line; a dashed line
	// marks where this session began.
	void drawGraph(SDLState& state, const Stats& stats){
		const std::vector<long long>& h = stats.history[scope];
		SDL_Color dim{150, 170, 150, 255}, white{235, 235, 235, 255}, gold{200, 180, 100, 255};
		long long net = stats.get(scope, Stats::NetWinnings);
		long long session = net - stats.sessionBase[scope];

		std::string summary = "LIFETIME " + signedNum(net) + "    THIS SESSION " + signedNum(session);
		DigitFont::drawText(state, summary, (1440.0f - DigitFont::textWidth(summary, 4.5f)) / 2.0f, 125.0f, 4.5f, gold);

		const float left = 170.0f, right = 1340.0f, top = 175.0f, bottom = 590.0f;
		if(h.size() < 2){
			std::string none = "PLAY A FEW ROUNDS TO SEE A GRAPH";
			DigitFont::drawText(state, none, (1440.0f - DigitFont::textWidth(none, 5.0f)) / 2.0f, 360.0f, 5.0f, white);
			return;
		}

		long long lo = 0, hi = 0;
		for(long long v : h){ lo = std::min(lo, v); hi = std::max(hi, v); }
		if(hi == lo) hi = lo + 1;
		auto yFor = [&](long long v){ return bottom - (float)(v - lo) / (float)(hi - lo) * (bottom - top); };
		auto xFor = [&](size_t k){ return left + (right - left) * (float)k / (float)(h.size() - 1); };

		SDL_FRect frame{ .x = left, .y = top, .w = right - left, .h = bottom - top };
		SDL_SetRenderDrawColor(state.renderer, 20, 45, 28, 255);
		SDL_RenderFillRect(state.renderer, &frame);
		SDL_SetRenderDrawColor(state.renderer, 90, 110, 90, 255);
		SDL_RenderRect(state.renderer, &frame);

		// Break-even.
		float zeroY = yFor(0);
		SDL_SetRenderDrawColor(state.renderer, 160, 160, 160, 255);
		SDL_RenderLine(state.renderer, left, zeroY, right, zeroY);
		DigitFont::drawText(state, "0", left - 20.0f - DigitFont::textWidth("0", 3.5f), zeroY - 8.0f, 3.5f, dim);
		if(hi > 0 && yFor(hi) < zeroY - 30.0f)
			DigitFont::drawText(state, signedNum(hi), left - 20.0f - DigitFont::textWidth(signedNum(hi), 3.5f), top, 3.5f, dim);
		if(lo < 0 && yFor(lo) > zeroY + 30.0f)
			DigitFont::drawText(state, signedNum(lo), left - 20.0f - DigitFont::textWidth(signedNum(lo), 3.5f), bottom - 18.0f, 3.5f, dim);

		// Where this session started: as many points back as rounds played
		// since the app opened.
		size_t sessionRounds = std::min(h.size(), (size_t)stats.sessionRounds[scope]);
		if(sessionRounds > 0 && sessionRounds < h.size()){
			float sx = xFor(h.size() - 1 - sessionRounds);  // the last point before it
			SDL_SetRenderDrawColor(state.renderer, 200, 180, 100, 255);
			for(float y = top; y < bottom; y += 14.0f)
				SDL_RenderLine(state.renderer, sx, y, sx, std::min(bottom, y + 7.0f));
			DigitFont::drawText(state, "SESSION", sx + 8.0f, top + 8.0f, 3.0f, gold);
		}

		for(size_t k = 1; k < h.size(); k++){
			float x0 = xFor(k - 1), y0 = yFor(h[k - 1]), x1 = xFor(k), y1 = yFor(h[k]);
			bool up = h[k] >= 0;
			SDL_SetRenderDrawColor(state.renderer, up ? 110 : 240, up ? 225 : 110, up ? 110 : 90, 255);
			for(float d = -1.0f; d <= 1.0f; d += 1.0f)
				SDL_RenderLine(state.renderer, x0, y0 + d, x1, y1 + d);
		}

		std::string axis = std::to_string(h.size()) + " POINTS, ONE PER ROUND";
		if(h.size() >= Stats::HISTORY_MAX / 2)
			axis = "OLDER ROUNDS ARE THINNED OUT";
		DigitFont::drawText(state, axis, (1440.0f - DigitFont::textWidth(axis, 3.0f)) / 2.0f, bottom + 12.0f, 3.0f, dim);
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
		if(SDL_PointInRectFloat(&p, &pageButton)){
			page = (page + 1) % PageCount;
			confirmingReset = false;
		}
		if(SDL_PointInRectFloat(&p, &prevButton)){
			scope = (scope + Stats::ScopeCount - 1) % Stats::ScopeCount;
			confirmingReset = false;
		}
		if(SDL_PointInRectFloat(&p, &nextButton)){
			scope = (scope + 1) % Stats::ScopeCount;
			confirmingReset = false;
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

	// Leaving the page disarms RESET and goes back to the main stats.
	void onLeave(){
		confirmingReset = false;
		page = MainPage;
	}

	std::vector<SDL_FRect> focusRects(){
		return { prevButton, nextButton, backButton, pageButton, resetButton };
	}

private:
	bool confirmingReset = false;
	enum{ MainPage, SideBetsPage, GraphPage, PageCount };
	int page = MainPage;
	int scope = Stats::AllGames;

	SDL_FRect prevButton{ .x = 30, .y = 30, .w = 90, .h = 64 };
	SDL_FRect nextButton{ .x = 1320, .y = 30, .w = 90, .h = 64 };

	SDL_FRect backButton{ .x = 350, .y = 630, .w = 200, .h = 56 };
	SDL_FRect pageButton{ .x = 580, .y = 630, .w = 280, .h = 56 };
	SDL_FRect resetButton{ .x = 890, .y = 630, .w = 200, .h = 56 };

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
		if(w > rect.w - 20.0f){
			pixel *= (rect.w - 20.0f) / w;
			w = DigitFont::textWidth(label, pixel);
		}
		DigitFont::drawText(state, label, rect.x + (rect.w - w) / 2.0f, rect.y + (rect.h - 5 * pixel) / 2.0f, pixel, SDL_Color{255, 255, 255, 255});
	}
};
