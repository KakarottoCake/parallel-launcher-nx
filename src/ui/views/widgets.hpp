#pragma once

#include <functional>
#include <string>

#include <borealis.hpp>

#include "src/types.hpp"
#include "src/ui/theme.hpp"

namespace nxui {

// A small rounded pill of coloured text. The colour is supplied as a callback
// rather than a value so the chip can recolour itself when the user flips
// between the light and dark themes.
class Chip : public brls::Box, public ThemeAware {
  public:
    enum class Style {
        Filled,  // solid background, white text
        Outline, // transparent background, coloured text and border
    };

    Chip(const std::string& text, std::function<NVGcolor()> colorProvider,
        Style style = Style::Filled, float fontSize = 13.0f);

    void setText(const std::string& text);
    void setColorProvider(std::function<NVGcolor()> provider);

    void draw(NVGcontext* vg, float x, float y, float width, float height,
        brls::Style style, brls::FrameContext* ctx) override;

  private:
    void applyTheme();

    brls::Label*              m_label = nullptr;
    std::function<NVGcolor()> m_colorProvider;
    Style                     m_style;
};

Chip* makePluginBadge(GfxPlugin plugin, Chip::Style style = Chip::Style::Filled,
    float fontSize = 13.0f);
Chip* makeDifficultyChip(double difficulty, float fontSize = 13.0f);

// Placeholder artwork for a hack. Real thumbnails are downloaded later; until
// then each hack gets a stable colour derived from its id plus its star count,
// which reads as deliberate rather than as a broken image.
class HackThumb : public brls::Box, public ThemeAware {
  public:
    HackThumb(const std::string& seed, int starCount, float width, float height);

    void setHack(const std::string& seed, int starCount);

    void draw(NVGcontext* vg, float x, float y, float width, float height,
        brls::Style style, brls::FrameContext* ctx) override;

  private:
    void applyTheme();

    std::string  m_seed;
    brls::Label* m_starLabel = nullptr;
};

// Centred "nothing here" panel used by the library and browse lists.
class EmptyState : public brls::Box, public ThemeAware {
  public:
    EmptyState(const std::string& glyph, const std::string& title,
        const std::string& subtitle);

    void setTitle(const std::string& title);
    void setSubtitle(const std::string& subtitle);

    void draw(NVGcontext* vg, float x, float y, float width, float height,
        brls::Style style, brls::FrameContext* ctx) override;

  private:
    void applyTheme();

    brls::Label* m_glyph    = nullptr;
    brls::Label* m_title    = nullptr;
    brls::Label* m_subtitle = nullptr;
};

// Coloured status strip: base ROM state, sync warnings, download errors.
class StatusBanner : public brls::Box, public ThemeAware {
  public:
    enum class Level { Ok, Warn, Error, Neutral };

    StatusBanner(Level level, const std::string& title, const std::string& detail);

    void set(Level level, const std::string& title, const std::string& detail);

    // Adds a trailing action button. Returns it so the caller can wire a click.
    brls::Button* addAction(const std::string& text);

    void draw(NVGcontext* vg, float x, float y, float width, float height,
        brls::Style style, brls::FrameContext* ctx) override;

  private:
    void applyTheme();

    Level        m_level  = Level::Neutral;
    brls::Label* m_title  = nullptr;
    brls::Label* m_detail = nullptr;
    brls::Box*   m_actions = nullptr;
};

// InputCell renders its value in the clear, which is wrong for a password.
// This variant keeps the real value private and shows bullets instead.
class PasswordCell : public brls::DetailCell {
  public:
    PasswordCell();

    void init(const std::string& title, std::function<void(std::string)> callback,
        const std::string& placeholder = "Required");

    const std::string& getValue() const { return m_value; }
    void               setValue(const std::string& value);
    void               clear();

  private:
    void updateUI();

    std::string                      m_value;
    std::string                      m_placeholder;
    std::function<void(std::string)> m_callback;
};

// Header row with a title on the left and free-form trailing content.
brls::Box* makeSectionHeader(const std::string& title, const std::string& subtitle = "");

// Formats 14400 -> "4h 0m", 300 -> "5m", 0 -> "Never played".
std::string formatPlayTime(int64_t seconds);
std::string formatDownloads(int downloads);
std::string formatRating(double rating);

} // namespace nxui
