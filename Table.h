#pragma once
#include <random>
#include <algorithm>
#include <queue>
#include <optional>
#include <unordered_map>
#include <cmath>
#include <functional>

#include "Person.h"
#include "BettingSquare.h"
#include "DigitFont.h"
#include "StrategyChart.h"
#include "GameModeMenu.h"
#include "Stats.h"
#include "Sound.h"

struct CardAnimation {
	Card card;

	SDL_FPoint start;
	SDL_FPoint end;
	SDL_FRect destination;
	float startAngle = 0;
	float currentAngle = 0;
	float finalAngle = 0;

	float elapsed = 0.0f;
	float duration = 0.6f;

	int playerIndex;
	int handIndex;
	bool split;
	bool showCard;
	bool isDealer;
	bool discard;
	bool doubleHand;
};

struct DealRequest{
	int playerIndex;
	int handIndex = 0;
	bool isDealer;
	bool discard = false;
	bool showCard;
	bool split = false;
	bool removeFromHand = false;
	bool doubleHand = false;

	SDL_FPoint from;
	SDL_FPoint to;
	Card card;
};

// Several of these run at once (every hand settles together), unlike
// cards, which are strictly one at a time -- so they have their own
// vector rather than going through dealQueue.
// One bet being settled on the table, the way a dealer does it:
//   win  -- the winnings come from the tray and are set down beside the
//           bet, then bet and winnings together go to the player's
//           bankroll (credited when they arrive);
//   push -- the bet goes back to the bankroll;
//   loss -- the bet is taken to the tray.
// spot is where the bet sits (a hand's chip stack or a side-bet circle).
struct ChipAnimation{
	enum Kind{ Win, Push, Loss };
	Kind kind = Win;
	SDL_FPoint spot;
	SDL_FPoint tray;
	SDL_FPoint bankroll;
	int stake = 0;      // the bet's own chips
	int winnings = 0;   // what the house adds to it (wins only)
	float elapsed = 0.0f;
	int columnIndex;
	int playerIndex;
	int creditAmount;
	// A push's payout is just the player's own bet coming back, not a net
	// gain -- flagged so the bankroll-change readout (see
	// queueBankrollChange()) can say so instead of showing it exactly like
	// a real win, which is misleading (bankroll visibly goes up, so it
	// looks like a gain, even though it's a net wash against what already
	// left the bankroll at deal time).
	bool isPush = false;
};

class Table
{
public:
	Table(int numberOfPlayers,bool H17,int numberOfDecks){
		this->numberOfPlayers = numberOfPlayers;
		this->H17 = H17;
		this->numberOfDecks = numberOfDecks;

		// Seats 0-2 and the dealer (index 5) are the original 3-player
		// layout, positions/rotations unchanged. Seats 3-4 are the two new
		// 5-player seats added along the bottom of Table5Player.png, at
		// their own measured tilt -- see BettingSquare.h for why the
		// direction bucket (last column) and rotation (5th arg) are
		// separate: seat 3 sits on the table's right half so it reuses
		// seat 0's (dir=1) stacking/betting-control math, seat 4 sits on
		// the left half so it reuses seat 2's (dir=-1).
		//
		// Seat 4's rotation was given as 300 but measured directly off
		// Table5Player.png's own seat outline (fit a line through its
		// actual border pixels, same clockwise-from-upright convention as
		// every other seat here): the seat is really tilted 30, not 300 --
		// reads like a stray extra 0. Seat 3 started as seat 4's angle
		// mirrored (rather than its own raw measured tilt, ~70) per direct
		// request, then nudged to taste: -5 to -35, +10 to -25, +5 to -20.
		// The seat's own drawn outline is no longer the reference point for
		// this one -- it's fully covered by the betting row on screen now,
		// so from here it's by-eye tuning against seat 4, not a measurement.
		float xs[6] = {1229,670,211, 978,235, 1058};
		float ys[6] = {391,507,92, 500,400, 423};
		int direction[6] = {1,0,-1, 1,-1, 5};
		float rotationDeg[6] = {-90,0,90, -20,30, 180};
		for(int i = 0; i < 6; i++){
			bettingSquares[i] = BettingSquare(xs[i], ys[i], direction[i], rotationDeg[i]);
		}

		makeShoe();

		dealer = Person(false, bettingSquares[5]);
		for(int i = 0; i < numberOfPlayers; i++){
			players[i] = Person(true, bettingSquares[seatIndexFor(i, numberOfPlayers)]);
		}
	}

	// bettingSquares[0..2] are right/bottom-center/left (see xs[]/ys[] just
	// above), dealt/seated in that index order -- P1 at the right seat,
	// P2 center, P3 left, i.e. right-to-left, clockwise around the table
	// (the dealer deals to their own left first, which is the viewer's
	// right). A single player landing in seat 0 put them off in the
	// far-right seat instead of facing the dealer head-on though, which
	// reads as an odd default for the one-player case specifically --
	// only that case gets remapped; 2 and 3 players keep the existing
	// seats/order.
	//
	// 4-5 players extends the same right-to-left/clockwise sweep across
	// the 2 new seats too: P1 right, P2 new-bottom-right, P3 center, P4
	// new-bottom-left, P5 left -- RIGHT_TO_LEFT walks the whole arc by
	// actual on-screen position so dealing order keeps reading the same
	// way (right to left) it always has, just over 5 seats instead of 3.
	int seatIndexFor(int playerIndex, int totalPlayers){
		if(totalPlayers == 1)
			return 1;
		if(totalPlayers <= 3)
			return playerIndex;

		static const int RIGHT_TO_LEFT[5] = {0, 3, 1, 4, 2};
		return RIGHT_TO_LEFT[playerIndex];
	}

	// Called from mina.cpp once the setup screen (or a loaded save, for
	// Resume) knows the real player count/bankrolls/starting bets -- the
	// constructor above just seeds a default 5-seat table since the actual
	// choice isn't made until after AppContext exists. Re-seats every
	// active player fresh, so this is only meant to run before startGame()
	// deals anything.
	// sideBetSizes is nullptr for modes with no side bet -- callers that
	// don't have one (or don't care) can just omit it. Applied according to
	// whichever game mode configureGameMode() already set, so call that
	// first: Lucky Ladies gets one side bet, Player's Edge gets both of its
	// Match bets seeded to the same starting amount (see
	// Person::setInitialMatchBets()).
	void configurePlayers(int newNumberOfPlayers, const int bankrolls[5], const int initialBets[5], const int* sideBetSizes = nullptr){
		numberOfPlayers = newNumberOfPlayers;
		for(int i = 0; i < numberOfPlayers; i++){
			players[i] = Person(true, bettingSquares[seatIndexFor(i, numberOfPlayers)]);
			players[i].setBankroll(bankrolls[i]);
			players[i].setInitialBet(initialBets[i]);

			if(sideBetSizes){
				if(hasLuckyLadies(gameMode) || hasLuckyStiff(gameMode))
					players[i].setInitialSideBet(sideBetSizes[i]);
				else if(isPlayersEdge(gameMode))
					players[i].setInitialMatchBets(sideBetSizes[i]);
			}
		}
	}

	// Set from GameOptionsMenu's own choices (see mina.cpp), before
	// startGame() -- see each field's own declaration for what it does.
	void setDealerSpeedFactor(float factor){ dealerSpeedFactor = factor; }
	void setFaceDownDoubles(bool value){ faceDownDoubles = value; }
	void setHideInactiveHands(bool value){ hideInactiveHands = value; }

	// Called from mina.cpp once GameModeMenu picks a real mode (or a loaded
	// save, for Resume) -- rebuilds the shoe from scratch at that deck
	// count/composition and remembers the mode for everything that varies
	// by it (side-bet UI, Spanish shoe, eventually Spanish 21 payouts).
	void configureGameMode(GameMode mode){
		StrategyChart::useChartFor(mode);
		gameMode = mode;
		if(stats)
			stats->setGame(mode);
		numberOfDecks = deckCountFor(mode);
		makeShoe();
		runningCount = 0;
	}

	// So screens outside Table (the About screen, mainly) can show the
	// right content for whatever's actually being played, without mina.cpp
	// needing to separately track and keep a second copy in sync (the
	// Resume path only calls configureGameMode() -- there's no other spot
	// that would remember the mode otherwise).
	GameMode getGameMode(){
		return gameMode;
	}

	// Table is constructed up front (see AppContext), but shouldn't start
	// dealing until the player actually picks Start/Resume off the menu --
	// called once, from mina.cpp, on that transition. Rather than dealing
	// immediately, this opens the betting phase (see awaitingBets) so bets
	// get set before the very first round too, not just every one after it.
	void startGame(){
		openBettingPhase();
	}

	// The only two places a betting phase actually opens (the very first
	// one, and every one after a round finishes) -- both route through
	// here instead of setting awaitingBets directly so a bankroll that
	// dropped from the last round's result (or never covered a standing
	// bet to begin with) gets its stale bet clamped down immediately,
	// before the player ever sees or touches the raise/lower controls.
	// Without this, a bet that was affordable when it was set could sit
	// there un-reclamped, visibly exceeding the new bankroll, until the
	// player happened to tap a control themselves.
	void openBettingPhase(){
		for(int i = 0; i < numberOfPlayers; i++)
			clampBetsToBankroll(i);

		awaitingBets = true;
	}

	bool isAwaitingBets(){
		return awaitingBets;
	}

	// For the pause menu's Strategy Table: works out which cell of the
	// basic-strategy chart matches whatever's actually happening right
	// now, so it can highlight it. section: 0=hard totals, 1=soft totals,
	// 2=pairs; row/col are indices into StrategyChart's own data (col is
	// the dealer's up-card: 0->2,...,8->10,9->Ace). Returns false when
	// there's no meaningful "current hand" -- not mid-turn, no cards yet,
	// dealer's hole card not revealed.
	bool getStrategySituation(int& section, int& row, int& col){
		if(awaitingBets || activePlayer >= numberOfPlayers)
			return false;

		Person& p = players[activePlayer];
		int handIdx = p.getActiveHand();
		if(handIdx >= p.hands.size())
			return false;

		Hand& hand = p.hands[handIdx];
		if(hand.getHandSize() < 2)
			return false;
		// A doubled Player's Edge hand still gets a tip -- drawQuickTip()
		// reads the chart through doubledAdvice(), since it can only
		// stand, redouble or rescue from here.

		int dealerUp = -1;
		for(Card& c : dealer.hands[0].cards){
			if(c.getShown()){
				int v = c.getValue();
				dealerUp = (v > 10) ? 10 : v;
				break;
			}
		}
		if(dealerUp < 0)
			return false;

		col = (dealerUp == 1) ? 9 : dealerUp - 2;

		// A pair: exactly 2 cards sharing the same blackjack value-tier
		// (face cards all collapse to 10, matching how onSplit() itself
		// doesn't require identical ranks either).
		if(hand.getHandSize() == 2){
			int v0 = hand.cards[0].getValue();
			int v1 = hand.cards[1].getValue();
			int tier0 = (v0 > 10) ? 10 : v0;
			int tier1 = (v1 > 10) ? 10 : v1;
			if(tier0 == tier1){
				section = 2;
				row = (tier0 == 1) ? 9 : tier0 - 2;
				return true;
			}
		}

		// Every card in a player's own hand is always face-up, so this
		// reads as a plain hard/soft total getter here too.
		auto [hard, soft] = hand.getShownTotals();
		if(soft != hard && soft >= 13 && soft <= 20){
			section = 1;
			row = soft - 13;
		} else{
			section = 0;
			int total = hard;
			if(total <= 8)
				row = 0;
			else if(total >= 17)
				row = 9;
			else
				row = total - 8;
		}

		return true;
	}

	// Top-right-ish, near the active hand HUD -- toggles the in-game quick
	// tip popup. Public (along with the hit-test/toggle below) since
	// mina.cpp needs to claim its own finger the same way it already does
	// for the pause button (see AppContext::uiClaimedFinger), to stop a
	// tap here from also registering as a "hit" gesture.
	// Directly below the pause button (same x/width as mina.cpp's
	// pauseButtonRect()), so the two read as a pair of HUD buttons in the
	// same corner. Both are big on purpose: a tap that misses them lands on
	// the table, and any tap there is a HIT.
	SDL_FRect quickTipButton(){
		return SDL_FRect{ .x = 1352, .y = 98, .w = 82, .h = 84 };
	}

	// The button plus a margin (edges of the screen, a strip toward the
	// table, down to y 204), so a near miss still counts as TIP instead of
	// falling through as a HIT. Starts where the pause button's margin ends
	// (mina.cpp's pauseButtonHitRect()).
	SDL_FRect quickTipHitRect(){
		return SDL_FRect{ .x = 1330, .y = 94, .w = 110, .h = 110 };
	}

	bool isQuickTipButtonHit(SDLState& state, float windowX, float windowY){
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			return false;

		SDL_FPoint p{x, y};
		SDL_FRect hit = quickTipHitRect();
		return SDL_PointInRectFloat(&p, &hit);
	}

	void toggleQuickTip(){
		showQuickTip = !showQuickTip;
		sound(Sfx::Tap);
	}

	// Directly above the discard pile it toggles the overlay on (matching
	// its width, cardWidth), not stacked with the pause/quick-tip buttons
	// -- same claimed-finger handling in mina.cpp regardless of where it
	// sits. Lets the player hide the running count/true count overlay
	// (drawCardCountStats()) to actually test their own counting instead
	// of just reading the answer off the discard pile the whole time.
	SDL_FRect cardCountToggleButton(){
		return SDL_FRect{ .x = discardPosition.x, .y = discardPosition.y + cardHeight + 6.0f, .w = cardWidth, .h = 44 };
	}

	bool isCardCountToggleHit(SDLState& state, float windowX, float windowY){
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			return false;

		SDL_FPoint p{x, y};
		SDL_FRect btn = cardCountToggleButton();
		return SDL_PointInRectFloat(&p, &btn);
	}

	void toggleCardCount(){
		showCardCount = !showCardCount;
		sound(Sfx::Tap);
	}

	void drawCardCountToggleButton(SDLState& state){
		SDL_FRect btn = cardCountToggleButton();
		SDL_SetRenderDrawColor(state.renderer, showCardCount ? 90 : 70, showCardCount ? 150 : 70, showCardCount ? 110 : 140, 255);
		SDL_RenderFillRect(state.renderer, &btn);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &btn);

		std::string label = "CNT";
		float pixel = 5.0f;
		// Height is 44 (still leaves a gap above the center betting box),
		// so the default pixel size fits -- the height check below only
		// matters if the button is ever shrunk again.
		float maxW = btn.w - 8.0f;
		float maxH = btn.h - 4.0f;
		float w = DigitFont::textWidth(label, pixel);
		if(w > maxW && w > 0.0f)
			pixel *= maxW / w;
		if(5 * pixel > maxH)
			pixel = maxH / 5.0f;
		w = DigitFont::textWidth(label, pixel);
		DigitFont::drawText(state, label, btn.x + (btn.w - w) / 2.0f, btn.y + (btn.h - 5 * pixel) / 2.0f, pixel, SDL_Color{255, 255, 255, 255});
	}

	// DEAL was pressed: locks in whatever each player's raise/lower buttons
	// landed on and actually starts the round. A player who bet nothing
	// sits this round out entirely -- no cards, no turn (see firstDeal()
	// and skipZeroBetPlayers()).
	void beginRound(){
		if(!awaitingBets)
			return;

		awaitingBets = false;

		// Belt-and-suspenders: every raise already re-clamps itself (see
		// handleBettingPoint()), but re-checking once more right before
		// bets actually leave the bankroll costs nothing and guarantees
		// this invariant holds no matter how it got here.
		for(int i = 0; i < numberOfPlayers; i++)
			clampBetsToBankroll(i);

		for(int i = 0; i < numberOfPlayers; i++){
			if(players[i].getBet() > 0){
				// Total leaving the bankroll this deal -- main bet plus
				// whichever side bet(s) are actually in play -- shown as
				// one combined -$ readout rather than several at once.
				int totalWagered = players[i].getBet();
				if(hasAnySideBet(gameMode))
					totalWagered += players[i].getSideBet() + players[i].getMatchUpBet() + players[i].getMatchDownBet();

				players[i].deductBet();
				players[i].startRoundBet();
				if(hasAnySideBet(gameMode))
					players[i].deductSideBets();

				queueBankrollChange(i, -totalWagered);
			}

			sideBetResult[i] = HandResult::None;
			matchUpResult[i] = HandResult::None;
			matchDownResult[i] = HandResult::None;
			matchDownResolved = false;
			luckyStiffPending[i] = false;
		}

		activePlayer = 0;
		skipZeroBetPlayers();

		firstDeal();
		awaitingInitialDeal = hasAnySideBet(gameMode);
		awaitingPeek = true;
		awaitingInsurance = false;
		insuranceOffered = false;
		insuranceQueue.clear();
		for(int i = 0; i < 5; i++)
			insuranceBet[i] = 0;
	}

	// Keeps a seat's main bet + side bet(s) from ever adding up to more
	// than that player's own bankroll -- raiseBet()/raiseSideBet()/etc.
	// each independently clamp themselves to bankroll, but that only
	// guarantees any *one* of them fits alone, not that they all still fit
	// together (e.g. a full-bankroll main bet, then a side bet raised on
	// top of it). Side bets give way first (they're the optional extra,
	// not the hand itself), then the main bet itself is pinned down to
	// whatever's actually left.
	void clampBetsToBankroll(int playerIndex){
		Person& p = players[playerIndex];
		int bankroll = p.getBankroll();
		int mainBet = p.getBet();
		int sideTotal = p.getSideBet() + p.getMatchUpBet() + p.getMatchDownBet();

		if(mainBet + sideTotal <= bankroll)
			return;

		p.zeroSideBets();
		p.setBetDirect(std::min(mainBet, bankroll));
	}

	// windowX/windowY: raw event coordinates in window space, same
	// convention as Menu/SetupMenu's handlePoint. Self-contained: mutates
	// each player's bet directly and calls beginRound() itself once DEAL
	// is hit, so mina.cpp doesn't need to do anything with the return.
	void handleBettingPoint(SDLState& state, float windowX, float windowY){
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			return;

		SDL_FPoint p{x, y};

		for(int i = 0; i < numberOfPlayers; i++){
			if(players[i].getBankroll() <= 0){
				SDL_FRect buyIn = buyInButton(i);
				if(SDL_PointInRectFloat(&p, &buyIn)){
					players[i].rebuy();
					sound(Sfx::Tap);
				}
				continue;
			}

			BetRow row = betRow(i);
			int denom = BET_DENOMS[chipIndex[i]];

			if(SDL_PointInRectFloat(&p, &row.lower)){
				players[i].lowerBet(denom);
				sound(Sfx::Tap);
				return;
			}
			if(SDL_PointInRectFloat(&p, &row.raise)){
				players[i].raiseBet(denom);
				sound(Sfx::ChipBet);
				clampBetsToBankroll(i);
				return;
			}
			if(SDL_PointInRectFloat(&p, &row.selMinus)){
				chipIndex[i] = std::max(0, chipIndex[i] - 1);
				sound(Sfx::Tap);
				return;
			}
			if(SDL_PointInRectFloat(&p, &row.selPlus)){
				chipIndex[i] = std::min(4, chipIndex[i] + 1);
				sound(Sfx::Tap);
				return;
			}

			// Side bets use the same currently-selected chip denomination
			// as the main bet -- one denom control per seat, not a second
			// one duplicated for the side bet(s).
			if(hasLuckyLadies(gameMode) || hasLuckyStiff(gameMode)){
				SideBetRow sb = sideBetRow(i, 0);
				if(SDL_PointInRectFloat(&p, &sb.selMinus)){
					players[i].lowerSideBet(denom);
				sound(Sfx::Tap);
					return;
				}
				if(SDL_PointInRectFloat(&p, &sb.selPlus)){
					players[i].raiseSideBet(denom);
				sound(Sfx::ChipBet);
					clampBetsToBankroll(i);
					return;
				}
			} else if(isPlayersEdge(gameMode)){
				SideBetRow up = sideBetRow(i, 0);
				if(SDL_PointInRectFloat(&p, &up.selMinus)){
					players[i].lowerMatchUpBet(denom);
				sound(Sfx::Tap);
					return;
				}
				if(SDL_PointInRectFloat(&p, &up.selPlus)){
					players[i].raiseMatchUpBet(denom);
				sound(Sfx::ChipBet);
					clampBetsToBankroll(i);
					return;
				}

				SideBetRow down = sideBetRow(i, 1);
				if(SDL_PointInRectFloat(&p, &down.selMinus)){
					players[i].lowerMatchDownBet(denom);
				sound(Sfx::Tap);
					return;
				}
				if(SDL_PointInRectFloat(&p, &down.selPlus)){
					players[i].raiseMatchDownBet(denom);
				sound(Sfx::ChipBet);
					clampBetsToBankroll(i);
					return;
				}
			}
		}

		SDL_FRect deal = dealButton();
		if(SDL_PointInRectFloat(&p, &deal)){
			sound(Sfx::Tap);
			beginRound();
		}
	}

	void draw(SDLState& state, Resources& res){

		// table
        SDL_RenderTexture(state.renderer,res.tableCloth,nullptr,nullptr);

		// Drawn before any cards (and unconditionally, not just once play
		// starts) so it's visible from the moment the game loads, and so
		// cards land on top of it rather than the other way around.
		drawChipTray(state, res);

		// Part of the table too: under the cards, flying cards and bet
		// controls alike.
		drawSideBetCircles(state, res);
		drawMainBetChips(state, res);

		for(int i = 0; i < discard.size(); i++){
			discard[i].draw(state,res);
		}

		drawCardCountStats(state);

		// Right under the discard pile, so drawn with the table furniture
		// (before any hand or the flying card) -- cards swept to discard
		// pass over it instead of disappearing underneath it.
		drawCardCountToggleButton(state);

		// player hands
		// hideInactiveHands (GameOptionsMenu): only while a round's
		// actually being played through -- never during betting (nothing's
		// dealt yet) and never once the dealer's finished (awaitingNewRound
		// -- every hand needs to actually show for its WIN/PUSH/LOSE result
		// to mean anything).
		bool applyHideInactiveHands = hideInactiveHands && !awaitingBets && !awaitingNewRound;
		for(int i = 0; i < numberOfPlayers; i++){
			// activePlayer starts each round pointing at seat 0 before any
			// bets are even placed, so the turn arrow would otherwise show
			// up during betting pointing at an empty hand.
			players[i].draw(state,res, playersTurn() && activePlayer == i, applyHideInactiveHands);
		}

		// x computed live from each card's *current* index, not a position
		// baked in once by makeShoe() -- shoe[i]'s stored position never
		// changes as getNextCard() erases the front of the vector, so a
		// stack drawn from stale stored positions just revealed
		// progressively-further-right cards as it depleted instead of
		// visibly shrinking back toward shoePosition the way cards are
		// actually being pulled from.
		//
		// Cards already taken by getNextCard() but still waiting in the
		// deal queue are drawn at the front of the stack until they
		// actually fly, so the shoe shrinks one card at a time as they
		// leave instead of all at once when a whole deal is queued.
		std::vector<int> queuedFromShoe;
		{
			std::queue<DealRequest> pending = dealQueue;
			while(!pending.empty()){
				DealRequest r = pending.front();
				if(r.from.x == shoePosition.x && r.from.y == shoePosition.y)
					queuedFromShoe.push_back(r.card.getValue());
				pending.pop();
			}
		}
		int stackSize = (int)(queuedFromShoe.size() + shoe.size());
		for(int i = stackSize - 1; i >= 0; i--){
			SDL_FRect dest{
				.x = shoePosition.x + i * shoeCardSpacing,
				.y = shoePosition.y,
				.w = cardWidth / 10,
				.h = cardHeight
			};

			int value = i < (int)queuedFromShoe.size() ? queuedFromShoe[i] : shoe[i - queuedFromShoe.size()].getValue();
			if(value == 14)
				SDL_RenderTexture(state.renderer,res.allCards,&shoeYellow,&dest);
			else
				SDL_RenderTexture(state.renderer,res.allCards,&shoeBack,&dest);
		}

		// dealer hand
		dealer.draw(state,res,false);

		if(shuffling)
			drawShuffle(state, res);

		if(cardAnimation.has_value()) {
			CardAnimation& animation = cardAnimation.value();

			// hideInactiveHands (GameOptionsMenu): the same render-only
			// override Person::draw() applies to an already-landed hand
			// has to apply to a card still mid-flight too -- otherwise a
			// hidden player's own initial cards flew in plainly visible,
			// only actually disappearing once they landed. Doesn't touch
			// animation.showCard itself (that's still the card's real
			// shown/hidden state, read at landing for counting).
			bool forceHidden = hideInactiveHands && !awaitingBets && !awaitingNewRound
				&& !animation.isDealer && animation.playerIndex != activePlayer;
			SDL_FRect source = (animation.showCard && !forceHidden) ? animation.card.getSrc() : backOFCard;

			SDL_RenderTextureRotated(state.renderer,res.allCards,&source,&animation.destination,animation.currentAngle,&rotationTopLeft,SDL_FLIP_NONE);
		}

		drawHandTotals(state);
		drawBankrolls(state);
		drawHandResults(state);

		if(awaitingBets)
			drawBetting(state);
		else{
			drawQuickTipButton(state);
			drawQuickTip(state);
		}

		drawChipAnimations(state, res);
		drawInsurancePrompt(state);
		drawJackpotCallout(state);
	}

	// Hit/stand/double/etc. only make sense mid-turn: not while a card is
	// flying or a pause is running, not while bets are open, and not
	// while the finished round is being resolved and swept away
	// (awaitingNewRound) -- clearTable() resets activePlayer to 0 for the
	// sweep, so without that last check a tap in the frame between the
	// last discard landing and betting opening would act on player 1's
	// already-empty hand.
	bool acceptingPlayerInput(){
		// dealQueue too, not just the flying card: a card queued this
		// frame (say by H) hasn't started moving yet, and a second key in
		// the same frame would otherwise act on the hand as if it weren't
		// coming -- e.g. surrendering or doubling a hand that's already
		// been hit.
		return tableIdle() && !activeHandNeedsSecondCard() && !activeHandIsTwentyOne();
	}

	// Nothing moving, queued, paused or pending between rounds.
	bool tableIdle(){
		return !cardAnimation.has_value() && dealQueue.empty() && pauseTimer <= 0.0f && !awaitingBets
			&& !awaitingNewRound && !awaitingInitialDeal && !awaitingPeek && !awaitingInsurance && !shuffling;
	}

	// A split hand starts with just the one card it was split off with;
	// update() deals its second card as soon as play reaches it, and no
	// input is taken until it has.
	bool activeHandNeedsSecondCard(){
		if(activePlayer >= numberOfPlayers)
			return false;
		Person& p = players[activePlayer];
		int h = p.getActiveHand();
		return h < p.hands.size() && p.hands[h].isFromSplit() && p.hands[h].getHandSize() == 1;
	}

	// Pays a 21 on the spot: a natural blackjack 3:2 (Player's Edge:
	// spanish21Credit(), its bonuses and Super Bonus). Zeroing the hand's
	// bet is what keeps resolveRound() from paying it a second time (it
	// skips any hand with bet<=0, same as a surrendered one).
	void payTwentyOne(int playerIndex, int handIdx){
		Hand& hand = players[playerIndex].hands[handIdx];
		int bet = hand.getBet();
		if(bet <= 0 || hand.getHandTotal() != 21)
			return;
		bool natural = hand.getHandSize() == 2 && !hand.isFromSplit();
		int credit = isPlayersEdge(gameMode) ? spanish21Credit(hand, 0, false, false) : bet + bet * 3 / 2;
		if(isPlayersEdge(gameMode))
			credit += superBonus(playerIndex, hand);
		if(stats)
			stats->recordHand(credit - bet, true, false, natural, false, false);
		hand.setResult(HandResult::Win);
		queueChipPayout(playerIndex, credit, false, bet, betSpot(playerIndex, handIdx));
		hand.setBet(0);
	}

	// Right after the dealer's peek (no dealer blackjack), every natural
	// at the table is paid at once, the way a dealer pays blackjacks
	// before anyone plays -- not left waiting for that seat's turn.
	void payNaturals(){
		for(int i = 0; i < numberOfPlayers; i++){
			Person& p = players[i];
			if(p.hands.size() == 1 && p.hands[0].getHandSize() == 2 && !p.hands[0].isFromSplit())
				payTwentyOne(i, 0);
		}
	}

	// A player is actually playing a hand right now -- the turn arrow only
	// shows then: not while betting, dealing the opening cards, peeking,
	// asking about insurance, while the dealer plays, or between rounds.
	bool playersTurn(){
		if(awaitingBets || awaitingInitialDeal || awaitingPeek || awaitingInsurance || awaitingNewRound || shuffling)
			return false;
		if(activePlayer >= numberOfPlayers)
			return false;
		Person& p = players[activePlayer];
		int h = p.getActiveHand();
		return h < p.hands.size() && p.hands[h].getHandSize() > 0;
	}

	bool activeHandIsTwentyOne(){
		if(activePlayer >= numberOfPlayers)
			return false;
		Person& p = players[activePlayer];
		int h = p.getActiveHand();
		return h < p.hands.size() && p.hands[h].getHandTotal() == 21;
	}

	// For mina.cpp's save-after-every-round: true once a betting phase is
	// open and every payout chip has actually landed (bankrolls are only
	// credited on arrival), so the saved numbers are final.
	bool isSettledForSave(){
		return awaitingBets && chipAnimations.empty();
	}

	int getNumberOfPlayers(){ return numberOfPlayers; }

	// Lifetime stats (Stats.h), owned and saved by mina.cpp; null in tests.
	void setStats(Stats* s){
		stats = s;
		if(stats)
			stats->setGame(gameMode);
	}

	// Sound effects (Sound.h): mina.cpp hands in Audio::play(); unset in
	// tests, so the table just stays silent.
	void setSoundPlayer(SoundPlayer player){ soundPlayer = std::move(player); }
	int getPlayerBankroll(int i){ return players[i].getBankroll(); }
	int getPlayerTotalBuyIns(int i){ return players[i].getInitialBankroll(); }

	// Resume: after configurePlayers() has seated everyone at their
	// original buy-in, put back where each bankroll had actually got to.
	void restoreProgress(const int current[5], const int totalBuyIns[5]){
		for(int i = 0; i < numberOfPlayers; i++)
			players[i].restoreBankroll(current[i], totalBuyIns[i]);
	}
	int getPlayerBet(int i){ return players[i].getBet(); }
	// The per-seat side-bet size, in the same terms configurePlayers()
	// takes it (Player's Edge's two Match bets are seeded equal).
	int getPlayerSideBet(int i){
		return isPlayersEdge(gameMode) ? players[i].getMatchUpBet() : players[i].getSideBet();
	}

	void handleEvent(const SDL_Event& event)
	{
		switch(event.type)
		{
			// Key down (not up), ignoring auto-repeat: mina.cpp handles
			// menus/Space/Esc on key down too, and splitting one press
			// across down/up let a single key act twice (e.g. Space both
			// pressing GO on a menu and then dealing on release). Keep
			// KeyboardMenu.h's list in sync with these bindings.
			case SDL_EVENT_KEY_DOWN:
				if(!event.key.repeat && isAwaitingInsurance()){
					if(event.key.scancode == SDL_SCANCODE_Y)
						answerInsurance(true);
					else if(event.key.scancode == SDL_SCANCODE_N)
						answerInsurance(false);
					break;
				}
				if(event.key.repeat || !acceptingPlayerInput())
					break;

				switch(event.key.scancode)
				{
					case SDL_SCANCODE_S:
						playerAction('S');
					break;

					case SDL_SCANCODE_H:
						playerAction('H');
					break;

					case SDL_SCANCODE_P:
						playerAction('P');
					break;

					case SDL_SCANCODE_D:
						playerAction('D');
					break;

					case SDL_SCANCODE_R:
						playerAction('R');
					break;

					default:
					break;
				}
			break;

			// Touch bookkeeping always runs, even mid-animation, so a finger
			// lifted while an animation is playing doesn't leave a phantom
			// entry in activeTouches and throw off the next gesture's finger count.
			case SDL_EVENT_FINGER_DOWN: {
				SDL_FingerID id = event.tfinger.fingerID;
				activeTouches[id] = TouchPoint{
					event.tfinger.x, event.tfinger.y,
					event.tfinger.x, event.tfinger.y
				};
				break;
			}

			case SDL_EVENT_FINGER_MOTION: {
				auto it = activeTouches.find(event.tfinger.fingerID);
				if(it != activeTouches.end()){
					it->second.x = event.tfinger.x;
					it->second.y = event.tfinger.y;
				}
				break;
			}

			case SDL_EVENT_FINGER_UP: {
				auto it = activeTouches.find(event.tfinger.fingerID);
				if(it != activeTouches.end()){
					it->second.x = event.tfinger.x;
					it->second.y = event.tfinger.y;

					endedTouches.push_back(it->second);
					activeTouches.erase(it);
				}

				// Only once every finger from this gesture has lifted do we
				// know the final finger count and can classify the gesture.
				if(activeTouches.empty() && !endedTouches.empty()){
					if(acceptingPlayerInput())
						processGesture(endedTouches);

					endedTouches.clear();
				}
				break;
			}

			default:
			break;
		}
	}

	void update(float deltaTime) {
		// Payout chips fly independently of everything else below -- they
		// start firing exactly when the dealer-finish pause does (see
		// dealDealer()) and need to keep progressing *during* that pause,
		// not be blocked by it like dealing/resolving are.
		for(auto it = chipAnimations.begin(); it != chipAnimations.end();){
			float before = it->elapsed;
			it->elapsed += deltaTime;
			// The pile leaving the felt for the bankroll gets its own sound.
			if(it->kind != ChipAnimation::Loss){
				float leave = chipAnimationLength(*it) - CHIPS_TO_BANKROLL;
				if(before < leave && it->elapsed >= leave)
					sound(Sfx::ChipsTake);
			}
			if(it->elapsed >= chipAnimationLength(*it)){
				if(it->kind != ChipAnimation::Loss){
					players[it->playerIndex].credit(it->creditAmount);
					queueBankrollChange(it->playerIndex, it->creditAmount, it->isPush);
				} else{
					// A collected (lost) bet only goes into the tray once it
					// actually arrives -- chip by chip, each in its own
					// colour. A payout already took its chips out the
					// moment it was queued.
					for(int d : chipsFor(it->stake))
						putInTray(d);
				}

				it = chipAnimations.erase(it);
			} else{
				++it;
			}
		}

		// The banner plays out on its own clock too -- it shouldn't hold up
		// the round, just sit on top of it.
		if(!jackpotCallouts.empty()){
			jackpotCallouts.front().elapsed += deltaTime;
			if(jackpotCallouts.front().elapsed >= CALLOUT_DURATION)
				jackpotCallouts.erase(jackpotCallouts.begin());
		}

		// Ages independently of everything else below too, same reasoning
		// as chipAnimations just above.
		for(auto it = bankrollChanges.begin(); it != bankrollChanges.end();){
			it->elapsed += deltaTime;
			if(it->elapsed >= it->duration)
				it = bankrollChanges.erase(it);
			else
				++it;
		}

		// A fresh shoe being shuffled (drawShuffle()): betting opens once
		// the shuffle's done and the new shoe is in place.
		if(shuffling){
			shuffleElapsed += deltaTime;
			if(shuffleElapsed < SHUFFLE_DURATION)
				return;
			shuffling = false;
			makeShoe();
			runningCount = 0;
			openBettingPhase();
			return;
		}

		// Hold everything -- no dealing, no resolving -- until a pending
		// pause (bust shown, or dealer's finished hand) has run its course.
		if(pauseTimer > 0.0f){
			pauseTimer -= deltaTime;
			if(pauseTimer > 0.0f)
				return;

			pauseTimer = 0.0f;
			if(onPauseComplete){
				std::function<void()> action = onPauseComplete;
				onPauseComplete = nullptr;
				action();
			}

			// The action above can start a pause of its own -- e.g. the
			// hole-card reveal's continueDealerPlay() finding the dealer
			// already on 17+ resolves the round and starts the
			// dealer-finish pause (whose own action is clearTable()).
			// Falling through here would see an empty deal queue and
			// awaitingNewRound, and open betting immediately -- before
			// the WIN/LOSE pause or the discard sweep ever ran, leaving
			// the bet controls up while the old cards were still being
			// pulled off the table.
			if(pauseTimer > 0.0f)
				return;
		}

		// If no card is moving, start the next queued deal.
		if(!cardAnimation.has_value()) {
			startDeal();
			if(!cardAnimation.has_value() && dealQueue.empty()){
				if(awaitingInitialDeal){
					awaitingInitialDeal = false;
					resolveSideBets();
				}
				if(awaitingPeek && !insuranceOffered){
					insuranceOffered = true;
					if(startInsurance())
						return;
				}
				if(awaitingInsurance)
					return;
				if(awaitingPeek){
					awaitingPeek = false;
					if(dealerPeek())
						return;
					payNaturals();
					skipZeroBetPlayers();
				}

				if(tableIdle() && activeHandNeedsSecondCard()){
					onHit();
					return;
				}
				// A hand already on 21 when its turn comes up (a natural,
				// or a split hand's second card) plays itself out instead
				// of waiting for input -- see checkBreak().
				if(tableIdle() && activeHandIsTwentyOne()){
					checkBreak();
					return;
				}
				// Let every payout and collection finish where it can be
				// seen before the table moves on to the next round.
				if(awaitingNewRound && !chipAnimations.empty())
					return;
				if(awaitingNewRound){
					awaitingNewRound = false;

					// Every card from the just-finished round has landed
					// in discard by this point (dealQueue's empty, nothing
					// is animating) -- safe to sweep it and cut a fresh
					// shoe before opening bets on the next round.
					if(shoeNeedsReshuffle){
						// The discards are gathered up and shuffled on the
						// table, to the shuffle sound, before going back in
						// the shoe -- see the shuffling branch above.
						shoeNeedsReshuffle = false;
						discard.clear();
						shoe.clear();
						shuffling = true;
						shuffleElapsed = 0.0f;
						// A fresh shoe comes with a full tray.
						for(int& fill : trayFillCount)
							fill = TRAY_FILL_COUNT;
						sound(Sfx::Shuffle);
						return;
					}

					openBettingPhase();
				}
			}
			return;
		}

		CardAnimation& animation = cardAnimation.value();

		animation.elapsed += deltaTime;

		float progress = animation.elapsed / animation.duration;

		if(progress > 1.0f)
			progress = 1.0f;

		animation.destination.x =
			animation.start.x +
			(animation.end.x - animation.start.x) * progress;

		animation.destination.y =
			animation.start.y +
			(animation.end.y - animation.start.y) * progress;

		animation.currentAngle = animation.startAngle + (animation.finalAngle - animation.startAngle) * progress;

		if(progress >= 1.0f) {
			animation.card.setPostion(animation.end);
			// Persist the settled rotation onto the card itself, same as
			// position just above -- matters most for the discard pile,
			// which (unlike Hand::draw()) has nothing else to pass a
			// rotation in with; it relies entirely on Card::draw() reading
			// whatever's already stored on the card.
			animation.card.setRotation(animation.finalAngle);
			if(animation.discard){
				// A card arriving in the discard pile keeps whatever isShown
				// it had in the hand (true, for any card that was dealt face
				// up) unless told otherwise here -- addCard() does this same
				// showCard() call for the dealer/player branches below, but
				// nothing did it for discard until now, so discarded cards
				// were rendering face-up forever.
				animation.card.showCard(animation.showCard);
				discard.push_back(animation.card);
			} else if(animation.isDealer) {
				dealer.addCard(animation.card, animation.showCard);
				// The dealer's hole card is dealt with showCard=false and
				// counted separately once it's actually revealed -- see
				// dealDealer().
				if(animation.showCard)
					addToRunningCount(animation.card.getValue());
			} else {
				players[animation.playerIndex].addCard(animation.card, animation.showCard, animation.split, animation.doubleHand);
				// A split card is one that already landed (and was
				// already counted) once before -- it's just moving to a
				// new hand, not a newly-seen card, so it doesn't count
				// again here.
				if(animation.showCard && !animation.split)
					addToRunningCount(animation.card.getValue());
				checkBreak(animation.doubleHand);
			}

			cardAnimation.reset();
			startDeal();
		}
	}

	void dealDealer(){
		if(activePlayer < numberOfPlayers)
			return;

		// Every player can be done before the deal even finishes (all
		// naturals, or nobody betting) -- the dealer still has to peek
		// first (update()), or a dealer blackjack would be settled twice.
		if(awaitingInitialDeal || awaitingPeek || awaitingInsurance || awaitingNewRound || shuffling)
			return;

		if(cardAnimation.has_value() || !dealQueue.empty() || pauseTimer > 0.0f)
			return;

		// The hole card is revealed exactly once (guarded on its own
		// isShown, since dealDealer() gets called again on every
		// subsequent frame while the dealer keeps hitting past this
		// point), with a brief pause afterward so it actually registers
		// before the next card -- if the dealer needs to hit -- starts
		// flying. Without this, revealing the hole card and queuing the
		// next card happened in the same frame, easy to miss entirely.
		if(dealer.hands[0].getHandSize() >= 2 && !dealer.hands[0].cards[1].getShown()){
			// The hole card counts the moment it's actually revealed, not
			// at deal time.
			addToRunningCount(dealer.hands[0].cards[1].getValue());
			dealer.showCards();
			sound(Sfx::Flip);
			resolveMatchDown();

			pauseTimer = HOLE_CARD_REVEAL_PAUSE_DURATION * dealerSpeedFactor;
			onPauseComplete = [this](){
				continueDealerPlay();
			};
			return;
		}

		continueDealerPlay();
	}

	// Hitting (and doubling) split aces: only Player's Edge (Spanish 21)
	// allows it -- standard blackjack, Lucky Ladies/Lucky Stiff (standard
	// rules plus a side bet) and Free Bet Blackjack all deal split aces
	// one card each, the common Vegas rule.
	bool allowsHitSplitAces(){
		return isPlayersEdge(gameMode);
	}

	// Before the peek, with an Ace showing: queue every seat that can be
	// offered insurance (still has its bet riding -- a Player's Edge
	// blackjack is already paid -- and can afford half of it, or has a
	// blackjack, where the offer is even money instead). Returns true if
	// anyone's being asked, which holds the round until they've answered.
	bool startInsurance(){
		Hand& dealerHand = dealer.hands[0];
		if(dealerHand.getHandSize() < 2 || dealerHand.cards[0].getValue() != 1)
			return false;

		insuranceQueue.clear();
		for(int i = 0; i < numberOfPlayers; i++){
			Person& p = players[i];
			if(p.hands.empty() || p.hands[0].getBet() <= 0)
				continue;
			bool blackjack = playerHasBlackjack(i);
			if(blackjack && isPlayersEdge(gameMode))
				continue; // a Spanish 21 blackjack always wins 3:2 anyway
			if(blackjack || p.getBankroll() >= p.hands[0].getBet() / 2)
				insuranceQueue.push_back(i);
		}
		awaitingInsurance = !insuranceQueue.empty();
		return awaitingInsurance;
	}

	bool playerHasBlackjack(int i){
		Person& p = players[i];
		if(p.hands.size() != 1)
			return false;
		Hand& h = p.hands[0];
		return h.getHandSize() == 2 && h.getHandTotal() == 21 && !h.isFromSplit();
	}

public:
	bool isAwaitingInsurance(){
		return awaitingInsurance && !cardAnimation.has_value() && dealQueue.empty();
	}

	// One answer for the whole table -- every seat offered insurance takes
	// it (or not) together. Insurance: half the bet goes up now (paid back
	// 3x by dealerPeek() on a dealer blackjack). Even money (the seat has a
	// blackjack): paid 1:1 right now and the hand is settled, so a dealer
	// blackjack can't push it.
	void answerInsurance(bool yes){
		if(!isAwaitingInsurance() || insuranceQueue.empty())
			return;

		std::vector<int> seats = insuranceQueue;
		insuranceQueue.clear();
		for(int i : seats)
			answerInsuranceFor(i, yes);
		awaitingInsurance = false;
	}

	void answerInsuranceFor(int i, bool yes){
		Person& p = players[i];

		if(yes){
			Hand& h = p.hands[0];
			if(playerHasBlackjack(i)){
				if(stats)
					stats->recordHand(h.getBet(), true, false, true, false, false);
				queueChipPayout(i, h.getBet() * 2, false, h.getBet(), betSpot(i, 0));
				h.setResult(HandResult::Win);
				h.setBet(0);
			} else{
				int ins = h.getBet() / 2;
				if(ins > 0 && p.getBankroll() >= ins){
					p.deductFromBankroll(ins);
					queueBankrollChange(i, -ins);
					insuranceBet[i] = ins;
					if(stats)
						stats->bump(Stats::InsuranceTaken);
				}
			}
		}
	}

	// Tap/click on the insurance prompt's YES/NO.
	void handleInsurancePoint(SDLState& state, float windowX, float windowY){
		float x, y;
		if(!SDL_RenderCoordinatesFromWindow(state.renderer, windowX, windowY, &x, &y))
			return;
		SDL_FPoint p{x, y};
		SDL_FRect yes = insuranceYesButton(), no = insuranceNoButton();
		if(SDL_PointInRectFloat(&p, &yes)){
			sound(Sfx::Tap);
			answerInsurance(true);
		} else if(SDL_PointInRectFloat(&p, &no)){
			sound(Sfx::Tap);
			answerInsurance(false);
		}
	}

private:
	SDL_FRect insurancePanel(){ return SDL_FRect{ .x = 440, .y = 270, .w = 560, .h = 180 }; }
	SDL_FRect insuranceYesButton(){ return SDL_FRect{ .x = 510, .y = 374, .w = 190, .h = 56 }; }
	SDL_FRect insuranceNoButton(){ return SDL_FRect{ .x = 740, .y = 374, .w = 190, .h = 56 }; }

	void drawInsurancePrompt(SDLState& state){
		if(!isAwaitingInsurance() || insuranceQueue.empty())
			return;
		// Totals for everyone being asked: what insurance costs the table,
		// and how many blackjacks would take even money instead.
		int cost = 0, blackjacks = 0;
		for(int i : insuranceQueue){
			if(playerHasBlackjack(i))
				blackjacks++;
			else
				cost += players[i].hands[0].getBet() / 2;
		}
		bool evenMoney = cost == 0;

		SDL_FRect panel = insurancePanel();
		SDL_SetRenderDrawColor(state.renderer, 15, 25, 18, 255);
		SDL_RenderFillRect(state.renderer, &panel);
		SDL_SetRenderDrawColor(state.renderer, 255, 210, 40, 255);
		for(int k = 0; k < 3; k++){
			SDL_FRect ring{ panel.x - k, panel.y - k, panel.w + 2 * k, panel.h + 2 * k };
			SDL_RenderRect(state.renderer, &ring);
		}

		std::string q = evenMoney ? "EVEN MONEY?" : "TABLE INSURANCE?";
		float qPixel = 5.5f;
		float qW = DigitFont::textWidth(q, qPixel);
		DigitFont::drawText(state, q, panel.x + (panel.w - qW) / 2.0f, panel.y + 18.0f, qPixel, SDL_Color{255, 255, 255, 255});

		std::string sub = evenMoney ? "BLACKJACKS PAID 1:1 NOW"
			: "COSTS " + std::to_string(cost) + ", PAYS 2:1"
				+ (blackjacks > 0 ? ", BLACKJACKS EVEN MONEY" : "");
		float subPixel = 4.0f;
		float subW = DigitFont::textWidth(sub, subPixel);
		DigitFont::drawText(state, sub, panel.x + (panel.w - subW) / 2.0f, panel.y + 18.0f + 5 * qPixel + 18.0f, subPixel, SDL_Color{210, 210, 210, 255});

		SDL_FRect yes = insuranceYesButton(), no = insuranceNoButton();
		drawButton(state, yes, SDL_Color{60, 130, 70, 255});
		drawButton(state, no, SDL_Color{130, 60, 60, 255});
		float bPixel = 6.0f;
		float yW = DigitFont::textWidth("YES", bPixel), nW = DigitFont::textWidth("NO", bPixel);
		DigitFont::drawText(state, "YES", yes.x + (yes.w - yW) / 2.0f, yes.y + (yes.h - 5 * bPixel) / 2.0f, bPixel, SDL_Color{255, 255, 255, 255});
		DigitFont::drawText(state, "NO", no.x + (no.w - nW) / 2.0f, no.y + (no.h - 5 * bPixel) / 2.0f, bPixel, SDL_Color{255, 255, 255, 255});
	}

	// Runs once the initial deal has landed. With an Ace or 10-value
	// up-card the dealer checks the hole card; on a blackjack it's turned
	// over and the round settles immediately -- before anyone can double
	// or split into it. Returns true if the round just ended that way.
	bool dealerPeek(){
		Hand& dealerHand = dealer.hands[0];
		if(dealerHand.getHandSize() < 2)
			return false;

		int up = dealerHand.cards[0].getValue();
		if(up != 1 && up < 10)
			return false;

		bool dealerBlackjack = dealerHand.getHandTotal() == 21;

		// Insurance pays 2:1 (the stake back plus twice it) on a dealer
		// blackjack -- 5:1 in Player's Edge when that blackjack is suited;
		// otherwise the stake, taken when it was placed, is lost.
		int insuranceOdds = (isPlayersEdge(gameMode) && dealerHand.cards[0].getSuit() == dealerHand.cards[1].getSuit()) ? 5 : 2;
		for(int i = 0; i < numberOfPlayers; i++){
			if(insuranceBet[i] <= 0)
				continue;
			if(stats)
				stats->addNet(dealerBlackjack ? insuranceBet[i] * insuranceOdds : -insuranceBet[i]);
			if(dealerBlackjack)
				queueChipPayout(i, insuranceBet[i] * (insuranceOdds + 1), false, insuranceBet[i], seatPoint(i, 0, cardWidth / 2.0f, cardHeight + SPOT_MARGIN + 4.0f));
		}

		if(!dealerBlackjack)
			return false;

		if(!dealerHand.cards[1].getShown()){
			addToRunningCount(dealerHand.cards[1].getValue());
			dealer.showCards();
			sound(Sfx::Flip);
		}

		jackpotCallouts.push_back(JackpotCallout{ .title = "DEALER BLACKJACK", .detail = "" });

		resolveRound();
		awaitingNewRound = true;
		// Nobody's turn anymore (hides the turn arrow); clearTable() puts
		// it back to 0 for the sweep, same as a normal round end.
		activePlayer = numberOfPlayers;
		pauseTimer = DEALER_FINISH_PAUSE_DURATION * dealerSpeedFactor;
		onPauseComplete = [this](){
			clearTable();
		};
		return true;
	}

	// The rest of dealDealer() -- hit again if under 17, otherwise resolve
	// the round -- split out so it can run either immediately (hole card
	// already revealed on some earlier call) or once the reveal pause above
	// actually finishes.
	void continueDealerPlay(){
		std::cout << "Deal the dealer" << std::endl;

		if(dealer.getHandTotal() < 17)
			dealQueue.push(DealRequest{
			.playerIndex = -1,
			.isDealer = true,
			.showCard = true,
			.from = shoePosition,
			.to = dealer.getNextCardPosition(),
			.card = getNextCard()
			});
		else{
			// "When the dealer is done" -- settle every hand against the
			// final dealer total before their cards get swept away.
			resolveRound();

			// hideInactiveHands (GameOptionsMenu) stops force-hiding a
			// seat's cards once awaitingNewRound is true (see draw()) --
			// set here, not in onPauseComplete below, so every hand is
			// already showing for the WIN/PUSH/LOSE reveal and payout that
			// just happened above, not still hidden until the pause (and
			// the discard sweep after it) finishes playing out. Real
			// casinos turn every hand up before paying anyone, not after.
			awaitingNewRound = true;

			// Let the finished dealer hand sit on screen for a beat before
			// sweeping every hand into the discard pile.
			pauseTimer = DEALER_FINISH_PAUSE_DURATION * dealerSpeedFactor;
			onPauseComplete = [this](){
				clearTable();
			};
		}
	}

private:
	struct TouchPoint {
		float startX, startY;
		float x, y;
	};

	Person dealer;
	Person players[5];
	BettingSquare bettingSquares[6];

	int numberOfPlayers;
	bool H17;

	// Which game is actually being played -- drives side-bet UI/resolution
	// (hasLuckyLadies()/isPlayersEdge(), GameModeMenu.h) and, later, which
	// rules engine resolveRound() uses. Set once via configureGameMode().
	GameMode gameMode = GameMode::TwoDeck;

	int numberOfDecks;
	std::vector<Card> shoe;
	std::vector<Card> discard;

	int activePlayer = 0;
	std::queue<DealRequest> dealQueue;
	std::optional<CardAnimation> cardAnimation;
	std::vector<ChipAnimation> chipAnimations;

	// A rare side-bet hit (see queueJackpotCallout()) announced with a
	// big flashing banner across the middle of the table. Queued, not
	// replaced, so two players hitting on the same deal both get their
	// moment, one after the other.
	struct JackpotCallout{
		std::string title;
		std::string detail;
		float elapsed = 0.0f;
	};
	std::vector<JackpotCallout> jackpotCallouts;
	static constexpr float CALLOUT_DURATION = 3.5f;

	// A brief "-$25"/"+$50" readout under a seat's bankroll number
	// whenever it actually changes -- initial bet, split, double, a
	// side-bet or hand payout landing -- so a bankroll number moving or a
	// chip flying isn't the only sign anything happened. Purely time-based
	// (no fade, just disappears once elapsed >= duration): simpler than
	// wiring up alpha blending for what's meant to be a quick, glanceable
	// readout, not a polished animation.
	struct BankrollChange{
		int playerIndex;
		std::string text;
		SDL_Color color;
		float elapsed = 0.0f;
		float duration = 1.6f;
	};
	std::vector<BankrollChange> bankrollChanges;

	// isPush: a push's credit is the player's own bet coming back, not a
	// net gain -- labeled and colored differently (neutral white "PUSH
	// +$X" instead of a green "+$X") so it doesn't read as a win. Without
	// this, a push and an actual win looked identical here, which is what
	// made a push look like it was somehow paying double: bankroll visibly
	// went up by the full bet, styled exactly like a real payout.
	void queueBankrollChange(int playerIndex, int amount, bool isPush = false){
		if(amount == 0)
			return;

		std::string text = isPush
			? "PUSH +$" + std::to_string(amount)
			: (amount > 0 ? "+$" : "-$") + std::to_string(std::abs(amount));
		SDL_Color color = isPush
			? SDL_Color{220, 220, 220, 255}
			: (amount > 0 ? SDL_Color{90, 220, 110, 255} : SDL_Color{230, 80, 80, 255});

		bankrollChanges.push_back(BankrollChange{
			.playerIndex = playerIndex,
			.text = text,
			.color = color
		});
	}

	bool awaitingNewRound = false;

	// Set by getNextCard() the moment the yellow cut card is burned;
	// consumed once the round it was drawn during actually finishes (see
	// update()'s awaitingNewRound handling) -- discards everything and
	// deals a fresh shoe before the next round's bets open, same as a real
	// table cutting to a new shoe once the cut card's reached.
	bool shoeNeedsReshuffle = false;

	// A real table burns one card only when a fresh shoe goes into play,
	// not before every single hand -- makeShoe() sets this true, firstDeal()
	// clears it once it's actually burned the card.
	bool needsBurnCard = true;

	// Hi-Lo running count -- persists across every hand dealt from the
	// current shoe, reset only when the shoe itself is (a fresh
	// configureGameMode() or the cut-card reshuffle above), never per hand.
	// See addToRunningCount()'s call sites for exactly when a card counts.
	int runningCount = 0;

	// Per-card horizontal offset the shoe's on-table stack draws with --
	// set by makeShoe(), read by draw()'s shoe-rendering loop (see there
	// for why position isn't just baked into each Card once).
	float shoeCardSpacing = 3.0f;

	// Mirrors awaitingNewRound's "deal queue just drained" detection, but
	// for the *opening* deal instead of the closing one -- set by
	// beginRound() right after firstDeal() is queued, consumed the moment
	// those cards actually finish landing (see update()), which is exactly
	// when Lucky Ladies/Match Up (Table::resolveSideBets()) should fire:
	// as soon as each seat's first two cards are known, before anyone's
	// first decision.
	bool awaitingInitialDeal = false;

	// Set when a round is dealt; once the initial cards have all landed
	// the dealer checks for blackjack (see dealerPeek()) before anyone
	// can act -- US rules, so a dealer blackjack only ever takes the
	// original bets, never doubles or splits made against it.
	bool awaitingPeek = false;

	// Insurance / even money, offered when the dealer's up-card is an Ace
	// -- before the peek, one eligible seat at a time (insuranceQueue,
	// front = the seat being asked). insuranceBet[i] is what seat i put up
	// (half its bet); it pays 2:1 if the dealer turns out to have
	// blackjack, and is simply lost otherwise.
	Stats* stats = nullptr;
	SoundPlayer soundPlayer;
	void sound(Sfx sfx){ if(soundPlayer) soundPlayer(sfx); }

	bool awaitingInsurance = false;
	bool insuranceOffered = false;
	std::vector<int> insuranceQueue;
	int insuranceBet[5] = { 0, 0, 0, 0, 0 };

	// Snapshot of each seat's original first two cards, taken the moment
	// resolveSideBets() runs and kept for the rest of the round -- side
	// bets (Lucky Ladies, Match Up/Down) are always judged against these
	// exact two cards, never whatever's in hands[0] later. That distinction
	// only matters after a split (which pulls the second original card out
	// of hands[0] and into a new hand, backfilling hands[0] with a freshly
	// dealt one) -- without this, a Match Down resolved at round end could
	// silently match against a card the player was never dealt as part of
	// their original two.
	struct InitialTwoCards{ bool valid = false; int suit1 = 0, value1 = 0, suit2 = 0, value2 = 0; };
	InitialTwoCards initialTwoCards[5];

	// Per-seat side-bet outcomes, so drawSideBetRows() can show a WIN/LOSE
	// readout next to each selector instead of the payout/collection chip
	// flight being the only sign anything happened. Reset to None at the
	// start of every round (see beginRound()); set the moment each bet
	// actually resolves (resolveSideBets() for Lucky Ladies/Match Up/most
	// Lucky Stiff hands, resolveRound() for Match Down and a pending Lucky
	// Stiff hand, since those resolve later -- see their own comments).
	HandResult sideBetResult[5] = { HandResult::None, HandResult::None, HandResult::None, HandResult::None, HandResult::None };
	HandResult matchUpResult[5] = { HandResult::None, HandResult::None, HandResult::None, HandResult::None, HandResult::None };
	HandResult matchDownResult[5] = { HandResult::None, HandResult::None, HandResult::None, HandResult::None, HandResult::None };

	// Lucky Stiff only: set by evaluateLuckyStiffImmediate() when the
	// starting hand is an unpaired hard 12-16 -- neither an immediate win
	// nor an immediate loss, it rides along with the main hand and pays
	// (or doesn't) based on whether that hand ends up beating the dealer.
	// Consumed in resolveRound(), reset false at the start of every round.
	bool luckyStiffPending[5] = { false, false, false, false, false };

	// Set once resolveMatchDown() has run this round.
	bool matchDownResolved = false;

	// Each seat's bankroll label, as last drawn (drawBankrolls()).
	SDL_FPoint bankrollLabelCenter[5] = {};

	// Between rounds, while a fresh shoe is shuffled on the table.
	static constexpr float SHUFFLE_DURATION = 2.9f; // the shuffle sound's length
	bool shuffling = false;
	float shuffleElapsed = 0.0f;

	// True between rounds (including before the very first one) while the
	// table's waiting on bets -- see startGame()/beginRound(). Drives both
	// which input mina.cpp routes to (betting controls vs. gameplay
	// gestures) and whether drawBetting() renders.
	bool awaitingBets = false;

	// Which BET_DENOMS entry each player's raise/lower buttons currently
	// use, set via their own chip-size selector. Defaults to index 2 (25).
	int chipIndex[5] = {2, 2, 2, 2, 2};

	// In-game "quick view" -- unlike the pause menu's full StrategyChart,
	// this just pops up the one relevant row (see getStrategySituation())
	// so you don't have to leave the hand to check it.
	bool showQuickTip = false;
	// Defaults off -- the overlay used to always show, which defeats the
	// point of being able to test your own count; toggled on via
	// cardCountToggleButton() only when the player actually wants to check
	// themselves against it.
	bool showCardCount = false;

	// Freezes the table (no deals, no input) so the player has a moment to
	// actually see a busted/finished hand before its cards get swept off to
	// the discard pile. onPauseComplete holds whichever action was deferred
	// (discarding the bust, or clearing the table) and fires once when the
	// timer runs out.
	float pauseTimer = 0.0f;
	std::function<void()> onPauseComplete;

	SDL_FRect shoeBack{
		.x = 0,
		.y = 140,
		.w = 5,
		.h = 70
	};

	SDL_FRect shoeYellow{
		.x = 0,
		.y = 210,
		.w = 5,
		.h = 70
	};

	std::unordered_map<SDL_FingerID, TouchPoint> activeTouches;
	std::vector<TouchPoint> endedTouches;
	static constexpr float TAP_MOVE_THRESHOLD = 0.02f;
	static constexpr float SPLIT_DOT_THRESHOLD = -0.2f;

	static constexpr float DEAL_DURATION = 0.6f;
	static constexpr float DISCARD_DURATION = 0.2f;
	static constexpr float BUST_PAUSE_DURATION = 1.2f;
	// Long enough to read the WIN/PUSH/LOSE labels before clearTable()
	// sweeps the hands away, without feeling sluggish during normal play.
	static constexpr float DEALER_FINISH_PAUSE_DURATION = 2.5f;
	// Just the hole card's own reveal -- a beat to actually register it
	// before the next card (if the dealer needs to hit) starts flying, not
	// long enough to feel like a second full stop on top of
	// DEALER_FINISH_PAUSE_DURATION's longer one at the very end.
	static constexpr float HOLE_CARD_REVEAL_PAUSE_DURATION = 1.0f;

	// Dealer speed setting (see GameOptionsMenu): the 2 pauses the dealer
	// themselves impose -- reading the hole card, sitting on a finished
	// hand -- scale by this before being applied to pauseTimer (see
	// HOLE_CARD_REVEAL_PAUSE_DURATION/DEALER_FINISH_PAUSE_DURATION's own
	// call sites). Deliberately not applied to BUST_PAUSE_DURATION (a
	// *player's* own bust, not the dealer's pace) or to
	// DEAL_DURATION/DISCARD_DURATION (a card's own flight, shared by every
	// seat's deals alike). <1 is faster, >1 is slower, 1 is unchanged.
	float dealerSpeedFactor = 1.0f;

	// GameOptionsMenu's 2 house-rule toggles, both off (today's existing
	// behavior) by default. See onDouble()/resolveRound() for the first,
	// and the players[i].draw() call site in draw() for the second.
	bool faceDownDoubles = false;
	bool hideInactiveHands = false;

	// HUD: dealer's shown-card total, bottom left; the current active
	// hand's total, bottom right. Fixed screen-space boxes, not tied to any
	// seat. activeHandTotalBox sits higher than dealerTotalBox specifically
	// to leave room for the active bet/chip display drawn under it.
	// x is nudged in from the very edge (was 20 / 1290) -- drawTotalBox()
	// centers the "DEALER HAND"/"ACTIVE HAND" label on the box's width, and
	// that label is wider than the box itself, so at the old x the label
	// text ran past the canvas edge (x<0 on the left, >1440 on the right)
	// instead of just the box looking close to it.
	SDL_FRect dealerTotalBox{ .x = 40, .y = 645, .w = 130, .h = 50 };
	SDL_FRect activeHandTotalBox{ .x = 1270, .y = 590, .w = 130, .h = 50 };

	void drawHandTotals(SDLState& state){
		// Only once the dealer's actually holding cards -- during betting
		// (or briefly right after clearTable() sweeps the last round away)
		// there's nothing to show a total *of* yet, and drawing "0" there
		// unconditionally just left an empty-looking box sitting on the
		// board the whole time bets were being placed.
		if(dealer.hands[0].getHandSize() > 0){
			auto [dealerHard, dealerSoft] = dealer.hands[0].getShownTotals();
			drawTotalBox(state, dealerTotalBox, "DEALER HAND", dealerHard, dealerSoft);
		}

		// Only meaningful while a player is actually taking their turn --
		// once activePlayer reaches numberOfPlayers, play has moved on to
		// the dealer and there's no "current active hand" to show. Also
		// guard getActiveHand() itself: clearTable() resets activePlayer
		// back to 0 the instant the dealer finishes, but that player's own
		// hands/activeHand stay stale (often one past the end, from their
		// last stand/bust) until their cards finish discarding one at a
		// time over the following frames -- reading hands[activeHand]
		// during that window is exactly the out-of-range crash this hit.
		if(!awaitingBets && activePlayer < numberOfPlayers){
			Person& p = players[activePlayer];
			int hand = p.getActiveHand();
			if(hand < p.hands.size()){
				auto [hard, soft] = p.hands[hand].getShownTotals();
				drawTotalBox(state, activeHandTotalBox, "ACTIVE HAND", hard, soft);
			}
		}
	}

	// Only worth showing (and tappable) when there's actually a hand to
	// give advice on -- reuses the exact same situation getStrategySituation()
	// already provides for highlighting the full chart.
	void drawQuickTipButton(SDLState& state){
		int section, row, col;
		if(awaitingBets || !getStrategySituation(section, row, col))
			return;

		SDL_FRect btn = quickTipButton();
		SDL_SetRenderDrawColor(state.renderer, showQuickTip ? 90 : 70, showQuickTip ? 150 : 70, showQuickTip ? 110 : 140, 255);
		SDL_RenderFillRect(state.renderer, &btn);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &btn);

		std::string label = "TIP";
		float pixel = 6.0f;
		float maxW = btn.w - 8.0f;
		float w = DigitFont::textWidth(label, pixel);
		if(w > maxW && w > 0.0f)
			pixel *= maxW / w;
		w = DigitFont::textWidth(label, pixel);
		DigitFont::drawText(state, label, btn.x + (btn.w - w) / 2.0f, btn.y + (btn.h - 5 * pixel) / 2.0f, pixel, SDL_Color{255, 255, 255, 255});
	}

	// The actual popup: just the one relevant row (hard/soft/pairs, per
	// getStrategySituation()) instead of StrategyChart's full 3-table
	// screen -- a glance, not a lookup. Closes itself the moment there's
	// no longer a valid situation to show (hand resolved, turn moved on),
	// same as the button itself.
	bool activeHandDoubled(){
		if(activePlayer >= numberOfPlayers)
			return false;
		Person& p = players[activePlayer];
		int h = p.getActiveHand();
		return h < p.hands.size() && p.hands[h].getDoubleCount() > 0;
	}

	// The chart's move for a hand that's already been doubled (Player's
	// Edge only -- elsewhere a double ends the hand): hitting isn't
	// allowed any more, so H becomes S; D means redouble while doubles
	// are left; R means double-down rescue (see canSurrenderActiveHand()).
	char doubledAdvice(char action){
		if(action == 'H')
			return 'S';
		if(action == 'D' && !canDoubleActiveHand())
			return 'S';
		return action;
	}

	void drawQuickTip(SDLState& state){
		// Once turned on, TIP stays on from hand to hand until it's turned
		// off or every player's hand has been played (the dealer's turn);
		// between hands -- a card still landing -- it just isn't drawn.
		if(!showQuickTip)
			return;
		if(awaitingBets || activePlayer >= numberOfPlayers){
			showQuickTip = false;
			return;
		}
		int section, row, col;
		if(!getStrategySituation(section, row, col))
			return;

		float cellW = 46.0f, cellH = 40.0f, gap = 5.0f;
		float stride = cellW + gap;
		// 10 cells with a gap *between* each (9 gaps, not 10) -- the loop
		// below never draws a trailing gap after the last column, so
		// 10*stride overcounted the real content width by one gap's worth,
		// leaving the right-side padding 5px wider than the left's.
		float tableW = 10 * cellW + 9 * gap;

		// No row-label column here (unlike StrategyChart.h's full table) --
		// the row's already named in the title above ("HARD TOTALS 16"),
		// so reserving space for one left an empty 60px gap that made the
		// whole grid sit well left of center inside the box, even though
		// the box itself was centered on screen.
		float boxW = tableW + 40.0f;
		// Was cellH + 90 with rowY (below) at boxY+40 -- the title (at
		// boxY+12, 30 tall at titlePixel 6) actually ended at boxY+42,
		// *past* where the header row started, hence the overlap. +104
		// instead of +90 gives rowY the extra room it needs (see below)
		// while keeping the same bottom padding under the cells.
		float boxH = cellH + 104.0f;
		float boxX = 720.0f - boxW / 2.0f;
		// Was 420 -- sat squarely over the active hand's own cards, which
		// is exactly what a player wants visible while deciding what to
		// do with them. Moved up over the chip tray/shoe area instead (75
		// clears the CNT toggle button now sitting right above the
		// discard pile, y 21-69).
		float boxY = 75.0f;

		SDL_SetRenderDrawColor(state.renderer, 10, 30, 15, 245);
		SDL_FRect bg{ .x = boxX, .y = boxY, .w = boxW, .h = boxH };
		SDL_RenderFillRect(state.renderer, &bg);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &bg);

		std::string title = std::string(StrategyChart::sectionTitle(section)) + " " + StrategyChart::rowLabel(section, row);
		float titlePixel = 6.0f;
		float titleW = DigitFont::textWidth(title, titlePixel);
		DigitFont::drawText(state, title, 720.0f - titleW / 2.0f, boxY + 12.0f, titlePixel, SDL_Color{255, 255, 255, 255});

		// Was boxY + 40 -- the title's own bottom edge (boxY+42, see boxH's
		// comment above) sat past this, so the header row started before
		// the title even finished. +54 actually clears it.
		float rowY = boxY + 54.0f;
		float tableX = boxX + 20.0f;
		const char* data = StrategyChart::rowData(section, row);

		for(int c = 0; c < 10; c++){
			SDL_FRect headerRect{ .x = tableX + c * stride, .y = rowY, .w = cellW, .h = 22.0f };
			SDL_SetRenderDrawColor(state.renderer, 40, 60, 45, 255);
			SDL_RenderFillRect(state.renderer, &headerRect);
			SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
			SDL_RenderRect(state.renderer, &headerRect);
			std::string colLabel = StrategyChart::dealerColLabel(c);
			float lw = DigitFont::textWidth(colLabel, 4.0f);
			DigitFont::drawText(state, colLabel, headerRect.x + (cellW - lw) / 2.0f, headerRect.y + (22.0f - 5 * 4.0f) / 2.0f, 4.0f, SDL_Color{255, 255, 255, 255});

			char action = activeHandDoubled() ? doubledAdvice(data[c]) : data[c];
			bool hl = (c == col);
			SDL_Color fill = hl ? SDL_Color{255, 225, 60, 255} : StrategyChart::colorFor(action);
			SDL_Color textColor = hl ? SDL_Color{20, 20, 20, 255} : SDL_Color{255, 255, 255, 255};

			SDL_FRect cellRect{ .x = tableX + c * stride, .y = rowY + 26.0f, .w = cellW, .h = cellH };
			SDL_SetRenderDrawColor(state.renderer, fill.r, fill.g, fill.b, fill.a);
			SDL_RenderFillRect(state.renderer, &cellRect);
			SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
			SDL_RenderRect(state.renderer, &cellRect);

			std::string text(1, action);
			float pixel = 8.0f;
			float tw = DigitFont::textWidth(text, pixel);
			DigitFont::drawText(state, text, cellRect.x + (cellW - tw) / 2.0f, cellRect.y + (cellH - 5 * pixel) / 2.0f, pixel, textColor);

			if(hl){
				SDL_SetRenderDrawColor(state.renderer, 20, 20, 20, 255);
				SDL_FRect outer{ .x = cellRect.x - 2, .y = cellRect.y - 2, .w = cellRect.w + 4, .h = cellRect.h + 4 };
				SDL_RenderRect(state.renderer, &outer);
			}
		}
	}

	// Above each hand, once resolveRound() has settled it: WIN/PUSH/LOSE.
	// Naturally stops showing once a hand's cards start actually leaving
	// the table (getHandSize() drops to 0 as each one's individual discard
	// animation lands) and resets to None on its own once a fresh Hand
	// exists for the next round -- no explicit clearing needed either way.
	void drawHandResults(SDLState& state){
		for(int i = 0; i < numberOfPlayers; i++){
			for(Hand& hand : players[i].hands){
				if(hand.getHandSize() == 0)
					continue;
				if(hand.getResult() == HandResult::None){
					// Shown while a busted/surrendered hand waits to be
					// swept to the discard pile.
					if(hand.isBust())
						drawResultBanner(state, hand, hand.isSurrendered() ? "SURRENDER" : "BUST", SDL_Color{230, 70, 70, 255});
					continue;
				}

				std::string text;
				SDL_Color color;
				switch(hand.getResult()){
					case HandResult::Win:  text = "WIN";  color = SDL_Color{80, 220, 100, 255}; break;
					case HandResult::Push: text = "PUSH"; color = SDL_Color{230, 230, 230, 255}; break;
					case HandResult::Loss: text = "LOSE"; color = SDL_Color{230, 70, 70, 255}; break;
					default: continue;
				}

				drawResultBanner(state, hand, text, color);
			}
		}

		// Dealer BUST, mirrored from the player WIN/PUSH/LOSE banners --
		// drawResultBanner() itself doesn't care whose hand it is. Shown
		// once the hole card's actually revealed (before that, the total
		// isn't final -- an unrevealed hole card could still bring it back
		// under 21) and the hand's still on the table, same "stops once
		// cards start leaving" behavior as the player ones.
		Hand& dealerHand = dealer.hands[0];
		if(dealerHand.getHandSize() > 0 && dealerHand.cards.size() >= 2
				&& dealerHand.cards[1].getShown() && dealerHand.getHandTotal() > 21){
			drawResultBanner(state, dealerHand, "BUST", SDL_Color{230, 70, 70, 255});
		}
	}

	// A solid-black, fully opaque banner laid diagonally across the hand:
	// from the first card's bottom-left corner to the last card's
	// top-right corner, both corners rotated by the card's own actual
	// on-table rotation (matches SDL_RenderTextureRotated's own
	// top-left-pivot, clockwise-in-Y-down convention -- the same one
	// already worked out for betRow()'s seat footprints).
	// DigitFont/SDL_RenderFillRect can't rotate directly, so the label is
	// first drawn to a small off-screen texture at its natural size, then
	// that whole texture is rotated in one shot with the same primitive
	// already used to draw every card.
	void drawResultBanner(SDLState& state, Hand& hand, const std::string& text, SDL_Color color){
		constexpr float PI = 3.14159265358979323846f;

		Card& first = hand.cards.front();
		Card& last = hand.cards.back();
		float rad = first.getRotation() * PI / 180.0f;
		float cosT = std::cos(rad), sinT = std::sin(rad);

		SDL_FPoint firstPos = first.getPosition();
		SDL_FPoint lastPos = last.getPosition();

		// Local corner (0, cardHeight) = bottom-left, before rotation.
		SDL_FPoint start{
			firstPos.x + (-cardHeight * sinT),
			firstPos.y + (cardHeight * cosT)
		};

		// Local corner (cardWidth, 0) = top-right, before rotation.
		SDL_FPoint end{
			lastPos.x + (cardWidth * cosT),
			lastPos.y + (cardWidth * sinT)
		};

		float angleDeg = std::atan2(end.y - start.y, end.x - start.x) * 180.0f / PI;

		// atan2 gives the direction of an infinite line, which is
		// identical at angle and angle+180 -- but rotated TEXT very much
		// isn't, since one of those two readings is upside down. For some
		// seats (dir==1 in particular) the raw start->end angle lands well
		// past vertical, which flips the text; wrapping into (-90,90]
		// keeps the same diagonal line but always the upright reading of
		// it. (An upside-down "N" happens to be pixel-identical to this
		// font's "M" -- that's what "WIN" rendering like "WIX" actually was.)
		if(angleDeg > 90.0f)
			angleDeg -= 180.0f;
		else if(angleDeg < -90.0f)
			angleDeg += 180.0f;

		float pixel = 6.0f;
		float padX = 8.0f, padY = 6.0f;
		int texW = (int)(DigitFont::textWidth(text, pixel) + padX * 2.0f);
		int texH = (int)(5.0f * pixel + padY * 2.0f);

		SDL_Texture* label = SDL_CreateTexture(state.renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, texW, texH);
		SDL_SetRenderTarget(state.renderer, label);
		SDL_SetRenderDrawColor(state.renderer, 0, 0, 0, 255);
		SDL_RenderClear(state.renderer);
		DigitFont::drawText(state, text, padX, padY, pixel, color);
		SDL_SetRenderTarget(state.renderer, nullptr);

		SDL_FPoint mid{ (start.x + end.x) / 2.0f, (start.y + end.y) / 2.0f };
		SDL_FRect dst{ .x = mid.x - texW / 2.0f, .y = mid.y - texH / 2.0f, .w = (float)texW, .h = (float)texH };
		SDL_FPoint pivot{ texW / 2.0f, texH / 2.0f };
		SDL_RenderTextureRotated(state.renderer, label, nullptr, &dst, angleDeg, &pivot, SDL_FLIP_NONE);

		SDL_DestroyTexture(label);
	}

	void drawTotalBox(SDLState& state, const SDL_FRect& box, const std::string& label, int hard, int soft){
		// The label ("DEALER HAND"/"ACTIVE HAND") is always wider than the
		// box itself -- floats centered above it rather than being clipped
		// to box.w -- so it's auto-shrunk against a fixed budget instead of
		// box.w, just enough to keep it from running off the canvas edge.
		float labelPixel = 4.0f;
		float maxLabelW = 200.0f;
		float labelW = DigitFont::textWidth(label, labelPixel);
		if(labelW > maxLabelW && labelW > 0.0f)
			labelPixel *= maxLabelW / labelW;
		labelW = DigitFont::textWidth(label, labelPixel);
		float labelX = box.x + (box.w - labelW) / 2.0f;
		float labelY = box.y - (5 * labelPixel) - 6.0f;
		DigitFont::drawText(state, label, labelX, labelY, labelPixel, SDL_Color{255, 255, 255, 255});

		SDL_SetRenderDrawColor(state.renderer, 10, 40, 20, 230);
		SDL_RenderFillRect(state.renderer, &box);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &box);

		// Only worth showing both numbers when they actually differ --
		// otherwise (no ace among the shown cards, or the soft total would
		// bust) hard == soft and there's just one true total.
		std::string text = (soft != hard)
			? std::to_string(hard) + "/" + std::to_string(soft)
			: std::to_string(hard);

		float pixel = 6.0f;
		float maxTextW = box.w - 10.0f;
		float textW = DigitFont::textWidth(text, pixel);
		if(textW > maxTextW && textW > 0.0f)
			pixel *= maxTextW / textW;
		textW = DigitFont::textWidth(text, pixel);
		float textX = box.x + (box.w - textW) / 2.0f;
		float textY = box.y + (box.h - 5 * pixel) / 2.0f;

		DigitFont::drawText(state, text, textX, textY, pixel, SDL_Color{255, 255, 255, 255});
	}

	// Betting phase -- runs before every round (including the first one,
	// see startGame()). Each active player gets one raise/lower button
	// (not five -- see chipIndex) flanking their bet total, anchored below
	// their seat, with a small chip-size selector directly above the total
	// that cycles which BET_DENOMS entry those two buttons actually use.
	// DEAL sits where the dealer's own cards actually land. A player who
	// bet 0 sits the round out entirely -- see beginRound()/firstDeal().
	static constexpr int BET_DENOMS[5] = {1, 5, 25, 100, 500};
	static constexpr float BET_BTN_W = 56;
	static constexpr float BET_BTN_H = 48;
	static constexpr float BET_GAP = 8;
	static constexpr float BET_TOTAL_W = 110;
	static constexpr float BET_ROW_W = BET_BTN_W + BET_GAP + BET_TOTAL_W + BET_GAP + BET_BTN_W;

	// At least as big as BET_BTN_W/H (the bet lower/raise buttons) -- any
	// smaller and it's hard to tap accurately, which is exactly what this
	// selector was before.
	static constexpr float SEL_BTN_W = 56;
	static constexpr float SEL_BTN_H = 48;
	static constexpr float SEL_VALUE_W = 72;
	static constexpr float SEL_GAP = 6;
	static constexpr float SEL_W = SEL_BTN_W + SEL_GAP + SEL_VALUE_W + SEL_GAP + SEL_BTN_W;

	struct BetRow{ SDL_FRect lower, total, raise, selMinus, selValue, selPlus; };

	// Centered on the seat's actual on-screen card footprint, not its raw
	// anchor point (bettingSquare.firstPoint(), the *unrotated* card rect's
	// top-left corner -- SDL_RenderTextureRotated pivots each card around
	// that same corner, via rotationTopLeft{0,0}). What that footprint
	// actually looks like depends on which way the seat's rotated:
	//   dir==0  (0 deg):   card sits right+down of anchor  -- normal.
	//   dir==1  (-90 deg): card sits right+UP of anchor     -- anchor.y is
	//                       already the card's *bottom* edge.
	//   dir==-1 (+90 deg): card sits LEFT+down of anchor    -- anchor.x is
	//                       already the card's *right* edge.
	// Both axes are clamped to the screen bounds -- the bottom seat's
	// natural "just below the hand" position ran the row off the bottom
	// of the 720-tall canvas entirely before this.
	// The 2 new 5-player seats reuse dir=1/dir=-1's bucket verbatim here,
	// same as the original seat that bucket belongs to -- P2 (dir=1's new
	// seat) stays on the untouched shared formula per explicit request.
	// P4 (dir=-1's own new seat, picked out by its non-cardinal rotation)
	// gets a real, checked-against-everything-else placement instead: its
	// shared-formula spot (centerX ~185, belowY 500) put its own BET row's
	// bottom edge (~624) a few pixels into the "DEALER HAND" label's own
	// space (label top ~619, box at x 40-170/y 645-695) -- moving it up
	// enough to clear that landed it overlapping P4's *own* card box
	// instead (x ~155-330/y ~390-580), since at this X they occupy the
	// same vertical band. That first spot (centerX 460, belowY 400) was
	// checked against P4's own card box, P5's row, and the dealer hand
	// corner -- nudged further since, to taste (currently centerX 270,
	// belowY 435).
	bool isP4Seat(int i, int dir){
		return dir == -1 && players[i].getSeatRotation() != 90.0f;
	}

	float seatCenterX(int i){
		Point anchor = players[i].getSeatAnchor();
		int dir = players[i].getDirection();

		if(isP4Seat(i, dir))
			return 270.0f;

		if(dir == 0)
			return anchor.x + cardWidth / 2.0f;
		if(dir == 1)
			return anchor.x + cardHeight / 2.0f;
		return anchor.x - cardHeight / 2.0f;
	}

	BetRow betRow(int i){
		Point anchor = players[i].getSeatAnchor();
		int dir = players[i].getDirection();

		float centerX = seatCenterX(i);
		float belowY;
		if(dir == 0){
			belowY = anchor.y + cardHeight;
		} else if(dir == 1){
			belowY = anchor.y;
		} else if(isP4Seat(i, dir)){
			belowY = 435.0f;
		} else{
			belowY = anchor.y + cardWidth;
		}

		float rowX = centerX - BET_ROW_W / 2.0f;
		float blockH = SEL_BTN_H + 8.0f + BET_BTN_H;
		float blockY = belowY + 20.0f;

		rowX = std::max(10.0f, std::min(rowX, 1440.0f - BET_ROW_W - 10.0f));
		blockY = std::max(10.0f, std::min(blockY, 720.0f - blockH - 10.0f));

		float selY = blockY;
		float rowY = blockY + SEL_BTN_H + 8.0f;

		float totalX = rowX + BET_BTN_W + BET_GAP;
		float raiseX = totalX + BET_TOTAL_W + BET_GAP;

		BetRow row{};
		row.lower = SDL_FRect{ .x = rowX, .y = rowY, .w = BET_BTN_W, .h = BET_BTN_H };
		row.total = SDL_FRect{ .x = totalX, .y = rowY, .w = BET_TOTAL_W, .h = BET_BTN_H };
		row.raise = SDL_FRect{ .x = raiseX, .y = rowY, .w = BET_BTN_W, .h = BET_BTN_H };

		// Directly above the total, not below it or off to the side.
		float selX = totalX + (BET_TOTAL_W - SEL_W) / 2.0f;
		row.selMinus = SDL_FRect{ .x = selX, .y = selY, .w = SEL_BTN_W, .h = SEL_BTN_H };
		row.selValue = SDL_FRect{ .x = selX + SEL_BTN_W + SEL_GAP, .y = selY, .w = SEL_VALUE_W, .h = SEL_BTN_H };
		row.selPlus = SDL_FRect{ .x = selX + SEL_BTN_W + SEL_GAP + SEL_VALUE_W + SEL_GAP, .y = selY, .w = SEL_BTN_W, .h = SEL_BTN_H };

		return row;
	}

	struct SideBetRow{ SDL_FRect selMinus, selValue, selPlus; };

	// A side-bet amount selector -- same SEL_* sizing/shape as betRow()'s
	// own chip-size selector, per the "a selector just like the hand does"
	// ask, but stacked *above* the whole existing bet block instead of
	// being part of it. betIndex 0 sits directly above the chip-size
	// selector; betIndex 1 (Player's Edge's second side bet, Match Down)
	// stacks one more block above that. Horizontal position is borrowed
	// straight from betRow() so both selectors stay centered on the same
	// column.
	// Each block reserves SEL_BTN_H (the row itself) + 8 (gap) -- the
	// resolved-outcome readout lives inside the row's own value box (see
	// drawSideBetSelector()), not a separate line underneath, so no extra
	// height is needed here for it.
	static constexpr float SIDE_BET_BLOCK_H = SEL_BTN_H + 8.0f;

	SideBetRow sideBetRow(int i, int betIndex){
		BetRow row = betRow(i);
		float y = row.selMinus.y - SIDE_BET_BLOCK_H * (betIndex + 1);

		SideBetRow r{};
		r.selMinus = SDL_FRect{ .x = row.selMinus.x, .y = y, .w = SEL_BTN_W, .h = SEL_BTN_H };
		r.selValue = SDL_FRect{ .x = row.selValue.x, .y = y, .w = SEL_VALUE_W, .h = SEL_BTN_H };
		r.selPlus  = SDL_FRect{ .x = row.selPlus.x,  .y = y, .w = SEL_BTN_W, .h = SEL_BTN_H };
		return r;
	}

	// Centered on the table itself -- the dealer's own seat (xs[5]=1058,
	// ys[5]=423) isn't actually at the table's visual center, so anchoring
	// on it instead of the true canvas center kept landing off to one
	// side. 1440x720 is the fixed logical canvas size (see main()'s
	// SDL_SetRenderLogicalPresentation call), so its center never moves.
	//
	// On touch screens it goes in the bottom-right corner instead, where a
	// right thumb rests (setTouchLayout(), from mina.cpp).
	SDL_FRect dealButton(){
		float w = 220.0f, h = 80.0f;
		if(touchLayout)
			return SDL_FRect{ .x = 1440.0f - w - 16.0f, .y = 720.0f - 90.0f - 12.0f, .w = w, .h = 90.0f };
		return SDL_FRect{ .x = 1440.0f / 2.0f - w / 2.0f, .y = 720.0f / 2.0f - h / 2.0f, .w = w, .h = h };
	}

	bool touchLayout = false;

public:
	void setTouchLayout(bool touch){ touchLayout = touch; }

private:

	void drawButton(SDLState& state, const SDL_FRect& rect, SDL_Color color){
		SDL_SetRenderDrawColor(state.renderer, color.r, color.g, color.b, color.a);
		SDL_RenderFillRect(state.renderer, &rect);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &rect);
	}

	void drawDenomButton(SDLState& state, const SDL_FRect& rect, int denom, bool isRaise){
		drawButton(state, rect, isRaise ? SDL_Color{40, 110, 50, 255} : SDL_Color{120, 50, 50, 255});

		std::string text = std::to_string(denom);
		float pixel = 4.0f;
		float maxW = rect.w - 8.0f;
		float w = DigitFont::textWidth(text, pixel);
		if(w > maxW && w > 0.0f)
			pixel *= maxW / w;
		w = DigitFont::textWidth(text, pixel);
		DigitFont::drawText(state, text, rect.x + (rect.w - w) / 2.0f, rect.y + (rect.h - 5 * pixel) / 2.0f, pixel, SDL_Color{255, 255, 255, 255});
	}

	// A bankrupt seat (bankroll == 0 -- raiseBet()/raiseSideBet() already
	// clamp every wager to bankroll, so this is the only way a seat can
	// ever end up unable to bet anything at all) gets this instead of the
	// normal bet row -- there's nothing to raise/lower when there's
	// nothing to wager with. Reuses the same rect the bet row's lower/
	// total/raise buttons would occupy, just as one single wide button.
	SDL_FRect buyInButton(int i){
		BetRow row = betRow(i);
		return SDL_FRect{ .x = row.lower.x, .y = row.lower.y, .w = BET_ROW_W, .h = BET_BTN_H };
	}

	void drawBuyInButton(SDLState& state, int i){
		SDL_FRect btn = buyInButton(i);
		drawButton(state, btn, SDL_Color{150, 110, 40, 255});

		std::string label = "BUY IN";
		float pixel = 5.0f;
		float maxW = btn.w - 8.0f;
		float w = DigitFont::textWidth(label, pixel);
		if(w > maxW && w > 0.0f)
			pixel *= maxW / w;
		w = DigitFont::textWidth(label, pixel);
		DigitFont::drawText(state, label, btn.x + (btn.w - w) / 2.0f, btn.y + (btn.h - 5 * pixel) / 2.0f, pixel, SDL_Color{255, 255, 255, 255});
	}

	void drawBetRow(SDLState& state, int i){
		BetRow row = betRow(i);
		int denom = BET_DENOMS[chipIndex[i]];

		// BET labels the lower/total/raise row, running down its left side
		// -- there's no clear strip above it to put a horizontal label in
		// (the chip-size selector already sits directly above), so this
		// mirrors the side-bet selectors' vertical side-label style instead.
		// Was offset by labelPixel*4 (the old 3-wide font's full stride) --
		// the new font's glyphs are 5 columns wide, so that offset no
		// longer cleared the glyph's own width from the anchor point and
		// the label ran into the button next to it instead of sitting
		// cleanly to its left.
		float betLabelPixel = 3.0f;
		float betLabelH = DigitFont::verticalTextHeight("BET", betLabelPixel);
		DigitFont::drawVerticalText(state, "BET", row.lower.x - betLabelPixel * 6.0f, row.lower.y + (BET_BTN_H - betLabelH) / 2.0f, betLabelPixel, SDL_Color{220, 220, 220, 255});

		drawDenomButton(state, row.lower, denom, false);

		SDL_SetRenderDrawColor(state.renderer, 10, 40, 20, 230);
		SDL_RenderFillRect(state.renderer, &row.total);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &row.total);

		std::string betText = std::to_string(players[i].getBet());
		float pixel = 5.0f;
		float maxBetW = row.total.w - 8.0f;
		float w = DigitFont::textWidth(betText, pixel);
		if(w > maxBetW && w > 0.0f)
			pixel *= maxBetW / w;
		w = DigitFont::textWidth(betText, pixel);
		DigitFont::drawText(state, betText, row.total.x + (row.total.w - w) / 2.0f, row.total.y + (row.total.h - 5 * pixel) / 2.0f, pixel, SDL_Color{255, 255, 255, 255});

		drawDenomButton(state, row.raise, denom, true);

		// Chip-size selector, directly above the bet total -- CHP (not the
		// full "CHIP": 4 stacked letters would run taller than the row
		// itself) labeled down its left side, same as BET above.
		// Deliberately not a horizontal label above the row: when this
		// mode also has a side bet, sideBetRow() stacks its own row(s)
		// directly above this one with no gap to spare for a caption there.
		// Offset by labelPixel*6 (a full stride at the new 5-wide font),
		// not *4 -- see the BET label's comment above.
		float chipLabelPixel = 3.0f;
		float chipLabelH = DigitFont::verticalTextHeight("CHP", chipLabelPixel);
		DigitFont::drawVerticalText(state, "CHP", row.selMinus.x - chipLabelPixel * 6.0f, row.selMinus.y + (SEL_BTN_H - chipLabelH) / 2.0f, chipLabelPixel, SDL_Color{220, 220, 220, 255});

		drawButton(state, row.selMinus, SDL_Color{80, 80, 80, 255});
		float symPixel = 4.0f;
		DigitFont::drawText(state, "-", row.selMinus.x + (SEL_BTN_W - 3 * symPixel) / 2.0f, row.selMinus.y + (SEL_BTN_H - 5 * symPixel) / 2.0f, symPixel, SDL_Color{255, 255, 255, 255});

		SDL_SetRenderDrawColor(state.renderer, 10, 40, 20, 230);
		SDL_RenderFillRect(state.renderer, &row.selValue);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &row.selValue);
		std::string denomText = std::to_string(denom);
		float dPixel = 4.0f;
		float dw = DigitFont::textWidth(denomText, dPixel);
		DigitFont::drawText(state, denomText, row.selValue.x + (row.selValue.w - dw) / 2.0f, row.selValue.y + (row.selValue.h - 5 * dPixel) / 2.0f, dPixel, SDL_Color{255, 255, 255, 255});

		drawButton(state, row.selPlus, SDL_Color{80, 80, 80, 255});
		DigitFont::drawText(state, "+", row.selPlus.x + (SEL_BTN_W - 3 * symPixel) / 2.0f, row.selPlus.y + (SEL_BTN_H - 5 * symPixel) / 2.0f, symPixel, SDL_Color{255, 255, 255, 255});
	}

	// A side-bet amount readout -- deliberately NOT the same look as
	// drawBetRow()'s chip-size selector (neutral gray -/+ buttons around a
	// dark green value), which read as "just another chip selector" and
	// was easy to mistake for it. Instead: a short label running down the
	// left side in vertical text (so it doesn't need its own horizontal
	// strip above the row, and reads as a tag on the control rather than a
	// caption floating over it), and the -/+ buttons themselves tinted the
	// same theme color as the value box instead of neutral gray, so the
	// whole row reads as one colored unit at a glance.
	void drawSideBetSelector(SDLState& state, const SideBetRow& row, const std::string& label, int amount, SDL_Color themeColor, HandResult result = HandResult::None){
		SDL_Color dim{
			static_cast<Uint8>(themeColor.r * 0.6f),
			static_cast<Uint8>(themeColor.g * 0.6f),
			static_cast<Uint8>(themeColor.b * 0.6f),
			255
		};

		// Offset by labelPixel*6 (a full stride at the new 5-wide font),
		// not *4 -- see drawBetRow()'s BET label comment for why.
		float labelPixel = 3.0f;
		float labelH = DigitFont::verticalTextHeight(label, labelPixel);
		DigitFont::drawVerticalText(state, label, row.selMinus.x - labelPixel * 6.0f, row.selMinus.y + (SEL_BTN_H - labelH) / 2.0f, labelPixel, themeColor);

		drawButton(state, row.selMinus, dim);
		float symPixel = 4.0f;
		DigitFont::drawText(state, "-", row.selMinus.x + (SEL_BTN_W - 3 * symPixel) / 2.0f, row.selMinus.y + (SEL_BTN_H - 5 * symPixel) / 2.0f, symPixel, SDL_Color{255, 255, 255, 255});

		// The value box always shows the wager amount -- never overwritten
		// with WIN/LOSE text, so the player can still see what they bet
		// after the fact -- just tinted green/red once
		// resolveSideBets()/resolveRound() actually settle it
		// (sideBetResult/matchUpResult/matchDownResult), instead of a win/
		// loss only being visible as a chip silently flying to or from the
		// tray.
		SDL_Color valueFill = themeColor;
		std::string valueText = std::to_string(amount);
		if(result == HandResult::Win){
			valueFill = SDL_Color{40, 150, 70, 255};
		} else if(result == HandResult::Loss){
			valueFill = SDL_Color{150, 50, 50, 255};
		} else if(result == HandResult::Push){
			// Only Lucky Stiff's pending-hand tier can push -- a neutral
			// tint distinct from both the win/loss colors and the theme
			// color a still-live bet shows.
			valueFill = SDL_Color{140, 140, 140, 255};
		}

		SDL_SetRenderDrawColor(state.renderer, valueFill.r, valueFill.g, valueFill.b, valueFill.a);
		SDL_RenderFillRect(state.renderer, &row.selValue);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &row.selValue);
		float dPixel = 4.0f;
		float dw = DigitFont::textWidth(valueText, dPixel);
		DigitFont::drawText(state, valueText, row.selValue.x + (row.selValue.w - dw) / 2.0f, row.selValue.y + (row.selValue.h - 5 * dPixel) / 2.0f, dPixel, SDL_Color{255, 255, 255, 255});

		drawButton(state, row.selPlus, dim);
		DigitFont::drawText(state, "+", row.selPlus.x + (SEL_BTN_W - 3 * symPixel) / 2.0f, row.selPlus.y + (SEL_BTN_H - 5 * symPixel) / 2.0f, symPixel, SDL_Color{255, 255, 255, 255});
	}

	// Draws whichever side bet(s) the active game mode actually has for
	// this seat -- Lucky Ladies gets one selector labeled "LL" (purple),
	// Player's Edge gets two stacked ones labeled "UP"/"DN" (blue),
	// standard modes get none. Each shows its own WIN/LOSE readout once
	// resolveSideBets()/resolveRound() have actually settled it.
	void drawSideBetRows(SDLState& state, int i){
		if(hasLuckyLadies(gameMode)){
			drawSideBetSelector(state, sideBetRow(i, 0), "LL", players[i].getSideBet(), SDL_Color{150, 70, 170, 255}, sideBetResult[i]);
		} else if(isPlayersEdge(gameMode)){
			drawSideBetSelector(state, sideBetRow(i, 0), "UP", players[i].getMatchUpBet(), SDL_Color{50, 110, 170, 255}, matchUpResult[i]);
			drawSideBetSelector(state, sideBetRow(i, 1), "DN", players[i].getMatchDownBet(), SDL_Color{50, 110, 170, 255}, matchDownResult[i]);
		} else if(hasLuckyStiff(gameMode)){
			drawSideBetSelector(state, sideBetRow(i, 0), "LS", players[i].getSideBet(), SDL_Color{170, 100, 50, 255}, sideBetResult[i]);
		}
	}

	// Top of the board, on the brown border strip (y=0-41 there, see
	// Table.png) -- drawn unconditionally (not just during betting) so
	// it's visible through the whole round. Each player's bankroll is
	// positioned above their own seat via seatCenterX(), not clustered
	// into one centered string -- so it's legible at a glance whose
	// number is whose instead of a single "P1 500  P2 500  P3 500" line.
	void drawBankrolls(SDLState& state){
		// Was 5.0f, clamped only to the canvas edge -- the right seat's
		// bankroll sits close enough to the pause button (x 1352+) that
		// at the new font's width it ran under/past it instead of just
		// the canvas edge, hence the smaller size and the tighter
		// right-side clamp (1344, not 1440) below.
		float pixel = 3.5f;

		struct Label{ int playerIndex; float centerX, width, x; std::string text; SDL_Color color; };
		std::vector<Label> labels;
		for(int i = 0; i < numberOfPlayers; i++){
			int bankroll = players[i].getBankroll();
			int buyIn = players[i].getInitialBankroll();

			// Colored against the buy-in, not round to round -- red if
			// you're down overall, green if you're up, plain white if
			// you're exactly even.
			SDL_Color color{255, 255, 255, 255};
			if(bankroll < buyIn)
				color = SDL_Color{230, 80, 80, 255};
			else if(bankroll > buyIn)
				color = SDL_Color{90, 220, 110, 255};

			std::string text = "P" + std::to_string(i + 1) + " " + std::to_string(bankroll);
			float w = DigitFont::textWidth(text, pixel);
			labels.push_back(Label{i, seatCenterX(i), w, 0.0f, text, color});
		}

		// P2/P4 (the 2 new 5-player seats) sit right next to an original
		// seat (P1/P3 and P3/P5 respectively), close enough that their own
		// seatCenterX() fought that neighbor for space even after the
		// de-overlap sweep below evened things out. P1/P3/P5 (the original
		// 3 seats) keep their own natural position; P2/P4 instead get
		// placed exactly halfway between whichever 2 of those they sit
		// between.
		if(numberOfPlayers >= 5){
			auto centerOf = [&](int idx) -> float {
				for(const Label& l : labels)
					if(l.playerIndex == idx)
						return l.centerX;
				return 0.0f;
			};
			for(Label& label : labels){
				if(label.playerIndex == 1)
					label.centerX = (centerOf(0) + centerOf(2)) / 2.0f;
				else if(label.playerIndex == 3)
					label.centerX = (centerOf(2) + centerOf(4)) / 2.0f;
			}
		}

		// Sorted left-to-right, then swept the same way so no two labels
		// can land on top of each other -- with the 2 new 5-player seats,
		// the old "left" seat and the new bottom-left seat sit only ~24px
		// apart at their natural centers, well inside each other's ~120px-
		// wide label, which without this rendered as one garbled overlapped
		// mess instead of 2 readable labels.
		std::sort(labels.begin(), labels.end(), [](const Label& a, const Label& b){ return a.centerX < b.centerX; });
		float GAP = 10.0f;
		float prevRight = -1e9f;
		for(Label& label : labels){
			float x = std::max(10.0f, std::min(label.centerX - label.width / 2.0f, 1344.0f - label.width));
			if(x < prevRight + GAP)
				x = prevRight + GAP;
			label.x = x;
			prevRight = x + label.width;
		}

		float labelX[5]{};
		for(const Label& label : labels){
			labelX[label.playerIndex] = label.x;
			bankrollLabelCenter[label.playerIndex] = SDL_FPoint{ label.x + label.width / 2.0f, 15.0f + 5 * pixel / 2.0f };
			DigitFont::drawText(state, label.text, label.x, 15.0f, pixel, label.color);
		}

		// -$/+$ readouts, one line each, stacked directly under whichever
		// seat's bankroll they belong to -- reuses that seat's already
		// de-overlapped label x (not a fresh seatCenterX() computation) so
		// a readout never drifts out from under, or overlaps, its own
		// bankroll number. See queueBankrollChange().
		float changePixel = 3.0f;
		float changeY[5] = {15.0f + 5 * 3.5f + 4.0f, 15.0f + 5 * 3.5f + 4.0f, 15.0f + 5 * 3.5f + 4.0f, 15.0f + 5 * 3.5f + 4.0f, 15.0f + 5 * 3.5f + 4.0f};
		for(const BankrollChange& change : bankrollChanges){
			int i = change.playerIndex;
			if(i < 0 || i >= numberOfPlayers)
				continue;

			DigitFont::drawText(state, change.text, labelX[i], changeY[i], changePixel, change.color);
			changeY[i] += 5 * changePixel + 4.0f;
		}
	}

	// The shuffle, in the middle of the table: the gathered cards split
	// into two halves and riffle back together, twice, then the squared-up
	// deck slides into the shoe. Card backs only -- it's a pile of cards,
	// not anything the player needs to read.
	void drawShuffle(SDLState& state, Resources& res){
		constexpr float CX = 720.0f, CY = 285.0f;
		constexpr int LAYERS = 7;
		constexpr float SPREAD = 170.0f, RIFFLE = 1.15f, GATHER = 0.6f;
		auto pile = [&](float x, float y, int layers){
			for(int k = 0; k < layers; k++){
				SDL_FRect dst{ .x = x - cardWidth / 2.0f + k * 1.5f, .y = y - k * 2.0f, .w = cardWidth, .h = cardHeight };
				SDL_RenderTexture(state.renderer, res.allCards, &backOFCard, &dst);
			}
		};
		auto ease = [](float t){ t = std::clamp(t, 0.0f, 1.0f); return t * t * (3.0f - 2.0f * t); };

		float t = shuffleElapsed;
		float riffleEnd = SHUFFLE_DURATION - GATHER;
		if(t < riffleEnd){
			float c = std::fmod(t, RIFFLE) / RIFFLE;
			if(c < 0.25f){
				// split: the deck comes apart into two halves
				float off = SPREAD * ease(c / 0.25f);
				pile(CX - off, CY, LAYERS / 2 + 1);
				pile(CX + off, CY, LAYERS / 2 + 1);
			} else if(c < 0.8f){
				// riffle: cards drop alternately off each half onto the middle
				float r = (c - 0.25f) / 0.55f;
				int dropped = (int)(r * LAYERS * 2);
				int left = std::max(1, LAYERS - (dropped + 1) / 2), right = std::max(1, LAYERS - dropped / 2);
				pile(CX - SPREAD, CY, left);
				pile(CX + SPREAD, CY, right);
				pile(CX, CY, std::max(1, dropped / 2));
				// the card in the air
				float f = std::fmod(r * LAYERS * 2, 1.0f);
				float fromX = (dropped % 2 == 0) ? CX - SPREAD : CX + SPREAD;
				SDL_FRect dst{ .x = fromX + (CX - fromX) * f - cardWidth / 2.0f, .y = CY - 20.0f * std::sin(f * 3.14159f), .w = cardWidth, .h = cardHeight };
				double angle = (dropped % 2 == 0 ? -8.0 : 8.0) * (1.0 - f);
				SDL_RenderTextureRotated(state.renderer, res.allCards, &backOFCard, &dst, angle, nullptr, SDL_FLIP_NONE);
			} else{
				// square up the riffled deck
				pile(CX, CY, LAYERS);
			}
		} else{
			// slide the squared deck over into the shoe
			float g = ease((t - riffleEnd) / GATHER);
			float x = CX + (shoePosition.x + cardWidth / 2.0f - CX) * g;
			float y = CY + (shoePosition.y - CY) * g;
			pile(x, y, LAYERS);
		}
	}

	// A spot on the table in a seat's own frame -- the first card's
	// top-left is the origin, cards run +x, the dealer is -y -- for hand
	// number `hand` (split hands step sideways, see Person::calcOffset()),
	// turned into screen coordinates the way the seat's cards are rotated.
	SDL_FPoint seatPoint(int playerIndex, int hand, float localX, float localY){
		constexpr float PI = 3.14159265358979323846f;
		offSets adj = players[playerIndex].calcOffset();
		float rad = adj.rotation * PI / 180.0f;
		float cosT = std::cos(rad), sinT = std::sin(rad);
		Point anchor = players[playerIndex].getSeatAnchor();
		return SDL_FPoint{
			anchor.x + adj.xMoveHand * hand + (localX * cosT - localY * sinT),
			anchor.y + adj.yMoveHand * hand + (localX * sinT + localY * cosT)
		};
	}

	// How far a betting spot's outer border sits out from its first card.
	static constexpr float SPOT_MARGIN = 12.0f;

	// A stack of chips for an amount, broken into BET_DENOMS (bigger chips
	// at the bottom), centered on (cx, cy) at its
	// base. `amounts` lets a stack be built in layers -- the original bet
	// and then each double on top of it.
	// The chips (BET_DENOMS indexes) an amount is made of, the way a
	// dealer would make it visible: starting from the biggest chip that
	// goes in at least twice, so a 25 bet is five 5s rather than one lone
	// chip. The same breakdown is drawn on the felt and taken from (or
	// put back into) the tray.
	static std::vector<int> chipsFor(int amount){
		std::vector<int> chips;
		int top = 4;
		while(top > 0 && amount < 2 * BET_DENOMS[top])
			top--;
		for(int d = top; d >= 0 && amount > 0; d--){
			while(amount >= BET_DENOMS[d] && chips.size() < 60){
				chips.push_back(d);
				amount -= BET_DENOMS[d];
			}
		}
		return chips;
	}

	void drawChipStack(SDLState& state, Resources& res, float cx, float cy, const std::vector<int>& amounts, float size = 36.0f){
		std::vector<int> chips;
		for(int amount : amounts){
			std::vector<int> part = chipsFor(amount);
			chips.insert(chips.end(), part.begin(), part.end());
		}
		if(chips.empty())
			return;
		// Side on, like the chip tray: each chip's edge (Chips.png's thin
		// strips) piled up from the base. Taller stacks pack tighter so
		// they stay a sensible height.
		float edgeH = std::min(6.0f, 48.0f / std::max<size_t>(1, chips.size()));
		float baseY = cy + 10.0f;
		for(size_t k = 0; k < chips.size(); k++){
			SDL_FRect src{ .x = chips[k] * CHIP_SRC_SIZE, .y = 0.0f, .w = CHIP_SRC_SIZE, .h = CHIP_EDGE_SRC_H };
			SDL_FRect dst{ .x = cx - size / 2.0f, .y = baseY - (k + 1) * edgeH, .w = size, .h = edgeH };
			SDL_RenderTexture(state.renderer, res.chips, &src, &dst);
		}
	}

	// Each seat's side-bet circles, printed on the felt like a real table:
	// cut into the top corners of the betting spot -- top right for the
	// first side bet (Lucky Ladies, Lucky Stiff, Match Up), top left for
	// a second one (Match Down). The bet sits in it as a stack of chips.
	// Once a round settles it, the circle turns green on a win, red on a
	// loss, grey on a push, until the next betting phase. "Top" is toward
	// the dealer, whatever way the seat faces.
	void drawSideBetCircles(SDLState& state, Resources& res){
		if(!hasAnySideBet(gameMode))
			return;

		for(int i = 0; i < numberOfPlayers; i++){
			// Mid-round, only seats that were dealt in.
			bool inRound = awaitingBets || players[i].getBet() > 0 || initialTwoCards[i].valid;
			if(isPlayersEdge(gameMode)){
				drawSideBetCircle(state, res, i, true, inRound ? players[i].getMatchUpBet() : 0, matchUpResult[i]);
				drawSideBetCircle(state, res, i, false, inRound ? players[i].getMatchDownBet() : 0, matchDownResult[i]);
			} else{
				drawSideBetCircle(state, res, i, true, inRound ? players[i].getSideBet() : 0, sideBetResult[i]);
			}
		}
	}

	void drawSideBetCircle(SDLState& state, Resources& res, int playerIndex, bool topRight, int amount, HandResult result){
		constexpr float RADIUS = 25.0f;
		if(awaitingBets || amount <= 0)
			result = HandResult::None;

		// Centered just past the spot's corner, so it cuts into the
		// border a little.
		float out = SPOT_MARGIN + 4.0f;
		SDL_FPoint c = seatPoint(playerIndex, 0, topRight ? cardWidth + out : -out, -out);

		SDL_FColor ring{0.92f, 0.92f, 0.85f, 1.0f};
		SDL_FColor felt{0.18f, 0.30f, 0.12f, 1.0f};
		if(result == HandResult::Win) felt = SDL_FColor{0.15f, 0.62f, 0.25f, 1.0f};
		else if(result == HandResult::Loss) felt = SDL_FColor{0.75f, 0.16f, 0.16f, 1.0f};
		else if(result == HandResult::Push) felt = SDL_FColor{0.45f, 0.45f, 0.45f, 1.0f};
		fillCircle(state, c.x, c.y, RADIUS + 2.0f, ring);
		fillCircle(state, c.x, c.y, RADIUS, felt);

		// Once settled, the chips are part of a ChipAnimation.
		if(amount > 0 && result == HandResult::None)
			drawChipStack(state, res, c.x, c.y + 4.0f, { amount });
	}

	// Each hand's main bet as chips on the bottom-left corner of its
	// betting spot. Split hands each get their own stack beside their
	// cards; a double stacks its chips on top of the original bet.
	void drawMainBetChips(SDLState& state, Resources& res){
		float out = SPOT_MARGIN + 4.0f;
		for(int i = 0; i < numberOfPlayers; i++){
			Person& p = players[i];
			if(awaitingBets){
				if(p.getBankroll() > 0 && p.getBet() > 0){
					SDL_FPoint c = seatPoint(i, 0, -out, cardHeight + out);
					drawChipStack(state, res, c.x, c.y, { p.getBet() });
				}
				continue;
			}
			for(int h = 0; h < (int)p.hands.size(); h++){
				Hand& hand = p.hands[h];
				int bet = hand.getBet();
				// Shown from the moment the bet's down -- before its first card
				// arrives too. Once settled (or a bust collected), the chips
				// are part of a ChipAnimation instead.
				if(bet <= 0 || hand.getResult() != HandResult::None || hand.isChipsCollected())
					continue;
				std::vector<int> layers;
				int doubles = hand.getDoubleCount();
				int base = bet >> doubles;
				layers.push_back(base);
				for(int k = 0; k < doubles; k++)
					layers.push_back(base << k);
				SDL_FPoint c = seatPoint(i, h, -out, cardHeight + out);
				drawChipStack(state, res, c.x, c.y, layers);
			}
		}
	}

	static void fillCircle(SDLState& state, float cx, float cy, float r, SDL_FColor color){
		constexpr int SEGMENTS = 28;
		constexpr float PI = 3.14159265358979323846f;
		SDL_Vertex verts[SEGMENTS + 2];
		verts[0] = SDL_Vertex{ SDL_FPoint{cx, cy}, color, SDL_FPoint{0, 0} };
		for(int k = 0; k <= SEGMENTS; k++){
			float a = 2.0f * PI * k / SEGMENTS;
			verts[k + 1] = SDL_Vertex{ SDL_FPoint{cx + r * std::cos(a), cy + r * std::sin(a)}, color, SDL_FPoint{0, 0} };
		}
		int indices[SEGMENTS * 3];
		for(int k = 0; k < SEGMENTS; k++){
			indices[k * 3] = 0;
			indices[k * 3 + 1] = k + 1;
			indices[k * 3 + 2] = k + 2;
		}
		SDL_RenderGeometry(state.renderer, nullptr, verts, SEGMENTS + 2, indices, SEGMENTS * 3);
	}

	// Chips.png is a 150x33 strip: 5 chips of 30x30 starting at y=3, with
	// each chip's edge-on slice in rows 0-2 (drawChipStack(), the tray).
	static constexpr float CHIP_SRC_SIZE = 30.0f;
	static constexpr float CHIP_SRC_Y = 3.0f;

	// A 7-column chip tray: 1s, two columns of 5s, two of 25s, one of
	// 100s, one of 500s. Column X positions were given directly; the Y
	// range (57 to 240 for every column -- NOT the alternating 57/67 first
	// given, which turned out to be an approximation) is measured straight
	// from Table.png's actual gray column art, so the stack sits flush
	// with its top and fills exactly to its bottom. Filled downward with
	// the *edge-on* chip slice -- Chips.png rows 0-2, a chip lying flat as
	// it would sit in a real tray (the same slices drawChipStack() uses).
	static constexpr int TRAY_COLUMN_DENOM[7] = {0, 1, 1, 2, 2, 3, 4};
	static constexpr float TRAY_COLUMN_X[7] = {586, 626, 666, 706, 746, 786, 826};
	static constexpr float TRAY_TOP_Y = 57.0f;
	static constexpr float TRAY_BOTTOM_Y = 240.0f;
	static constexpr float TRAY_SLICE_H = 4.0f;
	static constexpr int TRAY_FILL_COUNT = (int)((TRAY_BOTTOM_Y - TRAY_TOP_Y) / TRAY_SLICE_H);
	static constexpr float CHIP_EDGE_SRC_H = 3.0f;

	// How many layers each column currently shows -- starts full, drops
	// when a payout is queued (that chip's visibly leaving) and rises
	// when a collected (lost-bet) chip actually lands back (see
	// queueChipPayout()/update()'s chip-animation landing). Clamped to
	// [0, TRAY_FILL_COUNT] so a long losing/winning streak can't under-
	// or overflow a column.
	int trayFillCount[7] = {
		TRAY_FILL_COUNT, TRAY_FILL_COUNT, TRAY_FILL_COUNT, TRAY_FILL_COUNT,
		TRAY_FILL_COUNT, TRAY_FILL_COUNT, TRAY_FILL_COUNT
	};

	// One chip of BET_DENOMS[denom] out of / into the tray: from the
	// fullest column of that colour, into the emptiest.
	void takeFromTray(int denom){
		int best = -1;
		for(int c = 0; c < 7; c++)
			if(TRAY_COLUMN_DENOM[c] == denom && (best < 0 || trayFillCount[c] > trayFillCount[best]))
				best = c;
		if(best >= 0)
			trayFillCount[best] = std::max(0, trayFillCount[best] - 1);
	}

	void putInTray(int denom){
		int best = -1;
		for(int c = 0; c < 7; c++)
			if(TRAY_COLUMN_DENOM[c] == denom && (best < 0 || trayFillCount[c] < trayFillCount[best]))
				best = c;
		if(best >= 0)
			trayFillCount[best] = std::min(TRAY_FILL_COUNT, trayFillCount[best] + 1);
	}

	// First column whose denomination matches -- used both to pick which
	// column a payout/collection actually affects and, in drawChipTray(),
	// implicitly (every column already knows its own denom).
	int firstColumnForDenom(int denomIndex){
		for(int col = 0; col < 7; col++)
			if(TRAY_COLUMN_DENOM[col] == denomIndex)
				return col;
		return 0;
	}

	// Hi-Lo card counting: 2-6 count +1, 7-9 count 0, 10/J/Q/K/A count -1.
	// Tracked incrementally (see runningCount/addToRunningCount()) rather
	// than rescanned from currently-visible cards each frame -- a card
	// swept into the discard pile at round end gets forced back to
	// showCard(false) once it lands there (see update()'s discard branch,
	// which exists so the muck shows card backs, not faces), so a scan
	// that only trusted getShown() would silently "forget" every past
	// hand's cards and only ever reflect the one currently in progress.
	// Counting the moment a card is actually revealed instead means it
	// stays correct across the whole shoe regardless of what the discard
	// pile's cards currently render as.
	int hiLoValue(int cardValue){
		if(cardValue >= 2 && cardValue <= 6)
			return 1;
		if(cardValue >= 7 && cardValue <= 9)
			return 0;
		return -1; // 10, J, Q, K, A
	}

	void addToRunningCount(int cardValue){
		runningCount += hiLoValue(cardValue);
	}

	// value*10, rounded, split back into whole and tenths -- avoids
	// pulling in <cstdio> just for one float-to-string formatting call.
	std::string formatOneDecimal(float value){
		bool negative = value < 0;
		int tenths = static_cast<int>(std::round(std::fabs(value) * 10.0f));
		std::string s = (negative && tenths != 0 ? "-" : "") + std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
		return s;
	}

	// Drawn right over the discard pile -- running count (raw Hi-Lo sum)
	// and true count (running count / decks remaining in the shoe, the
	// number that actually adjusts for how much of the shoe is left) are
	// what a counter tracks, not just the raw sum, so both are shown.
	// cardsPerDeck accounts for Player's Edge's Spanish (48-card, no 10s)
	// decks so "decks remaining" isn't inflated by counting a deck that's
	// missing 4 cards as if it still had 52.
	void drawCardCountStats(SDLState& state){
		if(!showCardCount)
			return;

		float cardsPerDeck = isPlayersEdge(gameMode) ? 48.0f : 52.0f;
		float decksRemaining = std::max(0.25f, shoe.size() / cardsPerDeck);
		float trueCount = runningCount / decksRemaining;

		SDL_FRect box{ .x = discardPosition.x, .y = discardPosition.y, .w = cardWidth, .h = cardHeight };
		SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(state.renderer, 0, 0, 0, 170);
		SDL_RenderFillRect(state.renderer, &box);
		SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_NONE);
		SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
		SDL_RenderRect(state.renderer, &box);

		// Was 4.0f -- "COUNT" (5 characters) actually overflowed the
		// 100-wide box at the new font's width; 3.0f fits with margin.
		// Gaps below were also uneven (8 between a label and its own
		// value, 18 between that value and the next label) -- now a
		// consistent, tighter rhythm throughout instead.
		float pixel = 3.0f;
		auto centeredLine = [&](const std::string& text, float y, SDL_Color color){
			float w = DigitFont::textWidth(text, pixel);
			DigitFont::drawText(state, text, box.x + (box.w - w) / 2.0f, y, pixel, color);
		};

		SDL_Color label{200, 180, 100, 255};
		SDL_Color value{255, 255, 255, 255};

		float lineH = 5 * pixel;
		float y = box.y + 12.0f;
		centeredLine("COUNT", y, label);
		y += lineH + 6.0f;
		centeredLine(std::to_string(runningCount), y, value);
		y += lineH + 12.0f;
		centeredLine("TRUE", y, label);
		y += lineH + 6.0f;
		centeredLine(formatOneDecimal(trueCount), y, value);
	}

	void drawChipTray(SDLState& state, Resources& res){
		// Each of Table.png's tray columns is exactly 30px wide (measured
		// directly -- matches CHIP_SRC_SIZE 1:1, no scaling needed); 36 was
		// overflowing 6px into the gap on either side.
		float chipW = CHIP_SRC_SIZE;

		for(int col = 0; col < 7; col++){
			SDL_FRect src{ .x = TRAY_COLUMN_DENOM[col] * CHIP_SRC_SIZE, .y = 0.0f, .w = CHIP_SRC_SIZE, .h = CHIP_EDGE_SRC_H };

			for(int layer = 0; layer < trayFillCount[col]; layer++){
				SDL_FRect dst{ .x = TRAY_COLUMN_X[col], .y = TRAY_TOP_Y + layer * TRAY_SLICE_H, .w = chipW, .h = TRAY_SLICE_H };
				SDL_RenderTexture(state.renderer, res.chips, &src, &dst);
			}
		}
	}

	void drawChipAnimations(SDLState& state, Resources& res){
		auto lerp = [](SDL_FPoint a, SDL_FPoint b, float t){
			t = std::clamp(t, 0.0f, 1.0f);
			t = t * t * (3.0f - 2.0f * t);
			return SDL_FPoint{ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
		};
		constexpr float BESIDE = 34.0f; // winnings stack, to the right of the bet

		for(ChipAnimation& a : chipAnimations){
			float t = a.elapsed;
			if(a.kind == ChipAnimation::Loss){
				SDL_FPoint p = lerp(a.spot, a.tray, t / CHIPS_TO_TRAY);
				drawChipStack(state, res, p.x, p.y, { a.stake });
				continue;
			}

			SDL_FPoint beside{ a.spot.x + BESIDE, a.spot.y };
			float toBankroll = chipAnimationLength(a) - CHIPS_TO_BANKROLL;
			if(t < toBankroll){
				if(a.stake > 0)
					drawChipStack(state, res, a.spot.x, a.spot.y, { a.stake });
				if(a.kind == ChipAnimation::Win && a.winnings > 0){
					SDL_FPoint p = lerp(a.tray, a.stake > 0 ? beside : a.spot, t / CHIPS_TO_SPOT);
					drawChipStack(state, res, p.x, p.y, { a.winnings });
				}
			} else{
				// bet and winnings, picked up together
				float u = (t - toBankroll) / CHIPS_TO_BANKROLL;
				SDL_FPoint p = lerp(a.spot, a.bankroll, u);
				if(a.stake > 0)
					drawChipStack(state, res, p.x, p.y, { a.stake });
				if(a.winnings > 0)
					drawChipStack(state, res, p.x + (a.stake > 0 ? BESIDE * (1.0f - u) : 0.0f), p.y, { a.winnings });
			}
		}
	}

	void drawBetting(SDLState& state){
		SDL_FRect deal = dealButton();
		drawButton(state, deal, SDL_Color{60, 130, 70, 255});
		float dealPixel = 8.5f;
		float dealW = DigitFont::textWidth("DEAL", dealPixel);
		DigitFont::drawText(state, "DEAL", deal.x + (deal.w - dealW) / 2.0f, deal.y + (deal.h - 5 * dealPixel) / 2.0f, dealPixel, SDL_Color{255, 255, 255, 255});

		for(int i = 0; i < numberOfPlayers; i++){
			if(players[i].getBankroll() <= 0){
				drawBuyInButton(state, i);
				continue;
			}

			drawBetRow(state, i);
			drawSideBetRows(state, i);
		}
	}

	void processGesture(const std::vector<TouchPoint>& touches){
		if(touches.size() == 1){
			const TouchPoint& t = touches[0];
			float dx = t.x - t.startX;
			float dy = t.y - t.startY;
			float dist = std::sqrt(dx * dx + dy * dy);

			if(dist < TAP_MOVE_THRESHOLD){
				playerAction('H');
			} else if(std::fabs(dy) > std::fabs(dx)){
				// Steep swipe (up or down) -> surrender.
				playerAction('R');
			} else {
				// Shallow swipe (left or right) -> stand.
				playerAction('S');
			}
		} else if(touches.size() == 2){
			const TouchPoint& a = touches[0];
			const TouchPoint& b = touches[1];

			float aDx = a.x - a.startX, aDy = a.y - a.startY;
			float bDx = b.x - b.startX, bDy = b.y - b.startY;

			float aDist = std::sqrt(aDx * aDx + aDy * aDy);
			float bDist = std::sqrt(bDx * bDx + bDy * bDy);

			if(aDist < TAP_MOVE_THRESHOLD && bDist < TAP_MOVE_THRESHOLD){
				playerAction('D');
			} else if(aDist >= TAP_MOVE_THRESHOLD && bDist >= TAP_MOVE_THRESHOLD){
				float dot = (aDx * bDx + aDy * bDy) / (aDist * bDist);

				if(dot < SPLIT_DOT_THRESHOLD)
					playerAction('P');
				else
					std::cout << "Two-finger gesture not recognized (moved together)\n";
			} else {
				std::cout << "Two-finger gesture not recognized (one finger held still)\n";
			}
		} else if(!touches.empty()){
			std::cout << "Unsupported gesture with " << touches.size() << " fingers\n";
		}
	}


	// player moves
	// Every hit/stand/double/split/surrender the player makes (keys and
	// gestures alike) comes through here, so a legal move can be scored
	// against the strategy chart for the STATS page before it's played.
	// Illegal moves (e.g. double on 3 cards in standard) do nothing and
	// aren't scored.
	void playerAction(char action){
		bool legal = true;
		switch(action){
			case 'D': legal = canDoubleActiveHand(); break;
			case 'P': legal = canSplitActiveHand(); break;
			case 'R': legal = canSurrenderActiveHand(); break;
			case 'H': {
				Person& p = players[activePlayer];
				int h = p.getActiveHand();
				legal = !(isPlayersEdge(gameMode) && h < p.hands.size() && p.hands[h].getDoubleCount() > 0);
				break;
			}
			default: break;
		}

		if(legal && stats){
			char advice;
			if(chartAdvice(advice))
				stats->recordDecision(advice == action);
		}

		switch(action){
			case 'H': onHit(); break;
			case 'S': onStand(); break;
			case 'D': onDouble(); break;
			case 'P': onSplit(); break;
			case 'R': onSurrender(); break;
			default: break;
		}
	}

	// What the current mode's chart says for the active hand, turned into
	// a move that's actually available: a chart double on a hand that
	// can't double means hit (stand on soft 18/19), a chart surrender that
	// isn't allowed means stand on 17 / split 8s / otherwise hit. False
	// when there's no chart situation or the chart's split can't be made.
	bool chartAdvice(char& advice){
		int section, row, col;
		if(!getStrategySituation(section, row, col))
			return false;
		advice = StrategyChart::rowData(section, row)[col];
		if(activeHandDoubled()){
			advice = doubledAdvice(advice);
			return true;
		}
		if(advice == 'D' && !canDoubleActiveHand())
			advice = (section == 1 && row >= 5) ? 'S' : 'H';
		else if(advice == 'R' && !canSurrenderActiveHand()){
			if(section == 0 && row == 9) advice = 'S';
			else if(section == 2 && canSplitActiveHand()) advice = 'P';
			else advice = 'H';
		}
		else if(advice == 'P' && !canSplitActiveHand())
			return false;
		return true;
	}

	// Player's Edge (Spanish 21): once doubled, a hand only draws more
	// cards by redoubling -- a doubled hand can't simply be hit. (In every
	// other mode a double ends the hand anyway, see checkBreak().)
	static constexpr int MAX_PLAYERS_EDGE_DOUBLES = 3;
	static constexpr int MAX_SPLIT_HANDS = 4;

	void onHit(){
		{
			Person& p = players[activePlayer];
			int h = p.getActiveHand();
			if(isPlayersEdge(gameMode) && h < p.hands.size() && p.hands[h].getDoubleCount() > 0)
				return;
		}
		dealQueue.push(DealRequest{
			.playerIndex = activePlayer,
			.isDealer = false,
			.showCard = true,
			.from = shoePosition,
			.to = players[activePlayer].getNextCardPosition(),
			.card = getNextCard()
		});
	}

	void onStand(){
		activePlayer += players[activePlayer].stand();
		skipZeroBetPlayers();
	}

	// Free Bet Blackjack only: is the active hand's pair eligible for a
	// free split -- any value-tier except 10 (verified against
	// wizardofodds.com: "all pairs except 10's").
	bool isFreeSplitEligible(){
		Person& p = players[activePlayer];
		Hand& hand = p.hands[p.getActiveHand()];
		int v0 = hand.cards[0].getValue();
		int tier = (v0 > 10) ? 10 : v0;
		return tier != 10;
	}

	// Free Bet Blackjack only: is the active hand eligible for a free
	// double -- exactly 2 cards, hard total 9, 10, or 11 (an Ace counts as
	// 1 here, same "true hard total" reasoning as Lucky Stiff's
	// evaluateLuckyStiffImmediate()).
	bool isFreeDoubleEligible(){
		Person& p = players[activePlayer];
		Hand& hand = p.hands[p.getActiveHand()];
		if(hand.getHandSize() != 2)
			return false;

		auto tier = [](int v){ return v > 10 ? 10 : v; };
		int hardTotal = tier(hand.cards[0].getValue()) + tier(hand.cards[1].getValue());
		return hardTotal >= 9 && hardTotal <= 11;
	}

	// Exactly 2 cards, sharing the same blackjack value-tier (face cards
	// all collapse to 10 -- e.g. a King and a Queen are a splittable pair,
	// same as two Kings) -- the same tiering getStrategySituation() already
	// uses to classify a hand as a pair, kept in sync with it rather than
	// re-deriving separately. Without this, the split gesture used to fire
	// unconditionally on whatever hand happened to be active, silently
	// "splitting" a 3-card hand or two unrelated cards. Requires the
	// bankroll to cover a matching second wager -- a split doubles what's
	// at risk on this hand, same as a double does -- unless Free Bet
	// Blackjack is footing this particular split for free, in which case
	// no bankroll is needed at all. Free Bet Blackjack also caps
	// re-splitting at 4 total hands (including aces), where every other
	// mode has no such cap.
	bool canSplitActiveHand(){
		Person& p = players[activePlayer];
		int handIdx = p.getActiveHand();
		if(handIdx >= p.hands.size())
			return false;

		Hand& hand = p.hands[handIdx];
		if(hand.getHandSize() != 2)
			return false;

		// Split and re-split up to 4 hands in every mode -- the usual casino
		// limit, and as many as fit beside each other at a seat.
		if(p.hands.size() >= MAX_SPLIT_HANDS)
			return false;

		bool free = isFreeBet(gameMode) && isFreeSplitEligible();
		if(!free && p.getBankroll() < hand.getBet())
			return false;

		int v0 = hand.cards[0].getValue();
		int v1 = hand.cards[1].getValue();
		int tier0 = (v0 > 10) ? 10 : v0;
		int tier1 = (v1 > 10) ? 10 : v1;
		return tier0 == tier1;
	}

	// Same affordability check as canSplitActiveHand() -- a double also
	// wagers the active hand's bet a second time, unless Free Bet
	// Blackjack is footing this double for free.
	bool canDoubleActiveHand(){
		Person& p = players[activePlayer];
		int handIdx = p.getActiveHand();
		if(handIdx >= p.hands.size())
			return false;

		Hand& hand = p.hands[handIdx];

		// Player's Edge is Spanish 21, which allows doubling on any
		// number of cards (and redoubling, see checkBreak()) -- every
		// other mode only doubles a hand's first two cards.
		if(!isPlayersEdge(gameMode) && hand.getHandSize() != 2)
			return false;
		// Spanish 21 redoubling: a hand can be doubled at most 3 times.
		if(isPlayersEdge(gameMode) && hand.getDoubleCount() >= MAX_PLAYERS_EDGE_DOUBLES)
			return false;
		if(hand.isSplitAces() && !allowsHitSplitAces())
			return false;
		// Double-deck games don't allow doubling after a split
		// (Clearwater's double deck rules); shoe games do.
		if(hand.isFromSplit() && numberOfDecks <= 2 && !isPlayersEdge(gameMode))
			return false;

		if(isFreeBet(gameMode) && isFreeDoubleEligible())
			return true;

		return p.getBankroll() >= hand.getBet();
	}

	void onSplit(){
		if(!canSplitActiveHand())
			return;
		if(stats)
			stats->bump(Stats::Splits);

		// The new hand costs the same as the one being split -- split()
		// (Person.h) already copies the stake onto it. Ordinarily this is
		// what actually pulls that matching amount out of the bankroll --
		// but if Free Bet Blackjack is footing this split for free, no
		// money moves at all, and the new hand's entire bet gets flagged
		// as free (addFreeBetAmount(), after split() so it isn't wiped by
		// Hand::setBet()'s own reset) so a loss on it costs nothing and a
		// push only returns real money (there is none here).
		int betAmount = players[activePlayer].getActiveHandBet();
		bool free = isFreeBet(gameMode) && isFreeSplitEligible();
		if(!free){
			players[activePlayer].deductFromBankroll(betAmount);
			queueBankrollChange(activePlayer, -betAmount);
		}

		bool aces = players[activePlayer].hands[players[activePlayer].getActiveHand()].cards[0].getValue() == 1;
		Card c = players[activePlayer].split();
		{
			Person& p = players[activePlayer];
			int h = p.getActiveHand();
			p.hands[h].markSplit(aces);
			p.hands[h + 1].markSplit(aces);
		}

		if(free){
			int newHandIdx = players[activePlayer].getActiveHand() + 1;
			players[activePlayer].hands[newHandIdx].addFreeBetAmount(betAmount);
		}

		SDL_FPoint from = c.getPosition();
		SDL_FPoint to = players[activePlayer].getNextCardPositionOfBlankHand();

		dealQueue.push(DealRequest{
			.playerIndex = activePlayer,
			.isDealer = false,
			.showCard = true,
			.split = true,
			.from = from,
			.to = to,
			.card = c
		});

		// hit
		onHit();
	}

	void onDouble(){
		if(!canDoubleActiveHand())
			return;
		if(stats)
			stats->bump(Stats::Doubles);

		std::cout << "Double. ActivePlayer: " << activePlayer << '\n';

		// Doubling wagers the same amount again -- deduct that extra
		// before doubling the hand's own recorded bet, so both the
		// bankroll and the bet/chip display end up reflecting it. Unless
		// Free Bet Blackjack is footing this double for free, in which
		// case no money moves and the newly-added half is flagged free
		// (addFreeBetAmount(), after doubleActiveHandBet() so it adds to
		// -- not gets wiped by -- any free amount this hand already had
		// from a free split).
		int betAmount = players[activePlayer].getActiveHandBet();
		bool free = isFreeBet(gameMode) && isFreeDoubleEligible();
		if(!free){
			players[activePlayer].deductFromBankroll(betAmount);
			queueBankrollChange(activePlayer, -betAmount);
		}
		players[activePlayer].doubleActiveHandBet();
		if(free){
			int handIdx = players[activePlayer].getActiveHand();
			players[activePlayer].hands[handIdx].addFreeBetAmount(betAmount);
		}

		// The double card lies sideways: turned 90 degrees about its
		// top-left corner, so it's moved one card-height "down" the seat
		// (its own +y) to sit in line with the hand. Worked out from the
		// seat's actual angle -- the two angled 5-player seats aren't
		// straight up, left or right, and a cardinal-only shift threw
		// their double card off the hand.
		SDL_FPoint to = players[activePlayer].getNextCardPosition();
		{
			constexpr float PI = 3.14159265358979323846f;
			float rad = players[activePlayer].getSeatRotation() * PI / 180.0f;
			to.x += -cardHeight * std::sin(rad);
			to.y += cardHeight * std::cos(rad);
		}

		// faceDownDoubles (GameOptionsMenu): the double-down card lands
		// face-down and stays that way until the dealer's actually done --
		// resolveRound() reveals it (and every other card) right before
		// settling results, same moment the dealer's own hole card is
		// already showing by. getHandTotal() sums every card regardless of
		// isShown, so hitting/standing/bust logic in the meantime is
		// unaffected either way -- this only changes what's drawn.
		dealQueue.push(DealRequest{
			.playerIndex = activePlayer,
			.isDealer = false,
			.showCard = !faceDownDoubles,
			.doubleHand = true,
			.from = shoePosition,
			.to = to,
			.card = getNextCard()
		});

		// The hand doesn't actually end until this card lands -- see
		// checkBreak(forceStandIfNotBust) in update()'s resolve step, which
		// stands (or, on a bust, discards) once the card is really dealt.
		// Standing here instead advanced activeHand before the card was
		// ever added to the hand, so it landed on an already-out-of-range
		// index when addCard() finally ran ("subscript out of range").
	}

	// Late surrender: only on the hand's first two cards, never after a
	// split, and not at all in Free Bet Blackjack (its rules don't offer
	// it -- the free doubles/splits are the trade).
	bool canSurrenderActiveHand(){
		if(isFreeBet(gameMode))
			return false;
		Person& p = players[activePlayer];
		int handIdx = p.getActiveHand();
		if(handIdx >= p.hands.size())
			return false;
		// Player's Edge "double down rescue": a doubled hand can be
		// surrendered, losing just the original bet (see onSurrender()).
		if(isPlayersEdge(gameMode) && p.hands[handIdx].getDoubleCount() > 0)
			return true;
		if(p.hands.size() != 1)
			return false;
		return p.hands[handIdx].getHandSize() == 2;
	}

	void onSurrender(){
		if(!canSurrenderActiveHand())
			return;

		std::cout << "Surrender. ActivePlayer: " << activePlayer << '\n';

		// Half the bet comes back (rounded down, like a casino keeping the
		// odd chip); zeroing the bet keeps resolveRound() from settling
		// the hand again.
		{
			Hand& hand = players[activePlayer].hands[players[activePlayer].getActiveHand()];
			// A rescued double (Player's Edge) gives back everything but the
			// original bet; a plain surrender gives back half.
			int refund = hand.getDoubleCount() > 0
				? hand.getBet() - (hand.getBet() >> hand.getDoubleCount())
				: hand.getBet() / 2;
			if(stats)
				stats->recordHand(-(hand.getBet() - refund), false, false, false, false, true);
			hand.setBet(0);
			if(refund > 0)
				queueChipPayout(activePlayer, refund, true, refund, betSpot(activePlayer, players[activePlayer].getActiveHand()));
		}

		// Surrendering forfeits the hand same as busting does (labelled
		// SURRENDER while it's swept away). Has to happen before busted()
		// below advances past this hand.
		players[activePlayer].surrenderActiveHand();

		std::vector<Card> forfeited = players[activePlayer].busted();

		// Don't also insert into discard directly here -- these cards get
		// added to discard once each already-queued discard animation below
		// actually lands (see update()'s resolve step). Doing both meant
		// every surrendered card showed up twice: once instantly, face-up,
		// at its old spot in the hand, and again later, correctly, once it
		// finished animating to the discard pile.
		for(Card c : forfeited){
			dealQueue.push(DealRequest{
				.playerIndex = activePlayer,
				.isDealer = false,
				.discard = true,
				.showCard = false,
				.from = c.getPosition(),
				.to = discardPosition,
				.card = c,
			});
		}

		// Not stand() here -- busted() above already advanced activeHand
		// past this hand (same as stand() would), so calling stand() too
		// double-advances it. Harmless-looking on a single hand (it still
		// lands on the next player, just via a corrupted activeHand that
		// gets reset before it's ever read again), but on a split hand it
		// skips the second hand entirely, exactly like the double-down bug
		// above -- a stand-equivalent fired a second time on top of one
		// that already ran. Mirrors checkBreak()'s own bust-resolution check.
		if(players[activePlayer].getActiveHand() > players[activePlayer].hands.size() - 1)
			activePlayer++;
		skipZeroBetPlayers();
	}


	// add animation object to the queue
	void startDeal(){
		if(cardAnimation.has_value() || dealQueue.empty())
			return;

		DealRequest request = dealQueue.front();
		dealQueue.pop();

		Card card = request.card;
		if(request.from.x == shoePosition.x && request.from.y == shoePosition.y)
			sound(Sfx::Deal);

		if(request.removeFromHand){
			if(request.isDealer)
				dealer.discardOneCard(request.handIndex);
			else
				players[request.playerIndex].discardOneCard(request.handIndex);
		}

		// Used to be -90*dir (or 180 for the dealer) -- a cardinal-only
		// approximation that happened to equal every seat's real rotation
		// back when there were only 4 possible seat angles. The 2 new
		// 5-player seats reuse dir=1/dir=-1's bucket but sit at their own
		// non-cardinal angle (see BettingSquare.h), so that approximation
		// made a normal deal visibly fly in at the wrong tilt for those 2
		// seats before snapping to the correct one the instant it landed
		// (addCard() sets the real rotation on arrival, independently of
		// this). Reading the seat's own stored rotation directly fixes the
		// flight itself, not just the landed result.
		float angle = request.isDealer ? dealer.getSeatRotation() : players[request.playerIndex].getSeatRotation();

		angle -= (request.doubleHand) ? 90 : 0;

		// The cut card goes straight from the shoe to the discard pile,
		// never through anyone's seat -- slide it flat instead of
		// spinning it in from the dealer seat's 180 degrees.
		if(card.getValue() == 14)
			angle = 0;

		cardAnimation.emplace(CardAnimation{
			.card = card,
			.start = request.from,
			.end = request.to,
			.destination = SDL_FRect{
				.x = shoePosition.x,
				.y = shoePosition.y,
				.w = cardWidth,
				.h = cardHeight
			},
			.startAngle = request.split || request.discard ? angle : 0,
			.currentAngle = request.split || request.discard ? angle : 0,
			.finalAngle =  request.discard ? 0 : angle,
			// Was pause-timers-only -- barely perceptible, since those only
			// fire once or twice a round. Scaling every card's own flight
			// too makes the setting actually visible on every single deal,
			// which is also what GameOptionsMenu's own live demo card
			// needs to be demonstrating in the first place.
			// The cut card flies at deal speed, not the quicker discard
			// sweep speed, so it's actually noticeable.
			.duration = (request.discard && card.getValue() != 14 ? DISCARD_DURATION : DEAL_DURATION) * dealerSpeedFactor,
			.playerIndex = request.playerIndex,
			.handIndex = request.handIndex,
			.split = request.split,
			.showCard = request.showCard,
			.isDealer = request.isDealer,
			.discard = request.discard,
			.doubleHand = request.doubleHand
			});
	}


	// restart the table
	void clearTable(){
		for(int i = 0; i < numberOfPlayers; i++){
			for(int h = 0; h < players[i].hands.size(); h++){
				std::vector<Card>& cards = players[i].hands[h].cards;

				for(int c = cards.size() - 1; c >= 0; c--){
					dealQueue.push(DealRequest{
						.playerIndex = i,
						.handIndex = h,
						.isDealer = false,
						.discard = true,
						.showCard = false,
						.removeFromHand = true,
						.from = cards[c].getPosition(),
						.to = discardPosition,
						.card = cards[c],
					});
				}
			}
		}

		std::vector<Card>& dealerCards = dealer.hands[0].cards;
		for(int c = dealerCards.size() - 1; c >= 0; c--){
			dealQueue.push(DealRequest{
				.playerIndex = -1,
				.handIndex = 0,
				.isDealer = true,
				.discard = true,
				.showCard = false,
				.removeFromHand = true,
				.from = dealerCards[c].getPosition(),
				.to = discardPosition,
				.card = dealerCards[c],
			});
		}

		activePlayer = 0;
	}

public:
	// Restart (both the main menu's and the pause menu's) bails out to the
	// GameMode screen entirely -- but that screen's/Setup's own update()
	// loop never touches Table, only clearTable()'s did (draining the
	// animated discard-sweep queue it pushes), and that loop only runs
	// while AppScreen::Playing is showing. Navigating away before it's had
	// a chance to drain left every card still sitting in player/dealer
	// hands, cards visibly there again the moment a new game started
	// (dealer's hand most obviously, since it's the first thing seen while
	// betting). This instead clears everything immediately, no animation
	// needed since there's no longer a Playing screen for one to play out
	// on. Public (unlike clearTable(), which only Table itself ever
	// drives) since mina.cpp's Restart handlers call this directly.
	void resetForNewGame(){
		dealQueue = {};
		cardAnimation.reset();
		chipAnimations.clear();
		bankrollChanges.clear();
		jackpotCallouts.clear();

		dealer.resetHands();
		for(int i = 0; i < numberOfPlayers; i++)
			players[i].resetHands();

		// Discard pile and shoe both empty -- configureGameMode() rebuilds
		// a fresh shoe once the next game's mode is actually picked, but
		// clearing them now (rather than leaving the previous game's
		// half-depleted shoe/discard sitting there in the meantime) is
		// what "like a fresh game" actually means for these two.
		discard.clear();
		shoe.clear();

		// Chip tray: each seat's selected bet denomination back to its
		// original default (index 2 -- see chipIndex's own declaration).
		for(int i = 0; i < 5; i++)
			chipIndex[i] = 2;

		activePlayer = 0;
		awaitingBets = false;
		shuffling = false;
		awaitingNewRound = false;
		awaitingInitialDeal = false;
		awaitingPeek = false;
		awaitingInsurance = false;
		insuranceQueue.clear();
		shoeNeedsReshuffle = false;
		runningCount = 0;

		for(int i = 0; i < 5; i++){
			sideBetResult[i] = HandResult::None;
			matchUpResult[i] = HandResult::None;
			matchDownResult[i] = HandResult::None;
			matchDownResolved = false;
			luckyStiffPending[i] = false;
		}
	}

private:
	void firstDeal() {
		// Burn the first card -- only when this shoe hasn't had one yet
		// (see needsBurnCard's own declaration). Used to run every single
		// hand, burning a fresh card each time instead of just once per
		// shoe like a real table does.
		if(needsBurnCard){
			needsBurnCard = false;
			dealQueue.push(DealRequest{
				.playerIndex = activePlayer,
				.isDealer = false,
				.discard = true,
				.showCard = false,
				.from = shoePosition,
				.to = discardPosition,
				.card = getNextCard(),
			});
		}

		for(int loop = 0; loop < 2; loop++) {
			for(int i = 0; i < numberOfPlayers; i++) {
				// A player who bet 0 sits this round out -- no cards.
				if(players[i].getBet() <= 0)
					continue;

				dealQueue.push(DealRequest{
					.playerIndex = i,
					.isDealer = false,
					.showCard = true,
					.from = shoePosition,
					.to = players[i].getNextCardPosition(loop),
					.card = getNextCard()
				});
			}

			dealQueue.push(DealRequest{
				.playerIndex = -1,
				.isDealer = true,
				.showCard = (loop == 0),
				.from = shoePosition,
				.to = dealer.getNextCardPosition(loop),
				.card = getNextCard()
			});
		}
	}


	// initialize shoe and cards
	void makeShoe(){
		// Safe to call again (configureGameMode() does, once the game-mode
		// screen picks a real mode) -- without this, a second call
		// appended onto whatever cards were already here from the
		// constructor's own initial makeShoe() instead of replacing them.
		shoe.clear();

		// A fresh shoe needs burning once, not every hand dealt from it --
		// firstDeal() reads this and clears it once the actual burn
		// happens (see there).
		needsBurnCard = true;

		std::random_device rd;
		std::mt19937 gen(rd());

		struct c{
			int suit;
			int value;
		};

		std::vector<c> values;

		// Spanish decks (Player's Edge) drop all four 10s per suit, keeping
		// J/Q/K -- the defining difference from a standard 52-card deck.
		bool spanishDeck = isPlayersEdge(gameMode);
		for(int deck = 1; deck <= numberOfDecks; deck++){
			for(int suit = 0; suit < 4; suit++){
				for(int value = 1; value <= 13; value++){
					if(spanishDeck && value == 10)
						continue;
					values.push_back(c(suit,value));
				}
			}
		}
		std::shuffle(values.begin(),values.end(),gen);

		std::random_device rdn; // Seed from hardware
		std::mt19937 genn(rdn()); // Mersenne Twister engine
		std::uniform_int_distribution<> dist(values.size() / 2,values.size() - values.size() / 8); // Inclusive range

		int randomValue = dist(genn);
		values.insert(values.begin() + randomValue,c(0,14));

		// The on-table stack's total width is capped at MAX_SHOE_STACK_WIDTH
		// regardless of deck count, instead of a fixed 3px-per-card step --
		// at a fixed step, a 6+ deck shoe's far end (computed below so the
		// *near* end always lands back at shoePosition, see spacing math)
		// would start hundreds of pixels past the 1440-wide canvas's right
		// edge and render mostly off-screen. 315 is exactly this game's old
		// 2-deck width (105 cards * 3px), so a 2-deck shoe looks pixel-
		// identical to before; anything bigger just packs its cards tighter
		// instead of running off the edge.
		constexpr float MAX_SHOE_STACK_WIDTH = 315.f;
		shoeCardSpacing = std::min(3.f, MAX_SHOE_STACK_WIDTH / std::max<size_t>(1, values.size()));

		// shoe[0] (the first card dealt -- see getNextCard(), which pops
		// shoe.front()) sits at shoePosition itself, the near/left end of
		// the stack; later indices step rightward, deeper into the shoe.
		// Each card's own stored position doesn't actually matter for
		// rendering (draw() computes it live from the card's *current*
		// index * shoeCardSpacing, so the stack visibly shifts left as
		// cards are dealt instead of a stale baked-in position just
		// revealing progressively-further-right cards as the front of the
		// vector empties) -- shoePosition itself is a fine placeholder.
		for(int i = 0; i < values.size(); i++){
			c card = values[i];
			shoe.push_back(Card(card.suit,card.value,false,shoePosition,0));
		}
	}


	// helpers

	// A player who bet 0 got no cards this round (see firstDeal()) and has
	// no turn to take -- called after every activePlayer advance (stand,
	// bust, surrender, double-resolution) so play skips straight past them
	// to the next player who's actually in the hand. Safe to call even
	// when activePlayer didn't just change: it only loops while the
	// *current* player has no bet, which is never true for whoever was
	// already mid-turn (they have cards, so they bet something).
	void skipZeroBetPlayers(){
		while(activePlayer < numberOfPlayers && players[activePlayer].getBet() <= 0)
			activePlayer++;
	}

	// Table.png has a dedicated white rectangle for this at (892,69) to
	// (1145,212) -- measured directly from the art, not a guess -- distinct
	// from the big dark-green rounded box in the middle (which is a
	// separate, unrelated placeholder) and from DEAL's canvas-center spot
	// (the two never show at once, but they were never meant to be the
	// same place regardless).
	// Rough middle of the 7-column tray (see drawChipTray()) -- the
	// middle column's X (706, the columns are evenly spaced 40 apart) and
	// the average column top plus half the filled stack's height. Only
	// used as where a payout chip animation starts from; the tray's own
	// columns don't reference this.
	// Where a payout chip animation starts from: the bottom of whichever
	// tray column matches its denomination -- a tray dispenses from the
	// bottom of the stack, not by lifting one out of the middle. Falls
	// back to the first column (the 1s) if denomIndex somehow doesn't
	// match any column, which shouldn't happen since every BET_DENOMS
	// entry has at least one column.
	SDL_FPoint trayColumnBottom(int col){
		return SDL_FPoint{ TRAY_COLUMN_X[col] + CHIP_SRC_SIZE / 2.0f, TRAY_BOTTOM_Y };
	}

	// Settles every hand still holding a bet against the dealer's final
	// total -- standard blackjack rules: a bust hand loses outright
	// regardless of the dealer; a natural (2-card) 21 pays 3:2 unless the
	// dealer also has one (push); otherwise the dealer busting or a higher
	// total wins 1:1, equal totals push (bet back, no gain), anything else
	// loses. Applies per hand, not per player, so a split's two hands
	// resolve independently. A two-card 21 on a split hand (say A+10) is
	// just 21, paid 1:1 -- only an unsplit hand's first two cards are a
	// blackjack, the standard casino rule. Surrendered hands never reach
	// here at all: onSurrender() zeroes their bet.
	void resolveRound(){
		int dealerTotal = dealer.getHandTotal();
		bool dealerBust = dealerTotal > 21;
		bool dealerBlackjack = dealer.hands[0].getHandSize() == 2 && dealerTotal == 21;

		// faceDownDoubles (GameOptionsMenu) leaves a double-down card
		// hidden until now -- reveal it before anything else here, same as
		// the dealer's own hole card already was back in dealDealer().
		// Counted the moment it's actually revealed (addToRunningCount()'s
		// own landing-time hook in update() only fires for a card that's
		// *already* shown when it lands, so a still-hidden one would
		// otherwise never get counted at all, silently throwing the count
		// off for the rest of the shoe).
		bool revealedDouble = false;
		for(int i = 0; i < numberOfPlayers; i++){
			for(Hand& hand : players[i].hands){
				for(Card& c : hand.cards){
					if(!c.getShown()){
						addToRunningCount(c.getValue());
						c.showCard(true);
						revealedDouble = true;
					}
				}
			}
		}
		if(revealedDouble)
			sound(Sfx::Flip);

		// Normally already settled the moment the hole card flipped
		// (dealDealer()); this catches the dealer-blackjack path, where
		// dealerPeek() reveals the hole card itself.
		resolveMatchDown();

		for(int i = 0; i < numberOfPlayers; i++){
			for(int h = 0; h < (int)players[i].hands.size(); h++){
				Hand& hand = players[i].hands[h];
				int bet = hand.getBet();
				if(bet <= 0)
					continue;

				int total = hand.getHandTotal();
				bool bust = hand.isBust() || total > 21;
				bool blackjack = hand.getHandSize() == 2 && total == 21 && !hand.isFromSplit();

				int credit = 0;
				if(isPlayersEdge(gameMode)){
					credit = spanish21Credit(hand, dealerTotal, dealerBust, dealerBlackjack);
				} else if(isFreeBet(gameMode)){
					credit = freeBetCredit(hand, dealerTotal, dealerBust, dealerBlackjack);
				} else if(bust){
					credit = 0;
				} else if(dealerBlackjack && blackjack){
					credit = bet;
				} else if(dealerBlackjack){
					credit = 0;
				} else if(blackjack){
					credit = bet + bet * 3 / 2;
				} else if(dealerBust || total > dealerTotal){
					credit = bet * 2;
				} else if(total == dealerTotal){
					credit = bet;
				} else{
					credit = 0;
				}

				// credit==0 is a clean loss; credit==bet is a push (bet
				// just comes back, no gain); anything more is a win --
				// covers both the plain 1:1 (bet*2) and blackjack (bet*2.5)
				// cases without re-deriving them here. credit itself is
				// always computed on the *full* bet (see freeBetCredit()),
				// so this classification logic doesn't need to know
				// anything about free-bet money -- only the actual
				// payout/collection amount below does: getRealBet() is
				// just bet for every other mode (freeBetAmount is always
				// 0), so this is a no-op everywhere except Free Bet
				// Blackjack, where a loss or push only costs/returns
				// whatever part of the bet was actually real money.
				if(stats){
					bool isWin = credit > bet, isPush = credit == bet && credit > 0;
					long long net = credit == 0 ? -hand.getRealBet() : isPush ? 0 : credit - hand.getRealBet();
					stats->recordHand(net, isWin, isPush, isWin && blackjack, hand.isBust(), false);
				}

				if(credit == 0){
					hand.setResult(HandResult::Loss);
					// A bust's chips were already taken when it busted.
					if(!hand.isChipsCollected())
						queueChipCollection(i, hand.getRealBet(), betSpot(i, h));
				} else{
					// A push still needs its bet credited back -- just
					// with no gain (credit == bet). Only the label differs
					// from an outright win; the payout mechanics are the same.
					// isPush is passed through so the bankroll-change
					// readout can say "PUSH +$X" instead of styling it
					// exactly like a real win.
					bool isPush = credit == bet;
					hand.setResult(isPush ? HandResult::Push : HandResult::Win);
					queueChipPayout(i, isPush ? hand.getRealBet() : credit, isPush, hand.getRealBet(), betSpot(i, h));
				}

				hand.setBet(0);
			}
		}

		// A pending Lucky Stiff bet (unpaired hard 12-16 -- see
		// evaluateLuckyStiffImmediate()) rides along with the main hand:
		// it can't split (it's explicitly not a pair) so hands[0] is
		// always the one and only hand to check, and by now the loop just
		// above has already set its result.
		if(hasLuckyStiff(gameMode)){
			for(int i = 0; i < numberOfPlayers; i++){
				if(!luckyStiffPending[i])
					continue;

				luckyStiffPending[i] = false;

				int wager = players[i].getSideBet();
				if(wager <= 0)
					continue;

				HandResult mainResult = players[i].hands[0].getResult();
				recordSideBet(Stats::LuckyStiff, wager,
					mainResult == HandResult::Win ? wager * 6 : mainResult == HandResult::Push ? wager : 0);
				if(mainResult == HandResult::Win){
					queueChipPayout(i, wager + wager * 5, false, wager, sideBetSpot(i, true));
					sideBetResult[i] = HandResult::Win;
				} else if(mainResult == HandResult::Push){
					queueChipPayout(i, wager, true, wager, sideBetSpot(i, true));
					sideBetResult[i] = HandResult::Push;
				} else{
					queueChipCollection(i, wager, sideBetSpot(i, true));
					sideBetResult[i] = HandResult::Loss;
				}
			}
		}
	}

	// Player's Edge Super Bonus (Clearwater): a hand of three suited 7s,
	// not split or doubled, while the dealer shows a 7, wins a flat $1,000
	// on a bet under $25 or $5,000 on $25 or more -- on top of its 7-7-7
	// bonus -- and every other player in the round gets a $50 Envy Bonus.
	// Returns the hand's extra credit (0 if it doesn't qualify).
	int superBonus(int playerIndex, Hand& hand){
		if(hand.getHandSize() != 3 || hand.isFromSplit() || hand.getDoubleCount() > 0)
			return 0;
		if(dealer.hands[0].getHandSize() < 1 || dealer.hands[0].cards[0].getValue() != 7)
			return 0;
		for(Card& c : hand.cards)
			if(c.getValue() != 7 || c.getSuit() != hand.cards[0].getSuit())
				return 0;

		int bonus = hand.getBet() >= 25 ? 5000 : 1000;
		queueJackpotCallout(playerIndex, "SUPER BONUS!", "SUITED 777 VS DEALER 7", bonus);
		for(int j = 0; j < numberOfPlayers; j++){
			if(j == playerIndex || !initialTwoCards[j].valid)
				continue;
			queueChipPayout(j, ENVY_BONUS, false, 0, seatPoint(j, 0, cardWidth / 2.0f, cardHeight + SPOT_MARGIN + 4.0f));
			if(stats)
				stats->addNet(ENVY_BONUS);
		}
		return bonus;
	}
	static constexpr int ENVY_BONUS = 50;

	// Match Down is judged against the dealer's hole card, so it settles
	// the moment that card is turned over -- before the dealer draws
	// (dealDealer()), or in dealerPeek()'s dealer-blackjack path via
	// resolveRound(). Match Up and Lucky Ladies settle right after the
	// initial deal (resolveSideBets()). Once per round.
	void resolveMatchDown(){
		if(matchDownResolved || !isPlayersEdge(gameMode) || dealer.hands[0].getHandSize() < 2)
			return;
		matchDownResolved = true;
		{
			Card& dealerDown = dealer.hands[0].cards[1];
			for(int i = 0; i < numberOfPlayers; i++){
				// Everyone dealt in, including a hand that's already busted
				// or been paid on a 21 -- the side bet is separate.
				if(!initialTwoCards[i].valid)
					continue;

				int wager = players[i].getMatchDownBet();
				if(wager <= 0)
					continue;

				int payout = evaluateMatchBet(i, dealerDown, wager);
				recordSideBet(Stats::MatchDown, wager, payout);
				if(payout > 0){
					queueChipPayout(i, payout, false, wager, sideBetSpot(i, false));
					matchDownResult[i] = HandResult::Win;

					// A pair matching the hole card is three of a kind;
					// if the up-card is that rank too, it's four.
					bool allSuited = false;
					if(matchCount(i, dealerDown, allSuited) == 2){
						bool fourOfAKind = dealer.hands[0].cards[0].getValue() == dealerDown.getValue();
						queueJackpotCallout(i,
							fourOfAKind ? "FOUR OF A KIND!" : (allSuited ? "SUITED THREE OF A KIND!" : "THREE OF A KIND!"),
							(fourOfAKind ? "FOUR " : "MATCH DOWN  ") + rankPlural(dealerDown.getValue()),
							payout);
					}
				} else{
					queueChipCollection(i, wager, sideBetSpot(i, false));
					matchDownResult[i] = HandResult::Loss;
				}
			}
		}

	}

	// Spanish 21's payout rules, used by resolveRound() in place of the
	// standard branch above whenever isPlayersEdge(gameMode). The one
	// defining difference: a player 21 always wins outright, no matter what
	// the dealer has (including another 21) -- standard blackjack's "equal
	// totals push" never applies to a made 21 here. On top of that, a 21
	// pays a bonus scaled by how it was made: 5/6/7+-card 21s pay
	// progressively more, and a 6-7-8 or 7-7-7 (of any rank order) pays
	// 3:2/2:1/3:1 by suit (mixed/same-suit/all-spades). Below 21, resolution
	// is identical to standard blackjack (bust loses, better total or a
	// dealer bust wins 1:1, tied totals push).
	//
	// Not implemented: the Super Bonus (suited 7-7-7 specifically against a
	// dealer up-card of 7) pays a flat cash amount tiered by original bet
	// size, not a documented single table -- left out rather than guessed;
	// and double-down rescue (surrendering just a bad double for the
	// original bet back) has no player action in this game at all yet, so
	// there's nothing here to hook it into.
	int spanish21Credit(Hand& hand, int dealerTotal, bool dealerBust, bool dealerBlackjack){
		int bet = hand.getBet();
		int total = hand.getHandTotal();
		if(hand.isBust() || total > 21)
			return 0;

		if(total == 21){
			int cardCount = hand.getHandSize();

			// Washington's Player's Edge 21 rules: the 21 bonuses pay
			// after splitting but not after doubling -- a doubled 21 is a
			// plain 1:1 win (on the doubled bet).
			if(hand.getDoubleCount() > 0)
				return bet * 2;

			std::vector<int> values, suits;
			for(Card& c : hand.cards){
				values.push_back(c.getValue());
				suits.push_back(c.getSuit());
			}
			std::vector<int> sortedValues = values;
			std::sort(sortedValues.begin(), sortedValues.end());

			bool is678 = cardCount == 3 && sortedValues[0] == 6 && sortedValues[1] == 7 && sortedValues[2] == 8;
			bool is777 = cardCount == 3 && sortedValues[0] == 7 && sortedValues[1] == 7 && sortedValues[2] == 7;

			if(is678 || is777){
				bool allSameSuit = suits[0] == suits[1] && suits[1] == suits[2];
				// Clearwater pays the top tier on all diamonds.
				bool allDiamonds = allSameSuit && suits[0] == 2; // 2 = diamond, Card.h
				if(allDiamonds)
					return bet + bet * 3;
				if(allSameSuit)
					return bet + bet * 2;
				return bet + bet * 3 / 2;
			}

			if(cardCount >= 7)
				return bet + bet * 3;
			if(cardCount == 6)
				return bet + bet * 2;
			if(cardCount == 5)
				return bet + bet * 3 / 2;
			if(cardCount == 2 && !hand.isFromSplit())
				return bet + bet * 3 / 2; // natural (a split hand's A+10 is plain 21)
			return bet * 2; // an unremarkable 3-4 card 21
		}

		if(dealerBlackjack)
			return 0;
		if(dealerBust || total > dealerTotal)
			return bet * 2;
		if(total == dealerTotal)
			return bet;
		return 0;
	}

	// Free Bet Blackjack's one rule difference from standard blackjack:
	// "Push 22" -- a dealer bust with a total of *exactly* 22 pushes every
	// surviving player hand (any total 21 or under) instead of paying it,
	// which is what funds the free doubles/splits (see
	// isFreeDoubleEligible()/isFreeSplitEligible(), onDouble()/onSplit()).
	// A dealer bust at 23+ still pays normally. Player blackjack is exempt
	// from Push 22 entirely -- it pays 3:2 (or pushes a true dealer
	// blackjack) regardless of the dealer's eventual total, verified
	// against wizardofodds.com's Free Bet Blackjack page rather than
	// guessed. Returned credit is always computed on the hand's *full*
	// bet, same as every other mode's credit function -- the caller
	// (resolveRound()) is what adjusts the actual paid/collected amount
	// down to just the real (non-free) portion on a loss or push, via
	// Hand::getRealBet().
	int freeBetCredit(Hand& hand, int dealerTotal, bool dealerBust, bool dealerBlackjack){
		int bet = hand.getBet();
		int total = hand.getHandTotal();
		if(hand.isBust() || total > 21)
			return 0;

		bool blackjack = hand.getHandSize() == 2 && total == 21 && !hand.isFromSplit();
		if(dealerBlackjack && blackjack)
			return bet;
		if(dealerBlackjack)
			return 0;
		if(blackjack)
			return bet + bet * 3 / 2;

		if(dealerBust){
			if(dealerTotal == 22)
				return bet; // Push 22
			return bet * 2;
		}

		if(total > dealerTotal)
			return bet * 2;
		if(total == dealerTotal)
			return bet;
		return 0;
	}

	static std::string rankName(int value){
		switch(value){
			case 1: return "A";
			case 11: return "J";
			case 12: return "Q";
			case 13: return "K";
			default: return std::to_string(value);
		}
	}

	// Plural for "PAIR OF KINGS" / "FOUR 7S" style detail lines.
	static std::string rankPlural(int value){
		switch(value){
			case 1: return "ACES";
			case 11: return "JACKS";
			case 12: return "QUEENS";
			case 13: return "KINGS";
			default: return std::to_string(value) + "S";
		}
	}

	static std::string suitName(int suit){
		switch(suit){
			case 0: return "SPADES";
			case 1: return "CLUBS";
			case 2: return "DIAMONDS";
			default: return "HEARTS";
		}
	}

	void recordSideBet(Stats::SideBet bet, int wager, int credit){
		if(stats)
			stats->recordSideBet(bet, wager, credit);
	}

	// Announces a rare side-bet hit -- only the long-shot tiers, not every
	// side-bet win, so it still means something when it shows up: Lucky
	// Ladies' matched 20 (19:1, a Queen of Hearts pair 125:1 or 1000:1),
	// and Player's Edge three of a kind (a pair matching a dealer card)
	// or four of a kind (a pair matching both). payout is the full credit
	// coming back (wager + winnings).
	void queueJackpotCallout(int playerIndex, const std::string& title, const std::string& what, int payout){
		if(stats)
			stats->bump(Stats::JackpotHits);
		jackpotCallouts.push_back(JackpotCallout{
			.title = title,
			.detail = "P" + std::to_string(playerIndex + 1) + "  " + what + "  +" + std::to_string(payout),
		});
	}

	// Two suited/unsuited matches == the player's own pair plus the dealer
	// card, i.e. three of a kind (see evaluateMatchBet()).
	int matchCount(int playerIndex, Card& dealerCard, bool& allSuited){
		const InitialTwoCards& ic = initialTwoCards[playerIndex];
		allSuited = true;
		if(!ic.valid)
			return 0;
		int count = 0;
		int values[2] = { ic.value1, ic.value2 };
		int suits[2] = { ic.suit1, ic.suit2 };
		for(int k = 0; k < 2; k++){
			if(values[k] != dealerCard.getValue())
				continue;
			count++;
			if(suits[k] != dealerCard.getSuit())
				allSuited = false;
		}
		return count;
	}

	void drawJackpotCallout(SDLState& state){
		if(jackpotCallouts.empty())
			return;

		const JackpotCallout& c = jackpotCallouts.front();
		float t = c.elapsed;

		// Pops open over the first 0.25s, fades over the last 0.4s.
		float open = std::min(1.0f, t / 0.25f);
		float fade = std::clamp((CALLOUT_DURATION - t) / 0.4f, 0.0f, 1.0f);
		Uint8 alpha = (Uint8)(255 * fade);

		float fullH = 190.0f;
		float h = fullH * open;
		SDL_FRect band{ .x = 0, .y = 360.0f - h / 2.0f, .w = 1440, .h = h };

		SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(state.renderer, 10, 10, 10, (Uint8)(238 * fade));
		SDL_RenderFillRect(state.renderer, &band);

		// Border flashes gold/red five times a second.
		bool flash = ((int)(t * 5.0f)) % 2 == 0;
		SDL_Color border = flash ? SDL_Color{255, 210, 40, alpha} : SDL_Color{230, 50, 50, alpha};
		SDL_SetRenderDrawColor(state.renderer, border.r, border.g, border.b, border.a);
		for(int i = 0; i < 6; i++){
			SDL_FRect top{ .x = 0, .y = band.y + i, .w = 1440, .h = 1 };
			SDL_FRect bottom{ .x = 0, .y = band.y + band.h - 1 - i, .w = 1440, .h = 1 };
			SDL_RenderFillRect(state.renderer, &top);
			SDL_RenderFillRect(state.renderer, &bottom);
		}

		if(open >= 1.0f){
			float titlePixel = 11.0f;
			float titleW = DigitFont::textWidth(c.title, titlePixel);
			if(titleW > 1340.0f){
				titlePixel *= 1340.0f / titleW;
				titleW = DigitFont::textWidth(c.title, titlePixel);
			}
			SDL_Color titleColor = flash ? SDL_Color{255, 225, 80, alpha} : SDL_Color{255, 255, 255, alpha};
			DigitFont::drawText(state, c.title, (1440.0f - titleW) / 2.0f, band.y + 38.0f, titlePixel, titleColor);

			float detailPixel = 5.0f;
			float detailW = DigitFont::textWidth(c.detail, detailPixel);
			DigitFont::drawText(state, c.detail, (1440.0f - detailW) / 2.0f, band.y + 38.0f + 5 * titlePixel + 28.0f, detailPixel, SDL_Color{230, 230, 230, alpha});
		}
		SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_NONE);
	}

	// Lucky Ladies, Clearwater Casino's pay table: any first-two-card 20
	// wins, soft 20 (A-9) included --
	//                                              2 deck   6 deck
	//   Queen of Hearts pair, dealer has blackjack  1000:1   1000:1
	//   Queen of Hearts pair                         200:1    125:1
	//   matched 20 (same rank and suit)               25:1     19:1
	//   suited 20                                     10:1      9:1
	//   any other 20                                   4:1      4:1
	// Only the best tier pays. Returns the total credit (wager +
	// winnings), or 0 if the hand isn't a 20.
	int evaluateLuckyLadies(int playerIndex, int wager){
		const InitialTwoCards& c = initialTwoCards[playerIndex];
		if(!c.valid)
			return 0;

		auto points = [](int v){ return v == 1 ? 11 : (v > 10 ? 10 : v); };
		if(points(c.value1) + points(c.value2) != 20)
			return 0;

		bool suited = c.suit1 == c.suit2;
		bool matched = suited && c.value1 == c.value2;
		bool queenOfHeartsPair = matched && c.value1 == 12 && c.suit1 == 3;

		// The 2-deck game pays more on the top three tiers.
		bool twoDeck = numberOfDecks <= 2;
		if(queenOfHeartsPair)
			return wager + wager * (dealerHasBlackjack() ? 1000 : (twoDeck ? 200 : 125));
		if(matched)
			return wager + wager * (twoDeck ? 25 : 19);
		if(suited)
			return wager + wager * (twoDeck ? 10 : 9);
		return wager + wager * 4;
	}

	// The dealer's two cards make a blackjack (hole card included, whether
	// or not it's been turned over yet).
	bool dealerHasBlackjack(){
		if(dealer.hands[0].getHandSize() != 2)
			return false;
		auto tier = [](int v){ return v > 10 ? 10 : v; };
		int a = tier(dealer.hands[0].cards[0].getValue()), b = tier(dealer.hands[0].cards[1].getValue());
		return (a == 1 && b == 10) || (a == 10 && b == 1);
	}

	// Shared by Match Up (vs. the dealer's up-card) and Match Down (vs. the
	// dealer's hole card, once revealed) -- checks each of the player's
	// first two cards against one dealer card, tallies rank matches (split
	// into suited vs. unsuited), and looks up the payout. Tiers are one
	// specific point picked from the approved range in 58 Pa. Code Sec
	// 635c.2 (that regulation gives the win condition exactly, but only a
	// range for the payout -- no single universal paytable is published
	// the way Lucky Ladies has one): 20:1 two suited matches, 14:1 one
	// suited + one unsuited, 11:1 one suited match alone, 7:1 two unsuited
	// matches, 4:1 one unsuited match alone. Returns 0 (a clean loss) if
	// neither card matches at all.
	int evaluateMatchBet(int playerIndex, Card& dealerCard, int wager){
		const InitialTwoCards& ic = initialTwoCards[playerIndex];
		if(!ic.valid)
			return 0;

		int cardValues[2] = { ic.value1, ic.value2 };
		int cardSuits[2] = { ic.suit1, ic.suit2 };

		int suitedMatches = 0;
		int unsuitedMatches = 0;
		for(int k = 0; k < 2; k++){
			if(cardValues[k] != dealerCard.getValue())
				continue;
			if(cardSuits[k] == dealerCard.getSuit())
				suitedMatches++;
			else
				unsuitedMatches++;
		}

		// Clearwater's Player's Edge 21 Match the Dealer table.
		int multiplier = 0;
		if(suitedMatches == 2)
			multiplier = 18;
		else if(suitedMatches == 1 && unsuitedMatches == 1)
			multiplier = 13;
		else if(suitedMatches == 1)
			multiplier = 9;
		else if(unsuitedMatches == 2)
			multiplier = 8;
		else if(unsuitedMatches == 1)
			multiplier = 4;

		if(multiplier == 0)
			return 0;
		return wager + wager * multiplier;
	}

	// Lucky Stiff, verified against wizardofodds.com's Lucky Stiff page
	// rather than guessed: a starting blackjack pays 1:1; a "stiff pair"
	// (two 6s, two 7s, or two 8s) pays 10:1, regardless of what the dealer
	// has -- both resolve immediately. An unpaired hard 12-16 (ace counted
	// as 1, so e.g. A-5 is a hard 6, not a hard 16, and doesn't qualify)
	// stays live and pays 5:1 if the main hand goes on to beat the dealer,
	// pushes if it ties, loses if it doesn't -- pending is set true for
	// this case so resolveSideBets() knows to leave it for resolveRound()
	// instead of resolving it here. Everything else (any other made hand
	// under 12 or over 16 that isn't a blackjack) loses immediately.
	int evaluateLuckyStiffImmediate(int playerIndex, int wager, bool& pending){
		pending = false;

		const InitialTwoCards& c = initialTwoCards[playerIndex];
		if(!c.valid)
			return 0;

		// Face cards collapse to 10; an Ace deliberately stays 1 here (not
		// 11) so this sum is the hand's true *hard* total, matching what
		// "hard 12-16" means.
		auto tier = [](int v){ return v > 10 ? 10 : v; };
		int t1 = tier(c.value1);
		int t2 = tier(c.value2);

		bool isBlackjack = (c.value1 == 1 && t2 == 10) || (c.value2 == 1 && t1 == 10);
		if(isBlackjack)
			return wager + wager * 1;

		bool sameRank = c.value1 == c.value2;
		bool isStiffPair = sameRank && (c.value1 == 6 || c.value1 == 7 || c.value1 == 8);
		if(isStiffPair)
			return wager + wager * 10;

		int hardTotal = t1 + t2;
		if(!sameRank && hardTotal >= 12 && hardTotal <= 16){
			pending = true;
			return 0;
		}

		return 0;
	}

	// Fires right after the initial deal lands (see awaitingInitialDeal in
	// update()) -- Lucky Ladies and Match Up both only need the player's
	// own first two cards plus the dealer's already-shown up-card, so both
	// can pay out immediately instead of waiting for the round to finish.
	// Match Down is the odd one out (needs the hole card revealed) and is
	// resolved separately, from resolveRound() itself.
	void resolveSideBets(){
		// Snapshot every seated player's original first two cards up front
		// -- taken unconditionally (cheap), used by both this function and
		// resolveRound()'s later Match Down check. See InitialTwoCards.
		for(int i = 0; i < numberOfPlayers; i++){
			initialTwoCards[i] = InitialTwoCards{};
			if(players[i].getBet() <= 0)
				continue;

			std::vector<Card>& cards = players[i].hands[0].cards;
			if(cards.size() < 2)
				continue;

			initialTwoCards[i] = InitialTwoCards{
				.valid = true,
				.suit1 = cards[0].getSuit(),
				.value1 = cards[0].getValue(),
				.suit2 = cards[1].getSuit(),
				.value2 = cards[1].getValue()
			};
		}

		for(int i = 0; i < numberOfPlayers; i++){
			if(players[i].getBet() <= 0)
				continue;

			if(hasLuckyLadies(gameMode)){
				int wager = players[i].getSideBet();
				if(wager <= 0)
					continue;

				int payout = evaluateLuckyLadies(i, wager);
				recordSideBet(Stats::LuckyLadies, wager, payout);
				if(payout > 0){
					queueChipPayout(i, payout, false, wager, sideBetSpot(i, true));
					sideBetResult[i] = HandResult::Win;

					const InitialTwoCards& c = initialTwoCards[i];
					if(c.value1 == c.value2 && c.suit1 == c.suit2){
						bool queenOfHearts = c.value1 == 12 && c.suit1 == 3;
						queueJackpotCallout(i,
							queenOfHearts ? (dealerHasBlackjack() ? "QUEENS + DEALER BLACKJACK!" : "QUEEN OF HEARTS PAIR!") : "MATCHED TWENTY!",
							"PAIR OF " + rankPlural(c.value1) + " OF " + suitName(c.suit1),
							payout);
					}
				} else{
					queueChipCollection(i, wager, sideBetSpot(i, true));
					sideBetResult[i] = HandResult::Loss;
				}
			} else if(isPlayersEdge(gameMode)){
				int wager = players[i].getMatchUpBet();
				if(wager <= 0 || dealer.hands[0].getHandSize() < 1)
					continue;

				Card& dealerUp = dealer.hands[0].cards[0];
				int payout = evaluateMatchBet(i, dealerUp, wager);
				recordSideBet(Stats::MatchUp, wager, payout);
				bool allSuited = false;
				int matches = matchCount(i, dealerUp, allSuited);
				if(payout > 0){
					queueChipPayout(i, payout, false, wager, sideBetSpot(i, true));
					matchUpResult[i] = HandResult::Win;

					if(matches == 2)
						queueJackpotCallout(i,
							allSuited ? "SUITED THREE OF A KIND!" : "THREE OF A KIND!",
							"MATCH UP  " + rankPlural(dealerUp.getValue()),
							payout);
				} else{
					queueChipCollection(i, wager, sideBetSpot(i, true));
					matchUpResult[i] = HandResult::Loss;
				}
			} else if(hasLuckyStiff(gameMode)){
				int wager = players[i].getSideBet();
				if(wager <= 0)
					continue;

				bool pending = false;
				int payout = evaluateLuckyStiffImmediate(i, wager, pending);
				if(!pending)
					recordSideBet(Stats::LuckyStiff, wager, payout);
				if(pending){
					luckyStiffPending[i] = true;
				} else if(payout > 0){
					queueChipPayout(i, payout, false, wager, sideBetSpot(i, true));
					sideBetResult[i] = HandResult::Win;
				} else{
					queueChipCollection(i, wager, sideBetSpot(i, true));
					sideBetResult[i] = HandResult::Loss;
				}
			}
		}
	}


	// Stage lengths for ChipAnimation (seconds).
	static constexpr float CHIPS_TO_SPOT = 0.75f;   // winnings tray -> beside the bet
	static constexpr float CHIPS_HOLD = 1.1f;       // both stacks sit on the felt
	static constexpr float CHIPS_TO_BANKROLL = 0.75f;
	static constexpr float CHIPS_TO_TRAY = 0.75f;

	float chipAnimationLength(const ChipAnimation& a) const {
		switch(a.kind){
			case ChipAnimation::Win:  return CHIPS_TO_SPOT + CHIPS_HOLD + CHIPS_TO_BANKROLL;
			case ChipAnimation::Push: return CHIPS_HOLD + CHIPS_TO_BANKROLL;
			default:                  return CHIPS_TO_TRAY;
		}
	}

	// Where a seat's bankroll is shown along the top -- winnings fly there.
	SDL_FPoint bankrollPoint(int playerIndex){
		if(bankrollLabelCenter[playerIndex].x > 0.0f)
			return bankrollLabelCenter[playerIndex];
		return SDL_FPoint{ seatCenterX(playerIndex), 25.0f };
	}

	// Where a hand's main bet chips sit: the bottom-left corner of its
	// spot (drawMainBetChips()).
	SDL_FPoint betSpot(int playerIndex, int hand){
		float out = SPOT_MARGIN + 4.0f;
		return seatPoint(playerIndex, hand, -out, cardHeight + out);
	}

	// A side bet's circle: top right (first side bet) or top left (Match Down).
	SDL_FPoint sideBetSpot(int playerIndex, bool topRight){
		float out = SPOT_MARGIN + 4.0f;
		return seatPoint(playerIndex, 0, topRight ? cardWidth + out : -out, -out);
	}

	// Pays a bet out (credit = everything that comes back, stake included;
	// isPush when that's just the stake). stake is the bet's own chips,
	// already sitting at spot; the rest is winnings brought from the tray.
	// Without a stake (a bonus nobody bet on), it's all winnings. The tray
	// column the winnings come from drops a layer right away.
	void queueChipPayout(int playerIndex, int credit, bool isPush, int stake, SDL_FPoint spot){
		sound(Sfx::ChipsPay);
		int winnings = isPush ? 0 : std::max(0, credit - stake);
		// Every chip of the winnings comes out of its own column.
		std::vector<int> chips = chipsFor(winnings);
		int col = firstColumnForDenom(chips.empty() ? 0 : chips.front());
		for(int d : chips)
			takeFromTray(d);

		chipAnimations.push_back(ChipAnimation{
			.kind = isPush ? ChipAnimation::Push : ChipAnimation::Win,
			.spot = spot,
			.tray = trayColumnBottom(col),
			.bankroll = bankrollPoint(playerIndex),
			.stake = isPush ? credit : std::min(stake, credit),
			.winnings = winnings,
			.columnIndex = col,
			.playerIndex = playerIndex,
			.creditAmount = credit,
			.isPush = isPush
		});
	}

	// A lost bet: its chips are taken from spot to the tray. creditAmount
	// is 0 -- the bet already left the bankroll at deal time. The tray
	// column only gets its layer back once the chips arrive.
	void queueChipCollection(int playerIndex, int amount, SDL_FPoint spot){
		sound(Sfx::ChipsTake);
		std::vector<int> chips = chipsFor(amount);
		int col = firstColumnForDenom(chips.empty() ? 0 : chips.front());
		chipAnimations.push_back(ChipAnimation{
			.kind = ChipAnimation::Loss,
			.spot = spot,
			.tray = trayColumnBottom(col),
			.bankroll = bankrollPoint(playerIndex),
			.stake = amount,
			.columnIndex = col,
			.playerIndex = playerIndex,
			.creditAmount = 0
		});
	}

	void checkBreak(bool forceStandIfNotBust = false){
		if(players[activePlayer].checkBreak()){
			// Give the player a moment to see the busted hand before its
			// cards get pulled off to the discard pile.
			pauseTimer = BUST_PAUSE_DURATION;
			onPauseComplete = [this](){
				// The dealer takes a bust's chips straight away, along
				// with its cards (resolveRound() still settles the stats).
				{
					Person& p = players[activePlayer];
					int h = p.getActiveHand();
					if(h < p.hands.size() && p.hands[h].getBet() > 0 && !p.hands[h].isChipsCollected()){
						queueChipCollection(activePlayer, p.hands[h].getRealBet(), betSpot(activePlayer, h));
						p.hands[h].markChipsCollected();
					}
				}
				busted();
				if(players[activePlayer].getActiveHand() > players[activePlayer].hands.size() - 1)
					activePlayer++;
				skipZeroBetPlayers();
			};
		} else {
			// Still dealing the opening cards (or waiting on the peek /
			// insurance): nothing is decided yet. A natural plays out once
			// its turn comes up (update()'s activeHandIsTwentyOne()).
			if(awaitingPeek || awaitingInsurance)
				return;

			// A 21 -- natural or built up to over a few hits -- auto-stands
			// instead of waiting on a hit/stand gesture: no legal move ever
			// improves it and hitting again can only bust, so there's
			// nothing for the player to decide. This is what actually pays
			// out a Player's Edge 21 promptly instead of leaving it sitting
			// in "your turn" limbo (resolveRound() already handles the
			// payout math correctly; it just never used to get reached
			// until the player manually stood).
			int handIdx = players[activePlayer].getActiveHand();
			bool reachedTwentyOne = handIdx < players[activePlayer].hands.size()
				&& players[activePlayer].hands[handIdx].getHandTotal() == 21;

			// Any Player's Edge 21 is paid the moment it's made (its
			// payout never depends on the dealer's hand -- see
			// spanish21Credit()'s total==21 branch). Naturals were already
			// paid right after the peek (payNaturals()).
			if(reachedTwentyOne && isPlayersEdge(gameMode))
				payTwentyOne(activePlayer, handIdx);

			// Standard blackjack only allows one double per hand, which is
			// why a double has always force-stood here -- Player's Edge
			// (Spanish 21) specifically allows "double, double, double"
			// (redoubling as many times as the player likes), so a double
			// there just deals the card and leaves the hand active for
			// another hit/double/stand, same as any other card. A 21 still
			// always auto-stands regardless of mode (see above).
			// Split aces get one card each (except Player's Edge -- see
			// allowsHitSplitAces()), so the second card ends the hand.
			bool splitAceDone = handIdx < players[activePlayer].hands.size()
				&& players[activePlayer].hands[handIdx].isSplitAces()
				&& players[activePlayer].hands[handIdx].getHandSize() >= 2
				&& !allowsHitSplitAces();

			// Player's Edge redoubling tops out at 3 doubles -- after the
			// third, standing is the only move left.
			bool doublesUsedUp = forceStandIfNotBust && isPlayersEdge(gameMode)
				&& handIdx < players[activePlayer].hands.size()
				&& players[activePlayer].hands[handIdx].getDoubleCount() >= MAX_PLAYERS_EDGE_DOUBLES;

			bool forceStand = reachedTwentyOne || splitAceDone || doublesUsedUp || (forceStandIfNotBust && !isPlayersEdge(gameMode));

			if(forceStand){
				activePlayer += players[activePlayer].stand();
				skipZeroBetPlayers();
			}
		}
	}

	// The yellow cut card (value 14, see makeShoe()) is a shoe marker, not
	// a real card -- it should never end up in anyone's hand. If it comes
	// up, quietly burn it straight to the discard pile (no flight
	// animation; a real cut card isn't shown to players either) and return
	// the actual next card instead. Reaching it also means "reshuffle once
	// this hand is over" -- see shoeNeedsReshuffle, consumed in update()
	// once the round actually finishes.
	Card getNextCard(){
		Card c = shoe.front();
		shoe.erase(shoe.begin());

		if(c.getValue() == 14){
			// Flies face-up from the shoe to the discard pile (queued
			// ahead of whatever card the caller is about to deal, so it
			// goes first) and stays on top of the pile, yellow, until the
			// next discard covers it -- a visible "reshuffle after this
			// round" signal instead of vanishing silently.
			shoeNeedsReshuffle = true;
			c.showCard(true);
			c.setRotation(0);
			dealQueue.push(DealRequest{
				.playerIndex = -1,
				.isDealer = true,
				.discard = true,
				.showCard = true,
				.from = shoePosition,
				.to = discardPosition,
				.card = c,
			});
			return getNextCard();
		}

		return c;
	}

	void busted(){
		std::vector<Card> busted = players[activePlayer].busted();

		for(Card c : busted){
			dealQueue.push(DealRequest{
				.playerIndex = activePlayer,
				.isDealer = false,
				.discard = true,
				.showCard = false,
				.from = c.getPosition(),
				.to = discardPosition,
				.card = c,
			});
		}
	}
};