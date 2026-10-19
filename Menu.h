#pragma once
#include "Game.h"
#include "DigitFont.h"
#include "UI.h"
#include "Platform.h"
#include <algorithm>
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
	Drills,
	Planning,
	SessionLog,
	Backup,
	Settings,
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
		UI::background(state);
		UI::centered(state, "BLACKJACK VARIANTS", 56.0f, UI::HERO, UI::ACCENT);

		// The one main action, big and green; RESTART beside it in grey.
		if(hasSavedGame){
			UI::button(state, resumeButton, UI::PRIMARY, "RESUME", UI::TITLE);
			UI::button(state, restartButton, UI::SECONDARY, "RESTART", UI::BODY);
		} else{
			UI::button(state, startButton, UI::PRIMARY, "START", UI::TITLE);
		}

		for(int i = 0; i < ITEM_COUNT; i++)
			UI::button(state, itemRect(i), UI::NAV, ITEMS[i].label, UI::LARGE);

		if(!updateVersion.empty())
			UI::button(state, updateButton, updating ? UI::SECONDARY : UI::PRIMARY,
				updating ? std::string("UPDATING...") : "UPDATE TO V" + updateVersion, UI::BODY);
		if(!currentVersion.empty()){
			std::string v = "V" + currentVersion;
			UI::text(state, v, 1440.0f - UI::width(v, UI::SMALL) - 20.0f, 690.0f, UI::SMALL, UI::DIM);
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

		for(int i = 0; i < ITEM_COUNT; i++){
			SDL_FRect r = itemRect(i);
			if(SDL_PointInRectFloat(&p, &r))
				return ITEMS[i].choice;
		}
		if(!updateVersion.empty() && !updating && SDL_PointInRectFloat(&p, &updateButton))
			return MenuChoice::Update;

		return MenuChoice::None;
	}

	// Every button currently on screen, for mina.cpp's arrow-key
	// navigation (it "clicks" the highlighted one via handlePoint()).
	std::vector<SDL_FRect> focusRects(){
		std::vector<SDL_FRect> rects = hasSavedGame
			? std::vector<SDL_FRect>{ resumeButton, restartButton }
			: std::vector<SDL_FRect>{ startButton };
		for(int i = 0; i < ITEM_COUNT; i++)
			rects.push_back(itemRect(i));
		if(!updateVersion.empty())
			rects.push_back(updateButton);
		return rects;
	}

private:
	// Title, then the big play button(s), then a 3-wide grid of
	// everything else (the last row centered).
	SDL_FRect startButton{ .x = 560, .y = 160, .w = 320, .h = 104 };
	SDL_FRect resumeButton{ .x = 560, .y = 150, .w = 320, .h = 84 };
	SDL_FRect restartButton{ .x = 610, .y = 246, .w = 220, .h = 48 };
	SDL_FRect updateButton{ .x = 520, .y = 620, .w = 400, .h = 56 };

	struct Item{ const char* label; MenuChoice choice; };
	static constexpr int ITEM_COUNT = 8;
	static constexpr Item ITEMS[ITEM_COUNT] = {
		{ "HOW TO PLAY", MenuChoice::Tutorial },
		{ "CHARTS", MenuChoice::Charts },
		{ "STATS", MenuChoice::Stats },
		{ "DRILLS", MenuChoice::Drills },
		{ "PLANNING", MenuChoice::Planning },
		{ "SESSION LOG", MenuChoice::SessionLog },
		{ "SETTINGS", MenuChoice::Settings },
		{ "BACKUP", MenuChoice::Backup },
	};
	static constexpr float BTN_W = 360.0f, BTN_H = 64.0f, GAP = 20.0f;
	static SDL_FRect itemRect(int i){
		int row = i / 3, col = i % 3;
		int inRow = std::min(3, ITEM_COUNT - row * 3);
		float rowW = inRow * BTN_W + (inRow - 1) * GAP;
		return SDL_FRect{ .x = 720.0f - rowW / 2.0f + col * (BTN_W + GAP), .y = 352.0f + row * (BTN_H + 22.0f), .w = BTN_W, .h = BTN_H };
	}
};
