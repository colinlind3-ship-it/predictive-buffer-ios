#include "Predictor.hpp"
#include "Config.hpp"

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace pbuf {

static Predictor s_predictor;
Predictor& predictor() { return s_predictor; }

PlayerObject* Predictor::createSimPlayer(PlayLayer* pl) {
    PlayerObject* player = PlayerObject::create(1, 1, pl, pl, true);
    if (!player) return nullptr;
    player->retain();
    player->setPosition({0, 105});
    player->setVisible(false);
    pl->m_objectLayer->addChild(player);
    return player;
}

void Predictor::init(PlayLayer* pl) {
    quit();
    if (!pl) return;
    m_sim1 = createSimPlayer(pl);
    m_sim2 = createSimPlayer(pl);
    m_simulating = false;
    m_simDead = false;
}

void Predictor::quit() {
    if (m_sim1) { m_sim1->release(); m_sim1 = nullptr; }
    if (m_sim2) { m_sim2->release(); m_sim2 = nullptr; }
    m_simulating = false;
}

bool Predictor::onDestroyPlayer(PlayerObject* player) {
    if (m_simulating && (player == m_sim1 || player == m_sim2)) {
        m_simDead = true;
        return true;
    }
    return false;
}

void Predictor::onRealPlayerUpdate(float dt) {
    auto pl = PlayLayer::get();
    float warp = pl ? pl->m_gameState.m_timeWarp : 1.f;
    m_frameDt = warp > 0.f ? dt / warp : dt;

    // TPS auto-detection: only trust frames without a time-warp trigger active
    if (dt > 0.0005f && dt < 2.f && std::fabs(warp - 1.f) < 0.001f) {
        int tps = static_cast<int>(std::lround(60.0 / dt));
        if (tps >= 30 && tps <= 20000) m_detectedTps = tps;
    }
}

float Predictor::stepDt() const {
    if (m_frameDt > 0.001f && m_frameDt < 2.0f) return m_frameDt;
    return 0.25f;
}

void Predictor::resetCollisionLog(PlayerObject* p) {
    p->m_collisionLogTop->removeAllObjects();
    p->m_collisionLogBottom->removeAllObjects();
    p->m_collisionLogLeft->removeAllObjects();
    p->m_collisionLogRight->removeAllObjects();
    p->m_lastCollisionLeft = -1;
    p->m_lastCollisionRight = -1;
    p->m_lastCollisionBottom = -1;
    p->m_lastCollisionTop = -1;
}

// Same attribute + hitbox synchronisation as Show Trajectory (v1 behaviour)
void Predictor::syncSimPlayer(PlayerObject* sim, PlayerObject* real) {
    sim->setVisible(false);
    sim->copyAttributes(real);
    sim->setScale(real->getScale());
    sim->setContentSize(real->getContentSize());
    sim->m_width = real->m_width;
    sim->m_height = real->m_height;
    sim->m_gravityMod = real->m_gravityMod;
    sim->m_isOnGround = real->m_isOnGround;
    sim->m_isUpsideDown = real->m_isUpsideDown;
    sim->m_vehicleSize = real->m_vehicleSize;
    sim->m_playerSpeed = real->m_playerSpeed;
}

void Predictor::apply(PlayerObject* sim, bool push) {
    if (!sim) return;
    if (push) sim->pushButton(PlayerButton::Jump);
    else sim->releaseButton(PlayerButton::Jump);
}

Predictor::SimResult Predictor::survival(PlayLayer* pl, PlayerObject* a, PlayerObject* b,
                                         int delayFrames, bool push, int maxTicks, float k) {
    SimResult res;
    if (!pl || !m_sim1 || !a) { res.survivedAfter = maxTicks; return res; }

    bool dual = b && m_sim2;
    m_simulating = true;
    m_simDead = false;

    syncSimPlayer(m_sim1, a);
    if (dual) syncSimPlayer(m_sim2, b);

    // Before the input lands, keep the pre-input button state
    if (delayFrames > 0) {
        apply(m_sim1, !push);
        if (dual) apply(m_sim2, !push);
    }

    float dt = stepDt();
    bool continuous = a->m_isDart || a->m_isBird || a->m_isSwing ||
                      (b && (b->m_isDart || b->m_isBird || b->m_isSwing));
    int airborneNeed = std::max(1, static_cast<int>(std::lround(15 * k)));
    int landedNeed = std::max(1, static_cast<int>(std::lround(10 * k)));
    int airborne = 0;
    int landed = 0;

    for (int i = 0; i < delayFrames + maxTicks; i++) {
        if (i == delayFrames) {
            apply(m_sim1, push);
            if (dual) apply(m_sim2, push);
        }

        m_sim1->update(dt);
        m_sim1->updateRotation(dt);
        if (dual) {
            m_sim2->update(dt);
            m_sim2->updateRotation(dt);
        }
        if (m_simDead) break;

        resetCollisionLog(m_sim1);
        pl->checkCollisions(m_sim1, dt, false);
        if (m_simDead) break;

        if (dual) {
            resetCollisionLog(m_sim2);
            pl->checkCollisions(m_sim2, dt, false);
            if (m_simDead) break;
        }

        res.total++;
        if (i >= delayFrames) {
            res.survivedAfter++;
            if (!continuous) {
                bool g1 = m_sim1->m_isOnGround;
                bool g2 = dual ? m_sim2->m_isOnGround : true;
                if (!g1 || !g2) {
                    airborne++;
                    landed = 0;
                } else if (airborne >= airborneNeed) {
                    landed++;
                    if (landed >= landedNeed) {
                        // Landed safely after the jump: consider it a full survival
                        res.survivedAfter = maxTicks;
                        res.total = delayFrames + maxTicks;
                        break;
                    }
                }
            }
        }
    }

    m_simulating = false;
    return res;
}

Predictor::Eval Predictor::evaluate(PlayLayer* pl, PlayerObject* a, PlayerObject* b, bool push,
                                    int maxDelay, float k, bool allowDrop) {
    Eval ev;
    auto const& c = cfg();

    bool isWave = a->m_isDart || (b && b->m_isDart);
    bool isMiniWave = (a->m_isDart && a->m_vehicleSize < 1.0f) ||
                      (b && b->m_isDart && b->m_vehicleSize < 1.0f);

    int horizonTicks = isMiniWave ? c.horizonMiniWave : (isWave ? c.horizonWave : c.horizonGround);
    int horizon = std::clamp(static_cast<int>(std::lround(horizonTicks * k)), 8, 900);
    ev.horizon = horizon;

    // 1. Immediate input across the whole horizon
    int f0 = survival(pl, a, b, 0, push, horizon, k).survivedAfter;
    ev.frame0 = f0;
    ev.best = f0;
    if (f0 >= horizon) return ev;

    // 2. Find the earliest delay that genuinely clears the obstacle
    float s = c.strictness;
    int bestDelay = 0;
    int bestTicks = f0;
    for (int delay = 1; delay <= maxDelay; delay++) {
        int ticks = survival(pl, a, b, delay, push, horizon, k).survivedAfter;
        if (isWave) {
            // In a flat corridor, delaying a wave turn naturally adds ~2*delay ticks
            // against the opposite wall: only buffer when it really clears something.
            int fullGain = static_cast<int>(std::lround((delay * 2 + 6 * k) * s));
            int minGain = static_cast<int>(std::lround((delay * 2 + 10 * k) * s));
            int step = std::max(1, static_cast<int>(std::lround(2 * k)));
            if (ticks >= horizon && (ticks - f0) >= fullGain) {
                bestDelay = delay;
                bestTicks = ticks;
                break;
            }
            if (ticks >= f0 + minGain && ticks > bestTicks + step) {
                bestDelay = delay;
                bestTicks = ticks;
                if (ticks >= horizon) break;
            }
        } else {
            if (ticks >= horizon) {
                bestDelay = delay;
                bestTicks = ticks;
                break;
            }
            int gain = std::max(1, static_cast<int>(std::lround(12 * k * s)));
            if (ticks > bestTicks + gain) {
                bestDelay = delay;
                bestTicks = ticks;
            }
        }
    }

    ev.delay = bestDelay;
    ev.best = bestTicks;

    // 3. Lethal input filter: no delay saves it, but NOT doing it survives the horizon
    if (allowDrop && bestDelay == 0) {
        auto none = survival(pl, a, b, horizon, push, 0, k);
        if (none.total >= horizon) {
            ev.drop = true;
        }
    }
    return ev;
}

} // namespace pbuf
