#pragma once
#include <stdio.h>
#include "Injector.h"

class ByteDropInjector : public Injector {
public:
    ByteDropInjector(uint32_t numBytes, uint32_t every_n);

    size_t inject(uint8_t* buffer, size_t data_len) override;

private:
    uint32_t numBytes;
    uint32_t every_n;
};
