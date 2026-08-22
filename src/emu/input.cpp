#include "src/emu/input.hpp"

#include <algorithm>
#include <cmath>

#include <borealis.hpp>
#include <libretro.h>

namespace emu {

namespace {

// Switch analog sticks report roughly +-32767 with Y positive upwards;
// libretro expects Y positive downwards.
constexpr int16_t kStickMax = 32767;

int16_t clampToStick(float value) {
    return static_cast<int16_t>(std::max(-32767.0f, std::min(32767.0f, value)));
}

// ---------------------------------------------------------------------------
// Default bindings
// ---------------------------------------------------------------------------

// Pro Controller, Joy-Con in a grip, and handheld all share the same physical
// layout, so they share one mapping.
//
// Face buttons are A/B rather than the C diamond: N64 A is the jump button and
// wants to be under the thumb's resting position. The C buttons live on the
// right stick, which is what every SM64 player expects from a modern pad, with
// X and Y giving digital C-Up/C-Down for camera zoom.
//
// Z is on ZL *and* L: ZL matches Nintendo's own N64 app, but a lot of SM64
// players ground-pound with L, and binding both costs nothing.
const std::vector<Binding> kStandardBindings = {
    { N64Button::A,      HidNpadButton_A },
    { N64Button::B,      HidNpadButton_B },
    { N64Button::CUp,    HidNpadButton_X | HidNpadButton_StickRUp },
    { N64Button::CDown,  HidNpadButton_Y | HidNpadButton_StickRDown },
    { N64Button::CLeft,  HidNpadButton_StickRLeft },
    { N64Button::CRight, HidNpadButton_StickRRight },
    { N64Button::Z,      HidNpadButton_ZL | HidNpadButton_L },
    { N64Button::R,      HidNpadButton_ZR | HidNpadButton_R },
    { N64Button::L,      HidNpadButton_StickL },
    { N64Button::Start,  HidNpadButton_Plus },
    { N64Button::DUp,    HidNpadButton_Up },
    { N64Button::DDown,  HidNpadButton_Down },
    { N64Button::DLeft,  HidNpadButton_Left },
    { N64Button::DRight, HidNpadButton_Right },
};

// A GameCube pad is very nearly an N64 pad: the C-stick is the C buttons and
// Z is already in the right place, so this is close to one-to-one. It is the
// best way to play SM64 hacks on a Switch and deserves an exact mapping.
//
// libnx reports GC triggers through the ZL/ZR bits once they pass the click
// point, and the analog L/R travel through the right stick axes.
const std::vector<Binding> kGameCubeBindings = {
    { N64Button::A,      HidNpadButton_A },
    { N64Button::B,      HidNpadButton_B },
    { N64Button::CUp,    HidNpadButton_StickRUp },
    { N64Button::CDown,  HidNpadButton_StickRDown },
    { N64Button::CLeft,  HidNpadButton_StickRLeft },
    { N64Button::CRight, HidNpadButton_StickRRight },
    // GC Z sits under the right index finger, exactly like N64 Z.
    { N64Button::Z,      HidNpadButton_R },
    { N64Button::L,      HidNpadButton_ZL },
    { N64Button::R,      HidNpadButton_ZR },
    { N64Button::Start,  HidNpadButton_Plus },
    { N64Button::DUp,    HidNpadButton_Up },
    { N64Button::DDown,  HidNpadButton_Down },
    { N64Button::DLeft,  HidNpadButton_Left },
    { N64Button::DRight, HidNpadButton_Right },
};

// A single Joy-Con held sideways has one stick, four face buttons and the two
// rail buttons. There is no room for a full N64 pad, so this keeps the
// essentials playable: jump, punch, Z and Start. C buttons are unreachable,
// which is worth telling the user rather than pretending otherwise.
const std::vector<Binding> kJoyConLeftBindings = {
    { N64Button::A,      HidNpadButton_Down },  // sideways: bottom of the diamond
    { N64Button::B,      HidNpadButton_Left },
    { N64Button::CUp,    HidNpadButton_Up },
    { N64Button::CDown,  HidNpadButton_Right },
    { N64Button::Z,      HidNpadButton_LeftSL },
    { N64Button::R,      HidNpadButton_LeftSR },
    { N64Button::Start,  HidNpadButton_Minus },
};

const std::vector<Binding> kJoyConRightBindings = {
    { N64Button::A,      HidNpadButton_B },
    { N64Button::B,      HidNpadButton_Y },
    { N64Button::CUp,    HidNpadButton_X },
    { N64Button::CDown,  HidNpadButton_A },
    { N64Button::Z,      HidNpadButton_RightSL },
    { N64Button::R,      HidNpadButton_RightSR },
    { N64Button::Start,  HidNpadButton_Plus },
};

} // namespace

const char* n64ButtonName(N64Button button) {
    switch (button) {
        case N64Button::A:      return "A";
        case N64Button::B:      return "B";
        case N64Button::CUp:    return "C-Up";
        case N64Button::CDown:  return "C-Down";
        case N64Button::CLeft:  return "C-Left";
        case N64Button::CRight: return "C-Right";
        case N64Button::Z:      return "Z";
        case N64Button::L:      return "L";
        case N64Button::R:      return "R";
        case N64Button::Start:  return "Start";
        case N64Button::DUp:    return "D-Pad Up";
        case N64Button::DDown:  return "D-Pad Down";
        case N64Button::DLeft:  return "D-Pad Left";
        case N64Button::DRight: return "D-Pad Right";
        default:                return "?";
    }
}

unsigned libretroSlot(N64Button button) {
    switch (button) {
        case N64Button::A:      return RETRO_DEVICE_ID_JOYPAD_B;
        case N64Button::B:      return RETRO_DEVICE_ID_JOYPAD_Y;
        case N64Button::CUp:    return RETRO_DEVICE_ID_JOYPAD_X;
        case N64Button::CDown:  return RETRO_DEVICE_ID_JOYPAD_A;
        case N64Button::CLeft:  return RETRO_DEVICE_ID_JOYPAD_L;
        case N64Button::CRight: return RETRO_DEVICE_ID_JOYPAD_R;
        case N64Button::Z:      return RETRO_DEVICE_ID_JOYPAD_L2;
        case N64Button::L:      return RETRO_DEVICE_ID_JOYPAD_SELECT;
        case N64Button::R:      return RETRO_DEVICE_ID_JOYPAD_R2;
        case N64Button::Start:  return RETRO_DEVICE_ID_JOYPAD_START;
        case N64Button::DUp:    return RETRO_DEVICE_ID_JOYPAD_UP;
        case N64Button::DDown:  return RETRO_DEVICE_ID_JOYPAD_DOWN;
        case N64Button::DLeft:  return RETRO_DEVICE_ID_JOYPAD_LEFT;
        case N64Button::DRight: return RETRO_DEVICE_ID_JOYPAD_RIGHT;
        default:                return RETRO_DEVICE_ID_JOYPAD_B;
    }
}

const char* padKindName(PadKind kind) {
    switch (kind) {
        case PadKind::ProController: return "Pro Controller";
        case PadKind::JoyConDual:    return "Joy-Con (dual)";
        case PadKind::Handheld:      return "Handheld";
        case PadKind::JoyConLeft:    return "Joy-Con (left, sideways)";
        case PadKind::JoyConRight:   return "Joy-Con (right, sideways)";
        case PadKind::GameCube:      return "GameCube controller";
        default:                     return "No controller";
    }
}

const std::vector<Binding>& defaultBindings(PadKind kind) {
    switch (kind) {
        case PadKind::GameCube:    return kGameCubeBindings;
        case PadKind::JoyConLeft:  return kJoyConLeftBindings;
        case PadKind::JoyConRight: return kJoyConRightBindings;
        default:                   return kStandardBindings;
    }
}

std::vector<std::pair<std::string, std::string>> describeBindings(PadKind kind) {
    struct NameBit {
        uint64_t    bit;
        const char* name;
    };

    static const NameBit kNames[] = {
        { HidNpadButton_A, "A" },
        { HidNpadButton_B, "B" },
        { HidNpadButton_X, "X" },
        { HidNpadButton_Y, "Y" },
        { HidNpadButton_L, "L" },
        { HidNpadButton_R, "R" },
        { HidNpadButton_ZL, "ZL" },
        { HidNpadButton_ZR, "ZR" },
        { HidNpadButton_Plus, "+" },
        { HidNpadButton_Minus, "-" },
        { HidNpadButton_Up, "D-Up" },
        { HidNpadButton_Down, "D-Down" },
        { HidNpadButton_Left, "D-Left" },
        { HidNpadButton_Right, "D-Right" },
        { HidNpadButton_StickL, "Left stick click" },
        { HidNpadButton_StickR, "Right stick click" },
        { HidNpadButton_StickRUp, "Right stick up" },
        { HidNpadButton_StickRDown, "Right stick down" },
        { HidNpadButton_StickRLeft, "Right stick left" },
        { HidNpadButton_StickRRight, "Right stick right" },
        { HidNpadButton_LeftSL, "SL" },
        { HidNpadButton_LeftSR, "SR" },
        { HidNpadButton_RightSL, "SL" },
        { HidNpadButton_RightSR, "SR" },
    };

    std::vector<std::pair<std::string, std::string>> rows;
    rows.emplace_back("Control stick", "Left stick");

    for (const Binding& binding : defaultBindings(kind)) {
        std::string sources;
        for (const NameBit& name : kNames) {
            if ((binding.hidMask & name.bit) == 0)
                continue;
            if (!sources.empty())
                sources += " or ";
            sources += name.name;
        }
        if (sources.empty())
            continue;

        rows.emplace_back(n64ButtonName(binding.button), sources);
    }

    return rows;
}

PadKind detectConnectedKind(int player) {
    const HidNpadIdType id = player == 0
        ? HidNpadIdType_Handheld
        : static_cast<HidNpadIdType>(HidNpadIdType_No1 + player);

    // In handheld mode player 1 reports on the handheld id; docked it reports
    // on No1. Check both before giving up.
    u32 style = hidGetNpadStyleSet(id);
    if (style == 0 && player == 0)
        style = hidGetNpadStyleSet(HidNpadIdType_No1);

    if (style & HidNpadStyleTag_NpadGc)
        return PadKind::GameCube;
    if (style & HidNpadStyleTag_NpadFullKey)
        return PadKind::ProController;
    if (style & HidNpadStyleTag_NpadHandheld)
        return PadKind::Handheld;
    if (style & HidNpadStyleTag_NpadJoyDual)
        return PadKind::JoyConDual;
    if (style & HidNpadStyleTag_NpadJoyLeft)
        return PadKind::JoyConLeft;
    if (style & HidNpadStyleTag_NpadJoyRight)
        return PadKind::JoyConRight;

    return PadKind::Unknown;
}

// ---------------------------------------------------------------------------
// InputManager
// ---------------------------------------------------------------------------

void InputManager::init(int maxPlayers) {
    m_playerCount = std::max(1, std::min(maxPlayers, 4));

    // Request the GameCube style alongside the standard ones so an official
    // USB adapter shows up as a normal pad rather than being ignored.
    padConfigureInput(static_cast<u32>(m_playerCount),
        HidNpadStyleSet_NpadStandard | HidNpadStyleTag_NpadGc);

    m_pads.assign(static_cast<size_t>(m_playerCount), PadSlot {});

    for (int i = 0; i < m_playerCount; ++i) {
        // Player 1 also accepts handheld mode, which is a separate npad id.
        const u64 mask = (i == 0)
            ? (1UL << HidNpadIdType_No1) | (1UL << HidNpadIdType_Handheld)
            : (1UL << static_cast<unsigned>(HidNpadIdType_No1 + i));
        padInitializeWithMask(&m_pads[static_cast<size_t>(i)].pad, mask);
    }

    brls::Logger::info("Input: configured {} player(s), GameCube style enabled", m_playerCount);
}

PadKind InputManager::detectKind(const PadState& pad) {
    const u32 style = padGetStyleSet(const_cast<PadState*>(&pad));

    if (style & HidNpadStyleTag_NpadGc)
        return PadKind::GameCube;
    if (style & HidNpadStyleTag_NpadFullKey)
        return PadKind::ProController;
    if (style & HidNpadStyleTag_NpadHandheld)
        return PadKind::Handheld;
    if (style & HidNpadStyleTag_NpadJoyDual)
        return PadKind::JoyConDual;
    if (style & HidNpadStyleTag_NpadJoyLeft)
        return PadKind::JoyConLeft;
    if (style & HidNpadStyleTag_NpadJoyRight)
        return PadKind::JoyConRight;

    return PadKind::Unknown;
}

void InputManager::applyDeadzone(int16_t& x, int16_t& y) const {
    const float deadzone = static_cast<float>(m_deadzonePercent) / 100.0f;
    if (deadzone <= 0.0f)
        return;

    const float fx        = static_cast<float>(x) / kStickMax;
    const float fy        = static_cast<float>(y) / kStickMax;
    const float magnitude = std::sqrt(fx * fx + fy * fy);

    if (magnitude <= deadzone) {
        x = 0;
        y = 0;
        return;
    }

    // Rescale the remaining travel back over the full range so the edge of the
    // stick still produces full deflection.
    const float scaled = std::min(1.0f, (magnitude - deadzone) / (1.0f - deadzone));
    const float factor = scaled / magnitude;

    x = clampToStick(fx * factor * kStickMax);
    y = clampToStick(fy * factor * kStickMax);
}

void InputManager::poll() {
    bool anyMenu = false;

    for (size_t i = 0; i < m_pads.size(); ++i) {
        PadSlot& slot = m_pads[i];

        padUpdate(&slot.pad);

        slot.connected = padIsConnected(&slot.pad);
        slot.kind      = slot.connected ? detectKind(slot.pad) : PadKind::Unknown;
        slot.buttons   = slot.connected ? padGetButtons(&slot.pad) : 0;

        if (slot.connected) {
            const HidAnalogStickState left  = padGetStickPos(&slot.pad, 0);
            const HidAnalogStickState right = padGetStickPos(&slot.pad, 1);

            slot.leftX = static_cast<int16_t>(left.x);
            slot.leftY = static_cast<int16_t>(-left.y); // libretro Y grows downwards
            applyDeadzone(slot.leftX, slot.leftY);

            slot.rightX = static_cast<int16_t>(right.x);
            slot.rightY = static_cast<int16_t>(-right.y);
            applyDeadzone(slot.rightX, slot.rightY);
        } else {
            slot.leftX = slot.leftY = slot.rightX = slot.rightY = 0;
        }

        // Minus is the way back to the launcher on a full pad. A single
        // Joy-Con needs Minus for Start, so it uses the stick click instead.
        const bool sideways = slot.kind == PadKind::JoyConLeft || slot.kind == PadKind::JoyConRight;
        const uint64_t menuMask = sideways ? HidNpadButton_StickL : HidNpadButton_Minus;
        if (slot.buttons & menuMask)
            anyMenu = true;
    }

    m_menuHeld = anyMenu;
    m_menuHeldFrames = anyMenu ? m_menuHeldFrames + 1 : 0;
}

bool InputManager::connected(unsigned port) const {
    return port < m_pads.size() && m_pads[port].connected;
}

PadKind InputManager::kindFor(unsigned port) const {
    return port < m_pads.size() ? m_pads[port].kind : PadKind::Unknown;
}

int16_t InputManager::state(unsigned port, unsigned device, unsigned index, unsigned id) const {
    if (port >= m_pads.size())
        return 0;

    const PadSlot& slot = m_pads[port];
    if (!slot.connected)
        return 0;

    if (device == RETRO_DEVICE_ANALOG) {
        if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT)
            return id == RETRO_DEVICE_ID_ANALOG_X ? slot.leftX : slot.leftY;
        if (index == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
            return id == RETRO_DEVICE_ID_ANALOG_X ? slot.rightX : slot.rightY;
        return 0;
    }

    if (device != RETRO_DEVICE_JOYPAD)
        return 0;

    const std::vector<Binding>& bindings = defaultBindings(slot.kind);

    // RETRO_DEVICE_ID_JOYPAD_MASK asks for every button at once as a bitfield.
    if (id == RETRO_DEVICE_ID_JOYPAD_MASK) {
        int16_t mask = 0;
        for (const Binding& binding : bindings) {
            if (slot.buttons & binding.hidMask)
                mask |= static_cast<int16_t>(1 << libretroSlot(binding.button));
        }
        return mask;
    }

    for (const Binding& binding : bindings) {
        if (libretroSlot(binding.button) != id)
            continue;
        if (slot.buttons & binding.hidMask)
            return 1;
    }

    return 0;
}

} // namespace emu
