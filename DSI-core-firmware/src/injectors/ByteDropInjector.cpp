#include "injectors/ByteDropInjector.h"
#include <Arduino.h>


ByteDropInjector::ByteDropInjector(uint32_t numBytes, uint32_t every_n) : numBytes(numBytes), every_n(every_n)
{}

size_t ByteDropInjector::inject(uint8_t* buffer, size_t data_len)
{
    if (data_len == 0)
    {
        Serial.println("[BytesDropInjector] called with data_len=0");
        return 0;
    }

    // drop numBytes at every n and check if it is within bounds
    size_t drop_index = (every_n == 0 ? (data_len - numBytes) : ((every_n - 1) % data_len));

    Serial.printf("[BytesDropInjector] in=%u, every_n %u, numBytes=%u, drop_index=%u \n", (unsigned)data_len, (unsigned)every_n, (unsigned)numBytes, (unsigned)drop_index);
    Serial.print("Dropped hexs: ");
    for (size_t i = 0; i < numBytes; i++)
    {
        if(numBytes + i < data_len)
        {
            Serial.printf("0x%02X ", buffer[numBytes+i]);
        }
    }
    Serial.println();

    // shift left
    for (size_t i=drop_index+i; i<data_len; ++i)
    {
        buffer[i-numBytes] = buffer[i];
    }
    size_t out = data_len-numBytes;
    // print for logs
    Serial.printf("[BytesDropInjector] original size data out: %u\n", numBytes);
    Serial.printf("[BytesDropInjector] size data out: %u\n", out);
    Serial.printf("[BytesDropInjector] data str len: %u\n", data_len);
    return out;
}
