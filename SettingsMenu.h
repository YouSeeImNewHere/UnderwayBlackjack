#pragma once
#include "Game.h"
#include "DigitFont.h"
#include "GameOptionsMenu.h"
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
		SDL_SetRenderDrawColor(state.renderer, 20, 70, 35, 255);
		SDL_RenderFillRect(state.renderer, nullptr);
		centered(state, "SETTINGS", 30.0f, 7.0f, WHITE);

		DigitFont::drawText(state, "SOUND AND CONTROLS", LEFT_LABEL, 100.0f, 3.6f, GOLD);
		DigitFont::drawText(state, "TABLE", RIGHT_LABEL, 100.0f, 3.6f, GOLD);

		label(state, LEFT_LABEL, rowY(0), "VOLUME");
		drawSlider(state, sliderRect(0), volume, volume == 0 ? "MUTE" : std::to_string(volume) + "%", "", "");
		label(state, LEFT_LABEL, rowY(1), "SOUND");
		drawToggle(state, toggleRect(LEFT_CONTROL, 1), options->soundEffects);
		label(state, LEFT_LABEL, rowY(2), "ONE-HANDED DRILL");
		drawToggle(state, toggleRect(LEFT_CONTROL, 2), oneHandedDrill);
		DigitFont::drawText(state, oneHandedDrill ? "PHONES: GESTURES, NO BUTTONS" : "PHONES: TAP THE MOVE BUTTONS",
			LEFT_LABEL, rowY(2) + ROW_H + 4.0f, 2.6f, DIM);
		label(state, LEFT_LABEL, rowY(3), "ONE-HANDED TABLE");
		drawToggle(state, toggleRect(LEFT_CONTROL, 3), oneHandedTable);
		DigitFont::drawText(state, oneHandedTable ? "SWIPE UP DOUBLE, DOWN SURRENDER, HOLD SPLIT" : "TWO FINGERS DOUBLE AND SPLIT",
			LEFT_LABEL, rowY(3) + ROW_H + 4.0f, 2.6f, DIM);

		label(state, RIGHT_LABEL, rowY(0), "DEALER SPEED");
		drawSlider(state, sliderRect(1), options->dealerSpeed, options->speedLabel(), "SLOW", "FAST");
		label(state, RIGHT_LABEL, rowY(1), "FACE-DOWN DOUBLES");
		drawToggle(state, toggleRect(RIGHT_CONTROL, 1), options->faceDownDoubles);
		label(state, RIGHT_LABEL, rowY(2), "HIDE HANDS TIL YOUR TURN");
		drawToggle(state, toggleRect(RIGHT_CONTROL, 2), options->hideInactiveHands);
		label(state, RIGHT_LABEL, rowY(3), "DOUBLE FOR LESS");
		drawToggle(state, toggleRect(RIGHT_CONTROL, 3), options->doubleForLess);
		label(state, RIGHT_LABEL, rowY(4), soft17Choice() ? "DEALER HITS SOFT 17" : "SOFT 17 - ALWAYS HITS");
		drawToggle(state, toggleRect(RIGHT_CONTROL, 4), soft17Choice() ? options->dealerHitsSoft17 : true, soft17Choice());
		label(state, RIGHT_LABEL, rowY(5), indexChoice() ? "INDEX PLAYS" : "INDEX PLAYS - STANDARD ONLY");
		drawToggle(state, toggleRect(RIGHT_CONTROL, 5), indexChoice() && options->indexPlays, indexChoice());
		DigitFont::drawText(state, "COUNT DEVIATIONS IN TIP AND CHARTS", RIGHT_LABEL, rowY(5) + ROW_H + 4.0f, 2.6f, DIM);

		button(state, resetTrainingButton, armed == RESET_TRAINING ? SDL_Color{170, 50, 50, 255} : SDL_Color{90, 60, 60, 255},
			armed == RESET_TRAINING ? "TAP AGAIN TO RESET" : "RESET TRAINING", 4.0f);
		button(state, resetStatsButton, armed == RESET_STATS ? SDL_Color{170, 50, 50, 255} : SDL_Color{90, 60, 60, 255},
			armed == RESET_STATS ? "TAP AGAIN TO RESET" : "RESET STATS", 4.0f);
		if(!note.empty())
			centered(state, note, 600.0f, 3.2f, GREEN);
		button(state, backButton, SDL_Color{80, 80, 80, 255}, "BACK", 6.0f);
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
		if(hit(p, toggleRect(LEFT_CONTROL, 1))) options->soundEffects = !options->soundEffects;
		if(hit(p, toggleRect(LEFT_CONTROL, 2))) oneHandedDrill = !oneHandedDrill;
		if(hit(p, toggleRect(LEFT_CONTROL, 3))) oneHandedTable = !oneHandedTable;
		if(hit(p, toggleRect(RIGHT_CONTROL, 1))) options->faceDownDoubles = !options->faceDownDoubles;
		if(hit(p, toggleRect(RIGHT_CONTROL, 2))) options->hideInactiveHands = !options->hideInactiveHands;
		if(hit(p, toggleRect(RIGHT_CONTROL, 3))) options->doubleForLess = !options->doubleForLess;
		if(soft17Choice() && hit(p, toggleRect(RIGHT_CONTROL, 4))) options->dealerHitsSoft17 = !options->dealerHitsSoft17;
		if(indexChoice() && hit(p, toggleRect(RIGHT_CONTROL, 5))) options->indexPlays = !options->indexPlays;
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
			toggleRect(LEFT_CONTROL, 1), toggleRect(LEFT_CONTROL, 2), toggleRect(LEFT_CONTROL, 3),
			toggleRect(RIGHT_CONTROL, 1), toggleRect(RIGHT_CONTROL, 2), toggleRect(RIGHT_CONTROL, 3),
			toggleRect(RIGHT_CONTROL, 4), toggleRect(RIGHT_CONTROL, 5),
			resetTrainingButton, resetStatsButton, backButton };
	}

private:
	static constexpr SDL_Color WHITE{255, 255, 255, 255};
	static constexpr SDL_Color GOLD{230, 210, 140, 255};
	static constexpr SDL_Color DIM{170, 195, 170, 255};
	static constexpr SDL_Color GREEN{110, 230, 110, 255};
	static constexpr float LEFT_LABEL = 40.0f, LEFT_CONTROL = 430.0f;
	static constexpr float RIGHT_LABEL = 760.0f, RIGHT_CONTROL = 1130.0f;
	static constexpr float SLIDER_W = 280.0f, KNOB_W = 30.0f, ROW_H = 52.0f;

	GameOptionsMenu* options = nullptr;
	Choice armed = NONE;
	std::string note;
	int dragging = -1; // 0 volume, 1 dealer speed

	bool playing = false;
	// Under the left column.
	SDL_FRect resetTrainingButton{ .x = LEFT_LABEL, .y = 440, .w = 330, .h = ROW_H };
	SDL_FRect resetStatsButton{ .x = LEFT_LABEL, .y = 516, .w = 330, .h = ROW_H };

	bool soft17Choice() const{ return !playing || options->soft17Choosable(); }
	bool indexChoice() const{ return !playing || options->indexChoosable(); }
	SDL_FRect backButton{ .x = 620, .y = 640, .w = 200, .h = 56 };

	static float rowY(int row){ return 136.0f + row * 76.0f; }

	SDL_FRect sliderRect(int i) const{
		return SDL_FRect{ .x = i == 0 ? LEFT_CONTROL : RIGHT_CONTROL, .y = rowY(0), .w = SLIDER_W, .h = ROW_H };
	}
	SDL_FRect sliderHitRect(int i) const{
		SDL_FRect r = sliderRect(i);
		return SDL_FRect{ .x = r.x - 20.0f, .y = r.y - 16.0f, .w = r.w + 40.0f, .h = r.h + 32.0f };
	}
	static SDL_FRect toggleRect(float x, int row){
		return SDL_FRect{ .x = x, .y = rowY(row), .w = 160.0f, .h = ROW_H };
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

	static void centered(SDLState& state, const std::string& s, float y, float pixel, SDL_Color c){
		DigitFont::drawText(state, s, (1440.0f - DigitFont::textWidth(s, pixel)) / 2.0f, y, pixel, c);
	}

	static void label(SDLState& state, float x, float y, const std::string& s){
		float pixel = 4.4f, maxW = 340.0f;
		float w = DigitFont::textWidth(s, pixel);
		if(w > maxW)
			pixel *= maxW / w;
		DigitFont::drawText(state, s, x, y + (ROW_H - 5 * pixel) / 2.0f, pixel, WHITE);
	}

	static void button(SDLState& state, const SDL_FRect& r, SDL_Color c, const std::string& s, float pixel){
		SDL_SetRenderDrawColor(state.renderer, c.r, c.g, c.b, 255);
		SDL_RenderFillRect(state.renderer, &r);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &r);
		float w = DigitFont::textWidth(s, pixel);
		if(w > r.w - 12.0f){
			pixel *= (r.w - 12.0f) / w;
			w = DigitFont::textWidth(s, pixel);
		}
		DigitFont::drawText(state, s, r.x + (r.w - w) / 2.0f, r.y + (r.h - 5 * pixel) / 2.0f, pixel, WHITE);
	}

	static void drawToggle(SDLState& state, const SDL_FRect& r, bool on, bool enabled = true){
		button(state, r, !enabled ? SDL_Color{50, 55, 52, 255} : on ? SDL_Color{60, 130, 70, 255} : SDL_Color{80, 80, 80, 255}, on ? "ON" : "OFF", 5.5f);
	}

	void drawSlider(SDLState& state, const SDL_FRect& r, int value, const std::string& valueText, const std::string& low, const std::string& high){
		float midY = r.y + r.h / 2.0f;
		float knobX = r.x + KNOB_W / 2.0f + (r.w - KNOB_W) * value / 100.0f;
		SDL_FRect track{ .x = r.x, .y = midY - 6.0f, .w = r.w, .h = 12.0f };
		SDL_SetRenderDrawColor(state.renderer, 70, 70, 80, 255);
		SDL_RenderFillRect(state.renderer, &track);
		SDL_FRect filled{ .x = r.x, .y = track.y, .w = knobX - r.x, .h = track.h };
		SDL_SetRenderDrawColor(state.renderer, 60, 130, 70, 255);
		SDL_RenderFillRect(state.renderer, &filled);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &track);
		SDL_FRect knob{ .x = knobX - KNOB_W / 2.0f, .y = r.y + 4.0f, .w = KNOB_W, .h = r.h - 8.0f };
		SDL_SetRenderDrawColor(state.renderer, 230, 230, 230, 255);
		SDL_RenderFillRect(state.renderer, &knob);
		SDL_SetRenderDrawColor(state.renderer, 40, 40, 40, 255);
		SDL_RenderRect(state.renderer, &knob);

		float y = r.y + r.h + 4.0f;
		if(!low.empty())
			DigitFont::drawText(state, low, r.x, y, 2.8f, DIM);
		if(!high.empty())
			DigitFont::drawText(state, high, r.x + r.w - DigitFont::textWidth(high, 2.8f), y, 2.8f, DIM);
		DigitFont::drawText(state, valueText, r.x + (r.w - DigitFont::textWidth(valueText, 3.6f)) / 2.0f, y, 3.6f, WHITE);
	}
};
