#pragma once
#define TX_AMPLITUDE (300 << 4)
#define SPS 10

void generate_bits(uint8_t *bits, size_t string_lenght, const char *string);
void map_bpsk(const uint8_t *bits, int16_t *buffer, size_t num_symbols);
void map_qpsk(const uint8_t *bits, int16_t *buffer, size_t num_symbols);
void upsample_zeros(const int16_t *in_symbols, size_t num_symbols, int16_t *out_buffer, size_t sps);
void shaping_filter(const int16_t *in_upsampled, int16_t *out_shaped, size_t total_samples);

