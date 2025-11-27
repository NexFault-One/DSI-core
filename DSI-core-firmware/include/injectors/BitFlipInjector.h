#pragma once
#include <iostream>
#include <vector>
#include <stdio.h>
#include <Arduino.h>
#include "Injector.h"

enum class BitFlipMode
{
    PERIODIC, // flip one bit at every_n bits
    RANDOM // flip a random nunber of bits
};

class BitFlipInjector : public Injector {
public:
    BitFlipInjector(BitFlipMode mode, uint32_t every_n = 0, uint32_t num_flips = 1);

    size_t inject(uint8_t* buffer, size_t data_len) override;

private:
    BitFlipMode mode;
    uint32_t every_n;
    uint32_t num_flips;

    static void flip_global_bit(uint8_t* buffer, size_t data_len, uint32_t bit_index);
};
