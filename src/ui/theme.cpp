#include "src/ui/theme.hpp"

#include <cmath>

namespace nxui {

namespace {

struct ColorPair {
    const char* name;
    NVGcolor    light;
    NVGcolor    dark;
};

// Every custom colour the app uses, defined for both variants so nothing
// falls back to an unreadable default when the user flips the theme.
const ColorPair kColors[] = {
    // Surfaces
    { "nx/card",            nvgRGB(0xFF, 0xFF, 0xFF), nvgRGB(0x2D, 0x2F, 0x36) },
    { "nx/card_alt",        nvgRGB(0xF4, 0xF4, 0xF6), nvgRGB(0x25, 0x27, 0x2D) },
    { "nx/card_border",     nvgRGB(0xE2, 0xE2, 0xE6), nvgRGB(0x3A, 0x3D, 0x45) },
    { "nx/panel",           nvgRGB(0xFA, 0xFA, 0xFB), nvgRGB(0x21, 0x23, 0x28) },

    // Text
    { "nx/text_secondary",  nvgRGB(0x6B, 0x70, 0x7B), nvgRGB(0x9C, 0xA3, 0xAF) },
    { "nx/text_tertiary",   nvgRGB(0x98, 0x9D, 0xA8), nvgRGB(0x6E, 0x74, 0x80) },

    // Accents
    { "nx/star",            nvgRGB(0xC8, 0x8A, 0x00), nvgRGB(0xFF, 0xCC, 0x33) },
    { "nx/accent",          nvgRGB(0xD8, 0x3A, 0x3F), nvgRGB(0xE5, 0x48, 0x4D) },

    // Status banners
    { "nx/ok_bg",           nvgRGB(0xDC, 0xF7, 0xE9), nvgRGB(0x14, 0x2C, 0x21) },
    { "nx/ok_fg",           nvgRGB(0x0E, 0x6B, 0x45), nvgRGB(0x5C, 0xD9, 0x9B) },
    { "nx/warn_bg",         nvgRGB(0xFF, 0xF4, 0xD6), nvgRGB(0x3A, 0x2C, 0x10) },
    { "nx/warn_fg",         nvgRGB(0x8A, 0x5D, 0x00), nvgRGB(0xF5, 0xC1, 0x4B) },
    { "nx/error_bg",        nvgRGB(0xFF, 0xE2, 0xE4), nvgRGB(0x3C, 0x16, 0x1A) },
    { "nx/error_fg",        nvgRGB(0xA8, 0x1F, 0x28), nvgRGB(0xF8, 0x71, 0x77) },

    // Graphics plugins. Each renderer keeps one colour everywhere it appears.
    { "nx/plugin_parallel",  nvgRGB(0xD8, 0x3A, 0x3F), nvgRGB(0xE5, 0x48, 0x4D) },
    { "nx/plugin_gliden64",  nvgRGB(0x25, 0x63, 0xEB), nvgRGB(0x3B, 0x82, 0xF6) },
    { "nx/plugin_ogre",      nvgRGB(0x0F, 0x82, 0x55), nvgRGB(0x22, 0xA0, 0x6B) },
    { "nx/plugin_other",     nvgRGB(0x7C, 0x3A, 0xED), nvgRGB(0xA8, 0x55, 0xF7) },
    { "nx/plugin_auto",      nvgRGB(0x6B, 0x70, 0x7B), nvgRGB(0x6B, 0x72, 0x80) },

    // Difficulty scale, 0..5
    { "nx/diff_easy",       nvgRGB(0x0F, 0x82, 0x55), nvgRGB(0x34, 0xD3, 0x99) },
    { "nx/diff_normal",     nvgRGB(0x25, 0x63, 0xEB), nvgRGB(0x60, 0xA5, 0xFA) },
    { "nx/diff_hard",       nvgRGB(0xB4, 0x7A, 0x00), nvgRGB(0xFB, 0xBF, 0x24) },
    { "nx/diff_veryhard",   nvgRGB(0xC2, 0x54, 0x0C), nvgRGB(0xFB, 0x92, 0x3C) },
    { "nx/diff_kaizo",      nvgRGB(0xB0, 0x1B, 0x24), nvgRGB(0xF8, 0x71, 0x77) },
};

} // namespace

void registerThemeColors() {
    brls::Theme& light = brls::Theme::getLightTheme();
    brls::Theme& dark  = brls::Theme::getDarkTheme();

    for (const ColorPair& c : kColors) {
        light.addColor(c.name, c.light);
        dark.addColor(c.name, c.dark);
    }
}

NVGcolor color(const std::string& name) {
    return brls::Application::getTheme()[name];
}

NVGcolor pluginColor(GfxPlugin plugin) {
    switch (plugin) {
        case GfxPlugin::ParaLLEl:   return color("nx/plugin_parallel");
        case GfxPlugin::GLideN64:   return color("nx/plugin_gliden64");
        case GfxPlugin::OGRE:       return color("nx/plugin_ogre");
        case GfxPlugin::UseDefault: return color("nx/plugin_auto");
        default:                    return color("nx/plugin_other");
    }
}

NVGcolor difficultyColor(double difficulty) {
    if (difficulty <= 0.0)  return color("nx/text_tertiary");
    if (difficulty <= 1.5)  return color("nx/diff_easy");
    if (difficulty <= 2.5)  return color("nx/diff_normal");
    if (difficulty <= 3.5)  return color("nx/diff_hard");
    if (difficulty <= 4.5)  return color("nx/diff_veryhard");
    return color("nx/diff_kaizo");
}

const char* difficultyName(double difficulty) {
    if (difficulty <= 0.0)  return "Unrated";
    if (difficulty <= 1.5)  return "Casual";
    if (difficulty <= 2.5)  return "Normal";
    if (difficulty <= 3.5)  return "Hard";
    if (difficulty <= 4.5)  return "Very Hard";
    return "Kaizo";
}

NVGcolor hashColor(const std::string& seed) {
    // FNV-1a, then map the low bits onto a fixed-saturation hue wheel so every
    // tile stays legible against white text in both themes.
    uint32_t hash = 2166136261u;
    for (unsigned char ch : seed) {
        hash ^= ch;
        hash *= 16777619u;
    }

    const float hue = static_cast<float>(hash % 360) / 360.0f;
    const bool  dark = brls::Application::getThemeVariant() == brls::ThemeVariant::DARK;
    return nvgHSL(hue, 0.42f, dark ? 0.32f : 0.46f);
}

bool ThemeAware::needsThemeRefresh() {
    const int current = static_cast<int>(brls::Application::getThemeVariant());
    if (current == m_appliedVariant)
        return false;

    m_appliedVariant = current;
    return true;
}

} // namespace nxui
