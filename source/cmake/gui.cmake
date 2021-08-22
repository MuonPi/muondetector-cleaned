set(MUONDETECTOR_GUI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gui/src")
set(MUONDETECTOR_GUI_UI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gui/src")
set(MUONDETECTOR_GUI_RES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gui/")
set(MUONDETECTOR_GUI_HEADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gui/include")
set(MUONDETECTOR_GUI_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gui/config")
set(MUONDETECTOR_GUI_QML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gui/qml")
set(MUONDETECTOR_GUI_APPLE_RES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gui/res")



set(MUONDETECTOR_GUI_SOURCE_FILES
    "${MUONDETECTOR_GUI_SOURCE_DIR}/calibform.cpp"
    "${MUONDETECTOR_GUI_SOURCE_DIR}/calibscandialog.cpp"
    "${MUONDETECTOR_GUI_SOURCE_DIR}/custom_histogram_widget.cpp"
    "${MUONDETECTOR_GUI_SOURCE_DIR}/custom_plot_widget.cpp"
    "${MUONDETECTOR_GUI_SOURCE_DIR}/gnssposwidget.cpp"
    "${MUONDETECTOR_GUI_SOURCE_DIR}/gpssatsform.cpp"
    "${MUONDETECTOR_GUI_SOURCE_DIR}/histogramdataform.cpp"
    "${MUONDETECTOR_GUI_SOURCE_DIR}/i2cform.cpp"
    "${MUONDETECTOR_GUI_SOURCE_DIR}/logplotswidget.cpp"
    "${MUONDETECTOR_GUI_SOURCE_DIR}/main.cpp"
    "${MUONDETECTOR_GUI_SOURCE_DIR}/mainwindow.cpp"
    "${MUONDETECTOR_GUI_SOURCE_DIR}/map.cpp"
    "${MUONDETECTOR_GUI_SOURCE_DIR}/parametermonitorform.cpp"
    "${MUONDETECTOR_GUI_SOURCE_DIR}/plotcustom.cpp"
    "${MUONDETECTOR_GUI_SOURCE_DIR}/scanform.cpp"
    "${MUONDETECTOR_GUI_SOURCE_DIR}/settings.cpp"
    "${MUONDETECTOR_GUI_SOURCE_DIR}/spiform.cpp"
    "${MUONDETECTOR_GUI_SOURCE_DIR}/status.cpp"
    )

set(MUONDETECTOR_GUI_HEADER_FILES
    "${MUONDETECTOR_GUI_HEADER_DIR}/calibform.h"
    "${MUONDETECTOR_GUI_HEADER_DIR}/calibscandialog.h"
    "${MUONDETECTOR_GUI_HEADER_DIR}/custom_histogram_widget.h"
    "${MUONDETECTOR_GUI_HEADER_DIR}/custom_plot_widget.h"
    "${MUONDETECTOR_GUI_HEADER_DIR}/gnssposwidget.h"
    "${MUONDETECTOR_GUI_HEADER_DIR}/gpssatsform.h"
    "${MUONDETECTOR_GUI_HEADER_DIR}/histogramdataform.h"
    "${MUONDETECTOR_GUI_HEADER_DIR}/i2cform.h"
    "${MUONDETECTOR_GUI_HEADER_DIR}/logplotswidget.h"
    "${MUONDETECTOR_GUI_HEADER_DIR}/mainwindow.h"
    "${MUONDETECTOR_GUI_HEADER_DIR}/map.h"
    "${MUONDETECTOR_GUI_HEADER_DIR}/parametermonitorform.h"
    "${MUONDETECTOR_GUI_HEADER_DIR}/plotcustom.h"
    "${MUONDETECTOR_GUI_HEADER_DIR}/scanform.h"
    "${MUONDETECTOR_GUI_HEADER_DIR}/settings.h"
    "${MUONDETECTOR_GUI_HEADER_DIR}/spiform.h"
    "${MUONDETECTOR_GUI_HEADER_DIR}/status.h"
    )
set(MUONDETECTOR_GUI_UI_FILES
    "${MUONDETECTOR_GUI_UI_DIR}/calibform.ui"
    "${MUONDETECTOR_GUI_UI_DIR}/calibscandialog.ui"
    "${MUONDETECTOR_GUI_UI_DIR}/gnssposwidget.ui"
    "${MUONDETECTOR_GUI_UI_DIR}/gpssatsform.ui"
    "${MUONDETECTOR_GUI_UI_DIR}/histogramdataform.ui"
    "${MUONDETECTOR_GUI_UI_DIR}/i2cform.ui"
    "${MUONDETECTOR_GUI_UI_DIR}/logplotswidget.ui"
    "${MUONDETECTOR_GUI_UI_DIR}/mainwindow.ui"
    "${MUONDETECTOR_GUI_UI_DIR}/map.ui"
    "${MUONDETECTOR_GUI_UI_DIR}/parametermonitorform.ui"
    "${MUONDETECTOR_GUI_UI_DIR}/scanform.ui"
    "${MUONDETECTOR_GUI_UI_DIR}/settings.ui"
    "${MUONDETECTOR_GUI_UI_DIR}/spiform.ui"
    "${MUONDETECTOR_GUI_UI_DIR}/status.ui"
    )

set(MUONDETECTOR_GUI_RESOURCE_FILES "")

if (WIN32)
set(MUONDETECTOR_GUI_RESOURCE_FILES
    "${MUONDETECTOR_GUI_RESOURCE_FILES}"
    "${MUONDETECTOR_GUI_RES_DIR}/windows.rc"
    )
configure_file(
    "${MUONDETECTOR_GUI_RES_DIR}/res/muon.ico"
    "${CMAKE_CURRENT_BINARY_DIR}/muon.ico"
    )
else()

configure_file(
    "${MUONDETECTOR_GUI_CONFIG_DIR}/muondetector-gui.1"
    "${CMAKE_CURRENT_BINARY_DIR}/muondetector-gui.1"
    )

endif()

if(APPLE)
    #QWT-Framwork suchen
    find_library(QWT
        NAMES qwt
        HINTS /opt/local/libexec/qt5/lib
        REQUIRED)
    if(QWT)
        include_directories(${QWT}/Headers)
        link_libraries(${QWT})
        message("QWT found: ${QWT}")
    endif()
elseif(WIN32)
    find_library(QWT
        NAMES qwt
        HINTS ${QWT_DIR}/lib
        REQUIRED)
        if(QWT)
            message("QWT found: ${QWT}")
        endif()
else()
    find_library(QWT_QT5 qwt-qt5 REQUIRED)

endif()

find_package(Qt5 COMPONENTS Network Svg Widgets Gui Quick QuickWidgets Qml REQUIRED)
find_package(Qt5QuickCompiler)

if(Qt5QuickCompiler_FOUND)

qtquick_compiler_add_resources(qml_QRC "${MUONDETECTOR_GUI_RES_DIR}/resources.qrc")

else()

qt5_add_resources(qml_QRC "${MUONDETECTOR_GUI_RES_DIR}/resources.qrc")

endif()

if(APPLE)
  

  set(MACOSX_BUNDLE_ICON_FILE muon.icns)
  set(myApp_ICON ${MUONDETECTOR_GUI_APPLE_RES_DIR}/muon.icns)
  set_source_files_properties(${myApp_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

  add_executable(muondetector-gui MACOSX_BUNDLE ${myApp_ICON} ${MUONDETECTOR_GUI_SOURCE_FILES} ${MUONDETECTOR_GUI_HEADER_FILES} ${MUONDETECTOR_GUI_UI_FILES} ${MUONDETECTOR_GUI_RESOURCE_FILES} ${qml_QRC} )  

  set_target_properties(muondetector-gui PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${MUONDETECTOR_GUI_APPLE_RES_DIR}/Info.plist.in)

else()

add_executable(muondetector-gui  ${MUONDETECTOR_GUI_SOURCE_FILES} ${MUONDETECTOR_GUI_HEADER_FILES} ${MUONDETECTOR_GUI_UI_FILES} ${MUONDETECTOR_GUI_RESOURCE_FILES} ${qml_QRC})

endif()


add_dependencies(muondetector-gui muondetector-shared)

set_target_properties(muondetector-gui PROPERTIES POSITION_INDEPENDENT_CODE 1)

target_include_directories(muondetector-gui PUBLIC
    $<BUILD_INTERFACE:${MUONDETECTOR_GUI_HEADER_DIR}>
    $<BUILD_INTERFACE:${LIBRARY_INCLUDE_DIR}>
    $<BUILD_INTERFACE:/usr/include/qwt>
    )

if(WIN32)
target_compile_definitions(muondetector-gui PUBLIC QWT_DLL)

target_include_directories(muondetector-gui PUBLIC
    $<BUILD_INTERFACE:${CRYPTOPP_DIR}/include>
    $<INSTALL_INTERFACE:${CRYPTOPP_DIR}/include>
    $<BUILD_INTERFACE:${QWT_DIR}/include>
    $<INSTALL_INTERFACE:${QWT_DIR}/include>
    ${Qt5Widgets_INCLUDE_DIRS}
    ${Qt5Svg_INCLUDE_DIRS}
    ${Qt5Network_INCLUDE_DIRS}
    ${Qt5Quick_INCLUDE_DIRS}
    ${Qt5QuickWidgets_INCLUDE_DIRS}
    ${Qt5Qml_INCLUDE_DIRS}
    )

target_link_libraries(muondetector-gui
    Qt5::Network Qt5::Svg Qt5::Widgets Qt5::Gui Qt5::Quick Qt5::QuickWidgets Qt5::Qml
    muondetector-shared
    ${QWT}
    )

elseif(APPLE)

target_link_libraries(muondetector-gui
    Qt5::Network Qt5::Svg Qt5::Widgets Qt5::Gui Qt5::Quick Qt5::QuickWidgets Qt5::Qml
    muondetector-shared
    pthread
    )
#strip muss im Bundle erfolgen
if (CMAKE_BUILD_TYPE STREQUAL Release)
    add_custom_command(TARGET muondetector-gui POST_BUILD
            COMMAND ${CMAKE_STRIP} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/muondetector-gui.app/Contents/MacOS/muondetector-gui")
endif ()

else()

target_link_libraries(muondetector-gui
    Qt5::Network Qt5::Svg Qt5::Widgets Qt5::Gui Qt5::Quick Qt5::QuickWidgets Qt5::Qml
    muondetector-shared
    pthread
    qwt-qt5
    )
if (CMAKE_BUILD_TYPE STREQUAL Release)
    add_custom_command(TARGET muondetector-gui POST_BUILD
            COMMAND ${CMAKE_STRIP} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/muondetector-gui")
endif ()


install(FILES "${MUONDETECTOR_GUI_CONFIG_DIR}/muon.ico" DESTINATION "share/pixmaps/" COMPONENT gui)
install(FILES "${MUONDETECTOR_GUI_CONFIG_DIR}/muondetector-gui.desktop" DESTINATION "share/applications/" COMPONENT gui)
endif()


install(TARGETS muondetector-gui DESTINATION bin COMPONENT gui)

if(APPLE)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Macdeployqt.cmake")
set(macdeploy_options
    -qmldir="${MUONDETECTOR_GUI_QML_DIR}"    
) # additional options for macdeployqt

macdeployqt(muondetector-gui "${PROJECT_BINARY_DIR}/bin" "${macdeploy_options}")

elseif(WIN32)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Windeployqt.cmake")
set(windeploy_options
    --qmldir "${MUONDETECTOR_GUI_QML_DIR}"
    -opengl
    -printsupport
) # additional options for windeployqt.exe


windeployqt(muondetector-gui "${PROJECT_BINARY_DIR}/bin" "${windeploy_options}")
# create a list of files to copy
set( THIRD_PARTY_DLLS
   "${QWT_DIR}/lib/qwt.dll"
)

# do the copying
foreach( file_i ${THIRD_PARTY_DLLS})
    add_custom_command(
            TARGET muondetector-gui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                    "${file_i}"
                    "${PROJECT_BINARY_DIR}/bin/"
    )
    install(FILES "${file_i}" DESTINATION "bin" COMPONENT gui)
endforeach( file_i )


set(CPACK_GENERATOR "NSIS")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${MUONDETECTOR_GUI_CONFIG_DIR}/description.txt")
set(CPACK_NSIS_MODIFY_PATH ON)
include(InstallRequiredSystemLibraries)

#set(CPACK_PACKAGE_INSTALL_DIRECTORY "CMake ${CMake_VERSION_MAJOR}.${CMake_VERSION_MINOR}")

# There is a bug in NSI that does not handle full UNIX paths properly.
# Make sure there is at least one set of four backlashes.
#set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\muondetector-gui.exe")
set(CPACK_NSIS_DISPLAY_NAME "Muondetector gui")
set(CPACK_NSIS_HELP_LINK "https://muonpi.org")
set(CPACK_NSIS_URL_INFO_ABOUT "https://muonpi.org")
set(CPACK_NSIS_CONTACT "support@muonpi.org")
set(CPACK_NSIS_MUI_ICON "${MUONDETECTOR_GUI_RES_DIR}/res/muon.ico")
set(CPACK_NSIS_MUI_UNIICON "${MUONDETECTOR_GUI_RES_DIR}/res/muon.ico")
set(CPACK_PACKAGE_EXECUTABLES "muondetector-gui" "muondetector-gui")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "GUI for monitoring and controlling the muondetector-daemon.")

else()

include(GNUInstallDirs)

add_custom_target(prep-gui ALL COMMAND mkdir -p "${CMAKE_CURRENT_BINARY_DIR}/gui")
add_custom_target(changelog-gui ALL COMMAND gzip -cn9 "${MUONDETECTOR_GUI_CONFIG_DIR}/changelog" > "${CMAKE_CURRENT_BINARY_DIR}/gui/changelog.gz")
add_dependencies(changelog-gui prep-gui)
add_custom_target(manpage-gui ALL COMMAND gzip -cn9 "${CMAKE_CURRENT_BINARY_DIR}/muondetector-gui.1" > "${CMAKE_CURRENT_BINARY_DIR}/muondetector-gui.1.gz")

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/gui/changelog.gz" DESTINATION "${CMAKE_INSTALL_DOCDIR}-gui" COMPONENT gui)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/muondetector-gui.1.gz" DESTINATION "share/man/man1/" COMPONENT gui)
install(FILES "${MUONDETECTOR_CONFIG_DIR}/copyright" DESTINATION "${CMAKE_INSTALL_DOCDIR}-gui" COMPONENT gui)

if (MUONDETECTOR_BUILD_DAEMON)
set(CPACK_DEBIAN_GUI_PACKAGE_DEPENDS "qml-module-qtpositioning (>=5), qml-module-qtlocation (>=5), qml-module-qtquick2 (>=5), qml-module-qtquick-layouts (>=5), qml-module-qtquick-controls2 (>=5), qml-module-qtquick-controls (>=5), qml-module-qtquick-templates2 (>=5)")
set(CPACK_DEBIAN_GUI_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_GUI_DESCRIPTION " GUI for monitoring and controlling the muondetector-daemon.
 It connects to muondetector-daemon via TCP. It is based on Qt and C++.
 It lets you change the settings for the muondetector hardware and
 uses qml for displaying the current position on the map if connected
 the muondetector-daemon.
 It is licensed under the GNU Lesser General Public License version 3 (LGPL v3).")
set(CPACK_COMPONENT_GUI_DESCRIPTION "${CPACK_DEBIAN_GUI_DESCRIPTION}")
set(CPACK_DEBIAN_GUI_PACKAGE_NAME "muondetector-gui")
else()
set(CPACK_DEBIAN_PACKAGE_DEPENDS "qml-module-qtpositioning (>=5), qml-module-qtlocation (>=5), qml-module-qtquick2 (>=5), qml-module-qtquick-layouts (>=5), qml-module-qtquick-controls2 (>=5), qml-module-qtquick-controls (>=5), qml-module-qtquick-templates2 (>=5)")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION " GUI for monitoring and controlling the muondetector-daemon.
 It connects to muondetector-daemon via TCP. It is based on Qt and C++.
 It lets you change the settings for the muondetector hardware and
 uses qml for displaying the current position on the map if connected
 the muondetector-daemon.
 It is licensed under the GNU Lesser General Public License version 3 (LGPL v3).")
set(CPACK_DEBIAN_PACKAGE_NAME "muondetector-gui")
endif ()

endif()

