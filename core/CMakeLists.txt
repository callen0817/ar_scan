add_library(av_core STATIC
    schemas/calibration_provenance.cpp
    schemas/device_profile.cpp
    schemas/scanner_config.cpp
    schemas/sensor_instance.cpp
    schemas/capture.cpp
    schemas/scanner_node.cpp
    schemas/session.cpp
    schemas/project.cpp
    schemas/map_metadata.cpp
    logging/logger.cpp
    storage/storage_engine.cpp
)

target_include_directories(av_core PUBLIC
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(av_core PUBLIC
    nlohmann_json
    stdc++fs
)

if(TARGET Eigen3::Eigen)
    target_link_libraries(av_core PUBLIC Eigen3::Eigen)
endif()
