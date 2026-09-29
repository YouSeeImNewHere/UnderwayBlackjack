#pragma once
#include "Game.h"
#include "DigitFont.h"
#include <string>
#include <vector>

// A read-only reference for the touch gestures Table::processGesture()
// actually recognizes -- opened from the main menu (before a game exists,
// so a new player can learn the controls up front) and from the pause
// menu (as a reminder mid-game). Same structural shape as StrategyChart.h/
// AboutMenu.h (draw()/handlePoint() returning a bool for BACK).
class GesturesMenu
{
public:
	void draw(SDLState& state, Resources& res){
		SDL_SetRenderDrawColor(state.renderer, 10, 30, 15, 255);
		SDL_RenderFillRect(state.renderer, nullptr);

		float titlePixel = 7.0f;
		std::string title = "GESTURES";
		float titleW = DigitFont::textWidth(title, titlePixel);
		DigitFont::drawText(state, title, (1440.0f - titleW) / 2.0f, 50.0f, titlePixel, SDL_Color{255, 255, 255, 255});

		float y = 160.0f;
		for(const auto& [action, gesture] : GESTURES){
			float actionPixel = 5.0f;
			DigitFont::drawText(state, action, 280.0f, y, actionPixel, SDL_Color{200, 180, 100, 255});

			float gesturePixel = 4.5f;
			DigitFont::drawText(state, gesture, 640.0f, y + 3.0f, gesturePixel, SDL_Color{220, 220, 220, 255});

			y += 80.0f;
		}

		drawButton(state, backButton, SDL_Color{80, 80, 80, 255}, "BACK");
	}

	// windowX/windowY: raw event coordinates in window space, same
	// convention as Menu/SetupMenu's handlePoint. Returns true once BACK is
	// hit -- caller (mina.cpp) is responsible for what "back" means (the
	// main menu when opened from there, the pause menu when opened mid-game).
	bool handlePoint(SDLState& state, float windowX, float windowY){
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			return false;

		SDL_FPoint p{x, y};
		return SDL_PointInRectFloat(&p, &backButton);
	}

	std::vector<SDL_FRect> focusRects(){
		return { backButton };
	}

private:
	// Mirrors Table::processGesture() exactly -- update this alongside any
	// change there instead of letting it drift out of date.
	inline static const std::vector<std::pair<std::string, std::string>> GESTURES{
		{"HIT",       "TAP"},
		{"STAND",     "SWIPE LEFT OR RIGHT"},
		{"SURRENDER", "SWIPE UP OR DOWN"},
		{"DOUBLE",    "TWO FINGER TAP"},
		{"SPLIT",     "TWO FINGERS APART"}
	};

	SDL_FRect backButton{ .x = 620, .y = 620, .w = 200, .h = 56 };

	void drawButton(SDLState& state, const SDL_FRect& rect, SDL_Color color, const std::string& label){
		SDL_SetRenderDrawColor(state.renderer, color.r, color.g, color.b, color.a);
		SDL_RenderFillRect(state.renderer, &rect);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &rect);

		// Auto-shrinks to fit -- see GameModeMenu.h's drawButton() for why.
		float pixel = 6.0f;
		float maxW = rect.w - 12.0f;
		float w = DigitFont::textWidth(label, pixel);
		if(w > maxW && w > 0.0f)
			pixel *= maxW / w;

		w = DigitFont::textWidth(label, pixel);
		DigitFont::drawText(state, label, rect.x + (rect.w - w) / 2.0f, rect.y + (rect.h - 5 * pixel) / 2.0f, pixel, SDL_Color{255, 255, 255, 255});
	}
};
