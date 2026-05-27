import { RTCMMessagePicker } from './RTCMMessagePicker';
import type { Config } from '../../types/config';

interface RTCMConfigProps {
  config: Config['rtcm'];
  onChange: (rtcm: Config['rtcm']) => void;
}

export function RTCMConfig({ config, onChange }: RTCMConfigProps) {
  return (
    <div className="space-y-4">
      <RTCMMessagePicker
        allowedTypes={config.messageTypes}
        priorityTypes={config.priorityMessageTypes}
        onChange={(messageTypes, priorityMessageTypes) => {
          onChange({ ...config, messageTypes, priorityMessageTypes });
        }}
      />
    </div>
  );
}
