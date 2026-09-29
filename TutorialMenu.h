#pragma once
#include "Game.h"
#include "DigitFont.h"
#include "Platform.h"
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
	void open(){ page = 0; }

	void draw(SDLState& state, Resources& res){
		SDL_SetRenderDrawColor(state.renderer, 10, 30, 15, 255);
		SDL_RenderFillRect(state.renderer, nullptr);

		const Page& pg = pages()[page];

		float titlePixel = 7.0f;
		float titleW = DigitFont::textWidth(pg.title, titlePixel);
		DigitFont::drawText(state, pg.title, (1440.0f - titleW) / 2.0f, 50.0f, titlePixel, SDL_Color{255, 225, 80, 255});

		float y = 150.0f;
		for(const std::string& line : pg.lines){
			float pixel = 4.5f;
			float w = DigitFont::textWidth(line, pixel);
			if(w > 1320.0f){
				pixel *= 1320.0f / w;
				w = DigitFont::textWidth(line, pixel);
			}
			DigitFont::drawText(state, line, (1440.0f - w) / 2.0f, y, pixel, SDL_Color{230, 230, 230, 255});
			y += 50.0f;
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

	static constexpr int PAGE_COUNT = 4;
	inline static const Page TOUCH_PAGES[PAGE_COUNT] = {
		{"WELCOME", {
			"BEAT THE DEALER TO 21 WITHOUT GOING OVER.",
			"PICK FROM FIVE GAMES: STANDARD, LUCKY LADIES,",
			"PLAYERS EDGE, LUCKY STIFF AND FREE BET.",
			"TAP ABOUT NEXT TO A GAME TO READ ITS RULES.",
			"UP TO 5 PLAYERS SHARE ONE TABLE,",
			"EACH WITH THEIR OWN BANKROLL.",
			"PROGRESS SAVES AFTER EVERY ROUND."
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
			"HIT        TAP",
			"STAND      SWIPE LEFT OR RIGHT",
			"DOUBLE     TWO FINGER TAP",
			"SPLIT      TWO FINGERS APART",
			"SURRENDER  SWIPE UP OR DOWN",
			"THE ARROW SHOWS WHOSE TURN IT IS.",
			"WITH AN ACE SHOWING YOU MAY BE OFFERED INSURANCE."
		}},
		{"HELP AT THE TABLE", {
			"TIP SHOWS THE BEST MOVE FOR YOUR HAND.",
			"CNT SHOWS THE CARD COUNT.",
			"PAUSE HAS THE STRATEGY TABLE, GAME RULES,",
			"GAME OPTIONS, CONTROLS AND YOUR STATS.",
			"A YELLOW CARD ON THE DISCARD PILE MEANS",
			"A FRESH SHOE AFTER THIS ROUND.",
			"GOOD LUCK!"
		}},
	};

	inline static const Page KEYBOARD_PAGES[PAGE_COUNT] = {
		{"WELCOME", {
			"BEAT THE DEALER TO 21 WITHOUT GOING OVER.",
			"PICK FROM FIVE GAMES: STANDARD, LUCKY LADIES,",
			"PLAYERS EDGE, LUCKY STIFF AND FREE BET.",
			"CLICK ABOUT NEXT TO A GAME TO READ ITS RULES.",
			"UP TO 5 PLAYERS SHARE ONE TABLE,",
			"EACH WITH THEIR OWN BANKROLL.",
			"ARROW KEYS AND ENTER WORK IN EVERY MENU."
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
			"HIT        H",
			"STAND      S",
			"DOUBLE     D",
			"SPLIT      P",
			"SURRENDER  R",
			"INSURANCE  Y OR N, WHEN THE DEALER SHOWS AN ACE",
			"ESC PAUSES THE GAME AND GOES BACK IN MENUS."
		}},
		{"HELP AT THE TABLE", {
			"TIP SHOWS THE BEST MOVE FOR YOUR HAND.",
			"CNT SHOWS THE CARD COUNT.",
			"PAUSE HAS THE STRATEGY TABLE, GAME RULES,",
			"GAME OPTIONS, CONTROLS AND YOUR STATS.",
			"A YELLOW CARD ON THE DISCARD PILE MEANS",
			"A FRESH SHOE AFTER THIS ROUND.",
			"PROGRESS SAVES AFTER EVERY ROUND. GOOD LUCK!"
		}},
	};

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
