/**
 * @modified picospuch
 */

#ifndef __PN532_I2C_H__
#define __PN532_I2C_H__

#include <Wire.h>
#include <driver/i2c_master.h>
#include "PN532Interface.h"

class PN532_I2C : public PN532Interface {
public:
    PN532_I2C(TwoWire &wire, int sda = -1, int scl = -1, uint8_t address = 0x24);
    
    void begin();
    bool isReady() const { return _ready; }
    void wakeup();
    virtual int8_t writeCommand(const uint8_t *header, uint8_t hlen, const uint8_t *body = 0, uint8_t blen = 0);
    int16_t readResponse(uint8_t buf[], uint8_t len, uint16_t timeout);
    
private:
    TwoWire* _wire;
    i2c_master_dev_handle_t _device = nullptr;
    int _sda;
    int _scl;
    uint8_t _address;
    bool _ready = false;
    uint8_t command;
    // Groesse: PN532-Frame = 6 (RDY+Preamble+Startcode+LEN+LCS) + Nutzlast + 2
    // (DCS+Postamble). Fuer EMV-Antworten (Nutzlast bis 255) daher >= 263.
    // Fruehere 32 Byte reichten nur fuer UID-/Type-2-Reads und verwarfen jede
    // groessere APDU-Antwort bereits in requestFrom().
    uint8_t _txBuffer[270] = {};
    uint8_t _txLength = 0;
    uint8_t _rxBuffer[270] = {};
    size_t _rxLength = 0;
    size_t _rxIndex = 0;
    
    bool waitReady(uint16_t timeout);
    void drainReadBuffer();
    void beginTransmission();
    uint8_t write(uint8_t data);
    uint8_t endTransmission();
    size_t requestFrom(size_t length);
    int read();
    int available() const;
    int8_t readAckFrame();
    int16_t getResponseLength(uint8_t buf[], uint8_t len, uint16_t timeout);
};

#endif
