// relay_controller.h — 8-channel relay control via XL9535 I2C expander.
#pragma once

#include <Arduino.h>
#include "config.h"
#include "xl9535.h"

// Relay mode. Three modes:
//   ALWAYS_OFF  - relay never energized
//   ALWAYS_ON   - relay always energized
//   AUTO        - hysteresis-controlled by price:
//                   ON  when price <= on_below
//                   OFF when price >= off_above
//                   Between the two: hold previous state.
//                 Setting on_below == off_above makes the rule a single-
//                 threshold switch (the simple "ON below X" case).
//                 Setting on_below < off_above gives true hysteresis,
//                 preventing rapid flapping when price hovers near the
//                 cut-in threshold.
//                 Setting on_below > off_above is invalid (the controller
//                 will then never turn the relay on); the UI prevents this.
enum RelayMode : uint8_t {
    RMODE_ALWAYS_OFF = 0,
    RMODE_ALWAYS_ON  = 1,
    RMODE_AUTO       = 2,
};

struct RelayRule {
    RelayMode mode;
    float     on_below;    // EUR/kWh, RAW (no VAT) — turn ON when price <= this
    float     off_above;   // EUR/kWh, RAW (no VAT) — turn OFF when price >= this
    char      name[17];
    bool      state;
    uint8_t   icon;        // RelayIcon enum value (see relay_icons.h)
};

class RelayController {
public:
    RelayController();

    bool begin(uint8_t addr = XL9535_I2C_ADDR);

    void evaluate(float priceRaw);
    void applyStates();

    RelayRule&       rule(int i)       { return _rules[i]; }
    const RelayRule& rule(int i) const { return _rules[i]; }

    void load();
    void save();

    bool available() const { return _xl.ok(); }

private:
    RelayRule _rules[NUM_RELAYS];
    XL9535    _xl;

    void setOutput(int ch, bool on);
    void writeAll();
};
