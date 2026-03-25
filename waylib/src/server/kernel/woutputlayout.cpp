// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputlayout.h"
#include "private/woutputlayout_p.h"
#include "woutput.h"
#include "wserver.h"

extern "C" {
#include <wlr/types/wlr_output_layout.h>
}

#include <QRect>

WAYLIB_SERVER_BEGIN_NAMESPACE

WOutputLayoutPrivate::~WOutputLayoutPrivate()
{
    for (auto o : std::as_const(outputs)) {
        o->setLayout(nullptr);
    }
    if (handle)
        wlr_output_layout_destroy(handle);
}

void WOutputLayoutPrivate::doAdd(WOutput *output)
{
    Q_ASSERT(!outputs.contains(output));
    outputs.append(output);

    Q_ASSERT(output->layout() == q);

    updateImplicitSize();

    Q_EMIT q->outputAdded(output);
    Q_EMIT q->outputsChanged();
}

void WOutputLayoutPrivate::updateImplicitSize()
{
    wlr_box tmp_box;
    wlr_output_layout_get_box(handle, nullptr, &tmp_box);
    QRect newSize(tmp_box.x, tmp_box.y, tmp_box.width, tmp_box.height);

    if (implicitWidth != newSize.x() + newSize.width()) {
        implicitWidth = newSize.x() + newSize.width();
        Q_EMIT q->implicitWidthChanged();
    }
    if (implicitHeight != newSize.y() + newSize.height()) {
        implicitHeight = newSize.y() + newSize.height();
        Q_EMIT q->implicitHeightChanged();
    }
}

WOutputLayout::WOutputLayout(WOutputLayoutPrivate *dd, WServer *server, QObject *parent)
    : QObject(parent)
    , d(dd)
{
    d->handle = wlr_output_layout_create(server->handle());
    Q_ASSERT(d->handle);
    d->handle->data = this;
}

WOutputLayout::WOutputLayout(WServer *server, QObject *parent)
    : WOutputLayout(new WOutputLayoutPrivate(this), server, parent)
{}

WOutputLayout::~WOutputLayout()
{
    // d->~WOutputLayoutPrivate() handles wlr_output_layout_destroy
}

wlr_output_layout *WOutputLayout::handle() const
{
    return d->handle;
}

const QList<WOutput*> &WOutputLayout::outputs() const
{
    return d->outputs;
}

void WOutputLayout::add(WOutput *output, const QPoint &pos)
{
    output->setLayout(this);
    wlr_output_layout_add(d->handle, output->handle(), pos.x(), pos.y());
    d->doAdd(output);
}

void WOutputLayout::autoAdd(WOutput *output)
{
    output->setLayout(this);
    wlr_output_layout_add_auto(d->handle, output->handle());
    d->doAdd(output);
}

void WOutputLayout::move(WOutput *output, const QPoint &pos)
{
    Q_ASSERT(d->outputs.contains(output));
    Q_ASSERT(output->layout());

    if (output->position() == pos)
        return;

    wlr_output_layout_add(d->handle, output->handle(), pos.x(), pos.y());

    d->updateImplicitSize();
}

void WOutputLayout::remove(WOutput *output)
{
    Q_ASSERT(d->outputs.contains(output));
    d->outputs.removeOne(output);

    wlr_output_layout_remove(d->handle, output->handle());
    output->setLayout(nullptr);
    d->updateImplicitSize();

    Q_EMIT outputRemoved(output);
    Q_EMIT outputsChanged();
}

QList<WOutput*> WOutputLayout::getIntersectedOutputs(const QRect &geometry) const
{
    QList<WOutput*> outputs;

    for (auto o : std::as_const(d->outputs)) {
        wlr_box tmp;
        wlr_output_layout_get_box(d->handle, o->handle(), &tmp);
        const QRect og(tmp.x, tmp.y, tmp.width, tmp.height);
        if (og.intersects(geometry))
            outputs << o;
    }

    return outputs;
}

int WOutputLayout::implicitWidth() const
{
    return d->implicitWidth;
}

int WOutputLayout::implicitHeight() const
{
    return d->implicitHeight;
}

WAYLIB_SERVER_END_NAMESPACE
