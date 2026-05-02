// relay_controller.h — 8-channel relay control via XL9535 I2C expander.
#pragma once

#include <Arduino.h>
#include "config.h"
#include "xl9535.h"

enum RelayMode : uint8_t {
    RMODE_ALWAYS_OFF = 0,
    RMODE_ALWAYS_ON  = 1,
    RMODE_ON_BELOW   = 2,
    RMODE_OFF_ABOVE  = 3,
};

struct RelayRule {
    RelayMode mode;
    float     threshold;   // EUR/kWh, RAW (no VAT)
    char      name[17];
    bool      state;
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
