#pragma once

#include <functional>
#include <string>

#include "src/core/rom.hpp"

// Caches the result of inspecting the configured base ROM.
//
// Hashing 8 MB takes long enough to drop frames, so the check runs on a worker
// thread and the result is delivered back on the UI thread. Views ask for the
// cached answer and subscribe to be told when it changes.
class RomService {
  public:
    enum class State {
        Unchecked,
        Checking,
        Done,
    };

    static RomService& instance();

    State                     state() const { return m_state; }
    const rom::BaseRomInfo&   info() const { return m_info; }
    const std::string&        checkedPath() const { return m_checkedPath; }

    // True once a usable clean USA dump has been confirmed.
    bool hasUsableBaseRom() const {
        return m_state == State::Done && m_info.isUsableBase();
    }

    // Kicks off a check of the path currently in settings. Does nothing if a
    // check is already running. `onDone` runs on the UI thread.
    void refresh(std::function<void()> onDone = nullptr);

    // Re-checks even if the cached result is for the same path.
    void invalidate() { m_state = State::Unchecked; }

    // One-line summary suitable for a status banner.
    std::string summary() const;

  private:
    RomService() = default;

    State             m_state = State::Unchecked;
    rom::BaseRomInfo  m_info;
    std::string       m_checkedPath;
};
