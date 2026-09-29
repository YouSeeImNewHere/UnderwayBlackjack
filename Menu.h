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
	Keyboard,
	Stats,
	Tutorial
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

		float titlePixel = 9.0f;
		std::string title = "BLACKJACK VARIANTS";
		float titleW = DigitFont::textWidth(title, titlePixel);
		DigitFont::drawText(state, title, (1440.0f - titleW) / 2.0f, 60.0f, titlePixel, SDL_Color{255, 225, 80, 255});

		if(hasSavedGame){
			drawButton(state, resumeButton, SDL_Color{60, 130, 70, 255}, "RESUME");
			drawButton(state, restartButton, SDL_Color{150, 60, 60, 255}, "RESTART");
		} else{
			drawButton(state, startButton, SDL_Color{60, 130, 70, 255}, "START");
		}

		drawButton(state, tutorialButton, SDL_Color{50, 120, 130, 255}, "HOW TO PLAY");
		drawButton(state, statsButton, SDL_Color{120, 70, 130, 255}, "STATS");
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
		if(SDL_PointInRectFloat(&p, &statsButton))
			return MenuChoice::Stats;
		if(SDL_PointInRectFloat(&p, &tutorialButton))
			return MenuChoice::Tutorial;

		return MenuChoice::None;
	}

	// Every button currently on screen, for mina.cpp's arrow-key
	// navigation (it "clicks" the highlighted one via handlePoint()).
	std::vector<SDL_FRect> focusRects(){
		if(hasSavedGame)
			return { resumeButton, restartButton, tutorialButton, statsButton, gesturesButton, keyboardButton };
		return { startButton, tutorialButton, statsButton, gesturesButton, keyboardButton };
	}

private:
	// Title, then the big play button(s), then a 2x2 grid of the
	// reference pages underneath.
	SDL_FRect startButton{ .x = 570, .y = 190, .w = 300, .h = 110 };
	SDL_FRect resumeButton{ .x = 570, .y = 170, .w = 300, .h = 80 };
	SDL_FRect restartButton{ .x = 570, .y = 262, .w = 300, .h = 70 };
	SDL_FRect tutorialButton{ .x = 410, .y = 400, .w = 300, .h = 70 };
	SDL_FRect statsButton{ .x = 730, .y = 400, .w = 300, .h = 70 };
	SDL_FRect gesturesButton{ .x = 410, .y = 490, .w = 300, .h = 70 };
	SDL_FRect keyboardButton{ .x = 730, .y = 490, .w = 300, .h = 70 };

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
