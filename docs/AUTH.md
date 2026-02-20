# KindPath Q — User Authentication

## Overview

The standalone app shows a login/register screen before the main UI. User
credentials are stored locally — no network requests are made.

## How It Works

### Credential Storage

- File: `~/Library/Application Support/KindPath Q/users.json` (macOS)
- Format: JSON array of `{ "email": "...", "passwordHash": "..." }` objects
- Passwords are SHA-256 hashed with a static application salt before storage

### Classes

| Class | Location | Responsibility |
|---|---|---|
| `kindpath::auth::UserAuth` | `core/auth/UserAuth.h/.cpp` | Account management (register, login, logout) |
| `kindpath::ui::LoginComponent` | `ui/LoginComponent.h/.cpp` | JUCE UI for login / registration |

### Flow (Standalone App)

1. `MainWindow` creates a `LoginComponent` as its initial content.
2. The user either logs in (existing account) or registers (new account).
3. On success, `LoginComponent::onAuthSuccess` fires, and `MainWindow` swaps
   the content to `StandaloneMainComponent`.
4. Logout is not exposed in v0.1 UI — quitting and relaunching returns to the
   login screen.

## Validation Rules

- Email must be non-empty and contain `@`.
- Password must be ≥ 6 characters.
- Emails are stored and matched case-insensitively.

## Plugin (AU / VST3)

The plugin editor does not require authentication. Plugin instances run inside
a host DAW process and share no session state with the standalone app. Auth for
cloud-backed plugin features is a future consideration.

## Future Improvements

- Per-user salts and PBKDF2/bcrypt for stronger password hashing.
- "Stay logged in" session token persisted across launches.
- Email validation beyond `@` presence.
- Password reset flow (requires cloud backend).
