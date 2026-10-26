#pragma once
#define TX_AMPLITUDE 1500
#define SPS 10
#include <cstdint>

void generate_bits(uint8_t *bits, size_t num_bits);
void map_bpsk(const uint8_t *bits, int16_t *buffer, size_t num_symbols);
void map_qpsk(const uint8_t *bits, int16_t *buffer, size_t num_symbols);
void upsample_zeros(const int16_t *in_symbols, size_t num_symbols, int16_t *out_buffer, size_t sps);
void convolution(const int16_t *in_upsampled, int16_t *out_shaped, size_t total_samples);

