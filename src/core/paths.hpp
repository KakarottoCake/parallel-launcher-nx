#pragma once

#include <string>

// All launcher state lives under one directory on the SD card so it can be
// backed up or deleted as a unit.
namespace paths {

constexpr const char* kRoot   = "sdmc:/switch/parallel-launcher";
constexpr const char* kConfig = "sdmc:/switch/parallel-launcher/config.json";

inline std::string roms()    { return std::string(kRoot) + "/roms"; }
inline std::string hacks()   { return std::string(kRoot) + "/hacks"; }
inline std::string cache()   { return std::string(kRoot) + "/cache"; }
inline std::string saves()   { return std::string(kRoot) + "/saves"; }
inline std::string art()     { return std::string(kRoot) + "/art"; }
inline std::string patches() { return std::string(kRoot) + "/patches"; }
inline std::string log()     { return std::string(kRoot) + "/launcher.log"; }

// Creates every directory the launcher writes to. Safe to call repeatedly.
// Returns false if the SD card is unwritable, which the UI surfaces as an
// error banner rather than crashing.
bool ensureDirectories();

bool fileExists(const std::string& path);
bool directoryExists(const std::string& path);

} // namespace paths
