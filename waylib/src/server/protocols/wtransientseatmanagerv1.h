// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <wserver.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WTransientSeatManagerV1 : public QObject, public WObject, public WServerInterface
{
    Q_OBJECT
public:
    WTransientSeatManagerV1() = default;
    ~WTransientSeatManagerV1() override { teardown(); }

    wlr_transient_seat_manager_v1 *handle() const;
    QByteArrayView interfaceName() const override;

Q_SIGNALS:
    void createSeat(wlr_transient_seat_v1 *seat);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE
