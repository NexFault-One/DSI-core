#pragma once
#include <stdio.h>

class Injector
{
public:
    Injector();
    virtual size_t inject(uint8_t* buffer, size_t data_len) = 0;
};