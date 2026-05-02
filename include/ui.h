// ui.h — LVGL dashboard for the JC4827W543 480×272 panel.
#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include "price_data.h"
#include "relay_controller.h"

class UI {
public:
    UI(PriceData& prices, RelayController& relays);
    void begin();

    // Push fresh state from price feed / clock / wifi into LVGL widgets.
    // Cheap to call repeatedly; widgets only redraw if something changed.
    void refresh();

    // Force a full rebuild of all three screens. Call this after settings
    // changes that affect static labels (language, VAT toggle, etc.).
    // Doing this on the periodic timer would be wasteful — only call from
    // event handlers that flip those settings.
    void redraw();

    void setWifiConnected(bool c);
    void setIpString(const String& ip);
    void setConfigMode(bool c);

    void setShowVat(bool v);
    void setVatPercent(float v);
    bool  showVat() const     { return _showVat; }
    float vatPercent() const  { return _vat; }

private:
    PriceData&       _prices;
    RelayController& _relays;

    bool _wifi = false;
    bool _configMode = false;
    bool _showVat = true;
    float _vat = 21.0f;
    String _ip = "";

    // ---- Screens ----
    lv_obj_t* _scrMain      = nullptr;
    lv_obj_t* _scrRelays    = nullptr;
    lv_obj_t* _scrEdit      = nullptr;
    lv_obj_t* _scrIconPick  = nullptr;
    lv_obj_t* _scrFullChart = nullptr;
    int       _editIdx      = 0;

    // ---- Main screen widgets ----
    lv_obj_t* _hdrTitle    = nullptr;
    lv_obj_t* _hdrStatus   = nullptr;   // small dot
    lv_obj_t* _hdrIp       = nullptr;
    lv_obj_t* _hdrClock    = nullptr;
    lv_obj_t* _hdrDate     = nullptr;

    lv_obj_t* _cardPrev    = nullptr;
    lv_obj_t* _cardCur     = nullptr;
    lv_obj_t* _cardNext    = nullptr;
    lv_obj_t* _lblPrev     = nullptr;
    lv_obj_t* _lblCur      = nullptr;
    lv_obj_t* _lblNext     = nullptr;

    lv_obj_t* _chart       = nullptr;
    lv_chart_series_t* _chartSeries = nullptr;
    lv_obj_t* _chartUnits  = nullptr;
    lv_obj_t* _chartNowMarker = nullptr;     // thin light-blue vertical line — current time
    lv_obj_t* _chartHourLbls[8] = {};        // "06", "12", "18", "00", "06", "12", "18", "00" labels under the 6h grid lines
    lv_obj_t* _chartAvgLine = nullptr;       // dashed average line overlay
    float     _chartMin = 0, _chartMax = 0, _chartAvg = 0;
    int       _chartFirstIdx = -1;           // first price entry shown in chart
    int       _chartLastIdx  = -1;
    int       _chartNowIdx   = -1;
    lv_obj_t* _chartCard   = nullptr;        // parent card of _chart — used for absolute positioning of overlays

    lv_obj_t* _statAvg     = nullptr;
    lv_obj_t* _statMin     = nullptr;
    lv_obj_t* _statMax     = nullptr;
    lv_obj_t* _statUpd     = nullptr;

    String _lastClock;
    int    _lastNowIdx = -1;

    // ---- Relay screen widgets ----
    struct RelayTile {
        lv_obj_t* card;
        lv_obj_t* icon;
        lv_obj_t* name;
        lv_obj_t* state;
        lv_obj_t* mode;
        lv_obj_t* threshold;
    };
    RelayTile _tiles[NUM_RELAYS] = {};

    // ---- Edit screen widgets ----
    lv_obj_t* _editTitle   = nullptr;
    lv_obj_t* _modeBtns[3] = {};                   // ALWAYS_OFF, ALWAYS_ON, AUTO
    lv_obj_t* _editOnBelowVal  = nullptr;          // value label for the ON_BELOW spinner
    lv_obj_t* _editOffAboveVal = nullptr;          // value label for the OFF_ABOVE spinner
    lv_obj_t* _editOnBelowRow  = nullptr;          // container for ON_BELOW spinner — hidden in non-AUTO modes
    lv_obj_t* _editOffAboveRow = nullptr;          // container for OFF_ABOVE spinner — hidden in non-AUTO modes
    lv_obj_t* _editHysteresisLbl = nullptr;        // small caption that explains the gap (or shows "no hysteresis")
    lv_obj_t* _editIconBtn    = nullptr;     // tap to open icon picker
    lv_obj_t* _editIconLabel  = nullptr;     // shows the chosen icon glyph

    // ---- Icon picker screen widgets ----
    lv_obj_t* _iconGrid       = nullptr;

    // ---- Fullscreen chart screen widgets ----
    lv_obj_t* _fullChart       = nullptr;
    lv_chart_series_t* _fullChartSeries = nullptr;
    lv_obj_t* _fullAvgLine     = nullptr;
    lv_obj_t* _fullChartTitle  = nullptr;
    lv_obj_t* _fullChartTip    = nullptr;    // floating "HH:00 — N.NN c/kWh"
    lv_obj_t* _fullYAxis       = nullptr;
    lv_obj_t* _fullXAxis       = nullptr;
    int       _fullFirstIdx    = -1;
    int       _fullLastIdx     = -1;
    float     _fullMin = 0, _fullMax = 0, _fullAvg = 0;

    // ---- Construction helpers ----
    void buildMain();
    void buildRelays();
    void buildEdit();
    void buildIconPick();
    void buildFullChart();

    void refreshHeader();
    void refreshPriceCards();
    void refreshChart();
    void refreshChartOverlays();   // place "now" line + 6h hour labels (cheap, called every refresh tick)
    void refreshStats();
    void refreshRelayTiles();
    void refreshEdit();
    void refreshFullChart();

    String currentTimeString() const;
    String dateString() const;
    String formatPrice(float raw) const;
    String unitsLabel() const;
    lv_color_t colorForPrice(float p, float mn, float mx) const;

    // ---- Event handlers (static thunks) ----
    static void onMainTouched(lv_event_t* e);
    static void onChartTouched(lv_event_t* e);          // open fullscreen
    static void onChartDrawPart(lv_event_t* e);         // per-segment color
    static void onChartDrawGrid(lv_event_t* e);         // 6h vertical grid lines
    static void onFullChartDrawPart(lv_event_t* e);
    static void onFullChartDrawGrid(lv_event_t* e);     // 6h vertical grid lines (full)
    static void onFullChartTouched(lv_event_t* e);      // tap-to-inspect
    static void onFullChartBack(lv_event_t* e);
    static void onRelayTileTouched(lv_event_t* e);
    static void onBackToMainTouched(lv_event_t* e);
    static void onBackToRelaysTouched(lv_event_t* e);
    static void onModeBtnTouched(lv_event_t* e);
    static void onMinusTouched(lv_event_t* e);
    static void onPlusTouched(lv_event_t* e);
    static void onIconBtnTouched(lv_event_t* e);        // open icon picker
    static void onIconChosen(lv_event_t* e);
    static void onIconPickBack(lv_event_t* e);

    void openRelays();
    void openEdit(int idx);
    void openMain();
    void openIconPick();
    void openFullChart();
};
