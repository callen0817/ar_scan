set(CMAKE_AUTOMOC ON)

add_library(av_gui STATIC
    qml_bridge.cpp
)

target_include_directories(av_gui PUBLIC
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(av_gui PUBLIC
    av_core
    av_platform
    Qt5::Core
    Qt5::Gui
    Qt5::Quick
    Qt5::Qml
)
