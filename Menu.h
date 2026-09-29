#pragma once
#include "Game.h"
#include "DigitFont.h"
#include <string>
#include <vector>

enum class MenuChoice{
	None,
	Start,
	Resume,
	Restart,
	Gestures,
	Keyboard
};

// The very first thing the player sees.
class Menu
{
public:
	// Set from SaveData before the menu is ever drawn: true once the player
	// has started a game before (this launch or a previous one), which
	// swaps the single Start button for Resume + Restart instead.
	bool hasSavedGame = false;

	void draw(SDLState& state, Resources& res){
		SDL_SetRenderDrawColor(state.renderer, 20, 70, 35, 255);
		SDL_RenderFillRect(state.renderer, nullptr);

		if(hasSavedGame){
			drawButton(state, resumeButton, SDL_Color{60, 130, 70, 255}, "RESUME");
			drawButton(state, restartButton, SDL_Color{150, 60, 60, 255}, "RESTART");
		} else{
			drawButton(state, startButton, SDL_Color{60, 130, 70, 255}, "START");
		}

		drawButton(state, gesturesButton, SDL_Color{60, 90, 150, 255}, "GESTURES");
		drawButton(state, keyboardButton, SDL_Color{110, 90, 60, 255}, "KEYBOARD");
	}

	// windowX/windowY: raw event coordinates in window space (SDL_EVENT_
	// MOUSE_BUTTON_UP's event.button.x/y, or a finger event's normalized
	// x/y already multiplied back out to window pixels by the caller).
	// Converted here into the renderer's logical (letterboxed) coordinate
	// space before hit-testing, so it lines up with where draw() actually
	// puts the buttons regardless of window size/aspect.
	MenuChoice handlePoint(SDLState& state, float windowX, float windowY){
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			return MenuChoice::None;

		SDL_FPoint p{x, y};

		if(hasSavedGame){
			if(SDL_PointInRectFloat(&p, &resumeButton))
				return MenuChoice::Resume;
			if(SDL_PointInRectFloat(&p, &restartButton))
				return MenuChoice::Restart;
		} else if(SDL_PointInRectFloat(&p, &startButton)){
			return MenuChoice::Start;
		}

		if(SDL_PointInRectFloat(&p, &gesturesButton))
			return MenuChoice::Gestures;
		if(SDL_PointInRectFloat(&p, &keyboardButton))
			return MenuChoice::Keyboard;

		return MenuChoice::None;
	}

	// Every button currently on screen, for mina.cpp's arrow-key
	// navigation (it "clicks" the highlighted one via handlePoint()).
	std::vector<SDL_FRect> focusRects(){
		if(hasSavedGame)
			return { resumeButton, restartButton, gesturesButton, keyboardButton };
		return { startButton, gesturesButton, keyboardButton };
	}

private:
	SDL_FRect startButton{ .x = 570, .y = 300, .w = 300, .h = 90 };
	SDL_FRect resumeButton{ .x = 570, .y = 260, .w = 300, .h = 80 };
	SDL_FRect restartButton{ .x = 570, .y = 370, .w = 300, .h = 80 };
	SDL_FRect gesturesButton{ .x = 570, .y = 470, .w = 300, .h = 70 };
	SDL_FRect keyboardButton{ .x = 570, .y = 555, .w = 300, .h = 70 };

	void drawButton(SDLState& state, const SDL_FRect& rect, SDL_Color color, const std::string& label){
		SDL_SetRenderDrawColor(state.renderer, color.r, color.g, color.b, color.a);
		SDL_RenderFillRect(state.renderer, &rect);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &rect);

		// Auto-shrinks to fit -- see GameModeMenu.h's drawButton() for why.
		float pixel = 8.0f;
		float maxW = rect.w - 12.0f;
		float w = DigitFont::textWidth(label, pixel);
		if(w > maxW && w > 0.0f)
			pixel *= maxW / w;

		w = DigitFont::textWidth(label, pixel);
		DigitFont::drawText(state, label, rect.x + (rect.w - w) / 2.0f, rect.y + (rect.h - 5 * pixel) / 2.0f, pixel, SDL_Color{255, 255, 255, 255});
	}
};
