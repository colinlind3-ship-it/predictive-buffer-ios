# Changelog

## v2.0.0
- New engine: every jump input goes through a per-player event queue, injected at the start of its target physics step (at most one transition per step, strict chronological order).
- **7 strategies**, chosen per gamemode: Predictive, Landing Buffer, Hybrid, Min Hold, Min Gap, Grid Snap, Debounce.
- **Presets**: Original v1, Classic Input Buffer, Light, Balanced, Aggressive, Consistency.
- Per-gamemode windows (`-1` = main window, `0` = off), wave cap like v1 (16 / 8).
- **Auto TPS detection** from the physics step (TPS bypass supported), manual override available.
- Predictive tuning: lookahead per mode, strictness, release prediction toggle, configurable spam guard (wave only or all modes).
- **Lethal input filter** (optional): ignores an input that kills you at every delay when not doing it survives.
- Landing buffer also fires on orbs; held clicks are left to the game.
- 2-player levels: player 2 is buffered independently. 1-player dual keeps the v1 mirroring.
- Platformer support (jump button only) and practice-only option.
- On-screen overlay (mode, strategy, window, TPS, saved inputs) that flashes on every saved input.
- Native Geode keybinds (rebindable, only while the game is focused) instead of a global F6 poll.
- Pause menu **BUF** button opening the settings.
- The v1 live console is now optional (setting), saved inputs always go to the Geode log.
- Simulation: collision log reset now also clears the last-collision markers.
- CMake: C++23 (required by Geode 5), Windows only.

## v1.0.0
- Trajectory-based 1-2 frame press and wave release input buffer (lilWC).
