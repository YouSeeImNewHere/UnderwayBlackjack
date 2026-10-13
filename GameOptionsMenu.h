#pragma once
#include "Game.h"
#include "DigitFont.h"
#include "GameModeMenu.h"
#include "SaveData.h"
#include "UI.h"
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>

// Shown once, between GameModeMenu and SetupMenu -- dealer pacing and 2
// house-rule toggles that change what's shown and when, not anything about
// bets or player count (that's still SetupMenu's own job, right after
// this). mina.cpp reads dealerSpeedFactor()/faceDownDoubles/hideInactiveHands
// off this once GO is pressed and hands them to Table (see
// Table::setDealerSpeedFactor()/setFaceDownDoubles()/setHideInactiveHands()).
class GameOptionsMenu
{
public:
	// The DEALER SPEED slider: 0 (slowest) to 100 (fastest). Saved as is
	// (SaveData::dealerSpeed).
	int dealerSpeed = SaveData::DEFAULT_DEALER_SPEED;
	bool faceDownDoubles = false;
	bool hideInactiveHands = false;
	// SOUND row: card and chip sound effects.
	bool soundEffects = true;
	// DOUBLE FOR LESS row: pressing double asks how much (up to the bet)
	// instead of always doubling the full bet.
	bool doubleForLess = false;
	// DEALER HITS SOFT 17 row: off = the dealer stands on soft 17 (and
	// the strategy charts switch to match). Free Bet's dealer always hits.
	bool dealerHitsSoft17 = true;
	// INDEX PLAYS row: card counting's chart deviations (by true count)
	// in TIP / practice, a bet ramp while betting, and an insurance hint.
	// Standard games only.
	bool indexPlays = false;
	// AUTO SHUFFLER row: a continuous shuffling machine -- the cards go
	// back in after every round, so counting gets nothing.
	bool autoShuffler = false;

	// Set by mina.cpp right before this screen shows (from GameModeMenu's
	// pick, pre-game, or Table::getGameMode() when reopened from the pause
	// menu mid-game) -- Player's Edge allows doubling down on every split
	// hand at once (its whole "double double double" appeal), so a
	// face-down double there would mean several simultaneously-hidden
	// cards instead of one, defeating the point of the rule. Only offered
	// where at most one double can ever be on the table at a time.
	void setGameMode(GameMode mode){
		gameMode = mode;
		if(isPlayersEdge(gameMode))
			faceDownDoubles = false;
	}

	// The rule switches the current game offers (Free Bet's dealer always
	// hits soft 17; index plays are for the standard games).
	bool soft17Choosable() const { return !isFreeBet(gameMode); }
	bool indexChoosable() const { return !isPlayersEdge(gameMode) && !isFreeBet(gameMode); }

	bool faceDownDoublesAllowed() const {
		return !isPlayersEdge(gameMode);
	}

	// Table::dealerSpeedFactor's units -- <1 is faster, >1 is slower.
	// Exponential along the slider, so each step feels like the same
	// change whether it's near the slow or the fast end: 2.5 (0.4x) at 0,
	// about 1.0 at the default 40, 0.25 (4x) at 100.
	float dealerSpeedFactor() const {
		return SLOWEST_FACTOR * std::pow(FASTEST_FACTOR / SLOWEST_FACTOR, dealerSpeed / 100.0f);
	}

	// The speed as a multiple of normal, e.g. "1.5X".
	std::string speedLabel() const {
		char buf[16];
		std::snprintf(buf, sizeof(buf), "%.1fX", 1.0f / dealerSpeedFactor());
		return buf;
	}

	// Arrow keys on the highlighted slider (see mina.cpp).
	void nudgeSpeed(int delta){
		dealerSpeed = std::clamp(dealerSpeed + delta, 0, 100);
	}

	// focusRects() index of the slider -- mina.cpp gives it Left/Right
	// instead of moving the highlight.
	static constexpr int SLIDER_FOCUS_INDEX = 0;

	// Dragging the slider: a press on it starts a drag that follows the
	// pointer until release (handlePoint(), from mina.cpp's mouse/finger up).
	void pointerDown(SDLState& state, float windowX, float windowY){
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			return;
		SDL_FPoint p{x, y};
		SDL_FRect hit = sliderHitRect();
		if(SDL_PointInRectFloat(&p, &hit)){
			dragging = true;
			setSpeedFromX(x);
		}
	}

	void pointerMove(SDLState& state, float windowX, float windowY){
		float x, y;
		if(dragging && SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			setSpeedFromX(x);
	}

	// A live demo card sliding down its own track, at whatever duration
	// the currently-selected speed would actually give a real deal (same
	// base duration as Table::DEAL_DURATION -- this menu has no access to
	// Table's own private constant, so it just mirrors the number) -- so
	// changing the speed is something you can *see* immediately, not just
	// a label that may as well not do anything. Loops on its own with a
	// short pause at the top between passes. Called every frame this
	// screen is showing (see mina.cpp).
	void update(float deltaTime){
		float duration = DEMO_BASE_DURATION * dealerSpeedFactor();
		demoElapsed += deltaTime;
		if(demoElapsed >= duration + DEMO_LOOP_PAUSE)
			demoElapsed = 0.0f;
	}

	void draw(SDLState& state, Resources& res){
		UI::background(state);
		UI::title(state, "GAME OPTIONS");

		drawRowLabel(state, ROW_Y[0], "DEALER SPEED");
		drawSlider(state);

		bool doublesAllowed = faceDownDoublesAllowed();
		drawRowLabel(state, ROW_Y[1], doublesAllowed ? "FACE-DOWN DOUBLES" : "FACE-DOWN DOUBLES - N/A");
		UI::toggle(state, toggleButton(1), doublesAllowed && faceDownDoubles, doublesAllowed);

		drawRowLabel(state, ROW_Y[2], "HIDE OTHER HANDS");
		UI::toggle(state, toggleButton(2), hideInactiveHands);

		drawRowLabel(state, ROW_Y[3], "SOUND");
		UI::toggle(state, toggleButton(3), soundEffects);

		drawRowLabel(state, ROW_Y[4], "DOUBLE FOR LESS");
		UI::toggle(state, toggleButton(4), doubleForLess);

		bool soft17Choice = !isFreeBet(gameMode);
		drawRowLabel(state, ROW_Y[5], soft17Choice ? "DEALER HITS SOFT 17" : "ALWAYS HITS SOFT 17");
		UI::toggle(state, toggleButton(5), soft17Choice ? dealerHitsSoft17 : true, soft17Choice);

		bool indexChoice = !isPlayersEdge(gameMode) && !isFreeBet(gameMode);
		drawRowLabel(state, ROW_Y[6], indexChoice ? "INDEX PLAYS" : "INDEX PLAYS - N/A");
		UI::toggle(state, toggleButton(6), indexChoice && indexPlays, indexChoice);

		drawRowLabel(state, ROW_Y[7], "AUTO SHUFFLER");
		UI::toggle(state, toggleButton(7), autoShuffler);
		if(autoShuffler){
			// Why the count stays near zero, beside the switch.
			float x = CONTROL_X + TOGGLE_BTN_W + 16.0f;
			UI::text(state, "SHUFFLED EVERY ROUND", x, ROW_Y[7] + 6.0f, UI::SMALL, UI::DIM);
			UI::text(state, "COUNTING DOES NOT WORK", x, ROW_Y[7] + ROW_H - 5 * UI::SMALL - 6.0f, UI::SMALL, UI::DIM);
		}

		drawSpeedDemo(state, res);

		UI::button(state, confirmButton(), UI::PRIMARY, "GO", UI::TITLE);
		UI::button(state, backButton(), UI::SECONDARY, "BACK", UI::LARGE);
	}

	// True when BACK is hit -- mina.cpp decides where back goes.
	bool handleBackPoint(SDLState& state, float windowX, float windowY){
		if(dragging)
			return false;
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			return false;

		SDL_FPoint p{x, y};
		SDL_FRect back = backButton();
		return SDL_PointInRectFloat(&p, &back);
	}

	// Every enabled control, for mina.cpp's arrow-key navigation.
	std::vector<SDL_FRect> focusRects(){
		std::vector<SDL_FRect> rects{ sliderRect() }; // SLIDER_FOCUS_INDEX
		if(faceDownDoublesAllowed())
			rects.push_back(toggleButton(1));
		rects.push_back(toggleButton(2));
		rects.push_back(toggleButton(3));
		rects.push_back(toggleButton(4));
		if(!isFreeBet(gameMode))
			rects.push_back(toggleButton(5));
		if(!isPlayersEdge(gameMode) && !isFreeBet(gameMode))
			rects.push_back(toggleButton(6));
		rects.push_back(toggleButton(7));
		rects.push_back(backButton());
		rects.push_back(confirmButton());
		return rects;
	}

	// windowX/windowY: raw event coordinates in window space, same
	// convention as every other menu's handlePoint. Returns true only once,
	// when GO is tapped/clicked -- the caller's signal to bake these 3
	// choices into Table and move on to SetupMenu.
	bool handlePoint(SDLState& state, float windowX, float windowY){
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			return false;

		SDL_FPoint p{x, y};

		// The end of a slider drag: wherever it's released, nothing else
		// on the screen gets clicked.
		if(dragging){
			setSpeedFromX(x);
			dragging = false;
			return false;
		}
		SDL_FRect slider = sliderHitRect();
		if(SDL_PointInRectFloat(&p, &slider)){
			setSpeedFromX(x);
			return false;
		}

		SDL_FRect faceDown = toggleButton(1);
		if(faceDownDoublesAllowed() && SDL_PointInRectFloat(&p, &faceDown))
			faceDownDoubles = !faceDownDoubles;

		SDL_FRect hideHands = toggleButton(2);
		if(SDL_PointInRectFloat(&p, &hideHands))
			hideInactiveHands = !hideInactiveHands;

		SDL_FRect soundButton = toggleButton(3);
		if(SDL_PointInRectFloat(&p, &soundButton))
			soundEffects = !soundEffects;

		SDL_FRect lessButton = toggleButton(4);
		if(SDL_PointInRectFloat(&p, &lessButton))
			doubleForLess = !doubleForLess;

		SDL_FRect soft17Button = toggleButton(5);
		if(!isFreeBet(gameMode) && SDL_PointInRectFloat(&p, &soft17Button))
			dealerHitsSoft17 = !dealerHitsSoft17;

		SDL_FRect indexButton = toggleButton(6);
		if(!isPlayersEdge(gameMode) && !isFreeBet(gameMode) && SDL_PointInRectFloat(&p, &indexButton))
			indexPlays = !indexPlays;

		SDL_FRect shufflerButton = toggleButton(7);
		if(SDL_PointInRectFloat(&p, &shufflerButton))
			autoShuffler = !autoShuffler;

		SDL_FRect confirm = confirmButton();
		if(SDL_PointInRectFloat(&p, &confirm))
			return true;

		return false;
	}

private:
	GameMode gameMode = GameMode::TwoDeck;

	static constexpr SDL_Color WHITE{255, 255, 255, 255};

	// Same left-caption/right-controls shape as SetupMenu's stepper rows,
	// just 8 rows instead of a per-player block -- LABEL_X is where a
	// row's caption starts, CONTROL_X where its buttons start.
	static constexpr float LABEL_X = 230.0f;
	static constexpr float CONTROL_X = 760.0f;
	static constexpr float ROW_H = 52.0f;
	static constexpr float ROW_Y[8] = {110.0f, 214.0f, 274.0f, 334.0f, 394.0f, 454.0f, 514.0f, 574.0f};

	// Demo card track -- off to the right of the 3 rows above, clear of
	// their controls (the slider ends at CONTROL_X + SLIDER_W, 1200).
	static constexpr float DEMO_X = 1250.0f;
	static constexpr float DEMO_TOP_Y = 130.0f;
	static constexpr float DEMO_BOTTOM_Y = 410.0f;
	static constexpr float DEMO_BASE_DURATION = 0.6f; // mirrors Table::DEAL_DURATION
	static constexpr float DEMO_LOOP_PAUSE = 0.4f;
	float demoElapsed = 0.0f;

	void drawSpeedDemo(SDLState& state, Resources& res){
		float duration = DEMO_BASE_DURATION * dealerSpeedFactor();
		float progress = std::min(1.0f, demoElapsed / duration);
		float y = DEMO_TOP_Y + (DEMO_BOTTOM_Y - DEMO_TOP_Y) * progress;

		SDL_FRect card{ .x = DEMO_X, .y = y, .w = cardWidth, .h = cardHeight };
		SDL_RenderTexture(state.renderer, res.allCards, &backOFCard, &card);

		UI::centered(state, "SPEED", DEMO_TOP_Y - 5 * UI::SMALL - 8.0f, UI::SMALL, UI::DIM, DEMO_X + cardWidth / 2.0f);
	}

	void drawRowLabel(SDLState& state, float y, const std::string& label){
		UI::label(state, label, LABEL_X, y, ROW_H);
	}

	static constexpr float SLOWEST_FACTOR = 2.5f;
	static constexpr float FASTEST_FACTOR = 0.25f;
	static constexpr float SLIDER_W = 440.0f;
	static constexpr float KNOB_W = 30.0f; // UI::slider's knob
	bool dragging = false;

	// The whole slider row: what arrow keys highlight.
	SDL_FRect sliderRect(){
		return SDL_FRect{ .x = CONTROL_X, .y = ROW_Y[0], .w = SLIDER_W, .h = ROW_H };
	}

	// Where a press grabs the slider: its row plus some slack above and
	// below, so a thumb doesn't have to land exactly on the thin track.
	SDL_FRect sliderHitRect(){
		return SDL_FRect{ .x = CONTROL_X - 20.0f, .y = ROW_Y[0] - 30.0f, .w = SLIDER_W + 40.0f, .h = ROW_H + 60.0f };
	}

	// The knob's center travels between these, so it never hangs off an end.
	float trackStart(){ return CONTROL_X + KNOB_W / 2.0f; }
	float trackEnd(){ return CONTROL_X + SLIDER_W - KNOB_W / 2.0f; }

	void setSpeedFromX(float x){
		float t = (x - trackStart()) / (trackEnd() - trackStart());
		dealerSpeed = std::clamp((int)std::lround(t * 100.0f), 0, 100);
	}

	void drawSlider(SDLState& state){
		UI::slider(state, sliderRect(), dealerSpeed, speedLabel(), "SLOW", "FAST");
	}

	static constexpr float TOGGLE_BTN_W = 160.0f;

	SDL_FRect toggleButton(int row){
		return SDL_FRect{ .x = CONTROL_X, .y = ROW_Y[row], .w = TOGGLE_BTN_W, .h = ROW_H };
	}

	// GO and BACK sit side by side, the pair centered.
	SDL_FRect confirmButton(){ return UI::pairGoRect(); }

	SDL_FRect backButton(){ return UI::pairBackRect(); }
};
