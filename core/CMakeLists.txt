cmake_minimum_required(VERSION 3.10)
project(zanni-node-core VERSION 0.1.0 LANGUAGES C)

option(ZANNI_VERSION_IN_OUTPUT_NAME "Append version to output library filename" OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONC REQUIRED IMPORTED_TARGET json-c)

# Generate version header from template
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/znc_version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/znc_version.h
    @ONLY
)

include(FetchContent)

FetchContent_Declare(
    zanni-protocol
    GIT_REPOSITORY https://github.com/Ggorets0dev/zanni-gate-protocol.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(zanni-protocol)

# Define the static library
add_library(zanni-node-core STATIC
    src/znc.c
    src/znc_log.c
    src/znc_config.c
    src/znc_execute.c
)

target_link_libraries(zanni-node-core PUBLIC 
    zanni-protocol)

# Specify include directories
target_include_directories(zanni-node-core PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(zanni-node-core PRIVATE
    PkgConfig::JSONC
)

# Set library output name if versioned filename option is enabled
if(ZANNI_VERSION_IN_OUTPUT_NAME)
    set_target_properties(zanni-node-core PROPERTIES 
        OUTPUT_NAME "zanni-node-core-${PROJECT_VERSION}"
    )
endif()
