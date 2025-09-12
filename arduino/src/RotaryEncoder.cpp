#include "RotaryEncoder.h"

uint8_t RotaryEncoder::_pinA = 0;
uint8_t RotaryEncoder::_pinB = 0;
uint8_t RotaryEncoder::_lastEncoded = 0;
volatile int16_t RotaryEncoder::_position = 0;
volatile bool RotaryEncoder::_changed = false;