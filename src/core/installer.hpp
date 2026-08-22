#pragma once

#include <functional>
#include <string>
#include <vector>

#include "src/core/models.hpp"

// Turns a downloaded patch plus the user's own base ROM into a playable ROM on
// the SD card. Nothing here distributes copyrighted material: the patch comes
// from the hack author and the base ROM is supplied by the user.
namespace installer {

struct Outcome {
    bool        ok = false;
    std::string romPath;   // where the patched ROM landed
    std::string sha1;      // of the patched ROM
    std::string error;     // human-readable failure reason
};

// Applies `patchBytes` to the configured base ROM and writes the result to
// the hacks directory. Synchronous; call it off the UI thread.
Outcome installFromMemory(const HackInfo& hack, const std::vector<uint8_t>& patchBytes);

// Same, reading the patch from a file on the SD card. Used by the "install a
// patch you already have" flow and by the download path once the file lands
// in the cache directory.
Outcome installFromFile(const HackInfo& hack, const std::string& patchPath);

// Deletes a hack's patched ROM. Save files are deliberately left alone.
bool remove(const HackInfo& hack, std::string* error = nullptr);

// Where a hack's patched ROM lives.
std::string romPathFor(const HackInfo& hack);

// Runs installFromFile on a worker thread and reports back on the UI thread.
void installFromFileAsync(const HackInfo& hack, const std::string& patchPath,
    std::function<void(Outcome)> onDone);

} // namespace installer
