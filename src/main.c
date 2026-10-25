#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <complex.h>
#include <string.h>

#include "sdr_configures.h"
#include "TX.h"

int main(int argc, char *argv[])
{
    /* настройка параметров SDR в зависимости
    от передаваемого аргумента */
    SDRconfig SDR_config;
    if (set_params(argc, argv, &SDR_config) != 0)
    {
        return EXIT_FAILURE;
    };

    // настройка аргументов SoapySDR
    SoapyConfig Soapy_config;
    if (set_soapy_args(&SDR_config, &Soapy_config, "uri") != 0)
    {
        return EXIT_FAILURE;
    };

    // настройка устройств rx/tx
    if (rx_tx_configure(&SDR_config, &Soapy_config) != 0)
    {
        return EXIT_FAILURE;
    };

    // Настройка каналов и стримов
    if (channels_configure(&SDR_config, &Soapy_config) != 0)
    {
        return EXIT_FAILURE;
    };

    // Получение MTU, в нашем случае - размер буферов.
    size_t rx_mtu = SoapySDRDevice_getStreamMTU(Soapy_config.sdr, Soapy_config.rxStream);
    size_t tx_mtu = SoapySDRDevice_getStreamMTU(Soapy_config.sdr, Soapy_config.txStream);
    printf("MTU - TX: %lu, RX: %lu\n", tx_mtu, rx_mtu);

    size_t total_samples = tx_mtu;            // Столько комплексных сэмплов отдаем в ЦАП
    size_t num_symbols = total_samples / SPS; // Число полезных символов
    size_t num_bits  = num_symbols;            // Количество битов (по умолчанию размер для bpsk)
    if (strcmp(SDR_config.mod_type, "qpsk") == 0)
    {
        num_bits = num_symbols * 2;             // Для qpsk требуется в 2 раза больше бит
    }

    uint8_t tx_bits[num_bits];            // Буфер сырых бит
    int16_t mod_symbols[2 * num_symbols]; // Буфер после маппера (без нулей)
    int16_t upsampled_buff[2 * total_samples]; // Буфер после мапера забитый нулями
    int16_t tx_buff[2 * total_samples]; // Буфер на передачу почле свертки

    int16_t rx_buffer[2 * rx_mtu];  // Буфер приема


    SoapySDRDevice_activateStream(Soapy_config.sdr, Soapy_config.rxStream, 0, 0, 0);
    SoapySDRDevice_activateStream(Soapy_config.sdr, Soapy_config.txStream, 0, 0, 0);


    printf("Start test...\n");
    const long timeoutUs = 400000; // arbitrarily chosen (взяли из srsRAN)

    long long last_time = 0;

    // Открытие файла если режим sdr == rx
    FILE *file = NULL;
    if (strcmp(SDR_config.mode, "rx") == 0)
    {
        const char *filename_Rx = "pluto1.pcm";
        file = fopen(filename_Rx, "wb");
        if (file == NULL)
        {
            printf("Не удалось открыть файл %s для записи\n", filename_Rx);
            SoapySDRDevice_deactivateStream(Soapy_config.sdr, Soapy_config.rxStream, 0, 0);
            SoapySDRDevice_deactivateStream(Soapy_config.sdr, Soapy_config.txStream, 0, 0);
            SoapySDRDevice_closeStream(Soapy_config.sdr, Soapy_config.rxStream);
            SoapySDRDevice_closeStream(Soapy_config.sdr, Soapy_config.txStream);
            SoapySDRDevice_unmake(Soapy_config.sdr);
            return EXIT_FAILURE;
        }
    }

    /* Количество прочитанных буфферов, для "Прореживания"
    передачи буферов */
    size_t buffers_read = 0;

    // Основной цикл, tx читает и передает, rx читает и пишет в файл
    while (1)
    {
        void *rx_buffs[] = {rx_buffer};
        int flags;
        long long timeNs;

        int sr = SoapySDRDevice_readStream(Soapy_config.sdr, Soapy_config.rxStream, rx_buffs, rx_mtu, &flags, &timeNs, timeoutUs);
        if (sr < 0)
        {
            continue;
        }

        printf("Buffer: %lu - Samples: %i, Flags: %i, Time: %lli, TimeDiff: %lli\n", buffers_read, sr, flags, timeNs, timeNs - last_time);
        // Пишем в файл ТОЛЬКО принятые RX-сэмплы на rx
        if ((strcmp(SDR_config.mode, "rx") == 0) && file != NULL)
        {
            fwrite(rx_buffer, 2 * (size_t)sr * sizeof(int16_t), 1, file);
        }
        last_time = timeNs;
        buffers_read++;

        if (strcmp(SDR_config.mode, "tx") == 0)
        {
            if (buffers_read % 2 == 0)
            {
                // Время передачи: на 4 мс в будущее
                long long tx_time = timeNs + (4 * 1000 * 1000);

                // Генерируем массив случайных бит
                generate_bits(tx_bits, num_bits);

                // Модулируем биты в комплексные сэмплы (BPSK или QPSK) 
                if (strcmp(SDR_config.mod_type, "qpsk") == 0)
                {
                    map_qpsk(tx_bits, mod_symbols, num_symbols);
                }
                else if (strcmp(SDR_config.mod_type, "bpsk") == 0)
                {
                    map_bpsk(tx_bits, mod_symbols, num_symbols);
                }

                // Добиваем нулями
                upsample_zeros(mod_symbols, num_symbols, upsampled_buff, SPS);

                // Свертка
                convolution(upsampled_buff, tx_buff, total_samples);

                flags = SOAPY_SDR_HAS_TIME;
                void *tx_buffs[] = {tx_buff};

                // Отправка
                int st = SoapySDRDevice_writeStream(Soapy_config.sdr, Soapy_config.txStream,
                                                    (const void *const *)tx_buffs,
                                                    total_samples, &flags, tx_time, timeoutUs);
                if ((size_t)st != total_samples)
                {
                    printf("TX Failed: %i\n", st);
                }
            }
        }
    }

    if (file != NULL)
    {
        fclose(file);
    }

    stop_streaming_and_clear(&Soapy_config);
    return EXIT_SUCCESS;
}