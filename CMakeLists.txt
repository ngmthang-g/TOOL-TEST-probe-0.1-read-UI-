cmake_minimum_required(VERSION 3.20)
project(ThanLongUiInternalProbe VERSION 0.1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(STATUS "Windows controller/bridge targets are disabled on non-Windows hosts; probe_logic_test remains available.")
endif()

foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${config}" config_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/bin")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/bin")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/lib")
endforeach()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(probe_logic_test tests/probe_logic_test.cpp)
target_include_directories(probe_logic_test PRIVATE src)

if(WIN32)
    add_library(ProbeBridge SHARED src/probe_bridge.cpp)
    target_include_directories(ProbeBridge PRIVATE src)
    target_compile_definitions(ProbeBridge PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

    add_executable(ProbeController WIN32 src/probe_controller.cpp)
    target_include_directories(ProbeController PRIVATE src)
    target_compile_definitions(ProbeController PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(ProbeController PRIVATE comctl32 user32 shell32)

    if(MSVC)
        set_property(TARGET ProbeBridge ProbeController PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        target_compile_options(ProbeBridge PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(ProbeController PRIVATE /W4 /permissive- /utf-8)
    endif()
endif()

if(MSVC)
    target_compile_options(probe_logic_test PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(probe_logic_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME probe_logic_test COMMAND probe_logic_test)
