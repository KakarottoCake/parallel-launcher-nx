#include "src/ui/views/hack_card.hpp"

HackCard::HackCard() {
    // The recycler gives every cell a separator line by default; these cards
    // carry their own background so the line would only add noise.
    this->setLineTop(0.0f);
    this->setLineBottom(0.0f);

    this->setHeight(kHeight);
    this->setAxis(brls::Axis::COLUMN);
    this->setPadding(0.0f, 0.0f, 10.0f, 0.0f);

    m_card = new brls::Box(brls::Axis::ROW);
    m_card->setGrow(1.0f);
    m_card->setAlignItems(brls::AlignItems::CENTER);
    m_card->setCornerRadius(10.0f);
    m_card->setPadding(12.0f, 16.0f, 12.0f, 14.0f);
    this->addView(m_card);

    m_thumb = new nxui::HackThumb("", 0, 124.0f, 78.0f);
    m_thumb->setMarginRight(16.0f);
    m_card->addView(m_thumb);

    // ---- centre column -----------------------------------------------------
    brls::Box* centre = new brls::Box(brls::Axis::COLUMN);
    centre->setGrow(1.0f);
    centre->setJustifyContent(brls::JustifyContent::CENTER);

    m_title = new brls::Label();
    m_title->setFontSize(20.0f);
    m_title->setSingleLine(true);
    centre->addView(m_title);

    m_byline = new brls::Label();
    m_byline->setFontSize(14.0f);
    m_byline->setSingleLine(true);
    m_byline->setMarginTop(4.0f);
    centre->addView(m_byline);

    brls::Box* stats = new brls::Box(brls::Axis::ROW);
    stats->setAlignItems(brls::AlignItems::CENTER);
    stats->setMarginTop(7.0f);

    m_rating = new brls::Label();
    m_rating->setFontSize(14.0f);
    m_rating->setSingleLine(true);
    m_rating->setMarginRight(14.0f);
    stats->addView(m_rating);

    m_downloads = new brls::Label();
    m_downloads->setFontSize(14.0f);
    m_downloads->setSingleLine(true);
    m_downloads->setMarginRight(14.0f);
    stats->addView(m_downloads);

    m_difficulty = new nxui::Chip("Unrated", []() { return nxui::difficultyColor(0.0); },
        nxui::Chip::Style::Outline, 12.0f);
    stats->addView(m_difficulty);

    centre->addView(stats);
    m_card->addView(centre);

    // ---- right column ------------------------------------------------------
    brls::Box* right = new brls::Box(brls::Axis::COLUMN);
    right->setAlignItems(brls::AlignItems::FLEX_END);
    right->setJustifyContent(brls::JustifyContent::CENTER);
    right->setMarginLeft(14.0f);

    m_plugin = nxui::makePluginBadge(GfxPlugin::UseDefault);
    right->addView(m_plugin);

    m_installed = new brls::Label();
    m_installed->setFontSize(13.0f);
    m_installed->setSingleLine(true);
    m_installed->setMarginTop(8.0f);
    m_installed->setText("Installed");
    m_installed->setVisibility(brls::Visibility::GONE);
    right->addView(m_installed);

    m_card->addView(right);
}

void HackCard::setHack(const HackInfo& hack) {
    m_thumb->setHack(hack.id, hack.starCount);

    m_title->setText(hack.name);
    m_byline->setText(hack.author + "  ·  " + hack.category);
    m_rating->setText(nxui::formatRating(hack.rating) + "★");
    m_downloads->setText(nxui::formatDownloads(hack.downloads));

    const double difficulty = hack.difficulty;
    m_difficulty->setText(nxui::difficultyName(difficulty));
    m_difficulty->setColorProvider([difficulty]() { return nxui::difficultyColor(difficulty); });

    // Show the renderer that will actually be used, not the raw author field,
    // so a per-hack override is visible at a glance from the list.
    const GfxPlugin plugin = hack.pluginOverride != GfxPlugin::UseDefault
        ? hack.pluginOverride
        : hack.defaultPlugin;
    m_plugin->setText(getGfxPluginShortName(plugin));
    m_plugin->setColorProvider([plugin]() { return nxui::pluginColor(plugin); });

    m_installed->setVisibility(hack.isInstalled ? brls::Visibility::VISIBLE
                                                : brls::Visibility::GONE);

    applyTheme();
}

void HackCard::prepareForReuse() {
    m_installed->setVisibility(brls::Visibility::GONE);
}

void HackCard::applyTheme() {
    m_card->setBackgroundColor(nxui::color("nx/card"));
    m_byline->setTextColor(nxui::color("nx/text_secondary"));
    m_rating->setTextColor(nxui::color("nx/star"));
    m_downloads->setTextColor(nxui::color("nx/text_secondary"));
    m_installed->setTextColor(nxui::color("nx/ok_fg"));
}

void HackCard::draw(NVGcontext* vg, float x, float y, float width, float height,
    brls::Style style, brls::FrameContext* ctx) {
    if (needsThemeRefresh())
        applyTheme();

    // setIndexPath() re-enables a top line on the first row and the global
    // input-type handler recolours it; keep it invisible either way.
    this->setLineColor(nvgRGBA(0, 0, 0, 0));

    brls::RecyclerCell::draw(vg, x, y, width, height, style, ctx);
}

HackCard* HackCard::create() {
    return new HackCard();
}
