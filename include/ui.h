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
    lv_obj_t* _scrMain     = nullptr;
    lv_obj_t* _scrRelays   = nullptr;
    lv_obj_t* _scrEdit     = nullptr;
    int       _editIdx     = 0;

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
    lv_obj_t* _chartNowMarker = nullptr;

    lv_obj_t* _statAvg     = nullptr;
    lv_obj_t* _statMin     = nullptr;
    lv_obj_t* _statMax     = nullptr;
    lv_obj_t* _statUpd     = nullptr;

    String _lastClock;
    int    _lastNowIdx = -1;

    // ---- Relay screen widgets ----
    struct RelayTile {
        lv_obj_t* card;
        lv_obj_t* name;
        lv_obj_t* state;
        lv_obj_t* mode;
        lv_obj_t* threshold;
    };
    RelayTile _tiles[NUM_RELAYS] = {};

    // ---- Edit screen widgets ----
    lv_obj_t* _editTitle   = nullptr;
    lv_obj_t* _modeBtns[4] = {};
    lv_obj_t* _editValueLabel = nullptr;

    // ---- Construction helpers ----
    void buildMain();
    void buildRelays();
    void buildEdit();

    void refreshHeader();
    void refreshPriceCards();
    void refreshChart();
    void refreshStats();
    void refreshRelayTiles();
    void refreshEdit();

    String currentTimeString() const;
    String dateString() const;
    String formatPrice(float raw) const;
    String unitsLabel() const;
    lv_color_t colorForPrice(float p, float mn, float mx) const;

    // ---- Event handlers (static thunks) ----
    static void onMainTouched(lv_event_t* e);
    static void onRelayTileTouched(lv_event_t* e);
    static void onBackToMainTouched(lv_event_t* e);
    static void onBackToRelaysTouched(lv_event_t* e);
    static void onModeBtnTouched(lv_event_t* e);
    static void onMinusTouched(lv_event_t* e);
    static void onPlusTouched(lv_event_t* e);

    void openRelays();
    void openEdit(int idx);
    void openMain();
};
