#include "Tlv.h"

namespace Tlv {

void parseTrack2(const uint8_t* track2Data, uint8_t track2Len, String& outPan, String& outExp) {
  outPan = "";
  outExp = "";
  bool readingPan = true;
  String digits = "";

  for (int i = 0; i < track2Len; ++i) {
    uint8_t high = (track2Data[i] >> 4) & 0x0F;
    uint8_t low = track2Data[i] & 0x0F;

    uint8_t nibbles[2] = {high, low};
    for (uint8_t n : nibbles) {
      if (n == 0x0D) { // Trennzeichen 'D'
        readingPan = false;
        continue;
      }
      if (n == 0x0F) { // End-Padding
        break;
      }
      if (readingPan) {
        outPan += String(n);
      } else {
        digits += String(n);
      }
    }
  }

  // Ablaufdatum folgt direkt nach 'D': JJMM
  if (digits.length() >= 4) {
    outExp = digits.substring(0, 2) + "/" + digits.substring(2, 4);
  }
}

void extractAllAids(const uint8_t* data, int dataLen, std::vector<EmvAid>& aidList) {
  int i = 0;
  while (i < dataLen - 2) {
    // Suche nach Tag 0x61 (Application Template) oder direkt Tag 0x4F (AID)
    if (data[i] == 0x4F) {
      int tagLen = data[i + 1];
      if (tagLen > 0 && tagLen <= 16 && (i + 2 + tagLen <= dataLen)) {
        EmvAid item{};
        item.len = tagLen;
        memcpy(item.aid, &data[i + 2], tagLen);
        item.priority = 0;
        item.label[0] = '\0';
        aidList.push_back(item);
      }
      i += 2 + tagLen;
    } else if (data[i] == 0x61) {
      // Application Template gefunden -> darin Tag 0x4F und 0x50/0x87 suchen
      int tmplLen = data[i + 1];
      int subStart = i + 2;
      int subEnd = subStart + tmplLen;
      if (subEnd > dataLen) subEnd = dataLen;

      EmvAid item{};
      item.len = 0;
      item.label[0] = '\0';
      item.priority = 50;

      int s = subStart;
      while (s < subEnd - 1) {
        uint32_t tag = data[s];
        int tSize = 1;
        if ((data[s] & 0x1F) == 0x1F) {
          tag = (data[s] << 8) | data[s + 1];
          tSize = 2;
        }
        if (s + tSize >= subEnd) break;
        int l = data[s + tSize];
        const uint8_t* val = &data[s + tSize + 1];

        if (tag == 0x4F && l <= 16) {
          item.len = l;
          memcpy(item.aid, val, l);
        } else if (tag == 0x50 && l < sizeof(item.label)) {
          memcpy(item.label, val, l);
          item.label[l] = '\0';
        } else if (tag == 0x87 && l >= 1) {
          item.priority = val[0] & 0x0F;
        }
        s += tSize + 1 + l;
      }

      if (item.len > 0) {
        aidList.push_back(item);
      }
      i = subEnd;
    } else {
      // Nächster Tag
      int tSize = 1;
      if ((data[i] & 0x1F) == 0x1F) {
        tSize = 2;
      }
      if (i + tSize >= dataLen) break;
      int l = data[i + tSize];
      i += tSize + 1 + l;
    }
  }
}

int findTag(const uint8_t* data, int dataLen, uint32_t tag, int depth) {
  if (depth > 8 || dataLen <= 0) return -1;   // Rekursionsschutz (boesartige Karte)

  int i = 0;
  while (i < dataLen) {
    const int tagStart = i;

    // --- Tag lesen (1 oder 2 Bytes) ---
    uint32_t currentTag = data[i];
    int tSize = 1;
    if ((data[i] & 0x1F) == 0x1F) {           // mehrbyte-Tag (z. B. 0x5F24, 0x9F38)
      if (i + 1 >= dataLen) break;
      currentTag = (static_cast<uint32_t>(data[i]) << 8) | data[i + 1];
      tSize = 2;
    }
    if (i + tSize >= dataLen) break;

    // --- Laenge lesen (Kurz- oder Langform). Alle Laengen stammen von der
    //     Karte und werden strikt gegen dataLen validiert. ---
    const int lenPos = i + tSize;
    int len = data[lenPos];
    int lenSize = 1;
    if (len & 0x80) {
      const int n = len & 0x7F;
      if (n == 0 || n > 2 || lenPos + n >= dataLen) break;  // EMV nutzt nur 81/82
      len = 0;
      for (int k = 0; k < n; ++k) {
        len = (len << 8) | data[lenPos + 1 + k];
      }
      lenSize = 1 + n;
    }
    const int valPos = lenPos + lenSize;
    if (len < 0 || valPos > dataLen || len > dataLen - valPos) break;  // OOB-Schutz

    if (currentTag == tag) {
      return tagStart;
    }

    // In konstruierte Templates absteigen (Bit 6 des ersten Tag-Bytes gesetzt:
    // 0x6F, 0xA5, 0x70, 0x77, 0x61, ...). EMV ist stark verschachtelt.
    if (data[tagStart] & 0x20) {
      const int sub = findTag(&data[valPos], len, tag, depth + 1);
      if (sub != -1) return valPos + sub;
    }

    i = valPos + len;   // valPos > i und len >= 0 -> garantierter Fortschritt
  }
  return -1;
}

void printHex(const uint8_t* data, uint8_t length) {
  for (uint8_t i = 0; i < length; ++i) {
    if (data[i] < 0x10) Serial.print('0');
    Serial.print(data[i], HEX);
    if (i + 1 < length) Serial.print(' ');
  }
}

}  // namespace Tlv
