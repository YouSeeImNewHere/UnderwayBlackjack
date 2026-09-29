#pragma once
#include "Game.h"
#include "DigitFont.h"
#include "GameModeMenu.h"
#include <string>
#include <vector>
#include <algorithm>

// Shown once, between GameModeMenu and SetupMenu -- dealer pacing and 2
// house-rule toggles that change what's shown and when, not anything about
// bets or player count (that's still SetupMenu's own job, right after
// this). mina.cpp reads dealerSpeedFactor()/faceDownDoubles/hideInactiveHands
// off this once GO is pressed and hands them to Table (see
// Table::setDealerSpeedFactor()/setFaceDownDoubles()/setHideInactiveHands()).
class GameOptionsMenu
{
public:
	enum class DealerSpeed{ Slow, Normal, Fast };

	DealerSpeed dealerSpeed = DealerSpeed::Normal;
	bool faceDownDoubles = false;
	bool hideInactiveHands = false;

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

	bool faceDownDoublesAllowed() const {
		return !isPlayersEdge(gameMode);
	}

	// Table::dealerSpeedFactor's units -- <1 is faster, >1 is slower. Wide
	// enough gap between tiers that it's obvious on every single deal, not
	// just the 2 rare dealer-only pauses this used to be limited to.
	float dealerSpeedFactor() const {
		switch(dealerSpeed){
			case DealerSpeed::Slow: return 2.2f;
			case DealerSpeed::Fast: return 0.35f;
			default: return 1.0f;
		}
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
		SDL_SetRenderDrawColor(state.renderer, 20, 70, 35, 255);
		SDL_RenderFillRect(state.renderer, nullptr);

		float titlePixel = 7.0f;
		std::string title = "GAME OPTIONS";
		float titleW = DigitFont::textWidth(title, titlePixel);
		DigitFont::drawText(state, title, (1440.0f - titleW) / 2.0f, 50.0f, titlePixel, WHITE);

		drawRowLabel(state, ROW_Y[0], "DEALER SPEED");
		drawSpeedButton(state, speedButton(0), "SLOW", dealerSpeed == DealerSpeed::Slow);
		drawSpeedButton(state, speedButton(1), "NORMAL", dealerSpeed == DealerSpeed::Normal);
		drawSpeedButton(state, speedButton(2), "FAST", dealerSpeed == DealerSpeed::Fast);

		bool doublesAllowed = faceDownDoublesAllowed();
		drawRowLabel(state, ROW_Y[1], doublesAllowed ? "FACE-DOWN DOUBLES" : "FACE-DOWN DOUBLES (N/A)");
		if(doublesAllowed)
			drawToggleButton(state, toggleButton(1), faceDownDoubles);
		else{
			SDL_FRect r = toggleButton(1);
			SDL_SetRenderDrawColor(state.renderer, 50, 50, 50, 255);
			SDL_RenderFillRect(state.renderer, &r);
			SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
			SDL_RenderRect(state.renderer, &r);
		}

		drawRowLabel(state, ROW_Y[2], "HIDE HANDS TIL YOUR TURN");
		drawToggleButton(state, toggleButton(2), hideInactiveHands);

		drawSpeedDemo(state, res);

		SDL_FRect go = confirmButton();
		SDL_SetRenderDrawColor(state.renderer, 60, 130, 70, 255);
		SDL_RenderFillRect(state.renderer, &go);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &go);
		float goW = DigitFont::textWidth("GO", 8.0f);
		DigitFont::drawText(state, "GO", go.x + (go.w - goW) / 2.0f, go.y + (go.h - 5 * 8.0f) / 2.0f, 8.0f, WHITE);

		SDL_FRect back = backButton();
		SDL_SetRenderDrawColor(state.renderer, 80, 80, 80, 255);
		SDL_RenderFillRect(state.renderer, &back);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &back);
		float backW = DigitFont::textWidth("BACK", 6.0f);
		DigitFont::drawText(state, "BACK", back.x + (back.w - backW) / 2.0f, back.y + (back.h - 5 * 6.0f) / 2.0f, 6.0f, WHITE);
	}

	// True when BACK is hit -- mina.cpp decides where back goes.
	bool handleBackPoint(SDLState& state, float windowX, float windowY){
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			return false;

		SDL_FPoint p{x, y};
		SDL_FRect back = backButton();
		return SDL_PointInRectFloat(&p, &back);
	}

	// Every enabled control, for mina.cpp's arrow-key navigation.
	std::vector<SDL_FRect> focusRects(){
		std::vector<SDL_FRect> rects{ speedButton(0), speedButton(1), speedButton(2) };
		if(faceDownDoublesAllowed())
			rects.push_back(toggleButton(1));
		rects.push_back(toggleButton(2));
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

		SDL_FRect slow = speedButton(0), normal = speedButton(1), fast = speedButton(2);
		if(SDL_PointInRectFloat(&p, &slow))
			dealerSpeed = DealerSpeed::Slow;
		else if(SDL_PointInRectFloat(&p, &normal))
			dealerSpeed = DealerSpeed::Normal;
		else if(SDL_PointInRectFloat(&p, &fast))
			dealerSpeed = DealerSpeed::Fast;

		SDL_FRect faceDown = toggleButton(1);
		if(faceDownDoublesAllowed() && SDL_PointInRectFloat(&p, &faceDown))
			faceDownDoubles = !faceDownDoubles;

		SDL_FRect hideHands = toggleButton(2);
		if(SDL_PointInRectFloat(&p, &hideHands))
			hideInactiveHands = !hideInactiveHands;

		SDL_FRect confirm = confirmButton();
		if(SDL_PointInRectFloat(&p, &confirm))
			return true;

		return false;
	}

private:
	GameMode gameMode = GameMode::TwoDeck;

	static constexpr SDL_Color WHITE{255, 255, 255, 255};

	// Same left-caption/right-controls shape as SetupMenu's stepper rows,
	// just 3 rows instead of a per-player block -- LABEL_X is where a
	// row's caption starts, CONTROL_X where its buttons start.
	static constexpr float LABEL_X = 380.0f;
	static constexpr float CONTROL_X = 760.0f;
	static constexpr float ROW_H = 70.0f;
	static constexpr float ROW_Y[3] = {160.0f, 280.0f, 400.0f};

	// Demo card track -- off to the right of the 3 rows above, clear of
	// their controls (which end around CONTROL_X + 3*SPEED_BTN_W, ~1198).
	static constexpr float DEMO_X = 1250.0f;
	static constexpr float DEMO_TOP_Y = 150.0f;
	static constexpr float DEMO_BOTTOM_Y = 480.0f;
	static constexpr float DEMO_BASE_DURATION = 0.6f; // mirrors Table::DEAL_DURATION
	static constexpr float DEMO_LOOP_PAUSE = 0.4f;
	float demoElapsed = 0.0f;

	void drawSpeedDemo(SDLState& state, Resources& res){
		float duration = DEMO_BASE_DURATION * dealerSpeedFactor();
		float progress = std::min(1.0f, demoElapsed / duration);
		float y = DEMO_TOP_Y + (DEMO_BOTTOM_Y - DEMO_TOP_Y) * progress;

		SDL_FRect card{ .x = DEMO_X, .y = y, .w = cardWidth, .h = cardHeight };
		SDL_RenderTexture(state.renderer, res.allCards, &backOFCard, &card);

		std::string label = "SPEED";
		float pixel = 3.5f;
		float w = DigitFont::textWidth(label, pixel);
		DigitFont::drawText(state, label, DEMO_X + (cardWidth - w) / 2.0f, DEMO_TOP_Y - 5 * pixel - 8.0f, pixel, WHITE);
	}

	void drawRowLabel(SDLState& state, float y, const std::string& label){
		float pixel = 5.0f;
		float maxW = CONTROL_X - LABEL_X - 20.0f;
		float w = DigitFont::textWidth(label, pixel);
		if(w > maxW && w > 0.0f)
			pixel *= maxW / w;
		DigitFont::drawText(state, label, LABEL_X, y + (ROW_H - 5 * pixel) / 2.0f, pixel, WHITE);
	}

	static constexpr float SPEED_BTN_W = 130.0f;
	static constexpr float SPEED_BTN_GAP = 16.0f;

	SDL_FRect speedButton(int index){
		return SDL_FRect{ .x = CONTROL_X + index * (SPEED_BTN_W + SPEED_BTN_GAP), .y = ROW_Y[0], .w = SPEED_BTN_W, .h = ROW_H };
	}

	void drawSpeedButton(SDLState& state, const SDL_FRect& r, const std::string& label, bool selected){
		SDL_SetRenderDrawColor(state.renderer, selected ? 60 : 70, selected ? 130 : 70, selected ? 70 : 80, 255);
		SDL_RenderFillRect(state.renderer, &r);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &r);

		float pixel = 4.5f;
		float maxW = r.w - 8.0f;
		float w = DigitFont::textWidth(label, pixel);
		if(w > maxW && w > 0.0f)
			pixel *= maxW / w;
		w = DigitFont::textWidth(label, pixel);
		DigitFont::drawText(state, label, r.x + (r.w - w) / 2.0f, r.y + (r.h - 5 * pixel) / 2.0f, pixel, WHITE);
	}

	static constexpr float TOGGLE_BTN_W = 160.0f;

	SDL_FRect toggleButton(int row){
		return SDL_FRect{ .x = CONTROL_X, .y = ROW_Y[row], .w = TOGGLE_BTN_W, .h = ROW_H };
	}

	void drawToggleButton(SDLState& state, const SDL_FRect& r, bool on){
		SDL_SetRenderDrawColor(state.renderer, on ? 60 : 80, on ? 130 : 80, on ? 70 : 80, 255);
		SDL_RenderFillRect(state.renderer, &r);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &r);

		std::string label = on ? "ON" : "OFF";
		float pixel = 6.0f;
		float w = DigitFont::textWidth(label, pixel);
		DigitFont::drawText(state, label, r.x + (r.w - w) / 2.0f, r.y + (r.h - 5 * pixel) / 2.0f, pixel, WHITE);
	}

	// GO and BACK sit side by side, the pair centered.
	SDL_FRect confirmButton(){
		float w = 260.0f, h = 56.0f;
		return SDL_FRect{ .x = 720.0f + 10.0f, .y = 520.0f, .w = w, .h = h };
	}

	SDL_FRect backButton(){
		float w = 200.0f, h = 56.0f;
		return SDL_FRect{ .x = 720.0f - 10.0f - w, .y = 520.0f, .w = w, .h = h };
	}
};
