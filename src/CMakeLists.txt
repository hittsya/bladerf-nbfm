add_executable(fm_rx
    main.cpp
    Logger.h
    Logger.cpp
    BladeRF.cpp
    BladeRF.h
    Application.cpp
    Application.h
)

target_link_libraries(fm_rx
    bladeRF
    libcurl
    fftw3f
    spdlog::spdlog
    SDL3::SDL3
    OpenGL::GL
    imgui
)

target_compile_options(fm_rx
    PUBLIC
    "$<$<CONFIG:Release>:-O3;-ffast-math;-fopenmp>"
)
