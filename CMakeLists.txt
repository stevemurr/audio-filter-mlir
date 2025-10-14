cmake_minimum_required(VERSION 3.13)
project(audio-filter-mlir C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate compile_commands.json for LSP support (clangd, Zed, etc.)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Add cmake module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Optional: Enable MLIR support
option(ENABLE_MLIR "Enable MLIR optimizations" ON)

if(ENABLE_MLIR)
    find_package(MLIR)
    if(MLIR_FOUND)
        message(STATUS "MLIR support enabled")
        add_definitions(-DUSE_MLIR)
        include_directories(${MLIR_INCLUDE_DIRS})
    else()
        message(WARNING "MLIR not found, building without MLIR support")
        set(ENABLE_MLIR OFF)
    endif()
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

#
# Audio I/O Library
#
set(AUDIO_IO_SOURCES src/audio_io.c)
add_library(audio_io STATIC ${AUDIO_IO_SOURCES})
target_include_directories(audio_io PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(audio_io m)

#
# BiQuad Filter Library
#
set(BIQUAD_SOURCES src/biquad.c)
add_library(biquad STATIC ${BIQUAD_SOURCES})
target_include_directories(biquad PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(biquad m)

#
# High-Pass Filter Library
#
set(HPF_SOURCES src/hpf.c)
add_library(hpf STATIC ${HPF_SOURCES})
target_include_directories(hpf PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(hpf biquad audio_io m)

#
# Low-Pass Filter Library
#
set(LPF_SOURCES src/lpf.c)
add_library(lpf STATIC ${LPF_SOURCES})
target_include_directories(lpf PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(lpf biquad audio_io m)

#
# Parametric EQ Library
#
set(PARAMETRIC_SOURCES src/parametric.c)
add_library(parametric STATIC ${PARAMETRIC_SOURCES})
target_include_directories(parametric PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(parametric biquad audio_io m)

#
# MLIR Context Library (optional, C++ code)
#
if(ENABLE_MLIR)
    set(MLIR_CONTEXT_SOURCES src/mlir_context.cpp)
    add_library(mlir_context STATIC ${MLIR_CONTEXT_SOURCES})
    target_include_directories(mlir_context PUBLIC ${CMAKE_SOURCE_DIR}/include ${MLIR_INCLUDE_DIRS})
    target_link_libraries(mlir_context ${MLIR_LIBRARIES})
    set_target_properties(mlir_context PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )

    # MLIR BiQuad Library (C++ code, JIT compilation)
    set(MLIR_BIQUAD_SOURCES src/mlir_biquad.cpp)
    add_library(mlir_biquad STATIC ${MLIR_BIQUAD_SOURCES})
    target_include_directories(mlir_biquad PUBLIC ${CMAKE_SOURCE_DIR}/include ${MLIR_INCLUDE_DIRS})
    target_link_libraries(mlir_biquad mlir_context biquad ${MLIR_LIBRARIES})
    set_target_properties(mlir_biquad PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
endif()

#
# Main Utility
#
add_executable(audio-util src/audio_util.c)
if(ENABLE_MLIR)
    target_link_libraries(audio-util hpf lpf parametric biquad audio_io mlir_biquad mlir_context ${MLIR_LIBRARIES} m)
else()
    target_link_libraries(audio-util hpf lpf parametric biquad audio_io m)
endif()

#
# Tests
#
# Create test_data directory
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests/test_data)

# Audio I/O tests
add_executable(test_audio_io tests/test_audio_io.c)
target_link_libraries(test_audio_io audio_io)

# BiQuad tests
add_executable(test_biquad tests/test_biquad.c)
target_link_libraries(test_biquad biquad)

# HPF tests
add_executable(test_hpf tests/test_hpf.c)
if(ENABLE_MLIR)
    target_link_libraries(test_hpf hpf biquad audio_io mlir_biquad mlir_context ${MLIR_LIBRARIES} m)
else()
    target_link_libraries(test_hpf hpf biquad audio_io)
endif()

# LPF tests
add_executable(test_lpf tests/test_lpf.c)
if(ENABLE_MLIR)
    target_link_libraries(test_lpf lpf biquad audio_io mlir_biquad mlir_context ${MLIR_LIBRARIES} m)
else()
    target_link_libraries(test_lpf lpf biquad audio_io)
endif()

# Parametric EQ tests
add_executable(test_parametric tests/test_parametric.c)
if(ENABLE_MLIR)
    target_link_libraries(test_parametric parametric biquad audio_io mlir_biquad mlir_context ${MLIR_LIBRARIES} m)
else()
    target_link_libraries(test_parametric parametric biquad audio_io)
endif()

# MLIR basic tests (optional)
if(ENABLE_MLIR)
    add_executable(test_mlir_basic tests/test_mlir_basic.c)
    target_link_libraries(test_mlir_basic mlir_context ${MLIR_LIBRARIES})

    # MLIR BiQuad tests
    add_executable(test_mlir_biquad tests/test_mlir_biquad.c)
    target_link_libraries(test_mlir_biquad mlir_biquad mlir_context biquad ${MLIR_LIBRARIES} m)

    # Simple MLIR test for debugging
    add_executable(test_mlir_simple tests/test_mlir_simple.c)
    target_link_libraries(test_mlir_simple mlir_biquad mlir_context biquad ${MLIR_LIBRARIES} m)

    # MLIR BiQuad benchmark
    add_executable(bench_mlir_biquad tests/bench_mlir_biquad.c)
    target_link_libraries(bench_mlir_biquad mlir_biquad mlir_context biquad ${MLIR_LIBRARIES} m)
endif()

# Enable testing
enable_testing()
add_test(NAME audio_io_tests COMMAND test_audio_io WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME biquad_tests COMMAND test_biquad WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME hpf_tests COMMAND test_hpf WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME lpf_tests COMMAND test_lpf WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME parametric_tests COMMAND test_parametric WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

if(ENABLE_MLIR)
    add_test(NAME mlir_basic_tests COMMAND test_mlir_basic WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    add_test(NAME mlir_biquad_tests COMMAND test_mlir_biquad WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif()
