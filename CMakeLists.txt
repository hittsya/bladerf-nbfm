cmake_minimum_required(VERSION 3.30)
project(fm_bladerf_demod)

set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SDR_SAMPLERATE 1152000 CACHE STRING "SDR sample rate in Hz")
set(FM_DEEMPH_TAU  50      CACHE STRING "50 for EU, 70 for US" )

include(cmake/FetchCPM.cmake)
include(cmake/FetchSpdLog.cmake)
include(cmake/FetchCURL.cmake)
include(cmake/FetchSDL3.cmake)
include(cmake/FetchImGUI.cmake)
include(cmake/FetchFFT.cmake)

target_sources(imgui
  PRIVATE
    cmake/FetchFFT.cmake
)

find_package(OpenGL REQUIRED)
link_libraries(libcurl_static)

include(cmake/FetchLibBladeRF.cmake)


if (USE_TEST_SIGNAL)
    add_compile_definitions(-DUSE_TEST_SIGNAL)
endif(USE_TEST_SIGNAL)

add_compile_definitions(-DSDR_SAMPLERATE=${SDR_SAMPLERATE})
add_compile_definitions(-DDEEMPH_TAU=${FM_DEEMPH_TAU})
add_subdirectory(src)
