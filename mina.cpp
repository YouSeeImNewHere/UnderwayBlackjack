
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
#include "KeyboardMenu.h"
#include "StatsMenu.h"
#include "TutorialMenu.h"
#include "Stats.h"
#include "UpdateCheck.h"
#include "SaveData.h"

enum class AppScreen {
    Menu,
    GameMode,
    GameModeAbout,
    GameOptions,
    Setup,
    Gestures,
    Keyboard,
    Stats,
    Tutorial,
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
    Keyboard,
    Stats,
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
    KeyboardMenu keyboardMenu;
    StatsMenu statsMenu;
    TutorialMenu tutorialMenu;
    Stats stats;
    UpdateCheck update;
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

    // Arrow-key menu navigation: index into the current screen's
    // focusRects(), or -1 for no highlight (the default, and after any
    // mouse click/touch -- the highlight only shows once arrows are used).
    // Reset whenever the screen changes (see focusScreenKey()).
    int focusIndex = -1;
    int focusScreenKey = -1;

    // GameOptionsMenu as it was when opened from the pause menu, so its
    // BACK can undo changes that were never applied (GO is what applies
    // them to Table) instead of leaving the menu showing settings the
    // running game isn't actually using.
    GameOptionsMenu optionsBeforeEdit;

    // saveProgress() runs once per betting phase (see mainLoopIteration);
    // reset whenever bets close.
    bool progressSavedThisBetting = false;

    AppContext() : table(3, true, 2) {}
};

// "||" icon, top right -- opens the pause menu. Kept as plain rects
// rather than DigitFont text, since a pause icon is universally
// recognizable. 82x84, with TIP the same size right under it: big enough
// that a thumb aimed at either one lands on it, not on the table (where
// any tap is a HIT). Table::drawBankrolls() keeps the P1 label left of
// x 1344 to leave room.
static SDL_FRect pauseButtonRect(){
    return SDL_FRect{ .x = 1352.0f, .y = 6.0f, .w = 82.0f, .h = 84.0f };
}

// Where a tap counts as the pause button: the button plus a margin out to
// the screen edges and partway toward the table, so a near miss doesn't
// fall through as a HIT. Ends where TIP's own margin starts (see
// Table::quickTipHitRect()).
static SDL_FRect pauseButtonHitRect(){
    return SDL_FRect{ .x = 1330.0f, .y = 0.0f, .w = 110.0f, .h = 94.0f };
}

static void drawPauseButton(AppContext& ctx){
    SDL_FRect btn = pauseButtonRect();
    SDL_SetRenderDrawColor(ctx.state.renderer, 40, 40, 40, 220);
    SDL_RenderFillRect(ctx.state.renderer, &btn);
    SDL_SetRenderDrawColor(ctx.state.renderer, 255, 255, 255, 255);
    SDL_RenderRect(ctx.state.renderer, &btn);

    SDL_FRect bar1{ .x = btn.x + btn.w / 2.0f - 14.0f, .y = btn.y + 22.0f, .w = 10.0f, .h = 40.0f };
    SDL_FRect bar2{ .x = btn.x + btn.w / 2.0f + 4.0f, .y = btn.y + 22.0f, .w = 10.0f, .h = 40.0f };
    SDL_RenderFillRect(ctx.state.renderer, &bar1);
    SDL_RenderFillRect(ctx.state.renderer, &bar2);
}

static bool isPauseButtonHit(AppContext& ctx, float windowX, float windowY){
    float lx, ly;
    if(!SDL_RenderCoordinatesFromWindow(ctx.state.renderer, windowX, windowY, &lx, &ly))
        return false;

    SDL_FPoint p{lx, ly};
    SDL_FRect hit = pauseButtonHitRect();
    return SDL_PointInRectFloat(&p, &hit);
}

// GameOptionsMenu's current choices -> the running Table.
static void applyOptionsToTable(AppContext& ctx){
    ctx.table.setDealerSpeedFactor(ctx.gameOptionsMenu.dealerSpeedFactor());
    ctx.table.setFaceDownDoubles(ctx.gameOptionsMenu.faceDownDoubles);
    ctx.table.setHideInactiveHands(ctx.gameOptionsMenu.hideInactiveHands);
}

static void saveOptions(AppContext& ctx){
    ctx.save.saveOptions(ctx.gameOptionsMenu.dealerSpeed,
        ctx.gameOptionsMenu.faceDownDoubles, ctx.gameOptionsMenu.hideInactiveHands);
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
            if(ctx.save.hasProgress)
                ctx.table.restoreProgress(ctx.save.currentBankrolls, ctx.save.totalBuyIns);
            ctx.gameOptionsMenu.setGameMode(static_cast<GameMode>(ctx.save.gameModeIndex));
            applyOptionsToTable(ctx);
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

        case MenuChoice::Keyboard:
            ctx.screen = AppScreen::Keyboard;
        break;

        case MenuChoice::Stats:
            ctx.screen = AppScreen::Stats;
        break;

        case MenuChoice::Update:
            // Downloads and installs in the background (see UpdateCheck.h);
            // mainLoopIteration() restarts into the new version when it's
            // done. Where that isn't possible, the release page instead.
            if(!ctx.update.installUpdate())
                SDL_OpenURL(UpdateCheck::RELEASES_PAGE);
        break;

        case MenuChoice::Tutorial:
            ctx.tutorialMenu.open();
            ctx.screen = AppScreen::Tutorial;
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
    applyOptionsToTable(ctx);
    saveOptions(ctx);
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

        case PauseChoice::Keyboard:
            ctx.pauseState = PauseState::Keyboard;
        break;

        case PauseChoice::Stats:
            ctx.pauseState = PauseState::Stats;
        break;

        case PauseChoice::Options:
            // The pre-game flow calls this via applyGameModeChoice()
            // instead, right as the mode's picked -- reopening mid-game
            // needs its own call since GameOptionsMenu can't just read
            // ctx.chosenMode/ctx.setupMenu itself.
            ctx.gameOptionsMenu.setGameMode(ctx.table.getGameMode());
            ctx.optionsBeforeEdit = ctx.gameOptionsMenu;
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
    applyOptionsToTable(ctx);
    saveOptions(ctx);
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

// One step "back" from wherever the player is: the BACK button on every
// menu and the Esc key both land here, so no screen is ever a dead end.
// Pre-game goes back one step through Menu -> GameMode -> GameOptions ->
// Setup; mid-game, Esc opens/closes the pause menu and every page off it
// returns to it.
static void goBack(AppContext& ctx){
    switch(ctx.screen){
    case AppScreen::Menu:
        return;
    case AppScreen::Tutorial:
        // Closing it any way counts as seen -- it won't open by itself again.
        ctx.save.saveTutorialSeen();
        ctx.screen = AppScreen::Menu;
        return;
    case AppScreen::GameMode:
    case AppScreen::Gestures:
    case AppScreen::Keyboard:
    case AppScreen::Stats:
        ctx.statsMenu.onLeave();
        // A Restart on the way here already cleared the save, so Resume
        // would have nothing to resume.
        ctx.menu.hasSavedGame = ctx.save.gameStarted;
        ctx.screen = AppScreen::Menu;
        return;
    case AppScreen::GameModeAbout:
    case AppScreen::GameOptions:
        ctx.screen = AppScreen::GameMode;
        return;
    case AppScreen::Setup:
        ctx.screen = AppScreen::GameOptions;
        return;
    case AppScreen::Playing:
        break;
    }

    switch(ctx.pauseState){
    case PauseState::None:
        ctx.pauseState = PauseState::Menu;
        return;
    case PauseState::Menu:
        ctx.pauseState = PauseState::None;
        return;
    case PauseState::Options:
        ctx.gameOptionsMenu = ctx.optionsBeforeEdit;
        ctx.pauseState = PauseState::Menu;
        return;
    case PauseState::Strategy:
    case PauseState::About:
    case PauseState::Gestures:
    case PauseState::Keyboard:
    case PauseState::Stats:
        ctx.statsMenu.onLeave();
        ctx.pauseState = PauseState::Menu;
        return;
    }
}

// A click/tap/Enter on whatever menu is showing (anything but live
// gameplay), in window coordinates -- the one place mouse, touch and
// arrow-key "Enter" all route through, so each screen's buttons behave
// identically however they were pressed.
static void handleMenuClick(AppContext& ctx, float wx, float wy){
    switch(ctx.screen){
    case AppScreen::Menu:
        applyMenuChoice(ctx, ctx.menu.handlePoint(ctx.state, wx, wy));
        return;
    case AppScreen::GameMode: {
        if(ctx.gameModeMenu.handleBackPoint(ctx.state, wx, wy)){
            goBack(ctx);
            return;
        }
        GameMode aboutMode = ctx.gameModeMenu.handleAboutPoint(ctx.state, wx, wy);
        if(aboutMode != GameMode::None){
            ctx.gameModeAboutPreview = aboutMode;
            ctx.screen = AppScreen::GameModeAbout;
        } else{
            applyGameModeChoice(ctx, ctx.gameModeMenu.handlePoint(ctx.state, wx, wy));
        }
        return;
    }
    case AppScreen::GameModeAbout:
        if(ctx.aboutMenu.handlePoint(ctx.state, wx, wy))
            goBack(ctx);
        return;
    case AppScreen::GameOptions:
        if(ctx.gameOptionsMenu.handleBackPoint(ctx.state, wx, wy))
            goBack(ctx);
        else if(ctx.gameOptionsMenu.handlePoint(ctx.state, wx, wy))
            applyGameOptionsComplete(ctx);
        return;
    case AppScreen::Setup:
        if(ctx.setupMenu.handleBackPoint(ctx.state, wx, wy))
            goBack(ctx);
        else if(ctx.setupMenu.handlePoint(ctx.state, wx, wy))
            applySetupComplete(ctx);
        return;
    case AppScreen::Gestures:
        if(ctx.gesturesMenu.handlePoint(ctx.state, wx, wy))
            goBack(ctx);
        return;
    case AppScreen::Keyboard:
        if(ctx.keyboardMenu.handlePoint(ctx.state, wx, wy))
            goBack(ctx);
        return;
    case AppScreen::Stats: {
        bool changed = false;
        if(ctx.statsMenu.handlePoint(ctx.state, wx, wy, ctx.stats, changed))
            goBack(ctx);
        if(changed)
            ctx.stats.save();
        return;
    }
    case AppScreen::Tutorial:
        if(ctx.tutorialMenu.handlePoint(ctx.state, wx, wy))
            goBack(ctx);
        return;
    case AppScreen::Playing:
        break;
    }

    switch(ctx.pauseState){
    case PauseState::None:
        return;
    case PauseState::Menu:
        applyPauseChoice(ctx, ctx.pauseMenu.handlePoint(ctx.state, wx, wy));
        return;
    case PauseState::Strategy:
        if(ctx.strategyChart.handlePoint(ctx.state, wx, wy))
            goBack(ctx);
        return;
    case PauseState::About:
        if(ctx.aboutMenu.handlePoint(ctx.state, wx, wy))
            goBack(ctx);
        return;
    case PauseState::Gestures:
        if(ctx.gesturesMenu.handlePoint(ctx.state, wx, wy))
            goBack(ctx);
        return;
    case PauseState::Keyboard:
        if(ctx.keyboardMenu.handlePoint(ctx.state, wx, wy))
            goBack(ctx);
        return;
    case PauseState::Stats: {
        bool changed = false;
        if(ctx.statsMenu.handlePoint(ctx.state, wx, wy, ctx.stats, changed))
            goBack(ctx);
        if(changed)
            ctx.stats.save();
        return;
    }
    case PauseState::Options:
        if(ctx.gameOptionsMenu.handleBackPoint(ctx.state, wx, wy))
            goBack(ctx);
        else if(ctx.gameOptionsMenu.handlePoint(ctx.state, wx, wy))
            applyGameOptionsFromPause(ctx);
        return;
    }
}

static bool inMenu(const AppContext& ctx){
    return ctx.screen != AppScreen::Playing || ctx.pauseState != PauseState::None;
}

// The selectable buttons on whatever menu is showing, in logical
// (1440x720) coordinates. Empty during live gameplay.
static std::vector<SDL_FRect> currentFocusRects(AppContext& ctx){
    switch(ctx.screen){
    case AppScreen::Menu:          return ctx.menu.focusRects();
    case AppScreen::GameMode:      return ctx.gameModeMenu.focusRects();
    case AppScreen::GameModeAbout: return ctx.aboutMenu.focusRects();
    case AppScreen::GameOptions:   return ctx.gameOptionsMenu.focusRects();
    case AppScreen::Setup:         return ctx.setupMenu.focusRects();
    case AppScreen::Gestures:      return ctx.gesturesMenu.focusRects();
    case AppScreen::Keyboard:      return ctx.keyboardMenu.focusRects();
    case AppScreen::Stats:         return ctx.statsMenu.focusRects();
    case AppScreen::Tutorial:      return ctx.tutorialMenu.focusRects();
    case AppScreen::Playing:       break;
    }
    switch(ctx.pauseState){
    case PauseState::Menu:     return ctx.pauseMenu.focusRects();
    case PauseState::Strategy: return ctx.strategyChart.focusRects();
    case PauseState::About:    return ctx.aboutMenu.focusRects();
    case PauseState::Gestures: return ctx.gesturesMenu.focusRects();
    case PauseState::Keyboard: return ctx.keyboardMenu.focusRects();
    case PauseState::Stats:    return ctx.statsMenu.focusRects();
    case PauseState::Options:  return ctx.gameOptionsMenu.focusRects();
    case PauseState::None:     break;
    }
    return {};
}

static bool optionsShowing(const AppContext& ctx){
    return ctx.screen == AppScreen::GameOptions
        || (ctx.screen == AppScreen::Playing && ctx.pauseState == PauseState::Options);
}

// The DEALER SPEED slider has the highlight: Left/Right move it, and
// Enter does nothing (there's nothing to click).
static bool sliderFocused(const AppContext& ctx){
    return optionsShowing(ctx) && ctx.focusIndex == GameOptionsMenu::SLIDER_FOCUS_INDEX;
}

// Moves the highlight to the nearest button in the arrow's direction
// (by screen position, not list order), so grids like GameModeMenu's and
// SetupMenu's steppers navigate the way they look.
static void moveFocus(AppContext& ctx, int dx, int dy){
    std::vector<SDL_FRect> rects = currentFocusRects(ctx);
    if(rects.empty())
        return;
    if(ctx.focusIndex < 0 || ctx.focusIndex >= (int)rects.size()){
        ctx.focusIndex = 0;
        return;
    }

    const SDL_FRect& cur = rects[ctx.focusIndex];
    float cx = cur.x + cur.w / 2.0f, cy = cur.y + cur.h / 2.0f;
    int best = -1;
    bool bestInLine = false;
    float bestScore = 0.0f;
    for(int i = 0; i < (int)rects.size(); i++){
        if(i == ctx.focusIndex)
            continue;
        const SDL_FRect& r = rects[i];
        float ox = r.x + r.w / 2.0f - cx;
        float oy = r.y + r.h / 2.0f - cy;
        float along = ox * dx + oy * dy;        // distance in the arrow's direction
        float across = std::fabs(ox * dy) + std::fabs(oy * dx); // sideways drift
        if(along <= 1.0f)
            continue;
        // "In line": shares some of the current button's row (for
        // left/right) or column (for up/down). Those always win over
        // diagonal neighbours, so Right from a wide button goes to the
        // thing beside it, not one a row up that happens to be closer.
        bool inLine = dx != 0
            ? (r.y < cur.y + cur.h && r.y + r.h > cur.y)
            : (r.x < cur.x + cur.w && r.x + r.w > cur.x);
        // Left/right stay within the row -- otherwise Right on the last
        // button of a row jumps to some unrelated button above it.
        if(dx != 0 && !inLine)
            continue;
        float score = along + across * 2.5f;
        if(best < 0 || (inLine && !bestInLine) || (inLine == bestInLine && score < bestScore)){
            best = i;
            bestInLine = inLine;
            bestScore = score;
        }
    }
    if(best >= 0)
        ctx.focusIndex = best;
}

// Enter/Space on a highlighted button: click its center, exactly as a
// mouse would.
static void activateFocus(AppContext& ctx){
    if(sliderFocused(ctx))
        return;
    std::vector<SDL_FRect> rects = currentFocusRects(ctx);
    if(ctx.focusIndex < 0 || ctx.focusIndex >= (int)rects.size()){
        if(!rects.empty())
            ctx.focusIndex = 0;
        return;
    }
    const SDL_FRect& r = rects[ctx.focusIndex];
    float wx, wy;
    if(!SDL_RenderCoordinatesToWindow(ctx.state.renderer, r.x + r.w / 2.0f, r.y + r.h / 2.0f, &wx, &wy))
        return;
    handleMenuClick(ctx, wx, wy);
}

static void drawFocusHighlight(AppContext& ctx){
    std::vector<SDL_FRect> rects = currentFocusRects(ctx);
    if(ctx.focusIndex < 0 || ctx.focusIndex >= (int)rects.size())
        return;
    const SDL_FRect& r = rects[ctx.focusIndex];
    SDL_SetRenderDrawColor(ctx.state.renderer, 255, 215, 0, 255);
    for(int i = 3; i <= 7; i++){
        SDL_FRect ring{ r.x - i, r.y - i, r.w + 2 * i, r.h + 2 * i };
        SDL_RenderRect(ctx.state.renderer, &ring);
    }
}

// Color for the letterbox bars around the fixed 1440x720 canvas (visible on
// wider screens like modern phones). SDL_RenderClear fills the whole window,
// bars included, so clearing with each screen's own edge color makes the
// screen look like it runs edge to edge instead of leaving the bars white
// (whatever draw color happened to be left over from last frame). Keep in
// sync with each menu's background SDL_RenderFillRect color.
static SDL_Color letterboxColor(const AppContext &ctx) {
    const SDL_Color darkGreen{10, 30, 15, 255};   // About, Gestures, StrategyChart
    const SDL_Color menuGreen{20, 70, 35, 255};   // Menu, GameMode, GameOptions
    const SDL_Color setupGreen{15, 55, 28, 255};  // SetupMenu
    const SDL_Color tableRail{86, 57, 15, 255};   // Table5Player.png's outer edge
    // tableRail under PauseMenu's 165-alpha black overlay
    const SDL_Color tableRailPaused{30, 20, 5, 255};

    switch(ctx.screen){
    case AppScreen::Menu:
    case AppScreen::GameMode:
    case AppScreen::GameOptions:
        return menuGreen;
    case AppScreen::Setup:
        return setupGreen;
    case AppScreen::GameModeAbout:
    case AppScreen::Gestures:
    case AppScreen::Keyboard:
    case AppScreen::Stats:
    case AppScreen::Tutorial:
        return darkGreen;
    case AppScreen::Playing:
        break;
    }

    switch(ctx.pauseState){
    case PauseState::None:
        return tableRail;
    case PauseState::Menu:
        return tableRailPaused;
    case PauseState::Options:
        return menuGreen;
    case PauseState::Strategy:
    case PauseState::About:
    case PauseState::Gestures:
    case PauseState::Keyboard:
    case PauseState::Stats:
        return darkGreen;
    }
    return SDL_Color{0, 0, 0, 255};
}

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

            if(inMenu(ctx)){
                // DEALER SPEED slider drags (the release goes through
                // handleMenuClick() below like any tap).
                if(optionsShowing(ctx) && event.type == SDL_EVENT_FINGER_DOWN)
                    ctx.gameOptionsMenu.pointerDown(ctx.state, event.tfinger.x * ctx.state.width, event.tfinger.y * ctx.state.height);
                else if(optionsShowing(ctx) && event.type == SDL_EVENT_FINGER_MOTION)
                    ctx.gameOptionsMenu.pointerMove(ctx.state, event.tfinger.x * ctx.state.width, event.tfinger.y * ctx.state.height);
                if(event.type == SDL_EVENT_FINGER_UP){
                    ctx.focusIndex = -1;
                    handleMenuClick(ctx, event.tfinger.x * ctx.state.width, event.tfinger.y * ctx.state.height);
                }
            } else if(ctx.screen == AppScreen::Playing && ctx.table.isAwaitingInsurance()){
                // Insurance/even money prompt: YES/NO buttons, not gestures.
                if(event.type == SDL_EVENT_FINGER_UP)
                    ctx.table.handleInsurancePoint(ctx.state,
                        event.tfinger.x * ctx.state.width,
                        event.tfinger.y * ctx.state.height);
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

        // A real mouse dragging the DEALER SPEED slider (touches arrive as
        // finger events above, so their synthetic mouse copies are skipped).
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if(event.button.button == SDL_BUTTON_LEFT && event.button.which != SDL_TOUCH_MOUSEID && optionsShowing(ctx))
                ctx.gameOptionsMenu.pointerDown(ctx.state, event.button.x, event.button.y);
        break;

        case SDL_EVENT_MOUSE_MOTION:
            if(event.motion.which != SDL_TOUCH_MOUSEID && optionsShowing(ctx))
                ctx.gameOptionsMenu.pointerMove(ctx.state, event.motion.x, event.motion.y);
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
                if(inMenu(ctx)){
                    ctx.focusIndex = -1;
                    handleMenuClick(ctx, event.button.x, event.button.y);
                } else if(ctx.screen == AppScreen::Playing && isPauseButtonHit(ctx, event.button.x, event.button.y))
                    ctx.pauseState = PauseState::Menu;
                else if(ctx.screen == AppScreen::Playing && ctx.table.isQuickTipButtonHit(ctx.state, event.button.x, event.button.y))
                    ctx.table.toggleQuickTip();
                else if(ctx.screen == AppScreen::Playing && ctx.table.isCardCountToggleHit(ctx.state, event.button.x, event.button.y))
                    ctx.table.toggleCardCount();
                else if(ctx.screen == AppScreen::Playing && ctx.table.isAwaitingInsurance())
                    ctx.table.handleInsurancePoint(ctx.state, event.button.x, event.button.y);
                else if(ctx.screen == AppScreen::Playing && ctx.table.isAwaitingBets())
                    ctx.table.handleBettingPoint(ctx.state, event.button.x, event.button.y);
            }
        break;

        case SDL_EVENT_KEY_DOWN: {
            // Everything keyboard happens on key down (see Table::
            // handleEvent) -- arrows may auto-repeat, nothing else does.
            SDL_Scancode key = event.key.scancode;
            bool isArrow = key == SDL_SCANCODE_UP || key == SDL_SCANCODE_DOWN
                || key == SDL_SCANCODE_LEFT || key == SDL_SCANCODE_RIGHT;
            if(event.key.repeat && !isArrow)
                break;

            // Android's system Back arrives as AC_BACK -- same as Esc.
            if(key == SDL_SCANCODE_ESCAPE || key == SDL_SCANCODE_AC_BACK){
                goBack(ctx);
                break;
            }

            if(inMenu(ctx)){
                if(sliderFocused(ctx) && (key == SDL_SCANCODE_LEFT || key == SDL_SCANCODE_RIGHT)){
                    ctx.gameOptionsMenu.nudgeSpeed(key == SDL_SCANCODE_LEFT ? -5 : 5);
                    break;
                }
                if(key == SDL_SCANCODE_UP)         moveFocus(ctx, 0, -1);
                else if(key == SDL_SCANCODE_DOWN)  moveFocus(ctx, 0, 1);
                else if(key == SDL_SCANCODE_LEFT)  moveFocus(ctx, -1, 0);
                else if(key == SDL_SCANCODE_RIGHT) moveFocus(ctx, 1, 0);
                else if(key == SDL_SCANCODE_RETURN || key == SDL_SCANCODE_KP_ENTER || key == SDL_SCANCODE_SPACE)
                    activateFocus(ctx);
                break;
            }

            if(key == SDL_SCANCODE_SPACE && ctx.table.isAwaitingBets()){
                ctx.table.beginRound();
                break;
            }
            ctx.table.handleEvent(event);
        break;
        }
        }
    }

    if(ctx.screen == AppScreen::Playing && ctx.pauseState == PauseState::None){
        ctx.table.update(deltaTime);
        ctx.table.dealDealer();
    }

    // Save every seat's bankroll/bets once per betting phase, after the
    // last round's payouts have landed -- so Resume picks up from here.
    if(ctx.screen == AppScreen::Playing){
        if(!ctx.table.isAwaitingBets())
            ctx.progressSavedThisBetting = false;
        else if(!ctx.progressSavedThisBetting && ctx.table.isSettledForSave()){
            int current[5] = {0, 0, 0, 0, 0}, buyIns[5] = {0, 0, 0, 0, 0};
            int bets[5] = {0, 0, 0, 0, 0}, sideBets[5] = {0, 0, 0, 0, 0};
            for(int i = 0; i < ctx.table.getNumberOfPlayers(); i++){
                current[i] = ctx.table.getPlayerBankroll(i);
                buyIns[i] = ctx.table.getPlayerTotalBuyIns(i);
                bets[i] = ctx.table.getPlayerBet(i);
                sideBets[i] = ctx.table.getPlayerSideBet(i);
            }
            ctx.save.saveProgress(current, buyIns, bets, sideBets);
            ctx.stats.save();
            ctx.progressSavedThisBetting = true;
        }
    }
    else if(ctx.screen == AppScreen::GameOptions
            || (ctx.screen == AppScreen::Playing && ctx.pauseState == PauseState::Options))
        ctx.gameOptionsMenu.update(deltaTime);

    switch(ctx.update.installState()){
        case UpdateCheck::Install::Done:
            // The new files are in place: start the new copy and quit this one.
            if(ctx.update.launchNewVersion())
                ctx.running = false;
            else
                SDL_OpenURL(UpdateCheck::RELEASES_PAGE);
            ctx.update.acknowledgeInstall();
        break;
        case UpdateCheck::Install::Failed:
            // Couldn't update in place: let the player do it by hand.
            SDL_OpenURL(UpdateCheck::RELEASES_PAGE);
            ctx.update.acknowledgeInstall();
        break;
        default:
        break;
    }

    if(ctx.screen == AppScreen::Menu){
        ctx.menu.updating = ctx.update.installState() == UpdateCheck::Install::Working;
        ctx.menu.updateVersion = ctx.update.updateAvailable() ? ctx.update.latestVersion() : "";
        ctx.menu.currentVersion = UpdateCheck::isDevBuild() ? "" : UpdateCheck::currentVersion();
    }

    int screenKey = static_cast<int>(ctx.screen) * 16 + static_cast<int>(ctx.pauseState);
    if(screenKey != ctx.focusScreenKey){
        ctx.focusScreenKey = screenKey;
        ctx.focusIndex = -1;
    }

    // perform drawing commands
    SDL_Color clearColor = letterboxColor(ctx);
    SDL_SetRenderDrawColor(ctx.state.renderer, clearColor.r, clearColor.g, clearColor.b, clearColor.a);
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
    else if(ctx.screen == AppScreen::Keyboard)
        ctx.keyboardMenu.draw(ctx.state, ctx.res);
    else if(ctx.screen == AppScreen::Stats)
        ctx.statsMenu.draw(ctx.state, ctx.res, ctx.stats);
    else if(ctx.screen == AppScreen::Tutorial)
        ctx.tutorialMenu.draw(ctx.state, ctx.res);
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
        else if(ctx.pauseState == PauseState::Keyboard)
            ctx.keyboardMenu.draw(ctx.state, ctx.res);
        else if(ctx.pauseState == PauseState::Stats)
            ctx.statsMenu.draw(ctx.state, ctx.res, ctx.stats);
        else if(ctx.pauseState == PauseState::Options)
            ctx.gameOptionsMenu.draw(ctx.state, ctx.res);
    }

    if(inMenu(ctx))
        drawFocusHighlight(ctx);

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
    ctx->stats.load();
    ctx->table.setStats(&ctx->stats);
    // Phones/tablets get DEAL in the bottom-right corner (Platform.h).
    ctx->table.setTouchLayout(usesTouchControls());
    // Windows release builds only (see UpdateCheck.h); a no-op elsewhere.
    ctx->update.start();
    // First launch: open HOW TO PLAY before anything else.
    if(!ctx->save.tutorialSeen){
        ctx->tutorialMenu.open();
        ctx->screen = AppScreen::Tutorial;
    }
    // Last-used Game Options, for Resume and for the next new game alike.
    if(ctx->save.hasOptions){
        ctx->gameOptionsMenu.dealerSpeed = ctx->save.dealerSpeed;
        ctx->gameOptionsMenu.faceDownDoubles = ctx->save.faceDownDoubles;
        ctx->gameOptionsMenu.hideInactiveHands = ctx->save.hideInactiveHands;
    }

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
    SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#if defined(SDL_PLATFORM_IOS) || defined(SDL_PLATFORM_ANDROID)
    // Fullscreen hides the status bar; landscape-only matches the fixed
    // 1440x720 canvas (the Info.plist/AndroidManifest say the same).
    windowFlags |= SDL_WINDOW_FULLSCREEN;
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
#endif
#ifdef SDL_PLATFORM_IOS
    // Deferring system gestures means a swipe near the bottom edge --
    // which the game uses for its own gestures -- needs a second swipe
    // before iOS treats it as "go home".
    SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "2");
#endif
#ifdef SDL_PLATFORM_ANDROID
    // Deliver the system Back button/gesture to the game (as
    // SDL_SCANCODE_AC_BACK, handled like Esc) instead of letting it close
    // the app from any screen.
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
#endif
#ifdef SDL_PLATFORM_WINDOWS
    // Title bar/taskbar icon from the .exe's own icon resource
    // (windows/UnderwayBlackjack.rc).
    SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON, "1");
    SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON_SMALL, "1");
#endif
    state.window = SDL_CreateWindow("Blackjack Variants",state.width,state.height,windowFlags);
    if(!state.window){
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Error","Error creating window",nullptr);
        cleanup(state);
        initSucess = false;
    }
#ifndef __EMSCRIPTEN__
    // On phones/tablets the OS decides the window size (always the full
    // screen), so the 1440x720 requested above isn't what we get. Read back
    // the real size -- finger events are normalized against it -- instead
    // of waiting on a SDL_EVENT_WINDOW_RESIZED that may never come.
    if(state.window)
        SDL_GetWindowSize(state.window, &state.width, &state.height);
#endif
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
