// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "imagecopycapturemanagerinterface.h"
#include "qwayland-server-ext-image-copy-capture-v1.h"
#include "wayland-ext-image-copy-capture-v1-server-protocol.h"

#include <wserver.h>
#include <woutput.h>
#include <wsurface.h>

#include <QDebug>
#include <QRegion>

WAYLIB_SERVER_USE_NAMESPACE

#define EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_VERSION 1

class EXTImageCopyCaptureManagerInterfaceV1Private : public QtWaylandServer::ext_image_copy_capture_manager_v1
{
public:
    explicit EXTImageCopyCaptureManagerInterfaceV1Private(EXTImageCopyCaptureManagerInterfaceV1 *q);

    EXTImageCopyCaptureManagerInterfaceV1 *q;

protected:
    void ext_image_copy_capture_manager_v1_create_session(Resource *resource,
                                                          uint32_t session,
                                                          struct ::wl_resource *source,
                                                          uint32_t options) override;
    void ext_image_copy_capture_manager_v1_create_pointer_cursor_session(Resource *resource,
                                                                         uint32_t session,
                                                                         struct ::wl_resource *source,
                                                                         struct ::wl_resource *pointer) override;
    void ext_image_copy_capture_manager_v1_destroy(Resource *resource) override;
};

EXTImageCopyCaptureManagerInterfaceV1Private::EXTImageCopyCaptureManagerInterfaceV1Private(EXTImageCopyCaptureManagerInterfaceV1 *q)
    : q(q)
{
}

void EXTImageCopyCaptureManagerInterfaceV1Private::ext_image_copy_capture_manager_v1_create_session(
    Resource *resource,
    uint32_t session,
    struct ::wl_resource *source,
    uint32_t options)
{
    // Validate options
    if (options & ~EXTImageCopyCaptureManagerInterfaceV1::PaintCursors) {
        wl_resource_post_error(resource->handle, EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_ERROR_INVALID_OPTION,
                               "invalid options bitfield");
        return;
    }

    wl_resource *session_resource = wl_resource_create(resource->client(),
                                                       &ext_image_copy_capture_session_v1_interface,
                                                       resource->version(),
                                                       session);
    if (!session_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto *sessionInterface = new EXTImageCopyCaptureSessionInterfaceV1(source, options, session_resource);
    Q_EMIT q->sessionCreated(sessionInterface);
}

void EXTImageCopyCaptureManagerInterfaceV1Private::ext_image_copy_capture_manager_v1_create_pointer_cursor_session(
    Resource *resource,
    uint32_t session,
    struct ::wl_resource *source,
    struct ::wl_resource *pointer)
{
    wl_resource *cursor_session_resource = wl_resource_create(resource->client(),
                                                              &ext_image_copy_capture_cursor_session_v1_interface,
                                                              resource->version(),
                                                              session);
    if (!cursor_session_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto *cursorSessionInterface = new EXTImageCopyCaptureCursorSessionInterfaceV1(source, pointer, cursor_session_resource);
    Q_EMIT q->cursorSessionCreated(cursorSessionInterface);
}

void EXTImageCopyCaptureManagerInterfaceV1Private::ext_image_copy_capture_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

// EXTImageCopyCaptureManagerInterfaceV1 implementation
EXTImageCopyCaptureManagerInterfaceV1::EXTImageCopyCaptureManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(std::make_unique<EXTImageCopyCaptureManagerInterfaceV1Private>(this))
{
}

EXTImageCopyCaptureManagerInterfaceV1::~EXTImageCopyCaptureManagerInterfaceV1() = default;

void EXTImageCopyCaptureManagerInterfaceV1::create(WServer *server)
{
    d->init(server->handle()->handle(), EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_VERSION);
}

void EXTImageCopyCaptureManagerInterfaceV1::destroy(WServer *server)
{
    Q_UNUSED(server);
    d.reset();
}

wl_global *EXTImageCopyCaptureManagerInterfaceV1::global() const
{
    return d->global();
}

QByteArrayView EXTImageCopyCaptureManagerInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

// EXTImageCopyCaptureSessionInterfaceV1 implementation
class EXTImageCopyCaptureSessionInterfaceV1Private : public QtWaylandServer::ext_image_copy_capture_session_v1
{
public:
    explicit EXTImageCopyCaptureSessionInterfaceV1Private(EXTImageCopyCaptureSessionInterfaceV1 *q,
                                                          wl_resource *source,
                                                          uint32_t options,
                                                          wl_resource *resource);

    EXTImageCopyCaptureSessionInterfaceV1 *q;
    wl_resource *sourceResource;
    uint32_t captureOptions;

protected:
    void ext_image_copy_capture_session_v1_create_frame(Resource *resource, uint32_t frame) override;
    void ext_image_copy_capture_session_v1_destroy(Resource *resource) override;
};

EXTImageCopyCaptureSessionInterfaceV1Private::EXTImageCopyCaptureSessionInterfaceV1Private(
    EXTImageCopyCaptureSessionInterfaceV1 *q,
    wl_resource *source,
    uint32_t options,
    wl_resource *resource)
    : q(q)
    , sourceResource(source)
    , captureOptions(options)
    , QtWaylandServer::ext_image_copy_capture_session_v1(resource)
{
}

void EXTImageCopyCaptureSessionInterfaceV1Private::ext_image_copy_capture_session_v1_create_frame(
    Resource *resource,
    uint32_t frame)
{
    wl_resource *frame_resource = wl_resource_create(resource->client(),
                                                     &ext_image_copy_capture_frame_v1_interface,
                                                     resource->version(),
                                                     frame);
    if (!frame_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto *frameInterface = new EXTImageCopyCaptureFrameInterfaceV1(frame_resource);
    Q_EMIT q->frameCreated(frameInterface);
}

void EXTImageCopyCaptureSessionInterfaceV1Private::ext_image_copy_capture_session_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

EXTImageCopyCaptureSessionInterfaceV1::EXTImageCopyCaptureSessionInterfaceV1(wl_resource *source, uint32_t options, wl_resource *resource)
    : QObject(nullptr)
    , d(std::make_unique<EXTImageCopyCaptureSessionInterfaceV1Private>(this, source, options, resource))
{
}

EXTImageCopyCaptureSessionInterfaceV1::~EXTImageCopyCaptureSessionInterfaceV1() = default;

wl_resource *EXTImageCopyCaptureSessionInterfaceV1::source() const
{
    return d->sourceResource;
}

uint32_t EXTImageCopyCaptureSessionInterfaceV1::options() const
{
    return d->captureOptions;
}

bool EXTImageCopyCaptureSessionInterfaceV1::paintCursors() const
{
    return d->captureOptions & EXTImageCopyCaptureManagerInterfaceV1::PaintCursors;
}

void EXTImageCopyCaptureSessionInterfaceV1::sendBufferSize(const QSize &size)
{
    d->send_buffer_size(size.width(), size.height());
}

void EXTImageCopyCaptureSessionInterfaceV1::sendShmFormat(uint32_t format)
{
    d->send_shm_format(format);
}

void EXTImageCopyCaptureSessionInterfaceV1::sendDmabufDevice(const QByteArray &device)
{
    struct wl_array device_array;
    wl_array_init(&device_array);
    void *data = wl_array_add(&device_array, device.size());
    memcpy(data, device.constData(), device.size());
    d->send_dmabuf_device(&device_array);
    wl_array_release(&device_array);
}

void EXTImageCopyCaptureSessionInterfaceV1::sendDmabufFormat(uint32_t format, const QByteArray &modifiers)
{
    struct wl_array modifier_array;
    wl_array_init(&modifier_array);
    void *data = wl_array_add(&modifier_array, modifiers.size());
    memcpy(data, modifiers.constData(), modifiers.size());
    d->send_dmabuf_format(format, &modifier_array);
    wl_array_release(&modifier_array);
}

void EXTImageCopyCaptureSessionInterfaceV1::sendDone()
{
    d->send_done();
}

void EXTImageCopyCaptureSessionInterfaceV1::sendStopped()
{
    d->send_stopped();
}

// EXTImageCopyCaptureFrameInterfaceV1 implementation
class EXTImageCopyCaptureFrameInterfaceV1Private : public QtWaylandServer::ext_image_copy_capture_frame_v1
{
public:
    explicit EXTImageCopyCaptureFrameInterfaceV1Private(EXTImageCopyCaptureFrameInterfaceV1 *q, wl_resource *resource);

    EXTImageCopyCaptureFrameInterfaceV1 *q;
    wl_resource *attachedBuffer = nullptr;
    QRegion bufferDamage;

protected:
    void ext_image_copy_capture_frame_v1_destroy(Resource *resource) override;
    void ext_image_copy_capture_frame_v1_attach_buffer(Resource *resource, struct ::wl_resource *buffer) override;
    void ext_image_copy_capture_frame_v1_damage_buffer(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) override;
    void ext_image_copy_capture_frame_v1_capture(Resource *resource) override;
};

EXTImageCopyCaptureFrameInterfaceV1Private::EXTImageCopyCaptureFrameInterfaceV1Private(
    EXTImageCopyCaptureFrameInterfaceV1 *q,
    wl_resource *resource)
    : q(q)
    , QtWaylandServer::ext_image_copy_capture_frame_v1(resource)
{
}

void EXTImageCopyCaptureFrameInterfaceV1Private::ext_image_copy_capture_frame_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void EXTImageCopyCaptureFrameInterfaceV1Private::ext_image_copy_capture_frame_v1_attach_buffer(
    Resource *resource,
    struct ::wl_resource *buffer)
{
    attachedBuffer = buffer;
    Q_EMIT q->bufferAttached(buffer);
}

void EXTImageCopyCaptureFrameInterfaceV1Private::ext_image_copy_capture_frame_v1_damage_buffer(
    Resource *resource,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height)
{
    QRect rect(x, y, width, height);
    bufferDamage += rect;
    Q_EMIT q->damageChanged(bufferDamage);
}

void EXTImageCopyCaptureFrameInterfaceV1Private::ext_image_copy_capture_frame_v1_capture(Resource *resource)
{
    if (!attachedBuffer) {
        wl_resource_post_error(resource->handle, EXT_IMAGE_COPY_CAPTURE_FRAME_V1_ERROR_NO_BUFFER,
                               "no buffer attached");
        return;
    }

    Q_EMIT q->captureRequested();
}

EXTImageCopyCaptureFrameInterfaceV1::EXTImageCopyCaptureFrameInterfaceV1(wl_resource *resource)
    : QObject(nullptr)
    , d(std::make_unique<EXTImageCopyCaptureFrameInterfaceV1Private>(this, resource))
{
}

EXTImageCopyCaptureFrameInterfaceV1::~EXTImageCopyCaptureFrameInterfaceV1() = default;

wl_resource *EXTImageCopyCaptureFrameInterfaceV1::buffer() const
{
    return d->attachedBuffer;
}

QRegion EXTImageCopyCaptureFrameInterfaceV1::damage() const
{
    return d->bufferDamage;
}

void EXTImageCopyCaptureFrameInterfaceV1::sendTransform(uint32_t transform)
{
    d->send_transform(transform);
}

void EXTImageCopyCaptureFrameInterfaceV1::sendDamage(const QRect &damage)
{
    d->send_damage(damage.x(), damage.y(), damage.width(), damage.height());
}

void EXTImageCopyCaptureFrameInterfaceV1::sendPresentationTime(uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec)
{
    d->send_presentation_time(tv_sec_hi, tv_sec_lo, tv_nsec);
}

void EXTImageCopyCaptureFrameInterfaceV1::sendReady()
{
    d->send_ready();
}

void EXTImageCopyCaptureFrameInterfaceV1::sendFailed(FailureReason reason)
{
    d->send_failed(static_cast<uint32_t>(reason));
}

// EXTImageCopyCaptureCursorSessionInterfaceV1 implementation
class EXTImageCopyCaptureCursorSessionInterfaceV1Private : public QtWaylandServer::ext_image_copy_capture_cursor_session_v1
{
public:
    explicit EXTImageCopyCaptureCursorSessionInterfaceV1Private(EXTImageCopyCaptureCursorSessionInterfaceV1 *q,
                                                                wl_resource *source,
                                                                wl_resource *pointer,
                                                                wl_resource *resource);

    EXTImageCopyCaptureCursorSessionInterfaceV1 *q;
    wl_resource *sourceResource;
    wl_resource *pointerResource;

protected:
    void ext_image_copy_capture_cursor_session_v1_destroy(Resource *resource) override;
    void ext_image_copy_capture_cursor_session_v1_get_capture_session(Resource *resource, uint32_t session) override;
};

EXTImageCopyCaptureCursorSessionInterfaceV1Private::EXTImageCopyCaptureCursorSessionInterfaceV1Private(
    EXTImageCopyCaptureCursorSessionInterfaceV1 *q,
    wl_resource *source,
    wl_resource *pointer,
    wl_resource *resource)
    : q(q)
    , sourceResource(source)
    , pointerResource(pointer)
    , QtWaylandServer::ext_image_copy_capture_cursor_session_v1(resource)
{
}

void EXTImageCopyCaptureCursorSessionInterfaceV1Private::ext_image_copy_capture_cursor_session_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void EXTImageCopyCaptureCursorSessionInterfaceV1Private::ext_image_copy_capture_cursor_session_v1_get_capture_session(
    Resource *resource,
    uint32_t session)
{
    wl_resource *session_resource = wl_resource_create(resource->client(),
                                                       &ext_image_copy_capture_session_v1_interface,
                                                       resource->version(),
                                                       session);
    if (!session_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto *sessionInterface = new EXTImageCopyCaptureSessionInterfaceV1(sourceResource, 0, session_resource);
    Q_EMIT q->captureSessionRequested(sessionInterface);
}

EXTImageCopyCaptureCursorSessionInterfaceV1::EXTImageCopyCaptureCursorSessionInterfaceV1(wl_resource *source, wl_resource *pointer, wl_resource *resource)
    : QObject(nullptr)
    , d(std::make_unique<EXTImageCopyCaptureCursorSessionInterfaceV1Private>(this, source, pointer, resource))
{
}

EXTImageCopyCaptureCursorSessionInterfaceV1::~EXTImageCopyCaptureCursorSessionInterfaceV1() = default;

wl_resource *EXTImageCopyCaptureCursorSessionInterfaceV1::source() const
{
    return d->sourceResource;
}

wl_resource *EXTImageCopyCaptureCursorSessionInterfaceV1::pointer() const
{
    return d->pointerResource;
}

void EXTImageCopyCaptureCursorSessionInterfaceV1::sendEnter(uint32_t serial)
{
    d->send_enter(serial);
}

void EXTImageCopyCaptureCursorSessionInterfaceV1::sendLeave(uint32_t serial)
{
    d->send_leave(serial);
}

void EXTImageCopyCaptureCursorSessionInterfaceV1::sendPosition(double x, double y)
{
    d->send_position(wl_fixed_from_double(x), wl_fixed_from_double(y));
}

void EXTImageCopyCaptureCursorSessionInterfaceV1::sendHotspot(int32_t x, int32_t y)
{
    d->send_hotspot(x, y);
}

#include "imagecopycapturemanagerinterface.moc"