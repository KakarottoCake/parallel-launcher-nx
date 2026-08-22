#include "src/core/settings.hpp"

#include <chrono>
#include <fstream>

#include <borealis.hpp>
#include <nlohmann/json.hpp>

#include "src/core/paths.hpp"
#include "src/core/text_util.hpp"

using nlohmann::json;

namespace {

// Reads key into out only when it is present and of the expected type, so a
// hand-edited config with one bad field still loads everything else.
template <typename T>
void readInto(const json& j, const char* key, T& out) {
    auto it = j.find(key);
    if (it == j.end())
        return;

    try {
        out = it->get<T>();
    } catch (const json::exception& e) {
        brls::Logger::warning("config.json: ignoring bad value for '{}' ({})", key, e.what());
    }
}

void readEnum(const json& j, const char* key, GfxPlugin& out) {
    int raw = static_cast<int>(out);
    readInto(j, key, raw);
    switch (static_cast<GfxPlugin>(raw)) {
        case GfxPlugin::UseDefault:
        case GfxPlugin::ParaLLEl:
        case GfxPlugin::Glide64:
        case GfxPlugin::Angrylion:
        case GfxPlugin::GLideN64:
        case GfxPlugin::Rice:
        case GfxPlugin::OGRE:
            out = static_cast<GfxPlugin>(raw);
            break;
        default:
            brls::Logger::warning("config.json: unknown graphics plugin {}, keeping default", raw);
            break;
    }
}

void readEnum(const json& j, const char* key, ControllerType& out) {
    int raw = static_cast<int>(out);
    readInto(j, key, raw);
    if (raw >= 0 && raw <= static_cast<int>(ControllerType::JoyConSingle))
        out = static_cast<ControllerType>(raw);
}

int clampInt(int value, int lo, int hi) {
    return value < lo ? lo : (value > hi ? hi : value);
}

uint64_t nowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

// Sliders fire on every frame of a drag; hold the write back until the value
// has been still for this long.
constexpr uint64_t kSaveDebounceMs = 750;

} // namespace

AppSettings& AppSettings::instance() {
    static AppSettings s_instance;
    return s_instance;
}

void AppSettings::load() {
    m_loaded = true;

    if (!paths::fileExists(paths::kConfig)) {
        brls::Logger::info("No config.json yet, using defaults");
        return;
    }

    std::ifstream in(paths::kConfig);
    if (!in) {
        brls::Logger::error("Could not open {} for reading", paths::kConfig);
        return;
    }

    json j;
    try {
        in >> j;
    } catch (const json::exception& e) {
        brls::Logger::error("config.json is not valid JSON ({}), using defaults", e.what());
        return;
    }

    if (!j.is_object()) {
        brls::Logger::error("config.json is not a JSON object, using defaults");
        return;
    }

    readInto(j, "baseRomPath", baseRomPath);
    readInto(j, "romDirectory", romDirectory);
    readInto(j, "cacheDirectory", cacheDirectory);

    readEnum(j, "defaultGraphicsPlugin", defaultGraphicsPlugin);
    readInto(j, "upscalingMultiplier", upscalingMultiplier);
    readInto(j, "widescreenHack", widescreenHack);
    readInto(j, "enableFramebufferEmulation", enableFramebufferEmulation);
    readInto(j, "enableCorrectDepthCompare", enableCorrectDepthCompare);
    readInto(j, "enableParallelAntiAliasing", enableParallelAntiAliasing);
    readEnum(j, "ogreSubstitute", ogreSubstitute);

    readEnum(j, "defaultController", defaultController);
    readInto(j, "stickDeadzone", stickDeadzone);
    readInto(j, "rumbleEnabled", rumbleEnabled);

    readInto(j, "darkMode", darkMode);
    readInto(j, "showFps", showFps);
    readInto(j, "autoSyncPlaylists", autoSyncPlaylists);
    readInto(j, "lastTab", lastTab);

    readInto(j, "rhdcUsername", rhdcUsername);
    readInto(j, "rhdcToken", rhdcToken);

    baseRomPath    = textutil::sanitizeUserInput(baseRomPath, 512);
    romDirectory   = textutil::sanitizeUserInput(romDirectory, 512);
    cacheDirectory = textutil::sanitizeUserInput(cacheDirectory, 512);
    rhdcUsername   = textutil::sanitizeUserInput(rhdcUsername, 128);
    rhdcToken      = textutil::sanitizeUserInput(rhdcToken, 512);

    upscalingMultiplier = clampInt(upscalingMultiplier, 1, 4);
    stickDeadzone       = clampInt(stickDeadzone, 0, 40);
    lastTab             = clampInt(lastTab, 0, 3);

    if (ogreSubstitute == GfxPlugin::OGRE || ogreSubstitute == GfxPlugin::UseDefault) {
        brls::Logger::warning("config.json: ogreSubstitute cannot be OGRE or Auto, using Rice");
        ogreSubstitute = GfxPlugin::Rice;
    }

    brls::Logger::info("Loaded settings from {}", paths::kConfig);
}

void AppSettings::requestSave() {
    m_dirty        = true;
    m_dirtySinceMs = nowMs();
}

void AppSettings::flushPendingSave() {
    if (!m_dirty)
        return;
    if (nowMs() - m_dirtySinceMs < kSaveDebounceMs)
        return;

    save();
}

bool AppSettings::save() {
    m_dirty = false;

    if (!paths::ensureDirectories()) {
        brls::Logger::error("Cannot save settings: launcher directories are unavailable");
        return false;
    }

    // Every one of these can originate from the software keyboard. Encoding
    // an invalid UTF-8 sequence makes nlohmann throw, so scrub them first and
    // keep the scrubbed value in memory too.
    baseRomPath    = textutil::sanitizeUserInput(baseRomPath, 512);
    romDirectory   = textutil::sanitizeUserInput(romDirectory, 512);
    cacheDirectory = textutil::sanitizeUserInput(cacheDirectory, 512);
    rhdcUsername   = textutil::sanitizeUserInput(rhdcUsername, 128);
    rhdcToken      = textutil::sanitizeUserInput(rhdcToken, 512);

    const json j = {
        { "baseRomPath", baseRomPath },
        { "romDirectory", romDirectory },
        { "cacheDirectory", cacheDirectory },

        { "defaultGraphicsPlugin", static_cast<int>(defaultGraphicsPlugin) },
        { "upscalingMultiplier", upscalingMultiplier },
        { "widescreenHack", widescreenHack },
        { "enableFramebufferEmulation", enableFramebufferEmulation },
        { "enableCorrectDepthCompare", enableCorrectDepthCompare },
        { "enableParallelAntiAliasing", enableParallelAntiAliasing },
        { "ogreSubstitute", static_cast<int>(ogreSubstitute) },

        { "defaultController", static_cast<int>(defaultController) },
        { "stickDeadzone", stickDeadzone },
        { "rumbleEnabled", rumbleEnabled },

        { "darkMode", darkMode },
        { "showFps", showFps },
        { "autoSyncPlaylists", autoSyncPlaylists },
        { "lastTab", lastTab },

        { "rhdcUsername", rhdcUsername },
        { "rhdcToken", rhdcToken },
    };

    std::string encoded;
    try {
        encoded = j.dump(2);
    } catch (const json::exception& e) {
        brls::Logger::error("Could not encode settings as JSON ({}); config not written",
            e.what());
        return false;
    }

    std::ofstream out(paths::kConfig, std::ios::trunc);
    if (!out) {
        brls::Logger::error("Could not open {} for writing", paths::kConfig);
        return false;
    }

    out << encoded << std::endl;
    if (!out) {
        brls::Logger::error("Write to {} failed", paths::kConfig);
        return false;
    }

    brls::Logger::debug("Saved settings to {}", paths::kConfig);
    return true;
}
