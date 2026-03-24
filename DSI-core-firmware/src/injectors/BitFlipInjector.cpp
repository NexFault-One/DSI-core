#include "injectors/BitFlipInjector.h"


BitFlipInjector::BitFlipInjector(BitFlipMode mode, uint32_t every_n, uint32_t num_flips) : mode(mode), every_n(every_n), num_flips(num_flips)
{

}

void BitFlipInjector::flip_global_bit(uint8_t* buffer, size_t data_len, uint32_t bit_index)
{
    size_t total_bits = data_len*8;
    if(total_bits == 0 || bit_index >= total_bits) return;
    
    size_t byte_index = bit_index / 8;
    uint8_t bit_in_byte = bit_index % 8; // 0 = lsb
    uint8_t mask = (1u << bit_in_byte);
    buffer[byte_index] ^= mask;
}

size_t BitFlipInjector::inject(uint8_t* buffer, size_t data_len)
{
  if (data_len == 0 || buffer == nullptr || data_len > 512) {
        Serial.println("[BitFlipInjector] called with data_len=0 or buffer=null or data_len > 512");
        return 0;
    }

    size_t total_bits = data_len * 8;
    if (total_bits == 0) {
        Serial.println("[BitFlipInjector] no bits to flip");
        return data_len;
    }

    Serial.printf("[BitFlipInjector] mode=%s, data_len=%u bytes (%u bits)\n",
                  (mode == BitFlipMode::PERIODIC) ? "PERIODIC" : "RANDOM",
                  (unsigned)data_len, (unsigned)total_bits);

    if (mode == BitFlipMode::PERIODIC) {
        if (every_n == 0) {
            Serial.println("[BitFlipInjector] PERIODIC mode but every_n == 0 -> nothing to do");
            return data_len;
        }

        Serial.printf("[BitFlipInjector] flipping one bit every %u bits\n", (unsigned)every_n);

        // Flip bits at positions every_n-1, 2*every_n-1, ... (0-based bit indices)
        // Example: every_n=2 flips bits 1,3,5,...
        uint32_t flipped = 0;
        for (uint32_t bit = every_n - 1; bit < total_bits; bit += every_n) {
            flip_global_bit(buffer, data_len, bit);
            Serial.printf("[BitFlipInjector] flipped bit index %u (byte %u bit %u)\n",
                          (unsigned)bit,
                          (unsigned)(bit / 8),
                          (unsigned)(bit % 8));
            ++flipped;
            // protect against overflow of bit variable when every_n==0, but we checked earlier
            if (every_n == 0) break;
            // stop if next increment would overflow (guard)
            if (bit + every_n < bit) break;
        }
        Serial.printf("[BitFlipInjector] PERIODIC flipped total: %u\n", (unsigned)flipped);
    }
    else { // RANDOM
        if (num_flips == 0) {
            Serial.println("[BitFlipInjector] RANDOM mode but num_flips == 0 -> nothing to do");
            return data_len;
        }

        // Bound number of flips to total bits so we don't attempt impossible unique flips
        uint32_t flips_to_do = num_flips;
        if (flips_to_do > total_bits) flips_to_do = (uint32_t)total_bits;
        TMI_LockReport();
        tmi_data.report.bits_flipped = num_flips;
        TMI_UnlockReport();
        Serial.printf("[BitFlipInjector] RANDOM mode flipping %u bits\n", (unsigned)flips_to_do);

        // We'll avoid flipping the same bit twice by tracking a small bitmap of visited bits.
        // For large messages this vector is memory-heavy. You can duplicate to cancel if too much memory.
        // Here we use a dynamic bitmap sized to total_bits.
        // If total_bits is huge this might fail; but typical messages are modest.
        std::vector<uint8_t> visited((total_bits + 7) / 8, 0);
        if(total_bits > 8192)
        {
            Serial.println("[BitFlipInjector] WARNING: large message size may cause high memory usage for visited bitmap");
            return data_len;
        }

        uint32_t done = 0;
        uint32_t attempts = 0;
        const uint32_t max_attempts = flips_to_do * 10 + 100; // safety to avoid infinite loops

        while (done < flips_to_do && attempts < max_attempts) {
            // random() [0, n[
            unsigned long r = (unsigned long)random(total_bits); // total_bits fits into unsigned long for typical sizes
            uint32_t bit_index = (uint32_t)r;

            size_t idx = bit_index / 8;
            uint8_t bitmask = (1u << (bit_index % 8));
            if ((visited[idx] & bitmask) == 0) {
                // not yet visited
                visited[idx] |= bitmask;
                flip_global_bit(buffer, data_len, bit_index);
                Serial.printf("[BitFlipInjector] flipped random bit %u (byte %u bit %u)\n",
                              (unsigned)bit_index,
                              (unsigned)(bit_index / 8),
                              (unsigned)(bit_index % 8));
                ++done;
            }
            ++attempts;
        }

        Serial.printf("[BitFlipInjector] RANDOM done flips=%u attempts=%u\n", (unsigned)done, (unsigned)attempts);
    }

    // Bit flipping doesn't change length
    return data_len;
}