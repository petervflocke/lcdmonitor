#pragma once
#include <Arduino.h>

class RotaryEncoder {
public:
    static void init(uint8_t pinA, uint8_t pinB) {
        _pinA = pinA;
        _pinB = pinB;
        pinMode(_pinA, INPUT_PULLUP);
        pinMode(_pinB, INPUT_PULLUP);
        _lastEncoded = (digitalRead(_pinB) << 1) | digitalRead(_pinA);
    }

    static void handleInterrupt() {
        // Get current state
        uint8_t MSB = digitalRead(_pinA);
        uint8_t LSB = digitalRead(_pinB);
        uint8_t encoded = (MSB << 1) | LSB;
        
        uint8_t sum = (_lastEncoded << 2) | encoded;
        _lastEncoded = encoded;

        // Table for valid rotations and their directions
        if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) {
            _position++;
            _changed = true;
        }
        else if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) {
            _position--;
            _changed = true;
        }
    }

    static int16_t getMovement() {
        noInterrupts();
        int16_t change = 0;
        if (_changed) {
            // Convert to full steps
            change = _position >> 2;  // Divide by 4 to get full steps
            _position &= 0x03;        // Keep remainder
            _changed = false;
        }
        interrupts();
        return change;
    }

private:
    static uint8_t _pinA;
    static uint8_t _pinB;
    static uint8_t _lastEncoded;
    static volatile int16_t _position;
    static volatile bool _changed;
};