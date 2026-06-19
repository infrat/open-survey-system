import { useState, useEffect, useRef, useCallback } from 'react';
import type { Config, WiFiNetwork, ApiResponse } from '../types/config';

const API_BASE = '/api';

const RESTART_TIMEOUT_MS = 3000;
const DEVICE_POLL_INTERVAL_MS = 2000;
const DEVICE_POLL_MAX_ATTEMPTS = 30;

const defaultConfig: Config = {
  wifi: {
    ssid: '',
    password: '',
    timeout: 30000,
  },
  ntrip: {
    host: '91.198.76.2',
    port: 2101,
    mountpoint: 'RTN4G_VRS_RTCM32',
    user: '',
    password: '',
    timeout: 10000,
  },
  gga: {
    latitude: 50.2346,
    longitude: 19.2084,
    altitude: 266.0,
    sendInterval: 2000,
  },
  lora: {
    frequency: 433000000,
    spreadingFactor: 7,
    bandwidth: 125000,
    codingRate: 5,
    txPower: 20,
    syncWord: 18,
  },
  rtcm: {
    messageTypes: [1005, 1007, 1019, 1020, 1033, 1075, 1085, 1095, 1125, 1230],
    priorityMessageTypes: [1005, 1075, 1085, 1095, 1125, 1230],
  },
  maintenance: {
    enabled: false,
    intervalMs: 120000,
    listenDurationMs: 2000,
    rxWindowEnabled: false,
    enabledCommands: [0x01, 0x05], // RESET + PING
    deferredStopEnabled: false,
    activeWindowMs: 300000, // 5 minutes
  },
};

export function useConfig() {
  const [config, setConfig] = useState<Config>(defaultConfig);
  const [loading, setLoading] = useState(false);
  const [restarting, setRestarting] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const loadAbortRef = useRef<AbortController | null>(null);

  /** Poll the device until it responds, then reload config */
  const waitForDevice = useCallback(async (): Promise<void> => {
    for (let i = 0; i < DEVICE_POLL_MAX_ATTEMPTS; i++) {
      await new Promise((r) => setTimeout(r, DEVICE_POLL_INTERVAL_MS));
      try {
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), RESTART_TIMEOUT_MS);
        const response = await fetch(`${API_BASE}/config`, {
          signal: controller.signal,
        });
        clearTimeout(timeoutId);
        if (response.ok) {
          const data: Config = await response.json();
          setConfig(data);
          return;
        }
      } catch {
        // Device still down, continue polling
      }
    }
    throw new Error('Device did not come back online');
  }, []);

  const loadConfig = useCallback(async () => {
    // Abort previous loadConfig call if still in flight
    if (loadAbortRef.current) {
      loadAbortRef.current.abort();
    }
    const controller = new AbortController();
    loadAbortRef.current = controller;

    setLoading(true);
    setError(null);
    try {
      const response = await fetch(`${API_BASE}/config`, {
        signal: controller.signal,
      });
      if (!response.ok) throw new Error('Failed to load configuration');
      const data: Config = await response.json();
      setConfig(data);
    } catch (err) {
      if (err instanceof DOMException && err.name === 'AbortError') return;
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      if (!controller.signal.aborted) {
        setLoading(false);
      }
    }
  }, []);

  const saveConfig = async (updatedConfig: Config): Promise<boolean> => {
    setLoading(true);
    setError(null);
    try {
      // Try to save - response may or may not arrive before device restarts
      const controller = new AbortController();
      const timeoutId = setTimeout(() => controller.abort(), RESTART_TIMEOUT_MS * 2);
      try {
        const response = await fetch(`${API_BASE}/config`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(updatedConfig),
          signal: controller.signal,
        });
        clearTimeout(timeoutId);
        const data: ApiResponse = await response.json();
        if (!response.ok || !data.success) {
          throw new Error(data.message || 'Failed to save configuration');
        }
        setConfig(updatedConfig);
      } catch (err) {
        clearTimeout(timeoutId);
        // AbortError or network error means device restarted before responding
        const isAbort =
          err instanceof DOMException && err.name === 'AbortError';
        const isNetworkError = err instanceof TypeError;
        if (!isAbort && !isNetworkError) {
          throw err; // Real API error - propagate
        }
      }

      // Device restarts after save - wait for it to come back
      setLoading(false);
      setRestarting(true);
      await waitForDevice();
      return true;
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
      return false;
    } finally {
      setLoading(false);
      setRestarting(false);
    }
  };

  const restartDevice = async (): Promise<boolean> => {
    setLoading(true);
    setError(null);
    try {
      // Fire the restart request - response may not arrive before device restarts
      const controller = new AbortController();
      const timeoutId = setTimeout(() => controller.abort(), RESTART_TIMEOUT_MS);
      try {
        await fetch(`${API_BASE}/restart`, {
          method: 'POST',
          signal: controller.signal,
        });
        clearTimeout(timeoutId);
      } catch {
        clearTimeout(timeoutId);
        // Timeout/network error is expected - device is restarting
      }

      // Wait for device to come back
      setLoading(false);
      setRestarting(true);
      await waitForDevice();
      return true;
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
      return false;
    } finally {
      setLoading(false);
      setRestarting(false);
    }
  };

  const scanWiFi = async (): Promise<WiFiNetwork[]> => {
    setLoading(true);
    setError(null);
    try {
      const response = await fetch(`${API_BASE}/wifi/scan`, {
        method: 'POST',
      });
      const data: ApiResponse = await response.json();
      if (!response.ok || !data.success) {
        throw new Error(data.message || 'WiFi scan failed');
      }
      return data.networks || [];
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
      return [];
    } finally {
      setLoading(false);
    }
  };

  const fetchMountpoints = async (
    host: string,
    port: number,
    user?: string,
    password?: string
  ): Promise<string[]> => {
    setLoading(true);
    setError(null);
    try {
      const params = new URLSearchParams({
        host,
        port: port.toString(),
        ...(user && { user }),
        ...(password && { password }),
      });
      const response = await fetch(`${API_BASE}/ntrip/mountpoints`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: params,
      });
      const data: ApiResponse = await response.json();
      if (data.error) {
        throw new Error(data.error);
      }
      return data.mountpoints || [];
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
      return [];
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadConfig();
  }, [loadConfig]);

  return {
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
  };
}
