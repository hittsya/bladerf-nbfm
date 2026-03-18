#pragma once

#include <libbladeRF.h>
#include <cstdint>
#include <vector>
#include <string>
#include <complex>

#define SAMPLES_PER_RX 8192*2

namespace sdr {

class BladeRF {
public:
    BladeRF();
    ~BladeRF();
    bool setup();
    bool setRxSamplerate(std::uint32_t samplerate);
    bool setRxFrequency(std::uint32_t freq);
    bool startRxStream();
    bool stopRxStream();
    bool receive(std::vector<std::complex<float>>& out);
    bool getIsStarted();
private:
    static bool downloadBitstreamFiles();
    static bool downloadFile(std::string const& url, std::string const& filename);
private:
    static constexpr unsigned int num_buffers   = 32;
    static constexpr unsigned int buffer_size   = 8192*2;
    static constexpr unsigned int num_transfers = 8;
    static constexpr unsigned int timeout_ms    = 5000;

    std::string m_serial;
    bladerf* m_bladerf;
    bool m_rx_running = false;
};

}
