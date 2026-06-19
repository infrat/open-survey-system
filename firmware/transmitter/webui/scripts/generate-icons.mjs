import fs from 'fs';
import { createCanvas, loadImage } from 'canvas';

async function generateIcon(size) {
  const canvas = createCanvas(size, size);
  const ctx = canvas.getContext('2d');

  // Background with rounded corners
  const radius = size / 8;
  ctx.fillStyle = '#ffffff';
  ctx.beginPath();
  ctx.moveTo(radius, 0);
  ctx.lineTo(size - radius, 0);
  ctx.quadraticCurveTo(size, 0, size, radius);
  ctx.lineTo(size, size - radius);
  ctx.quadraticCurveTo(size, size, size - radius, size);
  ctx.lineTo(radius, size);
  ctx.quadraticCurveTo(0, size, 0, size - radius);
  ctx.lineTo(0, radius);
  ctx.quadraticCurveTo(0, 0, radius, 0);
  ctx.closePath();
  ctx.fill();

  // Load and draw OSS logo
  const logo = await loadImage('./public/oss-logo.png');
  const logoSize = size * 0.7;
  const logoX = (size - logoSize) / 2;
  const logoY = (size - logoSize) / 2;
  ctx.drawImage(logo, logoX, logoY, logoSize, logoSize);

  return canvas;
}

// Generate both icon sizes
const icon192 = await generateIcon(192);
const icon512 = await generateIcon(512);

// Save to public directory
fs.writeFileSync('./public/icon-192.png', icon192.toBuffer('image/png'));
fs.writeFileSync('./public/icon-512.png', icon512.toBuffer('image/png'));

console.log('✓ Generated icon-192.png and icon-512.png');
