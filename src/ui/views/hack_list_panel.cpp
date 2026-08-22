#include "src/ui/views/hack_list_panel.hpp"

#include "src/core/text_util.hpp"
#include "src/ui/views/hack_card.hpp"
#include "src/ui/views/hack_detail_view.hpp"

namespace {

// Kept in the same order as the sort dropdown so an index maps straight back.
const SortOrder kSortOrders[] = {
    SortOrder::Popular,
    SortOrder::TopRated,
    SortOrder::Newest,
    SortOrder::NameAsc,
    SortOrder::StarCount,
    SortOrder::Difficulty,
};

int indexOfSort(SortOrder order) {
    for (int i = 0; i < static_cast<int>(std::size(kSortOrders)); ++i) {
        if (kSortOrders[i] == order)
            return i;
    }
    return 0;
}

} // namespace

HackListPanel::HackListPanel(bool installedOnly, const std::string& emptyGlyph,
    const std::string& emptyTitle, const std::string& emptySubtitle)
    : m_emptyTitle(emptyTitle)
    , m_emptySubtitle(emptySubtitle) {
    m_query.installedOnly = installedOnly;
    m_categories          = Catalog::instance().categories();

    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(1.0f);
    this->setPadding(20.0f, 30.0f, 0.0f, 30.0f);

    buildToolbar();

    m_countLabel = new brls::Label();
    m_countLabel->setFontSize(14.0f);
    m_countLabel->setMarginBottom(10.0f);
    this->addView(m_countLabel);

    // RecyclerFrame places its cells starting at y=0 of its own content, but
    // culls them against getVisibleFrame(), whose origin is the frame's offset
    // inside its *parent*. Any sibling above the recycler in the same flex box
    // therefore shifts that origin down and the top rows get thrown away. Give
    // the recycler a dedicated host box so its offset inside it is always zero.
    brls::Box* listHost = new brls::Box(brls::Axis::COLUMN);
    listHost->setGrow(1.0f);

    m_recycler = new brls::RecyclerFrame();
    m_recycler->setGrow(1.0f);
    m_recycler->estimatedRowHeight = HackCard::kHeight;
    m_recycler->registerCell("Hack", []() { return HackCard::create(); });
    // The panel owns itself; the recycler must not delete its data source.
    m_recycler->setDataSource(this, false);
    listHost->addView(m_recycler);
    this->addView(listHost);
    m_listHost = listHost;

    m_empty = new nxui::EmptyState(emptyGlyph, emptyTitle, emptySubtitle);
    m_empty->setVisibility(brls::Visibility::GONE);
    this->addView(m_empty);

    reload();
}

void HackListPanel::buildToolbar() {
    brls::Box* toolbar = new brls::Box(brls::Axis::ROW);
    toolbar->setAlignItems(brls::AlignItems::CENTER);
    toolbar->setMarginBottom(12.0f);

    m_searchButton = new brls::Button();
    m_searchButton->setStyle(&brls::BUTTONSTYLE_BORDERED);
    m_searchButton->setGrow(1.0f);
    m_searchButton->setMarginRight(10.0f);
    m_searchButton->registerClickAction([this](brls::View*) {
        openSearch();
        return true;
    });
    toolbar->addView(m_searchButton);

    m_categoryButton = new brls::Button();
    m_categoryButton->setStyle(&brls::BUTTONSTYLE_BORDERED);
    m_categoryButton->setMarginRight(10.0f);
    m_categoryButton->registerClickAction([this](brls::View*) {
        openCategoryPicker();
        return true;
    });
    toolbar->addView(m_categoryButton);

    m_sortButton = new brls::Button();
    m_sortButton->setStyle(&brls::BUTTONSTYLE_BORDERED);
    m_sortButton->registerClickAction([this](brls::View*) {
        openSortPicker();
        return true;
    });
    toolbar->addView(m_sortButton);

    this->addView(toolbar);
}

void HackListPanel::openSearch() {
    brls::Application::getImeManager()->openForText(
        [this](std::string text) {
            m_query.search = textutil::sanitizeUserInput(text, 64);
            reload();
        },
        "Search hacks", "Matches title, author and category", 64, m_query.search,
        brls::KeyboardKeyDisableBitmask::KEYBOARD_DISABLE_NONE);
}

void HackListPanel::openCategoryPicker() {
    int selected = 0;
    for (int i = 0; i < static_cast<int>(m_categories.size()); ++i) {
        if (m_categories[i] == m_query.category) {
            selected = i;
            break;
        }
    }

    brls::Dropdown* dropdown = new brls::Dropdown(
        "Category", m_categories,
        [this](int index) {
            if (index >= 0 && index < static_cast<int>(m_categories.size())) {
                m_query.category = m_categories[index];
                reload();
            }
        },
        selected);
    brls::Application::pushActivity(new brls::Activity(dropdown));
}

void HackListPanel::openSortPicker() {
    std::vector<std::string> names;
    names.reserve(std::size(kSortOrders));
    for (SortOrder order : kSortOrders)
        names.emplace_back(getSortOrderName(order));

    brls::Dropdown* dropdown = new brls::Dropdown(
        "Sort by", names,
        [this](int index) {
            if (index >= 0 && index < static_cast<int>(std::size(kSortOrders))) {
                m_query.sort = kSortOrders[index];
                reload();
            }
        },
        indexOfSort(m_query.sort));
    brls::Application::pushActivity(new brls::Activity(dropdown));
}

void HackListPanel::updateToolbarLabels() {
    m_searchButton->setText(m_query.search.empty() ? "Search…"
                                                   : "Search: " + m_query.search);
    m_categoryButton->setText(m_query.category);
    m_sortButton->setText(getSortOrderName(m_query.sort));
}

void HackListPanel::reload() {
    m_results = Catalog::instance().query(m_query);

    updateToolbarLabels();

    const bool isFiltered = !m_query.search.empty() || m_query.category != "All";
    const int  count      = static_cast<int>(m_results.size());

    m_countLabel->setText(count == 1 ? "1 hack" : std::to_string(count) + " hacks");

    brls::Logger::debug("List reload: {} results", count);

    if (count == 0) {
        m_listHost->setVisibility(brls::Visibility::GONE);
        m_empty->setVisibility(brls::Visibility::VISIBLE);
        if (isFiltered) {
            m_empty->setTitle("No matches");
            m_empty->setSubtitle("Nothing here fits the current search and category. "
                                 "Clear the filters to see everything again.");
        } else {
            m_empty->setTitle(m_emptyTitle);
            m_empty->setSubtitle(m_emptySubtitle);
        }
    } else {
        m_empty->setVisibility(brls::Visibility::GONE);
        m_listHost->setVisibility(brls::Visibility::VISIBLE);
    }

    m_recycler->reloadData();
}

int HackListPanel::numberOfRows(brls::RecyclerFrame*, int) {
    return static_cast<int>(m_results.size());
}

brls::RecyclerCell* HackListPanel::cellForRow(brls::RecyclerFrame* recycler,
    brls::IndexPath index) {
    HackCard* cell = static_cast<HackCard*>(recycler->dequeueReusableCell("Hack"));
    if (index.row >= 0 && index.row < static_cast<int>(m_results.size()))
        cell->setHack(*m_results[index.row]);
    return cell;
}

float HackListPanel::heightForRow(brls::RecyclerFrame*, brls::IndexPath) {
    return HackCard::kHeight;
}

void HackListPanel::didSelectRowAt(brls::RecyclerFrame*, brls::IndexPath index) {
    if (index.row < 0 || index.row >= static_cast<int>(m_results.size()))
        return;

    const std::string id = m_results[index.row]->id;
    HackDetailView::show(id, [this]() { reload(); });
}

bool HackListPanel::focusIsInsideList() const {
    for (brls::View* view = brls::Application::getCurrentFocus(); view; view = view->getParent()) {
        if (view == m_recycler)
            return true;
    }
    return false;
}

void HackListPanel::applyTheme() {
    m_countLabel->setTextColor(nxui::color("nx/text_secondary"));
}

void HackListPanel::draw(NVGcontext* vg, float x, float y, float width, float height,
    brls::Style style, brls::FrameContext* ctx) {
    if (needsThemeRefresh())
        applyTheme();

    // RecyclerFrame settles on a non-zero scroll offset while the surrounding
    // flex box is still being measured, which silently hides the first rows.
    // Pin it to the top until the user actually moves focus into the list,
    // which also gives filter changes the "jump back to the top" behaviour
    // you want from a search box.
    if (!focusIsInsideList() && m_recycler->getContentOffsetY() != 0.0f)
        m_recycler->setContentOffsetY(0.0f, false);
    brls::Box::draw(vg, x, y, width, height, style, ctx);
}
