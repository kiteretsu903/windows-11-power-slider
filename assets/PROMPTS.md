# macOS 26 inspired glass icon assets

Generated with the built-in GPT image tool on 2026-09-04.
Source art: power.png (plug, also app and tray), battery.png, coffee.png.
These are generated artwork, not Apple assets or emoji.
The RGB outputs contain white mattes. prepare-assets.ps1 keys the matte to alpha,
preserves aspect ratio, and packages UI PNGs and multi-size ICO.

## Prompt set

Common direction: macOS 26 Liquid Glass visual style, restrained translucent
beveled edges, subtle specular rim, front elevation, clean simple silhouette
legible at 24px. Transparent alpha background; if unavailable, pure white.
No checkerboard, tile, text, emoji or cast shadow.

- Plug: sapphire blue electrical plug with exactly two parallel upward prongs,
  compact rounded body and short centered cable stem. No lightning or battery.
  Also serves as tray and application icon.
- Battery: horizontal emerald-teal battery, right terminal, three charge bars.
  No lightning, plug or charging symbol.
- Coffee: honey-amber glass coffee cup with small handle on the right, no saucer,
  no steam, front-facing rounded silhouette.
