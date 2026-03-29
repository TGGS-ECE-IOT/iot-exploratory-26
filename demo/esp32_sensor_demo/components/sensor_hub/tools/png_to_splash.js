#!/usr/bin/env node

const fs = require("fs");
const path = require("path");

let PNG;
try {
  PNG = require("pngjs").PNG;
} catch (err) {
  console.error("pngjs not found. Run: npm install pngjs --no-save");
  process.exit(1);
}

function getArg(name) {
  const idx = process.argv.indexOf(name);
  if (idx < 0 || idx + 1 >= process.argv.length) return null;
  return process.argv[idx + 1];
}

const input = getArg("--input");
const outC = getArg("--out-c");
const outH = getArg("--out-h");

if (!input || !outC || !outH) {
  console.error("Usage: png_to_splash.js --input <png> --out-c <file.c> --out-h <file.h>");
  process.exit(1);
}

const pngBuf = fs.readFileSync(input);
const png = PNG.sync.read(pngBuf);

if (png.width !== 128 || png.height !== 64) {
  console.error(`Expected 128x64 PNG, got ${png.width}x${png.height}`);
  process.exit(1);
}

const bytes = [];
for (let y = 0; y < png.height; y++) {
  for (let xByte = 0; xByte < png.width / 8; xByte++) {
    let b = 0;
    for (let bit = 0; bit < 8; bit++) {
      const x = xByte * 8 + bit;
      const idx = (y * png.width + x) * 4;
      const a = png.data[idx + 3];
      if (a > 16) {
        b |= (1 << bit);
      }
    }
    bytes.push(b);
  }
}

const header = `#ifndef SPLASH_ASSETS_H
#define SPLASH_ASSETS_H

#include <stdint.h>

#define ECE_LOGO_WIDTH 128
#define ECE_LOGO_HEIGHT 64

extern const uint8_t ece_logo_128x64_bits[];

#endif
`;

let body = "#include <stdint.h>\n\n";
body += "const uint8_t ece_logo_128x64_bits[] = {\n";
for (let i = 0; i < bytes.length; i++) {
  if (i % 16 === 0) body += "    ";
  body += `0x${bytes[i].toString(16).padStart(2, "0")}`;
  body += i < bytes.length - 1 ? ", " : "";
  if (i % 16 === 15) body += "\n";
}
if (bytes.length % 16 !== 0) body += "\n";
body += "};\n";

fs.mkdirSync(path.dirname(outC), { recursive: true });
fs.mkdirSync(path.dirname(outH), { recursive: true });
fs.writeFileSync(outH, header);
fs.writeFileSync(outC, body);
