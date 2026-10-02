#pragma once
#include "Game.h"
#include "DigitFont.h"
#include "GameModeMenu.h"
#include <string>
#include <vector>

// The classic 3-part basic-strategy reference (hard totals / soft totals /
// pairs, each x dealer up-card 2-10/A), one set per rule family -- the
// game's own rules differ enough between them that one chart gives wrong
// advice in the others:
//   - Standard (2/6-deck, Lucky Ladies, Lucky Stiff): multi-deck, dealer
//     hits soft 17, double after split, late surrender.
//   - Player's Edge (Spanish 21): 48-card decks, a player 21 always wins,
//     5+ card 21 bonuses, double on any cards and redouble up to 3 times.
//   - Free Bet: free doubles on hard 9-11 and free splits (except 10s),
//     dealer 22 pushes, no surrender.
// Table::configureGameMode() picks the set (useChartFor()); the pause
// menu's chart and the in-game quick tip both read it. If
// Table::getStrategySituation() finds a hand in progress, that cell is
// highlighted, so it doubles as "what should I do right now."
//
// H=hit, S=stand, D=double, P=split, R=surrender (half the bet back).
class StrategyChart
{
public:
	void draw(SDLState& state, Resources& res, bool hasHighlight, int hSection, int hRow, int hCol){
		SDL_SetRenderDrawColor(state.renderer, 10, 30, 15, 255);
		SDL_RenderFillRect(state.renderer, nullptr);

		float titlePixel = 7.0f;
		std::string title = std::string("STRATEGY - ") + CHARTS[activeChart].name;
		float titleW = DigitFont::textWidth(title, titlePixel);
		DigitFont::drawText(state, title, (1440.0f - titleW) / 2.0f, 20.0f, titlePixel, SDL_Color{255, 255, 255, 255});

		const ChartSet& chart = CHARTS[activeChart];
		drawTable(state, 8.0f, "HARD TOTALS", HARD_LABELS, chart.hard, 10,
			hasHighlight && hSection == 0 ? hRow : -1, hasHighlight && hSection == 0 ? hCol : -1);
		drawTable(state, 488.0f, "SOFT TOTALS", SOFT_LABELS, chart.soft, 8,
			hasHighlight && hSection == 1 ? hRow : -1, hasHighlight && hSection == 1 ? hCol : -1);
		drawTable(state, 968.0f, "PAIRS", PAIR_LABELS, chart.pairs, 10,
			hasHighlight && hSection == 2 ? hRow : -1, hasHighlight && hSection == 2 ? hCol : -1);

		drawLegend(state);
		if(activeChart == 1){
			std::string note = "S4, D3...: HIT WITH THAT MANY CARDS OR MORE.  YELLOW CORNER: HIT IF A 6-7-8 OR SUITED 7-7-7 BONUS IS STILL POSSIBLE";
			float np = std::min(3.0f, 1400.0f / DigitFont::textWidth(note, 1.0f));
			DigitFont::drawText(state, note, (1440.0f - DigitFont::textWidth(note, np)) / 2.0f, 595.0f, np, SDL_Color{230, 210, 140, 255});
		}
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

	// Free Bet's free-hand row (see FREEBET_FREE_*), or nullptr when the
	// regular chart applies.
	static const char* freeHandRowData(int section, int row){
		if(activeChart != 2 || section == 2)
			return nullptr;
		return section == 0 ? FREEBET_FREE_HARD[row] : FREEBET_FREE_SOFT[row];
	}

	static const char* rowData(int section, int row){
		const ChartSet& chart = CHARTS[activeChart];
		if(section == 0) return chart.hard[row];
		if(section == 1) return chart.soft[row];
		return chart.pairs[row];
	}

	// Player's Edge (Spanish 21) strategy also depends on how many cards
	// are in the hand: a number after a cell's letter (S4, D3, ...) means
	// that move only with fewer cards than that -- with that many or more,
	// hit. 0 = no limit. From the published Spanish 21 H17 chart
	// (wizardofodds.com), applied where it agrees with this chart's own
	// letter. Other charts have none.
	static int cardLimit(int section, int row, int col){
		if(activeChart != 1 || section == 2)
			return 0;
		const char* digits = section == 0 ? SPANISH_HARD_CARDS[row] : SPANISH_SOFT_CARDS[row];
		return digits[col] - '0';
	}

	// Cells (marked with a yellow corner on the chart) where Player's Edge's 6-7-8 / 7-7-7
	// bonuses change the play: hit hard 13-15 when the two cards could
	// still make 6-7-8 (some cells only when they're suited, or both
	// spades), and hit -- not split -- suited 7s vs 7. ' ' = none, 'A' any
	// 6-7-8, 'S' suited, 'K' spades, '7' suited sevens.
	static char bonusRule(int section, int row, int col){
		if(activeChart != 1 || section == 1)
			return ' ';
		if(section == 2)
			return (row == 5 && col == 5) ? '7' : ' ';
		return SPANISH_HARD_BONUS[row][col];
	}

	// Which rule family's chart the pause-menu chart and quick tip show --
	// set whenever the table's game mode is (Table::configureGameMode()).
	static void useChartFor(GameMode mode){
		if(isPlayersEdge(mode))
			activeChart = 1;
		else if(isFreeBet(mode))
			activeChart = 2;
		else if(deckCountFor(mode) <= 2)
			activeChart = 3;
		else
			activeChart = 0;
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

	static constexpr const char* HARD_LABELS[10] = {"8","9","10","11","12","13","14","15","16","17"};
	static constexpr const char* SOFT_LABELS[8] = {"A2","A3","A4","A5","A6","A7","A8","A9"};
	static constexpr const char* PAIR_LABELS[10] = {"2","3","4","5","6","7","8","9","10","A"};

	struct ChartSet{
		const char* name;
		const char* const* hard;  // HARD_LABELS rows
		const char* const* soft;  // SOFT_LABELS rows
		const char* const* pairs; // PAIR_LABELS rows
	};

	// Standard: verified against wizardofodds.com's multi-deck (4-8 deck)
	// strategy notes: their S17 baseline surrenders hard 15 vs. 10 and
	// hard 16 (not a pair of 8s) vs. 9/10/A, and their H17 deltas on top of
	// that are "surrender 15/17 vs. A", "double 11 vs. A", "double
	// soft 18 vs. 2", "double soft 19 vs. 6". (17 vs. A and 8s vs. A were
	// described but missing from the rows until now.) Used for the shoe
	// games (6 and 8 decks, double after split allowed); the 2-deck games
	// have their own chart (DOUBLE_DECK_*).
	//
	// Every set was also cross-checked with an infinite-deck expected-
	// value solver run against this game's exact rules for that family --
	// it reproduces this standard chart cell for cell except soft 13 vs. 5
	// (a known 6-deck-vs-infinite-deck edge case, left as the 6-deck play).
	static constexpr const char* STANDARD_HARD[10] = {
		"HHHHHHHHHH",
		"HDDDDHHHHH",
		"DDDDDDDDHH",
		"DDDDDDDDDD",
		"HHSSSHHHHH",
		"SSSSSHHHHH",
		"SSSSSHHHHH",
		"SSSSSHHHRR",
		"SSSSSHHRRR",
		"SSSSSSSSSR"
	};
	static constexpr const char* STANDARD_SOFT[8] = {
		"HHHDDHHHHH",
		"HHHDDHHHHH",
		"HHDDDHHHHH",
		"HHDDDHHHHH",
		"HDDDDHHHHH",
		"DDDDDSSHHH",
		"SSSSDSSSSS",
		"SSSSSSSSSS"
	};
	static constexpr const char* STANDARD_PAIRS[10] = {
		"PPPPPPHHHH",
		"PPPPPPHHHH",
		"HHHPPHHHHH",
		"DDDDDDDDHH",
		"PPPPPHHHHH",
		"PPPPPPHHHH",
		"PPPPPPPPPP", // 8s: split even vs A (surrender only without DAS)
		"PPPPPSPPSS",
		"SSSSSSSSSS",
		"PPPPPPPPPP"
	};

	// Player's Edge (Spanish 21, dealer hits soft 17): the published
	// Spanish 21 H17 basic strategy (wizardofodds.com), cell for cell,
	// including its card-count limits (SPANISH_HARD_CARDS/SOFT_CARDS) and
	// 6-7-8 / suited 7-7-7 bonus plays (SPANISH_HARD_BONUS, bonusRule()).
	// Player's Edge pays the same 21 bonuses that chart assumes. Pairs of
	// 4s and 5s are never split (played as hard 8 and hard 10), and 17 vs
	// an Ace surrenders with 2 cards, otherwise hits.
	static constexpr const char* SPANISH_HARD[10] = {
		"HHHHHHHHHH", // 8 (and less)
		"HHHHDHHHHH", // 9
		"DDDDDDDHHH", // 10
		"DDDDDDDDDD", // 11
		"HHHHHHHHHH", // 12
		"HHHHSHHHHH", // 13
		"HHSSSHHHHH", // 14
		"SSSSSHHHHH", // 15
		"SSSSSHHHHR", // 16
		"SSSSSSSSSR"  // 17
	};
	// Card-count limits (see cardLimit()), rows/cols as SPANISH_HARD/SOFT.
	static constexpr const char* SPANISH_HARD_CARDS[10] = {
		"0000000000", // 8
		"0000000000", // 9
		"5500043000", // 10: D5 D5 D D D D4 D3
		"4555544433", // 11: D4 D5 D5 D5 D5 D4 D4 D4 D3 D3
		"0000000000", // 12
		"0000400000", // 13: S4* vs 6
		"0045600000", // 14: S4* S5' S6" vs 4-6
		"4566000000", // 15: S4* S5' S6 S6 vs 2-5
		"6660000000", // 16: S6 vs 2-4
		"0000006663"  // 17: S6 vs 8-10; vs A (surrender, else) hit with 3+
	};
	static constexpr const char* SPANISH_HARD_BONUS[10] = {
		"          ", "          ", "          ", "          ", "          ",
		"    A     ", // 13 vs 6
		"  ASK     ", // 14 vs 4/5/6
		"AS        ", // 15 vs 2/3
		"          ", "          "
	};
	static constexpr const char* SPANISH_SOFT_CARDS[8] = {
		"0000000000", // A2
		"0000000000", // A3
		"0000400000", // A4: D4 vs 6
		"0003400000", // A5: D3 vs 5, D4 vs 6
		"0034500000", // A6: D3 D4 D5 vs 4-6
		"4445664000", // A7: S4 S4 D4 D5 D6 S6 S4
		"0000000066", // A8: S6 vs 10/A
		"0000000000"  // A9
	};
	static constexpr const char* SPANISH_SOFT[8] = {
		"HHHHHHHHHH", // A2
		"HHHHHHHHHH", // A3
		"HHHHDHHHHH", // A4
		"HHHDDHHHHH", // A5
		"HHDDDHHHHH", // A6
		"SSDDDSSHHH", // A7
		"SSSSSSSSSS", // A8
		"SSSSSSSSSS"  // A9
	};
	static constexpr const char* SPANISH_PAIRS[10] = {
		"PPPPPPPHHH", // 2s
		"PPPPPPPHHH", // 3s
		"HHHHHHHHHH", // 4s: never split, hard 8
		"DDDDDDDHHH", // 5s: never split, hard 10
		"HHPPPHHHHH", // 6s
		"PPPPPPHHHH", // 7s (suited 7s vs 7 hit, see bonusRule())
		"PPPPPPPPPR", // 8s
		"SPPPPSPPSS", // 9s
		"SSSSSSSSSS", // 10s
		"PPPPPPPPPP"  // As
	};

	// Free Bet (6 decks, dealer hits soft 17): computed by the same solver
	// with free doubles on hard 9-11 and free splits of every pair but 10s
	// (the split-off hand rides on a free bet), a dealer 22 pushing, and no
	// surrender. Matches the published Free Bet advice checked so far:
	// always take the free doubles and free splits (5s double instead),
	// never split 10s. The D on hard 9-11 is the free double; a D anywhere
	// else costs the usual extra bet.
	static constexpr const char* FREEBET_HARD[10] = {
		"HHHHHHHHHH",
		"DDDDDDDDDD",
		"DDDDDDDDDD",
		"DDDDDDDDDD",
		"HHHSSHHHHH",
		"HSSSSHHHHH",
		"SSSSSHHHHH",
		"SSSSSHHHHH",
		"SSSSSHHHHH",
		"SSSSSSSSSS"
	};
	static constexpr const char* FREEBET_SOFT[8] = {
		"HHHHHHHHHH",
		"HHHHHHHHHH",
		"HHHHHHHHHH",
		"HHHHDHHHHH", // A5: double vs 6
		"HHHDDHHHHH", // A6: double vs 5-6
		"SSSDDSSHHH",
		"SSSSSSSSSS",
		"SSSSSSSSSS"
	};
	// The free hand -- a split-off hand riding on the free bet, nothing
	// of the player's own at stake -- plays more aggressively
	// (wizardofodds.com's Free Bet "free hand" chart). Pairs are the same.
	static constexpr const char* FREEBET_FREE_HARD[10] = {
		"HHHHHHHHHH", // 8
		"DDDDDDDDDD", // 9 (free double)
		"DDDDDDDDDD", // 10
		"DDDDDDDDDD", // 11
		"HHHSSHHHHH", // 12
		"HSSSSHHHHH", // 13
		"HSSSSHHHHH", // 14
		"SSSSSHHHHH", // 15
		"SSSSSHHHHH", // 16
		"SSSSSHHHSH"  // 17
	};
	static constexpr const char* FREEBET_FREE_SOFT[8] = {
		"HHHHHHHHHH", // A2
		"HHHHHHHHHH", // A3
		"HHHHHHHHHH", // A4
		"HHHHDHHHHH", // A5
		"HHHDDHHHHH", // A6
		"HHDDDSHHHH", // A7
		"SSSDDSSSSS", // A8
		"SSSSDSSSSS"  // A9
	};
	static constexpr const char* FREEBET_PAIRS[10] = {
		"PPPPPPPPPP",
		"PPPPPPPPPP",
		"PPPPPPPPPP",
		"DDDDDDDDDD",
		"PPPPPPPPPP",
		"PPPPPPPPPP",
		"PPPPPPPPPP",
		"PPPPPPPPPP",
		"SSSSSSSSSS",
		"PPPPPPPPPP"
	};

	// Double deck (2 Deck and Lucky Ladies 2 Deck: dealer hits soft 17,
	// no double after split, late surrender): wizardofodds.com's published
	// double-deck H17 chart, with its "only if double after split" splits
	// played as hits and 8s vs an Ace surrendered (no DAS).
	static constexpr const char* DOUBLE_DECK_HARD[10] = {
		"HHHHHHHHHH", // 8
		"DDDDDHHHHH", // 9
		"DDDDDDDDHH", // 10
		"DDDDDDDDDD", // 11
		"HHSSSHHHHH", // 12
		"SSSSSHHHHH", // 13
		"SSSSSHHHHH", // 14
		"SSSSSHHHRR", // 15
		"SSSSSHHHRR", // 16
		"SSSSSSSSSR"  // 17
	};
	static constexpr const char* DOUBLE_DECK_SOFT[8] = {
		"HHHDDHHHHH", // A2
		"HHDDDHHHHH", // A3
		"HHDDDHHHHH", // A4
		"HHDDDHHHHH", // A5
		"HDDDDHHHHH", // A6
		"DDDDDSSHHH", // A7
		"SSSSDSSSSS", // A8
		"SSSSSSSSSS"  // A9
	};
	static constexpr const char* DOUBLE_DECK_PAIRS[10] = {
		"HHPPPPHHHH", // 2s
		"HHPPPPHHHH", // 3s
		"HHHHHHHHHH", // 4s
		"DDDDDDDDHH", // 5s
		"PPPPPHHHHH", // 6s
		"PPPPPPHHHH", // 7s
		"PPPPPPPPPR", // 8s
		"PPPPPSPPSS", // 9s
		"SSSSSSSSSS", // 10s
		"PPPPPPPPPP"  // As
	};

	static constexpr ChartSet CHARTS[4] = {
		{ "STANDARD", STANDARD_HARD, STANDARD_SOFT, STANDARD_PAIRS },
		{ "SPANISH 21", SPANISH_HARD, SPANISH_SOFT, SPANISH_PAIRS },
		{ "FREE BET", FREEBET_HARD, FREEBET_SOFT, FREEBET_PAIRS },
		{ "DOUBLE DECK", DOUBLE_DECK_HARD, DOUBLE_DECK_SOFT, DOUBLE_DECK_PAIRS }
	};

	inline static int activeChart = 0;

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
				int section = rows == CHARTS[activeChart].hard ? 0 : rows == CHARTS[activeChart].soft ? 1 : 2;
				int limit = cardLimit(section, r, c);
				if(limit > 0 && (action == 'S' || action == 'D' || (action == 'R' && section == 0 && r == 9)))
					text += std::to_string(limit);
				drawCell(state, rect, colorFor(action), text, cellPixel, hl);
				// 6-7-8 / 7-7-7 bonus plays: a yellow corner on the cell.
				if(bonusRule(section, r, c) != ' '){
					SDL_FRect corner{ .x = rect.x + rect.w - 9.0f, .y = rect.y + 1.0f, .w = 8.0f, .h = 8.0f };
					SDL_SetRenderDrawColor(state.renderer, 255, 225, 60, 255);
					SDL_RenderFillRect(state.renderer, &corner);
				}
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
