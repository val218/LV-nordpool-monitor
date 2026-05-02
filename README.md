#include "price_data.h"
#include <string.h>
#include <stdlib.h>

PriceData::PriceData() {}

time_t PriceData::parseLocalTs(const char* s) {
    // Expects "YYYY-MM-DD HH:MM:SS"
    if (!s || strlen(s) < 19) return 0;
    struct tm t = {};
    t.tm_year = atoi(s) - 1900;
    t.tm_mon  = atoi(s + 5) - 1;
    t.tm_mday = atoi(s + 8);
    t.tm_hour = atoi(s + 11);
    t.tm_min  = atoi(s + 14);
    t.tm_sec  = atoi(s + 17);
    t.tm_isdst = -1;  // let mktime figure out DST using the configured TZ
    return mktime(&t);
}

int PriceData::parseCsv(const String& csv) {
    _count = 0;
    int start = 0;
    bool headerSkipped = false;
    const int len = csv.length();

    while (start < len && _count < MAX_PRICE_ENTRIES) {
        int eol = csv.indexOf('\n', start);
        if (eol < 0) eol = len;
        int lineLen = eol - start;
        if (lineLen > 0) {
            // Copy line into a small stack buffer (max ~60 chars expected).
            char line[96];
            int copyLen = lineLen < (int)sizeof(line) - 1 ? lineLen : (int)sizeof(line) - 1;
            memcpy(line, csv.c_str() + start, copyLen);
            line[copyLen] = 0;
            // strip trailing CR
            if (copyLen > 0 && line[copyLen - 1] == '\r') line[copyLen - 1] = 0;

            if (!headerSkipped) {
                headerSkipped = true;
                // Skip header row that starts with "ts_start"
                if (strncmp(line, "ts_start", 8) == 0) {
                    start = eol + 1;
                    continue;
                }
            }

            // Split by commas: ts_start,ts_end,price
            char* c1 = strchr(line, ',');
            if (c1) {
                *c1 = 0;
                char* c2 = strchr(c1 + 1, ',');
                if (c2) {
                    *c2 = 0;
                    const char* tsStart = line;
                    const char* priceStr = c2 + 1;
                    time_t epoch = parseLocalTs(tsStart);
                    float p = atof(priceStr);
                    if (epoch > 0) {
                        _entries[_count].ts_start = epoch;
                        _entries[_count].price_raw = p;
                        _count++;
                    }
                }
            }
        }
        start = eol + 1;
    }

    sortByTime();
    _lastUpdate = time(nullptr);
    return _count;
}

void PriceData::sortByTime() {
    // Simple insertion sort — N is small (<=72).
    for (int i = 1; i < _count; ++i) {
        PriceEntry key = _entries[i];
        int j = i - 1;
        while (j >= 0 && _entries[j].ts_start > key.ts_start) {
            _entries[j + 1] = _entries[j];
            --j;
        }
        _entries[j + 1] = key;
    }
}

const PriceEntry* PriceData::getForTime(time_t epoch) const {
    // Find entry whose [ts_start, ts_start+3600) covers `epoch`.
    for (int i = 0; i < _count; ++i) {
        if (epoch >= _entries[i].ts_start && epoch < _entries[i].ts_start + 3600) {
            return &_entries[i];
        }
    }
    return nullptr;
}

void PriceData::stats(float& outMin, float& outMax, float& outAvg) const {
    if (_count <= 0) { outMin = outMax = outAvg = 0; return; }
    float mn = _entries[0].price_raw;
    float mx = mn;
    float sum = 0;
    for (int i = 0; i < _count; ++i) {
        float p = _entries[i].price_raw;
        if (p < mn) mn = p;
        if (p > mx) mx = p;
        sum += p;
    }
    outMin = mn; outMax = mx; outAvg = sum / _count;
}

bool PriceData::statsForDay(time_t dayEpoch, float& outMin, float& outMax, float& outAvg) const {
    struct tm day;
    localtime_r(&dayEpoch, &day);
    int y = day.tm_year, m = day.tm_mon, d = day.tm_mday;

    bool found = false;
    float mn = 0, mx = 0, sum = 0;
    int n = 0;
    for (int i = 0; i < _count; ++i) {
        struct tm t;
        time_t ts = _entries[i].ts_start;
        localtime_r(&ts, &t);
        if (t.tm_year == y && t.tm_mon == m && t.tm_mday == d) {
            float p = _entries[i].price_raw;
            if (!found) { mn = mx = p; found = true; }
            else { if (p < mn) mn = p; if (p > mx) mx = p; }
            sum += p; n++;
        }
    }
    if (!found) return false;
    outMin = mn; outMax = mx; outAvg = sum / n;
    return true;
}
