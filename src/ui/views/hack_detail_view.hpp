#pragma once

#include <functional>
#include <string>

#include <borealis.hpp>

#include "src/core/models.hpp"
#include "src/ui/theme.hpp"
#include "src/ui/views/widgets.hpp"

// Full-screen detail page for a single hack: artwork, metadata, renderer
// override, version picker and the install / play actions.
class HackDetailView : public brls::Box, public nxui::ThemeAware {
  public:
    // onChanged fires whenever this view mutates the catalog entry, so the
    // list underneath can refresh when the user backs out.
    HackDetailView(const std::string& hackId, std::function<void()> onChanged);

    static void show(const std::string& hackId, std::function<void()> onChanged);

    void draw(NVGcontext* vg, float x, float y, float width, float height,
        brls::Style style, brls::FrameContext* ctx) override;

  private:
    void applyTheme();
    void buildHero(brls::Box* parent);
    void buildRendererSection(brls::Box* parent);
    void buildVersionSection(brls::Box* parent);
    void buildRequirements(brls::Box* parent);
    void buildDescription(brls::Box* parent);
    void buildLocalInfo(brls::Box* parent);

    void refreshActions();
    void onPrimaryAction();
    void startInstall();
    void startLaunch();
    void onRemove();

    HackInfo*             m_hack = nullptr; // owned by Catalog
    std::function<void()> m_onChanged;

    brls::Label*  m_byline        = nullptr;
    brls::Label*  m_downloads     = nullptr;
    brls::Label*  m_rating        = nullptr;
    brls::Label*  m_rendererBlurb = nullptr;
    brls::Label*  m_description   = nullptr;
    brls::Label*  m_localInfo     = nullptr;
    brls::Button* m_primaryButton = nullptr;
    brls::Button* m_removeButton  = nullptr;
    nxui::Chip*   m_pluginChip    = nullptr;
    std::vector<brls::Label*> m_secondaryLabels;
};
