#pragma once
#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>

typedef struct
{
    int mode;
    char uri[128];
    double tx_freq;
    double rx_freq;
    double rate;
    double tx_gain;
    double rx_gain;
} SDRconfig;

typedef struct
{
    SoapySDRDevice *sdr;
    SoapySDRStream *rxStream;
    SoapySDRStream *txStream;
    size_t channels_count;
} SoapyConfig;

int set_params(int argc, char *argv[], SDRconfig *Config);
int set_soapy_args (SDRconfig *SDR_config, SoapyConfig *SOAPY_config, const char *mode);
int rx_tx_configure (SDRconfig *SDR_config, SoapyConfig *SOAPY_config);
int channels_configure(SDRconfig *SDR_config, SoapyConfig *SOAPY_config);