// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpaperswitcheritem.h"

#include "seat/helper.h"
#include "workspace/workspacemodel.h"
#include "wallpapershellinterfacev1.h"
#include "wallpapermanager.h"
#include "workspace.h"
#include "shellhandler.h"
#include "greeterproxy.h"

#include <woutput.h>
#include <wsurfaceitem.h>

#include <QPropertyAnimation>
#include <QTimer>
#include <QLoggingCategory>

WAYLIB_SERVER_USE_NAMESPACE

Q_LOGGING_CATEGORY(qLcWallpaperSwitcher, "treeland.wallpaperSwitcher")

// WallpaperSlot: manages a single wallpaper surface, wrapping WSurfaceItemContent.
class WallpaperSlot : public WSurfaceItemContent
{
    Q_OBJECT

    friend class WallpaperSwitcherItem;

public:
    WallpaperSlot(WallpaperSwitcherItem::WallpaperRole role,
                  QQuickItem *parent)
        : WSurfaceItemContent(parent)
        , m_role(role)
    {
        setLive(true);
    }

    void setSlotRole(WallpaperSwitcherItem::WallpaperRole role) { m_role = role; }
    void setOutput(WOutput *output)
    {
        if (m_output == output)
            return;
        m_output = output;
        updateSurface();
    }
    void setWorkspace(WorkspaceModel *workspace)
    {
        if (m_workspace == workspace)
            return;
        m_workspace = workspace;
        updateSurface();
    }

    void updateSurface()
    {
        if (!m_output)
            return;

        auto config = Helper::instance()->m_wallpaperManager->getOutputConfig(m_output->nativeHandle());

        if (m_role == WallpaperSwitcherItem::Lockscreen) {
            if (config.lockscreenWallpaper != m_source) {
                auto *interface = TreelandWallpaperSurfaceInterfaceV1::get(config.lockscreenWallpaper);
                if (!interface)
                    return;
                m_source = config.lockscreenWallpaper;
                setSurface(interface->wSurface());
                interface->wSurface()->enterOutput(m_output);
                update();
            }
            return;
        }

        if (m_role == WallpaperSwitcherItem::Desktop) {
            if (!m_workspace)
                return;
            for (const auto &wsConfig : std::as_const(config.workspaces)) {
                if (wsConfig.workspaceId == m_workspace->id() &&
                    wsConfig.desktopWallpaper != m_source) {
                    auto *interface = TreelandWallpaperSurfaceInterfaceV1::get(wsConfig.desktopWallpaper);
                    if (!interface)
                        return;
                    m_source = wsConfig.desktopWallpaper;
                    setSurface(interface->wSurface());
                    interface->wSurface()->enterOutput(m_output);
                    update();
                    break;
                }
            }
        }
    }

    void scheduleUpdate()
    {
        if (m_role == WallpaperSwitcherItem::Lockscreen) {
            updateSurface();
        } else {
            QTimer::singleShot(1000, this, [this]() {
                updateSurface();
                if (!Helper::instance()->m_greeterProxy->isLocked())
                    setPlay(false);
            });
        }
    }

    void setPlay(bool value)
    {
        auto *interface = TreelandWallpaperSurfaceInterfaceV1::get(m_source);
        if (!interface)
            return;
        interface->setPlay(value);
    }

    void slowDown()
    {
        auto *interface = TreelandWallpaperSurfaceInterfaceV1::get(m_source);
        if (!interface)
            return;
        interface->slowDown();
    }

    QString source() const { return m_source; }

private:
    WallpaperSwitcherItem::WallpaperRole m_role;
    QPointer<WOutput> m_output;
    QPointer<WorkspaceModel> m_workspace;
    QString m_source;
};

// WallpaperSwitcherItem implementation

WallpaperSwitcherItem::WallpaperSwitcherItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    qCDebug(qLcWallpaperSwitcher) << "WallpaperSwitcherItem created";

    // Create initial slot
    m_currentSlot = new WallpaperSlot(m_wallpaperRole, parentItem());

    connect(Helper::instance()->m_wallpaperManager,
            &WallpaperManager::updateWallpaper,
            this,
            &WallpaperSwitcherItem::handleWallpaperUpdate);
    connect(Helper::instance()->shellHandler()->wallpaperShell(),
            &TreelandWallpaperShellInterfaceV1::wallpaperSurfaceAdded,
            this,
            &WallpaperSwitcherItem::handleWallpaperUpdate);
    connect(Helper::instance()->workspace(),
            &Workspace::workspaceAdded,
            this,
            &WallpaperSwitcherItem::handleWorkspaceAdded);
}

WallpaperSwitcherItem::~WallpaperSwitcherItem()
{
    qCDebug(qLcWallpaperSwitcher) << "WallpaperSwitcherItem destroyed";
    delete m_oldSlot;
    delete m_currentSlot;
}

WOutput *WallpaperSwitcherItem::output() const
{
    return m_output;
}

void WallpaperSwitcherItem::setOutput(WOutput *output)
{
    if (m_output == output)
        return;

    m_output = output;
    Q_EMIT outputChanged();

    if (m_currentSlot)
        m_currentSlot->setOutput(output);
    if (m_oldSlot)
        m_oldSlot->setOutput(output);
}

WorkspaceModel *WallpaperSwitcherItem::workspace() const
{
    return m_workspace;
}

void WallpaperSwitcherItem::setWorkspace(WorkspaceModel *workspace)
{
    if (m_workspace == workspace)
        return;

    m_workspace = workspace;
    Q_EMIT workspaceChanged();

    if (m_currentSlot)
        m_currentSlot->setWorkspace(workspace);
    if (m_oldSlot)
        m_oldSlot->setWorkspace(workspace);
}

WallpaperSwitcherItem::WallpaperRole WallpaperSwitcherItem::wallpaperRole() const
{
    return m_wallpaperRole;
}

void WallpaperSwitcherItem::setWallpaperRole(WallpaperRole role)
{
    if (m_wallpaperRole == role)
        return;

    m_wallpaperRole = role;
    Q_EMIT wallpaperRoleChanged();

    if (m_currentSlot)
        m_currentSlot->setSlotRole(role);
    if (m_oldSlot)
        m_oldSlot->setSlotRole(role);
}

bool WallpaperSwitcherItem::play() const
{
    return m_play;
}

void WallpaperSwitcherItem::setPlay(bool value)
{
    if (m_play == value)
        return;

    m_play = value;
    Q_EMIT playChanged();

    if (m_currentSlot)
        m_currentSlot->setPlay(value);
    if (m_oldSlot)
        m_oldSlot->setPlay(value);
}

QString WallpaperSwitcherItem::source() const
{
    return m_currentSlot ? m_currentSlot->source() : QString();
}

int WallpaperSwitcherItem::opacityDuration() const
{
    return m_opacityDuration;
}

void WallpaperSwitcherItem::setOpacityDuration(int duration)
{
    if (m_opacityDuration == duration)
        return;

    m_opacityDuration = duration;
    Q_EMIT opacityDurationChanged();
}

void WallpaperSwitcherItem::slowDown()
{
    if (m_currentSlot)
        m_currentSlot->slowDown();
}

void WallpaperSwitcherItem::handleWallpaperUpdate()
{
    if (!m_output || !m_currentSlot)
        return;

    auto config = Helper::instance()->m_wallpaperManager->getOutputConfig(m_output->nativeHandle());
    QString newSource;

    if (m_wallpaperRole == Lockscreen) {
        newSource = config.lockscreenWallpaper;
    } else if (m_workspace) {
        for (const auto &wsConfig : std::as_const(config.workspaces)) {
            if (wsConfig.workspaceId == m_workspace->id()) {
                newSource = wsConfig.desktopWallpaper;
                break;
            }
        }
    }

    if (newSource == m_currentSlot->source())
        return;

    qCDebug(qLcWallpaperSwitcher) << "Wallpaper update triggered, switching to new slot";
    switchToNewSlot();
}

void WallpaperSwitcherItem::handleWorkspaceAdded()
{
    if (m_wallpaperRole == WallpaperSwitcherItem::Lockscreen)
        return;
    Helper::instance()->m_wallpaperManager->syncAddWorkspace();
    if (m_currentSlot)
        m_currentSlot->scheduleUpdate();
}

void WallpaperSwitcherItem::switchToNewSlot()
{
    // Step 1: Keep current slot alive as old slot (will fade out)
    m_oldSlot = m_currentSlot;
    m_currentSlot = nullptr;

    // Step 2: Create a new slot (starts invisible)
    auto *newSlot = new WallpaperSlot(m_wallpaperRole, parentItem());
    newSlot->setOutput(m_output);
    newSlot->setWorkspace(m_workspace);
    newSlot->setOpacity(0);
    m_currentSlot = newSlot;

    Q_EMIT sourceChanged();

    // Step 3: Animate - fade in new, fade out old
    auto *fadeIn = new QPropertyAnimation(m_currentSlot, "opacity");
    fadeIn->setDuration(m_opacityDuration);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->setEasingCurve(QEasingCurve::InOutQuad);

    auto *fadeOut = new QPropertyAnimation(m_oldSlot, "opacity");
    fadeOut->setDuration(m_opacityDuration);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);
    fadeOut->setEasingCurve(QEasingCurve::InOutQuad);

    // Step 4: When fade-out finishes, destroy old slot
    connect(fadeOut, &QPropertyAnimation::finished, this, &WallpaperSwitcherItem::onAnimationFinished);

    // Clean up animation objects when done
    QObject::connect(fadeIn, &QPropertyAnimation::finished, fadeIn, &QObject::deleteLater);
    QObject::connect(fadeOut, &QPropertyAnimation::finished, fadeOut, &QObject::deleteLater);

    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
    fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
}

void WallpaperSwitcherItem::onAnimationFinished()
{
    if (m_oldSlot) {
        qCDebug(qLcWallpaperSwitcher) << "Fade animation finished, deleting old wallpaper slot";
        m_oldSlot->deleteLater();
        m_oldSlot = nullptr;
    }
}

#include "wallpaperswitcheritem.moc"
