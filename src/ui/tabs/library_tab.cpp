#include "src/ui/tabs/library_tab.hpp"

#include "src/core/paths.hpp"
#include "src/core/rom_service.hpp"
#include "src/core/settings.hpp"

LibraryTab::LibraryTab() {
    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(1.0f);

    m_banner = new nxui::StatusBanner(nxui::StatusBanner::Level::Neutral, "", "");
    m_banner->setMarginTop(20.0f);
    m_banner->setMarginLeft(30.0f);
    m_banner->setMarginRight(30.0f);

    brls::Button* verify = m_banner->addAction("Re-check");
    verify->registerClickAction([this](brls::View*) {
        RomService::instance().invalidate();
        RomService::instance().refresh([this]() { refreshBaseRomBanner(); });
        refreshBaseRomBanner();
        return true;
    });

    this->addView(m_banner);

    m_list = new HackListPanel(/*installedOnly=*/true, "★",
        "Nothing installed yet",
        "Head to Browse to pick a hack. It gets downloaded as a patch and applied to your "
        "own Super Mario 64 base ROM.");
    this->addView(m_list);

    refreshBaseRomBanner();

    // Hashing 8 MB would stall the first frames, so kick it off in the
    // background and let the banner update when the answer arrives.
    RomService::instance().refresh([this]() { refreshBaseRomBanner(); });
}

void LibraryTab::refreshBaseRomBanner() {
    RomService& service = RomService::instance();
    const std::string& path = AppSettings::instance().baseRomPath;

    switch (service.state()) {
        case RomService::State::Unchecked:
            m_banner->set(nxui::StatusBanner::Level::Neutral, "Base ROM", "Not checked yet");
            return;

        case RomService::State::Checking:
            m_banner->set(nxui::StatusBanner::Level::Neutral, "Checking base ROM",
                "Hashing " + path);
            return;

        case RomService::State::Done:
            break;
    }

    const rom::BaseRomInfo& info = service.info();

    if (info.isUsableBase()) {
        std::string detail = "SHA-1 " + info.z64Sha1;
        if (!info.isZ64) {
            detail += "  ·  stored as " + std::string(rom::byteOrderName(info.byteOrder))
                + ", converted on load";
        }
        m_banner->set(nxui::StatusBanner::Level::Ok,
            std::string(rom::regionName(info.region)) + " verified", detail);
    } else if (!info.readable) {
        m_banner->set(nxui::StatusBanner::Level::Warn, "No base ROM",
            "Copy a clean Super Mario 64 (USA) dump to " + path);
    } else {
        m_banner->set(nxui::StatusBanner::Level::Error, "Base ROM cannot be used", info.error);
    }
}

brls::View* LibraryTab::create() {
    return new LibraryTab();
}
