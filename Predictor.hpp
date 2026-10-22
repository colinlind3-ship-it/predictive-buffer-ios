#pragma once

#include <Geode/Geode.hpp>

// ============================================================================
//  Trajectory predictor (port of the v1 "Show Trajectory" style simulator)
//  Two hidden PlayerObjects are synced with the real players and stepped
//  through the level to see whether an input made now, or N ticks later,
//  survives.
// ============================================================================

namespace pbuf {

class Predictor {
public:
    struct Eval {
        int delay = 0;      // steps to delay the input (0 = fire now)
        int frame0 = 0;     // survived steps if fired now
        int best = 0;       // survived steps with the chosen delay
        int horizon = 0;    // lookahead used
        bool drop = false;  // lethal filter: the input should be ignored entirely
    };

    void init(PlayLayer* pl);
    void quit();

    bool isSimulating() const { return m_simulating; }

    // Returns true when the death belongs to a simulated player (swallow it)
    bool onDestroyPlayer(PlayerObject* player);

    void onRealPlayerUpdate(float dt);
    float stepDt() const;          // physics dt passed to PlayerObject::update
    int detectedTps() const { return m_detectedTps; }

    // a = main real player, b = optional second real player simulated together
    Eval evaluate(PlayLayer* pl, PlayerObject* a, PlayerObject* b, bool push,
                  int maxDelaySteps, float stepsPerTick, bool allowDrop);

private:
    struct SimResult {
        int survivedAfter = 0; // steps survived after the input was applied
        int total = 0;         // total steps simulated before death / end
    };

    PlayerObject* createSimPlayer(PlayLayer* pl);
    void syncSimPlayer(PlayerObject* sim, PlayerObject* real);
    void resetCollisionLog(PlayerObject* p);
    void apply(PlayerObject* sim, bool push);
    SimResult survival(PlayLayer* pl, PlayerObject* a, PlayerObject* b,
                       int delayFrames, bool push, int maxTicks, float k);

    PlayerObject* m_sim1 = nullptr;
    PlayerObject* m_sim2 = nullptr;
    bool m_simulating = false;
    bool m_simDead = false;
    float m_frameDt = 0.25f;
    int m_detectedTps = 240;
};

Predictor& predictor();

} // namespace pbuf
