find_package(PkgConfig REQUIRED)
find_package(TreelandProtocols REQUIRED)
set(MODULE_NAME module_imagecopycapture)

pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)

local_qtwayland_server_protocol_treeland(libtreeland
    PROTOCOL ${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-image-copy-capture/ext-image-copy-capture-v1.xml
    BASENAME ext-image-copy-capture-v1
)

impl_treeland(
    NAME
        ${MODULE_NAME}
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/image-copy-capture/imagecopycapturemanagerinterface.h
        ${CMAKE_SOURCE_DIR}/src/modules/image-copy-capture/imagecopycapturemanagerinterface.cpp
        ${CMAKE_BINARY_DIR}/src/modules/image-copy-capture/wayland-ext-image-copy-capture-v1-server-protocol.c
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)
