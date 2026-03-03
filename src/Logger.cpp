#include "Logger.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace logger {

    namespace {
        std::shared_ptr<spdlog::logger> core_logger;
        std::shared_ptr<spdlog::logger> sdr_logger;
        std::shared_ptr<spdlog::logger> dsp_logger;
    }

    void
    init()
    {
        if (spdlog::get("CORE")) {
            return;
        }

        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
        std::vector<spdlog::sink_ptr> sinks{ console_sink };

        core_logger =
        std::make_shared<spdlog::logger>("CORE", sinks.begin(), sinks.end());

        dsp_logger =
        std::make_shared<spdlog::logger>("DSP", sinks.begin(), sinks.end());

        sdr_logger =
        std::make_shared<spdlog::logger>("SDR", sinks.begin(), sinks.end());

        spdlog::register_logger(core_logger);
        spdlog::register_logger(dsp_logger);
        spdlog::register_logger(sdr_logger);

        spdlog::set_level(spdlog::level::trace);
    }

    void
    shutdown()
    {
        spdlog::shutdown();
    }

    std::shared_ptr<spdlog::logger>
    core()
    {
        return core_logger;
    }

    std::shared_ptr<spdlog::logger>
    dsp()
    {
        return dsp_logger;
    }

    std::shared_ptr<spdlog::logger>
    sdr()
    {
        return sdr_logger;
    }
}
