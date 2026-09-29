#pragma once
#include "Game.h"
#include <string>

// A tiny dependency-free "dot matrix" font for rendering short HUD/menu
// text -- there's no SDL_ttf or any other text rendering in this codebase,
// and pulling one in just for a handful of short labels/numbers is more
// than this needs.
//
// 5 wide x 5 tall, not the original 3 wide x 5 tall. At 3 columns, a real
// diagonal stroke genuinely doesn't fit -- W, N, X, K, Y all need one, and
// every attempt at faking it with bars/verticals ended up either
// illegible or pixel-identical to some other letter (an upside-down N
// reading as M, W reading as X, and so on, each requiring its own
// after-the-fact fix). 5 columns is enough room for an actual diagonal, so
// this covers the full alphabet properly instead of "only what current
// labels happen to need."
namespace DigitFont{

struct GlyphRows{ const char* rows[5]; };

inline GlyphRows glyphFor(char c){
	switch(c){
		case '0': return GlyphRows{{".###.","#...#","#...#","#...#",".###."}};
		case '1': return GlyphRows{{"..#..",".##..","..#..","..#..",".###."}};
		case '2': return GlyphRows{{".###.","#...#","..##.",".#...","#####"}};
		case '3': return GlyphRows{{".###.","#...#","..##.","#...#",".###."}};
		case '4': return GlyphRows{{"#..#.","#..#.","#####","...#.","...#."}};
		case '5': return GlyphRows{{"#####","#....","####.","....#","####."}};
		case '6': return GlyphRows{{".###.","#....","####.","#...#",".###."}};
		case '7': return GlyphRows{{"#####","....#","...#.","..#..","..#.."}};
		case '8': return GlyphRows{{".###.","#...#",".###.","#...#",".###."}};
		case '9': return GlyphRows{{".###.","#...#",".####","....#",".###."}};
		case '/': return GlyphRows{{"....#","...#.","..#..",".#...","#...."}};
		case 'A': return GlyphRows{{".###.","#...#","#####","#...#","#...#"}};
		case 'B': return GlyphRows{{"####.","#...#","####.","#...#","####."}};
		case 'C': return GlyphRows{{".###.","#....","#....","#....",".###."}};
		case 'D': return GlyphRows{{"####.","#...#","#...#","#...#","####."}};
		case 'E': return GlyphRows{{"#####","#....","###..","#....","#####"}};
		case 'F': return GlyphRows{{"#####","#....","###..","#....","#...."}};
		case 'G': return GlyphRows{{".###.","#....","#.###","#...#",".###."}};
		case 'H': return GlyphRows{{"#...#","#...#","#####","#...#","#...#"}};
		case 'I': return GlyphRows{{"#####","..#..","..#..","..#..","#####"}};
		case 'J': return GlyphRows{{"..###","...#.","...#.","#..#.",".##.."}};
		case 'K': return GlyphRows{{"#...#","#..#.","###..","#..#.","#...#"}};
		case 'L': return GlyphRows{{"#....","#....","#....","#....","#####"}};
		case 'M': return GlyphRows{{"#...#","##.##","#.#.#","#...#","#...#"}};
		case 'N': return GlyphRows{{"#...#","##..#","#.#.#","#..##","#...#"}};
		case 'O': return GlyphRows{{".###.","#...#","#...#","#...#",".###."}};
		case 'P': return GlyphRows{{"####.","#...#","####.","#....","#...."}};
		case 'Q': return GlyphRows{{".###.","#...#","#...#","#..#.",".####"}};
		case 'R': return GlyphRows{{"####.","#...#","####.","#..#.","#...#"}};
		case 'S': return GlyphRows{{".####","#....",".###.","....#","####."}};
		case 'T': return GlyphRows{{"#####","..#..","..#..","..#..","..#.."}};
		case 'U': return GlyphRows{{"#...#","#...#","#...#","#...#",".###."}};
		case 'V': return GlyphRows{{"#...#","#...#","#...#",".#.#.","..#.."}};
		case 'W': return GlyphRows{{"#...#","#...#","#.#.#","##.##","#...#"}};
		case 'X': return GlyphRows{{"#...#",".#.#.","..#..",".#.#.","#...#"}};
		case 'Y': return GlyphRows{{"#...#",".#.#.","..#..","..#..","..#.."}};
		case 'Z': return GlyphRows{{"#####","...#.","..#..",".#...","#####"}};
		case ':': return GlyphRows{{".....","..#..",".....","..#..","....."}};
		case ',': return GlyphRows{{".....",".....",".....","..#..",".#..."}};
		case '.': return GlyphRows{{".....",".....",".....",".....","..#.."}};
		case '-': return GlyphRows{{".....",".....","#####",".....","....."}};
		case '!': return GlyphRows{{"..#..","..#..","..#..",".....","..#.."}};
		case '+': return GlyphRows{{".....","..#..",".###.","..#..","....."}};
		case '<': return GlyphRows{{"...#.","..#..",".#...","..#..","...#."}};
		case '>': return GlyphRows{{".#...","..#..","...#.","..#..",".#..."}};
		default:  return GlyphRows{{".....",".....",".....",".....","....."}};
	}
}

// Each glyph cell is 5 wide + 1 pixel gap = 6 * pixel per character.
inline float textWidth(const std::string& text, float pixel){
	return text.empty() ? 0.0f : (text.size() * 6 - 1) * pixel;
}

inline void drawText(SDLState& state, const std::string& text, float x, float y, float pixel, SDL_Color color){
	SDL_SetRenderDrawColor(state.renderer, color.r, color.g, color.b, color.a);

	float cursorX = x;
	for(char c : text){
		GlyphRows g = glyphFor(c);
		for(int row = 0; row < 5; row++){
			for(int col = 0; col < 5; col++){
				if(g.rows[row][col] == '#'){
					SDL_FRect px{
						.x = cursorX + col * pixel,
						.y = y + row * pixel,
						.w = pixel,
						.h = pixel
					};
					SDL_RenderFillRect(state.renderer, &px);
				}
			}
		}
		cursorX += 6 * pixel;
	}
}

// Each glyph cell is 5 tall + 1 pixel gap = 6 * pixel per character.
inline float verticalTextHeight(const std::string& text, float pixel){
	return text.empty() ? 0.0f : (text.size() * 6 - 1) * pixel;
}

// Stacks each character below the previous one -- each glyph is still
// drawn upright (not rotated 90 degrees), just arranged in a column
// instead of a row. Used for narrow side labels where there's no room for
// a horizontal word, like a tag running down the side of a control.
inline void drawVerticalText(SDLState& state, const std::string& text, float x, float y, float pixel, SDL_Color color){
	SDL_SetRenderDrawColor(state.renderer, color.r, color.g, color.b, color.a);

	float cursorY = y;
	for(char c : text){
		GlyphRows g = glyphFor(c);
		for(int row = 0; row < 5; row++){
			for(int col = 0; col < 5; col++){
				if(g.rows[row][col] == '#'){
					SDL_FRect px{
						.x = x + col * pixel,
						.y = cursorY + row * pixel,
						.w = pixel,
						.h = pixel
					};
					SDL_RenderFillRect(state.renderer, &px);
				}
			}
		}
		cursorY += 6 * pixel;
	}
}

}
