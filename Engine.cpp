#include "Engine.hpp"
#include "Predictor.hpp"

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace pbuf {

static Engine s_engine;
Engine& engine() { return s_engine; }

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

static GM modeOf(PlayerObject* p) {
    if (p->m_isShip) return GM::Ship;
    if (p->m_isBird) return GM::Ufo;
    if (p->m_isDart) return GM::Wave;
    if (p->m_isSwing) return GM::Swing;
    if (p->m_isBall) return GM::Ball;
    if (p->m_isRobot) return GM::Robot;
    if (p->m_isSpider) return GM::Spider;
    return GM::Cube;
}

static bool isGround(GM gm) {
    return gm == GM::Cube || gm == GM::Ball || gm == GM::Robot || gm == GM::Spider;
}

static int ringsTouching(PlayerObject* p) {
    return p->m_touchingRings ? static_cast<int>(p->m_touchingRings->count()) : 0;
}

static char const* act(bool down) { return down ? "PRESS" : "RELEASE"; }

// ---------------------------------------------------------------------------
//  State queries
// ---------------------------------------------------------------------------

bool Engine::isActive(PlayLayer* pl) const {
    auto const& c = cfg();
    if (!c.enabled || !pl) return false;
    if (c.practiceOnly && !pl->m_isPracticeMode) return false;
    if (pl->m_isPlatformer && !c.platformer) return false;
    return true;
}

int Engine::tps() const {
    int o = cfg().tpsOverride;
    return o > 0 ? o : predictor().detectedTps();
}

float Engine::stepsPerTick() const {
    return std::max(0.05f, tps() / 240.0f);
}

int Engine::ticksToSteps(int ticks) const {
    if (ticks <= 0) return 0;
    return std::max(1, static_cast<int>(std::lround(ticks * stepsPerTick())));
}

bool Engine::twoPlayer(PlayLayer* pl) const {
    if (pl->m_levelSettings) return pl->m_levelSettings->m_twoPlayerMode;
    return pl->m_level && pl->m_level->m_twoPlayerMode;
}

bool Engine::dual(PlayLayer* pl) const {
    return pl->m_gameState.m_isDualMode && pl->m_player2 != nullptr;
}

PlayerObject* Engine::primary(PlayLayer* pl, int c) const {
    if (c == 1 && twoPlayer(pl)) return pl->m_player2;
    return pl->m_player1;
}

PlayerObject* Engine::secondary(PlayLayer* pl, int c) const {
    if (c == 0 && !twoPlayer(pl) && dual(pl)) return pl->m_player2;
    return nullptr;
}

Strat Engine::effectiveStrat(PlayLayer* pl, int c, GM gm) const {
    Strat s = stratFor(gm);
    if (windowTicksFor(gm) <= 0) return Strat::Off;
    // 1-player dual: if the other icon's mode is not buffered, stay raw
    // (v1 rule: "never touch ship, or dual with ship")
    if (auto p2 = secondary(pl, c)) {
        GM g2 = modeOf(p2);
        if (stratFor(g2) == Strat::Off || windowTicksFor(g2) <= 0) return Strat::Off;
    }
    return s;
}

GM Engine::currentMode(PlayLayer* pl) const {
    if (!pl || !pl->m_player1) return GM::Cube;
    return modeOf(pl->m_player1);
}

// ---------------------------------------------------------------------------
//  Lifecycle
// ---------------------------------------------------------------------------

void Engine::clearController(Controller& s, bool keepGame) {
    s.queue.clear();
    s.suppress = false;
    s.landPending = false;
    s.streak = 0;
    if (!keepGame) s.game = false;
}

void Engine::onLevelStart() {
    m_step = 0;
    m_inStep = false;
    m_injecting = false;
    for (auto& s : m_ctl) s = Controller{};
    m_stats.savedAttempt = 0;
    m_stats.lastKind.clear();
    m_dirty = true;
}

void Engine::onReset() {
    m_step = 0;
    for (auto& s : m_ctl) {
        clearController(s, false);
        s.lastPressApplied = -1000000;
        s.lastReleaseApplied = -1000000;
        s.lastInjectStep = -1000000;
        s.lastInputStep = -1000000;
        s.lastModeIdx = -1;
    }
    m_stats.savedAttempt = 0;
    m_dirty = true;
}

void Engine::onDeath() {
    // Real player died: drop anything pending so the respawn is 100% clean
    for (auto& s : m_ctl) clearController(s, false);
}

// ---------------------------------------------------------------------------
//  Scheduling
// ---------------------------------------------------------------------------

void Engine::applied(Controller& s, bool down, int step) {
    if (down) s.lastPressApplied = step;
    else s.lastReleaseApplied = step;
}

// Returns true when the event was queued (swallow it), false when it must go
// to the game right now (the bookkeeping is already done in that case).
bool Engine::enqueue(Controller& s, bool down, int target, int step, char const* why) {
    if (!s.queue.empty()) {
        if (s.queue.back().down == down) return true; // redundant
        target = std::max(target, s.queue.back().target + 1);
    } else {
        if (down == s.game) return false;             // no state change, let it through
        // never apply two opposite transitions in the same physics step
        if (s.lastInjectStep >= step) target = std::max(target, s.lastInjectStep + 1);
        if (target <= step) {
            s.game = down;
            applied(s, down, step);
            return false;
        }
    }
    s.queue.push_back({down, target, why});
    dbg(fmt::format("queue {} for step {} (now {}) [{}]", act(down), target, step, why));
    return true;
}

int Engine::pendingPressStep(Controller const& s) const {
    for (auto it = s.queue.rbegin(); it != s.queue.rend(); ++it) {
        if (it->down) return it->target;
    }
    return s.lastPressApplied;
}

int Engine::pendingReleaseStep(Controller const& s) const {
    for (auto it = s.queue.rbegin(); it != s.queue.rend(); ++it) {
        if (!it->down) return it->target;
    }
    return s.lastReleaseApplied;
}

void Engine::inject(GJBaseGameLayer* layer, int c, bool down, char const* why) {
    auto pl = PlayLayer::get();
    m_injecting = true;
    layer->handleButton(down, 1, c == 0);
    if (c == 0 && pl && !twoPlayer(pl) && dual(pl)) {
        layer->handleButton(down, 1, false);
    }
    m_injecting = false;

    auto& s = m_ctl[c];
    s.game = down;
    s.lastInjectStep = m_step;
    applied(s, down, m_step);
    dbg(fmt::format("P{} inject {} at step {} [{}]", c + 1, act(down), m_step, why));
}

void Engine::reportSave(char const* kind, int c, GM gm, bool down, int delaySteps, std::string const& extra) {
    float ticks = delaySteps / stepsPerTick();
    m_stats.savedAttempt++;
    m_stats.savedSession++;
    m_stats.lastSavedTicksX10 = static_cast<int>(std::lround(ticks * 10.f));
    m_stats.lastKind = kind;
    m_flash = true;
    m_dirty = true;
    out(fmt::format("*** SAVED +{:.1f} TICKS ({:.2f} frames @ 60fps) {} [{} P{} {}] {} ***",
                    ticks, ticks / 4.0f, act(down), kind, c + 1, gmName(gm), extra));
}

// ---------------------------------------------------------------------------
//  Input
// ---------------------------------------------------------------------------

bool Engine::onInput(GJBaseGameLayer* layer, bool down, bool isPlayer1) {
    auto pl = PlayLayer::get();
    if (!pl || static_cast<GJBaseGameLayer*>(pl) != layer) return false;

    auto const& c = cfg();
    int ci = isPlayer1 ? 0 : 1;
    bool twoP = twoPlayer(pl);

    if (!isActive(pl)) {
        m_ctl[ci].physical = down;
        m_ctl[ci].game = down;
        return false;
    }

    // 1-player mode: player 2 inputs simply follow player 1 (dual mirroring)
    if (!isPlayer1 && !twoP) {
        auto const& m = m_ctl[0];
        return !m.queue.empty() || m.suppress;
    }

    Controller& s = m_ctl[ci];
    if (!isPlayer1 && !c.twoPlayer) {
        s.physical = down;
        s.game = down;
        return false;
    }

    int step = inputStep();
    s.physical = down;

    PlayerObject* p = primary(pl, ci);
    if (!p || p->m_isDead) {
        clearController(s, true);
        s.game = down;
        applied(s, down, step);
        return false;
    }

    // Lethal filter / debounce swallow everything until the button is back
    // in the state the game already has.
    if (s.suppress) {
        if (down == s.game) s.suppress = false;
        dbg(fmt::format("P{} suppressed {}", ci + 1, act(down)));
        return true;
    }

    // Spam streak (consecutive inputs closer than the spam interval)
    int spamIv = ticksToSteps(c.spamInterval);
    if (step - s.lastInputStep < spamIv) s.streak++;
    else s.streak = 0;
    s.lastInputStep = step;

    PlayerObject* p2 = secondary(pl, ci);
    GM gm = modeOf(p);
    Strat st = effectiveStrat(pl, ci, gm);
    int win = ticksToSteps(windowTicksFor(gm));
    bool hybridFly = st == Strat::Hybrid && !isGround(gm);
    bool hybridGround = st == Strat::Hybrid && isGround(gm);

    // Shaping strategies compute a target step without simulation
    auto shapedTarget = [&](Strat strat) -> int {
        switch (strat) {
            case Strat::MinHold:
                if (!down) return std::max(step, pendingPressStep(s) + win);
                return step;
            case Strat::MinGap:
                if (down) return std::max(step, pendingReleaseStep(s) + win);
                return step;
            case Strat::Grid:
                return ((step + win - 1) / win) * win;
            default:
                return step;
        }
    };

    // Something is already queued: keep strict chronological order behind it
    if (!s.queue.empty()) {
        Strat orderStrat = hybridFly ? Strat::MinHold : st;
        return enqueue(s, down, shapedTarget(orderStrat), step, "order");
    }

    if (st == Strat::Off || win <= 0) {
        return enqueue(s, down, step, step, "raw");
    }

    switch (st) {
        case Strat::MinHold:
        case Strat::MinGap:
        case Strat::Grid: {
            int target = shapedTarget(st);
            if (target > step) {
                dbg(fmt::format("P{} {} {} shaped +{} steps", ci + 1, stratShort(st), act(down), target - step));
            }
            return enqueue(s, down, target, step, stratShort(st));
        }

        case Strat::Debounce: {
            if (down && step - s.lastReleaseApplied < win && !s.game) {
                s.suppress = true;
                dbg(fmt::format("P{} DEBOUNCE dropped press ({} steps after release)", ci + 1, step - s.lastReleaseApplied));
                return true;
            }
            return enqueue(s, down, step, step, "raw");
        }

        case Strat::Landing:
        case Strat::Hybrid:
        case Strat::Predictive:
            break;

        default:
            return enqueue(s, down, step, step, "raw");
    }

    // ---- Landing buffer (ground modes) -------------------------------------
    if (st == Strat::Landing || hybridGround) {
        if (down) {
            bool early = !p->m_isOnGround && ringsTouching(p) == 0;
            if (early) {
                s.landPending = true;
                s.landStart = step;
                dbg(fmt::format("P{} early press remembered [{}] window {} steps", ci + 1, gmName(gm), win));
                return enqueue(s, down, step, step, "raw");
            }
            s.landPending = false;
        }
        if (st == Strat::Landing) return enqueue(s, down, step, step, "raw");
    }

    // ---- Predictive (trajectory simulation) -----------------------------------
    bool isWave = p->m_isDart || (p2 && p2->m_isDart);
    bool spamApplies = c.spamGuard && (isWave || c.spamGuardAll);
    if (spamApplies && s.streak >= c.spamStreak) {
        dbg(fmt::format("P{} spam guard bypass (streak {})", ci + 1, s.streak));
        if (hybridFly && !down) return enqueue(s, down, shapedTarget(Strat::MinHold), step, "MINHOLD");
        return enqueue(s, down, step, step, "spam");
    }

    // Only wave and robot change trajectory on RELEASE
    bool releaseAffects = p->m_isDart || p->m_isRobot || (p2 && (p2->m_isDart || p2->m_isRobot));
    if (!down && (!releaseAffects || !c.predReleases)) {
        if (hybridFly) return enqueue(s, down, shapedTarget(Strat::MinHold), step, "MINHOLD");
        return enqueue(s, down, step, step, "raw");
    }

    auto ev = predictor().evaluate(pl, p, p2, down, win, stepsPerTick(), c.lethalFilter);

    if (ev.drop) {
        s.suppress = true;
        reportSave("LETHAL-FILTER", ci, gm, down, 0,
                   fmt::format("(input ignored: now {} / none {}+)", ev.frame0, ev.horizon));
        return true;
    }

    int target = step + ev.delay;
    if (hybridFly && !down) target = std::max(target, shapedTarget(Strat::MinHold));

    if (ev.delay > 0) {
        reportSave("PRED", ci, gm, down, ev.delay,
                   fmt::format("(Frame0: {} -> Delayed: {}) [Limit: {} steps]", ev.frame0, ev.best, win));
    }
    return enqueue(s, down, target, step, ev.delay > 0 ? "PRED" : "raw");
}

// ---------------------------------------------------------------------------
//  Physics step
// ---------------------------------------------------------------------------

void Engine::beginStep(GJBaseGameLayer* layer) {
    auto pl = PlayLayer::get();
    if (!pl || static_cast<GJBaseGameLayer*>(pl) != layer) return;

    m_step++;
    m_inStep = true;

    bool active = isActive(pl);
    bool twoP = twoPlayer(pl);

    for (int ci = 0; ci < 2; ci++) {
        auto& s = m_ctl[ci];
        bool tracked = ci == 0 || (twoP && cfg().twoPlayer);
        if (!tracked) continue;

        if (!active) {
            if (!s.queue.empty() || s.suppress || s.landPending) {
                clearController(s, true);
                if (s.game != s.physical) inject(layer, ci, s.physical, "flush");
            }
            continue;
        }

        // Dead: drop everything, the reset restores a clean state
        if (auto p = primary(pl, ci); p && p->m_isDead) {
            if (!s.queue.empty() || s.landPending || s.suppress) clearController(s, true);
            continue;
        }

        // Gamemode changed (portal) while something was queued: collapse it,
        // the sync below restores the real button state.
        if (auto p = primary(pl, ci)) {
            int mi = static_cast<int>(modeOf(p));
            if (s.lastModeIdx != -1 && mi != s.lastModeIdx && !s.queue.empty()) {
                dbg(fmt::format("P{} gamemode changed, dropping {} queued event(s)", ci + 1, s.queue.size()));
                s.queue.clear();
            }
            if (mi != s.lastModeIdx) m_dirty = true;
            s.lastModeIdx = mi;
        }

        if (!s.queue.empty()) {
            // At most one transition per physics step
            if (s.queue.front().target <= m_step) {
                auto ev = s.queue.front();
                s.queue.pop_front();
                inject(layer, ci, ev.down, ev.why);
            }
        } else if (!s.suppress && s.game != s.physical) {
            // Failsafe: with nothing queued, the game must match the real button
            inject(layer, ci, s.physical, "sync");
        }
    }
}

void Engine::endStep(GJBaseGameLayer* layer) {
    auto pl = PlayLayer::get();
    if (!pl || static_cast<GJBaseGameLayer*>(pl) != layer) return;
    m_inStep = false;
    if (!isActive(pl)) return;

    auto const& c = cfg();
    for (int ci = 0; ci < 2; ci++) {
        auto& s = m_ctl[ci];
        if (!s.landPending) continue;

        auto p = primary(pl, ci);
        if (!p || p->m_isDead) { s.landPending = false; continue; }

        GM gm = modeOf(p);
        Strat st = effectiveStrat(pl, ci, gm);
        int win = ticksToSteps(windowTicksFor(gm));
        int waited = m_step - s.landStart;
        bool landingOn = st == Strat::Landing || (st == Strat::Hybrid && isGround(gm));

        if (!landingOn || win <= 0 || waited > win) {
            dbg(fmt::format("P{} early press expired (waited {} / {} steps) [{}]", ci + 1, waited, win, gmName(gm)));
            s.landPending = false;
            continue;
        }

        bool onRing = c.landingOrbs && ringsTouching(p) > 0;
        bool onGround = p->m_isOnGround;
        if (!onGround && !onRing) continue;

        s.landPending = false;
        if (s.physical) {
            // Still holding: the game already handles it (holding = auto jump)
            dbg(fmt::format("P{} reached {} still holding, left to the game", ci + 1, onRing ? "orb" : "ground"));
            continue;
        }
        if (!s.queue.empty()) continue;

        s.queue.push_back({true, m_step + 1, "LAND"});
        s.queue.push_back({false, m_step + 2, "LAND"});
        reportSave("LAND", ci, gm, true, waited, onRing ? "ORB" : "GROUND");
    }
}

} // namespace pbuf
