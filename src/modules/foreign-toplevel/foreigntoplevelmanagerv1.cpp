// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "foreigntoplevelmanagerv1.h"

#include "modules/foreign-toplevel/qwayland-server-treeland-foreign-toplevel-manager-v1.h"

#include "common/treelandlogging.h"
#include "core/rootsurfacecontainer.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"

#include <woutput.h>
#include <wsocket.h>
#include <wtoplevelsurface.h>
#include <wxdgtoplevelsurface.h>
#include <wxwaylandsurface.h>

#include <wayland-server-core.h>

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwoutput.h>
#include <qwxdgshell.h>

extern "C" {
#define static
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_seat.h>
#undef static
}

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

static QList<DockPreviewContextInterfaceV1 *> s_dockPreviewContexts;
static QList<ForeignToplevelHandleInterfaceV1 *> s_toplevelHandles;

// ============================================================================
// DockPreviewContextInterfaceV1Private
// ============================================================================

class DockPreviewContextInterfaceV1Private
    : public QtWaylandServer::treeland_dock_preview_context_v1
{
public:
    DockPreviewContextInterfaceV1Private(DockPreviewContextInterfaceV1 *_q,
                                         wlr_surface *_relative_surface,
                                         wl_resource *_resource);
    ~DockPreviewContextInterfaceV1Private() override = default;

    DockPreviewContextInterfaceV1 *q = nullptr;
    wlr_surface *relativeSurface = nullptr;
    wl_listener relativeSurfaceDestroyListener {};

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void show(Resource *resource, wl_array *surfaces, int32_t x, int32_t y, uint32_t direction) override;
    void show_tooltip(Resource *resource, const QString &tooltip, int32_t x, int32_t y, uint32_t direction) override;
    void close(Resource *resource) override;
};

DockPreviewContextInterfaceV1Private::DockPreviewContextInterfaceV1Private(
    DockPreviewContextInterfaceV1 *_q,
    wlr_surface *_relative_surface,
    wl_resource *_resource)
    : q(_q)
    , relativeSurface(_relative_surface)
{
    init(_resource);

    // Setup listener for relative surface destruction
    relativeSurfaceDestroyListener.notify = [](wl_listener *listener, void *) {
        auto *self = wl_container_of(listener, self, relativeSurfaceDestroyListener);
        self->relativeSurface = nullptr;
    };
    if (relativeSurface) {
        wl_signal_add(&relativeSurface->events.destroy, &relativeSurfaceDestroyListener);
    }
}

void DockPreviewContextInterfaceV1Private::destroy_resource(Resource *resource)
{
    if (relativeSurface) {
        wl_list_remove(&relativeSurfaceDestroyListener.link);
    }
    s_dockPreviewContexts.removeOne(q);
    Q_EMIT q->beforeDestroy();
    delete q;
}

void DockPreviewContextInterfaceV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void DockPreviewContextInterfaceV1Private::show(Resource *resource,
                                                 wl_array *surfaces,
                                                 int32_t x,
                                                 int32_t y,
                                                 uint32_t direction)
{
    if (!relativeSurface) {
        return;
    }

    std::vector<uint32_t> s;
    const uint32_t *data = reinterpret_cast<const uint32_t *>(surfaces->data);
    const size_t count = surfaces->size / sizeof(uint32_t);
    for (size_t i = 0; i != count; ++i) {
        s.push_back(data[i]);
    }

    if (!surfaces->size)
        qCCritical(treelandProtocol) << "Got empty surface list for dock preview!";

    Q_EMIT q->requestShow(s, x, y, direction);
}

void DockPreviewContextInterfaceV1Private::show_tooltip(Resource *resource,
                                                         const QString &tooltip,
                                                         int32_t x,
                                                         int32_t y,
                                                         uint32_t direction)
{
    if (!relativeSurface) {
        return;
    }

    Q_EMIT q->requestShowTooltip(tooltip, x, y, direction);
}

void DockPreviewContextInterfaceV1Private::close(Resource *resource)
{
    if (!relativeSurface) {
        return;
    }

    Q_EMIT q->requestClose();
}

// ============================================================================
// ForeignToplevelHandleInterfaceV1Private
// ============================================================================

class ForeignToplevelHandleInterfaceV1Private
    : public QtWaylandServer::treeland_foreign_toplevel_handle_v1
{
public:
    ForeignToplevelHandleInterfaceV1Private(ForeignToplevelHandleInterfaceV1 *_q,
                                            ForeignToplevelManagerInterfaceV1 *_manager,
                                            wl_resource *_resource);
    ~ForeignToplevelHandleInterfaceV1Private() override = default;

    ForeignToplevelHandleInterfaceV1 *q = nullptr;
    ForeignToplevelManagerInterfaceV1 *manager = nullptr;
    wl_event_source *idle_source = nullptr;

    QString title;
    QString app_id;
    uint32_t identifier = 0;
    pid_t pid = 0;

    ForeignToplevelHandleInterfaceV1 *parentHandle = nullptr;
    QList<treeland_foreign_toplevel_handle_v1_output> outputs;
    ForeignToplevelHandleInterfaceV1::States state;

    void update_idle_source();
    void send_state_internal();
    void send_output_internal(qw_output *output, bool enter);
    void send_parent_internal();

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void set_fullscreen(Resource *resource, struct ::wl_resource *output) override;
    void unset_fullscreen(Resource *resource) override;
    void set_maximized(Resource *resource) override;
    void unset_maximized(Resource *resource) override;
    void set_minimized(Resource *resource) override;
    void unset_minimized(Resource *resource) override;
    void activate(Resource *resource, struct ::wl_resource *seat) override;
    void close(Resource *resource) override;
    void set_rectangle(Resource *resource, struct ::wl_resource *surface, int32_t x, int32_t y, int32_t width, int32_t height) override;
};

static void toplevel_idle_send_done(void *data)
{
    auto *priv = static_cast<ForeignToplevelHandleInterfaceV1Private *>(data);
    priv->send_done();
    priv->idle_source = nullptr;
}

void ForeignToplevelHandleInterfaceV1Private::update_idle_source()
{
    if (idle_source) {
        return;
    }

    auto *display = wl_client_get_display(wl_resource_get_client(resource()->handle));
    auto *event_loop = wl_display_get_event_loop(display);
    idle_source = wl_event_loop_add_idle(event_loop, toplevel_idle_send_done, this);
}

static bool fill_state_array(wl_array *array, ForeignToplevelHandleInterfaceV1::States state)
{
    if (state.testFlag(ForeignToplevelHandleInterfaceV1::State::Maximized)) {
        uint32_t *index = static_cast<uint32_t *>(wl_array_add(array, sizeof(uint32_t)));
        if (!index) return false;
        *index = TREELAND_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED;
    }
    if (state.testFlag(ForeignToplevelHandleInterfaceV1::State::Minimized)) {
        uint32_t *index = static_cast<uint32_t *>(wl_array_add(array, sizeof(uint32_t)));
        if (!index) return false;
        *index = TREELAND_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED;
    }
    if (state.testFlag(ForeignToplevelHandleInterfaceV1::State::Activated)) {
        uint32_t *index = static_cast<uint32_t *>(wl_array_add(array, sizeof(uint32_t)));
        if (!index) return false;
        *index = TREELAND_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED;
    }
    if (state.testFlag(ForeignToplevelHandleInterfaceV1::State::Fullscreen)) {
        uint32_t *index = static_cast<uint32_t *>(wl_array_add(array, sizeof(uint32_t)));
        if (!index) return false;
        *index = TREELAND_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN;
    }
    return true;
}

void ForeignToplevelHandleInterfaceV1Private::send_state_internal()
{
    struct wl_array states;
    wl_array_init(&states);

    if (!fill_state_array(&states, state)) {
        wl_resource_post_no_memory(resource()->handle);
        wl_array_release(&states);
        return;
    }

    send_state(QByteArray::fromRawData(reinterpret_cast<const char *>(states.data), static_cast<int>(states.size)));
    wl_array_release(&states);

    update_idle_source();
}

static void send_output_to_resource(wl_resource *toplevel_resource, wlr_output *output, bool enter)
{
    const wl_client *client = wl_resource_get_client(toplevel_resource);
    struct wl_resource *output_resource;

    wl_resource_for_each(output_resource, &output->resources)
    {
        if (wl_resource_get_client(output_resource) == client) {
            auto *handle = QtWaylandServer::treeland_foreign_toplevel_handle_v1::Resource::fromResource(toplevel_resource);
            if (handle && handle->object()) {
                if (enter) {
                    handle->object()->send_output_enter(output_resource);
                } else {
                    handle->object()->send_output_leave(output_resource);
                }
            }
        }
    }
}

void ForeignToplevelHandleInterfaceV1Private::send_output_internal(qw_output *output, bool enter)
{
    send_output_to_resource(resource()->handle, output->handle(), enter);
    update_idle_source();
}

void ForeignToplevelHandleInterfaceV1Private::send_parent_internal()
{
    if (!parentHandle) {
        send_parent(nullptr);
    } else {
        // Find parent resource for same client
        auto *client = wl_resource_get_client(resource()->handle);
        // We need to find the parent's resource for this client
        // For simplicity, we send null if we can't find it
        send_parent(nullptr);
    }
    update_idle_source();
}

ForeignToplevelHandleInterfaceV1Private::ForeignToplevelHandleInterfaceV1Private(
    ForeignToplevelHandleInterfaceV1 *_q,
    ForeignToplevelManagerInterfaceV1 *_manager,
    wl_resource *_resource)
    : q(_q)
    , manager(_manager)
{
    init(_resource);
}

void ForeignToplevelHandleInterfaceV1Private::destroy_resource(Resource *resource)
{
    s_toplevelHandles.removeOne(q);
    Q_EMIT q->beforeDestroy();

    // Need to ensure no other toplevels hold a pointer to this one as a parent
    for (auto *tl : s_toplevelHandles) {
        if (tl->d->parentHandle == q) {
            tl->set_parent(nullptr);
        }
    }

    if (idle_source) {
        wl_event_source_remove(idle_source);
        idle_source = nullptr;
    }

    delete q;
}

void ForeignToplevelHandleInterfaceV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ForeignToplevelHandleInterfaceV1Private::set_fullscreen(Resource *resource,
                                                              struct ::wl_resource *output)
{
    struct wlr_output *wlr_output = nullptr;
    if (output) {
        wlr_output = wlr_output_from_resource(output);
    }
    Q_EMIT q->requestFullscreen(true, wlr_output);
}

void ForeignToplevelHandleInterfaceV1Private::unset_fullscreen(Resource *resource)
{
    Q_EMIT q->requestFullscreen(false, nullptr);
}

void ForeignToplevelHandleInterfaceV1Private::set_maximized(Resource *resource)
{
    Q_EMIT q->requestMaximize(true);
}

void ForeignToplevelHandleInterfaceV1Private::unset_maximized(Resource *resource)
{
    Q_EMIT q->requestMaximize(false);
}

void ForeignToplevelHandleInterfaceV1Private::set_minimized(Resource *resource)
{
    Q_EMIT q->requestMinimize(true);
}

void ForeignToplevelHandleInterfaceV1Private::unset_minimized(Resource *resource)
{
    Q_EMIT q->requestMinimize(false);
}

void ForeignToplevelHandleInterfaceV1Private::activate(Resource *resource,
                                                        struct ::wl_resource *seat)
{
    const wlr_seat_client *seat_client = wlr_seat_client_from_resource(seat);
    if (!seat_client) {
        return;
    }
    Q_EMIT q->requestActivate(seat_client->seat);
}

void ForeignToplevelHandleInterfaceV1Private::close(Resource *resource)
{
    Q_EMIT q->requestClose();
}

void ForeignToplevelHandleInterfaceV1Private::set_rectangle(Resource *resource,
                                                             struct ::wl_resource *surface,
                                                             int32_t x,
                                                             int32_t y,
                                                             int32_t width,
                                                             int32_t height)
{
    if (width < 0 || height < 0) {
        wl_resource_post_error(resource->handle,
                               TREELAND_FOREIGN_TOPLEVEL_HANDLE_V1_ERROR_INVALID_RECTANGLE,
                               "invalid rectangle passed to set_rectangle: width/height < 0");
        return;
    }

    Q_EMIT q->rectangleChanged(wlr_surface_from_resource(surface), x, y, width, height);
}

// ============================================================================
// ForeignToplevelManagerInterfaceV1Private
// ============================================================================

class ForeignToplevelManagerInterfaceV1Private
    : public QtWaylandServer::treeland_foreign_toplevel_manager_v1
{
public:
    ForeignToplevelManagerInterfaceV1Private(ForeignToplevelManagerInterfaceV1 *_q);
    ~ForeignToplevelManagerInterfaceV1Private() override = default;

    wl_global *global() const;
    ForeignToplevelManagerInterfaceV1 *q;
    QList<Resource *> m_resources;

protected:
    void bind_resource(Resource *resource) override;
    void destroy_resource(Resource *resource) override;
    void destroy_global() override;
    void destroy(Resource *resource) override;
    void get_dock_preview_context(Resource *resource, struct ::wl_resource *relative_surface, uint32_t id) override;
    void stop(Resource *resource) override;
};

ForeignToplevelManagerInterfaceV1Private::ForeignToplevelManagerInterfaceV1Private(
    ForeignToplevelManagerInterfaceV1 *_q)
    : q(_q)
{
}

wl_global *ForeignToplevelManagerInterfaceV1Private::global() const
{
    return m_global;
}

void ForeignToplevelManagerInterfaceV1Private::bind_resource(Resource *resource)
{
    m_resources.append(resource);

    // Send all existing toplevels to the new client
    for (auto *toplevel : s_toplevelHandles) {
        // Create a new resource for this toplevel for this client
        auto *toplevelResource = wl_resource_create(resource->client(),
                                                    &treeland_foreign_toplevel_handle_v1_interface,
                                                    resource->version(),
                                                    0);
        if (!toplevelResource) {
            wl_client_post_no_memory(resource->client());
            continue;
        }

        // Initialize the toplevel's private with this new resource
        toplevel->d->add(resource->client(), 0, resource->version());
        send_toplevel(toplevelResource);
    }

    // Send details about each toplevel
    for (auto *toplevel : s_toplevelHandles) {
        toplevel->send_initial_state(resource->handle);
    }
}

void ForeignToplevelManagerInterfaceV1Private::destroy_resource(Resource *resource)
{
    m_resources.removeOne(resource);
}

void ForeignToplevelManagerInterfaceV1Private::destroy_global()
{
    m_resources.clear();
    delete q;
}

void ForeignToplevelManagerInterfaceV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ForeignToplevelManagerInterfaceV1Private::get_dock_preview_context(
    Resource *resource,
    struct ::wl_resource *relative_surface,
    uint32_t id)
{
    if (!relative_surface) {
        wl_resource_post_error(resource->handle, 0, "relative_surface resource is NULL!");
        return;
    }

    struct wl_resource *context_resource = wl_resource_create(resource->client(),
                                                              &treeland_dock_preview_context_v1_interface,
                                                              resource->version(),
                                                              id);
    if (context_resource == nullptr) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    auto *wlrRelativeSurface = wlr_surface_from_resource(relative_surface);
    auto *previewContext = new DockPreviewContextInterfaceV1(wlrRelativeSurface, context_resource);
    s_dockPreviewContexts.append(previewContext);

    QObject::connect(previewContext,
                     &DockPreviewContextInterfaceV1::requestClose,
                     q,
                     &ForeignToplevelManagerInterfaceV1::requestDockPreviewClose);
    QObject::connect(previewContext,
                     &DockPreviewContextInterfaceV1::requestShow,
                     q,
                     [this, wlrRelativeSurface](std::vector<uint32_t> toplevels, int32_t x, int32_t y, uint32_t direction) {
                         std::vector<SurfaceWrapper *> surfaces;
                         Q_EMIT q->findSurfaceByIdentifier(toplevels, surfaces);
                         Q_EMIT q->requestDockPreview(surfaces,
                                                      WSurface::fromHandle(wlrRelativeSurface),
                                                      QPoint(x, y),
                                                      static_cast<ForeignToplevelManagerInterfaceV1::PreviewDirection>(direction));
                     });
    QObject::connect(previewContext,
                     &DockPreviewContextInterfaceV1::requestShowTooltip,
                     q,
                     [this, wlrRelativeSurface](QString tooltip, int32_t x, int32_t y, uint32_t direction) {
                         Q_EMIT q->requestDockPreviewTooltip(tooltip,
                                                             WSurface::fromHandle(wlrRelativeSurface),
                                                             QPoint(x, y),
                                                             static_cast<ForeignToplevelManagerInterfaceV1::PreviewDirection>(direction));
                     });
    QObject::connect(previewContext, &QObject::destroyed, [previewContext]() {
        s_dockPreviewContexts.removeOne(previewContext);
    });
}

void ForeignToplevelManagerInterfaceV1Private::stop(Resource *resource)
{
    send_finished(resource->handle);
    wl_resource_destroy(resource->handle);
}

// ============================================================================
// ForeignToplevelManagerInterfaceV1
// ============================================================================

ForeignToplevelManagerInterfaceV1::ForeignToplevelManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(new ForeignToplevelManagerInterfaceV1Private(this))
{
}

ForeignToplevelManagerInterfaceV1::~ForeignToplevelManagerInterfaceV1()
{
    s_dockPreviewContexts.clear();
    s_toplevelHandles.clear();
}

void ForeignToplevelManagerInterfaceV1::addSurface(SurfaceWrapper *wrapper)
{
    // Create toplevel handle for this surface
    // This will be called when a new surface is added
}

void ForeignToplevelManagerInterfaceV1::removeSurface(SurfaceWrapper *wrapper)
{
    // Remove toplevel handle for this surface
}

void ForeignToplevelManagerInterfaceV1::enterDockPreview(WSurface *relative_surface)
{
    for (auto *context : s_dockPreviewContexts) {
        if (context->relativeSurface() == relative_surface->handle()) {
            context->enter();
            break;
        }
    }
}

void ForeignToplevelManagerInterfaceV1::leaveDockPreview(WSurface *relative_surface)
{
    for (auto *context : s_dockPreviewContexts) {
        if (context->relativeSurface() == relative_surface->handle()) {
            context->leave();
            break;
        }
    }
}

void ForeignToplevelManagerInterfaceV1::create(WServer *server)
{
    d->init(server->handle()->handle(), InterfaceVersion);
}

void ForeignToplevelManagerInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d = nullptr;
}

wl_global *ForeignToplevelManagerInterfaceV1::global() const
{
    return d->global();
}

QByteArrayView ForeignToplevelManagerInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

// ============================================================================
// ForeignToplevelHandleInterfaceV1
// ============================================================================

ForeignToplevelHandleInterfaceV1::ForeignToplevelHandleInterfaceV1(
    ForeignToplevelManagerInterfaceV1 *manager,
    wl_resource *resource)
    : QObject(nullptr)
    , d(new ForeignToplevelHandleInterfaceV1Private(this, manager, resource))
{
    s_toplevelHandles.append(this);
}

ForeignToplevelHandleInterfaceV1::~ForeignToplevelHandleInterfaceV1()
{
    s_toplevelHandles.removeOne(this);
    if (d->idle_source) {
        wl_event_source_remove(d->idle_source);
    }
}

wl_resource *ForeignToplevelHandleInterfaceV1::resource() const
{
    return d->resource()->handle;
}

void ForeignToplevelHandleInterfaceV1::set_title(const QString &title)
{
    if (d->title == title) {
        return;
    }
    d->title = title;
    d->send_title(title);
    d->update_idle_source();
}

void ForeignToplevelHandleInterfaceV1::set_app_id(const QString &app_id)
{
    if (d->app_id == app_id) {
        return;
    }
    d->app_id = app_id;
    d->send_app_id(app_id);
    d->update_idle_source();
}

void ForeignToplevelHandleInterfaceV1::set_pid(const pid_t pid)
{
    d->pid = pid;
    d->send_pid(pid);
    d->update_idle_source();
}

void ForeignToplevelHandleInterfaceV1::set_identifier(uint32_t identifier)
{
    d->identifier = identifier;
    d->send_identifier(identifier);
    d->update_idle_source();
}

uint32_t ForeignToplevelHandleInterfaceV1::identifier() const
{
    return d->identifier;
}

void ForeignToplevelHandleInterfaceV1::output_enter(qw_output *output)
{
    if (std::any_of(d->outputs.begin(),
                    d->outputs.end(),
                    [output](const treeland_foreign_toplevel_handle_v1_output &toplevel_output) {
                        return toplevel_output.output == output;
                    })) {
        return;
    }

    auto toplevel_output =
        treeland_foreign_toplevel_handle_v1_output{ .output = output, .toplevel = this };
    d->outputs.append(toplevel_output);

    connect(output, &qw_output::notify_bind, this, [this, output](wlr_output_event_bind *event) {
        const wl_client *client = wl_resource_get_client(event->resource);
        send_output_to_resource(resource(), output->handle(), true);
        d->update_idle_source();
    });

    connect(output, &qw_output::before_destroy, this, [this, output]() {
        output_leave(output);
    });

    d->send_output_internal(output, true);
}

void ForeignToplevelHandleInterfaceV1::output_leave(qw_output *output)
{
    d->outputs.removeIf([output](const treeland_foreign_toplevel_handle_v1_output &handle_output) {
        return handle_output.output == output;
    });
    d->send_output_internal(output, false);
}

void ForeignToplevelHandleInterfaceV1::set_maximized(bool maximized)
{
    if (d->state.testFlag(State::Maximized) == maximized) {
        return;
    }
    d->state.setFlag(State::Maximized, maximized);
    d->send_state_internal();
}

void ForeignToplevelHandleInterfaceV1::set_minimized(bool minimized)
{
    if (d->state.testFlag(State::Minimized) == minimized) {
        return;
    }
    d->state.setFlag(State::Minimized, minimized);
    d->send_state_internal();
}

void ForeignToplevelHandleInterfaceV1::set_activated(bool activated)
{
    if (d->state.testFlag(State::Activated) == activated) {
        return;
    }
    d->state.setFlag(State::Activated, activated);
    d->send_state_internal();
}

void ForeignToplevelHandleInterfaceV1::set_fullscreen(bool fullscreen)
{
    if (d->state.testFlag(State::Fullscreen) == fullscreen) {
        return;
    }
    d->state.setFlag(State::Fullscreen, fullscreen);
    d->send_state_internal();
}

void ForeignToplevelHandleInterfaceV1::set_parent(ForeignToplevelHandleInterfaceV1 *parent)
{
    if (d->parentHandle == parent) {
        return;
    }

    d->parentHandle = parent;
    d->send_parent_internal();
}

void ForeignToplevelHandleInterfaceV1::send_done()
{
    d->send_done();
}

void ForeignToplevelHandleInterfaceV1::reset_idle_source()
{
    d->idle_source = nullptr;
}

void ForeignToplevelHandleInterfaceV1::send_initial_state(wl_resource *manager_resource)
{
    if (!d->title.isEmpty()) {
        d->send_title(d->title);
    }
    if (!d->app_id.isEmpty()) {
        d->send_app_id(d->app_id);
    }

    d->send_pid(d->pid);
    d->send_identifier(d->identifier);

    for (const auto &output : d->outputs) {
        send_output_to_resource(resource(), output.output->handle(), true);
    }

    d->send_state_internal();
    d->send_parent_internal();
    d->send_done();
}

ForeignToplevelHandleInterfaceV1 *ForeignToplevelHandleInterfaceV1::get(const wl_resource *resource)
{
    for (auto *handle : s_toplevelHandles) {
        if (handle->resource() == resource) {
            return handle;
        }
    }
    return nullptr;
}

ForeignToplevelHandleInterfaceV1 *ForeignToplevelHandleInterfaceV1::create(
    ForeignToplevelManagerInterfaceV1 *manager,
    wl_client *client,
    uint32_t id,
    int version)
{
    struct wl_resource *resource = wl_resource_create(client,
                                                      &treeland_foreign_toplevel_handle_v1_interface,
                                                      version,
                                                      id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return nullptr;
    }

    return new ForeignToplevelHandleInterfaceV1(manager, resource);
}

// ============================================================================
// DockPreviewContextInterfaceV1
// ============================================================================

DockPreviewContextInterfaceV1::DockPreviewContextInterfaceV1(wlr_surface *relative_surface,
                                                             wl_resource *resource)
    : QObject(nullptr)
    , d(new DockPreviewContextInterfaceV1Private(this, relative_surface, resource))
{
}

DockPreviewContextInterfaceV1::~DockPreviewContextInterfaceV1()
{
}

wlr_surface *DockPreviewContextInterfaceV1::relativeSurface() const
{
    return d->relativeSurface;
}

wl_resource *DockPreviewContextInterfaceV1::resource() const
{
    return d->resource()->handle;
}

void DockPreviewContextInterfaceV1::enter()
{
    d->send_enter();
}

void DockPreviewContextInterfaceV1::leave()
{
    d->send_leave();
}

DockPreviewContextInterfaceV1 *DockPreviewContextInterfaceV1::get(const wl_resource *resource)
{
    for (auto *context : s_dockPreviewContexts) {
        if (context->resource() == resource) {
            return context;
        }
    }
    return nullptr;
}
