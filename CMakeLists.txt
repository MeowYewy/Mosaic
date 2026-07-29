cmake_minimum_required(VERSION 3.21)
project(Mosaic VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(DEFINED ENV{QT_DIR})
    set(QT_ROOT "$ENV{QT_DIR}" CACHE PATH "Qt installation path")
elseif(NOT DEFINED QT_ROOT)
    set(QT_ROOT "D:/Qt/6.11.1/mingw_64" CACHE PATH "Qt installation path")
    if(NOT EXISTS "${QT_ROOT}/bin/qmake.exe")
        set(QT_ROOT "C:/Qt/6.11.1/mingw_64" CACHE PATH "Qt installation path" FORCE)
    endif()
endif()
list(APPEND CMAKE_PREFIX_PATH "${QT_ROOT}")

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Quick
    QuickControls2
    QuickEffects
    Widgets
    Concurrent
    Network
)

qt_standard_project_setup()
qt_policy(SET QTP0001 NEW)

set(SOURCES
    src/main.cpp
    src/appsettings.cpp
    src/securestorage.cpp
    src/appcontroller.cpp
    src/filelistmodel.cpp
    src/redactionmodel.cpp
    src/piidetector.cpp
    src/documentloader.cpp
    src/docxrenderer.cpp
    src/pdfpagerenderer.cpp
    src/mosaicengine.cpp
    src/previewimageprovider.cpp
    src/ocrengine.cpp
    src/pipelinediag.cpp
    src/labreportdetector.cpp
    src/aimarkengine.cpp
    src/qwenocrengine.cpp
    src/privacyredactionpolicy.cpp
    src/textregionmapper.cpp
    src/privacyformmapper.cpp
    src/updatechecker.cpp
    src/filedropbridge.cpp
    src/pdf/pdfappcontroller.cpp
    src/pdf/pdffilelistmodel.cpp
    src/pdf/pdfthumbprovider.cpp
    src/pdf/pdfpreviewmodel.cpp
    src/pdf/pdfengine.cpp
    src/pdf/filepicker.cpp
    src/pdf/officeconverter.cpp
    src/pdf/officetextextractor.cpp
    src/pdf/watermarklayout.cpp
)

set(HEADERS
    src/appsettings.h
    src/securestorage.h
    src/appcontroller.h
    src/filelistmodel.h
    src/redactionmodel.h
    src/piidetector.h
    src/documentloader.h
    src/docxrenderer.h
    src/pdfpagerenderer.h
    src/mosaicengine.h
    src/previewimageprovider.h
    src/ocrengine.h
    src/pipelinediag.h
    src/labreportdetector.h
    src/aimarkengine.h
    src/qwenocrengine.h
    src/privacyredactionpolicy.h
    src/textregionmapper.h
    src/privacyformmapper.h
    src/updatechecker.h
    src/filedropbridge.h
    src/pdf/pdfappcontroller.h
    src/pdf/pdffilelistmodel.h
    src/pdf/pdfthumbprovider.h
    src/pdf/pdfpreviewmodel.h
    src/pdf/pdfengine.h
    src/pdf/filepicker.h
    src/pdf/officeconverter.h
    src/pdf/officetextextractor.h
    src/pdf/watermarklayout.h
)

qt_add_executable(Mosaic ${SOURCES} ${HEADERS})

target_include_directories(Mosaic PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/src/pdf"
)

set_source_files_properties(qml/style/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/components/FileDropPaths.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(Mosaic
    URI ProjectO
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/style/Theme.qml
        qml/components/AppHeader.qml
        qml/components/ShadowCard.qml
        qml/components/StyledButton.qml
        qml/components/FileDropZone.qml
        qml/components/FileDropPaths.qml
        qml/components/FileListView.qml
        qml/components/IconDeleteButton.qml
        qml/components/IconToolButton.qml
        qml/components/ToolStripButton.qml
        qml/components/StyledToolTip.qml
        qml/components/LucideIcon.qml
        qml/components/DragGrip.qml
        qml/components/StatusToast.qml
        qml/components/PreviewCanvas.qml
        qml/components/ToolBar.qml
        qml/components/SettingsDropdown.qml
        qml/components/MenuOption.qml
        qml/components/ThemeToggleButton.qml
        qml/components/AboutDialog.qml
        qml/components/SettingsDialog.qml
        qml/components/SettingsOptionRow.qml
        qml/components/SettingsTextRow.qml
        qml/components/SettingsIdCardDigitRow.qml
        qml/components/ShortcutInfoTip.qml
        qml/components/AppLogo.qml
        qml/components/UpdateStatusIcon.qml
        qml/components/UpdateNewVersionLabel.qml
        qml/components/FeatureTabs.qml
        qml/components/FeatureTabsRow.qml
        qml/components/GlobalProgressBar.qml
        qml/components/ActionBar.qml
        qml/components/ScrollPreview.qml
        qml/components/PdfFileListView.qml
        qml/components/PdfFileDropZone.qml
        qml/components/FilePickerDialog.qml
        qml/components/ChangelogDialog.qml
        qml/components/FileKindIcon.qml
        qml/components/StyledComboBox.qml
        qml/components/RotateComboBox.qml
        qml/components/CompactComboBox.qml
        qml/components/ConvertComboBox.qml
        qml/components/CompressComboBox.qml
        qml/components/WatermarkColorPicker.qml
        qml/components/WatermarkCountCombo.qml
        qml/components/WatermarkInputField.qml
        qml/components/SwitchToggle.qml
        qml/components/PathBreadcrumbBar.qml
        qml/components/PickerFilterCombo.qml
        qml/components/FilePickerIconButton.qml
        qml/components/MaskView.qml
        qml/components/AiMarkResultsPanel.qml
        qml/components/MaskShortcutHandler.qml
        qml/components/ThemeTransitionOverlay.qml
        qml/pages/PageBase.qml
        qml/pages/SplitPage.qml
        qml/pages/MergePage.qml
        qml/pages/RotatePage.qml
        qml/pages/ConvertPage.qml
        qml/pages/CompressPage.qml
        qml/pages/WatermarkPage.qml
    RESOURCES
        resources/logo.svg
        resources/app-icon.png
        resources/app-icon.ico
        resources/update.json
        resources/changelog.json
        resources/icons/full-screen.svg
        resources/icons/expand.svg
        resources/icons/shrink.svg
        resources/icons/square.svg
        resources/icons/square-mouse-pointer.svg
        resources/icons/square-filled.svg
        resources/icons/mouse-pointer-2.svg
        resources/icons/square-dashed-mouse-pointer.svg
        resources/icons/trash-2.svg
        resources/icons/grid-3x3.svg
        resources/icons/eye.svg
        resources/icons/info.svg
        resources/icons/menu.svg
        resources/icons/chevron-left.svg
        resources/icons/clipboard-paste.svg
        resources/icons/copy.svg
        resources/icons/file.svg
        resources/icons/file-braces.svg
        resources/icons/file-image.svg
        resources/icons/file-text.svg
        resources/icons/file-type.svg
        resources/icons/file-video.svg
        resources/icons/file-volume.svg
        resources/icons/folder.svg
        resources/icons/hard-drive.svg
        resources/icons/loader-circle.svg
        resources/icons/pdf.svg
        resources/icons/pencil.svg
        resources/icons/scissors.svg
)

target_link_libraries(Mosaic PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickEffects
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Network
)

if(WIN32)
    target_link_libraries(Mosaic PRIVATE crypt32)
endif()

if(TARGET Qt6::Pdf)
    target_link_libraries(Mosaic PRIVATE Qt6::Pdf)
    target_compile_definitions(Mosaic PRIVATE HAS_QT_PDF=1)
    message(STATUS "Qt6 Pdf found — using built-in PDF preview")
else()
    message(STATUS "Qt6 Pdf not found — using poppler pdftoppm fallback")
endif()

if(WIN32)
    set_target_properties(Mosaic PROPERTIES WIN32_EXECUTABLE TRUE)
    if(EXISTS "${CMAKE_SOURCE_DIR}/resources/app-icon.ico")
        target_sources(Mosaic PRIVATE "${CMAKE_SOURCE_DIR}/resources/app-icon.rc")
    endif()

    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_ROOT}/bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET Mosaic POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                "--qmldir" "${CMAKE_SOURCE_DIR}/qml"
                "--no-translations"
                "$<TARGET_FILE:Mosaic>"
            COMMENT "Deploy Qt runtime (windeployqt)"
            VERBATIM
        )
    else()
        message(WARNING "windeployqt not found; run scripts/deploy-dev.bat after build")
    endif()

    get_filename_component(_MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(_rtdll IN ITEMS libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
        if(EXISTS "${_MINGW_BIN}/${_rtdll}")
            add_custom_command(TARGET Mosaic POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_MINGW_BIN}/${_rtdll}"
                    "$<TARGET_FILE_DIR:Mosaic>/${_rtdll}"
                COMMENT "Copy runtime ${_rtdll}"
            )
        endif()
    endforeach()
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/tools/qpdf")
    add_custom_command(TARGET Mosaic POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/tools/qpdf"
            "$<TARGET_FILE_DIR:Mosaic>/tools/qpdf"
        COMMENT "Copy qpdf tools for PDF operations"
    )
endif()

# Copy poppler tools if present (run setup-poppler.bat once)
if(EXISTS "${CMAKE_SOURCE_DIR}/tools/poppler/pdftoppm.exe")
    add_custom_command(TARGET Mosaic POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/tools/poppler"
            "$<TARGET_FILE_DIR:Mosaic>/tools/poppler"
        COMMENT "Copy poppler tools to build output"
    )
endif()

# Copy bundled Tesseract OCR if present (offline scan recognition)
set(PROJECTO_TESSERACT "${CMAKE_SOURCE_DIR}/tools/tesseract")
if(EXISTS "${PROJECTO_TESSERACT}/tesseract.exe")
    add_custom_command(TARGET Mosaic POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Mosaic>/tools/tesseract"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECTO_TESSERACT}" "$<TARGET_FILE_DIR:Mosaic>/tools/tesseract"
        COMMENT "Copy Tesseract OCR for offline scan recognition"
    )
endif()
