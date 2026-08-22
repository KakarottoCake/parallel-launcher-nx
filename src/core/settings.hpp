#pragma once

#include <cstdint>
#include <string>

#include "src/types.hpp"

// User-facing configuration, persisted as JSON next to the launcher's data on
// the SD card. Every field has a working default so a missing or corrupt
// config file is never fatal.
struct AppSettings {
    // ROM & storage
    std::string baseRomPath = "sdmc:/switch/parallel-launcher/sm64.us.z64";
    std::string romDirectory = "sdmc:/switch/parallel-launcher/roms";
    std::string cacheDirectory = "sdmc:/switch/parallel-launcher/cache";

    // Emulation
    GfxPlugin defaultGraphicsPlugin = GfxPlugin::GLideN64;
    int  upscalingMultiplier        = 2; // 1x=240p, 2x=480p, 3x=720p, 4x=1080p
    bool widescreenHack             = false;
    bool enableFramebufferEmulation = true;
    bool enableCorrectDepthCompare  = true;
    bool enableParallelAntiAliasing = true;

    // OGRE has no Switch build. Hacks that ask for it run on this legacy
    // renderer instead; see emulator::resolveRenderer().
    GfxPlugin ogreSubstitute = GfxPlugin::Rice;

    // Controls
    ControllerType defaultController = ControllerType::ProController;
    int  stickDeadzone = 10; // percent
    bool rumbleEnabled = true;

    // UI & system
    bool darkMode          = true;
    bool showFps           = false;
    bool autoSyncPlaylists = true;

    // Sidebar tab to open on launch: 0 Library, 1 Browse, 2 Account, 3 Settings.
    int lastTab = 0;

    // Account. The auth token is stored, the password never is.
    std::string rhdcUsername;
    std::string rhdcToken;

    static AppSettings& instance();

    // Reads config.json if present. Missing keys keep their defaults.
    void load();

    // Writes config.json immediately. Returns false if the write failed.
    // Never throws: a value that cannot be encoded is replaced rather than
    // allowed to propagate out of a UI callback.
    bool save();

    // Marks the config dirty without touching the SD card. Controls that fire
    // continuously (sliders) use this so a drag does not rewrite the file on
    // every frame.
    void requestSave();

    // Writes a pending save once the debounce window has elapsed. Called once
    // per frame from the main loop.
    void flushPendingSave();

    // True once load() has run, so the UI can avoid saving over a config it
    // never read.
    bool isLoaded() const { return m_loaded; }

  private:
    bool     m_loaded = false;
    bool     m_dirty  = false;
    uint64_t m_dirtySinceMs = 0;
};
