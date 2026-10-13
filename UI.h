#pragma once
#include "Game.h"
#include "DigitFont.h"
#include <algorithm>
#include <string>

// The menus' shared look, so every screen agrees: one background, colours
// that mean something, a handful of whole-pixel text sizes (fractional
// sizes make the pixel font blotchy), one button, one switch, and the same
// title and BACK/GO positions everywhere.
//
// Colours:
//   PRIMARY  green -- the one thing to do next (START, RESUME, GO)
//   NAV      dark green -- going somewhere (menu items, choices)
//   SECONDARY grey -- BACK, ABOUT, anything minor
//   DANGER   red -- only for what can't be undone (RESET)
//   ACCENT   gold -- headings and names, never a button
namespace UI{

inline constexpr SDL_Color BG{18, 60, 32, 255};
inline constexpr SDL_Color PANEL{11, 38, 21, 255};
// The content screens' background (drills, planning, stats, charts, how
// to play): darker than the menus', so cards and numbers stand out.
inline constexpr SDL_Color DEEP{10, 30, 15, 255};
inline constexpr SDL_Color TEXT{240, 240, 232, 255};
inline constexpr SDL_Color DIM{165, 192, 170, 255};
inline constexpr SDL_Color ACCENT{240, 205, 95, 255};
inline constexpr SDL_Color PRIMARY{50, 140, 72, 255};
inline constexpr SDL_Color NAV{32, 92, 54, 255};
inline constexpr SDL_Color SECONDARY{72, 80, 78, 255};
inline constexpr SDL_Color DANGER{150, 55, 50, 255};
inline constexpr SDL_Color BORDER{225, 235, 225, 255};

// Text sizes, in font pixels (a glyph is 5 of them tall).
inline constexpr float HERO = 9.0f;   // the app's name on the main menu
inline constexpr float TITLE = 7.0f;  // each screen's title
inline constexpr float LARGE = 5.0f;  // big buttons, switch states
inline constexpr float BODY = 4.0f;   // labels and normal buttons
inline constexpr float SMALL = 3.0f;  // captions and fine print

inline constexpr float CX = 720.0f;
inline constexpr float TITLE_Y = 34.0f;
inline constexpr float BOTTOM_Y = 640.0f, BOTTOM_H = 56.0f;

// BACK alone, bottom centre; or BACK beside the screen's main action.
inline SDL_FRect backRect(){ return SDL_FRect{ CX - 100.0f, BOTTOM_Y, 200.0f, BOTTOM_H }; }
inline SDL_FRect pairBackRect(){ return SDL_FRect{ CX - 230.0f, BOTTOM_Y, 200.0f, BOTTOM_H }; }
inline SDL_FRect pairGoRect(){ return SDL_FRect{ CX + 10.0f, BOTTOM_Y, 220.0f, BOTTOM_H }; }

inline float width(const std::string& s, float size){ return DigitFont::textWidth(s, size); }

// The largest whole size, at most `size`, that fits `maxW`.
inline float fit(const std::string& s, float size, float maxW){
	while(size > 2.0f && width(s, size) > maxW)
		size -= 1.0f;
	return size;
}

inline void text(SDLState& state, const std::string& s, float x, float y, float size, SDL_Color c = TEXT){
	DigitFont::drawText(state, s, x, y, size, c);
}
inline void centered(SDLState& state, const std::string& s, float y, float size, SDL_Color c = TEXT, float cx = CX){
	DigitFont::drawText(state, s, cx - width(s, size) / 2.0f, y, size, c);
}
// Vertically centred on a row of height h.
inline void label(SDLState& state, const std::string& s, float x, float rowY, float h, float size = BODY, SDL_Color c = TEXT){
	DigitFont::drawText(state, s, x, rowY + (h - 5 * size) / 2.0f, size, c);
}

inline void background(SDLState& state, SDL_Color c = BG){
	SDL_SetRenderDrawColor(state.renderer, c.r, c.g, c.b, 255);
	SDL_RenderFillRect(state.renderer, nullptr);
}
// A size snapped to a whole number of font pixels (never below SMALL's 3,
// the smallest that reads on a phone) -- fractional sizes blotch.
inline float snap(float size){ return std::max(3.0f, (float)(int)(size + 0.5f)); }
inline void title(SDLState& state, const std::string& s){
	centered(state, s, TITLE_Y, TITLE, TEXT);
}

inline void box(SDLState& state, const SDL_FRect& r, SDL_Color fill, SDL_Color border = BORDER){
	SDL_SetRenderDrawColor(state.renderer, fill.r, fill.g, fill.b, fill.a);
	SDL_RenderFillRect(state.renderer, &r);
	SDL_SetRenderDrawColor(state.renderer, border.r, border.g, border.b, border.a);
	SDL_RenderRect(state.renderer, &r);
}

inline void button(SDLState& state, const SDL_FRect& r, SDL_Color fill, const std::string& s, float size = BODY){
	box(state, r, fill);
	size = fit(s, size, r.w - 16.0f);
	DigitFont::drawText(state, s, r.x + (r.w - width(s, size)) / 2.0f, r.y + (r.h - 5 * size) / 2.0f, size, TEXT);
}

// An ON/OFF switch: green when on, grey when off, dimmed when it can't
// be changed here.
inline void toggle(SDLState& state, const SDL_FRect& r, bool on, bool enabled = true){
	SDL_Color fill = !enabled ? SDL_Color{45, 52, 50, 255} : on ? PRIMARY : SECONDARY;
	box(state, r, fill, enabled ? BORDER : SDL_Color{120, 130, 125, 255});
	std::string s = on ? "ON" : "OFF";
	DigitFont::drawText(state, s, r.x + (r.w - width(s, LARGE)) / 2.0f, r.y + (r.h - 5 * LARGE) / 2.0f, LARGE,
		enabled ? TEXT : DIM);
}

// A slider: track, filled part, knob; `value` 0-100. The value's text
// sits under the middle, optional end words under each end.
inline void slider(SDLState& state, const SDL_FRect& r, int value, const std::string& valueText,
	const std::string& low = "", const std::string& high = ""){
	const float knobW = 30.0f;
	float knobX = r.x + knobW / 2.0f + (r.w - knobW) * value / 100.0f;
	float midY = r.y + r.h / 2.0f;
	SDL_FRect track{ r.x, midY - 5.0f, r.w, 10.0f };
	box(state, track, SDL_Color{55, 65, 60, 255});
	SDL_FRect filled{ r.x, track.y, knobX - r.x, track.h };
	SDL_SetRenderDrawColor(state.renderer, PRIMARY.r, PRIMARY.g, PRIMARY.b, 255);
	SDL_RenderFillRect(state.renderer, &filled);
	SDL_FRect knob{ knobX - knobW / 2.0f, r.y + 4.0f, knobW, r.h - 8.0f };
	box(state, knob, SDL_Color{230, 232, 228, 255}, SDL_Color{40, 40, 40, 255});
	float y = r.y + r.h + 6.0f;
	if(!low.empty())
		text(state, low, r.x, y, SMALL, DIM);
	if(!high.empty())
		text(state, high, r.x + r.w - width(high, SMALL), y, SMALL, DIM);
	centered(state, valueText, y, SMALL, TEXT, r.x + r.w / 2.0f);
}

}
