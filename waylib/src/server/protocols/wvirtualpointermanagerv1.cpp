// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wvirtualpointermanagerv1.h"
#include "private/wglobal_p.h"
#include "wscoplistener.h"

#include <wlr_all.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WVirtualPointerManagerV1Private : public WObjectPrivate
{
public:
    explicit WVirtualPointerManagerV1Private(WVirtualPointerManagerV1 *qq)
        : WObjectPrivate(qq)
    {
    }

    W_DECLARE_PUBLIC(WVirtualPointerManagerV1)
};

WVirtualPointerManagerV1::WVirtualPointerManagerV1()
    : WObject(*new WVirtualPointerManagerV1Private(this))
{
}

wlr_virtual_pointer_manager_v1 *WVirtualPointerManagerV1::handle() const
{
    return reinterpret_cast<wlr_virtual_pointer_manager_v1 *>(m_handle);
}

QByteArrayView WVirtualPointerManagerV1::interfaceName() const
{
    return "zwlr_virtual_pointer_manager_v1";
}

void WVirtualPointerManagerV1::create(WServer *server)
{
    if (m_handle)
        return;

    m_handle = wlr_virtual_pointer_manager_v1_create(server->handle());
    Q_ASSERT(m_handle);
    if (m_handle)
        listeners()->add(&handle()->events.new_virtual_pointer, this,
                         &WVirtualPointerManagerV1::newVirtualPointer);
}

void WVirtualPointerManagerV1::destroy([[maybe_unused]] WServer *server)
{
    // wlroots owns the manager until the display is destroyed.
    m_handle = nullptr;
}

wl_global *WVirtualPointerManagerV1::global() const
{
    return handle() ? handle()->global : nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
