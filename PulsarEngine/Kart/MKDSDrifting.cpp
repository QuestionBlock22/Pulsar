#include <kamek.hpp>
#include <MarioKartWii/Kart/KartMovement.hpp>
#include <MarioKartWii/Kart/KartLink.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <MarioKartWii/Input/ControllerHolder.hpp>
#include <MarioKartWii/Input/InputState.hpp>
#include <PulsarSystem.hpp>

using namespace Kart;
using namespace Input;

// Inject at Movement ctor: zero some fields so they have a predictable initial value.
static void InitDriftFieldsForMovement(Movement& mov) {
    // unknown_0x254 is a 4 byte array at offset 0x254; zero first halfword to mimic sth r30,0x254(r31)
    mov.unknown_0x254[0] = 0;
    mov.unknown_0x254[1] = 0;

    // unknown_0xca is at 0xca and is used as an opposite-stick flag in our patches.
    mov.unknown_0xca[0] = 0;
}

// Replacement for UpdateMTCharge at 0x8057ee50.
// Implements the threshold-based logic described in the assembly snippet the user provided.
static int UpdateMTCharge_Patched(Movement& movement) {
    // if SMT is already charged (driftState >= 3) exit immediately
    if (movement.driftState >= 3) return 0;

    // exit if this player is not local (player is remote)
    // Movement inherits Link, so IsLocal() applies
    if (!movement.IsLocal()) return 0;

    // Threshold is defined at address 0x808b5ccc in the original snippet
    const float& STICK_CHARGE_THRESHOLD = *(const float*)0x808b5ccc;

    // read the stick X from the controller for this player
    // Movement inherits Link, so use the Link methods
    ControllerHolder& ctrl = movement.GetControllerHolder();
    float stickX = ctrl.inputStates[0].stick.x;

    // hopStickX (direction of the hop) - -1 means left, 1 means right
    int hopStickX = movement.hopStickX;

    // the negative threshold
    const float negThreshold = -STICK_CHARGE_THRESHOLD;

    // When hopStickX == -1 we prefer left checks; otherwise prefer right
    if (hopStickX == -1) {
        // If the stick is left enough, update drift state
        if (stickX < negThreshold) {
            goto calcDriftState; 
        }

        // If the stick is pressed on the opposite side (right) then mark opposite flag
        if (stickX > STICK_CHARGE_THRESHOLD) {
            movement.unknown_0xca[0] = 1;
            return 0;
        }

        // otherwise do nothing and return
        return 0;
    }

    // default (hopStickX != -1) - prefer right
    if (stickX > STICK_CHARGE_THRESHOLD) {
        goto calcDriftState;
    }

    if (stickX < negThreshold) {
        movement.unknown_0xca[0] = 1;
        return 0;
    }

    return 0;

calcDriftState:
    // only proceed if previously the stick was in the opposite threshold
    if (movement.unknown_0xca[0] == 0) return 0;

    // increment the drift state counter
    movement.driftState = movement.driftState + 1;

    // clear the opposite-stick flag
    movement.unknown_0xca[0] = 0;

    return 0;
}

// Hook in the constructor of Movement so the fields are predictable on creation
kmCall(0x80578088, InitDriftFieldsForMovement);

// Replace UpdateMTCharge with our patched implementation
kmBranch(0x8057ee50, UpdateMTCharge_Patched);
