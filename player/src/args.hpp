#pragma once

#include <string>

// Command line the launcher hands to a player NRO.
//
//   player.nro <rom.z64> --options <cfg> --return <launcher.nro> [--return-args "..."]
//
// --return is what makes the launcher feel like one app: the player registers
// it with envSetNextLoad before exiting, so quitting a game drops you back
// into ParaLLEl Launcher NX instead of the homebrew menu.
struct PlayerArgs {
    std::string romPath;
    std::string optionsPath;
    std::string returnNro;
    std::string returnArgs;
    std::string savePath;   // directory for SRAM/EEPROM, defaults under the launcher tree
    std::string systemPath; // directory for core system files
    bool        valid = false;
    std::string error;

    static PlayerArgs parse(int argc, char** argv);

    // The argv string handed back to the launcher, carrying the rom we played
    // so it can update play time and reopen the right page.
    std::string buildReturnArgs() const;
};
