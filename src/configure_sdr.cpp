#include "sdr_configures.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <strings.h>

// Настройка SDR в зависимости от переданных параметров
int set_params(int argc, char *argv[], SDRconfig *Config)
{
    if (argc < 2)
    {
        printf("Использование: %s <tx|rx> [bpsk|qpsk]\n", argv[0]);
        return EXIT_FAILURE;
    }

    // tx или rx
    if (strcasecmp(argv[1], "tx") == 0)
    {
        strcpy(Config->mode, "tx");
        Config->rx_freq = 400e6;
        Config->tx_freq = 200e6;
        Config->rate = 1e6;
        Config->tx_gain = -40.0;
        Config->rx_gain = 30.0;
        strcpy(Config->uri, "usb:3.6.5");
    }
    else if (strcasecmp(argv[1], "rx") == 0)
    {
        strcpy(Config->mode, "rx");
        Config->rx_freq = 200e6;
        Config->tx_freq = 400e6;
        Config->rate = 1e6;
        Config->tx_gain = -40.0;
        Config->rx_gain = 30.0;
        strcpy(Config->uri, "usb:3.7.5");
    }
    else
    {
        printf("Ошибка: неизвестный режим '%s'. Допустимо только 'tx' или 'rx'.\n", argv[1]);
        return EXIT_FAILURE;
    }

    // Выбор режима модуляции
    if (argc >= 3 && strcasecmp(argv[2], "qpsk") == 0)
    {
        strcpy(Config->mod_type, "qpsk");
    }
    else
    {
        strcpy(Config->mod_type, "bpsk");
    }

    printf("Конфигурация: Mode = %s, Modulation = %s, URI = %s\n",
           Config->mode, Config->mod_type, Config->uri);

    return EXIT_SUCCESS;
}

// установка аргументов SoaPY
int set_soapy_args(SDRconfig *SDR_config, SoapyConfig *SOAPY_config, const char *mode)
{
    SoapySDRKwargs args = {0};
    SoapySDRKwargs_set(&args, "driver", "plutosdr");
    if (strcmp(mode, "uri") == 0)
    {
        SoapySDRKwargs_set(&args, "uri", SDR_config->uri);
    }
    else
    {
        SoapySDRKwargs_set(&args, "uri", "ip:192.168.2.1");
    }
    SoapySDRKwargs_set(&args, "direct", "1");
    SoapySDRKwargs_set(&args, "timestamp_every", "1920");
    SoapySDRKwargs_set(&args, "loopback", "0");
    SOAPY_config->sdr = SoapySDRDevice_make(&args);
    SoapySDRKwargs_clear(&args);

    if (SOAPY_config->sdr == NULL)
    {
        printf("SoapySDRDevice_make fail: %s\n", SoapySDRDevice_lastError());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

// Настройка rx и tx на SRR
int rx_tx_configure(SDRconfig *SDR_config, SoapyConfig *SOAPY_config)
{
    if (SOAPY_config->sdr == NULL)
    {
        printf("rx_tx_configure: sdr is NULL\n");
        return EXIT_FAILURE;
    }
    if (SoapySDRDevice_setSampleRate(SOAPY_config->sdr, SOAPY_SDR_RX, 0, SDR_config->rate) != 0)
    {
        printf("setSampleRate rx fail: %s\n", SoapySDRDevice_lastError());
        return EXIT_FAILURE;
    }
    if (SoapySDRDevice_setFrequency(SOAPY_config->sdr, SOAPY_SDR_RX, 0, SDR_config->rx_freq, NULL) != 0)
    {
        printf("setFrequency rx fail: %s\n", SoapySDRDevice_lastError());
        return EXIT_FAILURE;
    }
    if (SoapySDRDevice_setSampleRate(SOAPY_config->sdr, SOAPY_SDR_TX, 0, SDR_config->rate) != 0)
    {
        printf("setSampleRate tx fail: %s\n", SoapySDRDevice_lastError());
        return EXIT_FAILURE;
    }
    if (SoapySDRDevice_setFrequency(SOAPY_config->sdr, SOAPY_SDR_TX, 0, SDR_config->tx_freq, NULL) != 0)
    {
        printf("setFrequency tx fail: %s\n", SoapySDRDevice_lastError());
        return EXIT_FAILURE;
    }
    printf("SoapySDRDevice_getFrequency tx: %lf\n", SoapySDRDevice_getFrequency(SOAPY_config->sdr, SOAPY_SDR_TX, 0));
    return EXIT_SUCCESS;
}

// Настройка каналов SDR
int channels_configure(SDRconfig *SDR_config, SoapyConfig *SOAPY_config)
{
    size_t channels[] = {0};

    SOAPY_config->channels_count = sizeof(channels) / sizeof(channels[0]);
    SOAPY_config->rxStream = SoapySDRDevice_setupStream(SOAPY_config->sdr, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, SOAPY_config->channels_count, NULL);
    if (SOAPY_config->rxStream == NULL)
    {
        printf("setupStream rx fail: %s\n", SoapySDRDevice_lastError());
        SoapySDRDevice_unmake(SOAPY_config->sdr);
        return EXIT_FAILURE;
    }
    if (SoapySDRDevice_setGain(SOAPY_config->sdr, SOAPY_SDR_RX, channels[0], SDR_config->rx_gain) != 0)
    {
        printf("setGain rx fail: %s\n", SoapySDRDevice_lastError());
    }
    if (SoapySDRDevice_setGain(SOAPY_config->sdr, SOAPY_SDR_TX, channels[0], SDR_config->tx_gain) != 0)
    {
        printf("setGain tx fail: %s\n", SoapySDRDevice_lastError());
    }
    SOAPY_config->txStream = SoapySDRDevice_setupStream(SOAPY_config->sdr, SOAPY_SDR_TX, SOAPY_SDR_CS16, channels, SOAPY_config->channels_count, NULL);
    if (SOAPY_config->txStream == NULL)
    {
        printf("setupStream tx fail: %s\n", SoapySDRDevice_lastError());
        SoapySDRDevice_unmake(SOAPY_config->sdr);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

// Завершение работы и отчистка ресурсов
int stop_streaming_and_clear(SoapyConfig *SOAPY_config)
{
    // stop streaming
    SoapySDRDevice_deactivateStream(SOAPY_config->sdr, SOAPY_config->rxStream, 0, 0);
    SoapySDRDevice_deactivateStream(SOAPY_config->sdr, SOAPY_config->txStream, 0, 0);

    // shutdown the stream
    SoapySDRDevice_closeStream(SOAPY_config->sdr, SOAPY_config->rxStream);
    SoapySDRDevice_closeStream(SOAPY_config->sdr, SOAPY_config->txStream);

    // cleanup device handle
    SoapySDRDevice_unmake(SOAPY_config->sdr);

    // Process each rx buffer, looking for transmitted timestamp
    for (size_t j = 0; j < SOAPY_config->channels_count; j++)
    {
        printf("Checking channel %zu\n", j);
    }

    printf("test complete!\n");
    return EXIT_SUCCESS;
}
