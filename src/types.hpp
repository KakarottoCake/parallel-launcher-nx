#pragma once

#include <cstdint>
#include <string>
#include <type_traits>

// ---------------------------------------------------------------------------
// Graphics plugins
// ---------------------------------------------------------------------------
// Values are pinned so they can be written to the on-SD config without ever
// being renumbered by a future insert.
enum class GfxPlugin : uint8_t {
    UseDefault = 0, // "whatever the hack author asked for", resolved at launch
    ParaLLEl   = 1, // Vulkan ParaLLEl-RDP / RSP, pixel accurate, heaviest
    Glide64    = 2, // Legacy Glide64
    Angrylion  = 3, // CPU software RDP, reference accurate, very slow
    GLideN64   = 4, // Modern OpenGL / GLES HLE plugin, broad compatibility
    Rice       = 5, // Legacy Rice
    OGRE       = 7, // Open Glide64 Replacement Engine, SM64-Editor era hacks
};

// The three plugins this launcher actually ships and can auto-select between.
inline constexpr GfxPlugin kSupportedPlugins[] = {
    GfxPlugin::GLideN64,
    GfxPlugin::ParaLLEl,
    GfxPlugin::OGRE,
};

inline const char* getGfxPluginName(GfxPlugin plugin) {
    switch (plugin) {
        case GfxPlugin::ParaLLEl:  return "ParaLLEl-RDP (Vulkan)";
        case GfxPlugin::GLideN64:  return "GLideN64 (OpenGL)";
        case GfxPlugin::OGRE:      return "OGRE (Legacy HLE)";
        case GfxPlugin::Glide64:   return "Glide64 (Legacy HLE)";
        case GfxPlugin::Rice:      return "Rice (Legacy HLE)";
        case GfxPlugin::Angrylion: return "Angrylion (Software)";
        case GfxPlugin::UseDefault:
        default:                   return "Auto (Author's choice)";
    }
}

// Short form used on cards and badges where horizontal space is tight.
inline const char* getGfxPluginShortName(GfxPlugin plugin) {
    switch (plugin) {
        case GfxPlugin::ParaLLEl:  return "ParaLLEl";
        case GfxPlugin::GLideN64:  return "GLideN64";
        case GfxPlugin::OGRE:      return "OGRE";
        case GfxPlugin::Glide64:   return "Glide64";
        case GfxPlugin::Rice:      return "Rice";
        case GfxPlugin::Angrylion: return "Angrylion";
        case GfxPlugin::UseDefault:
        default:                   return "Auto";
    }
}

// One-line rationale shown under the plugin picker in the detail view.
inline const char* getGfxPluginBlurb(GfxPlugin plugin) {
    switch (plugin) {
        case GfxPlugin::ParaLLEl:
            return "Pixel-accurate low-level RDP on Vulkan. Best for hacks that "
                   "rely on framebuffer tricks. Heaviest on battery.";
        case GfxPlugin::GLideN64:
            return "High-level OpenGL renderer. Fast, upscales cleanly, and "
                   "handles the majority of modern SM64 hacks.";
        case GfxPlugin::OGRE:
            return "Glide64 replacement tuned for SM64-Editor era hacks that misbehave "
                   "under newer HLE renderers. No Switch build exists, so these hacks run "
                   "on the stand-in chosen in Settings.";
        case GfxPlugin::Rice:
            return "Legacy HLE renderer from the same era as OGRE. Fast, and often the "
                   "closest match for older SM64-Editor hacks.";
        case GfxPlugin::Glide64:
            return "The renderer OGRE was written to replace. A reasonable stand-in when "
                   "Rice misdraws a hack.";
        case GfxPlugin::Angrylion:
            return "Software RDP. Reference accuracy, far too slow for real-time "
                   "play on Switch hardware.";
        case GfxPlugin::UseDefault:
        default:
            return "Uses whichever renderer the hack's author marked as "
                   "recommended, falling back to your default below.";
    }
}

// ---------------------------------------------------------------------------
// Controllers
// ---------------------------------------------------------------------------
enum class ControllerType : uint8_t {
    ProController  = 0,
    JoyConDual     = 1,
    JoyConHandheld = 2,
    GameCube       = 3,
    JoyConSingle   = 4,
};

inline const char* getControllerTypeName(ControllerType type) {
    switch (type) {
        case ControllerType::ProController:  return "Pro Controller";
        case ControllerType::JoyConDual:     return "Joy-Con (Dual Grip)";
        case ControllerType::JoyConHandheld: return "Handheld Mode";
        case ControllerType::GameCube:       return "GameCube Controller (USB)";
        case ControllerType::JoyConSingle:   return "Joy-Con (Single, sideways)";
        default:                             return "Standard Controller";
    }
}

// ---------------------------------------------------------------------------
// Romhacking.com metadata
// ---------------------------------------------------------------------------
enum class RhdcRating : uint8_t {
    NotRated = 0,
    Disliked = 1,
    Neutral  = 2,
    Liked    = 3,
    Loved    = 4,
};

// Per-version requirement flags advertised by the hack author on RHDC.
// These drive the emulator config the launcher generates.
enum class RhdcHackFlag : uint16_t {
    None         = 0,
    NoOverclock  = 1 << 0, // hack breaks when the VR4300 is overclocked
    DualAnalog   = 1 << 1, // second stick drives the C-buttons
    BigEEPROM    = 1 << 2, // requires the 16 kB EEPROM save format
    SupportsSD   = 1 << 3, // 64DD / SD-card expansion aware
    OverclockVI  = 1 << 4, // wants the video interface overclocked
    ExpansionPak = 1 << 5, // requires the 8 MB RAM expansion
    WidescreenOk = 1 << 6, // author blessed the 16:9 hack
};

inline RhdcHackFlag operator|(RhdcHackFlag a, RhdcHackFlag b) {
    using U = std::underlying_type_t<RhdcHackFlag>;
    return static_cast<RhdcHackFlag>(static_cast<U>(a) | static_cast<U>(b));
}

inline RhdcHackFlag& operator|=(RhdcHackFlag& a, RhdcHackFlag b) {
    a = a | b;
    return a;
}

inline bool hasFlag(RhdcHackFlag set, RhdcHackFlag flag) {
    using U = std::underlying_type_t<RhdcHackFlag>;
    return (static_cast<U>(set) & static_cast<U>(flag)) != 0;
}
