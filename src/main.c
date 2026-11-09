// #define HAS_SDR

#ifdef HAS_SDR
#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <complex.h>
#include <string.h>

#include "sdr_configures.h"
#include "TX.h"
#include "RX.h"

#ifndef HAS_SDR
#define DEFAULT_SIM_MTU 1920
#endif

int main(int argc, char *argv[])
{
    /* настройка параметров SDR в зависимости
    от передаваемого аргумента */
    SDRconfig SDR_config;
    if (set_params(argc, argv, &SDR_config) != 0)
    {
        return EXIT_FAILURE;
    };

#ifdef HAS_SDR
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

    size_t tx_mtu = SoapySDRDevice_getStreamMTU(Soapy_config.sdr, Soapy_config.txStream);
    size_t rx_mtu = 2 * tx_mtu;
#else
    printf("Запущена отладка приемника с использованием файла с семплами.\n");
    size_t tx_mtu = DEFAULT_SIM_MTU;
    size_t rx_mtu = 2 * DEFAULT_SIM_MTU;
#endif

    printf("MTU - TX: %lu, RX: %lu\n", tx_mtu, rx_mtu);

    size_t total_samples = tx_mtu;            // Столько комплексных сэмплов отдаем в ЦАП
    size_t num_symbols = total_samples / SPS; // Число полезных символов
    size_t num_bits = num_symbols;            // Количество битов (по умолчанию размер для bpsk)
    if (strcmp(SDR_config.mod_type, "qpsk") == 0)
    {
        num_bits = num_symbols * 2; // Для qpsk требуется в 2 раза больше бит
    }

    uint8_t tx_bits[num_bits];                 // Буфер сырых бит
    int16_t mod_symbols[2 * num_symbols];      // Буфер после маппера (без нулей)
    int16_t upsampled_buff[2 * total_samples]; // Буфер после мапера забитый нулями
    int16_t tx_buff[2 * total_samples];        // Буфер на передачу почле свертки

    int16_t rx_buffer[2 * rx_mtu]; // Буфер приема
    int16_t shaped_rx_buffer[2 * rx_mtu];

    memset(tx_bits, 0, num_bits);
    memset(mod_symbols, 0, sizeof(int16_t) * 2 * num_symbols);
    memset(upsampled_buff, 0, sizeof(int16_t) * 2 * total_samples);
    memset(tx_buff, 0, sizeof(int16_t) * 2 * total_samples);

#ifdef HAS_SDR
    SoapySDRDevice_activateStream(Soapy_config.sdr, Soapy_config.rxStream, 0, 0, 0);
    SoapySDRDevice_activateStream(Soapy_config.sdr, Soapy_config.txStream, 0, 0, 0);
#endif

    printf("Start test...\n");
    const long timeoutUs = 400000; // arbitrarily chosen (взяли из srsRAN)
    long long last_time = 0;
    FILE *file = NULL;
    FILE *out_file = NULL;
#ifdef HAS_SDR
    // Открытие файла для записи принятых данных из ЭФИРА
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
#else
    const char *sim_filename = "pluto1.pcm";
    file = fopen(sim_filename, "rb");
    if (file == NULL)
    {
        printf("Ошибка: не удалось открыть файл с записанными сэмплами: %s\n", sim_filename);
        return EXIT_FAILURE;
    }
#endif

    /* Количество прочитанных буфферов, для "Прореживания"
    передачи буферов */
    size_t buffers_read = 0;

    const char *tx_string = "NeverGiveUpKeepGoing2k26";
    const size_t string_size = strlen(tx_string);

    const char *out_filename = "filtered_rx.pcm";
    out_file = fopen(out_filename, "wb");
    if (out_file == NULL)
    {
        printf("Не удалось открыть файл %s для записи фильтрованных данных\n", out_filename);
        if (file != NULL)
            fclose(file);
        return EXIT_FAILURE;
    }

    // Основной цикл, tx читает и передает, rx читает и пишет в файл
    while (1)
    {
        int sr = 0;
        int flags = 0;
        long long timeNs = 0;

#ifdef HAS_SDR
        void *rx_buffs[] = {rx_buffer};
        sr = SoapySDRDevice_readStream(Soapy_config.sdr, Soapy_config.rxStream, rx_buffs, rx_mtu, &flags, &timeNs, timeoutUs);
        if (sr < 0)
        {
            continue;
        }

        // Пишем в файл ТОЛЬКО принятые RX-сэмплы на rx
        if ((strcmp(SDR_config.mode, "rx") == 0) && file != NULL)
        {
            fwrite(rx_buffer, 2 * (size_t)sr * sizeof(int16_t), 1, file);
        }
#else
        // Чтение из записанного файла
        size_t samples_read = fread(rx_buffer, 2 * sizeof(int16_t), rx_mtu, file);

        if (samples_read == 0)
        {
            if (feof(file))
            {
                fseek(file, 0, SEEK_SET);
                samples_read = fread(rx_buffer, 2 * sizeof(int16_t), rx_mtu, file);
                if (samples_read == 0)
                    break;
            }
            else
            {
                printf("Ошибка чтения файла сэмплов\n");
                break;
            }
        }
        sr = (int)samples_read;
        timeNs = last_time + 1000000;
#endif

        // printf("Buffer: %lu - Samples: %i, Flags: %i, Time: %lli, TimeDiff: %lli\n", buffers_read, sr, flags, timeNs, timeNs - last_time);
        last_time = timeNs;
        buffers_read++;

        matched_filter(rx_buffer, shaped_rx_buffer, (size_t)sr);

        if (out_file != NULL && sr > 0)
        {
            fwrite(shaped_rx_buffer, 2 * sizeof(int16_t), (size_t)sr, out_file);
        }

        int start_idx = find_packet_start(shaped_rx_buffer, (size_t)sr, 1000);
        if (start_idx >= 0)
        {
            printf("Найдено начало пакета: индекс %d (буфер %lu)\n", start_idx, buffers_read);
        }

#ifdef HAS_SDR
        if (strcmp(SDR_config.mode, "tx") == 0)
        {
            if (buffers_read % 10 == 0)
            {
                // Время передачи: на 4 мс в будущее
                long long tx_time = timeNs + (4 * 1000 * 1000);

                // Генерируем массив случайных бит
                generate_bits(tx_bits, string_size, tx_string);

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
                shaping_filter(upsampled_buff, tx_buff, total_samples);

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
#endif
    }
    if (file != NULL)
    {
        fclose(file);
    }
    if (out_file != NULL)
    {
        fclose(out_file);
    }

#ifdef HAS_SDR
    stop_streaming_and_clear(&Soapy_config);
#endif
    return EXIT_SUCCESS;
}