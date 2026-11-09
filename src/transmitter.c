#include <stdint.h>
#include <stdlib.h>
#include "TX.h"

static const int16_t h[SPS] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

// Генератор битовой последовательности
void generate_bits(uint8_t *bits, size_t string_lenght, const char *string)
{
    for (size_t i = 0; i < string_lenght; i++){
        for (int j = 7; j >= 0; j--){
            uint8_t bit = ((uint8_t )string[i] >> j) & 1;
            bits[i * 8 + (7 - j)] = bit;
        }
    }
}

// Маппер bpsk
void map_bpsk(const uint8_t *bits, int16_t *buffer, size_t num_symbols)
{
    for (size_t sym = 0; sym < num_symbols; sym++)
    {
        uint8_t bit = bits[sym];

        buffer[2 * sym] = bit ? -TX_AMPLITUDE : TX_AMPLITUDE; // I
        buffer[2 * sym + 1] = 0;                              // Q, в bpsk всегда 0
    }
}

// Маппер qpsk
void map_qpsk(const uint8_t *bits, int16_t *buffer, size_t num_symbols)
{
    for (size_t sym = 0; sym < num_symbols; sym++)
    {
        uint8_t bit_i = bits[2 * sym];
        uint8_t bit_q = bits[2 * sym + 1];

        buffer[2 * sym] = bit_i ? -TX_AMPLITUDE : TX_AMPLITUDE;     // I
        buffer[2 * sym + 1] = bit_q ? -TX_AMPLITUDE : TX_AMPLITUDE; // Q
    }
}

// Первоначальная добивка нулями для апсемплинга
void upsample_zeros(const int16_t *in_symbols, size_t num_symbols, int16_t *out_buffer, size_t sps)
{
    for (size_t sym = 0; sym < num_symbols; sym++)
    {
        size_t base_idx = sym * sps * 2; // Номер символа * количество семплов на символ * 2 (т.к. комплексное)

        // Сам комплексный символ (I и Q)
        out_buffer[base_idx] = in_symbols[2 * sym];
        out_buffer[base_idx + 1] = in_symbols[2 * sym + 1];

        // Добивка 9 нулями
        for (size_t k = 1; k < sps; k++)
        {
            out_buffer[base_idx + 2 * k] = 0;     // I
            out_buffer[base_idx + 2 * k + 1] = 0; // Q
        }
    }
}

// свертка
void shaping_filter(const int16_t *in_upsampled, int16_t *out_shaped, size_t total_samples)
{
    for (size_t n = 0; n < total_samples; n++)
    {
        int32_t acc_i = 0;
        int32_t acc_q = 0;

        for (size_t k = 0; k < SPS; k++)
        {
            if (n >= k)
            {
                size_t past_idx = n - k;
                acc_i += in_upsampled[2 * past_idx] * h[k];     // I
                acc_q += in_upsampled[2 * past_idx + 1] * h[k]; // Q
            }
        }

        out_shaped[2 * n] = (int16_t)acc_i;
        out_shaped[2 * n + 1] = (int16_t)acc_q;
    }
}
