#include "ntrip_client.h"
#include <WiFi.h>
#include <ArduinoJson.h>

NTRIPClient::NTRIPClient(ConfigManager &configMgr)
    : configManager(configMgr), lastDataTime(0), lastGGATime(0), lastConnectionAttempt(0), connected(false)
{
}

bool NTRIPClient::connect()
{
    if (connected && client.connected())
    {
        return true;
    }

    // For initial connection, always attempt. For subsequent reconnections, check retry interval.
    if (lastConnectionAttempt > 0 && millis() - lastConnectionAttempt < CONNECTION_RETRY_INTERVAL)
    {
        return false;
    }

    lastConnectionAttempt = millis();

    SystemConfig &cfg = configManager.getConfig();

#ifdef DEBUG
    Serial.println("Connecting to NTRIP caster...");
    Serial.print("Host: ");
    Serial.print(cfg.ntripHost);
    Serial.print(":");
    Serial.println(cfg.ntripPort);
    Serial.print("Mountpoint: ");
    Serial.println(cfg.ntripMountpoint);
    Serial.print("Username: ");
    Serial.println(cfg.ntripUser);

    // First test basic connectivity
    Serial.println("Testing basic connectivity to host...");
#endif
    if (!testConnectivity())
    {
#ifdef DEBUG
        Serial.println("Basic connectivity test failed!");
#endif
        return false;
    }

#ifdef DEBUG
    Serial.println("Attempting NTRIP connection...");
#endif
    if (!client.connect(cfg.ntripHost.c_str(), cfg.ntripPort))
    {
#ifdef DEBUG
        Serial.println("NTRIP TCP connection failed!");
        Serial.println("Possible causes:");
        Serial.println("- Host unreachable");
        Serial.println("- Port blocked by firewall");
        Serial.println("- DNS resolution failed!");
        Serial.println("- Server not responding");
#endif
        connected = false;
        return false;
    }

#ifdef DEBUG
    Serial.println("TCP connection successful!");
#endif

    // Send NTRIP request
    String auth = String(cfg.ntripUser) + ":" + String(cfg.ntripPassword);
    String authEncoded = encodeBase64(auth);

    String request = "GET /" + String(cfg.ntripMountpoint) + " HTTP/1.0\r\n";
    request += "User-Agent: NTRIP ESP32Client/1.0\r\n";
    request += "Authorization: Basic " + authEncoded + "\r\n";
    request += "Connection: close\r\n";
    request += "\r\n";

    Serial.println("Sending NTRIP request:");
    Serial.print(request);
    client.print(request);

    // Wait for response
#ifdef DEBUG
    Serial.println("Waiting for server response...");
#endif
    unsigned long startTime = millis();
    while (!client.available() && millis() - startTime < cfg.ntripTimeout)
    {
        delay(100);
#ifdef DEBUG
        if ((millis() - startTime) % 2000 == 0) // Print progress every 2 seconds
        {
            Serial.print(".");
        }
#endif
    }
#ifdef DEBUG
    Serial.println();
#endif

    if (!client.available())
    {
#ifdef DEBUG
        Serial.print("NTRIP: No response from server after ");
        Serial.print(cfg.ntripTimeout);
        Serial.println("ms timeout");
#endif
        client.stop();
        connected = false;
        return false;
    }

    // Read response
    String response = client.readStringUntil('\n');
    Serial.print("NTRIP Response: ");
    Serial.println(response);

    if (response.indexOf("200") != -1)
    {
        // Skip HTTP headers and print them for debugging
        Serial.println("HTTP Headers:");
        String header;
        while (client.available() && (header = client.readStringUntil('\n')) != "\r")
        {
            Serial.print("  ");
            Serial.println(header);
        }
        connected = true;
        lastDataTime = millis();
        lastGGATime = 0; // Send GGA immediately
        return true;
    }
    else
    {
        Serial.println("NTRIP authentication/request failed!");
        Serial.println("Full response:");

        // Read and print the full response for debugging
        while (client.available())
        {
            String line = client.readStringUntil('\n');
            Serial.println(line);
        }

        client.stop();
        connected = false;
        return false;
    }
}

bool NTRIPClient::isConnected()
{
    if (!connected || !client.connected())
    {
        connected = false;
        return false;
    }

    // Check for data timeout
    if (millis() - lastDataTime > DATA_TIMEOUT)
    {
        Serial.println("NTRIP data timeout!");
        client.stop();
        connected = false;
        return false;
    }

    return true;
}

void NTRIPClient::disconnect()
{
    client.stop();
    connected = false;
}

size_t NTRIPClient::available()
{
    if (!isConnected())
    {
        return 0;
    }
    return client.available();
}

size_t NTRIPClient::read(uint8_t *buffer, size_t length)
{
    if (!isConnected())
    {
        return 0;
    }

    size_t bytesRead = client.read(buffer, length);
    if (bytesRead > 0)
    {
        lastDataTime = millis();
    }

    return bytesRead;
}

void NTRIPClient::keepAlive()
{
    if (!isConnected())
    {
        return;
    }

    SystemConfig &cfg = configManager.getConfig();

    // Send GGA periodically
    if (millis() - lastGGATime >= cfg.ggaSendInterval)
    {
        sendGGA();
        lastGGATime = millis();
    }
}

String NTRIPClient::generateGGAString()
{
    SystemConfig &cfg = configManager.getConfig();

    // Convert latitude and longitude to NMEA format
    double lat = abs(cfg.ggaLatitude);
    double lon = abs(cfg.ggaLongitude);

    int latDeg = (int)lat;
    double latMin = (lat - latDeg) * 60.0;

    int lonDeg = (int)lon;
    double lonMin = (lon - lonDeg) * 60.0;

    char latStr[12], lonStr[12];
    sprintf(latStr, "%02d%08.5f", latDeg, latMin);
    sprintf(lonStr, "%03d%08.5f", lonDeg, lonMin);

    char latHem = (cfg.ggaLatitude >= 0) ? 'N' : 'S';
    char lonHem = (cfg.ggaLongitude >= 0) ? 'E' : 'W';

    // Current time (use millis as approximation)
    unsigned long now = millis() / 1000;
    int hours = (now / 3600) % 24;
    int minutes = (now / 60) % 60;
    int seconds = now % 60;

    char timeStr[10];
    sprintf(timeStr, "%02d%02d%02d.00", hours, minutes, seconds);

    String gga = "$GPGGA," + String(timeStr) + "," +
                 String(latStr) + "," + String(latHem) + "," +
                 String(lonStr) + "," + String(lonHem) + "," +
                 "1,08,1.0," + String(cfg.ggaAltitude, 1) + ",M,46.9,M,,";

    // Calculate checksum
    uint8_t checksum = 0;
    for (int i = 1; i < gga.length(); i++)
    {
        checksum ^= gga[i];
    }

    char checksumStr[3];
    sprintf(checksumStr, "%02X", checksum);

    return gga + "*" + String(checksumStr) + "\r\n";
}

void NTRIPClient::sendGGA()
{
    if (!isConnected())
    {
        return;
    }

    String gga = generateGGAString();
    client.print(gga);

    Serial.print("Sent GGA: ");
    Serial.print(gga);
}

String NTRIPClient::encodeBase64(const String &input)
{
    const char *chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    String output = "";

    for (int i = 0; i < input.length(); i += 3)
    {
        uint32_t val = 0;
        int padding = 0;

        for (int j = 0; j < 3; j++)
        {
            val <<= 8;
            if (i + j < input.length())
            {
                val |= input[i + j];
            }
            else
            {
                padding++;
            }
        }

        for (int j = 0; j < 4; j++)
        {
            if (j >= 4 - padding)
            {
                output += "=";
            }
            else
            {
                output += chars[(val >> (18 - j * 6)) & 0x3F];
            }
        }
    }

    return output;
}

bool NTRIPClient::testConnectivity()
{
    SystemConfig &cfg = configManager.getConfig();

#ifdef DEBUG
    Serial.println("Testing network connectivity...");
#endif

    // Test DNS resolution first
    IPAddress serverIP;
    if (!WiFi.hostByName(cfg.ntripHost.c_str(), serverIP))
    {
#ifdef DEBUG
        Serial.println("ERROR: DNS resolution failed!");
        Serial.print("Cannot resolve hostname: ");
        Serial.println(cfg.ntripHost);
#endif
        return false;
    }

#ifdef DEBUG
    Serial.print("DNS resolution successful: ");
    Serial.print(cfg.ntripHost);
    Serial.print(" -> ");
    Serial.println(serverIP);
#endif

    // Test basic TCP connection
    WiFiClient testClient;
#ifdef DEBUG
    Serial.print("Testing TCP connection to ");
    Serial.print(serverIP);
    Serial.print(":");
    Serial.print(cfg.ntripPort);
    Serial.println("...");
#endif

    if (!testClient.connect(serverIP, cfg.ntripPort))
    {
#ifdef DEBUG
        Serial.println("ERROR: TCP connection test failed!");
#endif
        return false;
    }

#ifdef DEBUG
    Serial.println("TCP connection test successful!");
#endif
    testClient.stop();

    return true;
}

String NTRIPClient::getMountpoints(const String &host, uint16_t port, const String &user, const String &password)
{
    WiFiClient tempClient;

#ifdef DEBUG
    Serial.println("Fetching mountpoints from NTRIP caster...");
    Serial.printf("Host: %s:%d\n", host.c_str(), port);
#endif

    // Connect to NTRIP caster
    if (!tempClient.connect(host.c_str(), port))
    {
#ifdef DEBUG
        Serial.println("Failed to connect to NTRIP caster");
#endif
        return "{\"error\":\"Failed to connect to NTRIP caster\"}";
    }

    // Request sourcetable
    String auth = user + ":" + password;
    String authEncoded = encodeBase64(auth);

    String request = "GET / HTTP/1.0\r\n";
    request += "User-Agent: NTRIP ESP32Client/1.0\r\n";
    request += "Authorization: Basic " + authEncoded + "\r\n";
    request += "\r\n";

    tempClient.print(request);

    // Wait for response
    unsigned long startTime = millis();
    while (!tempClient.available() && millis() - startTime < 10000)
    {
        delay(10);
    }

    if (!tempClient.available())
    {
#ifdef DEBUG
        Serial.println("No response from NTRIP caster");
#endif
        tempClient.stop();
        return "{\"error\":\"No response from NTRIP caster\"}";
    }

    // Read HTTP status line
    String statusLine = tempClient.readStringUntil('\n');
#ifdef DEBUG
    Serial.printf("Response: %s\n", statusLine.c_str());
#endif

    if (statusLine.indexOf("200") == -1)
    {
#ifdef DEBUG
        Serial.println("Failed to get sourcetable (not 200 OK)");
#endif
        tempClient.stop();
        return "{\"error\":\"Authentication failed or sourcetable not available\"}";
    }

    // Skip HTTP headers
    while (tempClient.available())
    {
        String line = tempClient.readStringUntil('\n');
        if (line == "\r" || line == "\n" || line.length() == 0)
            break;
    }

    // Parse sourcetable and extract mountpoints
    String mountpointsJson = "[";
    bool first = true;
    int count = 0;

    while (tempClient.available() && count < 100) // Limit to 100 mountpoints
    {
        String line = tempClient.readStringUntil('\n');
        line.trim();

        // STR lines contain mountpoint information
        if (line.startsWith("STR;"))
        {
            int firstSemicolon = line.indexOf(';');
            int secondSemicolon = line.indexOf(';', firstSemicolon + 1);

            if (secondSemicolon > firstSemicolon)
            {
                String mountpoint = line.substring(firstSemicolon + 1, secondSemicolon);
                mountpoint.trim();

                if (mountpoint.length() > 0)
                {
                    if (!first)
                    {
                        mountpointsJson += ",";
                    }
                    mountpointsJson += "\"" + mountpoint + "\"";
                    first = false;
                    count++;
                }
            }
        }
    }

    mountpointsJson += "]";

    tempClient.stop();

#ifdef DEBUG
    Serial.printf("Found %d mountpoints\n", count);
#endif

    if (count == 0)
    {
        return "{\"error\":\"No mountpoints found\"}";
    }

    return "{\"mountpoints\":" + mountpointsJson + "}";
}