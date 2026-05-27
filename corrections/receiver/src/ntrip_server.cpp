#include "ntrip_server.h"
#include "config.h"
#include "debug.h"

WiFiServer ntripServer(NTRIP_PORT);
NTRIPClient ntripClients[NTRIP_MAX_CLIENTS];
int activeClientCount = 0;

void setupNTRIPServer()
{
    DEBUG_PRINTF("Starting NTRIP Server on port %d...\n", NTRIP_PORT);

    // Initialize client array
    for (int i = 0; i < NTRIP_MAX_CLIENTS; i++)
    {
        ntripClients[i].active = false;
        ntripClients[i].authenticated = false;
        ntripClients[i].lastActivity = 0;
    }

    // Start the NTRIP server
    ntripServer.begin();
    DEBUG_PRINTF("NTRIP Server started on port %d\n", NTRIP_PORT);
    DEBUG_PRINTF("Mount point: /%s\n", NTRIP_MOUNT_POINT);
}

void handleNTRIPClients()
{
    // Check for new clients
    WiFiClient newClient = ntripServer.available();
    if (newClient)
    {
        handleNewNTRIPClient(newClient);
    }

    // Handle existing clients
    for (int i = 0; i < NTRIP_MAX_CLIENTS; i++)
    {
        if (ntripClients[i].active && ntripClients[i].client.connected())
        {
            if (ntripClients[i].client.available())
            {
                // Read client request
                String request = ntripClients[i].client.readStringUntil('\n');
                request.trim();

                DEBUG_PRINTF("NTRIP Client %d request: %s\n", i, request.c_str());

                // Simple NTRIP request handling
                if (request.startsWith("GET"))
                {
                    if (request.indexOf(NTRIP_MOUNT_POINT) > 0)
                    {
                        // Mount point request - send OK response
                        ntripClients[i].client.println("ICY 200 OK");
                        ntripClients[i].client.println("Content-Type: gnss/data");
                        ntripClients[i].client.println("");
                        ntripClients[i].authenticated = true;
                        DEBUG_PRINTF("NTRIP Client %d authenticated for mount point %s\n", i, NTRIP_MOUNT_POINT);
                    }
                    else if (request.indexOf("HTTP") > 0)
                    {
                        // Source table request
                        sendSourceTable(ntripClients[i].client);
                    }
                }
                ntripClients[i].lastActivity = millis();
            }
        }
    }

    // Remove disconnected clients
    removeDisconnectedClients();
}

void handleNewNTRIPClient(WiFiClient &client)
{
    // Find available slot
    for (int i = 0; i < NTRIP_MAX_CLIENTS; i++)
    {
        if (!ntripClients[i].active)
        {
            ntripClients[i].client = client;
            ntripClients[i].active = true;
            ntripClients[i].authenticated = false;
            ntripClients[i].lastActivity = millis();
            activeClientCount++;

            DEBUG_PRINTF("New NTRIP client %d connected from %s\n", i, client.remoteIP().toString().c_str());
            return;
        }
    }

    // No available slots
    DEBUG_PRINTLN("NTRIP server full, rejecting client");
    client.println("HTTP/1.1 503 Service Unavailable");
    client.println("Connection: close");
    client.println("");
    client.stop();
}

void removeDisconnectedClients()
{
    for (int i = 0; i < NTRIP_MAX_CLIENTS; i++)
    {
        if (ntripClients[i].active && !ntripClients[i].client.connected())
        {
            DEBUG_PRINTF("NTRIP Client %d disconnected\n", i);
            ntripClients[i].active = false;
            ntripClients[i].authenticated = false;
            ntripClients[i].client.stop();
            activeClientCount--;
        }
    }
}

void broadcastRTCMData(const uint8_t *data, size_t length)
{
    int sentCount = 0;

    for (int i = 0; i < NTRIP_MAX_CLIENTS; i++)
    {
        if (ntripClients[i].active && ntripClients[i].authenticated && ntripClients[i].client.connected())
        {
            size_t written = ntripClients[i].client.write(data, length);
            if (written == length)
            {
                sentCount++;
                ntripClients[i].lastActivity = millis();
            }
            else
            {
                DEBUG_PRINTF("Failed to send RTCM data to client %d\n", i);
            }
        }
    }

    if (sentCount > 0)
    {
        DEBUG_PRINTF("Broadcasted %u bytes RTCM data to %d clients\n", length, sentCount);
    }
}

void sendSourceTable(WiFiClient &client)
{
    DEBUG_PRINTLN("Sending NTRIP source table to client");

    client.println("SOURCETABLE 200 OK");
    client.println("Content-Type: gnss/sourcetable");
    client.println("Content-Length: 150");
    client.println("");

    // Simple source table entry
    client.printf("STR;%s;%s;RTCM 3.x;1005,1077,1087,1097,1127,1230;2;GPS+GLO+GAL+BDS;SNIP;ESP32;N;N;560;none\r\n",
                  NTRIP_MOUNT_POINT, NTRIP_MOUNT_POINT);
    client.println("ENDSOURCETABLE");

    client.stop();
}

int getNTRIPClientCount()
{
    return activeClientCount;
}