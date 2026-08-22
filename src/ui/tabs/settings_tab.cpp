#include "src/ui/tabs/settings_tab.hpp"

#include <cmath>

#include "src/core/emulator.hpp"
#include "src/emu/input.hpp"
#include "src/core/paths.hpp"
#include "src/core/rom_service.hpp"
#include "src/core/settings.hpp"
#include "src/core/text_util.hpp"
#include "src/ui/theme.hpp"
#include "src/ui/views/widgets.hpp"

namespace {

// Dropdown order for the default renderer. Index 0 is "Auto".
const GfxPlugin kRendererChoices[] = {
    GfxPlugin::GLideN64,
    GfxPlugin::ParaLLEl,
    GfxPlugin::OGRE,
};

int indexOfRenderer(GfxPlugin plugin) {
    for (int i = 0; i < static_cast<int>(std::size(kRendererChoices)); ++i) {
        if (kRendererChoices[i] == plugin)
            return i;
    }
    return 0;
}

// Deadzone is stored as a percentage but the slider works in 0..1; cap the
// usable range at 40% so the stick can never be made unusable from the UI.
constexpr int kMaxDeadzonePercent = 40;

std::string deadzoneTitle(int percent) {
    return "Analog stick deadzone (" + std::to_string(percent) + "%)";
}

// brls::Header renders its subtitle on the same line as the title, so long
// explanatory text collides with it. Emit the header and a wrapped caption
// underneath instead.
void addSection(brls::Box* parent, const std::string& title, const std::string& caption = "") {
    brls::Header* header = new brls::Header();
    header->setTitle(title);
    parent->addView(header);

    if (caption.empty())
        return;

    brls::Label* label = new brls::Label();
    label->setFontSize(14.0f);
    label->setLineHeight(1.4f);
    label->setMarginTop(6.0f);
    label->setMarginBottom(4.0f);
    label->setTextColor(nxui::color("nx/text_secondary"));
    label->setText(caption);
    parent->addView(label);
}

void showDialog(const std::string& text) {
    brls::Dialog* dialog = new brls::Dialog(text);
    dialog->addButton("OK", [dialog]() { dialog->close(); });
    dialog->open();
}

} // namespace

SettingsTab::SettingsTab() {
    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(1.0f);

    brls::ScrollingFrame* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1.0f);

    brls::Box* content = new brls::Box(brls::Axis::COLUMN);
    content->setPadding(12.0f, 40.0f, 40.0f, 40.0f);

    buildStorageSection(content);
    buildGraphicsSection(content);
    buildControlsSection(content);
    buildSystemSection(content);

    scroll->setContentView(content);
    this->addView(scroll);
}

void SettingsTab::commit() {
    AppSettings::instance().save();
}

void SettingsTab::buildStorageSection(brls::Box* parent) {
    AppSettings& s = AppSettings::instance();

    addSection(parent, "Base ROM & storage",
        "The launcher patches your own Super Mario 64 dump. No ROM is bundled.");

    brls::InputCell* baseRom = new brls::InputCell();
    baseRom->init(
        "Base ROM path", s.baseRomPath,
        [this](std::string path) {
            AppSettings::instance().baseRomPath = textutil::sanitizeUserInput(path, 512);
            commit();
        },
        "Not set", "Full sdmc: path to a clean SM64 (USA) .z64", 128);
    parent->addView(baseRom);

    brls::DetailCell* verify = new brls::DetailCell();
    verify->setText("Verify base ROM");
    verify->setDetailText(RomService::instance().hasUsableBaseRom() ? "Verified" : "Check");
    verify->registerClickAction([verify](brls::View*) {
        RomService& service = RomService::instance();
        verify->setDetailText("Checking...");

        service.invalidate();
        service.refresh([verify]() {
            const rom::BaseRomInfo& info = RomService::instance().info();
            verify->setDetailText(info.isUsableBase() ? "Verified" : "Failed");

            if (info.isUsableBase()) {
                showDialog(std::string(rom::regionName(info.region)) + "\n\n"
                    + "Byte order: " + rom::byteOrderName(info.byteOrder) + "\n"
                    + "Size: " + std::to_string(info.sizeBytes / 1024) + " kB\n"
                    + "SHA-1: " + info.z64Sha1 + "\n\n"
                    + "This is a clean dump and can be used to patch hacks.");
            } else if (!info.readable) {
                showDialog("No readable file at\n" + AppSettings::instance().baseRomPath
                    + "\n\n" + info.error);
            } else {
                showDialog("This file cannot be used as a patch base.\n\n"
                    + std::string("Detected: ") + rom::regionName(info.region) + "\n"
                    + "SHA-1: " + info.z64Sha1 + "\n\n" + info.error);
            }
        });
        return true;
    });
    parent->addView(verify);

    brls::DetailCell* storage = new brls::DetailCell();
    storage->setText("Launcher data folder");
    storage->setDetailText(paths::directoryExists(paths::kRoot) ? "Ready" : "Not created");
    storage->registerClickAction([storage](brls::View*) {
        const bool ok = paths::ensureDirectories();
        storage->setDetailText(ok ? "Ready" : "Failed");
        showDialog(ok ? std::string("Launcher folders are in place under\n") + paths::kRoot
                      : std::string("Could not create folders under\n") + paths::kRoot
                            + "\n\nIs the SD card write-protected or full?");
        return true;
    });
    parent->addView(storage);
}

void SettingsTab::buildGraphicsSection(brls::Box* parent) {
    AppSettings& s = AppSettings::instance();

    addSection(parent, "Graphics",
        "Used when a hack's author has not named a renderer.");

    std::vector<std::string> rendererNames;
    for (GfxPlugin plugin : kRendererChoices)
        rendererNames.emplace_back(getGfxPluginName(plugin));

    brls::SelectorCell* renderer = new brls::SelectorCell();
    renderer->init("Default renderer", rendererNames, indexOfRenderer(s.defaultGraphicsPlugin),
        [this](int index) {
            if (index < 0 || index >= static_cast<int>(std::size(kRendererChoices)))
                return;
            AppSettings::instance().defaultGraphicsPlugin = kRendererChoices[index];
            commit();
        });
    parent->addView(renderer);

    const std::vector<std::string> resolutions = {
        "Native (320x240)",
        "2x (640x480)",
        "3x (960x720)",
        "4x (1280x960)",
    };
    brls::SelectorCell* upscale = new brls::SelectorCell();
    upscale->init("Internal resolution", resolutions,
        std::max(0, std::min(3, s.upscalingMultiplier - 1)), [this](int index) {
            AppSettings::instance().upscalingMultiplier = index + 1;
            commit();
        });
    parent->addView(upscale);

    // OGRE cannot run on Switch, so hacks pinned to it need a stand-in. Which
    // legacy renderer looks right varies by hack, so make it a choice rather
    // than a hard-coded fallback.
    std::vector<std::string> substituteNames;
    for (GfxPlugin plugin : emulator::kOgreSubstitutes)
        substituteNames.emplace_back(getGfxPluginName(plugin));

    int substituteIndex = 0;
    for (int i = 0; i < static_cast<int>(std::size(emulator::kOgreSubstitutes)); ++i) {
        if (emulator::kOgreSubstitutes[i] == s.ogreSubstitute) {
            substituteIndex = i;
            break;
        }
    }

    brls::SelectorCell* ogre = new brls::SelectorCell();
    ogre->init("Run OGRE hacks with", substituteNames, substituteIndex, [this](int index) {
        if (index < 0 || index >= static_cast<int>(std::size(emulator::kOgreSubstitutes)))
            return;
        AppSettings::instance().ogreSubstitute = emulator::kOgreSubstitutes[index];
        commit();
    });
    parent->addView(ogre);

    brls::Label* ogreNote = new brls::Label();
    ogreNote->setFontSize(13.0f);
    ogreNote->setLineHeight(1.4f);
    ogreNote->setMarginTop(6.0f);
    ogreNote->setMarginBottom(4.0f);
    ogreNote->setTextColor(nxui::color("nx/text_secondary"));
    ogreNote->setText("OGRE is a Parallel Launcher plugin with no Switch build. Hacks that ask "
                      "for it run on this legacy renderer instead. Rice is the default; try "
                      "Glide64 if a hack misdraws.");
    parent->addView(ogreNote);

    brls::BooleanCell* widescreen = new brls::BooleanCell();
    widescreen->init("16:9 widescreen hack", s.widescreenHack, [this](bool on) {
        AppSettings::instance().widescreenHack = on;
        commit();
    });
    parent->addView(widescreen);

    brls::BooleanCell* framebuffer = new brls::BooleanCell();
    framebuffer->init("Framebuffer emulation", s.enableFramebufferEmulation, [this](bool on) {
        AppSettings::instance().enableFramebufferEmulation = on;
        commit();
    });
    parent->addView(framebuffer);

    brls::BooleanCell* depth = new brls::BooleanCell();
    depth->init("Accurate depth compare", s.enableCorrectDepthCompare, [this](bool on) {
        AppSettings::instance().enableCorrectDepthCompare = on;
        commit();
    });
    parent->addView(depth);

    brls::BooleanCell* aa = new brls::BooleanCell();
    aa->init("ParaLLEl anti-aliasing", s.enableParallelAntiAliasing, [this](bool on) {
        AppSettings::instance().enableParallelAntiAliasing = on;
        commit();
    });
    parent->addView(aa);
}

void SettingsTab::buildControlsSection(brls::Box* parent) {
    AppSettings& s = AppSettings::instance();

    addSection(parent, "Controls",
        "Pro Controller, Joy-Con and USB GameCube pads are all supported.");

    std::vector<std::string> controllerNames;
    for (int i = 0; i <= static_cast<int>(ControllerType::JoyConSingle); ++i)
        controllerNames.emplace_back(getControllerTypeName(static_cast<ControllerType>(i)));

    brls::SelectorCell* controller = new brls::SelectorCell();
    controller->init("Default controller preset", controllerNames,
        static_cast<int>(s.defaultController), [this](int index) {
            AppSettings::instance().defaultController = static_cast<ControllerType>(index);
            commit();
        });
    parent->addView(controller);

    m_deadzoneCell = new brls::SliderCell();
    m_deadzoneCell->init(deadzoneTitle(s.stickDeadzone),
        static_cast<float>(s.stickDeadzone) / kMaxDeadzonePercent, [this](float value) {
            const int percent = static_cast<int>(std::lround(value * kMaxDeadzonePercent));
            AppSettings::instance().stickDeadzone = percent;
            m_deadzoneCell->title->setText(deadzoneTitle(percent));
            // Fires on every frame of a drag, so debounce rather than commit.
            AppSettings::instance().requestSave();
        });
    parent->addView(m_deadzoneCell);

    brls::BooleanCell* rumble = new brls::BooleanCell();
    rumble->init("Rumble", s.rumbleEnabled, [this](bool on) {
        AppSettings::instance().rumbleEnabled = on;
        commit();
    });
    parent->addView(rumble);

    brls::DetailCell* mapping = new brls::DetailCell();
    mapping->setText("N64 button mapping");
    mapping->setDetailText(emu::padKindName(emu::detectConnectedKind(0)));
    mapping->registerClickAction([mapping](brls::View*) {
        const emu::PadKind kind = emu::detectConnectedKind(0);
        mapping->setDetailText(emu::padKindName(kind));

        std::string text = std::string(emu::padKindName(kind)) + "\n\n";
        for (const auto& row : emu::describeBindings(kind)) {
            std::string label = row.first;
            label.resize(std::max<size_t>(label.size(), 12), ' ');
            text += label + "  " + row.second + "\n";
        }

        if (kind == emu::PadKind::JoyConLeft || kind == emu::PadKind::JoyConRight) {
            text += "\nA single Joy-Con cannot reach every N64 button. Use a Pro Controller, "
                    "both Joy-Con, or a GameCube pad for hacks that need the C buttons.";
        } else if (kind == emu::PadKind::GameCube) {
            text += "\nThe C-stick drives the C buttons directly, which is as close to a real "
                    "N64 pad as this gets.";
        } else if (kind == emu::PadKind::Unknown) {
            text += "\nNo controller detected yet. Connect one and open this again.";
        }

        showDialog(text);
        return true;
    });
    parent->addView(mapping);
}

void SettingsTab::buildSystemSection(brls::Box* parent) {
    AppSettings& s = AppSettings::instance();

    addSection(parent, "Application");

    brls::BooleanCell* darkMode = new brls::BooleanCell();
    darkMode->init("Dark theme", s.darkMode, [this](bool on) {
        AppSettings::instance().darkMode = on;
        brls::Application::getPlatform()->setThemeVariant(
            on ? brls::ThemeVariant::DARK : brls::ThemeVariant::LIGHT);
        commit();
    });
    parent->addView(darkMode);

    brls::BooleanCell* fps = new brls::BooleanCell();
    fps->init("Show FPS counter in game", s.showFps, [this](bool on) {
        AppSettings::instance().showFps = on;
        commit();
    });
    parent->addView(fps);

    brls::BooleanCell* sync = new brls::BooleanCell();
    sync->init("Auto-sync playlists when signed in", s.autoSyncPlaylists, [this](bool on) {
        AppSettings::instance().autoSyncPlaylists = on;
        commit();
    });
    parent->addView(sync);

    brls::DetailCell* about = new brls::DetailCell();
    about->setText("About");
    about->setDetailText(APP_VERSION);
    about->registerClickAction([](brls::View*) {
        showDialog(
            std::string("ParaLLEl Launcher NX ") + APP_VERSION + "\n\n"
            "A Nintendo Switch port of Parallel Launcher for Super Mario 64 ROM hacks.\n\n"
            "Built on Borealis, devkitA64 and libnx.\n"
            "Renderers: GLideN64, ParaLLEl-RDP, OGRE.\n\n"
            "No copyrighted ROM is included or distributed.");
        return true;
    });
    parent->addView(about);
}

brls::View* SettingsTab::create() {
    return new SettingsTab();
}
