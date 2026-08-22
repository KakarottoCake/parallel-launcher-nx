#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "src/types.hpp"

// A single downloadable revision of a hack. RHDC keeps every published
// version around, and older ones are often the only ones that still work
// under a particular renderer, so the launcher exposes all of them.
struct HackVersion {
    std::string  sha1;         // sha1 of the *patched* rom, used to verify
    std::string  name;         // "v1.2", "Final", ...
    std::string  downloadUrl;  // bps/ips patch, not a rom
    int64_t      releasedAt = 0;
    int64_t      sizeBytes  = 0;
    GfxPlugin    recommendedPlugin = GfxPlugin::UseDefault;
    RhdcHackFlag flags       = RhdcHackFlag::None;
    bool         archived    = false;
};

struct HackInfo {
    // Identity
    std::string id;
    std::string name;
    std::string slug;
    std::string author;
    std::string category;
    std::string description;

    // Catalog metadata
    int    starCount  = 0;   // collectable stars in the hack itself
    int    downloads  = 0;
    double rating     = 0.0; // 0..5
    double difficulty = 0.0; // 0..5
    int    releaseYear = 0;

    // Artwork
    std::string thumbnailUrl;
    std::string screenshotUrl;

    // Versions, newest first
    std::vector<HackVersion> versions;

    // What the author asks for. GfxPlugin::UseDefault means "no preference",
    // in which case the launcher falls back to the user's default.
    GfxPlugin    defaultPlugin = GfxPlugin::UseDefault;
    RhdcHackFlag flags         = RhdcHackFlag::None;

    // Local state
    bool        isInstalled    = false;
    std::string localRomPath;
    std::string installedVersion;
    int64_t     lastPlayed     = 0; // unix seconds, 0 = never
    int64_t     playTime       = 0; // seconds

    // Per-hack override of the renderer. UseDefault defers to defaultPlugin.
    GfxPlugin pluginOverride = GfxPlugin::UseDefault;

    // Renderer actually used at launch, after applying override -> author
    // preference -> global default.
    GfxPlugin resolvePlugin(GfxPlugin globalDefault) const {
        if (pluginOverride != GfxPlugin::UseDefault)
            return pluginOverride;
        if (defaultPlugin != GfxPlugin::UseDefault)
            return defaultPlugin;
        return globalDefault;
    }

    const HackVersion* latestVersion() const {
        for (const auto& v : versions) {
            if (!v.archived)
                return &v;
        }
        return versions.empty() ? nullptr : &versions.front();
    }
};

struct UserProfile {
    bool        isLoggedIn = false;
    std::string username;
    std::string userId;
    std::string role = "User";
    std::string authToken;
    std::string avatarUrl;
    int         starsCollected = 0;
    int         hacksCompleted = 0;
};
