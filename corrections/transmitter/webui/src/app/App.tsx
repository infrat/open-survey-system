import { useState } from 'react';
import { Tabs, TabsContent, TabsList, TabsTrigger } from './components/ui/tabs';
import { Button } from './components/ui/button';
import { AlertDialog, AlertDialogAction, AlertDialogCancel, AlertDialogContent, AlertDialogDescription, AlertDialogFooter, AlertDialogHeader, AlertDialogTitle } from './components/ui/alert-dialog';
import { WiFiConfig } from './components/config/WiFiConfig';
import { NTRIPConfig } from './components/config/NTRIPConfig';
import { GGAConfig } from './components/config/GGAConfig';
import { LoRaConfig } from './components/config/LoRaConfig';
import { RTCMConfig } from './components/config/RTCMConfig';
import { MaintenanceConfig } from './components/config/MaintenanceConfig';
import { WiFiScanModal } from './components/modals/WiFiScanModal';
import { MountpointsModal } from './components/modals/MountpointsModal';
import { HelpModal } from './components/modals/HelpModal';
import { useConfig } from './hooks/useConfig';
import { Toaster } from './components/ui/sonner';
import { toast } from 'sonner';
import { Save, RefreshCw, Power, Radio, HelpCircle, Wrench } from 'lucide-react';
import type { WiFiNetwork } from './types/config';
import { HELP_CONTENT } from './data/help-content';
import { OSSLogo } from './components/OSSLogo';

export default function App() {
  const {
    config,
    setConfig,
    loading,
    restarting,
    error,
    loadConfig,
    saveConfig,
    restartDevice,
    scanWiFi,
    fetchMountpoints,
  } = useConfig();

  const busy = loading || restarting;

  const [showSaveDialog, setShowSaveDialog] = useState(false);
  const [showRestartDialog, setShowRestartDialog] = useState(false);
  const [showWiFiModal, setShowWiFiModal] = useState(false);
  const [showMountpointsModal, setShowMountpointsModal] = useState(false);
  const [showHelpModal, setShowHelpModal] = useState(false);
  const [activeTab, setActiveTab] = useState<keyof typeof HELP_CONTENT>('wifi');
  const [wifiNetworks, setWifiNetworks] = useState<WiFiNetwork[]>([]);
  const [mountpoints, setMountpoints] = useState<string[]>([]);
  const [isScanning, setIsScanning] = useState(false);
  const [isFetching, setIsFetching] = useState(false);

  const handleSave = async () => {
    // Validation
    if (!config.wifi.ssid) {
      toast.error('WiFi SSID is required');
      return;
    }
    if (!config.ntrip.host) {
      toast.error('NTRIP host is required');
      return;
    }
    if (config.ntrip.port < 1 || config.ntrip.port > 65535) {
      toast.error('NTRIP port must be between 1 and 65535');
      return;
    }
    if (config.gga.latitude < -90 || config.gga.latitude > 90) {
      toast.error('Latitude must be between -90 and 90');
      return;
    }
    if (config.gga.longitude < -180 || config.gga.longitude > 180) {
      toast.error('Longitude must be between -180 and 180');
      return;
    }
    if (config.lora.frequency < 137e6 || config.lora.frequency > 1020e6) {
      toast.error('LoRa frequency out of valid range (137-1020 MHz)');
      return;
    }
    if (config.lora.spreadingFactor < 6 || config.lora.spreadingFactor > 12) {
      toast.error('Spreading factor must be between 6 and 12');
      return;
    }
    if (config.lora.txPower < 2 || config.lora.txPower > 20) {
      toast.error('TX power must be between 2 and 20 dBm');
      return;
    }
    if (config.rtcm.messageTypes.length === 0) {
      toast.error('At least one RTCM message type must be configured');
      return;
    }

    setShowSaveDialog(true);
  };

  const confirmSave = async () => {
    setShowSaveDialog(false);
    const success = await saveConfig(config);
    if (success) {
      toast.success('Configuration saved. Device reconnected.');
    } else {
      toast.error('Failed to save configuration');
    }
  };

  const handleRestart = async () => {
    setShowRestartDialog(false);
    const success = await restartDevice();
    if (success) {
      toast.success('Device restarted successfully.');
    } else {
      toast.error('Failed to restart device');
    }
  };

  const handleWiFiScan = async () => {
    setIsScanning(true);
    setShowWiFiModal(true);
    const networks = await scanWiFi();
    setWifiNetworks(networks);
    setIsScanning(false);
  };

  const handleMountpointsFetch = async () => {
    setIsFetching(true);
    setShowMountpointsModal(true);
    const mps = await fetchMountpoints(
      config.ntrip.host,
      config.ntrip.port,
      config.ntrip.user,
      config.ntrip.password
    );
    setMountpoints(mps);
    setIsFetching(false);
  };

  const openHelp = () => {
    setShowHelpModal(true);
  };

  return (
    <div className="min-h-screen bg-background">
      <div className="max-w-2xl mx-auto p-4 pb-24">
        <header className="mb-6">
          <div className="flex items-center gap-3 mb-2">
            <OSSLogo className="h-10 w-auto text-foreground" />
            <h1 className="text-2xl font-bold">OSS RTCM Transmitter</h1>
          </div>
          <p className="text-sm text-muted-foreground">
            Configure your RTCM LoRa transmitter
          </p>
        </header>

        <Tabs defaultValue="wifi" className="w-full" onValueChange={(v) => setActiveTab(v as keyof typeof HELP_CONTENT)}>
          <TabsList className="grid w-full grid-cols-6 mb-6">
            <TabsTrigger value="wifi">WiFi</TabsTrigger>
            <TabsTrigger value="ntrip">NTRIP</TabsTrigger>
            <TabsTrigger value="gga">GGA</TabsTrigger>
            <TabsTrigger value="lora">LoRa</TabsTrigger>
            <TabsTrigger value="rtcm">RTCM</TabsTrigger>
            <TabsTrigger value="maintenance"><Wrench className="h-3 w-3" /></TabsTrigger>
          </TabsList>

          <TabsContent value="wifi">
            <WiFiConfig
              config={config.wifi}
              onChange={(wifi) => setConfig({ ...config, wifi })}
              onScanClick={handleWiFiScan}
              isScanning={isScanning}
            />
          </TabsContent>

          <TabsContent value="ntrip">
            <NTRIPConfig
              config={config.ntrip}
              onChange={(ntrip) => setConfig({ ...config, ntrip })}
              onFetchClick={handleMountpointsFetch}
              isFetching={isFetching}
            />
          </TabsContent>

          <TabsContent value="gga">
            <GGAConfig
              config={config.gga}
              onChange={(gga) => setConfig({ ...config, gga })}
            />
          </TabsContent>

          <TabsContent value="lora">
            <LoRaConfig
              config={config.lora}
              onChange={(lora) => setConfig({ ...config, lora })}
            />
          </TabsContent>

          <TabsContent value="rtcm">
            <RTCMConfig
              config={config.rtcm}
              onChange={(rtcm) => setConfig({ ...config, rtcm })}
            />
          </TabsContent>

          <TabsContent value="maintenance">
            <MaintenanceConfig
              config={config.maintenance}
              onChange={(maintenance) => setConfig({ ...config, maintenance })}
            />
          </TabsContent>
        </Tabs>
      </div>

      <div className="fixed bottom-0 left-0 right-0 bg-background border-t p-4 shadow-lg">
        <div className="max-w-2xl mx-auto flex gap-2">
          {restarting ? (
            <div className="flex-1 flex items-center justify-center gap-2 h-9 text-sm text-muted-foreground">
              <RefreshCw className="size-4 animate-spin" />
              Reconnecting to device...
            </div>
          ) : (
            <>
              <Button
                onClick={handleSave}
                disabled={busy}
                className="flex-1"
              >
                <Save className="size-4 mr-2" />
                Save & Restart
              </Button>
              <Button
                onClick={loadConfig}
                disabled={busy}
                variant="outline"
              >
                <RefreshCw className={`size-4 ${loading ? 'animate-spin' : ''}`} />
              </Button>
              <Button
                onClick={openHelp}
                disabled={busy}
                variant="outline"
              >
                <HelpCircle className="size-4" />
              </Button>
              <Button
                onClick={() => setShowRestartDialog(true)}
                disabled={busy}
                variant="destructive"
              >
                <Power className="size-4" />
              </Button>
            </>
          )}
        </div>
      </div>

      <WiFiScanModal
        open={showWiFiModal}
        onOpenChange={setShowWiFiModal}
        networks={wifiNetworks}
        onNetworkSelect={(ssid) => setConfig({ ...config, wifi: { ...config.wifi, ssid } })}
        onRescan={async () => {
          setIsScanning(true);
          const networks = await scanWiFi();
          setWifiNetworks(networks);
          setIsScanning(false);
        }}
        isScanning={isScanning}
      />

      <MountpointsModal
        open={showMountpointsModal}
        onOpenChange={setShowMountpointsModal}
        mountpoints={mountpoints}
        onMountpointSelect={(mp) => setConfig({ ...config, ntrip: { ...config.ntrip, mountpoint: mp } })}
        onRefresh={async () => {
          setIsFetching(true);
          const mps = await fetchMountpoints(
            config.ntrip.host,
            config.ntrip.port,
            config.ntrip.user,
            config.ntrip.password
          );
          setMountpoints(mps);
          setIsFetching(false);
        }}
        isFetching={isFetching}
      />

      <AlertDialog open={showSaveDialog} onOpenChange={setShowSaveDialog}>
        <AlertDialogContent>
          <AlertDialogHeader>
            <AlertDialogTitle>Save Configuration?</AlertDialogTitle>
            <AlertDialogDescription>
              This will save the configuration and restart the device. The device will be unavailable for a few seconds.
            </AlertDialogDescription>
          </AlertDialogHeader>
          <AlertDialogFooter>
            <AlertDialogCancel>Cancel</AlertDialogCancel>
            <AlertDialogAction onClick={confirmSave}>Save & Restart</AlertDialogAction>
          </AlertDialogFooter>
        </AlertDialogContent>
      </AlertDialog>

      <AlertDialog open={showRestartDialog} onOpenChange={setShowRestartDialog}>
        <AlertDialogContent>
          <AlertDialogHeader>
            <AlertDialogTitle>Restart Device?</AlertDialogTitle>
            <AlertDialogDescription>
              This will restart the device without saving changes. The device will be unavailable for a few seconds.
            </AlertDialogDescription>
          </AlertDialogHeader>
          <AlertDialogFooter>
            <AlertDialogCancel>Cancel</AlertDialogCancel>
            <AlertDialogAction onClick={handleRestart}>Restart</AlertDialogAction>
          </AlertDialogFooter>
        </AlertDialogContent>
      </AlertDialog>

      <HelpModal
        open={showHelpModal}
        onOpenChange={setShowHelpModal}
        content={HELP_CONTENT[activeTab]}
      />

      <Toaster />
    </div>
  );
}