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

bool NetManager::fetchEcenaHtml(String& out) {
    if (!isConnected()) return false;

    WiFiClientSecure client;
    client.setInsecure();   // public price data, cert validation not critical

    HTTPClient http;
    http.setTimeout(10000);
    if (!http.begin(client, ECENA_URL)) {
        Serial.println("[NET] ecena: http.begin() failed");
        return false;
    }
    // Identify ourselves and ask explicitly for HTML — some servers serve
    // different responses based on Accept headers.
    http.setUserAgent("NordpoolMonitor/1.0 (ESP32-S3)");
    http.addHeader("Accept", "text/html");

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[NET] ecena: HTTP GET failed: %d\n", code);
        http.end();
        return false;
    }

    // The body is small (~7 KB). Reading it into a String is safe.
    out = http.getString();
    http.end();
    Serial.printf("[NET] ecena: got %d bytes\n", (int)out.length());
    return out.length() > 0;
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

bool NetManager::fetchPricesCsvStream(LineCallback onLine, void* ctx) {
    if (!isConnected()) return false;

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(20);  // seconds for read operations
    // The TLS handshake on ESP32 with a generic CA bundle is the slow part;
    // setting a small client-side buffer keeps memory usage down.

    HTTPClient http;
    http.setTimeout(30000);  // ms — Nordpool source is slow
    if (!http.begin(client, NORDPOOL_CSV_URL)) {
        Serial.println("[NET] http.begin() failed");
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[NET] HTTP GET failed: %d\n", code);
        http.end();
        return false;
    }

    // Get the underlying stream and read line-by-line. We never hold more
    // than one line in memory.
    WiFiClient* stream = http.getStreamPtr();
    if (!stream) {
        Serial.println("[NET] no stream pointer");
        http.end();
        return false;
    }

    int totalLines = 0;
    unsigned long lastRead = millis();
    String line;
    line.reserve(128);
    bool keepGoing = true;

    while (http.connected() && keepGoing) {
        if (stream->available()) {
            char c = stream->read();
            lastRead = millis();
            if (c == '\n') {
                // Strip trailing CR if present
                int len = line.length();
                if (len > 0 && line[len - 1] == '\r') line.remove(len - 1);
                if (line.length() > 0) {
                    keepGoing = onLine(line, ctx);
                    totalLines++;
                }
                line = "";
            } else {
                // Cap line length to defend against malformed input.
                if (line.length() < 1024) line += c;
            }
        } else {
            // No data available right now. Yield briefly so the network stack
            // and other tasks can run, but bail out if we've stalled too long.
            if (millis() - lastRead > 30000) {
                Serial.println("[NET] stream stall — aborting");
                break;
            }
            delay(2);
        }
    }
    // Process any trailing line that didn't end with '\n'.
    if (keepGoing && line.length() > 0) {
        onLine(line, ctx);
        totalLines++;
    }

    http.end();
    Serial.printf("[NET] streamed %d lines\n", totalLines);
    return totalLines > 0;
}
