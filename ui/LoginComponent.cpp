#include "ui/LoginComponent.h"

namespace kindpath::ui
{
    static constexpr int kFormWidth  = 320;
    static constexpr int kFormHeight = 330;
    static constexpr int kPanelPadX  = 24;
    static constexpr int kPanelPadY  = 16;

    LoginComponent::LoginComponent(kindpath::auth::UserAuth& auth)
        : userAuth(auth)
    {
        titleLabel.setText("KindPath Q", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(28.0f, juce::Font::bold));
        titleLabel.setJustificationType(juce::Justification::centred);
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(titleLabel);

        emailLabel.setText("Email", juce::dontSendNotification);
        emailLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
        addAndMakeVisible(emailLabel);

        emailField.setInputRestrictions(128);
        emailField.setJustification(juce::Justification::centredLeft);
        emailField.setTextToShowWhenEmpty("you@example.com", juce::Colour(0xff555555));
        addAndMakeVisible(emailField);

        passwordLabel.setText("Password", juce::dontSendNotification);
        passwordLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
        addAndMakeVisible(passwordLabel);

        passwordField.setPasswordCharacter(0x2022); // bullet point
        passwordField.setInputRestrictions(128);
        passwordField.setJustification(juce::Justification::centredLeft);
        passwordField.setTextToShowWhenEmpty("6 characters minimum", juce::Colour(0xff555555));
        addAndMakeVisible(passwordField);

        primaryButton.setButtonText("Log In");
        primaryButton.onClick = [this] { attemptLogin(); };
        addAndMakeVisible(primaryButton);

        toggleButton.setButtonText("Create an account");
        toggleButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        toggleButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        toggleButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff7eb8f7));
        toggleButton.setColour(juce::TextButton::textColourOnId,  juce::Colour(0xff7eb8f7));
        toggleButton.onClick = [this] { setMode(! isRegisterMode); };
        addAndMakeVisible(toggleButton);

        statusLabel.setJustificationType(juce::Justification::centred);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::orangered);
        addAndMakeVisible(statusLabel);
    }

    void LoginComponent::resized()
    {
        const int cx = (getWidth()  - kFormWidth)  / 2;
        const int cy = (getHeight() - kFormHeight) / 2;

        titleLabel.setBounds    (cx, cy,       kFormWidth, 40);
        emailLabel.setBounds    (cx, cy + 56,  kFormWidth, 18);
        emailField.setBounds    (cx, cy + 76,  kFormWidth, 32);
        passwordLabel.setBounds (cx, cy + 120, kFormWidth, 18);
        passwordField.setBounds (cx, cy + 140, kFormWidth, 32);
        primaryButton.setBounds (cx, cy + 192, kFormWidth, 36);
        toggleButton.setBounds  (cx, cy + 240, kFormWidth, 28);
        statusLabel.setBounds   (cx, cy + 275, kFormWidth, 44);
    }

    void LoginComponent::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colour(0xff0b0d12));

        const int cx = (getWidth()  - kFormWidth)  / 2 - kPanelPadX;
        const int cy = (getHeight() - kFormHeight) / 2 - kPanelPadY;

        g.setColour(juce::Colour(0xff1a1d24));
        g.fillRoundedRectangle(static_cast<float>(cx),
                               static_cast<float>(cy),
                               static_cast<float>(kFormWidth  + kPanelPadX * 2),
                               static_cast<float>(kFormHeight + kPanelPadY * 2),
                               10.0f);
    }

    void LoginComponent::setMode(bool registerMode)
    {
        isRegisterMode = registerMode;
        statusLabel.setText({}, juce::dontSendNotification);

        if (isRegisterMode)
        {
            primaryButton.setButtonText("Create Account");
            primaryButton.onClick = [this] { attemptRegister(); };
            toggleButton.setButtonText("Already have an account? Log in");
        }
        else
        {
            primaryButton.setButtonText("Log In");
            primaryButton.onClick = [this] { attemptLogin(); };
            toggleButton.setButtonText("Create an account");
        }
    }

    void LoginComponent::showStatus(const juce::String& message, bool isError)
    {
        statusLabel.setColour(juce::Label::textColourId,
                              isError ? juce::Colours::orangered
                                      : juce::Colours::limegreen);
        statusLabel.setText(message, juce::dontSendNotification);
    }

    void LoginComponent::attemptLogin()
    {
        juce::String error;
        if (userAuth.loginUser(emailField.getText(), passwordField.getText(), error))
        {
            if (onAuthSuccess)
                onAuthSuccess(userAuth.getCurrentUserEmail());
        }
        else
        {
            showStatus(error, true);
        }
    }

    void LoginComponent::attemptRegister()
    {
        juce::String error;
        if (userAuth.registerUser(emailField.getText(), passwordField.getText(), error))
        {
            if (onAuthSuccess)
                onAuthSuccess(userAuth.getCurrentUserEmail());
        }
        else
        {
            showStatus(error, true);
        }
    }
}
