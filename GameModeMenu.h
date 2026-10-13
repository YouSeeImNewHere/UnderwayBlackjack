#pragma once
#include "Game.h"
#include "DigitFont.h"
#include "UI.h"
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
		UI::background(state);
		std::string title = training == Training::Practice ? "PRACTICE: PICK A STRATEGY TABLE"
			: training == Training::CountQuiz ? "COUNT QUIZ: PICK A GAME" : "CHOOSE A GAME";
		UI::title(state, title);

		// A list: each game's name in gold, a button per deck count, and
		// RULES (that game's rules page, ABOUT before).
		static const char* NAMES[5] = { "STANDARD", "LUCKY LADIES", "PLAYER'S EDGE", "LUCKY STIFF", "FREE BET" };
		static const float ROWS[5] = { ROW1_Y, ROW2_Y, ROW3_Y, ROW4_Y, ROW5_Y };
		for(int i = 0; i < 5; i++)
			UI::label(state, NAMES[i], NAME_X, ROWS[i], BTN_H, UI::LARGE, UI::ACCENT);
		UI::button(state, twoDeckButton(), UI::NAV, "2 DECK", UI::LARGE);
		UI::button(state, sixDeckButton(), UI::NAV, "6 DECK", UI::LARGE);
		UI::button(state, twoDeckLuckyLadiesButton(), UI::NAV, "2 DECK", UI::LARGE);
		UI::button(state, sixDeckLuckyLadiesButton(), UI::NAV, "6 DECK", UI::LARGE);
		UI::button(state, twoDeckPlayersEdgeButton(), UI::NAV, "2 DECK", UI::LARGE);
		UI::button(state, sixDeckPlayersEdgeButton(), UI::NAV, "6 DECK", UI::LARGE);
		UI::button(state, luckyStiffButton(), UI::NAV, "8 DECK", UI::LARGE);
		UI::button(state, freeBetButton(), UI::NAV, "6 DECK", UI::LARGE);
		if(training == Training::None){
			UI::label(state, "TRAINING", NAME_X, ROW6_Y, BTN_H, UI::LARGE, UI::DIM);
			UI::button(state, practiceButton_, UI::NAV, "PRACTICE", UI::BODY);
			UI::button(state, countQuizButton_, UI::NAV, "COUNT QUIZ", UI::BODY);
		} else{
			UI::centered(state, training == Training::Practice
				? "OFF-CHART MOVES ARE CAUGHT BEFORE THEY PLAY"
				: "AFTER EVERY ROUND, NAME THE RUNNING COUNT", ROW6_Y + (BTN_H - 5 * UI::BODY) / 2.0f, UI::BODY, UI::DIM);
		}

		// One RULES button per row, not per deck count -- the rules text
		// for a variant doesn't differ between its 2-deck and 6-deck
		// buttons (AboutMenu's rulesFor()/sideBetsFor() only branch on the
		// game family, e.g. hasLuckyLadies(), not on deck count).
		drawAboutButton(state, standardAboutButton());
		drawAboutButton(state, luckyLadiesAboutButton());
		drawAboutButton(state, playersEdgeAboutButton());
		drawAboutButton(state, luckyStiffAboutButton());
		drawAboutButton(state, freeBetAboutButton());

		UI::button(state, backButton_, UI::SECONDARY, "BACK", UI::LARGE);
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
	static constexpr float BTN_W = 256.0f;
	static constexpr float BTN_H = 62.0f;
	static constexpr float COL_GAP = 20.0f;
	static constexpr float ROW_GAP = 12.0f;
	static constexpr float NAME_X = 150.0f;
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
	static constexpr float ABOUT_BTN_W = 140.0f;
	static constexpr float ABOUT_BTN_X = BLOCK_X + BLOCK_W + 20.0f;
	SDL_FRect standardAboutButton_{ .x = ABOUT_BTN_X, .y = ROW1_Y, .w = ABOUT_BTN_W, .h = BTN_H };
	SDL_FRect luckyLadiesAboutButton_{ .x = ABOUT_BTN_X, .y = ROW2_Y, .w = ABOUT_BTN_W, .h = BTN_H };
	SDL_FRect playersEdgeAboutButton_{ .x = ABOUT_BTN_X, .y = ROW3_Y, .w = ABOUT_BTN_W, .h = BTN_H };
	SDL_FRect luckyStiffAboutButton_{ .x = ABOUT_BTN_X, .y = ROW4_Y, .w = ABOUT_BTN_W, .h = BTN_H };
	SDL_FRect freeBetAboutButton_{ .x = ABOUT_BTN_X, .y = ROW5_Y, .w = ABOUT_BTN_W, .h = BTN_H };
	// Same size/x as the other screens' BACK buttons, under the last row.
	SDL_FRect backButton_ = UI::backRect();

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

	void drawAboutButton(SDLState& state, const SDL_FRect& rect){
		UI::button(state, rect, UI::SECONDARY, "RULES", UI::BODY);
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
