# UMS duel opponents: who plays the enemy

Bot-side counterpart: Atlantis `_AI/CHALLENGES/UmsDuelOpponents.md`
(same facts, bot paths). This file is the engine side.

- UMS duel slots: p0 open (external bot), p1-3 computer. What the
  computer slots DO depends on the host, not the map.
- Under BWAPILauncher + TestAIModule: `TestAIModule/Source/Dll.cpp`
  returns `DefaultTestModule` for every map that is not a named test
  map. That module draws text and sends `show me the money` /
  `operation cwal`; it issues ZERO orders. The enemy therefore runs
  on engine default unit AI only: idle until acquisition range, then
  pile the nearest target. There is no AIScript interpreter (A1.2b),
  so map-authored behavior does not exist here either.
- Measured 2026-10-21 (`ZealDrag_v_LingsUpg.scm`, 12 preplaced ours
  vs 32 upgraded Lings, triggers = Melee.trg only): the opening
  wipes 4 ours for 0 ling kills in ~300 frames, then both sides sit
  5500+ frames (the map has bases, so no baseless hunt fires) until
  the bot force-leaves. Same bot + map on real StarCraft: 2/3
  batch wins, 32-8 IDE VICTORY in 36 game-sec.
- Do not read OpenBW duel W/L as bot skill vs the map's intended
  opponent. It measures anti-blob-surround only.
