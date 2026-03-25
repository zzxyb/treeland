// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WServer>

#include <QObject>

extern "C" {
#include <wlr/backend/session.h>
#include <wlr/backend.h>
#include <wlr/backend/multi.h>
#define static
#include <wlr/backend/drm.h>
#undef static
#include <wlr/backend/wayland.h>
#ifdef WLR_HAVE_X11_BACKEND
#include <wlr/backend/x11.h>
#endif
#include <wlr/backend/libinput.h>
#include <wlr/backend/headless.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutput;
class WInputDevice;
class WBackendPrivate;
class WAYLIB_SERVER_EXPORT WBackend : public QObject
{
    Q_OBJECT
public:
    explicit WBackend(WServer *server, QObject *parent = nullptr);
    ~WBackend() override;
    wlr_backend *handle() const;
    wlr_session *session() const;

    QList<WOutput*> outputList() const;
    QList<WInputDevice*> inputDeviceList() const;

    bool isDrm() const;
    bool isX11() const;
    bool isWayland() const;

    bool isSessionActive() const;
    void activateSession();
    void deactivateSession();

Q_SIGNALS:
    void outputAdded(WOutput *output);
    void outputRemoved(WOutput *output);

    void inputAdded(WInputDevice *input);
    void inputRemoved(WInputDevice *input);

private Q_SLOTS:
    void onOutputDestroyed(WOutput *output);

private:
    friend class WBackendPrivate;
    std::unique_ptr<WBackendPrivate> d;
};

WAYLIB_SERVER_END_NAMESPACE
