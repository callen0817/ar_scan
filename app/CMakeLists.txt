set(CMAKE_AUTOMOC ON)

add_executable(av_scan
    application.cpp
    main.cpp
)

target_include_directories(av_scan PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(av_scan PRIVATE
    av_gui
    av_platform
    av_core
    Qt5::Core
    Qt5::Gui
    Qt5::Quick
    Qt5::Qml
    ${CMAKE_DL_LIBS}
)
