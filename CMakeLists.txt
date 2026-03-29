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

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Network Concurrent Test WebSockets LinguistTools)

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

# Fetch libarchive for multi-format archive extraction (BSD license)
FetchContent_Declare(
  libarchive
  GIT_REPOSITORY https://github.com/libarchive/libarchive.git
  GIT_TAG v3.7.4
)

set(ENABLE_TAR ON)
set(ENABLE_CPIO OFF)
set(ENABLE_CAT OFF)
set(ENABLE_TEST OFF)
set(ENABLE_INSTALL OFF)
set(ENABLE_STATIC ON)
set(ENABLE_SHARED OFF)
set(ENABLE_ACL OFF)
set(ENABLE_XATTR OFF)

set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS TRUE CACHE INTERNAL "")
FetchContent_MakeAvailable(libarchive)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS FALSE CACHE INTERNAL "")

set_target_properties(archive_static PROPERTIES
  AUTOMOC OFF
  AUTOUIC OFF
  AUTORCC OFF
)

qt_standard_project_setup()

add_subdirectory(updater)
add_subdirectory(src)
add_subdirectory(tests)
