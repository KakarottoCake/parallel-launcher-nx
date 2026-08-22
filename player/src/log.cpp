#include "log.hpp"

#include <cstdarg>
#include <cstdio>

namespace plog {

namespace {

std::FILE*  g_file = nullptr;
std::string g_lastError;

void write(const char* level, const char* fmt, va_list args) {
    if (!g_file)
        return;

    std::fprintf(g_file, "[%s] ", level);
    std::vfprintf(g_file, fmt, args);
    std::fputc('\n', g_file);
    std::fflush(g_file);
}

} // namespace

void open(const std::string& path) {
    g_file = std::fopen(path.c_str(), "w");
    if (g_file)
        std::setvbuf(g_file, nullptr, _IOLBF, 4096);
}

void close() {
    if (g_file) {
        std::fclose(g_file);
        g_file = nullptr;
    }
}

void info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    write("info", fmt, args);
    va_end(args);
}

void warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    write("warn", fmt, args);
    va_end(args);
}

void error(const char* fmt, ...) {
    char buffer[512];

    va_list capture;
    va_start(capture, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, capture);
    va_end(capture);

    g_lastError = buffer;

    va_list args;
    va_start(args, fmt);
    write("error", fmt, args);
    va_end(args);
}

const std::string& lastError() {
    return g_lastError;
}

} // namespace plog
