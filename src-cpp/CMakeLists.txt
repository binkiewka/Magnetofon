cmake_minimum_required(VERSION 3.16)
project(Magnetofon VERSION 1.0.2 LANGUAGES CXX C)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Widgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
pkg_check_modules(PULSE REQUIRED IMPORTED_TARGET libpulse-simple)
pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil)

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
    ui/VuMeterItem.cpp
)

set(HEADERS
    audio/AudioPlayer.hpp
    audio/PulseAudioAnalyzer.hpp
    audio/TrackMetadataReader.hpp
    models/PlaylistModel.hpp
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
    PkgConfig::MPV
    PkgConfig::PULSE
    PkgConfig::AVFORMAT
    PkgConfig::AVCODEC
    PkgConfig::AVUTIL
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
        PkgConfig::MPV
        PkgConfig::PULSE
        PkgConfig::AVFORMAT
        PkgConfig::AVCODEC
        PkgConfig::AVUTIL
    )

    add_test(NAME MagnetofonIntegration COMMAND MagnetofonTests)
    set_tests_properties(MagnetofonIntegration PROPERTIES TIMEOUT 20)
endif()
