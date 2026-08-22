#include <cstdio>
#include <cstdlib>
#include <exception>

#include <borealis.hpp>
#include <switch.h>

#include "src/core/catalog.hpp"
#include "src/core/emulator.hpp"
#include "src/core/paths.hpp"
#include "src/core/settings.hpp"
#include "src/ui/main_activity.hpp"
#include "src/ui/tabs/account_tab.hpp"
#include "src/ui/tabs/browse_tab.hpp"
#include "src/ui/tabs/library_tab.hpp"
#include "src/ui/tabs/settings_tab.hpp"
#include "src/ui/theme.hpp"

namespace {

// Borealis logs to stdout, which is discarded on a console without nxlink and
// under emulators. Mirror it to a file on the SD card so a crash leaves a
// readable trail. Returns nullptr when the card is unwritable, in which case
// logging simply stays on stdout.
std::FILE* openLogFile() {
    std::FILE* file = std::fopen(paths::log().c_str(), "w");
    if (file)
        std::setvbuf(file, nullptr, _IOLBF, 4096);
    return file;
}

} // namespace

int main(int argc, char* argv[]) {
    // Directories first: the log file, settings and downloads all live under
    // them. A failure here is not fatal; the UI reports it.
    const bool storageReady = paths::ensureDirectories();

    std::FILE* logFile = storageReady ? openLogFile() : nullptr;
    if (logFile)
        brls::Logger::setLogOutput(logFile);

    brls::Logger::setLogLevel(brls::LogLevel::LOG_DEBUG);
    brls::Logger::info("ParaLLEl Launcher NX {} starting", APP_VERSION);

    try {
        if (!storageReady)
            brls::Logger::error("Could not create launcher directories on the SD card");

        if (!brls::Application::init()) {
            brls::Logger::error("Failed to initialise Borealis");
            return EXIT_FAILURE;
        }

        AppSettings::instance().load();
        Catalog::instance().loadSampleData();

        // Leave a documented emulator.json on the card the first time we run,
        // so the core option keys can be corrected against a real RetroArch
        // build without rebuilding the launcher.
        if (!paths::fileExists(std::string(paths::kRoot) + "/emulator.json"))
            emulator::Profile::writeReference();

        emulator::logEnvironment();

        brls::Application::createWindow("ParaLLEl Launcher NX");

        nxui::registerThemeColors();
        brls::Application::getPlatform()->setThemeVariant(AppSettings::instance().darkMode
                ? brls::ThemeVariant::DARK
                : brls::ThemeVariant::LIGHT);

        // The launcher owns the back button on the root activity, so Borealis
        // must not quit out from under it.
        brls::Application::setGlobalQuit(false);

        brls::Application::registerXMLView("LibraryTab", LibraryTab::create);
        brls::Application::registerXMLView("BrowseTab", BrowseTab::create);
        brls::Application::registerXMLView("AccountTab", AccountTab::create);
        brls::Application::registerXMLView("SettingsTab", SettingsTab::create);

        brls::Application::pushActivity(new MainActivity());

        brls::Logger::info("ParaLLEl Launcher NX {} running", APP_VERSION);

        while (brls::Application::mainLoop()) {
            AppSettings::instance().flushPendingSave();
        }

        AppSettings::instance().save();
        brls::Logger::info("ParaLLEl Launcher NX exiting cleanly");
    } catch (const std::exception& e) {
        brls::Logger::error("Unhandled exception, aborting: {}", e.what());
        if (logFile)
            std::fclose(logFile);
        return EXIT_FAILURE;
    } catch (...) {
        brls::Logger::error("Unknown unhandled exception, aborting");
        if (logFile)
            std::fclose(logFile);
        return EXIT_FAILURE;
    }

    if (logFile)
        std::fclose(logFile);

    return EXIT_SUCCESS;
}
