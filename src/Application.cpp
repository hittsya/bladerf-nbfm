#include "Application.h"
#include "Logger.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <pthread.h>
#include <sched.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>

template<typename T>
void decimateSig(std::vector<T>& sig, std::vector<T>& out, core::DecimatorCoeffs const& decim)
{
    out.clear();

    size_t const cap = sig.size() / decim.M;
    if (out.capacity() != cap)
        out.reserve(cap);

    size_t const N = sig.size();

    for (auto n = 0; n < N; n += decim.M) {
        T acc = 0.0f;

        for (size_t k = 0; k < decim.num_taps; ++k)
            if (n >= k)
                acc += decim.taps[k] * sig[n-k];

        out.push_back(acc);
    }
}

bool core::Application::setup()
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    m_window = SDL_CreateWindow("FM", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    m_gl_context = SDL_GL_CreateContext(m_window);
    SDL_GL_MakeCurrent(m_window, m_gl_context);
    SDL_GL_SetSwapInterval(1);

    SDL_zero(m_audio_spec);
    m_audio_spec.freq = 48000;
    m_audio_spec.format = SDL_AUDIO_F32;
    m_audio_spec.channels = 1;
    m_audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &m_audio_spec);
    m_audio_stream = SDL_CreateAudioStream(&m_audio_spec, &m_audio_spec);

    SDL_BindAudioStream(m_audio_device, m_audio_stream);
    SDL_ResumeAudioDevice(m_audio_device);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForOpenGL(m_window, m_gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &m_waterfall_tex);
    glBindTexture(GL_TEXTURE_2D, m_waterfall_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, FFT_BINS, WATERFALL_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_fft_in = reinterpret_cast<fftwf_complex*>(fftwf_malloc(sizeof(fftwf_complex) * FFT_BINS));
    m_fft_out = reinterpret_cast<fftwf_complex*>(fftwf_malloc(sizeof(fftwf_complex) * FFT_BINS));
    m_fft_plan = fftwf_plan_dft_1d(FFT_BINS, m_fft_in, m_fft_out, FFTW_FORWARD, FFTW_MEASURE);

    for (int i=0; i < FFT_BINS; ++i)
        m_hamm[i] = 0.54f - 0.46f * cosf((2.0f * M_PI * i) / (FFT_BINS - 1));

    m_dec_pre.M = 6;
    makeLowPass((100e3 / (SDR_SAMPLERATE/2.0)) / 2.0, 128, m_dec_pre);

    m_dec_post.M = 4;
    makeLowPass(16000.0 / (SDR_SAMPLERATE / m_dec_pre.M / 2.0), 4*4, m_dec_post);

    memset(m_waterfall_rgb, 0, WATERFALL_HEIGHT*FFT_BINS*3);
    memset(m_spectrogram, 0, FFT_BINS*sizeof(float));

    m_sdr = std::make_unique<sdr::BladeRF>();
    m_rx_running = true;
    m_rx_thread  = std::thread([this]() { sampleConsumerThreadProc(); });
    return true;
}

void core::Application::sampleConsumerThreadProc()
{
    logger::core()->info("Starting RX reciever thread...");

    pthread_t this_thread = pthread_self();
    struct sched_param params;
    params.sched_priority = 50;
    if(pthread_setschedparam(this_thread, SCHED_FIFO, &params) != 0) {
        logger::core()->error("Failed setting thread prio");
    }

    m_sdr->setup();
    m_sdr->startRxStream();

    std::vector<std::complex<float>> iq(SAMPLES_PER_RX);
    std::vector<std::complex<float>> decimated(SAMPLES_PER_RX/6);

    std::vector<float> demodulated;
    std::vector<float> audio;

    while (m_rx_running) {
        m_sdr->receive(iq);

        decimateSig(iq, decimated, m_dec_pre);
        fmDemodulate(decimated, demodulated);
        dcBlock(demodulated);
        deemph(demodulated);

        if (audio.capacity() != demodulated.size())
            audio.resize(demodulated.size());

        decimateSig(demodulated, audio, m_dec_post);

        for(int i=0; i < audio.size(); ++i)
            audio[i] = std::clamp(audio[i], -0.75f, 0.75f);

        SDL_PutAudioStreamData(m_audio_stream, audio.data(), audio.size() * sizeof(float));
        waterfallFeedSamples(iq);
    }
}

void core::Application::dcBlock(std::vector<float>& sig)
{
    const float R = 0.995f;

    static float x_prev = 0.0f;
    static float y_prev = 0.0f;

    for (auto& x : sig) {
        float y = x - x_prev + R * y_prev;

        x_prev = x;
        y_prev = y;

        x = y;
    }
}

void core::Application::deemph(std::vector<float>& sig)
{
    const float tau = DEEMPH_TAU * 1e-6f;
    const float Fs  = SDR_SAMPLERATE / m_dec_pre.M;
    const float dt  = 1.0f / Fs;
    const float alpha = dt / (tau + dt);

    static float y = 0.0f;
    for (auto &x : sig) {
        y = y + alpha * (x - y);
        x = y;
    }
}

void core::Application::waterfallFeedSamples(std::vector<std::complex<float>> const& iq)
{
    static std::complex<float> sample{0.0f, 0.0f};

    for (size_t i = 0; i < FFT_BINS; i++) {
        m_fft_in[i][0] = iq[i].real() * m_hamm[i];
        m_fft_in[i][1] = iq[i].imag() * m_hamm[i];
    }

    fftwf_execute(m_fft_plan);

    for (int y = WATERFALL_HEIGHT - 1; y > 0; y--)
        std::memcpy(m_waterfall_rgb[y], m_waterfall_rgb[y - 1], FFT_BINS * 3);

    for (size_t i = 0; i < FFT_BINS; i++) {
        float re = m_fft_out[i][0];
        float im = m_fft_out[i][1];

        float db = 10.0f * log10(re*re+im*im + 1e-12f);
        float norm = std::clamp(db / 66.0f, 0.0f, 1.0f);
        std::size_t shifted = (i+FFT_BINS/2) % FFT_BINS;

        turboColormap(norm,
                      m_waterfall_rgb[0][shifted][0],
                      m_waterfall_rgb[0][shifted][1],
                      m_waterfall_rgb[0][shifted][2]);

        m_spectrogram[shifted] = norm;
    }
}

void core::Application::makeLowPass(double Fc, unsigned int order, DecimatorCoeffs& decim)
{
    decim.num_taps = order + 1;
    decim.taps.resize(decim.num_taps);

    double alpha = order / 2.0;
    double sum = 0.0;

    for (auto n = 0; n <= order; ++n) {
        double x = n - alpha;
        double h;

        if (std::abs(x) < 1e-12)
            h = 2.0 * Fc;
        else
            h = std::sin(2.0 * M_PI * Fc * x) / (M_PI * x);

        double w = 0.54 - 0.46 * std::cos(2.0 * M_PI * n / order);
        decim.taps[n] = static_cast<float>(h * w);
        sum += decim.taps[n];
    }

    for (auto& tap: decim.taps) {
        tap /= sum;
    }
}

void core::Application::fmDemodulate(std::vector<std::complex<float>>& sig, std::vector<float>& out)
{
    auto const N = sig.size();
    static float phase = 0.0f;
    float new_phase, delta_phase;

    if (out.capacity() != N)
        out.resize(N);

    for (size_t i = 0; i < N; i++) {
        new_phase = std::atan2(sig[i].real(), sig[i].imag());
        if (new_phase > M_PI)
            new_phase -= 2*M_PI;

        delta_phase = new_phase - phase;
        if (delta_phase > M_PI)
            delta_phase -= 2*M_PI;
        if (delta_phase < -M_PI)
            delta_phase += 2*M_PI;

        phase = new_phase;
        out[i] = delta_phase;
    }
}

void core::Application::turboColormap(float x, uint8_t& r, uint8_t& g, uint8_t& b)
{
    const float r_c[] = {0.13572138f, 4.61539260f, -42.66032258f, 132.13108234f, -152.94239396f, 59.28637943f};
    const float g_c[] = {0.09140261f, 2.19418839f, 4.84296658f, -14.18503333f, 4.27729857f, 2.82956604f};
    const float b_c[] = {0.10667330f, 12.64194608f, -60.58204836f, 110.36276771f, -89.90310912f, 27.34824973f};

    float x2 = x * x;
    float x3 = x2 * x;
    float x4 = x3 * x;
    float x5 = x4 * x;

    float rf = r_c[0] + r_c[1]*x + r_c[2]*x2 + r_c[3]*x3 + r_c[4]*x4 + r_c[5]*x5;
    float gf = g_c[0] + g_c[1]*x + g_c[2]*x2 + g_c[3]*x3 + g_c[4]*x4 + g_c[5]*x5;
    float bf = b_c[0] + b_c[1]*x + b_c[2]*x2 + b_c[3]*x3 + b_c[4]*x4 + b_c[5]*x5;

    r = static_cast<uint8_t>(std::clamp(rf,0.0f,1.0f)*255.0f);
    g = static_cast<uint8_t>(std::clamp(gf,0.0f,1.0f)*255.0f);
    b = static_cast<uint8_t>(std::clamp(bf,0.0f,1.0f)*255.0f);
}

void core::Application::spectrogramRender()
{
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    static ImVec2 line_pts[FFT_BINS];

    float width  = WINDOW_WIDTH;
    float height = WINDOW_HEIGHT - WATERFALL_HEIGHT;
    float base_y = WATERFALL_HEIGHT + height;

    for (size_t i = 0; i < FFT_BINS; i++) {
        float x = ((float)i / (FFT_BINS - 1)) * width;
        float y = (1.0f - m_spectrogram[i]) * height;
        line_pts[i] = {x, y};
    }

    draw->AddPolyline(line_pts, FFT_BINS, IM_COL32(255,255,255,255), ImDrawFlags_None, 2.0f);
}

void core::Application::waterfallRender()
{
    glBindTexture(GL_TEXTURE_2D, m_waterfall_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, FFT_BINS, WATERFALL_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, m_waterfall_rgb);
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    ImVec2 pos = {0, WINDOW_HEIGHT - WATERFALL_HEIGHT};
    ImVec2 size = {WINDOW_WIDTH, WATERFALL_HEIGHT};
    draw->AddImage((ImTextureID)(intptr_t)m_waterfall_tex, pos, {pos.x + size.x, pos.y + size.y});
}

bool core::Application::cleanup()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(m_gl_context);
    SDL_DestroyWindow(m_window);
    SDL_Quit();

    m_rx_running = false;

    if (m_rx_thread.joinable())
        m_rx_thread.join();

    fftwf_destroy_plan(m_fft_plan);
    fftwf_free(m_fft_in);
    fftwf_free(m_fft_out);

    return true;
}

void core::Application::loop()
{
    m_running = true;

    while (m_running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);

            if (event.type == SDL_EVENT_QUIT) {
                logger::core()->info("Received SDL_EVENT_QUIT, shutting down...");
                m_running = false;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        if (m_sdr->getIsStarted())
        {
            static uint64_t freq_hz = 86300000;
            float freq_mhz = freq_hz / 1e6f;

            ImGui::InputFloat("Frequency (MHz)", &freq_mhz, 0.1f, 1.0f, "%.1f");
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                freq_mhz = std::round(freq_mhz * 10.0f) / 10.0f;
                freq_hz = (uint64_t)(freq_mhz * 1e6);
                m_sdr->setRxFrequency(freq_hz);
            }
        }

        spectrogramRender();
        waterfallRender();

        ImGui::Render();
        glViewport(0, 0, 1280, 720);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(m_window);
    }
}
