
#include <string>
#include <iostream>
#include <optional>
#include <SDL3/SDL_main.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include "Table.h"
#include "Menu.h"
#include "GameModeMenu.h"
#include "GameOptionsMenu.h"
#include "SetupMenu.h"
#include "PauseMenu.h"
#include "StrategyChart.h"
#include "AboutMenu.h"
#include "GesturesMenu.h"
#include "SaveData.h"

enum class AppScreen {
    Menu,
    GameMode,
    GameModeAbout,
    GameOptions,
    Setup,
    Gestures,
    Playing
};

// Only meaningful while screen == Playing. None: normal gameplay. Menu:
// the pause overlay (Table::update()/dealDealer() are skipped, but still
// drawn underneath, frozen). Strategy: the basic-strategy chart, opened
// from the pause menu. About: the rules/payouts reference, also from the
// pause menu. Gestures: the touch-controls reference, also from the pause
// menu (AppScreen::Gestures is the same screen reached from the main menu,
// before a game even exists).
enum class PauseState {
    None,
    Menu,
    Strategy,
    About,
    Gestures,
    Options
};

// Which small HUD button (if any) currently owns AppContext::uiClaimedFinger.
enum class ClaimedUIButton {
    None,
    Pause,
    Tip,
    CardCount
};

struct AppContext {
    SDLState state;
    Resources res;
    Table table;
    Menu menu;
    GameModeMenu gameModeMenu;
    GameOptionsMenu gameOptionsMenu;
    SetupMenu setupMenu;
    PauseMenu pauseMenu;
    StrategyChart strategyChart;
    AboutMenu aboutMenu;
    // One shared instance for both entry points -- the main menu's
    // AppScreen::Gestures (BACK always returns to AppScreen::Menu, the only
    // place that route is reachable from) and the pause menu's
    // PauseState::Gestures (BACK returns to PauseState::Menu, same pattern
    // as Strategy/About).
    GesturesMenu gesturesMenu;
    SaveData save;
    AppScreen screen = AppScreen::Menu;
    // Remembered between GameModeMenu and applySetupComplete().
    GameMode chosenMode = GameMode::TwoDeck;
    // Which mode's rules AppScreen::GameModeAbout is currently showing --
    // set right before switching to that screen, read by the draw
    // dispatch below. Reuses the same AboutMenu instance PauseState::About
    // does (it's just a stateless renderer keyed entirely off the mode
    // passed to draw()), so no second AboutMenu is needed.
    GameMode gameModeAboutPreview = GameMode::TwoDeck;
    PauseState pauseState = PauseState::None;
    uint64_t prevTime;
    bool running = true;

    // The finger that touched a small HUD button (pause, quick tip), if
    // any -- claimed at FINGER_DOWN and held until its matching FINGER_UP,
    // so it never once reaches Table::handleEvent. Table's own gesture
    // recognizer doesn't care where on screen a tap happens, only that
    // one did, so a tap on either button would otherwise also register as
    // a "hit" gesture.
    std::optional<SDL_FingerID> uiClaimedFinger;
    ClaimedUIButton uiClaimedButton = ClaimedUIButton::None;

    AppContext() : table(3, true, 2) {}
};

// Small "||" icon, top right -- opens the pause menu. Kept as plain rects
// rather than DigitFont text since there's no room for a readable word at
// this size, and a pause icon is universally recognizable anyway. At least
// as big as Table.h's bet buttons (56x48) -- the game-wide minimum for a
// tap target.
static SDL_FRect pauseButtonRect(){
    return SDL_FRect{ .x = 1364.0f, .y = 8.0f, .w = 68.0f, .h = 48.0f };
}

static void drawPauseButton(AppContext& ctx){
    SDL_FRect btn = pauseButtonRect();
    SDL_SetRenderDrawColor(ctx.state.renderer, 40, 40, 40, 220);
    SDL_RenderFillRect(ctx.state.renderer, &btn);
    SDL_SetRenderDrawColor(ctx.state.renderer, 255, 255, 255, 255);
    SDL_RenderRect(ctx.state.renderer, &btn);

    SDL_FRect bar1{ .x = btn.x + btn.w / 2.0f - 9.0f, .y = btn.y + 11.0f, .w = 6.0f, .h = 26.0f };
    SDL_FRect bar2{ .x = btn.x + btn.w / 2.0f + 3.0f, .y = btn.y + 11.0f, .w = 6.0f, .h = 26.0f };
    SDL_RenderFillRect(ctx.state.renderer, &bar1);
    SDL_RenderFillRect(ctx.state.renderer, &bar2);
}

static bool isPauseButtonHit(AppContext& ctx, float windowX, float windowY){
    float lx, ly;
    if(!SDL_RenderCoordinatesFromWindow(ctx.state.renderer, windowX, windowY, &lx, &ly))
        return false;

    SDL_FPoint p{lx, ly};
    SDL_FRect btn = pauseButtonRect();
    return SDL_PointInRectFloat(&p, &btn);
}

// Shared by both mouse and touch handling below: applies whichever menu
// button was tapped/clicked. Start/Restart both go to the game-mode screen
// first now (which deck count?), then Setup, before actually dealing;
// Resume skips both and just replays whatever was saved last time.
static void applyMenuChoice(AppContext& ctx, MenuChoice choice){
    switch(choice){
        case MenuChoice::Start:
            ctx.screen = AppScreen::GameMode;
        break;

        case MenuChoice::Resume:
            ctx.table.configureGameMode(static_cast<GameMode>(ctx.save.gameModeIndex));
            ctx.table.configurePlayers(ctx.save.numberOfPlayers, ctx.save.bankrolls, ctx.save.initialBets, ctx.save.sideBetSizes);
            ctx.table.startGame();
            ctx.screen = AppScreen::Playing;
        break;

        case MenuChoice::Restart:
            // See SaveData::clear()'s comment -- if the app closes before
            // SetupMenu's GO is ever reached, this keeps a future launch
            // from silently resuming the pre-restart game instead of
            // showing Start again. resetForNewGame() clears the actual
            // table state (dealer/player hands) that clearTable()'s own
            // animated sweep never got to drain once the screen changed
            // away from Playing; a fresh SetupMenu drops the previous
            // game's player count/bankroll/bet choices instead of
            // carrying them into the next one.
            ctx.save.clear();
            ctx.table.resetForNewGame();
            ctx.setupMenu = SetupMenu();
            ctx.screen = AppScreen::GameMode;
        break;

        case MenuChoice::Gestures:
            ctx.screen = AppScreen::Gestures;
        break;

        case MenuChoice::None:
        break;
    }
}

// GameModeMenu's button was tapped/clicked: remember the mode (deck count
// plus whichever side bet it has, if any), then move on to GameOptions
// (dealer speed, house-rule toggles) before Setup gets its own turn
// (players/bankrolls/side-bet size).
static void applyGameModeChoice(AppContext& ctx, GameMode mode){
    if(mode == GameMode::None)
        return;

    ctx.chosenMode = mode;
    ctx.setupMenu.setGameMode(mode);
    ctx.gameOptionsMenu.setGameMode(mode);
    ctx.screen = AppScreen::GameOptions;
}

// GameOptionsMenu's GO was tapped/clicked: bake its 3 choices into Table
// (Setup hasn't configured players yet, but these don't depend on player
// count, so there's no reason to wait), then move on to Setup.
static void applyGameOptionsComplete(AppContext& ctx){
    ctx.table.setDealerSpeedFactor(ctx.gameOptionsMenu.dealerSpeedFactor());
    ctx.table.setFaceDownDoubles(ctx.gameOptionsMenu.faceDownDoubles);
    ctx.table.setHideInactiveHands(ctx.gameOptionsMenu.hideInactiveHands);
    ctx.screen = AppScreen::Setup;
}

// SetupMenu's GO was tapped/clicked: bake its config into the table, save
// it so Resume can restore it on a future launch, then start dealing.
static void applySetupComplete(AppContext& ctx){
    int bankrolls[5] = {0, 0, 0, 0, 0};
    int initialBets[5] = {0, 0, 0, 0, 0};
    int sideBetSizes[5] = {0, 0, 0, 0, 0};
    int sideBetCount = sideBetCountFor(ctx.chosenMode);
    for(int i = 0; i < ctx.setupMenu.numberOfPlayers; i++){
        bankrolls[i] = ctx.setupMenu.playerConfigs[i].effectiveBankroll(sideBetCount);
        initialBets[i] = ctx.setupMenu.playerConfigs[i].minBet;
        sideBetSizes[i] = ctx.setupMenu.playerConfigs[i].sideBetSize;
    }

    ctx.table.configureGameMode(ctx.chosenMode);
    ctx.table.configurePlayers(ctx.setupMenu.numberOfPlayers, bankrolls, initialBets, sideBetSizes);
    ctx.save.saveGameConfig(static_cast<int>(ctx.chosenMode), ctx.setupMenu.numberOfPlayers, bankrolls, initialBets, sideBetSizes);
    ctx.table.startGame();
    ctx.screen = AppScreen::Playing;
}

// A PauseMenu button was tapped/clicked while mid-game.
static void applyPauseChoice(AppContext& ctx, PauseChoice choice){
    switch(choice){
        case PauseChoice::Resume:
            ctx.pauseState = PauseState::None;
        break;

        case PauseChoice::Restart:
            // Same as the main menu's Restart: back out to game-mode
            // selection entirely, not just a fresh round in the same mode
            // -- lets the player actually pick a different game, not only
            // reconfigure players/bankroll for the one they're already in.
            // See applyMenuChoice()'s own Restart case for why both
            // resetForNewGame() and a fresh SetupMenu are needed here too.
            ctx.save.clear();
            ctx.table.resetForNewGame();
            ctx.setupMenu = SetupMenu();
            ctx.pauseState = PauseState::None;
            ctx.screen = AppScreen::GameMode;
        break;

        case PauseChoice::StrategyTable:
            ctx.pauseState = PauseState::Strategy;
        break;

        case PauseChoice::About:
            ctx.pauseState = PauseState::About;
        break;

        case PauseChoice::Gestures:
            ctx.pauseState = PauseState::Gestures;
        break;

        case PauseChoice::Options:
            // The pre-game flow calls this via applyGameModeChoice()
            // instead, right as the mode's picked -- reopening mid-game
            // needs its own call since GameOptionsMenu can't just read
            // ctx.chosenMode/ctx.setupMenu itself.
            ctx.gameOptionsMenu.setGameMode(ctx.table.getGameMode());
            ctx.pauseState = PauseState::Options;
        break;

        case PauseChoice::None:
        break;
    }
}

// GameOptionsMenu's GO was tapped/clicked while reached from the pause
// menu mid-game, not the pre-game GameMode->GameOptions->Setup flow --
// applies the (possibly just-changed) settings to the already-running
// Table directly and drops back to the pause menu itself, instead of
// applyGameOptionsComplete()'s own move on to Setup.
static void applyGameOptionsFromPause(AppContext& ctx){
    ctx.table.setDealerSpeedFactor(ctx.gameOptionsMenu.dealerSpeedFactor());
    ctx.table.setFaceDownDoubles(ctx.gameOptionsMenu.faceDownDoubles);
    ctx.table.setHideInactiveHands(ctx.gameOptionsMenu.hideInactiveHands);
    ctx.pauseState = PauseState::Menu;
}

#ifdef __EMSCRIPTEN__
// Set once in main() after the window exists; used by onBrowserResize()
// below, which the web shell calls on every genuine browser resize/rotate.
static AppContext *g_ctx = nullptr;

// True when web/shell.html's CSS is currently rotating the canvas 90deg to
// fill a portrait phone screen with the (still internally landscape) game.
// A CSS transform rotates the *rendered output* only -- it never touches
// touch-event coordinates, so mainLoopIteration() below has to correct
// SDL_EVENT_FINGER_* positions back into the game's own unrotated space
// whenever this is true. Kept in sync with the CSS by re-checking the same
// media query every time the window size is (re)computed.
static bool g_rotatedForPortrait = false;

// Shared by main()'s initial sizing and onBrowserResize(): works out the
// largest 1440:720-letterboxed size that fits the current browser viewport.
// When the CSS is rotating the canvas 90deg for portrait, the width/height
// the *game* should fill are swapped from the raw viewport's, since what's
// visually tall on screen is, pre-rotation, wide from the canvas's own
// point of view.
static void computeLetterboxedWindowSize(AppContext *ctx, int &outW, int &outH) {
    double viewportW = EM_ASM_DOUBLE({ return window.innerWidth; });
    double viewportH = EM_ASM_DOUBLE({ return window.innerHeight; });

    g_rotatedForPortrait = EM_ASM_INT({
        return window.matchMedia('(pointer: coarse) and (orientation: portrait)').matches ? 1 : 0;
    });

    if (g_rotatedForPortrait) {
        double tmp = viewportW;
        viewportW = viewportH;
        viewportH = tmp;
    }

    double gameAspect = (double)ctx->state.logW / (double)ctx->state.logH;

    if (viewportW / viewportH > gameAspect) {
        viewportW = viewportH * gameAspect;
    } else {
        viewportH = viewportW / gameAspect;
    }

    outW = (int)viewportW;
    outH = (int)viewportH;
}

// Recomputes the largest 1440:720-letterboxed window size that fits the
// current browser viewport and applies it. Called from JS (web/shell.html)
// on window resize/orientationchange -- SDL doesn't reliably pick up
// genuine browser-driven size changes on its own here, so this drives it
// explicitly rather than hoping SDL's own resize-observer catches it.
extern "C" EMSCRIPTEN_KEEPALIVE void onBrowserResize() {
    if (!g_ctx || !g_ctx->state.window)
        return;

    int w, h;
    computeLetterboxedWindowSize(g_ctx, w, h);

    SDL_SetWindowSize(g_ctx->state.window, w, h);

    // SDL_CreateWindow sets the canvas's inline CSS width/height to match
    // at creation time, but SDL_SetWindowSize doesn't repeat that on later
    // resizes -- confirmed: the backing buffer (canvas.width/height
    // attributes) updates fine, but the CSS display size stays frozen at
    // whatever it was initially, leaving the game rendered correctly but
    // tiny in a corner. Set it explicitly here too. (CSS separately applies
    // the 90deg rotation transform on top of this width/height -- see
    // web/shell.html -- so this is still the pre-rotation size.)
    EM_ASM({
        var c = document.getElementById('canvas');
        c.style.width = $0 + 'px';
        c.style.height = $1 + 'px';
    }, w, h);
}
#endif

static void mainLoopIteration(void *arg) {
    AppContext &ctx = *static_cast<AppContext *>(arg);

    uint64_t nowTime = SDL_GetTicks();
    float deltaTime = (nowTime - ctx.prevTime) / 1000.0f;

    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        switch(event.type)
        {
        case SDL_EVENT_QUIT:
            ctx.running = false;
        break;

        case SDL_EVENT_WINDOW_RESIZED:
            ctx.state.width = event.window.data1;
            ctx.state.height = event.window.data2;
        break;

        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_UP:
#ifdef __EMSCRIPTEN__
            // CSS only rotates the *rendered pixels* -- touch coordinates
            // still arrive in the canvas's own unrotated space, so they
            // need the inverse of that 90deg rotation applied here before
            // the game ever sees them. See g_rotatedForPortrait's comment.
            if (g_rotatedForPortrait) {
                float origX = event.tfinger.x;
                float origY = event.tfinger.y;
                event.tfinger.x = origY;
                event.tfinger.y = 1.0f - origX;
            }
#endif
            // tfinger.x/y are normalized [0,1] against the window, not the
            // menu's logical/letterboxed coordinate space -- scale back out
            // to window pixels first; handlePoint() does the rest of the
            // conversion from there.

            // A finger that touched the pause or quick-tip button is
            // claimed for its whole DOWN/MOTION/UP lifecycle -- never
            // forwarded to Table at all, so it can never also register as
            // a gameplay tap (Table's gesture recognizer doesn't care
            // where on screen a tap lands, just that one happened).
            if(ctx.screen == AppScreen::Playing && ctx.pauseState == PauseState::None){
                if(event.type == SDL_EVENT_FINGER_DOWN){
                    float wx = event.tfinger.x * ctx.state.width;
                    float wy = event.tfinger.y * ctx.state.height;
                    if(isPauseButtonHit(ctx, wx, wy)){
                        ctx.uiClaimedFinger = event.tfinger.fingerID;
                        ctx.uiClaimedButton = ClaimedUIButton::Pause;
                    } else if(ctx.table.isQuickTipButtonHit(ctx.state, wx, wy)){
                        ctx.uiClaimedFinger = event.tfinger.fingerID;
                        ctx.uiClaimedButton = ClaimedUIButton::Tip;
                    } else if(ctx.table.isCardCountToggleHit(ctx.state, wx, wy)){
                        ctx.uiClaimedFinger = event.tfinger.fingerID;
                        ctx.uiClaimedButton = ClaimedUIButton::CardCount;
                    }
                }

                if(ctx.uiClaimedFinger && *ctx.uiClaimedFinger == event.tfinger.fingerID){
                    if(event.type == SDL_EVENT_FINGER_UP){
                        float wx = event.tfinger.x * ctx.state.width;
                        float wy = event.tfinger.y * ctx.state.height;
                        if(ctx.uiClaimedButton == ClaimedUIButton::Pause && isPauseButtonHit(ctx, wx, wy))
                            ctx.pauseState = PauseState::Menu;
                        else if(ctx.uiClaimedButton == ClaimedUIButton::Tip && ctx.table.isQuickTipButtonHit(ctx.state, wx, wy))
                            ctx.table.toggleQuickTip();
                        else if(ctx.uiClaimedButton == ClaimedUIButton::CardCount && ctx.table.isCardCountToggleHit(ctx.state, wx, wy))
                            ctx.table.toggleCardCount();

                        ctx.uiClaimedFinger.reset();
                        ctx.uiClaimedButton = ClaimedUIButton::None;
                    }
                    break;
                }
            }

            if(ctx.screen == AppScreen::Menu){
                if(event.type == SDL_EVENT_FINGER_UP)
                    applyMenuChoice(ctx, ctx.menu.handlePoint(ctx.state,
                        event.tfinger.x * ctx.state.width,
                        event.tfinger.y * ctx.state.height));
            } else if(ctx.screen == AppScreen::GameMode){
                if(event.type == SDL_EVENT_FINGER_UP){
                    float wx = event.tfinger.x * ctx.state.width;
                    float wy = event.tfinger.y * ctx.state.height;
                    GameMode aboutMode = ctx.gameModeMenu.handleAboutPoint(ctx.state, wx, wy);
                    if(aboutMode != GameMode::None){
                        ctx.gameModeAboutPreview = aboutMode;
                        ctx.screen = AppScreen::GameModeAbout;
                    } else{
                        applyGameModeChoice(ctx, ctx.gameModeMenu.handlePoint(ctx.state, wx, wy));
                    }
                }
            } else if(ctx.screen == AppScreen::GameModeAbout){
                if(event.type == SDL_EVENT_FINGER_UP && ctx.aboutMenu.handlePoint(ctx.state,
                        event.tfinger.x * ctx.state.width,
                        event.tfinger.y * ctx.state.height))
                    ctx.screen = AppScreen::GameMode;
            } else if(ctx.screen == AppScreen::GameOptions){
                if(event.type == SDL_EVENT_FINGER_UP && ctx.gameOptionsMenu.handlePoint(ctx.state,
                        event.tfinger.x * ctx.state.width,
                        event.tfinger.y * ctx.state.height))
                    applyGameOptionsComplete(ctx);
            } else if(ctx.screen == AppScreen::Setup){
                if(event.type == SDL_EVENT_FINGER_UP && ctx.setupMenu.handlePoint(ctx.state,
                        event.tfinger.x * ctx.state.width,
                        event.tfinger.y * ctx.state.height))
                    applySetupComplete(ctx);
            } else if(ctx.screen == AppScreen::Gestures){
                if(event.type == SDL_EVENT_FINGER_UP && ctx.gesturesMenu.handlePoint(ctx.state,
                        event.tfinger.x * ctx.state.width,
                        event.tfinger.y * ctx.state.height))
                    ctx.screen = AppScreen::Menu;
            } else if(ctx.screen == AppScreen::Playing && ctx.pauseState == PauseState::Menu){
                if(event.type == SDL_EVENT_FINGER_UP)
                    applyPauseChoice(ctx, ctx.pauseMenu.handlePoint(ctx.state,
                        event.tfinger.x * ctx.state.width,
                        event.tfinger.y * ctx.state.height));
            } else if(ctx.screen == AppScreen::Playing && ctx.pauseState == PauseState::Strategy){
                if(event.type == SDL_EVENT_FINGER_UP && ctx.strategyChart.handlePoint(ctx.state,
                        event.tfinger.x * ctx.state.width,
                        event.tfinger.y * ctx.state.height))
                    ctx.pauseState = PauseState::Menu;
            } else if(ctx.screen == AppScreen::Playing && ctx.pauseState == PauseState::About){
                if(event.type == SDL_EVENT_FINGER_UP && ctx.aboutMenu.handlePoint(ctx.state,
                        event.tfinger.x * ctx.state.width,
                        event.tfinger.y * ctx.state.height))
                    ctx.pauseState = PauseState::Menu;
            } else if(ctx.screen == AppScreen::Playing && ctx.pauseState == PauseState::Gestures){
                if(event.type == SDL_EVENT_FINGER_UP && ctx.gesturesMenu.handlePoint(ctx.state,
                        event.tfinger.x * ctx.state.width,
                        event.tfinger.y * ctx.state.height))
                    ctx.pauseState = PauseState::Menu;
            } else if(ctx.screen == AppScreen::Playing && ctx.pauseState == PauseState::Options){
                if(event.type == SDL_EVENT_FINGER_UP && ctx.gameOptionsMenu.handlePoint(ctx.state,
                        event.tfinger.x * ctx.state.width,
                        event.tfinger.y * ctx.state.height))
                    applyGameOptionsFromPause(ctx);
            } else if(ctx.screen == AppScreen::Playing && ctx.table.isAwaitingBets()){
                // Betting phase: raise/lower/DEAL, not gameplay gestures.
                if(event.type == SDL_EVENT_FINGER_UP)
                    ctx.table.handleBettingPoint(ctx.state,
                        event.tfinger.x * ctx.state.width,
                        event.tfinger.y * ctx.state.height);
            } else {
                ctx.table.handleEvent(event);
            }
        break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            // Menu/Setup/betting: gameplay itself is touch/keyboard-gesture
            // driven by design (see Table::handleEvent), but a plain mouse
            // click is the natural way to work these screens on a
            // desktop/native run.
            // SDL synthesizes a mouse event for every touch on most
            // platforms (touchscreens, mobile browsers) so old mouse-only
            // code still works -- but this codebase already handles the
            // real SDL_EVENT_FINGER_UP directly, so without this check a
            // single physical tap fired both paths and double-processed
            // (e.g. the chip selector jumping two steps per tap: 25->100
            // from the finger event, then ->500 again from the synthetic
            // mouse one). SDL_TOUCH_MOUSEID marks exactly those synthetic
            // events; a real mouse's `which` is never that value.
            if(event.button.button == SDL_BUTTON_LEFT && event.button.which != SDL_TOUCH_MOUSEID){
                if(ctx.screen == AppScreen::Menu)
                    applyMenuChoice(ctx, ctx.menu.handlePoint(ctx.state, event.button.x, event.button.y));
                else if(ctx.screen == AppScreen::GameMode){
                    GameMode aboutMode = ctx.gameModeMenu.handleAboutPoint(ctx.state, event.button.x, event.button.y);
                    if(aboutMode != GameMode::None){
                        ctx.gameModeAboutPreview = aboutMode;
                        ctx.screen = AppScreen::GameModeAbout;
                    } else{
                        applyGameModeChoice(ctx, ctx.gameModeMenu.handlePoint(ctx.state, event.button.x, event.button.y));
                    }
                }
                else if(ctx.screen == AppScreen::GameModeAbout){
                    if(ctx.aboutMenu.handlePoint(ctx.state, event.button.x, event.button.y))
                        ctx.screen = AppScreen::GameMode;
                }
                else if(ctx.screen == AppScreen::GameOptions && ctx.gameOptionsMenu.handlePoint(ctx.state, event.button.x, event.button.y))
                    applyGameOptionsComplete(ctx);
                else if(ctx.screen == AppScreen::Setup && ctx.setupMenu.handlePoint(ctx.state, event.button.x, event.button.y))
                    applySetupComplete(ctx);
                else if(ctx.screen == AppScreen::Gestures){
                    if(ctx.gesturesMenu.handlePoint(ctx.state, event.button.x, event.button.y))
                        ctx.screen = AppScreen::Menu;
                }
                else if(ctx.screen == AppScreen::Playing && ctx.pauseState == PauseState::Menu)
                    applyPauseChoice(ctx, ctx.pauseMenu.handlePoint(ctx.state, event.button.x, event.button.y));
                else if(ctx.screen == AppScreen::Playing && ctx.pauseState == PauseState::Strategy){
                    if(ctx.strategyChart.handlePoint(ctx.state, event.button.x, event.button.y))
                        ctx.pauseState = PauseState::Menu;
                } else if(ctx.screen == AppScreen::Playing && ctx.pauseState == PauseState::About){
                    if(ctx.aboutMenu.handlePoint(ctx.state, event.button.x, event.button.y))
                        ctx.pauseState = PauseState::Menu;
                } else if(ctx.screen == AppScreen::Playing && ctx.pauseState == PauseState::Gestures){
                    if(ctx.gesturesMenu.handlePoint(ctx.state, event.button.x, event.button.y))
                        ctx.pauseState = PauseState::Menu;
                } else if(ctx.screen == AppScreen::Playing && ctx.pauseState == PauseState::Options){
                    if(ctx.gameOptionsMenu.handlePoint(ctx.state, event.button.x, event.button.y))
                        applyGameOptionsFromPause(ctx);
                } else if(ctx.screen == AppScreen::Playing && isPauseButtonHit(ctx, event.button.x, event.button.y))
                    ctx.pauseState = PauseState::Menu;
                else if(ctx.screen == AppScreen::Playing && ctx.table.isQuickTipButtonHit(ctx.state, event.button.x, event.button.y))
                    ctx.table.toggleQuickTip();
                else if(ctx.screen == AppScreen::Playing && ctx.table.isCardCountToggleHit(ctx.state, event.button.x, event.button.y))
                    ctx.table.toggleCardCount();
                else if(ctx.screen == AppScreen::Playing && ctx.table.isAwaitingBets())
                    ctx.table.handleBettingPoint(ctx.state, event.button.x, event.button.y);
            }
        break;

        case SDL_EVENT_KEY_UP:
            if(ctx.screen == AppScreen::Playing && ctx.pauseState == PauseState::None)
                ctx.table.handleEvent(event);
        break;
        }
    }

    if(ctx.screen == AppScreen::Playing && ctx.pauseState == PauseState::None){
        ctx.table.update(deltaTime);
        ctx.table.dealDealer();
    }
    else if(ctx.screen == AppScreen::GameOptions
            || (ctx.screen == AppScreen::Playing && ctx.pauseState == PauseState::Options))
        ctx.gameOptionsMenu.update(deltaTime);

    // perform drawing commands
    SDL_RenderClear(ctx.state.renderer);
    if(ctx.screen == AppScreen::Menu)
        ctx.menu.draw(ctx.state, ctx.res);
    else if(ctx.screen == AppScreen::GameMode)
        ctx.gameModeMenu.draw(ctx.state, ctx.res);
    else if(ctx.screen == AppScreen::GameModeAbout)
        ctx.aboutMenu.draw(ctx.state, ctx.res, ctx.gameModeAboutPreview);
    else if(ctx.screen == AppScreen::GameOptions)
        ctx.gameOptionsMenu.draw(ctx.state, ctx.res);
    else if(ctx.screen == AppScreen::Setup)
        ctx.setupMenu.draw(ctx.state, ctx.res);
    else if(ctx.screen == AppScreen::Gestures)
        ctx.gesturesMenu.draw(ctx.state, ctx.res);
    else{
        // The table's still drawn even while paused -- frozen underneath
        // the overlay -- rather than swapped out for a blank screen.
        ctx.table.draw(ctx.state, ctx.res);

        if(ctx.pauseState == PauseState::None)
            drawPauseButton(ctx);
        else if(ctx.pauseState == PauseState::Menu)
            ctx.pauseMenu.draw(ctx.state, ctx.res);
        else if(ctx.pauseState == PauseState::Strategy){
            int section = 0, row = 0, col = 0;
            bool hasHighlight = ctx.table.getStrategySituation(section, row, col);
            ctx.strategyChart.draw(ctx.state, ctx.res, hasHighlight, section, row, col);
        }
        else if(ctx.pauseState == PauseState::About)
            ctx.aboutMenu.draw(ctx.state, ctx.res, ctx.table.getGameMode());
        else if(ctx.pauseState == PauseState::Gestures)
            ctx.gesturesMenu.draw(ctx.state, ctx.res);
        else if(ctx.pauseState == PauseState::Options)
            ctx.gameOptionsMenu.draw(ctx.state, ctx.res);
    }

    // swap buffer and present
    SDL_RenderPresent(ctx.state.renderer);
    ctx.prevTime = nowTime;

#ifdef __EMSCRIPTEN__
    if(!ctx.running)
        emscripten_cancel_main_loop();
#endif
}

int main(int argc,char *argv[]) {

    // Heap-allocated and leaked deliberately: on the web the "loop" is a
    // browser callback that outlives main() returning, so this can't live
    // on main's stack the way it did in the native single-threaded-loop version.
    AppContext *ctx = new AppContext();
    ctx->state.width = 1440;
    ctx->state.height = 720;
    ctx->state.logW = 1440;
    ctx->state.logH = 720;

    ctx->save.load();
    ctx->menu.hasSavedGame = ctx->save.gameStarted;

#ifdef __EMSCRIPTEN__
    // SDL writes the window's w/h straight onto the canvas as an inline
    // CSS style the moment it's created, which always wins over anything
    // in web/shell.html's stylesheet. So instead of fighting that after
    // the fact, ask the browser for the actual viewport size up front and
    // request a correctly letterboxed (1440:720) window size to begin with.
    computeLetterboxedWindowSize(ctx, ctx->state.width, ctx->state.height);
#endif

    // initilize screen
    if(!initialize(ctx->state)){
        return 1;
    }

#ifdef __EMSCRIPTEN__
    // Now that the window exists, let onBrowserResize() (called from JS on
    // every subsequent resize/orientationchange -- see web/shell.html) keep
    // it correctly sized. SDL doesn't reliably resize itself in response to
    // genuine browser-driven size changes here, so this drives it explicitly.
    g_ctx = ctx;
#endif

    //load game assets
    ctx->res.load(ctx->state);

    ctx->prevTime = SDL_GetTicks();

#ifdef __EMSCRIPTEN__
    // Browsers can't block on a while loop without freezing the tab, so the
    // game loop runs as a callback driven by requestAnimationFrame instead.
    emscripten_set_main_loop_arg(mainLoopIteration, ctx, 0, true);
#else
    while(ctx->running){
        mainLoopIteration(ctx);
    }

    ctx->res.unload();
    cleanup(ctx->state);
    delete ctx;
#endif

    return 0;
}

bool initialize(SDLState &state){
    bool initSucess = true;
    if(!SDL_Init(SDL_INIT_VIDEO)){
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Error","Error initializing SDL3",nullptr);
        initSucess = false;
    }

    // window
    // SDL_WINDOW_HIGH_PIXEL_DENSITY makes the canvas backing buffer match
    // the display's actual pixel density instead of rendering 1 pixel per
    // CSS pixel and getting blurrily upscaled on high-DPI phone screens.
    // web/shell.html has no CSS sizing rules for the canvas at all -- the
    // correct size is computed from the real viewport and requested here
    // directly (see main()), and kept in sync afterward by onBrowserResize().
    state.window = SDL_CreateWindow("Hello World",state.width,state.height,SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if(!state.window){
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Error","Error creating window",nullptr);
        cleanup(state);
        initSucess = false;
    }
    //renderer
    state.renderer = SDL_CreateRenderer(state.window,nullptr);
    if(!state.renderer){
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Error","Error creating renderer",state.window);
        cleanup(state);
        initSucess = false;
    }
    // maintain aspect resolution no matter the screen resolution
    SDL_SetRenderLogicalPresentation(state.renderer,state.logW,state.logH,SDL_LOGICAL_PRESENTATION_LETTERBOX);
    return initSucess;
}

void cleanup(SDLState &state){
    SDL_DestroyRenderer(state.renderer);
    SDL_DestroyWindow(state.window);
    SDL_Quit();
}
