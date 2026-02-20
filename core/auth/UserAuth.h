#pragma once

#include "shared/JuceIncludes.h"

namespace kindpath::auth
{
    // UserAuth manages local user accounts for the KindPath Q standalone app.
    // Credentials (email + hashed password) are stored in the user's app data
    // directory. Passwords are hashed with SHA-256 before storage.
    class UserAuth
    {
    public:
        UserAuth();

        // Register a new account. Returns false and sets errorOut on failure.
        bool registerUser(const juce::String& email,
                          const juce::String& password,
                          juce::String& errorOut);

        // Log in with an existing account. Returns false and sets errorOut on failure.
        bool loginUser(const juce::String& email,
                       const juce::String& password,
                       juce::String& errorOut);

        void logout();

        bool isLoggedIn() const noexcept { return loggedIn; }
        juce::String getCurrentUserEmail() const { return currentUserEmail; }

    private:
        static juce::String hashPassword(const juce::String& password);
        static juce::File getCredentialsFile();

        bool loadUsers(juce::var& outArray) const;
        bool saveUsers(const juce::var& usersArray) const;

        bool loggedIn = false;
        juce::String currentUserEmail;
    };
}
