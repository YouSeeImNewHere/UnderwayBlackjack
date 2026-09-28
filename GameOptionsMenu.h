#pragma once
#include "Game.h"
#include "DigitFont.h"
#include <string>

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

	// Table::dealerSpeedFactor's units -- <1 is faster, >1 is slower.
	float dealerSpeedFactor() const {
		switch(dealerSpeed){
			case DealerSpeed::Slow: return 1.6f;
			case DealerSpeed::Fast: return 0.5f;
			default: return 1.0f;
		}
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

		drawRowLabel(state, ROW_Y[1], "FACE-DOWN DOUBLES");
		drawToggleButton(state, toggleButton(1), faceDownDoubles);

		drawRowLabel(state, ROW_Y[2], "HIDE HANDS TIL YOUR TURN");
		drawToggleButton(state, toggleButton(2), hideInactiveHands);

		SDL_FRect go = confirmButton();
		SDL_SetRenderDrawColor(state.renderer, 60, 130, 70, 255);
		SDL_RenderFillRect(state.renderer, &go);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &go);
		float goW = DigitFont::textWidth("GO", 8.0f);
		DigitFont::drawText(state, "GO", go.x + (go.w - goW) / 2.0f, go.y + (go.h - 5 * 8.0f) / 2.0f, 8.0f, WHITE);
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
		if(SDL_PointInRectFloat(&p, &faceDown))
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
	static constexpr SDL_Color WHITE{255, 255, 255, 255};

	// Same left-caption/right-controls shape as SetupMenu's stepper rows,
	// just 3 rows instead of a per-player block -- LABEL_X is where a
	// row's caption starts, CONTROL_X where its buttons start.
	static constexpr float LABEL_X = 380.0f;
	static constexpr float CONTROL_X = 760.0f;
	static constexpr float ROW_H = 70.0f;
	static constexpr float ROW_Y[3] = {160.0f, 280.0f, 400.0f};

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

	SDL_FRect confirmButton(){
		float w = 260.0f, h = 56.0f;
		return SDL_FRect{ .x = (1440.0f - w) / 2.0f, .y = 520.0f, .w = w, .h = h };
	}
};
