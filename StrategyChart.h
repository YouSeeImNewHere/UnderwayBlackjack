#pragma once
#include "Game.h"
#include "DigitFont.h"
#include "GameModeMenu.h"
#include <string>
#include <vector>
#include <algorithm>
#include <functional>

// The classic 3-part basic-strategy reference (hard totals / soft totals /
// pairs, each x dealer up-card 2-10/A). Every chart is the published one
// from wizardofodds.com for its rules, transcribed cell for cell:
//   - Standard blackjack for 1 deck, 2 decks and 4-8 decks, each with the
//     dealer hitting or standing on soft 17.
//   - Spanish 21 (Player's Edge) for H17 and S17, including its card-count
//     and 6-7-8 / 7-7-7 bonus plays.
//   - Free Bet blackjack (dealer always hits soft 17), with its separate
//     chart for a free split-off hand.
// The cells keep the published "if allowed" codes, so one chart resolves
// for whether the table allows double after split and surrender:
//   H hit, S stand, D double (else hit), d double (else stand), P split,
//   p split if DAS (else hit), x split if DAS (else double), y split if
//   DAS (else stand), R surrender (else hit), r surrender (else stand),
//   q surrender if allowed and no DAS (else split).
// Table::configureGameMode() picks the chart for the game being played
// (useChartFor()); the pause menu's chart and the in-game TIP read it, and
// the home page's CHARTS screen (ChartsMenu.h) shows any of them.
// Which chart (StrategyChart::Set), and the table rules it's resolved for.
struct StrategyRules{
	int set = 0;
	bool das = true;        // double after split
	bool surrender = true;  // late surrender
};

struct StrategyChartSet{
	const char* name;
	const char* const* hard;  // HARD_LABELS rows (8 and less ... 17; 18+ always stands)
	const char* const* soft;  // SOFT_LABELS rows
	const char* const* pairs; // PAIR_LABELS rows
	const char* const* hardCards = nullptr; // Spanish 21 card-count limits
	const char* const* softCards = nullptr;
	const char* const* hardBonus = nullptr; // Spanish 21 6-7-8 plays
};

class StrategyChart
{
public:
	using Rules = StrategyRules;

	enum Set{ SHOE_H17, SHOE_S17, DOUBLE_DECK_H17, DOUBLE_DECK_S17, SINGLE_DECK_H17, SINGLE_DECK_S17,
		SPANISH_H17, SPANISH_S17, FREE_BET, SET_COUNT };

	// A cell's index play, set by the caller when the count's deviations
	// should show (see IndexPlays::deviations()): the count's move and when
	// ("+4" = at +4 or more, "<0" = below 0).
	using Marks = std::function<bool(int section, int row, int col, std::string& when, char& move)>;

	// The pause menu's chart, with INDEX PLAYS on: its deviations on the
	// cells, and a DEVIATIONS button for them written out (mina.cpp sets
	// both when the chart opens; both empty otherwise).
	Marks marks;
	std::vector<std::string> deviationLines;
	std::string deviationFooter;
	bool listing = false;
	int detailSection = -1, detailRow = 0, detailCol = 0; // a tapped cell

	// The pause menu's chart: the game being played, with its hand
	// highlighted when there is one.
	void draw(SDLState& state, Resources& res, bool hasHighlight, int hSection, int hRow, int hCol){
		if(listing && !deviationLines.empty())
			drawDeviationList(state, "INDEX PLAYS - " + titleFor(active), deviationLines, deviationFooter);
		else
			drawChart(state, active, hasHighlight, hSection, hRow, hCol, false, nullptr, marks ? &marks : nullptr);
		drawButton(state, backRect(), SDL_Color{80, 80, 80, 255}, "BACK");
		if(!deviationLines.empty())
			drawButton(state, listButton, SDL_Color{190, 120, 40, 255}, listing ? "CHART" : "DEVIATIONS", 5.0f);
		if(detailSection >= 0 && !listing)
			drawCellDetail(state, active, detailSection, detailRow, detailCol, marks ? &marks : nullptr);
	}

	// The cell under a point on drawChart()'s layout, if any.
	static bool cellAt(SDL_FPoint p, int& section, int& row, int& col){
		static const float X[3] = { 8.0f, 488.0f, 968.0f };
		for(int s = 0; s < 3; s++){
			int c = (int)std::floor((p.x - X[s]) / STRIDE_W) - 1;
			if(c < 0 || c > 9 || p.x - X[s] - STRIDE_W * (c + 1) > CELL_W)
				continue;
			int rows = rowCount(s);
			int k = (int)std::floor((p.y - TABLE_TOP) / STRIDE_H); // 1..rows from the top
			if(k < 1 || k > rows || p.y - TABLE_TOP - STRIDE_H * k > CELL_H)
				continue;
			section = s; row = rows - k; col = c;
			return true;
		}
		return false;
	}

	static std::string handTitle(int section, int row, int col){
		static const char* PAIR[10] = { "2S", "3S", "4S", "5S", "6S", "7S", "8S", "9S", "10S", "ACES" };
		std::string hand = section == 0 ? (row == 0 ? std::string("HARD 8 OR LESS") : "HARD " + std::to_string(row + 8))
			: section == 1 ? "SOFT " + std::to_string(row + 13) + ", A-" + std::to_string(row + 2)
			: std::string("PAIR OF ") + PAIR[row];
		return hand + " VS " + (col == 9 ? std::string("ACE") : std::string(COL_LABELS[col]));
	}

	static const char* moveName(char m){
		switch(m){
			case 'H': return "HIT";
			case 'S': return "STAND";
			case 'D': return "DOUBLE";
			case 'P': return "SPLIT";
			case 'R': return "SURRENDER";
			default:  return "?";
		}
	}

	// A tapped cell, large: the hand, the play for these rules, what to do
	// when that move isn't allowed, and its index play when there is one.
	static void drawCellDetail(SDLState& state, const Rules& rules, int section, int row, int col, const Marks* marks){
		SDL_FRect shade{ 0, 0, 1440, 720 };
		SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(state.renderer, 0, 0, 0, 170);
		SDL_RenderFillRect(state.renderer, &shade);
		SDL_FRect panel{ 220, 150, 1000, 400 };
		fill(state, panel, SDL_Color{15, 30, 20, 255});
		SDL_SetRenderDrawColor(state.renderer, 255, 210, 40, 255);
		for(int k = 0; k < 3; k++){
			SDL_FRect ring{ panel.x - k, panel.y - k, panel.w + 2 * k, panel.h + 2 * k };
			SDL_RenderRect(state.renderer, &ring);
		}
		auto line = [&](const std::string& t, float y, float pixel, SDL_Color c){
			pixel = std::min(pixel, (panel.w - 60.0f) / std::max(1.0f, DigitFont::textWidth(t, 1.0f)));
			DigitFont::drawText(state, t, panel.x + (panel.w - DigitFont::textWidth(t, pixel)) / 2.0f, y, pixel, c);
		};
		char cd = code(rules.set, section, row, col);
		char move = resolve(cd, rules.das, rules.surrender);
		line(handTitle(section, row, col), panel.y + 30.0f, 6.0f, SDL_Color{255, 255, 255, 255});
		SDL_FRect chip{ panel.x + panel.w / 2.0f - 260.0f, panel.y + 95.0f, 520.0f, 80.0f };
		fill(state, chip, colorFor(move));
		line(moveName(move), chip.y + 16.0f, 10.0f, SDL_Color{255, 255, 255, 255});

		// What the published code means past the plain move.
		std::string detail;
		switch(cd){
			case 'D': detail = "DOUBLE IF ALLOWED, OTHERWISE HIT"; break;
			case 'd': detail = "DOUBLE IF ALLOWED, OTHERWISE STAND"; break;
			case 'R': detail = rules.surrender ? "SURRENDER ON YOUR FIRST 2 CARDS, OTHERWISE HIT" : "NO SURRENDER AT THIS TABLE, SO HIT"; break;
			case 'r': detail = rules.surrender ? "SURRENDER ON YOUR FIRST 2 CARDS, OTHERWISE STAND" : "NO SURRENDER AT THIS TABLE, SO STAND"; break;
			case 'p': detail = rules.das ? "SPLIT - YOU CAN DOUBLE AFTER" : "NO DOUBLE AFTER SPLIT HERE, SO HIT"; break;
			case 'x': detail = rules.das ? "SPLIT - YOU CAN DOUBLE AFTER" : "NO DOUBLE AFTER SPLIT HERE, SO DOUBLE"; break;
			case 'y': detail = rules.das ? "SPLIT - YOU CAN DOUBLE AFTER" : "NO DOUBLE AFTER SPLIT HERE, SO STAND"; break;
			case 'q': detail = move == 'R' ? "SURRENDER, OTHERWISE SPLIT" : "SPLIT"; break;
			default: break;
		}
		int limit = cardLimit(rules.set, section, row, col);
		if(limit > 0)
			detail += (detail.empty() ? "" : ".  ") + std::string("HIT INSTEAD WITH ") + std::to_string(limit) + " CARDS OR MORE";
		if(bonusRule(rules.set, section, row, col) != ' ')
			detail += (detail.empty() ? "" : ".  ") + std::string("HIT IF A 6-7-8 OR SUITED 7-7-7 BONUS IS STILL POSSIBLE");
		float y = chip.y + chip.h + 30.0f;
		if(!detail.empty()){
			line(detail, y, 3.6f, SDL_Color{230, 210, 140, 255});
			y += 40.0f;
		}
		std::string when;
		char devMove = 0;
		if(marks && (*marks)(section, row, col, when, devMove)){
			bool below = !when.empty() && when[0] == '<';
			std::string n = below ? when.substr(1) : when;
			line(std::string("COUNTING: ") + moveName(devMove) + (below ? " BELOW TRUE COUNT " + n : " AT TRUE COUNT " + n + " OR MORE"),
				y, 4.4f, MARK_ABOVE);
		}
		line("TAP ANYWHERE TO CLOSE", panel.y + panel.h - 34.0f, 3.0f, SDL_Color{160, 180, 160, 255});
	}

	// Every index play, one per line, in two columns.
	static void drawDeviationList(SDLState& state, const std::string& title, const std::vector<std::string>& lines, const std::string& footer = ""){
		SDL_SetRenderDrawColor(state.renderer, 10, 30, 15, 255);
		SDL_RenderFillRect(state.renderer, nullptr);
		float tp = std::min(5.0f, 1400.0f / DigitFont::textWidth(title, 1.0f));
		DigitFont::drawText(state, title, (1440.0f - DigitFont::textWidth(title, tp)) / 2.0f, 20.0f, tp, SDL_Color{255, 255, 255, 255});
		const char* intro = "HI-LO TRUE COUNT, ROUNDED DOWN.  OFF THE COUNT, PLAY THE CHART";
		DigitFont::drawText(state, intro, (1440.0f - DigitFont::textWidth(intro, 2.8f)) / 2.0f, 62.0f, 2.8f, SDL_Color{230, 210, 140, 255});
		size_t half = (lines.size() + 1) / 2;
		float pixel = 3.4f;
		for(const std::string& line : lines)
			pixel = std::min(pixel, 680.0f / std::max(1.0f, DigitFont::textWidth(line, 1.0f)));
		for(size_t i = 0; i < lines.size(); i++){
			float x = i < half ? 30.0f : 740.0f;
			float y = 104.0f + (i < half ? i : i - half) * 50.0f;
			DigitFont::drawText(state, lines[i], x, y, pixel, SDL_Color{235, 235, 235, 255});
		}
		if(!footer.empty()){
			float fp = std::min(3.0f, 1400.0f / DigitFont::textWidth(footer, 1.0f));
			DigitFont::drawText(state, footer, (1440.0f - DigitFont::textWidth(footer, fp)) / 2.0f, 590.0f, fp, SDL_Color{255, 225, 80, 255});
		}
	}

	// The whole chart for `rules`. browserLayout leaves room under it for
	// the CHARTS screen's rule buttons.
	// A cell colour override (the review screen's accuracy colours):
	// returns true and sets the colour to use for (section, row, col).
	using Tint = std::function<bool(int section, int row, int col, SDL_Color& color)>;

	// tint, when given, recolours cells and leaves out the legend.
	static void drawChart(SDLState& state, const Rules& rules, bool hasHighlight, int hSection, int hRow, int hCol, bool browserLayout,
			const Tint* tint = nullptr, const Marks* marks = nullptr){
		SDL_SetRenderDrawColor(state.renderer, 10, 30, 15, 255);
		SDL_RenderFillRect(state.renderer, nullptr);

		float titlePixel = 6.0f;
		std::string title = titleFor(rules);
		float titleW = DigitFont::textWidth(title, titlePixel);
		if(titleW > 1400.0f){
			titlePixel *= 1400.0f / titleW;
			titleW = DigitFont::textWidth(title, titlePixel);
		}
		DigitFont::drawText(state, title, (1440.0f - titleW) / 2.0f, 20.0f, titlePixel, SDL_Color{255, 255, 255, 255});

		static const float X[3] = { 8.0f, 488.0f, 968.0f };
		for(int section = 0; section < 3; section++)
			drawTable(state, rules, section, X[section],
				hasHighlight && hSection == section ? hRow : -1, hasHighlight && hSection == section ? hCol : -1, tint, marks);

		if(tint)
			return;
		float legendY = browserLayout ? 510.0f : 545.0f;
		drawLegend(state, legendY);
		float noteY = legendY + 40.0f;
		if(marks){
			std::string key = "ORANGE BOX = INDEX PLAY: TOP IS THE CHART, BOTTOM THE MOVE BY THE COUNT.  +4 = TRUE COUNT +4 OR MORE,  <0 = BELOW 0";
			float kp = std::min(2.6f, 1400.0f / DigitFont::textWidth(key, 1.0f));
			DigitFont::drawText(state, key, (1440.0f - DigitFont::textWidth(key, kp)) / 2.0f, legendY + 36.0f, kp, MARK_ABOVE);
			noteY = legendY + 54.0f;
		}
		std::string note = noteFor(rules);
		float np = std::min(marks ? 2.6f : 3.0f, 1400.0f / DigitFont::textWidth(note, 1.0f));
		DigitFont::drawText(state, note, (1440.0f - DigitFont::textWidth(note, np)) / 2.0f, noteY, np, SDL_Color{230, 210, 140, 255});
	}

	// windowX/windowY: raw event coordinates in window space, same
	// convention as Menu/SetupMenu's handlePoint. Returns true once BACK
	// is hit -- caller (mina.cpp) is responsible for what "back" means.
	bool handlePoint(SDLState& state, float windowX, float windowY){
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			return false;

		SDL_FPoint p{x, y};
		// An open cell explanation: any tap closes it.
		if(detailSection >= 0){
			detailSection = -1;
			return false;
		}
		if(!listing && cellAt(p, detailSection, detailRow, detailCol))
			return false;
		detailSection = -1;
		if(!deviationLines.empty() && SDL_PointInRectFloat(&p, &listButton)){
			listing = !listing;
			return false;
		}
		SDL_FRect back = backRect();
		return SDL_PointInRectFloat(&p, &back);
	}

	std::vector<SDL_FRect> focusRects(){
		if(deviationLines.empty())
			return { backRect() };
		return { backRect(), listButton };
	}

	static constexpr SDL_Color MARK_ABOVE{255, 170, 60, 255};

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

	// A cell's text for these rules ("S", "DS", "S4"...).
	static std::string cellLabel(const Rules& rules, int section, int row, int col){
		return cellText(rules, section, row, col, code(rules.set, section, row, col));
	}

	static int rowCount(int section){ return section == 1 ? 8 : 10; }

	static const char* rowLabel(int section, int row){
		if(section == 0) return HARD_LABELS[row];
		if(section == 1) return SOFT_LABELS[row];
		return PAIR_LABELS[row];
	}

	// The published code for a cell of a chart (see the codes above).
	static char code(int set, int section, int row, int col){
		const ChartSet& chart = CHARTS[set];
		if(section == 0) return chart.hard[row][col];
		if(section == 1) return chart.soft[row][col];
		return chart.pairs[row][col];
	}
	static char code(int section, int row, int col){ return code(active.set, section, row, col); }

	// A code as a plain move for the given rules.
	static char resolve(char c, bool das, bool surrender){
		switch(c){
			case 'd': return 'D';
			case 'p': return das ? 'P' : 'H';
			case 'x': return das ? 'P' : 'D';
			case 'y': return das ? 'P' : 'S';
			case 'R': return surrender ? 'R' : 'H';
			case 'r': return surrender ? 'R' : 'S';
			case 'q': return (surrender && !das) ? 'R' : 'P';
			default: return c;
		}
	}
	static char resolve(char c){ return resolve(c, active.das, active.surrender); }

	// What a code falls back to when its move can't be made right now
	// (double on a hand that can't double, surrender after a hit).
	static char fallback(char c){
		switch(c){
			case 'd': case 'y': case 'r': return 'S';
			case 'q': return 'P';
			default: return 'H';
		}
	}

	// Spanish 21's card-count limit for a cell: the move holds only with
	// fewer cards than this; with that many or more, hit. 0 = no limit.
	static int cardLimit(int set, int section, int row, int col){
		const ChartSet& chart = CHARTS[set];
		const char* const* digits = section == 0 ? chart.hardCards : section == 1 ? chart.softCards : nullptr;
		return digits ? digits[row][col] - '0' : 0;
	}
	static int cardLimit(int section, int row, int col){ return cardLimit(active.set, section, row, col); }

	// Spanish 21 cells (a yellow corner on the chart) where the 6-7-8 /
	// 7-7-7 bonuses change the play: hit hard 13-15 when the two cards
	// could still make 6-7-8 ('A' any suits, 'S' suited, 'K' both
	// spades), and hit -- not split -- suited 7s vs 7 ('7').
	static char bonusRule(int set, int section, int row, int col){
		const ChartSet& chart = CHARTS[set];
		if(!chart.hardBonus || section == 1)
			return ' ';
		if(section == 2)
			return (row == 5 && col == 5) ? '7' : ' ';
		return chart.hardBonus[row][col];
	}
	static char bonusRule(int section, int row, int col){ return bonusRule(active.set, section, row, col); }

	// Free Bet's free-hand row (codes), or nullptr when the regular chart
	// applies.
	static const char* freeHandRow(int section, int row){
		if(active.set != FREE_BET || section == 2)
			return nullptr;
		return section == 0 ? FREEBET_FREE_HARD[row] : FREEBET_FREE_SOFT[row];
	}

	// The chart and rules for a game: double after split is allowed in
	// the shoe games, not the 2-deck ones; Free Bet has no surrender and
	// its dealer always hits soft 17.
	static Rules rulesFor(GameMode mode, bool dealerHitsSoft17){
		Rules r;
		if(isPlayersEdge(mode))
			r.set = dealerHitsSoft17 ? SPANISH_H17 : SPANISH_S17;
		else if(isFreeBet(mode))
			r.set = FREE_BET;
		else if(deckCountFor(mode) <= 2)
			r.set = dealerHitsSoft17 ? DOUBLE_DECK_H17 : DOUBLE_DECK_S17;
		else
			r.set = dealerHitsSoft17 ? SHOE_H17 : SHOE_S17;
		r.das = deckCountFor(mode) > 2 || isPlayersEdge(mode);
		r.surrender = !isFreeBet(mode);
		return r;
	}

	static void useChartFor(GameMode mode, bool dealerHitsSoft17 = true){
		active = rulesFor(mode, dealerHitsSoft17);
	}

	static bool isSpanish(int set){ return set == SPANISH_H17 || set == SPANISH_S17; }
	static bool isStandard(int set){ return set <= SINGLE_DECK_S17; }

	static std::string titleFor(const Rules& rules){
		std::string name = CHARTS[rules.set].name;
		if(isStandard(rules.set)){
			name += rules.das ? ", DAS" : ", NO DAS";
			name += rules.surrender ? ", SURRENDER" : ", NO SURRENDER";
		} else if(isSpanish(rules.set) && !rules.surrender){
			name += ", NO SURRENDER";
		}
		return name;
	}

	inline static Rules active{};

private:
	// STRIDE is the distance between cell starts; CELL_W/H (smaller than
	// the stride) is the box actually drawn, leaving a visible gap between
	// cells. 3 tables x 11 columns have to fit across 1440.
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

	using ChartSet = StrategyChartSet;

	// ---- Standard, 4-8 decks ----
	static constexpr const char* SHOE_H17_HARD[10] = {
		"HHHHHHHHHH", "HDDDDHHHHH", "DDDDDDDDHH", "DDDDDDDDDD", "HHSSSHHHHH",
		"SSSSSHHHHH", "SSSSSHHHHH", "SSSSSHHHRR", "SSSSSHHRRR", "SSSSSSSSSr"
	};
	static constexpr const char* SHOE_H17_SOFT[8] = {
		"HHHDDHHHHH", "HHHDDHHHHH", "HHDDDHHHHH", "HHDDDHHHHH",
		"HDDDDHHHHH", "dddddSSHHH", "SSSSdSSSSS", "SSSSSSSSSS"
	};
	static constexpr const char* SHOE_H17_PAIRS[10] = {
		"ppPPPPHHHH", "ppPPPPHHHH", "HHHppHHHHH", "DDDDDDDDHH", "pPPPPHHHHH",
		"PPPPPPHHHH", "PPPPPPPPPq", "PPPPPSPPSS", "SSSSSSSSSS", "PPPPPPPPPP"
	};
	static constexpr const char* SHOE_S17_HARD[10] = {
		"HHHHHHHHHH", "HDDDDHHHHH", "DDDDDDDDHH", "DDDDDDDDDH", "HHSSSHHHHH",
		"SSSSSHHHHH", "SSSSSHHHHH", "SSSSSHHHRH", "SSSSSHHRRR", "SSSSSSSSSS"
	};
	static constexpr const char* SHOE_S17_SOFT[8] = {
		"HHHDDHHHHH", "HHHDDHHHHH", "HHDDDHHHHH", "HHDDDHHHHH",
		"HDDDDHHHHH", "SddddSSHHH", "SSSSSSSSSS", "SSSSSSSSSS"
	};
	static constexpr const char* SHOE_S17_PAIRS[10] = {
		"ppPPPPHHHH", "ppPPPPHHHH", "HHHppHHHHH", "DDDDDDDDHH", "pPPPPHHHHH",
		"PPPPPPHHHH", "PPPPPPPPPP", "PPPPPSPPSS", "SSSSSSSSSS", "PPPPPPPPPP"
	};

	// ---- Standard, 2 decks ----
	static constexpr const char* DD_H17_HARD[10] = {
		"HHHHHHHHHH", "DDDDDHHHHH", "DDDDDDDDHH", "DDDDDDDDDD", "HHSSSHHHHH",
		"SSSSSHHHHH", "SSSSSHHHHH", "SSSSSHHHRR", "SSSSSHHHRR", "SSSSSSSSSr"
	};
	static constexpr const char* DD_H17_SOFT[8] = {
		"HHHDDHHHHH", "HHDDDHHHHH", "HHDDDHHHHH", "HHDDDHHHHH",
		"HDDDDHHHHH", "dddddSSHHH", "SSSSdSSSSS", "SSSSSSSSSS"
	};
	static constexpr const char* DD_H17_PAIRS[10] = {
		"ppPPPPHHHH", "ppPPPPHHHH", "HHHppHHHHH", "DDDDDDDDHH", "PPPPPpHHHH",
		"PPPPPPpHHH", "PPPPPPPPPq", "PPPPPSPPSS", "SSSSSSSSSS", "PPPPPPPPPP"
	};
	static constexpr const char* DD_S17_HARD[10] = {
		"HHHHHHHHHH", "DDDDDHHHHH", "DDDDDDDDHH", "DDDDDDDDDD", "HHSSSHHHHH",
		"SSSSSHHHHH", "SSSSSHHHHH", "SSSSSHHHRH", "SSSSSHHHRR", "SSSSSSSSSS"
	};
	static constexpr const char* DD_S17_SOFT[8] = {
		"HHHDDHHHHH", "HHHDDHHHHH", "HHDDDHHHHH", "HHDDDHHHHH",
		"HDDDDHHHHH", "SddddSSHHH", "SSSSSSSSSS", "SSSSSSSSSS"
	};
	static constexpr const char* DD_S17_PAIRS[10] = {
		"ppPPPPHHHH", "ppPPPPHHHH", "HHHppHHHHH", "DDDDDDDDHH", "PPPPPpHHHH",
		"PPPPPPpHHH", "PPPPPPPPPP", "PPPPPSPPSS", "SSSSSSSSSS", "PPPPPPPPPP"
	};

	// ---- Standard, 1 deck (the "8" row is 8 only; 7 and less hit) ----
	static constexpr const char* SD_H17_HARD[10] = {
		"HHHDDHHHHH", "DDDDDHHHHH", "DDDDDDDDHH", "DDDDDDDDDD", "HHSSSHHHHH",
		"SSSSSHHHHH", "SSSSSHHHHH", "SSSSSHHHHR", "SSSSSHHHRR", "SSSSSSSSSr"
	};
	static constexpr const char* SD_H17_SOFT[8] = {
		"HHDDDHHHHH", "HHDDDHHHHH", "HHDDDHHHHH", "HHDDDHHHHH",
		"DDDDDHHHHH", "SddddSSHHH", "SSSSdSSSSS", "SSSSSSSSSS"
	};
	static constexpr const char* SD_H17_PAIRS[10] = {
		"pPPPPPHHHH", "ppPPPPpHHH", "HHpxxHHHHH", "DDDDDDDDHH", "PPPPPpHHHH",
		"PPPPPPpHrR", "PPPPPPPPPP", "PPPPPSPPSy", "SSSSSSSSSS", "PPPPPPPPPP"
	};
	static constexpr const char* SD_S17_HARD[10] = {
		"HHHDDHHHHH", "DDDDDHHHHH", "DDDDDDDDHH", "DDDDDDDDDD", "HHSSSHHHHH",
		"SSSSSHHHHH", "SSSSSHHHHH", "SSSSSHHHHH", "SSSSSHHHRR", "SSSSSSSSSS"
	};
	static constexpr const char* SD_S17_SOFT[8] = {
		"HHDDDHHHHH", "HHDDDHHHHH", "HHDDDHHHHH", "HHDDDHHHHH",
		"DDDDDHHHHH", "SddddSSHHS", "SSSSdSSSSS", "SSSSSSSSSS"
	};
	static constexpr const char* SD_S17_PAIRS[10] = {
		"pPPPPPHHHH", "ppPPPPpHHH", "HHpxxHHHHH", "DDDDDDDDHH", "PPPPPpHHHH",
		"PPPPPPpHrH", "PPPPPPPPPP", "PPPPPSPPSS", "SSSSSSSSSS", "PPPPPPPPPP"
	};

	// ---- Spanish 21 (Player's Edge). Doubling is allowed on any number
	// of cards; 4s, 5s and 10s are never split. The digit strings are the
	// card-count limits (S4 = stand, but hit with 4+ cards), the bonus
	// strings the 6-7-8 plays. ----
	static constexpr const char* SP_H17_HARD[10] = {
		"HHHHHHHHHH", "HHHHDHHHHH", "DDDDDDDHHH", "DDDDDDDDDD", "HHHHHHHHHH",
		"HHHHSHHHHH", "HHSSSHHHHH", "SSSSSHHHHH", "SSSSSHHHHR", "SSSSSSSSSR"
	};
	static constexpr const char* SP_H17_HARD_CARDS[10] = {
		"0000000000", "0000000000", "5500043000", "4555544433", "0000000000",
		"0000400000", "0045600000", "4566000000", "6660000000", "0000006663"
	};
	static constexpr const char* SP_H17_HARD_BONUS[10] = {
		"          ", "          ", "          ", "          ", "          ",
		"    A     ", "  ASK     ", "AS        ", "          ", "          "
	};
	static constexpr const char* SP_H17_SOFT[8] = {
		"HHHHHHHHHH", "HHHHHHHHHH", "HHHHDHHHHH", "HHHDDHHHHH",
		"HHDDDHHHHH", "SSDDDSSHHH", "SSSSSSSSSS", "SSSSSSSSSS"
	};
	static constexpr const char* SP_H17_SOFT_CARDS[8] = {
		"0000000000", "0000000000", "0000400000", "0003400000",
		"0034500000", "4445664000", "0000000066", "0000000000"
	};
	static constexpr const char* SP_H17_PAIRS[10] = {
		"PPPPPPPHHH", "PPPPPPPHHH", "HHHHHHHHHH", "DDDDDDDHHH", "HHPPPHHHHH",
		"PPPPPPHHHH", "PPPPPPPPPR", "SPPPPSPPSS", "SSSSSSSSSS", "PPPPPPPPPP"
	};
	static constexpr const char* SP_S17_HARD[10] = {
		"HHHHHHHHHH", "HHHHDHHHHH", "DDDDDDDHHH", "DDDDDDDDDD", "HHHHHHHHHH",
		"HHHHHHHHHH", "HHSSSHHHHH", "SSSSSHHHHH", "SSSSSHHHHH", "SSSSSSSSSR"
	};
	static constexpr const char* SP_S17_HARD_CARDS[10] = {
		"0000000000", "0000400000", "5500043000", "4555544433", "0000000000",
		"0000000000", "0045400000", "4556600000", "5660000000", "0000006663"
	};
	static constexpr const char* SP_S17_HARD_BONUS[10] = {
		"          ", "          ", "          ", "          ", "          ",
		"          ", "  AAA     ", "AAK K     ", "          ", "          "
	};
	static constexpr const char* SP_S17_SOFT[8] = {
		"HHHHHHHHHH", "HHHHHHHHHH", "HHHHHHHHHH", "HHHHDHHHHH",
		"HHDDDHHHHH", "SSDDDSSHHH", "SSSSSSSSSS", "SSSSSSSSSS"
	};
	static constexpr const char* SP_S17_SOFT_CARDS[8] = {
		"0000000000", "0000000000", "0000000000", "0000400000",
		"0034500000", "4445564000", "0000000060", "0000000060"
	};
	static constexpr const char* SP_S17_PAIRS[10] = {
		"PPPPPPPHHH", "PPPPPPPHHH", "HHHHHHHHHH", "DDDDDDDHHH", "HHPPPHHHHH",
		"PPPPPPHHHH", "PPPPPPPPPP", "SPPPPSPPSS", "SSSSSSSSSS", "PPPPPPPPPP"
	};

	// ---- Free Bet (6 decks, H17, no surrender). D on hard 9-11 is the
	// free double; free splits of everything but 10s (5s double). ----
	static constexpr const char* FREEBET_HARD[10] = {
		"HHHHHHHHHH", "DDDDDDDDDD", "DDDDDDDDDD", "DDDDDDDDDD", "HHHSSHHHHH",
		"HSSSSHHHHH", "SSSSSHHHHH", "SSSSSHHHHH", "SSSSSHHHHH", "SSSSSSSSSS"
	};
	static constexpr const char* FREEBET_SOFT[8] = {
		"HHHHHHHHHH", "HHHHHHHHHH", "HHHHHHHHHH", "HHHHDHHHHH",
		"HHHDDHHHHH", "SSSDDSSHHH", "SSSSSSSSSS", "SSSSSSSSSS"
	};
	static constexpr const char* FREEBET_PAIRS[10] = {
		"PPPPPPPPPP", "PPPPPPPPPP", "PPPPPPPPPP", "DDDDDDDDDD", "PPPPPPPPPP",
		"PPPPPPPPPP", "PPPPPPPPPP", "PPPPPPPPPP", "SSSSSSSSSS", "PPPPPPPPPP"
	};
	// A free split-off hand (nothing of the player's own at stake) plays
	// more aggressively.
	static constexpr const char* FREEBET_FREE_HARD[10] = {
		"HHHHHHHHHH", "DDDDDDDDDD", "DDDDDDDDDD", "DDDDDDDDDD", "HHHSSHHHHH",
		"HSSSSHHHHH", "HSSSSHHHHH", "SSSSSHHHHH", "SSSSSHHHHH", "SSSSSHHHSH"
	};
	static constexpr const char* FREEBET_FREE_SOFT[8] = {
		"HHHHHHHHHH", "HHHHHHHHHH", "HHHHHHHHHH", "HHHHDHHHHH",
		"HHHDDHHHHH", "HHDDDSHHHH", "SSSDDSSSSS", "SSSSDSSSSS"
	};

	static constexpr ChartSet CHARTS[SET_COUNT] = {
		{ "4-8 DECKS, DEALER HITS SOFT 17", SHOE_H17_HARD, SHOE_H17_SOFT, SHOE_H17_PAIRS },
		{ "4-8 DECKS, DEALER STANDS SOFT 17", SHOE_S17_HARD, SHOE_S17_SOFT, SHOE_S17_PAIRS },
		{ "2 DECKS, DEALER HITS SOFT 17", DD_H17_HARD, DD_H17_SOFT, DD_H17_PAIRS },
		{ "2 DECKS, DEALER STANDS SOFT 17", DD_S17_HARD, DD_S17_SOFT, DD_S17_PAIRS },
		{ "1 DECK, DEALER HITS SOFT 17", SD_H17_HARD, SD_H17_SOFT, SD_H17_PAIRS },
		{ "1 DECK, DEALER STANDS SOFT 17", SD_S17_HARD, SD_S17_SOFT, SD_S17_PAIRS },
		{ "SPANISH 21, DEALER HITS SOFT 17", SP_H17_HARD, SP_H17_SOFT, SP_H17_PAIRS,
			SP_H17_HARD_CARDS, SP_H17_SOFT_CARDS, SP_H17_HARD_BONUS },
		{ "SPANISH 21, DEALER STANDS SOFT 17", SP_S17_HARD, SP_S17_SOFT, SP_S17_PAIRS,
			SP_S17_HARD_CARDS, SP_S17_SOFT_CARDS, SP_S17_HARD_BONUS },
		{ "FREE BET BLACKJACK, DEALER HITS SOFT 17", FREEBET_HARD, FREEBET_SOFT, FREEBET_PAIRS }
	};

	SDL_FRect backButton{ .x = 620, .y = 630, .w = 200, .h = 56 };
	// With DEVIATIONS beside it, BACK moves left to keep the pair centred.
	SDL_FRect listButton{ .x = 730, .y = 630, .w = 260, .h = 56 };
	SDL_FRect backRect() const{
		return deviationLines.empty() ? backButton : SDL_FRect{ .x = 510, .y = 630, .w = 200, .h = 56 };
	}

	// What a chart's notes line under the legend says.
	static std::string noteFor(const Rules& rules){
		if(isSpanish(rules.set))
			return "S4, D3...: HIT WITH THAT MANY CARDS OR MORE.  YELLOW CORNER: HIT IF A 6-7-8 OR SUITED 7-7-7 BONUS IS STILL POSSIBLE";
		if(rules.set == FREE_BET)
			return "D ON HARD 9-11 AND 5S IS THE FREE DOUBLE.  A FREE SPLIT HAND PLAYS ITS OWN CHART - TIP SHOWS IT";
		return "DS: DOUBLE IF ALLOWED, OTHERWISE STAND.  18+ ALWAYS STANDS.  7 AND LESS ALWAYS HITS";
	}

	// A cell's text: its move for these rules, plus Spanish 21's card
	// limit ("S4") or "DS" for double-else-stand.
	static std::string cellText(const Rules& rules, int section, int row, int col, char c){
		char move = resolve(c, rules.das, rules.surrender);
		std::string text(1, move);
		if(c == 'd')
			text = "DS";
		int limit = cardLimit(rules.set, section, row, col);
		if(limit > 0 && (move == 'S' || move == 'D' || move == 'R'))
			text += std::to_string(limit);
		return text;
	}

	static void fill(SDLState& state, const SDL_FRect& r, SDL_Color c){
		SDL_SetRenderDrawColor(state.renderer, c.r, c.g, c.b, 255);
		SDL_RenderFillRect(state.renderer, &r);
	}

	// Centred in r, shrunk to fit its width.
	static void smallText(SDLState& state, const std::string& text, const SDL_FRect& r, float pixel, SDL_Color c){
		pixel = std::min(pixel, (r.w - 4.0f) / std::max(1.0f, DigitFont::textWidth(text, 1.0f)));
		DigitFont::drawText(state, text, r.x + (r.w - DigitFont::textWidth(text, pixel)) / 2.0f, r.y + (r.h - 5.0f * pixel) / 2.0f, pixel, c);
	}

	static void drawCell(SDLState& state, const SDL_FRect& rect, SDL_Color color, const std::string& text, float pixel, bool highlight){
		// Highlighted cell gets an unmistakable bright fill + black text
		// instead of its normal color.
		SDL_Color fill = highlight ? SDL_Color{255, 225, 60, 255} : color;
		SDL_Color textColor = highlight ? SDL_Color{20, 20, 20, 255} : SDL_Color{255, 255, 255, 255};

		SDL_SetRenderDrawColor(state.renderer, fill.r, fill.g, fill.b, fill.a);
		SDL_RenderFillRect(state.renderer, &rect);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &rect);

		if(!text.empty()){
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

	static void drawTable(SDLState& state, const Rules& rules, int section, float x, int highlightRow, int highlightCol, const Tint* tint, const Marks* marks = nullptr){
		const char* const* rowLabels = section == 0 ? HARD_LABELS : section == 1 ? SOFT_LABELS : PAIR_LABELS;
		int rowCount = section == 1 ? 8 : 10;

		float titlePixel = 5.0f;
		std::string title = sectionTitle(section);
		float titleW = DigitFont::textWidth(title, titlePixel);
		float gridW = 11.0f * CELL_W + 10.0f * CELL_GAP;
		DigitFont::drawText(state, title, x + (gridW - titleW) / 2.0f, TABLE_TOP - 36.0f, titlePixel, SDL_Color{255, 255, 255, 255});

		// One pixel size for every cell (2 characters fit at 2.7).
		float cellPixel = 2.7f;

		for(int c = 0; c < 10; c++){
			SDL_FRect rect{ .x = x + STRIDE_W * (c + 1), .y = TABLE_TOP, .w = CELL_W, .h = CELL_H };
			drawCell(state, rect, SDL_Color{40, 60, 45, 255}, COL_LABELS[c], cellPixel, false);
		}

		for(int r = 0; r < rowCount; r++){
			// Highest total right under the header, lowest at the bottom.
			float rowY = TABLE_TOP + STRIDE_H * (rowCount - r);

			SDL_FRect labelRect{ .x = x, .y = rowY, .w = CELL_W, .h = CELL_H };
			drawCell(state, labelRect, SDL_Color{40, 60, 45, 255}, rowLabels[r], cellPixel, false);

			for(int c = 0; c < 10; c++){
				char cd = code(rules.set, section, r, c);
				char move = resolve(cd, rules.das, rules.surrender);
				SDL_FRect rect{ .x = x + STRIDE_W * (c + 1), .y = rowY, .w = CELL_W, .h = CELL_H };
				bool hl = (r == highlightRow && c == highlightCol);
				SDL_Color color = colorFor(move);
				if(tint)
					(*tint)(section, r, c, color);
				drawCell(state, rect, color, cellText(rules, section, r, c, cd), cellPixel, hl);
				// INDEX PLAYS: the cell splits -- the chart's move on top, the
				// count's move (in its colour) and when, underneath.
				std::string when;
				char devMove = 0;
				if(marks && (*marks)(section, r, c, when, devMove)){
					SDL_FRect top{ rect.x, rect.y, rect.w, rect.h / 2.0f };
					SDL_FRect bottom{ rect.x, rect.y + rect.h / 2.0f, rect.w, rect.h / 2.0f };
					fill(state, top, color);
					fill(state, bottom, colorFor(devMove));
					smallText(state, cellText(rules, section, r, c, cd), top, 2.0f, SDL_Color{255, 255, 255, 255});
					smallText(state, std::string(1, devMove) + when, bottom, 2.0f, SDL_Color{255, 235, 150, 255});
					SDL_SetRenderDrawColor(state.renderer, MARK_ABOVE.r, MARK_ABOVE.g, MARK_ABOVE.b, 255);
					SDL_RenderRect(state.renderer, &rect);
					SDL_FRect inner{ rect.x + 1.0f, rect.y + 1.0f, rect.w - 2.0f, rect.h - 2.0f };
					SDL_RenderRect(state.renderer, &inner);
				}
				// 6-7-8 / 7-7-7 bonus plays: a yellow corner on the cell.
				if(bonusRule(rules.set, section, r, c) != ' '){
					SDL_FRect corner{ .x = rect.x + rect.w - 9.0f, .y = rect.y + 1.0f, .w = 8.0f, .h = 8.0f };
					SDL_SetRenderDrawColor(state.renderer, 255, 225, 60, 255);
					SDL_RenderFillRect(state.renderer, &corner);
				}
			}
		}
	}

	// A swatch + letter + meaning for each move, plus a sample of the
	// highlight style ("this is your hand").
	static void drawLegend(SDLState& state, float y){
		struct Entry{ char code; const char* label; };
		static const Entry entries[5] = {
			{'H', "HIT"}, {'S', "STAND"}, {'D', "DOUBLE"}, {'P', "SPLIT"}, {'R', "SURRENDER"}
		};
		std::string highlightLabel = "YOUR HAND";
		float swatchW = CELL_W, swatchH = CELL_H;
		float pixel = 3.3f;
		float swatchLabelGap = 10.0f;
		float entryGap = 24.0f;

		float widths[6];
		for(int i = 0; i < 5; i++)
			widths[i] = swatchW + swatchLabelGap + DigitFont::textWidth(entries[i].label, pixel);
		widths[5] = swatchW + swatchLabelGap + DigitFont::textWidth(highlightLabel, pixel);
		float totalW = 5 * entryGap;
		for(float w : widths)
			totalW += w;

		float x = (1440.0f - totalW) / 2.0f;
		for(int i = 0; i < 5; i++){
			SDL_FRect swatch{ .x = x, .y = y, .w = swatchW, .h = swatchH };
			drawCell(state, swatch, colorFor(entries[i].code), std::string(1, entries[i].code), pixel, false);
			DigitFont::drawText(state, entries[i].label, x + swatchW + swatchLabelGap, y + (swatchH - 5.0f * pixel) / 2.0f, pixel, SDL_Color{255, 255, 255, 255});
			x += widths[i] + entryGap;
		}
		SDL_FRect sample{ .x = x, .y = y, .w = swatchW, .h = swatchH };
		drawCell(state, sample, SDL_Color{90, 90, 90, 255}, "", pixel, true);
		DigitFont::drawText(state, highlightLabel, x + swatchW + swatchLabelGap, y + (swatchH - 5.0f * pixel) / 2.0f, pixel, SDL_Color{255, 255, 255, 255});
	}

public:
	static void drawButton(SDLState& state, const SDL_FRect& rect, SDL_Color color, const std::string& label, float pixel = 6.0f){
		SDL_SetRenderDrawColor(state.renderer, color.r, color.g, color.b, color.a);
		SDL_RenderFillRect(state.renderer, &rect);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &rect);

		float w = DigitFont::textWidth(label, pixel);
		if(w > rect.w - 16.0f){
			pixel *= (rect.w - 16.0f) / w;
			w = DigitFont::textWidth(label, pixel);
		}
		DigitFont::drawText(state, label, rect.x + (rect.w - w) / 2.0f, rect.y + (rect.h - 5 * pixel) / 2.0f, pixel, SDL_Color{255, 255, 255, 255});
	}
};
