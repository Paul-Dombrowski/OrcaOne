#pragma once

#include <Arduino.h>

struct EmvAid {
  uint8_t aid[16];
  uint8_t len;
  char label[24];
  uint8_t priority;
};
