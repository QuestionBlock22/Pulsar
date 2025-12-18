#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <MarioKartWii/Kart/KartMovement.hpp>
#include <MarioKartWii/Kart/KartPlayer.hpp>
#include <MarioKartWii/Kart/KartPointers.hpp>
#include <MarioKartWii/Kart/KartStatus.hpp>

namespace Pulsar {
namespace StartBoost {

static const s16 TWO_SECOND_FRAMES = 2 * 60;
static const u32 OFFROAD_IMMUNITY_FLAG = 0x80;
static const u32 MAX_PLAYERS = 12;
static const u32 SPECIAL_FLOOR_BOOST_PANEL = 0x1;
static const u32 SPECIAL_FLOOR_BOOST_RAMP = 0x2;

struct StartBoostChainState {
	s16 chainTimer;
	s16 prevMtFrames;
	s16 prevPanelFrames;
	bool active;
};

static StartBoostChainState sChainStates[MAX_PLAYERS];

static void ActivateChain(const Kart::Pointers& pointers, Kart::Movement& movement) {
	const Kart::Values* values = pointers.values;
	if(values == nullptr) return;
	const u8 slot = values->playerIdx;
	if(slot >= MAX_PLAYERS) return;
	StartBoostChainState& state = sChainStates[slot];
	state.chainTimer = TWO_SECOND_FRAMES;
	state.prevMtFrames = movement.mtBoost;
	state.prevPanelFrames = movement.boost.mushroomBoostPanelFrames;
	state.active = true;
}


static void ApplyStartBoostChainEffects(Kart::Movement& movement) {
	Kart::Pointers* pointers = movement.pointers;
	if(pointers == nullptr) return;
	Kart::Status* status = pointers->kartStatus;
	if(status != nullptr) status->bitfield1 |= OFFROAD_IMMUNITY_FLAG;

	if(movement.offroadInvincibilityFrames < TWO_SECOND_FRAMES) {
		movement.offroadInvincibilityFrames = TWO_SECOND_FRAMES;
	}
	ActivateChain(*pointers, movement);
}

static void ResetChainState(StartBoostChainState& state) {
	state.chainTimer = 0;
	state.prevMtFrames = 0;
	state.prevPanelFrames = 0;
	state.active = false;
}

static bool DidTriggerBoostPanel(Kart::Movement& movement, StartBoostChainState& state) {
	const s16 currentFrames = movement.boost.mushroomBoostPanelFrames;
	const bool increased = currentFrames > state.prevPanelFrames;
	state.prevPanelFrames = currentFrames;
	if(!increased) return false;

	const u32 floorFlags = movement.specialFloor;
	return (floorFlags & (SPECIAL_FLOOR_BOOST_PANEL | SPECIAL_FLOOR_BOOST_RAMP)) != 0;
}

static void SustainStartBoostChain() {
	Kart::Manager* manager = Kart::Manager::sInstance;
	bool slotTouched[MAX_PLAYERS] = {};
	if(manager != nullptr) {
		const u8 playerCount = manager->playerCount;
		for(u8 i = 0; i < playerCount; ++i) {
			Kart::Player* player = manager->players[i];
			if(player == nullptr) continue;
			Kart::Pointers& pointers = player->pointers;
			Kart::Movement* movement = pointers.kartMovement;
			Kart::Status* status = pointers.kartStatus;
			Kart::Values* values = pointers.values;
			if(movement == nullptr || status == nullptr || values == nullptr) continue;
			const u8 slot = values->playerIdx;
			if(slot >= MAX_PLAYERS) continue;
			slotTouched[slot] = true;
			StartBoostChainState& state = sChainStates[slot];
			if(DidTriggerBoostPanel(*movement, state)) {
				ApplyStartBoostChainEffects(*movement);
			}

			if(!state.active) {
				state.prevMtFrames = movement->mtBoost;
				continue;
			}

			const bool hadMtBoost = state.prevMtFrames > 0;
			const bool hasMtBoost = movement->mtBoost > 0;
			state.prevMtFrames = movement->mtBoost;

			if(hasMtBoost) {
				state.chainTimer = TWO_SECOND_FRAMES;
				if(!hadMtBoost) {
					movement->ApplyStartBoost(TWO_SECOND_FRAMES);
				}
			}
			else if(state.chainTimer > 0) {
				state.chainTimer--;
			}

			if(state.chainTimer > 0) {
				status->bitfield1 |= OFFROAD_IMMUNITY_FLAG;
				if(movement->offroadInvincibilityFrames < state.chainTimer) {
					movement->offroadInvincibilityFrames = state.chainTimer;
				}
			}
			else {
				ResetChainState(state);
			}
		}
	}

	for(u32 idx = 0; idx < MAX_PLAYERS; ++idx) {
		if(!slotTouched[idx]) {
			ResetChainState(sChainStates[idx]);
		}
	}
}
static RaceFrameHook sStartBoostFrameHook(SustainStartBoostChain);

static void ForceTwoSecondStartBoost(Kart::Movement* movement, s32 startBoostFrames) {
	if(movement == nullptr) return;

	const bool hasSuccessfulStartBoost = startBoostFrames > 0;
	const s32 framesToApply = hasSuccessfulStartBoost ? TWO_SECOND_FRAMES : startBoostFrames;
	movement->ApplyStartBoost(framesToApply);
	if(!hasSuccessfulStartBoost) return;

	ApplyStartBoostChainEffects(*movement);
}

kmRuntimeUse(0x80595ba8);
kmRuntimeUse(0x80595c8c);
} // namespace StartBoost
} // namespace Pulsar

