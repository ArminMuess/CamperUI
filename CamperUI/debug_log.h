#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <Arduino.h>
#include <HWCDC.h>

class WebServer;

class DebugSerialLogger : public Stream {
public:
    DebugSerialLogger();

    void begin(unsigned long baud = 115200);
    void setTxTimeoutMs(uint32_t timeout);

    int available() override;
    int read() override;
    int peek() override;
    void flush() override;

    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;

    void injectInput(char c);
    void injectInput(const char *str);

    void clear();
    size_t getLogCount();
    void streamToHttp(WebServer &server);

    operator bool() const { return true; }

private:
    HWCDC _hw_cdc;
};

extern DebugSerialLogger CamperSerial;
#define USBSerial CamperSerial
#define Serial CamperSerial

#endif // DEBUG_LOG_H

