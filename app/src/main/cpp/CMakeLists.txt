cmake_minimum_required(VERSION 3.22.1)
project(sscport LANGUAGES C CXX)
get_filename_component(THIRD_PARTY "${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party" ABSOLUTE)

# MiniDebugInfo (.gnu_debugdata) decompression for runtime symbol resolution.
set(XZ_DIR "${THIRD_PARTY}/xz-embedded")
add_library(xz_embedded STATIC
    ${XZ_DIR}/linux/lib/xz/xz_crc32.c
    ${XZ_DIR}/linux/lib/xz/xz_crc64.c
    ${XZ_DIR}/linux/lib/xz/xz_dec_stream.c
    ${XZ_DIR}/linux/lib/xz/xz_dec_lzma2.c)
target_include_directories(xz_embedded PUBLIC ${XZ_DIR}/linux/include/linux ${XZ_DIR}/userspace)
target_compile_definitions(xz_embedded PUBLIC XZ_USE_CRC64 XZ_DEC_ANY_CHECK)
set_target_properties(xz_embedded PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)

# NDK client for the AIDL audio effect HAL (Dolby Spatializer).
set(AIDL_DIR "${THIRD_PARTY}/aosp-effect-aidl-ndk")
file(GLOB_RECURSE AIDL_SOURCES CONFIGURE_DEPENDS ${AIDL_DIR}/src/*.cpp)
add_library(effect_aidl_ndk STATIC ${AIDL_SOURCES})
target_include_directories(effect_aidl_ndk SYSTEM PUBLIC ${AIDL_DIR}/include ${AIDL_DIR}/binder_ndk_cpp)
target_compile_features(effect_aidl_ndk PUBLIC cxx_std_20)
set_target_properties(effect_aidl_ndk PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden)

add_library(sscport SHARED
    module.cpp
    elf_symbols.cpp
    dolby_hal_stage.cpp
    ssc_negotiation.cpp
    ssc_codec_state.cpp
    ssc_codec_table.cpp
    ssc_factory.cpp
    ssc_registration.cpp
    ssc_frame.cpp
    ssc_bitrate.cpp
    ssc_packet.cpp
    ssc_interface.cpp
    ssc_vendor_dispatch.cpp)
target_compile_features(sscport PRIVATE cxx_std_20)
target_compile_options(sscport PRIVATE -Wall -Wextra -Werror -fno-exceptions -fno-rtti
    -fvisibility=hidden -fvisibility-inlines-hidden)
target_link_options(sscport PRIVATE -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exports.map)
target_link_libraries(sscport PRIVATE effect_aidl_ndk xz_embedded binder_ndk log dl)
