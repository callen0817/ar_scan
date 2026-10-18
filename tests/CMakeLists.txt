set(CMAKE_AUTOMOC ON)

# Unit Tests
add_executable(test_schemas unit/test_schemas.cpp)
target_link_libraries(test_schemas PRIVATE av_core)
add_test(NAME test_schemas COMMAND test_schemas)

add_executable(test_logging unit/test_logging.cpp)
target_link_libraries(test_logging PRIVATE av_core)
add_test(NAME test_logging COMMAND test_logging)

add_executable(test_storage unit/test_storage.cpp)
target_link_libraries(test_storage PRIVATE av_core)
add_test(NAME test_storage COMMAND test_storage)

add_executable(test_gui_bridge unit/test_gui_bridge.cpp)
target_link_libraries(test_gui_bridge PRIVATE
    av_gui
    av_platform
    av_core
    Qt5::Core
)
add_test(NAME test_gui_bridge COMMAND test_gui_bridge)

add_executable(test_cold_reopen unit/test_cold_reopen.cpp)
target_link_libraries(test_cold_reopen PRIVATE
    av_gui
    av_platform
    av_core
    Qt5::Core
)
add_test(NAME test_cold_reopen COMMAND test_cold_reopen)

add_executable(test_runtime_device_isolation unit/test_runtime_device_isolation.cpp)
target_link_libraries(test_runtime_device_isolation PRIVATE
    av_gui
    av_platform
    av_core
    Qt5::Core
)
add_test(NAME test_runtime_device_isolation COMMAND test_runtime_device_isolation)

add_executable(test_sensor_ecosystem unit/test_sensor_ecosystem.cpp)
target_link_libraries(test_sensor_ecosystem PRIVATE
    av_core
    av_platform
    Qt5::Core
)
add_test(NAME test_sensor_ecosystem COMMAND test_sensor_ecosystem)

# Acceptance Tests
add_executable(test_m1_acceptance acceptance/test_m1_acceptance.cpp)
target_link_libraries(test_m1_acceptance PRIVATE
    av_gui
    av_platform
    av_core
    Qt5::Core
    Qt5::Gui
    Qt5::Quick
    Qt5::Qml
    ${CMAKE_DL_LIBS}
)
add_test(NAME test_m1_acceptance COMMAND test_m1_acceptance)

add_executable(test_m2_acceptance acceptance/test_m2_acceptance.cpp)
target_link_libraries(test_m2_acceptance PRIVATE
    av_gui
    av_platform
    av_core
    Qt5::Core
    Qt5::Gui
    Qt5::Quick
    Qt5::Qml
    ${CMAKE_DL_LIBS}
)
add_test(NAME test_m2_acceptance COMMAND test_m2_acceptance)

add_executable(test_airy_adapter unit/test_airy_adapter.cpp)
target_link_libraries(test_airy_adapter PRIVATE av_core)
add_test(NAME test_airy_adapter COMMAND test_airy_adapter)

add_executable(test_m3_acceptance acceptance/test_m3_acceptance.cpp)
target_link_libraries(test_m3_acceptance PRIVATE
    av_gui
    av_platform
    av_core
    Qt5::Core
    Qt5::Gui
    Qt5::Quick
    Qt5::Qml
    ${CMAKE_DL_LIBS}
)
add_test(NAME test_m3_acceptance COMMAND test_m3_acceptance)

add_executable(test_airy_hardware_live acceptance/test_airy_hardware_live.cpp)
target_link_libraries(test_airy_hardware_live PRIVATE
    av_gui
    av_platform
    av_core
    Qt5::Core
    Qt5::Gui
    Qt5::Quick
    Qt5::Qml
    ${CMAKE_DL_LIBS}
)

add_executable(test_gemini_adapter unit/test_gemini_adapter.cpp)
target_link_libraries(test_gemini_adapter PRIVATE av_core)
add_test(NAME test_gemini_adapter COMMAND test_gemini_adapter)

add_executable(test_m4_acceptance acceptance/test_m4_acceptance.cpp)
target_link_libraries(test_m4_acceptance PRIVATE
    av_gui
    av_platform
    av_core
    Qt5::Core
    Qt5::Gui
    Qt5::Quick
    Qt5::Qml
    ${CMAKE_DL_LIBS}
)
add_test(NAME test_m4_acceptance COMMAND test_m4_acceptance)

add_executable(test_gemini_hardware_live acceptance/test_gemini_hardware_live.cpp)
target_link_libraries(test_gemini_hardware_live PRIVATE
    av_gui
    av_platform
    av_core
    Qt5::Core
    Qt5::Gui
    Qt5::Quick
    Qt5::Qml
    ${CMAKE_DL_LIBS}
)

add_executable(test_viture_adapter unit/test_viture_adapter.cpp)
target_link_libraries(test_viture_adapter PRIVATE av_core)
add_test(NAME test_viture_adapter COMMAND test_viture_adapter)

add_executable(test_m5_acceptance acceptance/test_m5_acceptance.cpp)
target_link_libraries(test_m5_acceptance PRIVATE
    av_gui
    av_platform
    av_core
    Qt5::Core
    Qt5::Gui
    Qt5::Quick
    Qt5::Qml
    ${CMAKE_DL_LIBS}
)
add_test(NAME test_m5_acceptance COMMAND test_m5_acceptance)

add_executable(test_viture_hardware_live acceptance/test_viture_hardware_live.cpp)
target_link_libraries(test_viture_hardware_live PRIVATE
    av_gui
    av_platform
    av_core
    Qt5::Core
    Qt5::Gui
    Qt5::Quick
    Qt5::Qml
    ${CMAKE_DL_LIBS}
)

add_executable(test_m5_phase5_qualification acceptance/test_m5_phase5_qualification.cpp)
target_link_libraries(test_m5_phase5_qualification PRIVATE
    av_gui
    av_platform
    av_core
    Qt5::Core
    Qt5::Gui
    Qt5::Quick
    Qt5::Qml
    ${CMAKE_DL_LIBS}
)

add_executable(test_m5_5_acceptance acceptance/test_m5_5_acceptance.cpp)
target_link_libraries(test_m5_5_acceptance PRIVATE
    av_gui
    av_platform
    av_core
    Qt5::Core
    Qt5::Gui
    Qt5::Quick
    Qt5::Qml
    ${CMAKE_DL_LIBS}
)
add_test(NAME test_m5_5_acceptance COMMAND test_m5_5_acceptance)

add_executable(test_m6_acceptance acceptance/test_m6_acceptance.cpp)
target_link_libraries(test_m6_acceptance PRIVATE
    av_gui
    av_platform
    av_core
    Qt5::Core
    Qt5::Gui
    Qt5::Quick
    Qt5::Qml
    ${CMAKE_DL_LIBS}
)
add_test(NAME test_m6_acceptance COMMAND test_m6_acceptance)

add_executable(test_m13_acceptance acceptance/test_m13_acceptance.cpp)
target_link_libraries(test_m13_acceptance PRIVATE
    av_gui
    av_platform
    av_core
    Qt5::Core
    Qt5::Gui
    Qt5::Quick
    Qt5::Qml
    ${CMAKE_DL_LIBS}
)
add_test(NAME test_m13_acceptance COMMAND test_m13_acceptance)

add_executable(test_m14_acceptance acceptance/test_m14_acceptance.cpp)
target_link_libraries(test_m14_acceptance PRIVATE
    av_core
    Qt5::Core
)
add_test(NAME test_m14_acceptance COMMAND test_m14_acceptance)

add_executable(test_m10_acceptance acceptance/test_m10_acceptance.cpp)
target_link_libraries(test_m10_acceptance PRIVATE
    av_core
    Qt5::Core
)
add_test(NAME test_m10_acceptance COMMAND test_m10_acceptance)

add_executable(test_m9_acceptance acceptance/test_m9_acceptance.cpp)
target_link_libraries(test_m9_acceptance PRIVATE
    av_gui
    av_platform
    av_core
    Qt5::Core
    Qt5::Gui
    Qt5::Quick
    Qt5::Qml
)
add_test(NAME test_m9_acceptance COMMAND test_m9_acceptance)

add_executable(test_m12_acceptance acceptance/test_m12_acceptance.cpp)
target_link_libraries(test_m12_acceptance PRIVATE
    av_gui
    av_platform
    av_core
    Qt5::Core
    Qt5::Gui
    Qt5::Quick
    Qt5::Qml
)
add_test(NAME test_m12_acceptance COMMAND test_m12_acceptance)

add_executable(test_m7_m8_acceptance acceptance/test_m7_m8_acceptance.cpp)
target_link_libraries(test_m7_m8_acceptance PRIVATE
    av_gui
    av_platform
    av_core
    Qt5::Core
    Qt5::Gui
    Qt5::Quick
    Qt5::Qml
)
add_test(NAME test_m7_m8_acceptance COMMAND test_m7_m8_acceptance)



