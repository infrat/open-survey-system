import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogDescription } from '../ui/dialog';
import { Button } from '../ui/button';
import { Wifi, Lock, RefreshCw } from 'lucide-react';
import type { WiFiNetwork } from '../../types/config';

interface WiFiScanModalProps {
  open: boolean;
  onOpenChange: (open: boolean) => void;
  networks: WiFiNetwork[];
  onNetworkSelect: (ssid: string) => void;
  onRescan: () => void;
  isScanning: boolean;
}

function getSignalIcon(rssi: number) {
  if (rssi >= -50) return '▂▄▆█';
  if (rssi >= -60) return '▂▄▆';
  if (rssi >= -70) return '▂▄';
  return '▂';
}

export function WiFiScanModal({
  open,
  onOpenChange,
  networks,
  onNetworkSelect,
  onRescan,
  isScanning,
}: WiFiScanModalProps) {
  const sortedNetworks = [...networks].sort((a, b) => b.rssi - a.rssi);

  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent className="max-w-md max-h-[80vh] flex flex-col">
        <DialogHeader>
          <DialogTitle>Available WiFi Networks</DialogTitle>
          <DialogDescription>Select a network to configure your device</DialogDescription>
        </DialogHeader>

        <div className="flex-1 overflow-y-auto space-y-1 min-h-0">
          {sortedNetworks.length === 0 ? (
            <div className="text-center py-8 text-muted-foreground">
              {isScanning ? 'Scanning...' : 'No networks found'}
            </div>
          ) : (
            sortedNetworks.map((network, idx) => (
              <button
                key={`${network.bssid}-${idx}`}
                onClick={() => {
                  onNetworkSelect(network.ssid);
                  onOpenChange(false);
                }}
                className="w-full flex items-center gap-3 p-3 rounded-lg hover:bg-accent text-left transition-colors"
              >
                <Wifi className="size-5 flex-shrink-0" />
                <div className="flex-1 min-w-0">
                  <div className="flex items-center gap-2">
                    <span className="font-medium truncate">{network.ssid}</span>
                    {network.encryption === 'secured' && <Lock className="size-3 flex-shrink-0" />}
                  </div>
                  <div className="text-xs text-muted-foreground flex gap-2">
                    <span>{network.bssid}</span>
                    <span>Ch {network.channel}</span>
                  </div>
                </div>
                <div className="text-right flex-shrink-0">
                  <div className="font-mono text-sm">{getSignalIcon(network.rssi)}</div>
                  <div className="text-xs text-muted-foreground">{network.rssi} dBm</div>
                </div>
              </button>
            ))
          )}
        </div>

        <div className="flex-shrink-0 pt-4 border-t">
          <Button
            onClick={onRescan}
            disabled={isScanning}
            variant="outline"
            className="w-full"
          >
            <RefreshCw className={`size-4 mr-2 ${isScanning ? 'animate-spin' : ''}`} />
            {isScanning ? 'Scanning...' : 'Rescan'}
          </Button>
        </div>
      </DialogContent>
    </Dialog>
  );
}
