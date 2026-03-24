#include "injectors/ByteDropInjector.h"
#include <Arduino.h>


ByteDropInjector::ByteDropInjector(uint32_t numBytes, uint32_t every_n) : numBytes(numBytes), every_n(every_n)
{}

size_t ByteDropInjector::inject(uint8_t* buffer, size_t data_len)
{
    if (!buffer || data_len == 0) {
        Serial.println("[ByteDropInjector] called with data_len=0 or buffer=null");
        return 0;
    }

    if (numBytes == 0) {
        return data_len;
    }

    // Determine drop index safely
    size_t drop_index = 0;
    if (every_n == 0) {
        // default to start of buffer
        drop_index = 0;
    } else {
        drop_index = (every_n - 1) % data_len;
    }

    // clamp drop length so we don't drop past the end
    size_t actual_drop = numBytes;
    if (actual_drop > data_len - drop_index) {
        actual_drop = data_len - drop_index;
    }
    if (actual_drop == 0) {
        return data_len;
    }
    TMI_LockReport();
    tmi_data.report.bytes_dropped = numBytes;
    TMI_UnlockReport();
    Serial.printf("[ByteDropInjector] in=%u, every_n=%u, numBytes=%u, drop_index=%u\n",
                  (unsigned)data_len, (unsigned)every_n, (unsigned)numBytes, (unsigned)drop_index);

    // optional: print dropped bytes (guarded)
    Serial.print("Dropped hex values: ");
    for (size_t i = 0; i < actual_drop && (drop_index + i) < data_len; ++i) {
        Serial.printf("0x%02X ", buffer[drop_index + i]);
    }
    Serial.println();

    // Shift remainder left safely using memmove
    size_t tail_len = data_len - (drop_index + actual_drop);
    if (tail_len > 0) {
        memmove(buffer + drop_index, buffer + drop_index + actual_drop, tail_len);
    }

    size_t out = data_len - actual_drop;

    Serial.printf("[ByteDropInjector] dropped %u bytes, new_len=%u\n", (unsigned)actual_drop, (unsigned)out);
    return out;
}
