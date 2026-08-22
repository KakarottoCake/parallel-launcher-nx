#include "src/core/installer.hpp"

#include <cstdio>

#include <borealis.hpp>

#include "src/core/checksum.hpp"
#include "src/core/patch.hpp"
#include "src/core/paths.hpp"
#include "src/core/rom.hpp"
#include "src/core/settings.hpp"

namespace installer {

namespace {

bool readFile(const std::string& path, std::vector<uint8_t>& out, std::string& error) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        error = "Could not open " + path;
        return false;
    }

    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);

    if (size < 0) {
        std::fclose(file);
        error = "Could not measure " + path;
        return false;
    }

    out.resize(static_cast<size_t>(size));
    const size_t read = std::fread(out.data(), 1, out.size(), file);
    const bool   bad  = read != out.size() || std::ferror(file) != 0;
    std::fclose(file);

    if (bad) {
        error = "Could not read all of " + path;
        return false;
    }
    return true;
}

bool writeFileAtomically(const std::string& path, const std::vector<uint8_t>& data,
    std::string& error) {
    // Write to a temporary neighbour first so an interrupted write cannot
    // leave a half-patched ROM that looks installed.
    const std::string temporary = path + ".part";

    std::FILE* file = std::fopen(temporary.c_str(), "wb");
    if (!file) {
        error = "Could not create " + temporary;
        return false;
    }

    const size_t written = std::fwrite(data.data(), 1, data.size(), file);
    const bool   bad     = written != data.size() || std::ferror(file) != 0;
    std::fclose(file);

    if (bad) {
        std::remove(temporary.c_str());
        error = "Ran out of space writing " + temporary;
        return false;
    }

    std::remove(path.c_str()); // rename() will not overwrite on FAT32
    if (std::rename(temporary.c_str(), path.c_str()) != 0) {
        std::remove(temporary.c_str());
        error = "Could not move the patched ROM into place";
        return false;
    }

    return true;
}

} // namespace

std::string romPathFor(const HackInfo& hack) {
    return paths::hacks() + "/" + hack.id + ".z64";
}

Outcome installFromMemory(const HackInfo& hack, const std::vector<uint8_t>& patchBytes) {
    Outcome outcome;

    if (!paths::ensureDirectories()) {
        outcome.error = "The launcher's folders on the SD card are not writable.";
        return outcome;
    }

    // Load and normalise the base ROM. A v64/n64 dump is converted in memory;
    // the user's file on disk is never modified.
    const std::string baseRomPath = AppSettings::instance().baseRomPath;

    std::vector<uint8_t> base;
    if (!readFile(baseRomPath, base, outcome.error))
        return outcome;

    rom::BaseRomInfo info = rom::inspectBaseRomBuffer(base);
    if (!info.isUsableBase()) {
        outcome.error = info.error.empty()
            ? "The configured base ROM is not a clean Super Mario 64 (USA) dump."
            : info.error;
        return outcome;
    }

    const patch::Format format = patch::detectFormat(patchBytes.data(), patchBytes.size());
    if (format == patch::Format::Unknown) {
        outcome.error = "That file is not a BPS or IPS patch.";
        return outcome;
    }

    brls::Logger::info("Applying {} patch for {} ({} bytes)", patch::formatName(format),
        hack.name, patchBytes.size());

    const patch::Result result = patch::apply(base, patchBytes);
    if (!result.ok) {
        outcome.error = result.error;
        return outcome;
    }

    outcome.romPath = romPathFor(hack);
    if (!writeFileAtomically(outcome.romPath, result.output, outcome.error))
        return outcome;

    outcome.sha1 = checksum::sha1Hex(result.output);
    outcome.ok   = true;

    brls::Logger::info("Installed {} to {} (sha1 {})", hack.name, outcome.romPath, outcome.sha1);
    return outcome;
}

Outcome installFromFile(const HackInfo& hack, const std::string& patchPath) {
    Outcome outcome;

    std::vector<uint8_t> patchBytes;
    if (!readFile(patchPath, patchBytes, outcome.error))
        return outcome;

    // A patch is tens of kilobytes to a few megabytes; anything far larger is
    // not a patch and should not be fed to the decoder.
    if (patchBytes.size() > 64u * 1024u * 1024u) {
        outcome.error = "That patch file is implausibly large.";
        return outcome;
    }

    return installFromMemory(hack, patchBytes);
}

bool remove(const HackInfo& hack, std::string* error) {
    const std::string path = romPathFor(hack);

    if (std::remove(path.c_str()) != 0 && paths::fileExists(path)) {
        if (error)
            *error = "Could not delete " + path;
        return false;
    }

    brls::Logger::info("Removed {}", path);
    return true;
}

void installFromFileAsync(const HackInfo& hack, const std::string& patchPath,
    std::function<void(Outcome)> onDone) {
    // Copy the hack: the catalog entry can move while the worker runs.
    HackInfo snapshot = hack;

    brls::async([snapshot, patchPath, onDone]() {
        Outcome outcome = installFromFile(snapshot, patchPath);
        brls::sync([outcome = std::move(outcome), onDone]() {
            if (onDone)
                onDone(outcome);
        });
    });
}

} // namespace installer
