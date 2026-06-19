import sharp from 'sharp';

async function generateIcon(size) {
  // Create white background with rounded corners
  const background = await sharp({
    create: {
      width: size,
      height: size,
      channels: 4,
      background: { r: 255, g: 255, b: 255, alpha: 1 }
    }
  })
  .png()
  .toBuffer();

  // Resize logo to 70% of icon size
  const logoSize = Math.floor(size * 0.7);
  const offset = Math.floor((size - logoSize) / 2);

  const logo = await sharp('./public/oss-logo.svg')
    .resize(logoSize, logoSize, { fit: 'contain', background: { r: 255, g: 255, b: 255, alpha: 0 } })
    .png()
    .toBuffer();

  // Composite logo on background
  const icon = await sharp(background)
    .composite([
      {
        input: logo,
        left: offset,
        top: offset
      }
    ])
    .png()
    .toBuffer();

  return icon;
}

// Generate both icon sizes
const icon192 = await generateIcon(192);
const icon512 = await generateIcon(512);

// Save to public directory
await sharp(icon192).toFile('./public/icon-192.png');
await sharp(icon512).toFile('./public/icon-512.png');

console.log('✓ Generated icon-192.png and icon-512.png');
