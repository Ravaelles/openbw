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
#include <cstdio>
#include <cstdlib>
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

	// Slot selection belongs HERE, not in onStart(). GameImpl::switchToPlayer
	// is a no-op once isInGame() is true, and it is true by the time onStart()
	// runs (measured). In gameInit it is still false, so the call takes effect;
	// the engine then keeps the choice because sync() honours module_slot_set.
	if (game) {
		if (const char* slot = std::getenv("STEAMHAMMER_SLOT")) {
			char* end = nullptr;
			long n = std::strtol(slot, &end, 10);
			if (end && *end == '\0' && n >= 0 && n < 8) {
				game->switchToPlayer(game->getPlayer((int)n));
			}
		}
	}
}

// Why a wrapper at all:
//   Steamhammer's onStart() calls BWAPI::Broodwar->self()->getRace(). An AI
//   module loaded from a file is not a sync client, so the engine never assigns
//   it a player slot and self() is nullptr. TestAIModule avoids this by never
//   touching self(); Steamhammer cannot.
//
//   The wrapper does one thing: on the first onStart() it switches to the slot
//   named by STEAMHAMMER_SLOT (0..7), then delegates everything to the real
//   module. No upstream source is changed, so behaviour otherwise stays as
//   shipped. Without STEAMHAMMER_SLOT it delegates immediately, which keeps the
//   default identical to upstream.
namespace {
class SlotAssigningModule : public BWAPI::AIModule {
public:
	SlotAssigningModule() : inner_(new UAlbertaBot::UAlbertaBotModule()) {}
	~SlotAssigningModule() override { delete inner_; }

	void onStart() override {
		// Diagnostic: which slot did we get, and does it own a base?
		// Needed because "our base is unknown" can mean either "no slot" or
		// "a slot with no starting base", and the two have different fixes.
		{
			BWAPI::Player self = BWAPI::Broodwar->self();
			int units = self ? (int)self->getUnits().size() : -1;
			std::fprintf(stderr, "steamhammer: slot=%d self=%p units=%d start=%d,%d\n",
				self ? self->getID() : -1,
				(void*)self, units,
				self ? self->getStartLocation().x : -1,
				self ? self->getStartLocation().y : -1);
		}
		// Slot already chosen in gameInit (switchToPlayer no-ops here).
		inner_->onStart();
	}
	void onEnd(bool isWinner) override { inner_->onEnd(isWinner); }
	void onFrame() override { inner_->onFrame(); }
	void onSendText(std::string text) override { inner_->onSendText(text); }
	void onReceiveText(BWAPI::Player p, std::string text) override { inner_->onReceiveText(p, text); }
	void onPlayerLeft(BWAPI::Player p) override { inner_->onPlayerLeft(p); }
	void onNukeDetect(BWAPI::Position t) override { inner_->onNukeDetect(t); }
	void onUnitDiscover(BWAPI::Unit u) override { inner_->onUnitDiscover(u); }
	void onUnitEvade(BWAPI::Unit u) override { inner_->onUnitEvade(u); }
	void onUnitShow(BWAPI::Unit u) override { inner_->onUnitShow(u); }
	void onUnitHide(BWAPI::Unit u) override { inner_->onUnitHide(u); }
	void onUnitCreate(BWAPI::Unit u) override { inner_->onUnitCreate(u); }
	void onUnitDestroy(BWAPI::Unit u) override { inner_->onUnitDestroy(u); }
	void onUnitMorph(BWAPI::Unit u) override { inner_->onUnitMorph(u); }
	void onUnitRenegade(BWAPI::Unit u) override { inner_->onUnitRenegade(u); }
	void onSaveGame(std::string name) override { inner_->onSaveGame(name); }
	void onUnitComplete(BWAPI::Unit u) override { inner_->onUnitComplete(u); }

private:
	BWAPI::AIModule* inner_;
};
} // namespace

STEAMHAMMER_EXPORT BWAPI::AIModule* newAIModule() {
	return new SlotAssigningModule();
}
