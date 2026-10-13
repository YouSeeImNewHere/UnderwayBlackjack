#pragma once
#include "Game.h"
#include "DigitFont.h"
#include "UI.h"
#include "Platform.h"
#include <string>
#include <algorithm>
#include <vector>

enum class PauseChoice{
	None,
	Resume,
	MainMenu,
	StrategyTable,
	About,
	Gestures,
	Keyboard,
	Stats,
	Options
};

// Drawn as a dim overlay on top of the (frozen, still-visible) table --
// mina.cpp stops calling Table::update()/dealDealer() while this is up,
// but still draws the table underneath so the board doesn't just vanish.
class PauseMenu
{
public:
	void draw(SDLState& state, Resources& res){
		SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(state.renderer, 0, 0, 0, 185);
		SDL_RenderFillRect(state.renderer, nullptr);
		SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_NONE);

		// A panel over the frozen table: the title, RESUME as the one
		// main action, the rest in a grid, MAIN MENU last and in grey.
		UI::box(state, panel, UI::PANEL);
		UI::centered(state, "PAUSED", panel.y + 24.0f, UI::TITLE);
		UI::button(state, resumeButton, UI::PRIMARY, "RESUME", UI::TITLE);
		UI::button(state, strategyButton, UI::NAV, "CHARTS", UI::LARGE);
		UI::button(state, optionsButton, UI::NAV, "SETTINGS", UI::LARGE);
		UI::button(state, statsButton, UI::NAV, "STATS", UI::LARGE);
		UI::button(state, controlsButton, UI::NAV, "CONTROLS", UI::LARGE);
		UI::button(state, aboutButton, UI::NAV, "RULES", UI::LARGE);
		UI::button(state, mainMenuButton, UI::SECONDARY, "MAIN MENU", UI::LARGE);
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
		if(SDL_PointInRectFloat(&p, &mainMenuButton))
			return PauseChoice::MainMenu;
		if(SDL_PointInRectFloat(&p, &strategyButton))
			return PauseChoice::StrategyTable;
		if(SDL_PointInRectFloat(&p, &aboutButton))
			return PauseChoice::About;
		// Gestures page on touch devices, keyboard page on desktops.
		if(SDL_PointInRectFloat(&p, &controlsButton))
			return usesTouchControls() ? PauseChoice::Gestures : PauseChoice::Keyboard;
		if(SDL_PointInRectFloat(&p, &statsButton))
			return PauseChoice::Stats;
		if(SDL_PointInRectFloat(&p, &optionsButton))
			return PauseChoice::Options;

		return PauseChoice::None;
	}

	// For mina.cpp's arrow-key navigation, top to bottom.
	std::vector<SDL_FRect> focusRects(){
		return { resumeButton, strategyButton, optionsButton, statsButton, controlsButton, aboutButton, mainMenuButton };
	}

private:
	// RESUME full width under the title, then a 2-column grid, each
	// button thumb-sized within the 720-tall canvas.
	SDL_FRect panel{ .x = 400, .y = 96, .w = 640, .h = 430 };
	SDL_FRect resumeButton{ .x = 430, .y = 176, .w = 580, .h = 76 };
	SDL_FRect strategyButton{ .x = 430, .y = 272, .w = 285, .h = 66 };
	SDL_FRect optionsButton{ .x = 725, .y = 272, .w = 285, .h = 66 };
	SDL_FRect statsButton{ .x = 430, .y = 352, .w = 285, .h = 66 };
	SDL_FRect controlsButton{ .x = 725, .y = 352, .w = 285, .h = 66 };
	SDL_FRect aboutButton{ .x = 430, .y = 432, .w = 285, .h = 66 };
	SDL_FRect mainMenuButton{ .x = 725, .y = 432, .w = 285, .h = 66 };
};
