#pragma once

#include <cstdint>
#include <string>
#include <vector>

// BPS and IPS patch application. Pure C++, host-testable.
//
// Romhacking.com distributes hacks as patches rather than ROMs, so this is the
// step that turns the user's own Super Mario 64 dump into a playable hack.
namespace patch {

enum class Format {
    Unknown,
    Bps,
    Ips,
};

Format detectFormat(const uint8_t* data, size_t length);
const char* formatName(Format format);

struct Result {
    bool                 ok = false;
    std::vector<uint8_t> output;
    std::string          error;

    // BPS only: checksums recorded in the patch footer, useful for telling
    // "wrong base ROM" apart from "corrupt download".
    uint32_t expectedSourceCrc = 0;
    uint32_t expectedTargetCrc = 0;
    bool     sourceCrcMatched  = false;
};

// Applies a BPS patch. Verifies the source CRC before patching and the target
// CRC afterwards; a source mismatch is reported rather than silently producing
// a broken ROM.
Result applyBps(const std::vector<uint8_t>& source, const std::vector<uint8_t>& patchData);

// Applies an IPS patch. IPS carries no checksums, so the only validation
// possible is structural.
Result applyIps(const std::vector<uint8_t>& source, const std::vector<uint8_t>& patchData);

// Dispatches on the detected format.
Result apply(const std::vector<uint8_t>& source, const std::vector<uint8_t>& patchData);

} // namespace patch
