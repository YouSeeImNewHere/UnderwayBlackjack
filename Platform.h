#pragma once
#include <SDL3/SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

// Which controls the help pages should talk about: touch gestures on
// phones/tablets, keyboard and mouse on desktops. The web build decides by
// the browser's primary pointer (a finger reports "coarse"), the same test
// web/shell.html uses to rotate the game for portrait phones.
inline bool usesTouchControls(){
#if defined(SDL_PLATFORM_IOS) || defined(SDL_PLATFORM_ANDROID)
	return true;
#elif defined(__EMSCRIPTEN__)
	static const bool touch = EM_ASM_INT({
		return window.matchMedia('(pointer: coarse)').matches ? 1 : 0;
	}) != 0;
	return touch;
#else
	return false;
#endif
}
