#include "Config.hpp"

#if defined(_WIN32)
#include <Windows.h>
#endif
#include <cstdio>

using namespace geode::prelude;

namespace pbuf {

static Config s_cfg;
Config& cfg() { return s_cfg; }

// ---------------------------------------------------------------------------
//  Names
// ---------------------------------------------------------------------------

static constexpr char const* GM_NAMES[GM_COUNT] = {
    "cube", "ship", "ball", "ufo", "wave", "robot", "spider", "swing"
};

static constexpr char const* STRAT_NAMES[] = {
    "Off", "Predictive", "Landing Buffer", "Hybrid", "Min Hold", "Min Gap", "Grid Snap", "Debounce"
};
static constexpr char const* STRAT_SHORT[] = {
    "OFF", "PRED", "LAND", "HYBRID", "MINHOLD", "MINGAP", "GRID", "DEBOUNCE"
};

static constexpr char const* PRESET_NAMES[] = {
    "Custom",
    "Original v1",
    "Classic Input Buffer",
    "Light",
    "Balanced",
    "Aggressive",
    "Consistency",
};
static constexpr int PRESET_COUNT = sizeof(PRESET_NAMES) / sizeof(PRESET_NAMES[0]);

char const* gmName(GM gm) { return GM_NAMES[static_cast<int>(gm)]; }
char const* stratName(Strat s) { return STRAT_NAMES[static_cast<int>(s)]; }
char const* stratShort(Strat s) { return STRAT_SHORT[static_cast<int>(s)]; }
char const* presetName(int idx) { return PRESET_NAMES[std::clamp(idx, 0, PRESET_COUNT - 1)]; }
int presetCount() { return PRESET_COUNT; }

static bool isGroundGM(GM gm) {
    return gm == GM::Cube || gm == GM::Ball || gm == GM::Robot || gm == GM::Spider;
}

bool stratValidFor(GM gm, Strat s) {
    switch (s) {
        case Strat::Off:
        case Strat::Predictive:
        case Strat::Hybrid:
        case Strat::Grid:
        case Strat::Debounce:
            return true;
        case Strat::Landing:
            return isGroundGM(gm);
        case Strat::MinHold:
            return !isGroundGM(gm) || gm == GM::Robot;
        case Strat::MinGap:
            return !isGroundGM(gm);
        default:
            return false;
    }
}

std::string stratSettingKey(GM gm) { return std::string(gmName(gm)) + "-strategy"; }
std::string windowSettingKey(GM gm) { return std::string(gmName(gm)) + "-window"; }

static Strat parseStrat(std::string const& s, Strat def) {
    for (int i = 0; i < static_cast<int>(Strat::Count); i++) {
        if (s == STRAT_NAMES[i]) return static_cast<Strat>(i);
    }
    return def;
}

// ---------------------------------------------------------------------------
//  Presets (they only replace the strategy of each gamemode;
//  windows always come from the window settings / main window)
//  Order: cube, ship, ball, ufo, wave, robot, spider, swing
// ---------------------------------------------------------------------------

using S = Strat;
static constexpr Strat PRESETS[PRESET_COUNT][GM_COUNT] = {
    // Custom (unused)
    {S::Off, S::Off, S::Off, S::Off, S::Off, S::Off, S::Off, S::Off},
    // Original v1: trajectory buffer everywhere except ship
    {S::Predictive, S::Off, S::Predictive, S::Predictive, S::Predictive, S::Predictive, S::Predictive, S::Predictive},
    // Classic Input Buffer (Lyophi's MacOS mod defaults)
    {S::Landing, S::MinHold, S::Landing, S::MinHold, S::Grid, S::Landing, S::Landing, S::MinHold},
    // Light: only remembers early clicks
    {S::Landing, S::Off, S::Landing, S::Off, S::Off, S::Landing, S::Landing, S::Off},
    // Balanced
    {S::Hybrid, S::MinHold, S::Hybrid, S::Predictive, S::Predictive, S::Hybrid, S::Hybrid, S::Predictive},
    // Aggressive
    {S::Hybrid, S::Hybrid, S::Hybrid, S::Hybrid, S::Hybrid, S::Hybrid, S::Hybrid, S::Hybrid},
    // Consistency
    {S::Landing, S::MinHold, S::Landing, S::MinHold, S::Grid, S::MinHold, S::Landing, S::MinHold},
};

Strat stratFor(GM gm) {
    auto const& c = s_cfg;
    int i = static_cast<int>(gm);
    if (c.preset > 0 && c.preset < PRESET_COUNT) return PRESETS[c.preset][i];
    return c.modes[i].strat;
}

int windowTicksFor(GM gm) {
    auto const& c = s_cfg;
    int w = c.modes[static_cast<int>(gm)].window;
    if (w < 0) {
        w = c.mainWindow;
        if (gm == GM::Wave && w > 0 && c.waveCap > 0) w = std::min(w, c.waveCap);
    }
    return std::max(0, w);
}

// ---------------------------------------------------------------------------
//  Loading
// ---------------------------------------------------------------------------

static Strat defaultStrat(GM gm) {
    switch (gm) {
        case GM::Ship: return Strat::Off;
        case GM::Ufo:
        case GM::Wave:
        case GM::Swing: return Strat::Predictive;
        default: return Strat::Hybrid;
    }
}

void loadConfig() {
    auto m = Mod::get();
    auto& c = s_cfg;

    auto getB = [&](char const* k) { return m->getSettingValue<bool>(k); };
    auto getI = [&](char const* k) { return static_cast<int>(m->getSettingValue<int64_t>(k)); };
    auto getF = [&](char const* k) { return static_cast<float>(m->getSettingValue<double>(k)); };
    auto getS = [&](char const* k) { return m->getSettingValue<std::string>(k); };

    c.enabled = getB("enabled");
    {
        auto p = getS("preset");
        c.preset = 0;
        for (int i = 0; i < PRESET_COUNT; i++) if (p == PRESET_NAMES[i]) c.preset = i;
    }
    c.mainWindow = getI("main-window");
    c.waveCap = getI("wave-cap");
    c.tpsOverride = getI("tps");
    c.practiceOnly = getB("practice-only");
    c.platformer = getB("platformer");
    c.twoPlayer = getB("two-player");

    for (int i = 0; i < GM_COUNT; i++) {
        auto gm = static_cast<GM>(i);
        Strat s = parseStrat(getS(stratSettingKey(gm).c_str()), defaultStrat(gm));
        if (!stratValidFor(gm, s)) s = defaultStrat(gm);
        c.modes[i].strat = s;
        c.modes[i].window = getI(windowSettingKey(gm).c_str());
    }

    c.predReleases = getB("predict-releases");
    c.horizonGround = getI("horizon-ground");
    c.horizonWave = getI("horizon-wave");
    c.horizonMiniWave = getI("horizon-mini-wave");
    {
        auto s = getS("strictness");
        c.strictness = s == "Strict" ? 1.5f : (s == "Loose" ? 0.6f : 1.0f);
    }
    c.spamGuard = getB("spam-guard");
    c.spamGuardAll = getB("spam-guard-all");
    c.spamInterval = getI("spam-interval");
    c.spamStreak = getI("spam-streak");
    c.lethalFilter = getB("lethal-filter");

    c.landingOrbs = getB("landing-orbs");

    c.overlay = getB("overlay");
    {
        auto s = getS("overlay-position");
        c.overlayPos = s == "Top Left" ? 0 : s == "Top Right" ? 1 : s == "Bottom Right" ? 3 : 2;
    }
    c.overlayScale = getF("overlay-scale");
    c.overlayOpacity = getI("overlay-opacity");
    c.overlayFlash = getB("overlay-flash");
    c.notifyHotkeys = getB("notify-hotkeys");

    c.console = getB("console");
    c.debug = getB("debug");
    c.logSaves = getB("log-saves");

    if (c.console) ensureConsole();
}

// ---------------------------------------------------------------------------
//  Logging
// ---------------------------------------------------------------------------

static bool s_consoleReady = false;

void ensureConsole() {
    if (s_consoleReady) return;
    s_consoleReady = true;
    if (!GetConsoleWindow()) AllocConsole();
    FILE* f = nullptr;
    freopen_s(&f, "CONOUT$", "w", stdout);
    SetConsoleTitleA("Predictive Input Buffer v2 - Live Console");
    std::printf("============================================================\n");
    std::printf(" [InputBuffer] v2.0.0 live console\n");
    std::printf("============================================================\n");
}

void out(std::string const& line) {
    if (!s_cfg.logSaves) return;
    log::info("{}", line);
    if (s_consoleReady && s_cfg.console) {
        std::printf("[InputBuffer] %s\n", line.c_str());
        std::fflush(stdout);
    }
}

void dbg(std::string const& line) {
    if (!s_cfg.debug) return;
    log::info("[debug] {}", line);
    if (s_consoleReady && s_cfg.console) {
        std::printf("[InputBuffer:dbg] %s\n", line.c_str());
        std::fflush(stdout);
    }
}

} // namespace pbuf
