# OSS RTCM Transmitter - Configuration UI

Mobile-first Progressive Web App for configuring Open Survey System RTCM LoRa GNSS transmitters.

![OSS RTCM Transmitter](public/oss-logo.svg)

## Features

- 📱 **Mobile-optimized** - Designed for phones and tablets
- 🔌 **Offline-ready** - Works without internet connectivity
- 📡 **WiFi Scanner** - Scan and select available networks
- 🗺️ **NTRIP Integration** - Fetch mountpoints from caster
- 📍 **GPS Location** - Auto-fill coordinates using device GPS
- ⚙️ **LoRa Configuration** - Full SX1276 parameter control
- 🎯 **RTCM Filtering** - Advanced message type picker with descriptions
- ❓ **Built-in Help** - Context-sensitive help for each configuration section
- 🎨 **PWA Support** - Install as app on home screen

## Quick Start

### Development

```bash
# Install dependencies
pnpm install

# Start dev server (PWA features won't work in dev mode)
pnpm dev
```

### Build for ESP32

```bash
# Build optimized bundle for ESP32
pnpm run build:esp32

# Output: dist-esp32/
# Upload entire directory to ESP32 SPIFFS/LittleFS
```

### Regenerate PWA Icons

```bash
# After updating public/oss-logo.svg
pnpm run generate:icons
```

## Configuration Sections

1. **WiFi** - Network credentials, connection timeout, network scanner
2. **NTRIP** - Caster connection, mountpoint selection
3. **GGA Position** - Base station coordinates with GPS auto-fill
4. **LoRa Radio** - Frequency, spreading factor, bandwidth, coding rate, TX power
5. **RTCM Filtering** - Message type selection with priorities

## Tech Stack

- React 18 + TypeScript
- Tailwind CSS v4 (mobile-first)
- Radix UI / shadcn/ui components
- Vite 6 build tool
- vite-plugin-pwa for Progressive Web App support

## Project Structure

```
src/
├── app/
│   ├── App.tsx              # Main app with tabs
│   ├── components/
│   │   ├── config/          # Config forms for each tab
│   │   ├── modals/          # WiFi scan, mountpoints, help
│   │   └── ui/              # Reusable UI components
│   ├── hooks/
│   │   └── useConfig.ts     # API communication
│   ├── types/
│   │   └── config.ts        # TypeScript interfaces
│   └── data/
│       ├── rtcm-messages.ts # RTCM message definitions
│       └── help-content.ts  # Help text content
public/
├── oss-logo.svg             # OSS logo (vector)
├── icon-192.png             # PWA icon 192x192
├── icon-512.png             # PWA icon 512x512
└── manifest.json            # PWA manifest
```

## API Endpoints

The UI expects these endpoints on the ESP32:

- `GET /api/config` - Get current configuration
- `POST /api/config` - Save configuration (triggers restart)
- `POST /api/restart` - Restart device
- `POST /api/wifi/scan` - Scan WiFi networks
- `POST /api/ntrip/mountpoints` - Fetch NTRIP mountpoints

See `src/imports/config-structure.md` for full API specification.

## Development

For detailed development guide, see:

- **[docs/development.md](docs/development.md)** - Architecture, patterns, how to add features
- **[contributing.md](contributing.md)** - Contributing guidelines, PR process
- **[.github/copilot-instructions.md](.github/copilot-instructions.md)** - GitHub Copilot context

## Building & Deploying

### Production Build

```bash
pnpm run build:esp32
```

This creates a `dist-esp32/` directory with:

- `index.html` - Main HTML file
- `assets/` - Bundled JS/CSS (no external dependencies)
- PWA manifest and service worker
- Icon files

### ESP32 Deployment

1. Build the project
2. Upload contents of `dist-esp32/` to ESP32 filesystem (SPIFFS/LittleFS)
3. Serve `index.html` from root path `/`
4. Ensure API endpoints available at `/api/*`

### Local Testing

```bash
# Build
pnpm run build:esp32

# Serve locally
cd dist-esp32
python3 -m http.server 8080

# Open http://localhost:8080 in browser
# PWA features will work on localhost
```

## PWA Installation

### On Mobile (iOS/Android)

1. Open the config UI in Safari (iOS) or Chrome (Android)
2. Tap Share → "Add to Home Screen"
3. Icon with OSS logo will appear on home screen
4. Opens in standalone mode (no browser UI)

**Note**: PWA features (service worker, geolocation) work best when installed as home screen app.

### Geolocation

The app can auto-fill GGA coordinates using device GPS:

- Works in installed PWA mode
- Requires location permission
- Gracefully degrades to manual input if unavailable
- GPS works offline (no internet needed for satellites)

## Browser Support

- Chrome/Edge 90+ (recommended)
- Safari 15+ (iOS)
- Firefox 88+

PWA features require HTTPS or localhost (ESP32 AP is HTTP-only, but works when installed).

## License

Open source - see LICENSE file for details.

## Related Projects

- ESP32 Firmware: [Link to firmware repo]
- Hardware Design: [Link to hardware repo]
- Documentation: [Link to docs]

## Contributing

See [contributing.md](contributing.md) for development setup and guidelines.

## Support

For issues, questions, or feature requests:

- GitHub Issues: [Link to issues]
- Documentation: See `docs/development.md` and `src/imports/config-structure.md`
