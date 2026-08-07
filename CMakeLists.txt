cmake_minimum_required(VERSION 3.31)

if(APPLE)
    project(DustGrains VERSION 0.1.1 LANGUAGES C CXX OBJC OBJCXX)
else()
    project(DustGrains VERSION 0.1.1 LANGUAGES C CXX)
endif()

option(DUSTGRAINS_BUILD_PLUGIN "Build the YUP Standalone/VST3/AU targets" ON)
option(DUSTGRAINS_BUILD_TESTS "Build deterministic DSP regression tests" ON)
option(DUSTGRAINS_ADHOC_SIGN "Ad-hoc sign local macOS plugin and app bundles after building" ON)
set(DUSTGRAINS_YUP_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../yup" CACHE PATH
    "Path to a local YUP checkout; fetched from upstream when unavailable")

add_library(dustgrains_engine STATIC
    source/violent/DustGrainsEngine.cpp)
target_include_directories(dustgrains_engine PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include")
target_compile_features(dustgrains_engine PUBLIC cxx_std_20)
set_target_properties(dustgrains_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(dustgrains_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(DUSTGRAINS_BUILD_TESTS)
    include(CTest)

    add_executable(dustgrains_engine_tests
        tests/DustGrainsEngineTests.cpp)
    target_link_libraries(dustgrains_engine_tests PRIVATE dustgrains_engine)
    target_compile_features(dustgrains_engine_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(dustgrains_engine_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    elseif(MSVC)
        target_compile_options(dustgrains_engine_tests PRIVATE /UNDEBUG)
    endif()
    add_test(NAME dustgrains_engine_tests COMMAND dustgrains_engine_tests)
endif()

if(DUSTGRAINS_BUILD_PLUGIN)
    set(DUSTGRAINS_CREATE_AU OFF)
    if(APPLE)
        set(DUSTGRAINS_CREATE_AU ON)
    endif()

    set(YUP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_PLUGINS OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_PLUGINVAL OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_VST3_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_CLAP_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_AUVAL_VALIDATOR OFF CACHE BOOL "" FORCE)

    if(EXISTS "${DUSTGRAINS_YUP_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory("${DUSTGRAINS_YUP_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/yup")
    else()
        include(FetchContent)
        FetchContent_Declare(yup
            GIT_REPOSITORY https://github.com/kunitoki/yup.git
            GIT_TAG 9a1c9bc699b6a714f6f52486462d98a140c8bf95)
        FetchContent_MakeAvailable(yup)
    endif()

    if(APPLE)
        include(cmake/YupMacOSIconWorkaround.cmake)
    endif()

    yup_audio_plugin(
        TARGET_NAME dustgrains
        TARGET_VERSION ${PROJECT_VERSION}
        TARGET_IDE_GROUP "DustGrains"
        TARGET_APP_ID "audio.2bit.dustgrains"
        TARGET_APP_NAMESPACE "audio.2bit"
        TARGET_CXX_STANDARD 20
        PLUGIN_ID "audio.2bit.DustGrains"
        PLUGIN_NAME "DustGrains"
        PLUGIN_VENDOR "2bit"
        PLUGIN_EMAIL ""
        PLUGIN_VERSION "${PROJECT_VERSION}"
        PLUGIN_DESCRIPTION "Stochastic fixed-pool granular fragmentation instrument"
        PLUGIN_URL "https://github.com/2bbb/DustGrains"
        PLUGIN_IS_SYNTH ON
        PLUGIN_IS_MONO OFF
        PLUGIN_CREATE_CLAP OFF
        PLUGIN_CREATE_VST3 ON
        PLUGIN_CREATE_AU ${DUSTGRAINS_CREATE_AU}
        PLUGIN_AU_SUBTYPE "DsGr"
        PLUGIN_AU_MANUFACTURER "2Bit"
        PLUGIN_AU_SANDBOX_SAFE ON
        PLUGIN_CREATE_AUv3 OFF
        PLUGIN_CREATE_LV2 OFF
        PLUGIN_CREATE_AAX OFF
        PLUGIN_CREATE_STANDALONE ON
        PLUGIN_COPY_AFTER_BUILD OFF
        MODULES
            yup::yup_gui
            yup::yup_audio_processors)

    target_sources(dustgrains_shared INTERFACE
        source/violent/ParameterGridEditor.cpp
        source/violent/ParameterGridEditor.h
        source/violent/ProductState.h
        source/violent/plugins/DustGrainsPlugin.cpp
        source/violent/plugins/DustGrainsPlugin.h)
    target_link_libraries(dustgrains_shared INTERFACE dustgrains_engine)
    target_include_directories(dustgrains_shared INTERFACE
        "${CMAKE_CURRENT_LIST_DIR}/include"
        "${CMAKE_CURRENT_LIST_DIR}/source")

    set(DUSTGRAINS_BUNDLE_TARGETS
        dustgrains_standalone_plugin
        dustgrains_vst3_plugin)
    if(APPLE)
        list(APPEND DUSTGRAINS_BUNDLE_TARGETS dustgrains_au_plugin)
    endif()
    add_custom_target(dustgrains_bundles DEPENDS ${DUSTGRAINS_BUNDLE_TARGETS})

    if(APPLE AND DUSTGRAINS_ADHOC_SIGN)
        find_program(DUSTGRAINS_CODESIGN_EXECUTABLE codesign REQUIRED)
        add_custom_command(TARGET dustgrains_vst3_plugin POST_BUILD
            COMMAND "${DUSTGRAINS_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:dustgrains_vst3_plugin>"
            COMMENT "Ad-hoc signing DustGrains VST3 for local loading"
            VERBATIM)
        add_custom_command(TARGET dustgrains_standalone_plugin POST_BUILD
            COMMAND "${DUSTGRAINS_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:dustgrains_standalone_plugin>"
            COMMENT "Ad-hoc signing DustGrains standalone app for local launch"
            VERBATIM)
    endif()
endif()
