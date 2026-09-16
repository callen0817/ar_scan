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

# Acceptance Test
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
