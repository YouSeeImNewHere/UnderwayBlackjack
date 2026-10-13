#pragma once
#include "UI.h"
#include "TrainingMenus.h"
#include <cmath>
#include <ctime>
#include <random>
#include <string>
#include <vector>

// The counting screens reached from TRAINING: BET RAMP (the ramp written
// out, at your unit, with what counting is worth at each kind of table),
// the BET RAMP DRILL (a count shows, pick the bet) and the SESSION LOG
// (real casino sessions against what the planner expected).

namespace CountingUI{
	inline const int UNITS[10] = { 5, 10, 15, 25, 50, 75, 100, 200, 300, 500 };

	inline std::string money(double v){
		long r = std::lround(std::fabs(v));
		return std::string(v < -0.5 ? "-$" : "$") + std::to_string(r);
	}
	inline std::string plusMoney(double v){ return v >= 0.5 ? "+" + money(v) : money(v); }

	// The counting games and their measured results per round (HouseEdge).
	struct CountGame{ const char* name; HouseEdge::Counting count; };
	inline const std::vector<CountGame>& countGames(){
		using namespace HouseEdge;
		static const std::vector<CountGame> G = {
			{ "6 DECKS H17", COUNT_6D_H17 },
			{ "2 DECKS H17", COUNT_2D_H17 },
			{ "8 DECKS H17", COUNT_8D_H17 },
			{ "6 DECKS S17", COUNT_6D_S17 },
			{ "2 DECKS S17", COUNT_2D_S17 },
			{ "8 DECKS S17", COUNT_8D_S17 },
		};
		return G;
	}

	// Units to start with so the chance of ever going broke is `risk`
	// (lifetime risk of ruin, -ln(risk) * variance / (2 * mean)); 0 when
	// the game doesn't beat the house.
	inline double lifetimeBankrollUnits(const HouseEdge::Counting& c, double risk){
		if(c.win <= 0.0f)
			return 0.0;
		return -std::log(risk) * c.sd * c.sd / (2.0 * c.win);
	}
}

// ---------------------------------------------------------------------
// BET RAMP: true count -> units -> dollars, for the unit chosen here, and
// what that's worth at each table.
class RampMenu
{
public:
	void open(int startingBet){
		unitIndex = 3;
		for(int i = 0; i < 10; i++)
			if(CountingUI::UNITS[i] == startingBet)
				unitIndex = i;
	}

	void draw(SDLState& state){
		using namespace TrainingUI;
		clear(state);
		text(state, "BET RAMP", 18.0f, 6.0f, WHITE);
		text(state, "HI-LO TRUE COUNT = RUNNING COUNT / DECKS LEFT, ROUNDED DOWN.  THE COUNT BEFORE THE DEAL SETS THE BET", 62.0f, 3.0f, DIM);

		int unit = CountingUI::UNITS[unitIndex];
		button(state, minusButton, UI::SECONDARY, "-", 5.0f);
		button(state, plusButton, UI::SECONDARY, "+", 5.0f);
		DigitFont::drawText(state, "1 UNIT", 60.0f, 108.0f, 4.0f, GOLD);
		text(state, "$" + std::to_string(unit), 108.0f, 5.0f, UI::ACCENT, 335.0f);

		// The ramp, large.
		static const char* COUNTS[5] = { "+1 OR LESS", "+2", "+3", "+4", "+5 OR MORE" };
		static const int TC[5] = { 1, 2, 3, 4, 5 };
		DigitFont::drawText(state, "TRUE COUNT", 60.0f, 170.0f, 3.0f, DIM);
		DigitFont::drawText(state, "UNITS", 390.0f, 170.0f, 3.0f, DIM);
		DigitFont::drawText(state, "BET", 500.0f, 170.0f, 3.0f, DIM);
		for(int i = 0; i < 5; i++){
			float y = 205.0f + i * 54.0f;
			SDL_FRect band{ 50.0f, y - 8.0f, 600.0f, 46.0f };
			SDL_SetRenderDrawColor(state.renderer, i % 2 ? 20 : 28, i % 2 ? 45 : 58, i % 2 ? 26 : 34, 255);
			SDL_RenderFillRect(state.renderer, &band);
			int u = IndexPlays::betUnits(TC[i]);
			DigitFont::drawText(state, COUNTS[i], 60.0f, y, 5.0f, WHITE);
			DigitFont::drawText(state, std::to_string(u), 410.0f, y, 5.0f, UI::ACCENT);
			DigitFont::drawText(state, "$" + std::to_string(u * unit), 500.0f, y, 4.0f, GREEN);
		}
		static const char* TIPS[5] = {
			"SPREAD 1-8: TOP BET IS 8 UNITS.",
			"1 UNIT = YOUR STARTING BET.",
			"NEGATIVE COUNT: 1 UNIT OR SIT OUT.",
			"INSURANCE ONLY AT +3 OR MORE.",
			"PRACTICE: DRILLS > BET RAMP DRILL",
		};
		for(int i = 0; i < 5; i++)
			DigitFont::drawText(state, TIPS[i], 60.0f, 490.0f + i * 26.0f, 3.0f, GOLD);

		// What it's worth, per table kind, at this unit.
		DigitFont::drawText(state, "AT 70 HANDS AN HOUR, 75% PENETRATION", 690.0f, 104.0f, 3.0f, DIM);
		DigitFont::drawText(state, "TABLE", 690.0f, 140.0f, 3.0f, DIM);
		DigitFont::drawText(state, "EDGE", 920.0f, 140.0f, 3.0f, DIM);
		DigitFont::drawText(state, "PER HOUR", 1060.0f, 140.0f, 3.0f, DIM);
		DigitFont::drawText(state, "BANKROLL", 1250.0f, 140.0f, 3.0f, DIM);
		const auto& games = CountingUI::countGames();
		for(size_t i = 0; i < games.size(); i++){
			const HouseEdge::Counting& c = games[i].count;
			float y = 170.0f + i * 40.0f;
			double edge = 100.0 * c.win / c.avgBet;
			char e[16];
			std::snprintf(e, sizeof(e), "%+.2f%%", edge);
			SDL_Color col = c.win > 0.0f ? GREEN : RED;
			DigitFont::drawText(state, games[i].name, 690.0f, y, 3.0f, WHITE);
			DigitFont::drawText(state, e, 920.0f, y, 3.0f, col);
			DigitFont::drawText(state, CountingUI::plusMoney(c.win * unit * 70.0), 1060.0f, y, 3.0f, col);
			double units = CountingUI::lifetimeBankrollUnits(c, 0.05);
			DigitFont::drawText(state, units > 0.0 ? CountingUI::money(units * unit) : std::string("-"), 1250.0f, y, 3.0f, WHITE);
		}
		DigitFont::drawText(state, "BANKROLL: 95% SAFE FROM GOING BROKE", 690.0f, 420.0f, 3.0f, DIM);
		drawTuning(state, unit);

		button(state, backButton, UI::SECONDARY, "BACK", 6.0f);
	}

	bool handlePoint(SDLState& state, float wx, float wy){
		SDL_FPoint p;
		if(!TrainingUI::toLogical(state, wx, wy, p))
			return false;
		if(TrainingUI::hit(p, backButton))
			return true;
		if(TrainingUI::hit(p, minusButton)) unitIndex = std::max(0, unitIndex - 1);
		if(TrainingUI::hit(p, plusButton)) unitIndex = std::min(9, unitIndex + 1);
		return false;
	}

	std::vector<SDL_FRect> focusRects(){ return { minusButton, plusButton, backButton }; }

private:
	int unitIndex = 3;
	SDL_FRect minusButton{ .x = 220, .y = 96, .w = 50, .h = 44 };
	SDL_FRect plusButton{ .x = 400, .y = 96, .w = 50, .h = 44 };
	SDL_FRect backButton{ .x = 620, .y = 640, .w = 200, .h = 56 };

	// What changes the most at a 6-deck H17 table (TUNING_* below).
	void drawTuning(SDLState& state, int unit);
};

// Measured for 6 decks, dealer hits soft 17 (tune_sim: a counter beside
// a flat-betting player, 20 million rounds each): win per round seen, in
// units, and the share of rounds played. "Sit out" leaves at a true count
// of -1 or less and comes back at 0.
struct Tuning{ const char* name; float win; float played; };
inline const std::vector<Tuning>& tuning6DH17(){
	static const std::vector<Tuning> T = {
		{ "1-8, EVERY HAND, 75% CUT", 0.0042f, 1.000f },
		{ "1-8, SHALLOW 65% CUT", 0.0006f, 1.000f },
		{ "1-12 SPREAD", 0.0077f, 1.000f },
		{ "1-8, SIT OUT AT -1 OR LESS", 0.0097f, 0.596f },
		{ "1-8, DEEP 85% CUT", 0.0114f, 1.000f },
		{ "1-12 AND SIT OUT", 0.0140f, 0.596f },
		{ "1-12, SIT OUT, 85% CUT", 0.0237f, 0.582f },
	};
	return T;
}

inline void RampMenu::drawTuning(SDLState& state, int unit){
	using namespace TrainingUI;
	// Columns: the setup, what it makes an hour, and how many of the
	// hands it actually plays.
	DigitFont::drawText(state, "6 DECK H17: WHAT HELPS MOST", 690.0f, 450.0f, 3.0f, GOLD);
	DigitFont::drawText(state, "SETUP", 690.0f, 474.0f, 3.0f, DIM);
	DigitFont::drawText(state, "PER HR", 1180.0f, 474.0f, 3.0f, DIM);
	DigitFont::drawText(state, "PLAYED", 1320.0f, 474.0f, 3.0f, DIM);
	const auto& t = tuning6DH17();
	for(size_t i = 0; i < t.size(); i++){
		float y = 496.0f + i * 19.0f;
		char played[16];
		std::snprintf(played, sizeof(played), "%d%%", (int)std::lround(t[i].played * 100.0f));
		DigitFont::drawText(state, t[i].name, 690.0f, y, 3.0f, WHITE);
		DigitFont::drawText(state, CountingUI::plusMoney(t[i].win * unit * 70.0), 1180.0f, y, 3.0f, t[i].win > 0.0f ? GREEN : RED);
		DigitFont::drawText(state, played, 1320.0f, y, 3.0f, DIM);
	}
}

// ---------------------------------------------------------------------
// BET RAMP DRILL: a count shows, tap the bet. Either the true count, or
// (harder) the running count and decks left, to turn into one first.
class RampDrillMenu
{
public:
	bool dirty = false;

	void open(Trainer& t){
		trainer = &t;
		streak = 0;
		newQuestion();
	}

	void update(float dt){
		if(phase == CORRECT){
			timer -= dt;
			if(timer <= 0.0f)
				newQuestion();
		}
	}

	void draw(SDLState& state){
		using namespace TrainingUI;
		clear(state);
		bool tall = portrait;
		if(tall)
			DigitFont::drawText(state, "BET RAMP DRILL", 190.0f, 34.0f, 5.0f, WHITE);
		else
			text(state, "BET RAMP DRILL", 18.0f, 6.0f, WHITE);
		button(state, modeButton(), UI::NAV, fromRunning ? "SHOWS: RUNNING COUNT + DECKS" : "SHOWS: TRUE COUNT", tall ? 4.0f : 3.6f);

		float qy = tall ? 330.0f : 150.0f;
		if(fromRunning){
			text(state, "RUNNING COUNT " + signedNum(running), qy, tall ? 6.0f : 6.0f, UI::ACCENT, tall ? CX : 720.0f, tall ? MAXW : 1400.0f);
			text(state, half(decksHalves) + " DECKS LEFT", qy + (tall ? 80.0f : 60.0f), tall ? 6.0f : 5.0f, UI::ACCENT, tall ? CX : 720.0f, tall ? MAXW : 1400.0f);
		} else{
			text(state, "TRUE COUNT " + signedNum(trueCount), qy + 20.0f, tall ? 9.0f : 8.0f, UI::ACCENT, tall ? CX : 720.0f, tall ? MAXW : 1400.0f);
		}
		text(state, "HOW MANY UNITS?", qy + (tall ? 190.0f : 130.0f), tall ? 4.0f : 3.6f, WHITE, tall ? CX : 720.0f);

		float fy = tall ? 640.0f : 330.0f;
		if(phase == CORRECT){
			text(state, "CORRECT - " + units(answer), fy, tall ? 6.0f : 5.0f, GREEN, tall ? CX : 720.0f);
		} else if(phase == WRONG){
			text(state, "BET " + units(answer), fy, tall ? 6.0f : 5.0f, RED, tall ? CX : 720.0f);
			text(state, why, fy + (tall ? 60.0f : 44.0f), tall ? 3.0f : 3.2f, GOLD, tall ? CX : 720.0f, tall ? MAXW : 1400.0f);
			text(state, tall ? "TAP ANYWHERE FOR THE NEXT ONE" : "TAP OR ENTER FOR THE NEXT ONE", fy + (tall ? 100.0f : 76.0f), 3.0f, DIM, tall ? CX : 720.0f);
		}

		for(int i = 0; i < 5; i++){
			SDL_Color c = phase != ASKING && CHOICES[i] == answer ? UI::PRIMARY
				: phase == WRONG && CHOICES[i] == chosen ? UI::DANGER
				: UI::NAV;
			button(state, choiceRect(i), c, units(CHOICES[i]), tall ? 7.0f : 5.0f);
		}

		std::string score = "STREAK " + std::to_string(streak) + "    RIGHT " + std::to_string(trainer->rampDrillsRight)
			+ " OF " + std::to_string(trainer->rampDrills);
		text(state, score, tall ? 1420.0f - 30.0f : 600.0f, 3.0f, DIM, tall ? CX : 720.0f);
		button(state, backButton(), UI::SECONDARY, "BACK", 6.0f);
	}

	bool handlePoint(SDLState& state, float wx, float wy){
		SDL_FPoint p;
		if(!TrainingUI::toLogical(state, wx, wy, p))
			return false;
		if(TrainingUI::hit(p, backButton()))
			return true;
		if(TrainingUI::hit(p, modeButton())){
			fromRunning = !fromRunning;
			newQuestion();
			return false;
		}
		if(phase == ASKING){
			for(int i = 0; i < 5; i++)
				if(TrainingUI::hit(p, choiceRect(i)))
					choose(CHOICES[i]);
		} else{
			newQuestion();
		}
		return false;
	}

	void handleKey(SDL_Scancode sc){
		if(phase != ASKING){
			if(sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_SPACE || sc == SDL_SCANCODE_KP_ENTER)
				newQuestion();
			return;
		}
		static const SDL_Scancode KEYS[5] = { SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_4, SDL_SCANCODE_6, SDL_SCANCODE_8 };
		for(int i = 0; i < 5; i++)
			if(sc == KEYS[i])
				choose(CHOICES[i]);
	}

	// Number keys answer; arrows and Enter still walk the buttons.
	static bool takesKey(SDL_Scancode sc){
		return sc == SDL_SCANCODE_1 || sc == SDL_SCANCODE_2 || sc == SDL_SCANCODE_4 || sc == SDL_SCANCODE_6 || sc == SDL_SCANCODE_8;
	}
	bool waitingForNext() const{ return phase != ASKING; }

	std::vector<SDL_FRect> focusRects(){
		std::vector<SDL_FRect> r{ modeButton() };
		for(int i = 0; i < 5; i++)
			r.push_back(choiceRect(i));
		r.push_back(backButton());
		return r;
	}

private:
	enum Phase{ ASKING, CORRECT, WRONG };
	static constexpr int CHOICES[5] = { 1, 2, 4, 6, 8 };
	Trainer* trainer = nullptr;
	std::mt19937 rng{ std::random_device{}() };
	Phase phase = ASKING;
	bool fromRunning = false;
	int trueCount = 0, running = 0, decksHalves = 8, answer = 1, chosen = 1, streak = 0;
	float timer = 0.0f;
	std::string why;

	static std::string units(int u){ return std::to_string(u) + (u == 1 ? " UNIT" : " UNITS"); }
	static std::string half(int halves){ return std::to_string(halves / 2) + (halves % 2 ? ".5" : ""); }

	SDL_FRect modeButton(){
		return TrainingUI::portrait ? SDL_FRect{ .x = 20, .y = 130, .w = 680, .h = 90 } : SDL_FRect{ .x = 470, .y = 72, .w = 500, .h = 50 };
	}
	SDL_FRect choiceRect(int i){
		if(TrainingUI::portrait)
			return SDL_FRect{ .x = 20, .y = 780.0f + i * 118.0f, .w = 680, .h = 106 };
		return SDL_FRect{ .x = 200.0f + i * 212.0f, .y = 470, .w = 196, .h = 80 };
	}
	SDL_FRect backButton(){ return TrainingUI::backRect(); }

	int rnd(int n){ return (int)(rng() % (unsigned)n); }

	void newQuestion(){
		phase = ASKING;
		why.clear();
		// Mostly the counts where the bet changes, some negatives.
		trueCount = -3 + rnd(11);
		if(fromRunning){
			decksHalves = 2 + rnd(11);                       // 1 to 6 decks left
			float decks = decksHalves / 2.0f;
			// A running count whose true count, rounded down, is trueCount.
			int lo = (int)std::ceil(trueCount * decks), hi = (int)std::ceil((trueCount + 1) * decks) - 1;
			running = hi >= lo ? lo + rnd(hi - lo + 1) : lo;
			trueCount = IndexPlays::floorCount(running / decks);
		}
		answer = IndexPlays::betUnits(trueCount);
	}

	void choose(int u){
		chosen = u;
		bool ok = u == answer;
		trainer->rampDrills++;
		if(ok){
			trainer->rampDrillsRight++;
			streak++;
			phase = CORRECT;
			timer = 0.7f;
		} else{
			streak = 0;
			phase = WRONG;
			why = fromRunning
				? TrainingUI::signedNum(running) + " OVER " + half(decksHalves) + " DECKS IS TRUE COUNT " + TrainingUI::signedNum(trueCount) + ": " + units(answer)
				: "TRUE COUNT " + TrainingUI::signedNum(trueCount) + ": " + units(answer) + " - SEE TRAINING, BET RAMP";
			misses++;
		}
		dirty = true;
	}

public:
	int misses = 0; // for mina.cpp's buzz on a wrong answer
};

// ---------------------------------------------------------------------
// SESSION LOG: each real session (date, game, hours, unit, result), the
// totals, and what the planner expected for the same play.
class SessionLogMenu
{
public:
	bool dirty = false;

	struct Game{ const char* name; bool counting; HouseEdge::Counting count; float basicEdge; float basicSd; };
	static const std::vector<Game>& games(){
		using namespace HouseEdge;
		static const std::vector<Game> G = {
			{ "6D H17 COUNTING", true, COUNT_6D_H17, 0, 0 },
			{ "2D H17 COUNTING", true, COUNT_2D_H17, 0, 0 },
			{ "8D H17 COUNTING", true, COUNT_8D_H17, 0, 0 },
			{ "6D H17 BASIC", false, {}, EDGE_6D_H17, 1.15f },
			{ "2D H17 BASIC", false, {}, EDGE_2D_H17, 1.15f },
			{ "PLAYER'S EDGE", false, {}, EDGE_PE6_H17, 1.16f },
			{ "FREE BET", false, {}, EDGE_FREE_BET, 1.07f },
		};
		return G;
	}

	void open(Trainer& t, int startingBet){
		trainer = &t;
		date = today();
		unitIndex = 3;
		for(int i = 0; i < 10; i++)
			if(CountingUI::UNITS[i] == startingBet)
				unitIndex = i;
		confirmDelete = false;
	}

	void draw(SDLState& state){
		using namespace TrainingUI;
		clear(state);
		text(state, "SESSION LOG", 16.0f, 6.0f, WHITE);
		text(state, "YOUR REAL CASINO SESSIONS, AND WHAT THE PLANNER EXPECTED FOR THE SAME PLAY", 56.0f, 3.0f, DIM);

		// The new session.
		stepper(state, 0, "DATE", dateText(date));
		button(state, gameButton, UI::NAV, std::string("GAME: ") + games()[game].name, 4.0f);
		stepper(state, 1, "HOURS", std::to_string(tenths / 10) + "." + std::to_string(tenths % 10));
		stepper(state, 2, "1 UNIT", "$" + std::to_string(CountingUI::UNITS[unitIndex]));
		DigitFont::drawText(state, "RESULT", 40.0f, 215.0f, 4.0f, WHITE);
		static const char* STEP[6] = { "-100", "-25", "-5", "+5", "+25", "+100" };
		for(int i = 0; i < 6; i++)
			button(state, resultRect(i), UI::SECONDARY, STEP[i], 3.0f);
		text(state, CountingUI::plusMoney(result), 216.0f, 5.0f, result >= 0 ? GREEN : RED, 720.0f);
		button(state, saveButton, UI::PRIMARY, "SAVE SESSION", 4.0f);

		// The log, newest first.
		const std::vector<Trainer::Session>& log = trainer->sessions;
		static const float X[5] = { 40.0f, 260.0f, 640.0f, 790.0f, 940.0f };
		static const char* HEAD[5] = { "DATE", "GAME", "HOURS", "UNIT", "RESULT" };
		for(int i = 0; i < 5; i++)
			DigitFont::drawText(state, HEAD[i], X[i], 300.0f, 3.0f, GOLD);
		int shown = 0;
		for(int i = (int)log.size() - 1; i >= 0 && shown < 8; i--, shown++){
			const Trainer::Session& x = log[i];
			float y = 326.0f + shown * 26.0f;
			DigitFont::drawText(state, dateText(x.date), X[0], y, 3.0f, WHITE);
			DigitFont::drawText(state, games()[std::clamp(x.game, 0, (int)games().size() - 1)].name, X[1], y, 3.0f, WHITE);
			DigitFont::drawText(state, std::to_string(x.tenthsOfHour / 10) + "." + std::to_string(x.tenthsOfHour % 10), X[2], y, 3.0f, WHITE);
			DigitFont::drawText(state, "$" + std::to_string(x.unit), X[3], y, 3.0f, WHITE);
			DigitFont::drawText(state, CountingUI::plusMoney(x.result), X[4], y, 3.0f, x.result >= 0 ? GREEN : RED);
		}
		if(log.empty())
			text(state, "NO SESSIONS YET - FILL IN THE ROW ABOVE AFTER A REAL SESSION AND SAVE IT", 360.0f, 3.0f, DIM);

		// Totals against the model.
		double hours = 0.0, net = 0.0, expected = 0.0, var = 0.0;
		for(const Trainer::Session& x : log){
			double h = x.tenthsOfHour / 10.0, hands = h * 70.0;
			const Game& g = games()[std::clamp(x.game, 0, (int)games().size() - 1)];
			hours += h;
			net += x.result;
			if(g.counting){
				expected += g.count.win * x.unit * hands;
				var += g.count.sd * g.count.sd * x.unit * x.unit * hands;
			} else{
				expected += -g.basicEdge * x.unit * hands;
				var += g.basicSd * g.basicSd * x.unit * x.unit * hands;
			}
		}
		if(!log.empty()){
			char perHour[32];
			std::snprintf(perHour, sizeof(perHour), "%s/HR", CountingUI::plusMoney(hours > 0 ? net / hours : 0).c_str());
			std::string line = std::to_string(log.size()) + " SESSIONS, " + std::to_string((int)std::lround(hours)) + " HOURS:  "
				+ CountingUI::plusMoney(net) + "  -  " + perHour;
			text(state, line, 545.0f, 3.0f, net >= 0 ? GREEN : RED);
			double sd = std::sqrt(var);
			double z = sd > 0 ? (net - expected) / sd : 0.0;
			std::string luck = std::fabs(z) < 1.0 ? "WITHIN NORMAL LUCK" : z > 0 ? "LUCKIER THAN USUAL" : "UNLUCKIER THAN USUAL";
			text(state, "EXPECTED " + CountingUI::plusMoney(expected) + ",  USUAL SWING +-" + CountingUI::money(sd) + "  -  " + luck,
				585.0f, 3.0f, GOLD);
		}

		button(state, backButton, UI::SECONDARY, "BACK", 6.0f);
		if(!log.empty())
			button(state, deleteButton, confirmDelete ? UI::DANGER : UI::SECONDARY,
				confirmDelete ? "TAP AGAIN TO DELETE" : "DELETE NEWEST", 4.0f);
	}

	bool handlePoint(SDLState& state, float wx, float wy){
		SDL_FPoint p;
		if(!TrainingUI::toLogical(state, wx, wy, p))
			return false;
		if(TrainingUI::hit(p, backButton))
			return true;
		bool wasConfirming = confirmDelete;
		confirmDelete = false;
		if(TrainingUI::hit(p, gameButton))
			game = (game + 1) % (int)games().size();
		for(int i = 0; i < 3; i++){
			if(TrainingUI::hit(p, minusRect(i))) step(i, -1);
			if(TrainingUI::hit(p, plusRect(i))) step(i, 1);
		}
		static const int STEP[6] = { -100, -25, -5, 5, 25, 100 };
		for(int i = 0; i < 6; i++)
			if(TrainingUI::hit(p, resultRect(i)))
				result += STEP[i];
		if(TrainingUI::hit(p, saveButton) && tenths > 0){
			Trainer::Session x;
			x.date = date; x.game = game; x.tenthsOfHour = tenths; x.unit = CountingUI::UNITS[unitIndex]; x.result = result;
			trainer->sessions.push_back(x);
			if((int)trainer->sessions.size() > Trainer::MAX_SESSIONS)
				trainer->sessions.erase(trainer->sessions.begin());
			result = 0;
			dirty = true;
		}
		if(!trainer->sessions.empty() && TrainingUI::hit(p, deleteButton)){
			if(wasConfirming){
				trainer->sessions.pop_back();
				dirty = true;
			} else{
				confirmDelete = true;
			}
		}
		return false;
	}

	std::vector<SDL_FRect> focusRects(){
		std::vector<SDL_FRect> r{ gameButton };
		for(int i = 0; i < 3; i++){
			r.push_back(minusRect(i));
			r.push_back(plusRect(i));
		}
		for(int i = 0; i < 6; i++)
			r.push_back(resultRect(i));
		r.push_back(saveButton);
		r.push_back(backButton);
		if(!trainer->sessions.empty())
			r.push_back(deleteButton);
		return r;
	}

private:
	Trainer* trainer = nullptr;
	int date = 20260101, game = 0, tenths = 30, unitIndex = 3, result = 0;
	bool confirmDelete = false;

	SDL_FRect gameButton{ .x = 760, .y = 84, .w = 640, .h = 46 };
	SDL_FRect saveButton{ .x = 1135, .y = 204, .w = 265, .h = 50 };
	SDL_FRect backButton{ .x = 510, .y = 640, .w = 200, .h = 56 };
	SDL_FRect deleteButton{ .x = 730, .y = 640, .w = 300, .h = 56 };

	// DATE, HOURS, 1 UNIT: rows down the left, then HOURS and 1 UNIT on the right.
	SDL_FRect minusRect(int i){
		static const float X[3] = { 200.0f, 960.0f, 200.0f }, Y[3] = { 84.0f, 144.0f, 144.0f };
		return SDL_FRect{ .x = X[i], .y = Y[i], .w = 50, .h = 46 };
	}
	SDL_FRect plusRect(int i){ SDL_FRect m = minusRect(i); m.x += 290.0f; return m; }
	SDL_FRect resultRect(int i){
		float x = i < 3 ? 200.0f + i * 100.0f : 820.0f + (i - 3) * 100.0f;
		return SDL_FRect{ .x = x, .y = 204, .w = 90, .h = 50 };
	}

	void stepper(SDLState& state, int i, const std::string& label, const std::string& value){
		using namespace TrainingUI;
		SDL_FRect m = minusRect(i), pl = plusRect(i);
		DigitFont::drawText(state, label, m.x - 160.0f, m.y + 12.0f, 4.0f, WHITE);
		button(state, m, UI::SECONDARY, "-", 4.0f);
		button(state, pl, UI::SECONDARY, "+", 4.0f);
		text(state, value, m.y + 10.0f, 4.0f, UI::ACCENT, (m.x + m.w + pl.x) / 2.0f, 230.0f);
	}

	void step(int i, int d){
		if(i == 0) date = addDays(date, d);
		else if(i == 1) tenths = std::clamp(tenths + 5 * d, 5, 240);
		else unitIndex = std::clamp(unitIndex + d, 0, 9);
	}

	// Dates as YYYYMMDD; stepping by days through a day count.
	static int today(){
		SDL_Time now = 0;
		SDL_DateTime dt{};
		if(SDL_GetCurrentTime(&now) && SDL_TimeToDateTime(now, &dt, true))
			return dt.year * 10000 + dt.month * 100 + dt.day;
		return 20260101;
	}
	static long daysFromCivil(int y, int m, int d){
		y -= m <= 2;
		long era = (y >= 0 ? y : y - 399) / 400;
		long yoe = y - era * 400;
		long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
		long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
		return era * 146097 + doe - 719468;
	}
	static int civilFromDays(long z){
		z += 719468;
		long era = (z >= 0 ? z : z - 146096) / 146097;
		long doe = z - era * 146097;
		long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
		long y = yoe + era * 400;
		long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
		long mp = (5 * doy + 2) / 153;
		long d = doy - (153 * mp + 2) / 5 + 1;
		long m = mp + (mp < 10 ? 3 : -9);
		y += m <= 2;
		return (int)(y * 10000 + m * 100 + d);
	}
	static int addDays(int ymd, int n){
		return civilFromDays(daysFromCivil(ymd / 10000, (ymd / 100) % 100, ymd % 100) + n);
	}
	static std::string dateText(int ymd){
		static const char* MONTHS[12] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
		int m = std::clamp((ymd / 100) % 100, 1, 12);
		return std::string(MONTHS[m - 1]) + " " + std::to_string(ymd % 100) + " " + std::to_string(ymd / 10000);
	}
};
