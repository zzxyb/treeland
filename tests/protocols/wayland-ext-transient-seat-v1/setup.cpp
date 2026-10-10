// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "state.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "seat/seatmanager.h"
#include <wtransientseatmanagerv1.h>
#include <wcursor.h>
#include <QJsonArray>
#include <QJsonObject>

static SeatManager *manager;

void protocol_test_setup(Helper *helper)
{
    Q_ASSERT(find_server_interface<WTransientSeatManagerV1>(helper));
    manager = helper->seatManager();
    add_headless_output(helper->backend(), false);
}

void transient_read_state(void *data)
{
    auto *state = static_cast<transient_state *>(data);
    *state = {};
    for (auto *seat : manager->seats()) {
        if (!seat->name().startsWith(QStringLiteral("transient-")))
            continue;
        ++state->count;
        if (seat->handle() && seat->cursor() && seat->cursor()->layout()
            && seat->keyboardFocusWindow() && seat->eventFilter())
            ++state->configured;
    }
    const auto saved = manager->saveConfig()["seats"].toArray();
    for (const auto &value : saved) {
        if (value.toObject()["name"].toString().startsWith(QStringLiteral("transient-")))
            ++state->persisted;
    }
}

void transient_remove_seats(void *)
{
    for (auto *seat : manager->seats()) {
        if (seat->name().startsWith(QStringLiteral("transient-")))
            manager->removeSeat(seat);
    }
}
