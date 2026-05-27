import { Button } from '../ui/button';
import { Input } from '../ui/input';
import { Label } from '../ui/label';
import { Download, Loader2 } from 'lucide-react';
import type { Config } from '../../types/config';

interface NTRIPConfigProps {
  config: Config['ntrip'];
  onChange: (ntrip: Config['ntrip']) => void;
  onFetchClick: () => void;
  isFetching?: boolean;
}

export function NTRIPConfig({ config, onChange, onFetchClick, isFetching }: NTRIPConfigProps) {
  return (
    <div className="space-y-4">
      <div className="space-y-2">
        <Label htmlFor="ntrip-host">Host *</Label>
        <Input
          id="ntrip-host"
          value={config.host}
          onChange={(e) => onChange({ ...config, host: e.target.value })}
          placeholder="e.g., 91.198.76.2"
          required
        />
      </div>

      <div className="space-y-2">
        <Label htmlFor="ntrip-port">Port *</Label>
        <Input
          id="ntrip-port"
          type="number"
          value={config.port}
          onChange={(e) => onChange({ ...config, port: parseInt(e.target.value) || 0 })}
          min={1}
          max={65535}
          required
        />
      </div>

      <div className="space-y-2">
        <Label htmlFor="ntrip-user">Username</Label>
        <Input
          id="ntrip-user"
          value={config.user}
          onChange={(e) => onChange({ ...config, user: e.target.value })}
          placeholder="Optional"
        />
      </div>

      <div className="space-y-2">
        <Label htmlFor="ntrip-password">Password</Label>
        <Input
          id="ntrip-password"
          type="password"
          value={config.password}
          onChange={(e) => onChange({ ...config, password: e.target.value })}
          placeholder="Optional"
        />
      </div>

      <div className="space-y-2">
        <Label htmlFor="ntrip-mountpoint">Mountpoint *</Label>
        <div className="flex gap-2">
          <Input
            id="ntrip-mountpoint"
            value={config.mountpoint}
            onChange={(e) => onChange({ ...config, mountpoint: e.target.value })}
            placeholder="e.g., RTN4G_VRS_RTCM32"
            required
          />
          <Button
            type="button"
            variant="outline"
            size="icon"
            onClick={onFetchClick}
            disabled={isFetching || !config.host || !config.port}
          >
            {isFetching ? <Loader2 className="size-4 animate-spin" /> : <Download className="size-4" />}
          </Button>
        </div>
      </div>

      <div className="space-y-2">
        <Label htmlFor="ntrip-timeout">Connection Timeout (ms) *</Label>
        <Input
          id="ntrip-timeout"
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
