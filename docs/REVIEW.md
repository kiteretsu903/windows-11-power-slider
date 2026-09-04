# Windows 11 Power Slider v1.0.0 validation

## Verified

- Native C++ / Win32 x64 build completes with LLVM-MinGW.
- Keep Awake battery safety, tray click handling, native motion, four tray status glyphs, DPI sizes and icon handle cleanup pass the automated test suite.
- The production flyout passes repeated hide/show, interrupted animation and tray-click close tests.
- Best power efficiency was previously verified through the production power controller with the original mode restored afterward.
- The per-user installer succeeds, reports version 1.0.0 and installs a byte-identical application payload.
- GitHub Pages deploys from site/ and its public pages, canonical URLs and sitemap return HTTP 200 without indexing blocks.

## Release boundaries

- Windows 11 x64 only. Power-mode API availability depends on the device.
- The executable is unsigned and may trigger an unknown-publisher or SmartScreen warning.
- Acrylic uses an undocumented Windows interface and falls back to a solid background when unavailable.
- Keep Awake releases this app's request at low battery. Windows and firmware remain responsible for final sleep, hibernation or shutdown.
- Full keyboard navigation and UI Automation semantics are not claimed for v1.0.0.
