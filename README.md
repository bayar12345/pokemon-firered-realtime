# Pokémon FireRed Real-Time

A FireRed ROM hack where battles happen in real time, right on the map. No turn menus: you walk your Pokémon around, aim, dodge, and every move has its own cooldown. Pokémon use Mystery Dungeon sprites with 8-direction movement, every move has a Mystery Dungeon attack animation, and Fly zooms out to a painted map of Kanto with your bird flying across it.

This repo has **the full source code** of the hack, and **a patch** you can put on your own FireRed ROM to just play it. Free to use.

Made by me, with most of the code written by AI: **Claude Opus 5.5** (Anthropic) and **ChatGPT SOL 6.2** (OpenAI), the two of them combined.

---

## Just want to play?
1. Get a clean **Pokémon FireRed (USA) v1.0** ROM that you dumped yourself.
   SHA-1: `41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc`
2. Download the patch: [patch/FireRed-Realtime-v3.bps](patch/FireRed-Realtime-v3.bps) (click it, then the download button).
3. Patch it with [Rom Patcher JS](https://www.marcrobledo.com/RomPatcher.js/) or Floating IPS.
4. Play in mGBA (or any GBA emulator) at normal speed.

## What's in it
- **Real-time battles everywhere.** Wild Pokémon, trainers, gyms, Elite Four. Gyms, the Elite Four and the Champion have their own arenas with cover, hazards and breakable trees. The legendaries (Articuno, Zapdos, Moltres, Mewtwo, Lugia, Ho-Oh, Deoxys) get boss arenas with a second phase at half HP.
- **Pokémon Mystery Dungeon sprites.** Pokémon walk, attack, get hit and sleep with PMD animations, in 8 directions.
- **PMD move animations.** Every FireRed move has an attack effect from Mystery Dungeon.
- **Follower.** Your lead Pokémon walks behind you.
- **New Fly.** Pick a spot on the normal Fly map, then the camera zooms out to a painted map of Kanto and the Sevii Islands with clouds while your bird flies there.
- **AI difficulty.** Easy, Normal, Hard, Nightmare.
- **Other stuff.** Learn new moves mid-battle, trainers use their own potions, weather and abilities work in real time, trainer rematches get stronger every time.

## Controls
Keyboard keys are mGBA's defaults.

| Button | Key | What it does |
|---|---|---|
| D-pad | Arrow keys | Move your Pokémon |
| A / B / L / R | X / Z / A / S | Your 4 moves (the bottom panel shows which is which and the cooldowns) |
| SELECT | Backspace | Pause: ball/team panel. Throw a Poké Ball to catch, or switch Pokémon. L/R shows move details |
| START | Enter | Run from a wild battle (you can't run from trainers) |

Options menu: AI difficulty and the battle start countdown.
Fly: normal FireRed Fly. Arrow keys to pick, A (X) to go, B (Z) to back out.

---

## Why some files are missing (and how to get them)

I can't publish everything. The Pokémon and attack sprites come from **Pokémon Mystery Dungeon: Red Rescue Team**. That art belongs to Nintendo / Spike Chunsoft, so it is **not in this repo**. Same goes for ROMs: none are included, and I won't share them.

What *is* here is all the code, plus the tools that pull the art out of **your own** PMD Red ROM. So you can rebuild the exact same game yourself:

1. Set up the build tools like normal pret FireRed ([INSTALL.md](INSTALL.md): devkitARM / `arm-none-eabi`, agbcc, Python 3 with Pillow).
2. Get **Pokémon Mystery Dungeon: Red Rescue Team (USA, Australia)** that you dumped yourself and point the importer at it:
   ```sh
   export PMD_RED_ROM="/path/to/Pokemon Mystery Dungeon - Red Rescue Team (USA, Australia).gba"
   python3 tools/import_pmd_mons.py
   python3 tools/import_pmd_attacks.py
   ```
   This creates `graphics/pmd_mons/`, `graphics/pmd_attacks/` and three generated headers in `src/data/`. They're in `.gitignore` so they never get uploaded.
3. Build:
   ```sh
   make -j8
   ```
   You get `pokefirered.gba`, the same ROM the patch makes (checked: byte-for-byte identical).

The FireRed source itself is the [pret pokefirered](https://github.com/pret/pokefirered) decompilation, the same base most FireRed hacks use. The Fly map picture in `graphics/fly_atlas/` is built from FireRed's own maps (rebuild it with the scripts in `tools/flyatlas/`).

---

## Use the pieces in your own game
You don't have to take the whole hack. Grab the parts you want and put them in your own FireRed, port them to Emerald (the pret decomps are very similar), another GBA hack, or even a different game. Figuring this stuff out from scratch takes hours. With this code to start from (and AI to help adapt it), it saves you a lot of headaches.

| Feature | Where it lives |
|---|---|
| Real-time battle system | `src/realtime_battle.c` + all `src/realtime_*.inc.c` (AI, damage, moves, status, arenas, HUD, camera, spawning) |
| PMD sprite runtime (8 directions, animations, multi-part sprites) | `src/hgss_sprites.c`, `include/hgss_sprites.h` |
| PMD art importers (extract from your own PMD Red ROM) | `tools/import_pmd_mons.py`, `tools/import_pmd_attacks.py`, `tools/pmd/` |
| Move animation playback (PMD effects in battle) | `src/realtime_ptd*.inc.c` |
| Follower Pokémon | `src/mon_follower.c` |
| Fly journey (zoom-out map, clouds, bird glide) | `src/fly_journey.c`, `include/fly_journey.h`, hooked in `src/field_effect.c` (`Task_UseFly`) |
| Fly map picture generator | `tools/flyatlas/` (`atlas3.py`, `build_fly_atlas.py`, `build_clouds.py`) |
| Extra trainer flags (more trainers than vanilla save space) | `src/extra_trainer_flags.c` |
| AI difficulty option | `src/option_menu.c`, `src/realtime_ai.inc.c` |
| Optional playtest menu ("GOD MODE") | `src/god_mode.c`, off by default (build flag `PLAYTEST_TOOLS=1`) |

My own code here is free to use, change and share. Credit is appreciated but not required. The pret decompilation and all Pokémon content keep their original owners' terms.

## About this version
This is just a version for now. It's the normal Kanto + Sevii map. An older build had a massive extra world, but it took up all the GBA space, so I cut it. I'm using this version to test and polish the features while the game stays fully playable. If I find the time, there might be an extended version later.

## Credits
- Made by me. Most of the code was written by **Claude Opus 5.5** (Anthropic) and **ChatGPT SOL 6.2** (OpenAI) together.
- Built on the [pret pokefirered](https://github.com/pret/pokefirered) decompilation. The original pret README is kept as [README-pret.md](README-pret.md).
- Pokémon, FireRed and Pokémon Mystery Dungeon belong to Nintendo, Game Freak, Creatures and Spike Chunsoft. This is a free, non-commercial fan project and is not affiliated with or endorsed by any of them.
