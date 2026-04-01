find_package(TreelandProtocols REQUIRED)

local_qtwayland_server_protocol_treeland(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-foreign-toplevel-manager-v1.xml
    BASENAME treeland-foreign-toplevel-manager-v1
)

impl_treeland(
    NAME
        module_foreigntoplevel
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/foreign-toplevel/foreigntoplevelmanagerv1.h
        # ${CMAKE_SOURCE_DIR}/src/modules/foreign-toplevel/impl/foreign_toplevel_manager_impl.h
        ${CMAKE_SOURCE_DIR}/src/modules/foreign-toplevel/foreigntoplevelmanagerv1.cpp
        # ${CMAKE_SOURCE_DIR}/src/modules/foreign-toplevel/impl/foreign_toplevel_manager_impl.cpp
        # ${WAYLAND_PROTOCOLS_OUTPUTDIR}/treeland-foreign-toplevel-manager-protocol.c
        # ${CMAKE_BINARY_DIR}/src/modules/foreigntoplevel/wayland-treeland-foreign-toplevel-manager-v1-server-protocol.c
    INCLUDE
        $<BUILD_INTERFACE:${WAYLAND_PROTOCOLS_OUTPUTDIR}>
)
