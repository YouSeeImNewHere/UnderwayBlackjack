#pragma once
#include "Game.h"
#include "DigitFont.h"
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

class GameModeMenu
{
public:
	void draw(SDLState& state, Resources& res){
		SDL_SetRenderDrawColor(state.renderer, 20, 70, 35, 255);
		SDL_RenderFillRect(state.renderer, nullptr);

		float titlePixel = 7.0f;
		std::string title = "CHOOSE A GAME";
		float titleW = DigitFont::textWidth(title, titlePixel);
		DigitFont::drawText(state, title, (1440.0f - titleW) / 2.0f, 60.0f, titlePixel, SDL_Color{255, 255, 255, 255});

		drawButton(state, twoDeckButton(), SDL_Color{60, 130, 70, 255}, "2 DECK", 6.0f);
		drawButton(state, sixDeckButton(), SDL_Color{60, 130, 70, 255}, "6 DECK", 6.0f);
		drawButton(state, twoDeckLuckyLadiesButton(), SDL_Color{140, 80, 140, 255}, "LUCKY LADIES 2D", 4.0f);
		drawButton(state, sixDeckLuckyLadiesButton(), SDL_Color{140, 80, 140, 255}, "LUCKY LADIES 6D", 4.0f);
		drawButton(state, twoDeckPlayersEdgeButton(), SDL_Color{60, 100, 150, 255}, "PLAYERS EDGE 2D", 4.0f);
		drawButton(state, sixDeckPlayersEdgeButton(), SDL_Color{60, 100, 150, 255}, "PLAYERS EDGE 6D", 4.0f);
		drawButton(state, luckyStiffButton(), SDL_Color{150, 90, 50, 255}, "LUCKY STIFF 8 DECK", 5.0f);
		drawButton(state, freeBetButton(), SDL_Color{50, 140, 130, 255}, "FREE BET BLACKJACK 6 DECK", 4.0f);

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
		return {
			twoDeckButton_, sixDeckButton_, standardAboutButton_,
			twoDeckLuckyLadiesButton_, sixDeckLuckyLadiesButton_, luckyLadiesAboutButton_,
			twoDeckPlayersEdgeButton_, sixDeckPlayersEdgeButton_, playersEdgeAboutButton_,
			luckyStiffButton_, luckyStiffAboutButton_,
			freeBetButton_, freeBetAboutButton_,
			backButton_
		};
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

		return GameMode::None;
	}

private:
	// 2 columns x 3 rows, plus 2 more rows of one button each spanning
	// both columns (Lucky Stiff, Free Bet Blackjack -- neither is offered
	// at more than one deck count, so neither needs a left/right pair like
	// the others). Centered as a block: BLOCK_W wide, starting at BLOCK_X
	// so the whole block sits in the middle of the 1440-wide canvas.
	static constexpr float BTN_W = 300.0f;
	static constexpr float BTN_H = 70.0f;
	static constexpr float COL_GAP = 40.0f;
	static constexpr float ROW_GAP = 14.0f;
	static constexpr float BLOCK_W = BTN_W * 2 + COL_GAP;
	static constexpr float BLOCK_X = (1440.0f - BLOCK_W) / 2.0f;
	static constexpr float ROW1_Y = 140.0f;
	static constexpr float ROW2_Y = ROW1_Y + BTN_H + ROW_GAP;
	static constexpr float ROW3_Y = ROW2_Y + BTN_H + ROW_GAP;
	static constexpr float ROW4_Y = ROW3_Y + BTN_H + ROW_GAP;
	static constexpr float ROW5_Y = ROW4_Y + BTN_H + ROW_GAP;

	SDL_FRect twoDeckButton_{ .x = BLOCK_X, .y = ROW1_Y, .w = BTN_W, .h = BTN_H };
	SDL_FRect sixDeckButton_{ .x = BLOCK_X + BTN_W + COL_GAP, .y = ROW1_Y, .w = BTN_W, .h = BTN_H };
	SDL_FRect twoDeckLuckyLadiesButton_{ .x = BLOCK_X, .y = ROW2_Y, .w = BTN_W, .h = BTN_H };
	SDL_FRect sixDeckLuckyLadiesButton_{ .x = BLOCK_X + BTN_W + COL_GAP, .y = ROW2_Y, .w = BTN_W, .h = BTN_H };
	SDL_FRect twoDeckPlayersEdgeButton_{ .x = BLOCK_X, .y = ROW3_Y, .w = BTN_W, .h = BTN_H };
	SDL_FRect sixDeckPlayersEdgeButton_{ .x = BLOCK_X + BTN_W + COL_GAP, .y = ROW3_Y, .w = BTN_W, .h = BTN_H };
	SDL_FRect luckyStiffButton_{ .x = BLOCK_X, .y = ROW4_Y, .w = BLOCK_W, .h = BTN_H };
	SDL_FRect freeBetButton_{ .x = BLOCK_X, .y = ROW5_Y, .w = BLOCK_W, .h = BTN_H };

	// One per row, to the right of the block (which ends at BLOCK_X +
	// BLOCK_W) -- there's a wide, otherwise-empty margin out there (the
	// block itself is centered with ~400px clear on each side), plenty of
	// room without touching the mode buttons' own layout at all.
	static constexpr float ABOUT_BTN_W = 100.0f;
	static constexpr float ABOUT_BTN_X = BLOCK_X + BLOCK_W + 20.0f;
	SDL_FRect standardAboutButton_{ .x = ABOUT_BTN_X, .y = ROW1_Y, .w = ABOUT_BTN_W, .h = BTN_H };
	SDL_FRect luckyLadiesAboutButton_{ .x = ABOUT_BTN_X, .y = ROW2_Y, .w = ABOUT_BTN_W, .h = BTN_H };
	SDL_FRect playersEdgeAboutButton_{ .x = ABOUT_BTN_X, .y = ROW3_Y, .w = ABOUT_BTN_W, .h = BTN_H };
	SDL_FRect luckyStiffAboutButton_{ .x = ABOUT_BTN_X, .y = ROW4_Y, .w = ABOUT_BTN_W, .h = BTN_H };
	SDL_FRect freeBetAboutButton_{ .x = ABOUT_BTN_X, .y = ROW5_Y, .w = ABOUT_BTN_W, .h = BTN_H };
	// Same size/x as the other screens' BACK buttons, under the last row.
	SDL_FRect backButton_{ .x = 620, .y = ROW5_Y + BTN_H + 40.0f, .w = 200, .h = 56 };

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
		drawButton(state, rect, SDL_Color{70, 70, 70, 255}, "ABOUT", 3.5f);
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
