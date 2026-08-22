#include "src/core/patch.hpp"

#include <cstring>

#include "src/core/checksum.hpp"

namespace patch {

namespace {

constexpr uint8_t kBpsMagic[4] = { 'B', 'P', 'S', '1' };
constexpr uint8_t kIpsMagic[5] = { 'P', 'A', 'T', 'C', 'H' };
constexpr uint8_t kIpsEof[3]   = { 'E', 'O', 'F' };

// BPS number encoding: 7 bits per byte, little end first, high bit marks the
// last byte, and each continuation adds an implicit bias so encodings are
// unique.
class BpsReader {
  public:
    BpsReader(const uint8_t* data, size_t length)
        : m_data(data)
        , m_length(length) {}

    bool   overrun() const { return m_overrun; }
    size_t offset() const { return m_offset; }

    uint8_t readByte() {
        if (m_offset >= m_length) {
            m_overrun = true;
            return 0;
        }
        return m_data[m_offset++];
    }

    uint64_t readNumber() {
        uint64_t data  = 0;
        uint64_t shift = 1;

        for (int i = 0; i < 10; ++i) { // 10 * 7 bits covers a 64-bit value
            const uint8_t x = readByte();
            if (m_overrun)
                return 0;

            data += static_cast<uint64_t>(x & 0x7F) * shift;
            if (x & 0x80)
                return data;

            shift <<= 7;
            data += shift;
        }

        m_overrun = true;
        return 0;
    }

    // Signed variant used by the copy actions: bit 0 is the sign.
    int64_t readSignedNumber() {
        const uint64_t data = readNumber();
        const int64_t  magnitude = static_cast<int64_t>(data >> 1);
        return (data & 1) ? -magnitude : magnitude;
    }

  private:
    const uint8_t* m_data;
    size_t         m_length;
    size_t         m_offset  = 0;
    bool           m_overrun = false;
};

uint32_t readLe32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
        | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

} // namespace

const char* formatName(Format format) {
    switch (format) {
        case Format::Bps: return "BPS";
        case Format::Ips: return "IPS";
        default:          return "unknown";
    }
}

Format detectFormat(const uint8_t* data, size_t length) {
    if (data && length >= sizeof(kBpsMagic) && std::memcmp(data, kBpsMagic, sizeof(kBpsMagic)) == 0)
        return Format::Bps;
    if (data && length >= sizeof(kIpsMagic) && std::memcmp(data, kIpsMagic, sizeof(kIpsMagic)) == 0)
        return Format::Ips;
    return Format::Unknown;
}

Result applyBps(const std::vector<uint8_t>& source, const std::vector<uint8_t>& patchData) {
    Result result;

    // 4-byte magic plus a 12-byte footer of three CRCs is the bare minimum.
    if (patchData.size() < 4 + 12) {
        result.error = "Patch file is too short to be a valid BPS patch.";
        return result;
    }
    if (detectFormat(patchData.data(), patchData.size()) != Format::Bps) {
        result.error = "Patch file is not in BPS format.";
        return result;
    }

    const size_t bodyEnd = patchData.size() - 12;
    const uint8_t* footer = patchData.data() + bodyEnd;

    result.expectedSourceCrc = readLe32(footer);
    result.expectedTargetCrc = readLe32(footer + 4);
    const uint32_t expectedPatchCrc = readLe32(footer + 8);

    const uint32_t actualPatchCrc = checksum::crc32(patchData.data(), bodyEnd + 8);
    if (actualPatchCrc != expectedPatchCrc) {
        result.error = "The patch file is corrupt (its own checksum does not match). "
                       "Download it again.";
        return result;
    }

    BpsReader reader(patchData.data(), bodyEnd);
    reader.readByte(); // consume the four magic bytes
    reader.readByte();
    reader.readByte();
    reader.readByte();

    const uint64_t sourceSize   = reader.readNumber();
    const uint64_t targetSize   = reader.readNumber();
    const uint64_t metadataSize = reader.readNumber();

    if (reader.overrun()) {
        result.error = "Patch header is truncated.";
        return result;
    }

    for (uint64_t i = 0; i < metadataSize; ++i)
        reader.readByte();

    if (reader.overrun()) {
        result.error = "Patch metadata is truncated.";
        return result;
    }

    if (sourceSize != source.size()) {
        result.error = "This patch expects a " + std::to_string(sourceSize / 1024)
            + " kB base ROM but yours is " + std::to_string(source.size() / 1024) + " kB.";
        return result;
    }

    const uint32_t sourceCrc = checksum::crc32(source);
    result.sourceCrcMatched  = sourceCrc == result.expectedSourceCrc;
    if (!result.sourceCrcMatched) {
        result.error = "This patch was made for a different base ROM. Check that yours is a "
                       "clean, unmodified Super Mario 64 (USA) dump.";
        return result;
    }

    // Guard against a hostile or corrupt patch claiming an absurd output size.
    if (targetSize > 256u * 1024u * 1024u) {
        result.error = "Patch declares an implausibly large output ROM.";
        return result;
    }

    std::vector<uint8_t>& target = result.output;
    target.resize(static_cast<size_t>(targetSize));

    size_t  outputOffset     = 0;
    int64_t sourceRelative   = 0;
    int64_t targetRelative   = 0;

    while (reader.offset() < bodyEnd && outputOffset < target.size()) {
        const uint64_t command = reader.readNumber();
        if (reader.overrun())
            break;

        const uint64_t action = command & 3;
        const uint64_t length = (command >> 2) + 1;

        if (outputOffset + length > target.size()) {
            result.error = "Patch tried to write past the end of the output ROM.";
            return result;
        }

        switch (action) {
            case 0: // SourceRead: copy from the same offset in the source
                if (outputOffset + length > source.size()) {
                    result.error = "Patch reads past the end of the base ROM.";
                    return result;
                }
                std::memcpy(target.data() + outputOffset, source.data() + outputOffset,
                    static_cast<size_t>(length));
                outputOffset += length;
                break;

            case 1: // TargetRead: literal bytes stored in the patch
                for (uint64_t i = 0; i < length; ++i)
                    target[outputOffset++] = reader.readByte();
                if (reader.overrun()) {
                    result.error = "Patch data is truncated.";
                    return result;
                }
                break;

            case 2: { // SourceCopy: copy from a moving cursor in the source
                sourceRelative += reader.readSignedNumber();
                if (reader.overrun()) {
                    result.error = "Patch data is truncated.";
                    return result;
                }
                if (sourceRelative < 0
                    || static_cast<uint64_t>(sourceRelative) + length > source.size()) {
                    result.error = "Patch reads outside the base ROM.";
                    return result;
                }
                for (uint64_t i = 0; i < length; ++i)
                    target[outputOffset++] = source[static_cast<size_t>(sourceRelative++)];
                break;
            }

            case 3: { // TargetCopy: copy from earlier in the output
                targetRelative += reader.readSignedNumber();
                if (reader.overrun()) {
                    result.error = "Patch data is truncated.";
                    return result;
                }
                if (targetRelative < 0
                    || static_cast<uint64_t>(targetRelative) + length > target.size()) {
                    result.error = "Patch reads outside the output ROM.";
                    return result;
                }
                // Deliberately byte at a time: runs are allowed to overlap the
                // bytes they are producing.
                for (uint64_t i = 0; i < length; ++i)
                    target[outputOffset++] = target[static_cast<size_t>(targetRelative++)];
                break;
            }

            default:
                result.error = "Unknown action in patch.";
                return result;
        }
    }

    if (outputOffset != target.size()) {
        result.error = "Patch ended early; the output ROM is incomplete.";
        return result;
    }

    const uint32_t targetCrc = checksum::crc32(target);
    if (targetCrc != result.expectedTargetCrc) {
        result.error = "The patched ROM does not match the checksum the author recorded.";
        return result;
    }

    result.ok = true;
    return result;
}

Result applyIps(const std::vector<uint8_t>& source, const std::vector<uint8_t>& patchData) {
    Result result;

    if (detectFormat(patchData.data(), patchData.size()) != Format::Ips) {
        result.error = "Patch file is not in IPS format.";
        return result;
    }

    result.output = source;

    size_t offset = sizeof(kIpsMagic);
    while (true) {
        if (offset + 3 > patchData.size()) {
            result.error = "Patch is truncated: no EOF marker.";
            return result;
        }

        if (std::memcmp(patchData.data() + offset, kIpsEof, sizeof(kIpsEof)) == 0) {
            offset += 3;
            break;
        }

        const size_t writeAt = (static_cast<size_t>(patchData[offset]) << 16)
            | (static_cast<size_t>(patchData[offset + 1]) << 8)
            | static_cast<size_t>(patchData[offset + 2]);
        offset += 3;

        if (offset + 2 > patchData.size()) {
            result.error = "Patch is truncated inside a record header.";
            return result;
        }

        const size_t size = (static_cast<size_t>(patchData[offset]) << 8)
            | static_cast<size_t>(patchData[offset + 1]);
        offset += 2;

        if (size == 0) {
            // RLE record: a run length and a single byte to repeat.
            if (offset + 3 > patchData.size()) {
                result.error = "Patch is truncated inside an RLE record.";
                return result;
            }
            const size_t runLength = (static_cast<size_t>(patchData[offset]) << 8)
                | static_cast<size_t>(patchData[offset + 1]);
            const uint8_t value = patchData[offset + 2];
            offset += 3;

            if (writeAt + runLength > result.output.size())
                result.output.resize(writeAt + runLength, 0);

            std::memset(result.output.data() + writeAt, value, runLength);
        } else {
            if (offset + size > patchData.size()) {
                result.error = "Patch is truncated inside a data record.";
                return result;
            }

            if (writeAt + size > result.output.size())
                result.output.resize(writeAt + size, 0);

            std::memcpy(result.output.data() + writeAt, patchData.data() + offset, size);
            offset += size;
        }
    }

    // Optional 3-byte truncation field after EOF.
    if (offset + 3 == patchData.size()) {
        const size_t truncateTo = (static_cast<size_t>(patchData[offset]) << 16)
            | (static_cast<size_t>(patchData[offset + 1]) << 8)
            | static_cast<size_t>(patchData[offset + 2]);
        if (truncateTo < result.output.size())
            result.output.resize(truncateTo);
    }

    result.ok = true;
    return result;
}

Result apply(const std::vector<uint8_t>& source, const std::vector<uint8_t>& patchData) {
    switch (detectFormat(patchData.data(), patchData.size())) {
        case Format::Bps:
            return applyBps(source, patchData);
        case Format::Ips:
            return applyIps(source, patchData);
        default: {
            Result result;
            result.error = "Unrecognised patch format. The launcher understands BPS and IPS.";
            return result;
        }
    }
}

} // namespace patch
