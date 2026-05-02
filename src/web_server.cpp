#include "web_server.h"
#include "i18n.h"
#include <Preferences.h>
#include <WiFi.h>
#include <ArduinoJson.h>

// Embedded HTML UI. Kept small and self-contained so everything fits in flash.
static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>Nordpool Monitor</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body { margin:0; background:#0b0f14; color:#e8eef7; font:14px -apple-system,Segoe UI,Roboto,sans-serif; padding:14px; padding-bottom:40px; }
  h1 { margin:0 0 10px; font-size:18px; }
  h2 { margin:18px 0 8px; font-size:15px; color:#9cb; }
  .card { background:#131a23; border:1px solid #223; border-radius:10px; padding:12px; margin-bottom:10px; }
  label { display:block; font-size:12px; color:#9ab; margin:6px 0 2px; }
  input, select, button { width:100%; padding:10px; font-size:14px; border-radius:8px;
    border:1px solid #334; background:#0f151c; color:#e8eef7; }
  button { background:#1e6fd9; border-color:#1e6fd9; color:#fff; font-weight:600; margin-top:8px; }
  button.secondary { background:#2a3544; border-color:#2a3544; }
  .row { display:flex; gap:8px; flex-wrap:wrap; }
  .row > * { flex:1; min-width:100px; }
  .dot { display:inline-block; width:8px; height:8px; border-radius:50%; margin-right:6px; vertical-align:middle; }
  .dot.ok { background:#2c2; } .dot.bad { background:#c22; } .dot.warn { background:#e92; }
  .relay { border:1px solid #223; border-radius:8px; padding:10px; margin-bottom:8px; }
  .relay-head { display:flex; justify-content:space-between; align-items:center; }
  .relay-state { padding:2px 8px; border-radius:12px; font-size:11px; font-weight:700; }
  .relay-state.on { background:#1a4; color:#fff; } .relay-state.off { background:#333; color:#aaa; }
  .muted { color:#8a9; font-size:12px; }
  .price { font-size:24px; font-weight:700; color:#5cf; }
</style></head>
<body>
<h1>Nordpool Monitor</h1>
<div id="status" class="card"><span class="dot warn"></span>Loading…</div>

<div class="card">
  <h2>Current</h2>
  <div id="priceNow" class="price">-- c/kWh</div>
  <div class="muted" id="priceMeta">prev — | next —</div>
</div>

<div class="card">
  <h2>Wi-Fi</h2>
  <label>SSID</label>
  <select id="ssidSel"><option value="">-- scanning --</option></select>
  <label>Password</label>
  <input id="wpass" type="password" autocomplete="off">
  <div class="row">
    <button onclick="scanWifi()" class="secondary">Rescan</button>
    <button onclick="saveWifi()">Connect</button>
  </div>
</div>

<div class="card">
  <h2>Display</h2>
  <label>Language</label>
  <select id="lang">
    <option value="0">English</option>
    <option value="1">Latviešu</option>
    <option value="2">Русский</option>
  </select>
  <label>VAT %</label>
  <input id="vat" type="number" step="0.1" min="0" max="50">
  <label><input id="showvat" type="checkbox"> Show prices with VAT</label>
  <button onclick="saveDisplay()">Save</button>
</div>

<div class="card">
  <h2>Relays</h2>
  <div id="relays"></div>
  <button onclick="saveRelays()">Save relays</button>
</div>

<script>
async function loadState() {
  const r = await fetch('/api/state');
  const j = await r.json();
  const s = document.getElementById('status');
  const dot = j.wifi ? 'ok' : (j.ap ? 'warn' : 'bad');
  s.innerHTML = '<span class="dot '+dot+'"></span>'
    + (j.ap ? 'AP mode ('+j.ip+')' : (j.wifi ? 'Online — ' + j.ip : 'Offline'))
    + ' • ' + (j.time || '--:--');
  const fmt = p => p==null ? '—' : (p*100).toFixed(2) + ' c/kWh';
  document.getElementById('priceNow').textContent = fmt(j.current);
  document.getElementById('priceMeta').textContent = 'prev ' + fmt(j.prev) + ' | next ' + fmt(j.next);
  document.getElementById('lang').value = j.lang;
  document.getElementById('vat').value = j.vat;
  document.getElementById('showvat').checked = !!j.showvat;
  renderRelays(j.relays);
}
function renderRelays(rs) {
  const el = document.getElementById('relays');
  el.innerHTML = '';
  const modes = ['Always OFF', 'Always ON', 'ON below', 'OFF above'];
  rs.forEach((r, i) => {
    const div = document.createElement('div');
    div.className = 'relay';
    div.innerHTML = `
      <div class="relay-head">
        <strong>Relay ${i+1}</strong>
        <span class="relay-state ${r.state?'on':'off'}">${r.state?'ON':'OFF'}</span>
      </div>
      <label>Name</label>
      <input data-i="${i}" data-k="name" value="${r.name}" maxlength="16">
      <label>Mode</label>
      <select data-i="${i}" data-k="mode">
        ${modes.map((m,idx)=>`<option value="${idx}" ${r.mode==idx?'selected':''}>${m}</option>`).join('')}
      </select>
      <label>Threshold (c/kWh)</label>
      <input data-i="${i}" data-k="threshold" type="number" step="0.1" min="0" max="200" value="${(r.threshold*100).toFixed(1)}">
    `;
    el.appendChild(div);
  });
}
async function scanWifi() {
  const sel = document.getElementById('ssidSel');
  sel.innerHTML = '<option value="">-- scanning --</option>';
  const r = await fetch('/api/scan');
  const j = await r.json();
  sel.innerHTML = '';
  j.forEach(n => {
    const o = document.createElement('option');
    o.value = n.ssid; o.textContent = n.ssid + ' (' + n.rssi + ')';
    sel.appendChild(o);
  });
  if (j.length === 0) sel.innerHTML = '<option value="">(no networks found)</option>';
}
async function saveWifi() {
  const ssid = document.getElementById('ssidSel').value;
  const pass = document.getElementById('wpass').value;
  if (!ssid) { alert('Pick an SSID'); return; }
  const body = new URLSearchParams({ssid, pass}).toString();
  const r = await fetch('/api/wifi', {method:'POST', body, headers:{'Content-Type':'application/x-www-form-urlencoded'}});
  alert(await r.text());
}
async function saveDisplay() {
  const body = new URLSearchParams({
    lang: document.getElementById('lang').value,
    vat: document.getElementById('vat').value,
    showvat: document.getElementById('showvat').checked ? '1' : '0'
  }).toString();
  await fetch('/api/display', {method:'POST', body, headers:{'Content-Type':'application/x-www-form-urlencoded'}});
  loadState();
}
async function saveRelays() {
  const inputs = document.querySelectorAll('#relays [data-i]');
  const data = {};
  inputs.forEach(inp => {
    const i = inp.dataset.i, k = inp.dataset.k;
    if (!data[i]) data[i] = {};
    data[i][k] = k === 'threshold' ? (parseFloat(inp.value)/100.0) : (k === 'mode' ? parseInt(inp.value) : inp.value);
  });
  await fetch('/api/relays', {method:'POST', body: JSON.stringify(data), headers:{'Content-Type':'application/json'}});
  loadState();
}
loadState(); scanWifi();
setInterval(loadState, 10000);
</script>
</body></html>)HTML";

extern Lang g_lang;  // defined in main.cpp

WebUI::WebUI(NetManager& net, RelayController& relays, UI& ui)
    : _net(net), _relays(relays), _ui(ui), _server(80) {}

String WebUI::buildStateJson() {
    JsonDocument doc;
    doc["wifi"] = _net.isConnected();
    doc["ap"]   = _net.isApMode();
    doc["ip"]   = _net.ipString();
    doc["lang"] = (int)g_lang;
    doc["vat"]  = _ui.vatPercent();
    doc["showvat"] = _ui.showVat();

    time_t now = time(nullptr);
    if (now > 100000) {
        struct tm t; localtime_r(&now, &t);
        char tb[8]; snprintf(tb, sizeof(tb), "%02d:%02d", t.tm_hour, t.tm_min);
        doc["time"] = tb;
    }

    extern class PriceData g_prices;
    const PriceEntry* cur = g_prices.getCurrent(now);
    const PriceEntry* prv = g_prices.getPrevious(now);
    const PriceEntry* nxt = g_prices.getNext(now);
    if (cur) doc["current"] = cur->price_raw; else doc["current"] = nullptr;
    if (prv) doc["prev"]    = prv->price_raw; else doc["prev"]    = nullptr;
    if (nxt) doc["next"]    = nxt->price_raw; else doc["next"]    = nullptr;

    JsonArray rs = doc["relays"].to<JsonArray>();
    for (int i = 0; i < NUM_RELAYS; ++i) {
        const RelayRule& r = _relays.rule(i);
        JsonObject o = rs.add<JsonObject>();
        o["name"] = r.name;
        o["mode"] = (int)r.mode;
        o["threshold"] = r.threshold;
        o["state"] = r.state;
    }

    String out;
    serializeJson(doc, out);
    return out;
}

void WebUI::begin() {
    _server.on("/", HTTP_GET, [this](AsyncWebServerRequest* req) {
        // INDEX_HTML is in PROGMEM; ESPAsyncWebServer's modern send() handles
        // that transparently on ESP32 (flash is memory-mapped, no special copy
        // needed). The older send_P() is deprecated and emits warnings.
        req->send(200, "text/html; charset=utf-8", INDEX_HTML);
    });

    _server.on("/api/state", HTTP_GET, [this](AsyncWebServerRequest* req) {
        req->send(200, "application/json", buildStateJson());
    });

    _server.on("/api/scan", HTTP_GET, [this](AsyncWebServerRequest* req) {
        int n = WiFi.scanComplete();
        if (n == WIFI_SCAN_FAILED || n == WIFI_SCAN_RUNNING) {
            WiFi.scanNetworks(true);  // async
            req->send(200, "application/json", "[]");
            return;
        }
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (int i = 0; i < n; ++i) {
            JsonObject o = arr.add<JsonObject>();
            o["ssid"] = WiFi.SSID(i);
            o["rssi"] = WiFi.RSSI(i);
        }
        WiFi.scanDelete();
        WiFi.scanNetworks(true);  // kick off a fresh scan for next call
        String s; serializeJson(doc, s);
        req->send(200, "application/json", s);
    });

    _server.on("/api/wifi", HTTP_POST, [this](AsyncWebServerRequest* req) {
        String ssid, pass;
        if (req->hasParam("ssid", true)) ssid = req->getParam("ssid", true)->value();
        if (req->hasParam("pass", true)) pass = req->getParam("pass", true)->value();
        if (ssid.isEmpty()) { req->send(400, "text/plain", "Missing SSID"); return; }
        req->send(200, "text/plain", "Saved. Attempting to connect…");
        // Defer actual connect slightly so the response flushes
        _net.setCredentials(ssid, pass);
    });

    _server.on("/api/display", HTTP_POST, [this](AsyncWebServerRequest* req) {
        if (req->hasParam("lang", true)) {
            int l = req->getParam("lang", true)->value().toInt();
            if (l >= 0 && l < LANG_COUNT) g_lang = (Lang)l;
        }
        if (req->hasParam("vat", true)) {
            _ui.setVatPercent(req->getParam("vat", true)->value().toFloat());
        }
        if (req->hasParam("showvat", true)) {
            _ui.setShowVat(req->getParam("showvat", true)->value() == "1");
        }
        Preferences p; p.begin(PREFS_NS, false);
        p.putUChar("lang", (uint8_t)g_lang);
        p.putFloat("vat", _ui.vatPercent());
        p.putBool("showvat", _ui.showVat());
        p.end();
        _ui.redraw();
        req->send(200, "text/plain", "OK");
    });

    _server.on("/api/relays", HTTP_POST,
        [](AsyncWebServerRequest*){},
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index != 0 || len != total) { req->send(400, "text/plain", "Chunked not supported"); return; }
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) { req->send(400, "text/plain", "Bad JSON"); return; }
            JsonObject root = doc.as<JsonObject>();
            for (JsonPair kv : root) {
                int i = String(kv.key().c_str()).toInt();
                if (i < 0 || i >= NUM_RELAYS) continue;
                JsonObject o = kv.value().as<JsonObject>();
                RelayRule& r = _relays.rule(i);
                if (o["name"].is<const char*>()) {
                    const char* nm = o["name"];
                    strncpy(r.name, nm, sizeof(r.name) - 1);
                    r.name[sizeof(r.name) - 1] = 0;
                }
                if (o["mode"].is<int>()) r.mode = (RelayMode)(int)o["mode"];
                if (o["threshold"].is<float>()) r.threshold = o["threshold"];
            }
            _relays.save();
            _ui.redraw();
            req->send(200, "text/plain", "OK");
        });

    _server.onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "text/plain", "Not found");
    });

    _server.begin();
    Serial.println("[WEB] HTTP server started on port 80");
}
