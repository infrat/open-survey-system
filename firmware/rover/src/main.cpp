#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "config.h"
#include "logger.h"
#include "settings.h"
#include "uart_handler.h"
#include "transport_factory.h"
#include "wifi_manager.h"
#include "ota_service.h"
#include "telnet_service.h"
#include "web_server.h"
#include "bridge.h"
#include "system_control.h"

// Persistent configuration (edited through the web UI)
static RoverSettings settings;
static SettingsStore settingsStore;

// Modules
static UARTHandler uartHandler;
static WifiManager wifi;
static OtaService ota;
static TelnetService telnet;
static RoverWebServer webServer;
static Bridge bridge;

// Created from settings.transport. Stays nullptr during the WiFi-only setup
// window: WiFi and Bluetooth share one radio and most of the heap, so the
// Bluetooth transports only start once WiFi has been switched off.
static ITransport *transport = nullptr;

static bool networkServicesStarted = false;
static bool ntpStarted = false;
static unsigned long setupLedMs = 0;

// Called by WifiManager right before it switches the radio off
static void onWifiShutdown()
{
    webServer.end();
    telnet.stop();
    ota.end();
    networkServicesStarted = false;
}

// OTA, mDNS and Telnet need a live interface, so they start once the WiFi
// manager reports one (STA got an IP, or the AP is up).
static void startNetworkServices()
{
    ota.begin(&wifi);
    telnet.begin(&wifi, &transport);
    networkServicesStarted = true;
}

static void haltWithBlink(uint32_t periodMs)
{
    while (1)
    {
        digitalWrite(LINK_STATUS_LED_PIN, !digitalRead(LINK_STATUS_LED_PIN));
        delay(periodMs);
    }
}

// Bring up the client-facing transport (BLE NUS, Bluetooth SPP or WiFi TCP)
// and connect it to the GNSS UART.
static void startTransport()
{
    ITransport *t = createTransport(settings);
    logPrintln("[Setup] Initializing %s...", t->name());

    // Set logging callback so the transport can use logPrintln
    t->setLogCallback(logPrintln);

    if (!t->begin(BT_DEVICE_NAME))
    {
        logPrintln("[ERROR] %s initialization failed!", t->name());
        haltWithBlink(200);
    }

    // Radio-specific low power TX level (see BLE_TX_POWER / SPP_TX_POWER)
    t->setTxPower();

    bridge.begin(&uartHandler, t);
    transport = t; // published last: the web UI / Telnet read it from other tasks

    logPrintln("Waiting for GPS data and client connection...\n");
}

// Setup window: no client link yet. Drop GNSS output so the first client does
// not get a stale backlog, and show a distinct LED pattern (short flash every
// second, vs. the even blink of "waiting for a client").
static void setupWindowLoop()
{
    // Read only what is already buffered: readData() waits for the full length
    // otherwise, and this loop also has to serve OTA and the window timeout.
    static uint8_t scratch[256];
    int pending = uartHandler.available();
    if (pending > 0)
    {
        uartHandler.readData(scratch, min((size_t)pending, sizeof(scratch)));
    }

    unsigned long phase = (millis() - setupLedMs) % 1000;
    digitalWrite(LINK_STATUS_LED_PIN, phase < 60 ? HIGH : LOW);
}

void setup()
{
    // Initialize Debug UART (UART0 - USB Serial)
    Serial.begin(DEBUG_BAUD_RATE);
    delay(1000);

    logPrintln("\n\n========================================");
    logPrintln("OSS RTK Rover %s", FIRMWARE_VERSION);
    logPrintln("GPS (NMEA) ↔ UART bridge");
    logPrintln("========================================\n");

    // Initialize link status LED
    pinMode(LINK_STATUS_LED_PIN, OUTPUT);
    digitalWrite(LINK_STATUS_LED_PIN, LOW); // LOW = OFF (active HIGH)

    // 1. Settings
    settingsStore.load(settings);
    logPrintln("[Setup] Transport: %s, WiFi mode: %s, setup window: %u s",
               transportModeName(settings.transport), wifiModeId(settings.wifiMode), settings.setupWindowSec);

    // 2. WiFi + web UI (non-blocking)
    wifi.setShutdownCallback(onWifiShutdown);
    wifi.begin(settings);
    webServer.begin(&settings, &settingsStore, &wifi, &transport);

    // 3. Initialize UART for GPS
    logPrintln("[Setup] Initializing UART...");

    // Set logging callback so UART handler can use logPrintln
    uartHandler.setLogCallback(logPrintln);

    if (!uartHandler.begin())
    {
        logPrintln("[ERROR] UART initialization failed!");
        haltWithBlink(100);
    }

    // 4. Client-facing transport. WiFi TCP runs alongside the web UI, so it
    //    starts now. Bluetooth waits for the setup window to close (see loop()).
    if (settings.transport == TransportMode::TCP)
    {
        startTransport();
    }
    else
    {
        setupLedMs = millis();
        logPrintln("[Setup] Setup window open: WiFi only, %s starts when it closes", transportModeName(settings.transport));
    }

    logPrintln("\n[Setup] Initialization complete!");
}

void loop()
{
    // ===== WiFi window, services =====
    wifi.loop();

    if (wifi.enabled())
    {
        if (!networkServicesStarted && wifi.servicesReady())
        {
            startNetworkServices();
        }

        // Time only makes sense with internet access, i.e. a connected STA
        if (!ntpStarted && wifi.staConnected())
        {
            configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
            ntpStarted = true;
            logPrintln("[NTP] Synchronizing time...");
        }

        if (networkServicesStarted)
        {
            ota.loop();
            telnet.loop();
        }
    }

    if (transport)
    {
        // ===== GPS ↔ client bridge (+ status LED) =====
        bridge.loop();
    }
    else if (!wifi.enabled())
    {
        // Setup window just closed: the radio and its heap are free for Bluetooth
        startTransport();
    }
    else
    {
        setupWindowLoop();
    }

    systemControlLoop();

    // Small delay for stability
    delay(10);
}
