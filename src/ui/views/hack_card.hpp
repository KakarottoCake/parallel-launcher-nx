#pragma once

#include <borealis.hpp>

#include "src/core/models.hpp"
#include "src/ui/theme.hpp"
#include "src/ui/views/widgets.hpp"

// One row in the library / browse lists. Built as a RecyclerCell so long
// catalogs only ever allocate as many views as fit on screen.
//
// The visible card is an inner box inset from the cell bounds; the cell's own
// bottom padding is what produces the gap between cards.
class HackCard : public brls::RecyclerCell, public nxui::ThemeAware {
  public:
    HackCard();

    // Reconfigures an existing (possibly recycled) card for a new hack.
    void setHack(const HackInfo& hack);

    void prepareForReuse() override;

    void draw(NVGcontext* vg, float x, float y, float width, float height,
        brls::Style style, brls::FrameContext* ctx) override;

    static constexpr float kHeight = 116.0f;

    static HackCard* create();

  private:
    void applyTheme();

    brls::Box*      m_card       = nullptr;
    nxui::HackThumb* m_thumb     = nullptr;
    brls::Label*    m_title      = nullptr;
    brls::Label*    m_byline     = nullptr;
    brls::Label*    m_rating     = nullptr;
    brls::Label*    m_downloads  = nullptr;
    nxui::Chip*     m_difficulty = nullptr;
    nxui::Chip*     m_plugin     = nullptr;
    brls::Label*    m_installed  = nullptr;
};
