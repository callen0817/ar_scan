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

