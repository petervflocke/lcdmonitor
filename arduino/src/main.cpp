#include <Arduino.h>
#include <LiquidCrystal.h>
#include "ScrollBuffer.h"

// LCD pins: RS=7, E=8, D4=9, D5=10, D6=11, D7=12
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

// Rotary encoder pins
constexpr uint8_t PIN_ENC_A = 2;  // D2
constexpr uint8_t PIN_ENC_B = 3;  // D3
constexpr uint8_t PIN_BTN = 4;    // D4

volatile uint8_t enc_prev = 0;
volatile int16_t enc_delta = 0;  // accumulated steps

ScrollBuffer buffer;
int16_t scroll = 0;

void onEncChange() {
  // Read both pins and decode quadrature
  uint8_t a = digitalRead(PIN_ENC_A);
  uint8_t b = digitalRead(PIN_ENC_B);
  uint8_t state = (a << 1) | b;
  uint8_t prev = enc_prev;
  enc_prev = state;

  // Transition table for clockwise = +1, counter = -1, others = 0
  int8_t dir = 0;
  switch ((prev << 2) | state) {
    case 0b0001:
    case 0b0111:
    case 0b1110:
    case 0b1000:
      dir = +1;  // CW
      break;
    case 0b0010:
    case 0b0100:
    case 0b1101:
    case 0b1011:
      dir = -1;  // CCW
      break;
    default:
      break;
  }
  enc_delta += dir;
}

static void render() {
  lcd.clear();
  char line[ScrollBuffer::kWidth + 1];
  for (uint8_t row = 0; row < 4; ++row) {
    uint16_t idx = scroll + row;
    buffer.get(idx, line);
    lcd.setCursor(0, row);
    // Ensure line padded/cleared
    char padded[ScrollBuffer::kWidth + 1];
    uint8_t i = 0;
    for (; i < ScrollBuffer::kWidth && line[i] != '\0'; ++i) padded[i] = line[i];
    for (; i < ScrollBuffer::kWidth; ++i) padded[i] = ' ';
    padded[ScrollBuffer::kWidth] = '\0';
    lcd.print(padded);
  }
}

void setup() {
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_BTN, INPUT_PULLUP);

  // Initialize prev state
  enc_prev = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), onEncChange, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), onEncChange, CHANGE);

  lcd.begin(20, 4);
  lcd.clear();

  // Seed buffer with demo lines for Phase 2
  buffer.clear();
  buffer.push("Hello World");
  for (uint8_t i = 0; i < 20; ++i) {
    char msg[21];
    snprintf(msg, sizeof(msg), "Item %02u - scroll", i);
    buffer.push(msg);
  }
  render();
}

void loop() {
  noInterrupts();
  int16_t delta = enc_delta;
  enc_delta = 0;
  interrupts();

  if (delta != 0) {
    int16_t maxScroll = 0;
    if (buffer.size() > 4) {
      maxScroll = static_cast<int16_t>(buffer.size() - 4);
    }
    scroll += delta;
    if (scroll < 0) scroll = 0;
    if (scroll > maxScroll) scroll = maxScroll;
    render();
  }

  delay(5);
}
