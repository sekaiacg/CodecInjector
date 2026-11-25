cmake_minimum_required(VERSION 3.24)
project(galaxybudsport LANGUAGES C CXX)

get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
add_subdirectory(${ROOT}/core/src/main/cpp core)
add_subdirectory(${ROOT}/codec-ssc/src/main/cpp codec-ssc)
add_subdirectory(${ROOT}/codec-aeld/src/main/cpp codec-aeld)

# One LSPosed native module with all codecs: they share the same Bluetooth stack hooks.
add_library(galaxybudsport SHARED codecs.cpp)
target_compile_features(galaxybudsport PRIVATE cxx_std_20)
target_compile_options(galaxybudsport PRIVATE -Wall -Wextra -Werror -fno-exceptions -fno-rtti
    -fvisibility=hidden -fvisibility-inlines-hidden)
target_link_options(galaxybudsport PRIVATE -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exports.map)
# The JNI entry points live in the static libraries: link them whole.
target_link_libraries(galaxybudsport PRIVATE
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,port_core>"
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,codec_ssc>"
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,codec_aeld>")
