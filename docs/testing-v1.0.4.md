# v1.0.4 validation

The tester on the previously affected Windows 11 26100 machine reported a
successful startup with the v1.0.4 candidate. Its diagnostic trace confirmed
tray registration and icon rectangle lookup succeeded before font initialization.
This validates that run, but does not establish the exact cause of the old failure.

## Confirmed fixes and checks

- An existing v1.0.3 process opened its flyout on all three repeated `--startup`
  launches. The installed v1.0.4 candidate stayed hidden in the same test.
- Foreground denial and focus loss during a context menu now dismiss the flyout.
  Tests cover the activation grace period, owned focus, preview exclusion, and
  cancelled slider capture without applying a power change.
- The context menu and executable resources share `src/version.h`.
- A tray integration test confirms registration occurs without initializing
  the font factories or Acrylic backdrop. The existing tray recovery, icon
  ownership, animation and Keep Awake regression tests remain passing.
- `test-flyout.exe <pid> cycles` requires foreground permission. Exit 18 means
  the test launcher cannot obtain that permission, not that interactive opening
  passed. The local user confirmed normal tray opening, outside-click dismissal
  and the `1.0.4` menu entry.

## Manual checks

1. Check that the tray menu shows `1.0.4`.
2. Open and dismiss the panel several times using the tray icon and clicks on
   another window. Open the tray menu while the panel is visible, then dismiss
   the menu outside the app. The panel must not remain over the other window.
3. Open the desktop shortcut while the app is running. It should open the panel;
   repeated sign-in/startup launches must not open it.
4. Check installation and the first launch on an affected machine. Do not
   interpret a successful run as proof that every possible failure is resolved.

The release removes both lifecycle and opt-in per-frame logging. It does not
create or append diagnostic log files. Previously generated test logs are left
untouched, and no raw tester logs are included in the repository or release.
