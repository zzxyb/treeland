// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwdisplay.h>
#include <qwoutput.h>
#include <qwcompositor.h>

#include <wserver.h>
#include <wsurface.h>
#include <wxdgsurface.h>

#include <wayland-server-core.h>

extern "C" {
#include <wlr/types/wlr_seat.h>
}

class SurfaceWrapper;
struct wlr_surface;

QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

class ForeignToplevelManagerInterfaceV1Private;
class ForeignToplevelHandleInterfaceV1Private;
class DockPreviewContextInterfaceV1Private;

class ForeignToplevelManagerInterfaceV1 : public QObject, public WServerInterface
{
    Q_OBJECT
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")
public:
    enum class PreviewDirection
    {
        top = 0,
        right,
        bottom,
        left,
    };
    Q_ENUM(PreviewDirection);

    explicit ForeignToplevelManagerInterfaceV1(QObject *parent = nullptr);
    ~ForeignToplevelManagerInterfaceV1() override;

    static constexpr int InterfaceVersion = 1;

    void addSurface(SurfaceWrapper *wrapper);
    void removeSurface(SurfaceWrapper *wrapper);

    Q_INVOKABLE void enterDockPreview(WSurface *relative_surface);
    Q_INVOKABLE void leaveDockPreview(WSurface *relative_surface);

Q_SIGNALS:
    void requestDockPreview(std::vector<SurfaceWrapper *> surfaces,
                            WSurface *target,
                            QPoint abs,
                            PreviewDirection direction);
    void requestDockPreviewTooltip(QString tooltip,
                                   WSurface *target,
                                   QPoint abs,
                                   PreviewDirection direction);
    void requestDockPreviewClose();
    void findSurfaceByIdentifier(std::vector<uint32_t> identifiers, std::vector<SurfaceWrapper *> &surfaces);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
    QByteArrayView interfaceName() const override;

private:
    std::unique_ptr<ForeignToplevelManagerInterfaceV1Private> d;

    friend class ForeignToplevelManagerInterfaceV1Private;
    friend class ForeignToplevelHandleInterfaceV1Private;
};

class ForeignToplevelHandleInterfaceV1;
struct treeland_foreign_toplevel_handle_v1_output
{
    QW_NAMESPACE::qw_output *output{ nullptr };
    ForeignToplevelHandleInterfaceV1 *toplevel{ nullptr };
};

class ForeignToplevelHandleInterfaceV1 : public QObject
{
    Q_OBJECT
public:
    enum class State
    {
        Maximized = 1,
        Minimized = 2,
        Activated = 4,
        Fullscreen = 8,
    };
    Q_ENUM(State);
    Q_DECLARE_FLAGS(States, State)

    ~ForeignToplevelHandleInterfaceV1() override;

    wl_resource *resource() const;
    void set_title(const QString &title);
    void set_app_id(const QString &app_id);
    void set_pid(const pid_t pid);
    void set_identifier(uint32_t identifier);
    uint32_t identifier() const;
    void output_enter(QW_NAMESPACE::qw_output *output);
    void output_leave(QW_NAMESPACE::qw_output *output);

    void set_maximized(bool maximized);
    void set_minimized(bool minimized);
    void set_activated(bool activated);
    void set_fullscreen(bool fullscreen);
    void set_parent(ForeignToplevelHandleInterfaceV1 *parent);

    void send_done();
    void reset_idle_source();
    void send_initial_state(wl_resource *manager_resource);

    static ForeignToplevelHandleInterfaceV1 *get(const wl_resource *resource);
    static ForeignToplevelHandleInterfaceV1 *create(ForeignToplevelManagerInterfaceV1 *manager,
                                                     wl_client *client,
                                                     uint32_t id,
                                                     int version);

Q_SIGNALS:
    void beforeDestroy();
    void requestMaximize(bool maximized);
    void requestMinimize(bool minimized);
    void requestActivate(wlr_seat *seat);
    void requestFullscreen(bool fullscreen, wlr_output *output);
    void requestClose();
    void rectangleChanged(wlr_surface *surface, int32_t x, int32_t y, int32_t width, int32_t height);

private:
    explicit ForeignToplevelHandleInterfaceV1(ForeignToplevelManagerInterfaceV1 *manager,
                                              wl_resource *resource);

private:
    std::unique_ptr<ForeignToplevelHandleInterfaceV1Private> d;

    friend class ForeignToplevelManagerInterfaceV1Private;
    friend class ForeignToplevelHandleInterfaceV1Private;
    friend class DockPreviewContextInterfaceV1Private;
};

class DockPreviewContextInterfaceV1 : public QObject
{
    Q_OBJECT
public:
    ~DockPreviewContextInterfaceV1() override;

    wlr_surface *relativeSurface() const;
    wl_resource *resource() const;

    void enter();
    void leave();

    static DockPreviewContextInterfaceV1 *get(const wl_resource *resource);

Q_SIGNALS:
    void beforeDestroy();
    void requestShow(std::vector<uint32_t> toplevels, int32_t x, int32_t y, uint32_t direction);
    void requestShowTooltip(QString tooltip, int32_t x, int32_t y, uint32_t direction);
    void requestClose();

private:
    explicit DockPreviewContextInterfaceV1(wlr_surface *relative_surface, wl_resource *resource);

private:
    std::unique_ptr<DockPreviewContextInterfaceV1Private> d;

    friend class ForeignToplevelManagerInterfaceV1Private;
    friend class DockPreviewContextInterfaceV1Private;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ForeignToplevelHandleInterfaceV1::States)
