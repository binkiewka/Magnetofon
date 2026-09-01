cmake_minimum_required(VERSION 3.16)
project(Magnetofon VERSION 2.0.0 LANGUAGES CXX C)


include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Widgets Network)
if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
    pkg_check_modules(PULSE IMPORTED_TARGET libpulse-simple)
    pkg_check_modules(AVFORMAT IMPORTED_TARGET libavformat)
    pkg_check_modules(AVCODEC IMPORTED_TARGET libavcodec)
    pkg_check_modules(AVUTIL IMPORTED_TARGET libavutil)

    set(EXTRA_LIBS
        PkgConfig::MPV
        PkgConfig::AVFORMAT
        PkgConfig::AVCODEC
        PkgConfig::AVUTIL
    )
    if(PULSE_FOUND)
        list(APPEND EXTRA_LIBS PkgConfig::PULSE)
    endif()
elseif(WIN32)
    find_path(MPV_INCLUDE_DIR mpv/client.h PATHS ${MPV_INCLUDE_DIRS})
    find_library(MPV_LIBRARY NAMES mpv mpv-2 libmpv libmpv-2 PATHS ${MPV_LIBRARY_DIRS})

    add_library(MPV_Target UNKNOWN IMPORTED)
    set_target_properties(MPV_Target PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${MPV_INCLUDE_DIR}"
        IMPORTED_LOCATION "${MPV_LIBRARY}"
        IMPORTED_IMPLIB "${MPV_LIBRARY}"
    )
    set(EXTRA_LIBS MPV_Target)
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/models
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
)

set(SOURCES
    main.cpp
    audio/AudioPlayer.cpp
    audio/PulseAudioAnalyzer.cpp
    audio/TrackMetadataReader.cpp
    models/PlaylistModel.cpp
    models/PresetPackDownloader.cpp
    ui/VuMeterItem.cpp
)

set(HEADERS
    audio/AudioPlayer.hpp
    audio/PulseAudioAnalyzer.hpp
    audio/TrackMetadataReader.hpp
    models/PlaylistModel.hpp
    models/PresetPackDownloader.hpp
    ui/VuMeterItem.hpp
)

set(RESOURCES
    qml.qrc
)

add_executable(MagnetofonNative ${SOURCES} ${HEADERS} ${RESOURCES})

target_link_libraries(MagnetofonNative PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    Qt6::Network
    ${EXTRA_LIBS}
)



if(UNIX AND NOT APPLE)
    find_package(X11)
    if(X11_FOUND)
        target_link_libraries(MagnetofonNative PRIVATE X11::X11)
        target_compile_definitions(MagnetofonNative PRIVATE MAGNETOFON_HAVE_X11)
    endif()
endif()

set_target_properties(MagnetofonNative PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

install(TARGETS MagnetofonNative
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
)
install(FILES packaging/magnetofon.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)
install(FILES ../resources/icon.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/1024x1024/apps
    RENAME magnetofon.png
)
install(FILES ../resources/icon-256.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
    RENAME magnetofon.png
)
install(FILES ../resources/icon.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps
    RENAME magnetofon.png
)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)

    add_executable(MagnetofonTests
        tests/AudioPlayerIntegrationTest.cpp
        audio/AudioPlayer.cpp
        audio/PulseAudioAnalyzer.cpp
        audio/TrackMetadataReader.cpp
        models/PlaylistModel.cpp
        audio/AudioPlayer.hpp
        audio/PulseAudioAnalyzer.hpp
        audio/TrackMetadataReader.hpp
        models/PlaylistModel.hpp
    )

    target_include_directories(MagnetofonTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/audio
        ${CMAKE_CURRENT_SOURCE_DIR}/models
    )

    target_link_libraries(MagnetofonTests PRIVATE
        Qt6::Core
        Qt6::Test
        Qt6::Widgets
        ${EXTRA_LIBS}
    )


    add_test(NAME MagnetofonIntegration COMMAND MagnetofonTests)
    set_tests_properties(MagnetofonIntegration PROPERTIES TIMEOUT 20)
endif()

# CPack Packaging Configuration
set(CPACK_PACKAGE_NAME "magnetofon")
set(CPACK_PACKAGE_VENDOR "Magnetofon")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-Fidelity Audio Console (Native C++ / Qt 6 QML)")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "binkiewka <binia@seriouslyclose.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6core6, libqt6gui6, libqt6qml6, libqt6quick6, libpulse0")
include(CPack)
