#-----------------------------------------------------------------------------
# SpaceDub VST3
#
# A port of the 2003 Cakewalk DXi "SpaceDub" to a VST3 audio effect for macOS.
#
# Quick start:
#     ./setup-xcode.sh
#     open build/SpaceDub.xcodeproj
#
# Or by hand:
#     cmake -B build -G Xcode
#     cmake --build build --config Release
#
# The Steinberg VST3 SDK is fetched automatically on the first configure.
# To use an SDK you already have:
#     cmake -B build -G Xcode -DVST3_SDK_ROOT=/path/to/vst3sdk
#
# On macOS with the Xcode generator an Audio Unit (AUv2) wrapper is built
# alongside the VST3. Turn it off with -DSPACEDUB_BUILD_AU=OFF.
#-----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.25.0)

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.13 CACHE STRING "macOS deployment target")

# Build a universal binary. Without this the Xcode generator produces whatever
# the building Mac happens to be - arm64, in practice - while
# installer/distribution.xml advertises the installer as x86_64,arm64. The two
# disagreeing is a silent failure: on an Intel Mac, or in any host running
# under Rosetta, the package installs perfectly and nothing loads.
#
# Check what actually came out before publishing:
#     lipo -archs build/VST3/Release/SpaceDub.vst3/Contents/MacOS/SpaceDub
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")

# The single source of truth for the version. installer/build-installer.sh
# greps this exact line out of this file, so the installer and the bundles can
# never disagree - keep the set(...) spelling even though project() could carry
# the number on its own.
set(PLUGIN_VERSION "2.0.0.1")

project(SpaceDub
    VERSION ${PLUGIN_VERSION}
    DESCRIPTION "SpaceDub - stereo multi-tap delay with ladder filter"
)

#-----------------------------------------------------------------------------
# Locate (or fetch) the VST3 SDK
#-----------------------------------------------------------------------------
set(SPACEDUB_SDK_TAG "v3.8.1_build_84" CACHE STRING "VST3 SDK git tag to fetch")
set(VST3_SDK_ROOT "" CACHE PATH "Path to an existing Steinberg VST3 SDK checkout")

# -Dvst3sdk_SOURCE_DIR=... is the spelling Steinberg's own tutorials use;
# accept it as well as our own -DVST3_SDK_ROOT=...
if(vst3sdk_SOURCE_DIR AND NOT VST3_SDK_ROOT)
    set(VST3_SDK_ROOT "${vst3sdk_SOURCE_DIR}")
endif()

if(NOT VST3_SDK_ROOT)
    set(_bundled_sdk "${CMAKE_CURRENT_SOURCE_DIR}/external/vst3sdk")
    if(NOT EXISTS "${_bundled_sdk}/CMakeLists.txt")
        message(STATUS "SpaceDub: VST3 SDK not found - cloning ${SPACEDUB_SDK_TAG} into external/vst3sdk (this takes a few minutes)")
        find_package(Git REQUIRED)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} clone --depth 1 --recurse-submodules --shallow-submodules
                    --branch ${SPACEDUB_SDK_TAG}
                    https://github.com/steinbergmedia/vst3sdk.git
                    "${_bundled_sdk}"
            RESULT_VARIABLE _clone_result
        )
        if(NOT _clone_result EQUAL 0)
            message(FATAL_ERROR
                "SpaceDub: could not clone the VST3 SDK.\n"
                "Clone it yourself and re-run cmake with -DVST3_SDK_ROOT=/path/to/vst3sdk")
        endif()
    endif()
    set(VST3_SDK_ROOT "${_bundled_sdk}" CACHE PATH "" FORCE)
endif()

if(NOT EXISTS "${VST3_SDK_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "SpaceDub: VST3_SDK_ROOT is not a VST3 SDK: ${VST3_SDK_ROOT}")
endif()

set(vst3sdk_SOURCE_DIR "${VST3_SDK_ROOT}")

# The SDK appends this to CMAKE_MODULE_PATH in its own CMakeLists, but that is
# a child scope: the append does not reach back up here. Without it,
# include(SMTG_AddVST3AuV2) below fails with "include could not find requested
# file".
list(APPEND CMAKE_MODULE_PATH "${VST3_SDK_ROOT}/cmake/modules")

#-----------------------------------------------------------------------------
# Audio Unit (AUv2) wrapper
#
# Steinberg's wrapper loads the VST3 inside an .component bundle, so there is
# no second implementation to maintain - but it needs Apple's AudioUnitSDK,
# and the SDK only defines the AUv2 helper under the Xcode generator. Both are
# resolved here, before the SDK subdirectory is added, because that is where
# setupCoreAudioSupport() reads SMTG_AUDIOUNIT_SDK_PATH.
#-----------------------------------------------------------------------------
option(SPACEDUB_BUILD_AU "Also build an Audio Unit (AUv2) wrapper - macOS + Xcode only" ON)

# Which AudioUnitSDK to fetch.
#
# This is pinned deliberately. Steinberg's smtg_target_add_auv2 compiles the
# wrapper with target_compile_features(cxx_std_17), and Apple moved on:
#
#   1.0.0, 1.1.0   C++17          <- matches the wrapper as shipped
#   1.2.0          C++20 project, but headers still C++17-clean
#   1.3.0, 1.4.0   C++20 headers  <- std::span, concepts, requires-clauses
#
# Building 1.3.0+ against the C++17 wrapper fails in AUUtility.h with
# "no member named 'span' in namespace 'std'". 1.1.0 is the newest release
# whose own build standard matches what the wrapper compiles at, so the static
# library and our translation units agree.
#
# If you do point this at a newer one, the C++ standard of the SpaceDub-au
# target is raised automatically - see the detection further down.
set(SPACEDUB_AU_SDK_TAG "AudioUnitSDK-1.1.0" CACHE STRING "Apple AudioUnitSDK git tag to fetch")

set(SPACEDUB_AU_ENABLED OFF)
if(SPACEDUB_BUILD_AU)
    if(NOT APPLE)
        message(STATUS "SpaceDub: not macOS - skipping the Audio Unit target")
    elseif(NOT CMAKE_GENERATOR STREQUAL "Xcode")
        message(STATUS "SpaceDub: the Audio Unit target needs the Xcode generator - skipping it "
                       "(configure with -G Xcode to get one)")
    elseif(SMTG_AUDIOUNIT_SDK_PATH AND EXISTS "${SMTG_AUDIOUNIT_SDK_PATH}/AudioUnitSDK.xcodeproj")
        # Somebody has pointed us at their own checkout; leave it alone.
        message(STATUS "SpaceDub: using the AudioUnitSDK at ${SMTG_AUDIOUNIT_SDK_PATH}")
        set(SPACEDUB_AU_ENABLED ON)
    else()
        find_package(Git REQUIRED)
        set(_au_sdk "${CMAKE_CURRENT_SOURCE_DIR}/external/AudioUnitSDK")

        # A checkout left over from a different pin would silently be the wrong
        # version, which is exactly how the C++17/C++20 mismatch above bites.
        if(EXISTS "${_au_sdk}/AudioUnitSDK.xcodeproj")
            execute_process(
                COMMAND ${GIT_EXECUTABLE} -C "${_au_sdk}" describe --tags --exact-match
                OUTPUT_VARIABLE _au_have
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
            if(NOT _au_have STREQUAL "${SPACEDUB_AU_SDK_TAG}")
                message(STATUS "SpaceDub: external/AudioUnitSDK is '${_au_have}' but "
                               "'${SPACEDUB_AU_SDK_TAG}' is wanted - refetching")
                file(REMOVE_RECURSE "${_au_sdk}")
            endif()
        endif()

        if(NOT EXISTS "${_au_sdk}/AudioUnitSDK.xcodeproj")
            message(STATUS "SpaceDub: fetching Apple's AudioUnitSDK (${SPACEDUB_AU_SDK_TAG}) into external/AudioUnitSDK")
            execute_process(
                COMMAND ${GIT_EXECUTABLE} clone --depth 1 --branch ${SPACEDUB_AU_SDK_TAG}
                        https://github.com/apple/AudioUnitSDK.git "${_au_sdk}"
                RESULT_VARIABLE _au_clone_result
            )
        else()
            set(_au_clone_result 0)
        endif()

        if(_au_clone_result EQUAL 0 AND EXISTS "${_au_sdk}/AudioUnitSDK.xcodeproj")
            # Picked up by the SDK's setupCoreAudioSupport(), which then turns
            # SMTG_ENABLE_AUV2_BUILDS on for us.
            set(SMTG_AUDIOUNIT_SDK_PATH "${_au_sdk}" CACHE PATH "" FORCE)
            set(SPACEDUB_AU_ENABLED ON)
        else()
            # Never fatal: a missing AudioUnitSDK must not stop the VST3 build.
            message(WARNING
                "SpaceDub: could not fetch Apple's AudioUnitSDK, so the Audio Unit target is "
                "skipped. The VST3 build is unaffected. Clone it yourself into "
                "external/AudioUnitSDK, or configure with -DSPACEDUB_BUILD_AU=OFF.")
        endif()
    endif()
endif()

# Does this AudioUnitSDK need C++20? Detected from the headers rather than from
# the tag, so a hand-placed checkout is handled too.
set(SPACEDUB_AU_NEEDS_CXX20 OFF)
if(SPACEDUB_AU_ENABLED)
    set(_au_util "${SMTG_AUDIOUNIT_SDK_PATH}/include/AudioUnitSDK/AUUtility.h")
    if(EXISTS "${_au_util}")
        file(STRINGS "${_au_util}" _au_cxx20_hits REGEX "std::span|^concept |^[ \t]*requires ")
        if(_au_cxx20_hits)
            set(SPACEDUB_AU_NEEDS_CXX20 ON)
        endif()
    endif()
endif()

#-----------------------------------------------------------------------------
# Code signing
#
# The SDK defaults SMTG_CODE_SIGN_IDENTITY_MAC to "Apple Development" and its
# Audio Unit post-build step then runs
#
#     /usr/bin/codesign -v -s "Apple Development" --force --timestamp <bundle>
#
# unconditionally. On a machine with no developer certificate that fails with
# "Apple Development: no identity found" and takes the whole build down - after
# everything has already compiled and installed.
#
# So unless a real identity is given, the SDK's signing is switched off and the
# bundles are ad-hoc signed below by a step we control. Ad-hoc is what a local
# development build wants anyway, and it is enough for macOS to load the
# plug-in on Apple Silicon. Note that --timestamp is deliberately not passed:
# an ad-hoc signature cannot be timestamped.
#
# To sign properly, pass your identity:
#     cmake -B build -G Xcode -DSPACEDUB_CODE_SIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
#-----------------------------------------------------------------------------
set(SPACEDUB_CODE_SIGN_IDENTITY "" CACHE STRING
    "macOS signing identity. Empty means ad-hoc signing, which needs no certificate.")

set(SPACEDUB_ADHOC_SIGN OFF)
if(APPLE)
    if(SPACEDUB_CODE_SIGN_IDENTITY)
        set(SMTG_CODE_SIGN_IDENTITY_MAC "${SPACEDUB_CODE_SIGN_IDENTITY}" CACHE STRING "" FORCE)
        message(STATUS "SpaceDub: signing with '${SPACEDUB_CODE_SIGN_IDENTITY}'")
    else()
        set(SMTG_DISABLE_CODE_SIGNING ON CACHE BOOL "" FORCE)
        set(SPACEDUB_ADHOC_SIGN ON)
        message(STATUS "SpaceDub: no signing identity given - bundles will be ad-hoc signed")
    endif()
endif()

# Only build what this plug-in needs.
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES 0 CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES  0 CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT        1 CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK           1 CACHE BOOL "" FORCE)

set(SMTG_VSTGUI_ROOT "${VST3_SDK_ROOT}")

#-----------------------------------------------------------------------------
# Keep absolute build paths out of the shipped binary.
#
# VSTGUI's assert macros embed __FILE__, so without this a Release build of the
# plug-in carries ~50 absolute paths naming the home directory it was built in -
# public the moment an installer is published.
#
# Two things matter here. -fdebug-prefix-map does NOT do this: it rewrites debug
# info and leaves __FILE__ alone. And the paths are in VSTGUI's translation
# units rather than ours, so this has to be add_compile_options() BEFORE
# add_subdirectory() - target_compile_options() on the SpaceDub target would
# strip almost nothing.
#
# Guarded because an unknown flag is a hard error on an older toolchain, and
# this is cosmetic. Verify with:
#     strings -a build/VST3/Release/SpaceDub.vst3/Contents/MacOS/SpaceDub \
#         | grep -c "$HOME"        # want 0
# It only takes effect on a fresh configure, so re-run setup-xcode.sh --clean.
#-----------------------------------------------------------------------------
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-ffile-prefix-map=/a=b" SPACEDUB_HAVE_FILE_PREFIX_MAP)
if(SPACEDUB_HAVE_FILE_PREFIX_MAP)
    add_compile_options(
        "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=SpaceDub"
        "-ffile-prefix-map=${VST3_SDK_ROOT}=vst3sdk"
        "-ffile-prefix-map=${CMAKE_BINARY_DIR}=build")
else()
    message(STATUS "SpaceDub: this compiler has no -ffile-prefix-map, so the "
                   "Release binary will carry absolute build paths")
endif()

add_subdirectory(${VST3_SDK_ROOT} ${PROJECT_BINARY_DIR}/vst3sdk)
smtg_enable_vst3_sdk()

#-----------------------------------------------------------------------------
# The plug-in target
#-----------------------------------------------------------------------------
set(spacedub_sources
    source/version.h
    source/SpaceDubIDs.h
    source/SpaceDubParams.h
    source/SpaceDubParams.cpp
    source/DelayLine.h
    source/DelayLine.cpp
    source/SpaceDubProcessor.h
    source/SpaceDubProcessor.cpp
    source/SpaceDubController.h
    source/SpaceDubController.cpp
    source/SpaceDubControls.h
    source/SpaceDubControls.cpp
    source/SpaceDubEditor.h
    source/SpaceDubEditor.cpp
    source/SpaceDubEntry.cpp
    PORTING-NOTES.md
    README.md
)

smtg_add_vst3plugin(SpaceDub ${spacedub_sources})
smtg_target_configure_version_file(SpaceDub)

target_compile_features(SpaceDub PUBLIC cxx_std_17)
target_include_directories(SpaceDub PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")

target_link_libraries(SpaceDub
    PRIVATE
        sdk
        vstgui_support
)

smtg_target_add_plugin_resources(SpaceDub
    RESOURCES
        resource/background.png
        resource/handle_v.png
        resource/handle_h.png
        resource/groove_v.png
        resource/groove_h.png
        resource/button_small.png
        resource/button_filter.png
        resource/button_power.png
        resource/logo.png
        resource/knob.png
        resource/scale.png
        resource/dot.png
)

if(SMTG_MAC)
    smtg_target_set_bundle(SpaceDub
        BUNDLE_IDENTIFIER "audio.spacedub.vst3"
        COMPANY_NAME "A. E. Cobley"
    )
    smtg_target_set_debug_executable(SpaceDub
        "/Applications/VST3PluginTestHost.app"
        "--pluginfolder;$(BUILT_PRODUCTS_DIR)"
    )
    if(SPACEDUB_ADHOC_SIGN)
        add_custom_command(TARGET SpaceDub POST_BUILD
            COMMAND /usr/bin/codesign --force --sign -
                    "${CMAKE_BINARY_DIR}/VST3/$<CONFIG>/SpaceDub.vst3"
            COMMENT "Ad-hoc signing SpaceDub.vst3"
            VERBATIM
        )
    endif()
elseif(SMTG_WIN)
    if(MSVC)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT SpaceDub)
    endif()
endif(SMTG_MAC)

#-----------------------------------------------------------------------------
# The Audio Unit target
#
# smtg_target_add_auv2 builds the wrapper, symlinks the VST3 into
# SpaceDub.component/Contents/Resources/plugin.vst3, and copies the bundle to
# ~/Library/Audio/Plug-Ins/Components/ after every build.
#
# The four-character codes that identify the plug-in to macOS live in
# resource/au-info.plist, not here.
#-----------------------------------------------------------------------------
if(SPACEDUB_AU_ENABLED AND SMTG_ENABLE_AUV2_BUILDS)
    include(SMTG_AddVST3AuV2)

    # The helper only defines itself under the Xcode generator. Check rather
    # than assume, so a surprise here is a warning about the Audio Unit and not
    # a hard configure failure that also costs you the VST3.
    if(COMMAND smtg_target_add_auv2)
        smtg_target_add_auv2(SpaceDub-au
            BUNDLE_NAME SpaceDub
            BUNDLE_IDENTIFIER audio.spacedub.audiounit
            INFO_PLIST_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/resource/au-info.plist
            VST3_PLUGIN_TARGET SpaceDub
        )
        # Steinberg's helper pins the wrapper to C++17. Newer AudioUnitSDK
        # headers need more than that, so lift it when the detection above
        # found C++20 constructs.
        if(SPACEDUB_AU_NEEDS_CXX20)
            message(STATUS "SpaceDub: this AudioUnitSDK needs C++20 - raising the standard on SpaceDub-au")
            target_compile_features(SpaceDub-au PRIVATE cxx_std_20)
            set_target_properties(SpaceDub-au PROPERTIES
                CXX_STANDARD 20
                CXX_STANDARD_REQUIRED ON
                XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++20"
            )
        endif()

        # Runs after the helper's own post-build steps, so the copy in
        # ~/Library/Audio/Plug-Ins/Components has already been made.
        if(SPACEDUB_ADHOC_SIGN)
            add_custom_command(TARGET SpaceDub-au POST_BUILD
                COMMAND /usr/bin/codesign --force --sign -
                        "${CMAKE_BINARY_DIR}/VST3/$<CONFIG>/SpaceDub.component"
                COMMAND /usr/bin/codesign --force --sign -
                        "$ENV{HOME}/Library/Audio/Plug-Ins/Components/SpaceDub.component"
                COMMENT "Ad-hoc signing SpaceDub.component"
                VERBATIM
            )
        endif()

        message(STATUS "SpaceDub: Audio Unit target enabled (SpaceDub.component)")
    else()
        message(WARNING
            "SpaceDub: SMTG_AddVST3AuV2 did not define smtg_target_add_auv2, so the Audio Unit "
            "target is skipped. The VST3 build is unaffected.")
    endif()
elseif(SPACEDUB_AU_ENABLED)
    message(WARNING "SpaceDub: the VST3 SDK did not enable AUv2 builds - no Audio Unit target")
endif()

message(STATUS "SpaceDub: using VST3 SDK at ${VST3_SDK_ROOT}")
