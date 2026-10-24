# ============================================================================
#  main/ - die ESP-IDF-Einstiegskomponente
# ----------------------------------------------------------------------------
#  ESP-IDF verlangt eine Komponente namens "main". Sie enthaelt hier selbst
#  KEINEN Code, sondern bindet den gesamten Firmware-Code aus ../src/ ein.
#  Dadurch bleibt src/main.cpp dort liegen, wo die READMEs es beschreiben.
#
#  WICHTIG: Neue .cpp-Dateien (z. B. ein neues Modul) werden erst nach
#      idf.py reconfigure
#  erkannt (danach normal "idf.py build"). Diese Datei muss man dafuer NICHT
#  anfassen. (CONFIGURE_DEPENDS geht hier nicht: ESP-IDF liest diese Datei
#  zusaetzlich im CMake-Skript-Modus, in dem CONFIGURE_DEPENDS verboten ist.)
# ============================================================================

# Pfad relativ zu DIESER Datei (CMAKE_SOURCE_DIR stimmt im Skript-Modus nicht).
set(APP_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/../src")
file(GLOB_RECURSE APP_SOURCES "${APP_SRC_DIR}/*.cpp")

# INCLUDE_DIRS macht z. B. #include "config.h" auffindbar.
# "main" haengt automatisch von allen anderen Komponenten ab (u. a. arduino),
# daher ist hier kein explizites REQUIRES noetig.
idf_component_register(
  SRCS ${APP_SOURCES}
  INCLUDE_DIRS "${APP_SRC_DIR}"
)
