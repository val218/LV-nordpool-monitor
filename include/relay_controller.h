// relay_controller.h — 16-channel relay control via two PCF8574s on I2C.
// The two expanders are combined into one logical 16-channel bus so the rest
// of the firmware doesn't have to know how the channels are split.
#pragma once

#include <Arduino.h>
#include <PCF8574.h>
#include "price_data.h"

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

    // Initialize both expanders. Returns true if BOTH responded.
    bool begin(uint8_t addrA, uint8_t addrB);

    void evaluate(float priceRaw);
    void applyStates();

    RelayRule&       rule(int i)       { return _rules[i]; }
    const RelayRule& rule(int i) const { return _rules[i]; }

    void load();
    void save();

    bool available() const { return _okA && _okB; }

private:
    RelayRule _rules[NUM_RELAYS];
    PCF8574   _pcfA;
    PCF8574   _pcfB;
    bool      _okA = false;
    bool      _okB = false;

    void setOutput(int ch, bool on);
};
