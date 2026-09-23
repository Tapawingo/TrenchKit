cmake_minimum_required(VERSION 3.24)

include(cmake/version.cmake)

project(TrenchKit
  VERSION ${TRENCHKIT_VERSION_MAJOR}.${TRENCHKIT_VERSION_MINOR}.${TRENCHKIT_VERSION_PATCH}
  LANGUAGES CXX
)

if(TRENCHKIT_VERSION_PRERELEASE)
    set(TRENCHKIT_FULL_VERSION "${PROJECT_VERSION}-${TRENCHKIT_VERSION_PRERELEASE}")
else()
    set(TRENCHKIT_FULL_VERSION "${PROJECT_VERSION}")
endif()

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Sanitizer build types
set(CMAKE_CXX_FLAGS_ASAN
    "-g -O1 -fsanitize=address -fno-omit-frame-pointer"
    CACHE STRING "Flags for ASan build" FORCE
)
set(CMAKE_CXX_FLAGS_UBSAN
    "-g -O1 -fsanitize=undefined -fno-omit-frame-pointer"
    CACHE STRING "Flags for UBSan build" FORCE
)
set(CMAKE_CXX_FLAGS_TSAN
    "-g -O1 -fsanitize=thread -fno-omit-frame-pointer"
    CACHE STRING "Flags for TSan build" FORCE
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# WebEngineWidgets (Chromium) powers the in-app browser; MinGW kits do not ship it, so build with MSVC or gcc.
# WebChannel/Positioning are WebEngineWidgets' own dependencies; listed explicitly for a clear error
# if a Qt kit is missing them, rather than a cryptic failure deep inside WebEngineWidgets' own config.
find_package(Qt6 6.5 REQUIRED COMPONENTS
  Widgets Network Concurrent Test WebSockets WebEngineWidgets WebChannel Positioning LinguistTools)

# Fetch zip library for archive extraction (MIT, zero dependencies)
include(FetchContent)
FetchContent_Declare(
  zip
  GIT_REPOSITORY https://github.com/kuba--/zip.git
  GIT_TAG v0.3.2
)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(ZIP_STATIC ON)
FetchContent_MakeAvailable(zip)

set_target_properties(zip PROPERTIES
  AUTOMOC OFF
  AUTOUIC OFF
  AUTORCC OFF
)

target_compile_options(zip PRIVATE
  $<$<AND:$<COMPILE_LANGUAGE:C>,$<NOT:$<C_COMPILER_ID:MSVC>>>:-Wno-error>
)

# Fetch xz (liblzma, public domain): libarchive needs it to decode 7z (LZMA/LZMA2).
# libarchive only finds installed libraries, so it is built in-tree and handed over
# by pre-seeding the cache variables CMake's FindLibLZMA reads.
FetchContent_Declare(
  xz
  GIT_REPOSITORY https://github.com/tukaani-project/xz.git
  GIT_TAG v5.8.1
)

set(XZ_TOOL_XZ OFF)
set(XZ_TOOL_XZDEC OFF)
set(XZ_TOOL_LZMADEC OFF)
set(XZ_TOOL_LZMAINFO OFF)
set(XZ_TOOL_SYMLINKS OFF)
set(XZ_TOOL_SCRIPTS OFF)
set(XZ_NLS OFF)
set(XZ_DOXYGEN OFF)
set(BUILD_TESTING OFF)
set(BUILD_SHARED_LIBS OFF)
FetchContent_MakeAvailable(xz)
unset(BUILD_SHARED_LIBS)
unset(BUILD_TESTING)

set_target_properties(liblzma PROPERTIES
  AUTOMOC OFF
  AUTOUIC OFF
  AUTORCC OFF
)

set(LIBLZMA_INCLUDE_DIR "${xz_SOURCE_DIR}/src/liblzma/api" CACHE PATH "" FORCE)
set(LIBLZMA_LIBRARY liblzma CACHE STRING "" FORCE)
set(LIBLZMA_HAS_AUTO_DECODER ON CACHE INTERNAL "")
set(LIBLZMA_HAS_EASY_ENCODER ON CACHE INTERNAL "")
set(LIBLZMA_HAS_LZMA_PRESET ON CACHE INTERNAL "")
set(HAVE_LZMA_STREAM_ENCODER_MT 0 CACHE INTERNAL "")

# zlib (gzip, zip deflate) and bzip2 are compiled straight from source, without their own build
# systems, and handed to libarchive the same way. Without them .tar.gz/.tar.bz2 cannot be read.
enable_language(C)
FetchContent_Declare(
  zlib
  GIT_REPOSITORY https://github.com/madler/zlib.git
  GIT_TAG v1.3.1
  SOURCE_SUBDIR trenchkit-sources-only
)
FetchContent_Declare(
  bzip2
  URL https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz
  URL_HASH SHA256=ab5a03176ee106d3f0fa90e381da478ddae405918153cca248e682cd0c4a2269
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR trenchkit-sources-only
)
FetchContent_MakeAvailable(zlib bzip2)

add_library(tk_zlib STATIC
  ${zlib_SOURCE_DIR}/adler32.c
  ${zlib_SOURCE_DIR}/crc32.c
  ${zlib_SOURCE_DIR}/deflate.c
  ${zlib_SOURCE_DIR}/inffast.c
  ${zlib_SOURCE_DIR}/inflate.c
  ${zlib_SOURCE_DIR}/inftrees.c
  ${zlib_SOURCE_DIR}/trees.c
  ${zlib_SOURCE_DIR}/zutil.c
)
target_include_directories(tk_zlib PUBLIC ${zlib_SOURCE_DIR})

add_library(tk_bz2 STATIC
  ${bzip2_SOURCE_DIR}/blocksort.c
  ${bzip2_SOURCE_DIR}/bzlib.c
  ${bzip2_SOURCE_DIR}/compress.c
  ${bzip2_SOURCE_DIR}/crctable.c
  ${bzip2_SOURCE_DIR}/decompress.c
  ${bzip2_SOURCE_DIR}/huffman.c
  ${bzip2_SOURCE_DIR}/randtable.c
)
target_include_directories(tk_bz2 PUBLIC ${bzip2_SOURCE_DIR})

foreach(vendored tk_zlib tk_bz2)
  set_target_properties(${vendored} PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
    POSITION_INDEPENDENT_CODE ON
  )
  target_compile_options(${vendored} PRIVATE $<IF:$<C_COMPILER_ID:MSVC>,/w,-w>)
endforeach()

set(ZLIB_INCLUDE_DIR "${zlib_SOURCE_DIR}" CACHE PATH "" FORCE)
set(ZLIB_LIBRARY tk_zlib CACHE STRING "" FORCE)
set(BZIP2_INCLUDE_DIR "${bzip2_SOURCE_DIR}" CACHE PATH "" FORCE)
set(BZIP2_LIBRARIES tk_bz2 CACHE STRING "" FORCE)
set(BZIP2_NEED_PREFIX 1 CACHE INTERNAL "")

# Static libraries: skip dllimport decoration; libarchive's try-compile probes can't see the in-tree targets.
add_compile_definitions(LZMA_API_STATIC USE_BZIP2_STATIC)

# Fetch libarchive for multi-format archive extraction (BSD license)
FetchContent_Declare(
  libarchive
  GIT_REPOSITORY https://github.com/libarchive/libarchive.git
  GIT_TAG v3.7.4
)

set(ENABLE_TAR OFF)
set(ENABLE_CPIO OFF)
set(ENABLE_CAT OFF)
set(ENABLE_TEST OFF)
set(ENABLE_INSTALL OFF)
set(ENABLE_STATIC ON)
set(ENABLE_SHARED OFF)
set(ENABLE_ACL OFF)
set(ENABLE_XATTR OFF)
# Only archive_static is used. libarchive builds a shared library as well unless told otherwise, and that
# cannot link the static codec libraries above (liblzma.a is not position independent).
set(BUILD_SHARED_LIBS OFF)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS TRUE CACHE INTERNAL "")
set(CMAKE_WARN_DEPRECATED FALSE CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libarchive)
unset(BUILD_SHARED_LIBS)
set(CMAKE_WARN_DEPRECATED TRUE CACHE BOOL "" FORCE)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS FALSE CACHE INTERNAL "")
unset(CMAKE_POLICY_VERSION_MINIMUM)

set_target_properties(archive_static PROPERTIES
  AUTOMOC OFF
  AUTOUIC OFF
  AUTORCC OFF
)

qt_standard_project_setup()

add_subdirectory(updater)
add_subdirectory(src)
add_subdirectory(tests)
