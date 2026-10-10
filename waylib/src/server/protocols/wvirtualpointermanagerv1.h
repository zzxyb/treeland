// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include <WServer>
#include <QObject>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WVirtualPointerManagerV1Private;
class WAYLIB_SERVER_EXPORT WVirtualPointerManagerV1 : public QObject, public WObject, public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WVirtualPointerManagerV1)

public:
    explicit WVirtualPointerManagerV1();

    wlr_virtual_pointer_manager_v1 *handle() const;
    QByteArrayView interfaceName() const override;

Q_SIGNALS:
    void newVirtualPointer(wlr_virtual_pointer_v1_new_pointer_event *event);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE
