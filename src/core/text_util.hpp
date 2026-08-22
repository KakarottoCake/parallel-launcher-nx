#pragma once

#include <string>

namespace textutil {

// Everything the software keyboard hands back is untrusted: Borealis fills a
// 256-byte stack buffer and invokes the callback even when the applet was
// dismissed, so the string can contain uninitialised bytes, control
// characters, or an invalid UTF-8 tail. Anything that reaches JSON, a file
// path, or a label has to go through here first — nlohmann::json::dump()
// throws on invalid UTF-8, and an uncaught throw takes the whole app down.
//
// Drops control characters, truncates at the first invalid UTF-8 sequence,
// trims surrounding whitespace and caps the length.
std::string sanitizeUserInput(const std::string& input, size_t maxLength = 255);

// True when every byte sequence in the string is well-formed UTF-8.
bool isValidUtf8(const std::string& input);

} // namespace textutil
