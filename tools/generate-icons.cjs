// Original ink drawings for Aura Minimal. GPL-3.0-only.
// npm install sharp, then: node tools/generate-icons.cjs
const fs = require('fs');
const path = require('path');
const sharp = require('sharp');
const root = path.resolve(__dirname, '..');
const assets = path.join(root, 'website/assets/icons');
fs.mkdirSync(assets, { recursive: true });
const cloud = '<path d="M15 43C6 43 5 31 13 28C11 17 27 12 33 22C40 14 53 21 50 31C62 32 59 44 49 44Z" fill="white"/>';
const hatch = '<path d="m36 37 4-4m1 7 5-5m1 6 4-4" stroke-width="1.3"/>';
const rays = '<path d="m32 3-1 8m19 0-5 6m15 14-9 1m0 20-6-6m-14 14 1-8M11 51l6-6M3 31l9 1m0-21 6 6"/>';
function drawing(kind, small) {
  let art = '';
  if (kind === 'sun') art = rays + '<path d="M46 30c1 9-6 16-14 16S17 40 18 31s6-15 14-15 14 6 14 14Z" fill="white"/>' +
    (small ? '' : '<path d="m26 29 1-1m11 1-1-1m-10 8q5 4 10-1" stroke-width="1.6"/><path d="m21 33 3-3m-2 7 4-4" stroke-width="1"/>');
  if (kind === 'moon') art = '<path d="M43 9C19 0 7 26 18 44c9 15 32 12 39-5-18 5-33-15-14-30Z" fill="white"/>' +
    (small ? '' : '<path d="m18 31 5-5m-3 10 5-5m-3 10 5-5" stroke-width="1.2"/>') + '<path d="m51 9 1 4 4 1-4 1-1 4-1-4-4-1 4-1Z" fill="black" stroke-width="1"/>';
  if (kind === 'partly') art = '<path d="m21 2-1 6m15-1-4 4M7 11l4 4M2 25l6-1m31-4 6-2"/><circle cx="22" cy="23" r="12" fill="white"/>' + cloud + (small ? '' : hatch);
  if (kind === 'nightcloud') art = '<path d="M30 5C9 1 5 24 19 33c7 4 15 2 19-5-14 3-21-14-8-23Z" fill="white"/>' + cloud + (small ? '' : hatch);
  if (['cloud', 'rain', 'storm', 'snow', 'fog'].includes(kind)) {
    art = '<g transform="translate(0 -5)">' + cloud + (small ? '' : hatch) + '</g>';
    if (kind === 'cloud') art += '<path d="M8 51h12m26 1h10" stroke-width="1.5"/>';
    if (kind === 'rain') art += '<path d="m16 47-4 8m19-8-4 9m21-9-4 8"/>';
    if (kind === 'storm') art += '<path d="m34 39-9 13h8l-4 10 16-16h-9l5-7Z" fill="black" stroke-width="1.2"/>';
    if (kind === 'snow') art += '<path d="M16 47v12m-5-9 10 6m-10 0 10-6m20-3v12m-5-9 10 6m-10 0 10-6" stroke-width="1.8"/>';
    if (kind === 'fog') art += '<path d="M8 47h38m-31 7h41m-47 6h33"/>';
  }
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><g stroke="#000" stroke-width="${small ? 3.5 : 2.7}" fill="none" stroke-linecap="round" stroke-linejoin="round">${art}</g></svg>`;
}
async function main() {
  const kinds = ['sun','moon','partly','nightcloud','cloud','rain','storm','snow','fog'];
  let c = '// Original manga-inspired ink weather icons. GPL-3.0-only.\n#include "manga_icons.h"\n';
  let header = '#pragma once\n#include <lvgl.h>\n#ifdef __cplusplus\nextern "C" {\n#endif\n';
  for (const kind of kinds) {
    for (const size of [20,48]) {
      const svg = drawing(kind, size === 20);
      fs.writeFileSync(path.join(assets, `${kind}-${size}.svg`), svg);
      const pipeline = sharp(Buffer.from(svg)).resize(size,size);
      await pipeline.clone().png().toFile(path.join(assets, `${kind}-${size}.png`));
      const rgba = await pipeline.ensureAlpha().raw().toBuffer();
      const bytes = [];
      for (let p=0; p<rgba.length; p+=4) {
        const rgb = ((rgba[p] >> 3) << 11) | ((rgba[p+1] >> 2) << 5) | (rgba[p+2] >> 3);
        bytes.push(rgb & 255, rgb >> 8);
      }
      for (let p=3; p<rgba.length; p+=4) bytes.push(rgba[p]);
      const name = `manga_${kind}_${size}`;
      c += `\nstatic const uint8_t ${name}_data[] = {\n`;
      for (let i=0; i<bytes.length; i+=24) c += '  '+bytes.slice(i,i+24).map(v=>'0x'+v.toString(16).padStart(2,'0')).join(',')+',\n';
      c += `};\nconst lv_image_dsc_t ${name} = {\n  .header.magic = LV_IMAGE_HEADER_MAGIC,\n  .header.cf = LV_COLOR_FORMAT_RGB565A8,\n  .header.flags = 0,\n  .header.w = ${size}, .header.h = ${size}, .header.stride = ${size*2},\n  .data_size = sizeof(${name}_data), .data = ${name}_data, .reserved = NULL\n};\n`;
      header += `extern const lv_image_dsc_t ${name};\n`;
    }
  }
  header += '#ifdef __cplusplus\n}\n#endif\n\nstatic inline const lv_image_dsc_t *manga_weather_image(int code, int day, bool large) {\n';
  header += '#define MANGA_ICON(kind) (large ? &manga_##kind##_48 : &manga_##kind##_20)\n';
  header += '  if (code == 0) return day ? MANGA_ICON(sun) : MANGA_ICON(moon);\n';
  header += '  if (code == 1 || code == 2) return day ? MANGA_ICON(partly) : MANGA_ICON(nightcloud);\n';
  header += '  if (code == 45 || code == 48) return MANGA_ICON(fog);\n';
  header += '  if (code >= 95) return MANGA_ICON(storm);\n';
  header += '  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return MANGA_ICON(snow);\n';
  header += '  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return MANGA_ICON(rain);\n';
  header += '  return MANGA_ICON(cloud);\n#undef MANGA_ICON\n}\n';
  fs.writeFileSync(path.join(root,'firmware/aura_minimal/manga_icons.c'), c);
  fs.writeFileSync(path.join(root,'firmware/aura_minimal/manga_icons.h'), header);
  console.log('Generated 9 original ink icons at 20 and 48 pixels, plus firmware assets.');
}
main().catch(e=>{console.error(e);process.exit(1)});
