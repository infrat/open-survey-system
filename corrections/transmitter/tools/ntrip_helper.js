#!/usr/bin/env node

const net = require("net");
const fs = require("fs");
const path = require("path");

class NTRIPHelper {
  constructor(config) {
    this.host = config.host;
    this.port = config.port || 2101;
    this.mountpoint = config.mountpoint;
    this.username = config.username || "";
    this.password = config.password || "";
    this.userAgent = config.userAgent || "NTRIP NodeJS Helper/1.0";
    this.logFile = config.logFile || "ntrip_log.txt";

    this.socket = null;
    this.connected = false;
    this.rtcmStats = {
      totalMessages: 0,
      messageTypes: {},
      startTime: null,
      lastMessageTime: null,
    };

    this.logStream = fs.createWriteStream(this.logFile, { flags: "a" });
  }

  // Generuje ramkę GGA (symulowane współrzędne)
  generateGGAMessage() {
    const now = new Date();
    const hours = now.getUTCHours().toString().padStart(2, "0");
    const minutes = now.getUTCMinutes().toString().padStart(2, "0");
    const seconds = now.getUTCSeconds().toString().padStart(2, "0");
    const time = hours + minutes + seconds;

    // Przykładowe współrzędne (Warszawa)
    const latitude = "5213.0000"; // 52°13' N
    const latDir = "N";
    const longitude = "02100.0000"; // 21°00' E
    const lonDir = "E";
    const quality = "1"; // GPS fix
    const satellites = "08"; // liczba satelitów
    const hdop = "1.2"; // pozioma dokładność
    const altitude = "100.0"; // wysokość
    const altUnit = "M"; // metry
    const geoidHeight = "0.0"; // wysokość geoidy
    const geoidUnit = "M"; // metry
    const dgpsAge = ""; // wiek danych DGPS
    const dgpsStation = ""; // ID stacji DGPS

    const gga = `$GPGGA,${time},${latitude},${latDir},${longitude},${lonDir},${quality},${satellites},${hdop},${altitude},${altUnit},${geoidHeight},${geoidUnit},${dgpsAge},${dgpsStation}`;

    // Oblicz checksum
    let checksum = 0;
    for (let i = 1; i < gga.length; i++) {
      checksum ^= gga.charCodeAt(i);
    }

    return (
      gga + "*" + checksum.toString(16).toUpperCase().padStart(2, "0") + "\r\n"
    );
  }

  // Parsuje wiadomość RTCM3
  parseRTCMMessage(buffer) {
    if (buffer.length < 3) return null;

    const preamble = buffer[0];
    if (preamble !== 0xd3) return null; // RTCM3 preamble

    const length = ((buffer[1] & 0x03) << 8) | buffer[2];
    if (buffer.length < length + 6) return null; // +3 header +3 CRC

    const messageType = (buffer[3] << 4) | ((buffer[4] & 0xf0) >> 4);

    return {
      type: messageType,
      length: length,
      totalLength: length + 6,
    };
  }

  // Loguje informacje o wiadomości
  logMessage(messageInfo) {
    const timestamp = new Date().toISOString();
    const logEntry = `${timestamp} - RTCM Type: ${messageInfo.type}, Length: ${messageInfo.length} bytes\n`;

    console.log(logEntry.trim());
    this.logStream.write(logEntry);

    // Aktualizuj statystyki
    this.rtcmStats.totalMessages++;
    this.rtcmStats.messageTypes[messageInfo.type] =
      (this.rtcmStats.messageTypes[messageInfo.type] || 0) + 1;
    this.rtcmStats.lastMessageTime = new Date();
  }

  // Wyświetla statystyki
  showStats() {
    const now = new Date();
    const duration = this.rtcmStats.startTime
      ? Math.round((now - this.rtcmStats.startTime) / 1000)
      : 0;

    console.log("\n=== RTCM Statistics ===");
    console.log(`Duration: ${duration} seconds`);
    console.log(`Total messages: ${this.rtcmStats.totalMessages}`);
    console.log(
      `Messages per second: ${
        this.rtcmStats.totalMessages / Math.max(duration, 1)
      }`
    );
    console.log("\nMessage types:");

    for (const [type, count] of Object.entries(this.rtcmStats.messageTypes)) {
      const percentage = ((count / this.rtcmStats.totalMessages) * 100).toFixed(
        1
      );
      console.log(`  Type ${type}: ${count} messages (${percentage}%)`);
    }
    console.log("=====================\n");
  }

  // Nawiązuje połączenie z NTRIP Casterem
  connect() {
    return new Promise((resolve, reject) => {
      this.socket = net.createConnection(this.port, this.host);

      this.socket.on("connect", () => {
        console.log(`Connected to NTRIP Caster: ${this.host}:${this.port}`);

        // Wysyła żądanie HTTP GET dla mountpoint
        const auth = Buffer.from(`${this.username}:${this.password}`).toString(
          "base64"
        );
        const request =
          `GET /${this.mountpoint} HTTP/1.1\r\n` +
          `Host: ${this.host}:${this.port}\r\n` +
          `User-Agent: ${this.userAgent}\r\n` +
          `Authorization: Basic ${auth}\r\n` +
          `Connection: close\r\n\r\n`;

        this.socket.write(request);
      });

      let headerParsed = false;
      let buffer = Buffer.alloc(0);

      this.socket.on("data", (data) => {
        if (!headerParsed) {
          const dataStr = data.toString();
          if (
            dataStr.includes("HTTP/1.1 200 OK") ||
            dataStr.includes("ICY 200 OK")
          ) {
            console.log("Successfully authenticated with NTRIP Caster");
            headerParsed = true;
            this.connected = true;
            this.rtcmStats.startTime = new Date();

            // Wysyła ramkę GGA
            const gga = this.generateGGAMessage();
            console.log(`Sending GGA: ${gga.trim()}`);
            this.socket.write(gga);

            resolve();

            // Znajduje początek danych RTCM (po nagłówkach HTTP)
            const headerEnd = dataStr.indexOf("\r\n\r\n");
            if (headerEnd !== -1) {
              const rtcmStart = headerEnd + 4;
              if (rtcmStart < data.length) {
                buffer = Buffer.concat([buffer, data.slice(rtcmStart)]);
              }
            }
          } else if (dataStr.includes("401") || dataStr.includes("403")) {
            reject(new Error("Authentication failed"));
          } else if (dataStr.includes("404")) {
            reject(new Error("Mountpoint not found"));
          }
        } else {
          // Przetwarza dane RTCM
          buffer = Buffer.concat([buffer, data]);
          this.processRTCMBuffer(buffer);
        }
      });

      this.socket.on("error", (err) => {
        console.error("Socket error:", err.message);
        reject(err);
      });

      this.socket.on("close", () => {
        console.log("Connection closed");
        this.connected = false;
        this.showStats();
      });
    });
  }

  // Przetwarza bufor z danymi RTCM
  processRTCMBuffer(buffer) {
    let offset = 0;

    while (offset < buffer.length - 3) {
      if (buffer[offset] === 0xd3) {
        const messageInfo = this.parseRTCMMessage(buffer.slice(offset));
        if (messageInfo && offset + messageInfo.totalLength <= buffer.length) {
          this.logMessage(messageInfo);
          offset += messageInfo.totalLength;
        } else {
          break; // Niepełna wiadomość, czekamy na więcej danych
        }
      } else {
        offset++;
      }
    }

    // Zachowuj nieprzetworzone dane w buforze
    // Buffer jest już odpowiednio zarządzany przez wywołującą funkcję
  }

  // Wysyła ramkę GGA co określony interwał
  startGGATimer(intervalSeconds = 30) {
    this.ggaTimer = setInterval(() => {
      if (this.connected && this.socket) {
        const gga = this.generateGGAMessage();
        console.log(`Sending periodic GGA: ${gga.trim()}`);
        this.socket.write(gga);
      }
    }, intervalSeconds * 1000);
  }

  // Zatrzymuje timer GGA
  stopGGATimer() {
    if (this.ggaTimer) {
      clearInterval(this.ggaTimer);
      this.ggaTimer = null;
    }
  }

  // Zamyka połączenie
  disconnect() {
    this.stopGGATimer();
    if (this.socket) {
      this.socket.end();
    }
    if (this.logStream) {
      this.logStream.end();
    }
  }
}

// Funkcja główna
async function main() {
  // Sprawdź czy użytkownik prosi o pomoc
  if (process.argv.includes("--help") || process.argv.includes("-h")) {
    console.log("NTRIP Helper - Node.js RTCM Logger");
    console.log("");
    console.log(
      "Usage: node ntrip_helper.js [host] [port] [mountpoint] [username] [password]"
    );
    console.log("");
    console.log("Parameters:");
    console.log("  host        - NTRIP Caster hostname (default: rtk2go.com)");
    console.log("  port        - NTRIP Caster port (default: 2101)");
    console.log("  mountpoint  - NTRIP mountpoint name (default: WROC)");
    console.log("  username    - Username for authentication (optional)");
    console.log("  password    - Password for authentication (optional)");
    console.log("");
    console.log("Examples:");
    console.log("  node ntrip_helper.js rtk2go.com 2101 WROC");
    console.log(
      "  node ntrip_helper.js your-caster.com 2101 STATION user pass"
    );
    console.log("");
    process.exit(0);
  }

  const config = {
    host: process.argv[2] || "rtk2go.com",
    port: parseInt(process.argv[3]) || 2101,
    mountpoint: process.argv[4] || "WROC",
    username: process.argv[5] || "",
    password: process.argv[6] || "",
    logFile:
      "ntrip_log_" +
      new Date().toISOString().slice(0, 19).replace(/:/g, "-") +
      ".txt",
  };

  console.log("NTRIP Helper - Node.js RTCM Logger");
  console.log(
    "Usage: node ntrip_helper.js [host] [port] [mountpoint] [username] [password]"
  );
  console.log(
    `Connecting to: ${config.host}:${config.port}/${config.mountpoint}`
  );
  console.log(`Log file: ${config.logFile}\n`);

  const helper = new NTRIPHelper(config);

  // Obsługa Ctrl+C
  process.on("SIGINT", () => {
    console.log("\nShutting down...");
    helper.disconnect();
    process.exit(0);
  });

  try {
    await helper.connect();
    helper.startGGATimer(30); // Wysyłaj GGA co 30 sekund

    // Wyświetlaj statystyki co minutę
    setInterval(() => {
      helper.showStats();
    }, 60000);
  } catch (error) {
    console.error("Failed to connect:", error.message);
    process.exit(1);
  }
}

// Uruchom jeśli skrypt jest wywołany bezpośrednio
if (require.main === module) {
  main();
}

module.exports = NTRIPHelper;
