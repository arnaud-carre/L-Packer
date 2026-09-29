cmake_minimum_required(VERSION 3.16)

project(L-Packer VERSION 1.0 LANGUAGES C CXX)

# Makefiles/Ninja select a build type here; Visual Studio/Xcode select it at build time.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build configuration" FORCE)
endif()
if(NOT CMAKE_CONFIGURATION_TYPES)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release)
endif()

# Explicit sources exclude the separate bootstrap_builder application.
# Bootstrap data is supplied by the existing src/bootstrap_data_gen.h.
add_executable(L-Packer
    src/amigaExeLoader.cpp
    src/amigaExePack.cpp
    src/atariExePack.cpp
    src/bootstrap.cpp
    src/byteStream.cpp
    src/dataPack.cpp
    src/jobSystem.cpp
    src/lpacker.cpp
    src/packEngine.cpp
    src/relocList.cpp
    src/external/lz4/lz4.c
    src/external/lz4/lz4hc.c
    src/external/salvador/matchfinder.c
    src/external/salvador/shrink.c
    src/external/salvador/libdivsufsort/divsufsort.c
    src/external/salvador/libdivsufsort/divsufsort_utils.c
    src/external/salvador/libdivsufsort/sssort.c
    src/external/salvador/libdivsufsort/trsort.c
    src/external/zopfli/blocksplitter.c
    src/external/zopfli/cache.c
    src/external/zopfli/deflate.c
    src/external/zopfli/gzip_container.c
    src/external/zopfli/hash.c
    src/external/zopfli/katajainen.c
    src/external/zopfli/lz77.c
    src/external/zopfli/squeeze.c
    src/external/zopfli/tree.c
    src/external/zopfli/util.c
    src/external/zopfli/zlib_container.c
    src/external/zopfli/zopfli_lib.c
)

set_target_properties(L-Packer PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
target_include_directories(L-Packer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)
target_link_libraries(L-Packer PRIVATE Threads::Threads)

if(UNIX)
    # Zopfli uses functions from the system math library.
    target_link_libraries(L-Packer PRIVATE m)
endif()

if(MSVC)
    # Link the runtime statically (/MT for Release, /MTd for Debug).
    set_property(TARGET L-Packer PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(L-Packer PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/utf-8>")
endif()

# Examples (use Debug or Release):
#   cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build/debug --config Debug
#   cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/release --config Release
# Output: L-Packer.exe on Windows; L-Packer on macOS and Linux.
