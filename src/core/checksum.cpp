#include "src/core/checksum.hpp"

#include <cstdio>
#include <algorithm>
#include <cstring>

namespace checksum {

namespace {

inline uint32_t rotl(uint32_t value, int bits) {
    return (value << bits) | (value >> (32 - bits));
}

} // namespace

Sha1::Sha1()
    : m_bitCount(0)
    , m_bufferLength(0) {
    m_state[0] = 0x67452301;
    m_state[1] = 0xEFCDAB89;
    m_state[2] = 0x98BADCFE;
    m_state[3] = 0x10325476;
    m_state[4] = 0xC3D2E1F0;
}

void Sha1::processBlock(const uint8_t* block) {
    uint32_t w[80];

    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24)
            | (static_cast<uint32_t>(block[i * 4 + 1]) << 16)
            | (static_cast<uint32_t>(block[i * 4 + 2]) << 8)
            | static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 80; ++i)
        w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    uint32_t a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3], e = m_state[4];

    for (int i = 0; i < 80; ++i) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }

        const uint32_t temp = rotl(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rotl(b, 30);
        b = a;
        a = temp;
    }

    m_state[0] += a;
    m_state[1] += b;
    m_state[2] += c;
    m_state[3] += d;
    m_state[4] += e;
}

void Sha1::update(const uint8_t* data, size_t length) {
    m_bitCount += static_cast<uint64_t>(length) * 8;

    while (length > 0) {
        const size_t take = std::min(length, sizeof(m_buffer) - m_bufferLength);
        std::memcpy(m_buffer + m_bufferLength, data, take);
        m_bufferLength += take;
        data += take;
        length -= take;

        if (m_bufferLength == sizeof(m_buffer)) {
            processBlock(m_buffer);
            m_bufferLength = 0;
        }
    }
}

std::string Sha1::finalHex() {
    // Pad with 0x80, then zeros, so the message ends 8 bytes short of a block
    // boundary, then append the original length in bits (big endian).
    uint8_t tail[128];
    size_t  tailLength = 0;

    tail[tailLength++] = 0x80;
    while ((m_bufferLength + tailLength) % 64 != 56)
        tail[tailLength++] = 0x00;

    const uint64_t bitCount = m_bitCount;
    for (int i = 7; i >= 0; --i)
        tail[tailLength++] = static_cast<uint8_t>((bitCount >> (i * 8)) & 0xFF);

    // update() adds to m_bitCount, but the digest is already fixed by the
    // value captured above, so the drift does not matter.
    update(tail, tailLength);

    char out[41];
    for (int i = 0; i < 5; ++i)
        std::snprintf(out + i * 8, 9, "%08x", m_state[i]);
    return std::string(out, 40);
}

std::string sha1Hex(const uint8_t* data, size_t length) {
    Sha1 hash;
    hash.update(data, length);
    return hash.finalHex();
}

std::string sha1Hex(const std::vector<uint8_t>& data) {
    return sha1Hex(data.data(), data.size());
}

std::string sha1File(const std::string& path) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (!file)
        return "";

    Sha1                 hash;
    std::vector<uint8_t> buffer(64 * 1024);

    while (true) {
        const size_t read = std::fread(buffer.data(), 1, buffer.size(), file);
        if (read > 0)
            hash.update(buffer.data(), read);
        if (read < buffer.size())
            break;
    }

    const bool failed = std::ferror(file) != 0;
    std::fclose(file);

    return failed ? std::string() : hash.finalHex();
}

uint32_t crc32(const uint8_t* data, size_t length, uint32_t seed) {
    static uint32_t table[256];
    static bool     built = false;

    if (!built) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        built = true;
    }

    uint32_t crc = seed ^ 0xFFFFFFFFu;
    for (size_t i = 0; i < length; ++i)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

uint32_t crc32(const std::vector<uint8_t>& data, uint32_t seed) {
    return crc32(data.data(), data.size(), seed);
}

} // namespace checksum
