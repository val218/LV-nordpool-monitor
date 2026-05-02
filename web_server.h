// net_manager.h — Wi-Fi connection management (STA with AP fallback) + HTTPS fetch.
#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "price_data.h"

class NetManager {
public:
    NetManager();

    // Load stored SSID/pass from NVS and attempt connection.
    // If no credentials or connection fails, launches AP mode so the user
    // can configure via the web UI at http://192.168.4.1.
    void begin();

    // Call from loop()
    void loop();

    bool   isConnected() const { return WiFi.status() == WL_CONNECTED; }
    bool   isApMode() const    { return _apActive; }
    String ipString() const;

    // Save new credentials and (re)connect
    void setCredentials(const String& ssid, const String& pass);

    // Fetch the Nordpool CSV into `out`. Returns true on success.
    bool fetchPricesCsv(String& out);

private:
    bool _apActive = false;
    unsigned long _lastRetry = 0;

    void startAp();
    bool connectSta(const String& ssid, const String& pass, uint32_t timeoutMs = 15000);
    void loadAndConnect();
};
