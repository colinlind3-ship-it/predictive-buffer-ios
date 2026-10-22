# Predictive Input Buffer

Makes tight timings easier by fixing clicks that are a tiny bit **too early**, **too short** or that **would kill you**.

## Strategies (one per gamemode)

| Strategy | What it does |
|---|---|
| **Predictive** | Simulates your trajectory. If an input would kill you but the same input a few ticks later survives, it is delayed to the earliest safe tick. |
| **Landing Buffer** | A click made slightly before landing or before an orb is remembered and fired the moment you touch the ground / orb. |
| **Hybrid** | Ground modes: Landing Buffer + Predictive. Flying modes: Predictive + Min Hold. |
| **Min Hold** | A very quick tap is stretched to a minimum length. |
| **Min Gap** | After a release, the next press waits a minimum time. |
| **Grid Snap** | Every press and release snaps to the next line of a fixed timing grid. |
| **Debounce** | A press right after a release is ignored (double-click / worn switch fix). |

Plus: presets, auto TPS detection, spam guard, optional lethal input filter, 2-player support, on-screen overlay and rebindable keybinds.

## Keybinds (in a level)

- **F6** cycle the main window (2 / 4 / 8 / 16 ticks / Off)
- **F7** toggle the buffer
- **F8** toggle the overlay
- **F9** debug logging
- **F10** cycle the strategy of the current gamemode

The **BUF** button on the pause menu opens the settings.

## Note

This is an input assist. Use it for practice and fun, not for leaderboard or submitted runs.

Credits: v1 trajectory buffer by lilWC. Landing buffer, Min Hold / Min Gap / Grid ideas from Lyophi's *Input Buffer* (MacOS).
