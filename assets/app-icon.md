# Main application icon

`app-icon.png` is the source artwork for the launcher, installer and product branding.
The design was generated with OpenAI image generation for this project: a blue
glass dial split into three equal sectors, a single performance needle, and a
dark rounded-square tile. It is separate from the mode-specific monochrome tray
icons and the plugged-in/battery/Keep Awake illustrations.

`app-icon-source.png` preserves the generated artwork. Its exterior checkerboard
is removed by `node tools/prepare-main-icon.cjs` (requires `sharp`), producing a
1024px RGBA master with real transparent corners and 32px outer padding.

Run `tools/prepare-assets.ps1` to regenerate the application outputs:

- `src/PowerModeNative.ico`: 16, 20, 24, 32, 40, 48, 64, 128 and 256px frames.
- `site/assets/icon.png`: website brand and favicon.
- `docs/images/app-icon.png`: English and Chinese README brand.

The installer also copies the ICO to `PowerSlider-glass-dial-1.ico` for shortcuts
and the uninstall entry. Change the design token in `AppIconName` when replacing
the artwork so Explorer does not reuse an older cached icon.

Approved design prompt: Three equal 120-degree pie-shaped glass sectors in a
full circular automotive performance dial, with one needle pointing upward and
right. Restrained blue Liquid Glass styling on a midnight navy rounded-square
tile. No text, numbers, tick marks or platform logos.
