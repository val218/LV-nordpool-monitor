#include "relay_controller.h"
#include <Preferences.h>

RelayController::RelayController() : _xl(XL9535_I2C_ADDR) {
    for (int i = 0; i < NUM_RELAYS; ++i) {
        _rules[i].mode = RMODE_ALWAYS_OFF;
        _rules[i].threshold = 0.10f;
        _rules[i].state = false;
        _rules[i].icon = 0;   // ICON_GENERIC
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
            case RMODE_ON_BELOW:
                if (priceRaw >= 0) newState = (priceRaw <= r.threshold);
                break;
            case RMODE_OFF_ABOVE:
                if (priceRaw >= 0) newState = (priceRaw < r.threshold);
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
        _rules[i].mode = (RelayMode)p.getUChar(key, RMODE_ALWAYS_OFF);
        snprintf(key, sizeof(key), "rt%d", i);
        _rules[i].threshold = p.getFloat(key, 0.10f);
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
        snprintf(key, sizeof(key), "rt%d", i);
        p.putFloat(key, _rules[i].threshold);
        snprintf(key, sizeof(key), "rn%d", i);
        p.putString(key, _rules[i].name);
        snprintf(key, sizeof(key), "ri%d", i);
        p.putUChar(key, _rules[i].icon);
    }
    p.end();
}
