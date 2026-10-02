#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <random>
#include "Sound.h"

// Plays the game's sound effects (Sound.h) through SDL's own audio (no
// extra library). Each sound is a WAV from sounds/ (tools/make_sounds.py),
// loaded once at start and converted to the device's format. A handful of
// voices let sounds overlap -- a deal and a chip payout at once.
//
// The WAVs ship next to the game's PNGs, so they're looked up by bare
// name first, then under sounds/ (running from a source checkout, e.g.
// Visual Studio). A missing file or no audio device just means silence.
class Audio
{
public:
	bool effectsOn = true;

	void init(){
		if(!SDL_InitSubSystem(SDL_INIT_AUDIO))
			return;
		device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
		if(!device)
			return;
		SDL_GetAudioDeviceFormat(device, &deviceSpec, nullptr);

		static const char* NAMES[(int)Sfx::Count][3] = {
			{ "sfx-deal-1", "sfx-deal-2", "sfx-deal-3" },
			{ "sfx-flip", nullptr, nullptr },
			{ "sfx-chips-pay-1", "sfx-chips-pay-2", nullptr },
			{ "sfx-chips-take", nullptr, nullptr },
			{ "sfx-chip-bet", nullptr, nullptr },
			{ "sfx-shuffle", nullptr, nullptr },
		};
		for(int s = 0; s < (int)Sfx::Count; s++)
			for(const char* name : NAMES[s])
				if(name){
					Clip c = load(name);
					if(!c.data.empty())
						clips[s].push_back(std::move(c));
				}

		for(SDL_AudioStream*& v : voices){
			v = SDL_CreateAudioStream(&deviceSpec, &deviceSpec);
			if(v)
				SDL_BindAudioStream(device, v);
		}
	}

	void play(Sfx sfx){
		if(!effectsOn || !device)
			return;
		std::vector<Clip>& options = clips[(int)sfx];
		if(options.empty())
			return;

		// Rapid repeats of one sound (a row of payout chips) would just
		// stack into a roar; let the same effect start at most every 50ms.
		Uint64 now = SDL_GetTicks();
		if(now - lastPlayed[(int)sfx] < 50)
			return;
		lastPlayed[(int)sfx] = now;

		const Clip& clip = options[rng() % options.size()];
		SDL_AudioStream* voice = freeVoice();
		if(voice)
			SDL_PutAudioStreamData(voice, clip.data.data(), (int)clip.data.size());
	}

	// The app going to the background on a phone: stop all sound, then
	// pick back up on return.
	void pause(bool paused){
		if(!device)
			return;
		if(paused)
			SDL_PauseAudioDevice(device);
		else
			SDL_ResumeAudioDevice(device);
	}

private:
	struct Clip{ std::vector<Uint8> data; };

	static constexpr int VOICE_COUNT = 8;

	SDL_AudioDeviceID device = 0;
	SDL_AudioSpec deviceSpec{};
	std::vector<Clip> clips[(int)Sfx::Count];
	Uint64 lastPlayed[(int)Sfx::Count] = {};
	SDL_AudioStream* voices[VOICE_COUNT] = {};
	std::minstd_rand rng{12345};

	Clip load(const std::string& name){
		Clip clip;
		SDL_AudioSpec spec;
		Uint8* buf = nullptr;
		Uint32 len = 0;
		if(!SDL_LoadWAV((name + ".wav").c_str(), &spec, &buf, &len)
				&& !SDL_LoadWAV(("sounds/" + name + ".wav").c_str(), &spec, &buf, &len))
			return clip;

		Uint8* converted = nullptr;
		int convertedLen = 0;
		if(SDL_ConvertAudioSamples(&spec, buf, (int)len, &deviceSpec, &converted, &convertedLen)){
			clip.data.assign(converted, converted + convertedLen);
			SDL_free(converted);
		}
		SDL_free(buf);
		return clip;
	}

	// An idle voice (nothing left queued), or failing that the one closest
	// to finishing -- cut short rather than skip the new sound.
	SDL_AudioStream* freeVoice(){
		SDL_AudioStream* best = nullptr;
		int bestQueued = 0;
		for(SDL_AudioStream* v : voices){
			if(!v)
				continue;
			int queued = SDL_GetAudioStreamQueued(v);
			if(queued == 0)
				return v;
			if(!best || queued < bestQueued){
				best = v;
				bestQueued = queued;
			}
		}
		if(best)
			SDL_ClearAudioStream(best);
		return best;
	}
};
