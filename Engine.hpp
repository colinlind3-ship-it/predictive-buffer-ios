#pragma once

#include "Config.hpp"
#include <deque>
#include <string>

// ============================================================================
//  Input buffer engine
//  Every jump-button input goes through onInput(). Depending on the strategy
//  of the current gamemode it is either passed straight to the game, queued
//  for a later physics step, remembered until landing, or ignored.
//  Queued events are injected at the start of their target step (preStep).
// ============================================================================

namespace pbuf {

struct QueuedEvent {
    bool down = false;
    int target = 0;
    char const* why = "";
};

struct Controller {
    bool physical = false;      // what the player is really holding
    bool game = false;          // what the game currently has
    std::deque<QueuedEvent> queue;
    bool suppress = false;      // swallow inputs until physical == game again

    bool landPending = false;
    int landStart = 0;

    int lastPressApplied = -1000000;
    int lastReleaseApplied = -1000000;
    int lastInjectStep = -1000000;
    int lastInputStep = -1000000;
    int streak = 0;
    int lastModeIdx = -1;
};

struct Stats {
    int savedAttempt = 0;
    int savedSession = 0;
    int lastSavedTicksX10 = 0;  // last saved delay, in tenths of a tick
    std::string lastKind;
};

class Engine {
public:
    void onLevelStart();
    void onReset();
    void onDeath();

    // Called from the GJBaseGameLayer::handleButton hook.
    // Returns true when the input must be swallowed.
    bool onInput(GJBaseGameLayer* layer, bool down, bool isPlayer1);

    void beginStep(GJBaseGameLayer* layer);   // before the physics step
    void endStep(GJBaseGameLayer* layer);     // after the physics step

    bool isInjecting() const { return m_injecting; }
    bool isActive(PlayLayer* pl) const;

    int tps() const;
    float stepsPerTick() const;
    int ticksToSteps(int ticks) const;

    // Information for the overlay
    GM currentMode(PlayLayer* pl) const;
    Stats const& stats() const { return m_stats; }
    bool consumeDirty() { bool d = m_dirty; m_dirty = false; return d; }
    bool consumeFlash() { bool f = m_flash; m_flash = false; return f; }
    void markDirty() { m_dirty = true; }

private:
    PlayerObject* primary(PlayLayer* pl, int c) const;
    PlayerObject* secondary(PlayLayer* pl, int c) const;
    bool twoPlayer(PlayLayer* pl) const;
    bool dual(PlayLayer* pl) const;
    Strat effectiveStrat(PlayLayer* pl, int c, GM gm) const;

    void clearController(Controller& s, bool keepGame);
    void inject(GJBaseGameLayer* layer, int c, bool down, char const* why);
    void applied(Controller& s, bool down, int step);
    bool enqueue(Controller& s, bool down, int target, int step, char const* why);
    int pendingPressStep(Controller const& s) const;
    int pendingReleaseStep(Controller const& s) const;
    void reportSave(char const* kind, int c, GM gm, bool down, int delaySteps, std::string const& extra);

    int inputStep() const { return m_inStep ? m_step : m_step + 1; }

    Controller m_ctl[2];
    Stats m_stats;
    int m_step = 0;
    bool m_inStep = false;
    bool m_injecting = false;
    bool m_dirty = true;
    bool m_flash = false;
};

Engine& engine();

} // namespace pbuf
