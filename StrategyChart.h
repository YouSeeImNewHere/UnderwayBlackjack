#pragma once
#include "Game.h"
#include "DigitFont.h"
#include <string>
#include <vector>

// The classic 3-part basic-strategy reference (hard totals / soft totals /
// pairs, each x dealer up-card 2-10/A) -- multi-deck, dealer hits soft 17,
// double-after-split allowed, matching this game's own H17 setup. Purely a
// static reference table; not tuned per-rule beyond that. Opened from
// PauseMenu's STRATEGY TABLE button. If Table::getStrategySituation()
// finds a hand actually in progress, that cell gets highlighted so it
// doubles as "what should I do right now," not just a wall chart.
//
// H=hit, S=stand, D=double, P=split, R=surrender (falls back to hit in
// this game, which has no half-bet-back surrender payout -- still the
// textbook-correct call to flag).
class StrategyChart
{
public:
	void draw(SDLState& state, Resources& res, bool hasHighlight, int hSection, int hRow, int hCol){
		SDL_SetRenderDrawColor(state.renderer, 10, 30, 15, 255);
		SDL_RenderFillRect(state.renderer, nullptr);

		float titlePixel = 7.0f;
		std::string title = "STRATEGY TABLE";
		float titleW = DigitFont::textWidth(title, titlePixel);
		DigitFont::drawText(state, title, (1440.0f - titleW) / 2.0f, 20.0f, titlePixel, SDL_Color{255, 255, 255, 255});

		drawTable(state, 8.0f, "HARD TOTALS", HARD_LABELS, HARD_ROWS, 10,
			hasHighlight && hSection == 0 ? hRow : -1, hasHighlight && hSection == 0 ? hCol : -1);
		drawTable(state, 488.0f, "SOFT TOTALS", SOFT_LABELS, SOFT_ROWS, 8,
			hasHighlight && hSection == 1 ? hRow : -1, hasHighlight && hSection == 1 ? hCol : -1);
		drawTable(state, 968.0f, "PAIRS", PAIR_LABELS, PAIR_ROWS, 10,
			hasHighlight && hSection == 2 ? hRow : -1, hasHighlight && hSection == 2 ? hCol : -1);

		drawLegend(state);
		drawButton(state, backButton, SDL_Color{80, 80, 80, 255}, "BACK");
	}

	// windowX/windowY: raw event coordinates in window space, same
	// convention as Menu/SetupMenu's handlePoint. Returns true once BACK
	// is hit -- caller (mina.cpp) is responsible for what "back" means.
	bool handlePoint(SDLState& state, float windowX, float windowY){
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			return false;

		SDL_FPoint p{x, y};
		return SDL_PointInRectFloat(&p, &backButton);
	}

public:
	// Public so Table.h's in-game quick-view popup can reuse the same
	// data/coloring instead of duplicating it.
	static SDL_Color colorFor(char action){
		switch(action){
			case 'S': return SDL_Color{50, 140, 70, 255};
			case 'D': return SDL_Color{50, 90, 170, 255};
			case 'P': return SDL_Color{140, 80, 170, 255};
			case 'R': return SDL_Color{150, 50, 50, 255};
			default:  return SDL_Color{90, 90, 90, 255}; // H
		}
	}

	static const char* sectionTitle(int section){
		if(section == 0) return "HARD TOTALS";
		if(section == 1) return "SOFT TOTALS";
		return "PAIRS";
	}

	static const char* dealerColLabel(int col){
		return COL_LABELS[col];
	}

	static const char* rowLabel(int section, int row){
		if(section == 0) return HARD_LABELS[row];
		if(section == 1) return SOFT_LABELS[row];
		return PAIR_LABELS[row];
	}

	static const char* rowData(int section, int row){
		if(section == 0) return HARD_ROWS[row];
		if(section == 1) return SOFT_ROWS[row];
		return PAIR_ROWS[row];
	}

	std::vector<SDL_FRect> focusRects(){
		return { backButton };
	}

private:
	// STRIDE is the distance between cell starts; CELL_W/H (smaller than
	// the stride) is the box actually drawn, leaving a visible gap between
	// cells instead of them touching edge to edge. 3 tables x 11 columns
	// has to fit across 1440 -- STRIDE_W is close to the practical max for
	// that (much bigger and the tables themselves start colliding).
	static constexpr float CELL_W = 36.0f;
	static constexpr float CELL_H = 30.0f;
	static constexpr float CELL_GAP = 6.0f;
	static constexpr float STRIDE_W = CELL_W + CELL_GAP;
	static constexpr float STRIDE_H = CELL_H + CELL_GAP;
	static constexpr float TABLE_TOP = 110.0f;

	static constexpr const char* COL_LABELS[10] = {"2","3","4","5","6","7","8","9","10","A"};

	// Verified against wizardofodds.com's multi-deck (4-8 deck) text
	// strategy notes rather than memory: their S17 baseline surrenders hard
	// 15 vs. 10 and hard 16 (not a pair of 8s) vs. 9/10/A, and their H17
	// deltas on top of that are "surrender 15/pair-8s/17 vs. A", "double 11
	// vs. A", "double soft 18 vs. 2", "double soft 19 vs. 6" -- all folded
	// into HARD_ROWS/SOFT_ROWS below. No total-dependent difference exists
	// between double-deck and multi-deck at this level (WoO's own
	// double-deck page notes the only known exception, soft 18 vs. A, still
	// favors standing in practice), so this one chart covers both 2-deck
	// and 6-deck games -- only *composition*-dependent double-deck plays
	// (exact ranks, not just the total) differ, a finer tier than this
	// chart -- or Hand::getShownTotals() -- tracks.
	static constexpr const char* HARD_LABELS[10] = {"8","9","10","11","12","13","14","15","16","17"};
	static constexpr const char* HARD_ROWS[10] = {
		"HHHHHHHHHH",
		"HDDDDHHHHH",
		"DDDDDDDDHH",
		"DDDDDDDDDD",
		"HHSSSHHHHH",
		"SSSSSHHHHH",
		"SSSSSHHHHH",
		"SSSSSHHHRR",
		"SSSSSHHRRR",
		"SSSSSSSSSS"
	};

	static constexpr const char* SOFT_LABELS[8] = {"A2","A3","A4","A5","A6","A7","A8","A9"};
	static constexpr const char* SOFT_ROWS[8] = {
		"HHHDDHHHHH",
		"HHHDDHHHHH",
		"HHDDDHHHHH",
		"HHDDDHHHHH",
		"HDDDDHHHHH",
		"DDDDDSSHHH",
		"SSSSDSSSSS",
		"SSSSSSSSSS"
	};

	static constexpr const char* PAIR_LABELS[10] = {"2","3","4","5","6","7","8","9","10","A"};
	static constexpr const char* PAIR_ROWS[10] = {
		"PPPPPPHHHH",
		"PPPPPPHHHH",
		"HHHPPHHHHH",
		"DDDDDDDDHH",
		"PPPPPHHHHH",
		"PPPPPPHHHH",
		"PPPPPPPPPP",
		"PPPPPSPPSS",
		"SSSSSSSSSS",
		"PPPPPPPPPP"
	};

	SDL_FRect backButton{ .x = 620, .y = 630, .w = 200, .h = 56 };

	void drawCell(SDLState& state, const SDL_FRect& rect, SDL_Color color, const std::string& text, float pixel, bool highlight){
		// Highlighted cell gets an unmistakable bright fill + black text
		// instead of its normal color -- a thin border alone was too easy
		// to miss (or mistake for a rendering glitch) in a busy grid.
		SDL_Color fill = highlight ? SDL_Color{255, 225, 60, 255} : color;
		SDL_Color textColor = highlight ? SDL_Color{20, 20, 20, 255} : SDL_Color{255, 255, 255, 255};

		SDL_SetRenderDrawColor(state.renderer, fill.r, fill.g, fill.b, fill.a);
		SDL_RenderFillRect(state.renderer, &rect);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &rect);

		if(!text.empty()){
			// Auto-shrinks to fit -- DigitFont's 5-wide glyphs run ~50%
			// wider per character than the font's old 3-wide ones, and a
			// 2-digit label ("10") no longer fit CELL_W at a fixed pixel
			// size the way single digits/letters still do.
			float cellPixel = pixel;
			float maxW = rect.w - 6.0f;
			float w = DigitFont::textWidth(text, cellPixel);
			if(w > maxW && w > 0.0f)
				cellPixel *= maxW / w;
			w = DigitFont::textWidth(text, cellPixel);
			DigitFont::drawText(state, text, rect.x + (rect.w - w) / 2.0f, rect.y + (rect.h - 5 * cellPixel) / 2.0f, cellPixel, textColor);
		}

		if(highlight){
			SDL_SetRenderDrawColor(state.renderer, 20, 20, 20, 255);
			SDL_FRect outer{ .x = rect.x - 3, .y = rect.y - 3, .w = rect.w + 6, .h = rect.h + 6 };
			SDL_RenderRect(state.renderer, &outer);
			SDL_FRect outer2{ .x = rect.x - 2, .y = rect.y - 2, .w = rect.w + 4, .h = rect.h + 4 };
			SDL_RenderRect(state.renderer, &outer2);
		}
	}

	void drawTable(SDLState& state, float x, const std::string& title, const char* const* rowLabels, const char* const* rows, int rowCount, int highlightRow, int highlightCol){
		// Was TABLE_TOP - 26 -- at titlePixel 5 the text itself is 25 tall,
		// so the title's bottom edge sat almost flush against the header
		// row right below it. -36 leaves an actual gap between them.
		// Real grid width is 11 columns with a gap *between* each (10 gaps,
		// not 11) -- STRIDE_W*11 counted a trailing gap past the last
		// column that's never actually drawn, so the title centered
		// slightly right of the grid's true center.
		float titlePixel = 5.0f;
		float titleW = DigitFont::textWidth(title, titlePixel);
		float gridW = 11.0f * CELL_W + 10.0f * CELL_GAP;
		DigitFont::drawText(state, title, x + (gridW - titleW) / 2.0f, TABLE_TOP - 36.0f, titlePixel, SDL_Color{255, 255, 255, 255});

		// One uniform pixel size for every cell in this table (header, row
		// labels, and the action letters) -- drawCell()'s own auto-shrink
		// used to run independently per cell, so a 2-character cell
		// ("10", "A5", ...) shrank on its own while every 1-character cell
		// (H/S/D/P/R, "2".."9") stayed at the full 5.0f, leaving the grid
		// visibly two different sizes. 2.7 is sized to fit the widest
		// content (2 characters) in CELL_W, used everywhere in this table
		// so it's all one consistent size instead.
		float cellPixel = 2.7f;

		// Header row: blank corner cell, then each dealer up-card.
		for(int c = 0; c < 10; c++){
			SDL_FRect rect{ .x = x + STRIDE_W * (c + 1), .y = TABLE_TOP, .w = CELL_W, .h = CELL_H };
			drawCell(state, rect, SDL_Color{40, 60, 45, 255}, COL_LABELS[c], cellPixel, false);
		}

		for(int r = 0; r < rowCount; r++){
			// Inverted from the data's own order (row 0 -- the lowest total/
			// rank in HARD_LABELS/SOFT_LABELS/PAIR_LABELS -- used to draw
			// right under the header, working down to the highest at the
			// bottom): now the highest is right under the header and the
			// lowest is at the bottom, which is the reading order players
			// actually asked for. Only where each row draws changes here --
			// rowLabels[r]/rows[r] and the highlight-matching r index below
			// are untouched, so getStrategySituation() doesn't need to know
			// anything changed.
			float rowY = TABLE_TOP + STRIDE_H * (rowCount - r);

			SDL_FRect labelRect{ .x = x, .y = rowY, .w = CELL_W, .h = CELL_H };
			drawCell(state, labelRect, SDL_Color{40, 60, 45, 255}, rowLabels[r], cellPixel, false);

			for(int c = 0; c < 10; c++){
				char action = rows[r][c];
				SDL_FRect rect{ .x = x + STRIDE_W * (c + 1), .y = rowY, .w = CELL_W, .h = CELL_H };
				bool hl = (r == highlightRow && c == highlightCol);
				std::string text(1, action);
				drawCell(state, rect, colorFor(action), text, cellPixel, hl);
			}
		}
	}

	// Below the tables: a swatch + letter + meaning for each action code,
	// plus a sample of the highlight style itself so it reads as "this is
	// what your current hand looks like" rather than a stray box.
	void drawLegend(SDLState& state){
		struct Entry{ char code; std::string label; };
		Entry entries[5] = {
			{'H', "HIT"},
			{'S', "STAND"},
			{'D', "DOUBLE"},
			{'P', "SPLIT"},
			{'R', "SURRENDER"}
		};
		std::string highlightLabel = "YOUR HAND";

		// Same size as the table's own cells (was a slightly different
		// 34x24, close enough to look like a mismatch) so a swatch here
		// reads as literally the same box, not a lookalike.
		float swatchW = CELL_W, swatchH = CELL_H;
		// Was 5.0f with a 36 entryGap -- at the new font's width (~50%
		// more per character than the old one) the whole row no longer
		// fit across the 1440-wide canvas at that size.
		float pixel = 3.3f;
		float swatchLabelGap = 10.0f;
		float entryGap = 24.0f;

		// Each entry's full width (swatch + gap + label), computed up
		// front so the whole row can be centered as one block -- it used
		// to just start at a fixed, arbitrarily-guessed X and run
		// wherever that happened to end, which was neither centered nor
		// consistently spaced.
		float widths[6];
		for(int i = 0; i < 5; i++)
			widths[i] = swatchW + swatchLabelGap + DigitFont::textWidth(entries[i].label, pixel);
		widths[5] = swatchW + swatchLabelGap + DigitFont::textWidth(highlightLabel, pixel);

		float totalW = (5 * entryGap);
		for(int i = 0; i < 6; i++)
			totalW += widths[i];

		float y = 545.0f;
		float x = (1440.0f - totalW) / 2.0f;

		for(int i = 0; i < 5; i++){
			SDL_FRect swatch{ .x = x, .y = y, .w = swatchW, .h = swatchH };
			std::string code(1, entries[i].code);
			drawCell(state, swatch, colorFor(entries[i].code), code, pixel, false);

			DigitFont::drawText(state, entries[i].label, x + swatchW + swatchLabelGap, y + (swatchH - 5.0f * pixel) / 2.0f, pixel, SDL_Color{255, 255, 255, 255});

			x += widths[i] + entryGap;
		}

		// The highlight sample, on its own since it isn't an action code.
		SDL_FRect sample{ .x = x, .y = y, .w = swatchW, .h = swatchH };
		drawCell(state, sample, SDL_Color{90, 90, 90, 255}, "", pixel, true);
		DigitFont::drawText(state, highlightLabel, x + swatchW + swatchLabelGap, y + (swatchH - 5.0f * pixel) / 2.0f, pixel, SDL_Color{255, 255, 255, 255});
	}

	void drawButton(SDLState& state, const SDL_FRect& rect, SDL_Color color, const std::string& label){
		SDL_SetRenderDrawColor(state.renderer, color.r, color.g, color.b, color.a);
		SDL_RenderFillRect(state.renderer, &rect);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &rect);

		float pixel = 6.0f;
		float w = DigitFont::textWidth(label, pixel);
		DigitFont::drawText(state, label, rect.x + (rect.w - w) / 2.0f, rect.y + (rect.h - 5 * pixel) / 2.0f, pixel, SDL_Color{255, 255, 255, 255});
	}
};
