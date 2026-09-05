<img src="docs/images/app-icon.png" width="96" height="96" alt="Windows 11 Power Slider icon" />

# Windows 11 Power Slider

English · [简体中文](README.zh-CN.md)

A lightweight **native C++ / Win32** power-mode tray app with a modern Acrylic interface and low idle memory usage.

[Website](https://kiteretsu903.github.io/windows-11-power-slider/) · [Download](https://github.com/kiteretsu903/windows-11-power-slider/releases/latest)

![Windows 11 Power Slider with its taskbar tray icon](docs/images/preview.png)

Inspired by [PowerModeSlider](https://github.com/giulioungaretti/PowerModeSlider), independently rebuilt with separate AC/battery controls and a conventional installer.

## Features

- **Native and lightweight.** No .NET, WinUI 3, Qt or WebView runtime. Graphics resources are released when the panel closes.
- **Modern appearance.** Acrylic blur that follows the Windows light/dark mode, smooth flyout motion and DPI-aware tray icons showing the current mode.
- **Separate power controls.** Best power efficiency, Balanced and Best performance, with separate plugged-in and battery sliders.
- **Keep Awake with battery protection.** Keep the system and screen awake. Low battery or a sleep transition turns it off, without automatically re-enabling it.
- **Simple setup.** Portable EXE or per-user installer with a desktop shortcut, automatic English/Chinese selection and startup enabled by default. No certificate imports.

## Usage

Run `PowerModeNative.exe` or use the installer.

- Click the tray icon to open or close the panel. Clicking outside also closes it.
- Right-click for language, startup settings or Exit.
- Startup is enabled by default and can be disabled from the tray menu.

## Notes

Windows 11 x64; power-mode support depends on the APIs available on your device. The current build is unsigned. Acrylic uses an undocumented Windows API, with a solid-color fallback.

The battery safeguard releases this app's wake requests. Windows remains responsible for sleep, hibernation or shutdown; sudden power loss cannot be prevented.

## Build

Visual Studio C++ Build Tools, Windows SDK and CMake; Inno Setup 6 for the installer.

```powershell
.\build.ps1 -Configuration Release -Installer
```

Or build the portable EXE with LLVM-MinGW:

```powershell
.\build-portable.ps1 -Toolchain 'D:\Tools\llvm-mingw'
```

[Changelog](CHANGELOG.md) · [MIT License](LICENSE) · [Security](SECURITY.md) · [Icon credits](assets/fluent/SOURCE.md)
