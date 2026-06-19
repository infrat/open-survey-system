export interface HelpContent {
  title: string;
  content: string;
}

export const HELP_CONTENT: Record<string, HelpContent> = {
  wifi: {
    title: 'WiFi Configuration',
    content: `The transmitter needs WiFi connectivity to reach the NTRIP caster and download RTCM correction data.

**WiFi Network (SSID)** is the name of your wireless network. Use the scan button to discover nearby networks and select one automatically.

**Password** is your WiFi network password. Leave this field empty only if connecting to an open (unsecured) network.

**Connection Timeout** defines how long the device waits for a successful WiFi connection before giving up. If your network takes longer to authenticate, increase this value. Range: 5-60 seconds.

**Tip:** The device creates its own Access Point (AP) named OSS-LoRa-TX-XXXX when not connected to WiFi. Connect to this AP to access the configuration interface at http://192.168.4.1/`,
  },

  ntrip: {
    title: 'NTRIP Configuration',
    content: `NTRIP (Networked Transport of RTCM via Internet Protocol) is the protocol used to stream RTCM corrections from a remote caster server to your base station transmitter.

**Host**

The IP address or domain name of your NTRIP caster (e.g., rtk2go.com, your national RTK network provider).

**Port**

The TCP port number for the NTRIP service, typically 2101.

**Username and Password**

Credentials provided by your NTRIP service. Some public casters allow anonymous access (leave these empty).

**Mountpoint**

Identifies the specific correction stream on the caster. Different mountpoints may provide different message types, reference stations, or GNSS constellations. Use the fetch button to retrieve available mountpoints from the caster.

**Connection Timeout**

Controls how long to wait for the caster to respond before retrying. Range: 5-60 seconds.

**Tip:** For best RTK performance, choose a mountpoint geographically close to your operating area.`,
  },

  gga: {
    title: 'GGA Position Configuration',
    content: `Many NTRIP casters require the rover to send its approximate position in NMEA GGA format. Some services use this to select the nearest reference station or compute VRS (Virtual Reference Station) corrections.

**Latitude and Longitude**

Define the approximate center of your working area in decimal degrees. Positive latitude = North, negative = South. For best results, use coordinates within a few kilometers of your actual location.

**Altitude**

The height above mean sea level in meters. This value doesn't need to be precise — an error of ±50m is acceptable for most NTRIP services.

**Send Interval**

Controls how often GGA sentences are transmitted to the caster. Most casters require updates every 1-10 seconds. Sending too frequently wastes bandwidth; too slowly may cause the caster to disconnect. Range: 1-60 seconds.

**Tip:** If you're unsure of your coordinates, use a smartphone GPS app or online map tool to get approximate values. For stationary base stations, precise coordinates aren't critical — the caster typically uses them only for proximity checks.`,
  },

  lora: {
    title: 'LoRa Radio Configuration',
    content: `LoRa (Long Range) is a low-power radio modulation designed for long-distance, low-data-rate communication. All radio parameters must match between your transmitter (base) and receiver (rover).

**Frequency** is the center carrier frequency in MHz. Common ISM bands: 433 MHz (Europe), 868 MHz (Europe), 915 MHz (Americas). Check local regulations before transmitting. Range: 137-1020 MHz.

**Spreading Factor (SF)** controls the trade-off between range and data rate. Higher SF = longer range but slower transmission. For RTK, SF7-SF9 is typical. Range: SF6 (fastest) to SF12 (longest range).

**Bandwidth** defines the radio channel width. Narrower bandwidth increases range and sensitivity but reduces data rate. For RTK corrections, 125 kHz or 250 kHz is common.

**Coding Rate** adds forward error correction. Higher rates (4/8) improve reliability in noisy environments but reduce effective data rate. 4/5 is a good default.

**TX Power** is the transmit power in dBm. Higher power increases range but drains battery faster. Legal limits vary by region (typically 10-20 dBm for ISM bands). Range: 2-20 dBm.

**Sync Word** is a network identifier. Only radios with matching sync words can communicate. Use the default (0x12) unless sharing spectrum with other LoRa users. Format: 0x00-0xFF.

**Tip:** Start with SF7, 125 kHz bandwidth, and coding rate 4/5. Increase SF if you need more range but can tolerate slower updates.`,
  },

  rtcm: {
    title: 'RTCM Message Filtering',
    content: `The transmitter receives a continuous stream of RTCM correction messages from the NTRIP caster. Not all message types need to be forwarded over the LoRa radio link — filtering lets you select only the types your rover actually needs, saving valuable radio bandwidth.

**Allowed Message Types** define which RTCM messages are accepted for transmission. Any message type not on this list is silently discarded. At minimum, you need MSM observation messages (e.g. 1075, 1085, 1095, 1125) and a station reference position (1005) for the rover to compute an RTK solution.

**High Priority Types** control the transmission order when multiple messages are buffered. The transmitter keeps only the latest message of each type. During each transmission cycle:

• All high-priority messages are transmitted first (e.g. MSM observations, station position). These are time-sensitive and critical for RTK accuracy.

• Low-priority messages (e.g. ephemeris, antenna descriptors) are transmitted afterwards — but only if no new high-priority data has arrived in the meantime. If fresh high-priority data is waiting, low-priority messages are postponed to avoid delaying corrections.

• To prevent low-priority messages from being starved indefinitely, they are forced through at least once every 5 seconds, regardless of incoming high-priority traffic.

**Tip:** Station position (type 1005) is always treated as high priority, even if not explicitly listed.`,
  },

  maintenance: {
    title: 'Maintenance & Remote Control',
    content: `This tab configures three independent features that extend the LoRa link with telemetry and remote control capabilities.

---

**Maintenance Frame (Telemetry)**

When enabled, the device periodically broadcasts a telemetry frame over LoRa containing: uptime, RTCM frame counters, NTRIP and WiFi connection status, free heap, and optionally battery voltage.

Use this for passive monitoring — the frame is sent regardless of whether anyone is listening.

*Transmission interval* — how often the frame is sent. Longer intervals reduce radio overhead; shorter intervals give more frequent updates.

*RX window after frame* — when enabled, the radio briefly switches to receive mode immediately after each telemetry broadcast. This is the only time the device can accept a remote command during normal operation. The RTCM stream is paused for the window duration (2 s is safe for most rovers).

*Battery monitoring* — reads battery voltage via the configured ADC pin and includes it in the telemetry payload. On TTGO T-Beam v1.1, pin 35 is connected via a 2:1 voltage divider (scale factor 2.0).

---

**Deferred Stop**

When enabled, the device transmits RTCM corrections for a fixed window measured from power-on, then automatically enters standby. This is intended for two scenarios:

• **Connectivity test at installation** — power on the unit, verify corrections are reaching the rover for the configured time, then let the device go silent automatically. No manual intervention needed.
• **Timed deployments** — deploy for a known mission duration, then conserve power.

While in standby, the LoRa radio stays in continuous receive mode. Send **CMD_START** to resume transmission — the deferred stop timer is permanently cancelled for that session (it will not fire again until the next power cycle). **CMD_STOP** enters standby immediately at any time.

> **Important — half-duplex constraint:** LoRa is half-duplex. While the device is actively transmitting RTCM data, the radio is in TX mode and physically cannot receive. This means that without a Maintenance Frame RX window, there is no gap in which an operator can send CMD_STOP during the active TX phase. **If you want remote control during active transmission, enable Maintenance Frame and its RX window.** CMD_START always works once the device has entered standby (continuous RX).

*Active TX window* — how long (from boot) the device transmits before entering standby.

---

**Remote Commands**

Commands can be received during the RX window (after a maintenance frame) or while in standby (continuous RX). Only checked commands are executed — unchecked ones are silently ignored.

• **RESET / PING** — safe for any deployment.
• **START / STOP** — required when using Deferred Stop.
• **SLEEP / HTTP GET / SET PARAM** — enable only on trusted, access-controlled LoRa networks.`,
  },
};
