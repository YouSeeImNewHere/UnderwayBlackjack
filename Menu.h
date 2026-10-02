#pragma once
#include "Game.h"
#include "DigitFont.h"
#include "Platform.h"
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
	Tutorial,
	Charts,
	Training,
	Update
};

// The very first thing the player sees.
class Menu
{
public:
	// Set from SaveData before the menu is ever drawn: true once the player
	// has started a game before (this launch or a previous one), which
	// swaps the single Start button for Resume + Restart instead.
	bool hasSavedGame = false;

	// Set by mina.cpp each frame from UpdateCheck: the newer version to
	// offer ("" = none), and this build's own version ("" for dev builds).
	std::string updateVersion;
	std::string currentVersion;
	// True while the update downloads and installs.
	bool updating = false;

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
		drawButton(state, controlsButton, SDL_Color{60, 90, 150, 255}, "CONTROLS");
		drawButton(state, chartsButton, SDL_Color{150, 120, 40, 255}, "CHARTS");
		drawButton(state, trainingButton, SDL_Color{60, 130, 70, 255}, "TRAINING");

		if(!updateVersion.empty())
			drawButton(state, updateButton, updating ? SDL_Color{110, 90, 40, 255} : SDL_Color{190, 140, 30, 255},
				updating ? std::string("UPDATING...") : "UPDATE TO V" + updateVersion);
		if(!currentVersion.empty()){
			std::string v = "V" + currentVersion;
			DigitFont::drawText(state, v, 1440.0f - DigitFont::textWidth(v, 3.0f) - 16.0f, 690.0f, 3.0f, SDL_Color{150, 190, 150, 255});
		}
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

		// One CONTROLS button: the gestures page on a touch device, the
		// keyboard page on a desktop (see Platform.h).
		if(SDL_PointInRectFloat(&p, &controlsButton))
			return usesTouchControls() ? MenuChoice::Gestures : MenuChoice::Keyboard;
		if(SDL_PointInRectFloat(&p, &statsButton))
			return MenuChoice::Stats;
		if(SDL_PointInRectFloat(&p, &chartsButton))
			return MenuChoice::Charts;
		if(SDL_PointInRectFloat(&p, &trainingButton))
			return MenuChoice::Training;
		if(!updateVersion.empty() && !updating && SDL_PointInRectFloat(&p, &updateButton))
			return MenuChoice::Update;
		if(SDL_PointInRectFloat(&p, &tutorialButton))
			return MenuChoice::Tutorial;

		return MenuChoice::None;
	}

	// Every button currently on screen, for mina.cpp's arrow-key
	// navigation (it "clicks" the highlighted one via handlePoint()).
	std::vector<SDL_FRect> focusRects(){
		std::vector<SDL_FRect> rects = hasSavedGame
			? std::vector<SDL_FRect>{ resumeButton, restartButton, tutorialButton, statsButton, controlsButton, chartsButton, trainingButton }
			: std::vector<SDL_FRect>{ startButton, tutorialButton, statsButton, controlsButton, chartsButton, trainingButton };
		if(!updateVersion.empty())
			rects.push_back(updateButton);
		return rects;
	}

private:
	// Title, then the big play button(s), then HOW TO PLAY/STATS and
	// CONTROLS underneath.
	SDL_FRect startButton{ .x = 570, .y = 190, .w = 300, .h = 110 };
	SDL_FRect resumeButton{ .x = 570, .y = 170, .w = 300, .h = 80 };
	SDL_FRect restartButton{ .x = 570, .y = 262, .w = 300, .h = 70 };
	SDL_FRect tutorialButton{ .x = 250, .y = 400, .w = 300, .h = 70 };
	SDL_FRect statsButton{ .x = 570, .y = 400, .w = 300, .h = 70 };
	SDL_FRect controlsButton{ .x = 890, .y = 400, .w = 300, .h = 70 };
	SDL_FRect chartsButton{ .x = 410, .y = 490, .w = 300, .h = 70 };
	SDL_FRect trainingButton{ .x = 730, .y = 490, .w = 300, .h = 70 };
	SDL_FRect updateButton{ .x = 520, .y = 600, .w = 400, .h = 64 };

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
