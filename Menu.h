#pragma once
#include "Game.h"
#include "DigitFont.h"
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

		// Every label the same size: as large as the longest one allows.
		float pixel = 8.0f;
		for(const Item& item : ITEMS)
			pixel = std::min(pixel, (BTN_W - 28.0f) / DigitFont::textWidth(item.label, 1.0f));
		for(int i = 0; i < ITEM_COUNT; i++)
			drawButton(state, itemRect(i), ITEMS[i].color, ITEMS[i].label, pixel);

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
	SDL_FRect startButton{ .x = 570, .y = 160, .w = 300, .h = 110 };
	SDL_FRect resumeButton{ .x = 570, .y = 150, .w = 300, .h = 74 };
	SDL_FRect restartButton{ .x = 570, .y = 234, .w = 300, .h = 60 };
	SDL_FRect updateButton{ .x = 520, .y = 636, .w = 400, .h = 60 };

	struct Item{ const char* label; SDL_Color color; MenuChoice choice; };
	static constexpr int ITEM_COUNT = 7;
	static constexpr Item ITEMS[ITEM_COUNT] = {
		{ "HOW TO PLAY", {50, 120, 130, 255}, MenuChoice::Tutorial },
		{ "CHARTS", {150, 120, 40, 255}, MenuChoice::Charts },
		{ "STATS", {120, 70, 130, 255}, MenuChoice::Stats },
		{ "DRILLS", {60, 130, 70, 255}, MenuChoice::Drills },
		{ "PLANNING", {150, 100, 40, 255}, MenuChoice::Planning },
		{ "SESSION LOG", {90, 80, 140, 255}, MenuChoice::SessionLog },
		{ "SETTINGS", {80, 90, 100, 255}, MenuChoice::Settings },
	};
	static constexpr float BTN_W = 340.0f, BTN_H = 66.0f, GAP = 24.0f;
	static SDL_FRect itemRect(int i){
		int row = i / 3, col = i % 3;
		int inRow = std::min(3, ITEM_COUNT - row * 3);
		float rowW = inRow * BTN_W + (inRow - 1) * GAP;
		return SDL_FRect{ .x = 720.0f - rowW / 2.0f + col * (BTN_W + GAP), .y = 340.0f + row * (BTN_H + 20.0f), .w = BTN_W, .h = BTN_H };
	}

	void drawButton(SDLState& state, const SDL_FRect& rect, SDL_Color color, const std::string& label, float maxPixel = 8.0f){
		SDL_SetRenderDrawColor(state.renderer, color.r, color.g, color.b, color.a);
		SDL_RenderFillRect(state.renderer, &rect);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &rect);

		// Auto-shrinks to fit -- see GameModeMenu.h's drawButton() for why.
		float pixel = maxPixel;
		float maxW = rect.w - 12.0f;
		float w = DigitFont::textWidth(label, pixel);
		if(w > maxW && w > 0.0f)
			pixel *= maxW / w;

		w = DigitFont::textWidth(label, pixel);
		DigitFont::drawText(state, label, rect.x + (rect.w - w) / 2.0f, rect.y + (rect.h - 5 * pixel) / 2.0f, pixel, SDL_Color{255, 255, 255, 255});
	}
};
