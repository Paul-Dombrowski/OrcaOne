# ============================================================================
#  main/ - die ESP-IDF-Einstiegskomponente
# ----------------------------------------------------------------------------
#  ESP-IDF verlangt eine Komponente namens "main". Sie enthaelt hier selbst
#  KEINEN Code, sondern bindet den gesamten Firmware-Code aus ../src/ ein.
#  Dadurch bleibt src/main.cpp dort liegen, wo die READMEs es beschreiben.
#
#  CONFIGURE_DEPENDS: Neue .cpp-Dateien (z. B. ein neues Modul) werden beim
#  naechsten "idf.py build" AUTOMATISCH erkannt - diese Datei muss man dafuer
#  NICHT anfassen.
# ============================================================================
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")

# INCLUDE_DIRS "src" macht z. B. #include "config.h" auffindbar.
# "main" haengt automatisch von allen anderen Komponenten ab (u. a. arduino),
# daher ist hier kein explizites REQUIRES noetig.
idf_component_register(
  SRCS ${APP_SOURCES}
  INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/src"
)
