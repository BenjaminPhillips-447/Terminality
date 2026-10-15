cmake_minimum_required(VERSION 3.16)

project(base_c_template LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

add_executable(base_c_template
    src/renderer.c
    src/platform.c
    src/units.c
)

target_include_directories(base_c_template PRIVATE src)

if(NOT WIN32)
    target_link_libraries(base_c_template PRIVATE m)
endif()

if(MSVC)
    target_compile_options(base_c_template PRIVATE /W4)
else()
    target_compile_options(base_c_template PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(units_clipping_test
        tests/units_clipping_test.c
        src/units.c
    )
    target_include_directories(units_clipping_test PRIVATE src)
    if(NOT WIN32)
        target_link_libraries(units_clipping_test PRIVATE m)
    endif()
    add_test(NAME units_clipping_test COMMAND units_clipping_test)
endif()
