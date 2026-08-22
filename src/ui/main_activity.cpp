#include "src/ui/main_activity.hpp"

#include "src/core/settings.hpp"

namespace {

constexpr int kTabCount = 4; // Library, Browse, Account, Settings

} // namespace

int MainActivity::sidebarRowForTab(int tab) {
    // Row 2 is the separator between Browse and Account.
    return tab <= 1 ? tab : tab + 1;
}

void MainActivity::onContentAvailable() {
    m_tabs    = dynamic_cast<brls::TabFrame*>(this->getView("nx/tabs"));
    m_sidebar = dynamic_cast<brls::Sidebar*>(this->getView("brls/tab_frame/sidebar"));

    if (!m_tabs || !m_sidebar) {
        brls::Logger::error("main.xml did not yield the expected tab frame and sidebar");
        return;
    }

    int tab = AppSettings::instance().lastTab;
    if (tab < 0 || tab >= kTabCount)
        tab = 0;

    if (tab != 0) {
        m_tabs->focusTab(sidebarRowForTab(tab));
        brls::Logger::debug("Restored tab {}", tab);
    }

    // Borealis has no "tab changed" event, so watch global focus changes and
    // record whichever sidebar row now owns the focus.
    m_focusSubscription = brls::Application::getGlobalFocusChangeEvent()->subscribe(
        [this](brls::View* focused) {
            if (!m_sidebar || !focused)
                return;

            for (int i = 0; i < kTabCount; ++i) {
                if (m_sidebar->getItem(sidebarRowForTab(i)) != focused)
                    continue;

                if (AppSettings::instance().lastTab != i) {
                    AppSettings::instance().lastTab = i;
                    AppSettings::instance().requestSave();
                }
                return;
            }
        });
    m_subscribed = true;
}

MainActivity::~MainActivity() {
    if (m_subscribed)
        brls::Application::getGlobalFocusChangeEvent()->unsubscribe(m_focusSubscription);
}
