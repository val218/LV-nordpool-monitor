#include "relay_controller.h"
#include <Preferences.h>

RelayController::RelayController() : _xl(XL9535_I2C_ADDR) {
    for (int i = 0; i < NUM_RELAYS; ++i) {
        _rules[i].mode      = RMODE_OFF;
        _rules[i].on_below  = 0.10f;
        _rules[i].off_above = 0.12f;   // small default hysteresis for AUTO mode
        _rules[i].state     = false;
        _rules[i].pendingOn = false;
        _rules[i].icon      = 0;       // ICON_GENERIC
        snprintf(_rules[i].name, sizeof(_rules[i].name), "Relay %d", i + 1);
    }
}

bool RelayController::begin(uint8_t addr, TwoWire& bus) {
    _xl = XL9535(addr);
    if (!_xl.begin(bus)) return false;
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
    // Manual override — clear any pending turn-ons (the caller wants
    // exactly what's in _rules[i].state right now to be on the wire).
    for (int i = 0; i < NUM_RELAYS; ++i) _rules[i].pendingOn = false;
    writeAll();
}

// Decide each relay's *desired* state based on its mode and the price.
// Turn-OFFs are applied immediately. Turn-ONs are queued via pendingOn
// and dispatched one per RELAY_STAGGER_SEC by tick(). This avoids inrush
// spikes when several heating loads would otherwise switch on together.
void RelayController::evaluate(float priceRaw) {
    bool offChanged = false;
    for (int i = 0; i < NUM_RELAYS; ++i) {
        RelayRule& r = _rules[i];

        // Compute desired state.
        bool desired = r.state;   // default: hold
        switch (r.mode) {
            case RMODE_OFF: desired = false; break;
            case RMODE_ON:  desired = true;  break;
            case RMODE_PRICE:
                if (priceRaw >= 0) desired = (priceRaw <= r.on_below);
                break;
            case RMODE_AUTO:
                if (priceRaw < 0) break;  // no data — hold
                if      (priceRaw <= r.on_below)  desired = true;
                else if (priceRaw >= r.off_above) desired = false;
                // Between the thresholds: hold previous state.
                break;
        }

        if (desired == r.state) {
            // Already in the right state. If we had a pending turn-on
            // queued and the desired state went back to OFF before the
            // stagger let it through, clear the pending flag.
            if (!desired) r.pendingOn = false;
            continue;
        }

        if (desired) {
            // Want to turn ON — queue it for the staggered dispatcher.
            r.pendingOn = true;
        } else {
            // Want to turn OFF — apply immediately, no stagger.
            r.state     = false;
            r.pendingOn = false;
            offChanged  = true;
        }
    }
    if (offChanged) writeAll();

    // tick() will pick up the pending turn-ons. Call it once here so
    // that the FIRST queued turn-on can fire right away if the stagger
    // window has already elapsed since the previous turn-on; subsequent
    // ones get spaced out as tick() runs from the main loop.
    tick();
}

// Dispatch at most one queued turn-on per stagger window. Lowest-indexed
// pending relay wins, so the user can predict ordering (e.g. relay 1
// before relay 2 if both come due at the same time).
void RelayController::tick() {
    // Find the lowest-indexed relay that wants to turn on.
    int next = -1;
    for (int i = 0; i < NUM_RELAYS; ++i) {
        if (_rules[i].pendingOn) { next = i; break; }
    }
    if (next < 0) return;   // nothing to do

    uint32_t now = millis();
    const uint32_t windowMs = (uint32_t)RELAY_STAGGER_SEC * 1000UL;
    if (windowMs > 0 && _lastTurnOnMs != 0 &&
        (now - _lastTurnOnMs) < windowMs) {
        return;             // still in cool-down — try again next tick
    }

    _rules[next].state     = true;
    _rules[next].pendingOn = false;
    _lastTurnOnMs          = now == 0 ? 1 : now;   // never store 0 (sentinel)
    writeAll();
}

void RelayController::load() {
    Preferences p;
    if (!p.begin(PREFS_NS, true)) return;
    for (int i = 0; i < NUM_RELAYS; ++i) {
        char key[8];
        snprintf(key, sizeof(key), "rm%d", i);
        uint8_t mode = p.getUChar(key, RMODE_OFF);
        // Migrate legacy mode codes:
        //   Old layout was OFF=0, ON=1, ON_BELOW=2, OFF_ABOVE=3.
        //   The middle "AUTO" experiment used OFF=0, ON=1, AUTO=2.
        //   New layout is OFF=0, ON=1, PRICE=2, AUTO=3.
        // A saved value of 2 from the AUTO-only era should map to PRICE
        // (no hysteresis is the safer default; user can reconfigure to
        // AUTO if they want hysteresis back). A saved value of 3 from
        // the legacy 4-mode era was OFF_ABOVE — also map to PRICE.
        if (mode > RMODE_AUTO) mode = RMODE_OFF;
        _rules[i].mode = (RelayMode)mode;

        snprintf(key, sizeof(key), "rb%d", i);
        float onBelow = p.getFloat(key, NAN);
        snprintf(key, sizeof(key), "ra%d", i);
        float offAbove = p.getFloat(key, NAN);
        if (isnan(onBelow) || isnan(offAbove)) {
            // Fall back to the very old single-threshold "rt" key.
            snprintf(key, sizeof(key), "rt%d", i);
            float legacy = p.getFloat(key, 0.10f);
            if (isnan(onBelow))  onBelow  = legacy;
            if (isnan(offAbove)) offAbove = legacy + 0.02f;
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

        // pendingOn is runtime-only.
        _rules[i].pendingOn = false;
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
