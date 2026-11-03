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
        _wire->begin(_sda, _scl);
    } else {
        _wire->begin();
    }

    i2c_master_bus_handle_t bus = nullptr;
    if (i2c_master_get_bus_handle(
            static_cast<i2c_port_t>(_wire->getBusNum()), &bus) != ESP_OK) {
        return;
    }

    const i2c_device_config_t deviceConfig = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = _address,
        .scl_speed_hz = 100000,
        .scl_wait_us = 20000,
        .flags = {
            .disable_ack_check = false,
        },
    };
    _ready = i2c_master_bus_add_device(bus, &deviceConfig, &_device) == ESP_OK;
}

void PN532_I2C::beginTransmission()
{
    _txLength = 0;
}

uint8_t PN532_I2C::write(uint8_t data)
{
    if (_txLength >= sizeof(_txBuffer)) {
        return 0;
    }

    _txBuffer[_txLength++] = data;
    return 1;
}

uint8_t PN532_I2C::endTransmission()
{
    const esp_err_t result =
        i2c_master_transmit(_device, _txBuffer, _txLength, 100);
    _txLength = 0;
    return result == ESP_OK ? 0 : 1;
}

size_t PN532_I2C::requestFrom(size_t length)
{
    _rxIndex = 0;
    _rxLength = 0;
    if (length == 0 || length > sizeof(_rxBuffer)) {
        return 0;
    }

    if (i2c_master_receive(_device, _rxBuffer, length, 100) != ESP_OK) {
        return 0;
    }

    _rxLength = length;
    return _rxLength;
}

int PN532_I2C::read()
{
    if (_rxIndex >= _rxLength) {
        return -1;
    }
    return _rxBuffer[_rxIndex++];
}

int PN532_I2C::available() const
{
    return static_cast<int>(_rxLength - _rxIndex);
}

void PN532_I2C::wakeup()
{
    delay(500); // wait for all ready to manipulate pn532
}

void PN532_I2C::drainReadBuffer()
{
    while (available() > 0) {
        read();
    }
}

bool PN532_I2C::waitReady(uint16_t timeout)
{
    uint16_t time = 0;
    do {
        if (requestFrom(1) == 1) {
            const int status = read();
            if (status >= 0 && (status & 1)) {
                return true;
            }
        }

        drainReadBuffer();
        delay(1);
        time++;
        if (timeout != 0 && time > timeout) {
            return false;
        }
    } while (1);
}

int8_t PN532_I2C::writeCommand(const uint8_t *header, uint8_t hlen, const uint8_t *body, uint8_t blen)
{
    if (!_ready) {
        return PN532_INVALID_FRAME;
    }

    command = header[0];
    beginTransmission();
    
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
    
    if (endTransmission() != 0) {
        return PN532_INVALID_FRAME;
    }

    DMSG('\n');

    return readAckFrame();
}

int16_t PN532_I2C::getResponseLength(uint8_t buf[], uint8_t len, uint16_t timeout) {
    const uint8_t PN532_NACK[] = {0, 0, 0xFF, 0xFF, 0, 0};
    (void)buf;
    (void)len;

    if (!waitReady(timeout)) {
        return -1;
    }
    if (requestFrom(6) != 6 || available() < 6) {
        drainReadBuffer();
        return PN532_INVALID_FRAME;
    }
    const int status = read();
    if (status < 0 || !(status & 1)) {
        drainReadBuffer();
        return PN532_INVALID_FRAME;
    }
    
    if (0x00 != read()      ||       // PREAMBLE
            0x00 != read()  ||       // STARTCODE1
            0xFF != read()           // STARTCODE2
        ) {
        drainReadBuffer();
        return PN532_INVALID_FRAME;
    }
    
    const uint8_t length = read();
    const uint8_t lengthChecksum = read();
    if (length < 2 || static_cast<uint8_t>(length + lengthChecksum) != 0) {
        drainReadBuffer();
        return PN532_INVALID_FRAME;
    }

    // request for last respond msg again
    beginTransmission();
    for (uint16_t i = 0; i < sizeof(PN532_NACK); ++i) {
      write(PN532_NACK[i]);
    }
    if (endTransmission() != 0) {
        return PN532_INVALID_FRAME;
    }

    return length;
}

int16_t PN532_I2C::readResponse(uint8_t buf[], uint8_t len, uint16_t timeout)
{
    if (!_ready) {
        return PN532_INVALID_FRAME;
    }

    const int16_t responseLength = getResponseLength(buf, len, timeout);
    // Obergrenze 255 = max. Wert des 1-Byte-LEN-Felds des PN532. Die fruehere
    // 24 deckelte auf UID-/Type-2-Groesse und verwarf jede EMV-Antwort. Der
    // Empfangspuffer (_rxBuffer[270]) fasst den resultierenden Frame; die
    // Aufrufer-Kapazitaet bleibt durch "responseLength - 2 > len" geschuetzt.
    if (responseLength < 2 || responseLength > 255 ||
        responseLength - 2 > len) {
        return responseLength < 0 ? responseLength : PN532_INVALID_FRAME;
    }
    const uint8_t expectedLength = static_cast<uint8_t>(responseLength);

    // [RDY] 00 00 FF LEN LCS (TFI PD0 ... PDn) DCS 00
    if (!waitReady(timeout)) {
        return -1;
    }
    const uint8_t frameSize = 6 + expectedLength + 2;
    if (requestFrom(frameSize) != frameSize || available() < frameSize) {
        drainReadBuffer();
        return PN532_INVALID_FRAME;
    }
    const int status = read();
    if (status < 0 || !(status & 1)) {
        drainReadBuffer();
        return PN532_INVALID_FRAME;
    }
    
    if (0x00 != read()      ||       // PREAMBLE
            0x00 != read()  ||       // STARTCODE1
            0xFF != read()           // STARTCODE2
        ) {
        drainReadBuffer();
        return PN532_INVALID_FRAME;
    }
    
    uint8_t length = read();
    if (length != expectedLength) {
        drainReadBuffer();
        return PN532_INVALID_FRAME;
    }

    if (0 != (uint8_t)(length + read())) {   // checksum of length
        drainReadBuffer();
        return PN532_INVALID_FRAME;
    }
    
    uint8_t cmd = command + 1;               // response command
    if (PN532_PN532TOHOST != read() || (cmd) != read()) {
        drainReadBuffer();
        return PN532_INVALID_FRAME;
    }
    
    length -= 2;
    if (length > len) {
        drainReadBuffer();
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
        drainReadBuffer();
        return PN532_INVALID_FRAME;
    }
    read();         // POSTAMBLE
    drainReadBuffer();

    return length;
}

int8_t PN532_I2C::readAckFrame()
{
    const uint8_t PN532_ACK[] = {0, 0, 0xFF, 0, 0xFF, 0};
    uint8_t ackBuf[sizeof(PN532_ACK)];
    
    DMSG("wait for ack at : ");
    DMSG(millis());
    DMSG('\n');
    
    if (!waitReady(PN532_ACK_WAIT_TIME)) {
        DMSG("Time out when waiting for ACK\n");
        return PN532_TIMEOUT;
    }
    if (requestFrom(sizeof(PN532_ACK) + 1) !=
            sizeof(PN532_ACK) + 1 ||
        available() < sizeof(PN532_ACK) + 1) {
        drainReadBuffer();
        return PN532_INVALID_FRAME;
    }
    const int status = read();
    if (status < 0 || !(status & 1)) {
        drainReadBuffer();
        return PN532_INVALID_FRAME;
    }
    
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
