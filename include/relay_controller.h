// relay_controller.h — 8-channel relay control via XL9535 I2C expander.
#pragma once

#include <Arduino.h>
#include "config.h"
#include "xl9535.h"

// Relay operating mode.
//
//   OFF    - relay never energized.
//   ON     - relay always energized.
//   PRICE  - simple single-threshold switch, no hysteresis:
//              ON  when price <= on_below
//              OFF when price >  on_below
//            (the off_above field is ignored in this mode)
//   AUTO   - two-threshold hysteresis:
//              ON  when price <= on_below
//              OFF when price >= off_above
//            Between the two: hold previous state. Setting on_below
//            below off_above prevents rapid flapping when price hovers
//            around the cut-in threshold. Setting on_below > off_above
//            is invalid (the relay would never turn on); the UI flags
//            this in red but doesn't actively prevent it.
enum RelayMode : uint8_t {
    RMODE_OFF   = 0,
    RMODE_ON    = 1,
    RMODE_PRICE = 2,
    RMODE_AUTO  = 3,
};

// Legacy aliases — retained so existing call sites still compile.
// New code should prefer RMODE_OFF/RMODE_ON over the *_ALWAYS_* names.
static const RelayMode RMODE_ALWAYS_OFF = RMODE_OFF;
static const RelayMode RMODE_ALWAYS_ON  = RMODE_ON;

struct RelayRule {
    RelayMode mode;
    float     on_below;    // EUR/kWh, RAW (no VAT) — turn ON when price <= this
    float     off_above;   // EUR/kWh, RAW (no VAT) — turn OFF when price >= this
                           // (only used in AUTO mode; PRICE mode ignores this)
    char      name[17];
    bool      state;       // currently committed state (what's on the expander)
    uint8_t   icon;        // RelayIcon enum value (see relay_icons.h)

    // Pending switch-on requested by evaluate() but held back by the
    // stagger limiter. False once the request has been committed via
    // applyStates() / tick().
    bool      pendingOn;
};

class RelayController {
public:
    RelayController();

    bool begin(uint8_t addr = XL9535_I2C_ADDR, TwoWire& bus = Wire);

    // Re-evaluate every rule against the current price. This DOES NOT
    // immediately apply turn-ON requests — it queues them and tick()
    // dispatches one per RELAY_STAGGER_SEC seconds. Turn-OFF is applied
    // immediately. Pass priceRaw < 0 to indicate no data (rules then
    // hold their previous state).
    void evaluate(float priceRaw);

    // Drive the staggered turn-on queue. Should be called from the main
    // loop at a cadence of at least once per second. Cheap: returns
    // immediately if no work is pending.
    void tick();

    // Commit all currently-known state to the expander immediately,
    // bypassing the stagger queue. Used after manual configuration
    // changes from the UI where the user expects an instant response.
    void applyStates();

    RelayRule&       rule(int i)       { return _rules[i]; }
    const RelayRule& rule(int i) const { return _rules[i]; }

    void load();
    void save();

    bool available() const { return _xl.ok(); }

private:
    RelayRule _rules[NUM_RELAYS];
    XL9535    _xl;

    // millis() timestamp of the most recent staggered turn-ON commit,
    // or 0 if none has happened yet (in which case the next pendingOn
    // is dispatched immediately).
    uint32_t  _lastTurnOnMs = 0;

    void setOutput(int ch, bool on);
    void writeAll();
};
