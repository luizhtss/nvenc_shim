cmake_minimum_required(VERSION 3.15)
project(nvenc_shim C)

set(CMAKE_C_STANDARD 11)

add_library(nvEncodeAPI64 SHARED
    src/shim.c
    nvEncodeAPI64.def
)

target_include_directories(nvEncodeAPI64 PRIVATE src)

option(SHIM_DEBUG "Enable debug logging to nvenc_shim.log" OFF)
if(SHIM_DEBUG)
    target_compile_definitions(nvEncodeAPI64 PRIVATE SHIM_DEBUG)
endif()

if(MSVC)
    target_compile_options(nvEncodeAPI64 PRIVATE /W4 /WX- /wd4100 /wd4204)
    target_link_options(nvEncodeAPI64 PRIVATE /DEF:${CMAKE_CURRENT_SOURCE_DIR}/nvEncodeAPI64.def)
else()
    target_compile_options(nvEncodeAPI64 PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
