set(MODULE_NAME capture)

find_package(TreelandProtocols REQUIRED)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.10)
    find_package(Qt6 REQUIRED COMPONENTS QuickPrivate)
endif()

qt_add_library(${MODULE_NAME} SHARED)

set_target_properties(${MODULE_NAME} PROPERTIES
    OUTPUT_NAME "treeland-protocol-capture-v1"
    VERSION ${CMAKE_PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

local_qtwayland_server_protocol_treeland(${MODULE_NAME}
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-capture-unstable-v1.xml
    BASENAME treeland-capture-unstable-v1
)

qt_add_qml_module(${MODULE_NAME}
    URI Treeland.Capture
    VERSION 1.0
    NO_PLUGIN
    SOURCES
        ${CMAKE_SOURCE_DIR}/src/modules/capture/captureinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/capture/captureinterfacev1.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/wayland-treeland-capture-unstable-v1-server-protocol.c
    RESOURCE_PREFIX
        /qt/qml
    OUTPUT_DIRECTORY
        ${PROJECT_BINARY_DIR}/qt/qml/Treeland/Capture
)

target_compile_definitions(${MODULE_NAME}
    PRIVATE
    WLR_USE_UNSTABLE
)

target_include_directories(${MODULE_NAME}
PRIVATE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

target_link_libraries(${MODULE_NAME}
    PRIVATE
        PkgConfig::WLROOTS
        Waylib::WaylibServer
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::QuickPrivate
)

impl_treeland(
    NAME
        module_capture
    LINK
        ${MODULE_NAME}
)

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
