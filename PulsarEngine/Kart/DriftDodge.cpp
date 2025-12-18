#include <kamek.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/Kart/KartCollision.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <MarioKartWii/Kart/KartMovement.hpp>
#include <MarioKartWii/Kart/KartPlayer.hpp>
#include <MarioKartWii/Kart/KartPointers.hpp>
#include <MarioKartWii/Kart/KartValues.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace Race {

static const u32 MAX_PLAYERS = 12;

struct SmtDodgeState {
	s16 prevMtFrames;
	bool hasSmtBoost;
	s16 dodgeTimer; // frames of post-hop immunity
};

static SmtDodgeState sSmtStates[MAX_PLAYERS];

static void ResetState(u32 idx) {
	sSmtStates[idx].prevMtFrames = 0;
	sSmtStates[idx].hasSmtBoost = false;
	sSmtStates[idx].dodgeTimer = 0;
}

// Tracks when a new MT/SMT boost starts and remembers if it was an SMT-length boost.
static void UpdateSmtStatesPerFrame() {
	Kart::Manager* manager = Kart::Manager::sInstance;
	bool touched[MAX_PLAYERS] = {};
	if (manager != nullptr) {
		const u8 playerCount = manager->playerCount;
		for (u8 i = 0; i < playerCount; ++i) {
			Kart::Player* player = manager->players[i];
			if (player == nullptr) continue;

			Kart::Pointers& pointers = player->pointers;
			Kart::Movement* movement = pointers.kartMovement;
			Kart::Values* values = pointers.values;
			if (movement == nullptr || values == nullptr) continue;

			const u8 slot = values->playerIdx;
			if (slot >= MAX_PLAYERS) continue;
			touched[slot] = true;

			SmtDodgeState& state = sSmtStates[slot];
			const s16 mtFrames = movement->mtBoost;
			const Kart::Stats* stats = values->statsAndBsp.stats;
			const s16 baseMtFrames = (stats != nullptr) ? static_cast<s16>(stats->mt) : 0;

			if (mtFrames > state.prevMtFrames) {
				state.hasSmtBoost = mtFrames > baseMtFrames;
			} else if (mtFrames <= 0) {
				state.hasSmtBoost = false;
			}

			state.prevMtFrames = mtFrames;

			const bool hasSmtBoost = state.hasSmtBoost && mtFrames > 0;
			const bool isHopping = movement->hopFrame > 0;

			if (hasSmtBoost && isHopping) {
				state.dodgeTimer = 5; // Buffer time in Frames
			} else if (state.dodgeTimer > 0) {
				--state.dodgeTimer;
			}
		}
	}

	for (u32 idx = 0; idx < MAX_PLAYERS; ++idx) {
		if (!touched[idx]) ResetState(idx);
	}
}
static RaceFrameHook sSmtStateUpdater(UpdateSmtStatesPerFrame);

static bool CanDodgeBlueShell(const Kart::Movement& movement, u8 slot) {
	if (slot >= MAX_PLAYERS) return false;
	const SmtDodgeState& state = sSmtStates[slot];
	const bool hasSmtBoost = state.hasSmtBoost && movement.mtBoost > 0;
	const bool isHopping = movement.hopFrame > 0;
	const bool hasBuffer = state.dodgeTimer > 0;
	return (hasSmtBoost && isHopping) || hasBuffer;
}

static int HandleBlueShellCollision(Kart::Collision* collision, void* itemObj) {
	Kart::Movement* movement = nullptr;
	u8 slot = MAX_PLAYERS;

	if (collision != nullptr) {
		Kart::Pointers* pointers = collision->pointers;
		if (pointers != nullptr) {
			movement = pointers->kartMovement;
			Kart::Values* values = pointers->values;
			if (values != nullptr) slot = values->playerIdx;
		}
	}

	if (movement != nullptr && CanDodgeBlueShell(*movement, slot)) {
		return static_cast<int>(NO_DAMAGE);
	}

	const u8 isExplosionActive = (itemObj != nullptr) ? *reinterpret_cast<u8*>(reinterpret_cast<u8*>(itemObj) + 0x334) : 0;
	return (isExplosionActive == 0) ? static_cast<int>(LAUNCH_EXPLOSION)
									: static_cast<int>(SPINOUT_BANANA);
}
kmBranch(0x805731b0, HandleBlueShellCollision);

}  // namespace Race
}  // namespace Pulsar
