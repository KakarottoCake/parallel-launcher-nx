#pragma once

#include <string>
#include <vector>

#include "src/core/models.hpp"
#include "src/types.hpp"

// Hands a patched ROM off to an emulator core.
//
// This launcher does not emulate anything itself. Like desktop Parallel
// Launcher, it prepares the ROM and a matching renderer configuration and then
// chain-loads a libretro frontend, which libnx supports through
// envSetNextLoad: the frontend starts as this app exits.
namespace emulator {

enum class Availability {
    Available,
    MissingFrontend, // no libretro frontend on the SD card
    MissingCore,     // the core that provides this renderer is not installed
    Unreachable,     // no Switch core provides this renderer at all
};

// Which libretro core implements a renderer, and the core option that selects
// it. Kept as data rather than hard-coded strings because the option keys
// differ between core builds; emulator.json can override any of it without
// rebuilding the launcher.
struct RendererTarget {
    GfxPlugin   plugin      = GfxPlugin::UseDefault;
    std::string coreId;      // "mupen64plus_next" or "parallel_n64"
    std::string coreFile;    // core .nro filename inside the cores directory
    std::string optionKey;   // libretro core option that selects the renderer
    std::string optionValue; // value for that option
};

// Returns the target for a renderer, or nullptr when nothing provides it.
// OGRE is not resolved here: use resolveRenderer() so the substitution is
// visible to the caller.
const RendererTarget* targetFor(GfxPlugin plugin);

// OGRE has no Switch build. Rather than refuse to launch, the author's
// intent - "use a legacy HLE renderer, the modern ones break this hack" - is
// honoured by running the configured stand-in.
struct ResolvedRenderer {
    GfxPlugin requested   = GfxPlugin::UseDefault; // what the hack asked for
    GfxPlugin effective   = GfxPlugin::UseDefault; // what will actually run
    bool      substituted = false;
};

ResolvedRenderer resolveRenderer(const HackInfo& hack);

// Renderers offered as the OGRE stand-in, in the order they appear in
// Settings. All three are legacy HLE plugins in the same era as OGRE.
inline constexpr GfxPlugin kOgreSubstitutes[] = {
    GfxPlugin::Rice,
    GfxPlugin::Glide64,
    GfxPlugin::GLideN64,
};

struct Profile {
    std::string frontendNro = "sdmc:/switch/retroarch/retroarch_switch.nro";
    std::string coresDir    = "sdmc:/switch/retroarch/cores";
    std::string configDir   = "sdmc:/switch/parallel-launcher/emu";

    // Overrides for the built-in renderer table, keyed by plugin id.
    std::vector<RendererTarget> targetOverrides;

    static Profile load();
    void          save() const;

    // Writes a fully populated emulator.json documenting the defaults, so the
    // core option keys can be corrected against a real RetroArch build.
    static bool writeReference();
};

struct LaunchPlan {
    bool             ok = false;
    ResolvedRenderer renderer;
    Availability     availability = Availability::Available;
    std::string      frontendNro;
    std::string      coreId;
    std::string      corePath;
    std::string      romPath;
    std::string      argv;        // exactly what gets handed to envSetNextLoad
    std::string      optionsFile; // core options written for this launch
    std::string      error;

    // One line for the launch dialog, naming the substitution when there is one.
    std::string rendererSummary() const;
};

LaunchPlan plan(const HackInfo& hack);

bool writeCoreOptions(const LaunchPlan& plan, const HackInfo& hack, std::string* error);
bool launch(const LaunchPlan& plan, const HackInfo& hack, std::string* error);

// True when this process was started in a way that allows chain-loading.
bool canChainLoad();

// Writes the frontend/core/renderer situation to the log at startup. This is
// the first thing to look at when a hack refuses to launch, and it is what
// users can be asked to paste when reporting a problem.
void logEnvironment();

} // namespace emulator
