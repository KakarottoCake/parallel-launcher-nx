#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <switch.h>

// Switch controllers to N64 controllers.
//
// mupen64plus-next does not consume "N64 A" directly; it reads a fixed set of
// libretro joypad slots and maps those to N64 buttons internally. The slot
// assignment below is taken from the core's own input descriptor, so the names
// here describe the N64 function and libretroSlot() does the translation:
//
//     N64 A -> RETRO_DEVICE_ID_JOYPAD_B        N64 Z -> ..._JOYPAD_L2
//     N64 B -> RETRO_DEVICE_ID_JOYPAD_Y        N64 R -> ..._JOYPAD_R2
//     C-Up  -> RETRO_DEVICE_ID_JOYPAD_X        N64 L -> ..._JOYPAD_SELECT
//     C-Down-> RETRO_DEVICE_ID_JOYPAD_A
//     C-Left-> RETRO_DEVICE_ID_JOYPAD_L
//     C-Rght-> RETRO_DEVICE_ID_JOYPAD_R
namespace emu {

enum class N64Button {
    A,
    B,
    CUp,
    CDown,
    CLeft,
    CRight,
    Z,
    L,
    R,
    Start,
    DUp,
    DDown,
    DLeft,
    DRight,
    Count,
};

const char* n64ButtonName(N64Button button);

// The libretro joypad id this N64 function lives on.
unsigned libretroSlot(N64Button button);

// Physical controller shapes we tailor defaults for.
enum class PadKind {
    Unknown,
    ProController,
    JoyConDual,
    Handheld,
    JoyConLeft,  // single, held sideways
    JoyConRight, // single, held sideways
    GameCube,
};

const char* padKindName(PadKind kind);

// One N64 function driven by any of a set of Switch buttons.
struct Binding {
    N64Button button;
    uint64_t  hidMask; // OR of HidNpadButton bits
};

// Default bindings for a controller shape.
const std::vector<Binding>& defaultBindings(PadKind kind);

// A human-readable summary of a mapping, for the Controls screen.
std::vector<std::pair<std::string, std::string>> describeBindings(PadKind kind);

// Reads the style set for a player without touching HID configuration, so the
// launcher UI can report what is plugged in while Borealis still owns input.
PadKind detectConnectedKind(int player = 0);

class InputManager {
  public:
    // Sets up HID for up to `maxPlayers` N64 controllers. GameCube pads are
    // requested alongside the standard styles so a USB adapter just works.
    void init(int maxPlayers);

    // Reads every pad once per frame.
    void poll();

    // libretro's retro_input_state_t, minus the callback plumbing.
    int16_t state(unsigned port, unsigned device, unsigned index, unsigned id) const;

    PadKind kindFor(unsigned port) const;
    bool    connected(unsigned port) const;
    int     playerCount() const { return m_playerCount; }

    // True while the launcher menu combo is held (Minus). The run loop uses
    // this to leave the game and come back to the UI.
    bool menuHeld() const { return m_menuHeld; }

    // Frames the menu combo has been held for, so a deliberate hold can be
    // required rather than a stray press.
    int menuHeldFrames() const { return m_menuHeldFrames; }

    void setDeadzonePercent(int percent) { m_deadzonePercent = percent; }

  private:
    struct PadSlot {
        PadState pad {};
        PadKind  kind      = PadKind::Unknown;
        uint64_t buttons   = 0;
        int16_t  leftX     = 0;
        int16_t  leftY     = 0;
        int16_t  rightX    = 0;
        int16_t  rightY    = 0;
        bool     connected = false;
    };

    // Applies a radial deadzone and rescales so full deflection is still
    // reachable at the edge.
    void applyDeadzone(int16_t& x, int16_t& y) const;

    static PadKind detectKind(const PadState& pad);

    std::vector<PadSlot> m_pads;
    int                  m_playerCount     = 0;
    int                  m_deadzonePercent = 10;
    bool                 m_menuHeld        = false;
    int                  m_menuHeldFrames  = 0;
};

} // namespace emu
