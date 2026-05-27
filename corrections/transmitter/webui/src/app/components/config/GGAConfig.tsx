import { type ClipboardEvent } from 'react';
import { Input } from '../ui/input';
import { Label } from '../ui/label';
import { MapPin, ExternalLink } from 'lucide-react';
import { toast } from 'sonner';
import type { Config } from '../../types/config';

interface GGAConfigProps {
  config: Config['gga'];
  onChange: (gga: Config['gga']) => void;
}

/**
 * Parse a pasted string that may contain a lat/lon coordinate pair.
 * Supports formats:
 *  - "50.1378, 19.1874"  (Google Maps style)
 *  - "50.1378,19.1874"
 *  - "50.1378 19.1874"
 *  - "50,1378 19,1874"   (European comma-decimal, space separator)
 */
function parseCoordinatePair(text: string): { lat: number; lon: number } | null {
  const trimmed = text.trim();

  // Dot-decimal: "50.1378, 19.1874" / "50.1378 19.1874" / "50.1378,19.1874"
  const dotMatch = /^(-?\d+\.?\d*)[,\s]+\s*(-?\d+\.?\d*)$/.exec(trimmed);
  if (dotMatch) {
    const lat = Number.parseFloat(dotMatch[1]);
    const lon = Number.parseFloat(dotMatch[2]);
    if (Number.isFinite(lat) && Number.isFinite(lon) && lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) {
      return { lat, lon };
    }
  }

  // European comma-decimal: "50,1378 19,1874"
  const commaMatch = /^(-?\d+,\d+)\s+(-?\d+,\d+)$/.exec(trimmed);
  if (commaMatch) {
    const lat = Number.parseFloat(commaMatch[1].replace(',', '.'));
    const lon = Number.parseFloat(commaMatch[2].replace(',', '.'));
    if (Number.isFinite(lat) && Number.isFinite(lon) && lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) {
      return { lat, lon };
    }
  }

  return null;
}

export function GGAConfig({ config, onChange }: Readonly<GGAConfigProps>) {
  const handleCoordinatePaste = (e: ClipboardEvent<HTMLInputElement>) => {
    const text = e.clipboardData.getData('text');
    const parsed = parseCoordinatePair(text);
    if (parsed) {
      e.preventDefault();
      onChange({
        ...config,
        latitude: Math.round(parsed.lat * 10000) / 10000,
        longitude: Math.round(parsed.lon * 10000) / 10000,
      });
      toast.success('Coordinates parsed: ' + parsed.lat.toFixed(4) + ', ' + parsed.lon.toFixed(4));
    }
  };

  const mapsUrl = config.latitude && config.longitude
    ? `https://www.google.com/maps/search/?api=1&query=${config.latitude},${config.longitude}`
    : 'https://www.google.com/maps';

  return (
    <div className="space-y-4">
      <div className="bg-blue-50 dark:bg-blue-950/20 border border-blue-200 dark:border-blue-800 rounded-lg p-3">
        <div className="flex items-center gap-2 mb-2">
          <MapPin className="size-4 text-blue-600 dark:text-blue-400" />
          <span className="text-sm font-medium text-blue-900 dark:text-blue-100">
            Get coordinates from maps
          </span>
        </div>
        <a
          href={mapsUrl}
          target="_blank"
          rel="noopener noreferrer"
          className="inline-flex items-center gap-1.5 text-xs font-medium text-blue-600 dark:text-blue-400 hover:underline"
        >
          <ExternalLink className="size-3" />
          Open Google Maps
        </a>
        <p className="text-xs text-blue-900/70 dark:text-blue-100/70 mt-2">
          Open maps, find your location, copy coordinates and paste into any coordinate field below
        </p>
      </div>

      <div className="space-y-2">
        <Label htmlFor="gga-latitude">Latitude (°) *</Label>
        <Input
          id="gga-latitude"
          type="number"
          value={config.latitude}
          onChange={(e) => onChange({ ...config, latitude: Number.parseFloat(e.target.value) || 0 })}
          onPaste={handleCoordinatePaste}
          min={-90}
          max={90}
          step={0.0001}
          required
        />
        <p className="text-xs text-muted-foreground">Decimal degrees, positive = North. Paste "lat, lon" to fill both fields.</p>
      </div>

      <div className="space-y-2">
        <Label htmlFor="gga-longitude">Longitude (°) *</Label>
        <Input
          id="gga-longitude"
          type="number"
          value={config.longitude}
          onChange={(e) => onChange({ ...config, longitude: Number.parseFloat(e.target.value) || 0 })}
          onPaste={handleCoordinatePaste}
          min={-180}
          max={180}
          step={0.0001}
          required
        />
        <p className="text-xs text-muted-foreground">Decimal degrees. Paste "lat, lon" to fill both fields.</p>
      </div>

      <div className="space-y-2">
        <Label htmlFor="gga-altitude">Altitude (m) *</Label>
        <Input
          id="gga-altitude"
          type="number"
          value={config.altitude}
          onChange={(e) => onChange({ ...config, altitude: Number.parseFloat(e.target.value) || 0 })}
          step={0.1}
          required
        />
        <p className="text-xs text-muted-foreground">Meters above sea level</p>
      </div>

      <div className="space-y-2">
        <Label htmlFor="gga-interval">Send Interval (ms) *</Label>
        <Input
          id="gga-interval"
          type="number"
          value={config.sendInterval}
          onChange={(e) => onChange({ ...config, sendInterval: Number.parseInt(e.target.value) || 0 })}
          min={1000}
          max={60000}
          step={1000}
          required
        />
        <p className="text-xs text-muted-foreground">How often to send GGA to NTRIP caster</p>
      </div>
    </div>
  );
}
