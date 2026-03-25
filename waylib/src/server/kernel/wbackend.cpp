// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wbackend.h"
#include "woutput.h"
#include "wserver.h"
#include "winputdevice.h"
#include "platformplugin/qwlrootsintegration.h"
#include "platformplugin/qwlrootscreen.h"
#include "wcontainerof.h"

#include <QDebug>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WBackendPrivate
{
public:
    WBackendPrivate(WBackend *qq)
        : q(qq)
    {
    }

    static void onNewOutputCallback(wl_listener *listener, void *data);
    static void onNewInputCallback(wl_listener *listener, void *data);
    // static void onInputDestroyCallback(wl_listener *listener, void *data);

    void connect();

    WBackend *q = nullptr;;
    QList<WOutput*> outputList;
    QList<WInputDevice*> inputList;

    struct Keyboard {
        Keyboard(WBackendPrivate *self, wlr_input_device *d)
            : self(self), device(d) {}

        WBackendPrivate *self;
        wlr_input_device *device;

        wl_listener modifiers;
        wl_listener key;
    };

    wlr_session *session = nullptr;
    wlr_backend *handle = nullptr;

    wl_listener newOutputListener;
    wl_listener newInputListener;
    // wl_listener inputDestoryListener;
};

void WBackendPrivate::onNewOutputCallback(wl_listener *listener, void *data)
{
    WBackendPrivate *d =
        containerOf(listener, &WBackendPrivate::newOutputListener);
    wlr_output *output = static_cast<wlr_output *>(data);
    auto woutput = new WOutput(output, d->q);

    d->outputList << woutput;
    QWlrootsIntegration::instance()->addScreen(woutput);

    QObject::connect(woutput, &WOutput::handleDestroyed,
                     d->q, &WBackend::onOutputDestroyed, Qt::SingleShotConnection);

    Q_EMIT d->q->outputAdded(woutput);
}

void WBackendPrivate::onNewInputCallback(wl_listener *listener, void *data)
{
    WBackendPrivate *d =
        containerOf(listener, &WBackendPrivate::newInputListener);
    // W_Q(WBackend);
    // auto qinput_device = qw_input_device::from(device);
    // auto winput_device = new WInputDevice(qinput_device);
    // inputList << winput_device;
    // winput_device->safeConnect(&qw_input_device::before_destroy, q, [this, qinput_device] {
    //     on_input_destroy(qinput_device);
    // });

    // Q_EMIT q->inputAdded(winput_device);
}

// void WBackendPrivate::onInputDestroyCallback(wl_listener *listener, void *data)
// {
//     WBackendPrivate *d =
//         containerOf(listener, &WBackendPrivate::inputDestoryListener);
//     for (int i = 0; i < d->inputList.count(); ++i) {
//         if (d->inputList.at(i)->handle() == data) {
//             auto device = d->inputList.takeAt(i);

//             Q_EMIT d->q->inputRemoved(device);
//             device->safeDeleteLater();
//             return;
//         }
//     }
// }

void WBackendPrivate::connect()
{
    newOutputListener.notify = onNewOutputCallback;
    wl_signal_add(&handle->events.new_output, &newOutputListener);

    newInputListener.notify = onNewInputCallback;
    wl_signal_add(&handle->events.new_input, &newInputListener);
}

WBackend::WBackend(WServer *server, QObject *parent)
    : QObject(parent)
    , d(new WBackendPrivate(this))
{
    if (!d->handle) {
        d->handle = wlr_backend_autocreate(server->eventLoop(), &d->session);
        Q_ASSERT(d->handle);

        d->connect();
    }
}

WBackend::~WBackend()
{
    qDeleteAll(d->inputList);
    qDeleteAll(d->outputList);
    d->inputList.clear();
    d->outputList.clear();
    wl_list_remove(&d->newOutputListener.link);
    wl_list_remove(&d->newInputListener.link);
    wlr_backend_destroy(d->handle);
}

wlr_backend *WBackend::handle() const
{
    return d->handle;
}

wlr_session *WBackend::session() const
{
    return d->session;
}

QList<WOutput*> WBackend::outputList() const
{
    return d->outputList;
}

QList<WInputDevice *> WBackend::inputDeviceList() const
{
    return d->inputList;
}

bool WBackend::isDrm() const
{
    return wlr_backend_is_drm(d->handle);
}

bool WBackend::isX11() const
{
    return wlr_backend_is_x11(d->handle);
}

bool WBackend::isWayland() const
{
    return wlr_backend_is_wl(d->handle);
}

bool WBackend::isSessionActive() const
{
    return d->session && d->session->active;
}

void WBackend::activateSession()
{
    if (d->session) {
        d->session->active = true;
        wl_signal_emit_mutable(&d->session->events.active, nullptr);
    }
}

void WBackend::deactivateSession()
{
    if (d->session) {
        d->session->active = false;
        wl_signal_emit_mutable(&d->session->events.active, nullptr);
    }
}

void WBackend::onOutputDestroyed(WOutput *output)
{
    for (int i = 0; i < d->outputList.count(); ++i) {
        if (d->outputList.at(i) == output) {
            auto woutput = d->outputList.takeAt(i);
            Q_EMIT d->q->outputRemoved(woutput);
            QWlrootsIntegration::instance()->removeScreen(woutput);
            delete woutput;

            return;
        }
    }
}

WAYLIB_SERVER_END_NAMESPACE
