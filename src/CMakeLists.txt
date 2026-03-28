qt_add_executable(TrenchKit
  main.cpp
  MainWindow.h
  MainWindow.cpp
  MainWindow.ui

  common/widgets/TitleBar.h
  common/widgets/TitleBar.cpp
  common/widgets/GradientFrame.h
  common/widgets/GradientFrame.cpp
  common/widgets/PanelFrame.h
  common/widgets/PanelFrame.cpp

  views/mod_manager/InstallPathWidget.h
  views/mod_manager/InstallPathWidget.cpp
  views/mod_manager/ModListWidget.h
  views/mod_manager/ModListWidget.cpp
  views/mod_manager/DraggableModList.h
  views/mod_manager/DraggableModList.cpp
  views/mod_manager/ModRowWidget.h
  views/mod_manager/ModRowWidget.cpp
  views/mod_manager/ConflictTooltip.h
  views/mod_manager/ConflictTooltip.cpp
  views/mod_manager/ProfileRowWidget.h
  views/mod_manager/ProfileRowWidget.cpp
  views/mod_manager/DraggableProfileList.h
  views/mod_manager/DraggableProfileList.cpp
  views/mod_manager/RightPanelWidget.h
  views/mod_manager/RightPanelWidget.cpp
  views/mod_manager/ActionsWidget.h
  views/mod_manager/ActionsWidget.cpp
  views/mod_manager/BackupWidget.h
  views/mod_manager/BackupWidget.cpp
  views/mod_manager/LogEntryWidget.h
  views/mod_manager/LogEntryWidget.cpp
  views/mod_manager/ActivityLogWidget.h
  views/mod_manager/ActivityLogWidget.cpp
  views/mod_manager/LaunchWidget.h
  views/mod_manager/LaunchWidget.cpp
  views/mod_manager/ProfileManagerWidget.h
  views/mod_manager/ProfileManagerWidget.cpp

  views/settings/SettingsWidget.h
  views/settings/SettingsWidget.cpp

  core/managers/ModManager.h
  core/managers/ModManager.cpp
  core/managers/ProfileManager.h
  core/managers/ProfileManager.cpp

  core/services/ModUpdateService.h
  core/services/ModUpdateService.cpp
  core/services/ItchModUpdateService.h
  core/services/ItchModUpdateService.cpp
  core/services/UpdaterService.h
  core/services/UpdaterService.cpp
  core/services/ModConflictDetector.h
  core/services/ModConflictDetector.cpp

  core/api/NexusModsClient.h
  core/api/NexusModsClient.cpp
  core/api/NexusModsAuth.h
  core/api/NexusModsAuth.cpp
  core/api/ItchClient.h
  core/api/ItchClient.cpp
  core/api/ItchAuth.h
  core/api/ItchAuth.cpp

  core/models/ModInfo.h
  core/models/ModInfo.cpp
  core/models/ProfileInfo.h
  core/models/ProfileInfo.cpp
  core/models/NexusFileInfo.h
  core/models/NexusFileInfo.cpp
  core/models/ItchUploadInfo.h
  core/models/ItchUploadInfo.cpp
  core/models/ModUpdateInfo.h
  core/models/ItchUpdateInfo.h

  core/utils/FoxholeDetector.h
  core/utils/FoxholeDetector.cpp
  core/utils/Theme.h
  core/utils/Theme.cpp
  core/utils/Logger.h
  core/utils/Logger.cpp
  core/utils/ArchiveExtractor.h
  core/utils/ArchiveExtractor.cpp
  core/utils/UpdateArchiveExtractor.h
  core/utils/UpdateArchiveExtractor.cpp
  core/utils/UpdateCleanup.h
  core/utils/UpdateCleanup.cpp
  core/utils/NexusUrlParser.h
  core/utils/NexusUrlParser.cpp
  core/utils/ItchUrlParser.h
  core/utils/ItchUrlParser.cpp
  core/utils/ModManifestReader.h
  core/utils/ModManifestReader.cpp
  core/utils/PakFileReader.h
  core/utils/PakFileReader.cpp
  core/utils/TranslationManager.h
  core/utils/TranslationManager.cpp
  core/utils/TsTranslator.h
  core/utils/TsTranslator.cpp

  common/modals/ModalManager.h
  common/modals/ModalManager.cpp
  common/modals/ModalOverlay.h
  common/modals/ModalOverlay.cpp
  common/modals/ModalContainer.h
  common/modals/ModalContainer.cpp
  common/modals/BaseModalContent.h
  common/modals/BaseModalContent.cpp
  common/modals/MessageModal.h
  common/modals/MessageModal.cpp
  common/modals/InputModal.h
  common/modals/InputModal.cpp
  common/modals/ProgressModal.h
  common/modals/ProgressModal.cpp

  modals/mod_manager/FileSelectionModalContent.h
  modals/mod_manager/FileSelectionModalContent.cpp
  modals/mod_manager/BackupSelectionModalContent.h
  modals/mod_manager/BackupSelectionModalContent.cpp
  modals/mod_manager/ConflictResolutionModalContent.h
  modals/mod_manager/ConflictResolutionModalContent.cpp
  modals/mod_manager/ModMetadataModalContent.h
  modals/mod_manager/ModMetadataModalContent.cpp
  modals/mod_manager/ModUpdateModalContent.h
  modals/mod_manager/ModUpdateModalContent.cpp
  modals/mod_manager/ItchModUpdateModalContent.h
  modals/mod_manager/ItchModUpdateModalContent.cpp
  modals/mod_manager/NexusDownloadModalContent.h
  modals/mod_manager/NexusDownloadModalContent.cpp
  modals/mod_manager/ItchDownloadModalContent.h
  modals/mod_manager/ItchDownloadModalContent.cpp
  modals/mod_manager/AddModModalContent.h
  modals/mod_manager/AddModModalContent.cpp
  modals/mod_manager/ConflictDetailModalContent.h
  modals/mod_manager/ConflictDetailModalContent.cpp
  modals/mod_manager/NexusRegistrationModalContent.h
  modals/mod_manager/NexusRegistrationModalContent.cpp
  modals/mod_manager/ItchRegistrationModalContent.h
  modals/mod_manager/ItchRegistrationModalContent.cpp

  resources.qrc
  app.rc
)

target_compile_options(TrenchKit PRIVATE
  $<IF:$<CXX_COMPILER_ID:MSVC>,/W4,-Wall -Wextra -Wpedantic>
)

target_compile_definitions(TrenchKit PRIVATE
  TRENCHKIT_VERSION="${TRENCHKIT_FULL_VERSION}"
  LIBARCHIVE_STATIC
)

if(WIN32)
  target_link_options(TrenchKit PRIVATE
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/SUBSYSTEM:WINDOWS>
    $<$<AND:$<CONFIG:Release>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:-mwindows>
  )
endif()

target_link_libraries(TrenchKit
  PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
    Qt6::WebSockets
    zip
    archive_static
)

target_include_directories(TrenchKit
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

file(GLOB _TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/locales/TrenchKit_*.ts")
list(FILTER _TS_FILES EXCLUDE REGEX "TrenchKit_template\\.ts$")
qt_add_translations(TrenchKit TS_FILES ${_TS_FILES})

# Generate translation template for community translators
# Run: cmake --build build/debug --target update_translation_template
set(_LUPDATE_EN "${CMAKE_BINARY_DIR}/.lupdate/TrenchKit_en.ts")
set(_TEMPLATE_OUT "${CMAKE_CURRENT_SOURCE_DIR}/locales/TrenchKit_template.ts")
add_custom_target(update_translation_template
  DEPENDS update_translations
  COMMAND ${CMAKE_COMMAND} -E copy "${_LUPDATE_EN}" "${_TEMPLATE_OUT}"
  COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_SOURCE_DIR}/locales/strip_template.cmake"
  COMMENT "Generating translation template from lupdate output"
)

# Post-build: Copy platform-specific TLS backend
set(QT_TLS_PLUGIN_DIR "${Qt6_DIR}/../../../plugins/tls")
if(WIN32)
    set(TLS_PLUGIN "qschannelbackend.dll")
elseif(APPLE)
    set(TLS_PLUGIN "libqsecuretransportbackend.dylib")
else()
    set(TLS_PLUGIN "libqopensslbackend.so")
endif()

if(EXISTS "${QT_TLS_PLUGIN_DIR}/${TLS_PLUGIN}")
    add_custom_command(TARGET TrenchKit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:TrenchKit>/tls
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_TLS_PLUGIN_DIR}/${TLS_PLUGIN}"
            $<TARGET_FILE_DIR:TrenchKit>/tls/
    )
endif()

# Windows-specific runtime dependencies
if(WIN32)
    add_dependencies(TrenchKit updater)
    add_custom_command(TARGET TrenchKit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:updater>
            $<TARGET_FILE_DIR:TrenchKit>
        COMMENT "Copying updater helper to output directory"
    )

    # Copy zip DLL if it exists
    set(ZIP_DLL_PATH "${CMAKE_BINARY_DIR}/_deps/zip-build/libzip.dll")
    if(EXISTS "${ZIP_DLL_PATH}")
        add_custom_command(TARGET TrenchKit POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ZIP_DLL_PATH}"
                $<TARGET_FILE_DIR:TrenchKit>
            COMMENT "Copying libzip.dll to output directory"
        )
    endif()

    # Qt DLLs
    set(QT_BIN_DIR "${Qt6_DIR}/../../../bin")
    set(QT_APP_DLLS "")
    foreach(QT_DLL_NAME
            Qt6Core.dll
            Qt6Gui.dll
            Qt6Widgets.dll
            Qt6Network.dll
            Qt6Concurrent.dll
            Qt6WebSockets.dll)
        if(EXISTS "${QT_BIN_DIR}/${QT_DLL_NAME}")
            list(APPEND QT_APP_DLLS "${QT_BIN_DIR}/${QT_DLL_NAME}")
        endif()
    endforeach()
    if(QT_APP_DLLS)
        add_custom_command(TARGET TrenchKit POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${QT_APP_DLLS}
                $<TARGET_FILE_DIR:TrenchKit>
            COMMENT "Copying Qt DLLs to output directory"
        )
    endif()

    # MinGW runtime DLLs
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
            add_custom_command(TARGET TrenchKit POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${MINGW_DLLS}
                    $<TARGET_FILE_DIR:TrenchKit>
                COMMENT "Copying MinGW runtime DLLs to output directory"
            )
        endif()
    endif()

    # Qt platform plugin
    set(QT_PLATFORM_DLL "${Qt6_DIR}/../../../plugins/platforms/qwindows.dll")
    if(EXISTS "${QT_PLATFORM_DLL}")
        add_custom_command(TARGET TrenchKit POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                $<TARGET_FILE_DIR:TrenchKit>/platforms
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_PLATFORM_DLL}"
                $<TARGET_FILE_DIR:TrenchKit>/platforms/
            COMMENT "Copying Qt platform plugin to output directory"
        )
    endif()
endif()
