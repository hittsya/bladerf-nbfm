#pragma once

#include <memory>
#include <spdlog/logger.h>

namespace logger {
    void
    init();

    void
    shutdown();

    std::shared_ptr<spdlog::logger>
    core();

    std::shared_ptr<spdlog::logger>
    sdr();

    std::shared_ptr<spdlog::logger>
    dsp();

}