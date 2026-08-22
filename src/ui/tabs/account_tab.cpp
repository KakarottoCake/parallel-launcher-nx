#include "src/ui/tabs/account_tab.hpp"

#include "src/core/settings.hpp"
#include "src/core/text_util.hpp"

AccountTab::AccountTab() {
    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(1.0f);

    brls::ScrollingFrame* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1.0f);

    brls::Box* content = new brls::Box(brls::Axis::COLUMN);
    content->setPadding(24.0f, 40.0f, 40.0f, 40.0f);

    brls::Label* header = new brls::Label();
    header->setFontSize(26.0f);
    header->setText("Romhacking.com");
    content->addView(header);

    m_blurb = new brls::Label();
    m_blurb->setFontSize(15.0f);
    m_blurb->setLineHeight(1.5f);
    m_blurb->setMarginTop(6.0f);
    m_blurb->setMarginBottom(22.0f);
    m_blurb->setText("Signing in syncs your followed hacks, playlists and star ratings. "
                     "Browsing and playing work fine without an account.");
    content->addView(m_blurb);

    // --- error strip -------------------------------------------------------
    m_error = new nxui::StatusBanner(nxui::StatusBanner::Level::Error, "", "");
    m_error->setMarginBottom(16.0f);
    m_error->setVisibility(brls::Visibility::GONE);
    content->addView(m_error);

    // --- signed in ---------------------------------------------------------
    m_signedIn = new brls::Box(brls::Axis::COLUMN);
    m_signedIn->setVisibility(brls::Visibility::GONE);

    brls::Box* card = new brls::Box(brls::Axis::COLUMN);
    card->setPadding(18.0f, 22.0f, 18.0f, 22.0f);
    card->setCornerRadius(10.0f);
    card->setBackgroundColor(nxui::color("nx/card"));
    card->setMarginBottom(18.0f);

    m_profileName = new brls::Label();
    m_profileName->setFontSize(21.0f);
    card->addView(m_profileName);

    m_profileMeta = new brls::Label();
    m_profileMeta->setFontSize(15.0f);
    m_profileMeta->setMarginTop(5.0f);
    card->addView(m_profileMeta);

    m_signedIn->addView(card);

    brls::Button* logoutButton = new brls::Button();
    logoutButton->setText("Sign out");
    logoutButton->setStyle(&brls::BUTTONSTYLE_BORDERED);
    logoutButton->registerClickAction([this](brls::View*) {
        logout();
        return true;
    });
    m_signedIn->addView(logoutButton);
    content->addView(m_signedIn);

    // --- signed out --------------------------------------------------------
    m_signedOut = new brls::Box(brls::Axis::COLUMN);

    m_usernameCell = new brls::InputCell();
    m_usernameCell->init(
        "Username or email", AppSettings::instance().rhdcUsername,
        [this](std::string text) { m_username = textutil::sanitizeUserInput(text, 128); },
        "Required", "Your Romhacking.com account name", 64);
    m_signedOut->addView(m_usernameCell);
    m_username = AppSettings::instance().rhdcUsername;

    m_passwordCell = new nxui::PasswordCell();
    m_passwordCell->init("Password",
        [this](std::string text) { m_password = textutil::sanitizeUserInput(text, 128); });
    m_signedOut->addView(m_passwordCell);

    m_mfaCell = new brls::InputCell();
    m_mfaCell->init(
        "Two-factor code", "",
        [this](std::string text) { m_mfa = textutil::sanitizeUserInput(text, 12); },
        "Only if enabled", "6-digit code from your authenticator app", 6);
    m_signedOut->addView(m_mfaCell);

    m_loginButton = new brls::Button();
    m_loginButton->setText("Sign in");
    m_loginButton->setStyle(&brls::BUTTONSTYLE_PRIMARY);
    m_loginButton->setMarginTop(22.0f);
    m_loginButton->registerClickAction([this](brls::View*) {
        attemptLogin();
        return true;
    });
    m_signedOut->addView(m_loginButton);

    content->addView(m_signedOut);

    scroll->setContentView(content);
    this->addView(scroll);

    updateState();
}

void AccountTab::attemptLogin() {
    brls::Logger::debug("Login attempt: user={} chars, pass={} chars, mfa={} chars",
        m_username.size(), m_password.size(), m_mfa.size());

    if (m_username.empty() || m_password.empty()) {
        m_error->set(nxui::StatusBanner::Level::Error, "Missing details",
            "Enter both your username and your password.");
        m_error->setVisibility(brls::Visibility::VISIBLE);
        return;
    }

    m_error->setVisibility(brls::Visibility::GONE);

    // The RHDC auth request is not implemented yet. Rather than pretend the
    // credentials were checked, say so plainly and leave the session signed
    // out; the signed-in layout is reachable from the button below.
    brls::Dialog* dialog = new brls::Dialog(
        "Romhacking.com sign-in is not connected yet.\n\nNothing was sent over the network "
        "and your password was not stored.\n\nContinue with a local placeholder session so "
        "the signed-in screens can be tested?");
    dialog->addButton("Use placeholder", [this, dialog]() {
        brls::Logger::debug("Placeholder session accepted");

        m_profile.isLoggedIn     = true;
        m_profile.username       = m_username;
        m_profile.role           = "Member";
        m_profile.starsCollected = 482;
        m_profile.hacksCompleted = 11;

        AppSettings::instance().rhdcUsername = m_username;
        AppSettings::instance().save();

        m_password.clear();
        m_passwordCell->clear();

        // The sign-in button is about to be hidden. Move focus onto the
        // signed-in card first so the focus system is never left pointing at
        // a view with Visibility::GONE.
        brls::Application::giveFocus(m_signedIn);
        updateState();

        brls::Logger::debug("Signed-in layout shown");
        dialog->close();
    });
    dialog->addButton("Cancel", [dialog]() { dialog->close(); });
    dialog->open();
}

void AccountTab::logout() {
    brls::Logger::debug("Signing out");

    m_profile = UserProfile {};
    m_password.clear();
    m_mfa.clear();
    m_passwordCell->clear();

    AppSettings::instance().rhdcToken.clear();
    AppSettings::instance().save();

    brls::Application::giveFocus(m_signedOut);
    updateState();
    brls::Application::notify("Signed out");
}

void AccountTab::updateState() {
    if (m_profile.isLoggedIn) {
        m_signedIn->setVisibility(brls::Visibility::VISIBLE);
        m_signedOut->setVisibility(brls::Visibility::GONE);
        m_profileName->setText(m_profile.username);
        m_profileMeta->setText(m_profile.role + "  ·  " + std::to_string(m_profile.starsCollected)
            + "★ collected  ·  " + std::to_string(m_profile.hacksCompleted) + " hacks completed");
    } else {
        m_signedIn->setVisibility(brls::Visibility::GONE);
        m_signedOut->setVisibility(brls::Visibility::VISIBLE);
    }
}

void AccountTab::applyTheme() {
    m_blurb->setTextColor(nxui::color("nx/text_secondary"));
    if (m_profileMeta)
        m_profileMeta->setTextColor(nxui::color("nx/text_secondary"));
}

void AccountTab::draw(NVGcontext* vg, float x, float y, float width, float height,
    brls::Style style, brls::FrameContext* ctx) {
    if (needsThemeRefresh())
        applyTheme();
    brls::Box::draw(vg, x, y, width, height, style, ctx);
}

brls::View* AccountTab::create() {
    return new AccountTab();
}
