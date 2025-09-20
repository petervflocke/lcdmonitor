#include <Arduino.h>
#include <LiquidCrystal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ScrollBuffer.h"
#include "RotaryEncoder.h"

// LCD pins: RS=7, E=8, D4=9, D5=10, D6=11, D7=12
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

// Rotary encoder pins
constexpr uint8_t PIN_ENC_A = 2;  // D2
constexpr uint8_t PIN_ENC_B = 3;  // D3
constexpr uint8_t PIN_BTN = 4;    // D4

// --- Telemetry buffer/state ---
ScrollBuffer buffer;
int16_t scroll = 0;

// --- Modes ---
enum class UIMode : uint8_t { Telemetry = 0, CommandsWaiting = 1, Commands = 2 };
static UIMode mode = UIMode::Telemetry;
static UIMode requestedMode = UIMode::Telemetry;  // user’s desired mode

// --- Commands list state ---
static const uint8_t CMD_MAX = 12;  // max commands kept (fits our frame capacity)
struct CmdItem {
  char id[8];          // short id, e.g., "1", "42"
  char label[20];      // label text (will render in 19 cols)
};
static CmdItem commands[CMD_MAX];
static uint8_t commandsCount = 0;   // number of received commands (without Exit)
static int16_t cursorIndex = 0;     // selection within [0..commandsCount] where last is Exit
static int16_t windowStart = 0;     // top-most visible item index in commands view

// --- Frame watchdog ---
static const unsigned long FRAME_TIMEOUT_DEFAULT_MS = 10000;  // fallback watchdog
static const unsigned long FRAME_TIMEOUT_MIN_MS = 5000;
static const unsigned long FRAME_TIMEOUT_MAX_MS = 60000;
static unsigned long frameTimeoutMs = FRAME_TIMEOUT_DEFAULT_MS;
static unsigned long displayTimeoutMs = 0;
static unsigned long lastFrameMs = 0;
static bool haveData = false;
static uint8_t waitAnim = 0;
static unsigned long lastAnimMs = 0;

// --- Button handling (debounced, long/double press) ---
constexpr uint16_t BTN_DEBOUNCE_MS = 20;
constexpr uint16_t BTN_LONG_MS = 700;
constexpr uint16_t BTN_DOUBLE_GAP_MS = 350;
static uint8_t btnPrev = HIGH;
static unsigned long btnLastChangeMs = 0;
static unsigned long btnPressStartMs = 0;
static bool btnPressed = false;
static unsigned long lastShortReleaseMs = 0;

void encoderISR() {
    RotaryEncoder::handleInterrupt();
}

// --- Serial frame parsing ---
static char inLine[ScrollBuffer::kWidth + 1];
static uint8_t inIdx = 0;
static char frameLines[ScrollBuffer::kCapacity][ScrollBuffer::kWidth + 1];
static uint8_t frameCount = 0;

static void applyTelemetryFrame() {
  // Preserve current scroll position across frame updates
  int16_t prevScroll = scroll;
  buffer.clear();
  for (uint8_t i = 0; i < frameCount; ++i) {
    buffer.push(frameLines[i]);
  }
  int16_t maxScroll = 0;
  if (buffer.size() > 4) {
    maxScroll = static_cast<int16_t>(buffer.size() - 4);
  }
  if (prevScroll < 0) prevScroll = 0;
  if (prevScroll > maxScroll) prevScroll = maxScroll;
  scroll = prevScroll;
}

static void applyCommandsFrame() {
  // frameLines[0] == "COMMANDS v1" guaranteed by caller
  commandsCount = 0;
  for (uint8_t i = 1; i < frameCount && commandsCount < CMD_MAX; ++i) {
    // Each line: "<id> <label>" (max 20 chars). Split at first space.
    const char* ln = frameLines[i];
    // Find first space
    const char* sp = nullptr;
    for (uint8_t j = 0; j < ScrollBuffer::kWidth && ln[j] != '\0'; ++j) {
      if (ln[j] == ' ') { sp = &ln[j]; break; }
    }
    if (sp == nullptr) {
      // malformed; skip
      continue;
    }
    uint8_t idLen = static_cast<uint8_t>(sp - ln);
    if (idLen == 0) continue;
    uint8_t lblLen = 0;
    const char* lbl = sp + 1;
    while (lbl[lblLen] != '\0' && lblLen < 19) { ++lblLen; }
    // Copy id (truncate to 7)
    uint8_t idCopy = idLen < 7 ? idLen : 7;
    for (uint8_t k = 0; k < idCopy; ++k) commands[commandsCount].id[k] = ln[k];
    commands[commandsCount].id[idCopy] = '\0';
    // Copy label (truncate to 19)
    uint8_t labCopy = lblLen < 19 ? lblLen : 19;
    for (uint8_t k = 0; k < labCopy; ++k) commands[commandsCount].label[k] = lbl[k];
    commands[commandsCount].label[labCopy] = '\0';
    ++commandsCount;
  }
  // Reset cursor/window on fresh commands
  cursorIndex = 0;
  windowStart = 0;
}

static void commitFrameIfAny() {
  if (frameCount == 0) return;
  bool isCommands = (strncmp(frameLines[0], "COMMANDS v1", 11) == 0);
  if (!isCommands && strncmp(frameLines[0], "META ", 5) == 0) {
    const char* meta = frameLines[0];
    const char* intervalPtr = strstr(meta, "interval=");
    if (intervalPtr != nullptr) {
      intervalPtr += 9;  // skip "interval="
      char* endPtr = nullptr;
      double sec = strtod(intervalPtr, &endPtr);
      if (sec > 0.0) {
        unsigned long candidate = static_cast<unsigned long>(sec * 3000.0);  // 3x interval
        if (candidate < FRAME_TIMEOUT_MIN_MS) candidate = FRAME_TIMEOUT_MIN_MS;
        if (candidate > FRAME_TIMEOUT_MAX_MS) candidate = FRAME_TIMEOUT_MAX_MS;
        frameTimeoutMs = candidate;
        displayTimeoutMs = candidate;
      }
    }
    // remove META line before applying telemetry frame
    if (frameCount > 1) {
      for (uint8_t i = 1; i < frameCount; ++i) {
        strncpy(frameLines[i - 1], frameLines[i], ScrollBuffer::kWidth + 1);
        frameLines[i - 1][ScrollBuffer::kWidth] = '\0';
      }
      --frameCount;
    } else {
      frameCount = 0;
    }
  }

  if (frameCount == 0 && !isCommands) {
    // Only metadata present; treat as keepalive
    haveData = true;
    lastFrameMs = millis();
    waitAnim = 0;
    lastAnimMs = lastFrameMs;
    return;
  }

  if (isCommands) {
    applyCommandsFrame();
    requestedMode = UIMode::Commands;
    mode = UIMode::Commands;
  } else {
    applyTelemetryFrame();
    if (requestedMode == UIMode::Telemetry) {
      mode = UIMode::Telemetry;
      scroll = 0;
    }
  }
  frameCount = 0;

  // Update watchdog on completed frame
  haveData = true;
  lastFrameMs = millis();
  waitAnim = 0;
  lastAnimMs = lastFrameMs;
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

static void printPadded(const char* s, uint8_t width) {
  char padded[ScrollBuffer::kWidth + 1];
  uint8_t i = 0;
  for (; i < width && s[i] != '\0'; ++i) padded[i] = s[i];
  for (; i < width; ++i) padded[i] = ' ';
  padded[width] = '\0';
  lcd.print(padded);
}

static void render() {
  lcd.clear();
  if (!haveData) {
    static const char* anim = "|/-\\";
    lcd.setCursor(0, 0);
    char msg[21];
    snprintf(msg, sizeof(msg), "Waiting for data %c", anim[waitAnim % 4]);
    printPadded(msg, ScrollBuffer::kWidth);
    lcd.setCursor(0, 1);
    if (displayTimeoutMs == 0) {
      printPadded("Timeout: --", ScrollBuffer::kWidth);
    } else {
      unsigned long seconds = (displayTimeoutMs + 500) / 1000;
      char timeoutLine[21];
      snprintf(timeoutLine, sizeof(timeoutLine), "Timeout: %lus", seconds);
      printPadded(timeoutLine, ScrollBuffer::kWidth);
    }
    for (uint8_t row = 2; row < 4; ++row) {
      lcd.setCursor(0, row);
      printPadded("", ScrollBuffer::kWidth);
    }
    return;
  }

  if (mode == UIMode::Telemetry) {
    char line[ScrollBuffer::kWidth + 1];
    for (uint8_t row = 0; row < 4; ++row) {
      uint16_t idx = scroll + row;
      buffer.get(idx, line);
      lcd.setCursor(0, row);
      printPadded(line, ScrollBuffer::kWidth);
    }
  } else if (mode == UIMode::CommandsWaiting) {
    lcd.setCursor(0, 0);
    printPadded("> Loading commands...", ScrollBuffer::kWidth);
  } else {  // Commands
    // Total entries = commandsCount + 1 (Exit)
    int16_t total = static_cast<int16_t>(commandsCount) + 1;
    for (uint8_t row = 0; row < 4; ++row) {
      int16_t idx = windowStart + row;
      lcd.setCursor(0, row);
      char label[20];
      if (idx < 0 || idx >= total) {
        printPadded("", ScrollBuffer::kWidth);
        continue;
      }
      if (idx == commandsCount) {
        // Exit entry
        label[0] = '\0';
        strncpy(label, "Exit", 20);
      } else {
        strncpy(label, commands[idx].label, 19);
        label[19] = '\0';
      }
      // Render cursor and label (cursor at col 0, label at col 1 with width 19)
      lcd.print((idx == cursorIndex) ? ">" : " ");
      printPadded(label, 19);
    }
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
    mode = UIMode::Telemetry;
    requestedMode = UIMode::Telemetry;
    frameTimeoutMs = FRAME_TIMEOUT_DEFAULT_MS;
    displayTimeoutMs = 0;
    waitAnim = 0;
    lastAnimMs = millis();
    render();
    Serial.println("Starting up");

    // Initialize watchdog state
    haveData = false;
    lastFrameMs = millis();
}

void loop() {
    // Read and process incoming serial frames
    processSerial();
    
    int16_t movement = RotaryEncoder::getMovement();
    
    if (movement != 0) {
        if (mode == UIMode::Telemetry) {
            int16_t maxScroll = 0;
            if (buffer.size() > 4) {
                maxScroll = static_cast<int16_t>(buffer.size() - 4);
            }
            scroll += movement;
            if (scroll < 0) scroll = 0;
            if (scroll > maxScroll) scroll = maxScroll;
            render();
        } else if (mode == UIMode::Commands || mode == UIMode::CommandsWaiting) {
            // In waiting state, allow moving cursor once list arrives; still render cursor area.
            int16_t total = static_cast<int16_t>(commandsCount) + 1; // incl Exit
            if (total < 1) total = 1;
            cursorIndex += (movement > 0 ? 1 : -1);
            if (cursorIndex < 0) cursorIndex = 0;
            if (cursorIndex >= total) cursorIndex = total - 1;
            // Keep cursor visible within 4-row window
            if (cursorIndex < windowStart) windowStart = cursorIndex;
            if (cursorIndex > windowStart + 3) windowStart = cursorIndex - 3;
            if (windowStart < 0) windowStart = 0;
            if (windowStart > total - 4) windowStart = total > 4 ? total - 4 : 0;
            render();
        }
    }

    // Watchdog & waiting animation
    unsigned long now = millis();
    if (haveData) {
        if ((now - lastFrameMs) > frameTimeoutMs) {
            haveData = false;
            mode = UIMode::Telemetry;
            requestedMode = UIMode::Telemetry;
            commandsCount = 0;
            buffer.clear();
            buffer.push("Waiting for data...");
            waitAnim = 0;
            lastAnimMs = now;
            render();
        }
    } else {
        if ((now - lastAnimMs) >= 250) {
            waitAnim = (waitAnim + 1) & 0x03;
            lastAnimMs = now;
            render();
        }
    }

    // Button handling: debounce + long/double press
    uint8_t btn = digitalRead(PIN_BTN);
    unsigned long nowMs = millis();
    if (btn != btnPrev && (nowMs - btnLastChangeMs) > BTN_DEBOUNCE_MS) {
        btnLastChangeMs = nowMs;
        if (btn == LOW) {
            // pressed
            btnPressed = true;
            btnPressStartMs = nowMs;
        } else {
            // released
            if (btnPressed) {
                unsigned long held = nowMs - btnPressStartMs;
                btnPressed = false;
                if (held >= BTN_LONG_MS) {
                    // Long press: toggle Commands mode or exit to Telemetry
                    if (mode == UIMode::Telemetry) {
                        // Enter commands: request list and show waiting
                        mode = UIMode::CommandsWaiting;
                        requestedMode = UIMode::Commands;
                        cursorIndex = 0;
                        windowStart = 0;
                        render();
                        Serial.println("REQ COMMANDS");
                    } else {
                        // Exit to telemetry and reset scroll to top
                        mode = UIMode::Telemetry;
                        requestedMode = UIMode::Telemetry;
                        scroll = 0;
                        render();
                    }
                } else {
                    // Short press: check for double press
                    if ((nowMs - lastShortReleaseMs) <= BTN_DOUBLE_GAP_MS) {
                        // Double press: in Commands mode -> select
                        if (mode == UIMode::Commands) {
                            if (cursorIndex == commandsCount) {
                                // Exit entry selected
                                mode = UIMode::Telemetry;
                                requestedMode = UIMode::Telemetry;
                                scroll = 0;
                                render();
                            } else if (cursorIndex >= 0 && cursorIndex < commandsCount) {
                                Serial.print("SELECT ");
                                Serial.println(commands[cursorIndex].id);
                            }
                        }
                    }
                    lastShortReleaseMs = nowMs;
                }
            }
        }
        btnPrev = btn;
    }

    delay(5);
}
