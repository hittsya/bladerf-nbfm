CPMAddPackage(
        NAME spdlog
        GITHUB_REPOSITORY gabime/spdlog
        VERSION 1.14.1
        OPTIONS
        "SPDLOG_FMT_EXTERNAL OFF"
        "SPDLOG_BUILD_EXAMPLES OFF"
        "SPDLOG_BUILD_TESTS OFF"
)