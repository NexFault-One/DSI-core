#pragma once

#include "Protocol.h"

class UartProtocol : public Protocol {
public:
    UartProtocol();
    void inject(Injector* injector, uint8_t* data, size_t data_len) override;
    int receive(uint8_t* data, size_t max_len, uint32_t timeout_ms) override;
private:
    bool init() override;
};