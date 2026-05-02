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

    // The Nordpool source returns the entire historical CSV (often 1+ MB),
    // not just today and tomorrow. We only care about a narrow window:
    // yesterday's hours (so the chart can show "yesterday vs today") plus
    // today plus tomorrow's hours if they're already published. Filter
    // aggressively during parsing so we don't drown in old data.
    time_t now = time(nullptr);
    time_t windowStart = (now > 100000) ? (now - 24 * 3600) : 0;
    time_t windowEnd   = (now > 100000) ? (now + 48 * 3600) : 0x7FFFFFFFL;
    bool   timeKnown   = (now > 100000);

    int kept = 0, scanned = 0;

    while (start < len) {
        int eol = csv.indexOf('\n', start);
        if (eol < 0) eol = len;
        int lineLen = eol - start;
        if (lineLen > 0) {
            // Copy line into a small stack buffer (max ~60 chars expected).
            char line[96];
            int copyLen = lineLen < (int)sizeof(line) - 1 ? lineLen : (int)sizeof(line) - 1;
            memcpy(line, csv.c_str() + start, copyLen);
            line[copyLen] = 0;
            if (copyLen > 0 && line[copyLen - 1] == '\r') line[copyLen - 1] = 0;

            if (!headerSkipped) {
                headerSkipped = true;
                if (strncmp(line, "ts_start", 8) == 0) {
                    start = eol + 1;
                    continue;
                }
            }
            scanned++;

            // Split: ts_start,ts_end,price
            char* c1 = strchr(line, ',');
            if (c1) {
                *c1 = 0;
                char* c2 = strchr(c1 + 1, ',');
                if (c2) {
                    *c2 = 0;
                    time_t epoch = parseLocalTs(line);
                    float p = atof(c2 + 1);
                    // Only keep rows whose timestamp is within the window of
                    // interest. If time isn't yet synced (NTP hasn't run),
                    // keep everything until we run out of buffer — better
                    // than nothing on a cold boot.
                    bool inWindow = !timeKnown ||
                                    (epoch >= windowStart && epoch <= windowEnd);
                    if (epoch > 0 && inWindow) {
                        if (_count < MAX_PRICE_ENTRIES) {
                            _entries[_count].ts_start = epoch;
                            _entries[_count].price_raw = p;
                            _count++;
                            kept++;
                        }
                        // If buffer is full but we have time sync, we could
                        // still encounter newer/better entries later — so
                        // we don't stop early. The buffer will just hold the
                        // first MAX_PRICE_ENTRIES matches. With the window
                        // filter that's at most 72 hours of data which fits.
                    }
                }
            }
        }
        start = eol + 1;
    }

    sortByTime();
    _lastUpdate = time(nullptr);
    Serial.printf("[PRC] scanned %d rows, kept %d entries in window\n",
                  scanned, kept);
    return _count;
}

// ---------- e-cena.lv HTML scraper ----------
//
// The page layout is small and predictable:
//
//   <h4>02-05-2026</h4>
//   <table>
//     <tr><th>Cena</th><th>Laiks</th></tr>
//     <tr><td>0.010000</td><td>00:00</td></tr>
//     <tr><td>0.009210</td><td>00:15</td></tr>
//     ...
//   </table>
//   <h4>03-05-2026</h4>
//   ...
//
// We do a single linear pass: find <h4> for current date, then collect
// price/time pairs until the next <h4>. Each row is two consecutive <td>'s.
// We accumulate 4 quarter-hour prices per hour and average them.

namespace {

// Find the next occurrence of `needle` in `s` starting at `from`. Returns
// the position of `needle` itself, or -1.
int findFrom(const String& s, const char* needle, int from) {
    return s.indexOf(needle, from);
}

// Extract the text content between two positions, trimmed.
String slice(const String& s, int begin, int end) {
    if (begin < 0 || end < 0 || end <= begin) return String();
    String out = s.substring(begin, end);
    out.trim();
    return out;
}

// Parse "DD-MM-YYYY" into a tm with hour/min/sec zeroed. Returns true on
// success.
bool parseDDMMYYYY(const String& s, struct tm& out) {
    if (s.length() < 10) return false;
    out = {};
    out.tm_mday = s.substring(0, 2).toInt();
    out.tm_mon  = s.substring(3, 5).toInt() - 1;
    out.tm_year = s.substring(6, 10).toInt() - 1900;
    if (out.tm_year < 100 || out.tm_mon < 0 || out.tm_mday < 1) return false;
    return true;
}

// Parse "HH:MM" into hours and minutes. Returns true on success.
bool parseHHMM(const String& s, int& hh, int& mm) {
    if (s.length() < 5) return false;
    hh = s.substring(0, 2).toInt();
    mm = s.substring(3, 5).toInt();
    return hh >= 0 && hh < 24 && mm >= 0 && mm < 60;
}

}  // namespace

int PriceData::parseEcenaHtml(const String& html) {
    _count = 0;

    // Bucket: 24 hours × up to 3 days (yesterday/today/tomorrow padding).
    // Key it by (epoch-of-hour-start). We accumulate sum + count then average.
    // Using a flat array sized for ~96 hours is enough headroom.
    constexpr int MAX_BUCKETS = 96;
    struct Bucket {
        time_t hourStart = 0;
        float  sum = 0;
        int    count = 0;
    };
    Bucket buckets[MAX_BUCKETS];
    int bucketCount = 0;

    auto findOrCreateBucket = [&](time_t hourStart) -> Bucket* {
        for (int i = 0; i < bucketCount; ++i) {
            if (buckets[i].hourStart == hourStart) return &buckets[i];
        }
        if (bucketCount >= MAX_BUCKETS) return nullptr;
        buckets[bucketCount].hourStart = hourStart;
        buckets[bucketCount].sum = 0;
        buckets[bucketCount].count = 0;
        return &buckets[bucketCount++];
    };

    // Walk the document linearly. Each iteration either advances past one
    // <h4> (capturing the date) or one <tr> (capturing a price/time pair
    // when the row is a data row).
    int pos = 0;
    struct tm currentDay = {};
    bool haveDate = false;

    while (pos < (int)html.length()) {
        int h4 = findFrom(html, "<h4>", pos);
        int tr = findFrom(html, "<tr>", pos);

        // Pick whichever comes first (and is non-negative).
        int next;
        if (h4 < 0 && tr < 0) break;
        if (h4 < 0) next = tr;
        else if (tr < 0) next = h4;
        else next = (h4 < tr) ? h4 : tr;

        if (next == h4) {
            int end = findFrom(html, "</h4>", h4);
            if (end < 0) break;
            String txt = slice(html, h4 + 4, end);
            if (parseDDMMYYYY(txt, currentDay)) {
                haveDate = true;
            }
            pos = end + 5;
        } else {
            // <tr>...</tr> — extract the two <td> contents
            int rowEnd = findFrom(html, "</tr>", tr);
            if (rowEnd < 0) break;

            int td1 = findFrom(html, "<td>", tr);
            if (td1 < 0 || td1 > rowEnd) { pos = rowEnd + 5; continue; }
            int td1End = findFrom(html, "</td>", td1);
            if (td1End < 0 || td1End > rowEnd) { pos = rowEnd + 5; continue; }
            int td2 = findFrom(html, "<td>", td1End);
            if (td2 < 0 || td2 > rowEnd) { pos = rowEnd + 5; continue; }
            int td2End = findFrom(html, "</td>", td2);
            if (td2End < 0 || td2End > rowEnd) { pos = rowEnd + 5; continue; }

            String priceStr = slice(html, td1 + 4, td1End);
            String timeStr  = slice(html, td2 + 4, td2End);

            int hh, mm;
            if (haveDate && parseHHMM(timeStr, hh, mm)) {
                struct tm t = currentDay;
                t.tm_hour = hh;
                t.tm_min  = 0;     // bucket by hour start regardless of quarter
                t.tm_sec  = 0;
                t.tm_isdst = -1;
                time_t hourStart = mktime(&t);
                if (hourStart > 0) {
                    Bucket* b = findOrCreateBucket(hourStart);
                    if (b) {
                        b->sum += priceStr.toFloat();
                        b->count++;
                    }
                }
            }

            pos = rowEnd + 5;
        }
    }

    // Convert buckets into hourly entries, applying the same time window
    // filter we use for CSV parses (yesterday → day after tomorrow).
    time_t now = time(nullptr);
    time_t windowStart = (now > 100000) ? (now - 24 * 3600) : 0;
    time_t windowEnd   = (now > 100000) ? (now + 48 * 3600) : 0x7FFFFFFFL;
    bool   timeKnown   = (now > 100000);

    int kept = 0;
    for (int i = 0; i < bucketCount && _count < MAX_PRICE_ENTRIES; ++i) {
        if (buckets[i].count == 0) continue;
        bool inWindow = !timeKnown ||
                        (buckets[i].hourStart >= windowStart &&
                         buckets[i].hourStart <= windowEnd);
        if (!inWindow) continue;

        _entries[_count].ts_start  = buckets[i].hourStart;
        _entries[_count].price_raw = buckets[i].sum / buckets[i].count;
        _count++;
        kept++;
    }

    sortByTime();
    _lastUpdate = time(nullptr);
    Serial.printf("[PRC] ecena: %d buckets parsed, %d hourly entries kept\n",
                  bucketCount, kept);
    return _count;
}


void PriceData::beginStream() {
    _count = 0;
    _streamHeaderSkipped = false;
    _streamScanned = 0;
    _streamKept = 0;

    // Set up the time window once at the start of streaming so each line
    // doesn't recompute it. Window: yesterday → day-after-tomorrow.
    time_t now = time(nullptr);
    if (now > 100000) {
        _streamWindowStart = now - 24 * 3600;
        _streamWindowEnd   = now + 48 * 3600;
        _streamTimeKnown   = true;
    } else {
        _streamWindowStart = 0;
        _streamWindowEnd   = 0x7FFFFFFFL;
        _streamTimeKnown   = false;
    }
}

void PriceData::feedLine(const String& line) {
    if (!_streamHeaderSkipped) {
        _streamHeaderSkipped = true;
        if (line.startsWith("ts_start")) return;  // skip CSV header row
    }
    _streamScanned++;

    // Bail out cheaply if buffer is already full and we have a known time
    // window — we won't be storing any more anyway.
    if (_count >= MAX_PRICE_ENTRIES) return;

    // Split on commas. Use char-buffer instead of String::indexOf to avoid
    // creating temporary substrings on the heap (this gets called ~37k times).
    char buf[96];
    int copyLen = (int)line.length();
    if (copyLen >= (int)sizeof(buf)) copyLen = sizeof(buf) - 1;
    memcpy(buf, line.c_str(), copyLen);
    buf[copyLen] = 0;

    char* c1 = strchr(buf, ',');
    if (!c1) return;
    *c1 = 0;
    char* c2 = strchr(c1 + 1, ',');
    if (!c2) return;
    *c2 = 0;

    time_t epoch = parseLocalTs(buf);
    if (epoch <= 0) return;

    bool inWindow = !_streamTimeKnown ||
                    (epoch >= _streamWindowStart && epoch <= _streamWindowEnd);
    if (!inWindow) return;

    _entries[_count].ts_start  = epoch;
    _entries[_count].price_raw = atof(c2 + 1);
    _count++;
    _streamKept++;
}

void PriceData::endStream() {
    sortByTime();
    _lastUpdate = time(nullptr);
    Serial.printf("[PRC] streamed %d rows, kept %d entries in window\n",
                  _streamScanned, _streamKept);
}


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
