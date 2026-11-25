# Codec-independent native core: Bluetooth stack adapter, codec plugin registry, Dolby stage.
set(PORT_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
get_filename_component(THIRD_PARTY "${PORT_CORE_DIR}/../../../../third_party" ABSOLUTE)

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

add_library(port_core STATIC
    stack.cpp
    jni_core.cpp
    codec_registry.cpp
    factory.cpp
    registration.cpp
    vendor_dispatch.cpp
    elf_symbols.cpp
    dolby_hal_stage.cpp)
target_include_directories(port_core PUBLIC ${PORT_CORE_DIR})
target_compile_features(port_core PUBLIC cxx_std_20)
target_compile_options(port_core PRIVATE -Wall -Wextra -Werror -fno-exceptions -fno-rtti
    -fvisibility=hidden -fvisibility-inlines-hidden)
set_target_properties(port_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(port_core PUBLIC effect_aidl_ndk xz_embedded binder_ndk log dl)
