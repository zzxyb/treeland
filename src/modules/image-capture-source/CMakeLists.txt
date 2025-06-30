find_package(PkgConfig REQUIRED)
find_package(TreelandProtocols REQUIRED)
set(MODULE_NAME module_imagecapturesource)

pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)

local_qtwayland_server_protocol_treeland(libtreeland
    PROTOCOL ${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-image-capture-source/ext-image-capture-source-v1.xml
    BASENAME ext-image-capture-source-v1
)

impl_treeland(
    NAME
        ${MODULE_NAME}
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/image-capture-source/imagecapturesourcemanagerv1interface.h
        ${CMAKE_SOURCE_DIR}/src/modules/image-capture-source/imagecapturesourcemanagerv1interface.cpp
        ${CMAKE_BINARY_DIR}/src/modules/image-capture-source/wayland-ext-image-capture-source-v1-server-protocol.c
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)
