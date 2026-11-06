# AGENTS.md

Guidance for AI coding agents working in this repository.

## Communication

- Answer in both English and Korean in every response. Write the English version first, then the
  Korean version with the same content.
- Commit messages and code comments stay in English.
- Commit messages have no `Co-Authored-By` trailer and name no model.
- Branch names have no `claude` prefix.
- Feature specs (`docs/specs/`) are written in Korean only. Every other document is written in both
  Korean and English in one file: the Korean version first, then the English version with the same
  content.
- When a response refers to code, give the file path and line (`src/battle.c:16`) and quote the code itself in a snippet
  under it, so the reader sees what is meant without opening the file.

## Project

The auto-battler (`docs/specs/battle.md`): a single-player game in the style of Mechabellum (`docs/autobattler.md`), built on the nv
engine. It runs in the browser (WebAssembly, WebGPU) and on Windows (Direct3D 12) from one source, as the engine does.

The engine is the Git submodule `engine/` (`chromedays/engine`), added with `add_subdirectory(engine)`. **Its `engine/AGENTS.md`
holds the rules this game's code follows too**: the platform rules (only `gpu.h` reaches the GPU, `NV_DEBUG_EXPORT`, C that compiles
with emcc and MSVC, no VLAs or GNU extensions), shaders, ImGui and input, `T()`/`TL()` strings, `nv_log`, the build label, the
renderer and effects. The coding standard is `engine/docs/CODING_STANDARD.md`. Read both before writing code; this file adds only
what is the game's.

```
src/                       the game: battle.c/.h (the rules and the 30 Hz tick; no GPU), defs.c (reads data/default.abproj),
                           lab.c/.h (the unit lab's runs on those rules; no GPU), menu.c/.h (the main menu round's placement; no
                           GPU), main.c (window, frame loop, screens, debug exports), battle_view.c (drawing, deployment input, the
                           Battle panel), lab_view.c (the unit lab's camera, lines and panel sections), menu_view.c (the main menu:
                           its round, camera and window), game.h (its state), strings.c (its Korean table)
data/default.abproj        the rules, the units (the wiki's unit_stats.csv) and the stage (docs/specs/abproj.md, unit_fields.md)
engine/                    the engine, a submodule: its headers are included as <engine/name.h>; the UI font is engine/assets/fonts/
tests/                     tests that need no browser, built for Node (web) or natively (Windows) and run with ctest
tools/dashboard/           the build dashboard (docs/specs/build_dashboard.md): a Cloudflare Worker and Durable Object (src/), its page
                           (public/index.html), publish.sh (CI's upload to R2) and README.md (the one-time setup, running it locally)
docs/specs/                feature specs (read the relevant one before working on a feature)
.github/workflows/build.yml  CI: Release and Debug web builds, tests, and their upload to the build dashboard
.github/workflows/dashboard.yml  deploys the dashboard's Worker when tools/dashboard/ changes on master
```

Where code goes: ask "would another app use this?". Yes goes in the engine (a change in `engine/`, committed and pushed to
`chromedays/engine` first, then the submodule moved to that commit here); no goes in `src/`. The engine never refers to the game.

## Build and run

Requires the Emscripten SDK (tested with 6.0.10), CMake 3.30+ (the engine's ozz-animation needs it) and Git LFS. The engine's LFS
files are needed only for the UI font.

```sh
git clone --recursive https://github.com/chromedays/autobattler.git   # or, in a clone: git submodule update --init
git -C engine lfs pull --include "assets/fonts/*"
emcmake cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
python3 -m http.server -d build 8000   # then open http://localhost:8000/
```

`just web [Release|Debug]` builds the web game into `build-release` or `build-debug` (configuring that folder with `emcmake` the first
time), and `just serve [Release|Debug] [port]` builds it and serves it at `http://127.0.0.1:8000/` (by default; bound to this computer only,
since WebGPU needs a secure context and plain HTTP is one only on localhost and 127.0.0.1).

While working on it, serve the page with the engine's development server instead (`engine/docs/specs/dev_server.md`): it rebuilds
the game when a file of this repository or `engine/` is saved and reloads the open pages when the build changes (`just dev`).

```sh
node engine/tools/dev_server.mjs --build build   # http://localhost:8000/; --no-build only reloads
```

The engine's build downloads its shader tools once (`engine/cmake/shaders.cmake`, into `NV_TOOLS_DIR`); see `engine/AGENTS.md`.

### On Windows

Requires what `engine/AGENTS.md` lists (Windows 11 x64, MSVC and the Windows SDK, CMake 3.30+, Git LFS):

```sh
cmake -S . -B build-win -G "Visual Studio 18 2026" -A x64
cmake --build build-win --config Release -j
build-win\Release\autobattler.exe      # Debug\ for the Debug build
```

The `justfile` (recipes in PowerShell) wraps these: `just build Release`, `just run Release`, `just test`, `just script <file>`;
`just` lists them, and each takes Debug by default. A native build is not built by CI, so a change to a file that does not know the
platform is built for both the web and Windows before it is pushed.

### Moving the engine

`engine/` is pinned to one commit of `chromedays/engine`. To take a newer engine: `git -C engine fetch && git -C engine checkout
<commit>`, build and test both builds, then commit the submodule change with what it brings in the message. An engine change the
game needs is a pull request on `chromedays/engine` first; the game moves to its commit once it is pushed there.

## Conventions

Follow `engine/AGENTS.md` ("Conventions") and `engine/docs/CODING_STANDARD.md`. The game's own:

- The auto-battler (`docs/specs/battle.md`): `battle.c` is the rules and reads no GPU, ImGui or clock, so `tests/battle_test.c` runs whole
  rounds; the view (`battle_view.c`) steps it with `battle_tick` at a fixed 30 Hz and only reads it, changing it through `battle_place`,
  `battle_remove`, `battle_start` and `battle_retry`. The rules (grid, round time, gravity and so on), the units and the
  stage are one text file, `data/default.abproj` (`docs/specs/abproj.md`), read by `defs_read_project` in `defs.c` (a bad
  file blocks Start and is reported with its line). The panel opens, reloads and saves a local `.abproj` (`engine/local_file.h`;
  abproj.md, "Loading and saving"): a good file is put in place by `game_load_project`, a bad one leaves the project in use, and Save
  writes the kept text as it was read. The panel's Rules section edits that text in place, before a round only (`defs_rule_set`,
  `game_set_rule`; abproj.md, "Editing the rules"): a change rewrites only that rule's values and is read whole, a bad one is refused.
  A new key goes in `defs.c`'s field tables and the spec's tables. The format stays at version 1 while it is in development
  (abproj.md, "Versions"). A unit block is a row of the wiki's `unit_stats.csv` (`docs/specs/unit_fields.md`): its keys are the CSV's
  column names and its values the CSV's spellings, one line for each cell that is not empty; the wiki is where unit values are decided.
  Keys the round does not follow yet are read, checked and kept (`BattleDefs.unfollowed`, warned of once a load) until their rules are
  built; until then every projectile attack fires the interim shell (`BATTLE_INTERIM_LAUNCH_ANGLE`). A unit def is placed as a squad
  (`squad_size` units on `footprint_w` x `footprint_d` cells in the wiki's D-07 grid, `battle_squad_grid`), by its footprint's corner
  nearest the grid's origin; a rule is read from `defs->rules`, never from a macro, and tests try the format only
  with text of their own, never with the file's values.
- Its viewport, its panel and its playback bar (under the viewport: the timeline, Start and Retry or the lab's Restart, Pause,
  the ticks and speeds; battle.md, "Playback bar") must not overlap (input that starts in the viewport skips ImGui). The UI's size is the engine's
  `ui_scale` times the game's `Game.ui_zoom` (the panel's UI scale combo; 200% on a desktop to start, not saved; battle.md,
  "Executable"). The engine's `ui_scale` also tells a finger from a mouse, so the game never changes it and asks `game->touch`
  instead; pixel sizes the game draws itself (the lab's dimensions, the panel's width) go through `game_ui_scale`. UI text goes through `T()`/`TL()` with
  rows in `src/strings.c` (`engine/docs/specs/korean.md`); ctest's `strings_test` (the engine's `tests/strings_test.mjs`, run on `src/`)
  fails on a missing row.
- Debug builds export `Module._battle_debug(n)`, `_battle_debug_deploy`, `_battle_debug_start`, `_battle_debug_run`,
  `_battle_debug_set_speed`, `_battle_debug_step`, `_battle_debug_seek`, `_battle_debug_layout`, `_battle_debug_project`, `_battle_debug_load`,
  `_battle_debug_project_action`, `_battle_debug_project_message`, `_battle_debug_set_rule`, `_battle_debug_project_text`,
  `_battle_debug_unit_roles`, `_battle_debug_set_role_filter`, `_battle_debug_load_test`, and for the unit lab `_battle_debug_lab(n)`,
  `_battle_debug_lab_enter`, `_battle_debug_lab_set`, `_battle_debug_lab_action`, `_battle_debug_lab_run` and
  `_battle_debug_set_unit_value` (values in hundredths, so the Windows script can pass them), and for the main menu `_battle_debug_menu(n)`
  and `_battle_debug_menu_action` for tests; on Windows a new export is also a row of `battle_debug_calls` in `src/main.c`.
- The main menu (`docs/specs/main_menu.md`): the game starts at it (Start Game, Quit) over a round of its own (`Game.menu_battle`: the
  stage's enemy against its mirror, `menu_place_mirrored`, at 1x and again once over; it never touches the player's deployment), and the
  panel's Main menu and Esc go back to it (a running round goes back to deployment, the lab to the battle). `--editor` (`?editor` on the web,
  where the engine's page makes the address's options `argv`) starts in the game with no menu (`Game.editor`). Quit is `nv_quit`, which
  ends the program on both platforms; never `exit`, which on the web leaves the frame loop running. Screens change through
  `game_set_screen` (a fade); the menu turns on ImGui's keyboard navigation for itself and off for the game screen. Tests that expect the
  game screen at start open `?editor` (the Windows script `--editor`).
- The unit lab (`docs/specs/unit_lab.md`; the panel's `Unit Lab` mode button, from deployment only): one unit of a unit def (one of a
  squad) against a ground and an air target, on `battle.c`'s own rules. `lab.c` keeps its own small `BattleDefs` (the unit def, the two
  targets) and `Battle`; the targets are `UNIT_DUMMY` units (no target, no fire, moved by the velocity the lab sets), a unit that holds
  still is `UNIT_HOLD`, and the round is `endless`. A change of its settings, the unit's values (`defs_unit_set`, `game_set_unit_value`) or
  the rules goes into the run as it goes and is recorded, so `lab_seek` replays it exactly; Restart on change starts the run over instead.
  The view draws `game_shown_battle` with the same code, the lab's meshes and grey targets. Its settings are not saved.
- Roles (`roles`) are for people sorting units: `battle.c` never reads them, so no round depends on them. A unit's `unit_layer` (Ground
  or Air) and its `target_layer` decide what can hit what; air units fly at `altitude_m`, and a shell passes units whose layer its shooter
  cannot target. A unit's shield (`shield_hp`) is its own and takes a whole hit before its health (the wiki's D-05, D-10); a melee attack
  hits at once, with no shell (D-13). Speed controls change only how many ticks a frame runs (the game clock), never the tick, and the effects take the
  same game clock's dt. Stepping back is `battle_seek` (a replay from the start, which the deterministic rules make exact).
- The executable packs only what it reads into `autobattler.data` (`nv_setup_executable(... PRELOAD ...)` in `CMakeLists.txt`): the
  engine's font folder at `/assets/fonts` and `data/` at `/data`. CI installs both builds into the `autobattler-web` artifact
  (`release/`, `debug/`). Debug builds keep their DWARF in `autobattler.debug.wasm`, which only browser developer tools download.
- The build dashboard (`tools/dashboard/`, `docs/specs/build_dashboard.md`) shows the Release build of master and of every branch
  with an open pull request, from R2, each opened two ways: Game (the page as it is, from the main menu; `docs/specs/main_menu.md`)
  and Editor (`?editor`: straight into the battle and its panel). The Debug build is built and tested by CI but not published. It
  updates open pages live from GitHub's webhooks. CI's `publish` and `publish-opened` jobs upload it
  (`tools/dashboard/publish.sh`); they skip with a notice while the secrets are unset. No secret is written in the
  repository: tokens live only in GitHub Actions secrets and Worker secrets. Jobs that use secrets run only for this repository's
  pushes and branches, never `pull_request_target`. Its state rules (`tools/dashboard/src/state.mjs`) are plain functions that
  `tests/dashboard_test.mjs` runs; the Worker is tried with `wrangler dev` (`tools/dashboard/README.md`).

## Assets

- The game has no binary assets of its own; it uses the engine's UI font. A binary added here goes through Git LFS: `.gitattributes`
  lists the tracked types; add a pattern there before committing a new binary type, and check `git lfs ls-files` after adding it.

## Third-party libraries

- When implementing a feature or designing a spec, first look for third-party libraries that
  would help. Present the candidates to the user (what each does, language, license, how it fits
  this game and the coding standard, trade-offs) with a recommendation, including "write it
  ourselves" when that is the better fit.
- Do not add a library until the user confirms it. A library the engine would use goes in the engine and its coding standard's
  Dependencies section; one only the game uses gets its reason recorded in this file.

## Testing without a display

There is no physical display in cloud sessions. Headless Chromium renders WebGPU with
`--enable-unsafe-webgpu --enable-features=Vulkan --use-vulkan=swiftshader
--use-webgpu-adapter=swiftshader --use-angle=swiftshader --disable-vulkan-fallback-to-gl-for-testing`.
Test both a Release build and a Debug build (the Debug build enables `NV_ASSERT`), and check that the browser console has no WebGPU
error. Call the debug exports only after the module has started (wait a few seconds after the page loads): a call before that fails an
assertion and stops the module.
When driving ImGui with Playwright, hold clicks for about 100 ms (`mouse.down`, wait, `mouse.up`);
an instant click can land between frames and be missed, which real users never trigger. A touch on a panel widget takes a few frames
to count as a tap: tests that touch buttons wait about two seconds.
`ctest --test-dir build --output-on-failure` runs `tests/` under Node (CI runs it for both builds); `battle_test` reads `data/`
directly.

## Testing on Windows

The native Debug build can be driven by a script (`engine/docs/specs/d3d12_port.md`, "Debug exports and native tests"):
`autobattler.exe --script <file> --out <dir>` runs one command a line in the app's frames (`engine/debug_script.h`) and exits with 1
when the log holds an error. `call` finds the debug exports in `battle_debug_calls` (`src/main.c`). The Debug build turns on the D3D12
debug layer and GPU-based validation, so a run with no `[d3d12]` message in its log is the native counterpart of "no WebGPU error";
`NV_D3D12_VALIDATION=0`, `NV_STORAGE_DIR` and `NV_LOCAL_FILE` work as `engine/AGENTS.md` says. `ctest --test-dir build-win -C Debug`
(or `-C Release`) runs the C tests (the `.mjs` tests run only where Node is found). Test both a Release build and a Debug build on
Windows.
