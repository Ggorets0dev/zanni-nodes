cmake_minimum_required(VERSION 3.10)
project(zanni-node-linux VERSION 0.1.0 LANGUAGES C)

find_package(Threads REQUIRED)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../core ${CMAKE_CURRENT_BINARY_DIR}/core_build)

add_executable(zanni-node-linux
    src/main.c
    src/znl_systemd.c
)

target_include_directories(zanni-node-linux PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(zanni-node-linux PRIVATE
    zanni-node-core
    Threads::Threads
)
