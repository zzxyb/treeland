// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wtypes.h>

#include <QObject>
#include <QRect>
#include <QQmlEngine>

struct wlr_surface;
struct wlr_buffer;

WAYLIB_SERVER_BEGIN_NAMESPACE

class WServer;
class WOutput;
class WSurfacePrivate;
class WAYLIB_SERVER_EXPORT WSurface : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool mapped READ mapped NOTIFY mappedChanged)
    Q_PROPERTY(bool isSubsurface READ isSubsurface NOTIFY isSubsurfaceChanged)
    Q_PROPERTY(bool hasSubsurface READ hasSubsurface NOTIFY hasSubsurfaceChanged)
    Q_PROPERTY(bool needsFrame READ needsFrame)
    Q_PROPERTY(QList<WSurface*> subsurfaces READ subsurfaces NOTIFY newSubsurface)
    Q_PROPERTY(uint32_t preferredBufferScale READ preferredBufferScale WRITE setPreferredBufferScale RESET resetPreferredBufferScale NOTIFY preferredBufferScaleChanged FINAL)
    QML_NAMED_ELEMENT(WaylandSurface)
    QML_UNCREATABLE("Only create in C++")

public:
    explicit WSurface(wlr_surface *handle, QObject *parent = nullptr);
    ~WSurface() override;

    wlr_surface *handle() const;
    wlr_buffer *buffer() const;

    static WSurface *fromHandle(wlr_surface *handle);

    // for current state
    bool mapped() const;
    QSize size() const;
    QSize bufferSize() const;
    WLR::Transform orientation() const;
    int bufferScale() const;
    QPoint bufferOffset() const;

    void notifyFrameDone();

    bool isSubsurface() const;
    bool hasSubsurface() const;
    QList<WSurface*> subsurfaces() const;

    uint32_t preferredBufferScale() const;
    void setPreferredBufferScale(uint32_t newPreferredBufferScale);
    void resetPreferredBufferScale();

    bool needsFrame() const;
    bool scheduleFrameIfNeeded();

    bool inputRegionContains(const QPointF &localPos) const;

public Q_SLOTS:
    void enterOutput(WOutput *output);
    void leaveOutput(WOutput *output);
    const QList<WOutput *> &outputs() const;
    WOutput *framePacingOutput() const;

    void map();
    void unmap();

Q_SIGNALS:
    void mappedChanged();
    void bufferOffsetChanged();
    void isSubsurfaceChanged();
    void hasSubsurfaceChanged();
    void newSubsurface(WSurface *subsurface);
    void preferredBufferScaleChanged();
    void outputEntered(WOutput *output);
    void outputLeave(WOutput *output);
    void commit(quint32 committedState /*wlr_surface_state_field*/);
    void handleDestroyed(WSurface *surface);

private:
    friend class WSurfacePrivate;
    std::unique_ptr<WSurfacePrivate> d;
};

WAYLIB_SERVER_END_NAMESPACE
