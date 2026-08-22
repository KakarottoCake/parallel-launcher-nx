#pragma once

#include <borealis.hpp>

// Root activity: the sidebar tab frame declared in xml/activity/main.xml.
// Remembers which tab you were on so relaunching drops you back where you
// were rather than always on Library.
class MainActivity : public brls::Activity {
  public:
    CONTENT_FROM_XML_RES("activity/main.xml");

    ~MainActivity() override;

    void onContentAvailable() override;

  private:
    // Sidebar rows include separators, so a logical tab index and a sidebar
    // row index are not the same number.
    static int sidebarRowForTab(int tab);

    brls::Sidebar*              m_sidebar = nullptr;
    brls::TabFrame*             m_tabs    = nullptr;
    brls::GenericEvent::Subscription m_focusSubscription {};
    bool                        m_subscribed = false;
};
