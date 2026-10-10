// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wtransientseatmanagerv1.h"
#include "wscoplistener.h"

#include <wlr_all.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

wlr_transient_seat_manager_v1 *WTransientSeatManagerV1::handle() const
{
    return reinterpret_cast<wlr_transient_seat_manager_v1 *>(m_handle);
}

QByteArrayView WTransientSeatManagerV1::interfaceName() const
{
    return "ext_transient_seat_manager_v1";
}

void WTransientSeatManagerV1::create(WServer *server)
{
    if (!m_handle) {
        m_handle = wlr_transient_seat_manager_v1_create(server->handle());
        if (m_handle) {
            listeners()->add(&handle()->events.create_seat,
                             this,
                             &WTransientSeatManagerV1::createSeat);
        }
    }
}

void WTransientSeatManagerV1::destroy(WServer *)
{
    // The display owns the native manager; WServer detaches listeners first.
    m_handle = nullptr;
}

wl_global *WTransientSeatManagerV1::global() const
{
    return handle() ? handle()->global : nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
