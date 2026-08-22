#include "src/core/rom_service.hpp"

#include <borealis.hpp>

#include "src/core/settings.hpp"

RomService& RomService::instance() {
    static RomService s_instance;
    return s_instance;
}

void RomService::refresh(std::function<void()> onDone) {
    if (m_state == State::Checking)
        return;

    const std::string path = AppSettings::instance().baseRomPath;
    m_state       = State::Checking;
    m_checkedPath = path;

    brls::Logger::info("Checking base ROM at {}", path);

    brls::async([this, path, onDone]() {
        rom::BaseRomInfo result = rom::inspectBaseRom(path);

        brls::sync([this, result = std::move(result), onDone]() {
            m_info  = result;
            m_state = State::Done;

            if (m_info.isUsableBase()) {
                brls::Logger::info("Base ROM OK: {} (sha1 {})", rom::regionName(m_info.region),
                    m_info.z64Sha1);
            } else {
                brls::Logger::warning("Base ROM unusable: {}", m_info.error);
            }

            if (onDone)
                onDone();
        });
    });
}

std::string RomService::summary() const {
    switch (m_state) {
        case State::Unchecked:
            return "Not checked yet";
        case State::Checking:
            return "Checking...";
        case State::Done:
            break;
    }

    if (m_info.isUsableBase()) {
        std::string text = std::string(rom::regionName(m_info.region)) + " verified";
        if (!m_info.isZ64)
            text += ", converted from " + std::string(rom::byteOrderName(m_info.byteOrder));
        return text;
    }

    return m_info.error.empty() ? "Unusable base ROM" : m_info.error;
}
