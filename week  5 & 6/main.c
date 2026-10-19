#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <stdio.h> //printf
#include <stdlib.h> //free
#include <stdint.h>
#include <complex.h>

#include "sdr_configures.h"

int main(int argc, char *argv[])
{
    /* настройка параметров SDR в зависимости 
    от передаваемого аргумента */
    SDRconfig SDR_config;
    if(set_params(argc, argv, &SDR_config) != 0){
        return EXIT_FAILURE;
    };

    // настройка аргументов SoapySDR
    SoapyConfig Soapy_config;
    if(set_soapy_args(&SDR_config, &Soapy_config, "uri") != 0){
        return EXIT_FAILURE;
    };

    // настройка устройств rx/tx
    if(rx_tx_configure(&SDR_config, &Soapy_config) != 0){
        return EXIT_FAILURE;
    };
    
    // Настройка каналов и стримов
    if(channels_configure(&SDR_config, &Soapy_config) != 0){
        return EXIT_FAILURE;
    };

    // Получение MTU (Maximum Transmission Unit), в нашем случае - размер буферов. 
    size_t rx_mtu = SoapySDRDevice_getStreamMTU(Soapy_config.sdr, Soapy_config.rxStream);
    size_t tx_mtu = SoapySDRDevice_getStreamMTU(Soapy_config.sdr, Soapy_config.txStream);
    printf("MTU - TX: %lu, RX: %lu\n", tx_mtu, rx_mtu);
    

    int16_t tx_buff[2*tx_mtu];
    int16_t rx_buffer[2*rx_mtu];
    //int16_t *rx_buffer = (int16_t *)malloc(2 * rx_mtu * sizeof(int16_t));

    //заполнение tx_buff значениями сэмплов первые 16 бит - I, вторые 16 бит - Q.
    for (int i = 0; i < 2 * tx_mtu; i+=2)
    {
        tx_buff[i] = 2000 ;
        tx_buff[i+1] = 2000 ;
    }

    //prepare fixed bytes in transmit buffer
    //we transmit a pattern of FFFF FFFF [TS_0]00 [TS_1]00 [TS_2]00 [TS_3]00 [TS_4]00 [TS_5]00 [TS_6]00 [TS_7]00 FFFF FFFF
    //that is a flag (FFFF FFFF) followed by the 64 bit timestamp, split into 8 bytes and packed into the lsb of each of the DAC words.
    //DAC samples are left aligned 12-bits, so each byte is left shifted into place
    for(size_t i = 0; i < 2; i++)
    {
        tx_buff[0 + i] = 0xffff;
        // 8 x timestamp words
        tx_buff[10 + i] = 0xffff;
    }

    //activate streams
    SoapySDRDevice_activateStream(Soapy_config.sdr, Soapy_config.rxStream, 0, 0, 0); //start streaming
    SoapySDRDevice_activateStream(Soapy_config.sdr, Soapy_config.txStream, 0, 0, 0); //start streaming

    //here goes
    printf("Start test...\n");
    const long  timeoutUs = 400000; // arbitrarily chosen (взяли из srsRAN)

    long long last_time = 0;

    // Пример по записи сэмплов в фай. Не использовать в Release, операция не быстрая
    #if 0
    FILE *file = fopen("txdata.pcm", "a+");
    fwrite(txp[0], proc->writeBlockSize * sizeof(int), 1, file);
    fclose(file);
    #endif

    // Всегда пишем только RX-сэмплы в один файл
    const char *filename_Rx = "pluto1.pcm";
    FILE *file = fopen(filename_Rx, "w");
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

    // Количество итераций
    size_t iteration_count = 5000;
    // Начинается работа с получением и отправкой сэмплов
    for (size_t buffers_read = 0; buffers_read < iteration_count; buffers_read++)
    {
        void *rx_buffs[] = {rx_buffer};
        int flags;        // flags set by receive operation
        long long timeNs; //timestamp for receive buffer

        int sr = SoapySDRDevice_readStream(Soapy_config.sdr, Soapy_config.rxStream, rx_buffs, rx_mtu, &flags, &timeNs, timeoutUs);
        if (sr < 0)
        {
            // Skip read on error (likely timeout)
            continue;
        }
  
        // Dump info
        printf("Buffer: %lu - Samples: %i, Flags: %i, Time: %lli, TimeDiff: %lli\n", buffers_read, sr, flags, timeNs, timeNs - last_time);
        // Пишем в файл ТОЛЬКО принятые RX-сэмплы
        fwrite(rx_buffer, 2 * rx_mtu * sizeof(int16_t), 1, file);
        last_time = timeNs;

        // Переменная для времени отправки сэмплов относительно текущего приема
        long long tx_time = timeNs + (4 * 1000 * 1000); // на 4 [мс] в будущее

        // Добавляем время, когда нужно передать блок tx_buff, через N -наносекунд
        for(size_t i = 0; i < 8; i++)
        {
            // Extract byte from tx time
            uint8_t tx_time_byte = (tx_time >> (i * 8)) & 0xff;

            // Add byte to buffer
            tx_buff[2 + i] = tx_time_byte << 4;
        }

        // Send buffer
        void *tx_buffs[] = {tx_buff};
        if( (buffers_read % 2 == 0 && buffers_read > 0) ){
            printf("buffers_read: %d\n", buffers_read);
            flags = SOAPY_SDR_HAS_TIME;
            int st = SoapySDRDevice_writeStream(Soapy_config.sdr, Soapy_config.txStream, (const void * const*)tx_buffs, tx_mtu, &flags, tx_time, timeoutUs);
            if ((size_t)st != tx_mtu)
            {
                printf("TX Failed: %i\n", st);
            }
        }
        
    }
    fclose(file);

    //stop streaming
    SoapySDRDevice_deactivateStream(Soapy_config.sdr, Soapy_config.rxStream, 0, 0);
    SoapySDRDevice_deactivateStream(Soapy_config.sdr, Soapy_config.txStream, 0, 0);

    //shutdown the stream
    SoapySDRDevice_closeStream(Soapy_config.sdr, Soapy_config.rxStream);
    SoapySDRDevice_closeStream(Soapy_config.sdr, Soapy_config.txStream);

    //cleanup device handle
    SoapySDRDevice_unmake(Soapy_config.sdr);

    // Process each rx buffer, looking for transmitted timestamp
    for (size_t j = 0; j < Soapy_config.channels_count; j++)
    {
        printf("Checking channel %zu\n", j);
        //check_channel(sample_count, channel_count, j, tx_timestamps, rx_timestamps, (uint16_t**)rx_buff, rx_mtu);
    }

    //all done
    printf("test complete!\n");

    return EXIT_SUCCESS;
}