#pragma once
#include <SDL3/SDL.h>
#include "Game.h"

class Card
{
public:
	Card(int suit, int value, bool isShown, SDL_FPoint postion, float rotation){
		this->suit =  suit;
		this->value = value;
		this->isShown = isShown;
		this->position = SDL_FPoint(postion.x,postion.y);
		this->rotation = rotation;

		// back of card
		if(value == 0){
			this->src = SDL_FRect{
				.x = 0,
				.y = 0,
				.w = 50,
				.h = 70
			};
		}
		// yellow card
		else if(value == 14){
			this->src = SDL_FRect{
				.x = 0,
				.y = 70,
				.w = 50,
				.h = 70
			};
		}
		this->src = SDL_FRect{
			.x = static_cast<float> (value) * 50,
			.y = static_cast<float> (suit) * 70,
			.w = 50,
			.h = 70
		};
	}

	void setPostion(SDL_FPoint postion){
		this->position = postion;
	}

	void adjustPostion(float x, float  y){
		position.x += x;
		position.y += y;
	}

	void setRotation(float rotation){
		this->rotation = rotation;
	}

	float getRotation(){
		return rotation;
	}

	int getValue(){
		return value;
	}

	// 0-spade, 1-club, 2-diamond, 3-heart -- needed by side-bet evaluation
	// (Lucky Ladies/Match Up/Match Down all care whether two cards share a
	// suit, not just a rank), nothing else in the base game needed this
	// before now.
	int getSuit(){
		return suit;
	}

	void showCard(bool showCard){
		isShown = showCard;
	}

	const SDL_FRect& getSrc() const {
		return src;
	}

	bool getShown(){
		return isShown;
	}

	SDL_FPoint getPosition(){
		return position;
	}
	// forceHidden: GameOptionsMenu's "hide inactive hands" house rule --
	// draws the card back regardless of isShown, without touching isShown
	// itself (still the card's real, permanent shown/hidden state, used
	// for counting and for every other seat's own genuine reveal). Purely
	// a rendering override for whichever seat isn't the active one right
	// now (see Person::draw()), computed fresh every frame rather than
	// stored, so it never needs updating when a turn starts or ends.
	void draw(SDLState& state,Resources& res, bool forceHidden = false){
		SDL_FRect dest{
			.x = position.x,
			.y = position.y,
			.w = cardWidth,
			.h = cardHeight
		};

		const SDL_FRect& source = (isShown && !forceHidden) ? src : backOFCard;
		SDL_RenderTextureRotated(state.renderer,res.allCards,&source,&dest,rotation,&rotationTopLeft,SDL_FLIP_NONE);
	}

private:
	// 0 - spade, 1 - club, 2 - diamond, 3 - heart
	int suit;

	// 1 - A, 2 - 10 are just that. 11 - J, 12 - Q, 13 - K
	// 0 - back of card
	// 14 yellow card
	int value;
	bool isShown;

	SDL_FRect src;
	SDL_FPoint position;
	float rotation = 0;
};

