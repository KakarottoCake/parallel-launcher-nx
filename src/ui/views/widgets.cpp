#include "src/ui/views/widgets.hpp"

#include <cstdio>

namespace nxui {

// ---------------------------------------------------------------------------
// Chip
// ---------------------------------------------------------------------------

Chip::Chip(const std::string& text, std::function<NVGcolor()> colorProvider, Style style,
    float fontSize)
    : m_colorProvider(std::move(colorProvider))
    , m_style(style) {
    this->setAxis(brls::Axis::ROW);
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setJustifyContent(brls::JustifyContent::CENTER);
    this->setCornerRadius(11.0f);
    this->setPadding(3.0f, 10.0f, 3.0f, 10.0f);
    this->setHeight(brls::View::AUTO);

    m_label = new brls::Label();
    m_label->setFontSize(fontSize);
    m_label->setSingleLine(true);
    m_label->setText(text);
    this->addView(m_label);

    if (m_style == Style::Outline)
        this->setBorderThickness(1.5f);
}

void Chip::setText(const std::string& text) {
    m_label->setText(text);
}

void Chip::setColorProvider(std::function<NVGcolor()> provider) {
    m_colorProvider = std::move(provider);
    applyTheme();
}

void Chip::applyTheme() {
    if (!m_colorProvider)
        return;

    const NVGcolor c = m_colorProvider();
    if (m_style == Style::Filled) {
        this->setBackgroundColor(c);
        m_label->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
    } else {
        this->setBackground(brls::ViewBackground::NONE);
        this->setBorderColor(c);
        m_label->setTextColor(c);
    }
}

void Chip::draw(NVGcontext* vg, float x, float y, float width, float height,
    brls::Style style, brls::FrameContext* ctx) {
    if (needsThemeRefresh())
        applyTheme();
    brls::Box::draw(vg, x, y, width, height, style, ctx);
}

Chip* makePluginBadge(GfxPlugin plugin, Chip::Style style, float fontSize) {
    return new Chip(getGfxPluginShortName(plugin),
        [plugin]() { return pluginColor(plugin); }, style, fontSize);
}

Chip* makeDifficultyChip(double difficulty, float fontSize) {
    return new Chip(difficultyName(difficulty),
        [difficulty]() { return difficultyColor(difficulty); }, Chip::Style::Outline, fontSize);
}

// ---------------------------------------------------------------------------
// HackThumb
// ---------------------------------------------------------------------------

HackThumb::HackThumb(const std::string& seed, int starCount, float width, float height)
    : m_seed(seed) {
    this->setAxis(brls::Axis::COLUMN);
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setJustifyContent(brls::JustifyContent::CENTER);
    this->setCornerRadius(8.0f);
    this->setWidth(width);
    this->setHeight(height);

    m_starLabel = new brls::Label();
    m_starLabel->setFontSize(height >= 140.0f ? 34.0f : 20.0f);
    m_starLabel->setSingleLine(true);
    m_starLabel->setTextColor(nvgRGBA(0xFF, 0xFF, 0xFF, 0xF2));
    m_starLabel->setText(std::to_string(starCount) + "★");
    this->addView(m_starLabel);
}

void HackThumb::setHack(const std::string& seed, int starCount) {
    m_seed = seed;
    m_starLabel->setText(std::to_string(starCount) + "★");
    this->setBackgroundColor(hashColor(m_seed));
}

void HackThumb::applyTheme() {
    this->setBackgroundColor(hashColor(m_seed));
}

void HackThumb::draw(NVGcontext* vg, float x, float y, float width, float height,
    brls::Style style, brls::FrameContext* ctx) {
    if (needsThemeRefresh())
        applyTheme();
    brls::Box::draw(vg, x, y, width, height, style, ctx);
}

// ---------------------------------------------------------------------------
// EmptyState
// ---------------------------------------------------------------------------

EmptyState::EmptyState(const std::string& glyph, const std::string& title,
    const std::string& subtitle) {
    this->setAxis(brls::Axis::COLUMN);
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setJustifyContent(brls::JustifyContent::CENTER);
    this->setPadding(48.0f, 40.0f, 48.0f, 40.0f);
    this->setGrow(1.0f);

    m_glyph = new brls::Label();
    m_glyph->setFontSize(48.0f);
    m_glyph->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    m_glyph->setText(glyph);
    m_glyph->setMarginBottom(12.0f);
    this->addView(m_glyph);

    m_title = new brls::Label();
    m_title->setFontSize(22.0f);
    m_title->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    m_title->setText(title);
    m_title->setMarginBottom(8.0f);
    this->addView(m_title);

    m_subtitle = new brls::Label();
    m_subtitle->setFontSize(15.0f);
    m_subtitle->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    m_subtitle->setText(subtitle);
    this->addView(m_subtitle);
}

void EmptyState::setTitle(const std::string& title) {
    m_title->setText(title);
}

void EmptyState::setSubtitle(const std::string& subtitle) {
    m_subtitle->setText(subtitle);
}

void EmptyState::applyTheme() {
    m_glyph->setTextColor(color("nx/text_tertiary"));
    m_subtitle->setTextColor(color("nx/text_secondary"));
}

void EmptyState::draw(NVGcontext* vg, float x, float y, float width, float height,
    brls::Style style, brls::FrameContext* ctx) {
    if (needsThemeRefresh())
        applyTheme();
    brls::Box::draw(vg, x, y, width, height, style, ctx);
}

// ---------------------------------------------------------------------------
// StatusBanner
// ---------------------------------------------------------------------------

StatusBanner::StatusBanner(Level level, const std::string& title, const std::string& detail)
    : m_level(level) {
    this->setAxis(brls::Axis::ROW);
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setJustifyContent(brls::JustifyContent::SPACE_BETWEEN);
    this->setPadding(14.0f, 18.0f, 14.0f, 18.0f);
    this->setCornerRadius(10.0f);

    brls::Box* textBox = new brls::Box(brls::Axis::COLUMN);
    textBox->setGrow(1.0f);
    textBox->setJustifyContent(brls::JustifyContent::CENTER);

    m_title = new brls::Label();
    m_title->setFontSize(17.0f);
    m_title->setText(title);
    textBox->addView(m_title);

    m_detail = new brls::Label();
    m_detail->setFontSize(14.0f);
    m_detail->setMarginTop(3.0f);
    m_detail->setText(detail);
    textBox->addView(m_detail);

    this->addView(textBox);

    m_actions = new brls::Box(brls::Axis::ROW);
    m_actions->setAlignItems(brls::AlignItems::CENTER);
    m_actions->setMarginLeft(16.0f);
    this->addView(m_actions);
}

void StatusBanner::set(Level level, const std::string& title, const std::string& detail) {
    m_level = level;
    m_title->setText(title);
    m_detail->setText(detail);
    applyTheme();
}

brls::Button* StatusBanner::addAction(const std::string& text) {
    brls::Button* button = new brls::Button();
    button->setText(text);
    button->setStyle(&brls::BUTTONSTYLE_BORDERED);
    button->setMarginLeft(8.0f);
    m_actions->addView(button);
    return button;
}

void StatusBanner::applyTheme() {
    const char* bg = "nx/card";
    const char* fg = "nx/text_secondary";

    switch (m_level) {
        case Level::Ok:    bg = "nx/ok_bg";    fg = "nx/ok_fg";    break;
        case Level::Warn:  bg = "nx/warn_bg";  fg = "nx/warn_fg";  break;
        case Level::Error: bg = "nx/error_bg"; fg = "nx/error_fg"; break;
        case Level::Neutral:
        default: break;
    }

    this->setBackgroundColor(color(bg));
    m_title->setTextColor(brls::Application::getTheme()["brls/text"]);
    m_detail->setTextColor(color(fg));
}

void StatusBanner::draw(NVGcontext* vg, float x, float y, float width, float height,
    brls::Style style, brls::FrameContext* ctx) {
    if (needsThemeRefresh())
        applyTheme();
    brls::Box::draw(vg, x, y, width, height, style, ctx);
}

// ---------------------------------------------------------------------------
// PasswordCell
// ---------------------------------------------------------------------------

PasswordCell::PasswordCell() {
    this->registerClickAction([this](brls::View*) {
        brls::Application::getImeManager()->openForText(
            [this](std::string text) {
                this->setValue(text);
            },
            this->title->getFullText(), "Your password is never written to the SD card in "
                                        "plain text.",
            64, "", brls::KeyboardKeyDisableBitmask::KEYBOARD_DISABLE_NONE);
        return true;
    });
}

void PasswordCell::init(const std::string& title, std::function<void(std::string)> callback,
    const std::string& placeholder) {
    this->title->setText(title);
    m_placeholder = placeholder;
    m_callback    = std::move(callback);
    updateUI();
}

void PasswordCell::setValue(const std::string& value) {
    m_value = value;
    if (m_callback)
        m_callback(m_value);
    updateUI();
}

void PasswordCell::clear() {
    m_value.clear();
    updateUI();
}

void PasswordCell::updateUI() {
    brls::Theme theme = brls::Application::getTheme();
    if (m_value.empty()) {
        this->detail->setText(m_placeholder);
        this->detail->setTextColor(theme["brls/text_disabled"]);
    } else {
        this->detail->setText(std::string(m_value.size(), '*'));
        this->detail->setTextColor(theme["brls/list/listItem_value_color"]);
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

brls::Box* makeSectionHeader(const std::string& title, const std::string& subtitle) {
    brls::Box* box = new brls::Box(brls::Axis::COLUMN);
    box->setMarginTop(8.0f);
    box->setMarginBottom(10.0f);

    brls::Label* titleLabel = new brls::Label();
    titleLabel->setFontSize(21.0f);
    titleLabel->setText(title);
    box->addView(titleLabel);

    if (!subtitle.empty()) {
        brls::Label* subtitleLabel = new brls::Label();
        subtitleLabel->setFontSize(14.0f);
        subtitleLabel->setMarginTop(3.0f);
        subtitleLabel->setTextColor(color("nx/text_secondary"));
        subtitleLabel->setText(subtitle);
        box->addView(subtitleLabel);
    }

    return box;
}

std::string formatPlayTime(int64_t seconds) {
    if (seconds <= 0)
        return "Never played";
    if (seconds < 60)
        return "Under a minute";

    const int64_t hours   = seconds / 3600;
    const int64_t minutes = (seconds % 3600) / 60;

    if (hours == 0)
        return std::to_string(minutes) + "m played";
    return std::to_string(hours) + "h " + std::to_string(minutes) + "m played";
}

std::string formatDownloads(int downloads) {
    char buf[32];
    if (downloads >= 1000) {
        std::snprintf(buf, sizeof(buf), "%.1fk downloads", downloads / 1000.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%d downloads", downloads);
    }
    return buf;
}

std::string formatRating(double rating) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.2f", rating);
    return buf;
}

} // namespace nxui
