#include "src/core/rom.hpp"

#include <cstdio>

#include "src/core/checksum.hpp"

namespace rom {

namespace {

struct KnownRom {
    Sm64Region  region;
    const char* sha1;
};

// SHA-1 of each retail dump after normalising to Z64 byte order. These match
// the baserom hashes used by the Super Mario 64 decompilation project, which
// is also what hack authors patch against.
const KnownRom kKnownRoms[] = {
    { Sm64Region::UsaNtsc,   "9bef1128717f958171a4afac3ed78ee2bb4e86ce" },
    { Sm64Region::JapanNtsc, "8a20a45e36a5c71a33350e937b7702046dd9ce82" },
    { Sm64Region::EuropePal, "4ac5721683d0e0b6bbb561b58a71740845dceea9" },
    { Sm64Region::Shindou,   "3f319ae697533a255a1003d09202379d78d5a2e0" },
};

void swap16(std::vector<uint8_t>& data) {
    for (size_t i = 0; i + 1 < data.size(); i += 2)
        std::swap(data[i], data[i + 1]);
}

void swap32(std::vector<uint8_t>& data) {
    for (size_t i = 0; i + 3 < data.size(); i += 4) {
        std::swap(data[i], data[i + 3]);
        std::swap(data[i + 1], data[i + 2]);
    }
}

} // namespace

const char* byteOrderName(ByteOrder order) {
    switch (order) {
        case ByteOrder::Z64: return "z64 (big endian)";
        case ByteOrder::V64: return "v64 (byte swapped)";
        case ByteOrder::N64: return "n64 (little endian)";
        default:             return "unrecognised";
    }
}

const char* regionName(Sm64Region region) {
    switch (region) {
        case Sm64Region::UsaNtsc:   return "Super Mario 64 (USA)";
        case Sm64Region::JapanNtsc: return "Super Mario 64 (Japan)";
        case Sm64Region::EuropePal: return "Super Mario 64 (Europe)";
        case Sm64Region::Shindou:   return "Super Mario 64 Shindou Edition";
        default:                    return "unrecognised ROM";
    }
}

ByteOrder detectByteOrder(const uint8_t* data, size_t length) {
    if (!data || length < 4)
        return ByteOrder::Unknown;

    if (data[0] == 0x80 && data[1] == 0x37 && data[2] == 0x12 && data[3] == 0x40)
        return ByteOrder::Z64;
    if (data[0] == 0x37 && data[1] == 0x80 && data[2] == 0x40 && data[3] == 0x12)
        return ByteOrder::V64;
    if (data[0] == 0x40 && data[1] == 0x12 && data[2] == 0x37 && data[3] == 0x80)
        return ByteOrder::N64;

    return ByteOrder::Unknown;
}

bool convertToZ64(std::vector<uint8_t>& data) {
    switch (detectByteOrder(data.data(), data.size())) {
        case ByteOrder::Z64:
            return true;
        case ByteOrder::V64:
            swap16(data);
            return true;
        case ByteOrder::N64:
            swap32(data);
            return true;
        default:
            return false;
    }
}

Sm64Region regionForZ64Sha1(const std::string& sha1) {
    for (const KnownRom& known : kKnownRoms) {
        if (sha1 == known.sha1)
            return known.region;
    }
    return Sm64Region::Unknown;
}

std::string expectedSha1(Sm64Region region) {
    for (const KnownRom& known : kKnownRoms) {
        if (known.region == region)
            return known.sha1;
    }
    return "";
}

BaseRomInfo inspectBaseRomBuffer(std::vector<uint8_t>& data) {
    BaseRomInfo info;
    info.readable  = true;
    info.sizeBytes = static_cast<int64_t>(data.size());
    info.sha1      = checksum::sha1Hex(data);
    info.byteOrder = detectByteOrder(data.data(), data.size());
    info.isZ64     = info.byteOrder == ByteOrder::Z64;

    if (info.byteOrder == ByteOrder::Unknown) {
        info.error = "This file does not start with an N64 ROM header. It may be zipped, "
                     "or it may not be a ROM at all.";
        return info;
    }

    if (info.sizeBytes != kSm64RomSize) {
        info.error = "Expected an 8 MB cartridge dump but this file is "
            + std::to_string(info.sizeBytes / 1024) + " kB.";
        // Still hash it: the size alone does not prove what it is.
    }

    convertToZ64(data);
    info.z64Sha1 = info.isZ64 ? info.sha1 : checksum::sha1Hex(data);
    info.region  = regionForZ64Sha1(info.z64Sha1);

    if (info.region == Sm64Region::Unknown && info.error.empty()) {
        info.error = "Not a recognised Super Mario 64 dump. It may be modified, trimmed, "
                     "or overdumped.";
    } else if (info.region != Sm64Region::Unknown && info.region != Sm64Region::UsaNtsc) {
        info.error = std::string("This is ") + regionName(info.region)
            + ". SM64 hack patches are made against the USA release and will not apply.";
    }

    return info;
}

BaseRomInfo inspectBaseRom(const std::string& path) {
    BaseRomInfo info;

    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        info.error = "No file at " + path;
        return info;
    }

    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);

    if (size <= 0) {
        std::fclose(file);
        info.error = "That file is empty.";
        return info;
    }

    // Guard against being pointed at something enormous by mistake; a retail
    // cartridge dump is 8 MB and even an overdump stays well under this.
    if (size > 64 * 1024 * 1024) {
        std::fclose(file);
        info.error = "That file is far too large to be an N64 ROM.";
        return info;
    }

    std::vector<uint8_t> data(static_cast<size_t>(size));
    const size_t         read = std::fread(data.data(), 1, data.size(), file);
    const bool           bad  = read != data.size() || std::ferror(file) != 0;
    std::fclose(file);

    if (bad) {
        info.error = "Could not read the whole file. Is the SD card healthy?";
        return info;
    }

    return inspectBaseRomBuffer(data);
}

} // namespace rom
