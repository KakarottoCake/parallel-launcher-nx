#include "src/core/paths.hpp"

#include <sys/stat.h>
#include <cerrno>

#include <borealis.hpp>

namespace paths {

namespace {

bool makeDirectory(const std::string& path) {
    if (::mkdir(path.c_str(), 0777) == 0)
        return true;
    if (errno == EEXIST)
        return true;

    brls::Logger::error("mkdir failed for {}: {}", path, std::string(strerror(errno)));
    return false;
}

} // namespace

bool ensureDirectories() {
    // "sdmc:/switch" is created by every homebrew loader, but the launcher
    // subtree may not exist on a fresh card.
    bool ok = true;
    ok &= makeDirectory(kRoot);
    ok &= makeDirectory(roms());
    ok &= makeDirectory(hacks());
    ok &= makeDirectory(cache());
    ok &= makeDirectory(saves());
    ok &= makeDirectory(art());
    ok &= makeDirectory(patches());
    return ok;
}

bool fileExists(const std::string& path) {
    struct stat st {};
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool directoryExists(const std::string& path) {
    struct stat st {};
    return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

} // namespace paths
