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
    // CAUTION: holds the entire response in memory. Use fetchPricesCsvStream()
    // for large bodies — the .didnt.work source returns 1.8+ MB which will
    // exceed Arduino String's heap.
    bool fetchPricesCsv(String& out);

    // Streaming version: calls `onLine` once per CSV line. Returns true on
    // success. The callback's String argument is the line text without the
    // trailing newline. The callback returns true to keep going, false to
    // abort. Use this for the Nordpool source — it's the only fetch that
    // works with the full ~2 MB response on a 320 KB heap.
    typedef bool (*LineCallback)(const String& line, void* ctx);
    bool fetchPricesCsvStream(LineCallback onLine, void* ctx);

    // Fetch the e-cena.lv HTML page (~7 KB). Returns true on success and
    // fills `out` with the HTML body. This is the preferred data source —
    // small payload, only today + tomorrow, 15-minute resolution.
    bool fetchEcenaHtml(String& out);

private:
    bool _apActive = false;
    unsigned long _lastRetry = 0;

    void startAp();
    bool connectSta(const String& ssid, const String& pass, uint32_t timeoutMs = 15000);
    void loadAndConnect();
};
