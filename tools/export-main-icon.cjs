// Export approved artwork, without redrawing it. Requires Node.js and sharp.
const fs = require('node:fs/promises');
const path = require('node:path');
const sharp = require('sharp');
const root = path.resolve(__dirname, '..');

// Include exact 175% DPI sizes (28, 42, 56, 84, 112) so Explorer need not
// enlarge a smaller frame. Export directly from the RGBA master.
const sizes = [16, 20, 24, 28, 32, 36, 40, 42, 48, 56, 60, 64, 72, 80,
  84, 96, 112, 128, 144, 160, 168, 192, 224, 256];

(async () => {
  const frames = await Promise.all(sizes.map(size =>
    sharp(path.join(root, 'assets/app-icon.png'))
      .resize(size, size, {kernel: 'lanczos3', fastShrinkOnLoad: false})
      .png().toBuffer()));
  const directory = Buffer.alloc(6 + 16 * sizes.length);
  directory.writeUInt16LE(1, 2);
  directory.writeUInt16LE(sizes.length, 4);
  let offset = directory.length;
  for (let i = 0; i < sizes.length; i++) {
    const entry = 6 + i * 16;
    directory[entry] = directory[entry + 1] = sizes[i] === 256 ? 0 : sizes[i];
    directory.writeUInt16LE(1, entry + 4);
    directory.writeUInt16LE(32, entry + 6);
    directory.writeUInt32LE(frames[i].length, entry + 8);
    directory.writeUInt32LE(offset, entry + 12);
    offset += frames[i].length;
  }
  await fs.writeFile(path.join(root, 'src/PowerModeNative.ico'), Buffer.concat([directory, ...frames]));
  await fs.writeFile(path.join(root, 'site/assets/icon.png'), frames.at(-1));
  await fs.writeFile(path.join(root, 'docs/images/app-icon.png'), frames.at(-1));
  console.log(`Exported ${sizes.length} Lanczos-resampled RGBA icon frames: ${sizes.join(', ')}.`);
})().catch(error => { console.error(error); process.exitCode = 1; });
