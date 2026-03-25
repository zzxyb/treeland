// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickoutputlayout.h"
#include "private/woutputlayout_p.h"
#include "woutputitem.h"
#include "woutput.h"
#include "woutputlayout.h"

#include <QQuickWindow>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WQuickOutputLayoutPrivate : public WOutputLayoutPrivate
{
public:
    WQuickOutputLayoutPrivate(WQuickOutputLayout *qq)
        : WOutputLayoutPrivate(qq)
    {}

    WQuickOutputLayout *q() const { return static_cast<WQuickOutputLayout*>(WOutputLayoutPrivate::q); }

    QList<WOutputItem*> outputItems;
};

WQuickOutputLayout::WQuickOutputLayout(WServer *server)
    : WOutputLayout(new WQuickOutputLayoutPrivate(this), server)
{
}

const QList<WOutputItem*> &WQuickOutputLayout::outputs() const
{
    return static_cast<const WQuickOutputLayoutPrivate*>(d.get())->outputItems;
}

void WQuickOutputLayout::add(WOutputItem *output)
{
    auto *dq = static_cast<WQuickOutputLayoutPrivate*>(d.get());
    Q_ASSERT(!dq->outputItems.contains(output));
    dq->outputItems.append(output);
    WOutputLayout::add(output->output(), output->globalPosition().toPoint());

    auto updateOutput = [dq, this] {
        auto *output = qobject_cast<WOutputItem*>(sender());
        if (!output) // Maybe output has destroyed but event still in queue
            return;
        Q_ASSERT(dq->outputItems.contains(output));
        move(output->output(), output->globalPosition().toPoint());
        Q_EMIT maybeLayoutChanged();
    };

    connect(output, &WOutputItem::maybeGlobalPositionChanged, this, updateOutput, Qt::QueuedConnection);
    connect(output, &WOutputItem::transformChanged, this, &WQuickOutputLayout::maybeLayoutChanged);
    output->output()->setLayout(this);

    Q_EMIT outputsChanged();
    Q_EMIT maybeLayoutChanged();
}

void WQuickOutputLayout::remove(WOutputItem *output)
{
    auto *dq = static_cast<WQuickOutputLayoutPrivate*>(d.get());

    if (!dq->outputItems.removeOne(output))
        return;
    output->disconnect(this);

    if (auto o = output->output()) {
        WOutputLayout::remove(o);
    }

    Q_EMIT outputsChanged();
    Q_EMIT maybeLayoutChanged();
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wquickoutputlayout.cpp"
