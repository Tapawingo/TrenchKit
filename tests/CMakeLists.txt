qt_add_executable(tests_utilities
  TestUtilities.cpp
  ${CMAKE_SOURCE_DIR}/src/core/utils/ArchiveExtractor.h
  ${CMAKE_SOURCE_DIR}/src/core/utils/ArchiveExtractor.cpp
  ${CMAKE_SOURCE_DIR}/src/core/utils/UpdateArchiveExtractor.h
  ${CMAKE_SOURCE_DIR}/src/core/utils/UpdateArchiveExtractor.cpp
  ${CMAKE_SOURCE_DIR}/src/core/utils/UpdateCleanup.h
  ${CMAKE_SOURCE_DIR}/src/core/utils/UpdateCleanup.cpp
  ${CMAKE_SOURCE_DIR}/src/core/services/UpdaterService.h
  ${CMAKE_SOURCE_DIR}/src/core/services/UpdaterService.cpp
)

target_include_directories(tests_utilities
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/_deps/zip-src/src
    ${CMAKE_BINARY_DIR}/_deps/libarchive-src/libarchive/.
)

target_compile_definitions(tests_utilities
  PRIVATE
    LIBARCHIVE_STATIC
)

target_link_libraries(tests_utilities
  PRIVATE
    Qt6::Test
    Qt6::Network
    Qt6::Concurrent
    zip
    archive_static
)

add_test(NAME tests_utilities COMMAND tests_utilities)

if(WIN32)
  # Copy zip DLL if it exists
  set(ZIP_DLL_PATH "${CMAKE_BINARY_DIR}/_deps/zip-build/libzip.dll")
  if(EXISTS "${ZIP_DLL_PATH}")
    add_custom_command(TARGET tests_utilities POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ZIP_DLL_PATH}"
        $<TARGET_FILE_DIR:tests_utilities>
      COMMENT "Copying libzip.dll for tests_utilities"
    )
  endif()

  foreach(QT_TEST_TARGET
          Qt6::Core
          Qt6::Test
          Qt6::Network
          Qt6::Concurrent)
    add_custom_command(TARGET tests_utilities POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:${QT_TEST_TARGET}>
        $<TARGET_FILE_DIR:tests_utilities>
      COMMENT "Copying $<TARGET_FILE_NAME:${QT_TEST_TARGET}> for tests_utilities"
    )
  endforeach()

  if(MINGW)
    get_filename_component(MINGW_BIN_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
    set(MINGW_DLLS "")
    foreach(MINGW_DLL_NAME
            libgcc_s_seh-1.dll
            libstdc++-6.dll
            libwinpthread-1.dll)
      if(EXISTS "${MINGW_BIN_DIR}/${MINGW_DLL_NAME}")
        list(APPEND MINGW_DLLS "${MINGW_BIN_DIR}/${MINGW_DLL_NAME}")
      endif()
    endforeach()
    if(MINGW_DLLS)
      add_custom_command(TARGET tests_utilities POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${MINGW_DLLS}
          $<TARGET_FILE_DIR:tests_utilities>
        COMMENT "Copying MinGW runtime DLLs for tests_utilities"
      )
    endif()
  endif()
endif()

qt_add_executable(tests_mod_manifest
  TestModManifest.cpp
  ${CMAKE_SOURCE_DIR}/src/core/utils/PakFileReader.h
  ${CMAKE_SOURCE_DIR}/src/core/utils/PakFileReader.cpp
  ${CMAKE_SOURCE_DIR}/src/core/utils/ModManifestReader.h
  ${CMAKE_SOURCE_DIR}/src/core/utils/ModManifestReader.cpp
)

target_include_directories(tests_mod_manifest
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(tests_mod_manifest
  PRIVATE
    Qt6::Test
)

add_test(NAME tests_mod_manifest COMMAND tests_mod_manifest)

if(WIN32)
  foreach(QT_TEST_TARGET Qt6::Core Qt6::Test)
    add_custom_command(TARGET tests_mod_manifest POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:${QT_TEST_TARGET}>
        $<TARGET_FILE_DIR:tests_mod_manifest>
      COMMENT "Copying $<TARGET_FILE_NAME:${QT_TEST_TARGET}> for tests_mod_manifest"
    )
  endforeach()
endif()

qt_add_executable(tests_ts_translator
  TestTsTranslator.cpp
  ${CMAKE_SOURCE_DIR}/src/core/utils/TsTranslator.h
  ${CMAKE_SOURCE_DIR}/src/core/utils/TsTranslator.cpp
)

target_include_directories(tests_ts_translator
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(tests_ts_translator
  PRIVATE
    Qt6::Test
)

add_test(NAME tests_ts_translator COMMAND tests_ts_translator)

if(WIN32)
  foreach(QT_TEST_TARGET Qt6::Core Qt6::Test)
    add_custom_command(TARGET tests_ts_translator POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:${QT_TEST_TARGET}>
        $<TARGET_FILE_DIR:tests_ts_translator>
      COMMENT "Copying $<TARGET_FILE_NAME:${QT_TEST_TARGET}> for tests_ts_translator"
    )
  endforeach()
endif()

add_test(
  NAME ui_smoke
  COMMAND $<TARGET_FILE:TrenchKit> --smoke-test
)

if(WIN32)
  set_tests_properties(ui_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=windows")
else()
  set_tests_properties(ui_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
