#include "src/ui/views/hack_detail_view.hpp"

#include "src/core/catalog.hpp"
#include "src/core/emulator.hpp"
#include "src/core/installer.hpp"
#include "src/core/paths.hpp"
#include "src/core/rom_service.hpp"
#include "src/core/settings.hpp"

namespace {

// Renderer choices offered per hack, in dropdown order.
const GfxPlugin kRendererChoices[] = {
    GfxPlugin::UseDefault,
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

void showDialog(const std::string& text) {
    brls::Dialog* dialog = new brls::Dialog(text);
    dialog->addButton("OK", [dialog]() { dialog->close(); });
    dialog->open();
}

// Patches the user has dropped on the SD card themselves, named after the
// hack. Checked before falling back to a download.
std::string findLocalPatch(const std::string& hackId) {
    for (const char* extension : { ".bps", ".ips" }) {
        const std::string candidate = paths::patches() + "/" + hackId + extension;
        if (paths::fileExists(candidate))
            return candidate;
    }
    return "";
}

} // namespace

HackDetailView::HackDetailView(const std::string& hackId, std::function<void()> onChanged)
    : m_onChanged(std::move(onChanged)) {
    m_hack = Catalog::instance().findMutable(hackId);

    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(1.0f);

    if (!m_hack) {
        this->addView(new nxui::EmptyState("!", "Hack not found",
            "This entry is no longer in the catalog."));
        return;
    }

    brls::ScrollingFrame* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1.0f);

    brls::Box* content = new brls::Box(brls::Axis::COLUMN);
    content->setPadding(24.0f, 40.0f, 40.0f, 40.0f);

    buildHero(content);
    buildRendererSection(content);
    buildVersionSection(content);
    buildRequirements(content);
    buildDescription(content);
    buildLocalInfo(content);

    scroll->setContentView(content);
    this->addView(scroll);

    refreshActions();
}

void HackDetailView::buildHero(brls::Box* parent) {
    brls::Box* hero = new brls::Box(brls::Axis::ROW);
    hero->setAlignItems(brls::AlignItems::FLEX_START);
    hero->setMarginBottom(26.0f);

    nxui::HackThumb* thumb = new nxui::HackThumb(m_hack->id, m_hack->starCount, 300.0f, 190.0f);
    thumb->setMarginRight(26.0f);
    hero->addView(thumb);

    brls::Box* right = new brls::Box(brls::Axis::COLUMN);
    right->setGrow(1.0f);

    brls::Label* title = new brls::Label();
    title->setFontSize(28.0f);
    title->setText(m_hack->name);
    right->addView(title);

    m_byline = new brls::Label();
    m_byline->setFontSize(15.0f);
    m_byline->setMarginTop(5.0f);
    std::string byline = "by " + m_hack->author + "  ·  " + m_hack->category;
    if (m_hack->releaseYear > 0)
        byline += "  ·  " + std::to_string(m_hack->releaseYear);
    m_byline->setText(byline);
    right->addView(m_byline);

    // Stat strip
    brls::Box* stats = new brls::Box(brls::Axis::ROW);
    stats->setAlignItems(brls::AlignItems::CENTER);
    stats->setMarginTop(14.0f);

    m_rating = new brls::Label();
    m_rating->setFontSize(17.0f);
    m_rating->setMarginRight(18.0f);
    m_rating->setText(nxui::formatRating(m_hack->rating) + "★  ("
        + std::to_string(m_hack->starCount) + " stars to collect)");
    stats->addView(m_rating);

    m_downloads = new brls::Label();
    m_downloads->setFontSize(15.0f);
    m_downloads->setMarginRight(14.0f);
    m_downloads->setText(nxui::formatDownloads(m_hack->downloads));
    stats->addView(m_downloads);

    stats->addView(nxui::makeDifficultyChip(m_hack->difficulty, 13.0f));
    right->addView(stats);

    // Renderer summary
    brls::Box* pluginRow = new brls::Box(brls::Axis::ROW);
    pluginRow->setAlignItems(brls::AlignItems::CENTER);
    pluginRow->setMarginTop(14.0f);

    brls::Label* pluginLabel = new brls::Label();
    pluginLabel->setFontSize(15.0f);
    pluginLabel->setMarginRight(10.0f);
    pluginLabel->setText(m_hack->defaultPlugin == GfxPlugin::UseDefault
            ? "No renderer preference:"
            : "Author recommends:");
    m_secondaryLabels.push_back(pluginLabel);
    pluginRow->addView(pluginLabel);

    const emulator::ResolvedRenderer resolved = emulator::resolveRenderer(*m_hack);
    m_pluginChip = nxui::makePluginBadge(resolved.requested, nxui::Chip::Style::Filled, 13.0f);
    pluginRow->addView(m_pluginChip);

    if (resolved.substituted) {
        brls::Label* arrow = new brls::Label();
        arrow->setFontSize(14.0f);
        arrow->setMarginLeft(8.0f);
        arrow->setMarginRight(8.0f);
        arrow->setText("runs as");
        m_secondaryLabels.push_back(arrow);
        pluginRow->addView(arrow);

        pluginRow->addView(
            nxui::makePluginBadge(resolved.effective, nxui::Chip::Style::Outline, 13.0f));
    }

    right->addView(pluginRow);

    // Actions
    brls::Box* actions = new brls::Box(brls::Axis::ROW);
    actions->setMarginTop(20.0f);

    m_primaryButton = new brls::Button();
    m_primaryButton->setStyle(&brls::BUTTONSTYLE_PRIMARY);
    m_primaryButton->setMarginRight(14.0f);
    m_primaryButton->registerClickAction([this](brls::View*) {
        onPrimaryAction();
        return true;
    });
    actions->addView(m_primaryButton);

    m_removeButton = new brls::Button();
    m_removeButton->setStyle(&brls::BUTTONSTYLE_BORDERED);
    m_removeButton->setText("Remove");
    m_removeButton->registerClickAction([this](brls::View*) {
        onRemove();
        return true;
    });
    actions->addView(m_removeButton);

    right->addView(actions);
    hero->addView(right);
    parent->addView(hero);
}

void HackDetailView::buildRendererSection(brls::Box* parent) {
    parent->addView(nxui::makeSectionHeader("Renderer",
        "Overrides the author's recommendation for this hack only."));

    std::vector<std::string> names;
    names.reserve(std::size(kRendererChoices));
    for (GfxPlugin plugin : kRendererChoices) {
        names.emplace_back(plugin == GfxPlugin::UseDefault
                ? std::string("Auto — ") + getGfxPluginShortName(m_hack->defaultPlugin)
                : getGfxPluginName(plugin));
    }

    brls::SelectorCell* selector = new brls::SelectorCell();
    selector->init("Graphics plugin", names, indexOfRenderer(m_hack->pluginOverride),
        [this](int index) {
            if (index < 0 || index >= static_cast<int>(std::size(kRendererChoices)))
                return;

            m_hack->pluginOverride = kRendererChoices[index];

            const GfxPlugin resolved
                = m_hack->resolvePlugin(AppSettings::instance().defaultGraphicsPlugin);
            const GfxPlugin shown = m_hack->resolvePlugin(
                AppSettings::instance().defaultGraphicsPlugin);
            m_pluginChip->setText(getGfxPluginShortName(shown));
            m_pluginChip->setColorProvider([shown]() { return nxui::pluginColor(shown); });

            std::string blurb = getGfxPluginBlurb(shown);
            const emulator::ResolvedRenderer now = emulator::resolveRenderer(*m_hack);
            if (now.substituted) {
                blurb += std::string("\n\nThis hack will run on ")
                    + getGfxPluginName(now.effective) + ".";
            }
            m_rendererBlurb->setText(blurb);

            if (m_onChanged)
                m_onChanged();
        });
    parent->addView(selector);

    m_rendererBlurb = new brls::Label();
    m_rendererBlurb->setFontSize(14.0f);
    m_rendererBlurb->setMarginTop(8.0f);
    m_rendererBlurb->setMarginBottom(8.0f);
    {
        const emulator::ResolvedRenderer current = emulator::resolveRenderer(*m_hack);
        std::string blurb = getGfxPluginBlurb(current.requested);
        if (current.substituted) {
            blurb += std::string("\n\nThis hack will run on ")
                + getGfxPluginName(current.effective) + ".";
        }
        m_rendererBlurb->setText(blurb);
    }
    m_secondaryLabels.push_back(m_rendererBlurb);
    parent->addView(m_rendererBlurb);
}

void HackDetailView::buildVersionSection(brls::Box* parent) {
    if (m_hack->versions.empty())
        return;

    parent->addView(nxui::makeSectionHeader("Version"));

    std::vector<std::string> names;
    names.reserve(m_hack->versions.size());
    for (const HackVersion& v : m_hack->versions)
        names.push_back(v.name);

    brls::SelectorCell* selector = new brls::SelectorCell();
    selector->init("Release", names, 0, [](int index) {
        brls::Logger::debug("Version index {} selected", index);
    });
    parent->addView(selector);
}

void HackDetailView::buildRequirements(brls::Box* parent) {
    struct FlagLabel {
        RhdcHackFlag flag;
        const char*  text;
    };

    static const FlagLabel kFlagLabels[] = {
        { RhdcHackFlag::ExpansionPak, "Expansion Pak" },
        { RhdcHackFlag::BigEEPROM,    "16 kB EEPROM" },
        { RhdcHackFlag::DualAnalog,   "Dual analog" },
        { RhdcHackFlag::NoOverclock,  "No overclock" },
        { RhdcHackFlag::OverclockVI,  "VI overclock" },
        { RhdcHackFlag::WidescreenOk, "16:9 supported" },
    };

    brls::Box* row = new brls::Box(brls::Axis::ROW);
    row->setAlignItems(brls::AlignItems::CENTER);

    int shown = 0;
    for (const FlagLabel& f : kFlagLabels) {
        if (!hasFlag(m_hack->flags, f.flag))
            continue;

        nxui::Chip* chip = new nxui::Chip(f.text,
            []() { return nxui::color("nx/text_secondary"); }, nxui::Chip::Style::Outline, 12.0f);
        chip->setMarginRight(8.0f);
        row->addView(chip);
        ++shown;
    }

    if (shown == 0) {
        delete row;
        return;
    }

    parent->addView(nxui::makeSectionHeader("Requirements"));
    parent->addView(row);
}

void HackDetailView::buildDescription(brls::Box* parent) {
    parent->addView(nxui::makeSectionHeader("About this hack"));

    m_description = new brls::Label();
    m_description->setFontSize(15.0f);
    m_description->setLineHeight(1.5f);
    m_description->setText(m_hack->description.empty()
            ? "The author did not provide a description for this hack."
            : m_hack->description);
    m_secondaryLabels.push_back(m_description);
    parent->addView(m_description);
}

void HackDetailView::buildLocalInfo(brls::Box* parent) {
    parent->addView(nxui::makeSectionHeader("On this console"));

    m_localInfo = new brls::Label();
    m_localInfo->setFontSize(14.0f);
    m_localInfo->setLineHeight(1.5f);
    m_secondaryLabels.push_back(m_localInfo);
    parent->addView(m_localInfo);
}

void HackDetailView::refreshActions() {
    if (!m_hack)
        return;

    if (m_hack->isInstalled) {
        m_primaryButton->setText("Play");
        m_removeButton->setVisibility(brls::Visibility::VISIBLE);
    } else {
        m_primaryButton->setText("Download & Patch");
        m_removeButton->setVisibility(brls::Visibility::GONE);
    }

    if (m_localInfo) {
        std::string text;
        if (m_hack->isInstalled) {
            text = "Installed: " + m_hack->installedVersion + "\n"
                + "Path: " + m_hack->localRomPath + "\n"
                + nxui::formatPlayTime(m_hack->playTime);
        } else {
            text = "Not installed.\n\nThe launcher applies the author's patch to your own "
                   "verified Super Mario 64 (USA) dump. No ROM is downloaded or distributed.\n"
                   "Drop a patch at " + paths::patches() + "/" + m_hack->id + ".bps to install "
                   "it without a network connection.";
        }
        m_localInfo->setText(text);
    }
}

void HackDetailView::onPrimaryAction() {
    if (m_hack->isInstalled) {
        startLaunch();
        return;
    }

    startInstall();
}

void HackDetailView::startLaunch() {
    const emulator::LaunchPlan launchPlan = emulator::plan(*m_hack);

    if (!launchPlan.ok) {
        showDialog(launchPlan.error);
        return;
    }

    std::string body = "Launch " + m_hack->name + "?\n\nRenderer: "
        + launchPlan.rendererSummary() + "\nCore: " + launchPlan.corePath;

    if (launchPlan.renderer.substituted) {
        body += "\n\nOGRE has no Switch build, so this hack runs on "
            + std::string(getGfxPluginShortName(launchPlan.renderer.effective))
            + ". Change the stand-in under Settings > Graphics if it looks wrong.";
    }

    body += "\n\nThe launcher closes and the emulator starts in its place.";

    brls::Dialog* dialog = new brls::Dialog(body);

    dialog->addButton("Launch", [this, launchPlan, dialog]() {
        std::string error;
        if (!emulator::launch(launchPlan, *m_hack, &error)) {
            dialog->close();
            showDialog(error);
            return;
        }

        dialog->close();
        // The handoff is registered; quitting is what actually performs it.
        brls::Application::quit();
    });
    dialog->addButton("Cancel", [dialog]() { dialog->close(); });
    dialog->open();
}

void HackDetailView::startInstall() {
    if (!RomService::instance().hasUsableBaseRom()) {
        showDialog("A verified Super Mario 64 (USA) base ROM is required before anything can "
                   "be patched.\n\nPut your own dump at\n"
            + AppSettings::instance().baseRomPath + "\nand check it from the Library tab.");
        return;
    }

    const std::string localPatch = findLocalPatch(m_hack->id);
    if (localPatch.empty()) {
        showDialog("Downloading from Romhacking.com is not connected yet.\n\nYou can still "
                   "install this hack by putting its patch at\n"
            + paths::patches() + "/" + m_hack->id + ".bps\n(or .ips) and pressing this "
              "button again.");
        return;
    }

    m_primaryButton->setText("Patching...");

    installer::installFromFileAsync(*m_hack, localPatch, [this](installer::Outcome outcome) {
        if (!outcome.ok) {
            refreshActions();
            showDialog("Could not patch this hack.\n\n" + outcome.error);
            return;
        }

        m_hack->isInstalled      = true;
        m_hack->localRomPath     = outcome.romPath;
        m_hack->installedVersion = m_hack->versions.empty() ? "v1.0"
                                                            : m_hack->versions.front().name;
        refreshActions();

        if (m_onChanged)
            m_onChanged();

        showDialog("Installed " + m_hack->name + "\n\nWritten to " + outcome.romPath
            + "\nSHA-1 " + outcome.sha1);
    });
}

void HackDetailView::onRemove() {
    brls::Dialog* dialog = new brls::Dialog(
        "Remove \"" + m_hack->name + "\" from this console?\n\nThe patched ROM is deleted. "
        "Your save file is kept.");
    dialog->addButton("Remove", [this, dialog]() {
        std::string error;
        if (!installer::remove(*m_hack, &error)) {
            dialog->close();
            showDialog(error);
            return;
        }

        m_hack->isInstalled = false;
        m_hack->localRomPath.clear();
        m_hack->installedVersion.clear();
        refreshActions();
        if (m_onChanged)
            m_onChanged();
        dialog->close();
    });
    dialog->addButton("Cancel", [dialog]() { dialog->close(); });
    dialog->open();
}

void HackDetailView::applyTheme() {
    const NVGcolor secondary = nxui::color("nx/text_secondary");
    for (brls::Label* label : m_secondaryLabels)
        label->setTextColor(secondary);

    if (m_byline)
        m_byline->setTextColor(secondary);
    if (m_downloads)
        m_downloads->setTextColor(secondary);
    if (m_rating)
        m_rating->setTextColor(nxui::color("nx/star"));
}

void HackDetailView::draw(NVGcontext* vg, float x, float y, float width, float height,
    brls::Style style, brls::FrameContext* ctx) {
    if (needsThemeRefresh())
        applyTheme();
    brls::Box::draw(vg, x, y, width, height, style, ctx);
}

void HackDetailView::show(const std::string& hackId, std::function<void()> onChanged) {
    const HackInfo* hack = Catalog::instance().find(hackId);

    brls::AppletFrame* frame = new brls::AppletFrame(new HackDetailView(hackId, std::move(onChanged)));
    frame->setTitle(hack ? hack->name : "Hack");
    brls::Application::pushActivity(new brls::Activity(frame));
}
