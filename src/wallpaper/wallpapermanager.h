// Copyright (C) 2024 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QQmlEngine>
#include <QQuickItem>

namespace WAYLIB_SERVER_NAMESPACE {
class WOutput;
class WOutputItem;
} // namespace WAYLIB_SERVER_NAMESPACE

class WallpaperImage;
class WallpaperController;
class MpvVideoItem;

class WallpaperManager : public QObject
{
    Q_OBJECT
    explicit WallpaperManager(QQuickItem *parent = nullptr);
    ~WallpaperManager();

public:
    static WallpaperManager *instance();

    MpvVideoItem *get(WAYLIB_SERVER_NAMESPACE::WOutputItem *outputItem) const;
    MpvVideoItem *get(WAYLIB_SERVER_NAMESPACE::WOutput *output) const;

private:
    friend class WallpaperImage;
    friend class MpvVideoItem;
    // void add(WallpaperImage *proxy, WAYLIB_SERVER_NAMESPACE::WOutputItem *outputItem);
    // void remove(WallpaperImage *proxy);
    void add(MpvVideoItem *proxy, WAYLIB_SERVER_NAMESPACE::WOutputItem *outputItem);
    void remove(MpvVideoItem *proxy);
    void remove(WAYLIB_SERVER_NAMESPACE::WOutputItem *outputItem);

private:
    friend class WallpaperImage;
    friend class WallpaperController;
    friend class MpvVideoItem;
    bool isLocked(const WallpaperController *controller) const;
    bool isSelfLocked(const WallpaperController *controller) const;

private:
    friend class WallpaperController;
    // WallpaperImage *get(WAYLIB_SERVER_NAMESPACE::WOutputItem *outputItem) const;
    // WallpaperImage *get(WAYLIB_SERVER_NAMESPACE::WOutput *output) const;
    void setLock(WallpaperController *controller, bool lock);

private:
    // QMap<WAYLIB_SERVER_NAMESPACE::WOutputItem *, WallpaperImage *> m_proxys;
    QMap<WAYLIB_SERVER_NAMESPACE::WOutputItem *, MpvVideoItem *> m_proxys;
    QList<WallpaperController *> m_proxyLockList;
};
