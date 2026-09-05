# Changelog

## [1.0.3] - 2026-09-04

- Add a circular blue glass application icon for the app, installer, website and READMEs while retaining mode-specific tray icons.
- Improve small application icons with high-quality downsampling and additional DPI-specific frames.
- Use a separate, design-versioned icon file for shortcuts and the uninstall entry to bypass stale executable icon caches, and notify Explorer after installation.
- Preserve the last good tray image or use a resource fallback when icon creation fails; retry until the mode icon is available.
- Reconcile tray registration on the existing refresh timer, including when Explorer loses the icon without a notification.
- Handle repeated taskbar recreation notifications without getting stuck in a failed-registration state.
- Add regression tests for startup failures, recovery and icon handle ownership.

## [1.0.2] - 2026-09-04

- Enable Start with Windows by default while preserving an existing opt-out.
- Run the app with the Windows High priority class.
- Add a desktop-shortcut option to the installer, enabled by default.
- Repair startup and uninstall registration paths during upgrades.
- Close the running tray process cleanly before an install, upgrade or uninstall.
- Retry tray registration during sign-in and restore the icon after Explorer restarts.
- Preserve the original tray mouse protocol so each click toggles the panel only once.

## [1.0.1] - 2026-09-04

- Follow the Windows light or dark mode instead of the app theme setting.
- Add automatic English and Chinese website localization with a manual switch.
- Refresh the website and README preview around the taskbar tray experience.

## [1.0.0] - 2026-09-03

- Initial release of Windows 11 Power Slider.
- Add separate plugged-in and battery power-mode controls.
- Add Keep Awake with low-battery protection.
- Add Acrylic presentation, native motion and mode-aware tray icons.

[1.0.3]: https://github.com/kiteretsu903/windows-11-power-slider/releases/tag/v1.0.3
[1.0.2]: https://github.com/kiteretsu903/windows-11-power-slider/releases/tag/v1.0.2
[1.0.1]: https://github.com/kiteretsu903/windows-11-power-slider/releases/tag/v1.0.1
[1.0.0]: https://github.com/kiteretsu903/windows-11-power-slider/releases/tag/v1.0.0
