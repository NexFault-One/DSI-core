#pragma once

#include "Injector.h"

enum class PhantomByteMode
{
    RANDOM, // isnert phantom byte randomly
    MANUAL // insert phantom byte at a specific offset
};


class PhantomByteInjector : public Injector {
public:
    PhantomByteInjector(PhantomByteMode mode=PhantomByteMode::MANUAL, uint32_t phantom_byte=0, uint32_t offset=1);

    size_t inject(uint8_t* buffer, size_t data_len) override;
private:
    PhantomByteMode mode;
    uint32_t phantom_byte;
    uint32_t offset;


};