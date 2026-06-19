import { useState, useEffect } from 'react';
import { Input } from '../ui/input';
import { Label } from '../ui/label';
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '../ui/select';
import type { Config } from '../../types/config';

interface LoRaConfigProps {
  config: Config['lora'];
  onChange: (lora: Config['lora']) => void;
}

const bandwidthOptions = [
  { value: 7800, label: '7.8 kHz' },
  { value: 10400, label: '10.4 kHz' },
  { value: 15600, label: '15.6 kHz' },
  { value: 20800, label: '20.8 kHz' },
  { value: 31250, label: '31.25 kHz' },
  { value: 41700, label: '41.7 kHz' },
  { value: 62500, label: '62.5 kHz' },
  { value: 125000, label: '125 kHz' },
  { value: 250000, label: '250 kHz' },
  { value: 500000, label: '500 kHz' },
];

export function LoRaConfig({ config, onChange }: LoRaConfigProps) {
  const [freqInput, setFreqInput] = useState(() => {
    const mhz = config.frequency / 1e6;
    return mhz.toFixed(6).replace(/\.?0+$/, '');
  });
  const syncWordHex = `0x${config.syncWord.toString(16).toUpperCase().padStart(2, '0')}`;

  useEffect(() => {
    const mhz = config.frequency / 1e6;
    const normalized = mhz.toFixed(6).replace(/\.?0+$/, '');
    const inputNormalized = Number.parseFloat(freqInput.replace(',', '.'));
    if (!Number.isNaN(inputNormalized) && Math.round(inputNormalized * 1e6) === config.frequency) return;
    setFreqInput(normalized);
  }, [config.frequency]);

  const handleFreqChange = (value: string) => {
    setFreqInput(value);
    const parsed = Number.parseFloat(value.replace(',', '.'));
    if (!Number.isNaN(parsed) && parsed >= 137 && parsed <= 1020) {
      onChange({ ...config, frequency: Math.round(parsed * 1e6) });
    }
  };

  return (
    <div className="space-y-4">
      <div className="space-y-2">
        <Label htmlFor="lora-frequency">Frequency (MHz) *</Label>
        <Input
          id="lora-frequency"
          type="text"
          inputMode="decimal"
          value={freqInput}
          onChange={(e) => handleFreqChange(e.target.value)}
          required
        />
        <p className="text-xs text-muted-foreground">Range: 137-1020 MHz</p>
      </div>

      <div className="space-y-2">
        <Label htmlFor="lora-sf">Spreading Factor *</Label>
        <Select
          value={config.spreadingFactor.toString()}
          onValueChange={(v) => onChange({ ...config, spreadingFactor: parseInt(v) })}
        >
          <SelectTrigger id="lora-sf">
            <SelectValue />
          </SelectTrigger>
          <SelectContent>
            {[6, 7, 8, 9, 10, 11, 12].map((sf) => (
              <SelectItem key={sf} value={sf.toString()}>
                SF{sf}
              </SelectItem>
            ))}
          </SelectContent>
        </Select>
      </div>

      <div className="space-y-2">
        <Label htmlFor="lora-bw">Bandwidth *</Label>
        <Select
          value={config.bandwidth.toString()}
          onValueChange={(v) => onChange({ ...config, bandwidth: parseInt(v) })}
        >
          <SelectTrigger id="lora-bw">
            <SelectValue />
          </SelectTrigger>
          <SelectContent>
            {bandwidthOptions.map((bw) => (
              <SelectItem key={bw.value} value={bw.value.toString()}>
                {bw.label}
              </SelectItem>
            ))}
          </SelectContent>
        </Select>
      </div>

      <div className="space-y-2">
        <Label htmlFor="lora-cr">Coding Rate *</Label>
        <Select
          value={config.codingRate.toString()}
          onValueChange={(v) => onChange({ ...config, codingRate: parseInt(v) })}
        >
          <SelectTrigger id="lora-cr">
            <SelectValue />
          </SelectTrigger>
          <SelectContent>
            <SelectItem value="5">4/5</SelectItem>
            <SelectItem value="6">4/6</SelectItem>
            <SelectItem value="7">4/7</SelectItem>
            <SelectItem value="8">4/8</SelectItem>
          </SelectContent>
        </Select>
      </div>

      <div className="space-y-2">
        <Label htmlFor="lora-power">TX Power (dBm) *</Label>
        <Input
          id="lora-power"
          type="number"
          value={config.txPower}
          onChange={(e) => onChange({ ...config, txPower: parseInt(e.target.value) || 0 })}
          min={2}
          max={20}
          required
        />
      </div>

      <div className="space-y-2">
        <Label htmlFor="lora-sync">Sync Word (hex) *</Label>
        <Input
          id="lora-sync"
          value={syncWordHex}
          onChange={(e) => {
            const hex = e.target.value.replace(/^0x/i, '');
            const parsed = parseInt(hex, 16);
            if (!isNaN(parsed) && parsed >= 0 && parsed <= 255) {
              onChange({ ...config, syncWord: parsed });
            }
          }}
          pattern="^0x[0-9A-Fa-f]{1,2}$"
          placeholder="0x12"
          required
        />
      </div>
    </div>
  );
}
