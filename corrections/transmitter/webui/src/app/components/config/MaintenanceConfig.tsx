import { Switch } from '../ui/switch';
import { Label } from '../ui/label';
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '../ui/select';
import { Checkbox } from '../ui/checkbox';
import type { Config } from '../../types/config';

interface MaintenanceConfigProps {
  config: Config['maintenance'];
  onChange: (maintenance: Config['maintenance']) => void;
}

const COMMANDS: { id: number; label: string; description: string }[] = [
  { id: 0x01, label: 'RESET',      description: 'Restart the device remotely' },
  { id: 0x02, label: 'SLEEP',      description: 'Enter deep sleep for N seconds' },
  { id: 0x03, label: 'HTTP GET',   description: 'Perform an HTTP GET request' },
  { id: 0x04, label: 'SET PARAM',  description: 'Set a named configuration parameter' },
  { id: 0x05, label: 'PING',       description: 'Confirm device is alive (no action)' },
  { id: 0x06, label: 'LOG LEVEL',  description: 'Change serial log verbosity' },
  { id: 0x07, label: 'START',      description: 'Resume RTCM transmission (exit standby)' },
  { id: 0x08, label: 'STOP',       description: 'Halt RTCM TX and enter standby immediately' },
];

const INTERVAL_OPTIONS = [
  { value: 60000,  label: '1 minute' },
  { value: 120000, label: '2 minutes' },
  { value: 180000, label: '3 minutes' },
  { value: 300000, label: '5 minutes' },
  { value: 600000, label: '10 minutes' },
];

const LISTEN_OPTIONS = [
  { value: 1000,  label: '1 second' },
  { value: 2000,  label: '2 seconds' },
  { value: 3000,  label: '3 seconds' },
  { value: 5000,  label: '5 seconds' },
  { value: 10000, label: '10 seconds' },
];

const ACTIVE_WINDOW_OPTIONS = [
  { value: 60000,   label: '1 minute' },
  { value: 120000,  label: '2 minutes' },
  { value: 300000,  label: '5 minutes' },
  { value: 600000,  label: '10 minutes' },
  { value: 900000,  label: '15 minutes' },
  { value: 1800000, label: '30 minutes' },
];

// Whether any RX path is active — both options require maintenance frame to be enabled
function anyRxActive(config: Config['maintenance']): boolean {
  return config.enabled && (config.rxWindowEnabled || config.deferredStopEnabled);
}

export function MaintenanceConfig({ config, onChange }: MaintenanceConfigProps) {
  const toggleCommand = (id: number, checked: boolean) => {
    const updated = checked
      ? [...config.enabledCommands, id]
      : config.enabledCommands.filter((c) => c !== id);
    onChange({ ...config, enabledCommands: updated });
  };

  return (
    <div className="space-y-8">

      {/* ── Section 1: Maintenance Frame (telemetry) ── */}
      <div className="space-y-4">
        <div className="flex items-center justify-between">
          <div className="space-y-0.5">
            <Label htmlFor="maint-enabled" className="text-base font-semibold">
              Maintenance Frame
            </Label>
            <p className="text-xs text-muted-foreground">
              Periodically broadcasts a telemetry frame (uptime, NTRIP status, free heap, battery).
            </p>
          </div>
          <Switch
            id="maint-enabled"
            checked={config.enabled}
            onCheckedChange={(v) => onChange({ ...config, enabled: v, deferredStopEnabled: v ? config.deferredStopEnabled : false })}
          />
        </div>

        {config.enabled && (
          <div className="pl-4 border-l-2 border-muted space-y-4">
            {/* Interval */}
            <div className="space-y-2">
              <Label htmlFor="maint-interval">Transmission interval</Label>
              <Select
                value={config.intervalMs.toString()}
                onValueChange={(v) => onChange({ ...config, intervalMs: parseInt(v) })}
              >
                <SelectTrigger id="maint-interval">
                  <SelectValue />
                </SelectTrigger>
                <SelectContent>
                  {INTERVAL_OPTIONS.map((o) => (
                    <SelectItem key={o.value} value={o.value.toString()}>{o.label}</SelectItem>
                  ))}
                </SelectContent>
              </Select>
            </div>

            {/* RX window after maintenance frame */}
            <div className="space-y-3">
              <div className="flex items-center justify-between">
                <div className="space-y-0.5">
                  <Label htmlFor="maint-rx-window">RX window after frame</Label>
                  <p className="text-xs text-muted-foreground">
                    After each telemetry broadcast, briefly switch to receive mode to accept
                    a remote command. The receiver could send the command within this window.
                  </p>
                </div>
                <Switch
                  id="maint-rx-window"
                  checked={config.rxWindowEnabled}
                  onCheckedChange={(v) => onChange({ ...config, rxWindowEnabled: v })}
                />
              </div>

              {config.rxWindowEnabled && (
                <div className="pl-4 border-l-2 border-muted space-y-2">
                  <Label htmlFor="maint-listen">RX window duration</Label>
                  <Select
                    value={config.listenDurationMs.toString()}
                    onValueChange={(v) => onChange({ ...config, listenDurationMs: parseInt(v) })}
                  >
                    <SelectTrigger id="maint-listen">
                      <SelectValue />
                    </SelectTrigger>
                    <SelectContent>
                      {LISTEN_OPTIONS.map((o) => (
                        <SelectItem key={o.value} value={o.value.toString()}>{o.label}</SelectItem>
                      ))}
                    </SelectContent>
                  </Select>
                </div>
              )}
            </div>

            {/* Deferred Stop — requires Maintenance Frame */}
            <div className="space-y-3">
              <div className="flex items-center justify-between">
                <div className="space-y-0.5">
                  <Label htmlFor="maint-deferred">Deferred Stop</Label>
                  <p className="text-xs text-muted-foreground">
                    Transmit RTCM for a fixed window after power-on, then enter standby automatically.
                    Useful for timed deployments or connectivity testing at installation.
                  </p>
                </div>
                <Switch
                  id="maint-deferred"
                  checked={config.deferredStopEnabled}
                  onCheckedChange={(v) => onChange({ ...config, deferredStopEnabled: v })}
                />
              </div>

              {config.deferredStopEnabled && (
                <div className="pl-4 border-l-2 border-muted space-y-2">
                  <Label htmlFor="maint-active-window">Active TX window (from power-on)</Label>
                  <Select
                    value={config.activeWindowMs.toString()}
                    onValueChange={(v) => onChange({ ...config, activeWindowMs: parseInt(v) })}
                  >
                    <SelectTrigger id="maint-active-window">
                      <SelectValue />
                    </SelectTrigger>
                    <SelectContent>
                      {ACTIVE_WINDOW_OPTIONS.map((o) => (
                        <SelectItem key={o.value} value={o.value.toString()}>{o.label}</SelectItem>
                      ))}
                    </SelectContent>
                  </Select>
                  <p className="text-xs text-muted-foreground">
                    The device transmits normally for this duration after boot, then enters standby.
                    Send CMD_START to resume — the timer will not restart (one-shot per power cycle).
                  </p>
                  {!config.rxWindowEnabled && (
                    <p className="text-xs text-amber-600 dark:text-amber-400 border border-amber-300 dark:border-amber-700 rounded px-2 py-1.5 bg-amber-50 dark:bg-amber-950/30">
                      ⚠ Without an RX window, remote commands (including CMD_STOP) cannot reach the
                      device while it is actively transmitting. Enable <strong>RX window</strong> above
                      to allow remote control during the active TX phase.
                    </p>
                  )}
                </div>
              )}
            </div>
          </div>
        )}
      </div>

      {/* ── Section 3: Remote Commands ── */}
      <div className="space-y-4">
        <div className="space-y-0.5">
          <Label className="text-base font-semibold">Remote Commands</Label>
          <p className="text-xs text-muted-foreground">
            Commands are received during the RX window (after a maintenance frame) or while
            in standby (continuous RX). Only checked commands will be executed.
            {!anyRxActive(config) && (
              <span className="block mt-1 text-yellow-600 dark:text-yellow-400">
                ⚠ No RX path is currently active. Enable Maintenance Frame, then its RX window
                or Deferred Stop to receive commands.
              </span>
            )}
          </p>
        </div>
        <div className="space-y-2">
          {COMMANDS.map((cmd) => (
            <div key={cmd.id} className="flex items-start gap-3">
              <Checkbox
                id={`cmd-${cmd.id}`}
                checked={config.enabledCommands.includes(cmd.id)}
                onCheckedChange={(v) => toggleCommand(cmd.id, v === true)}
              />
              <div className="grid gap-0.5 leading-none">
                <label
                  htmlFor={`cmd-${cmd.id}`}
                  className="text-sm font-medium leading-none cursor-pointer"
                >
                  {cmd.label}
                </label>
                <p className="text-xs text-muted-foreground">{cmd.description}</p>
              </div>
            </div>
          ))}
        </div>
      </div>

    </div>
  );
}
