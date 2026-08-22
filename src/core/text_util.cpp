#include "src/core/text_util.hpp"

#include <cctype>

namespace textutil {

namespace {

// Returns the length in bytes of the UTF-8 sequence starting at `pos`, or 0 if
// the sequence is malformed or truncated.
size_t sequenceLength(const std::string& s, size_t pos) {
    const unsigned char lead = static_cast<unsigned char>(s[pos]);

    size_t length = 0;
    if (lead < 0x80)
        return 1;
    else if ((lead & 0xE0) == 0xC0)
        length = 2;
    else if ((lead & 0xF0) == 0xE0)
        length = 3;
    else if ((lead & 0xF8) == 0xF0)
        length = 4;
    else
        return 0; // continuation byte or invalid lead

    if (pos + length > s.size())
        return 0;

    for (size_t i = 1; i < length; ++i) {
        if ((static_cast<unsigned char>(s[pos + i]) & 0xC0) != 0x80)
            return 0;
    }

    // Reject overlong encodings and anything past the Unicode maximum.
    const uint32_t cp = length == 2
        ? ((lead & 0x1Fu) << 6) | (static_cast<unsigned char>(s[pos + 1]) & 0x3Fu)
        : length == 3
        ? ((lead & 0x0Fu) << 12) | ((static_cast<unsigned char>(s[pos + 1]) & 0x3Fu) << 6)
            | (static_cast<unsigned char>(s[pos + 2]) & 0x3Fu)
        : ((lead & 0x07u) << 18) | ((static_cast<unsigned char>(s[pos + 1]) & 0x3Fu) << 12)
            | ((static_cast<unsigned char>(s[pos + 2]) & 0x3Fu) << 6)
            | (static_cast<unsigned char>(s[pos + 3]) & 0x3Fu);

    if (length == 2 && cp < 0x80)      return 0;
    if (length == 3 && cp < 0x800)     return 0;
    if (length == 4 && cp < 0x10000)   return 0;
    if (cp > 0x10FFFF)                 return 0;
    if (cp >= 0xD800 && cp <= 0xDFFF)  return 0; // surrogate half

    return length;
}

} // namespace

bool isValidUtf8(const std::string& input) {
    size_t pos = 0;
    while (pos < input.size()) {
        const size_t length = sequenceLength(input, pos);
        if (length == 0)
            return false;
        pos += length;
    }
    return true;
}

std::string sanitizeUserInput(const std::string& input, size_t maxLength) {
    std::string out;
    out.reserve(input.size());

    size_t pos = 0;
    while (pos < input.size() && out.size() < maxLength) {
        const size_t length = sequenceLength(input, pos);
        if (length == 0)
            break; // malformed from here on; keep what we have

        if (length == 1) {
            const unsigned char c = static_cast<unsigned char>(input[pos]);
            if (c == '\0')
                break;
            // Drop control characters, including the stray newlines and tabs
            // the keyboard can produce.
            if (c >= 0x20 && c != 0x7F)
                out.push_back(static_cast<char>(c));
        } else {
            if (out.size() + length > maxLength)
                break;
            out.append(input, pos, length);
        }

        pos += length;
    }

    const size_t first = out.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";
    const size_t last = out.find_last_not_of(" \t\r\n");

    return out.substr(first, last - first + 1);
}

} // namespace textutil
