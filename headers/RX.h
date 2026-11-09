#define SPS 10

void matched_filter(const int16_t *input_signal, int16_t *out_signal, size_t total_samples);
int find_packet_start(const int16_t *shaped_rx, size_t total_samples, int64_t threshold_power);