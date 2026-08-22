#pragma once

#include <borealis.hpp>

#include "src/ui/views/hack_list_panel.hpp"
#include "src/ui/views/widgets.hpp"

// Hacks installed on this console, plus the base-ROM status strip that gates
// everything else the launcher can do.
class LibraryTab : public brls::Box {
  public:
    LibraryTab();

    static brls::View* create();

  private:
    void refreshBaseRomBanner();

    nxui::StatusBanner* m_banner = nullptr;
    HackListPanel*      m_list   = nullptr;
};
