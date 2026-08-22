#include "src/core/emulator.hpp"

#include <sys/stat.h>

#include <cstdio>
#include <fstream>

#include <borealis.hpp>
#include <nlohmann/json.hpp>
#include <switch.h>

#include "src/core/paths.hpp"
#include "src/core/settings.hpp"

using nlohmann::json;

namespace emulator {

namespace {

constexpr const char* kCoreMupen = "mupen64plus_next";
constexpr const char* kCoreParallelN64 = "parallel_n64";

// How each renderer is reached on Switch.
//
// mupen64plus-next carries GLideN64 and the ParaLLEl RDP; parallel-n64 carries
// the legacy HLE plugins. These option keys come from the upstream core
// definitions and should be checked against whichever RetroArch build is
// actually installed - emulator.json exists so they can be corrected there.
const RendererTarget kTargets[] = {
    { GfxPlugin::GLideN64,  kCoreMupen, "mupen64plus_next_libretro_libnx.nro",
        "mupen64plus-rdp-plugin", "gliden64" },
    { GfxPlugin::ParaLLEl,  kCoreMupen, "mupen64plus_next_libretro_libnx.nro",
        "mupen64plus-rdp-plugin", "parallel" },
    { GfxPlugin::Angrylion, kCoreMupen, "mupen64plus_next_libretro_libnx.nro",
        "mupen64plus-rdp-plugin", "angrylion" },
    { GfxPlugin::Rice,      kCoreParallelN64, "parallel_n64_libretro_libnx.nro",
        "parallel-n64-gfxplugin", "rice" },
    { GfxPlugin::Glide64,   kCoreParallelN64, "parallel_n64_libretro_libnx.nro",
        "parallel-n64-gfxplugin", "glide64" },
};

std::string profilePath() {
    return std::string(paths::kRoot) + "/emulator.json";
}

// Cached so plan() does not re-read the SD card for every button press.
const Profile& profile() {
    static Profile s_profile = Profile::load();
    return s_profile;
}

const RendererTarget* findOverride(GfxPlugin plugin) {
    for (const RendererTarget& target : profile().targetOverrides) {
        if (target.plugin == plugin)
            return &target;
    }
    return nullptr;
}

bool parseTarget(const json& j, RendererTarget& out) {
    if (!j.is_object() || !j.contains("plugin") || !j["plugin"].is_number_integer())
        return false;

    out.plugin = static_cast<GfxPlugin>(j["plugin"].get<int>());

    if (j.contains("coreId") && j["coreId"].is_string())
        out.coreId = j["coreId"].get<std::string>();
    if (j.contains("coreFile") && j["coreFile"].is_string())
        out.coreFile = j["coreFile"].get<std::string>();
    if (j.contains("optionKey") && j["optionKey"].is_string())
        out.optionKey = j["optionKey"].get<std::string>();
    if (j.contains("optionValue") && j["optionValue"].is_string())
        out.optionValue = j["optionValue"].get<std::string>();

    return !out.coreFile.empty() && !out.optionKey.empty();
}

} // namespace

const RendererTarget* targetFor(GfxPlugin plugin) {
    if (const RendererTarget* override = findOverride(plugin))
        return override;

    for (const RendererTarget& target : kTargets) {
        if (target.plugin == plugin)
            return &target;
    }
    return nullptr;
}

ResolvedRenderer resolveRenderer(const HackInfo& hack) {
    ResolvedRenderer resolved;
    resolved.requested = hack.resolvePlugin(AppSettings::instance().defaultGraphicsPlugin);
    resolved.effective = resolved.requested;

    if (resolved.requested == GfxPlugin::OGRE) {
        // OGRE is a Parallel Launcher plugin, not a libretro core, so there is
        // nothing on Switch that runs it. Hacks pinned to OGRE are pinned
        // because modern HLE renderers break them, so stand in another
        // legacy plugin rather than refusing to boot.
        resolved.effective   = AppSettings::instance().ogreSubstitute;
        resolved.substituted = true;
    }

    return resolved;
}

std::string LaunchPlan::rendererSummary() const {
    if (!renderer.substituted)
        return getGfxPluginName(renderer.effective);

    return std::string(getGfxPluginName(renderer.effective)) + "  (standing in for "
        + getGfxPluginShortName(renderer.requested) + ")";
}

Profile Profile::load() {
    Profile result;

    std::ifstream in(profilePath());
    if (!in)
        return result;

    json j;
    try {
        in >> j;
    } catch (const json::exception& e) {
        brls::Logger::warning("emulator.json is not valid JSON ({}), using defaults", e.what());
        return result;
    }

    if (!j.is_object())
        return result;

    if (j.contains("frontendNro") && j["frontendNro"].is_string())
        result.frontendNro = j["frontendNro"].get<std::string>();
    if (j.contains("coresDir") && j["coresDir"].is_string())
        result.coresDir = j["coresDir"].get<std::string>();
    if (j.contains("configDir") && j["configDir"].is_string())
        result.configDir = j["configDir"].get<std::string>();

    if (j.contains("renderers") && j["renderers"].is_array()) {
        for (const json& entry : j["renderers"]) {
            RendererTarget target;
            if (parseTarget(entry, target))
                result.targetOverrides.push_back(target);
            else
                brls::Logger::warning("emulator.json: skipping a malformed renderer entry");
        }
    }

    brls::Logger::info("Loaded emulator profile ({} renderer overrides)",
        result.targetOverrides.size());
    return result;
}

void Profile::save() const {
    json renderers = json::array();
    for (const RendererTarget& target : targetOverrides) {
        renderers.push_back({
            { "plugin", static_cast<int>(target.plugin) },
            { "coreId", target.coreId },
            { "coreFile", target.coreFile },
            { "optionKey", target.optionKey },
            { "optionValue", target.optionValue },
        });
    }

    const json j = {
        { "frontendNro", frontendNro },
        { "coresDir", coresDir },
        { "configDir", configDir },
        { "renderers", renderers },
    };

    std::ofstream out(profilePath(), std::ios::trunc);
    if (out)
        out << j.dump(2) << std::endl;
}

bool Profile::writeReference() {
    Profile reference;
    for (const RendererTarget& target : kTargets)
        reference.targetOverrides.push_back(target);

    reference.save();
    return paths::fileExists(profilePath());
}

bool canChainLoad() {
    // hbloader fills in the next-load slot only when it launched us itself.
    return envHasNextLoad();
}

LaunchPlan plan(const HackInfo& hack) {
    LaunchPlan result;

    result.frontendNro = profile().frontendNro;
    result.renderer    = resolveRenderer(hack);

    const RendererTarget* target = targetFor(result.renderer.effective);
    if (!target) {
        result.availability = Availability::Unreachable;
        result.error = std::string("No Switch core provides ")
            + getGfxPluginName(result.renderer.effective) + ".";
        if (result.renderer.substituted) {
            result.error += "\n\nIt is configured as the stand-in for OGRE; pick a different "
                            "one under Settings > Graphics.";
        }
        return result;
    }

    result.coreId   = target->coreId;
    result.corePath = profile().coresDir + "/" + target->coreFile;
    result.romPath  = hack.localRomPath;

    if (result.romPath.empty() || !paths::fileExists(result.romPath)) {
        result.error = "This hack is not installed yet.";
        return result;
    }

    if (!paths::fileExists(result.frontendNro)) {
        result.availability = Availability::MissingFrontend;
        result.error = "No libretro frontend at\n" + result.frontendNro
            + "\n\nInstall RetroArch, or point the launcher at your build by editing\n"
            + profilePath();
        return result;
    }

    if (!paths::fileExists(result.corePath)) {
        result.availability = Availability::MissingCore;
        result.error = std::string("The core that provides ")
            + getGfxPluginName(result.renderer.effective) + " is missing:\n" + result.corePath;
        if (result.renderer.substituted) {
            result.error += "\n\nThis renderer is standing in for OGRE. Another stand-in can be "
                            "chosen under Settings > Graphics.";
        }
        return result;
    }

    result.optionsFile = profile().configDir + "/retroarch-core-options.cfg";

    // libnx takes one flat command line rather than an argv array.
    result.argv = result.frontendNro + " -L " + result.corePath + " " + result.romPath;

    result.ok = true;
    return result;
}

bool writeCoreOptions(const LaunchPlan& launchPlan, const HackInfo& hack, std::string* error) {
    const RendererTarget* target = targetFor(launchPlan.renderer.effective);
    if (!target) {
        if (error)
            *error = "No renderer selected.";
        return false;
    }

    ::mkdir(profile().configDir.c_str(), 0777);

    std::ofstream out(launchPlan.optionsFile, std::ios::trunc);
    if (!out) {
        if (error)
            *error = "Could not write " + launchPlan.optionsFile;
        return false;
    }

    const AppSettings& settings = AppSettings::instance();
    const int  width  = 320 * settings.upscalingMultiplier;
    const int  height = 240 * settings.upscalingMultiplier;
    const bool wide   = settings.widescreenHack;

    out << "# Written by ParaLLEl Launcher NX for " << hack.name << "\n";
    if (launchPlan.renderer.substituted) {
        out << "# " << getGfxPluginShortName(launchPlan.renderer.requested)
            << " has no Switch build; using "
            << getGfxPluginShortName(launchPlan.renderer.effective) << " instead\n";
    }
    out << target->optionKey << " = \"" << target->optionValue << "\"\n";

    // The two cores use different option namespaces, so only write the keys
    // that belong to the core actually being launched.
    if (launchPlan.coreId == kCoreParallelN64) {
        out << "parallel-n64-screensize = \"" << width << "x" << height << "\"\n";
        out << "parallel-n64-aspectratiohint = \"" << (wide ? "widescreen" : "normal") << "\"\n";
        if (hasFlag(hack.flags, RhdcHackFlag::NoOverclock))
            out << "parallel-n64-cpucore = \"pure_interpreter\"\n";
    } else {
        out << "mupen64plus-43screensize = \"" << width << "x" << height << "\"\n";
        out << "mupen64plus-aspect = \"" << (wide ? "16:9 adjusted" : "4:3") << "\"\n";
        out << "mupen64plus-EnableFBEmulation = \""
            << (settings.enableFramebufferEmulation ? "True" : "False") << "\"\n";
        if (hasFlag(hack.flags, RhdcHackFlag::NoOverclock))
            out << "mupen64plus-cpucore = \"pure_interpreter\"\n";
        if (hasFlag(hack.flags, RhdcHackFlag::BigEEPROM))
            out << "mupen64plus-savetype = \"eeprom 16kb\"\n";
    }

    out.flush();
    if (!out) {
        if (error)
            *error = "Write to " + launchPlan.optionsFile + " failed.";
        return false;
    }

    brls::Logger::info("Core options for {}: {} = {} (core {})", hack.name, target->optionKey,
        target->optionValue, launchPlan.coreId);
    return true;
}

void logEnvironment() {
    const Profile& p = profile();

    brls::Logger::info("Emulator: chain-loading {}", canChainLoad() ? "available"
                                                                   : "UNAVAILABLE (applet mode)");
    brls::Logger::info("Emulator: frontend {} [{}]", p.frontendNro,
        paths::fileExists(p.frontendNro) ? "found" : "missing");
    brls::Logger::info("Emulator: cores dir {}", p.coresDir);

    for (GfxPlugin plugin : { GfxPlugin::GLideN64, GfxPlugin::ParaLLEl, GfxPlugin::Rice,
             GfxPlugin::Glide64, GfxPlugin::Angrylion }) {
        const RendererTarget* target = targetFor(plugin);
        if (!target) {
            brls::Logger::info("Emulator:   {:<10} no core", getGfxPluginShortName(plugin));
            continue;
        }

        const std::string path = p.coresDir + "/" + target->coreFile;
        brls::Logger::info("Emulator:   {:<10} {} = \"{}\" via {} [{}]",
            getGfxPluginShortName(plugin), target->optionKey, target->optionValue,
            target->coreFile, paths::fileExists(path) ? "found" : "missing");
    }

    const GfxPlugin substitute = AppSettings::instance().ogreSubstitute;
    brls::Logger::info("Emulator:   OGRE has no Switch build, standing in {}",
        getGfxPluginName(substitute));
}

bool launch(const LaunchPlan& launchPlan, const HackInfo& hack, std::string* error) {
    if (!launchPlan.ok) {
        if (error)
            *error = launchPlan.error;
        return false;
    }

    if (!canChainLoad()) {
        if (error) {
            *error = "This build cannot hand off to another application. Launch the launcher "
                     "from the homebrew menu rather than as an applet.";
        }
        return false;
    }

    if (!writeCoreOptions(launchPlan, hack, error))
        return false;

    AppSettings::instance().save();

    const Result rc = envSetNextLoad(launchPlan.frontendNro.c_str(), launchPlan.argv.c_str());
    if (R_FAILED(rc)) {
        if (error)
            *error = "The system refused the handoff (error " + std::to_string(rc) + ").";
        return false;
    }

    brls::Logger::info("Chain-loading {} with argv: {}", launchPlan.frontendNro, launchPlan.argv);
    return true;
}

} // namespace emulator
