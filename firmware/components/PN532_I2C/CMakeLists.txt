# PN532_I2C - I2C-Transport fuer den PN532 (passt zur Verkabelung in config.h).
idf_component_register(
  SRCS "PN532_I2C.cpp"
  INCLUDE_DIRS "."
  REQUIRES espressif__arduino-esp32 PN532
)
