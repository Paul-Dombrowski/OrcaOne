# ============================================================================
#  CMakeLists.txt - ESP-IDF-Projektdefinition fuer OrcaOne
# ----------------------------------------------------------------------------
#  Dies macht den firmware/-Ordner zu einem ESP-IDF-Projekt. Gebaut und
#  geflasht wird mit der ESP-IDF-Kommandozeile (idf.py), NICHT mit PlatformIO.
#
#  Der Arduino-Core wird als Komponente geladen (siehe main/idf_component.yml),
#  damit der bestehende Code (Arduino.h, setup()/loop() in src/main.cpp) und
#  alle Arduino-Bibliotheken unveraendert weiterlaufen.
#
#  Benutzung (im firmware/-Ordner, einmalig IDF 5.5 installiert):
#    idf.py set-target esp32      # einmalig: Ziel-Chip = ESP32 (D1 R32)
#    idf.py build                 # kompilieren (laedt beim 1. Mal den Arduino-Core)
#    idf.py -p <PORT> flash       # auf den ESP32 flashen
#    idf.py -p <PORT> monitor     # Serial Monitor (beenden mit Strg+])
# ============================================================================
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(orcaone)
