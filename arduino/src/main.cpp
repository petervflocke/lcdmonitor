#include <Arduino.h>
#include <LiquidCrystal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ScrollBuffer.h"
#include "RotaryEncoder.h"

// LCD pins: RS=7, E=8, D4=9, D5=10, D6=11, D7=12
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

// LCD geometry & string sizing
constexpr uint8_t LCD_COLS = ScrollBuffer::kWidth;
constexpr uint8_t LCD_ROWS = 4;
constexpr uint8_t LCD_BUFFER_LEN = LCD_COLS + 1;

constexpr uint8_t CMD_ID_STORAGE = 8;                  // 7 visible chars + null
constexpr uint8_t CMD_LABEL_VISIBLE = LCD_COLS - 1;    // reserve column 0 for cursor
constexpr uint8_t CMD_LABEL_STORAGE = CMD_LABEL_VISIBLE + 1;

constexpr char COMMANDS_HEADER[] = "COMMANDS v1";
constexpr size_t COMMANDS_HEADER_LEN = sizeof(COMMANDS_HEADER) - 1;
constexpr char META_PREFIX[] = "META ";
constexpr size_t META_PREFIX_LEN = sizeof(META_PREFIX) - 1;
constexpr char META_INTERVAL_KEY[] = "interval=";
constexpr size_t META_INTERVAL_KEY_LEN = sizeof(META_INTERVAL_KEY) - 1;

// Rotary encoder pins
constexpr uint8_t PIN_ENC_A = 2;   // D2
constexpr uint8_t PIN_ENC_B = 3;   // D3
constexpr uint8_t PIN_BTN = 4;     // D4

// Heartbeat LEDs
constexpr uint8_t PIN_LED_GREEN = 5;  // D5: healthy heartbeat
constexpr uint8_t PIN_LED_RED = 6;    // D6: stale/lost/ack indicator

static const unsigned long GREEN_PULSE_MS = 120;
static const unsigned long RED_STALE_PULSE_MS = 50;
static const unsigned long RED_ACK_PULSE_MS = 150;
static const unsigned long STALE_PERIOD_MS = 1000;
static const unsigned long STALE_THRESHOLD_MIN_MS = 500;
static const unsigned long HEARTBEAT_MIN_INTERVAL_MS = 250;
static const unsigned long FRAME_LOSS_MULTIPLIER = 10;

constexpr unsigned long WAITING_ANIM_INTERVAL_MS = 250;
constexpr uint8_t WAITING_ANIM_FRAMES = 4;

// --- Telemetry buffer/state ---
ScrollBuffer buffer;
int16_t scroll = 0;

// --- Modes ---
enum class UIMode : uint8_t { Telemetry = 0, CommandsWaiting = 1, Commands = 2 };
static UIMode mode = UIMode::Telemetry;
static UIMode requestedMode = UIMode::Telemetry;  // user’s desired mode

// --- Commands list state ---
constexpr uint8_t CMD_MAX = 12;  // max commands kept (fits our frame capacity)
struct CmdItem {
  char id[CMD_ID_STORAGE];           // short id, e.g., "1", "42"
  char label[CMD_LABEL_STORAGE];     // label text (will render in CMD_LABEL_VISIBLE cols)
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
static unsigned long heartbeatIntervalMs = 3000;
static bool haveData = false;
static uint8_t waitAnim = 0;
static unsigned long lastAnimMs = 0;

// --- Heartbeat LED state ---
static unsigned long greenPulseUntilMs = 0;
static unsigned long redPulseUntilMs = 0;
static unsigned long staleNextBlinkMs = 0;

static void triggerGreenPulse(unsigned long now, unsigned long duration = GREEN_PULSE_MS) {
  unsigned long expiry = now + duration;
  if (expiry < now) {
    expiry = now;  // overflow guard
  }
  if (expiry < greenPulseUntilMs) {
    expiry = greenPulseUntilMs;
  }
  greenPulseUntilMs = expiry;
  digitalWrite(PIN_LED_GREEN, HIGH);
}

static void triggerRedPulse(unsigned long now, unsigned long duration) {
  unsigned long expiry = now + duration;
  if (expiry < now) {
    expiry = now;
  }
  if (expiry < redPulseUntilMs) {
    expiry = redPulseUntilMs;
  }
  redPulseUntilMs = expiry;
  digitalWrite(PIN_LED_RED, HIGH);
  staleNextBlinkMs = now + STALE_PERIOD_MS;
}

static void updateHeartbeat(unsigned long now);

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
static char inLine[LCD_BUFFER_LEN];
static uint8_t inIdx = 0;
static char frameLines[ScrollBuffer::kCapacity][LCD_BUFFER_LEN];
static uint8_t frameCount = 0;

static void applyTelemetryFrame() {
  // Preserve current scroll position across frame updates
  int16_t prevScroll = scroll;
  buffer.clear();
  for (uint8_t i = 0; i < frameCount; ++i) {
    buffer.push(frameLines[i]);
  }
  int16_t maxScroll = 0;
  if (buffer.size() > LCD_ROWS) {
    maxScroll = static_cast<int16_t>(buffer.size() - LCD_ROWS);
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
    for (uint8_t j = 0; j < LCD_COLS && ln[j] != '\0'; ++j) {
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
    while (lbl[lblLen] != '\0' && lblLen < CMD_LABEL_VISIBLE) { ++lblLen; }
    // Copy id (truncate to storage)
    uint8_t idCopy = idLen < (CMD_ID_STORAGE - 1) ? idLen : (CMD_ID_STORAGE - 1);
    for (uint8_t k = 0; k < idCopy; ++k) commands[commandsCount].id[k] = ln[k];
    commands[commandsCount].id[idCopy] = '\0';
    // Copy label (truncate to visible width)
    uint8_t labCopy = lblLen < CMD_LABEL_VISIBLE ? lblLen : CMD_LABEL_VISIBLE;
    for (uint8_t k = 0; k < labCopy; ++k) commands[commandsCount].label[k] = lbl[k];
    commands[commandsCount].label[labCopy] = '\0';
    ++commandsCount;
  }
  // Reset cursor/window on fresh commands
  cursorIndex = 0;
  windowStart = 0;
}

static bool parseMetaLine(const char* line) {
  if (strncmp(line, META_PREFIX, META_PREFIX_LEN) != 0) {
    return false;
  }

  const char* intervalPtr = strstr(line, META_INTERVAL_KEY);
  if (intervalPtr != nullptr) {
    intervalPtr += META_INTERVAL_KEY_LEN;
    char* endPtr = nullptr;
    double sec = strtod(intervalPtr, &endPtr);
    if (sec > 0.0) {
      unsigned long intervalMs = static_cast<unsigned long>(sec * 1000.0);
      if (intervalMs < HEARTBEAT_MIN_INTERVAL_MS) {
        intervalMs = HEARTBEAT_MIN_INTERVAL_MS;
      }
      heartbeatIntervalMs = intervalMs;

      unsigned long candidate;
      if (intervalMs > FRAME_TIMEOUT_MAX_MS / FRAME_LOSS_MULTIPLIER) {
        candidate = FRAME_TIMEOUT_MAX_MS;
      } else {
        candidate = intervalMs * FRAME_LOSS_MULTIPLIER;
      }
      if (candidate < FRAME_TIMEOUT_MIN_MS) candidate = FRAME_TIMEOUT_MIN_MS;
      if (candidate > FRAME_TIMEOUT_MAX_MS) candidate = FRAME_TIMEOUT_MAX_MS;
      frameTimeoutMs = candidate;
      displayTimeoutMs = candidate;
    }
  }
  return true;
}

static void processTelemetryFrame() {
  applyTelemetryFrame();
  if (requestedMode == UIMode::Telemetry) {
    mode = UIMode::Telemetry;
    scroll = 0;
  }
}

static void processCommandsFrame() {
  applyCommandsFrame();
  requestedMode = UIMode::Commands;
  mode = UIMode::Commands;
}

static void updateWatchdog(unsigned long now, bool pulseGreen) {
  haveData = true;
  lastFrameMs = now;
  waitAnim = 0;
  lastAnimMs = now;
  staleNextBlinkMs = now + STALE_PERIOD_MS;
  if (pulseGreen) {
    triggerGreenPulse(now);
  }
  if (redPulseUntilMs == 0) {
    digitalWrite(PIN_LED_RED, LOW);
  }
}

static void commitFrameIfAny() {
  if (frameCount == 0) return;

  bool hadMeta = parseMetaLine(frameLines[0]);
  if (hadMeta) {
    if (frameCount > 1) {
      for (uint8_t i = 1; i < frameCount; ++i) {
        memcpy(frameLines[i - 1], frameLines[i], LCD_BUFFER_LEN);
      }
      --frameCount;
    } else {
      frameCount = 0;
    }
  }

  unsigned long now = millis();

  if (frameCount == 0) {
    if (hadMeta) {
      updateWatchdog(now, true);
    }
    return;
  }

  bool isCommands = (strncmp(frameLines[0], COMMANDS_HEADER, COMMANDS_HEADER_LEN) == 0);

  if (isCommands) {
    processCommandsFrame();
  } else {
    processTelemetryFrame();
  }

  frameCount = 0;
  updateWatchdog(now, !isCommands);
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
          strncpy(frameLines[frameCount], inLine, LCD_BUFFER_LEN);
          ++frameCount;
        }
        inIdx = 0;
      }
    } else {
      if (inIdx < LCD_COLS) {
        inLine[inIdx++] = c;
      }
    }
  }
}

static void printPadded(const char* s, uint8_t width) {
  char padded[LCD_BUFFER_LEN];
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
    char msg[LCD_BUFFER_LEN];
    snprintf(msg, sizeof(msg), "Waiting for data %c", anim[waitAnim % WAITING_ANIM_FRAMES]);
    printPadded(msg, LCD_COLS);
    lcd.setCursor(0, 1);
    if (displayTimeoutMs == 0) {
      printPadded("Timeout: --", LCD_COLS);
    } else {
      unsigned long seconds = (displayTimeoutMs + 500) / 1000;
      char timeoutLine[LCD_BUFFER_LEN];
      snprintf(timeoutLine, sizeof(timeoutLine), "Timeout: %lus", seconds);
      printPadded(timeoutLine, LCD_COLS);
    }
    for (uint8_t row = 2; row < LCD_ROWS; ++row) {
      lcd.setCursor(0, row);
      printPadded("", LCD_COLS);
    }
    return;
  }

  if (mode == UIMode::Telemetry) {
    char line[LCD_BUFFER_LEN];
    for (uint8_t row = 0; row < LCD_ROWS; ++row) {
      uint16_t idx = scroll + row;
      buffer.get(idx, line);
      lcd.setCursor(0, row);
      printPadded(line, LCD_COLS);
    }
  } else if (mode == UIMode::CommandsWaiting) {
    lcd.setCursor(0, 0);
    printPadded("> Loading commands...", LCD_COLS);
  } else {  // Commands
    // Total entries = commandsCount + 1 (Exit)
    int16_t total = static_cast<int16_t>(commandsCount) + 1;
    for (uint8_t row = 0; row < LCD_ROWS; ++row) {
      int16_t idx = windowStart + row;
      lcd.setCursor(0, row);
      char label[CMD_LABEL_STORAGE];
      if (idx < 0 || idx >= total) {
        printPadded("", LCD_COLS);
        continue;
      }
      if (idx == commandsCount) {
        // Exit entry
        label[0] = '\0';
        strncpy(label, "Exit", CMD_LABEL_STORAGE);
      } else {
        strncpy(label, commands[idx].label, CMD_LABEL_VISIBLE);
        label[CMD_LABEL_VISIBLE] = '\0';
      }
      label[CMD_LABEL_STORAGE - 1] = '\0';
      // Render cursor and label (cursor at col 0, label at col 1 with width CMD_LABEL_VISIBLE)
      lcd.print((idx == cursorIndex) ? ">" : " ");
      printPadded(label, CMD_LABEL_VISIBLE);
    }
  }
}

static void updateHeartbeat(unsigned long now) {
  if (greenPulseUntilMs != 0 && static_cast<long>(now - greenPulseUntilMs) >= 0) {
    greenPulseUntilMs = 0;
    digitalWrite(PIN_LED_GREEN, LOW);
  }

  if (redPulseUntilMs != 0 && static_cast<long>(now - redPulseUntilMs) >= 0) {
    redPulseUntilMs = 0;
    digitalWrite(PIN_LED_RED, LOW);
  }

  if (haveData) {
    unsigned long since = now - lastFrameMs;
    unsigned long staleThreshold = heartbeatIntervalMs * 2;
    if (staleThreshold < STALE_THRESHOLD_MIN_MS) staleThreshold = STALE_THRESHOLD_MIN_MS;
    if (staleThreshold > frameTimeoutMs) staleThreshold = frameTimeoutMs;

    if (since >= staleThreshold && since < frameTimeoutMs) {
      if (staleNextBlinkMs == 0 || static_cast<long>(now - staleNextBlinkMs) >= 0) {
        triggerRedPulse(now, RED_STALE_PULSE_MS);
        staleNextBlinkMs = now + STALE_PERIOD_MS;
      }
    } else {
      staleNextBlinkMs = now + STALE_PERIOD_MS;
      if (redPulseUntilMs == 0) {
        digitalWrite(PIN_LED_RED, LOW);
      }
    }
  } else {
    staleNextBlinkMs = now + STALE_PERIOD_MS;
    if (redPulseUntilMs == 0) {
      bool on = (waitAnim % 2) == 0;
      digitalWrite(PIN_LED_RED, on ? HIGH : LOW);
    }
    if (greenPulseUntilMs == 0) {
      digitalWrite(PIN_LED_GREEN, LOW);
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
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, LOW);

    attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), encoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), encoderISR, CHANGE);
    
    lcd.begin(LCD_COLS, LCD_ROWS);
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
    heartbeatIntervalMs = FRAME_TIMEOUT_DEFAULT_MS / 3;
    staleNextBlinkMs = lastFrameMs + STALE_PERIOD_MS;
}

void loop() {
    // Read and process incoming serial frames
    processSerial();
    
    int16_t movement = RotaryEncoder::getMovement();
    
    if (movement != 0) {
        if (mode == UIMode::Telemetry) {
            int16_t maxScroll = 0;
            if (buffer.size() > LCD_ROWS) {
                maxScroll = static_cast<int16_t>(buffer.size() - LCD_ROWS);
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
            // Keep cursor visible within LCD_ROWS window
            if (cursorIndex < windowStart) windowStart = cursorIndex;
            int16_t windowHeight = static_cast<int16_t>(LCD_ROWS);
            if (cursorIndex > windowStart + (windowHeight - 1)) windowStart = cursorIndex - (windowHeight - 1);
            if (windowStart < 0) windowStart = 0;
            int16_t maxWindowStart = (total > windowHeight) ? (total - windowHeight) : 0;
            if (windowStart > maxWindowStart) windowStart = maxWindowStart;
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
            greenPulseUntilMs = 0;
            redPulseUntilMs = 0;
            digitalWrite(PIN_LED_GREEN, LOW);
            digitalWrite(PIN_LED_RED, LOW);
            staleNextBlinkMs = now + STALE_PERIOD_MS;
            render();
        }
    } else {
        if ((now - lastAnimMs) >= WAITING_ANIM_INTERVAL_MS) {
            waitAnim = static_cast<uint8_t>((waitAnim + 1) % WAITING_ANIM_FRAMES);
            lastAnimMs = now;
            render();
        }
    }

    updateHeartbeat(now);

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
                                triggerRedPulse(nowMs, RED_ACK_PULSE_MS);
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
