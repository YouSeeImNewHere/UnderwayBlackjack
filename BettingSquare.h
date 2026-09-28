#pragma once
#include <string>
#include <SDL3/SDL.h>
#include "Game.h"

class BettingSquare
{
public:
	BettingSquare(){}

	// directionOfHand still drives every existing hand-stacking/card-fan/
	// betting-control position formula in Person::calcOffset() and
	// Table.h (seatCenterX(), betRow(), the turn arrow) -- those are all
	// hand-tuned per one of the 4 cardinal buckets (0/1/-1/5), not a
	// general function of an angle, so a genuinely new seat that doesn't
	// sit in one of those 4 spots (the two new 5-player seats) still picks
	// whichever existing bucket it's geometrically closest to for all of
	// that. rotationDegrees is separate: the actual on-table tilt the
	// seat's cards render at, which -- unlike the bucket -- can be any
	// angle, so a seat can visually tilt at its own measured angle while
	// still reusing a bucket's proven layout math. Defaults to the
	// bucket's own standard angle (see BettingSquare(x,y,dir)) when not
	// given explicitly.
	BettingSquare(float x, float y, int directionOfHand){
		this->directionOfHand = directionOfHand;
		this->rotationDegrees = (directionOfHand > 1) ? 180.0f : directionOfHand * -90.0f;
		point = Point(x,y);
	}

	BettingSquare(float x, float y, int directionOfHand, float rotationDegrees){
		this->directionOfHand = directionOfHand;
		this->rotationDegrees = rotationDegrees;
		point = Point(x,y);
	}

	Point firstPoint() const{
		return point;
	}

	int getDirection() const {
		return directionOfHand;
	}

	float getRotationDegrees() const {
		return rotationDegrees;
	}

private:
	Point point;
	int directionOfHand;
	float rotationDegrees;
};

