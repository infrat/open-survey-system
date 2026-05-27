#!/usr/bin/env node

const fs = require('fs');
const path = require('path');

/**
 * Converts OLED vertical byte-packed bitmap to XBM horizontal bit-packed format
 * OLED format: 8 rows × 128 bytes, each byte = 8 vertical pixels (LSB = top)
 * XBM format: horizontal scanlines, each byte = 8 horizontal pixels (LSB = left)
 */

const WIDTH = 128;
const HEIGHT = 64;

function parseOLEDBitmap(fileContent) {
    // Extract hex values from the C array
    const hexPattern = /0x[0-9a-fA-F]{2}/g;
    const matches = fileContent.match(hexPattern);
    
    if (!matches) {
        throw new Error('No hex values found in file');
    }
    
    const bytes = matches.map(hex => parseInt(hex, 16));
    
    if (bytes.length !== 1024) { // 128 * 8
        throw new Error(`Expected 1024 bytes, got ${bytes.length}`);
    }
    
    return bytes;
}

function convertOLEDtoXBM(oledBytes) {
    // Create a 2D pixel array [y][x]
    const pixels = Array(HEIGHT).fill(0).map(() => Array(WIDTH).fill(0));
    
    // Read OLED format: 8 rows of 128 bytes
    // Each byte represents 8 vertical pixels
    for (let x = 0; x < WIDTH; x++) {
        for (let row = 0; row < 8; row++) {
            const byteData = oledBytes[row * WIDTH + x];
            for (let bit = 0; bit < 8; bit++) {
                if (byteData & (1 << bit)) {
                    pixels[row * 8 + bit][x] = 1;
                }
            }
        }
    }
    
    // Convert to XBM format: horizontal scanlines
    // Each byte = 8 horizontal pixels, LSB first
    const xbmBytes = [];
    
    for (let y = 0; y < HEIGHT; y++) {
        for (let x = 0; x < WIDTH; x += 8) {
            let byte = 0;
            for (let bit = 0; bit < 8; bit++) {
                if (pixels[y][x + bit]) {
                    byte |= (1 << bit);
                }
            }
            xbmBytes.push(byte);
        }
    }
    
    return xbmBytes;
}

function generateXBMFile(xbmBytes, width, height) {
    let output = `#define splash_width ${width}\n`;
    output += `#define splash_height ${height}\n`;
    output += `static const unsigned char splash_bits[] = {\n`;
    
    // Format as hex with 12 values per line
    const lines = [];
    for (let i = 0; i < xbmBytes.length; i += 12) {
        const chunk = xbmBytes.slice(i, i + 12);
        const hexStrings = chunk.map(b => `0x${b.toString(16).padStart(2, '0')}`);
        lines.push('  ' + hexStrings.join(', '));
    }
    
    output += lines.join(',\n');
    output += '\n};\n';
    
    return output;
}

function main() {
    const args = process.argv.slice(2);
    
    if (args.length < 1) {
        console.log('Usage: node convert_to_xbm.js <input_file> [output_file]');
        console.log('Example: node convert_to_xbm.js oss_bitmap.h splash-xbm.h');
        process.exit(1);
    }
    
    const inputFile = args[0];
    const outputFile = args[1] || 'splash-xbm.h';
    
    try {
        console.log(`Reading ${inputFile}...`);
        const fileContent = fs.readFileSync(inputFile, 'utf8');
        
        console.log('Parsing OLED bitmap...');
        const oledBytes = parseOLEDBitmap(fileContent);
        
        console.log('Converting to XBM format...');
        const xbmBytes = convertOLEDtoXBM(oledBytes);
        
        console.log('Generating XBM file...');
        const xbmContent = generateXBMFile(xbmBytes, WIDTH, HEIGHT);
        
        fs.writeFileSync(outputFile, xbmContent);
        console.log(`✓ Successfully created ${outputFile}`);
        console.log(`  Image size: ${WIDTH}x${HEIGHT} pixels`);
        console.log(`  XBM bytes: ${xbmBytes.length}`);
        
    } catch (error) {
        console.error('Error:', error.message);
        process.exit(1);
    }
}

main();