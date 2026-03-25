// Copyright (C) 2023-2026 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "winputdevice.h"
#include "wseat.h"
#include "wcontainerof.h"

#include <QDebug>
#include <QInputDevice>
#include <QPointer>
#include <QScopeGuard>
#include <QRegularExpression>

#include <private/qpointingdevice_p.h>

#include <libudev.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

// Input device management and events
Q_LOGGING_CATEGORY(waylibInput, "waylib.server.input", QtInfoMsg)

class Q_DECL_HIDDEN WInputDevicePrivate
{
public:
    WInputDevicePrivate(WInputDevice *qq, wlr_input_device *_handle)
        : q(qq)
        , handle(_handle)
    {
        handle->data = qq;
    }

    static void onDestroyCallback(wl_listener *listener, void *data);

    WInputDevice *q;
    QPointer<QInputDevice> qtDevice;
    QPointer<QObject> hoverTarget;
    WSeat *seat = nullptr;
    wlr_input_device *handle = nullptr;

    wl_listener destroyListener;
};

WInputDevice::WInputDevice(wlr_input_device *handle)
    : d(new WInputDevicePrivate(this, handle))
{
    d->destroyListener.notify = WInputDevicePrivate::onDestroyCallback;
    wl_signal_add(&d->handle->events.destroy, &d->destroyListener);
}

WInputDevice::~WInputDevice()
{
    wl_list_remove(&d->destroyListener.link);
    d->seat->detachInputDevice(this);
}

wlr_input_device *WInputDevice::handle() const
{
    return d->handle;
}

WInputDevice *WInputDevice::fromHandle(wlr_input_device *handle)
{
    return static_cast<WInputDevice *>(handle->data);
}

WInputDevice *WInputDevice::from(const QInputDevice *device)
{
    if (device->systemId() < 65536)
        return nullptr;
    return reinterpret_cast<WInputDevice*>(device->systemId());
}

WInputDevice::Type WInputDevice::type() const
{
    switch (d->handle->type) {
    case WLR_INPUT_DEVICE_KEYBOARD: return Type::Keyboard;
    case WLR_INPUT_DEVICE_POINTER: return Type::Pointer;
    case WLR_INPUT_DEVICE_TOUCH: return Type::Touch;
    case WLR_INPUT_DEVICE_TABLET: return Type::Tablet;
    case WLR_INPUT_DEVICE_TABLET_PAD: return Type::TabletPad;
    case WLR_INPUT_DEVICE_SWITCH: return Type::Switch;
    }

    qCWarning(waylibInput) << "Unknown input device type:" << handle()->type
                           << "from device:" << QString::fromUtf8(handle()->name);
    return Type::Unknow;
}

QString WInputDevice::name() const
{
    if (d->handle && d->handle->name) {
        return QString::fromUtf8(d->handle->name);
    }

    if (d->qtDevice) {
        return d->qtDevice->name();
    }

    return QString();
}

void WInputDevice::setSeat(WSeat *seat)
{
    if (d->seat != seat) {
        qCDebug(waylibInput) << "Input device" << QString::fromUtf8(handle()->name)
                            << "assigned to seat:" << (seat ? seat->name() : QString("(null)"));
        d->seat = seat;
    }
}

WSeat *WInputDevice::seat() const
{
    return d->seat;
}

void WInputDevice::setQtDevice(QInputDevice *device)
{
    if (d->qtDevice != device) {
        qCDebug(waylibInput) << "Qt device" << (device ? device->name() : QString("(null)"))
                            << "associated with input device:"
                            << QString::fromUtf8(handle()->name);
        d->qtDevice = device;
    }
}

QInputDevice *WInputDevice::qtDevice() const
{
    return d->qtDevice;
}

QString WInputDevice::devicePath() const
{
    if (d->handle) {
        if (auto libinputDevice = wlr_libinput_get_device_handle(d->handle)) {
            if (auto udevDevice = libinput_device_get_udev_device(libinputDevice)) {
                auto deviceGuard = qScopeGuard([udevDevice] { udev_device_unref(udevDevice); });

                const char* physPath = udev_device_get_property_value(udevDevice, "PHYS");
                if (physPath) {
                    return QString::fromUtf8(physPath);
                }
                const char* devPath = udev_device_get_property_value(udevDevice, "DEVPATH");
                if (devPath) {
                    QString fullDevPath = QString::fromUtf8(devPath);
                    static const QRegularExpression usbRegex(
                        QStringLiteral("/devices/pci\\d+:\\d+/(\\d+:\\d+:\\d+\\.\\d+)/usb\\d+/1-\\d+/1-(\\d+\\.\\d+)/"));
                    auto match = usbRegex.match(fullDevPath);
                    if (match.hasMatch()) {
                        return QString("usb-%1-%2/input0").arg(match.captured(1)).arg(match.captured(2));
                    }
                }
            }
        }
    }
    return QString();
}

void WInputDevice::setExclusiveGrabber(QObject *grabber)
{
    auto pointerDevice = qobject_cast<QPointingDevice*>(d->qtDevice);
    if (!pointerDevice) {
        qCDebug(waylibInput) << "Cannot set exclusive grabber: device is not a pointing device";
        return;
    }
    auto dd = QPointingDevicePrivate::get(pointerDevice);
    if (dd->activePoints.isEmpty()) {
        qCDebug(waylibInput) << "Cannot set exclusive grabber: no active points";
        return;
    }
    auto firstPoint = dd->activePoints.values().first();
    qCDebug(waylibInput) << "Setting exclusive grabber" << grabber
                         << "for device:" << QString::fromUtf8(handle()->name);
    dd->setExclusiveGrabber(nullptr, firstPoint.eventPoint, grabber);
}

QObject *WInputDevice::exclusiveGrabber() const
{
    auto pointerDevice = qobject_cast<QPointingDevice*>(d->qtDevice);
    if (!pointerDevice)
        return nullptr;
    auto dd = QPointingDevicePrivate::get(pointerDevice);
    return dd->firstPointExclusiveGrabber();
}

QObject *WInputDevice::hoverTarget() const
{
    return d->hoverTarget;
}

void WInputDevice::setHoverTarget(QObject *object)
{
    d->hoverTarget = object;
}

void WInputDevicePrivate::onDestroyCallback(wl_listener *listener, void *data)
{
    WInputDevicePrivate *d =
        containerOf(listener, &WInputDevicePrivate::destroyListener);
    Q_EMIT d->q->handleDestroyed(d->q);
}

WAYLIB_SERVER_END_NAMESPACE
