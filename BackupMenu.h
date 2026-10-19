#pragma once
#include "Game.h"
#include "DigitFont.h"
#include "UI.h"
#include <string>
#include <vector>

// BACKUP, from the main menu: SAVE BACKUP puts everything the game keeps
// into one file (Backup.h); LOAD BACKUP puts a saved one back, after a
// second tap to confirm, since it replaces what's on this device.
// mina.cpp runs the file pickers and reports back with setStatus().
class BackupMenu
{
public:
	enum Choice{ NONE, BACK, SAVE, LOAD };

	void open(){
		armed = false;
		status.clear();
	}

	// After a save or load finishes (or is cancelled).
	void setStatus(const std::string& text, bool good){
		status = text;
		statusGood = good;
	}

	void draw(SDLState& state){
		UI::background(state, UI::DEEP);
		UI::title(state, "BACKUP");
		UI::centered(state, "ONE FILE WITH YOUR SAVED GAME, SETTINGS, STATS,", 120.0f, UI::BODY, UI::TEXT);
		UI::centered(state, "TRAINING PROGRESS AND SESSION LOG.", 150.0f, UI::BODY, UI::TEXT);
		UI::centered(state, "KEEP IT SOMEWHERE SAFE: A USB STICK, A CLOUD DRIVE OR AN EMAIL.", 200.0f, UI::SMALL, UI::DIM);
		UI::centered(state, "LOAD IT AFTER A RESET OR ON A NEW PHONE.", 224.0f, UI::SMALL, UI::DIM);

		UI::button(state, saveButton, UI::PRIMARY, "SAVE BACKUP", UI::LARGE);
		UI::button(state, loadButton, armed ? UI::DANGER : UI::NAV, armed ? "TAP AGAIN TO REPLACE" : "LOAD BACKUP", UI::LARGE);
		UI::centered(state, armed ? "LOADING REPLACES EVERYTHING ON THIS DEVICE" : "LOADING REPLACES WHAT IS ON THIS DEVICE",
			loadButton.y + loadButton.h + 12.0f, UI::SMALL, armed ? UI::ACCENT : UI::DIM);
		if(!status.empty())
			UI::centered(state, status, 560.0f, UI::BODY, statusGood ? SDL_Color{110, 230, 110, 255} : SDL_Color{255, 145, 130, 255});
		UI::button(state, backButton, UI::SECONDARY, "BACK", UI::LARGE);
	}

	Choice handlePoint(SDLState& state, float wx, float wy){
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, wx, wy, &x, &y))
			return NONE;
		SDL_FPoint p{x, y};
		if(SDL_PointInRectFloat(&p, &backButton))
			return BACK;
		if(SDL_PointInRectFloat(&p, &saveButton)){
			armed = false;
			status.clear();
			return SAVE;
		}
		if(SDL_PointInRectFloat(&p, &loadButton)){
			if(armed){
				armed = false;
				status.clear();
				return LOAD;
			}
			armed = true;
			return NONE;
		}
		armed = false;
		return NONE;
	}

	std::vector<SDL_FRect> focusRects(){ return { saveButton, loadButton, backButton }; }

private:
	bool armed = false;
	std::string status;
	bool statusGood = true;
	SDL_FRect saveButton{ .x = 470, .y = 290, .w = 500, .h = 84 };
	SDL_FRect loadButton{ .x = 470, .y = 410, .w = 500, .h = 84 };
	SDL_FRect backButton = UI::backRect();
};
