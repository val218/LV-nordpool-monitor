// price_data.h — parsing and in-memory storage of Nordpool price series
#pragma once

#include <Arduino.h>
#include <time.h>
#include "config.h"

struct PriceEntry {
    time_t  ts_start;   // UTC epoch seconds, start of the hour
    float   price_raw;  // EUR/kWh, WITHOUT VAT (as delivered by the source)
};

class PriceData {
public:
    PriceData();

    // Parse the CSV body from nordpool.didnt.work. Returns count of stored entries.
    // CSV shape: "ts_start,ts_end,price\n2026-03-22 00:00:00,2026-03-22 01:00:00,0.042660"
    // Timestamps are local Latvia time ("Attēlotais ir Latvijas laiks"), so we
    // interpret them as local time and convert to UTC epoch via mktime().
    int parseCsv(const String& csv);

    // Parse the e-cena.lv HTML page. Extracts 15-minute prices from the
    // tables under each <h4>DD-MM-YYYY</h4> day header, then averages each
    // group of 4 quarter-hours into a single hourly entry that fits the
    // existing storage schema. Returns the number of hourly entries stored.
    int parseEcenaHtml(const String& html);

    // Streaming parse — call beginStream() once, then feedLine(line) for each
    // line of the CSV body, then endStream() when done. Avoids holding the
    // full response in memory (the .didnt.work source is ~2 MB).
    void beginStream();
    void feedLine(const String& line);
    void endStream();

    // Return price entry for a given epoch (the hour that contains it).
    const PriceEntry* getForTime(time_t epoch) const;

    // Convenience wrappers (relative to `now`)
    const PriceEntry* getCurrent(time_t now) const { return getForTime(now); }
    const PriceEntry* getPrevious(time_t now) const { return getForTime(now - 3600); }
    const PriceEntry* getNext(time_t now)    const { return getForTime(now + 3600); }

    // Price with VAT applied (if showVat), in EUR/kWh
    float priced(float raw, bool showVat, float vatPercent) const {
        return showVat ? raw * (1.0f + vatPercent / 100.0f) : raw;
    }

    // Min / max / avg over all stored entries (raw, without VAT)
    void stats(float& outMin, float& outMax, float& outAvg) const;

    // Stats restricted to entries whose ts_start falls on the same calendar day
    // as `dayEpoch` (local time). Returns false if no matches.
    bool statsForDay(time_t dayEpoch, float& outMin, float& outMax, float& outAvg) const;

    // Raw access for graph rendering
    int  count() const { return _count; }
    const PriceEntry& at(int i) const { return _entries[i]; }

    // Sort entries ascending by ts_start (so graph draws left->right in time)
    void sortByTime();

    time_t lastUpdate() const { return _lastUpdate; }
    void   markUpdated() { _lastUpdate = time(nullptr); }

private:
    PriceEntry _entries[MAX_PRICE_ENTRIES];
    int        _count = 0;
    time_t     _lastUpdate = 0;

    // Stream-parse state — used during line-by-line feed.
    bool   _streamHeaderSkipped = false;
    int    _streamScanned = 0;
    int    _streamKept = 0;
    time_t _streamWindowStart = 0;
    time_t _streamWindowEnd = 0;
    bool   _streamTimeKnown = false;

    static time_t parseLocalTs(const char* s);  // "YYYY-MM-DD HH:MM:SS" local -> UTC epoch
};
