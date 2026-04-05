cmake_minimum_required(VERSION 3.24)
project(turing-machine
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "Turing machine interpreter using boost::sml"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(sml REQUIRED)

add_executable(turing src/main.cpp)

target_include_directories(turing PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(turing PRIVATE
    sml::sml
)
