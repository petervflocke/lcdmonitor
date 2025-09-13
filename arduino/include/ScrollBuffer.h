// Simple fixed-size scroll buffer for 20-char LCD lines
#pragma once
#include <Arduino.h>

class ScrollBuffer {
 public:
  static constexpr size_t kWidth = 20;
  static constexpr size_t kCapacity = 12;  // number of lines stored (limit)

  ScrollBuffer() { clear(); }

  void clear() {
    _count = 0;
    _head = 0;
    for (size_t i = 0; i < kCapacity; ++i) {
      _lines[i][0] = '\0';
    }
  }

  void push(const char* s) {
    // Truncate to kWidth and copy
    char* slot = _lines[_head];
    size_t i = 0;
    for (; i < kWidth && s[i] != '\0'; ++i) {
      slot[i] = s[i];
    }
    slot[i] = '\0';

    _head = (_head + 1) % kCapacity;
    if (_count < kCapacity) {
      ++_count;
    }
  }

  size_t size() const { return _count; }

  // Get line by absolute index from oldest=0 to newest=size-1
  void get(size_t index, char out[kWidth + 1]) const {
    if (index >= _count) {
      out[0] = '\0';
      return;
    }
    size_t oldest = (_count == kCapacity) ? _head : 0;
    size_t pos = (oldest + index) % kCapacity;
    strncpy(out, _lines[pos], kWidth + 1);
    out[kWidth] = '\0';
  }

 private:
  char _lines[kCapacity][kWidth + 1];
  size_t _count = 0;  // number of valid lines
  size_t _head = 0;   // next insert position
};
