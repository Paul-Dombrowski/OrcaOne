#pragma once

#include <Arduino.h>

// EMV-Tag-Namen und Hex-Ausgabe (zustandslos, keine PN532-Abhaengigkeit).
namespace EmvTags {
const char* name(uint32_t tag);
void printTag(uint32_t tag);
}  // namespace EmvTags
