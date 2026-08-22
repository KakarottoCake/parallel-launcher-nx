#pragma once

#include <string>
#include <vector>

#include <borealis.hpp>

#include "src/core/catalog.hpp"
#include "src/ui/theme.hpp"
#include "src/ui/views/widgets.hpp"

// Search + filter + sort toolbar over a recycled list of HackCards.
// Shared by the Library tab (installed only) and the Browse tab (everything),
// which differ only in the initial query and the empty-state copy.
class HackListPanel : public brls::Box,
                      private brls::RecyclerDataSource,
                      public nxui::ThemeAware {
  public:
    HackListPanel(bool installedOnly, const std::string& emptyGlyph,
        const std::string& emptyTitle, const std::string& emptySubtitle);

    // Re-runs the query and refreshes the list. Call after anything that can
    // change the catalog (install, delete, login sync).
    void reload();

    void draw(NVGcontext* vg, float x, float y, float width, float height,
        brls::Style style, brls::FrameContext* ctx) override;

  private:
    // RecyclerDataSource
    int                numberOfRows(brls::RecyclerFrame* recycler, int section) override;
    brls::RecyclerCell* cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override;
    float              heightForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override;
    void               didSelectRowAt(brls::RecyclerFrame* recycler, brls::IndexPath index) override;

    // True while the focused view lives somewhere inside the recycler.
    bool focusIsInsideList() const;

    void applyTheme();
    void buildToolbar();
    void openSearch();
    void openCategoryPicker();
    void openSortPicker();
    void updateToolbarLabels();

    CatalogQuery                 m_query;
    std::vector<const HackInfo*> m_results;
    std::vector<std::string>     m_categories;

    brls::Button*      m_searchButton   = nullptr;
    brls::Button*      m_categoryButton = nullptr;
    brls::Button*      m_sortButton     = nullptr;
    brls::Label*       m_countLabel     = nullptr;
    brls::Box*           m_listHost     = nullptr;
    brls::RecyclerFrame* m_recycler     = nullptr;
    nxui::EmptyState*  m_empty          = nullptr;

    std::string m_emptyTitle;
    std::string m_emptySubtitle;
};
