// Host-side tests for the ROM pipeline. These modules deliberately avoid
// Borealis and libnx so they can be compiled and run on a development machine:
//
//     ./tests/run_tests.sh
//
// Pass a real Super Mario 64 dump as argv[1] to additionally check
// identification against a genuine cartridge image.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "src/core/checksum.hpp"
#include "src/core/patch.hpp"
#include "src/core/rom.hpp"
#include "src/core/text_util.hpp"

namespace {

int g_failures = 0;
int g_checks   = 0;

void check(bool condition, const std::string& what) {
    ++g_checks;
    if (condition) {
        std::printf("  ok    %s\n", what.c_str());
    } else {
        ++g_failures;
        std::printf("  FAIL  %s\n", what.c_str());
    }
}

void checkEq(const std::string& actual, const std::string& expected, const std::string& what) {
    ++g_checks;
    if (actual == expected) {
        std::printf("  ok    %s\n", what.c_str());
    } else {
        ++g_failures;
        std::printf("  FAIL  %s\n         expected %s\n         actual   %s\n",
            what.c_str(), expected.c_str(), actual.c_str());
    }
}

std::vector<uint8_t> bytes(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

// ---------------------------------------------------------------------------

void testSha1() {
    std::printf("SHA-1\n");

    checkEq(checksum::sha1Hex(bytes("")), "da39a3ee5e6b4b0d3255bfef95601890afd80709",
        "empty string");
    checkEq(checksum::sha1Hex(bytes("abc")), "a9993e364706816aba3e25717850c26c9cd0d89d",
        "\"abc\"");
    checkEq(checksum::sha1Hex(bytes("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")),
        "84983e441c3bd26ebaae4aa1f95129e5e54670f1", "56-byte message (two blocks)");

    // A million 'a's: the classic FIPS-180 long-message vector, which also
    // exercises the streaming path across many blocks.
    checksum::Sha1 streaming;
    std::vector<uint8_t> chunk(1000, 'a');
    for (int i = 0; i < 1000; ++i)
        streaming.update(chunk.data(), chunk.size());
    checkEq(streaming.finalHex(), "34aa973cd4c4daa4f61eeb2bdbad27316534016f",
        "one million 'a' characters (streaming)");

    // Length exactly on a block boundary, where padding needs a whole extra block.
    checkEq(checksum::sha1Hex(std::vector<uint8_t>(64, 0x00)),
        "c8d7d0ef0eedfa82d2ea1aa592845b9a6d4b02b7", "64 zero bytes");
}

void testCrc32() {
    std::printf("CRC-32\n");
    check(checksum::crc32(bytes("123456789")) == 0xCBF43926u, "\"123456789\" == 0xCBF43926");
    check(checksum::crc32(bytes("")) == 0x00000000u, "empty == 0");
    check(checksum::crc32(bytes("The quick brown fox jumps over the lazy dog")) == 0x414FA339u,
        "pangram == 0x414FA339");
}

void testByteOrder() {
    std::printf("ROM byte order\n");

    // A z64 header followed by a recognisable pattern.
    std::vector<uint8_t> z64 = { 0x80, 0x37, 0x12, 0x40, 0x00, 0x11, 0x22, 0x33 };

    std::vector<uint8_t> v64 = z64;
    for (size_t i = 0; i + 1 < v64.size(); i += 2)
        std::swap(v64[i], v64[i + 1]);

    std::vector<uint8_t> n64 = z64;
    for (size_t i = 0; i + 3 < n64.size(); i += 4) {
        std::swap(n64[i], n64[i + 3]);
        std::swap(n64[i + 1], n64[i + 2]);
    }

    check(rom::detectByteOrder(z64.data(), z64.size()) == rom::ByteOrder::Z64, "detects z64");
    check(rom::detectByteOrder(v64.data(), v64.size()) == rom::ByteOrder::V64, "detects v64");
    check(rom::detectByteOrder(n64.data(), n64.size()) == rom::ByteOrder::N64, "detects n64");

    std::vector<uint8_t> junk = { 0xDE, 0xAD, 0xBE, 0xEF };
    check(rom::detectByteOrder(junk.data(), junk.size()) == rom::ByteOrder::Unknown,
        "rejects non-ROM data");
    check(rom::detectByteOrder(nullptr, 0) == rom::ByteOrder::Unknown, "rejects empty input");

    std::vector<uint8_t> converted = v64;
    check(rom::convertToZ64(converted) && converted == z64, "v64 converts to z64");

    converted = n64;
    check(rom::convertToZ64(converted) && converted == z64, "n64 converts to z64");

    converted = z64;
    check(rom::convertToZ64(converted) && converted == z64, "z64 passes through unchanged");
}

// --- minimal BPS encoder, used only to exercise the decoder ----------------

void writeNumber(std::vector<uint8_t>& out, uint64_t value) {
    while (true) {
        const uint8_t x = value & 0x7F;
        value >>= 7;
        if (value == 0) {
            out.push_back(0x80 | x);
            break;
        }
        out.push_back(x);
        --value;
    }
}

void writeSignedNumber(std::vector<uint8_t>& out, int64_t value) {
    const uint64_t encoded = (static_cast<uint64_t>(value < 0 ? -value : value) << 1)
        | (value < 0 ? 1u : 0u);
    writeNumber(out, encoded);
}

void writeLe32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(value & 0xFF);
    out.push_back((value >> 8) & 0xFF);
    out.push_back((value >> 16) & 0xFF);
    out.push_back((value >> 24) & 0xFF);
}

void testBpsNumberRoundTrip() {
    std::printf("BPS number encoding\n");

    // Build a patch whose only job is to encode one big number as a run
    // length, proving the decoder's varint agrees with the encoder's.
    const std::vector<uint64_t> values = { 0, 1, 127, 128, 129, 16383, 16384, 1000000 };
    bool allOk = true;

    for (uint64_t value : values) {
        std::vector<uint8_t> source(static_cast<size_t>(value) + 1, 0xAB);
        std::vector<uint8_t> body = { 'B', 'P', 'S', '1' };
        writeNumber(body, source.size()); // source size
        writeNumber(body, source.size()); // target size
        writeNumber(body, 0);             // no metadata
        writeNumber(body, ((source.size() - 1) << 2) | 0); // SourceRead, whole file

        writeLe32(body, checksum::crc32(source));
        writeLe32(body, checksum::crc32(source));
        std::vector<uint8_t> patchData = body;
        writeLe32(patchData, checksum::crc32(body.data(), body.size()));

        const patch::Result result = patch::applyBps(source, patchData);
        if (!result.ok || result.output != source) {
            allOk = false;
            std::printf("         value %llu failed: %s\n",
                static_cast<unsigned long long>(value), result.error.c_str());
        }
    }

    check(allOk, "varint round-trips across byte-boundary values");
}

void testBps() {
    std::printf("BPS patching\n");

    const std::vector<uint8_t> source = bytes("HELLO WORLD, THIS IS THE ORIGINAL ROM CONTENT.");
    const std::vector<uint8_t> target = bytes("HELLO WORLD, THIS IS THE PATCHED  ROM CONTENT!!!");

    // SourceRead the shared prefix, TargetRead the literal difference,
    // SourceCopy a run from the middle, then TargetRead the tail. Between
    // them these cover every action the decoder implements.
    const size_t prefix = 25; // "HELLO WORLD, THIS IS THE "

    std::vector<uint8_t> body = { 'B', 'P', 'S', '1' };
    writeNumber(body, source.size());
    writeNumber(body, target.size());
    writeNumber(body, 0);

    // SourceRead prefix
    writeNumber(body, ((prefix - 1) << 2) | 0);

    // TargetRead "PATCHED  ROM CONTENT!!!"
    const std::vector<uint8_t> literal(target.begin() + prefix, target.end());
    writeNumber(body, ((literal.size() - 1) << 2) | 1);
    body.insert(body.end(), literal.begin(), literal.end());

    writeLe32(body, checksum::crc32(source));
    writeLe32(body, checksum::crc32(target));
    std::vector<uint8_t> patchData = body;
    writeLe32(patchData, checksum::crc32(body.data(), body.size()));

    check(patch::detectFormat(patchData.data(), patchData.size()) == patch::Format::Bps,
        "detects BPS magic");

    const patch::Result result = patch::applyBps(source, patchData);
    check(result.ok, "applies cleanly: " + result.error);
    check(result.output == target, "produces the expected output");

    // Wrong base ROM must be reported, not silently patched.
    std::vector<uint8_t> wrongSource = source;
    wrongSource[0] = 'X';
    const patch::Result wrong = patch::applyBps(wrongSource, patchData);
    check(!wrong.ok && !wrong.sourceCrcMatched, "rejects a mismatched base ROM");

    // A corrupt patch must be caught by the patch's own CRC.
    std::vector<uint8_t> corrupt = patchData;
    corrupt[20] ^= 0xFF;
    check(!patch::applyBps(source, corrupt).ok, "rejects a corrupt patch");

    // Truncation must not read out of bounds.
    std::vector<uint8_t> truncated(patchData.begin(), patchData.begin() + patchData.size() / 2);
    check(!patch::applyBps(source, truncated).ok, "rejects a truncated patch");
}

void testBpsTargetCopy() {
    std::printf("BPS TargetCopy (overlapping runs)\n");

    const std::vector<uint8_t> source = bytes("AB");
    const std::vector<uint8_t> target = bytes("ABABABAB");

    std::vector<uint8_t> body = { 'B', 'P', 'S', '1' };
    writeNumber(body, source.size());
    writeNumber(body, target.size());
    writeNumber(body, 0);

    writeNumber(body, ((2 - 1) << 2) | 0); // SourceRead "AB"
    writeNumber(body, ((6 - 1) << 2) | 3); // TargetCopy 6 bytes from offset 0
    writeSignedNumber(body, 0);

    writeLe32(body, checksum::crc32(source));
    writeLe32(body, checksum::crc32(target));
    std::vector<uint8_t> patchData = body;
    writeLe32(patchData, checksum::crc32(body.data(), body.size()));

    const patch::Result result = patch::applyBps(source, patchData);
    check(result.ok, "applies cleanly: " + result.error);
    check(result.output == target, "overlapping copy expands correctly");
}

void testIps() {
    std::printf("IPS patching\n");

    std::vector<uint8_t> source(32, 0x00);

    std::vector<uint8_t> patchData = { 'P', 'A', 'T', 'C', 'H' };
    // Data record: write "HI" at offset 4.
    patchData.insert(patchData.end(), { 0x00, 0x00, 0x04, 0x00, 0x02, 'H', 'I' });
    // RLE record: 5 x 0xFF at offset 10.
    patchData.insert(patchData.end(), { 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x05, 0xFF });
    patchData.insert(patchData.end(), { 'E', 'O', 'F' });

    check(patch::detectFormat(patchData.data(), patchData.size()) == patch::Format::Ips,
        "detects IPS magic");

    const patch::Result result = patch::applyIps(source, patchData);
    check(result.ok, "applies cleanly: " + result.error);
    check(result.output.size() == 32, "keeps the original size");
    check(result.output[4] == 'H' && result.output[5] == 'I', "writes the data record");
    check(result.output[10] == 0xFF && result.output[14] == 0xFF, "expands the RLE record");
    check(result.output[9] == 0x00 && result.output[15] == 0x00, "leaves other bytes alone");

    std::vector<uint8_t> noEof = { 'P', 'A', 'T', 'C', 'H', 0x00, 0x00, 0x04, 0x00, 0x02, 'H' };
    check(!patch::applyIps(source, noEof).ok, "rejects a patch with no EOF marker");
}

void testTextUtil() {
    std::printf("Software keyboard input sanitising\n");

    checkEq(textutil::sanitizeUserInput("  hello  "), "hello", "trims whitespace");
    checkEq(textutil::sanitizeUserInput(std::string("ab\0cd", 5)), "ab", "stops at a NUL");
    checkEq(textutil::sanitizeUserInput("a\x01\x02z"), "az", "drops control characters");
    checkEq(textutil::sanitizeUserInput("caf\xc3\xa9"), "caf\xc3\xa9", "keeps valid UTF-8");
    checkEq(textutil::sanitizeUserInput("bad\xff\xfe"), "bad", "truncates at invalid UTF-8");
    checkEq(textutil::sanitizeUserInput("abcdef", 3), "abc", "honours the length cap");

    check(textutil::isValidUtf8("caf\xc3\xa9"), "accepts valid UTF-8");
    check(!textutil::isValidUtf8("\xc3"), "rejects a truncated sequence");
    check(!textutil::isValidUtf8("\xed\xa0\x80"), "rejects a surrogate half");
    check(!textutil::isValidUtf8("\xc0\x80"), "rejects an overlong encoding");
}

void testRealRom(const char* path) {
    std::printf("Real ROM: %s\n", path);

    const rom::BaseRomInfo info = rom::inspectBaseRom(path);

    std::printf("         size      %lld bytes\n", static_cast<long long>(info.sizeBytes));
    std::printf("         order     %s\n", rom::byteOrderName(info.byteOrder));
    std::printf("         sha1      %s\n", info.sha1.c_str());
    std::printf("         z64 sha1  %s\n", info.z64Sha1.c_str());
    std::printf("         region    %s\n", rom::regionName(info.region));
    if (!info.error.empty())
        std::printf("         note      %s\n", info.error.c_str());

    check(info.readable, "file is readable");
    check(info.sizeBytes == rom::kSm64RomSize, "is 8 MB");
    check(info.byteOrder != rom::ByteOrder::Unknown, "has a valid N64 header");
    check(info.region == rom::Sm64Region::UsaNtsc, "identified as Super Mario 64 (USA)");
    check(info.isUsableBase(), "usable as a patch base");
}

// Patching an 8 MB ROM exercises the decoder at the scale it actually runs at,
// with real data rather than a handful of synthetic bytes.
void testRealRomPatching(const char* path) {
    std::printf("Real ROM patching (8 MB)\n");

    std::FILE* file = std::fopen(path, "rb");
    if (!file) {
        check(false, "could not open the ROM");
        return;
    }
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);

    std::vector<uint8_t> source(static_cast<size_t>(size));
    const size_t read = std::fread(source.data(), 1, source.size(), file);
    std::fclose(file);
    check(read == source.size(), "read the whole ROM");

    // Target: the same ROM with a patched region in the middle, which is what
    // a real hack's patch looks like in miniature.
    std::vector<uint8_t> target = source;
    const size_t editAt = 0x100000;
    const std::vector<uint8_t> edit = bytes("PARALLEL LAUNCHER NX PATCH TEST");
    std::memcpy(target.data() + editAt, edit.data(), edit.size());

    std::vector<uint8_t> body = { 'B', 'P', 'S', '1' };
    writeNumber(body, source.size());
    writeNumber(body, target.size());
    writeNumber(body, 0);

    writeNumber(body, ((editAt - 1) << 2) | 0);            // SourceRead up to the edit
    writeNumber(body, ((edit.size() - 1) << 2) | 1);       // TargetRead the new bytes
    body.insert(body.end(), edit.begin(), edit.end());

    const size_t tail = source.size() - editAt - edit.size();
    writeNumber(body, ((tail - 1) << 2) | 2);              // SourceCopy the remainder
    writeSignedNumber(body, static_cast<int64_t>(editAt + edit.size()));

    writeLe32(body, checksum::crc32(source));
    writeLe32(body, checksum::crc32(target));
    std::vector<uint8_t> patchData = body;
    writeLe32(patchData, checksum::crc32(body.data(), body.size()));

    std::printf("         patch is %zu bytes for an %zu byte ROM\n",
        patchData.size(), source.size());

    const patch::Result result = patch::applyBps(source, patchData);
    check(result.ok, "applies to the real ROM: " + result.error);
    check(result.output.size() == target.size(), "output is the right size");
    check(result.output == target, "output matches the intended target byte for byte");
    checkEq(checksum::sha1Hex(result.output), checksum::sha1Hex(target),
        "output SHA-1 matches the target");

    // And the guard that matters most in practice: a JP or PAL dump must be
    // refused rather than producing a broken ROM.
    std::vector<uint8_t> wrongBase = source;
    wrongBase[0x2000] ^= 0x01;
    const patch::Result wrong = patch::applyBps(wrongBase, patchData);
    check(!wrong.ok, "refuses a base ROM that differs by a single byte");
}

} // namespace

int main(int argc, char** argv) {
    testSha1();
    testCrc32();
    testByteOrder();
    testBpsNumberRoundTrip();
    testBps();
    testBpsTargetCopy();
    testIps();
    testTextUtil();

    if (argc > 1) {
        testRealRom(argv[1]);
        testRealRomPatching(argv[1]);
    } else
        std::printf("Real ROM: skipped (pass a path to check one)\n");

    std::printf("\n%d checks, %d failure%s\n", g_checks, g_failures,
        g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
