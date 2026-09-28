#pragma once
#include "Game.h"
#include "DigitFont.h"
#include "GameModeMenu.h"
#include <string>
#include <algorithm>

// The vertical setup screen shown before a fresh game (Start or Restart off
// the main menu -- Resume skips this entirely and reuses whatever was saved
// last time, see SaveData). Lets the player pick how many are playing
// (1-3, capped by the table's fixed 3-seat layout -- see Table.h's
// xs[]/ys[]/direction[]) and each player's starting bankroll, either
// directly or via a per-player "count of min bets x min bet size"
// calculator. This only decides the *starting* number -- actually wagering
// it during play is separate, not-yet-built work.
//
// Layout: two columns per player (BANK+BET stacked left of CALC+CNT) with
// the caption inline to the left of each stepper rather than on its own
// line -- fits three players' worth of large (48px-tall), thumb-sized
// buttons within the 720-tall logical canvas without needing to scroll.
class SetupMenu
{
public:
	struct PlayerConfig{
		int bankroll = 500;
		bool useCalculator = false;
		int minBet = 25;
		int minBetCount = 10;
		// Only shown/used when the chosen GameMode actually has a side bet
		// (see gameMode/hasAnySideBet() below) -- Player's Edge seeds both
		// of its Match bets to this same amount (Person::setInitialMatchBets()).
		int sideBetSize = 5;

		// BET (+ side bet, if this mode has one) x CNT (how many rounds to
		// plan for) -- becomes the actual bankroll while the calculator's
		// toggled on. includeSideBet lets the caller decide based on the
		// active GameMode without this struct needing to know about modes
		// itself.
		int effectiveBankroll(bool includeSideBet) const{
			int perRoundStake = minBet + (includeSideBet ? sideBetSize : 0);
			return useCalculator ? perRoundStake * minBetCount : bankroll;
		}
	};

	int numberOfPlayers = 1;
	PlayerConfig playerConfigs[3];

	// Set by mina.cpp right after GameModeMenu picks a mode, before this
	// screen is ever shown -- drives whether the side-bet stepper (and its
	// extra vertical space) appears at all.
	void setGameMode(GameMode mode){
		gameMode = mode;
	}

	void draw(SDLState& state, Resources& res){
		SDL_SetRenderDrawColor(state.renderer, 15, 55, 28, 255);
		SDL_RenderFillRect(state.renderer, nullptr);

		drawStepper(state, playersStepper(), "PLAYERS", numberOfPlayers);

		for(int i = 0; i < numberOfPlayers; i++)
			drawPlayerRow(state, i);

		SDL_FRect go = confirmButton();
		drawButton(state, go, SDL_Color{60, 130, 70, 255});
		float goW = DigitFont::textWidth("GO", 8.0f);
		DigitFont::drawText(state, "GO", go.x + (go.w - goW) / 2.0f, go.y + (go.h - 5 * 8.0f) / 2.0f, 8.0f, WHITE);
	}

	// windowX/windowY: raw event coordinates in window space, same
	// convention as Menu::handlePoint -- converted here into the renderer's
	// logical space before hit-testing. Mutates this menu's own config
	// state directly; returns true only once, when GO is tapped/clicked,
	// which is the caller's signal to actually start the game with
	// whatever's configured (see PlayerConfig::effectiveBankroll()).
	bool handlePoint(SDLState& state, float windowX, float windowY){
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			return false;

		SDL_FPoint p{x, y};

		Stepper players = playersStepper();
		if(SDL_PointInRectFloat(&p, &players.minus))
			numberOfPlayers = std::max(1, numberOfPlayers - 1);
		else if(SDL_PointInRectFloat(&p, &players.plus))
			numberOfPlayers = std::min(3, numberOfPlayers + 1);

		for(int i = 0; i < numberOfPlayers; i++)
			handlePlayerRowPoint(i, p);

		SDL_FRect confirm = confirmButton();
		if(SDL_PointInRectFloat(&p, &confirm))
			return true;

		return false;
	}

private:
	static constexpr SDL_Color WHITE{255, 255, 255, 255};

	// Column A (BANK/CALC), column B (BET/CNT), and -- only when the chosen
	// mode has a side bet -- column C (a single SIDE BET stepper, to the
	// *right* of A/B rather than a 3rd row stacked underneath both: with 3
	// players, a stacked 3rd row pushed the block past the bottom of the
	// 720-tall canvas and cut off the last player entirely). Buttons are
	// thumb-sized (48px tall, matching Table.h's bet buttons -- the
	// game-wide minimum for anything meant to be tapped). The column count
	// changes how wide the whole block is, so COL_A_X is computed from
	// however many columns are actually showing, keeping it centered
	// either way instead of drifting left when the 3rd column appears.
	static constexpr float COL_GAP = 40;
	// Wide enough for "PLAYERS" (the longest caption) at CAPTION_PIXEL --
	// it was clipping into the stepper's minus button before.
	// Was 5.0f -- DigitFont's 5-wide glyphs run ~50% wider per character
	// than the font's old 3-wide ones, and "PLAYERS" no longer fit
	// CAPTION_W at the old size.
	static constexpr float CAPTION_PIXEL = 3.3f;
	static constexpr float CAPTION_W = 140;
	static constexpr float STEP_BTN_W = 56;
	static constexpr float STEP_BTN_H = 48;
	static constexpr float VALUE_W = 110;
	static constexpr float ROW_GAP = 8;
	static constexpr float COL_W = CAPTION_W + STEP_BTN_W + 6 + VALUE_W + 6 + STEP_BTN_W;

	int columnCount(){ return hasAnySideBet(gameMode) ? 3 : 2; }
	float blockWidth(){ return columnCount() * COL_W + (columnCount() - 1) * COL_GAP; }
	float colX(int col){ return (1440.0f - blockWidth()) / 2.0f + col * (COL_W + COL_GAP); }

	struct Stepper{ SDL_FRect minus, value, plus; std::string caption; };

	Stepper stepperAt(float x, float y, const std::string& caption){
		float sx = x + CAPTION_W;
		return Stepper{
			SDL_FRect{ .x = sx, .y = y, .w = STEP_BTN_W, .h = STEP_BTN_H },
			SDL_FRect{ .x = sx + STEP_BTN_W + 6, .y = y, .w = VALUE_W, .h = STEP_BTN_H },
			SDL_FRect{ .x = sx + STEP_BTN_W + 6 + VALUE_W + 6, .y = y, .w = STEP_BTN_W, .h = STEP_BTN_H },
			caption
		};
	}

	Stepper playersStepper(){ return stepperAt(colX(0), 30, "PLAYERS"); }

	GameMode gameMode = GameMode::TwoDeck;

	// Fixed per-player row height again now that the side bet lives in a
	// 3rd column instead of a 3rd row -- it no longer affects how tall a
	// player's block is, only how wide the whole screen's content is (see
	// columnCount()/blockWidth()).
	float playerRowTop(int i){ return 90.0f + i * 175.0f; }

	// Row 1: BANK (direct bankroll) | BET (min bet size) | SIDE BET
	Stepper bankStepper(int i){ return stepperAt(colX(0), playerRowTop(i) + 26, "BANK"); }
	Stepper betStepper(int i){ return stepperAt(colX(1), playerRowTop(i) + 26, "BET"); }
	// Column C, only present when hasAnySideBet(gameMode) -- "SIDE" (not
	// "SIDE BET") since CAPTION_W is sized for "PLAYERS" and the longer
	// label would clip.
	Stepper sideBetStepper(int i){ return stepperAt(colX(2), playerRowTop(i) + 26, "SIDE"); }

	// Row 2: CALC (toggle) | CNT (how many min bets)
	SDL_FRect calcToggle(int i){ return SDL_FRect{ colX(0), playerRowTop(i) + 26 + STEP_BTN_H + ROW_GAP, COL_W, STEP_BTN_H }; }
	Stepper cntStepper(int i){ return stepperAt(colX(1), playerRowTop(i) + 26 + STEP_BTN_H + ROW_GAP, "CNT"); }

	SDL_FRect confirmButton(){
		float y = playerRowTop(numberOfPlayers) + 8.0f;
		float w = 260.0f;
		return SDL_FRect{ colX(0) + (blockWidth() - w) / 2.0f, y, w, 56 };
	}

	void drawButton(SDLState& state, const SDL_FRect& rect, SDL_Color color){
		SDL_SetRenderDrawColor(state.renderer, color.r, color.g, color.b, color.a);
		SDL_RenderFillRect(state.renderer, &rect);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &rect);
	}

	void drawStepper(SDLState& state, const Stepper& s, const std::string& captionOverride, int value, SDL_Color valueColor = SDL_Color{10, 40, 20, 230}){
		const std::string& caption = captionOverride.empty() ? s.caption : captionOverride;
		// Auto-fits per caption instead of one pixel size shared by all of
		// them -- "PLAYERS" is the longest and needs CAPTION_PIXEL's small
		// size to fit CAPTION_W, but that left every shorter caption
		// ("BET", "CNT", ...) much smaller than it needed to be, with a
		// big empty gap around it. This starts from a bigger base size and
		// only shrinks a specific caption down if it actually has to.
		float captionPixel = 5.0f;
		float maxCaptionW = CAPTION_W - 8.0f;
		float captionW = DigitFont::textWidth(caption, captionPixel);
		if(captionW > maxCaptionW && captionW > 0.0f)
			captionPixel *= maxCaptionW / captionW;
		captionW = DigitFont::textWidth(caption, captionPixel);
		// Right-aligned within its CAPTION_W slot, not left-aligned at the
		// slot's start -- a short caption like "BET"/"CNT" left a wide gap
		// of empty space between itself and the button it's labeling,
		// since it no longer needs the full slot width the way "PLAYERS"
		// does. 10px keeps a small, consistent gap instead of butting
		// right up against the button.
		DigitFont::drawText(state, caption, s.minus.x - captionW - 10.0f, s.minus.y + (STEP_BTN_H - 5 * captionPixel) / 2.0f, captionPixel, WHITE);

		drawButton(state, s.minus, SDL_Color{80, 80, 80, 255});
		// Was 6.0f -- STEP_BTN_W has room for a bigger, bolder +/- at the
		// new font's size.
		float symPixel = 8.0f;
		DigitFont::drawText(state, "-", s.minus.x + (STEP_BTN_W - 5 * symPixel) / 2.0f, s.minus.y + (STEP_BTN_H - 5 * symPixel) / 2.0f, symPixel, WHITE);

		drawButton(state, s.value, valueColor);
		std::string text = std::to_string(value);
		// Was shrunk all the way to 4.0f -- typical 2-4 digit values fit
		// fine back at 6.0f; auto-shrink is still here as a safety net for
		// the rare much larger value (a big calculator total).
		float pixel = 6.0f;
		float maxW = s.value.w - 8.0f;
		float w = DigitFont::textWidth(text, pixel);
		if(w > maxW && w > 0.0f)
			pixel *= maxW / w;
		w = DigitFont::textWidth(text, pixel);
		DigitFont::drawText(state, text, s.value.x + (s.value.w - w) / 2.0f, s.value.y + (s.value.h - 5 * pixel) / 2.0f, pixel, WHITE);

		drawButton(state, s.plus, SDL_Color{80, 80, 80, 255});
		DigitFont::drawText(state, "+", s.plus.x + (STEP_BTN_W - 5 * symPixel) / 2.0f, s.plus.y + (STEP_BTN_H - 5 * symPixel) / 2.0f, symPixel, WHITE);
	}

	void drawStepper(SDLState& state, const Stepper& s, int value){
		drawStepper(state, s, "", value);
	}

	void drawPlayerRow(SDLState& state, int i){
		float top = playerRowTop(i);
		PlayerConfig& cfg = playerConfigs[i];

		DigitFont::drawText(state, "P" + std::to_string(i + 1), colX(0), top, 6.0f, WHITE);

		// BANK shows the calculated total (and turns green) once the
		// calculator's driving it -- bankroll *is* the total in that mode,
		// so there's no separate number worth showing on its own anymore.
		bool sideBetApplies = hasAnySideBet(gameMode);
		SDL_Color bankColor = cfg.useCalculator ? SDL_Color{40, 140, 60, 255} : SDL_Color{10, 40, 20, 230};
		drawStepper(state, bankStepper(i), "", cfg.effectiveBankroll(sideBetApplies), bankColor);
		drawStepper(state, betStepper(i), cfg.minBet);

		SDL_FRect calc = calcToggle(i);
		drawButton(state, calc, cfg.useCalculator ? SDL_Color{60, 130, 70, 255} : SDL_Color{80, 80, 80, 255});
		std::string calcLabel = cfg.useCalculator ? "CALC ON" : "CALC OFF";
		float calcPixel = 7.0f;
		float calcW = DigitFont::textWidth(calcLabel, calcPixel);
		DigitFont::drawText(state, calcLabel, calc.x + (calc.w - calcW) / 2.0f, calc.y + (calc.h - 5 * calcPixel) / 2.0f, calcPixel, WHITE);

		drawStepper(state, cntStepper(i), cfg.minBetCount);

		// Player's Edge has two side bets, but they always start equal (see
		// Person::setInitialMatchBets()), so one stepper covers both here.
		// A distinct purple value box (not the neutral dark-green every
		// other stepper uses) so it doesn't read as just another BANK/BET/
		// CNT field -- it's a different kind of number.
		if(sideBetApplies)
			drawStepper(state, sideBetStepper(i), "", cfg.sideBetSize, SDL_Color{90, 50, 110, 230});
	}

	void handlePlayerRowPoint(int i, const SDL_FPoint& p){
		PlayerConfig& cfg = playerConfigs[i];

		// BANK's +/- only matter when the calculator's off -- while it's
		// on, the displayed number is the calculated total (see
		// drawPlayerRow), and editing the stored bankroll behind it
		// wouldn't visibly do anything, which would just be confusing.
		if(!cfg.useCalculator){
			Stepper bank = bankStepper(i);
			if(SDL_PointInRectFloat(&p, &bank.minus))
				cfg.bankroll = std::max(50, cfg.bankroll - 50);
			else if(SDL_PointInRectFloat(&p, &bank.plus))
				cfg.bankroll = std::min(5000, cfg.bankroll + 50);
		}

		Stepper bet = betStepper(i);
		if(SDL_PointInRectFloat(&p, &bet.minus))
			cfg.minBet = std::max(5, cfg.minBet - 5);
		else if(SDL_PointInRectFloat(&p, &bet.plus))
			cfg.minBet = std::min(500, cfg.minBet + 5);

		SDL_FRect calc = calcToggle(i);
		if(SDL_PointInRectFloat(&p, &calc))
			cfg.useCalculator = !cfg.useCalculator;

		Stepper cnt = cntStepper(i);
		if(SDL_PointInRectFloat(&p, &cnt.minus))
			cfg.minBetCount = std::max(1, cfg.minBetCount - 1);
		else if(SDL_PointInRectFloat(&p, &cnt.plus))
			cfg.minBetCount = std::min(200, cfg.minBetCount + 1);

		if(hasAnySideBet(gameMode)){
			Stepper sideBet = sideBetStepper(i);
			if(SDL_PointInRectFloat(&p, &sideBet.minus))
				cfg.sideBetSize = std::max(0, cfg.sideBetSize - 5);
			else if(SDL_PointInRectFloat(&p, &sideBet.plus))
				cfg.sideBetSize = std::min(500, cfg.sideBetSize + 5);
		}
	}
};
