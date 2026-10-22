#pragma once

#include <Geode/Geode.hpp>
#include <string>

// ============================================================================
//  Predictive Input Buffer v2 - configuration
//  Every value here mirrors a setting in mod.json (or a preset override).
// ============================================================================

namespace pbuf {

enum class GM : int { Cube = 0, Ship, Ball, Ufo, Wave, Robot, Spider, Swing, Count };
constexpr int GM_COUNT = static_cast<int>(GM::Count);

enum class Strat : int {
    Off = 0,
    Predictive,   // trajectory simulation: delays a lethal input by up to N ticks
    Landing,      // early press memory: fires when you land / touch an orb
    Hybrid,       // ground: Landing + Predictive | flying: Predictive + Min Hold
    MinHold,      // a quick tap is stretched to at least N ticks
    MinGap,       // a press must wait N ticks after the last release
    Grid,         // presses and releases snap to the next N-tick grid line
    Debounce,     // a press within N ticks of the last release is ignored (anti-chatter)
    Count
};

struct ModeCfg {
    Strat strat = Strat::Off;
    int window = -1; // -1 = main window, 0 = off, >0 = ticks (1 tick = 1/240 s)
};

struct Config {
    // General
    bool enabled = true;
    int preset = 0;            // 0 = Custom
    int mainWindow = 2;        // ticks
    int waveCap = 8;           // cap for wave when it follows the main window
    int tpsOverride = 0;       // 0 = auto-detect
    bool practiceOnly = false;
    bool platformer = true;
    bool twoPlayer = true;

    ModeCfg modes[GM_COUNT];

    // Predictive tuning
    bool predReleases = true;
    int horizonGround = 160;   // ticks of lookahead
    int horizonWave = 65;
    int horizonMiniWave = 50;
    float strictness = 1.0f;   // multiplies the "real gain" thresholds
    bool spamGuard = true;
    bool spamGuardAll = false;
    int spamInterval = 8;
    int spamStreak = 3;
    bool lethalFilter = false;

    // Landing buffer
    bool landingOrbs = true;

    // Overlay / feedback
    bool overlay = true;
    int overlayPos = 2;        // 0 TL, 1 TR, 2 BL, 3 BR
    float overlayScale = 0.45f;
    int overlayOpacity = 200;
    bool overlayFlash = true;
    bool notifyHotkeys = true;

    // Debug
    bool console = false;
    bool debug = false;
    bool logSaves = true;
};

Config& cfg();
void loadConfig();

// Names
char const* gmName(GM gm);
char const* stratName(Strat s);
char const* stratShort(Strat s);
char const* presetName(int idx);
int presetCount();

// Which strategies make sense for a gamemode (used by the strategy hotkey)
bool stratValidFor(GM gm, Strat s);
// Key of the "<mode>-strategy" setting
std::string stratSettingKey(GM gm);
std::string windowSettingKey(GM gm);

// Effective strategy / window after presets
Strat stratFor(GM gm);
int windowTicksFor(GM gm); // 0 = off

// Logging helpers (Geode log + optional live console)
void ensureConsole();
void out(std::string const& line);       // always (if logSaves)
void dbg(std::string const& line);       // only with debug

} // namespace pbuf
