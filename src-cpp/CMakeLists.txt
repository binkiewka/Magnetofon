cmake_minimum_required(VERSION 3.16)
project(Magnetofon VERSION 2.2.0 LANGUAGES CXX C)


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
    pkg_check_modules(PULSE IMPORTED_TARGET libpulse)
    pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
    pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
    pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil)

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
    find_path(MPV_INCLUDE_DIR mpv/client.h PATHS ${MPV_INCLUDE_DIRS} ${MPV_INCLUDE_DIRS}/include)
    find_library(MPV_LIBRARY NAMES mpv mpv-2 libmpv libmpv-2 libmpv.dll.a PATHS ${MPV_LIBRARY_DIRS} ${MPV_LIBRARY_DIRS}/lib)


    add_library(MPV_Target UNKNOWN IMPORTED)
    set_target_properties(MPV_Target PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${MPV_INCLUDE_DIR}"
        IMPORTED_LOCATION "${MPV_LIBRARY}"
        IMPORTED_IMPLIB "${MPV_LIBRARY}"
    )
    set(EXTRA_LIBS MPV_Target)
endif()

option(MAGNETOFON_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
add_library(MagnetofonBuildOptions INTERFACE)
if(MSVC)
    # The portable capture/DSP sources use the conventional math constants;
    # MSVC requires this definition before its standard math headers are read.
    target_compile_definitions(MagnetofonBuildOptions INTERFACE _USE_MATH_DEFINES)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(MagnetofonBuildOptions INTERFACE -Wall -Wextra -Wpedantic)
    if(MAGNETOFON_ENABLE_SANITIZERS)
        target_compile_options(MagnetofonBuildOptions INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(MagnetofonBuildOptions INTERFACE -fsanitize=address,undefined)
    endif()
endif()
if(PULSE_FOUND)
    target_compile_definitions(MagnetofonBuildOptions INTERFACE MAGNETOFON_HAVE_PULSE)
endif()

set(SOURCES
    main.cpp
    audio/AudioPlayer.cpp
    audio/AudioRouting.cpp
    audio/PulseAudioAnalyzer.cpp
    audio/TrackMetadataReader.cpp
    models/PlaylistModel.cpp
    models/PresetPackDownloader.cpp
    models/VisualizerLauncher.cpp
    ui/VideoWindow.cpp
    ui/VuMeterItem.cpp
)

set(HEADERS
    audio/AudioPlayer.hpp
    audio/AudioRouting.hpp
    audio/PulseAudioAnalyzer.hpp
    audio/TrackMetadataReader.hpp
    models/PlaylistModel.hpp
    models/PresetPackDownloader.hpp
    models/VisualizerLauncher.hpp
    ui/VideoWindow.hpp
    ui/VuMeterItem.hpp
)


set(RESOURCES
    qml.qrc
)

add_executable(MagnetofonNative ${SOURCES} ${HEADERS} ${RESOURCES})
if(WIN32)
    configure_file(packaging/version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/magnetofon-version.rc @ONLY)
    target_sources(MagnetofonNative PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/magnetofon-version.rc)
endif()

target_include_directories(MagnetofonNative PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/models
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
)

target_compile_definitions(MagnetofonNative PRIVATE
    MAGNETOFON_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(MagnetofonNative PRIVATE
    MagnetofonBuildOptions
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
install(FILES packaging/magnetofon-deb.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    RENAME magnetofon.desktop
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
install(FILES ../LICENSE ../THIRD-PARTY.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/magnetofon
)
install(DIRECTORY ../resources/licenses/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/magnetofon/licenses
)
install(FILES ../resources/mpv/NOTICE.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/magnetofon
    RENAME mpv-NOTICE.md
)

if(UNIX AND NOT APPLE)
    install(DIRECTORY ../resources/projectm/linux-x64/
        DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/magnetofon/projectm
        USE_SOURCE_PERMISSIONS
        PATTERN "include" EXCLUDE
        PATTERN "cmake" EXCLUDE
        PATTERN "pkgconfig" EXCLUDE
        PATTERN "applications" EXCLUDE
        PATTERN "icons" EXCLUDE
    )
    install(DIRECTORY ../visuals/curated/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/magnetofon/visuals/curated
        PATTERN "*.zip" EXCLUDE
    )
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)

    add_executable(MagnetofonTests
        tests/AudioPlayerIntegrationTest.cpp
        ui/VuMeterItem.cpp
        ui/VuMeterItem.hpp
        qml.qrc
        audio/AudioPlayer.cpp
        audio/AudioRouting.cpp
        audio/PulseAudioAnalyzer.cpp
        audio/TrackMetadataReader.cpp
        models/PlaylistModel.cpp
        models/PresetPackDownloader.cpp
        models/VisualizerLauncher.cpp
        ui/VideoWindow.cpp
        audio/AudioPlayer.hpp
        audio/AudioRouting.hpp
        audio/PulseAudioAnalyzer.hpp
        audio/TrackMetadataReader.hpp
        models/PlaylistModel.hpp
        models/PresetPackDownloader.hpp
        models/VisualizerLauncher.hpp
        ui/VideoWindow.hpp
    )

    target_compile_definitions(MagnetofonTests PRIVATE MAGNETOFON_TESTING)
    target_include_directories(MagnetofonTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/audio
        ${CMAKE_CURRENT_SOURCE_DIR}/models
        ${CMAKE_CURRENT_SOURCE_DIR}/ui
    )

    target_link_libraries(MagnetofonTests PRIVATE
        MagnetofonBuildOptions
        Qt6::Core
        Qt6::Test
        Qt6::Widgets
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Network
        ${EXTRA_LIBS}
    )


    if(UNIX AND NOT APPLE)
        pkg_check_modules(AVFILTER QUIET IMPORTED_TARGET libavfilter)
        if(AVFILTER_FOUND AND AVUTIL_FOUND)
            add_executable(MagnetofonDspTests tests/AudioRoutingDspTest.cpp audio/AudioRouting.cpp)
            target_include_directories(MagnetofonDspTests PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}
                ${CMAKE_CURRENT_SOURCE_DIR}/audio
            )
            target_link_libraries(MagnetofonDspTests PRIVATE MagnetofonBuildOptions Qt6::Core Qt6::Test PkgConfig::AVFILTER PkgConfig::AVUTIL)
            add_test(NAME MagnetofonDsp COMMAND MagnetofonDspTests)
            set_tests_properties(MagnetofonDsp PROPERTIES TIMEOUT 20)
        endif()
    endif()

    add_test(NAME MagnetofonIntegration COMMAND MagnetofonTests)
    set_tests_properties(MagnetofonIntegration PROPERTIES TIMEOUT 90
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")

    add_executable(MagnetofonAnalyzerTests tests/PulseAudioAnalyzerTest.cpp audio/PulseAudioAnalyzer.cpp)
    target_include_directories(MagnetofonAnalyzerTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/audio)
    target_link_libraries(MagnetofonAnalyzerTests PRIVATE MagnetofonBuildOptions Qt6::Core Qt6::Test Qt6::Network)
    if(PULSE_FOUND)
        target_link_libraries(MagnetofonAnalyzerTests PRIVATE PkgConfig::PULSE)
    endif()
    if(PULSE_FOUND AND UNIX)
        add_executable(MagnetofonAudioIsolationTests tests/PulseAudioIsolationTest.cpp audio/PulseAudioAnalyzer.cpp)
        target_include_directories(MagnetofonAudioIsolationTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/audio)
        target_link_libraries(MagnetofonAudioIsolationTests PRIVATE MagnetofonBuildOptions Qt6::Core Qt6::Test PkgConfig::PULSE)
        add_test(NAME MagnetofonAudioIsolation COMMAND MagnetofonAudioIsolationTests)
        set_tests_properties(MagnetofonAudioIsolation PROPERTIES TIMEOUT 15)
    endif()

    add_test(NAME MagnetofonAnalyzer COMMAND MagnetofonAnalyzerTests)
    set_tests_properties(MagnetofonAnalyzer PROPERTIES TIMEOUT 15)

    add_executable(MagnetofonPlaylistTests tests/PlaylistModelTest.cpp
        models/PlaylistModel.cpp audio/TrackMetadataReader.cpp)
    target_include_directories(MagnetofonPlaylistTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/models ${CMAKE_CURRENT_SOURCE_DIR}/audio)
    target_link_libraries(MagnetofonPlaylistTests PRIVATE MagnetofonBuildOptions Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Test ${EXTRA_LIBS})
    add_test(NAME MagnetofonPlaylist COMMAND MagnetofonPlaylistTests)
    set_tests_properties(MagnetofonPlaylist PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(MagnetofonPresetTests tests/PresetPackDownloaderTest.cpp models/PresetPackDownloader.cpp)
    target_include_directories(MagnetofonPresetTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/models)
    target_link_libraries(MagnetofonPresetTests PRIVATE MagnetofonBuildOptions Qt6::Core Qt6::Network Qt6::Test)
    add_test(NAME MagnetofonPresets COMMAND MagnetofonPresetTests)
    set_tests_properties(MagnetofonPresets PROPERTIES TIMEOUT 45)

    add_executable(MagnetofonVisualizerTests tests/VisualizerLauncherTest.cpp
        models/VisualizerLauncher.cpp models/PresetPackDownloader.cpp)
    target_include_directories(MagnetofonVisualizerTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/models)
    target_link_libraries(MagnetofonVisualizerTests PRIVATE MagnetofonBuildOptions Qt6::Core Qt6::Network Qt6::Test)
    add_test(NAME MagnetofonVisualizer COMMAND MagnetofonVisualizerTests)
    set_tests_properties(MagnetofonVisualizer PROPERTIES TIMEOUT 30)

    find_program(QML_TEST_RUNNER qmltestrunner HINTS /usr/lib/qt6/bin)
    if(QML_TEST_RUNNER)
        add_test(NAME MagnetofonQml COMMAND ${QML_TEST_RUNNER} -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/qml)
        set_tests_properties(MagnetofonQml PROPERTIES TIMEOUT 30
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    endif()
endif()

# CPack Packaging Configuration
set(CPACK_PACKAGE_NAME "magnetofon")
set(CPACK_PACKAGE_VENDOR "Magnetofon")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-Fidelity Audio Console (Native C++ / Qt 6 QML)")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "binkiewka <binia@seriouslyclose.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/../resources/projectm/linux-x64/lib"
)
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "qml6-module-qtqml, qml6-module-qtqml-workerscript, qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qtquick-templates, qml6-module-qtquick-window, unzip"
)
include(CPack)
