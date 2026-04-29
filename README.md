# hyprpolkitagent
A simple polkit authentication agent for Hyprland, written in QT/QML.

![](./assets/polkit.png)

## Features

### Core UX / flow
- [x] Uses PAM request text for the prompt (not always "Password")
- [x] Surfaces PAM info/error messages in the UI
- [x] Disables Authenticate until input is present; resets input on failure
- [x] Retry UX (clears field, refocuses, shows visible error)
- [x] Password visibility toggle (show/hide)
- [x] Caps Lock warning on the password field

### Identity handling
- [x] Prefers the current user when multiple identities are supplied
- [x] User selector when multiple admin identities are available

### Robustness
- [x] Queues concurrent requests instead of rejecting them
- [x] Consistent cleanup on cancel (cancels running PAM session, distinguishes paths)
- [x] Refuses to prompt while the session is locked (logind `LockedHint`)

### Security / privacy
- [x] Zeroizes password buffer after submission

### Transparency / polish
- [x] Expandable "Details" section (action id, vendor, vendor URL, extras)
- [x] Surfaces "Remember authorization" notice when policy supports it
- [x] Action/app icon support (uses `iconName`/details, falls back to system theme)
- [x] User avatar display (via AccountsService)
- [x] User-visible strings wrapped for translation (`qsTr` / `tr`)

### Window placement
Window positioning under Wayland (xdg-shell) is decided by the compositor, so
the agent does not attempt to set absolute coordinates. Hyprland centers
floating dialogs by default.

## Usage

See [the hyprland wiki](https://wiki.hyprland.org/Hypr-Ecosystem/hyprpolkitagent/)
