// gt911.h — minimal GT911 capacitive touch driver for the JC4827W543.
// Reads a single touch point at a time (LVGL only needs one anyway).
#pragma once

#include <Arduino.h>
#include <Wire.h>

class GT911 {
public:
    GT911(int8_t intPin, int8_t rstPin, uint8_t addr = 0x5D)
        : _intPin(intPin), _rstPin(rstPin), _addr(addr) {}

    bool begin(TwoWire& bus = Wire);

    // Returns true if the screen is currently being touched, and fills x/y
    // with display-pixel coordinates (0..DISP_W-1, 0..DISP_H-1).
    bool read(int16_t& x, int16_t& y);

    bool ok() const { return _ok; }

private:
    TwoWire* _bus = nullptr;
    int8_t   _intPin;
    int8_t   _rstPin;
    uint8_t  _addr;
    bool     _ok = false;

    bool readReg(uint16_t reg, uint8_t* buf, uint8_t len);
    bool writeReg(uint16_t reg, uint8_t value);
};
