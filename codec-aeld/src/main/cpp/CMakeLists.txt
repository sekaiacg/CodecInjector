# Apple AAC-ELD codec plugin (AirPods).
add_library(codec_aeld STATIC
    aeld_codec.cpp
    aeld_codec_info.cpp
    aeld_codec_state.cpp
    aeld_fdk.cpp
    aeld_packet.cpp
    aeld_source.cpp)
target_include_directories(codec_aeld PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(codec_aeld PRIVATE -Wall -Wextra -Werror -fno-exceptions -fno-rtti
    -fvisibility=hidden -fvisibility-inlines-hidden)
set_target_properties(codec_aeld PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(codec_aeld PUBLIC port_core)
