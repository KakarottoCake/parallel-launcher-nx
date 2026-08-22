#pragma once

#include <cstdint>
#include <string>
#include <vector>

// N64 ROM identification and byte-order normalisation.
// Pure C++: no Borealis, no libnx, so it can be unit tested on a host.
namespace rom {

// N64 dumps circulate in three byte orders, distinguished by the first four
// bytes of the header.
enum class ByteOrder {
    Unknown,
    Z64, // big endian,    80 37 12 40  - what every emulator and patch expects
    V64, // byte-swapped,  37 80 40 12
    N64, // little endian, 40 12 37 80
};

const char* byteOrderName(ByteOrder order);

// Reads the byte order from a ROM header. Needs at least 4 bytes.
ByteOrder detectByteOrder(const uint8_t* data, size_t length);

// Rewrites data in place into Z64 order. No-op when it already is.
// Returns false if the order could not be determined.
bool convertToZ64(std::vector<uint8_t>& data);

// Which official Super Mario 64 release a dump corresponds to.
enum class Sm64Region {
    Unknown,
    UsaNtsc,
    JapanNtsc,
    EuropePal,
    Shindou,
};

const char* regionName(Sm64Region region);

struct BaseRomInfo {
    bool        readable   = false; // the file could be opened and read
    bool        isZ64      = false; // already in the byte order patches expect
    ByteOrder   byteOrder  = ByteOrder::Unknown;
    Sm64Region  region     = Sm64Region::Unknown;
    int64_t     sizeBytes  = 0;
    std::string sha1;               // of the file as it sits on disk
    std::string z64Sha1;            // of the Z64-normalised contents
    std::string error;              // human-readable reason when unusable

    // Only a clean USA NTSC dump can serve as a patch base: essentially every
    // SM64 hack patch is generated against it.
    bool isUsableBase() const { return region == Sm64Region::UsaNtsc; }
};

// Known-good SHA-1 digests of the Z64-normalised retail dumps.
Sm64Region regionForZ64Sha1(const std::string& sha1);
std::string expectedSha1(Sm64Region region);

// Loads a file, normalises byte order in memory, and identifies it.
// Never throws; failures land in BaseRomInfo::error.
BaseRomInfo inspectBaseRom(const std::string& path);

// Same, for a buffer already in memory. `data` is normalised in place.
BaseRomInfo inspectBaseRomBuffer(std::vector<uint8_t>& data);

// Expected size of a retail SM64 cartridge dump.
constexpr int64_t kSm64RomSize = 8 * 1024 * 1024;

} // namespace rom
