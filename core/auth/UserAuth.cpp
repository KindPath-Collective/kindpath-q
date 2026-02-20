#include "core/auth/UserAuth.h"

namespace kindpath::auth
{
    // A static application-specific salt prepended before hashing.
    static constexpr auto kSalt = "kindpath-q-v0.1:";

    UserAuth::UserAuth() = default;

    juce::String UserAuth::hashPassword(const juce::String& password)
    {
        const juce::String salted = kSalt + password;
        juce::SHA256 sha(salted.toRawUTF8(), salted.getNumBytesAsUTF8());
        return sha.toHexString();
    }

    juce::File UserAuth::getCredentialsFile()
    {
        const auto appData = juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory);
        const auto dir = appData.getChildFile("KindPath Q");
        dir.createDirectory();
        return dir.getChildFile("users.json");
    }

    bool UserAuth::loadUsers(juce::var& outArray) const
    {
        const auto file = getCredentialsFile();
        if (! file.existsAsFile())
        {
            outArray = juce::Array<juce::var>();
            return true;
        }

        outArray = juce::JSON::parse(file.loadFileAsString());
        if (! outArray.isArray())
            outArray = juce::Array<juce::var>();

        return true;
    }

    bool UserAuth::saveUsers(const juce::var& usersArray) const
    {
        return getCredentialsFile().replaceWithText(
            juce::JSON::toString(usersArray, true));
    }

    bool UserAuth::registerUser(const juce::String& email,
                                const juce::String& password,
                                juce::String& errorOut)
    {
        const auto trimmedEmail = email.trim();

        if (trimmedEmail.isEmpty())
        {
            errorOut = "Email cannot be empty.";
            return false;
        }

        if (! trimmedEmail.containsChar('@'))
        {
            errorOut = "Please enter a valid email address.";
            return false;
        }

        if (password.length() < 6)
        {
            errorOut = "Password must be at least 6 characters.";
            return false;
        }

        juce::var users;
        if (! loadUsers(users))
        {
            errorOut = "Failed to load user data.";
            return false;
        }

        const auto emailLower = trimmedEmail.toLowerCase();

        if (const auto* arr = users.getArray())
        {
            for (const auto& u : *arr)
            {
                if (u["email"].toString() == emailLower)
                {
                    errorOut = "An account with this email already exists.";
                    return false;
                }
            }
        }

        juce::DynamicObject::Ptr record = new juce::DynamicObject();
        record->setProperty("email", emailLower);
        record->setProperty("passwordHash", hashPassword(password));

        if (users.getArray() == nullptr)
            users = juce::Array<juce::var>();

        users.getArray()->add(juce::var(record.get()));

        if (! saveUsers(users))
        {
            errorOut = "Failed to save user data.";
            return false;
        }

        loggedIn = true;
        currentUserEmail = emailLower;
        return true;
    }

    bool UserAuth::loginUser(const juce::String& email,
                             const juce::String& password,
                             juce::String& errorOut)
    {
        if (email.trim().isEmpty() || password.isEmpty())
        {
            errorOut = "Email and password are required.";
            return false;
        }

        juce::var users;
        if (! loadUsers(users))
        {
            errorOut = "Failed to load user data.";
            return false;
        }

        const auto emailLower = email.trim().toLowerCase();
        const auto hash = hashPassword(password);

        if (const auto* arr = users.getArray())
        {
            for (const auto& u : *arr)
            {
                if (u["email"].toString() == emailLower &&
                    u["passwordHash"].toString() == hash)
                {
                    loggedIn = true;
                    currentUserEmail = emailLower;
                    return true;
                }
            }
        }

        errorOut = "Invalid email or password.";
        return false;
    }

    void UserAuth::logout()
    {
        loggedIn = false;
        currentUserEmail = {};
    }
}
