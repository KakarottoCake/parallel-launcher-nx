#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Self-contained SHA-1 and CRC-32. Kept free of Borealis and libnx so the
// ROM pipeline can be unit tested on a host machine.
namespace checksum {

// Lowercase 40-character hex digest.
std::string sha1Hex(const uint8_t* data, size_t length);
std::string sha1Hex(const std::vector<uint8_t>& data);

// Streaming SHA-1, for hashing an 8 MB ROM without a second copy in memory.
class Sha1 {
  public:
    Sha1();
    void        update(const uint8_t* data, size_t length);
    std::string finalHex();

  private:
    void processBlock(const uint8_t* block);

    uint32_t m_state[5];
    uint64_t m_bitCount;
    uint8_t  m_buffer[64];
    size_t   m_bufferLength;
};

// Hashes a file in 64 kB chunks. Returns an empty string if it cannot be read.
std::string sha1File(const std::string& path);

uint32_t crc32(const uint8_t* data, size_t length, uint32_t seed = 0);
uint32_t crc32(const std::vector<uint8_t>& data, uint32_t seed = 0);

} // namespace checksum
