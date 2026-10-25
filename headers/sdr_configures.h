#pragma once
#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>

typedef struct
{
    char mode[16];        // tx rx
    char mod_type[16];    // qpsk bpsk
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
int stop_streaming_and_clear(SoapyConfig *SOAPY_config);