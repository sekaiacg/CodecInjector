# Samsung Seamless Codec (SSC) codec plugin.
add_library(codec_ssc STATIC
    ssc_codec.cpp
    jni_ssc.cpp
    ssc_negotiation.cpp
    ssc_codec_state.cpp
    ssc_codec_table.cpp
    ssc_frame.cpp
    ssc_bitrate.cpp
    ssc_packet.cpp
    ssc_interface.cpp)
target_include_directories(codec_ssc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(codec_ssc PRIVATE -Wall -Wextra -Werror -fno-exceptions -fno-rtti
    -fvisibility=hidden -fvisibility-inlines-hidden)
set_target_properties(codec_ssc PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(codec_ssc PUBLIC port_core)
