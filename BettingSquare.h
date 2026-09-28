#pragma once
#include <string>
#include <SDL3/SDL.h>
#include "Game.h"

class BettingSquare
{
public:
	BettingSquare(){}

	BettingSquare(float x, float y, int directionOfHand){
		this->directionOfHand = directionOfHand;

		point = Point(x,y);
	}

	Point firstPoint() const{
		return point;
	}

	int getDirection() const {
		return directionOfHand;
	}

private:
	Point point;
	int directionOfHand;
};

