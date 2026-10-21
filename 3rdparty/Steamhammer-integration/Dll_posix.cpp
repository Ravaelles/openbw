// POSIX entry point for the vendored Steamhammer bot.
//
// WHY THIS FILE EXISTS
//   Upstream's Dll.cpp (in 3rdparty/Steamhammer/Steamhammer/Source/) is the
//   Windows entry point: it includes <windows.h>, uses __declspec(dllexport)
//   and defines DllMain. The CMakeLists excludes it, and on Linux it would not
//   compile.
//
//   Our engine loads AI modules with dlopen() and looks up exactly two symbols
//   (bwapi/BWAPI/Source/GameUpdate.cpp):
//       newAIModule -> BWAPI::AIModule*
//       gameInit    -> void(BWAPI::Game*)
//
//   This file provides those two symbols on POSIX, with default visibility so
//   dlsym() can find them. It is integration glue, deliberately OUTSIDE the
//   vendored tree, so upstream sources stay unmodified and can be updated.
//
// Symbol visibility matters: the harness builds with -fvisibility=hidden in
// some configurations, and dlsym() would then return nullptr and the loader
// would fall back to the dummy module with no error beyond a log line.
#include <BWAPI.h>
#include "UAlbertaBotModule.h"

#if defined(_WIN32)
#error "Dll_posix.cpp is the Linux/macOS entry point; use upstream Dll.cpp on Windows"
#endif

#define STEAMHAMMER_EXPORT extern "C" __attribute__((visibility("default")))

// BWAPI declares `extern Game* BroodwarPtr;` (bwapi/include/BWAPI/Game.h:1757)
// but nothing defines it: every AI module must define the variable itself.
// Without this definition the module links against an unresolved symbol and the
// loader cannot resolve it, so dlsym() succeeds while the first Broodwar call
// crashes. TestAIModule has the same definition in its Dll.cpp.
namespace BWAPI {
	Game* BroodwarPtr = nullptr;
}

STEAMHAMMER_EXPORT void gameInit(BWAPI::Game* game) {
	BWAPI::BroodwarPtr = game;
}

STEAMHAMMER_EXPORT BWAPI::AIModule* newAIModule() {
	return new UAlbertaBot::UAlbertaBotModule();
}
