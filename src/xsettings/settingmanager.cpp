// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "settingmanager.h"
#include "common/treelandlogging.h"
#include <QColor>

const static qreal BASE_DPI = 96;
const static qreal XSETTINGS_BASE_DPI_FIXED = BASE_DPI * 1024;

SettingManager::SettingManager(xcb_connection_t *connection, QObject *parent)
    : QObject(parent)
    , m_resource(new XResource(connection, this))
    , m_settings(new XSettings(connection, this))
{
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Xft_Antialias), 1);
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Xft_Hinting), 1);
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Xft_RGBA), QStringLiteral("rgb"));
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Xft_Antialias), 1);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Xft_Hinting), 1);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Xft_RGBA), QStringLiteral("rgb"));
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Qt_ActiveColor), QColor(31, 231, 110, 255));
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Qt_DarkActiveColor), QColor(2, 202, 76, 255));
    setWindowRadius(6);
}

SettingManager::~SettingManager()
{
}

void SettingManager::setGTKTheme(const QString &themeName)
{
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Gtk_ThemeName), themeName);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Gtk_ThemeName), themeName);
}

QString SettingManager::GTKTheme() const
{
    return m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Gtk_ThemeName)).toString();
}

void SettingManager::setFont(const QString &name)
{
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Gtk_FontName), name);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Gtk_FontName), name);
}

QString SettingManager::font() const
{
    return m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Gtk_FontName)).toString();
}

void SettingManager::setIconTheme(const QString &theme)
{
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Gtk_IconThemeName), theme);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Gtk_IconThemeName), theme);
}

QString SettingManager::iconTheme() const
{
    return m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Gtk_IconThemeName)).toString();
}

void SettingManager::setSoundTheme(const QString &theme)
{
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Net_SoundThemeName), theme);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Net_SoundThemeName), theme);
}

QString SettingManager::soundTheme() const
{
    return m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Net_SoundThemeName)).toString();
}

void SettingManager::setCursorTheme(const QString &theme)
{
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Gtk_CursorThemeName), theme);
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Xcursor_Theme), theme);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Gtk_CursorThemeName), theme);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Xcursor_Theme), theme);
}

QString SettingManager::cursorTheme() const
{
    const auto theme =
        m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Gtk_CursorThemeName));
    if (theme.isValid()) {
        return theme.toString();
    }

    return m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Xcursor_Theme))
        .toString();
}

void SettingManager::setCursorSize(qreal value)
{
    const int size = qRound(value);

    m_resource->setPropertyValue(XResource::toByteArray(XResource::Xcursor_Size), size);
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Gtk_CursorThemeSize), size);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Xcursor_Size), size);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Gtk_CursorThemeSize), size);
}

qreal SettingManager::cursorSize() const
{
    const auto size = m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Xcursor_Size));
    if (size.isValid()) {
        return size.toReal();
    }

    return m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Gtk_CursorThemeSize))
        .toReal();
}

void SettingManager::setDoubleClickInterval(int interval)
{
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Net_DoubleClickTime), interval);
}

void SettingManager::setGlobalScale(qreal scale)
{
    // Xft/DPI carries only the fractional remainder; the integer part is
    // conveyed by Gdk/WindowScalingFactor. GTK/Chromium compute effective
    // scale as (Xft/DPI / UnscaledDPI) * WindowScalingFactor, so Xft/DPI
    // must be UnscaledDPI * (scale / floor(scale)) to avoid double-counting.
    const int windowScalingFactor = qFloor(scale);
    const qreal fractionalScale = windowScalingFactor > 0
                                      ? scale / windowScalingFactor
                                      : scale;

    m_resource->setPropertyValue(XResource::toByteArray(XResource::Xft_DPI), fractionalScale * BASE_DPI);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Gdk_WindowScalingFactor), windowScalingFactor);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Gdk_UnscaledDPI), XSETTINGS_BASE_DPI_FIXED);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Xft_DPI), qRound(fractionalScale * XSETTINGS_BASE_DPI_FIXED));
}

qreal SettingManager::globalScale() const
{
    const qreal dpiRatio = m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Xft_DPI)).toReal() / XSETTINGS_BASE_DPI_FIXED;
    const int scalingFactor = m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Gdk_WindowScalingFactor)).toInt();
    return dpiRatio * scalingFactor;
}

void SettingManager::setWindowRadius(int radius)
{
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::DTK_WindowRadius), radius);
}

int SettingManager::windowRadius() const
{
    bool ok = false;
    const int radius = m_settings->getPropertyValue(XSettings::toByteArray(XSettings::DTK_WindowRadius)).toInt(&ok);
    return ok ? radius : 6;
}

void SettingManager::apply()
{
    m_resource->apply();
    m_settings->apply();
}
