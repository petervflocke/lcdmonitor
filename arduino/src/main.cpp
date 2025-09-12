#include <Arduino.h>
#include <LiquidCrystal.h>
#include "ScrollBuffer.h"
#include "RotaryEncoder.h"

// LCD pins: RS=7, E=8, D4=9, D5=10, D6=11, D7=12
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

// Rotary encoder pins
constexpr uint8_t PIN_ENC_A = 2;  // D2
constexpr uint8_t PIN_ENC_B = 3;  // D3
constexpr uint8_t PIN_BTN = 4;    // D4

ScrollBuffer buffer;
int16_t scroll = 0;

void encoderISR() {
    RotaryEncoder::handleInterrupt();
}

// --- Serial frame parsing ---
static char inLine[ScrollBuffer::kWidth + 1];
static uint8_t inIdx = 0;
static char frameLines[ScrollBuffer::kCapacity][ScrollBuffer::kWidth + 1];
static uint8_t frameCount = 0;

static void commitFrameIfAny() {
  if (frameCount == 0) return;
  buffer.clear();
  for (uint8_t i = 0; i < frameCount; ++i) {
    buffer.push(frameLines[i]);
  }
  scroll = 0;
  frameCount = 0;
}

static void render();

static void processSerial() {
  while (Serial.available() > 0) {
    char c = static_cast<char>(Serial.read());
    if (c == '\r') {
      continue;  // ignore CR
    }
    if (c == '\n') {
      // If we see a blank line, it's end-of-frame
      if (inIdx == 0) {
        commitFrameIfAny();
        // Show new frame immediately
        render();
      } else {
        // Terminate current line and add to frame
        inLine[inIdx] = '\0';
        if (frameCount < ScrollBuffer::kCapacity) {
          strncpy(frameLines[frameCount], inLine, ScrollBuffer::kWidth + 1);
          frameLines[frameCount][ScrollBuffer::kWidth] = '\0';
          ++frameCount;
        }
        inIdx = 0;
      }
    } else {
      if (inIdx < ScrollBuffer::kWidth) {
        inLine[inIdx++] = c;
      }
    }
  }
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
    Serial.begin(115200);
    while (!Serial) {
        ; // Wait for serial
    }
    
    RotaryEncoder::init(PIN_ENC_A, PIN_ENC_B);
    pinMode(PIN_BTN, INPUT_PULLUP);
    
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), encoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), encoderISR, CHANGE);
    
    lcd.begin(20, 4);
    lcd.clear();

    // Initial message shown until first frame arrives
    buffer.clear();
    buffer.push("Waiting for data...");
    render();
    Serial.println("Starting up");
}

void loop() {
    // Read and process incoming serial frames
    processSerial();
    
    int16_t movement = RotaryEncoder::getMovement();
    
    if (movement != 0) {
        // Serial.println(movement > 0 ? "+1" : "-1"); // Debug position

        int16_t maxScroll = 0;
        if (buffer.size() > 4) {
            maxScroll = static_cast<int16_t>(buffer.size() - 4);
        }
        scroll += movement;
        if (scroll < 0) scroll = 0;
        if (scroll > maxScroll) scroll = maxScroll;
        render();
    }

    delay(5);
}
