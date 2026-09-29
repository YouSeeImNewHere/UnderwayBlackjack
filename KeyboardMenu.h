#pragma once
#include "Game.h"
#include "DigitFont.h"
#include <string>
#include <vector>

// Keyboard counterpart to GesturesMenu.h -- same shape (draw()/handlePoint()
// returning a bool for BACK), opened from the main menu and the pause menu.
class KeyboardMenu
{
public:
	void draw(SDLState& state, Resources& res){
		SDL_SetRenderDrawColor(state.renderer, 10, 30, 15, 255);
		SDL_RenderFillRect(state.renderer, nullptr);

		float titlePixel = 7.0f;
		std::string title = "KEYBOARD";
		float titleW = DigitFont::textWidth(title, titlePixel);
		DigitFont::drawText(state, title, (1440.0f - titleW) / 2.0f, 40.0f, titlePixel, SDL_Color{255, 255, 255, 255});

		float y = 120.0f;
		for(const auto& [action, key] : KEYS){
			DigitFont::drawText(state, action, 360.0f, y, 4.5f, SDL_Color{200, 180, 100, 255});
			DigitFont::drawText(state, key, 760.0f, y, 4.5f, SDL_Color{220, 220, 220, 255});
			y += 52.0f;
		}

		drawButton(state, backButton, SDL_Color{80, 80, 80, 255}, "BACK");
	}

	// Same convention as GesturesMenu::handlePoint -- true once BACK is hit;
	// mina.cpp decides where back goes.
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
	// Mirrors Table::handleEvent()'s key bindings and mina.cpp's menu/
	// Space/Esc handling -- update alongside either.
	inline static const std::vector<std::pair<std::string, std::string>> KEYS{
		{"HIT",          "H"},
		{"STAND",        "S"},
		{"DOUBLE",       "D"},
		{"SPLIT",        "P"},
		{"SURRENDER",    "R"},
		{"DEAL",         "SPACE"},
		{"PAUSE / BACK", "ESC"},
		{"MENU SELECT",  "ARROW KEYS"},
		{"MENU CONFIRM", "ENTER OR SPACE"}
	};

	SDL_FRect backButton{ .x = 620, .y = 620, .w = 200, .h = 56 };

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
