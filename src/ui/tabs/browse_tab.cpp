#include "src/ui/tabs/browse_tab.hpp"

BrowseTab::BrowseTab() {
    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(1.0f);

    m_list = new HackListPanel(/*installedOnly=*/false, "?",
        "Catalog is empty",
        "No hacks were loaded. Check your internet connection and try again.");
    this->addView(m_list);
}

brls::View* BrowseTab::create() {
    return new BrowseTab();
}
