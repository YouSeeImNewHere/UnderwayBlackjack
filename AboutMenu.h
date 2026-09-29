#pragma once
#include "Game.h"
#include "DigitFont.h"
#include "GameModeMenu.h"
#include <string>
#include <vector>

// Read-only rules/payouts reference, opened from PauseMenu's ABOUT button --
// same structural shape as StrategyChart.h (draw()/handlePoint() returning
// a bool for BACK) so mina.cpp's routing is a straight copy of the existing
// PauseState::Strategy plumbing. draw() takes the GameMode actually being
// played (Table::getGameMode()) and shows that mode's own rules -- each
// mode plays differently enough (Player's Edge is a different rules engine
// entirely, not standard blackjack plus a few tweaks) that one generic
// rules list would either be wrong or too vague to be useful.
class AboutMenu
{
public:
	void draw(SDLState& state, Resources& res, GameMode mode){
		SDL_SetRenderDrawColor(state.renderer, 10, 30, 15, 255);
		SDL_RenderFillRect(state.renderer, nullptr);

		float titlePixel = 7.0f;
		std::string title = titleFor(mode);
		float titleW = DigitFont::textWidth(title, titlePixel);
		DigitFont::drawText(state, title, (1440.0f - titleW) / 2.0f, 40.0f, titlePixel, SDL_Color{255, 255, 255, 255});

		const std::vector<std::string>& rules = rulesFor(mode);
		const std::vector<std::string>& sideBets = sideBetsFor(mode);

		// Two columns (rules | side-bet payouts) when this mode actually
		// has a side bet worth a second column; otherwise just the rules,
		// centered, same as the very first version of this screen.
		if(sideBets.empty()){
			drawColumn(state, rules, "RULES", (1440.0f - COLUMN_W) / 2.0f);
		} else{
			float gap = 60.0f;
			float leftX = (1440.0f - (2.0f * COLUMN_W + gap)) / 2.0f;
			drawColumn(state, rules, "RULES", leftX);
			drawColumn(state, sideBets, "SIDE BET PAYOUTS", leftX + COLUMN_W + gap);
		}

		drawButton(state, backButton, SDL_Color{80, 80, 80, 255}, "BACK");
	}

	// windowX/windowY: raw event coordinates in window space, same
	// convention as Menu/SetupMenu's handlePoint. Returns true once BACK is
	// hit -- caller (mina.cpp) is responsible for what "back" means.
	bool handlePoint(SDLState& state, float windowX, float windowY){
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			return false;

		SDL_FPoint p{x, y};
		return SDL_PointInRectFloat(&p, &backButton);
	}

	std::vector<SDL_FRect> focusRects(){
		return { backButton };
	}

private:
	static constexpr float COLUMN_W = 620.0f;
	// Was 4.0f -- DigitFont's 5-wide glyphs run ~50% wider per character
	// than the font's old 3-wide ones, and the longest rule line here (39
	// chars) would overflow COLUMN_W at the old size. 2.5 keeps every line
	// (checked the longest ones by hand) comfortably inside the column.
	static constexpr float BODY_PIXEL = 2.5f;
	static constexpr float LINE_H = 5.0f * BODY_PIXEL + 10.0f;
	static constexpr float COLUMN_TOP = 130.0f;

	SDL_FRect backButton{ .x = 620, .y = 650, .w = 200, .h = 56 };

	std::string titleFor(GameMode mode){
		if(hasLuckyLadies(mode)) return "LUCKY LADIES RULES";
		if(isPlayersEdge(mode)) return "PLAYERS EDGE RULES";
		if(hasLuckyStiff(mode)) return "LUCKY STIFF RULES";
		if(isFreeBet(mode)) return "FREE BET BLACKJACK RULES";
		return "GAME RULES";
	}

	// Standard blackjack rules -- this engine's actual behavior, not
	// aspirational text (e.g. surrender here forfeits the whole bet, not
	// the standard casino half-back).
	const std::vector<std::string>& standardRules(){
		static const std::vector<std::string> lines{
			"STANDARD BLACKJACK, NO SIDE BETS",
			"2 OR 6 DECKS, DEALER HITS SOFT 17",
			"BLACKJACK PAYS 3:2",
			"DOUBLE ON ANY 2 CARDS, EVEN AFTER SPLIT",
			"SPLIT PAIRS, SPLIT AGAIN ALLOWED",
			"A 21 STANDS AT ONCE, PAID OUT WHEN",
			" THE DEALER FINISHES",
			"SURRENDER FORFEITS THE BET",
			"WIN PAYS EVEN MONEY",
			"PUSH RETURNS YOUR BET"
		};
		return lines;
	}

	// Spanish 21 rules -- deliberately its own list, not "standard rules
	// plus a few notes": several of these directly contradict standard
	// blackjack (a player 21 never pushes, redoubling is allowed at all).
	const std::vector<std::string>& playersEdgeRules(){
		static const std::vector<std::string> lines{
			"PLAYERS EDGE, A SPANISH 21 VARIANT",
			"48 CARD SPANISH DECK, NO 10S IN PLAY",
			"A PLAYER 21 ALWAYS WINS AT ONCE",
			" EVEN AGAINST A DEALER 21, NO PUSH",
			"5 CARD 21 PAYS 3:2",
			"6 CARD 21 PAYS 2:1",
			"7 OR MORE CARD 21 PAYS 3:1",
			"678 OR 777 PAYS 3:2 MIXED SUITS",
			" 2:1 SAME SUIT, 3:1 ALL SPADES",
			"DOUBLE ON ANY 2 CARDS, EVEN AFTER SPLIT",
			"DOUBLE, DOUBLE, DOUBLE AGAIN",
			" REDOUBLE AS MANY TIMES AS YOU LIKE",
			"SPLIT PAIRS, SPLIT AGAIN ALLOWED",
			"SURRENDER FORFEITS THE BET"
		};
		return lines;
	}

	// Lucky Stiff's main hand plays exactly like standard blackjack --
	// only the deck count differs (always 8) and there's a side bet (see
	// sideBetsFor()) -- so this is standardRules() with just that one line
	// swapped, not a from-scratch list like Player's Edge needed.
	const std::vector<std::string>& luckyStiffRules(){
		static const std::vector<std::string> lines{
			"STANDARD BLACKJACK, PLUS A SIDE BET",
			"8 DECKS, DEALER HITS SOFT 17",
			"BLACKJACK PAYS 3:2",
			"DOUBLE ON ANY 2 CARDS, EVEN AFTER SPLIT",
			"SPLIT PAIRS, SPLIT AGAIN ALLOWED",
			"A 21 STANDS AT ONCE, PAID OUT WHEN",
			" THE DEALER FINISHES",
			"SURRENDER FORFEITS THE BET",
			"WIN PAYS EVEN MONEY",
			"PUSH RETURNS YOUR BET"
		};
		return lines;
	}

	// Verified against wizardofodds.com's Free Bet Blackjack page rather
	// than guessed -- its own rules list, not standard-rules-plus-notes,
	// since the free double/split conditions and Push 22 are specific
	// enough to spell out in full.
	const std::vector<std::string>& freeBetRules(){
		static const std::vector<std::string> lines{
			"6 DECKS, DEALER HITS SOFT 17",
			"BLACKJACK PAYS 3:2",
			"DOUBLE YOUR INITIAL HARD 9, 10 OR 11",
			" FOR FREE, NO EXTRA MONEY DOWN",
			"SPLIT ANY PAIR EXCEPT 10S FOR FREE",
			" SPLIT AGAIN UP TO 4 HANDS, ACES TOO",
			"OTHER DOUBLES AND SPLITS OF 10S",
			" STILL COST THE USUAL EXTRA BET",
			"DEALER BUST OF 22 EXACTLY PUSHES",
			" ALL YOUR HANDS OF 21 OR LESS",
			"A BUST OF 23 OR MORE STILL PAYS",
			"YOUR BLACKJACK ALWAYS PAYS 3:2",
			" NEVER AFFECTED BY THE PUSH ON 22",
			"SURRENDER FORFEITS THE BET"
		};
		return lines;
	}

	const std::vector<std::string>& emptyLines(){
		static const std::vector<std::string> lines;
		return lines;
	}

	const std::vector<std::string>& rulesFor(GameMode mode){
		if(isPlayersEdge(mode))
			return playersEdgeRules();
		if(hasLuckyStiff(mode))
			return luckyStiffRules();
		if(isFreeBet(mode))
			return freeBetRules();
		return standardRules();
	}

	const std::vector<std::string>& sideBetsFor(GameMode mode){
		if(hasLuckyLadies(mode)){
			static const std::vector<std::string> lines{
				"LUCKY LADIES, PAYS ON YOUR OWN",
				" FIRST 2 CARDS",
				"",
				"SAME SUIT PAIR OF 10, J, Q OR K",
				" PAYS 200:1",
				"",
				"ANY OTHER SUITED 20 PAYS 25:1",
				"",
				"ANY OTHER 20 PAYS 10:1"
			};
			return lines;
		}

		if(isPlayersEdge(mode)){
			static const std::vector<std::string> lines{
				"MATCH UP PAYS ON YOUR CARDS",
				" MATCHING THE DEALERS UP CARD",
				"",
				"MATCH DOWN PAYS ON YOUR CARDS",
				" MATCHING THE DEALERS HOLE CARD",
				" REVEALED AT THE END OF THE ROUND",
				"",
				"2 SUITED MATCHES PAYS 20:1",
				"1 SUITED PLUS 1 PLAIN PAYS 14:1",
				"1 SUITED MATCH PAYS 11:1",
				"2 PLAIN MATCHES PAYS 7:1",
				"1 PLAIN MATCH PAYS 4:1"
			};
			return lines;
		}

		if(hasLuckyStiff(mode)){
			static const std::vector<std::string> lines{
				"LUCKY STIFF, PAYS ON YOUR OWN",
				" FIRST 2 CARDS",
				"",
				"A BLACKJACK PAYS 1:1 AT ONCE",
				"",
				"A PAIR OF 6S, 7S OR 8S",
				" PAYS 10:1 AT ONCE",
				"",
				"ANY OTHER HARD 12 TO 16 STAYS LIVE",
				" PAYS 5:1 IF YOUR HAND BEATS THE",
				" DEALER, PUSHES IF IT TIES",
				"",
				"ANY OTHER HAND LOSES AT ONCE"
			};
			return lines;
		}

		return emptyLines();
	}

	void drawColumn(SDLState& state, const std::vector<std::string>& lines, const std::string& heading, float x){
		float headingPixel = 5.0f;
		DigitFont::drawText(state, heading, x, COLUMN_TOP - headingPixel * 6.0f, headingPixel, SDL_Color{200, 180, 100, 255});

		// A small square bullet to the left of every real line -- rules
		// text used to run together into one solid block; a marker per
		// line (skipped for the "" spacer lines some lists already use to
		// group related rules) breaks it up without needing a bullet
		// glyph DigitFont doesn't have. A line that's really just the
		// wrapped continuation of the rule above it (too long for one
		// line) is marked with a single leading space in the source lists
		// below -- stripped before drawing, and skipped for a bullet of
		// its own, so one rule that spans two lines doesn't wrongly read
		// as two separate ones.
		float bulletSize = 5.0f;
		float bulletGap = 10.0f;
		float textX = x + bulletSize + bulletGap;

		float y = COLUMN_TOP;
		for(const std::string& line : lines){
			bool continuation = !line.empty() && line[0] == ' ';
			std::string display = continuation ? line.substr(1) : line;

			if(!line.empty() && !continuation){
				SDL_FRect bullet{ .x = x, .y = y + (5.0f * BODY_PIXEL - bulletSize) / 2.0f, .w = bulletSize, .h = bulletSize };
				SDL_SetRenderDrawColor(state.renderer, 200, 180, 100, 255);
				SDL_RenderFillRect(state.renderer, &bullet);
			}
			DigitFont::drawText(state, display, textX, y, BODY_PIXEL, SDL_Color{220, 220, 220, 255});
			y += LINE_H;
		}
	}

	void drawButton(SDLState& state, const SDL_FRect& rect, SDL_Color color, const std::string& label){
		SDL_SetRenderDrawColor(state.renderer, color.r, color.g, color.b, color.a);
		SDL_RenderFillRect(state.renderer, &rect);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &rect);

		// Auto-shrinks to fit -- see GameModeMenu.h's drawButton() for why.
		float pixel = 6.0f;
		float maxW = rect.w - 12.0f;
		float w = DigitFont::textWidth(label, pixel);
		if(w > maxW && w > 0.0f)
			pixel *= maxW / w;

		w = DigitFont::textWidth(label, pixel);
		DigitFont::drawText(state, label, rect.x + (rect.w - w) / 2.0f, rect.y + (rect.h - 5 * pixel) / 2.0f, pixel, SDL_Color{255, 255, 255, 255});
	}
};
