#pragma once
#include <iostream>
#include <stdio.h>
#include <Arduino.h>
#include "tasks/tmi_tasks.h"

class Injector
{
public:
    Injector() {}
    virtual size_t inject(uint8_t* buffer, size_t data_len) = 0;
    virtual ~Injector() = default;
};