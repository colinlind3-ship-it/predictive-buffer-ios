# Predictive Input Buffer v2

Geode mod for **Geometry Dash 2.2081 (Windows)**, Geode **5.11.2**.

Fixes clicks that are a tiny bit too early, too short, or that would kill you, with a strategy you choose for each gamemode.
Doesn't matter if Click Between Frames is on.

> This is an input assist. Use it for practice and fun, not for leaderboard or submitted runs.

## Strategies

| Strategy | Available on | Behaviour | Window meaning |
|---|---|---|---|
| Off | all | raw input | - |
| Predictive | all | Simulates the trajectory (hidden players, v1 "Show Trajectory" logic). If the input dies and a delayed one survives, it is delayed to the earliest safe tick. Releases are predicted for wave and robot. | max delay |
| Landing Buffer | cube, ball, robot, spider | Early click (in the air, not on an orb) that you already released is fired when you land or touch an orb. | how early |
| Hybrid | all | ground: Landing + Predictive, flying: Predictive + Min Hold | both |
| Min Hold | ship, UFO, wave, swing, robot | quick taps are stretched to N ticks | min tap length |
| Min Gap | ship, UFO, wave, swing | a press waits N ticks after the last release | min gap |
| Grid Snap | all | presses / releases wait for the next N-tick grid line | grid size |
| Debounce | all | a press less than N ticks after a release is ignored | debounce time |

Defaults (Custom preset): cube / ball / robot / spider **Hybrid**, UFO / wave / swing **Predictive**, ship **Off**, main window **2 ticks** (v1 stage 1), wave cap **8**.

### Presets

A preset replaces the strategy of every gamemode (windows still come from the settings).

| Preset | cube | ship | ball | ufo | wave | robot | spider | swing |
|---|---|---|---|---|---|---|---|---|
| Original v1 | Pred | Off | Pred | Pred | Pred | Pred | Pred | Pred |
| Classic Input Buffer | Land | MinHold | Land | MinHold | Grid | Land | Land | MinHold |
| Light | Land | Off | Land | Off | Off | Land | Land | Off |
| Balanced | Hybrid | MinHold | Hybrid | Pred | Pred | Hybrid | Hybrid | Pred |
| Aggressive | Hybrid | Hybrid | Hybrid | Hybrid | Hybrid | Hybrid | Hybrid | Hybrid |
| Consistency | Land | MinHold | Land | MinHold | Grid | MinHold | Land | MinHold |

### Ticks

1 tick = 1/240 s (about 4 ms). 4 ticks = 1 frame at 60 FPS. Windows are converted to your real TPS (auto-detected, or set by hand with the **TPS** setting).

## Other options

- Spam guard (v1): fast spam bypasses the predictive buffer (wave only, or all modes). Interval and streak are configurable.
- Lethal input filter (off by default): an input that kills at every delay, while not doing it survives the lookahead, is ignored.
- Lookahead per mode (ground 160, wave 65, mini wave 50 ticks like v1) and strictness (Strict / Normal / Loose).
- Practice-only, platformer (jump only), 2-player (player 2 buffered independently).
- Overlay: gamemode, strategy, window, TPS, inputs saved this attempt / session, flashes cyan on every save.
- Logs: `*** SAVED +N TICKS ... ***` in the Geode log, optional v1 live console window, debug logging.

## Keybinds (only in a level, rebindable)

| Default | Action |
|---|---|
| F6 | cycle main window 2 / 4 / 8 / 16 / Off |
| F7 | toggle buffer |
| F8 | toggle overlay |
| F9 | debug logging |
| F10 | cycle the strategy of the current gamemode (Custom preset) |
| (none) | cycle preset |

## How it works

- `GJBaseGameLayer::handleButton` (jump button only) -> `Engine::onInput` decides: pass now, queue for a later step, remember until landing, or ignore.
- `GJBaseGameLayer::processCommands` (one call per physics step) -> queued events are injected through the normal `handleButton` chain at the start of their target step, at most one transition per step. After the step, the landing buffer checks ground / orb contact.
- Failsafe: when nothing is queued, the game's button state is forced back to the real one.
- The predictor (`Predictor.cpp`) is the v1 simulator: two hidden `PlayerObject`s, side effects (triggers, rings, streaks, deaths...) muted while simulating.

## Building

Geode SDK 5.11.2 + Geode CLI, CMake, Visual Studio 2022+ (Desktop C++) or clang-cl.

```
geode build
```

## Credits

- v1 trajectory buffer: lilWC
- Landing buffer, Min Hold / Min Gap / Grid ideas: Lyophi, *Input Buffer* (MacOS, MIT)
