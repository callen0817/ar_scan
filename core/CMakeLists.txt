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
    drivers/airy/airy_ipc_client.cpp
    drivers/airy/airy_sensor_adapter.cpp
    slam/airy/airy_lio_backend.cpp
    drivers/gemini/gemini_ipc_client.cpp
    drivers/gemini/gemini_sensor_adapter.cpp
    slam/gemini/gemini_rgbd_backend.cpp
    drivers/viture/viture_ipc_client.cpp
    drivers/viture/viture_sensor_adapter.cpp
    slam/viture/viture_vio_backend.cpp
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
