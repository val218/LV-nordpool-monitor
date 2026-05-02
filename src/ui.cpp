#include "ui.h"
#include "i18n.h"
#include "np_fonts.h"
#include "relay_icons.h"
#include <time.h>

// LVGL color helpers
static lv_color_t COL_BG;
static lv_color_t COL_CARD;
static lv_color_t COL_TEXT;
static lv_color_t COL_DIM;
static lv_color_t COL_GREEN;
static lv_color_t COL_YELLOW;
static lv_color_t COL_ORANGE;
static lv_color_t COL_RED;
static lv_color_t COL_NOWBAR;

static void initColors() {
    COL_BG     = lv_color_hex(0x101218);
    COL_CARD   = lv_color_hex(0x1E2229);
    COL_TEXT   = lv_color_hex(0xFFFFFF);
    COL_DIM    = lv_color_hex(0x8A8F99);
    COL_GREEN  = lv_color_hex(0x4CC54E);
    COL_YELLOW = lv_color_hex(0xEDD400);
    COL_ORANGE = lv_color_hex(0xFD9728);
    COL_RED    = lv_color_hex(0xE34B4B);
    COL_NOWBAR = lv_color_hex(0x55C8FF);
}

// ---- Helpers --------------------------------------------------------

UI::UI(PriceData& prices, RelayController& relays)
    : _prices(prices), _relays(relays) {}

String UI::currentTimeString() const {
    time_t now = time(nullptr);
    if (now < 100000) return "--:--";
    struct tm t; localtime_r(&now, &t);
    char buf[8]; snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    return String(buf);
}

String UI::dateString() const {
    time_t now = time(nullptr);
    if (now < 100000) return "";
    struct tm t; localtime_r(&now, &t);
    static const char* MON[12] = {"Jan","Feb","Mar","Apr","May","Jun",
                                   "Jul","Aug","Sep","Oct","Nov","Dec"};
    char buf[12]; snprintf(buf, sizeof(buf), "%d %s", t.tm_mday, MON[t.tm_mon]);
    return String(buf);
}

String UI::formatPrice(float raw) const {
    if (isnan(raw)) return "--";
    float p = _showVat ? raw * (1.0f + _vat / 100.0f) : raw;
    char b[16]; snprintf(b, sizeof(b), "%.1f", p * 100.0f);
    return String(b);
}

String UI::unitsLabel() const {
    return _showVat ? "c/kWh+VAT" : "c/kWh";
}

lv_color_t UI::colorForPrice(float p, float mn, float mx) const {
    if (mx <= mn) return COL_GREEN;
    float t = (p - mn) / (mx - mn);
    if (t < 0) t = 0; if (t > 1) t = 1;
    if (t < 0.33f)      return COL_GREEN;
    else if (t < 0.60f) return COL_YELLOW;
    else if (t < 0.85f) return COL_ORANGE;
    else                return COL_RED;
}

// Convenience: a dark "card" container with rounded corners, no border.
static lv_obj_t* makeCard(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* c = lv_obj_create(parent);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_size(c, w, h);
    lv_obj_set_style_bg_color(c, COL_CARD, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_radius(c, 6, 0);
    lv_obj_set_style_pad_all(c, 4, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

static lv_obj_t* makeLabel(lv_obj_t* parent, const char* text,
                           lv_color_t col, const lv_font_t* font) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_color(l, col, 0);
    lv_obj_set_style_text_font(l, font, 0);
    return l;
}

// ---- Build & lifecycle ----------------------------------------------

void UI::begin() {
    initColors();

    // Tell LVGL's default theme to use our dark background.
    lv_disp_t* disp = lv_disp_get_default();
    lv_theme_t* th = lv_theme_default_init(
        disp, lv_palette_main(LV_PALETTE_BLUE),
              lv_palette_main(LV_PALETTE_LIGHT_BLUE),
              true,                              // dark
              LV_FONT_DEFAULT);
    lv_disp_set_theme(disp, th);

    buildMain();
    buildRelays();
    buildEdit();
    buildIconPick();
    buildFullTable();
    openMain();
}

// ---------- MAIN SCREEN ----------------------------------------------
//   y=0   : header (28 px)
//   y=30  : price cards row (74 px)
//   y=108 : chart (100 px)
//   y=212 : footer stats (56 px)
//   total = 268 / 272

void UI::buildMain() {
    _scrMain = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_scrMain, COL_BG, 0);
    lv_obj_set_style_bg_opa(_scrMain, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_scrMain, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(_scrMain, 0, 0);

    // Tap anywhere outside the chart on the main screen → relay screen.
    // We attach a click handler at the screen level; the chart absorbs taps,
    // so it acts as a natural exclusion zone.
    lv_obj_add_event_cb(_scrMain, onMainTouched, LV_EVENT_CLICKED, this);

    // ---- Header ----
    _hdrTitle  = makeLabel(_scrMain, T(S_TITLE), COL_TEXT, FONT_14);
    lv_obj_align(_hdrTitle, LV_ALIGN_TOP_LEFT, 6, 6);
    // Force a layout pass so we can read the actual rendered width of the
    // title below. Without this, lv_obj_get_width() returns 0 because the
    // label hasn't been measured yet.
    lv_obj_update_layout(_hdrTitle);

    _hdrStatus = lv_obj_create(_scrMain);
    lv_obj_set_size(_hdrStatus, 8, 8);
    lv_obj_set_style_radius(_hdrStatus, 4, 0);
    lv_obj_set_style_bg_color(_hdrStatus, COL_RED, 0);
    lv_obj_set_style_bg_opa(_hdrStatus, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_hdrStatus, 0, 0);
    // Anchor the status dot to the right edge of the title with a small gap.
    // Previously it was at a fixed x=130 which overlapped "Nordpool Monitor"
    // at FONT_14 (~155 px wide). Anchoring to the title makes the layout
    // robust to translation length differences.
    lv_obj_align_to(_hdrStatus, _hdrTitle, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    lv_obj_clear_flag(_hdrStatus, LV_OBJ_FLAG_SCROLLABLE);

    _hdrIp = makeLabel(_scrMain, "", COL_DIM, FONT_12);
    // IP label sits to the right of the status dot.
    lv_obj_align_to(_hdrIp, _hdrStatus, LV_ALIGN_OUT_RIGHT_MID, 6, 0);

    _hdrClock = makeLabel(_scrMain, "--:--", COL_TEXT, FONT_14);
    lv_obj_align(_hdrClock, LV_ALIGN_TOP_RIGHT, -6, 6);

    _hdrDate = makeLabel(_scrMain, "", COL_DIM, FONT_12);
    lv_obj_align(_hdrDate, LV_ALIGN_TOP_RIGHT, -54, 9);

    // ---- Price cards row (1:2:1) ----
    int rowY = 30, rowH = 74;
    int padX = 6;
    int unitW = (DISP_W - 4 * padX) / 4;
    int sideW = unitW;
    int curW  = unitW * 2;

    int prevX = padX;
    int curX  = prevX + sideW + padX;
    int nextX = curX + curW + padX;

    auto buildCard = [&](lv_obj_t** card, lv_obj_t** valLbl,
                         int x, int y, int w, int h,
                         const char* title, const lv_font_t* valFont) {
        *card = makeCard(_scrMain, x, y, w, h);
        lv_obj_t* t = makeLabel(*card, title, COL_DIM, FONT_12);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 4, 2);

        *valLbl = makeLabel(*card, "--", COL_TEXT, valFont);
        lv_obj_center(*valLbl);

        lv_obj_t* u = makeLabel(*card, unitsLabel().c_str(),
                                COL_DIM, FONT_12);
        lv_obj_align(u, LV_ALIGN_BOTTOM_MID, 0, -1);
    };

    buildCard(&_cardPrev, &_lblPrev, prevX, rowY, sideW, rowH,
              T(S_PREV), FONT_24);
    buildCard(&_cardCur,  &_lblCur,  curX,  rowY, curW,  rowH,
              T(S_CURRENT), FONT_40);
    buildCard(&_cardNext, &_lblNext, nextX, rowY, sideW, rowH,
              T(S_NEXT), FONT_24);

    // ---- Chart ----
    int chY = 108, chH = 100;
    _chartCard = makeCard(_scrMain, 4, chY, DISP_W - 8, chH);

    _chart = lv_chart_create(_chartCard);
    lv_obj_set_size(_chart, DISP_W - 16, chH - 8);
    lv_obj_align(_chart, LV_ALIGN_CENTER, 0, 0);
    // Line chart, not bars — looks like the e-cena reference and lets
    // us color individual segments by price tier via a draw callback.
    lv_chart_set_type(_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(_chart, 48);
    // Disable LVGL's built-in division lines (the faint gray "fuzz") AND
    // its built-in axis tick rendering. We draw our own crisp 6-hour
    // vertical lines in onChartDrawGrid and we lay out our own time
    // labels as ordinary lv_label children of the chart card — that way
    // both lines and labels share the same X math and align perfectly.
    lv_chart_set_div_line_count(_chart, 0, 0);
    lv_chart_set_axis_tick(_chart, LV_CHART_AXIS_PRIMARY_X, 0, 0, 0, 0, false, 0);
    lv_obj_set_style_bg_opa(_chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_chart, 0, 0);
    lv_obj_set_style_pad_all(_chart, 0, 0);
    // Reserve room at the bottom for our hour labels (height ~12 px + 2 px gap).
    lv_obj_set_style_pad_bottom(_chart, 14, LV_PART_MAIN);
    lv_obj_set_style_size(_chart, 0, LV_PART_INDICATOR);  // hide point dots
    lv_obj_set_style_line_width(_chart, 2, LV_PART_ITEMS);
    _chartSeries = lv_chart_add_series(_chart, COL_GREEN, LV_CHART_AXIS_PRIMARY_Y);

    // Per-segment color (green→yellow→orange→red by price tier).
    lv_obj_add_event_cb(_chart, onChartDrawPart,
                        LV_EVENT_DRAW_PART_BEGIN, this);
    // Custom 6-hour vertical grid lines, drawn AFTER everything else so
    // they sit on top of the plot area (similar to the web UI).
    lv_obj_add_event_cb(_chart, onChartDrawGrid,
                        LV_EVENT_DRAW_POST_END, this);
    // Tap chart → fullscreen view. The chart absorbs taps so its parent
    // card needs to be the actual click target.
    lv_obj_add_flag(_chart, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_chart, onChartTouched, LV_EVENT_CLICKED, this);

    _chartUnits = makeLabel(_chartCard, unitsLabel().c_str(),
                            COL_DIM, FONT_12);
    lv_obj_align(_chartUnits, LV_ALIGN_TOP_LEFT, 2, 0);

    // Hour labels under the 6-hour grid lines. We pre-create 8 of them and
    // position them later (in refreshChartOverlays) using the SAME math the
    // grid lines use, so they stay perfectly aligned. Hours shown:
    //   06, 12, 18, 00, 06, 12, 18, 00  (the last "00" sits at hour 48
    //   which is the right edge of the chart and is clipped/skipped).
    static const char* kHourLabels[8] = { "06","12","18","00","06","12","18","00" };
    for (int i = 0; i < 8; ++i) {
        _chartHourLbls[i] = makeLabel(_chartCard, kHourLabels[i],
                                       COL_DIM, FONT_12);
        // Provisional position; refreshChartOverlays() places them properly.
        lv_obj_set_pos(_chartHourLbls[i], 0, chH - 14);
    }

    // "Now" indicator — a 2-pixel-wide vertical line in COL_NOWBAR (light
    // blue) drawn on top of the chart card. Updated each refresh.
    _chartNowMarker = lv_obj_create(_chartCard);
    lv_obj_set_size(_chartNowMarker, 2, chH - 18);
    lv_obj_set_style_bg_color(_chartNowMarker, COL_NOWBAR, 0);
    lv_obj_set_style_bg_opa(_chartNowMarker, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_chartNowMarker, 0, 0);
    lv_obj_set_style_radius(_chartNowMarker, 0, 0);
    lv_obj_set_style_pad_all(_chartNowMarker, 0, 0);
    lv_obj_clear_flag(_chartNowMarker, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(_chartNowMarker, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(_chartNowMarker, 0, 4);
    lv_obj_add_flag(_chartNowMarker, LV_OBJ_FLAG_HIDDEN);    // shown once we have data

    // ---- Footer stats (4 mini cards) ----
    int ftY = 212, ftH = 56;
    int fW = (DISP_W - 5 * padX) / 4;

    auto buildStat = [&](lv_obj_t** lbl, int idx, const char* title) {
        int x = padX + idx * (fW + padX);
        lv_obj_t* c = makeCard(_scrMain, x, ftY, fW, ftH);
        lv_obj_t* t = makeLabel(c, title, COL_DIM, FONT_12);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 2, 1);
        *lbl = makeLabel(c, "--", COL_TEXT, FONT_20);
        lv_obj_align(*lbl, LV_ALIGN_BOTTOM_MID, 0, -2);
    };
    buildStat(&_statAvg, 0, T(S_PRICES_TODAY));
    buildStat(&_statMin, 1, T(S_MIN_PRICE));
    buildStat(&_statMax, 2, T(S_MAX_PRICE));
    buildStat(&_statUpd, 3, T(S_LAST_UPDATE));
}

// ---------- RELAY GRID SCREEN ----------------------------------------

void UI::buildRelays() {
    _scrRelays = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_scrRelays, COL_BG, 0);
    lv_obj_set_style_bg_opa(_scrRelays, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_scrRelays, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(_scrRelays, 0, 0);

    // Header strip
    lv_obj_t* title = makeLabel(_scrRelays, T(S_RELAYS),
                                COL_TEXT, FONT_16);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 6);

    lv_obj_t* back = makeCard(_scrRelays, DISP_W - 80, 4, 72, 24);
    lv_obj_set_style_radius(back, 12, 0);
    lv_obj_t* bl = makeLabel(back, T(S_BACK), COL_TEXT, FONT_12);
    lv_obj_center(bl);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(back, onBackToMainTouched, LV_EVENT_CLICKED, this);

    // 4×2 grid for 8 relays. With DISP_W=480, DISP_H=272 minus 36px header,
    // each tile is ~115×115 — finger-friendly.
    int top = 36;
    int gridH = DISP_H - top - 4;
    int cols = 4, rows = 2;
    int padX = 6, padY = 6;
    int tileW = (DISP_W - padX * (cols + 1)) / cols;
    int tileH = (gridH - padY * (rows + 1)) / rows;

    for (int i = 0; i < NUM_RELAYS; ++i) {
        int r = i / cols, c = i % cols;
        int tx = padX + c * (tileW + padX);
        int ty = top + r * (tileH + padY);

        RelayTile& tt = _tiles[i];
        tt.card = makeCard(_scrRelays, tx, ty, tileW, tileH);
        lv_obj_add_flag(tt.card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(tt.card, (void*)(intptr_t)i);
        lv_obj_add_event_cb(tt.card, onRelayTileTouched, LV_EVENT_CLICKED, this);

        // Icon glyph in the centre, displayed large.
        // Uses the dedicated icon font when available (FontAwesome-derived)
        // so the glyphs render properly. Falls back to text-font sized 28
        // showing the ASCII abbreviation.
        tt.icon = makeLabel(tt.card, "?", COL_TEXT, ICON_FONT_40);
        lv_obj_align(tt.icon, LV_ALIGN_CENTER, 0, -4);

        tt.name = makeLabel(tt.card, "Relay", COL_TEXT, FONT_12);
        lv_obj_align(tt.name, LV_ALIGN_TOP_LEFT, 2, 2);

        tt.state = makeLabel(tt.card, "OFF", COL_DIM, FONT_12);
        lv_obj_align(tt.state, LV_ALIGN_TOP_RIGHT, -2, 2);

        tt.mode = makeLabel(tt.card, "", COL_DIM, FONT_12);
        lv_obj_align(tt.mode, LV_ALIGN_BOTTOM_LEFT, 2, -16);

        tt.threshold = makeLabel(tt.card, "", COL_TEXT, FONT_12);
        lv_obj_align(tt.threshold, LV_ALIGN_BOTTOM_RIGHT, -2, -2);
    }
}

// ---------- EDIT SCREEN ----------------------------------------------

// ===================================================================
// EDIT SCREEN
// ===================================================================
//
// Layout (480 x 272 panel):
//
//   +-------------------------------------------------------+ y=0
//   | Relay N name                            [icon] [Back] |  header (32 px tall)
//   +-------------------------------------------------------+ y=36
//   |  [Always OFF]   [Always ON]      [Auto]               |  3 mode buttons (40 px tall)
//   +-------------------------------------------------------+ y=80
//   |  ON below                                             |  ON-BELOW row (44 px)
//   |  [ - ]    NN.N c/kWh                       [ + ]      |
//   +-------------------------------------------------------+ y=130
//   |  OFF above                                            |  OFF-ABOVE row (44 px)
//   |  [ - ]    NN.N c/kWh                       [ + ]      |
//   +-------------------------------------------------------+ y=180
//   |  Hysteresis: 2.0 c/kWh                                |  hint (only visible in AUTO mode)
//   +-------------------------------------------------------+ y=272
//
// In ALWAYS_OFF / ALWAYS_ON modes the two threshold rows and the
// hysteresis hint are hidden so the screen stays uncluttered.
//
// Each spinner button carries a small integer in its user_data:
//   0 = adjust on_below
//   1 = adjust off_above
// onMinus/onPlusTouched read this tag to dispatch.

void UI::buildEdit() {
    _scrEdit = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_scrEdit, COL_BG, 0);
    lv_obj_set_style_bg_opa(_scrEdit, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_scrEdit, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(_scrEdit, 0, 0);

    _editTitle = makeLabel(_scrEdit, "Relay 1", COL_TEXT, FONT_16);
    lv_obj_align(_editTitle, LV_ALIGN_TOP_LEFT, 8, 6);

    // Icon picker button — shows current icon, tapping opens the picker.
    _editIconBtn = makeCard(_scrEdit, DISP_W - 80 - 38, 4, 30, 24);
    lv_obj_set_style_radius(_editIconBtn, 12, 0);
    lv_obj_add_flag(_editIconBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_editIconBtn, onIconBtnTouched, LV_EVENT_CLICKED, this);
    _editIconLabel = makeLabel(_editIconBtn, "?", COL_TEXT, ICON_FONT_24);
    lv_obj_center(_editIconLabel);

    lv_obj_t* back = makeCard(_scrEdit, DISP_W - 80, 4, 72, 24);
    lv_obj_set_style_radius(back, 12, 0);
    lv_obj_t* bl = makeLabel(back, T(S_BACK), COL_TEXT, FONT_12);
    lv_obj_center(bl);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(back, onBackToRelaysTouched, LV_EVENT_CLICKED, this);

    // ---- 4 mode buttons across the top ----
    // Order chosen for ascending complexity: OFF -> ON -> PRICE (single
    // threshold, no hysteresis) -> AUTO (two thresholds with hysteresis).
    const char* labels[4] = { T(S_ALWAYS_OFF), T(S_ALWAYS_ON),
                              T(S_PRICE),       T(S_AUTO) };
    int btnY = 36, btnH = 40;
    int gap = 6;
    int btnW = (DISP_W - 5 * gap) / 4;
    for (int m = 0; m < 4; ++m) {
        int bx = gap + m * (btnW + gap);
        lv_obj_t* b = makeCard(_scrEdit, bx, btnY, btnW, btnH);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(b, (void*)(intptr_t)m);
        lv_obj_add_event_cb(b, onModeBtnTouched, LV_EVENT_CLICKED, this);
        lv_obj_t* l = makeLabel(b, labels[m], COL_TEXT, FONT_14);
        lv_obj_center(l);
        _modeBtns[m] = b;
    }

    // ---- Helper to build one spinner row ----
    //
    // Layout (vertical):
    //   y+0  : caption "ON below" / "OFF above" (small label, accent color)
    //   y+18 : [-]  [VALUE]  [+]   spinner controls
    //
    // The caption goes DIRECTLY on the edit screen (not inside the row
    // container) so we avoid any nested-coordinate or z-order surprises.
    // We track both pointers so refreshEdit() can hide/show them as a
    // unit; the wrapper row only owns the spinner widgets.
    struct SpinnerRowPtrs { lv_obj_t* row; lv_obj_t* caption; };
    auto buildSpinnerRow = [&](SpinnerRowPtrs& out, lv_obj_t** valLblOut,
                               int y, int tag,
                               const char* caption, lv_color_t accent) {
        const int capH  = 16;
        const int spinH = 36;
        const int spinW = 48;
        const int gap   = 6;
        const int totalH = capH + spinH;

        // Caption — placed DIRECTLY on the edit screen, above the row.
        out.caption = makeLabel(_scrEdit, caption, accent, FONT_14);
        lv_obj_set_pos(out.caption, 12, y);

        // Spinner row — a transparent container we can hide/show as one
        // unit. Sits below the caption, owns the [-] / value / [+] cards.
        lv_obj_t* row = lv_obj_create(_scrEdit);
        lv_obj_set_pos(row, 0, y + capH);
        lv_obj_set_size(row, DISP_W, spinH + 4);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        out.row = row;

        // [-] button on the left.
        lv_obj_t* minus = makeCard(row, 8, 0, spinW, spinH);
        lv_obj_set_style_radius(minus, 6, 0);
        lv_obj_set_style_border_width(minus, 1, 0);
        lv_obj_set_style_border_color(minus, accent, 0);
        lv_obj_add_flag(minus, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(minus, (void*)(intptr_t)tag);
        lv_obj_add_event_cb(minus, onMinusTouched, LV_EVENT_CLICKED, this);
        lv_obj_t* mlbl = makeLabel(minus, "-", COL_TEXT, FONT_28);
        lv_obj_center(mlbl);

        // [+] button on the far right.
        int xPlus = DISP_W - 8 - spinW;
        lv_obj_t* plus = makeCard(row, xPlus, 0, spinW, spinH);
        lv_obj_set_style_radius(plus, 6, 0);
        lv_obj_set_style_border_width(plus, 1, 0);
        lv_obj_set_style_border_color(plus, accent, 0);
        lv_obj_add_flag(plus, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(plus, (void*)(intptr_t)tag);
        lv_obj_add_event_cb(plus, onPlusTouched, LV_EVENT_CLICKED, this);
        lv_obj_t* plbl = makeLabel(plus, "+", COL_TEXT, FONT_28);
        lv_obj_center(plbl);

        // Value card filling the middle of the row.
        int valX = 8 + spinW + gap;
        int valW = xPlus - valX - gap;
        lv_obj_t* valCard = makeCard(row, valX, 0, valW, spinH);
        *valLblOut = makeLabel(valCard, "-- c/kWh", COL_TEXT, FONT_24);
        lv_obj_center(*valLblOut);

        (void)totalH;   // silence unused (kept for documentation only)
    };

    // Row 1: ON below (green accent — the "turn it on" condition).
    // Mode buttons end at y = 36 + 40 = 76; first row starts at y = 80.
    // Caption (16) + spinner (36) + 4 px gap below = 56 px per row.
    SpinnerRowPtrs row1, row2;
    buildSpinnerRow(row1, &_editOnBelowVal,
                    80, 0, T(S_TURN_ON_BELOW), COL_GREEN);
    _editOnBelowRow     = row1.row;
    _editOnBelowCaption = row1.caption;

    // Row 2: OFF above (red accent — the "turn it off" condition).
    buildSpinnerRow(row2, &_editOffAboveVal,
                    140, 1, T(S_TURN_OFF_ABOVE), COL_RED);
    _editOffAboveRow     = row2.row;
    _editOffAboveCaption = row2.caption;

    // Hysteresis hint at the bottom — visible only when both thresholds
    // exist AND they differ.
    _editHysteresisLbl = makeLabel(_scrEdit, "", COL_DIM, FONT_12);
    lv_obj_align(_editHysteresisLbl, LV_ALIGN_BOTTOM_LEFT, 8, -8);
}

// ---- Refresh (called periodically) ----------------------------------

void UI::refresh() {
    refreshHeader();

    if (lv_scr_act() == _scrMain) {
        refreshPriceCards();
        refreshChart();
        // refreshChartOverlays is also called inside refreshChart(), but
        // calling it here too means the "now" line keeps moving even on
        // ticks where refreshChart() itself early-outs (e.g. no new data).
        refreshChartOverlays();
        refreshStats();
    } else if (lv_scr_act() == _scrRelays) {
        refreshRelayTiles();
    } else if (lv_scr_act() == _scrEdit) {
        refreshEdit();
    }
}

// Rebuild the three screens from scratch. Static labels (titles, card
// captions, button labels) read their text from i18n.h during construction
// and never re-read afterwards, so a language change requires a full
// rebuild to take effect. We remember which screen was active and load it
// again at the end so the user doesn't get bounced back to the main view.
void UI::redraw() {
    lv_obj_t* active = lv_scr_act();
    int wasShowing = 0;   // 0 main, 1 relays, 2 edit
    if (active == _scrRelays) wasShowing = 1;
    else if (active == _scrEdit) wasShowing = 2;

    // Loading a "blank" placeholder screen first lets us safely delete the
    // three real screens without LVGL complaining about removing the
    // currently-loaded one.
    lv_obj_t* placeholder = lv_obj_create(NULL);
    lv_scr_load(placeholder);

    if (_scrMain)      { lv_obj_del(_scrMain);      _scrMain      = nullptr; }
    if (_scrRelays)    { lv_obj_del(_scrRelays);    _scrRelays    = nullptr; }
    if (_scrEdit)      { lv_obj_del(_scrEdit);      _scrEdit      = nullptr; }
    if (_scrIconPick)  { lv_obj_del(_scrIconPick);  _scrIconPick  = nullptr; }
    if (_scrFullChart) { lv_obj_del(_scrFullChart); _scrFullChart = nullptr; }

    // Pointer cleanup — every cached widget pointer now references freed
    // memory. Zero them so the rebuilders can re-populate.
    _hdrTitle = _hdrStatus = _hdrIp = _hdrClock = _hdrDate = nullptr;
    _cardPrev = _cardCur = _cardNext = nullptr;
    _lblPrev = _lblCur = _lblNext = nullptr;
    _chart = nullptr; _chartSeries = nullptr;
    _chartCard = nullptr;
    _chartUnits = _chartNowMarker = _chartAvgLine = nullptr;
    for (int i = 0; i < 8; ++i) _chartHourLbls[i] = nullptr;
    _chartFirstIdx = _chartLastIdx = _chartNowIdx = -1;
    _statAvg = _statMin = _statMax = _statUpd = nullptr;
    for (int i = 0; i < NUM_RELAYS; ++i) _tiles[i] = {};
    _editTitle = _editIconBtn = _editIconLabel = nullptr;
    for (int i = 0; i < 4; ++i) _modeBtns[i] = nullptr;
    _editOnBelowVal = _editOffAboveVal = nullptr;
    _editOnBelowRow = _editOffAboveRow = nullptr;
    _editOnBelowCaption = _editOffAboveCaption = nullptr;
    _editHysteresisLbl = nullptr;
    _iconGrid = nullptr;
    _fullTable = nullptr;
    _scrFullTitle = nullptr;
    _fullFirstIdx = _fullLastIdx = _fullNowIdx = -1;
    _lastClock = "";
    _lastNowIdx = -1;

    buildMain();
    buildRelays();
    buildEdit();
    buildIconPick();
    buildFullTable();

    // Now load the correct screen again
    switch (wasShowing) {
        case 1: lv_scr_load(_scrRelays); break;
        case 2: lv_scr_load(_scrEdit);   break;
        default: lv_scr_load(_scrMain);  break;
    }

    // Drop the placeholder — it's no longer the active screen so this is safe.
    lv_obj_del(placeholder);

    // Push current state into the freshly-built widgets.
    refresh();
}

void UI::refreshHeader() {
    // Status dot color & IP/clock text
    lv_color_t dotCol = _configMode ? COL_ORANGE
                                    : (_wifi ? COL_GREEN : COL_RED);
    lv_obj_set_style_bg_color(_hdrStatus, dotCol, 0);

    String ipStr = _configMode ? String(T(S_CONFIG_MODE)) + " " + _ip
                               : (_wifi ? _ip : String(T(S_OFFLINE)));
    lv_label_set_text(_hdrIp, ipStr.c_str());

    String clk = currentTimeString();
    if (clk != _lastClock) {
        lv_label_set_text(_hdrClock, clk.c_str());
        _lastClock = clk;
    }
    lv_label_set_text(_hdrDate, dateString().c_str());
}

void UI::refreshPriceCards() {
    time_t now = time(nullptr);
    const PriceEntry* prv = _prices.getPrevious(now);
    const PriceEntry* cur = _prices.getCurrent(now);
    const PriceEntry* nxt = _prices.getNext(now);

    float dMin = 0, dMax = 0, dAvg = 0;
    bool hasDay = _prices.statsForDay(now, dMin, dMax, dAvg);

    auto setVal = [&](lv_obj_t* lbl, const PriceEntry* e) {
        if (!e) { lv_label_set_text(lbl, "--");
                  lv_obj_set_style_text_color(lbl, COL_TEXT, 0); return; }
        lv_label_set_text(lbl, formatPrice(e->price_raw).c_str());
        lv_color_t c = hasDay ? colorForPrice(e->price_raw, dMin, dMax) : COL_TEXT;
        lv_obj_set_style_text_color(lbl, c, 0);
    };
    setVal(_lblPrev, prv);
    setVal(_lblCur,  cur);
    setVal(_lblNext, nxt);
}

void UI::refreshChart() {
    int n = _prices.count();
    if (n == 0) return;

    // Build a 48-entry window starting at start-of-today
    time_t now = time(nullptr);
    struct tm tnow; localtime_r(&now, &tnow);
    tnow.tm_hour = 0; tnow.tm_min = 0; tnow.tm_sec = 0;
    time_t todayStart = mktime(&tnow);

    int first = -1, last = -1;
    for (int i = 0; i < n; ++i) {
        time_t ts = _prices.at(i).ts_start;
        if (ts >= todayStart && ts < todayStart + 48 * 3600) {
            if (first < 0) first = i;
            last = i;
        }
    }
    if (first < 0) return;

    float mn = _prices.at(first).price_raw, mx = mn, sum = 0;
    int cnt = 0;
    for (int i = first; i <= last; ++i) {
        float p = _prices.at(i).price_raw;
        if (p < mn) mn = p;
        if (p > mx) mx = p;
        sum += p; cnt++;
    }
    float avg = cnt > 0 ? sum / cnt : 0;
    // Cache for the draw callback
    _chartFirstIdx = first;
    _chartLastIdx  = last;
    _chartMin = mn; _chartMax = mx; _chartAvg = avg;

    float lo = mn < 0 ? mn * 1.05f : 0;
    float hi = mx * 1.05f;
    if (hi - lo < 0.01f) hi = lo + 0.01f;

    int yMin = (int)(lo * 100000.0f);   // EUR/kWh × 100000
    int yMax = (int)(hi * 100000.0f);
    if (yMax <= yMin) yMax = yMin + 100;
    lv_chart_set_range(_chart, LV_CHART_AXIS_PRIMARY_Y, yMin, yMax);

    int pointCount = last - first + 1;
    if (pointCount < 1) pointCount = 1;
    lv_chart_set_point_count(_chart, pointCount);
    lv_chart_set_all_value(_chart, _chartSeries, LV_CHART_POINT_NONE);

    int nowIdx = -1;
    for (int i = first; i <= last; ++i) {
        if (now >= _prices.at(i).ts_start &&
            now <  _prices.at(i).ts_start + 3600) { nowIdx = i; break; }
    }
    _chartNowIdx = nowIdx;
    _lastNowIdx = nowIdx;

    for (int i = 0; i < pointCount; ++i) {
        float p = _prices.at(first + i).price_raw;
        lv_chart_set_value_by_id(_chart, _chartSeries, i, (int)(p * 100000.0f));
    }

    lv_label_set_text(_chartUnits, unitsLabel().c_str());

    // Re-position the now-line and hour labels too. They live on top of
    // the chart card, but their X positions depend on the chart's plot
    // dimensions which we just (potentially) changed.
    refreshChartOverlays();
}

// Position the chart's overlay widgets (the light-blue "now" line and
// the eight 6-hour time labels) in sync with the chart's plot area.
//
// IMPORTANT: the math here MUST match the math in onChartDrawGrid (and
// drawSixHourGrid) so the labels sit directly under the grid lines. See
// the math comment on drawSixHourGrid for the derivation.
//
// Cheap to call — at most 9 lv_obj_set_pos() calls per invocation.
void UI::refreshChartOverlays() {
    if (!_chart || !_chartCard) return;

    // We need the chart's PLOT area (chart bbox minus padding) expressed
    // in coordinates relative to its parent (the chart card), because
    // that's the coordinate space the overlay widgets live in.
    lv_obj_update_layout(_chart);
    lv_coord_t cx = lv_obj_get_x(_chart);
    lv_coord_t cy = lv_obj_get_y(_chart);
    lv_coord_t cw = lv_obj_get_width(_chart);
    lv_coord_t ch = lv_obj_get_height(_chart);
    lv_coord_t padL = lv_obj_get_style_pad_left  (_chart, LV_PART_MAIN);
    lv_coord_t padR = lv_obj_get_style_pad_right (_chart, LV_PART_MAIN);
    lv_coord_t padT = lv_obj_get_style_pad_top   (_chart, LV_PART_MAIN);
    lv_coord_t padB = lv_obj_get_style_pad_bottom(_chart, LV_PART_MAIN);

    lv_coord_t plotX = cx + padL;
    lv_coord_t plotY = cy + padT;
    lv_coord_t plotW = cw - padL - padR;
    lv_coord_t plotH = ch - padT - padB;
    if (plotW <= 0 || plotH <= 0) return;

    // The chart represents 48 hours (point 0..47 spanning the full plotW).
    // For point i: x = plotX + i * plotW / 47.
    static const int kHours[8] = { 6, 12, 18, 24, 30, 36, 42, 48 };

    // Position the 8 hour labels. The first 7 sit AT the corresponding
    // grid line; the 8th (hour 48) is at the far right edge and we
    // simply hide it because there's no room and the value duplicates
    // the leftmost "00" anyway. The labels are children of _chartCard,
    // so positions are in chart-card coordinates.
    for (int i = 0; i < 8; ++i) {
        lv_obj_t* lbl = _chartHourLbls[i];
        if (!lbl) continue;
        if (i == 7) {
            lv_obj_add_flag(lbl, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_HIDDEN);
        // Place the label centered horizontally on the grid line.
        // We need the label's measured width to center it properly.
        lv_obj_update_layout(lbl);
        lv_coord_t lblW = lv_obj_get_width(lbl);
        int h = kHours[i];
        lv_coord_t x = plotX + (lv_coord_t)((int32_t)plotW * h / 47);
        // Y position: just below the plot area, inside the chart's
        // pad_bottom region. plotY+plotH = bottom of plot; the chart's
        // pad_bottom (14 px) is what we reserved for these labels.
        lv_coord_t y = plotY + plotH + 1;
        lv_obj_set_pos(lbl, x - lblW / 2, y);
    }

    // Position the "now" indicator. We want fractional precision — the
    // line should glide smoothly between hour ticks, not jump every hour.
    if (_chartNowMarker) {
        time_t now = time(nullptr);
        // Need the same anchor as refreshChart() — start of "today" in
        // local time. The chart spans [todayStart, todayStart + 48h).
        struct tm tnow; localtime_r(&now, &tnow);
        tnow.tm_hour = 0; tnow.tm_min = 0; tnow.tm_sec = 0;
        time_t todayStart = mktime(&tnow);

        long offsetSec = (long)(now - todayStart);
        const long windowSec = 48L * 3600L;

        if (now < 100000 || offsetSec < 0 || offsetSec >= windowSec ||
            _chartFirstIdx < 0) {
            // No valid time, or we're outside the visible window.
            lv_obj_add_flag(_chartNowMarker, LV_OBJ_FLAG_HIDDEN);
        } else {
            // Map offsetSec onto the 0..47 point range (point i covers
            // hours [i, i+1)). At offsetSec = 0 we sit on point 0; at
            // offsetSec = 47*3600 (i.e. start of hour 47) we sit on
            // point 47 — but we have a fractional point index for
            // smooth motion.
            float fIdx = (float)offsetSec / 3600.0f;
            if (fIdx < 0) fIdx = 0;
            if (fIdx > 47.0f) fIdx = 47.0f;
            lv_coord_t x = plotX + (lv_coord_t)(plotW * fIdx / 47.0f);
            // Center the 2px line on the computed x.
            lv_obj_set_pos(_chartNowMarker, x - 1, plotY);
            lv_obj_set_size(_chartNowMarker, 2, plotH);
            lv_obj_clear_flag(_chartNowMarker, LV_OBJ_FLAG_HIDDEN);
            // Make sure the now-line draws ON TOP of the chart and labels.
            lv_obj_move_foreground(_chartNowMarker);
        }
    }
}

void UI::refreshStats() {
    time_t now = time(nullptr);
    float mn, mx, av;
    bool hasDay = _prices.statsForDay(now, mn, mx, av);

    if (hasDay) {
        lv_label_set_text(_statAvg, formatPrice(av).c_str());
        lv_label_set_text(_statMin, formatPrice(mn).c_str());
        lv_label_set_text(_statMax, formatPrice(mx).c_str());
        lv_obj_set_style_text_color(_statMin, COL_GREEN, 0);
        lv_obj_set_style_text_color(_statMax, COL_ORANGE, 0);
    } else {
        lv_label_set_text(_statAvg, "--");
        lv_label_set_text(_statMin, "--");
        lv_label_set_text(_statMax, "--");
    }

    time_t lu = _prices.lastUpdate();
    if (lu > 100000) {
        struct tm t; localtime_r(&lu, &t);
        char b[8]; snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min);
        lv_label_set_text(_statUpd, b);
    } else {
        lv_label_set_text(_statUpd, "--");
    }
}

void UI::refreshRelayTiles() {
    for (int i = 0; i < NUM_RELAYS; ++i) {
        const RelayRule& rr = _relays.rule(i);
        RelayTile& tt = _tiles[i];
        lv_label_set_text(tt.icon, iconText((RelayIcon)rr.icon));
        lv_obj_set_style_text_color(tt.icon,
            rr.state ? COL_GREEN : COL_TEXT, 0);

        lv_label_set_text(tt.name, rr.name);
        lv_label_set_text(tt.state, rr.state ? T(S_ON) : T(S_OFF));
        lv_obj_set_style_text_color(tt.state,
            rr.state ? COL_GREEN : COL_DIM, 0);

        const char* m = "";
        switch (rr.mode) {
            case RMODE_OFF:   m = T(S_ALWAYS_OFF); break;
            case RMODE_ON:    m = T(S_ALWAYS_ON);  break;
            case RMODE_PRICE: m = T(S_PRICE);      break;
            case RMODE_AUTO:  m = T(S_AUTO);       break;
        }
        lv_label_set_text(tt.mode, m);

        if (rr.mode == RMODE_PRICE) {
            // Single-threshold mode — show just the on_below value.
            char b[16];
            snprintf(b, sizeof(b), "%.1f c", rr.on_below * 100.0f);
            lv_label_set_text(tt.threshold, b);
        } else if (rr.mode == RMODE_AUTO) {
            char b[24];
            // If both thresholds happen to be equal, show one number to
            // keep the tile uncluttered. Otherwise show "ON / OFF" pair.
            if (fabsf(rr.on_below - rr.off_above) < 0.0001f) {
                snprintf(b, sizeof(b), "%.1f c", rr.on_below * 100.0f);
            } else {
                snprintf(b, sizeof(b), "%.1f / %.1f",
                         rr.on_below * 100.0f, rr.off_above * 100.0f);
            }
            lv_label_set_text(tt.threshold, b);
        } else {
            lv_label_set_text(tt.threshold, "");
        }

        // Highlight border when ON
        lv_obj_set_style_border_color(tt.card,
            rr.state ? COL_GREEN : COL_CARD, 0);
        lv_obj_set_style_border_width(tt.card, rr.state ? 2 : 0, 0);
    }
}

void UI::refreshEdit() {
    if (_editIdx < 0 || _editIdx >= NUM_RELAYS) return;
    const RelayRule& rr = _relays.rule(_editIdx);

    char title[40];
    snprintf(title, sizeof(title), "%s %d  %s", T(S_RELAY), _editIdx + 1, rr.name);
    lv_label_set_text(_editTitle, title);

    // Highlight the active mode button.
    for (int m = 0; m < 4; ++m) {
        bool active = (int)rr.mode == m;
        lv_obj_set_style_bg_color(_modeBtns[m],
            active ? COL_NOWBAR : COL_CARD, 0);
    }

    // Visibility of the threshold rows depends on the mode:
    //   OFF / ON  -> both rows hidden
    //   PRICE     -> on_below row visible (acts as a single threshold);
    //                off_above row hidden
    //   AUTO      -> both rows visible
    bool showOn  = (rr.mode == RMODE_PRICE || rr.mode == RMODE_AUTO);
    bool showOff = (rr.mode == RMODE_AUTO);

    if (_editOnBelowRow) {
        if (showOn) lv_obj_clear_flag(_editOnBelowRow, LV_OBJ_FLAG_HIDDEN);
        else        lv_obj_add_flag  (_editOnBelowRow, LV_OBJ_FLAG_HIDDEN);
    }
    if (_editOnBelowCaption) {
        if (showOn) lv_obj_clear_flag(_editOnBelowCaption, LV_OBJ_FLAG_HIDDEN);
        else        lv_obj_add_flag  (_editOnBelowCaption, LV_OBJ_FLAG_HIDDEN);
    }
    if (_editOffAboveRow) {
        if (showOff) lv_obj_clear_flag(_editOffAboveRow, LV_OBJ_FLAG_HIDDEN);
        else         lv_obj_add_flag  (_editOffAboveRow, LV_OBJ_FLAG_HIDDEN);
    }
    if (_editOffAboveCaption) {
        if (showOff) lv_obj_clear_flag(_editOffAboveCaption, LV_OBJ_FLAG_HIDDEN);
        else         lv_obj_add_flag  (_editOffAboveCaption, LV_OBJ_FLAG_HIDDEN);
    }

    if (showOn && _editOnBelowVal) {
        char vb[32];
        snprintf(vb, sizeof(vb), "%.1f c/kWh", rr.on_below * 100.0f);
        lv_label_set_text(_editOnBelowVal, vb);
    }
    if (showOff && _editOffAboveVal) {
        char vb[32];
        snprintf(vb, sizeof(vb), "%.1f c/kWh", rr.off_above * 100.0f);
        lv_label_set_text(_editOffAboveVal, vb);
    }

    // Hysteresis hint at the bottom — only meaningful in AUTO mode.
    // In PRICE mode we instead show a small note that there's no
    // hysteresis (so the user understands why there's only one input).
    if (_editHysteresisLbl) {
        char hb[64];
        if (rr.mode == RMODE_AUTO) {
            float gap = rr.off_above - rr.on_below;
            if (gap > 0.0001f) {
                snprintf(hb, sizeof(hb), "%s: %.1f c/kWh",
                         T(S_HYSTERESIS), gap * 100.0f);
                lv_obj_set_style_text_color(_editHysteresisLbl, COL_DIM, 0);
            } else if (gap < -0.0001f) {
                // ON-below higher than OFF-above is invalid (the relay
                // would oscillate or never turn off). Warn in red.
                snprintf(hb, sizeof(hb), "! ON > OFF (%.1f c)", -gap * 100.0f);
                lv_obj_set_style_text_color(_editHysteresisLbl, COL_RED, 0);
            } else {
                snprintf(hb, sizeof(hb), "%s: 0 c/kWh", T(S_HYSTERESIS));
                lv_obj_set_style_text_color(_editHysteresisLbl, COL_DIM, 0);
            }
            lv_label_set_text(_editHysteresisLbl, hb);
            lv_obj_clear_flag(_editHysteresisLbl, LV_OBJ_FLAG_HIDDEN);
        } else if (rr.mode == RMODE_PRICE) {
            snprintf(hb, sizeof(hb), "%s: 0 c/kWh", T(S_HYSTERESIS));
            lv_obj_set_style_text_color(_editHysteresisLbl, COL_DIM, 0);
            lv_label_set_text(_editHysteresisLbl, hb);
            lv_obj_clear_flag(_editHysteresisLbl, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(_editHysteresisLbl, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (_editIconLabel) {
        lv_label_set_text(_editIconLabel, iconText((RelayIcon)rr.icon));
    }
}

// ---- Public state setters ------------------------------------------

void UI::setWifiConnected(bool c)    { _wifi = c; }
void UI::setIpString(const String& ip) { _ip = ip; }
void UI::setConfigMode(bool c)       { _configMode = c; }
void UI::setShowVat(bool v)          { _showVat = v; }
void UI::setVatPercent(float v)      { _vat = v; }

// ---- Navigation -----------------------------------------------------

void UI::openMain()             { lv_scr_load(_scrMain); }
void UI::openRelays()           { lv_scr_load(_scrRelays); }
void UI::openEdit(int idx)      { _editIdx = idx; lv_scr_load(_scrEdit); }
void UI::openIconPick()         { lv_scr_load(_scrIconPick); }
void UI::openFullTable()        { lv_scr_load(_scrFullChart); refreshFullTable(); }

// ---- Event thunks ---------------------------------------------------
// LVGL passes a context pointer through user_data; we cast it to UI* and
// call the right method. This keeps event handling cohesive without
// forcing us to make every method public.

void UI::onMainTouched(lv_event_t* e) {
    UI* self = (UI*)lv_event_get_user_data(e);
    self->openRelays();
}

void UI::onRelayTileTouched(lv_event_t* e) {
    UI* self = (UI*)lv_event_get_user_data(e);
    lv_obj_t* tile = lv_event_get_current_target(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(tile);
    self->openEdit(idx);
}

void UI::onBackToMainTouched(lv_event_t* e) {
    UI* self = (UI*)lv_event_get_user_data(e);
    self->openMain();
}

void UI::onBackToRelaysTouched(lv_event_t* e) {
    UI* self = (UI*)lv_event_get_user_data(e);
    self->openRelays();
}

void UI::onModeBtnTouched(lv_event_t* e) {
    UI* self = (UI*)lv_event_get_user_data(e);
    lv_obj_t* btn = lv_event_get_current_target(e);
    int m = (int)(intptr_t)lv_obj_get_user_data(btn);
    if (self->_editIdx >= 0 && self->_editIdx < NUM_RELAYS) {
        self->_relays.rule(self->_editIdx).mode = (RelayMode)m;
        self->_relays.save();
        self->refreshEdit();
    }
}

// Spinner buttons carry an integer tag in user_data:
//   0 = adjust on_below, 1 = adjust off_above
// We walk one step (0.5 c/kWh = 0.005 EUR/kWh) per tap and clamp to a
// sensible range. The two thresholds are *not* coupled — the user can
// freely set on_below > off_above; refreshEdit() flags this visually so
// they can correct it if it was unintentional.
void UI::onMinusTouched(lv_event_t* e) {
    UI* self = (UI*)lv_event_get_user_data(e);
    if (self->_editIdx < 0 || self->_editIdx >= NUM_RELAYS) return;
    lv_obj_t* btn = lv_event_get_current_target(e);
    int tag = (int)(intptr_t)lv_obj_get_user_data(btn);
    RelayRule& rr = self->_relays.rule(self->_editIdx);
    float* p = (tag == 1) ? &rr.off_above : &rr.on_below;
    *p -= 0.005f;
    if (*p < 0) *p = 0;
    self->_relays.save();
    self->refreshEdit();
}

void UI::onPlusTouched(lv_event_t* e) {
    UI* self = (UI*)lv_event_get_user_data(e);
    if (self->_editIdx < 0 || self->_editIdx >= NUM_RELAYS) return;
    lv_obj_t* btn = lv_event_get_current_target(e);
    int tag = (int)(intptr_t)lv_obj_get_user_data(btn);
    RelayRule& rr = self->_relays.rule(self->_editIdx);
    float* p = (tag == 1) ? &rr.off_above : &rr.on_below;
    *p += 0.005f;
    if (*p > 2.0f) *p = 2.0f;
    self->_relays.save();
    self->refreshEdit();
}

// ===================================================================
// ICON PICKER SCREEN
// ===================================================================
//
// 5-column × 5-row grid (we have 21 icons → fits with 4 empty cells).
// Each cell is a card showing the icon glyph; tap selects + returns to
// the edit screen.

void UI::buildIconPick() {
    _scrIconPick = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_scrIconPick, COL_BG, 0);
    lv_obj_set_style_bg_opa(_scrIconPick, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_scrIconPick, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(_scrIconPick, 0, 0);

    lv_obj_t* title = makeLabel(_scrIconPick, T(S_ICON),
                                COL_TEXT, FONT_16);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 6);

    lv_obj_t* back = makeCard(_scrIconPick, DISP_W - 80, 4, 72, 24);
    lv_obj_set_style_radius(back, 12, 0);
    lv_obj_t* bl = makeLabel(back, T(S_BACK), COL_TEXT, FONT_12);
    lv_obj_center(bl);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(back, onIconPickBack, LV_EVENT_CLICKED, this);

    int top = 36, padX = 4, padY = 4;
    int cols = 7, rows = 3;   // 21 cells, fits ICON_COUNT
    int cellW = (DISP_W - padX * (cols + 1)) / cols;
    int cellH = (DISP_H - top - padY - padY * rows) / rows;

    for (int i = 0; i < ICON_COUNT && i < cols * rows; ++i) {
        int r = i / cols, c = i % cols;
        int x = padX + c * (cellW + padX);
        int y = top + r * (cellH + padY);

        lv_obj_t* cell = makeCard(_scrIconPick, x, y, cellW, cellH);
        lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(cell, (void*)(intptr_t)i);
        lv_obj_add_event_cb(cell, onIconChosen, LV_EVENT_CLICKED, this);

        lv_obj_t* g = makeLabel(cell, iconText((RelayIcon)i),
                                COL_TEXT, ICON_FONT_24);
        lv_obj_center(g);
    }
}

// ===================================================================
// FULLSCREEN PRICE-TABLE SCREEN
// ===================================================================
//
// Tapping the dashboard chart opens this screen which shows every
// available future hour as a colored cell in a 4-column scrollable
// grid. Each cell contains "HH:00\nN.N c/kWh"; the cell's background
// color reflects the price tier (green/yellow/orange/red), same scale
// as the dashboard chart. The cell representing the CURRENT hour is
// outlined in light blue (COL_NOWBAR) so the user can tell at a glance
// where they are in the timeline.
//
// Implementation notes:
// * We use lv_table for free scrolling, fixed-pitch cells, and the
//   built-in LV_EVENT_DRAW_PART_BEGIN hook for per-cell coloring.
// * Cell value text is "HH:00\nN.N" — a two-line label fits comfortably
//   in a 60-px-tall row at FONT_14.
// * Coloring + outlining happen entirely in onFullTableDrawPart by
//   modifying the rect_dsc / label_dsc fields LVGL passes us before it
//   actually draws the cell.

void UI::buildFullTable() {
    _scrFullChart = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_scrFullChart, COL_BG, 0);
    lv_obj_set_style_bg_opa(_scrFullChart, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_scrFullChart, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(_scrFullChart, 0, 0);

    // Header bar: title on the left, "Back" button on the right.
    _scrFullTitle = makeLabel(_scrFullChart, T(S_PRICES_TODAY),
                              COL_TEXT, FONT_16);
    lv_obj_align(_scrFullTitle, LV_ALIGN_TOP_LEFT, 8, 6);

    lv_obj_t* back = makeCard(_scrFullChart, DISP_W - 80, 4, 72, 24);
    lv_obj_set_style_radius(back, 12, 0);
    lv_obj_t* bl = makeLabel(back, T(S_BACK), COL_TEXT, FONT_12);
    lv_obj_center(bl);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(back, onFullTableBack, LV_EVENT_CLICKED, this);

    // The table itself fills the rest of the screen.
    int tableY = 32;
    int tableH = DISP_H - tableY;
    _fullTable = lv_table_create(_scrFullChart);
    lv_obj_set_pos(_fullTable, 0, tableY);
    lv_obj_set_size(_fullTable, DISP_W, tableH);
    lv_obj_set_style_bg_color(_fullTable, COL_BG, 0);
    lv_obj_set_style_bg_opa(_fullTable, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_fullTable, 0, 0);
    lv_obj_set_style_pad_all(_fullTable, 0, 0);

    // Column count + per-column width. 4 columns x 120 px = 480 px exactly.
    lv_table_set_col_cnt(_fullTable, _fullCols);
    int colW = DISP_W / _fullCols;
    for (int c = 0; c < _fullCols; ++c) {
        lv_table_set_col_width(_fullTable, c, colW);
    }

    // Cell appearance: small padding, dark separator between cells, white
    // text by default (we override per-cell in the draw callback).
    lv_obj_set_style_pad_all   (_fullTable, 6, LV_PART_ITEMS);
    lv_obj_set_style_text_align(_fullTable, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(_fullTable, COL_TEXT, LV_PART_ITEMS);
    lv_obj_set_style_text_font (_fullTable, FONT_14, LV_PART_ITEMS);
    lv_obj_set_style_border_color(_fullTable, COL_BG, LV_PART_ITEMS);
    lv_obj_set_style_border_width(_fullTable, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_side (_fullTable, LV_BORDER_SIDE_FULL, LV_PART_ITEMS);

    // Per-cell color + "now" outline are applied here.
    lv_obj_add_event_cb(_fullTable, onFullTableDrawPart,
                        LV_EVENT_DRAW_PART_BEGIN, this);
}

// Populate the table from the price feed: every available hour from the
// CURRENT one onward gets a cell, in chronological order, left-to-right
// then top-to-bottom (so reading order matches time order).
void UI::refreshFullTable() {
    if (!_fullTable) return;
    int n = _prices.count();
    if (n == 0) {
        lv_table_set_row_cnt(_fullTable, 1);
        lv_table_set_cell_value(_fullTable, 0, 0, T(S_NO_DATA));
        _fullFirstIdx = _fullLastIdx = _fullNowIdx = -1;
        return;
    }

    time_t now = time(nullptr);

    // Find the first entry whose hour CONTAINS "now" (or is in the
    // future). Everything from that index onward gets shown. If "now"
    // hasn't started yet (e.g. clock not synced), start at 0.
    int first = 0;
    bool found = false;
    for (int i = 0; i < n; ++i) {
        time_t ts = _prices.at(i).ts_start;
        if (now < ts + 3600) { first = i; found = true; break; }
    }
    if (!found) first = n - 1;
    int last = n - 1;
    int count = last - first + 1;
    if (count < 1) count = 1;

    _fullFirstIdx = first;
    _fullLastIdx  = last;

    // Min/max for the cell-color scale (use the WHOLE displayed range so
    // coloring stays consistent as the table scrolls).
    float mn = _prices.at(first).price_raw;
    float mx = mn;
    for (int i = first; i <= last; ++i) {
        float p = _prices.at(i).price_raw;
        if (p < mn) mn = p;
        if (p > mx) mx = p;
    }
    _fullMin = mn;
    _fullMax = mx;
    _fullAvg = 0;     // unused

    // Locate the "now" cell so the draw callback can outline it. The
    // first cell IS "now" by construction (or the future) — but only
    // outline if "now" is actually inside that hour, not before it.
    int nowAbsIdx = -1;
    for (int i = first; i <= last; ++i) {
        time_t ts = _prices.at(i).ts_start;
        if (now >= ts && now < ts + 3600) { nowAbsIdx = i; break; }
    }
    _fullNowIdx = (nowAbsIdx >= 0) ? (nowAbsIdx - first) : -1;

    // Write cell values.
    int rows = (count + _fullCols - 1) / _fullCols;
    lv_table_set_row_cnt(_fullTable, rows);
    for (int i = 0; i < count; ++i) {
        int row = i / _fullCols;
        int col = i % _fullCols;
        const PriceEntry& e = _prices.at(first + i);
        struct tm dt; localtime_r(&e.ts_start, &dt);
        char b[32];
        // Two-line cell: time on top, price below.
        snprintf(b, sizeof(b), "%02d:00\n%s",
                 dt.tm_hour, formatPrice(e.price_raw).c_str());
        lv_table_set_cell_value(_fullTable, row, col, b);
    }
    // Clear unused trailing cells (if the previous refresh had more).
    for (int i = count; i < rows * _fullCols; ++i) {
        int row = i / _fullCols;
        int col = i % _fullCols;
        lv_table_set_cell_value(_fullTable, row, col, "");
    }

    // Title: "Today  DD.MM"
    {
        struct tm dt; localtime_r(&now, &dt);
        char tb[40];
        snprintf(tb, sizeof(tb), "%s  %d.%02d",
                 T(S_PRICES_TODAY), dt.tm_mday, dt.tm_mon + 1);
        lv_label_set_text(_scrFullTitle, tb);
    }

    // Force redraw so the now-cell gets outlined / cells get recolored.
    lv_obj_invalidate(_fullTable);
}

// LVGL fires LV_EVENT_DRAW_PART_BEGIN for each cell in an lv_table BEFORE
// it actually paints the cell. dsc->id is row * col_count + col. We use
// it to look up the price for that cell and override the rect_dsc /
// label_dsc fields so the cell is drawn in the right tier color and
// (for the "now" cell) with a light-blue outline.
void UI::onFullTableDrawPart(lv_event_t* e) {
    UI* self = (UI*)lv_event_get_user_data(e);
    lv_obj_draw_part_dsc_t* dsc = lv_event_get_draw_part_dsc(e);
    if (!dsc || dsc->part != LV_PART_ITEMS) return;

    lv_obj_t* tbl = lv_event_get_target(e);
    uint16_t cols = lv_table_get_col_cnt(tbl);
    uint32_t row  = dsc->id / cols;
    uint32_t col  = dsc->id - row * cols;
    int rel       = (int)(row * cols + col);   // index within shown range
    int abs       = self->_fullFirstIdx + rel;

    if (abs < self->_fullFirstIdx || abs > self->_fullLastIdx) {
        // Trailing empty cell. Make its background match the screen so
        // it disappears visually.
        if (dsc->rect_dsc) {
            dsc->rect_dsc->bg_color = COL_BG;
            dsc->rect_dsc->bg_opa   = LV_OPA_COVER;
        }
        return;
    }

    if (abs >= 0 && abs < self->_prices.count()) {
        float p = self->_prices.at(abs).price_raw;
        lv_color_t c = self->colorForPrice(p, self->_fullMin, self->_fullMax);
        if (dsc->rect_dsc) {
            dsc->rect_dsc->bg_color = c;
            dsc->rect_dsc->bg_opa   = LV_OPA_COVER;
            // Default cell border = thin dark separator.
            dsc->rect_dsc->border_color = COL_BG;
            dsc->rect_dsc->border_width = 1;
            dsc->rect_dsc->radius       = 0;
            // Highlight the "now" cell with a thick light-blue border.
            if (rel == self->_fullNowIdx) {
                dsc->rect_dsc->border_color = COL_NOWBAR;
                dsc->rect_dsc->border_width = 3;
            }
        }
        if (dsc->label_dsc) {
            // Yellow is the only tier color light enough to need dark
            // text for readability. Everything else stays white.
            if (lv_color_to32(c) == lv_color_to32(COL_YELLOW)) {
                dsc->label_dsc->color = COL_BG;
            } else {
                dsc->label_dsc->color = COL_TEXT;
            }
            dsc->label_dsc->align = LV_TEXT_ALIGN_CENTER;
        }
    }
}

void UI::onFullTableBack(lv_event_t* e) {
    UI* self = (UI*)lv_event_get_user_data(e);
    self->openMain();
}

// ===================================================================
// CHART DRAW CALLBACKS — per-segment color
// ===================================================================
//
// LVGL fires LV_EVENT_DRAW_PART_BEGIN for each line segment of a chart
// series. We intercept and replace the color based on where this point's
// price falls relative to the day's min/max — same color tiers as the
// rest of the UI (green/yellow/orange/red).

void UI::onChartDrawPart(lv_event_t* e) {
    UI* self = (UI*)lv_event_get_user_data(e);
    lv_obj_draw_part_dsc_t* dsc = lv_event_get_draw_part_dsc(e);
    if (!dsc) return;

    // Tick label rendering for the X-axis. LVGL fires this with
    // LV_CHART_DRAW_PART_TICK_LABEL as the part type when it wants the
    // text for an axis tick. dsc->id is the tick index. We map that to
    // an hour value so labels read 0, 6, 12, 18, 24, 30, 36, 42, 47.
    if (dsc->part == LV_PART_TICKS &&
        dsc->id == LV_CHART_AXIS_PRIMARY_X &&
        dsc->text != NULL) {
        // The chart was set up with 9 major ticks across 48 points,
        // i.e. 0, 6, 12, 18, 24, 30, 36, 42, 47. dsc->value carries
        // the integer along the axis if LVGL's built-in mapping is used,
        // but we ignore it and compute from dsc->id directly to be safe.
        int hour = (dsc->value >= 0 && dsc->value < 48) ? dsc->value : 0;
        lv_snprintf(dsc->text, dsc->text_length, "%d", hour);
        return;
    }

    // Line segments — pick a color based on the value at this index.
    if (dsc->part != LV_PART_ITEMS) return;
    if (!dsc->p1) return;
    int idx = dsc->id;   // index of the SECOND endpoint of this segment
    int absIdx = self->_chartFirstIdx + idx;
    if (absIdx < 0 || absIdx >= self->_prices.count()) return;
    float p = self->_prices.at(absIdx).price_raw;
    lv_color_t c = self->colorForPrice(p, self->_chartMin, self->_chartMax);
    if (dsc->line_dsc) dsc->line_dsc->color = c;
}

// ===================================================================
// CUSTOM 6-HOUR GRID LINES
// ===================================================================
//
// We don't use LVGL's built-in lv_chart_set_div_line_count because at
// the default theme styling those lines render as faint translucent
// gray (the "fuzz" reported on the LCD) and they don't align with our
// 6-hour tick labels. Instead, we hook LV_EVENT_DRAW_POST_END which
// fires after the chart has finished drawing all its parts, and we
// draw vertical lines on top of the plot area at the same X positions
// LVGL uses for the major ticks.
//
// Plot-area math:
//   The chart has 48 points spanning the full plot width. LVGL puts
//   point 0 at the left edge of (chart->coords + pad_left) and point N-1
//   at the right edge minus pad_right. So the X position of point i is
//       x_i = x1 + i * (plot_w / (N - 1))
//   We draw lines at i = 6, 12, 18, 24, 30, 36, 42 (skipping 0 and 47
//   because they sit right on the chart's edges).

static void drawSixHourGrid(lv_event_t* e, int pointCount) {
    if (lv_event_get_code(e) != LV_EVENT_DRAW_POST_END) return;
    lv_obj_t* chart = lv_event_get_target(e);
    if (!chart) return;
    lv_draw_ctx_t* draw_ctx = lv_event_get_draw_ctx(e);
    if (!draw_ctx) return;

    // Compute the plot-area rect: chart bounding box, minus padding on
    // every side (LVGL allocates pad_top/pad_left for the plotting area
    // and pad_bottom/pad_right for axis ticks/labels).
    lv_coord_t x1 = chart->coords.x1 + lv_obj_get_style_pad_left  (chart, LV_PART_MAIN);
    lv_coord_t x2 = chart->coords.x2 - lv_obj_get_style_pad_right (chart, LV_PART_MAIN);
    lv_coord_t y1 = chart->coords.y1 + lv_obj_get_style_pad_top   (chart, LV_PART_MAIN);
    lv_coord_t y2 = chart->coords.y2 - lv_obj_get_style_pad_bottom(chart, LV_PART_MAIN);
    int plotW = x2 - x1;
    if (plotW <= 0 || pointCount < 2) return;

    // LVGL fits N points across [x1, x2] inclusive on both ends, so each
    // step is plotW / (N - 1). Hours we want vertical lines at: every 6h,
    // skipping the chart edges (hour 0 = left edge, hour ~N-1 = right).
    static const int kHours[] = { 6, 12, 18, 24, 30, 36, 42 };
    static const int kHoursN = sizeof(kHours) / sizeof(kHours[0]);

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_hex(0x3A3F4A); // slightly lighter than COL_CARD
    line_dsc.width = 1;
    line_dsc.opa   = LV_OPA_60;

    for (int k = 0; k < kHoursN; k++) {
        int h = kHours[k];
        if (h >= pointCount) continue;
        lv_coord_t x = x1 + (lv_coord_t)((int32_t)plotW * h / (pointCount - 1));
        lv_point_t p1 = { x, y1 };
        lv_point_t p2 = { x, y2 };
        lv_draw_line(draw_ctx, &line_dsc, &p1, &p2);
    }
}

void UI::onChartDrawGrid(lv_event_t* e) {
    drawSixHourGrid(e, 48);
}

void UI::onChartTouched(lv_event_t* e) {
    UI* self = (UI*)lv_event_get_user_data(e);
    // Stop the event from bubbling up to the main-screen click handler,
    // which would otherwise also fire and try to open the relay screen.
    lv_event_stop_bubbling(e);
    self->openFullTable();
}

// ===================================================================
// ICON PICKER EVENTS
// ===================================================================

void UI::onIconBtnTouched(lv_event_t* e) {
    UI* self = (UI*)lv_event_get_user_data(e);
    self->openIconPick();
}

void UI::onIconChosen(lv_event_t* e) {
    UI* self = (UI*)lv_event_get_user_data(e);
    lv_obj_t* cell = lv_event_get_current_target(e);
    int icon = (int)(intptr_t)lv_obj_get_user_data(cell);
    if (icon < 0 || icon >= ICON_COUNT) return;
    if (self->_editIdx < 0 || self->_editIdx >= NUM_RELAYS) return;
    self->_relays.rule(self->_editIdx).icon = (uint8_t)icon;
    self->_relays.save();
    self->openEdit(self->_editIdx);   // back to edit screen with new icon
}

void UI::onIconPickBack(lv_event_t* e) {
    UI* self = (UI*)lv_event_get_user_data(e);
    self->openEdit(self->_editIdx);
}
