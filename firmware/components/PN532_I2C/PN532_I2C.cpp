/**
 * @modified picospuch
 */

#include "PN532_I2C.h"
#include "PN532_debug.h"
#include "Arduino.h"

PN532_I2C::PN532_I2C(TwoWire &wire, int sda, int scl, uint8_t address)
{
    _wire = &wire;
    _sda = sda;
    _scl = scl;
    _address = address;
    command = 0;
}

void PN532_I2C::begin()
{
    if (_ready) {
        return;
    }

    if (_sda >= 0 && _scl >= 0) {
        _ready = _wire->begin(_sda, _scl);
    } else {
        _ready = _wire->begin();
    }
}

void PN532_I2C::wakeup()
{
    delay(500); // wait for all ready to manipulate pn532
}

int8_t PN532_I2C::writeCommand(const uint8_t *header, uint8_t hlen, const uint8_t *body, uint8_t blen)
{
    if (!_ready) {
        return PN532_INVALID_FRAME;
    }

    command = header[0];
    _wire->beginTransmission(_address);
    
    write(PN532_PREAMBLE);
    write(PN532_STARTCODE1);
    write(PN532_STARTCODE2);
    
    uint8_t length = hlen + blen + 1;   // length of data field: TFI + DATA
    write(length);
    write(~length + 1);                 // checksum of length
    
    write(PN532_HOSTTOPN532);
    uint8_t sum = PN532_HOSTTOPN532;    // sum of TFI + DATA
    
    DMSG("write: ");
       
    for (uint8_t i = 0; i < hlen; i++) {
        if (write(header[i])) {
            sum += header[i];
            
            DMSG_HEX(header[i]);
        } else {
            DMSG("\nToo many data to send, I2C doesn't support such a big packet\n");     // I2C max packet: 32 bytes
            return PN532_INVALID_FRAME;
        }
    }

    for (uint8_t i = 0; i < blen; i++) {
        if (write(body[i])) {
            sum += body[i];
            
            DMSG_HEX(body[i]);
        } else {
            DMSG("\nToo many data to send, I2C doesn't support such a big packet\n");     // I2C max packet: 32 bytes
            return PN532_INVALID_FRAME;
        }
    }
  
    uint8_t checksum = ~sum + 1;            // checksum of TFI + DATA
    write(checksum);
    write(PN532_POSTAMBLE);
    
    if (_wire->endTransmission() != 0) {
        return PN532_INVALID_FRAME;
    }

    DMSG('\n');

    return readAckFrame();
}

int16_t PN532_I2C::getResponseLength(uint8_t buf[], uint8_t len, uint16_t timeout) {
    const uint8_t PN532_NACK[] = {0, 0, 0xFF, 0xFF, 0, 0};
    uint16_t time = 0;
    (void)buf;
    (void)len;

    do {
        if (_wire->requestFrom(_address, 6)) {
            if (read() & 1) {  // check first byte --- status
                break;         // PN532 is ready
            }
        }

        delay(1);
        time++;
        if ((0 != timeout) && (time > timeout)) {
            return -1;
        }
    } while (1); 
    
    if (0x00 != read()      ||       // PREAMBLE
            0x00 != read()  ||       // STARTCODE1
            0xFF != read()           // STARTCODE2
        ) {
        
        return PN532_INVALID_FRAME;
    }
    
    const uint8_t length = read();
    if (length < 2) {
        return PN532_INVALID_FRAME;
    }

    // request for last respond msg again
    _wire->beginTransmission(_address);
    for (uint16_t i = 0; i < sizeof(PN532_NACK); ++i) {
      write(PN532_NACK[i]);
    }
    if (_wire->endTransmission() != 0) {
        return PN532_INVALID_FRAME;
    }

    return length;
}

int16_t PN532_I2C::readResponse(uint8_t buf[], uint8_t len, uint16_t timeout)
{
    if (!_ready) {
        return PN532_INVALID_FRAME;
    }

    uint16_t time = 0;
    const int16_t responseLength = getResponseLength(buf, len, timeout);
    if (responseLength < 2 || responseLength > 24 ||
        responseLength - 2 > len) {
        return responseLength < 0 ? responseLength : PN532_INVALID_FRAME;
    }
    const uint8_t expectedLength = static_cast<uint8_t>(responseLength);

    // [RDY] 00 00 FF LEN LCS (TFI PD0 ... PDn) DCS 00
    do {
        if (_wire->requestFrom(_address, 6 + expectedLength + 2)) {
            if (read() & 1) {  // check first byte --- status
                break;         // PN532 is ready
            }
        }

        delay(1);
        time++;
        if ((0 != timeout) && (time > timeout)) {
            return -1;
        }
    } while (1); 
    
    if (0x00 != read()      ||       // PREAMBLE
            0x00 != read()  ||       // STARTCODE1
            0xFF != read()           // STARTCODE2
        ) {
        
        return PN532_INVALID_FRAME;
    }
    
    uint8_t length = read();
    if (length != expectedLength) {
        return PN532_INVALID_FRAME;
    }

    if (0 != (uint8_t)(length + read())) {   // checksum of length
        return PN532_INVALID_FRAME;
    }
    
    uint8_t cmd = command + 1;               // response command
    if (PN532_PN532TOHOST != read() || (cmd) != read()) {
        return PN532_INVALID_FRAME;
    }
    
    length -= 2;
    if (length > len) {
        return PN532_NO_SPACE;  // not enough space
    }
    
    DMSG("read:  ");
    DMSG_HEX(cmd);
    
    uint8_t sum = PN532_PN532TOHOST + cmd;
    for (uint8_t i = 0; i < length; i++) {
        buf[i] = read();
        sum += buf[i];
        
        DMSG_HEX(buf[i]);
    }
    DMSG('\n');
    
    uint8_t checksum = read();
    if (0 != (uint8_t)(sum + checksum)) {
        DMSG("checksum is not ok\n");
        return PN532_INVALID_FRAME;
    }
    read();         // POSTAMBLE
    
    return length;
}

int8_t PN532_I2C::readAckFrame()
{
    const uint8_t PN532_ACK[] = {0, 0, 0xFF, 0, 0xFF, 0};
    uint8_t ackBuf[sizeof(PN532_ACK)];
    
    DMSG("wait for ack at : ");
    DMSG(millis());
    DMSG('\n');
    
    uint16_t time = 0;
    do {
        if (_wire->requestFrom(_address, sizeof(PN532_ACK) + 1)) {
            if (read() & 1) {  // check first byte --- status
                break;         // PN532 is ready
            }
        }

        delay(1);
        time++;
        if (time > PN532_ACK_WAIT_TIME) {
            DMSG("Time out when waiting for ACK\n");
            return PN532_TIMEOUT;
        }
    } while (1); 
    
    DMSG("ready at : ");
    DMSG(millis());
    DMSG('\n');
    

    for (uint8_t i = 0; i < sizeof(PN532_ACK); i++) {
        ackBuf[i] = read();
    }
    
    if (memcmp(ackBuf, PN532_ACK, sizeof(PN532_ACK))) {
        DMSG("Invalid ACK\n");
        return PN532_INVALID_ACK;
    }
    
    return 0;
}
