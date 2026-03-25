// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "woutputlayout.h"

extern "C" {
#include <wlr/types/wlr_output_layout.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WOutputLayoutPrivate
{
public:
    WOutputLayoutPrivate(WOutputLayout *qq) : q(qq) {}
    virtual ~WOutputLayoutPrivate();

    void doAdd(WOutput *output);
    void updateImplicitSize();

    WOutputLayout *q;
    wlr_output_layout *handle = nullptr;
    QList<WOutput*> outputs;

    int implicitWidth { 0 };
    int implicitHeight { 0 };
};

WAYLIB_SERVER_END_NAMESPACE
