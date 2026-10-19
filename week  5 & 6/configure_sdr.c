#include "sdr_configures.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int set_params(int argc, char *argv[], SDRconfig *Config)
{
    if (argc < 2)
    {
        printf("Необходимо указать аргумент: 1 - Rx, 0 - Tx\n");
        return EXIT_FAILURE;
    }
    if (atoi(argv[1]) == 0)
    {
        Config->mode = 0;
        Config->rx_freq = 400e6;
        Config->tx_freq = 200e6;
        Config->rate = 1e6;
        Config->tx_gain = -40.;
        Config->rx_gain = 30.;
        strcpy(Config->uri, "usb:3.6.5");
    }
    else
    {
        Config->mode = 1;
        Config->rx_freq = 200e6;
        Config->tx_freq = 400e6;
        Config->rate = 1e6;
        Config->tx_gain = -40.;
        Config->rx_gain = 30.;
        strcpy(Config->uri, "usb:3.7.5");
    }
    return EXIT_SUCCESS;
}

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

int channels_configure(SDRconfig *SDR_config, SoapyConfig *SOAPY_config){
    size_t channels[] = {0};

    SOAPY_config->channels_count = sizeof(channels) / sizeof(channels[0]);
    SOAPY_config->rxStream = SoapySDRDevice_setupStream(SOAPY_config->sdr, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, SOAPY_config->channels_count, NULL);
    if (SOAPY_config->rxStream == NULL)
    {
        printf("setupStream rx fail: %s\n", SoapySDRDevice_lastError());
        SoapySDRDevice_unmake(SOAPY_config->sdr);
        return EXIT_FAILURE;
    }
    if(SoapySDRDevice_setGain(SOAPY_config->sdr, SOAPY_SDR_RX, channels, SDR_config->rx_gain) !=0 ){
        printf("setGain rx fail: %s\n", SoapySDRDevice_lastError());
    }
    if(SoapySDRDevice_setGain(SOAPY_config->sdr, SOAPY_SDR_TX, channels, SDR_config->tx_gain) !=0 ){
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
