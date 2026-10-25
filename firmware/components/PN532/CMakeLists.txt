# PN532 - Kern-Treiber (Elechouse/Seeed). Transport (I2C) kommt aus PN532_I2C.
idf_component_register(
  SRCS "PN532.cpp" "emulatetag.cpp" "llcp.cpp" "mac_link.cpp" "snep.cpp"
  INCLUDE_DIRS "."
  REQUIRES espressif__arduino-esp32
)
