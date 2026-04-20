#include "relay_controller.h"
#include <Preferences.h>

RelayController::RelayController() : _pcfA(0x20), _pcfB(0x21) {
    for (int i = 0; i < NUM_RELAYS; ++i) {
        _rules[i].mode = RMODE_ALWAYS_OFF;
        _rules[i].threshold = 0.10f;
        _rules[i].state = false;
        snprintf(_rules[i].name, sizeof(_rules[i].name), "Relay %d", i + 1);
    }
}

bool RelayController::begin(uint8_t addrA, uint8_t addrB) {
    _pcfA = PCF8574(addrA);
    _pcfB = PCF8574(addrB);
    _okA = _pcfA.begin();
    _okB = _pcfB.begin();
    if (_okA || _okB) {
        // Drive all outputs to the "off" state on both expanders up front, so
        // the relays don't chatter if the user configured some as ALWAYS_ON
        // later — they'll flip on after the first evaluate() pass.
        for (int i = 0; i < NUM_RELAYS; ++i) setOutput(i, _rules[i].state);
    }
    return _okA && _okB;
}

void RelayController::setOutput(int ch, bool on) {
    if (ch < 0 || ch >= NUM_RELAYS) return;
    // Channels 0..7 live on expander A; channels 8..15 live on expander B.
    PCF8574& pcf = (ch < 8) ? _pcfA : _pcfB;
    bool ok      = (ch < 8) ? _okA  : _okB;
    if (!ok) return;
    int pin = ch & 0x07;
    bool pinHigh = RELAY_ACTIVE_LOW ? !on : on;
    pcf.write(pin, pinHigh ? HIGH : LOW);
}

void RelayController::applyStates() {
    for (int i = 0; i < NUM_RELAYS; ++i) setOutput(i, _rules[i].state);
}

void RelayController::evaluate(float priceRaw) {
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
            setOutput(i, newState);
        }
    }
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
    }
    p.end();
}
