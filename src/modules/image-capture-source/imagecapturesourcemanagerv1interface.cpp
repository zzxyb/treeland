// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "imagecapturesourcemanagerv1interface.h"
#include "qwayland-server-ext-image-capture-source-v1.h"
#include "wayland-ext-image-capture-source-v1-server-protocol.h"

#include <wserver.h>
#include <woutput.h>
#include <wsurface.h>

#include <QDebug>

WAYLIB_SERVER_USE_NAMESPACE

#define EXT_IMAGE_CAPTURE_SOURCE_V1_VERSION 1
#define EXT_OUTPUT_IMAGE_CAPTURE_SOURCE_MANAGER_V1_VERSION 1
#define EXT_FOREIGN_TOPLEVEL_IMAGE_CAPTURE_SOURCE_MANAGER_V1_VERSION 1

// EXTImageCaptureSourceInterfaceV1 Private Implementation
class EXTImageCaptureSourceInterfaceV1Private : public QtWaylandServer::ext_image_capture_source_v1
{
public:
    explicit EXTImageCaptureSourceInterfaceV1Private(EXTImageCaptureSourceInterfaceV1 *q, wl_resource *resource);

    EXTImageCaptureSourceInterfaceV1 *q;

protected:
    void ext_image_capture_source_v1_destroy(Resource *resource) override;
};

EXTImageCaptureSourceInterfaceV1Private::EXTImageCaptureSourceInterfaceV1Private(
    EXTImageCaptureSourceInterfaceV1 *q, wl_resource *resource)
    : q(q)
    , QtWaylandServer::ext_image_capture_source_v1(resource)
{
}

void EXTImageCaptureSourceInterfaceV1Private::ext_image_capture_source_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

// EXTImageCaptureSourceInterfaceV1 Implementation
EXTImageCaptureSourceInterfaceV1::EXTImageCaptureSourceInterfaceV1(wl_resource *resource)
    : QObject(nullptr)
    , d(std::make_unique<EXTImageCaptureSourceInterfaceV1Private>(this, resource))
{
}

EXTImageCaptureSourceInterfaceV1::~EXTImageCaptureSourceInterfaceV1() = default;

wl_resource *EXTImageCaptureSourceInterfaceV1::resource() const
{
    return d->resource()->handle;
}

// EXTOutputImageCaptureSourceManagerInterfaceV1 Private Implementation
class EXTOutputImageCaptureSourceManagerInterfaceV1Private : public QtWaylandServer::ext_output_image_capture_source_manager_v1
{
public:
    explicit EXTOutputImageCaptureSourceManagerInterfaceV1Private(EXTOutputImageCaptureSourceManagerInterfaceV1 *q);

    EXTOutputImageCaptureSourceManagerInterfaceV1 *q;

protected:
    void ext_output_image_capture_source_manager_v1_create_source(Resource *resource,
                                                                  uint32_t source,
                                                                  struct ::wl_resource *output) override;
    void ext_output_image_capture_source_manager_v1_destroy(Resource *resource) override;
};

EXTOutputImageCaptureSourceManagerInterfaceV1Private::EXTOutputImageCaptureSourceManagerInterfaceV1Private(EXTOutputImageCaptureSourceManagerInterfaceV1 *q)
    : q(q)
{
}

void EXTOutputImageCaptureSourceManagerInterfaceV1Private::ext_output_image_capture_source_manager_v1_create_source(
    Resource *resource,
    uint32_t source,
    struct ::wl_resource *output)
{
    wl_resource *source_resource = wl_resource_create(resource->client(),
                                                      &ext_image_capture_source_v1_interface,
                                                      EXT_IMAGE_CAPTURE_SOURCE_V1_VERSION,
                                                      source);
    if (!source_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto *sourceInterface = new EXTImageCaptureSourceInterfaceV1(source_resource);
    Q_EMIT q->sourceCreated(sourceInterface, output);
}

void EXTOutputImageCaptureSourceManagerInterfaceV1Private::ext_output_image_capture_source_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

// EXTOutputImageCaptureSourceManagerInterfaceV1 Implementation
EXTOutputImageCaptureSourceManagerInterfaceV1::EXTOutputImageCaptureSourceManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(std::make_unique<EXTOutputImageCaptureSourceManagerInterfaceV1Private>(this))
{
}

EXTOutputImageCaptureSourceManagerInterfaceV1::~EXTOutputImageCaptureSourceManagerInterfaceV1() = default;

void EXTOutputImageCaptureSourceManagerInterfaceV1::create(WServer *server)
{
    d->init(server->handle()->handle(), EXT_OUTPUT_IMAGE_CAPTURE_SOURCE_MANAGER_V1_VERSION);
}

void EXTOutputImageCaptureSourceManagerInterfaceV1::destroy(WServer *server)
{
    Q_UNUSED(server);
    d.reset();
}

wl_global *EXTOutputImageCaptureSourceManagerInterfaceV1::global() const
{
    return d->global();
}

QByteArrayView EXTOutputImageCaptureSourceManagerInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

// EXTForeignToplevelImageCaptureSourceManagerInterfaceV1 Private Implementation
class EXTForeignToplevelImageCaptureSourceManagerInterfaceV1Private : public QtWaylandServer::ext_foreign_toplevel_image_capture_source_manager_v1
{
public:
    explicit EXTForeignToplevelImageCaptureSourceManagerInterfaceV1Private(EXTForeignToplevelImageCaptureSourceManagerInterfaceV1 *q);

    EXTForeignToplevelImageCaptureSourceManagerInterfaceV1 *q;

protected:
    void ext_foreign_toplevel_image_capture_source_manager_v1_create_source(Resource *resource,
                                                                            uint32_t source,
                                                                            struct ::wl_resource *toplevel_handle) override;
    void ext_foreign_toplevel_image_capture_source_manager_v1_destroy(Resource *resource) override;
};

EXTForeignToplevelImageCaptureSourceManagerInterfaceV1Private::EXTForeignToplevelImageCaptureSourceManagerInterfaceV1Private(EXTForeignToplevelImageCaptureSourceManagerInterfaceV1 *q)
    : q(q)
{
}

void EXTForeignToplevelImageCaptureSourceManagerInterfaceV1Private::ext_foreign_toplevel_image_capture_source_manager_v1_create_source(
    Resource *resource,
    uint32_t source,
    struct ::wl_resource *toplevel_handle)
{
    wl_resource *source_resource = wl_resource_create(resource->client(),
                                                      &ext_image_capture_source_v1_interface,
                                                      EXT_IMAGE_CAPTURE_SOURCE_V1_VERSION,
                                                      source);
    if (!source_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto *sourceInterface = new EXTImageCaptureSourceInterfaceV1(source_resource);
    Q_EMIT q->sourceCreated(sourceInterface, toplevel_handle);
}

void EXTForeignToplevelImageCaptureSourceManagerInterfaceV1Private::ext_foreign_toplevel_image_capture_source_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

// EXTForeignToplevelImageCaptureSourceManagerInterfaceV1 Implementation
EXTForeignToplevelImageCaptureSourceManagerInterfaceV1::EXTForeignToplevelImageCaptureSourceManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(std::make_unique<EXTForeignToplevelImageCaptureSourceManagerInterfaceV1Private>(this))
{
}

EXTForeignToplevelImageCaptureSourceManagerInterfaceV1::~EXTForeignToplevelImageCaptureSourceManagerInterfaceV1() = default;

void EXTForeignToplevelImageCaptureSourceManagerInterfaceV1::create(WServer *server)
{
    d->init(server->handle()->handle(), EXT_FOREIGN_TOPLEVEL_IMAGE_CAPTURE_SOURCE_MANAGER_V1_VERSION);
}

void EXTForeignToplevelImageCaptureSourceManagerInterfaceV1::destroy(WServer *server)
{
    Q_UNUSED(server);
    d.reset();
}

wl_global *EXTForeignToplevelImageCaptureSourceManagerInterfaceV1::global() const
{
    return d->global();
}

QByteArrayView EXTForeignToplevelImageCaptureSourceManagerInterfaceV1::interfaceName() const
{
    return d->interfaceName();
} 