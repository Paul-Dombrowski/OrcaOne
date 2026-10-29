#pragma once

#include <Arduino.h>
#include <vector>
#include "EmvTypes.h"

namespace Tlv {

// depth begrenzt die Rekursion in verschachtelte Templates
// (Schutz gegen fehlerhafte/boesartige Kartendaten).
int findTag(const uint8_t* data, int dataLen, uint32_t tag, int depth = 0);
void extractAllAids(const uint8_t* data, int dataLen, std::vector<EmvAid>& aidList);
void parseTrack2(const uint8_t* track2Data, uint8_t track2Len, String& outPan, String& outExp);
void printHex(const uint8_t* data, uint8_t length);

}  // namespace Tlv
