#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <thread>
#include <vector>
#include <complex>
#include <atomic>
#include <fftw3.h>
#include "BladeRF.h"

#define WINDOW_WIDTH 1980
#define WINDOW_HEIGHT 1080

#define FFT_BINS 2048
#define WATERFALL_HEIGHT 720

namespace core {

struct DecimatorCoeffs
{
    unsigned int M = 6;
    unsigned int num_taps;
    std::vector<float> taps;
};

class Application {
public:
    bool setup();
    bool cleanup();
    void loop();
private:
    void sampleConsumerThreadProc();
    void waterfallFeedSamples(std::vector<std::complex<float>> const& iq);
    void waterfallRender();
    void spectrogramRender();
    void turboColormap(float x, uint8_t& r, uint8_t& g, uint8_t& b);
    void fmDemodulate(std::vector<std::complex<float>>& sig, std::vector<float>& out);
    void dcBlock(std::vector<float>& sig);
    void deemph(std::vector<float>& sig);
    void makeLowPass(double Fc, int unsigned order, DecimatorCoeffs& decim);
private:
    std::unique_ptr<sdr::BladeRF> m_sdr;

    bool m_running;
    std::thread m_rx_thread;
    std::atomic<bool> m_rx_running{false};

    fftwf_complex* m_fft_in;
    fftwf_complex* m_fft_out;
    fftwf_plan m_fft_plan;

    DecimatorCoeffs m_dec_pre;
    DecimatorCoeffs m_dec_post;

    std::uint8_t m_waterfall_rgb[WATERFALL_HEIGHT][FFT_BINS][3];
    float m_spectrogram[FFT_BINS];
    float m_hamm[FFT_BINS];
    GLuint m_waterfall_tex;

    SDL_Window* m_window;
    SDL_GLContext m_gl_context;
    SDL_AudioDeviceID m_audio_device;
    SDL_AudioSpec m_audio_spec;
    SDL_AudioStream* m_audio_stream;
};

}
