#include "xl9535.h"

// Register map (matches PCA9555 / TCA9535):
//   0x00  Input port 0
//   0x01  Input port 1
//   0x02  Output port 0
//   0x03  Output port 1
//   0x04  Polarity inversion port 0
//   0x05  Polarity inversion port 1
//   0x06  Configuration port 0  (1 = input, 0 = output)
//   0x07  Configuration port 1
static constexpr uint8_t REG_OUTPUT0 = 0x02;
static constexpr uint8_t REG_CONFIG0 = 0x06;

bool XL9535::begin(TwoWire& bus) {
    _bus = &bus;

    // Probe: try a write to the device address; if nothing ACKs, the chip
    // isn't there and we shouldn't pretend it is.
    _bus->beginTransmission(_addr);
    _ok = (_bus->endTransmission() == 0);
    if (!_ok) return false;

    // All pins as outputs, initial state 0xFFFF (= relays OFF if active-low).
    _state = 0xFFFF;
    bool a = setAllOutputs();
    bool b = writePort(_state);
    return a && b;
}

bool XL9535::setAllOutputs() {
    return writeReg(REG_CONFIG0, 0x0000);
}

bool XL9535::writePort(uint16_t value) {
    if (!writeReg(REG_OUTPUT0, value)) return false;
    _state = value;
    return true;
}

bool XL9535::writePin(uint8_t pin, bool high) {
    if (pin > 15) return false;
    uint16_t next = _state;
    if (high) next |=  (1u << pin);
    else      next &= ~(1u << pin);
    return writePort(next);
}

bool XL9535::writeReg(uint8_t reg, uint16_t value) {
    if (!_bus) return false;
    _bus->beginTransmission(_addr);
    _bus->write(reg);
    _bus->write((uint8_t)(value & 0xFF));         // port 0
    _bus->write((uint8_t)((value >> 8) & 0xFF));  // port 1
    return _bus->endTransmission() == 0;
}
