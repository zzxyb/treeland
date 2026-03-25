// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsurface.h"
#include "wseat.h"
#include "private/wsurface_p.h"
#include "woutput.h"

#include <QDebug>

extern "C" {
#include <time.h>
#include <wlr/util/edges.h>
#include <wlr/types/wlr_output.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

WSurfacePrivate::WSurfacePrivate(WSurface *qq, wlr_surface *h)
    : q(qq)
    , handle(h)
{
}

WSurfacePrivate::~WSurfacePrivate()
{
    if (handle) {
        handle->data = nullptr;
        wl_list_remove(&destroyListener.link);
        wl_list_remove(&commitListener.link);
        wl_list_remove(&mapListener.link);
        wl_list_remove(&unmapListener.link);
        wl_list_remove(&newSubsurfaceListener.link);
        if (subsurface)
            wl_list_remove(&subsurfaceDestroyListener.link);
    }
    if (buffer)
        wlr_buffer_unlock(buffer);
}

wl_client *WSurfacePrivate::waylandClient() const
{
    if (handle)
        return handle->resource->client;
    return nullptr;
}

void WSurfacePrivate::onDestroy(wl_listener *listener, void *)
{
    WSurfacePrivate *d = containerOf(listener, &WSurfacePrivate::destroyListener);
    wl_list_remove(&d->destroyListener.link);
    wl_list_remove(&d->commitListener.link);
    wl_list_remove(&d->mapListener.link);
    wl_list_remove(&d->unmapListener.link);
    wl_list_remove(&d->newSubsurfaceListener.link);
    if (d->subsurface) {
        wl_list_remove(&d->subsurfaceDestroyListener.link);
        d->subsurface = nullptr;
    }
    if (d->buffer) {
        wlr_buffer_unlock(d->buffer);
        d->buffer = nullptr;
    }
    d->handle = nullptr;
    Q_EMIT d->q->handleDestroyed(d->q);
}

void WSurfacePrivate::onCommit(wl_listener *listener, void *)
{
    WSurfacePrivate *d = containerOf(listener, &WSurfacePrivate::commitListener);
    d->on_commit();
}

void WSurfacePrivate::onMap(wl_listener *listener, void *)
{
    WSurfacePrivate *d = containerOf(listener, &WSurfacePrivate::mapListener);
    Q_EMIT d->q->mappedChanged();
}

void WSurfacePrivate::onUnmap(wl_listener *listener, void *)
{
    WSurfacePrivate *d = containerOf(listener, &WSurfacePrivate::unmapListener);
    Q_EMIT d->q->mappedChanged();
}

void WSurfacePrivate::onNewSubsurface(wl_listener *listener, void *data)
{
    WSurfacePrivate *d = containerOf(listener, &WSurfacePrivate::newSubsurfaceListener);
    auto *sub = static_cast<wlr_subsurface*>(data);
    d->setHasSubsurface(true);
    auto *surface = d->ensureSubsurface(sub);
    Q_EMIT d->q->newSubsurface(surface);
    for (auto output : std::as_const(d->outputs))
        surface->enterOutput(output);
}

void WSurfacePrivate::onSubsurfaceDestroy(wl_listener *listener, void *)
{
    WSurfacePrivate *d = containerOf(listener, &WSurfacePrivate::subsurfaceDestroyListener);
    d->subsurface = nullptr;
    d->isSubsurface = false;
    Q_EMIT d->q->isSubsurfaceChanged();
}

void WSurfacePrivate::on_commit()
{
    needsFrame = !wl_list_empty(&handle->current.frame_callback_list);

    if (handle->current.committed & WLR_SURFACE_STATE_BUFFER)
        updateBuffer();

    if (handle->current.committed & WLR_SURFACE_STATE_OFFSET)
        updateBufferOffset();

    if (hasSubsurface)
        updateHasSubsurface();

    Q_EMIT q->commit(handle->current.committed);
}

void WSurfacePrivate::init()
{
    handle->data = q;

    connect();
    updateBuffer();
    updateHasSubsurface();

    wlr_subsurface *sub = wlr_subsurface_try_from_wlr_surface(handle);
    if (sub)
        setSubsurface(sub);

    wlr_subsurface *subsurface;
    wl_list_for_each(subsurface, &handle->current.subsurfaces_below, current.link) {
        Q_EMIT q->newSubsurface(ensureSubsurface(subsurface));
    }
    wl_list_for_each(subsurface, &handle->current.subsurfaces_above, current.link) {
        Q_EMIT q->newSubsurface(ensureSubsurface(subsurface));
    }
}

void WSurfacePrivate::connect()
{
    destroyListener.notify = onDestroy;
    wl_signal_add(&handle->events.destroy, &destroyListener);

    commitListener.notify = onCommit;
    wl_signal_add(&handle->events.commit, &commitListener);

    mapListener.notify = onMap;
    wl_signal_add(&handle->events.map, &mapListener);

    unmapListener.notify = onUnmap;
    wl_signal_add(&handle->events.unmap, &unmapListener);

    newSubsurfaceListener.notify = onNewSubsurface;
    wl_signal_add(&handle->events.new_subsurface, &newSubsurfaceListener);
}

void WSurfacePrivate::updateOutputs()
{
    outputs.clear();
    framePacingOutput = nullptr;
    wlr_surface_output *output;
    wl_list_for_each(output, &handle->current_outputs, link) {
        auto qo = output->output;
        if (!qo)
            continue;
        auto o = WOutput::fromHandle(qo);
        if (!o)
            continue;
        outputs << o;

        if (!framePacingOutput || framePacingOutput->handle()->refresh < qo->refresh) {
            framePacingOutput = o;
        }
    }

    updatePreferredBufferScale();
}

void WSurfacePrivate::setBuffer(wlr_buffer *newBuffer)
{
    if (buffer) {
        auto *clientBuffer = wlr_client_buffer_get(buffer);
        if (clientBuffer) {
            Q_ASSERT(clientBuffer->n_ignore_locks > 0);
            clientBuffer->n_ignore_locks--;
        }
        wlr_buffer_unlock(buffer);
    }

    buffer = newBuffer;

    if (buffer) {
        auto *clientBuffer = wlr_client_buffer_get(buffer);
        if (clientBuffer)
            clientBuffer->n_ignore_locks++;
        wlr_buffer_lock(buffer);
    }
}

void WSurfacePrivate::updateBuffer()
{
    wlr_buffer *newBuffer = handle->buffer ? &handle->buffer->base : nullptr;
    setBuffer(newBuffer);
}

void WSurfacePrivate::updateBufferOffset()
{
    auto dBufferOffset = QPoint(handle->current.dx, handle->current.dy);
    if (!dBufferOffset.isNull()) {
        bufferOffset += dBufferOffset;
        Q_EMIT q->bufferOffsetChanged();
    }
}

void WSurfacePrivate::updatePreferredBufferScale()
{
    if (explicitPreferredBufferScale > 0)
        return;

    float maxScale = 1.0;
    for (auto o : std::as_const(outputs))
        maxScale = std::max(o->scale(), maxScale);
    if (handle)
        wlr_fractional_scale_v1_notify_scale(handle, maxScale);

    preferredBufferScale = qCeil(maxScale);
    preferredBufferScaleChange();
}

void WSurfacePrivate::preferredBufferScaleChange()
{
    if (handle)
        wlr_surface_set_preferred_buffer_scale(handle, q->preferredBufferScale());
    Q_EMIT q->preferredBufferScaleChanged();
}

WSurface *WSurfacePrivate::ensureSubsurface(wlr_subsurface *subsurface)
{
    if (auto *surface = WSurface::fromHandle(subsurface->surface))
        return surface;

    auto *surface = new WSurface(subsurface->surface, q);
    QObject::connect(surface, &WSurface::handleDestroyed, surface, &WSurface::deleteLater);
    return surface;
}

void WSurfacePrivate::setSubsurface(wlr_subsurface *newSubsurface)
{
    if (subsurface == newSubsurface)
        return;
    subsurface = newSubsurface;

    subsurfaceDestroyListener.notify = onSubsurfaceDestroy;
    wl_signal_add(&subsurface->events.destroy, &subsurfaceDestroyListener);

    isSubsurface = true;
    Q_EMIT q->isSubsurfaceChanged();
}

void WSurfacePrivate::setHasSubsurface(bool newHasSubsurface)
{
    if (hasSubsurface == newHasSubsurface)
        return;
    hasSubsurface = newHasSubsurface;
    Q_EMIT q->hasSubsurfaceChanged();
}

void WSurfacePrivate::updateHasSubsurface()
{
    setHasSubsurface(handle && (!wl_list_empty(&handle->current.subsurfaces_above)
                              || !wl_list_empty(&handle->current.subsurfaces_below)));
}

WSurface::WSurface(wlr_surface *handle, QObject *parent)
    : QObject(parent)
    , d(new WSurfacePrivate(this, handle))
{
    d->init();
}

WSurface::~WSurface()
{
    // d->~WSurfacePrivate() handles cleanup
}

wlr_surface *WSurface::handle() const
{
    return d->handle;
}

WSurface *WSurface::fromHandle(wlr_surface *handle)
{
    return static_cast<WSurface*>(handle->data);
}

bool WSurface::inputRegionContains(const QPointF &localPos) const
{
    return wlr_surface_point_accepts_input(d->handle, localPos.x(), localPos.y());
}

bool WSurface::mapped() const
{
    return d->handle && d->handle->mapped;
}

QSize WSurface::size() const
{
    return QSize(d->handle->current.width, d->handle->current.height);
}

QSize WSurface::bufferSize() const
{
    return QSize(d->handle->current.buffer_width, d->handle->current.buffer_height);
}

WLR::Transform WSurface::orientation() const
{
    return static_cast<WLR::Transform>(d->handle->current.transform);
}

int WSurface::bufferScale() const
{
    return d->handle->current.scale;
}

QPoint WSurface::bufferOffset() const
{
    return d->bufferOffset;
}

wlr_buffer *WSurface::buffer() const
{
    return d->buffer;
}

void WSurface::notifyFrameDone()
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_surface_send_frame_done(d->handle, &now);
}

void WSurface::enterOutput(WOutput *output)
{
    if (d->outputs.contains(output))
        return;
    wlr_surface_send_enter(d->handle, output->handle());

    d->updateOutputs();

    auto surface = d->handle;
    wlr_subsurface *subsurface;
    wl_list_for_each(subsurface, &surface->current.subsurfaces_below, current.link) {
        d->ensureSubsurface(subsurface)->enterOutput(output);
    }
    wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link) {
        d->ensureSubsurface(subsurface)->enterOutput(output);
    }

    Q_EMIT outputEntered(output);
}

void WSurface::leaveOutput(WOutput *output)
{
    if (!d->outputs.contains(output))
        return;
    wlr_surface_send_leave(d->handle, output->handle());

    d->updateOutputs();

    auto surface = d->handle;
    wlr_subsurface *subsurface;
    wl_list_for_each(subsurface, &surface->current.subsurfaces_below, current.link) {
        d->ensureSubsurface(subsurface)->leaveOutput(output);
    }
    wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link) {
        d->ensureSubsurface(subsurface)->leaveOutput(output);
    }

    Q_EMIT outputLeave(output);
}

const QList<WOutput *> &WSurface::outputs() const
{
    return d->outputs;
}

WOutput *WSurface::framePacingOutput() const
{
    return d->framePacingOutput;
}

bool WSurface::isSubsurface() const
{
    return d->isSubsurface;
}

bool WSurface::hasSubsurface() const
{
    return d->hasSubsurface;
}

QList<WSurface*> WSurface::subsurfaces() const
{
    QList<WSurface*> subsurfaceList;
    wlr_subsurface *subsurface;
    wl_list_for_each(subsurface, &d->handle->current.subsurfaces_below, current.link) {
        subsurfaceList.append(d->ensureSubsurface(subsurface));
    }
    wl_list_for_each(subsurface, &d->handle->current.subsurfaces_above, current.link) {
        subsurfaceList.append(d->ensureSubsurface(subsurface));
    }
    return subsurfaceList;
}

uint32_t WSurface::preferredBufferScale() const
{
    return d->explicitPreferredBufferScale > 0 ? d->explicitPreferredBufferScale : d->preferredBufferScale;
}

void WSurface::setPreferredBufferScale(uint32_t newPreferredBufferScale)
{
    if (d->explicitPreferredBufferScale == newPreferredBufferScale)
        return;
    const auto oldScale = preferredBufferScale();
    d->explicitPreferredBufferScale = newPreferredBufferScale;
    if (d->explicitPreferredBufferScale == 0)
        d->updatePreferredBufferScale();

    if (oldScale != preferredBufferScale())
        d->preferredBufferScaleChange();
}

void WSurface::resetPreferredBufferScale()
{
    setPreferredBufferScale(0);
}

void WSurface::map()
{
    wlr_surface_map(d->handle);
}

void WSurface::unmap()
{
    wlr_surface_unmap(d->handle);
}

bool WSurface::needsFrame() const
{
    return d->needsFrame;
}

bool WSurface::scheduleFrameIfNeeded()
{
    if (needsFrame() && d->framePacingOutput) {
        d->needsFrame = false;
        wlr_output_schedule_frame(d->framePacingOutput->handle());
        return true;
    }
    return false;
}

WAYLIB_SERVER_END_NAMESPACE




{
    W_DC(WSurface);
    return static_cast<WLR::Transform>(d->nativeHandle()->current.transform);
}

int WSurface::bufferScale() const
{
    W_DC(WSurface);
    return d->nativeHandle()->current.scale;
}

QPoint WSurface::bufferOffset() const
{
    W_DC(WSurface);
    return d->bufferOffset;
}

qw_buffer *WSurface::buffer() const
{
    W_DC(WSurface);
    return d->buffer.get();
}

void WSurface::notifyFrameDone()
{
    W_D(WSurface);
    /* This lets the client know that we've displayed that frame and it can
    * prepare another one now if it likes. */
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_surface_send_frame_done(d->nativeHandle(), &now);
}

void WSurface::enterOutput(WOutput *output)
{
    W_D(WSurface);
    if (d->outputs.contains(output))
        return;
    wlr_surface_send_enter(d->nativeHandle(), output->handle());

    // connect(output, &WOutput::aboutToBeInvalidated, this, [this, output] {
    //     leaveOutput(output);
    // });
    // output->safeConnect(&WOutput::scaleChanged, this, [d] {
    //     d->updatePreferredBufferScale();
    // });

    d->updateOutputs();

    // for subsurface
    auto surface = d->nativeHandle();
    wlr_subsurface *subsurface;
    wl_list_for_each(subsurface, &surface->current.subsurfaces_below, current.link) {
        d->ensureSubsurface(subsurface)->enterOutput(output);
    }

    wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link) {
        d->ensureSubsurface(subsurface)->enterOutput(output);
    }

    Q_EMIT outputEntered(output);
}

void WSurface::leaveOutput(WOutput *output)
{
    W_D(WSurface);
    if (!d->outputs.contains(output))
        return;
    wlr_surface_send_leave(d->nativeHandle(), output->handle());

    // output->safeDisconnect(this);
    d->updateOutputs();

    // for subsurface
    auto surface = d->nativeHandle();
    wlr_subsurface *subsurface;
    wl_list_for_each(subsurface, &surface->current.subsurfaces_below, current.link) {
        d->ensureSubsurface(subsurface)->leaveOutput(output);
    }

    wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link) {
        d->ensureSubsurface(subsurface)->leaveOutput(output);
    }

    Q_EMIT outputLeave(output);
}

const QList<WOutput *> &WSurface::outputs() const
{
    W_DC(WSurface);
    return d->outputs;
}

WOutput *WSurface::framePacingOutput() const
{
    W_DC(WSurface);
    return d->framePacingOutput;
}

bool WSurface::isSubsurface() const
{
    W_DC(WSurface);
    return d->isSubsurface;
}

bool WSurface::hasSubsurface() const
{
    W_DC(WSurface);
    return d->hasSubsurface;
}

QList<WSurface*> WSurface::subsurfaces() const
{
    auto d = const_cast<WSurface*>(this)->d_func();
    QList<WSurface*> subsurfaeList;

    auto surface = d->nativeHandle();
    wlr_subsurface *subsurface;
    wl_list_for_each(subsurface, &surface->current.subsurfaces_below, current.link) {
        subsurfaeList.append(d->ensureSubsurface(subsurface));
    }

    wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link) {
        subsurfaeList.append(d->ensureSubsurface(subsurface));
    }

    return subsurfaeList;
}

uint32_t WSurface::preferredBufferScale() const
{
    W_DC(WSurface);
    return d->explicitPreferredBufferScale > 0 ? d->explicitPreferredBufferScale : d->preferredBufferScale;
}

void WSurface::setPreferredBufferScale(uint32_t newPreferredBufferScale)
{
    W_D(WSurface);
    if (d->explicitPreferredBufferScale == newPreferredBufferScale)
        return;
    const auto oldScale = preferredBufferScale();
    d->explicitPreferredBufferScale = newPreferredBufferScale;
    if (d->explicitPreferredBufferScale == 0)
        d->updatePreferredBufferScale();

    if (oldScale != preferredBufferScale()) {
        d->preferredBufferScaleChange();
    }
}

void WSurface::resetPreferredBufferScale()
{
    setPreferredBufferScale(0);
}

void WSurface::map()
{
    W_D(WSurface);
    wlr_surface_map(d->nativeHandle());
}

void WSurface::unmap()
{
    W_D(WSurface);
    wlr_surface_unmap(d->nativeHandle());
}

void WSurfacePrivate::instantRelease()
{
    W_Q(WSurface);
    if (handle()) {
        handle()->set_data(nullptr, nullptr);
        handle()->disconnect(q);
        if (subsurface)
            subsurface->disconnect(q);
        for (auto o : std::as_const(outputs))
            o->safeDisconnect(q);
    }
}

