#include "net_manager.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>

NetManager::NetManager() {}

void NetManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    loadAndConnect();
}

void NetManager::loadAndConnect() {
    Preferences p;
    p.begin(PREFS_NS, true);
    String ssid = p.getString("wssid", "");
    String pass = p.getString("wpass", "");
    p.end();

    if (ssid.length() == 0) {
        Serial.println("[NET] No saved SSID — starting AP");
        startAp();
        return;
    }

    Serial.printf("[NET] Connecting to %s\n", ssid.c_str());
    if (!connectSta(ssid, pass)) {
        Serial.println("[NET] STA failed — starting AP");
        startAp();
    } else {
        _apActive = false;
        Serial.printf("[NET] STA connected: %s\n", WiFi.localIP().toString().c_str());
    }
}

bool NetManager::connectSta(const String& ssid, const String& pass, uint32_t timeoutMs) {
    if (_apActive) { WiFi.softAPdisconnect(true); _apActive = false; }
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        if (WiFi.status() == WL_CONNECTED) return true;
        delay(250);
    }
    return false;
}

void NetManager::startAp() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    _apActive = true;
    Serial.printf("[NET] AP started: %s (%s)\n", AP_SSID, WiFi.softAPIP().toString().c_str());
}

void NetManager::setCredentials(const String& ssid, const String& pass) {
    Preferences p;
    p.begin(PREFS_NS, false);
    p.putString("wssid", ssid);
    p.putString("wpass", pass);
    p.end();

    // Try to connect immediately
    if (connectSta(ssid, pass, 15000)) {
        _apActive = false;
    } else {
        startAp();
    }
}

String NetManager::ipString() const {
    if (_apActive) return WiFi.softAPIP().toString();
    if (WiFi.status() == WL_CONNECTED) return WiFi.localIP().toString();
    return String("--");
}

void NetManager::loop() {
    // If we're in AP mode but a known SSID was saved later, nothing to do here —
    // the web UI triggers setCredentials(). Auto-reconnect on STA is handled by WiFi library.
    if (!_apActive && WiFi.status() != WL_CONNECTED) {
        if (millis() - _lastRetry > 30000) {
            _lastRetry = millis();
            WiFi.reconnect();
        }
    }
}

bool NetManager::fetchPricesCsv(String& out) {
    if (!isConnected()) return false;

    WiFiClientSecure client;
    // The source isn't security-critical (public price data), so skip cert validation
    // to keep flash footprint reasonable. If you want strict TLS, bundle the CA.
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(15000);
    if (!http.begin(client, NORDPOOL_CSV_URL)) return false;

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[NET] HTTP GET failed: %d\n", code);
        http.end();
        return false;
    }
    out = http.getString();
    http.end();
    return out.length() > 0;
}
