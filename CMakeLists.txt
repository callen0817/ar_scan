cmake_minimum_required(VERSION 3.22)
project(av_scan VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Enable compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Locate Qt5
find_package(Qt5 REQUIRED COMPONENTS Core Gui Quick Qml)

# Locate nlohmann_json
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    # Check standard system include path
    find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp PATHS /usr/include /usr/local/include)
    if(NLOHMANN_JSON_INCLUDE_DIR)
        message(STATUS "Found nlohmann_json headers in ${NLOHMANN_JSON_INCLUDE_DIR}")
        add_library(nlohmann_json INTERFACE)
        target_include_directories(nlohmann_json INTERFACE ${NLOHMANN_JSON_INCLUDE_DIR})
    else()
        message(FATAL_ERROR "nlohmann_json not found!")
    endif()
endif()

# Locate Eigen3
find_package(Eigen3 QUIET)
if(NOT Eigen3_FOUND)
    find_path(EIGEN3_INCLUDE_DIR Eigen/Dense PATHS /usr/include/eigen3 /usr/local/include/eigen3)
    if(EIGEN3_INCLUDE_DIR)
        message(STATUS "Found Eigen3 headers in ${EIGEN3_INCLUDE_DIR}")
        add_library(Eigen3::Eigen INTERFACE IMPORTED)
        set_target_properties(Eigen3::Eigen PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${EIGEN3_INCLUDE_DIR}"
        )
    endif()
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_compile_definitions(AV_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib")
set(CMAKE_INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib")

# Enable testing before subdirectories
enable_testing()

# Subdirectories
add_subdirectory(core)
add_subdirectory(platform)
add_subdirectory(gui)
add_subdirectory(app)
add_subdirectory(tests)
