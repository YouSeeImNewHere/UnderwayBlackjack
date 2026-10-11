#pragma once
#include <SDL3/SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#ifdef SDL_PLATFORM_IOS
// ios/Haptics.m: the Taptic Engine (0 light tap, 1 firm, 2 a "wrong" buzz).
extern "C" void UB_Haptic(int kind);
#endif

// A short vibration for a moment worth feeling: a missed drill answer, a
// blackjack, a rare side-bet hit. Android through SDL's haptics (the
// phone's vibrator), iOS through the Taptic Engine, the web through
// navigator.vibrate (Android browsers). Silent on desktops. SETTINGS'
// VIBRATE turns it off.
class Haptics
{
public:
	enum Kind{ Light, Firm, Wrong };
	bool on = true;

	void init(){
#ifdef SDL_PLATFORM_ANDROID
		if(!SDL_InitSubSystem(SDL_INIT_HAPTIC))
			return;
		int count = 0;
		SDL_HapticID* ids = SDL_GetHaptics(&count);
		if(ids && count > 0){
			device = SDL_OpenHaptic(ids[0]);
			if(device && !SDL_InitHapticRumble(device)){
				SDL_CloseHaptic(device);
				device = nullptr;
			}
		}
		SDL_free(ids);
#endif
	}

	void buzz(Kind kind){
		if(!on)
			return;
#if defined(SDL_PLATFORM_ANDROID)
		if(device)
			SDL_PlayHapticRumble(device, kind == Light ? 0.4f : 0.8f, kind == Light ? 25 : kind == Firm ? 60 : 120);
#elif defined(SDL_PLATFORM_IOS)
		UB_Haptic((int)kind);
#elif defined(__EMSCRIPTEN__)
		EM_ASM({ if(navigator.vibrate) navigator.vibrate($0 == 2 ? [60, 60, 60] : $0 == 1 ? 60 : 25); }, (int)kind);
#else
		(void)kind;
#endif
	}

private:
#ifdef SDL_PLATFORM_ANDROID
	SDL_Haptic* device = nullptr;
#endif
};
