#pragma once

#include <borealis.hpp>

#include "src/ui/theme.hpp"

// Every control here writes straight into AppSettings and persists on change,
// so nothing is lost if the console is put to sleep or the app is killed.
class SettingsTab : public brls::Box {
  public:
    SettingsTab();

    static brls::View* create();

  private:
    void buildStorageSection(brls::Box* parent);
    void buildGraphicsSection(brls::Box* parent);
    void buildControlsSection(brls::Box* parent);
    void buildSystemSection(brls::Box* parent);

    void commit();

    brls::SliderCell* m_deadzoneCell = nullptr;
};
