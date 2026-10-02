#pragma once
#include <functional>

// The game's sound effects, as Table asks for them. Table only knows these
// names; mina.cpp hooks them up to the real player (Audio.h) with
// Table::setSoundPlayer(), so Table and the tests run without audio.
enum class Sfx{
	Deal,       // a card leaves the shoe
	Flip,       // the dealer's hole card (or a face-down double) turns over
	ChipsPay,   // a payout chip leaves the tray
	ChipsTake,  // a lost bet is collected
	ChipBet,    // a bet or side bet goes up during betting
	Shuffle,    // a fresh shoe
	Tap,        // any button
	Win,        // a blackjack
	Jackpot,    // a rare side-bet hit (the big callout banner)
	Count
};

using SoundPlayer = std::function<void(Sfx)>;
