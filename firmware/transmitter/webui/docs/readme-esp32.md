# OSS RTCM Transmitter - Web Configuration UI

Mobile-first Progressive Web App for configuring Open Survey System RTCM LoRa transmitters.

## Features

- 📱 **Mobile-optimized** - Designed for phones/tablets
- 🔌 **Offline-ready** - PWA with service worker caching
- 📡 **WiFi Scanner** - Scan and select available networks
- 🗺️ **NTRIP Integration** - Fetch mountpoints from caster
- ⚙️ **LoRa Configuration** - Full SX1276 parameter control
- 🎯 **RTCM Filtering** - Configure message types and priorities

## Build for ESP32

To build the static HTML/JS/CSS bundle for ESP32 filesystem:

```bash
pnpm install
pnpm run build:esp32
```

This creates a `dist-esp32/` directory with all static files ready to upload to ESP32.

### Build Output

The build produces:

- `index.html` - Main HTML file
- `assets/` - JS/CSS bundles (all dependencies inlined, no CDN)
- PWA manifest and service worker
- Icon files

### Uploading to ESP32

1. Build the project with `pnpm run build:esp32`
2. Upload all files from `dist-esp32/` to ESP32's SPIFFS/LittleFS
3. Serve `index.html` from ESP32 web server at `/`
4. Ensure API endpoints are available at `/api/*`

## PWA Icons

The project includes auto-generated icons with the OSS logo in `public/`:

- `icon-192.png` (192x192px)
- `icon-512.png` (512x512px)

These are generated from `scripts/generate-icons-sharp.mjs` using the OSS logo (`public/oss-logo.png`). To regenerate:

```bash
pnpm run generate:icons
```

To customize:

1. Replace `public/oss-logo.png` with your own logo
2. Run `pnpm run generate:icons`
3. Rebuild with `pnpm run build:esp32`

## API Endpoints

The UI expects these endpoints on the ESP32:

- `GET /api/config` - Get current configuration
- `POST /api/config` - Save configuration (triggers restart)
- `POST /api/restart` - Restart device
- `POST /api/wifi/scan` - Scan WiFi networks
- `POST /api/ntrip/mountpoints` - Fetch NTRIP mountpoints

See `src/imports/config-structure.md` for full API specification.

## Development

For local development (not for ESP32):

```bash
pnpm install
pnpm run dev
```

Note: You'll need to mock the API endpoints or proxy to a real device.

## Configuration Sections

1. **WiFi** - Network credentials and connection timeout
2. **NTRIP** - Caster host, port, mountpoint, authentication
3. **GGA Position** - Base station coordinates and send interval
4. **LoRa Radio** - Frequency, spreading factor, bandwidth, coding rate, TX power, sync word
5. **RTCM Filtering** - Allowed message types and priority types

## Technology Stack

- React 18
- TypeScript
- Tailwind CSS v4
- Radix UI (shadcn/ui components)
- Vite 6 (build tool)
- vite-plugin-pwa (PWA support)
