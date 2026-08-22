#pragma once

#include <string>

#include <borealis.hpp>

#include "src/core/models.hpp"
#include "src/ui/theme.hpp"
#include "src/ui/views/widgets.hpp"

// Romhacking.com sign-in. The network call is not wired up yet; the form,
// validation and signed-in/signed-out states are.
class AccountTab : public brls::Box, public nxui::ThemeAware {
  public:
    AccountTab();

    static brls::View* create();

    void draw(NVGcontext* vg, float x, float y, float width, float height,
        brls::Style style, brls::FrameContext* ctx) override;

  private:
    void applyTheme();
    void updateState();
    void attemptLogin();
    void logout();

    UserProfile m_profile;
    std::string m_username;
    std::string m_password;
    std::string m_mfa;

    brls::Box*          m_signedOut  = nullptr;
    brls::Box*          m_signedIn   = nullptr;
    nxui::StatusBanner* m_error      = nullptr;
    brls::InputCell*    m_usernameCell = nullptr;
    nxui::PasswordCell* m_passwordCell = nullptr;
    brls::InputCell*    m_mfaCell    = nullptr;
    brls::Button*       m_loginButton = nullptr;
    brls::Label*        m_profileName = nullptr;
    brls::Label*        m_profileMeta = nullptr;
    brls::Label*        m_blurb       = nullptr;
};
