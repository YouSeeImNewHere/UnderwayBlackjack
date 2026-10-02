# Store listing: Blackjack Variants

Everything needed to fill in the App Store and Google Play listings. Copy the
text as-is; each field is within its character limit.

## Links

| Field | Value |
| --- | --- |
| Privacy Policy URL | https://github.com/YouSeeImNewHere/UnderwayBlackjack/blob/master/PRIVACY.md |
| Support URL | https://github.com/YouSeeImNewHere/UnderwayBlackjack/issues |
| Marketing URL (optional) | https://github.com/YouSeeImNewHere/UnderwayBlackjack/releases/latest |

The privacy link only works once `PRIVACY.md` is merged into `master`.

## Apple App Store

**Name** (30 max): `Blackjack Variants`

**Subtitle** (30 max): `5 Blackjack Games & Strategy`

**Promotional text** (170 max, can be changed any time without a new build):

```
Five ways to play 21: Standard, Lucky Ladies, Player's Edge (Spanish 21), Lucky Stiff and Free Bet Blackjack, with a strategy chart for each.
```

**Description** (4000 max):

```
Blackjack Variants puts five casino blackjack games on one table.

- Standard blackjack with 2 or 6 decks
- Lucky Ladies: a side bet on your first two cards totaling 20, up to 1000 to 1 for a Queen of Hearts pair against a dealer blackjack
- Player's Edge: a Spanish 21 game where a player 21 always wins, with 5, 6 and 7 card 21 bonuses, redoubling, and Match Up / Match Down side bets
- Lucky Stiff: an 8 deck game with a side bet on stiff hands
- Free Bet Blackjack: free doubles on 9, 10 and 11 and free splits on most pairs, where a dealer 22 pushes

Play solo or pass the device around: up to five players can share the table, each with their own bankroll and bets.

Learn as you play:
- A strategy chart for every game, tuned to that game's rules, with your current hand highlighted
- A quick tip button that shows the best move right at the table
- A card count display for practicing running and true counts
- Stats for every game and all games combined: wins, streaks, biggest win, side bet results, and how often your moves match the strategy chart

Real casino rules: the dealer checks for blackjack, insurance and even money, late surrender, split and re-split up to four hands, and the dealer hits soft 17.

Play with taps and swipes, or with a keyboard: arrow keys work in every menu. Card and chip sounds can be switched off in Game Options. Your game saves after every round.

For entertainment only. There are no purchases and no real money, and nothing you win can be exchanged for money or prizes.
```

**Keywords** (100 max, comma separated, 95 used):

```
blackjack,21,spanish 21,free bet,lucky ladies,basic strategy,card counting,casino,cards,trainer
```

**Category:** Primary `Games`, then subcategories `Card` and `Casino`.

**Copyright:** `2026 Jared Trevino` (use whatever name should appear).

**Age rating questionnaire.** In App Store Connect: App Information, then Age
Rating, then Edit. Answer honestly:

- Simulated Gambling: **Frequent/Intense**. Every hand is simulated casino
  gambling.
- Gambling (real money, "contests"): **No**.
- Every other category (violence, language, and so on): **None**.

Apple assigns the rating from these answers. Frequent simulated gambling puts
the game in Apple's top age bracket.

**App Privacy.** In App Store Connect: App Privacy, then Get Started, then
**"No, we do not collect data from this app."**

**Export compliance:** already answered in the app (`ITSAppUsesNonExemptEncryption = NO`).

**Screenshots:** in `store/screenshots/`.

| Upload slot | Folder | Size |
| --- | --- | --- |
| iPhone 6.9" display | `iphone-6.9/` | 2868 x 1320 (landscape) |
| iPad 13" display | `ipad-13/` | 2752 x 2064 (landscape) |

Upload them in filename order: menu, game select, table with the tip open,
the Queen of Hearts banner, then the strategy chart. Apple scales these down
for smaller devices on its own.

## Google Play

**App name** (30 max): `Blackjack Variants`

**Short description** (80 max):

```
Five blackjack games with strategy charts, card counting practice and stats.
```

**Full description:** use the App Store description above. It's under Play's
4000-character limit.

**Graphics:**

| Asset | File |
| --- | --- |
| App icon (512 x 512) | `web/static/icon-512.png` |
| Feature graphic (1024 x 500) | `store/feature-graphic.png` (made by `store/gen_feature_graphic.py`) |
| Phone screenshots (16:9) | `store/screenshots/android/` (1920 x 1080) |

**Category:** `Games`, then `Card`. You could pick `Casino` instead.

**Content rating (IARC questionnaire):** say **yes** to simulated gambling.
There's no real-money gambling, no in-app purchases, and no user interaction or
sharing. The questionnaire assigns the rating.

**Data safety:** "Does your app collect or share any of the required user data
types?" **No.** Encryption in transit: not applicable, since nothing is sent.

**Target audience:** 18 and over, because of the gambling theme.

**Ads:** No ads.

## Regenerating the screenshots

The screenshots are real game frames, rendered at each store size with the
table's edge colors filling the extra space. If the look of the game changes,
ask for them to be rendered again.
