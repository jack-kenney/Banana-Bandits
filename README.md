# Banana Bandits

Banana Bandits is a Nintendo 64 homebrew arena brawler built with [libdragon](https://github.com/DragonMinded/libdragon) and [tiny3d](https://github.com/HailToDodongo/tiny3d). It was my first game design project, and it became a hands-on way to learn 3D graphics programming, asset pipelines, real-time gameplay code, and the shape of a small game engine.

Click the image below for a brief YouTube video of the gameplay.

[![Preview clip](https://img.youtube.com/vi/mCl8ZxVs_-0/0.jpg)](https://youtu.be/mCl8ZxVs_-0)

## Lessons Learned

- **A game loop is an engine decision, not just a `while` loop.** Early versions updated physics and rendering together every frame. That worked until frame timing started affecting movement, collision, attack windows, and input feel. The project now uses a fixed gameplay step so simulation is more predictable.
- **Rendering and simulation want different rhythms.** Player state, collision, hit detection, animation, and draw-list updates all started close together. Separating those responsibilities is one of the biggest architecture lessons from the project.
- **Asset pipelines matter as much as gameplay code.** Models, sprites, audio, compressed filesystem assets, and generated ROM data all need reproducible build rules. A stale sprite format was enough to crash at runtime, which was a very memorable reminder.
- **3D character work gets complex quickly.** Skinned models, animation blending, weapon attachment, hitboxes, and world-space transforms all interact. Even a small brawler needs careful state management.
- **Debug tooling saves projects.** USB logging, performance overlays, collision visualization, and profiler counters made it possible to understand what the game was doing on real N64-oriented constraints.

## Project Overview

The game is a local multiplayer arena prototype where banana characters move around a 3D map, pick up pipe weapons, attack each other, dodge, jump, and lose HP until one player remains.

Current features include:

- Main menu, pause menu, reset, and return-to-menu flow.
- Four-player entity setup with spawn points read from the map model when available.
- Skinned banana player model with idle, dodge, and punch animations.
- Weapon pickup, drop, attachment to the player's hand bone, attack hitboxes, and hit feedback sounds.
- AABB-based player, weapon, and map collision handling.
- HP bars, sprite assets, sound effects, and XM music playback.
- Optional debug/performance overlay and collision visualization.

## Tech Stack

- **Target:** Nintendo 64 homebrew ROM
- **Language:** C
- **Core SDK:** libdragon
- **3D renderer/model pipeline:** tiny3d
- **Assets:** GLB models, PNG sprites, WAV sound effects, XM music
- **Build system:** Makefile using libdragon/tiny3d toolchain helpers

The repository uses `libdragon` and `tiny3d` as submodules.

## Repository Layout

```text
assets/       Source art, models, audio, and textures
filesystem/   Converted runtime assets packaged into the ROM filesystem
src/          Game code
libdragon/    libdragon submodule
tiny3d/       tiny3d submodule
build/        Generated build output, ignored by git
```

## Building

This project expects a working libdragon/tiny3d N64 toolchain environment. After cloning, initialize submodules:

```sh
git submodule update --init --recursive
```

Then build the ROM:

```sh
make all
```

The build produces `BananaBandits.z64`.

To clean generated output:

```sh
make clean
```

## Regenerating Sprites

If libdragon reports an invalid sprite version at runtime, the checked-in sprite assets may need to be regenerated with the currently installed `mksprite` tool:

```sh
make -B $(find assets -maxdepth 1 -name '*.png' | sed 's#^assets/#filesystem/#; s#\.png$#.sprite#')
make all
```

## Controls

Gameplay controls are still prototype-oriented, but the current mappings are:

- Analog stick: move
- A: jump
- B: attack
- R: dodge
- Z: sprint/debug speed boost behavior
- C-Up: drop weapon
- C-Left / C-Right: adjust camera height
- C-Down: toggle debug collision/performance overlay
- Start: pause/resume

## Notes

This is a learning project and a snapshot of a first pass at game and engine design. Some systems are intentionally rough: gameplay simulation, animation, rendering prep, and collision still have places where they could be separated further. That roughness is part of the history of the project, and the code is useful as a record of learning how a real-time 3D game comes together from the bottom up.