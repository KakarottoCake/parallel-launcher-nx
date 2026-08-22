#pragma once

#include <string>

// The player has no UI to report problems through, and stdout goes nowhere on
// a console without nxlink, so everything goes to a file the launcher can read
// back and show the user.
namespace plog {

void open(const std::string& path);
void close();

void info(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void warn(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void error(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// Last error recorded, handed back to the launcher so it can surface a reason
// rather than just bouncing the user back to the menu.
const std::string& lastError();

} // namespace plog
