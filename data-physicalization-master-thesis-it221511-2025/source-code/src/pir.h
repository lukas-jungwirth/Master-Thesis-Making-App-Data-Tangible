#ifndef PIR_H
#define PIR_H

#include <Arduino.h>

namespace PIRConfig {
    constexpr int defaultPirPin = 6;
    constexpr unsigned long motionDuration = 30000;
}
class PIRSensor {
private:
    int pirPin;
    int motionStatus;
    int pirState;
    unsigned long lastMotionTime;

public:
    PIRSensor(int pin = PIRConfig::defaultPirPin)
        : pirPin(pin), motionStatus(0), pirState(0), lastMotionTime(0) {}

    // Initialize the PIR sensor
    void initPir() {
        pinMode(pirPin, INPUT);
    }

    // Get motion status
    int getMotionStatus() {
        motionStatus = digitalRead(pirPin);

        if (motionStatus == HIGH) {
            lastMotionTime = millis();
            if (pirState == 0) {
                pirState = 1;
                return 2; // First motion detected
            } else {
                return 1; // Continuous motion
            }
        } else {
            // Check if delay time has passed
            if ((millis() - lastMotionTime) < PIRConfig::motionDuration) {
                pirState = 1;
                return 1;
            }

            if (pirState == 1) {
                pirState = 0;
                return -1; // Motion stopped
            }

            return 0; // No motion
        }
    }
};

#endif // PIR_H
