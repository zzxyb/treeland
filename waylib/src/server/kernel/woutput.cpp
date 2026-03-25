// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutput.h"
#include "wbackend.h"
#include "wcursor.h"
#include "wseat.h"
#include "wtools.h"
#include "platformplugin/qwlrootscreen.h"
#include "private/wglobal_p.h"
#include "wcontainerof.h"

extern "C" {
#include <math.h>
#define static
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/render/interface.h>
#undef static
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/allocator.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/interfaces/wlr_output.h>
}
extern "C" {
#define slots slots_c
#include <wlr/render/swapchain.h>
#undef slots
}

#include <QLoggingCategory>
#include <QCoreApplication>
#include <QQuickWindow>
#include <QCursor>

#include <xf86drm.h>
#include <drm_fourcc.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

// Output management and configuration
Q_LOGGING_CATEGORY(waylibOutput, "waylib.server.output", QtInfoMsg)
// Hardware-specific output operations
Q_LOGGING_CATEGORY(waylibOutputHW, "waylib.server.output.hardware", QtInfoMsg)
// Buffer and swapchain management
Q_LOGGING_CATEGORY(waylibOutputBuffer, "waylib.server.output.buffer", QtDebugMsg)

class Q_DECL_HIDDEN WOutputPrivate
{
public:
    WOutputPrivate(WOutput *qq, wlr_output *handle)
        : q(qq)
        , handle(handle)
    {
        handle->data = qq;
    }

    inline QSize size() const {
        return QSize(handle->width, handle->height);
    }

    inline WOutput::Transform orientation() const {
        return static_cast<WOutput::Transform>(handle->transform);
    }

    static void onNotifyCommitCallback(wl_listener *listener, void *data);
    static void onDestroyCallback(wl_listener *listener, void *data);

    wlr_output *handle = nullptr;;
    WOutput *q = nullptr;
    bool forceSoftwareCursor = false;
    QWlrootsScreen *screen = nullptr;
    QQuickWindow *window = nullptr;

    WOutputLayout *layout = nullptr;

    wl_listener notifyCommitListener;
    wl_listener destoryListener;
};

WOutput::WOutput(wlr_output *handle, WBackend *backend, QObject *parent)
    : QObject(parent)
    , d(new WOutputPrivate(this, handle))
{
    d->notifyCommitListener.notify = WOutputPrivate::onNotifyCommitCallback;
    wl_signal_add(&handle->events.commit, &d->notifyCommitListener);

    d->destoryListener.notify = WOutputPrivate::onDestroyCallback;
    wl_signal_add(&handle->events.destroy, &d->destoryListener);
}

WOutput::~WOutput()
{
    if (d->handle) {
        wlr_output_destroy(d->handle);
    }
}

wlr_renderer *WOutput::renderer() const
{
    return d->handle->renderer;
}

wlr_swapchain *WOutput::swapchain() const
{
    return d->handle->swapchain;
}

wlr_allocator *WOutput::allocator() const
{
    return d->handle->allocator;
}

// Copy from wlroots
static const struct wlr_drm_format_set *wlr_renderer_get_render_formats(
    struct wlr_renderer *r) {
    if (!r->impl->get_render_formats) {
        return NULL;
    }
    return r->impl->get_render_formats(r);
}

static bool wlr_drm_format_copy(struct wlr_drm_format *dst, const struct wlr_drm_format *src) {
    assert(src->len <= src->capacity);

    uint64_t *modifiers = reinterpret_cast<uint64_t*>(malloc(sizeof(*modifiers) * src->len));
    if (!modifiers) {
        return false;
    }

    memcpy(modifiers, src->modifiers, sizeof(*modifiers) * src->len);

    wlr_drm_format_finish(dst);
    dst->capacity = src->len;
    dst->len = src->len;
    dst->format = src->format;
    dst->modifiers = modifiers;
    return true;
}

static bool wlr_drm_format_has(const struct wlr_drm_format *fmt, uint64_t modifier) {
    for (size_t i = 0; i < fmt->len; ++i) {
        if (fmt->modifiers[i] == modifier) {
            return true;
        }
    }
    return false;
}

static bool wlr_drm_format_add(struct wlr_drm_format *fmt, uint64_t modifier) {
    if (wlr_drm_format_has(fmt, modifier)) {
        return true;
    }

    if (fmt->len == fmt->capacity) {
        size_t capacity = fmt->capacity ? fmt->capacity * 2 : 4;

        uint64_t *new_modifiers = reinterpret_cast<uint64_t*>(realloc(fmt->modifiers, sizeof(*fmt->modifiers) * capacity));
        if (!new_modifiers) {
            qCCritical(waylibOutputBuffer) << "Failed to allocate memory for DRM format modifiers";
            return false;
        }

        fmt->capacity = capacity;
        fmt->modifiers = new_modifiers;
    }

    fmt->modifiers[fmt->len++] = modifier;
    return true;
}

static bool wlr_drm_format_intersect(struct wlr_drm_format *dst,
                                     const struct wlr_drm_format *a, const struct wlr_drm_format *b) {
    assert(a->format == b->format);

    size_t capacity = a->len < b->len ? a->len : b->len;
    uint64_t *modifiers = reinterpret_cast<uint64_t*>(malloc(sizeof(*modifiers) * capacity));
    if (!modifiers) {
        return false;
    }

    wlr_drm_format fmt = {
        .format = a->format,
        .len = 0,
        .capacity = capacity,
        .modifiers = modifiers,
    };

    for (size_t i = 0; i < a->len; i++) {
        for (size_t j = 0; j < b->len; j++) {
            if (a->modifiers[i] == b->modifiers[j]) {
                assert(fmt.len < fmt.capacity);
                fmt.modifiers[fmt.len++] = a->modifiers[i];
                break;
            }
        }
    }

    wlr_drm_format_finish(dst);
    *dst = fmt;
    return true;
}

static bool output_pick_format(struct wlr_output *output,
                               const struct wlr_drm_format_set *display_formats,
                               struct wlr_drm_format *format, uint32_t fmt) {
    struct wlr_renderer *renderer = output->renderer;
    assert(renderer != NULL && output->allocator != NULL);

    const struct wlr_drm_format_set *render_formats =
        wlr_renderer_get_render_formats(renderer);
    if (render_formats == NULL) {
        qCCritical(waylibOutputHW) << "Failed to get renderer format support information";
        return false;
    }

    const struct wlr_drm_format *render_format =
        wlr_drm_format_set_get(render_formats, fmt);
    if (render_format == NULL) {
        qCDebug(waylibOutputHW) << "Renderer does not support format:" << QString("0x%1").arg(fmt, 0, 16);
        return false;
    }

    if (display_formats != NULL) {
        const struct wlr_drm_format *display_format =
            wlr_drm_format_set_get(display_formats, fmt);
        if (display_format == NULL) {
            qCDebug(waylibOutputHW) << "Output does not support format:" << QString("0x%1").arg(fmt, 0, 16);
            return false;
        }
        if (!wlr_drm_format_intersect(format, display_format, render_format)) {
            qCWarning(waylibOutputHW) << "Failed to find compatible format modifiers for format"
                                     << QString("0x%1").arg(fmt, 0, 16)
                                     << "on output:" << QString::fromUtf8(output->name);
            return false;
        }
    } else {
        // The output can display any format
        if (!wlr_drm_format_copy(format, render_format)) {
            return false;
        }
    }

    if (format->len == 0) {
        wlr_drm_format_finish(format);
        qCWarning(waylibOutputHW) << "No compatible output format found";
        return false;
    }

    return true;
}

static struct wlr_swapchain *create_swapchain(struct wlr_output *output,
                                              int width, int height,
                                              uint32_t render_format,
                                              bool allow_modifiers) {
    wlr_allocator *allocator = output->allocator;
    assert(output->allocator != NULL);

    const struct wlr_drm_format_set *display_formats =
        wlr_output_get_primary_formats(output, allocator->buffer_caps);
    struct wlr_drm_format format{};
    if (!output_pick_format(output, display_formats, &format, render_format)) {
        qCWarning(waylibOutputBuffer) << "Failed to pick primary buffer format for output:" << QString::fromUtf8(output->name);
        return NULL;
    }

    char *format_name = drmGetFormatName(format.format);
    qCInfo(waylibOutputBuffer) << "Selected primary buffer format:"
                              << (format_name ? QString::fromUtf8(format_name) : QString("<unknown>"))
                              << QString("(0x%1)").arg(format.format, 8, 16, QLatin1Char('0'))
                              << "for output:" << QString::fromUtf8(output->name);
    free(format_name);

    if (!allow_modifiers && (format.len != 1 || format.modifiers[0] != DRM_FORMAT_MOD_LINEAR)) {
        if (!wlr_drm_format_has(&format, DRM_FORMAT_MOD_INVALID)) {
            qCWarning(waylibOutputHW) << "No support for implicit modifiers";
            wlr_drm_format_finish(&format);
            return NULL;
        }

        format.len = 0;
        if (!wlr_drm_format_add(&format, DRM_FORMAT_MOD_INVALID)) {
            qCWarning(waylibOutputHW) << "Failed to add implicit modifier to DRM format";
            wlr_drm_format_finish(&format);
            return NULL;
        }
    }

    struct wlr_swapchain *swapchain = wlr_swapchain_create(allocator, width, height, &format);
    wlr_drm_format_finish(&format);
    return swapchain;
}

static bool test_swapchain(struct wlr_output *output,
                           struct wlr_swapchain *swapchain, const struct wlr_output_state *state) {
    struct wlr_buffer *buffer = wlr_swapchain_acquire(swapchain);
    if (buffer == NULL) {
        return false;
    }

    struct wlr_output_state copy = *state;
    copy.committed |= WLR_OUTPUT_STATE_BUFFER;
    copy.buffer = buffer;
    bool ok = wlr_output_test_state(output, &copy);
    wlr_buffer_unlock(buffer);
    return ok;
}

static bool wlr_output_configure_primary_swapchain(struct wlr_output *output, int width, int height,
                                                   uint32_t format, struct wlr_swapchain **swapchain_ptr,
                                                   bool test) {
    wlr_output_state empty_state;
    wlr_output_state_init(&empty_state);
    wlr_output_state *state = &empty_state;

    // Re-use the existing swapchain if possible
    struct wlr_swapchain *old_swapchain = *swapchain_ptr;
    if (old_swapchain != NULL &&
        old_swapchain->width == width && old_swapchain->height == height &&
        old_swapchain->format.format == format) {
        return true;
    }

    struct wlr_swapchain *swapchain = create_swapchain(output, width, height, format, true);
    if (swapchain == NULL) {
        qCCritical(waylibOutputBuffer) << "Failed to create swapchain for output:" << QString::fromUtf8(output->name);
        return false;
    }

    if (test) {
        qCDebug(waylibOutputBuffer) << "Testing swapchain for output:" << QString::fromUtf8(output->name);
        if (!test_swapchain(output, swapchain, state)) {
            qCDebug(waylibOutputBuffer) << "Output test failed for" << QString::fromUtf8(output->name)
                                      << "- retrying without modifiers";
            wlr_swapchain_destroy(swapchain);
            swapchain = create_swapchain(output, width, height, format, false);
            if (swapchain == NULL) {
                qCCritical(waylibOutputBuffer) << "Failed to create modifier-less swapchain for output:"
                                             << QString::fromUtf8(output->name);
                return false;
            }
            qCDebug(waylibOutputBuffer) << "Testing modifier-less swapchain for output:"
                                      << QString::fromUtf8(output->name);
            if (!test_swapchain(output, swapchain, state)) {
                qCCritical(waylibOutputBuffer) << "Swapchain test failed for output:"
                                             << QString::fromUtf8(output->name);
                wlr_swapchain_destroy(swapchain);
                return false;
            }
        }
    }

    wlr_swapchain_destroy(*swapchain_ptr);
    *swapchain_ptr = swapchain;
    return true;
}

static bool output_pick_cursor_format(struct wlr_output *output,
                                      struct wlr_drm_format *format,
                                      uint32_t drm_format) {
    struct wlr_allocator *allocator = output->allocator;
    assert(allocator != NULL);

    const struct wlr_drm_format_set *display_formats = NULL;
    if (output->impl->get_cursor_formats) {
        display_formats =
            output->impl->get_cursor_formats(output, allocator->buffer_caps);
        if (display_formats == NULL) {
            qCDebug(waylibOutputHW) << "Failed to get cursor display formats from output";
            return false;
        }
    }

    return output_pick_format(output, display_formats, format, drm_format);
}
// End

bool WOutput::configurePrimarySwapchain(const QSize &size, uint32_t format,
                                        wlr_swapchain **swapchain, bool doTest)
{
    Q_ASSERT(!size.isEmpty());
    wlr_swapchain *sc = *swapchain;
    bool ok = wlr_output_configure_primary_swapchain(handle(), size.width(), size.height(),
                                                     format, &sc, doTest);
    if (!ok)
        return false;
    *swapchain = sc;
    return true;
}

bool WOutput::configureCursorSwapchain(const QSize &size, uint32_t drmFormat, wlr_swapchain **swapchain)
{
    Q_ASSERT(!size.isEmpty());
    auto sc = *swapchain;
    if (!sc || sc->width != size.width() || sc->height != size.height()) {
        wlr_drm_format format = {};
        if (!output_pick_cursor_format(handle(), &format, drmFormat)) {
            qCDebug(waylibOutputHW) << "Failed to select compatible cursor format";
            return false;
        }

        delete sc;
        sc = wlr_swapchain_create(allocator(), size.width(), size.height(), &format);
        wlr_drm_format_finish(&format);
        if (!sc) {
            qCDebug(waylibOutputBuffer) << "Failed to create cursor swapchain with selected format";
            return false;
        }
    }

    *swapchain = sc;
    return true;
}

wlr_output *WOutput::handle() const
{
    return d->handle;
}

WOutput *WOutput::fromHandle(wlr_output *handle)
{
    return static_cast<WOutput *>(handle->data);
}

WOutput *WOutput::fromScreen(const QScreen *screen)
{
    return static_cast<QWlrootsScreen*>(screen->handle())->output();
}

void WOutput::setScreen(QWlrootsScreen *screen)
{
    d->screen = screen;
}

QWlrootsScreen *WOutput::screen() const
{
    return d->screen;
}

QString WOutput::name() const
{
    return QString::fromUtf8(handle()->name);
}

bool WOutput::isEnabled() const
{
    return handle()->enabled;
}

QPoint WOutput::position() const
{
    QPoint p;

    if (Q_UNLIKELY(!d->layout))
        return p;

    auto l_output = wlr_output_layout_get(d->layout->handle(), handle());

    if (Q_UNLIKELY(!l_output))
        return p;

    return QPoint(l_output->x, l_output->y);
}

QSize WOutput::size() const
{
    return d->size();
}

QSize WOutput::transformedSize() const
{
    int width, height;
    wlr_output_transformed_resolution(handle(), &width, &height);
    return QSize( width, height );
}

QSize WOutput::effectiveSize() const
{
    int width, height;
    wlr_output_effective_resolution(handle(), &width, &height);
    return QSize( width, height );
}

WOutput::Transform WOutput::orientation() const
{
    return d->orientation();
}

float WOutput::scale() const
{
    return handle()->scale;
}

void WOutput::attach(QQuickWindow *window)
{
    d->window = window;
}

QQuickWindow *WOutput::attachedWindow() const
{
    return d->window;
}

void WOutput::setLayout(WOutputLayout *layout)
{
    if (d->layout == layout)
        return;

    d->layout = layout;
}

WOutputLayout *WOutput::layout() const
{
    return d->layout;
}

void WOutput::addCursor(WCursor *cursor)
{
    static_cast<QWlrootsCursor*>(screen()->cursor())->addCursor(cursor);
    Q_EMIT cursorAdded(cursor);
    Q_EMIT cursorListChanged();
}

void WOutput::removeCursor(WCursor *cursor)
{
    static_cast<QWlrootsCursor*>(screen()->cursor())->removeCursor(cursor);
    Q_EMIT cursorRemoved(cursor);
    Q_EMIT cursorListChanged();
}

const QList<WCursor *> &WOutput::cursorList() const
{
    return static_cast<QWlrootsCursor*>(screen()->cursor())->cursors;
}

bool WOutput::forceSoftwareCursor() const
{
    return d->forceSoftwareCursor;
}

void WOutput::setForceSoftwareCursor(bool on)
{
    if (d->forceSoftwareCursor == on)
        return;
    d->forceSoftwareCursor = on;
    wlr_output_lock_software_cursors(handle(), on);

    Q_EMIT forceSoftwareCursorChanged();
}

void WOutput::scheduleFrame()
{
    return wlr_output_schedule_frame(handle());
}

void WOutputPrivate::onNotifyCommitCallback(wl_listener *listener, void *data)
{
    WOutputPrivate *d =
        containerOf(listener, &WOutputPrivate::notifyCommitListener);
    wlr_output_event_commit *event = static_cast<wlr_output_event_commit *>(data);
    if (event->state->committed & WLR_OUTPUT_STATE_SCALE) {
        Q_EMIT d->q->scaleChanged();
        Q_EMIT d->q->effectiveSizeChanged();
    }

    if (event->state->committed & WLR_OUTPUT_STATE_MODE) {
        Q_EMIT d->q->modeChanged();
        Q_EMIT d->q->transformedSizeChanged();
        Q_EMIT d->q->effectiveSizeChanged();
    }

    if (event->state->committed & WLR_OUTPUT_STATE_TRANSFORM) {
        Q_EMIT d->q->orientationChanged();
        Q_EMIT d->q->transformedSizeChanged();
        Q_EMIT d->q->effectiveSizeChanged();
    }

    if (event->state->committed & WLR_OUTPUT_STATE_BUFFER)
        Q_EMIT d->q->bufferCommitted();

    if (event->state->committed & WLR_OUTPUT_STATE_ENABLED)
        Q_EMIT d->q->enabledChanged();
}

void WOutputPrivate::onDestroyCallback(wl_listener *listener, void *data)
{
    WOutputPrivate *d =
        containerOf(listener, &WOutputPrivate::destoryListener);

    // remove 一定要放在这里，防止 wlroots 中的断言触发
    wl_list_remove(&d->destoryListener.link);
    wl_list_remove(&d->notifyCommitListener.link);
    d->handle = nullptr; // 标记 handle 即将被 wlroots free

    Q_EMIT d->q->handleDestroyed(d->q);
}

WAYLIB_SERVER_END_NAMESPACE
