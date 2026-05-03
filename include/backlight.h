#pragma once

#include <Arduino.h>

class BacklightManager {
public:
    BacklightManager(int ledc_channel, int pin)
        : blPin(pin), blChannel(ledc_channel),
          lastTouchTime(millis()), dimmed(false) {}

    // Call this whenever touch input is detected
    void touchDetected() {
        lastTouchTime = millis();
        if (dimmed) {
            setBacklight(255);  // restore to full brightness
            dimmed = false;
        }
    }

    // Call this periodically from the main loop
    void update() {
        unsigned long now = millis();
        unsigned long inactiveTime = now - lastTouchTime;

        // 60000 ms = 60 seconds = 1 minute
        if (inactiveTime > 60000 && !dimmed) {
            setBacklight(128);  // 50% brightness (128 out of 255)
            dimmed = true;
        }
    }

    // Public method to check if currently dimmed
    bool isDimmed() const { return dimmed; }

private:
    int blPin;
    int blChannel;
    unsigned long lastTouchTime;
    bool dimmed;

    void setBacklight(int brightness) {
        ledcWrite(blChannel, brightness);
    }
};
