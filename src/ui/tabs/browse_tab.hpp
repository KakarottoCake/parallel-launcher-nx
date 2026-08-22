#pragma once

#include <borealis.hpp>

#include "src/ui/views/hack_list_panel.hpp"

// The full Romhacking.com catalog. Currently backed by the bundled sample set;
// the network client swaps in behind Catalog without this tab changing.
class BrowseTab : public brls::Box {
  public:
    BrowseTab();

    static brls::View* create();

  private:
    HackListPanel* m_list = nullptr;
};
