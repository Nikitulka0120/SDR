#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>

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
    std::cout << "MTU - TX: " << tx_mtu << ", RX: " << rx_mtu << std::endl;

    size_t total_samples = tx_mtu;            // Столько комплексных сэмплов отдаем в ЦАП
    size_t num_symbols = total_samples / SPS; // Число полезных символов
    size_t num_bits = num_symbols;            // Количество битов (по умолчанию размер для bpsk)
    if (strcmp(SDR_config.mod_type, "qpsk") == 0)
    {
        num_bits = num_symbols * 2; // Для qpsk требуется в 2 раза больше бит
    }

    std::vector<uint8_t> tx_bits(num_bits);                 // Буфер сырых бит
    std::vector<int16_t> mod_symbols(2 * num_symbols);      // Буфер после маппера (без нулей)
    std::vector<int16_t> upsampled_buff(2 * total_samples); // Буфер после мапера забитый нулями
    std::vector<int16_t> tx_buff(2 * total_samples);        // Буфер на передачу почле свертки

    std::vector<int16_t> rx_buffer(2 * rx_mtu); // Буфер приема

    SoapySDRDevice_activateStream(Soapy_config.sdr, Soapy_config.rxStream, 0, 0, 0);
    SoapySDRDevice_activateStream(Soapy_config.sdr, Soapy_config.txStream, 0, 0, 0);

    std::cout << "Start test..." << std::endl;
    const long timeoutUs = 400000; // arbitrarily chosen (взяли из srsRAN)

    long long last_time = 0;

    // Открытие файла если режим sdr == rx
    FILE *file = nullptr;
    if (strcmp(SDR_config.mode, "rx") == 0)
    {
        const char *filename_Rx = "pluto1.pcm";
        file = fopen(filename_Rx, "wb");
        if (!file)
        {
            std::cerr << "Не удалось открыть файл pluto1.pcm для записи" << std::endl;
            stop_streaming_and_clear(&Soapy_config);
            return EXIT_FAILURE;
        }
    }

    /* Количество прочитанных буфферов, для "Прореживания"
    передачи буферов */
    size_t buffers_read = 0;

    // Основной цикл, tx читает и передает, rx читает и пишет в файл
    while (1)
    {
        void *rx_buffs[] = {rx_buffer.data()};
        int flags;
        long long timeNs;

        int sr = SoapySDRDevice_readStream(Soapy_config.sdr, Soapy_config.rxStream, rx_buffs, rx_mtu, &flags, &timeNs, timeoutUs);
        if (sr < 0)
        {
            continue;
        }

        std::cout << "Buffer: " << buffers_read
                  << " - Samples: " << sr
                  << ", Flags: " << flags
                  << ", Time: " << timeNs
                  << ", TimeDiff: " << (timeNs - last_time)
                  << '\n';
        // Пишем в файл ТОЛЬКО принятые RX-сэмплы на rx
        if ((strcmp(SDR_config.mode, "rx") == 0) && file != nullptr)
        {
            fwrite(rx_buffer.data(), 2 * static_cast<size_t>(sr) * sizeof(int16_t), 1, file);
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
                generate_bits(tx_bits.data(), num_bits);

                // Модулируем биты в комплексные сэмплы (BPSK или QPSK)
                if (strcmp(SDR_config.mod_type, "qpsk") == 0)
                {
                    map_qpsk(tx_bits.data(), mod_symbols.data(), num_symbols);
                }
                else if (strcmp(SDR_config.mod_type, "bpsk") == 0)
                {
                    map_bpsk(tx_bits.data(), mod_symbols.data(), num_symbols);
                }

                // Добиваем нулями
                upsample_zeros(mod_symbols.data(), num_symbols, upsampled_buff.data(), SPS);

                // Свертка
                convolution(upsampled_buff.data(), tx_buff.data(), total_samples);

                flags = SOAPY_SDR_HAS_TIME;
                void *tx_buffs[] = {tx_buff.data()};

                // Отправка
                int st = SoapySDRDevice_writeStream(Soapy_config.sdr, Soapy_config.txStream,
                                                    (const void *const *)tx_buffs,
                                                    total_samples, &flags, tx_time, timeoutUs);
                if (static_cast<size_t>(st) != total_samples)
                {
                    std::cerr << "TX Failed: " << st << std::endl;
                }
            }
        }
    }

    if (file)
    {
        fclose(file);
    }

    stop_streaming_and_clear(&Soapy_config);
    return EXIT_SUCCESS;
}