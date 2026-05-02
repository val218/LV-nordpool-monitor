#include "relay_controller.h"
#include <Preferences.h>

RelayController::RelayController() : _xl(XL9535_I2C_ADDR) {
    for (int i = 0; i < NUM_RELAYS; ++i) {
        _rules[i].mode      = RMODE_ALWAYS_OFF;
        _rules[i].on_below  = 0.10f;   // turn on when price <= 10 c/kWh
        _rules[i].off_above = 0.10f;   // and off when >= same value (no hysteresis by default)
        _rules[i].state     = false;
        _rules[i].icon      = 0;       // ICON_GENERIC
        snprintf(_rules[i].name, sizeof(_rules[i].name), "Relay %d", i + 1);
    }
}

bool RelayController::begin(uint8_t addr) {
    _xl = XL9535(addr);
    if (!_xl.begin()) return false;
    // Initial state: write all rule states to expander now that hardware exists.
    writeAll();
    return true;
}

void RelayController::setOutput(int ch, bool on) {
    if (ch < 0 || ch >= NUM_RELAYS) return;
    bool pinHigh = RELAY_ACTIVE_LOW ? !on : on;
    _xl.writePin((uint8_t)ch, pinHigh);
}

void RelayController::writeAll() {
    // Build a 16-bit value for the expander in a single I2C transaction —
    // cheaper than 8 individual writePin() calls during bulk updates.
    uint16_t v = 0xFFFF;
    for (int i = 0; i < NUM_RELAYS; ++i) {
        bool pinHigh = RELAY_ACTIVE_LOW ? !_rules[i].state : _rules[i].state;
        if (pinHigh) v |=  (1u << i);
        else         v &= ~(1u << i);
    }
    _xl.writePort(v);
}

void RelayController::applyStates() {
    writeAll();
}

void RelayController::evaluate(float priceRaw) {
    bool changed = false;
    for (int i = 0; i < NUM_RELAYS; ++i) {
        RelayRule& r = _rules[i];
        bool newState = r.state;
        switch (r.mode) {
            case RMODE_ALWAYS_OFF: newState = false; break;
            case RMODE_ALWAYS_ON:  newState = true;  break;
            case RMODE_AUTO:
                if (priceRaw < 0) break;  // no data — hold previous state
                // Hysteresis. When the two thresholds are equal this still
                // works as a clean single-threshold switch (ON when price
                // is at or below the threshold, OFF when above).
                if (priceRaw <= r.on_below) {
                    newState = true;
                } else if (priceRaw >= r.off_above) {
                    newState = false;
                }
                // Between on_below and off_above: hold previous state.
                break;
        }
        if (newState != r.state) {
            r.state = newState;
            changed = true;
        }
    }
    if (changed) writeAll();
}

void RelayController::load() {
    Preferences p;
    if (!p.begin(PREFS_NS, true)) return;
    for (int i = 0; i < NUM_RELAYS; ++i) {
        char key[8];
        snprintf(key, sizeof(key), "rm%d", i);
        uint8_t mode = p.getUChar(key, RMODE_ALWAYS_OFF);
        // Migrate legacy modes (when 0..3 was the enum and threshold was a
        // single float). Any saved value of 2 (was ON_BELOW) or 3 (was
        // OFF_ABOVE) becomes RMODE_AUTO with both thresholds equal — same
        // behavior as the old single-threshold rule.
        if (mode == 3) mode = RMODE_AUTO;     // OFF_ABOVE -> AUTO
        if (mode == 2) mode = RMODE_AUTO;     // ON_BELOW  -> AUTO
        if (mode > RMODE_AUTO) mode = RMODE_ALWAYS_OFF;
        _rules[i].mode = (RelayMode)mode;

        // Try the new keys first; fall back to legacy "rt" (threshold) key
        // so existing devices upgrade smoothly.
        snprintf(key, sizeof(key), "rb%d", i);
        float onBelow = p.getFloat(key, NAN);
        snprintf(key, sizeof(key), "ra%d", i);
        float offAbove = p.getFloat(key, NAN);
        if (isnan(onBelow) || isnan(offAbove)) {
            snprintf(key, sizeof(key), "rt%d", i);
            float legacy = p.getFloat(key, 0.10f);
            if (isnan(onBelow))  onBelow  = legacy;
            if (isnan(offAbove)) offAbove = legacy;
        }
        _rules[i].on_below  = onBelow;
        _rules[i].off_above = offAbove;

        snprintf(key, sizeof(key), "rn%d", i);
        String nm = p.getString(key, "");
        if (nm.length()) {
            strncpy(_rules[i].name, nm.c_str(), sizeof(_rules[i].name) - 1);
            _rules[i].name[sizeof(_rules[i].name) - 1] = 0;
        }
        snprintf(key, sizeof(key), "ri%d", i);
        _rules[i].icon = p.getUChar(key, 0);
    }
    p.end();
}

void RelayController::save() {
    Preferences p;
    if (!p.begin(PREFS_NS, false)) return;
    for (int i = 0; i < NUM_RELAYS; ++i) {
        char key[8];
        snprintf(key, sizeof(key), "rm%d", i);
        p.putUChar(key, (uint8_t)_rules[i].mode);
        snprintf(key, sizeof(key), "rb%d", i);
        p.putFloat(key, _rules[i].on_below);
        snprintf(key, sizeof(key), "ra%d", i);
        p.putFloat(key, _rules[i].off_above);
        snprintf(key, sizeof(key), "rn%d", i);
        p.putString(key, _rules[i].name);
        snprintf(key, sizeof(key), "ri%d", i);
        p.putUChar(key, _rules[i].icon);
    }
    p.end();
}
