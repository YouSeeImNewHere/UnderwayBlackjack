#pragma once
#include "Game.h"
#include "DigitFont.h"
#include "GameOptionsMenu.h"
#include "UI.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// SETTINGS, from the home page: the app's preferences in one place.
// Left: sound and controls (volume, sound on/off, one-handed drill and
// one-handed table gestures).
// Right: the table preferences Game Options also has (dealer speed,
// face-down doubles, hiding hands, double for less) -- the same values,
// so changing one place changes both. Bottom: resetting training progress
// or stats, each needing a second tap to confirm.
//
// Changes take effect at once; mina.cpp saves them on the way out.
class SettingsMenu
{
public:
	enum Choice{ NONE, BACK, RESET_TRAINING, RESET_STATS };

	// 0 (silent) to 100 (full).
	int volume = 100;
	// Strategy drill on a phone: answer by gestures, no move buttons.
	bool oneHandedDrill = true;
	// The table games: every move with one thumb (Table::oneHandedGestures).
	bool oneHandedTable = false;
	// How much of the shoe is dealt before the cut card, percent; 0 = random.
	int penetration = 0;
	// Short vibrations (Haptics.h) on phones.
	bool vibrate = true;

	// Focus indices of the two sliders (arrow keys move them, see mina.cpp).
	static constexpr int VOLUME_FOCUS_INDEX = 0, SPEED_FOCUS_INDEX = 1;

	// inGame: opened from the pause menu, so DEALER HITS SOFT 17 and
	// INDEX PLAYS follow what the running game allows.
	void open(GameOptionsMenu& gameOptions, bool inGame = false){
		options = &gameOptions;
		playing = inGame;
		armed = NONE;
		dragging = -1;
	}

	void draw(SDLState& state){
		UI::background(state);
		UI::title(state, "SETTINGS");
		UI::text(state, "SOUND AND CONTROLS", LEFT_X, HEADER_Y, UI::BODY, UI::ACCENT);
		UI::text(state, "TABLE", RIGHT_X, HEADER_Y, UI::BODY, UI::ACCENT);

		// Left: sound and controls.
		row(state, LEFT_X, rowY(0), "VOLUME");
		UI::slider(state, sliderRect(0), volume, volume == 0 ? "MUTE" : std::to_string(volume) + "%");
		row(state, LEFT_X, rowY(1), "SOUND");
		UI::toggle(state, soundButton(), options->soundEffects);
		row(state, LEFT_X, rowY(2), "VIBRATE");
		UI::toggle(state, vibrateButton(), vibrate);
		row(state, LEFT_X, rowY(3), "ONE-HANDED DRILL");
		UI::toggle(state, toggleRect(LEFT_CONTROL, 3), oneHandedDrill);
		caption(state, LEFT_X, rowY(3), oneHandedDrill ? "PHONES: GESTURES, NO BUTTONS" : "PHONES: TAP THE MOVE BUTTONS");
		row(state, LEFT_X, rowY(4), "ONE-HANDED TABLE");
		UI::toggle(state, toggleRect(LEFT_CONTROL, 4), oneHandedTable);
		caption(state, LEFT_X, rowY(4), oneHandedTable ? "SWIPE UP DOUBLE, HOLD SPLIT" : "TWO FINGERS DOUBLE AND SPLIT");
		row(state, LEFT_X, rowY(5), "PENETRATION");
		UI::button(state, penetrationButton(), UI::NAV, penetration == 0 ? "RANDOM" : std::to_string(penetration) + "%");

		// Right: the table.
		row(state, RIGHT_X, rightRowY(0), "DEALER SPEED");
		UI::slider(state, sliderRect(1), options->dealerSpeed, options->speedLabel(), "SLOW", "FAST");
		row(state, RIGHT_X, rightRowY(1), "FACE-DOWN DOUBLES");
		UI::toggle(state, toggleRect(RIGHT_CONTROL, 1), options->faceDownDoubles);
		row(state, RIGHT_X, rightRowY(2), "HIDE OTHER HANDS");
		UI::toggle(state, toggleRect(RIGHT_CONTROL, 2), options->hideInactiveHands);
		row(state, RIGHT_X, rightRowY(3), "DOUBLE FOR LESS");
		UI::toggle(state, toggleRect(RIGHT_CONTROL, 3), options->doubleForLess);
		row(state, RIGHT_X, rightRowY(4), soft17Choice() ? "DEALER HITS SOFT 17" : "ALWAYS HITS SOFT 17");
		UI::toggle(state, toggleRect(RIGHT_CONTROL, 4), soft17Choice() ? options->dealerHitsSoft17 : true, soft17Choice());
		row(state, RIGHT_X, rightRowY(5), indexChoice() ? "INDEX PLAYS" : "INDEX PLAYS - N/A");
		UI::toggle(state, toggleRect(RIGHT_CONTROL, 5), indexChoice() && options->indexPlays, indexChoice());
		caption(state, RIGHT_X, rightRowY(5), "TIP AND CHARTS USE THE COUNT");
		row(state, RIGHT_X, rightRowY(6), "AUTO SHUFFLER");
		UI::toggle(state, toggleRect(RIGHT_CONTROL, 6), options->autoShuffler);
		caption(state, RIGHT_X, rightRowY(6), options->autoShuffler ? "SHUFFLED EVERY ROUND - NO COUNTING" : "A SHOE WITH A CUT CARD");

		// Resets: grey until armed, then red.
		UI::button(state, resetTrainingButton, armed == RESET_TRAINING ? UI::DANGER : UI::SECONDARY,
			armed == RESET_TRAINING ? "TAP TO CONFIRM" : "RESET TRAINING", UI::SMALL);
		UI::button(state, resetStatsButton, armed == RESET_STATS ? UI::DANGER : UI::SECONDARY,
			armed == RESET_STATS ? "TAP TO CONFIRM" : "RESET STATS", UI::SMALL);
		if(!note.empty())
			UI::text(state, note, LEFT_X, resetTrainingButton.y + ROW_H + 10.0f, UI::SMALL, UI::ACCENT);
		UI::button(state, backButton, UI::SECONDARY, "BACK", UI::LARGE);
	}

	// A press on a slider starts dragging it (mouse or finger down).
	void pointerDown(SDLState& state, float wx, float wy){
		SDL_FPoint p;
		if(!toLogical(state, wx, wy, p))
			return;
		for(int i = 0; i < 2; i++){
			SDL_FRect hit = sliderHitRect(i);
			if(SDL_PointInRectFloat(&p, &hit)){
				dragging = i;
				setFromX(i, p.x);
			}
		}
	}

	void pointerMove(SDLState& state, float wx, float wy){
		SDL_FPoint p;
		if(dragging >= 0 && toLogical(state, wx, wy, p))
			setFromX(dragging, p.x);
	}

	Choice handlePoint(SDLState& state, float wx, float wy){
		SDL_FPoint p;
		if(!toLogical(state, wx, wy, p))
			return NONE;
		if(dragging >= 0){
			setFromX(dragging, p.x);
			dragging = -1;
			return NONE;
		}
		if(hit(p, backButton))
			return BACK;
		for(int i = 0; i < 2; i++){
			SDL_FRect r = sliderHitRect(i);
			if(SDL_PointInRectFloat(&p, &r)){
				setFromX(i, p.x);
				return NONE;
			}
		}
		Choice wasArmed = armed;
		armed = NONE;
		note.clear();
		if(hit(p, soundButton())) options->soundEffects = !options->soundEffects;
		if(hit(p, vibrateButton())) vibrate = !vibrate;
		if(hit(p, toggleRect(LEFT_CONTROL, 3))) oneHandedDrill = !oneHandedDrill;
		if(hit(p, toggleRect(LEFT_CONTROL, 4))) oneHandedTable = !oneHandedTable;
		if(hit(p, toggleRect(RIGHT_CONTROL, 1))) options->faceDownDoubles = !options->faceDownDoubles;
		if(hit(p, toggleRect(RIGHT_CONTROL, 2))) options->hideInactiveHands = !options->hideInactiveHands;
		if(hit(p, toggleRect(RIGHT_CONTROL, 3))) options->doubleForLess = !options->doubleForLess;
		if(soft17Choice() && hit(p, toggleRect(RIGHT_CONTROL, 4))) options->dealerHitsSoft17 = !options->dealerHitsSoft17;
		if(indexChoice() && hit(p, toggleRect(RIGHT_CONTROL, 5))) options->indexPlays = !options->indexPlays;
		if(hit(p, toggleRect(RIGHT_CONTROL, 6))) options->autoShuffler = !options->autoShuffler;
		if(hit(p, penetrationButton())){
			static const int STEPS[7] = { 0, 60, 65, 70, 75, 80, 85 };
			int i = 0;
			while(i < 7 && STEPS[i] != penetration) i++;
			penetration = STEPS[(i + 1) % 7];
		}
		if(hit(p, resetTrainingButton)){
			if(wasArmed == RESET_TRAINING){
				note = "TRAINING PROGRESS RESET";
				return RESET_TRAINING;
			}
			armed = RESET_TRAINING;
		}
		if(hit(p, resetStatsButton)){
			if(wasArmed == RESET_STATS){
				note = "STATS RESET";
				return RESET_STATS;
			}
			armed = RESET_STATS;
		}
		return NONE;
	}

	// Arrow keys on a highlighted slider.
	void nudge(int focusIndex, int delta){
		if(focusIndex == VOLUME_FOCUS_INDEX)
			volume = std::clamp(volume + delta, 0, 100);
		else if(focusIndex == SPEED_FOCUS_INDEX)
			options->nudgeSpeed(delta);
	}

	std::vector<SDL_FRect> focusRects(){
		return { sliderRect(0), sliderRect(1), // VOLUME_FOCUS_INDEX, SPEED_FOCUS_INDEX
			soundButton(), vibrateButton(), toggleRect(LEFT_CONTROL, 3), toggleRect(LEFT_CONTROL, 4),
			toggleRect(RIGHT_CONTROL, 1), toggleRect(RIGHT_CONTROL, 2), toggleRect(RIGHT_CONTROL, 3),
			toggleRect(RIGHT_CONTROL, 4), toggleRect(RIGHT_CONTROL, 5), toggleRect(RIGHT_CONTROL, 6),
			penetrationButton(), resetTrainingButton, resetStatsButton, backButton };
	}

private:
	// Two columns, each a label on the left and its control flush with
	// the column's right edge; a caption, when a row has one, under it.
	static constexpr float LEFT_X = 40.0f, LEFT_END = 690.0f;
	static constexpr float RIGHT_X = 760.0f, RIGHT_END = 1400.0f;
	static constexpr float TOGGLE_W = 140.0f, SLIDER_W = 300.0f, KNOB_W = 30.0f, ROW_H = 48.0f;
	static constexpr float LEFT_CONTROL = LEFT_END - TOGGLE_W, RIGHT_CONTROL = RIGHT_END - TOGGLE_W;
	static constexpr float HEADER_Y = 100.0f;

	GameOptionsMenu* options = nullptr;
	Choice armed = NONE;
	std::string note;
	int dragging = -1; // 0 volume, 1 dealer speed

	bool playing = false;
	// Under the left column.
	SDL_FRect resetTrainingButton{ .x = LEFT_X, .y = 552, .w = 300, .h = ROW_H };
	SDL_FRect resetStatsButton{ .x = LEFT_X + 316.0f, .y = 552, .w = 300, .h = ROW_H };
	SDL_FRect soundButton() const{ return toggleRect(LEFT_CONTROL, 1); }
	SDL_FRect vibrateButton() const{ return toggleRect(LEFT_CONTROL, 2); }
	SDL_FRect penetrationButton() const{ return SDL_FRect{ .x = LEFT_END - 200.0f, .y = rowY(5), .w = 200, .h = ROW_H }; }

	bool soft17Choice() const{ return !playing || options->soft17Choosable(); }
	bool indexChoice() const{ return !playing || options->indexChoosable(); }
	SDL_FRect backButton = UI::backRect();

	// Row tops: a slider row and a captioned row get extra room under them.
	static float rowY(int row){
		static constexpr float Y[6] = { 140.0f, 218.0f, 278.0f, 338.0f, 416.0f, 494.0f };
		return Y[std::clamp(row, 0, 5)];
	}
	static float rightRowY(int row){
		static constexpr float Y[7] = { 140.0f, 218.0f, 278.0f, 338.0f, 398.0f, 458.0f, 536.0f };
		return Y[std::clamp(row, 0, 6)];
	}

	SDL_FRect sliderRect(int i) const{
		return SDL_FRect{ .x = (i == 0 ? LEFT_END : RIGHT_END) - SLIDER_W, .y = rowY(0), .w = SLIDER_W, .h = ROW_H };
	}
	SDL_FRect sliderHitRect(int i) const{
		SDL_FRect r = sliderRect(i);
		return SDL_FRect{ .x = r.x - 20.0f, .y = r.y - 16.0f, .w = r.w + 40.0f, .h = r.h + 32.0f };
	}
	static SDL_FRect toggleRect(float x, int row){
		return SDL_FRect{ .x = x, .y = x == RIGHT_CONTROL ? rightRowY(row) : rowY(row), .w = TOGGLE_W, .h = ROW_H };
	}

	static void row(SDLState& state, float x, float y, const std::string& s){ UI::label(state, s, x, y, ROW_H); }
	static void caption(SDLState& state, float x, float rowTop, const std::string& s){
		UI::text(state, s, x, rowTop + ROW_H + 6.0f, UI::SMALL, UI::DIM);
	}

	void setFromX(int slider, float x){
		SDL_FRect r = sliderRect(slider);
		float t = (x - r.x - KNOB_W / 2.0f) / (r.w - KNOB_W);
		int v = std::clamp((int)std::lround(t * 100.0f), 0, 100);
		if(slider == 0)
			volume = v;
		else
			options->dealerSpeed = v;
	}

	static bool toLogical(SDLState& state, float wx, float wy, SDL_FPoint& p){
		return SDL_RenderCoordinatesFromWindow(state.renderer, wx, wy, &p.x, &p.y);
	}
	static bool hit(SDL_FPoint p, const SDL_FRect& r){ return SDL_PointInRectFloat(&p, &r); }
};
