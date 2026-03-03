CPMAddPackage(
        NAME libbladeRF
        GITHUB_REPOSITORY Nuand/bladeRF
        GIT_TAG "libbladeRF_v2.6.0"
        OPTIONS
               "BUILD_LIBBLADERF_SHARED ON"
               "BUILD_LIBBLADERF_STATIC OFF"
               "BUILD_TOOLS OFF"
               "BUILD_UTILITIES OFF"
               "BUILD_EXAMPLES OFF"
               "BUILD_TESTING OFF"
)
