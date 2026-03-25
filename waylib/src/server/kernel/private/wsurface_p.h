// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wsurface.h"
#include "wcontainerof.h"

extern "C" {
#define static
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#undef static
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_client_buffer.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
}

#include <wayland-server-core.h>

#include <QObject>
#include <QPointer>

struct wlr_surface;
struct wlr_subsurface;

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WSurfacePrivate {
public:
    WSurfacePrivate(WSurface *qq, wlr_surface *handle);
    ~WSurfacePrivate();

    wl_client *waylandClient() const;

    // begin slot function
    void on_commit();
    // end slot function

    void init();
    void connect();
    void updateOutputs();
    void setBuffer(wlr_buffer *newBuffer);
    void updateBuffer();
    void updateBufferOffset();
    void updatePreferredBufferScale();
    void preferredBufferScaleChange();

    WSurface *ensureSubsurface(wlr_subsurface *subsurface);
    void setSubsurface(wlr_subsurface *newSubsurface);
    void setHasSubsurface(bool newHasSubsurface);
    void updateHasSubsurface();

    // static wl_listener callbacks
    static void onDestroy(wl_listener *listener, void *data);
    static void onCommit(wl_listener *listener, void *data);
    static void onMap(wl_listener *listener, void *data);
    static void onUnmap(wl_listener *listener, void *data);
    static void onNewSubsurface(wl_listener *listener, void *data);
    static void onSubsurfaceDestroy(wl_listener *listener, void *data);

    WSurface *q;
    wlr_surface *handle = nullptr;
    wlr_subsurface *subsurface = nullptr;

    wl_listener destroyListener;
    wl_listener commitListener;
    wl_listener mapListener;
    wl_listener unmapListener;
    wl_listener newSubsurfaceListener;
    wl_listener subsurfaceDestroyListener;

    bool hasSubsurface = false;
    bool isSubsurface = false;
    uint32_t preferredBufferScale = 1;
    uint32_t explicitPreferredBufferScale = 0;

    bool needsFrame = false;
    wlr_buffer *buffer = nullptr;
    QList<WOutput*> outputs;
    WOutput *framePacingOutput = nullptr;
    QMetaObject::Connection frameDoneConnection;
    QPoint bufferOffset;
};

WAYLIB_SERVER_END_NAMESPACE
