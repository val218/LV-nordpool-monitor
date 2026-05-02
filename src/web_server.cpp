#include "web_server.h"
#include "i18n.h"
#include "relay_icons.h"
#include <Preferences.h>
#include <WiFi.h>
#include <ArduinoJson.h>

// Embedded HTML UI. Kept compact but feature-rich: SVG price chart, full
// price table, and a per-relay FontAwesome icon picker. FA glyphs come from
// the public CDN (cdnjs.cloudflare.com) — they require internet on the
// browser side. No internet → icons render as small empty squares but the
// rest of the UI still works.
static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>Nordpool Monitor</title>
<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.4/css/all.min.css">
<style>
  :root { color-scheme: dark; --bg:#0b0f14; --card:#131a23; --line:#223; --text:#e8eef7;
          --dim:#9ab; --accent:#1e6fd9;
          --green:#5dd17a; --yellow:#e6c84d; --orange:#ff9b3d; --red:#ff5757; }
  * { box-sizing: border-box; }
  body { margin:0; background:var(--bg); color:var(--text); font:14px -apple-system,Segoe UI,Roboto,sans-serif; padding:14px; padding-bottom:40px; }
  h1 { margin:0 0 10px; font-size:18px; }
  h2 { margin:18px 0 8px; font-size:15px; color:#9cb; }
  .card { background:var(--card); border:1px solid var(--line); border-radius:10px; padding:12px; margin-bottom:10px; }
  label { display:block; font-size:12px; color:var(--dim); margin:6px 0 2px; }
  input, select, button { width:100%; padding:10px; font-size:14px; border-radius:8px;
    border:1px solid #334; background:#0f151c; color:var(--text); }
  button { background:var(--accent); border-color:var(--accent); color:#fff; font-weight:600; margin-top:8px; cursor:pointer; }
  button.secondary { background:#2a3544; border-color:#2a3544; }
  button.tiny { width:auto; padding:6px 10px; font-size:12px; margin:0; }
  .row { display:flex; gap:8px; flex-wrap:wrap; }
  .row > * { flex:1; min-width:100px; }
  .dot { display:inline-block; width:8px; height:8px; border-radius:50%; margin-right:6px; vertical-align:middle; }
  .dot.ok { background:#2c2; } .dot.bad { background:#c22; } .dot.warn { background:#e92; }
  .relay { border:1px solid var(--line); border-radius:8px; padding:10px; margin-bottom:8px; }
  .relay-head { display:flex; justify-content:space-between; align-items:center; gap:8px; }
  .relay-name { display:flex; align-items:center; gap:8px; flex:1; min-width:0; }
  .relay-icon { width:36px; height:36px; border-radius:6px; background:#0f151c; border:1px solid #334;
                display:flex; align-items:center; justify-content:center; font-size:18px; color:#9cb;
                cursor:pointer; flex-shrink:0; }
  .relay-icon:hover { background:#1a2332; }
  .relay-state { padding:2px 8px; border-radius:12px; font-size:11px; font-weight:700; flex-shrink:0; }
  .relay-state.on { background:#1a4; color:#fff; } .relay-state.off { background:#333; color:#aaa; }
  .muted { color:#8a9; font-size:12px; }
  .price { font-size:24px; font-weight:700; color:#5cf; }

  /* SVG chart */
  .chart-wrap { width:100%; }
  .chart-wrap svg { width:100%; height:200px; display:block; }
  .chart-meta { display:flex; justify-content:space-between; font-size:12px; color:var(--dim); margin-top:6px; }

  /* Collapsible price list */
  details summary { cursor:pointer; padding:6px 0; color:var(--dim); font-size:13px; user-select:none; }
  .price-table { width:100%; border-collapse:collapse; margin-top:8px; font-size:13px; }
  .price-table th { text-align:left; color:var(--dim); font-weight:500; padding:4px 6px; border-bottom:1px solid var(--line); }
  .price-table td { padding:3px 6px; border-bottom:1px solid #1a2230; }
  .price-table .swatch { display:inline-block; width:10px; height:10px; border-radius:2px; margin-right:6px; vertical-align:middle; }
  .now-row { background:#1a3a5c; }

  /* Icon picker modal */
  .modal-bg { position:fixed; inset:0; background:rgba(0,0,0,0.7); display:none; align-items:center; justify-content:center; padding:20px; z-index:100; }
  .modal-bg.open { display:flex; }
  .modal { background:var(--card); border:1px solid var(--line); border-radius:12px; padding:14px; max-width:420px; width:100%; max-height:80vh; overflow-y:auto; }
  .modal h3 { margin:0 0 10px; font-size:15px; }
  .icon-grid { display:grid; grid-template-columns:repeat(5, 1fr); gap:6px; }
  .icon-cell { aspect-ratio:1; border:1px solid var(--line); border-radius:6px; display:flex;
               flex-direction:column; align-items:center; justify-content:center; gap:4px;
               cursor:pointer; padding:6px; text-align:center; }
  .icon-cell:hover { background:#1a2332; }
  .icon-cell .glyph { font-size:20px; color:var(--text); }
  .icon-cell .lbl   { font-size:9px; color:var(--dim); line-height:1.1; }
  .icon-cell.selected { border-color:var(--accent); background:#15233a; }
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
  <h2 id="chartTitle">Today</h2>
  <div class="chart-wrap"><svg id="chart" viewBox="0 0 480 200" preserveAspectRatio="none"></svg></div>
  <div class="chart-meta">
    <span id="chartMin">min —</span>
    <span id="chartAvg">avg —</span>
    <span id="chartMax">max —</span>
  </div>
  <details>
    <summary>Show all prices</summary>
    <table class="price-table" id="priceTable">
      <thead><tr><th>Time</th><th>Price (c/kWh)</th></tr></thead>
      <tbody></tbody>
    </table>
  </details>
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

<!-- Icon picker modal -->
<div id="iconModal" class="modal-bg" onclick="if(event.target===this)closeIconModal()">
  <div class="modal">
    <h3>Pick an icon</h3>
    <div class="icon-grid" id="iconGrid"></div>
  </div>
</div>

<script>
let ICONS = [];        // catalog from /api/state
let CUR_ICON_RELAY = -1;
let CUR_RELAYS = [];   // last-known relay state, used by the picker

// -- Color tier (mirrors UI::colorForPrice on the LCD) -------------------
function colorForPrice(p, mn, mx) {
  if (mx <= mn) return 'var(--green)';
  const t = (p - mn) / (mx - mn);
  if (t < 0.25) return 'var(--green)';
  if (t < 0.55) return 'var(--yellow)';
  if (t < 0.80) return 'var(--orange)';
  return 'var(--red)';
}

// -- Chart rendering -----------------------------------------------------
async function loadPrices() {
  const r = await fetch('/api/prices');
  const j = await r.json();
  if (!j.entries || !j.entries.length) {
    document.getElementById('chart').innerHTML =
      '<text x="240" y="100" text-anchor="middle" fill="#688">no price data yet</text>';
    return;
  }
  drawChart(j.entries);
  fillPriceTable(j.entries);
}

function drawChart(entries) {
  // Filter to a "today + tomorrow" window starting at 00:00 of the most
  // recent local midnight not in the future. Same logic as the LCD.
  const now = Date.now() / 1000;
  const todayLocal = new Date();
  todayLocal.setHours(0,0,0,0);
  const todayStart = todayLocal.getTime() / 1000;
  const points = entries
    .filter(e => e.ts >= todayStart && e.ts < todayStart + 48*3600)
    .sort((a,b) => a.ts - b.ts);
  if (points.length < 2) {
    document.getElementById('chart').innerHTML =
      '<text x="240" y="100" text-anchor="middle" fill="#688">need more data</text>';
    return;
  }

  let mn = points[0].price, mx = mn, sum = 0;
  for (const p of points) { if (p.price < mn) mn = p.price; if (p.price > mx) mx = p.price; sum += p.price; }
  const avg = sum / points.length;

  // Layout in viewBox coordinates 0..480 × 0..200
  const padL = 32, padR = 8, padT = 6, padB = 22;
  const W = 480 - padL - padR;
  const H = 200 - padT - padB;
  const lo = Math.min(0, mn) * 1.05;
  const hi = mx * 1.05;
  const yRange = (hi - lo) || 0.01;
  const xAt = i => padL + (W * i) / (points.length - 1);
  const yAt = v => padT + H - ((v - lo) / yRange) * H;

  let svg = '';

  // Horizontal grid lines (4)
  for (let i = 0; i <= 4; ++i) {
    const y = padT + (H * i) / 4;
    svg += `<line x1="${padL}" y1="${y}" x2="${padL+W}" y2="${y}" stroke="#1a2230" stroke-width="1"/>`;
  }

  // X-axis hour labels every 6 hours
  for (let h = 0; h <= 47; h += 6) {
    if (h >= points.length) continue;
    const x = xAt(h);
    svg += `<line x1="${x}" y1="${padT+H}" x2="${x}" y2="${padT+H+3}" stroke="#445" stroke-width="1"/>`;
    svg += `<text x="${x}" y="${padT+H+15}" text-anchor="middle" font-size="10" fill="#8a9">${h}</text>`;
  }

  // Average dashed line
  const yAvg = yAt(avg);
  svg += `<line x1="${padL}" y1="${yAvg}" x2="${padL+W}" y2="${yAvg}" stroke="#fff" stroke-opacity="0.5" stroke-width="1" stroke-dasharray="3 3"/>`;

  // Now-line
  const nowFracIdx = (now - points[0].ts) / 3600;
  if (nowFracIdx >= 0 && nowFracIdx <= points.length - 1) {
    const xNow = xAt(nowFracIdx);
    svg += `<line x1="${xNow}" y1="${padT}" x2="${xNow}" y2="${padT+H}" stroke="#1e6fd9" stroke-width="1" stroke-opacity="0.6"/>`;
  }

  // Price line — drawn as individual segments so each can be colored
  // by its own price tier, matching the LCD.
  for (let i = 1; i < points.length; ++i) {
    const x1 = xAt(i-1), y1 = yAt(points[i-1].price);
    const x2 = xAt(i),   y2 = yAt(points[i].price);
    const c = colorForPrice(points[i].price, mn, mx);
    svg += `<line x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}" stroke="${c}" stroke-width="2.5" stroke-linecap="round"/>`;
  }

  // Y-axis labels (min / avg / max)
  svg += `<text x="${padL-4}" y="${padT+8}"        text-anchor="end" font-size="10" fill="#8a9">${(mx*100).toFixed(1)}</text>`;
  svg += `<text x="${padL-4}" y="${yAvg+3}"        text-anchor="end" font-size="10" fill="#fff" fill-opacity="0.6">${(avg*100).toFixed(1)}</text>`;
  svg += `<text x="${padL-4}" y="${padT+H-1}"      text-anchor="end" font-size="10" fill="#8a9">${(mn*100).toFixed(1)}</text>`;

  document.getElementById('chart').innerHTML = svg;

  document.getElementById('chartMin').textContent = 'min ' + (mn*100).toFixed(2);
  document.getElementById('chartAvg').textContent = 'avg ' + (avg*100).toFixed(2);
  document.getElementById('chartMax').textContent = 'max ' + (mx*100).toFixed(2);

  // Set chart title to "Today, D Mon"
  const t = new Date(points[0].ts * 1000);
  const months = ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
  document.getElementById('chartTitle').textContent =
    `Today, ${t.getDate()} ${months[t.getMonth()]}`;
}

function fillPriceTable(entries) {
  const now = Date.now() / 1000;
  const tbody = document.querySelector('#priceTable tbody');
  tbody.innerHTML = '';
  const sorted = [...entries].sort((a,b) => a.ts - b.ts);
  let mn = sorted[0].price, mx = mn;
  for (const e of sorted) { if (e.price < mn) mn = e.price; if (e.price > mx) mx = e.price; }
  for (const e of sorted) {
    const t = new Date(e.ts * 1000);
    const isNow = now >= e.ts && now < e.ts + 3600;
    const tr = document.createElement('tr');
    if (isNow) tr.className = 'now-row';
    const dd = String(t.getDate()).padStart(2,'0');
    const mm = String(t.getMonth()+1).padStart(2,'0');
    const hh = String(t.getHours()).padStart(2,'0');
    const c = colorForPrice(e.price, mn, mx);
    tr.innerHTML = `<td>${dd}.${mm} ${hh}:00</td>
                    <td><span class="swatch" style="background:${c}"></span>${(e.price*100).toFixed(2)}</td>`;
    tbody.appendChild(tr);
  }
}

// -- State + relays ------------------------------------------------------
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
  ICONS = j.icon_catalog || [];
  CUR_RELAYS = j.relays || [];
  renderRelays(j.relays);
}

function iconHtml(iconId) {
  const ic = ICONS[iconId] || ICONS[0];
  if (!ic) return '';
  return `<i class="fas fa-${ic.fa}"></i>`;
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
        <div class="relay-name">
          <div class="relay-icon" data-icon-i="${i}" title="Pick icon" data-icon-id="${r.icon||0}">${iconHtml(r.icon||0)}</div>
          <strong>Relay ${i+1}</strong>
        </div>
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
  // Wire up icon-cell clicks
  document.querySelectorAll('.relay-icon[data-icon-i]').forEach(el => {
    el.addEventListener('click', () => openIconModal(parseInt(el.dataset.iconI)));
  });
}

// -- Icon picker modal ---------------------------------------------------
function openIconModal(relayIdx) {
  CUR_ICON_RELAY = relayIdx;
  const grid = document.getElementById('iconGrid');
  const currentIcon = (CUR_RELAYS[relayIdx] && CUR_RELAYS[relayIdx].icon) || 0;
  grid.innerHTML = '';
  ICONS.forEach(ic => {
    const cell = document.createElement('div');
    cell.className = 'icon-cell' + (ic.id === currentIcon ? ' selected' : '');
    cell.innerHTML = `<span class="glyph"><i class="fas fa-${ic.fa}"></i></span>
                      <span class="lbl">${ic.label}</span>`;
    cell.addEventListener('click', () => chooseIcon(ic.id));
    grid.appendChild(cell);
  });
  document.getElementById('iconModal').classList.add('open');
}
function closeIconModal() { document.getElementById('iconModal').classList.remove('open'); }

async function chooseIcon(iconId) {
  if (CUR_ICON_RELAY < 0) return;
  // Reflect immediately on the page, then persist.
  const swatch = document.querySelector(`.relay-icon[data-icon-i="${CUR_ICON_RELAY}"]`);
  if (swatch) {
    swatch.innerHTML = iconHtml(iconId);
    swatch.dataset.iconId = String(iconId);
  }
  if (CUR_RELAYS[CUR_ICON_RELAY]) CUR_RELAYS[CUR_ICON_RELAY].icon = iconId;
  closeIconModal();
  // Save just the icon — keep it minimal so we don't fight with the
  // "Save relays" button for other dirty fields.
  const body = {};
  body[CUR_ICON_RELAY] = { icon: iconId };
  await fetch('/api/relays', {method:'POST', body: JSON.stringify(body), headers:{'Content-Type':'application/json'}});
}

// -- WiFi / Display / Relays save --------------------------------------
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
  // Carry along the current icon — the icon picker writes it asynchronously
  // but the user might also "Save relays" before it round-trips.
  document.querySelectorAll('.relay-icon[data-icon-i]').forEach(el => {
    const i = el.dataset.iconI;
    if (!data[i]) data[i] = {};
    data[i].icon = parseInt(el.dataset.iconId || '0');
  });
  await fetch('/api/relays', {method:'POST', body: JSON.stringify(data), headers:{'Content-Type':'application/json'}});
  loadState();
}

loadState();
loadPrices();
scanWifi();
setInterval(loadState, 10000);
setInterval(loadPrices, 5 * 60 * 1000);  // refresh chart every 5 min
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
        o["icon"] = (int)r.icon;
    }

    // Provide the icon catalog so the web UI can render a picker.
    JsonArray icons = doc["icon_catalog"].to<JsonArray>();
    for (int i = 0; i < ICON_COUNT; ++i) {
        JsonObject o = icons.add<JsonObject>();
        o["id"]    = i;
        o["label"] = g_iconCatalog[i].label;
        o["fa"]    = g_iconCatalog[i].fa;
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

    // Hourly price list — used by the web chart and the "show all prices"
    // table. Separate from /api/state because it changes only every 30 min
    // (vs. every 10 s for state) and is larger (~1.5 KB).
    _server.on("/api/prices", HTTP_GET, [](AsyncWebServerRequest* req) {
        extern class PriceData g_prices;
        JsonDocument doc;
        doc["count"] = g_prices.count();
        doc["updated"] = (uint32_t)g_prices.lastUpdate();
        JsonArray arr = doc["entries"].to<JsonArray>();
        for (int i = 0; i < g_prices.count(); ++i) {
            JsonObject o = arr.add<JsonObject>();
            o["ts"]    = (uint32_t)g_prices.at(i).ts_start;
            o["price"] = g_prices.at(i).price_raw;
        }
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
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
                if (o["icon"].is<int>()) {
                    int ic = o["icon"];
                    if (ic >= 0 && ic < ICON_COUNT) r.icon = (uint8_t)ic;
                }
            }
            _relays.save();
            _ui.redraw();
            req->send(200, "text/plain", "OK");
        });

    _server.on("/api/debug-fetch", HTTP_GET, [this](AsyncWebServerRequest* req) {
        // Synchronous fetch from e-cena.lv for diagnostic purposes. Reports
        // result back to the browser as plain text.
        String report;
        report.reserve(2048);
        report += "Price source diagnostic\n";
        report += "==========================\n\n";

        if (!_net.isConnected()) {
            report += "WiFi: NOT CONNECTED\n";
            req->send(200, "text/plain; charset=utf-8", report);
            return;
        }
        report += "WiFi: connected, IP " + _net.ipString() + "\n";
        report += "Source: " ECENA_URL "\n\n";

        unsigned long t0 = millis();
        String html;
        bool fetchOk = _net.fetchEcenaHtml(html);
        unsigned long fetchMs = millis() - t0;

        report += "Fetch: " + String(fetchOk ? "SUCCESS" : "FAILED") + "\n";
        report += "Time:  " + String(fetchMs) + " ms\n";
        report += "Bytes: " + String(html.length()) + "\n\n";

        if (fetchOk && html.length() > 0) {
            // Show a small preview so we can spot HTML format changes.
            report += "First 300 chars of HTML:\n";
            report += "---------------------------------\n";
            report += html.substring(0, 300);
            report += "\n---------------------------------\n\n";

            extern class PriceData g_prices;
            t0 = millis();
            int n = g_prices.parseEcenaHtml(html);
            report += "Parsed in " + String(millis() - t0) + " ms\n";
            report += "Hourly entries stored: " + String(n) + "\n";

            if (n > 0) {
                report += "\nFirst 5 entries:\n";
                for (int i = 0; i < 5 && i < n; ++i) {
                    time_t ts = g_prices.at(i).ts_start;
                    struct tm t; localtime_r(&ts, &t);
                    char buf[80];
                    snprintf(buf, sizeof(buf),
                             "  %04d-%02d-%02d %02d:00  =  %.4f EUR/kWh\n",
                             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                             t.tm_hour, g_prices.at(i).price_raw);
                    report += buf;
                }
            }
        }

        req->send(200, "text/plain; charset=utf-8", report);
    });

    _server.onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "text/plain", "Not found");
    });

    _server.begin();
    Serial.println("[WEB] HTTP server started on port 80");
}
