#pragma once
#include "Game.h"
#include "DigitFont.h"
#include "Platform.h"
#include <algorithm>
#include <string>
#include <vector>

// HOW TO PLAY: a few short pages shown automatically on the very first
// launch (SaveData::tutorialSeen) and any time from the main menu's HOW TO
// PLAY button. BACK/NEXT page through (BACK on the first page and DONE on
// the last leave), SKIP leaves from anywhere. Two wordings: touch
// (tap/swipe) on phones and tablets, keyboard/mouse (click/keys) on
// desktops -- see Platform.h. Keep the controls pages in sync with
// Table::handleEvent()/processGesture(), GesturesMenu.h and KeyboardMenu.h.
class TutorialMenu
{
public:
	// SETTINGS' ONE-HANDED TABLE (set by mina.cpp): the touch pages list
	// its gestures instead of the two-finger ones.
	bool oneHanded = false;

	void open(){ page = 0; }

	void draw(SDLState& state, Resources& res){
		SDL_SetRenderDrawColor(state.renderer, 10, 30, 15, 255);
		SDL_RenderFillRect(state.renderer, nullptr);

		const Page& pg = pages()[page];

		float titlePixel = 7.0f;
		float titleW = DigitFont::textWidth(pg.title, titlePixel);
		DigitFont::drawText(state, pg.title, (1440.0f - titleW) / 2.0f, 50.0f, titlePixel, SDL_Color{255, 225, 80, 255});

		// Three kinds of line: "LEFT|RIGHT" rows make a two-column table
		// (gold names, white details), aligned and centered as a block;
		// a line ending in ':' is a gold heading; anything else is
		// centered text.
		std::vector<std::string> lines;
		for(const std::string& original : pg.lines)
			lines.push_back(oneHanded ? oneHandedLine(original) : original);
		const float gap = 60.0f;
		float leftW = 0.0f, rightW = 0.0f;
		for(const std::string& line : lines){
			size_t bar = line.find('|');
			if(bar == std::string::npos)
				continue;
			leftW = std::max(leftW, DigitFont::textWidth(line.substr(0, bar), 1.0f));
			rightW = std::max(rightW, DigitFont::textWidth(line.substr(bar + 1), 1.0f));
		}
		// The whole table shrinks together if it's too wide.
		const float tablePixel = leftW > 0.0f ? std::min(4.5f, (1320.0f - gap) / (leftW + rightW)) : 4.5f;
		leftW *= tablePixel;
		rightW *= tablePixel;
		float tableX = (1440.0f - (leftW + gap + rightW)) / 2.0f;

		float y = 140.0f;
		bool inTable = false;
		for(const std::string& line : lines){
			size_t bar = line.find('|');
			// A little room between a table and the text after it.
			if(inTable && bar == std::string::npos)
				y += 14.0f;
			inTable = bar != std::string::npos;
			if(bar != std::string::npos){
				DigitFont::drawText(state, line.substr(0, bar), tableX, y, tablePixel, SDL_Color{255, 225, 80, 255});
				DigitFont::drawText(state, line.substr(bar + 1), tableX + leftW + gap, y, tablePixel, SDL_Color{230, 230, 230, 255});
				y += 42.0f;
				continue;
			}
			bool heading = !line.empty() && line.back() == ':';
			float pixel = 4.5f;
			float w = DigitFont::textWidth(line, pixel);
			if(w > 1320.0f){
				pixel *= 1320.0f / w;
				w = DigitFont::textWidth(line, pixel);
			}
			DigitFont::drawText(state, line, (1440.0f - w) / 2.0f, y, pixel,
				heading ? SDL_Color{255, 225, 80, 255} : SDL_Color{230, 230, 230, 255});
			y += heading ? 46.0f : 50.0f;
		}

		std::string pageLabel = std::to_string(page + 1) + " / " + std::to_string(PAGE_COUNT);
		float plW = DigitFont::textWidth(pageLabel, 4.0f);
		DigitFont::drawText(state, pageLabel, (1440.0f - plW) / 2.0f, 590.0f, 4.0f, SDL_Color{160, 160, 160, 255});

		drawButton(state, backButton, SDL_Color{80, 80, 80, 255}, "BACK");
		drawButton(state, nextButton, SDL_Color{60, 130, 70, 255}, page == PAGE_COUNT - 1 ? "DONE" : "NEXT");
		drawButton(state, skipButton, SDL_Color{70, 70, 70, 255}, "SKIP");
	}

	// Returns true when the tutorial should close.
	bool handlePoint(SDLState& state, float windowX, float windowY){
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			return false;

		SDL_FPoint p{x, y};
		if(SDL_PointInRectFloat(&p, &skipButton))
			return true;
		if(SDL_PointInRectFloat(&p, &nextButton)){
			if(page == PAGE_COUNT - 1)
				return true;
			page++;
		}
		if(SDL_PointInRectFloat(&p, &backButton)){
			if(page == 0)
				return true;
			page--;
		}
		return false;
	}

	std::vector<SDL_FRect> focusRects(){
		return { backButton, nextButton, skipButton };
	}

private:
	struct Page{ std::string title; std::vector<std::string> lines; };

	static constexpr int PAGE_COUNT = 6;
	inline static const Page TOUCH_PAGES[PAGE_COUNT] = {
		{"WELCOME", {
			"BEAT THE DEALER TO 21 WITHOUT GOING OVER.",
			"FIVE GAMES TO PICK FROM:",
			"STANDARD|2 OR 6 DECKS",
			"LUCKY LADIES|2 OR 6 DECKS, WITH A SIDE BET",
			"PLAYERS EDGE|2 OR 6 SPANISH DECKS",
			"LUCKY STIFF|8 DECKS, WITH A SIDE BET",
			"FREE BET|6 DECKS, FREE DOUBLES AND SPLITS",
			"TAP ABOUT NEXT TO A GAME FOR ITS FULL RULES.",
			"UP TO 5 PLAYERS. PROGRESS SAVES EVERY ROUND."
		}},
		{"BETTING", {
			"BEFORE EACH ROUND, SET EACH PLAYERS BET",
			"WITH THE BUTTONS AT THEIR SEAT.",
			"PICK A CHIP SIZE, THEN RAISE OR LOWER THE BET.",
			"SIDE BETS, WHEN A GAME HAS THEM, WORK THE SAME.",
			"A PLAYER WITH A 0 BET SITS THE ROUND OUT.",
			"THEN TAP DEAL.",
			"OUT OF CHIPS? TAP BUY IN FOR MORE."
		}},
		{"PLAYING YOUR HAND", {
			"HIT|TAP",
			"STAND|SWIPE LEFT OR RIGHT",
			"DOUBLE|TWO FINGER TAP",
			"SPLIT|TWO FINGERS APART",
			"SURRENDER|SWIPE UP OR DOWN",
			"THE ARROW SHOWS WHOSE TURN IT IS.",
			"WITH AN ACE SHOWING YOU MAY BE OFFERED INSURANCE."
		}},
		{"HELP AT THE TABLE", {
			"TIP SHOWS THE BEST MOVE FOR YOUR HAND.",
			"CNT SHOWS THE CARD COUNT.",
			"PAUSE HAS CHARTS, GAME RULES, SETTINGS,",
			"CONTROLS, STATS AND THE MAIN MENU.",
			"TAP ANY CHART SQUARE FOR WHAT IT MEANS.",
			"A YELLOW CARD ON THE DISCARD PILE MEANS",
			"A FRESH SHOE AFTER THIS ROUND."
		}},
		{"COUNTING CARDS", {
			"HI-LO: 2-6 COUNT +1, 7-9 COUNT 0, 10S AND ACES -1.",
			"TRUE COUNT = RUNNING COUNT / DECKS LEFT.",
			"TURN ON INDEX PLAYS IN SETTINGS: TIP, CHARTS AND",
			"DRILLS THEN USE THE COUNT, AND YOUR BETS ARE",
			"CHECKED AGAINST THE BET RAMP.",
			"CHARTS, DEVIATIONS LISTS EVERY COUNT PLAY.",
			"PLANNING SHOWS THE BET RAMP, DRILLS HAS ITS DRILL."
		}},
		{"THE MAIN MENU", {
			"DRILLS|STRATEGY, COUNTING, BET RAMP",
			"PLANNING|BET RAMP, BANKROLL, GAME ODDS",
			"SESSION LOG|YOUR REAL CASINO SESSIONS",
			"STATS|EVERY HAND YOU PLAY, BY GAME",
			"SETTINGS|SOUND, ONE-HANDED, SOFT 17 AND MORE",
			"GOOD LUCK!"
		}},
	};

	inline static const Page KEYBOARD_PAGES[PAGE_COUNT] = {
		{"WELCOME", {
			"BEAT THE DEALER TO 21 WITHOUT GOING OVER.",
			"FIVE GAMES TO PICK FROM:",
			"STANDARD|2 OR 6 DECKS",
			"LUCKY LADIES|2 OR 6 DECKS, WITH A SIDE BET",
			"PLAYERS EDGE|2 OR 6 SPANISH DECKS",
			"LUCKY STIFF|8 DECKS, WITH A SIDE BET",
			"FREE BET|6 DECKS, FREE DOUBLES AND SPLITS",
			"CLICK ABOUT NEXT TO A GAME FOR ITS FULL RULES.",
			"UP TO 5 PLAYERS. PROGRESS SAVES EVERY ROUND."
		}},
		{"BETTING", {
			"BEFORE EACH ROUND, SET EACH PLAYERS BET",
			"WITH THE BUTTONS AT THEIR SEAT.",
			"PICK A CHIP SIZE, THEN RAISE OR LOWER THE BET.",
			"SIDE BETS, WHEN A GAME HAS THEM, WORK THE SAME.",
			"A PLAYER WITH A 0 BET SITS THE ROUND OUT.",
			"THEN CLICK DEAL, OR PRESS SPACE.",
			"OUT OF CHIPS? CLICK BUY IN FOR MORE."
		}},
		{"PLAYING YOUR HAND", {
			"HIT|H",
			"STAND|S",
			"DOUBLE|D",
			"SPLIT|P",
			"SURRENDER|R",
			"INSURANCE|Y OR N",
			"ESC PAUSES THE GAME AND GOES BACK IN MENUS."
		}},
		{"HELP AT THE TABLE", {
			"TIP SHOWS THE BEST MOVE FOR YOUR HAND.",
			"CNT SHOWS THE CARD COUNT.",
			"PAUSE HAS CHARTS, GAME RULES, SETTINGS,",
			"CONTROLS, STATS AND THE MAIN MENU.",
			"CLICK ANY CHART SQUARE FOR WHAT IT MEANS.",
			"A YELLOW CARD ON THE DISCARD PILE MEANS",
			"A FRESH SHOE AFTER THIS ROUND."
		}},
		{"COUNTING CARDS", {
			"HI-LO: 2-6 COUNT +1, 7-9 COUNT 0, 10S AND ACES -1.",
			"TRUE COUNT = RUNNING COUNT / DECKS LEFT.",
			"TURN ON INDEX PLAYS IN SETTINGS: TIP, CHARTS AND",
			"DRILLS THEN USE THE COUNT, AND YOUR BETS ARE",
			"CHECKED AGAINST THE BET RAMP.",
			"CHARTS, DEVIATIONS LISTS EVERY COUNT PLAY.",
			"PLANNING SHOWS THE BET RAMP, DRILLS HAS ITS DRILL."
		}},
		{"THE MAIN MENU", {
			"DRILLS|STRATEGY, COUNTING, BET RAMP",
			"PLANNING|BET RAMP, BANKROLL, GAME ODDS",
			"SESSION LOG|YOUR REAL CASINO SESSIONS",
			"STATS|EVERY HAND YOU PLAY, BY GAME",
			"SETTINGS|SOUND, DEALER SPEED, SOFT 17 AND MORE",
			"GOOD LUCK!"
		}},
	};

	static std::string oneHandedLine(const std::string& line){
		if(line == "DOUBLE|TWO FINGER TAP") return "DOUBLE|SWIPE UP";
		if(line == "SPLIT|TWO FINGERS APART") return "SPLIT|PRESS AND HOLD";
		if(line == "SURRENDER|SWIPE UP OR DOWN") return "SURRENDER|SWIPE DOWN";
		return line;
	}

	static const Page* pages(){
		return usesTouchControls() ? TOUCH_PAGES : KEYBOARD_PAGES;
	}

	int page = 0;

	SDL_FRect backButton{ .x = 420, .y = 630, .w = 180, .h = 56 };
	SDL_FRect nextButton{ .x = 630, .y = 630, .w = 180, .h = 56 };
	SDL_FRect skipButton{ .x = 840, .y = 630, .w = 180, .h = 56 };

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
