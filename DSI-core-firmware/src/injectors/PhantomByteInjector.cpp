#include "injectors/PhantomByteInjector.h"

// by default: Mode=MANUAL, phantom_byte is 0x00, at offset 1
PhantomByteInjector::PhantomByteInjector(PhantomByteMode mode, uint32_t phantom_byte, uint32_t offset) : mode(mode), phantom_byte(phantom_byte), offset(offset){}

size_t PhantomByteInjector::inject(uint8_t* buffer, size_t data_len)
{
    if(buffer == nullptr)
    {
        Serial.println("[PhantomByteInjector] Error: buffer is null");
        return 0;
    }

    if(data_len>=512)
    {
        Serial.printf("[PhantomByteInjector] Error: data_len (%u) at max capacity, cannot inject\n", (unsigned)data_len);
        return data_len;
    }

    Serial.printf("[PhantomByteInjector] mode=%s, current_len=%u bytes, byte_to_inject=0x%02X\n",
                  (mode == PhantomByteMode::RANDOM) ? "RANDOM" : "MANUAL",
                  (unsigned)data_len, (unsigned)phantom_byte);

    // 2. Determine target index (Offset vs Index)
    uint32_t target_index;
    if (mode == PhantomByteMode::RANDOM) {
        // random(min, max) -> max is exclusive. 
        // We use data_len + 1 to allow injection at the very end (suffix).
        target_index = (uint32_t)random(0, data_len + 1);
    } else {
        // Clamp manual offset to current data_len to ensure it's contiguous
        target_index = (offset > data_len) ? (uint32_t)data_len : offset;
    }

    if(target_index > data_len)
    {
        target_index = data_len;
    }
    

    // 3. Shifting Logic
    // Start from the very end of the new size and pull data forward
    // to avoid overwriting the bytes we are moving.
    for (size_t i = data_len; i > target_index; i--) {
        if (i < 512)
        {
            buffer[i] = buffer[i - 1];
        }
    }

    // 4. Insertion
    buffer[target_index] = (uint8_t)phantom_byte;

    // 5. Important Logs
    Serial.printf("[PhantomByteInjector] injected 0x%02X at index %u\n", 
                  (unsigned)phantom_byte, (unsigned)target_index);
    
    if (target_index == 0) {
        Serial.println("[PhantomByteInjector] Note: Injection occurred at START (Prefix)");
    } else if (target_index == data_len) {
        Serial.println("[PhantomByteInjector] Note: Injection occurred at END (Suffix)");
    }

    // Return the new incremented length
    size_t new_len = data_len + 1;
    if(new_len > 512)
    {
        new_len = 512;
    }
    Serial.printf("[PhantomByteInjector] Done. New length: %u bytes\n", (unsigned)new_len);
    
    return new_len;
}