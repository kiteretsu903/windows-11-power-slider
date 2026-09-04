// Rasterize upstream Fluent and project-native tray artwork.
const fs = require('node:fs');
const path = require('node:path');
const sharp = require('sharp');
const root = path.resolve(__dirname, '..');
(async () => {
  const names = ['fluent/leaf_one', 'tray/efficiency', 'tray/balanced', 'fluent/flash'];
  const sizes = [16, 20, 24, 28, 32, 40, 48, 64];
  const masks = [];
  const previews = [];
  for (const [i, name] of names.entries()) {
    const svg = fs.readFileSync(path.join(root, 'assets', name + '.svg'));
    // Remove SVG artboard whitespace first. Keep only a 1-DIP safety margin,
    // and center the actual ink, not the original square viewBox.
    const cropped = await sharp(svg, {density: 768}).trim().png().toBuffer();
    for (const size of sizes) {
      const pad = Math.max(1, Math.round(size / 16));
      const {data} = await sharp(cropped).resize(size - pad * 2, size - pad * 2,
        {fit: 'contain', background: '#00000000'}).ensureAlpha()
        .extend({top: pad, bottom: pad, left: pad, right: pad, background: '#00000000'})
        .raw().toBuffer({resolveWithObject: true});
      const alpha = Array.from({length: size * size}, (_, j) => data[j * 4 + 3]);
      masks.push(`inline constexpr std::uint8_t kTrayAlpha${size}_${i}[]={\n` +
        Array.from({length: size}, (_, y) => '    ' + alpha.slice(y * size, (y + 1) * size).join(',')).join(',\n') + '\n};\n');
      if (size === 48 || size === 16) previews.push({input: await sharp(data,
        {raw: {width: size, height: size, channels: 4}}).png().toBuffer(),
        left: i * 120 + (120 - size) / 2, top: size === 48 ? 16 : 104});
    }
  }
  fs.writeFileSync(path.join(root, 'src', 'tray_artwork.h'),
    '// Generated from Fluent icons and custom tray SVGs (MIT). See assets/fluent and assets/tray.\n' +
    '#pragma once\n#include <cstdint>\n' + masks.join('\n') +
    'struct TrayArtwork { int size; const std::uint8_t* modes[4]; };\n' +
    'inline constexpr TrayArtwork kTrayArtwork[]={\n' + sizes.map(size =>
      `    {${size}, {${names.map((_, i) => `kTrayAlpha${size}_${i}`).join(',')}}}`).join(',\n') + '\n};\n');
  const labels = ['Saver', 'Efficiency', 'Balanced', 'Performance'];
  const text = Buffer.from('<svg width="480" height="100" xmlns="http://www.w3.org/2000/svg">' +
    labels.map((s, i) => `<text x="${i * 120 + 60}" y="87" text-anchor="middle" font-family="Segoe UI" font-size="13" fill="#202020">${s}</text>`).join('') + '</svg>');
  fs.mkdirSync(path.join(root, 'build'), {recursive: true});
  await sharp({create: {width: 480, height: 136, channels: 4, background: '#f2f2f2'}})
    .composite([...previews, {input: text, left: 0, top: 0}]).png().toFile(path.join(root, 'build', 'tray-preview.png'));
})();
