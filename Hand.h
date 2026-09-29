#pragma once
#include "Card.h"
#include "BettingSquare.h"
#include "Game.h"

#include <vector>
#include <set>
#include <utility>

// Set once by Table::resolveRound(), after the dealer's done -- a fresh
// Hand always starts at None, which is also what makes an unresolved (or
// next round's) hand naturally stop showing a stale result: there's no
// explicit reset needed, a new Hand object just starts here again.
enum class HandResult{ None, Win, Push, Loss };

class Hand
{
public:
	std::vector<Card> cards;

	// The hand's resting rotation (matches its seat's direction) -- set
	// once, whenever the hand is created (see Person's constructor and
	// addBlankHand()), not recomputed on every draw. A card's own rotation
	// only needs touching at two moments: when it's actually dealt into
	// this hand (addCard()), and if the hand busts afterward (checkIfBreak(),
	// the +15 tilt) -- draw() itself never writes to a card's rotation.
	void setBaseRotation(float rotation){
		baseRotation = rotation;
		applyRotationToAllCards();
	}

	void addCard(Card card, bool showCard = true, bool doubleHand = false){
		card.showCard(showCard);
		// -90 (not +90) to match the flight animation's own angle math in
		// Table::startDeal() (angle -= doubleHand ? 90 : 0) -- has to agree
		// with that sign or the card visibly snaps 180 degrees the instant
		// it lands, since this overwrites whatever rotation the animation
		// arrived at.
		card.setRotation(baseRotation - (doubleHand ? 90 : 0) + (bust ? 15 : 0));
		cards.push_back(card);
		if(card.getValue() == 1)
			aceLocations.insert(cards.size() - 1);
	}

	void draw(SDLState& state,Resources& res, bool forceHidden = false){
		for(Card& c : cards){
			c.draw(state, res, forceHidden);
		}
	}

	void shift(float x,float y){
		for(Card& c : cards){
			c.setPostion(SDL_FPoint{x,y});
		}
	}

	std::vector<Card> busted(){
		std::vector<Card> output;

		while(cards.size() > 2){
			output.push_back(cards.back());
			cards.pop_back();
		}

		return output;
	}

	void printHand(){
		for(Card c : cards){
			std::cout << c.getValue() << " ";
		}
		std::cout << std::endl;
	}

	Card getLast(){
		return cards.back();
	}

	void removeLast(){
		cards.pop_back();
	}

	int getHandSize(){
		return cards.size();
	}

	// This hand's own stake -- starts as the player's round bet (see
	// Person::startRoundBet()) but can diverge per hand afterward: doubling
	// doubles just this hand's bet, and splitting gives the new hand a
	// separate copy of it (see Person::split()/doubleActiveHandBet()).
	void setBet(int amount){
		bet = amount;
		freeBetAmount = 0;
	}

	int getBet(){
		return bet;
	}

	void setResult(HandResult r){
		result = r;
	}

	HandResult getResult(){
		return result;
	}

	void doubleBet(){
		bet *= 2;
	}

	// Free Bet Blackjack only: how much of this hand's bet was never
	// actually funded from bankroll (a free double's extra half, or a free
	// split's entire new-hand stake -- see Table::onDouble()/onSplit()).
	// A win still pays out on the *full* bet, but a loss only costs
	// bet - freeBetAmount, and a push only returns that same real portion
	// -- see Table::freeBetCredit(). Reset to 0 whenever setBet() starts a
	// fresh hand.
	void addFreeBetAmount(int amount){
		freeBetAmount += amount;
	}

	int getFreeBetAmount(){
		return freeBetAmount;
	}

	int getRealBet(){
		return bet - freeBetAmount;
	}

	// Lets a caller (Table, on a surrender) force this hand into the same
	// bust state/tilt checkIfBreak() gives a hand that actually goes over
	// 21, even though its total never did -- surrendering forfeits the hand
	// same as busting does, so it should look the part.
	void forceBust(){
		if(!bust){
			bust = true;
			applyRotationToAllCards();
		}
	}

	// Set on both hands when a pair is split (Table::onSplit()). fromSplit
	// drives the automatic second card when play reaches a split hand;
	// splitAces drives the one-card-per-ace rule (see
	// Table::allowsHitSplitAces()).
	bool isFromSplit() const{ return fromSplit; }
	bool isSplitAces() const{ return splitAces; }
	void markSplit(bool aces){
		fromSplit = true;
		splitAces = aces;
	}

	// Stays true once the hand has busted (or been surrendered, see
	// forceBust()), even after Table's bust handling sweeps every card past
	// the first two into the discard pile -- at that point getHandTotal()
	// only sees those two leftover cards and can read as a live total
	// (e.g. 10+6=16), so settling a hand must check this, not just
	// total > 21.
	bool isBust() const{
		return bust;
	}

	bool checkIfBreak(){
		int runningTotal = 0;
		bool wasBust = bust;

		for(int i = 0; i < cards.size(); i++){
			int value = cards[i].getValue();

			if(value >= 2 && value <= 10)
				runningTotal += value;
			else if(value > 10)
				runningTotal += 10;
			else{
				runningTotal += 11;
			}
		}

		// Every ace was just counted as 11 above; walk them back down to 1
		// (-10 each) one at a time until the total's 21 or under, same as
		// getHandTotal() does. This used to be gated behind `isSoft`, which
		// nothing in this class ever set true -- so the reduction never
		// actually ran, and any hand with an ace whose all-11 total went
		// over 21 (e.g. 10+A, then hit anything) was declared bust on the
		// spot even though counting that ace as 1 would have kept it alive.
		if(runningTotal > 21 && aceLocations.size() > 0){
			for(int i = 0; i < aceLocations.size(); i++){
				runningTotal -= 10;
				if(runningTotal <= 21)
					break;
			}
		}

		bust = runningTotal > 21;
		// Only re-stamp every card's rotation when bust actually just
		// changed -- this runs after *every* card dealt, not just ones
		// that bust, and applyRotationToAllCards() blindly overwrites
		// whatever each card's rotation was (e.g. a doubled card's
		// extra tilt), so calling it unconditionally here was wiping
		// that out the instant any card landed, bust or not.
		if(bust != wasBust)
			applyRotationToAllCards();
		std::cout << "Running total: " << runningTotal << std::endl;
		return bust;
	}

	int getHandTotal(){
		int runningTotal = 0;

		for(int i = 0; i < cards.size(); i++){
			int value = cards[i].getValue();

			if(value >= 2 && value <= 10)
				runningTotal += value;
			else if(value > 10)
				runningTotal += 10;
			else{
				runningTotal += 11;
			}
		}

		if(runningTotal > 21 && aceLocations.size() > 0){
			for(int i = 0; i < aceLocations.size(); i++){
				runningTotal -= 10;
				if(runningTotal <= 21)
					break;
			}
		}

		return runningTotal;
	}

	// Only counts cards currently face-up -- unlike getHandTotal() above,
	// which sums every card regardless. Needed for the dealer's on-table
	// total display, which must never leak the hidden hole card's value.
	// hard/soft only differ when a shown ace could count as 11 without
	// busting; otherwise (no ace, or the soft total would bust) they're
	// equal, meaning there's only one total worth showing.
	std::pair<int,int> getShownTotals(){
		int hard = 0;
		bool hasAce = false;

		for(Card& c : cards){
			if(!c.getShown())
				continue;

			int value = c.getValue();
			if(value == 1){
				hasAce = true;
				hard += 1;
			} else if(value >= 2 && value <= 10){
				hard += value;
			} else{
				hard += 10;
			}
		}

		int soft = (hasAce && hard + 10 <= 21) ? hard + 10 : hard;
		return {hard, soft};
	}

private:
	bool bust = false;
	bool fromSplit = false;
	bool splitAces = false;
	std::set<int> aceLocations;
	float baseRotation = 0;
	int bet = 0;
	int freeBetAmount = 0;
	HandResult result = HandResult::None;

	void applyRotationToAllCards(){
		for(Card& c : cards){
			c.setRotation(bust ? baseRotation + 15 : baseRotation);
		}
	}
};

