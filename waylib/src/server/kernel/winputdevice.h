// Copyright (C) 2023-2026 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

extern "C" {
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/types/wlr_touch.h>
#include <wlr/types/wlr_switch.h>

#define static
#include <wlr/backend/drm.h>
#undef static
#include <wlr/backend/wayland.h>
#ifdef WLR_HAVE_X11_BACKEND
#include <wlr/backend/x11.h>
#endif
#include <wlr/backend/libinput.h>
}

#include <wglobal.h>
#include <QObject>

QT_BEGIN_NAMESPACE
class QInputDevice;
class QEventPoint;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSeat;
class WInputDevicePrivate;
class WAYLIB_SERVER_EXPORT WInputDevice : public QObject
{
    Q_OBJECT
public:
    enum class Type {
        Unknow,
        Keyboard,
        Pointer,
        Touch,
        Tablet,
        TabletPad,
        Switch
    };
    Q_ENUM(Type)

    WInputDevice(wlr_input_device *handle);
    ~WInputDevice() override;
    wlr_input_device *handle() const;

    static WInputDevice *fromHandle(wlr_input_device *handle);

    template<class QInputDevice>
    inline QInputDevice *qtDevice() const {
        return qobject_cast<QInputDevice*>(qtDevice());
    }
    QInputDevice *qtDevice() const;
    static WInputDevice *from(const QInputDevice *device);

    Type type() const;
    QString name() const;
    void setSeat(WSeat *seat);
    WSeat *seat() const;
    QString devicePath() const;

private:
    friend class QWlrootsIntegration;
    friend class WSeat;
    friend class WSeatPrivate;
    void setQtDevice(QInputDevice *device);

    void setExclusiveGrabber(QObject *grabber);
    QObject *exclusiveGrabber() const;

    QObject *hoverTarget() const;
    void setHoverTarget(QObject *object);
    void handleDestroyed(WInputDevice * = nullptr);

private:
    friend class WInputDevicePrivate;
    std::unique_ptr<WInputDevicePrivate> d;
};

WAYLIB_SERVER_END_NAMESPACE
