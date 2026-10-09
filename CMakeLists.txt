# CMake build for the shipped apps: iOS (TestFlight), Android (built by
# android/'s Gradle project, which points here), and a standalone Windows
# .exe (plus plain macOS/Linux desktop builds). The Visual Studio solution
# is still there for day-to-day Windows development, and the web build
# keeps using web/build.sh -- neither reads this.
#
# iOS (on a Mac with Xcode):
#   cmake -S . -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS \
#         -DIOS_BUNDLE_ID=com.example.underwayblackjack -DIOS_TEAM_ID=ABCDE12345
#   open build-ios/UnderwayBlackjack.xcodeproj
#
# Windows (Visual Studio 2022 installed):
#   cmake -S . -B build-win
#   cmake --build build-win --config Release
#   -> build-win/Release/UnderwayBlackjack.exe (+ the PNGs next to it)
#
# See ios/README.md and BUILDING.md for the CI builds.
cmake_minimum_required(VERSION 3.22)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "")
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")
endif()

project(UnderwayBlackjack LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Shown in TestFlight as "Version (Build)". CI sets APP_VERSION from the
# latest vX.Y release tag; the build number just has to go up on every
# upload (CI passes the GitHub Actions run number).
set(APP_VERSION "1.0" CACHE STRING "User-facing version (CFBundleShortVersionString)")
set(APP_BUILD_NUMBER "1" CACHE STRING "Build number (CFBundleVersion)")
set(IOS_BUNDLE_ID "com.youseeimnewhere.underwayblackjack" CACHE STRING "iOS bundle identifier")
# The version shown on the main menu, and used by the Windows update check
# (UpdateCheck.h). The release workflows set it from the tag (v1.1 -> 1.1):
# Windows and Android from the release's own tag, iOS from the latest one.
# "dev" (the default) shows nothing and never checks.
set(GAME_VERSION "dev" CACHE STRING "Game version for the in-game update check")
set(IOS_TEAM_ID "" CACHE STRING "Apple Developer Team ID used for signing")

# ---- SDL3 + SDL3_image, same versions as the Windows build ----------------
include(FetchContent)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # The game never uses a camera or game controllers. Leaving these out
    # keeps SDL's camera (AVFoundation) and Bluetooth controller
    # (CoreBluetooth) code out of the iOS binary -- App Store Connect
    # rejects builds that reference those APIs without an Info.plist
    # purpose string (ITMS-90683). iOS only: SDL's Android Java side calls
    # into the HIDAPI native code unconditionally, so turning it off there
    # would crash on launch.
    set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
    set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)
endif()

# Windows: link the C/C++ runtime statically (SDL included), so the .exe
# runs on a PC without the Visual C++ Redistributable installed.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.16
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(SDL3)

# Only PNGs are ever loaded; skip every format that needs an extra library.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_VENDORED OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TESTS OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_JXL OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_WEBP OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG release-3.4.6
    GIT_SHALLOW TRUE
    # PNG is decoded by ImageIO on iOS (stb elsewhere), so none of the
    # vendored codec submodules are needed.
    GIT_SUBMODULES "")
FetchContent_MakeAvailable(SDL3_image)

# ---- Game --------------------------------------------------------------------
set(GAME_SOURCES
    BettingSquare.cpp
    Card.cpp
    Game.cpp
    Hand.cpp
    mina.cpp
    Person.cpp
    Table.cpp)

set(GAME_ASSETS
    ${CMAKE_CURRENT_SOURCE_DIR}/Cards.png
    ${CMAKE_CURRENT_SOURCE_DIR}/Table5Player.png
    ${CMAKE_CURRENT_SOURCE_DIR}/Arrow.png
    ${CMAKE_CURRENT_SOURCE_DIR}/Chips.png)
# Sound effects (tools/make_sounds.py) ship next to the PNGs, flat, the
# same as on every other platform -- Audio.h looks them up by bare name.
file(GLOB GAME_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/sounds/*.wav)
list(APPEND GAME_ASSETS ${GAME_SOUNDS})

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # ios/Orientation.m turns the screen for the portrait training drills.
    # Plain Objective-C: enabling OBJCXX makes CMake build SDL_image's .m
    # files as Objective-C++, which they don't compile as.
    enable_language(OBJC)
    list(APPEND GAME_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/ios/Orientation.m)
    set(IOS_RESOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/ios/Assets.xcassets
        ${CMAKE_CURRENT_SOURCE_DIR}/ios/LaunchScreen.storyboard)

    add_executable(UnderwayBlackjack MACOSX_BUNDLE ${GAME_SOURCES} ${GAME_ASSETS} ${IOS_RESOURCES})

    # Assets go in the root of the .app bundle; SDL resolves the relative
    # paths in Game.h ("Cards.png", ...) against the bundle's resource
    # directory on Apple platforms.
    set_source_files_properties(${GAME_ASSETS} ${IOS_RESOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    set_target_properties(UnderwayBlackjack PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist.in
        RESOURCE "${GAME_ASSETS};${IOS_RESOURCES}"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${IOS_BUNDLE_ID}"
        XCODE_ATTRIBUTE_PRODUCT_NAME "UnderwayBlackjack"
        XCODE_ATTRIBUTE_MARKETING_VERSION "${APP_VERSION}"
        XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION "${APP_BUILD_NUMBER}"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${IOS_TEAM_ID}"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
        XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
        XCODE_ATTRIBUTE_SKIP_INSTALL "NO"
        XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)"
        XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
        XCODE_ATTRIBUTE_GENERATE_INFOPLIST_FILE "NO")
elseif(ANDROID)
    # SDL's Java activity (android/) loads the game as libmain.so and calls
    # its SDL_main. The PNGs are packed into the APK's assets by Gradle,
    # where SDL's file loading finds relative paths on its own.
    add_library(UnderwayBlackjack SHARED ${GAME_SOURCES})
    set_target_properties(UnderwayBlackjack PROPERTIES OUTPUT_NAME main)
else()
    if(WIN32)
        # WIN32: a normal windowed app, no console window behind it. The .rc
        # gives the .exe its icon in Explorer and the taskbar.
        add_executable(UnderwayBlackjack WIN32 ${GAME_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/windows/UnderwayBlackjack.rc)
    else()
        add_executable(UnderwayBlackjack ${GAME_SOURCES})
    endif()

    # Desktop: copy the PNGs next to the executable and run from there.
    foreach(asset ${GAME_ASSETS})
        add_custom_command(TARGET UnderwayBlackjack POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${asset} $<TARGET_FILE_DIR:UnderwayBlackjack>)
    endforeach()
endif()

target_link_libraries(UnderwayBlackjack PRIVATE SDL3_image::SDL3_image SDL3::SDL3)
target_compile_definitions(UnderwayBlackjack PRIVATE GAME_VERSION_STRING="${GAME_VERSION}")
if(WIN32)
    target_link_libraries(UnderwayBlackjack PRIVATE winhttp)
endif()
