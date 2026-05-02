#include "gt911.h"

// GT911 register map — only the few registers we touch.
static constexpr uint16_t GT911_REG_STATUS = 0x814E;  // touch detection / point count
static constexpr uint16_t GT911_REG_POINT1 = 0x8150;  // first touch point: x_low,x_high,y_low,y_high,size_low,size_high

bool GT911::begin(TwoWire& bus) {
    _bus = &bus;

    // GT911 reset / address-select sequence:
    //  1. Drive INT and RST low for >10ms.
    //  2. Set INT to the address-select state (LOW = 0x5D, HIGH = 0x14)
    //     and release RST.
    //  3. Hold INT in that state for >5ms after RST goes high, then float INT.
    if (_rstPin >= 0) pinMode(_rstPin, OUTPUT);
    if (_intPin >= 0) pinMode(_intPin, OUTPUT);

    if (_rstPin >= 0) digitalWrite(_rstPin, LOW);
    if (_intPin >= 0) digitalWrite(_intPin, LOW);
    delay(15);

    // Choose address per the address pin
    bool intHigh = (_addr == 0x14);
    if (_intPin >= 0) digitalWrite(_intPin, intHigh ? HIGH : LOW);
    delayMicroseconds(120);

    if (_rstPin >= 0) digitalWrite(_rstPin, HIGH);
    delay(10);

    if (_intPin >= 0) {
        pinMode(_intPin, INPUT);   // float INT — chip will use it as IRQ output
    }
    delay(50);

    // Probe: read product ID register at 0x8140 — should respond with "911".
    uint8_t id[4] = {0};
    if (!readReg(0x8140, id, 4)) {
        // Some boards ship with the alternate address; try the other one.
        _addr = (_addr == 0x5D) ? 0x14 : 0x5D;
        if (!readReg(0x8140, id, 4)) {
            _ok = false;
            return false;
        }
    }
    _ok = true;
    return true;
}

bool GT911::read(int16_t& x, int16_t& y) {
    if (!_ok) return false;
    uint8_t status = 0;
    if (!readReg(GT911_REG_STATUS, &status, 1)) return false;

    uint8_t pointCount = status & 0x0F;
    bool touched = ((status & 0x80) != 0) && (pointCount >= 1);

    // The status register must be cleared after reading — otherwise the chip
    // won't update it on the next touch event.
    writeReg(GT911_REG_STATUS, 0x00);

    if (!touched) return false;

    uint8_t buf[4];
    if (!readReg(GT911_REG_POINT1, buf, 4)) return false;

    x = (int16_t)(buf[0] | (buf[1] << 8));
    y = (int16_t)(buf[2] | (buf[3] << 8));
    return true;
}

bool GT911::readReg(uint16_t reg, uint8_t* buf, uint8_t len) {
    if (!_bus) return false;
    _bus->beginTransmission(_addr);
    _bus->write((uint8_t)(reg >> 8));
    _bus->write((uint8_t)(reg & 0xFF));
    if (_bus->endTransmission(false) != 0) return false;

    uint8_t got = _bus->requestFrom((uint8_t)_addr, (uint8_t)len);
    if (got != len) return false;
    for (uint8_t i = 0; i < len; ++i) buf[i] = _bus->read();
    return true;
}

bool GT911::writeReg(uint16_t reg, uint8_t value) {
    if (!_bus) return false;
    _bus->beginTransmission(_addr);
    _bus->write((uint8_t)(reg >> 8));
    _bus->write((uint8_t)(reg & 0xFF));
    _bus->write(value);
    return _bus->endTransmission() == 0;
}
