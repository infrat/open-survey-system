#ifndef OTA_SERVICE_H
#define OTA_SERVICE_H

#include <Arduino.h>
#include "wifi_manager.h"

/**
 * @brief ArduinoOTA + mDNS (ossrtk.local). OTA transfers count as activity so
 *        the WiFi window cannot close in the middle of an update.
 */
class OtaService
{
public:
    void begin(WifiManager *wifi);
    void loop();
    void end();
    bool running() const { return _running; }

private:
    bool _running = false;
};

#endif // OTA_SERVICE_H
