# Main application icon

`app-icon.png` is the source artwork for the launcher, installer and product branding.
The design was generated with OpenAI image generation for this project: a blue
glass dial split into three equal sectors, a single performance needle, and a
dark rounded-square tile. The current icon is a circular crop of that original
dial, retaining its glass reflections and needle without the surrounding tile.
It is separate from the mode-specific monochrome tray
icons and the plugged-in/battery/Keep Awake illustrations.

`app-icon-source.png` preserves the generated artwork. Its exterior checkerboard
is removed by `node tools/prepare-main-icon.cjs` (requires `sharp`), producing a
reconstruction of the approved original, then a circular crop with a transparent
antialiased edge. The 880px RGBA output retains the original interior pixels and
adds 32px padding. No new artwork is generated for the circular version.

Run `node tools/export-main-icon.cjs` (requires `sharp`) to regenerate the
application outputs with Lanczos downsampling from the RGBA master:

- `src/PowerModeNative.ico`: 24 frames from 16 to 256px, including exact 175% DPI
  sizes of 28, 42, 56, 84 and 112px to avoid unnecessary Explorer rescaling.
- `site/assets/icon.png`: website brand and favicon.
- `docs/images/app-icon.png`: English and Chinese README brand.

Normal builds consume the committed ICO and do not require Node.js or `sharp`.

The installer also copies the ICO to `PowerSlider-glass-dial-3.ico` for shortcuts
and the uninstall entry. Change the design token in `AppIconName` when replacing
the artwork so Explorer does not reuse an older cached icon.

Approved design prompt: Three equal 120-degree pie-shaped glass sectors in a
full circular automotive performance dial, with one needle pointing upward and
right. Restrained blue Liquid Glass styling on a midnight navy rounded-square
tile. No text, numbers, tick marks or platform logos.
