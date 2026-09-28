#pragma once
#include "Game.h"
#include "DigitFont.h"
#include <string>

enum class PauseChoice{
	None,
	Resume,
	Restart,
	StrategyTable,
	About,
	Gestures
};

// Drawn as a dim overlay on top of the (frozen, still-visible) table --
// mina.cpp stops calling Table::update()/dealDealer() while this is up,
// but still draws the table underneath so the board doesn't just vanish.
class PauseMenu
{
public:
	void draw(SDLState& state, Resources& res){
		SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(state.renderer, 0, 0, 0, 165);
		SDL_RenderFillRect(state.renderer, nullptr);
		SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_NONE);

		drawButton(state, resumeButton, SDL_Color{60, 130, 70, 255}, "RESUME");
		drawButton(state, restartButton, SDL_Color{150, 60, 60, 255}, "RESTART");
		drawButton(state, strategyButton, SDL_Color{60, 90, 150, 255}, "STRATEGY TABLE");
		drawButton(state, aboutButton, SDL_Color{110, 90, 60, 255}, "ABOUT");
		drawButton(state, gesturesButton, SDL_Color{90, 70, 130, 255}, "GESTURES");
	}

	// windowX/windowY: raw event coordinates in window space, same
	// convention as Menu/SetupMenu's handlePoint.
	PauseChoice handlePoint(SDLState& state, float windowX, float windowY){
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			return PauseChoice::None;

		SDL_FPoint p{x, y};

		if(SDL_PointInRectFloat(&p, &resumeButton))
			return PauseChoice::Resume;
		if(SDL_PointInRectFloat(&p, &restartButton))
			return PauseChoice::Restart;
		if(SDL_PointInRectFloat(&p, &strategyButton))
			return PauseChoice::StrategyTable;
		if(SDL_PointInRectFloat(&p, &aboutButton))
			return PauseChoice::About;
		if(SDL_PointInRectFloat(&p, &gesturesButton))
			return PauseChoice::Gestures;

		return PauseChoice::None;
	}

private:
	// Spacing tightened to fit a 5th button in the same vertical span the
	// original 3 used.
	SDL_FRect resumeButton{ .x = 530, .y = 210, .w = 380, .h = 66 };
	SDL_FRect restartButton{ .x = 530, .y = 288, .w = 380, .h = 66 };
	SDL_FRect strategyButton{ .x = 530, .y = 366, .w = 380, .h = 66 };
	SDL_FRect aboutButton{ .x = 530, .y = 444, .w = 380, .h = 66 };
	SDL_FRect gesturesButton{ .x = 530, .y = 522, .w = 380, .h = 66 };

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
