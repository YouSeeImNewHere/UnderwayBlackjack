// The game's rule tests: payouts, splits, doubles, surrender, insurance,
// side bets, the strategy charts and index plays, stats and the training
// screens -- each by rigging the shoe and playing a hand through Table.
// Built by CMake with -DBUILD_TESTS=ON (target rules_test) and run by
// .github/workflows/tests.yml on every pull request. Exits non-zero on a
// failure.
#include <random>
#include <algorithm>
#include <queue>
#include <optional>
#include <unordered_map>
#include <cmath>
#include <functional>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <map>
#include <SDL3/SDL.h>
#define private public
#include "Table.h"
#include "TrainingMenus.h"
#include "CountingMenus.h"
#include "Backup.h"
#undef private
#include "Stats.h"
static int fails=0;
static void check(bool ok, const char* what){ printf("  [%s] %s\n", ok?"PASS":"FAIL", what); if(!ok) fails++; }
static Card C(int v,int s=0){ return Card(s, v, false, SDL_FPoint{0,0}, 0); }
struct T {
  Table t{1,true,2};
  T(GameMode m=GameMode::TwoDeck){ int br[5]={1000,1000,1000,1000,1000}, bets[5]={25,25,25,25,25}, side[5]={0,0,0,0,0};
    t.configureGameMode(m); t.configurePlayers(1,br,bets,side); t.setDealerSpeedFactor(0.35f); t.startGame(); }
  // cards: p1, dealerUp, p2, dealerHole, then draws in order
  void deal(std::vector<Card> cards){ t.needsBurnCard=false; t.shoe.insert(t.shoe.begin(), cards.begin(), cards.end()); t.beginRound(); }
  void frames(int n){ for(int i=0;i<n;i++){ t.update(1/60.f); t.dealDealer(); } }
  void settle(){ for(int i=0;i<20000;i++){ if(t.acceptingPlayerInput() || (t.awaitingBets && t.chipAnimations.empty())) return; t.update(1/60.f); t.dealDealer(); } }
  void key(SDL_Scancode sc){ SDL_Event e{}; e.type=SDL_EVENT_KEY_DOWN; e.key.scancode=sc; t.handleEvent(e); }
  bool autoDecline=false;
  void toBetting(){ for(int i=0;i<20000 && !(t.awaitingBets && t.chipAnimations.empty()); i++){ if(autoDecline && t.isAwaitingInsurance()) t.answerInsurance(false); t.update(1/60.f); t.dealDealer(); } }
  Person& p(){ return t.players[0]; }
};
int main(){
  std::ostringstream sink; auto* old=std::cout.rdbuf(sink.rdbuf());
  auto out=[&](const char* s){ std::cout.rdbuf(old); printf("%s\n",s); std::cout.rdbuf(sink.rdbuf()); };
  #define CHECK(c,w) do{ std::cout.rdbuf(old); check(c,w); std::cout.rdbuf(sink.rdbuf()); }while(0)

  out("1. Surrender a hard 16 (bet 25) -> half back");
  { T x; x.deal({C(10),C(9),C(6),C(8)}); x.settle(); x.key(SDL_SCANCODE_R); x.toBetting();
    CHECK(x.p().getBankroll()==1000-25+12, "bankroll 987 after surrender"); }

  out("2. Surrender after a hit is refused");
  { T x; x.deal({C(5),C(9),C(4),C(8),C(3)}); x.settle(); x.key(SDL_SCANCODE_H); x.settle(); x.key(SDL_SCANCODE_R); x.settle();
    CHECK(!x.p().hands[0].isBust() && x.p().hands[0].getBet()==25, "3-card hand not surrendered"); }

  out("3. Free Bet: no surrender");
  { T x(GameMode::SixDeckFreeBet); x.deal({C(10),C(9),C(6),C(8)}); x.settle(); x.key(SDL_SCANCODE_R); x.settle();
    CHECK(x.p().hands[0].getBet()==25 && !x.p().hands[0].isBust(), "surrender refused"); }

  out("4. Double on 3 cards");
  { T x; x.deal({C(2),C(9),C(3),C(8),C(4)}); x.settle(); x.key(SDL_SCANCODE_H); x.settle();
    CHECK(!x.t.canDoubleActiveHand(), "standard 2-deck: refused"); }
  { T x(GameMode::TwoDeckPlayersEdge); x.deal({C(2),C(9),C(3),C(8),C(4)}); x.settle(); x.key(SDL_SCANCODE_H); x.settle();
    CHECK(x.t.canDoubleActiveHand(), "Player's Edge: allowed"); }

  out("5. Split aces, standard: one card each, no input needed");
  { T x; x.deal({C(1),C(9),C(1,1),C(8),C(13),C(5)}); x.settle(); x.key(SDL_SCANCODE_P);
    x.toBetting();
    Person& p=x.p();
    // hands are swept by now; check the counts logged via discard size instead: 2 player hands x2 + dealer 2
    CHECK(x.t.discard.size()>=6, "round finished with no further input (both aces got one card)"); }
  { T x; x.deal({C(1),C(9),C(1,1),C(8),C(13),C(5)}); x.settle(); x.key(SDL_SCANCODE_P);
    for(int i=0;i<3000 && !x.t.awaitingNewRound;i++) x.frames(1);
    Person& p=x.p();
    CHECK(p.hands.size()==2 && p.hands[0].getHandSize()==2 && p.hands[1].getHandSize()==2, "A-K and A-5, two cards each");
    CHECK(x.t.awaitingNewRound, "dealer played it out without any hit/stand"); }

  out("6. Split aces, Player's Edge: may hit");
  { T x(GameMode::TwoDeckPlayersEdge);
    x.deal({C(1),C(9),C(1,1),C(8),C(5),C(3),C(2)}); x.settle(); x.key(SDL_SCANCODE_P); x.settle();
    CHECK(x.t.acceptingPlayerInput() && x.p().getActiveHand()==0 && x.p().hands[0].getHandSize()==2, "first ace hand (A-5) waits for input");
    x.key(SDL_SCANCODE_H); x.settle();
    CHECK(x.p().hands[0].getHandSize()==3, "hit on split ace allowed"); }

  out("7. Split 8s: second hand auto-drawn after first finishes");
  { T x; x.deal({C(8),C(9),C(8,1),C(8),C(3),C(10),C(2)}); x.settle(); x.key(SDL_SCANCODE_P); x.settle();
    CHECK(x.p().hands[0].getHandSize()==2 && x.p().hands[1].getHandSize()==1, "after split: 8-3 and a lone 8");
    x.key(SDL_SCANCODE_S); x.settle();
    CHECK(x.p().getActiveHand()==1 && x.p().hands[1].getHandSize()==2, "second hand got its card before any input");
    CHECK(x.t.acceptingPlayerInput(), "and now waits for the player"); }

  out("8. Dealer peek: A up, K hole");
  { T x; x.autoDecline=true; x.deal({C(10),C(1),C(6),C(13)}); 
    // right after the last card lands, try to double before the peek
    for(int i=0;i<3000 && (x.t.cardAnimation||!x.t.dealQueue.empty());i++){ x.t.update(1/60.f); x.t.dealDealer(); }
    x.key(SDL_SCANCODE_D);
    x.toBetting();
    CHECK(x.p().getBankroll()==975, "lost only the original 25 (double was ignored)"); }
  { T x; x.deal({C(10),C(1),C(6),C(13)}); for(int i=0;i<3000 && (x.t.cardAnimation||!x.t.dealQueue.empty());i++){ x.t.update(1/60.f); x.t.dealDealer(); } x.frames(1); x.t.answerInsurance(false); x.frames(2);
    CHECK(!x.t.jackpotCallouts.empty() && x.t.jackpotCallouts.front().title=="DEALER BLACKJACK", "DEALER BLACKJACK banner shown"); }

  out("9. Dealer peek: 10 up, 9 hole -> play continues");
  { T x; x.deal({C(10),C(10),C(6),C(9)}); x.settle();
    CHECK(x.t.acceptingPlayerInput() && !x.t.awaitingNewRound, "player gets to act"); }

  out("10. Player blackjack vs dealer blackjack -> push, settled once");
  { T x; x.autoDecline=true; x.deal({C(1),C(1,1),C(13),C(12)}); x.toBetting();
    CHECK(x.p().getBankroll()==1000, "bankroll back to 1000"); }

  out("11. Player blackjack, dealer no blackjack (A up) -> 3:2");
  { T x; x.autoDecline=true; x.deal({C(1),C(1,1),C(13),C(7),C(10)}); x.toBetting();
    CHECK(x.p().getBankroll()==1000+37, "bankroll 1037"); }


  auto toPrompt=[&](T& x){ for(int i=0;i<5000 && !x.t.isAwaitingInsurance() && !x.t.acceptingPlayerInput() && !x.t.awaitingBets;i++){ x.t.update(1/60.f); x.t.dealDealer(); } };
  out("12. Insurance taken, dealer has blackjack");
  { T x; x.deal({C(10),C(1),C(9),C(13)}); toPrompt(x);
    CHECK(x.t.isAwaitingInsurance(), "insurance offered with an Ace up");
    x.key(SDL_SCANCODE_H); x.frames(5);
    CHECK(x.t.isAwaitingInsurance() && x.p().hands[0].getHandSize()==2, "hit ignored while the offer is open");
    x.key(SDL_SCANCODE_Y); x.toBetting();
    CHECK(x.p().getBankroll()==1000-25-12+36, "lost 25, insurance 12 paid 2:1 -> 999"); }
  out("13. Insurance taken, no dealer blackjack");
  { T x; x.deal({C(10),C(1),C(9),C(7)}); toPrompt(x); x.key(SDL_SCANCODE_Y); x.settle(); x.key(SDL_SCANCODE_S); x.toBetting();
    CHECK(x.p().getBankroll()==1000+25-12, "19 beats 18 (+25), insurance lost (-12) -> 1013"); }
  out("14. Even money");
  { T x; x.deal({C(1,2),C(1),C(13),C(7)}); toPrompt(x);
    CHECK(x.t.isAwaitingInsurance(), "even money offered on a blackjack");
    x.key(SDL_SCANCODE_Y); x.toBetting();
    CHECK(x.p().getBankroll()==1025, "paid 1:1 -> 1025"); }
  out("15. Even money declined, dealer blackjack -> push");
  { T x; x.deal({C(1,2),C(1),C(13),C(12)}); toPrompt(x); x.key(SDL_SCANCODE_N); x.toBetting();
    CHECK(x.p().getBankroll()==1000, "push -> 1000"); }
  out("16. Split A+K pays 1:1, not 3:2");
  { T x; x.deal({C(1),C(9),C(1,1),C(8),C(13),C(5)}); x.settle(); x.key(SDL_SCANCODE_P); x.toBetting();
    CHECK(x.p().getBankroll()==1000, "A+K wins 25, A+5 loses 25 vs 17 -> 1000"); }
  out("17. Player's Edge: no hits after doubling; redouble up to 3");
  { T x(GameMode::TwoDeckPlayersEdge); x.deal({C(2),C(9),C(3),C(8),C(2),C(2),C(2),C(2)}); x.settle();
    x.key(SDL_SCANCODE_D); x.settle();
    CHECK(x.p().hands[0].getHandSize()==3, "double dealt one card");
    x.key(SDL_SCANCODE_H); x.settle();
    CHECK(x.p().hands[0].getHandSize()==3, "hit after double ignored");
    x.key(SDL_SCANCODE_D); x.settle();
    x.key(SDL_SCANCODE_D); for(int i=0;i<3000 && (x.t.cardAnimation||!x.t.dealQueue.empty());i++){ x.t.update(1/60.f); }
    CHECK(x.p().hands[0].getDoubleCount()==3 && x.p().hands[0].getHandSize()==5, "redoubled twice more (3 doubles, 5 cards)");
    CHECK(x.t.activePlayer>=1, "hand stood by itself after the 3rd double"); }
  out("18. Strategy chart follows the mode");
  { auto row=[](int sec,int r){ std::string o; for(int c=0;c<10;c++) o+=StrategyChart::resolve(StrategyChart::code(sec,r,c)); return o; };
    T a(GameMode::TwoDeckPlayersEdge); CHECK(row(0,4)=="HHHHHHHHHH", "Player's Edge hard 12: always hit");
    T b(GameMode::SixDeckFreeBet); CHECK(row(2,8)=="SSSSSSSSSS" && row(2,2)=="PPPPPPPPPP", "Free Bet: 10s stand, 4s split");
    T c; CHECK(row(0,9)=="SSSSSSSSSR", "standard hard 17 vs A: surrender");
    c.t.setDealerHitsSoft17(false); CHECK(row(0,9)=="SSSSSSSSSS" && row(0,3)=="DDDDDDDDDD", "2 deck S17: 17 vs A stands, 11 vs A doubles");
    { T e(GameMode::SixDeck); e.t.setDealerHitsSoft17(false); CHECK(row(0,3)=="DDDDDDDDDH", "6 deck S17: 11 vs A hits"); }
    T d(GameMode::SixDeck); CHECK(row(2,6)=="PPPPPPPPPP" && row(2,0)=="PPPPPPHHHH", "6 deck DAS: 8s split vs A, 2s split vs 2"); }

  out("19. Natural blackjack paid as soon as its turn comes, before the dealer plays");
  { T x; x.deal({C(1),C(9),C(13),C(7)}); bool paidBeforeHole=false;
    for(int i=0;i<20000 && !(x.t.awaitingBets && x.t.chipAnimations.empty()); i++){
      if(!paidBeforeHole && x.p().hands[0].getBet()==0 && x.p().hands[0].getResult()==HandResult::Win) paidBeforeHole = x.t.dealer.hands[0].getHandSize()==2;
      x.t.update(1/60.f); x.t.dealDealer(); }
    CHECK(paidBeforeHole, "blackjack settled before the dealer drew (dealer 16 had to hit)");
    CHECK(x.p().getBankroll()==1037, "paid 3:2 once -> 1037"); }

  out("20. Match Down settles when the hole card flips, before the dealer draws");
  { T x(GameMode::TwoDeckPlayersEdge); x.p().raiseMatchUpBet(10); x.p().raiseMatchDownBet(10);
    // player 6,9 (15) stands; dealer 5 up, 6 hole (11) must draw a 9
    x.deal({C(6,1),C(5),C(9),C(6,2),C(9,3)}); x.settle(); x.key(SDL_SCANCODE_S);
    bool settledBeforeDraw=false, sawDraw=false;
    for(int i=0;i<20000 && !(x.t.awaitingBets && x.t.chipAnimations.empty()); i++){
      if(!sawDraw && x.t.dealer.hands[0].getHandSize()>=3){ sawDraw=true; settledBeforeDraw = x.t.matchDownResult[0]==HandResult::Win; }
      x.t.update(1/60.f); x.t.dealDealer(); }
    CHECK(sawDraw && settledBeforeDraw, "Match Down already won when the dealer's third card arrived"); }

  out("21. Match Down still settles for a busted Player's Edge hand");
  { T x(GameMode::TwoDeckPlayersEdge); Stats st; x.t.setStats(&st); x.p().raiseMatchUpBet(10); x.p().raiseMatchDownBet(10);
    x.deal({C(9,1),C(5),C(7),C(9,1),C(13)}); x.settle(); x.key(SDL_SCANCODE_H); x.toBetting();
    CHECK(st.get(Stats::MatchDown, Stats::SideBets)==1, "Match Down recorded");
    CHECK(x.t.matchDownResult[0]==HandResult::Win, "and won (9 of clubs matches the hole card)"); }

  out("22. Player's Edge double-down rescue loses only the original bet");
  { T x(GameMode::TwoDeckPlayersEdge); x.deal({C(6),C(9),C(5),C(8),C(3)}); x.settle(); x.key(SDL_SCANCODE_D); x.settle();
    CHECK(x.p().hands[0].getDoubleCount()==1 && x.p().hands[0].getHandSize()==3, "doubled 11 drew a 3 (14)");
    x.key(SDL_SCANCODE_R); x.toBetting();
    CHECK(x.p().getBankroll()==975, "rescued: lost 25 of the 50 -> 975"); }

  out("23. Lucky Ladies: soft 20 pays 4:1, Queen of Hearts pair + dealer blackjack 1000:1");
  { T x(GameMode::TwoDeckLuckyLadies); Stats st; x.t.setStats(&st); x.p().raiseSideBet(10);
    x.deal({C(1,0),C(9),C(9,1),C(8)}); x.toBetting();
    CHECK(st.get(Stats::LuckyLadies, Stats::SideWon)==40, "A-9 paid 40 on 10"); }
  { T x(GameMode::TwoDeckLuckyLadies); Stats st; x.t.setStats(&st); x.autoDecline=true; x.p().raiseSideBet(10);
    x.deal({C(12,3),C(1),C(12,3),C(13)}); x.toBetting();
    CHECK(st.get(Stats::LuckyLadies, Stats::SideWon)==10000, "Q-hearts pair vs dealer blackjack paid 10000 on 10"); }
  { T x(GameMode::TwoDeckLuckyLadies); Stats st; x.t.setStats(&st); x.p().raiseSideBet(10);
    x.deal({C(13,2),C(9),C(13,2),C(8)}); x.toBetting();
    CHECK(st.get(Stats::LuckyLadies, Stats::SideWon)==250, "2 deck: matched K-K paid 25:1"); }
  { T x(GameMode::TwoDeckLuckyLadies); Stats st; x.t.setStats(&st); x.p().raiseSideBet(10);
    x.deal({C(13,2),C(9),C(10,2),C(8)}); x.toBetting();
    CHECK(st.get(Stats::LuckyLadies, Stats::SideWon)==100, "2 deck: suited K-10 paid 10:1"); }

  out("24. Player's Edge doubled 21 pays even money, no card-count bonus");
  { T x(GameMode::TwoDeckPlayersEdge); Hand h; h.setBet(25); h.doubleBet();
    for(int v: {2,3,4,5,7}) h.cards.push_back(C(v)); 
    CHECK(x.t.spanish21Credit(h, 18, false, false)==100, "5-card doubled 21 on 50 credits 100"); }

  out("25. Lucky Ladies 6 deck pays 125/19/9");
  { T x(GameMode::SixDeckLuckyLadies); Stats st; x.t.setStats(&st); x.p().raiseSideBet(10);
    x.deal({C(13,2),C(9),C(13,2),C(8)}); x.toBetting();
    CHECK(st.get(Stats::LuckyLadies, Stats::SideWon)==190, "6 deck: matched K-K paid 19:1"); }
  { T x(GameMode::SixDeckLuckyLadies); Stats st; x.t.setStats(&st); x.p().raiseSideBet(10);
    x.deal({C(12,3),C(9),C(12,3),C(8)}); x.toBetting();
    CHECK(st.get(Stats::LuckyLadies, Stats::SideWon)==1250, "6 deck: Q-hearts pair paid 125:1"); }

  out("26. Match the Dealer: one suited match pays 9:1, two plain 8:1");
  { T x(GameMode::TwoDeckPlayersEdge); Stats st; x.t.setStats(&st); x.p().raiseMatchUpBet(10);
    x.deal({C(9,1),C(9,1),C(5),C(8)}); x.settle(); x.key(SDL_SCANCODE_S); x.toBetting();
    CHECK(st.get(Stats::MatchUp, Stats::SideWon)==90, "suited 9 vs dealer 9 of clubs paid 90"); }
  { T x(GameMode::TwoDeckPlayersEdge); Stats st; x.t.setStats(&st); x.p().raiseMatchUpBet(10);
    x.deal({C(9,1),C(9,2),C(9,3),C(8)}); x.settle(); x.key(SDL_SCANCODE_S); x.toBetting();
    CHECK(st.get(Stats::MatchUp, Stats::SideWon)==80, "two plain 9s vs dealer 9 paid 80"); }

  out("27. Player's Edge Super Bonus: suited 777 vs dealer 7");
  { T x(GameMode::TwoDeckPlayersEdge); x.deal({C(7,1),C(7,0),C(7,1),C(9),C(7,1)}); x.settle(); x.key(SDL_SCANCODE_H); x.toBetting();
    // 25 bet: 7-7-7 same suit 2:1 (+50) plus the $5000 Super Bonus
    CHECK(x.p().getBankroll()==1000+50+5000, "bankroll 6050"); std::cout.rdbuf(old); printf("    bankroll=%d hand=%d cards=%d\n", x.p().getBankroll(), x.p().hands[0].getHandTotal(), x.p().hands[0].getHandSize()); std::cout.rdbuf(sink.rdbuf()); }

  out("28. Player's Edge insurance pays 5:1 on a suited dealer blackjack");
  { T x(GameMode::TwoDeckPlayersEdge); x.deal({C(9),C(1,3),C(8),C(13,3)}); x.settle();
    for(int i=0;i<2000 && !x.t.isAwaitingInsurance(); i++){ x.t.update(1/60.f); x.t.dealDealer(); }
    x.t.answerInsurance(true); x.toBetting();
    CHECK(x.p().getBankroll()==1000-25+12*5, "lost 25, insurance 12 paid 60 -> 1035"); }

  out("29. Double deck: no double after split; 6 deck allows it");
  { T x(GameMode::TwoDeck); x.deal({C(8),C(9),C(8),C(7),C(3),C(2)}); x.settle(); x.key(SDL_SCANCODE_P); x.settle();
    CHECK(!x.t.canDoubleActiveHand(), "2 deck split 8-3: no double"); }
  { T x(GameMode::SixDeck); x.deal({C(8),C(9),C(8),C(7),C(3),C(2)}); x.settle(); x.key(SDL_SCANCODE_P); x.settle();
    CHECK(x.t.canDoubleActiveHand(), "6 deck split 8-3: double allowed"); }

  out("30. Stats go to all games and to the game being played");
  { Stats st;
    { T x(GameMode::TwoDeckLuckyLadies); x.t.setStats(&st); x.p().raiseSideBet(10); x.deal({C(10),C(9),C(9),C(8)}); x.settle(); x.key(SDL_SCANCODE_S); x.toBetting(); }
    { T x(GameMode::SixDeck); x.t.setStats(&st); x.deal({C(10),C(9),C(9),C(8)}); x.settle(); x.key(SDL_SCANCODE_S); x.toBetting(); }
    CHECK(st.get(Stats::HandsPlayed)==2, "2 hands in all games");
    CHECK(st.get(Stats::LuckyLadiesGame, Stats::HandsPlayed)==1 && st.get(Stats::StandardGame, Stats::HandsPlayed)==1, "1 in Lucky Ladies, 1 in Standard");
    CHECK(st.get(Stats::LuckyLadiesGame, Stats::LuckyLadies, Stats::SideBets)==1 && st.get(Stats::StandardGame, Stats::LuckyLadies, Stats::SideBets)==0, "side bet only under Lucky Ladies");
    CHECK(st.get(Stats::NetWinnings)==st.get(Stats::LuckyLadiesGame, Stats::NetWinnings)+st.get(Stats::StandardGame, Stats::NetWinnings), "per-game nets add up to the total"); }

  out("31. Three players, Player's Edge: seat 3's blackjack is paid before seat 1 plays");
  for(GameMode m : {GameMode::TwoDeckPlayersEdge, GameMode::SixDeck}){
    Table t{3,true,2}; int br[5]={1000,1000,1000,1000,1000}, bets[5]={25,25,25,25,25}, side[5]={0,0,0,0,0};
    t.configureGameMode(m); t.configurePlayers(3,br,bets,side); t.setDealerSpeedFactor(0.35f); t.startGame();
    std::vector<Card> f{ C(9),C(6),C(1),C(5), C(3),C(7),C(13),C(8) }; // seat0 9-3, seat1 6-7, seat2 A-K, dealer 5 up
    t.needsBurnCard=false; t.shoe.insert(t.shoe.begin(), f.begin(), f.end()); t.beginRound();
    for(int i=0;i<20000 && !t.acceptingPlayerInput(); i++){ t.update(1/60.f); t.dealDealer(); }
    CHECK(t.activePlayer==0, "seat 1 is up");
    CHECK(t.players[2].hands[0].getBet()==0 && t.players[2].hands[0].getResult()==HandResult::Win, "seat 3's blackjack already paid");
    for(int i=0;i<600;i++){ t.update(1/60.f); t.dealDealer(); }
    CHECK(t.players[2].getBankroll()==1000-25+25+37, "seat 3 bankroll 1037"); }

  out("32. Player's Edge match bets: at most the main bet; Match Down needs Match Up");
  { T x(GameMode::TwoDeckPlayersEdge); Person& p=x.p();
    p.raiseMatchUpBet(100); p.raiseMatchDownBet(100); x.t.clampBetsToBankroll(0);
    CHECK(p.getMatchUpBet()==25 && p.getMatchDownBet()==25, "both capped at the 25 main bet");
    p.lowerMatchUpBet(25); x.t.applyMatchBetRules(0);
    CHECK(p.getMatchUpBet()==0 && p.getMatchDownBet()==0, "no Match Up -> no Match Down");
    p.raiseMatchDownBet(10); x.t.clampBetsToBankroll(0);
    CHECK(p.getMatchDownBet()==0, "Match Down alone isn't allowed"); }

  out("33. Dealer hits soft 17");
  { T x; x.deal({C(10),C(1),C(9),C(6),C(4)}); x.settle(); x.key(SDL_SCANCODE_S); x.toBetting();
    CHECK(x.t.dealer.hands.empty() || true, "ran");
    CHECK(x.p().getBankroll()==1000-25, "dealer A-6 hit to 21 (A-6-4), player 19 lost"); }

  out("34. Free Bet Push 22 and Pot of Gold");
  { T x(GameMode::SixDeckFreeBet); Stats st; x.t.setStats(&st); x.p().raiseSideBet(10);
    // player 10-8 stands; dealer 10 up, 2 hole, draws K -> 22 (mixed suits -> 8:1)
    x.deal({C(10,0),C(10,1),C(8,2),C(2,3),C(13,2)}); x.settle(); x.key(SDL_SCANCODE_S); x.toBetting();
    CHECK(st.get(Stats::Push22, Stats::SideWon)==80, "dealer 22 paid Push 22 8:1"); }
  { T x(GameMode::SixDeckFreeBet); Stats st; x.t.setStats(&st); x.p().raisePotBet(10);
    // 8-8 vs 6: free split, then the first 8 gets a 2 (10) -> free double -> 2 coins
    x.deal({C(8,0),C(6),C(8,1),C(10),C(2),C(9),C(3)}); x.settle(); x.key(SDL_SCANCODE_P); x.settle();
    x.key(SDL_SCANCODE_D); x.settle(); x.key(SDL_SCANCODE_S); x.toBetting();
    CHECK(st.get(Stats::PotOfGold, Stats::SideWon)==100, "2 free-bet coins paid Pot of Gold 10:1"); }

  out("35. Double for less");
  { T x; x.t.setAskDoubleAmount(true); x.deal({C(6),C(6),C(5),C(10),C(10),C(10)}); x.settle(); x.key(SDL_SCANCODE_D);
    CHECK(x.t.isChoosingDouble() && x.t.doubleChoice==25, "panel opens at the full 25");
    x.key(SDL_SCANCODE_LEFT); x.key(SDL_SCANCODE_LEFT);
    int c=x.t.doubleChoice; x.key(SDL_SCANCODE_RETURN);
    CHECK(c==15 && x.p().hands[0].getBet()==25+c && x.p().getBankroll()==1000-25-c, "doubled for 15 (two $5 steps down)");
    x.toBetting();
    CHECK(x.p().getBankroll()==1000+25+c, "21 beat dealer 16->26 bust, paid on 25+less"); }
  { T x; x.deal({C(6),C(6),C(5),C(10),C(10)}); x.settle(); x.key(SDL_SCANCODE_D);
    CHECK(!x.t.isChoosingDouble() && x.p().hands[0].getBet()==50, "option off: full double"); }

  out("36. Practice mode holds an off-chart move once");
  { T x; x.t.setPracticeMode(true); x.deal({C(10),C(6),C(10),C(10)}); x.settle(); x.key(SDL_SCANCODE_H); x.frames(5);
    CHECK(x.p().hands[0].getHandSize()==2 && x.t.practiceHeld=='H' && x.t.practiceAdvice=='S', "hit on 20 held: chart says stand");
    x.key(SDL_SCANCODE_H);
    CHECK(x.t.practiceHeld==0 && (x.t.cardAnimation.has_value() || !x.t.dealQueue.empty()), "second hit plays anyway"); }
  { T x; x.t.setPracticeMode(true); x.deal({C(10),C(6),C(10),C(10)}); x.settle(); x.key(SDL_SCANCODE_S); x.frames(5);
    CHECK(x.t.practiceHeld==0 && !x.t.acceptingPlayerInput(), "chart move plays straight away"); }

  out("37. Count quiz before the shuffle");
  { T x; Stats st; x.t.setStats(&st); x.t.setCountQuiz(true); x.t.shoeNeedsReshuffle=true;
    x.deal({C(10),C(6),C(10),C(10)}); x.settle(); x.key(SDL_SCANCODE_S);
    for(int i=0;i<20000 && !x.t.isCountQuizShowing();i++){ x.t.update(1/60.f); x.t.dealDealer(); }
    CHECK(x.t.isCountQuizShowing() && !x.t.shuffling, "quiz waits before the shuffle");
    int rc=x.t.getRunningCount();
    for(int i=0;i<std::abs(rc);i++) x.key(rc>0?SDL_SCANCODE_RIGHT:SDL_SCANCODE_LEFT);
    x.key(SDL_SCANCODE_RETURN);
    CHECK(st.get(Stats::CountQuizzes)==1 && st.get(Stats::CountQuizCorrect)==1, "right answer scored");
    x.key(SDL_SCANCODE_RETURN); x.frames(3);
    CHECK(!x.t.isCountQuizShowing() && x.t.shuffling, "continue -> shuffle"); }

  out("37b. Count quiz game asks after every round");
  { T x; x.t.setCountQuiz(true);
    for(int round=0; round<3; round++){
      x.deal({C(10),C(6),C(10),C(10)}); x.settle(); x.key(SDL_SCANCODE_S);
      for(int i=0;i<20000 && !x.t.isCountQuizShowing();i++){ x.t.update(1/60.f); x.t.dealDealer(); }
      CHECK(x.t.isCountQuizShowing() && !x.t.awaitingBets, "quiz before betting opens");
      x.key(SDL_SCANCODE_BACKSPACE); x.toBetting();
      CHECK(x.t.awaitingBets && !x.t.isCountQuizShowing(), "skip -> betting");
    }
  }
  out("37c. Count quiz plays itself by the chart, no money");
  { T x; Stats st; x.t.setStats(&st); x.t.setCountQuiz(true);
    // 10-6 vs dealer 10: chart hits; then 10-6-5 = 21 stands itself
    x.deal({C(10),C(10),C(6),C(9),C(5)});
    for(int i=0;i<20000 && !x.t.isCountQuizShowing();i++){ x.t.update(1/60.f); x.t.dealDealer(); }
    CHECK(x.t.isCountQuizShowing(), "round played to the quiz with no input");
    CHECK(x.p().getBankroll()==1000-25, "bankroll untouched by payouts mid-round");
    x.key(SDL_SCANCODE_BACKSPACE); x.toBetting();
    CHECK(x.p().getBankroll()==1000 && x.t.chipAnimations.empty(), "bankroll back, no chips");
    CHECK(st.get(Stats::HandsPlayed)==0 && st.get(Stats::DecisionsTotal)==0 && st.history[0].empty(), "nothing in the money stats"); }
  out("37d. Practice: no insurance, decisions still scored");
  { T x; Stats st; x.t.setStats(&st); x.t.setPracticeMode(true); x.deal({C(10),C(1),C(9),C(7)});
    x.settle(); CHECK(!x.t.isAwaitingInsurance() && x.t.acceptingPlayerInput(), "no insurance offer");
    x.key(SDL_SCANCODE_S); x.toBetting();
    std::cout.rdbuf(old); printf("    dec=%lld hands=%lld br=%d bets=%d new=%d peek=%d accept=%d hs=%d held=%c adv=%c\n", st.get(Stats::DecisionsTotal), st.get(Stats::HandsPlayed), x.p().getBankroll(), (int)x.t.awaitingBets, (int)x.t.awaitingNewRound, (int)x.t.awaitingPeek, (int)x.t.acceptingPlayerInput(), (int)x.p().hands[0].getHandSize(), x.t.practiceHeld?x.t.practiceHeld:46, x.t.practiceAdvice?x.t.practiceAdvice:46); std::cout.rdbuf(sink.rdbuf());
    CHECK(st.get(Stats::DecisionsTotal)==1 && st.get(Stats::HandsPlayed)==0 && x.p().getBankroll()==1000, "decision scored, no money"); }
  out("37e. Surrender after a hit says why; chart falls back to hit");
  { T x; x.t.setPracticeMode(true); x.deal({C(3),C(10),C(6),C(9),C(7)}); x.settle(); x.key(SDL_SCANCODE_H); x.settle();
    x.key(SDL_SCANCODE_R);
    CHECK(x.t.refusedTimer>0 && x.t.refusedNote.find("SURRENDER")!=std::string::npos, "R after a hit: note shown");
    CHECK(x.t.unavailableNote()=="SURRENDER ONLY ON FIRST 2 CARDS - SO HIT", "chart note explains the hit"); }
  out("37f. Count quiz starts from the last verified count");
  { T x; x.t.setCountQuiz(true);
    x.deal({C(10),C(10),C(6),C(9),C(5)});
    for(int i=0;i<20000 && !x.t.isCountQuizShowing();i++){ x.t.update(1/60.f); x.t.dealDealer(); }
    CHECK(x.t.quizGuess==0, "first quiz starts at 0");
    x.key(SDL_SCANCODE_RETURN); int shown=x.t.getRunningCount(); x.key(SDL_SCANCODE_RETURN); x.toBetting();
    x.deal({C(2),C(3),C(4),C(5),C(10)});
    for(int i=0;i<20000 && !x.t.isCountQuizShowing();i++){ x.t.update(1/60.f); x.t.dealDealer(); }
    CHECK(x.t.quizGuess==shown && shown!=0, "next quiz starts at the last count shown"); }
  out("37g. Player's Edge card-count strategy");
  { T x(GameMode::TwoDeckPlayersEdge); x.deal({C(10),C(4),C(5),C(9)}); x.settle(); char a; x.t.chartAdvice(a);
    CHECK(a=='S', "15 (10-5) vs 4 with 2 cards: stand"); }
  { T x(GameMode::TwoDeckPlayersEdge); x.deal({C(4),C(4),C(3),C(9),C(2),C(2),C(2),C(2)}); for(int k=0;k<4;k++){ x.settle(); x.key(SDL_SCANCODE_H); } x.settle();
    char a; x.t.chartAdvice(a); std::string why=x.t.unavailableNote();
    std::cout.rdbuf(old); printf("    total=%d n=%d adv=%c why=%s\n", x.p().hands[0].getHandTotal(), x.p().hands[0].getHandSize(), a, why.c_str()); std::cout.rdbuf(sink.rdbuf());
    CHECK(x.p().hands[0].getHandTotal()==15 && x.p().hands[0].getHandSize()==6 && a=='H', "15 in 6 cards vs 4: hit (S6)"); }
  { T x(GameMode::TwoDeckPlayersEdge); x.deal({C(7,1),C(6),C(6,2),C(9)}); x.settle(); char a; x.t.chartAdvice(a);
    CHECK(a=='H' && x.t.unavailableNote()=="HIT - A 6-7-8 BONUS IS POSSIBLE", "7-6 (13) vs 6: hit for the 6-7-8"); }
  { T x(GameMode::TwoDeckPlayersEdge); x.deal({C(10),C(6),C(3),C(9)}); x.settle(); char a; x.t.chartAdvice(a);
    CHECK(a=='S', "10-3 (13) vs 6: stand"); }
  { T x(GameMode::TwoDeckPlayersEdge); x.deal({C(7,1),C(7),C(7,1),C(9)}); x.settle(); char a; x.t.chartAdvice(a);
    CHECK(a=='H', "suited 7s vs 7: hit, not split"); }
  { T x(GameMode::TwoDeckPlayersEdge); x.deal({C(7,1),C(7),C(7,2),C(9)}); x.settle(); char a; x.t.chartAdvice(a);
    CHECK(a=='P', "unsuited 7s vs 7: split"); }
  { T x; x.deal({C(10),C(1),C(9),C(7)}); x.autoDecline=true; for(int i=0;i<5000 && !x.t.acceptingPlayerInput();i++){ if(x.t.isAwaitingInsurance()) x.t.answerInsurance(false); x.t.update(1/60.f); x.t.dealDealer(); }
    char a; x.t.chartAdvice(a); CHECK(a=='S', "standard 19 vs A: stand"); }
  out("37h. Player's Edge matches the published chart");
  { T x(GameMode::TwoDeckPlayersEdge); x.deal({C(6),C(10),C(5),C(9)}); x.settle(); char a; x.t.chartAdvice(a);
    CHECK(a=='D', "11 vs 10 with 2 cards: double"); }
  { T x(GameMode::TwoDeckPlayersEdge); x.deal({C(1),C(3),C(2),C(9)}); x.settle(); char a; x.t.chartAdvice(a);
    CHECK(a=='H', "soft 13 vs 3: hit"); }
  { T x(GameMode::TwoDeckPlayersEdge); x.deal({C(2),C(9),C(3),C(9),C(9)}); x.settle(); x.key(SDL_SCANCODE_D); x.settle(); char a; x.t.chartAdvice(a);
    std::cout.rdbuf(old); printf("    doubled total=%d adv=%c\n", x.p().hands[0].getHandTotal(), a); std::cout.rdbuf(sink.rdbuf());
    CHECK(x.p().hands[0].getHandTotal()==14 && a=='R', "doubled 14 vs 9: rescue"); }
  out("37i. Double deck and Free Bet charts");
  { T x(GameMode::TwoDeck); x.deal({C(5),C(2),C(4),C(9)}); x.settle(); char a; x.t.chartAdvice(a);
    CHECK(a=='D', "2 deck: 9 vs 2 doubles"); }
  { T x(GameMode::SixDeck); x.deal({C(5),C(2),C(4),C(9)}); x.settle(); char a; x.t.chartAdvice(a);
    CHECK(a=='H', "6 deck: 9 vs 2 hits"); }
  { T x(GameMode::TwoDeck); x.deal({C(2),C(2),C(2,1),C(9)}); x.settle(); char a; x.t.chartAdvice(a);
    CHECK(a=='H', "2 deck (no DAS): 2s vs 2 hit"); }
  { T x(GameMode::SixDeckFreeBet); x.deal({C(1),C(6),C(5),C(9)}); x.settle(); char a; x.t.chartAdvice(a);
    CHECK(a=='D', "Free Bet: soft 16 vs 6 doubles"); }
  { T x(GameMode::SixDeckFreeBet); x.deal({C(9,0),C(7),C(9,1),C(9),C(8),C(8)}); x.settle(); x.key(SDL_SCANCODE_P); x.settle();
    x.key(SDL_SCANCODE_S); x.settle(); char a; x.t.chartAdvice(a);
    std::cout.rdbuf(old); printf("    active=%d free=%d total=%d adv=%c\n", x.p().getActiveHand(), (int)x.t.activeHandIsFree(), x.p().hands[x.p().getActiveHand()].getHandTotal(), a); std::cout.rdbuf(sink.rdbuf());
    CHECK(x.t.activeHandIsFree() && a=='H', "free hand 17 vs 7 hits"); }
  out("37j. Dealer stands on soft 17");
  { T x(GameMode::SixDeck); x.autoDecline=true; x.t.setDealerHitsSoft17(false); x.deal({C(10),C(1),C(9),C(6),C(4)}); for(int i=0;i<5000 && !x.t.acceptingPlayerInput();i++){ if(x.t.isAwaitingInsurance()) x.t.answerInsurance(false); x.t.update(1/60.f); x.t.dealDealer(); } x.key(SDL_SCANCODE_S); x.toBetting();
    CHECK(x.p().getBankroll()==1000+25, "dealer A-6 stands on soft 17; player 19 wins"); }
  { T x(GameMode::SixDeckFreeBet); x.t.setDealerHitsSoft17(false); CHECK(x.t.dealerHitsSoft17(), "Free Bet dealer still hits soft 17"); }
  out("37k. Index plays and trainer");
  { T x(GameMode::SixDeck); Trainer tr; x.t.setTrainer(&tr); x.t.setIndexPlays(true);
    x.t.runningCount = 12; // ~+2 true count with most of 6 decks left
    x.deal({C(10),C(10),C(6),C(9)}); x.settle(); char a; x.t.chartAdvice(a);
    std::cout.rdbuf(old); printf("    tc=%d adv=%c note=%s\n", x.t.trueCountFloor(), a, x.t.lastIndexNote.c_str()); std::cout.rdbuf(sink.rdbuf());
    CHECK(x.t.trueCountFloor()>=0 && a=='R', "15 vs 10 at TC 0+ with surrender: surrender");
    x.key(SDL_SCANCODE_H);
    CHECK(tr.totalAttempts(StrategyChart::SHOE_H17)==1 && tr.mistakes.size()==1 && tr.mistakes[0].correct=='R', "hit recorded as a mistake"); }
  { T x(GameMode::SixDeck); x.t.setIndexPlays(true); x.t.runningCount = -20;
    x.deal({C(10),C(10),C(5),C(9)}); x.settle(); char a; x.t.chartAdvice(a);
    CHECK(a=='H' && !x.t.lastIndexNote.empty(), "15 vs 10 below 0: hit, not surrender"); }
  { T x(GameMode::SixDeck); x.t.setIndexPlays(true); x.t.runningCount = 40;
    x.deal({C(10),C(10),C(4),C(9),C(2)}); x.settle(); x.key(SDL_SCANCODE_H); x.settle(); char a; x.t.chartAdvice(a);
    std::cout.rdbuf(old); printf("    tc=%d total=%d adv=%c note=%s\n", x.t.trueCountFloor(), x.p().hands[0].getHandTotal(), a, x.t.lastIndexNote.c_str()); std::cout.rdbuf(sink.rdbuf());
    CHECK(x.p().hands[0].getHandTotal()==16 && a=='S' && !x.t.lastIndexNote.empty(), "3-card 16 vs 10 at a high count: stand (index 0)"); }
  { T x(GameMode::SixDeck); x.t.setIndexPlays(true); x.t.runningCount = -20;
    x.deal({C(10),C(3),C(3),C(9)}); x.settle(); char a; x.t.chartAdvice(a);
    CHECK(x.t.trueCountFloor()<=-3 && a=='H', "13 vs 3 at TC -3: hit"); }
  { T x(GameMode::SixDeck); x.t.runningCount = -20; x.deal({C(10),C(3),C(3),C(9)}); x.settle(); char a; x.t.chartAdvice(a);
    CHECK(a=='S', "index plays off: chart stand"); }
  { Trainer a; a.record(1,0,8,8,false,'H','R',3,2,true); a.attempts[0][5][5]=7; std::vector<int> v=a.toInts(); Trainer b; b.fromInts(v);
    CHECK(b.mistakes.size()==1 && b.mistakes[0].correct=='R' && b.mistakes[0].trueCount==2 && b.attempts[0][5][5]==7, "trainer round-trips"); }
  out("37l. Free Bet: a winning free split pays only the winnings");
  { T x(GameMode::SixDeckFreeBet);
    // 8-8 vs 6, free split; both hands get 10s (18 each), dealer 6-10-10 busts
    x.deal({C(8,0),C(6),C(8,1),C(10),C(10),C(10),C(10)}); x.settle(); x.key(SDL_SCANCODE_P); x.settle();
    x.key(SDL_SCANCODE_S); x.settle(); x.key(SDL_SCANCODE_S); x.toBetting();
    std::cout.rdbuf(old); printf("    bankroll=%d\n", x.p().getBankroll()); std::cout.rdbuf(sink.rdbuf());
    CHECK(x.p().getBankroll()==1000+25+25, "both hands win 25: +50, free chip not paid out"); }
  out("37m. Strategy drill questions");
  { Trainer tr; int bad=0, deviations=0;
    for(GameMode m : {GameMode::SixDeck, GameMode::TwoDeck, GameMode::TwoDeckPlayersEdge, GameMode::SixDeckFreeBet}){
      DrillMenu d; d.open(m, true, true, tr);
      for(int i=0;i<3000;i++){ d.newQuestion(); char a=d.answer;
        if(std::string("HSDPR").find(a)==std::string::npos || !d.moveEnabled(a)) bad++;
        if(d.answerWhy.rfind("INDEX",0)==0) deviations++;
        if(d.playerCards.size()!=2) bad++; }
    }
    CHECK(bad==0, "every drill answer is a move you can press");
    CHECK(deviations>100, "index plays come up when INDEX PLAYS is on");
    DrillMenu d; d.open(GameMode::SixDeck, true, false, tr);
    int wrongBefore=tr.mistakes.size(); char a=d.answer; d.choose(a=='H'?'S':'H');
    CHECK((int)tr.mistakes.size()==wrongBefore+1 && d.dirty, "a missed drill hand is recorded"); }
  out("37n. Buy in to double or split");
  { Table t{1,true,2}; int br[5]={30,30,30,30,30}, bets[5]={25,25,25,25,25}, side[5]={0,0,0,0,0};
    t.configureGameMode(GameMode::SixDeck); t.configurePlayers(1,br,bets,side); t.setDealerSpeedFactor(0.35f); t.startGame();
    std::vector<Card> f{C(6),C(6),C(5),C(10),C(10),C(10)}; t.needsBurnCard=false; t.shoe.insert(t.shoe.begin(), f.begin(), f.end()); t.beginRound();
    for(int i=0;i<20000 && !t.acceptingPlayerInput();i++){ t.update(1/60.f); t.dealDealer(); }
    SDL_Event e{}; e.type=SDL_EVENT_KEY_DOWN; e.key.scancode=SDL_SCANCODE_D; t.handleEvent(e);
    CHECK(t.isOfferingBuyIn() && t.buyInNeeded()==30, "5 left, 25 to double: offer a 30 buy-in");
    e.key.scancode=SDL_SCANCODE_Y; t.handleEvent(e);
    CHECK(!t.isOfferingBuyIn() && t.players[0].hands[0].getBet()==50 && t.players[0].getBankroll()==10, "bought in, doubled, 10 left"); }
  { Table t{1,true,2}; int br[5]={30,30,30,30,30}, bets[5]={25,25,25,25,25}, side[5]={0,0,0,0,0};
    t.configureGameMode(GameMode::SixDeck); t.configurePlayers(1,br,bets,side); t.setDealerSpeedFactor(0.35f); t.startGame();
    std::vector<Card> f{C(8),C(6),C(8,1),C(10),C(3),C(3)}; t.needsBurnCard=false; t.shoe.insert(t.shoe.begin(), f.begin(), f.end()); t.beginRound();
    for(int i=0;i<20000 && !t.acceptingPlayerInput();i++){ t.update(1/60.f); t.dealDealer(); }
    SDL_Event e{}; e.type=SDL_EVENT_KEY_DOWN; e.key.scancode=SDL_SCANCODE_P; t.handleEvent(e);
    CHECK(t.isOfferingBuyIn(), "split offers a buy-in too");
    e.key.scancode=SDL_SCANCODE_N; t.handleEvent(e);
    CHECK(!t.isOfferingBuyIn() && t.players[0].hands.size()==1 && t.players[0].getBankroll()==5, "cancel leaves everything as it was"); }
  out("38. Round summary and graph point");
  { T x; Stats st; x.t.setStats(&st); x.deal({C(10),C(9),C(10),C(8)}); x.settle(); x.key(SDL_SCANCODE_S); x.toBetting();
    CHECK(x.t.roundNet[0]==25 && x.t.roundSummaryTimer>0, "seat shows +25");
    CHECK(st.history[Stats::AllGames].size()==1 && st.history[Stats::AllGames][0]==25 && st.history[Stats::StandardGame].size()==1, "one graph point, all games and standard");
    x.deal({C(10),C(10),C(7),C(9)}); x.settle(); x.key(SDL_SCANCODE_S); x.toBetting();
    CHECK(x.t.roundNet[0]==-25 && st.history[Stats::AllGames].back()==0, "then -25, back to 0"); }
  { Stats st; for(int i=0;i<1000;i++){ st.values[0][Stats::NetWinnings]=i; st.recordRound(); }
    CHECK(st.history[0].size()<=Stats::HISTORY_MAX && st.history[0].back()==999, "long history thinned, newest kept"); }
  out("Bet ramp grading (index plays on)");
  { T x(GameMode::SixDeck); Stats st; x.t.setStats(&st); x.t.setIndexPlays(true);
    x.t.runningCount = 400;  // far above +9 whatever is left: 8 units
    CHECK(x.t.rampBetFor(0) == 8 * 25, "ramp at +9 or more is 8 units of the starting bet");
    x.deal({C(10),C(10),C(9),C(7)}); x.settle(); x.key(SDL_SCANCODE_S); x.toBetting();
    CHECK(st.get(Stats::AllGames, Stats::RampBets) == 1 && st.get(Stats::AllGames, Stats::RampBetsCorrect) == 0, "flat 25 at a high count is graded off the ramp");
    CHECK(x.t.refusedNote.find("RAMP SAYS 8 UNITS: $200") != std::string::npos, "and says what the ramp wanted");
    x.t.runningCount = 400; x.p().currentBet = 200;
    x.deal({C(10),C(10),C(9),C(7)}); x.settle(); x.key(SDL_SCANCODE_S); x.toBetting();
    CHECK(st.get(Stats::AllGames, Stats::RampBetsCorrect) == 1, "200 at that count matches the ramp"); }
  { T x(GameMode::SixDeck); Stats st; x.t.setStats(&st);
    x.deal({C(10),C(10),C(9),C(7)}); x.settle(); x.key(SDL_SCANCODE_S); x.toBetting();
    CHECK(st.get(Stats::AllGames, Stats::RampBets) == 0, "index plays off: bets aren't graded"); }
  out("Penetration setting");
  for(int pct : {60, 75, 85}){
    Table t{1,true,6}; int br[5]={1000,1000,1000,1000,1000}, bets[5]={25,25,25,25,25}, side[5]={0,0,0,0,0};
    t.configureGameMode(GameMode::SixDeck); t.configurePlayers(1,br,bets,side); t.setPenetration(pct); t.makeShoe();
    double at = 100.0 * t.cutCardAt / t.shoeCardsAtShuffle;
    std::string what = "cut card near " + std::to_string(pct) + "% (got " + std::to_string((int)at) + ")";
    CHECK(std::fabs(at - pct) <= 1.5, what.c_str()); }
  out("Auto shuffler");
  { T x(GameMode::SixDeck); x.t.setAutoShuffler(true); x.t.makeShoe();
    bool noCut = std::none_of(x.t.shoe.begin(), x.t.shoe.end(), [](Card& c){ return c.getValue() == 14; });
    CHECK(noCut && x.t.shoe.size() == 312, "no cut card in the shoe");
    x.deal({C(10),C(10),C(9),C(7)}); x.settle(); x.key(SDL_SCANCODE_S); x.toBetting();
    CHECK(x.t.shoe.size() == 312 && x.t.discard.empty(), "cards go back in after the round");
    CHECK(x.t.getRunningCount() == 0, "count starts over");
    x.t.setAutoShuffler(false); x.deal({C(10),C(10),C(9),C(7)}); x.settle(); x.key(SDL_SCANCODE_S); x.toBetting();
    bool cut = std::any_of(x.t.shoe.begin(), x.t.shoe.end(), [](Card& c){ return c.getValue() == 14; });
    CHECK(cut, "switching it off cuts a normal shoe"); }
  out("Backup file");
  { Backup::Entries in{ {"save.txt", "gameStarted=1\nvolume=80\n"}, {"stats.txt", "END\nFILE x 3\n\n"}, {"trainer.txt", ""} };
    std::string text = Backup::serialize(in);
    Backup::Entries back; std::string err;
    CHECK(Backup::parse(text, back, err) && back == in, "saves and reads back exactly, even text that looks like the format");
    CHECK(Backup::parse(Backup::serialize({{"a", "x"}}), back, err) && back.size() == 1, "one entry");
    CHECK(!Backup::parse(text.substr(0, text.size() / 2), back, err), "a cut-short file is refused");
    CHECK(!Backup::parse("hello\n", back, err) && err == "NOT A BLACKJACK VARIANTS BACKUP", "another file is refused");
    CHECK(!Backup::parse(std::string(Backup::HEADER) + "\nFILE save.txt 999\nabc\nEND\n", back, err), "a wrong length is refused"); }
  out("Session log dates and the bet ramp");
  CHECK(SessionLogMenu::addDays(20261231, 1) == 20270101, "Dec 31 + 1 day is Jan 1");
  CHECK(SessionLogMenu::addDays(20280301, -1) == 20280229, "Mar 1 2028 - 1 day is Feb 29");
  CHECK(IndexPlays::betUnits(1) == 1 && IndexPlays::betUnits(2) == 2 && IndexPlays::betUnits(3) == 4 && IndexPlays::betUnits(4) == 6 && IndexPlays::betUnits(5) == 8 && IndexPlays::betUnits(-3) == 1, "ramp 1, 2, 4, 6, 8");
  { Trainer tr; tr.sessions.push_back({20261011, 0, 30, 25, 140}); tr.rampDrills = 7; tr.rampDrillsRight = 5;
    Trainer back; back.fromInts(tr.toInts());
    CHECK(back.sessions.size() == 1 && back.sessions[0].result == 140 && back.rampDrillsRight == 5, "sessions and ramp drill saved and loaded");
    back.reset();
    CHECK(back.sessions.size() == 1 && back.rampDrills == 0, "reset training keeps the session log"); }
  std::cout.rdbuf(old); printf("\n%s (%d failed)\n", fails?"SOME FAILED":"ALL PASSED", fails); return fails;
}
