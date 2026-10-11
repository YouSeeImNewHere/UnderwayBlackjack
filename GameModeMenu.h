#pragma once
#include "Game.h"
#include "DigitFont.h"
#include <algorithm>
#include <string>
#include <vector>

// Shown once, between the main Menu and SetupMenu, whenever Start/Restart
// is chosen -- picks which game gets played. All modes from the original
// ask are wired up now: standard, Lucky Ladies, Player's Edge (Spanish 21),
// Lucky Stiff, and Free Bet Blackjack.
enum class GameMode{
	None,
	TwoDeck,
	SixDeck,
	TwoDeckLuckyLadies,
	SixDeckLuckyLadies,
	TwoDeckPlayersEdge,
	SixDeckPlayersEdge,
	EightDeckLuckyStiff,
	SixDeckFreeBet
};

// The two training games, picked on the bottom row of GameModeMenu. Each
// is played on top of a regular game, picked next: PRACTICE uses that
// game's strategy table, COUNT QUIZ its shoe.
enum class Training{ None, Practice, CountQuiz };

class GameModeMenu
{
public:
	// Set once PRACTICE or COUNT QUIZ is picked: the menu then asks which
	// game to play it with. Read by mina.cpp along with the chosen mode.
	Training training = Training::None;

	// Back to the plain game list (BACK while picking for a training game).
	bool leaveTraining(){
		if(training == Training::None)
			return false;
		training = Training::None;
		return true;
	}

	void draw(SDLState& state, Resources& res){
		SDL_SetRenderDrawColor(state.renderer, 20, 70, 35, 255);
		SDL_RenderFillRect(state.renderer, nullptr);

		float titlePixel = 7.0f;
		std::string title = training == Training::Practice ? "PRACTICE: CHOOSE A STRATEGY TABLE"
			: training == Training::CountQuiz ? "COUNT QUIZ: CHOOSE A GAME" : "CHOOSE A GAME";
		if(DigitFont::textWidth(title, titlePixel) > 1360.0f)
			titlePixel *= 1360.0f / DigitFont::textWidth(title, titlePixel);
		float titleW = DigitFont::textWidth(title, titlePixel);
		DigitFont::drawText(state, title, (1440.0f - titleW) / 2.0f, 60.0f, titlePixel, SDL_Color{255, 255, 255, 255});

		// A list: each game's name, then a button per deck count, then
		// ABOUT. Every name, and every button label, the same size.
		static const char* NAMES[5] = { "STANDARD", "LUCKY LADIES", "PLAYERS EDGE", "LUCKY STIFF", "FREE BET" };
		static const float ROWS[5] = { ROW1_Y, ROW2_Y, ROW3_Y, ROW4_Y, ROW5_Y };
		float namePixel = std::min(5.0f, (DECK2_X - NAME_X - 30.0f) / DigitFont::textWidth("PLAYERS EDGE", 1.0f));
		for(int i = 0; i < 5; i++)
			DigitFont::drawText(state, NAMES[i], NAME_X, ROWS[i] + (BTN_H - 5 * namePixel) / 2.0f, namePixel, SDL_Color{255, 225, 80, 255});
		const float deckPixel = 5.0f;
		drawButton(state, twoDeckButton(), SDL_Color{60, 130, 70, 255}, "2 DECK", deckPixel);
		drawButton(state, sixDeckButton(), SDL_Color{60, 130, 70, 255}, "6 DECK", deckPixel);
		drawButton(state, twoDeckLuckyLadiesButton(), SDL_Color{140, 80, 140, 255}, "2 DECK", deckPixel);
		drawButton(state, sixDeckLuckyLadiesButton(), SDL_Color{140, 80, 140, 255}, "6 DECK", deckPixel);
		drawButton(state, twoDeckPlayersEdgeButton(), SDL_Color{60, 100, 150, 255}, "2 DECK", deckPixel);
		drawButton(state, sixDeckPlayersEdgeButton(), SDL_Color{60, 100, 150, 255}, "6 DECK", deckPixel);
		drawButton(state, luckyStiffButton(), SDL_Color{150, 90, 50, 255}, "8 DECK", deckPixel);
		drawButton(state, freeBetButton(), SDL_Color{50, 140, 130, 255}, "6 DECK", deckPixel);
		if(training == Training::None){
			DigitFont::drawText(state, "TRAINING", NAME_X, ROW6_Y + (BTN_H - 5 * namePixel) / 2.0f, namePixel, SDL_Color{230, 210, 140, 255});
			float trainPixel = std::min(deckPixel, (BTN_W - 24.0f) / DigitFont::textWidth("COUNT QUIZ", 1.0f));
			drawButton(state, practiceButton_, SDL_Color{150, 120, 40, 255}, "PRACTICE", trainPixel);
			drawButton(state, countQuizButton_, SDL_Color{150, 120, 40, 255}, "COUNT QUIZ", trainPixel);
		} else{
			std::string hint = training == Training::Practice
				? "OFF-CHART MOVES ARE CAUGHT BEFORE THEY PLAY"
				: "AFTER EVERY ROUND, NAME THE RUNNING COUNT";
			float hp = 3.5f;
			DigitFont::drawText(state, hint, (1440.0f - DigitFont::textWidth(hint, hp)) / 2.0f, ROW6_Y + (BTN_H - 5 * hp) / 2.0f, hp, SDL_Color{230, 210, 140, 255});
		}

		// One ABOUT button per row, not per mode button -- the rules text
		// for a variant doesn't actually differ between its 2-deck and
		// 6-deck buttons (AboutMenu's rulesFor()/sideBetsFor() only branch
		// on which *family* a mode belongs to, e.g. hasLuckyLadies(), not
		// on deck count), so a second, identical about page per row would
		// just be redundant.
		drawAboutButton(state, standardAboutButton());
		drawAboutButton(state, luckyLadiesAboutButton());
		drawAboutButton(state, playersEdgeAboutButton());
		drawAboutButton(state, luckyStiffAboutButton());
		drawAboutButton(state, freeBetAboutButton());

		drawButton(state, backButton_, SDL_Color{80, 80, 80, 255}, "BACK", 6.0f);
	}

	// True when BACK is hit -- mina.cpp decides where back goes.
	bool handleBackPoint(SDLState& state, float windowX, float windowY){
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			return false;

		SDL_FPoint p{x, y};
		SDL_FRect back = backButton_;
		return SDL_PointInRectFloat(&p, &back);
	}

	// Every button, for mina.cpp's arrow-key navigation.
	std::vector<SDL_FRect> focusRects(){
		std::vector<SDL_FRect> rects{
			twoDeckButton_, sixDeckButton_, standardAboutButton_,
			twoDeckLuckyLadiesButton_, sixDeckLuckyLadiesButton_, luckyLadiesAboutButton_,
			twoDeckPlayersEdgeButton_, sixDeckPlayersEdgeButton_, playersEdgeAboutButton_,
			luckyStiffButton_, luckyStiffAboutButton_,
			freeBetButton_, freeBetAboutButton_,
		};
		if(training == Training::None){
			rects.push_back(practiceButton_);
			rects.push_back(countQuizButton_);
		}
		rects.push_back(backButton_);
		return rects;
	}

	// windowX/windowY: raw event coordinates in window space, same
	// convention as Menu/SetupMenu's handlePoint. Returns which mode's
	// rules the player wants to read (representative of that whole row,
	// see the comment on the about buttons above), or GameMode::None if no
	// about button was hit.
	GameMode handleAboutPoint(SDLState& state, float windowX, float windowY){
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			return GameMode::None;

		SDL_FPoint p{x, y};

		if(SDL_PointInRectFloat(&p, &standardAboutButton_))
			return GameMode::TwoDeck;
		if(SDL_PointInRectFloat(&p, &luckyLadiesAboutButton_))
			return GameMode::TwoDeckLuckyLadies;
		if(SDL_PointInRectFloat(&p, &playersEdgeAboutButton_))
			return GameMode::TwoDeckPlayersEdge;
		if(SDL_PointInRectFloat(&p, &luckyStiffAboutButton_))
			return GameMode::EightDeckLuckyStiff;
		if(SDL_PointInRectFloat(&p, &freeBetAboutButton_))
			return GameMode::SixDeckFreeBet;

		return GameMode::None;
	}

	// windowX/windowY: raw event coordinates in window space, same
	// convention as Menu/SetupMenu's handlePoint.
	GameMode handlePoint(SDLState& state, float windowX, float windowY){
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			return GameMode::None;

		SDL_FPoint p{x, y};

		if(SDL_PointInRectFloat(&p, &twoDeckButton_))
			return GameMode::TwoDeck;
		if(SDL_PointInRectFloat(&p, &sixDeckButton_))
			return GameMode::SixDeck;
		if(SDL_PointInRectFloat(&p, &twoDeckLuckyLadiesButton_))
			return GameMode::TwoDeckLuckyLadies;
		if(SDL_PointInRectFloat(&p, &sixDeckLuckyLadiesButton_))
			return GameMode::SixDeckLuckyLadies;
		if(SDL_PointInRectFloat(&p, &twoDeckPlayersEdgeButton_))
			return GameMode::TwoDeckPlayersEdge;
		if(SDL_PointInRectFloat(&p, &sixDeckPlayersEdgeButton_))
			return GameMode::SixDeckPlayersEdge;
		if(SDL_PointInRectFloat(&p, &luckyStiffButton_))
			return GameMode::EightDeckLuckyStiff;
		if(SDL_PointInRectFloat(&p, &freeBetButton_))
			return GameMode::SixDeckFreeBet;

		// PRACTICE / COUNT QUIZ: the same list again, now picking which
		// game to train on.
		if(training == Training::None){
			if(SDL_PointInRectFloat(&p, &practiceButton_))
				training = Training::Practice;
			else if(SDL_PointInRectFloat(&p, &countQuizButton_))
				training = Training::CountQuiz;
		}

		return GameMode::None;
	}

private:
	// A row per game: its name, a 2 DECK and a 6 DECK button (Lucky
	// Stiff and Free Bet come in one size, so theirs spans both), ABOUT.
	static constexpr float BTN_W = 220.0f;
	static constexpr float BTN_H = 62.0f;
	static constexpr float COL_GAP = 20.0f;
	static constexpr float ROW_GAP = 12.0f;
	static constexpr float NAME_X = 200.0f;
	static constexpr float DECK2_X = 600.0f;
	static constexpr float BLOCK_W = BTN_W * 2 + COL_GAP;
	static constexpr float BLOCK_X = DECK2_X;
	static constexpr float ROW1_Y = 122.0f;
	static constexpr float ROW2_Y = ROW1_Y + BTN_H + ROW_GAP;
	static constexpr float ROW3_Y = ROW2_Y + BTN_H + ROW_GAP;
	static constexpr float ROW4_Y = ROW3_Y + BTN_H + ROW_GAP;
	static constexpr float ROW5_Y = ROW4_Y + BTN_H + ROW_GAP;
	static constexpr float ROW6_Y = ROW5_Y + BTN_H + ROW_GAP;

	SDL_FRect twoDeckButton_{ .x = BLOCK_X, .y = ROW1_Y, .w = BTN_W, .h = BTN_H };
	SDL_FRect sixDeckButton_{ .x = BLOCK_X + BTN_W + COL_GAP, .y = ROW1_Y, .w = BTN_W, .h = BTN_H };
	SDL_FRect twoDeckLuckyLadiesButton_{ .x = BLOCK_X, .y = ROW2_Y, .w = BTN_W, .h = BTN_H };
	SDL_FRect sixDeckLuckyLadiesButton_{ .x = BLOCK_X + BTN_W + COL_GAP, .y = ROW2_Y, .w = BTN_W, .h = BTN_H };
	SDL_FRect twoDeckPlayersEdgeButton_{ .x = BLOCK_X, .y = ROW3_Y, .w = BTN_W, .h = BTN_H };
	SDL_FRect sixDeckPlayersEdgeButton_{ .x = BLOCK_X + BTN_W + COL_GAP, .y = ROW3_Y, .w = BTN_W, .h = BTN_H };
	SDL_FRect luckyStiffButton_{ .x = BLOCK_X, .y = ROW4_Y, .w = BLOCK_W, .h = BTN_H };
	SDL_FRect freeBetButton_{ .x = BLOCK_X, .y = ROW5_Y, .w = BLOCK_W, .h = BTN_H };
	SDL_FRect practiceButton_{ .x = BLOCK_X, .y = ROW6_Y, .w = BTN_W, .h = BTN_H };
	SDL_FRect countQuizButton_{ .x = BLOCK_X + BTN_W + COL_GAP, .y = ROW6_Y, .w = BTN_W, .h = BTN_H };

	// One per row, to the right of the block (which ends at BLOCK_X +
	// BLOCK_W) -- there's a wide, otherwise-empty margin out there (the
	// block itself is centered with ~400px clear on each side), plenty of
	// room without touching the mode buttons' own layout at all.
	static constexpr float ABOUT_BTN_W = 160.0f;
	static constexpr float ABOUT_BTN_X = BLOCK_X + BLOCK_W + 20.0f;
	SDL_FRect standardAboutButton_{ .x = ABOUT_BTN_X, .y = ROW1_Y, .w = ABOUT_BTN_W, .h = BTN_H };
	SDL_FRect luckyLadiesAboutButton_{ .x = ABOUT_BTN_X, .y = ROW2_Y, .w = ABOUT_BTN_W, .h = BTN_H };
	SDL_FRect playersEdgeAboutButton_{ .x = ABOUT_BTN_X, .y = ROW3_Y, .w = ABOUT_BTN_W, .h = BTN_H };
	SDL_FRect luckyStiffAboutButton_{ .x = ABOUT_BTN_X, .y = ROW4_Y, .w = ABOUT_BTN_W, .h = BTN_H };
	SDL_FRect freeBetAboutButton_{ .x = ABOUT_BTN_X, .y = ROW5_Y, .w = ABOUT_BTN_W, .h = BTN_H };
	// Same size/x as the other screens' BACK buttons, under the last row.
	SDL_FRect backButton_{ .x = 620, .y = ROW6_Y + BTN_H + 22.0f, .w = 200, .h = 56 };

	SDL_FRect standardAboutButton(){ return standardAboutButton_; }
	SDL_FRect luckyLadiesAboutButton(){ return luckyLadiesAboutButton_; }
	SDL_FRect playersEdgeAboutButton(){ return playersEdgeAboutButton_; }
	SDL_FRect luckyStiffAboutButton(){ return luckyStiffAboutButton_; }
	SDL_FRect freeBetAboutButton(){ return freeBetAboutButton_; }

	SDL_FRect twoDeckButton(){ return twoDeckButton_; }
	SDL_FRect sixDeckButton(){ return sixDeckButton_; }
	SDL_FRect twoDeckLuckyLadiesButton(){ return twoDeckLuckyLadiesButton_; }
	SDL_FRect sixDeckLuckyLadiesButton(){ return sixDeckLuckyLadiesButton_; }
	SDL_FRect twoDeckPlayersEdgeButton(){ return twoDeckPlayersEdgeButton_; }
	SDL_FRect sixDeckPlayersEdgeButton(){ return sixDeckPlayersEdgeButton_; }
	SDL_FRect luckyStiffButton(){ return luckyStiffButton_; }
	SDL_FRect freeBetButton(){ return freeBetButton_; }

	void drawButton(SDLState& state, const SDL_FRect& rect, SDL_Color color, const std::string& label, float pixel){
		SDL_SetRenderDrawColor(state.renderer, color.r, color.g, color.b, color.a);
		SDL_RenderFillRect(state.renderer, &rect);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &rect);

		// Auto-shrinks to fit -- DigitFont's 5-wide glyphs run noticeably
		// wider per character than the font's old 3-wide ones, which left
		// some labels sized for that old font overflowing their button.
		// Clamping here means every button self-corrects instead of each
		// needing its own pixel size hand-tuned for the new font.
		float maxW = rect.w - 12.0f;
		float w = DigitFont::textWidth(label, pixel);
		if(w > maxW && w > 0.0f)
			pixel *= maxW / w;

		w = DigitFont::textWidth(label, pixel);
		DigitFont::drawText(state, label, rect.x + (rect.w - w) / 2.0f, rect.y + (rect.h - 5 * pixel) / 2.0f, pixel, SDL_Color{255, 255, 255, 255});
	}

	void drawAboutButton(SDLState& state, const SDL_FRect& rect){
		drawButton(state, rect, SDL_Color{70, 70, 70, 255}, "ABOUT", 5.0f);
	}
};

inline int deckCountFor(GameMode mode){
	if(mode == GameMode::EightDeckLuckyStiff)
		return 8;
	if(mode == GameMode::SixDeck || mode == GameMode::SixDeckLuckyLadies || mode == GameMode::SixDeckPlayersEdge || mode == GameMode::SixDeckFreeBet)
		return 6;
	return 2;
}

inline bool hasLuckyLadies(GameMode mode){
	return mode == GameMode::TwoDeckLuckyLadies || mode == GameMode::SixDeckLuckyLadies;
}

inline bool isPlayersEdge(GameMode mode){
	return mode == GameMode::TwoDeckPlayersEdge || mode == GameMode::SixDeckPlayersEdge;
}

inline bool hasLuckyStiff(GameMode mode){
	return mode == GameMode::EightDeckLuckyStiff;
}

inline bool isFreeBet(GameMode mode){
	return mode == GameMode::SixDeckFreeBet;
}

// Every side-bet family -- SetupMenu's bankroll calculator just needs to
// know "does at least one side bet apply here," not which one. Free Bet
// Blackjack has two (Emerald Queen's): Push 22 and Pot of Gold.
inline bool hasAnySideBet(GameMode mode){
	return hasLuckyLadies(mode) || isPlayersEdge(mode) || hasLuckyStiff(mode) || isFreeBet(mode);
}

// How many side bets SetupMenu's bankroll calculator needs to stake for
// per round, at the same PlayerConfig::sideBetSize each -- 0 for a mode
// with none, 1 for Lucky Ladies/Lucky Stiff's single bet, 2 for Player's
// Edge, whose Match Up and Match Down are both wagered every round
// (Person::setInitialMatchBets() seeds them equally).
inline int sideBetCountFor(GameMode mode){
	if(isPlayersEdge(mode) || isFreeBet(mode))
		return 2;
	if(hasLuckyLadies(mode) || hasLuckyStiff(mode))
		return 1;
	return 0;
}
