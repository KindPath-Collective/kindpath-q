#pragma once

#include "shared/JuceIncludes.h"
#include "core/auth/UserAuth.h"

namespace kindpath::ui
{
    // LoginComponent presents email/password fields and handles login and registration.
    // Set onAuthSuccess to receive a callback when the user successfully authenticates.
    class LoginComponent : public juce::Component
    {
    public:
        explicit LoginComponent(kindpath::auth::UserAuth& auth);

        // Called after a successful login or registration with the authenticated email.
        std::function<void(const juce::String& email)> onAuthSuccess;

        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        void attemptLogin();
        void attemptRegister();
        void setMode(bool registerMode);
        void showStatus(const juce::String& message, bool isError);

        kindpath::auth::UserAuth& userAuth;

        juce::Label    titleLabel;
        juce::Label    emailLabel;
        juce::TextEditor emailField;
        juce::Label    passwordLabel;
        juce::TextEditor passwordField;
        juce::TextButton primaryButton;  // "Log In" or "Create Account"
        juce::TextButton toggleButton;   // Switch between modes
        juce::Label    statusLabel;

        bool isRegisterMode = false;
    };
}
