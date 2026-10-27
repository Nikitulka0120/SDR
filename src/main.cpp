#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>

// imgui
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <thread>

#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"
#include "imgui.h"
#include "implot.h"

#include "sdr_configures.h"
#include "TX.h"

struct TxBuffers
{
    std::vector<uint8_t> tx_bits;   // сырой буфер бит
    std::vector<int16_t> mod_symbols;  // выход с маппера
    std::vector<int16_t> upsampled_buff; // выход с нулями
    std::vector<int16_t> tx_buff; // готовый буфер на отправку
};

void run_data_transmition(SDRconfig &SDR_config,
                          SoapyConfig &Soapy_config,
                          size_t rx_mtu,
                          size_t tx_mtu,
                          bool &running)
{
    size_t total_samples = tx_mtu;            // Столько комплексных сэмплов отдаем в ЦАП
    size_t num_symbols = total_samples / SPS; // Число полезных символов
    size_t num_bits = num_symbols;            // Количество битов (по умолчанию размер для bpsk)
    if (strcmp(SDR_config.mod_type, "qpsk") == 0)
    {
        num_bits = num_symbols * 2; // Для qpsk требуется в 2 раза больше бит
    }

    TxBuffers tx;
    tx.tx_bits.resize(num_bits);
    tx.mod_symbols.resize(2 * num_symbols);
    tx.upsampled_buff.resize(2 * total_samples);
    tx.tx_buff.resize(2 * total_samples);

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
            return;
        }
    }

    /* Количество прочитанных буфферов, для "Прореживания"
    передачи буферов */
    size_t buffers_read = 0;

    // Основной цикл, tx читает и передает, rx читает и пишет в файл
    while (running)
    {
        void *rx_buffs[] = {rx_buffer.data()};
        int flags;
        long long timeNs;

        int sr = SoapySDRDevice_readStream(Soapy_config.sdr, Soapy_config.rxStream,
                                           rx_buffs, rx_mtu, &flags, &timeNs, timeoutUs);
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
                generate_bits(tx.tx_bits.data(), num_bits);

                // Модулируем биты в комплексные сэмплы (BPSK или QPSK)
                if (strcmp(SDR_config.mod_type, "qpsk") == 0)
                {
                    map_qpsk(tx.tx_bits.data(), tx.mod_symbols.data(), num_symbols);
                }
                else if (strcmp(SDR_config.mod_type, "bpsk") == 0)
                {
                    map_bpsk(tx.tx_bits.data(), tx.mod_symbols.data(), num_symbols);
                }

                // Добиваем нулями
                upsample_zeros(tx.mod_symbols.data(), num_symbols, tx.upsampled_buff.data(), SPS);

                // Свертка
                convolution(tx.upsampled_buff.data(), tx.tx_buff.data(), total_samples);

                flags = SOAPY_SDR_HAS_TIME;
                void *tx_buffs[] = {tx.tx_buff.data()};

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
}

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

    //инициализация sdl
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    SDL_Window* window = SDL_CreateWindow(
        "DSP", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1024, 768, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    // Инициализация контекста Dear Imgui
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Включить Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Включить Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Включить Docking

    // 2.1) Привязка Imgui к SDL2 и OpenGl backend'ам
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Получение MTU, в нашем случае - размер буферов.
    size_t rx_mtu = SoapySDRDevice_getStreamMTU(Soapy_config.sdr, Soapy_config.rxStream);
    size_t tx_mtu = SoapySDRDevice_getStreamMTU(Soapy_config.sdr, Soapy_config.txStream);
    std::cout << "MTU - TX: " << tx_mtu << ", RX: " << rx_mtu << std::endl;

    bool running = true;
    std::thread sdr_thread(run_data_transmition,
                           std::ref(SDR_config),
                           std::ref(Soapy_config),
                           rx_mtu,
                           tx_mtu,
                           std::ref(running));

    // график
if((strcmp(SDR_config.mode, "rx") == 0)){


    bool gui_running = true;
    while (gui_running) {

        // 3.0) Обработка event'ов (inputs, window resize, mouse moving, etc.);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            std::cout << "Processing some event: "<< event.type << std::endl;
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                gui_running = false;
            }
        }

        // 3.1) Начинаем создавать новый фрейм;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_None);

        ImGui::Begin("My Window");
            if (ImPlot::BeginPlot("Scatter Plot")) {
        ImPlot::PlotScatter("Data 1", xs1, ys1, 100);
        ImPlot::PlotScatter("Data 2", xs2, ys2, 50, {
            ImPlotProp_Marker, ImPlotMarker_Square,
            ImPlotProp_MarkerSize, 6,
            ImPlotProp_LineColor, GetColormapColor(1),
            ImPlotProp_FillColor, GetColormapColor(1),
            ImPlotProp_FillAlpha, 0.25f
        });
        ImPlot::EndPlot();
    }
ImGui::End();

        // 3.3) Отправляем на рендер;
        ImGui::Render();
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
    }

    // 4) Закрываем приложение безопасно.
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

    running = false;
    if (sdr_thread.joinable())
    {
        sdr_thread.join();
    }
    // Основной цикл, tx читает и передает, rx читает и пишет в файл
    stop_streaming_and_clear(&Soapy_config);
    return EXIT_SUCCESS;
}