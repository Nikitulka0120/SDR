#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "RX.h"


static const int16_t h[SPS] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

void matched_filter(const int16_t *input_signal, int16_t *out_signal, size_t total_samples)
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
                acc_i += input_signal[2 * past_idx] * h[k];     // I
                acc_q += input_signal[2 * past_idx + 1] * h[k]; // Q
            }
        }

        out_signal[2 * n] = (int16_t)acc_i;
        out_signal[2 * n + 1] = (int16_t)acc_q;
    }
}

int find_packet_start(const int16_t *shaped_rx, size_t total_samples, int64_t threshold_power)
{
    const int confirm_samples = 5;
    int counter = 0;

    for (size_t i = 0; i < total_samples; ++i)
    {
        double i_val = (double)shaped_rx[2 * i];
        double q_val = (double)shaped_rx[2 * i + 1];
        double mag = sqrt(i_val * i_val + q_val * q_val);

        if (mag > threshold_mag)
        {
            counter++;
            if (counter >= confirm_samples)
            {
                return (int)(i - confirm_samples + 1);
            }
        }
        else
        {
            counter = 0;
        }
    }

    return -1;
}