#pragma once

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <vector>
#include <string>
#include <iostream>

inline constexpr float cardWidth = 100;
inline constexpr float cardHeight = 140;

inline constexpr SDL_FPoint rotationTopLeft{0.0f,0.0f};

inline constexpr SDL_FPoint shoePosition{
    895.0f,
    72.0f
};

inline constexpr SDL_FPoint discardPosition{
    410.0f,
    75.0f
};

inline constexpr SDL_FRect backOFCard{
    .x = 0,
    .y = 0,
    .w = 50,
    .h = 70
};

struct Point{
    float x,y;

    Point(float x = 0.0f,float y = 0.0f)
        : x(x),y(y) {}
};

struct SDLState{
    SDL_Window *window;
    SDL_Renderer *renderer;
    int width,height,logW,logH;

    const bool *keys = SDL_GetKeyboardState(nullptr);
};

bool initialize(SDLState &state);
void cleanup(SDLState &state);

struct Resources{

    std::vector<SDL_Texture *> textures;
    SDL_Texture *allCards;
    SDL_Texture *tableCloth;
    SDL_Texture *arrow;
    SDL_Texture *chips;

    SDL_Texture *loadTexture(SDL_Renderer *renderer,const std::string &filepath){
        SDL_Texture *tex = IMG_LoadTexture(renderer,filepath.c_str());
        SDL_SetTextureScaleMode(tex,SDL_SCALEMODE_NEAREST);
        textures.push_back(tex);
        return tex;
    }

    void load(SDLState &state){
        allCards = loadTexture(state.renderer,"Cards.png");
        tableCloth = loadTexture(state.renderer,"Table.png");
        arrow = loadTexture(state.renderer,"Arrow.png");
        chips = loadTexture(state.renderer,"Chips.png");
    }

    void unload(){
        for(SDL_Texture *tex: textures){
            SDL_DestroyTexture(tex);
        }
    }
};

class Game
{};

