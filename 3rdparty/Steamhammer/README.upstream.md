# Vendored: Steamhammer

Copied verbatim on 2026-10-20 from:

    /ravaelles/JAVA/starcraft-ai/StardustDevEnvironment/test/3rdparty/opponents/Steamhammer

Upstream project: Steamhammer, a BWAPI 4.x bot (descended from UAlbertaBot).
Licence files are in `licenses/`. This directory is **upstream code**: do not
edit it to make it build. Put integration glue in `../Steamhammer-cmake/` or in
the harness instead, so a future upstream update can be applied cleanly.

Contents:
- `Steamhammer/Source/` - bot code (70 .cpp)
- `BOSS/source/`        - Blackboard architecture library (73 .cpp)
- `CMakeLists.txt`      - upstream build file (links `BWAPILIB`)
