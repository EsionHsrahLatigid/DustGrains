cmake_minimum_required(VERSION 3.31)

if(APPLE)
    project(DustGrains VERSION 0.2.0 LANGUAGES C CXX OBJC OBJCXX)
else()
    project(DustGrains VERSION 0.2.0 LANGUAGES C CXX)
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
        TARGET_APP_ID "jp.ehl.dustgrains"
        TARGET_APP_NAMESPACE "jp.ehl"
        TARGET_CXX_STANDARD 20
        PLUGIN_ID "jp.ehl.dustgrains"
        PLUGIN_NAME "DustGrains"
        PLUGIN_VENDOR "ehl_"
        PLUGIN_EMAIL ""
        PLUGIN_VERSION "${PROJECT_VERSION}"
        PLUGIN_DESCRIPTION "Stochastic fixed-pool granular fragmentation instrument"
        PLUGIN_URL "https://github.com/EsionHsrahLatigid/DustGrains"
        PLUGIN_IS_SYNTH ON
        PLUGIN_IS_MONO OFF
        PLUGIN_CREATE_CLAP OFF
        PLUGIN_CREATE_VST3 ON
        PLUGIN_CREATE_AU ${DUSTGRAINS_CREATE_AU}
        PLUGIN_AU_SUBTYPE "DsGr"
        PLUGIN_AU_MANUFACTURER "EHL1"
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

    if(DUSTGRAINS_BUILD_TESTS)
        add_executable(dustgrains_plugin_bridge_tests
            tests/DustGrainsPluginBridgeTests.cpp
            source/violent/plugins/DustGrainsPlugin.cpp
            source/violent/plugins/DustGrainsPlugin.h
            source/violent/ProductState.h)
        target_compile_definitions(dustgrains_plugin_bridge_tests PRIVATE DUSTGRAINS_HEADLESS_PLUGIN_TEST=1)
        target_link_libraries(dustgrains_plugin_bridge_tests PRIVATE dustgrains_engine yup::yup_audio_processors)
        target_include_directories(dustgrains_plugin_bridge_tests PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/include"
            "${CMAKE_CURRENT_LIST_DIR}/source")
        target_compile_features(dustgrains_plugin_bridge_tests PRIVATE cxx_std_20)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(dustgrains_plugin_bridge_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
        elseif(MSVC)
            target_compile_options(dustgrains_plugin_bridge_tests PRIVATE /UNDEBUG)
        endif()
        add_test(NAME dustgrains_plugin_bridge_tests COMMAND dustgrains_plugin_bridge_tests)
    endif()

    set(dustgrains_release_bundle_targets
        dustgrains_standalone_plugin
        dustgrains_vst3_plugin)
    if(APPLE)
        list(APPEND dustgrains_release_bundle_targets dustgrains_au_plugin)
    endif()

    add_custom_target(dustgrains_release_bundles
        DEPENDS ${dustgrains_release_bundle_targets})
    add_custom_target(dustgrains_bundles
        DEPENDS dustgrains_release_bundles)

    include(cmake/EhlYupArtifactLayout.cmake)
    ehl_yup_add_artifact_target(
        TARGET ehl_stage_products
        PRODUCT DustGrains
        SLUG dustgrains
        STANDALONE_TARGET dustgrains_standalone_plugin
        VST3_TARGET dustgrains_vst3_plugin
        AU_TARGET dustgrains_au_plugin)

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
