// xl9535.h — minimal driver for the XL9535 16-bit I2C IO expander.
// Only the registers we need: configuration (input/output), output port.
// Compatible with the PCA9555/TCA9535 register layout.
#pragma once

#include <Arduino.h>
#include <Wire.h>

class XL9535 {
public:
    explicit XL9535(uint8_t addr = 0x20) : _addr(addr) {}

    bool begin(TwoWire& bus = Wire);

    // Configure all 16 pins as outputs (the typical relay-board use case).
    bool setAllOutputs();

    // Write the full 16-bit output value (port 0 = low byte, port 1 = high byte).
    bool writePort(uint16_t value);

    // Toggle a single pin (0..15). Reads-modifies-writes the cached state.
    bool writePin(uint8_t pin, bool high);

    // Last value successfully written; useful for read-back / UI.
    uint16_t lastValue() const { return _state; }

    bool ok() const { return _ok; }

private:
    TwoWire* _bus = nullptr;
    uint8_t  _addr;
    uint16_t _state = 0xFFFF;   // outputs high by default (relays off if active-low)
    bool     _ok = false;

    bool writeReg(uint8_t reg, uint16_t value);
};
