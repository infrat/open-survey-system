import { Button } from '../ui/button';
import { Input } from '../ui/input';
import { Label } from '../ui/label';
import { Wifi, Loader2 } from 'lucide-react';
import type { Config } from '../../types/config';

interface WiFiConfigProps {
  config: Config['wifi'];
  onChange: (wifi: Config['wifi']) => void;
  onScanClick: () => void;
  isScanning?: boolean;
}

export function WiFiConfig({ config, onChange, onScanClick, isScanning }: WiFiConfigProps) {
  return (
    <div className="space-y-4">
      <div className="space-y-2">
        <Label htmlFor="wifi-ssid">WiFi Network (SSID) *</Label>
        <div className="flex gap-2">
          <Input
            id="wifi-ssid"
            value={config.ssid}
            onChange={(e) => onChange({ ...config, ssid: e.target.value })}
            placeholder="Network name"
            required
          />
          <Button
            type="button"
            variant="outline"
            size="icon"
            onClick={onScanClick}
            disabled={isScanning}
          >
            {isScanning ? <Loader2 className="size-4 animate-spin" /> : <Wifi className="size-4" />}
          </Button>
        </div>
      </div>

      <div className="space-y-2">
        <Label htmlFor="wifi-password">Password</Label>
        <Input
          id="wifi-password"
          type="password"
          value={config.password}
          onChange={(e) => onChange({ ...config, password: e.target.value })}
          placeholder="Leave empty for open network"
        />
      </div>

      <div className="space-y-2">
        <Label htmlFor="wifi-timeout">Connection Timeout (ms) *</Label>
        <Input
          id="wifi-timeout"
          type="number"
          value={config.timeout}
          onChange={(e) => onChange({ ...config, timeout: parseInt(e.target.value) || 0 })}
          min={5000}
          max={60000}
          step={1000}
          required
        />
      </div>
    </div>
  );
}
