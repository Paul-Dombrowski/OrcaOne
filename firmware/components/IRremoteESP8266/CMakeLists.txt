# IRremoteESP8266 als ESP-IDF-Komponente (Arduino-Bibliothek, Fall 2).
# Alle .cpp unter src/ werden uebersetzt; die Header (inkl. src/locale/) sind
# ueber INCLUDE_DIRS auffindbar. REQUIRES: Arduino.h/Stream der Arduino-Komponente.
idf_component_register(
  SRC_DIRS "src"
  INCLUDE_DIRS "src"
  REQUIRES espressif__arduino-esp32
)
