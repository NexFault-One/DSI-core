#pragma once

#include <iostream>
#include <Arduino.h>
#include "injectors/Injector.h"

class Protocol {
public:
    Protocol() {}
    virtual void inject(Injector* injector, uint8_t* data, size_t data_len) = 0;
    virtual int receive(uint8_t* data, size_t max_len, uint32_t timeout_ms) = 0;
    virtual ~Protocol() = default;    
private:
    virtual bool init() = 0;

};