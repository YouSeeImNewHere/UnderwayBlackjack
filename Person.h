#pragma once
#include "Hand.h"

#include <vector>
#include <algorithm>
#include <cmath>

const float arrowWidth = 40;
const float arrowHeight = 28;

struct offSets{
	float xMoveHand = 0;
	float yMoveHand = 0;
	float xMoveCard = 0;
	float yMoveCard = 0;
	float rotation = 0;
	float arrowRotaion = 0;
};

class Person
{
public:
	std::vector<Hand> hands;

	Person(){
		hands.push_back(Hand());
		isPlayer = false;
		hands[0].setBaseRotation(calcOffset().rotation);
	}

	Person(bool isPlayer,BettingSquare bettingSquare){
		this->isPlayer = isPlayer;
		this->bettingSquare = bettingSquare;
		hands.push_back(Hand());
		hands[0].setBaseRotation(calcOffset().rotation);
	}

	// hideIfInactive: GameOptionsMenu's "hide inactive hands" house rule,
	// applied only to seats this is turned on for (Table.h never passes
	// true for the dealer's own draw call -- the dealer already has its
	// own separate hole-card mechanic). Combined with activePlayer here
	// rather than by the caller, since this is already the one place that
	// knows whether this specific seat is the active one.
	void draw(SDLState& state,Resources& res,bool activePlayer,bool hideIfInactive = false){
		offSets adj = calcOffset();
		bool forceHidden = hideIfInactive && !activePlayer;

		// Was the same dir-bucket-only cardinal math as calcOffset()'s own
		// stacking offsets used to be (and, same as there, verified to be
		// exactly this local (dir==0) template rotated by the bucket's own
		// cardinal angle) -- rotating by the seat's actual stored angle
		// instead generalizes it to the 2 new 5-player seats' non-cardinal
		// tilt the same way. Without this, the arrow's own rotation was
		// already correct (adj.rotation, not a bucket angle) but its
		// position wasn't, off in a spot that no longer reads as "pointing
		// at this hand" once the two disagree.
		constexpr float PI = 3.14159265358979323846f;
		float rad = adj.rotation * PI / 180.0f;
		float cosT = std::cos(rad), sinT = std::sin(rad);
		float localX = -(arrowWidth + 15.0f);
		float localY = (cardHeight - arrowHeight) / 2.0f;

		SDL_FRect arrow{
			.x = bettingSquare.firstPoint().x + (localX * cosT - localY * sinT),
			.y = bettingSquare.firstPoint().y + (localX * sinT + localY * cosT),
			.w = arrowWidth,
			.h = arrowHeight
		};

		for(int hand = 0; hand < hands.size(); hand++){
			hands[hand].draw(state,res,forceHidden);

			if(activePlayer && activeHand == hand)
				SDL_RenderTextureRotated(state.renderer,res.arrow,nullptr,&arrow,adj.rotation,&rotationTopLeft,SDL_FLIP_NONE);
			arrow.x += adj.xMoveHand;
			arrow.y += adj.yMoveHand;
		}

	}

	// The new hand starts with the same stake as the one being split off
	// of -- Table::onSplit() is what actually pulls that amount out of
	// bankroll to cover it.
	Card split(){
		int bet = hands[activeHand].getBet();
		addBlankHand();
		hands[activeHand + 1].setBet(bet);
		return getLastCardAndRemove();
	}

	void addCard(Card card,bool showCard, bool split = false, bool doubleHand = false){
		if(split)
			hands[activeHand + 1].addCard(card,showCard,doubleHand);
		else
			hands[activeHand].addCard(card,showCard,doubleHand);
	}

	int stand(){
		activeHand++;
		return activeHand >= hands.size();
	}

	SDL_FPoint getNextCardPosition(int futureCards = 0){
		offSets adj = calcOffset();

		float x = bettingSquare.firstPoint().x + adj.xMoveHand * activeHand + adj.xMoveCard * (hands[activeHand].getHandSize() + futureCards);
		float y = bettingSquare.firstPoint().y + adj.yMoveHand * activeHand + adj.yMoveCard * (hands[activeHand].getHandSize() + futureCards);

		return SDL_FPoint(x,y);
	}

	SDL_FPoint getNextCardPositionOfBlankHand(){
		offSets adj = calcOffset();
		int hand = activeHand + 1;

		float x = bettingSquare.firstPoint().x + adj.xMoveHand * hand;
		float y = bettingSquare.firstPoint().y + adj.yMoveHand * hand;

		return SDL_FPoint(x,y);
	}

	std::vector<Card> busted(){
		std::vector r = hands[activeHand].busted();
		activeHand++;
		return r;
	}

	void surrenderActiveHand(){
		hands[activeHand].markSurrendered();
	}


	void discardOneCard(int handIndex){
		hands[handIndex].removeLast();

		bool allEmpty = true;
		for(int i = 0; i < hands.size(); i++){
			if(hands[i].getHandSize() > 0){
				allEmpty = false;
				break;
			}
		}

		if(allEmpty){
			hands.clear();
			hands.push_back(Hand());
			hands[0].setBaseRotation(calcOffset().rotation);
			activeHand = 0;
		}
	}

	bool checkBreak(){
		return hands[activeHand].checkIfBreak();
	}

	// Instantly back to one empty hand, no animation -- same end state as
	// discardOneCard()'s own "hand's all empty" reset above, but callable
	// directly. Used by Table::resetForNewGame() (see there for why
	// clearTable()'s animated discard sweep can't be reused for a Restart).
	void resetHands(){
		hands.clear();
		hands.push_back(Hand());
		hands[0].setBaseRotation(calcOffset().rotation);
		activeHand = 0;
	}

	int getDirection(){
		return bettingSquare.getDirection();
	}

	float getSeatRotation(){
		return bettingSquare.getRotationDegrees();
	}

	int getActiveHand(){
		return activeHand;
	}

	// Starting bankroll only, set once by the setup screen before a game
	// begins -- actually wagering/winning/losing it during play is separate,
	// not-yet-built work.
	// Only ever called once, from Table::configurePlayers() -- the buy-in
	// moment -- so this doubles as stamping both initialBankroll (the
	// reference point drawBankrolls() colors the live number against: red
	// below it, green above) and buyInAmount (the fixed amount rebuy()
	// adds each time).
	void setBankroll(int amount){
		bankroll = amount;
		initialBankroll = amount;
		buyInAmount = amount;
	}

	int getBankroll(){
		return bankroll;
	}

	// Resume (SaveData): put back a saved game's live bankroll and its
	// break-even point, after setBankroll() has stamped the seat's
	// original buy-in (which is still what a rebuy() adds).
	void restoreBankroll(int current, int totalBuyIns){
		bankroll = current;
		initialBankroll = totalBuyIns;
	}

	// The break-even reference drawBankrolls() colors against -- despite
	// the name, this is the running total of every buy-in so far (the
	// original one plus every rebuy()), not just the first one. A player
	// who's rebought should stay red until their current bankroll covers
	// everything they've ever put in, not just the original amount.
	int getInitialBankroll(){
		return initialBankroll;
	}

	// Bankrupt (bankroll == 0) but still seated -- adds another buy-in
	// worth of chips, same fixed amount as the original one (buyInAmount,
	// stamped once by setBankroll() and never itself changed by a rebuy).
	// initialBankroll rises by that same amount, since it now needs to
	// cover this new money too before drawBankrolls() calls the player
	// break-even again.
	void rebuy(){
		bankroll += buyInAmount;
		initialBankroll += buyInAmount;
	}

	// Sets the starting bet -- called once from Table::configurePlayers()
	// with whatever was configured on SetupMenu (or loaded from a saved
	// game), replacing the field's own hardcoded default. Bets chosen
	// afterward during actual play go through raiseBet()/lowerBet() below.
	void setInitialBet(int amount){
		currentBet = amount;
	}

	// Same idea as setInitialBet(), for whichever side bet(s) apply --
	// called once from Table::configurePlayers() with whatever SetupMenu's
	// side-bet stepper (or a loaded save) had configured.
	void setInitialSideBet(int amount){
		sideBet = amount;
	}

	// Player's Edge has two independent side bets; SetupMenu's calculator
	// only exposes one "side bet size" control (per the original ask), so
	// both start at the same amount -- raiseMatchUpBet()/lowerMatchDownBet()
	// etc. still let them diverge from there during actual play.
	void setInitialMatchBets(int amount){
		matchUpBet = amount;
		matchDownBet = amount;
	}

	// The bet chosen for the round about to be dealt -- set during Table's
	// betting phase (see Table::drawBetting()/handleBettingPoint()), before
	// firstDeal() runs. Clamped to [0, bankroll].
	void raiseBet(int amount){
		currentBet = std::min(bankroll, currentBet + amount);
	}

	void lowerBet(int amount){
		currentBet = std::max(0, currentBet - amount);
	}

	int getBet(){
		return currentBet;
	}

	// Called once by Table::beginRound() when the round actually starts.
	void deductBet(){
		bankroll -= currentBet;
	}

	// Side bets -- sideBet is Lucky Ladies' single wager; matchUpBet/
	// matchDownBet are Player's Edge's two independent ones (see
	// GameModeMenu.h's hasLuckyLadies()/isPlayersEdge()). All three mirror
	// currentBet's own raise/lower/deduct shape, including persisting
	// round-to-round rather than resetting to 0 -- same "the player's last
	// choice is probably still what they want" reasoning as the main bet.
	int getSideBet(){ return sideBet; }
	void raiseSideBet(int amount){ sideBet = std::min(bankroll, sideBet + amount); }
	void lowerSideBet(int amount){ sideBet = std::max(0, sideBet - amount); }

	int getMatchUpBet(){ return matchUpBet; }
	void raiseMatchUpBet(int amount){ matchUpBet = std::min(bankroll, matchUpBet + amount); }
	void lowerMatchUpBet(int amount){ matchUpBet = std::max(0, matchUpBet - amount); }

	int getMatchDownBet(){ return matchDownBet; }
	void raiseMatchDownBet(int amount){ matchDownBet = std::min(bankroll, matchDownBet + amount); }
	void lowerMatchDownBet(int amount){ matchDownBet = std::max(0, matchDownBet - amount); }

	// Called alongside deductBet() -- pulls whichever of the three side
	// bets are actually in play for this game mode out of bankroll up
	// front, same timing as the main bet.
	void deductSideBets(){
		bankroll -= (sideBet + matchUpBet + matchDownBet);
	}

	// Table::clampBetsToBankroll() calls this first, before ever touching
	// the main bet, when the combined wager would exceed bankroll -- side
	// bets are optional extras, so they're the ones given up, not the hand
	// itself.
	void zeroSideBets(){
		sideBet = 0;
		matchUpBet = 0;
		matchDownBet = 0;
	}

	// Direct setter, not relative like raiseBet()/lowerBet() -- used by
	// Table::clampBetsToBankroll() to pin the main bet down to exactly
	// whatever's left of the bankroll.
	void setBetDirect(int amount){
		currentBet = amount;
	}

	// Called once a payout chip animation actually lands -- see
	// Table::resolveRound()/queueChipPayout().
	void credit(int amount){
		bankroll += amount;
	}

	// Stamps the round's bet onto the fresh hand firstDeal() is about to
	// fill -- called right after deductBet(), before any cards land. From
	// here on, the hand's own bet (not currentBet) is the source of truth,
	// since a double/split can make it diverge from the round bet.
	void startRoundBet(){
		hands[0].setBet(currentBet);
	}

	int getActiveHandBet(){
		return hands[activeHand].getBet();
	}

	// Doubling and splitting both wager an extra amount beyond the
	// original bet -- this is that extra leaving the bankroll. Doesn't
	// check there's enough left; same not-yet-built scope as bankroll
	// going negative anywhere else right now.
	void deductFromBankroll(int amount){
		bankroll -= amount;
	}

	void doubleActiveHandBet(){
		hands[activeHand].doubleBet();
	}

	// Anchor point Table positions this seat's betting controls relative
	// to -- the same point cards are dealt from for this seat.
	Point getSeatAnchor(){
		return bettingSquare.firstPoint();
	}

	offSets calcOffset(){
		float xMoveHand = 0;
		float yMoveHand = 0;

		float xMoveCard = 0;
		float yMoveCard = 0;

		int dir = bettingSquare.getDirection();
		float rotation = bettingSquare.getRotationDegrees();

		if(dir < 5){
			// Every stacking/fan offset below used to be a set of values
			// hand-picked per direction bucket (0/1/-1) -- but each of
			// those turns out to be exactly the seat's *own* local,
			// unrotated (dir==0) template rotated by that bucket's own
			// cardinal angle (-90/0/90), same top-left-pivot/clockwise
			// convention as everywhere else rotation is done (see
			// Table.h's drawResultBanner()). Rotating by the seat's
			// actual stored angle instead of just its bucket's cardinal
			// one generalizes cleanly to the 2 new 5-player seats' 70/300
			// degree tilts -- without this, their cards fanned out along
			// the old cardinal axis while each card sprite itself drew
			// rotated to the new angle, crossing over each other instead
			// of fanning cleanly.
			constexpr float PI = 3.14159265358979323846f;
			float rad = rotation * PI / 180.0f;
			float cosT = std::cos(rad), sinT = std::sin(rad);
			auto rotateLocal = [&](float lx, float ly, float& wx, float& wy){
				wx = lx * cosT - ly * sinT;
				wy = lx * sinT + ly * cosT;
			};

			rotateLocal(0.0f, -(cardHeight + 5.0f), xMoveHand, yMoveHand);

			int offset = 3 * cardWidth / 10;
			if(hands.size() > 1){
				rotateLocal((float)offset, 0.0f, xMoveCard, yMoveCard);
			} else{
				rotateLocal((float)offset, -(float)offset, xMoveCard, yMoveCard);
			}
		} else {
			xMoveCard = -110;
			yMoveCard = 0;
		}

		offSets r{
			.xMoveHand = xMoveHand,
			.yMoveHand = yMoveHand,
			.xMoveCard = xMoveCard,
			.yMoveCard = yMoveCard,
			.rotation = rotation
		};

		return r;
	}

	int getHandTotal(){
		return hands[0].getHandTotal();
	}
	void showCards(){
		hands[0].cards[1].showCard(true);
	}

private:
	bool isPlayer;
	BettingSquare bettingSquare;
	int activeHand = 0;
	int bankroll = 0;
	int initialBankroll = 0;
	int buyInAmount = 0;
	int currentBet = 25;
	int sideBet = 0;
	int matchUpBet = 0;
	int matchDownBet = 0;

	// SPLIT HELPER FUNCTIONS
	void addBlankHand(){
		offSets adj = calcOffset();

		int pos = activeHand + 1;
		hands.insert(hands.begin() + pos,Hand());
		hands[pos].setBaseRotation(adj.rotation);

		for(int i = pos + 1; i < hands.size(); i++){
			float x = bettingSquare.firstPoint().x + adj.xMoveHand * i;
			float y = bettingSquare.firstPoint().y + adj.yMoveHand * i;
			hands[i].shift(x, y);
		}
	}
	Card getLastCardAndRemove(){
		Card c = hands[activeHand].getLast();
		hands[activeHand].removeLast();
		return c;
	}

};

