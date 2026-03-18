add_executable(fm_rx
    main.cpp
    Logger.h
    Logger.cpp
    BladeRF.cpp
    BladeRF.h
    Application.cpp
    Application.h
)

set_target_properties(fm_rx PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

target_link_libraries(fm_rx
    bladeRF
    libcurl_static
    fftw3f
    spdlog::spdlog
    SDL3::SDL3
    OpenGL::GL
    imgui
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast -ffast-math -fopenmp")
set(CMAKE_C_FLAGS   "${CMAKE_CXX_FLAGS} -Ofast -ffast-math -fopenmp")

