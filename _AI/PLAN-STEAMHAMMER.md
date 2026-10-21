# Plan: play 1v1 against Steamhammer instead of the native AI

Branch: `feature/use-steamhammer-as-native-ai`
Status: **plan only, no code moved yet**
Written: 2026-10-20, because the working context was about to end.

---

## 0. Goal

Host a 1v1 on OpenBW where the **opponent** is Steamhammer, not the (missing)
native StarCraft AI. One human/bot side plays our normal client, the other side
is Steamhammer driving its units.

Success criterion: a map loads, both sides get units, Steamhammer builds,
moves and attacks, and the match produces real kills - with
`state::skipped_native_ai_orders == 0`.

---

## 1. What we actually have (measured, not assumed)

### Source code - YES, present locally

```
/ravaelles/JAVA/starcraft-ai/StardustDevEnvironment/
  test/3rdparty/opponents/Steamhammer/    6.6 MB, 368 .cpp/.h
    Steamhammer/Source/                   ~90 files: CombatCommander, BuildingManager,
                                          BuildOrder, InformationManager, ...
    BOSS/                                 Blackboard architecture library it depends on
    CMakeLists.txt
```

Build file, verbatim:

```cmake
file(GLOB_RECURSE SRC_FILES .../Steamhammer/Source/*.cpp)
list(FILTER SRC_FILES EXCLUDE REGEX Dll.cpp)
file(GLOB_RECURSE BOSS_SRC_FILES .../BOSS/source/*.cpp)
list(FILTER BOSS_SRC_FILES EXCLUDE REGEX BOSS_main.cpp)
...
add_library(Steamhammer SHARED ${SRC_FILES} ${BOSS_SRC_FILES})
target_link_libraries(Steamhammer -L/usr/local/Cellar/llvm/13.0.0_2/lib BWAPILIB)
```

**Key fact:** Steamhammer is built as a **shared library that links against
`BWAPILIB`** - the same class of artifact as `TestAIModule.so`, which already
works in our harness. It is not an in-process engine component and must not be
compiled into `bwgame.h`.

### Binaries - present, but Windows-only

```
/home/rav/.scbw/bots/Steamhammer/AI/Steamhammer.dll    PE32 Windows x86
/home/rav/.scbw/bots/Steamhammer/BWAPI.dll             PE32 Windows x86
```

The .dll is a downloaded SSCAIT build and is **irrelevant** to us: our launcher
is native Linux. We use it only as a reference; we build from source.

### What the old test file is

`StardustDevEnvironment/test/Steamhammer.cpp` is a Stardust test that launches
`UAlbertaBotModule` under the label "Steamhammer". Steamhammer descends from
UAlbertaBot, so the behaviour is related, but that file is **not** the bot.

---

## 2. Why the naive approach fails

| attempt | why it fails |
|---|---|
| copy `Steamhammer.dll` into `/3rdparty/` | Windows x86 PE, needs Wine + StarCraft.exe |
| `#include` Steamhammer sources into `bwgame.h` | it is a BWAPI *client*, not engine code |
| run it against Wine | abandons the native-Linux setup we fixed |

The workable path is the one the code already supports: **build Steamhammer as
a Linux shared module against our BWAPILIB and load it like `TestAIModule`.**

---

## 3. Approach (in order, one step at a time)

### Step 1 - does our BWAPILIB support an external AI module? (already yes)

Evidence: `BWAPILauncher` loads `TestAIModule.so` via `BWAPI_CONFIG_AI__AI`,
and `bwapi.ini` has the `ai = ...` field. That path is already used by every
run we do.

**Task:** confirm how the module is selected (`BWAPI_CONFIG_AI__AI=<path>`),
and whether a *second* module can drive a different player slot. This decides
whether Steamhammer plays "the other side" through the same mechanism.

### Step 2 - build Steamhammer as a Linux module

```
# inside /sc-ai/OpenBW/openbw/3rdparty/Steamhammer (new copy, step 4)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
# expect: build/libSteamhammer.so
```

Link against **our** `BWAPILIB`. Two sub-tasks:

- 2a. point `target_link_libraries` at our `libBWAPI.so` and its include dir
  (`3rdparty/openbw/bwapi/bwapi/include`);
- 2b. compile with the same C++ ABI as the harness (gcc, `-std=c++17`), because
  a mismatched ABI cannot load into the same process.

Expected blockers (measure, do not guess):
- Steamhammer uses BWAPI 4.2 API; our `libBWAPI.so` is the same major version -
  check for missing symbols with `nm -D --undefined-only libSteamhammer.so`.
- `BOSS` may pull in Qt/emscripten sources; the CMake already excludes those.

### Step 3 - load it instead of the native AI

In `BWAPILauncher`, the AI module is picked from config. Run:

```
BWAPI_CONFIG_AI__AI=./libSteamhammer.so \
BWAPI_CONFIG_AUTO_MENU__MAP=<map> \
BWAPI_CONFIG_AUTO_MENU__GAME_TYPE=USE_MAP_SETTINGS \
./BWAPILauncher
```

Success for this step: the module loads and `onStart()` runs (any log line from
Steamhammer proves it).

### Step 4 - copy the sources into our repo

Per the request: new top-level `3rdparty/` in **our** repository:

```
/sc-ai/OpenBW/openbw/3rdparty/Steamhammer/
  Steamhammer/Source/     <- from the Stardust copy
  BOSS/source/
  CMakeLists.txt          <- adjusted for our libBWAPI paths
  LICENSE / licenses/     <- keep the licence files
  README.upstream.md      <- note the origin and commit, do not pretend it is ours
```

Do **not** modify upstream sources to make them compile if it can be avoided;
put our glue in `3rdparty/Steamhammer/cmake/` or a sibling `integration/`.

Record the origin precisely: repository URL, the Stardust path it came from,
and the date. Vendored code without provenance is untraceable later.

### Step 5 - give Steamhammer a player slot with units

This is the part the previous investigation already mapped:

- Steamhammer (like any BWAPI module) plays the slot it is switched into;
- our launcher's auto-menu puts the client into the first `EitherPreferHuman`
  slot;
- maps place units on specific slots.

**Task:** choose a map where the "human" slot and Steamhammer's slot both own
units, or use `create_melee_units_for_player` so the slot gets a start.

Evidence needed: a run where Steamhammer issues a real order (log line from its
`CombatCommander`, and `KILLED > 0` at the end).

### Step 6 - make it the default opponent on this branch

Bind the build so a run script needs no manual module path:

```
scripts/run-openbw-steamhammer.sh <map>
```

which sets `BWAPI_CONFIG_AI__AI` to the built module and the game type to
`USE_MAP_SETTINGS`.

---

## 4. Risks, ranked

| risk | likelihood | mitigation |
|---|---|---|
| ABI/symbol mismatch with our `libBWAPI.so` | medium | `nm -D --undefined-only` before blaming the bot |
| Steamhammer needs pygame / its config dir (`steamhammer/`) | medium | create the expected config directory next to the module |
| BWAPI version drift (4.1.2 vs 4.2) | medium | compare headers; patch only the shim, never the algorithm |
| BOSS build pulls in Qt/emscripten | low | already excluded in its CMakeLists |
| Our client cannot run **two** modules into different slots | medium | Step 1 answers this before any porting work |
| Map semantics (who owns what) | high, known | reuse the slot findings in the docs below |

---

## 5. What this does NOT do

- It does not implement the native StarCraft AI. `skipped_native_ai_orders`
  stays as the signal for maps that need it (BUG-12).
- It does not make Steamhammer part of `bwgame.h`. It stays an external client.
- It does not touch Atlantis.

---

## 6. Where the knowledge lives

- `_AI/CONVENTIONS.md` - measure before asserting; two axes of truth.
- `_AI/BUGS.md` BUG-12 - native AI is only partially supported; the counter.
- `_AI/NON-OBVIOUS-FACTS.md` - engine quirks (shared memory, handshake,
  revealer lists, slot semantics).
- `__SC_DOMAIN_EVIDENCE.md` - verified game facts with quotes.
- `/sc-ai/OpenBW/openbw/docs/` (if present) - older investigation notes.

---

## 7. First three actions when work resumes

1. `nm -D --undefined-only` on a first build attempt; list missing symbols.
2. Read `BWAPI_CONFIG_AI__AI` handling in `BWAPILauncher`; decide if two slots
   can be driven.
3. Copy the sources into `3rdparty/Steamhammer/` with a provenance README, then
   build unmodified and record the exact error (if any) before changing a line.
