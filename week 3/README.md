# Неделя 3. Подключение к SDR используя C код

## 1. Теория

### 1.1. Adalm Pluto. Архитектура
Adalm Pluto имеет два основных компонента - аналоговый радиочастотный приемопередатчик AD9363 и система на кристалле (SoC) Xilinx Zynq 7000 Series.

* **AD9363** содержит необходимые усилители, фильтры, а также цифро-аналоговые и аналого-цифровые преобразователи (12-bit ADC и DAC). Позволяет принимать и предоставлять IQ-сэмплы, формируемые или принимаемые системой на кристалле Zynq SoC. Пользователь может настроить несущую частоту, частоту дискретизации и т.д.
* **Система на кристалле** оснащена процессором ARM Cortex A9, работающим на частоте 667 МГц в сочетании с программируемой пользователем вентильной матрицей (FPGA). Пользовательские аппаратные модули реализованы в FPGA, обеспечивая связующий слой между прикладными процессорами (ARM Cortex) и радиочастотным приемопередатчиком (AD9363). Позволяет передавать и получать IQ-сэмплы пользовательскими приложениями, работающими на прикладных процессорах, через драйверы, размещенные в ядре Linux.

**Характеристики (Analog Devices):**
* RF coverage from 325 MHz to 3.8 GHz
* Up to 20 MHz of instantaneous bandwidth
* Flexible rate, 12-bit ADC and DAC
* One transmitter and one receiver, half or full duplex
* MATLAB®, Simulink® support
* GNU Radio sink and source blocks
* libiio, a C, C++, C#, and Python API

**Передача данных:**
Передача данных (IQ-сэмплов) между Adalm Pluto и хост-компьютером осуществляется посредством USB 2.0. Важно отметить, что в случае с SDR, данные необходимо передавать непрерывно в обе стороны (с хост-компьютера на SDR и обратно) одновременно. Хоть и теоретическая пропускная способность USB 2.0 равна 480 Mb/s, работа в полудуплексном режиме с передачей данных в обе стороны одновременно (с точки зрения пользователя) разительно снижается. Целевое значение (из опыта) частоты дискретизации желательно задавать в пределах ~6 Msps.

### 1.2. Timestamp. SoapySDR
Оригинальная статья энтузиаста, доработавшего проект `srsran/zynq_timestamping`, в рамках которого добавлены функции получения временных меток (timestamp) с ПЛИС (Xilinx Zynq). Временные метки (timestamp) привязаны к каждому запросу данных с буфера ПЛИС, что, в свою очередь, позволяет синхронно получать/передавать данные в потоках RX/TX.

Более того, из-за проблем с пропускной способностью USB 2.0 возникает проблема увеличения частоты дискретизации, при больших значениях которой USB 2.0 не может обеспечить полноценную передачу и прием (одновременных) сэмплов из Adalm Pluto на хост-компьютер. Выявить данную проблему можно благодаря реализации функции временных меток с Xilinx Zynq.

### 1.3. Установка необходимых библиотек

#### SoapySDR
```bash
sudo apt-get install python3-pip python3-setuptools
sudo apt-get install cmake g++ libpython3-dev python3-numpy swig python3-matplotlib

git clone --branch soapy-sdr-0.8.1 https://github.com/TelecomDep/SoapySDR.git
cd SoapySDR
mkdir build && cd build
cmake ../
make -j`nproc` # nproc - количество потоков, например make -j16 
sudo make install
sudo ldconfig
```

#### Libiio
```bash
sudo apt-get install libxml2 libxml2-dev bison flex libcdk5-dev cmake
sudo apt-get install libusb-1.0-0-dev libaio-dev pkg-config 
sudo apt install libavahi-common-dev libavahi-client-dev

git clone --branch v0.24 https://github.com/TelecomDep/libiio.git
cd libiio
mkdir build && cd build
cmake ../
make -j`nproc`
sudo make install
```

#### LibAD9361
```bash
git clone --branch v0.3 https://github.com/TelecomDep/libad9361-iio.git
cd libad9361-iio
mkdir build && cd build
cmake ../
make -j`nproc`
sudo make install
sudo ldconfig
```

#### SoapyPlutoSDR
```bash
git clone --branch sdr_gadget_timestamping https://github.com/TelecomDep/SoapyPlutoSDR.git
cd SoapyPlutoSDR
mkdir build && cd build
cmake ../
make -j`nproc`
sudo make install
sudo ldconfig
```


### 1.4. Работа с Adalm Pluto напрямую из C: сэмплы и буфер

#### Инициализация устройства в SoapySDR:
```c
SoapySDRKwargs args = {};
SoapySDRKwargs_set(&args, "driver", "plutosdr");        // Тип устройства 
SoapySDRKwargs_set(&args, "uri", "usb:");               // Способ обмена сэмплами (по USB)
SoapySDRKwargs_set(&args, "direct", "1");               // Прямой доступ
SoapySDRKwargs_set(&args, "timestamp_every", "1920");   // Размер буфера + временные метки
SoapySDRKwargs_set(&args, "loopback", "0");             // Используем антенны или нет
SoapySDRDevice *sdr = SoapySDRDevice_make(&args);       // Инициализация
SoapySDRKwargs_clear(&args);
```

#### Настройка параметров TX/RX:
```c
int sample_rate = 1e6;
int carrier_freq = 800e6;

// Параметры RX части
SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, sample_rate);
SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, carrier_freq, NULL);

// Параметры TX части
SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, 0, sample_rate);
SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, 0, carrier_freq, NULL);

size_t channels[] = {0};
SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, channels, 30.0);  // Чувствительность приемника
SoapySDRDevice_setGain(sdr, SOAPY_SDR_TX, channels, -40.0); // Усиление передатчика

// Потоки
size_t channel_count = sizeof(channels) / sizeof(channels[0]);
SoapySDRStream *rxStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, channel_count, NULL);
SoapySDRStream *txStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_TX, SOAPY_SDR_CS16, channels, channel_count, NULL);

SoapySDRDevice_activateStream(sdr, rxStream, 0, 0, 0);
SoapySDRDevice_activateStream(sdr, txStream, 0, 0, 0);
```

#### Выделение памяти под буферы:
Размер буфера определяется через MTU (Maximum Transmission Unit):
```c
size_t rx_mtu = SoapySDRDevice_getStreamMTU(sdr, rxStream);
size_t tx_mtu = SoapySDRDevice_getStreamMTU(sdr, txStream);

int16_t tx_buff[2 * tx_mtu];
int16_t rx_buffer[2 * rx_mtu];
```

#### Формирование сэмплов на передачу (TX):
В `tx_buff` каждые первые 16 бит - это $I$, вторые 16 бит - $Q$.
```c
for (int i = 0; i < 2 * tx_mtu; i += 2) {
    tx_buff[i] = 2000;
    tx_buff[i + 1] = 2000;
}

// Передача шаблона: флаг (FFFF FFFF), за которым следует 64-битный timestamp,
// разбитый на 8 байт и упакованный в младшие биты слов ЦАП со сдвигом влево
for (size_t i = 0; i < 2; i++) {
    tx_buff[0 + i] = 0xffff;
    tx_buff[10 + i] = 0xffff;
}
```

#### Получение сэмплов при приёме (RX) и передача по времени:
```c
for (size_t buffers_read = 0; buffers_read < iteration_count; buffers_read++) {
    void *rx_buffs[] = {rx_buffer};
    int flags;
    long long timeNs; // timestamp для принятого буфера

    int sr = SoapySDRDevice_readStream(sdr, rxStream, rx_buffs, rx_mtu, &flags, &timeNs, timeoutUs);
    printf("Buffer: %lu - Samples: %i, Flags: %i, Time: %lli, TimeDiff: %lli\n",
           buffers_read, sr, flags, timeNs, timeNs - last_time);
    fwrite(rx_buffer, 2 * rx_mtu * sizeof(int16_t), 1, file);
    last_time = timeNs;

    // Время отправки относительно текущего приема (+4 мс)
    long long tx_time = timeNs + (4 * 1000 * 1000);

    for (size_t i = 0; i < 8; i++) {
        uint8_t tx_time_byte = (tx_time >> (i * 8)) & 0xff;
        tx_buff[2 + i] = tx_time_byte << 4;
    }

    void *tx_buffs[] = {tx_buff};
    if (buffers_read == 2) {
        flags = SOAPY_SDR_HAS_TIME;
        int st = SoapySDRDevice_writeStream(sdr, txStream, (const void * const*)tx_buffs, tx_mtu, &flags, tx_time, timeoutUs);
    }
}
```

---

## 2. Практика: Сборка, запуск и результаты

### 2.1. CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.16)
project(PlutoSDR C)

set(CMAKE_BUILD_TYPE Debug)

option(INSTALL_DEPS "Установить зависимости" ON)
option(PLUTO_TIMESTAMP "Build pluto with timestamp" ON)

find_package(SoapySDR REQUIRED)

set(MAIN_SOURCE_FILES
    main.c)

add_executable(main ${MAIN_SOURCE_FILES})
target_link_libraries(main ${SoapySDR_LIBRARIES})
```

Сборка и запуск:
```bash
mkdir build && cd build
cmake ../
make
./main
```

### 2.2. Результаты выполнения

Лог выполнения программы в терминале:

![Лог выполнения Си-программы](../screens/execute%20result.png)

По факту работы:
* Программа подключилась к SDR через USB (`[INFO] USB direct mode enabled!`).
* Считано 10 буферов.
* На буфере №2 выполнена передача сэмплов (`buffers_read: 2`).

### 2.3. Бинарный файл txdata.pcm
Принятые отсчеты сохранены в файл `txdata.pcm`:

![Файл txdata.pcm в шестнадцатеричном виде](../screens/data%20of%20txdata.png)

### 2.4. График в Python
Чтение файла `txdata.pcm` выполнено скриптом на Python:

![График сэмплов из файла txdata.pcm](../screens/chart%20py.png)

На графике зафиксирован переданный радиоимпульс со ступенькой в районе отсчета 8000.
