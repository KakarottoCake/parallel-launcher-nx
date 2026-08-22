#pragma once

#include <string>

#include <borealis.hpp>

#include "src/types.hpp"

namespace nxui {

// Registers every "nx/*" colour on both Borealis themes.
// Call once, after brls::Application::init().
void registerThemeColors();

// Shorthand for brls::Application::getTheme()[name].
NVGcolor color(const std::string& name);

NVGcolor    pluginColor(GfxPlugin plugin);
NVGcolor    difficultyColor(double difficulty);
const char* difficultyName(double difficulty);

// Stable colour derived from an arbitrary string, used for thumbnail
// placeholders so each hack keeps the same tile colour between launches.
NVGcolor hashColor(const std::string& seed);

// Views that bake theme colours into their children at construction time
// inherit this so they can re-apply them when the user flips light/dark.
// needsThemeRefresh() returns true on the first call and after every variant
// change, and is cheap enough to poll from draw().
class ThemeAware {
  protected:
    bool needsThemeRefresh();

  private:
    int m_appliedVariant = -1;
};

} // namespace nxui
