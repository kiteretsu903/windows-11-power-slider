// One-time/reproducible alpha extraction from the approved generated artwork.
// Requires sharp, as does render-readme.cjs. Does not redraw the icon interior.
const path = require('node:path');
const sharp = require('sharp');
const root = path.resolve(__dirname, '..');

(async () => {
  const {data, info} = await sharp(path.join(root, 'assets/app-icon-source.png'))
    .ensureAlpha().raw().toBuffer({resolveWithObject: true});
  const {width, height} = info;
  const visited = new Uint8Array(width * height);
  const queue = new Uint32Array(width * height);
  let head = 0, tail = 0;
  const visit = i => {
    if (visited[i]) return;
    const p = i * 4;
    const low = Math.min(data[p], data[p + 1], data[p + 2]);
    const high = Math.max(data[p], data[p + 1], data[p + 2]);
    // The exterior checkerboard is neutral near-white. Only connected exterior
    // pixels are removed; white reflections surrounded by blue glass survive.
    if (low < 175 || high - low > 28) return;
    visited[i] = 1;
    queue[tail++] = i;
  };
  for (let x = 0; x < width; x++) { visit(x); visit((height - 1) * width + x); }
  for (let y = 0; y < height; y++) { visit(y * width); visit(y * width + width - 1); }
  while (head < tail) {
    const i = queue[head++], x = i % width, y = Math.floor(i / width);
    if (x) visit(i - 1);
    if (x + 1 < width) visit(i + 1);
    if (y) visit(i - width);
    if (y + 1 < height) visit(i + width);
  }
  if (tail < width * height * .05 || tail > width * height * .5)
    throw new Error('Unexpected background area; inspect artwork before exporting.');
  let left = width, top = height, right = 0, bottom = 0;
  for (let i = 0; i < visited.length; i++) {
    const p = i * 4;
    if (visited[i]) { data[p] = data[p + 1] = data[p + 2] = data[p + 3] = 0; continue; }
    const x = i % width, y = Math.floor(i / width);
    left = Math.min(left, x); top = Math.min(top, y);
    right = Math.max(right, x); bottom = Math.max(bottom, y);
  }
  const tile = await sharp(data, {raw: info})
    .extract({left, top, width: right - left + 1, height: bottom - top + 1})
    .resize(960, 960, {fit: 'contain', background: '#00000000', kernel: 'lanczos3'})
    .png().toBuffer();
  await sharp({create: {width: 1024, height: 1024, channels: 4, background: '#00000000'}})
    .composite([{input: tile, left: 32, top: 32}])
    .png().toFile(path.join(root, 'assets/app-icon.png'));
  console.log(`Exported 1024px RGBA app icon; removed ${tail} exterior pixels.`);
})().catch(error => { console.error(error); process.exitCode = 1; });
