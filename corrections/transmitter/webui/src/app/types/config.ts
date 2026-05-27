export interface Config {
  wifi: {
    ssid: string;
    password: string;
    timeout: number;
  };
  ntrip: {
    host: string;
    port: number;
    mountpoint: string;
    user: string;
    password: string;
    timeout: number;
  };
  gga: {
    latitude: number;
    longitude: number;
    altitude: number;
    sendInterval: number;
  };
  lora: {
    frequency: number; // Hz in API
    spreadingFactor: number;
    bandwidth: number;
    codingRate: number;
    txPower: number;
    syncWord: number; // integer in API
  };
  rtcm: {
    messageTypes: number[];
    priorityMessageTypes: number[];
  };
  display?: {
    updateInterval: number;
  };
  maintenance: {
    enabled: boolean;
    intervalMs: number;
    listenDurationMs: number;
    rxWindowEnabled: boolean;
    enabledCommands: number[];
    deferredStopEnabled: boolean;
    activeWindowMs: number;
  };
}

export interface WiFiNetwork {
  ssid: string;
  bssid: string;
  rssi: number;
  encryption: "open" | "secured";
  channel: number;
}

export interface ApiResponse<T = any> {
  success?: boolean;
  message?: string;
  error?: string;
  networks?: WiFiNetwork[];
  mountpoints?: string[];
}
