#include "BladeRF.h"
#include "Logger.h"

#include <cstdlib>
#include <complex>
#include <format>
#include <filesystem>
#include <curl/curl.h>
#include <SDL3/SDL.h>

namespace fs = std::filesystem;

#define MHZ(x) ((x)/1e6)

static size_t write_data(void* ptr, size_t size, size_t nmemb, void* stream) {
    size_t written = fwrite(ptr, size, nmemb, static_cast<FILE*>(stream));
    return written;
}

sdr::BladeRF::BladeRF()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

sdr::BladeRF::~BladeRF()
{
    curl_global_cleanup();

    if (m_bladerf) {
        stopRxStream();
        bladerf_close(m_bladerf);
    }
}

bool sdr::BladeRF::setup()
{
    bladerf_devinfo *devices;
    int status;
    std::string fpga_path;
    const char* home;
    struct bladerf_version fpga_version;
    bool load_fpga, fpga_configured, fpga_compat;

    if (auto const count = bladerf_get_device_list(&devices); count <= 0) {
        logger::sdr()->error("No BladeRF devices found");
        goto exit_error;
    }

    downloadBitstreamFiles();
    m_serial = std::string(devices[0].serial);

    status = bladerf_open(&m_bladerf, fmt::format("*:serial={}", m_serial).c_str());
    if (status != 0) {
        logger::sdr()->error("Failed to open BladeRF device: " + m_serial);
        goto exit_error;
    }
    logger::sdr()->info("Successfully opened device: " + m_serial);

    status = bladerf_fpga_version(m_bladerf, &fpga_version);
    fpga_compat = fpga_version.major == 0 && fpga_version.minor == 16 && fpga_version.patch == 0;
    fpga_configured = bladerf_is_fpga_configured(m_bladerf);

    if (status < 0 || !fpga_compat || !fpga_configured) {
        logger::sdr()->info("Uploading FPGA bistream to the device");

        home = std::getenv("HOME");
        if (!home) {
            logger::sdr()->error("Failed to get $HOME directory");
            goto exit_error;
        }

        fpga_path = (fs::path(home) / ".sdr-fm" / "hostedxA4-latest.rbf").string();
        logger::sdr()->info(fmt::format("Loading FPGA bitstream: {}, may take a while", fpga_path));

        status = bladerf_load_fpga(m_bladerf, fpga_path.c_str());
        if (status != 0) {
            logger::sdr()->error("Failed to load FPGA bitstream to the device: " + m_serial);
            goto exit_error;
        }
    }

    status = bladerf_set_gain_mode(m_bladerf, BLADERF_CHANNEL_RX(0), BLADERF_GAIN_FASTATTACK_AGC);
    if (status != 0) {
        logger::sdr()->error("Failed to set BLADERF_GAIN_FASTATTACK_AGC: " + m_serial);
        goto exit_error;
    }

    if (!setRxFrequency(94.3e6)) {
        goto exit_error;
    }

    if (!setRxSamplerate(SDR_SAMPLERATE)) {
        goto exit_error;
    }

    return true;
exit_error:
    bladerf_free_device_list(devices);
    return false;
}

bool sdr::BladeRF::getIsStarted()
{
    return m_rx_running;
}

bool sdr::BladeRF::stopRxStream()
{
    if (!m_rx_running) {
        logger::sdr()->error("stopRxStream() failed: not running");
        return false;
    }

    return true;
}

bool sdr::BladeRF::startRxStream()
{
    if (m_rx_running) {
        logger::sdr()->error("startRxStream() failed: already running");
        return false;
    }

    int status = bladerf_sync_config(m_bladerf, BLADERF_RX_X1, BLADERF_FORMAT_SC16_Q11, num_buffers, buffer_size, num_transfers, timeout_ms);
    if (status != 0) {
        logger::sdr()->error("bladerf_sync_config() failed");
        return false;
    }

    status = bladerf_enable_module(m_bladerf, BLADERF_CHANNEL_RX(0), true);
    if (status != 0) {
        logger::sdr()->error("bladerf_enable_module() failed");
        return false;
    }

    logger::sdr()->info("Starting RX thread");
    m_rx_running = true;
    return true;
}

bool sdr::BladeRF::receive(std::vector<std::complex<float>>& out)
{
    constexpr auto scale = 1.0f / 2047.0f;
    static std::int16_t rx_buffer[SAMPLES_PER_RX*2];

    auto status = bladerf_sync_rx(m_bladerf, rx_buffer, SAMPLES_PER_RX, nullptr, timeout_ms);
    if (status != 0) {
        logger::sdr()->error(fmt::format("RX error: {}", bladerf_strerror(status)));
        return false;
    }

    #pragma omp simd
    for (size_t i = 0; i < SAMPLES_PER_RX; ++i)
        out[i] = std::complex<float>(rx_buffer[2*i+0]*scale, rx_buffer[2*i+1]*scale);

    return true;
}

bool sdr::BladeRF::setRxSamplerate(std::uint32_t samplerate)
{
    uint32_t actual_samplerate;

    logger::sdr()->info(fmt::format("Set samplerate: {} MHz", MHZ(samplerate)));
    auto status = bladerf_set_sample_rate(m_bladerf, BLADERF_CHANNEL_RX(0), samplerate, &actual_samplerate);
    if (status != 0) {
        logger::sdr()->error(std::format("Failed to set samplerate: {}", MHZ(samplerate)));
        return false;
    }

    logger::sdr()->info(fmt::format("Actual samplerate: {} MHz", MHZ(actual_samplerate)));

    uint32_t actual_bandwidth;
    uint32_t bandwidth = samplerate*0.8;

    status = bladerf_set_bandwidth(m_bladerf, BLADERF_CHANNEL_RX(0), bandwidth, &actual_bandwidth);
    if (status != 0) {
        logger::sdr()->error(std::format("Failed to set bandwidth:  {}", MHZ(bandwidth)));
        return false;
    }

    logger::sdr()->info(fmt::format("Actual bandwidth: {} MHz", MHZ(actual_bandwidth)));
    return true;
}

bool sdr::BladeRF::setRxFrequency(std::uint32_t freq)
{
    auto status = bladerf_set_frequency(m_bladerf, BLADERF_CHANNEL_RX(0), freq);
    if (status != 0) {
        logger::sdr()->error(std::format("Failed to set carrier frequency: ", MHZ(freq)));
        return false;
    }

    logger::sdr()->info("Set carrier frequency: {} MHz", MHZ(freq));
    return true;
}

bool sdr::BladeRF::downloadBitstreamFiles()
{
    std::vector<std::pair<std::string, std::string>> bitstreams_locations = {
        {"https://www.nuand.com/fpga/hostedxA4-latest.rbf", "hostedxA4-latest.rbf"},
        {"https://www.nuand.com/fpga/hostedxA5-latest.rbf", "hostedxA5-latest.rbf"},
        {"https://www.nuand.com/fpga/hostedxA9-latest.rbf", "hostedxA9-latest.rbf"},
        {"https://www.nuand.com/fpga/hostedx40-latest.rbf", "hostedx40-latest.rbf"},
        {"https://www.nuand.com/fpga/hostedx115-latest.rbf", "hostedx115-latest.rbf"},
    };

    for (auto const& [url, filename]: bitstreams_locations) {
        if (!downloadFile(url, filename)) {
            return false;
        }
    }

    return true;
}

bool sdr::BladeRF::downloadFile(std::string const& url, std::string const& filename)
{
    const char* home = std::getenv("HOME");
    if (!home) {
        logger::sdr()->error("Failed to get $HOME directory");
        return false;
    }

    fs::path download_dir = fs::path(home) / ".sdr-fm";
    try {
        if (!fs::exists(download_dir)) {
            fs::create_directories(download_dir);
        }
    } catch (const fs::filesystem_error& e) {
        logger::sdr()->error(fmt::format("Failed to create the dir: {}", e.what()));
        return false;
    }

    fs::path destination = download_dir / filename;
    if (fs::exists(destination)) {
        logger::sdr()->info(fmt::format("Bitstream file {} is already downloaded", filename));
        return true;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        logger::sdr()->error("Failed to initialize CURL");
        return false;
    }

    FILE* fp = fopen(destination.c_str(), "wb");
    if (!fp) {
        logger::sdr()->error(fmt::format("Failed to open the file: {}", destination.string()));
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    logger::sdr()->info(fmt::format("Downloading {} to {}...", url, destination.string()));

    CURLcode res = curl_easy_perform(curl);
    fclose(fp);

    bool success = false;
    if (res != CURLE_OK) {
        logger::sdr()->info(fmt::format("Download failed: {}",  curl_easy_strerror(res)));
        fs::remove(destination);
    } else {
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

        if (response_code == 200) {
            logger::sdr()->info(fmt::format("Download completed: {}", destination.string()));
            success = true;
        } else {
            logger::sdr()->error(fmt::format("HTTP error code:  {}", response_code));
            fs::remove(destination);
        }
    }

    curl_easy_cleanup(curl);
    return success;
}
