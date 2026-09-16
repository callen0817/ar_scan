add_library(av_platform STATIC
    linux/linux_platform.cpp
)

target_include_directories(av_platform PUBLIC
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(av_platform PUBLIC
    av_core
)
