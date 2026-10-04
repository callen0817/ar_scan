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
    schemas/model_definition.cpp
    ecosystem/vendor_ecosystem.cpp
    ecosystem/compatibility_resolver.cpp
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
    drivers/uvc/generic_uvc_sensor_adapter.cpp
    reconstruction/lingbot/lingbot_ipc_client.cpp
    reconstruction/lingbot/lingbot_slam_backend.cpp
    reconstruction/abot/abot_ipc_client.cpp
    reconstruction/abot/abot_slam_backend.cpp
    profiling/capability_matcher.cpp
    profiling/device_profiler.cpp
    multi_node/node_identity_manager.cpp
    multi_node/discovery_service.cpp
    multi_node/coordination_service.cpp
    multi_node/multi_node_session_orchestrator.cpp
    schemas/registration_result.cpp
    registration/registration_engine.cpp
)

target_include_directories(av_core PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${PCL_INCLUDE_DIRS}
)

target_link_libraries(av_core PUBLIC
    nlohmann_json
    stdc++fs
    ${PCL_LIBRARIES}
)

if(TARGET Eigen3::Eigen)
    target_link_libraries(av_core PUBLIC Eigen3::Eigen)
endif()

if(OpenMP_CXX_FOUND)
    target_link_libraries(av_core PUBLIC OpenMP::OpenMP_CXX)
endif()
