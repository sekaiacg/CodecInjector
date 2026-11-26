# Samsung Seamless Codec (SSC) codec plugin.

# Open SSC encoder: libopus CELT (third_party/opus) built with the SSC format changes.
get_filename_component(OPUS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/opus" ABSOLUTE)
file(GLOB OPUS_CELT_SOURCES "${OPUS_DIR}/celt/*.c")
add_library(ssc_open STATIC ${OPUS_CELT_SOURCES}
    open/ssc_format.c
    open/ssc_open_encoder.c)
target_include_directories(ssc_open PRIVATE "${OPUS_DIR}/include" "${OPUS_DIR}/celt")
target_include_directories(ssc_open PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/open)
target_compile_definitions(ssc_open PRIVATE OPUS_BUILD CUSTOM_MODES VAR_ARRAYS HAVE_LRINTF SSC_FORMAT)
target_compile_options(ssc_open PRIVATE -O2 -w -fvisibility=hidden)
set_target_properties(ssc_open PROPERTIES POSITION_INDEPENDENT_CODE ON)
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
target_link_libraries(codec_ssc PUBLIC port_core PRIVATE ssc_open)
