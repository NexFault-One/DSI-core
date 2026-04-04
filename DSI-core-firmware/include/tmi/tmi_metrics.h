#pragma once
#include <stdint.h>
#include <cstring>


void TMI_AddBytesDropped(uint32_t n);
void TMI_AddBitsFlipped(uint32_t n);
void TMI_AddPhantomBytes(uint32_t n);
void TMI_AddModbusFrame(char* final_frame, uint32_t bytes);