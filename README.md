# CrimsonCode Retro Mac App

Classic Macintosh sample app project built with Metrowerks CodeWarrior (Mac OS 9).

The source is `SillyBalls.c`, a Color QuickDraw sample that opens a window and draws random colored "Bob" balls.

## Project contents

- `SillyBalls.c`: classic Mac Toolbox C source.
- `HelloMac9`: CodeWarrior project file.
- `HelloMac9 Data/`: CodeWarrior target/settings data.
- `MacOS Toolbox DEBUG PPC`: built PowerPC debug app (PEF format).
- `MacOS Toolbox DEBUG PPC.xSYM`: debug symbols for the PPC build.
- `SillyBalls.rsrc`: resource file placeholder (currently empty).

## Can it run directly on modern macOS?

No. The built binary is a classic Mac OS PowerPC PEF executable, not a modern macOS Mach-O binary.

Example:

```sh
./MacOS\ Toolbox\ DEBUG\ PPC
# zsh: exec format error
```

## How to run locally on a modern Mac

### Option 1 (recommended): run inside a local classic Mac emulator

Use a local Mac OS 9 environment, then open/run the existing binary or rebuild in CodeWarrior.

1. Install a classic Mac emulator:
   - SheepShaver: <https://www.sheepshaver.cebix.net/>
   - Mac emulator source/project docs: <https://github.com/cebix/macemu>
2. Set up a Mac OS install in the emulator (classic system + ROM as required by emulator docs).
3. Mount/share this repo into the emulator guest.
4. In the guest OS:
   - Double-click `MacOS Toolbox DEBUG PPC` to run the existing app, or
   - Open `HelloMac9` in CodeWarrior and build from source.

Notes:
- Keep `HelloMac9` and `HelloMac9 Data/` in the same folder.
- If one emulator build does not work well on your host, use another build/runtime for the same project docs.

### Option 2: UTM/QEMU PowerPC VM (good fallback, especially on Apple Silicon)

1. Install UTM: <https://mac.getutm.app/>
2. Create a PowerPC Mac VM and install a classic Mac OS version supported by your setup.
3. Transfer this project folder into the VM.
4. Run `MacOS Toolbox DEBUG PPC` or rebuild in CodeWarrior.

## Rebuilding in CodeWarrior (inside emulator)

1. Launch CodeWarrior in the guest Mac OS.
2. Open `HelloMac9`.
3. Select a MacOS Toolbox target (for example a PPC Debug target).
4. Build.
5. Run the generated app output from the project folder.

## If you want a native modern macOS app

This code uses classic Toolbox/QuickDraw APIs, so a native run on current macOS requires a port (Cocoa, SDL2, or another modern graphics layer).

## Included modern-mac game (non-invasive)

To keep the original CodeWarrior project untouched, this repo includes a separate terminal game:

- `modern/connect-four-virus.c` (UI + main)
- `modern/connect_four.c` / `modern/connect_four.h` (board rules)
- `modern/connect_four_ai.c` / `modern/connect_four_ai.h` (minimax AI)
- `Makefile`

Build and run:

```sh
make
make run
```

Binary output:

- `build-modern/connect-four-virus`

Controls:

- Left/Right (or `A`/`D`) to choose a column
- `Enter` or `Space` to drop
- `1`-`6` to jump-select columns
- `q` to quit, `r` to restart after game over
- After game over, a new round auto-starts after ~5 seconds (you can still press `r` immediately).

Gameplay note:

- The game includes a fake on-screen "Mac OS 9 VM console" with boot/status/event logs.
- When AI wins, a random `1`-`6` incident simulation runs in the fake VM (nVIR, MDEF/CDEF, WDEF/Zuc, macro viruses, AutoStart, SevenDust/666 themes).
- All malware behavior is visual/text simulation only. It does not execute real malware actions or modify your system outside the game process.
- Incident effects persist across rounds in the current run. Repeated hits of the same incident stack and intensify debuffs in later games.
- Player wins trigger a recovery sweep that lowers persistent threat so the game can gradually stabilize.
- A `System Compromised: XX%` meter now drives sabotage intensity. Every AI-win incident increases compromise.
- At higher compromise levels, punishments can hijack input (`"Virus moved you haha!"`), flip the grid for a turn, and force temporary purple takeover visuals.
- Greeting adapts with infection state (`Hello Player` -> `Hello Loser` at high compromise).
- Startup now includes a fake classic desktop sequence: enter your name, then open `SUSPICIOUS.EXE` to launch containment mode.

This modern build path does not overwrite any classic Mac files.
