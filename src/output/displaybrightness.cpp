// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "displaybrightness.h"

#include "backlight.h"
#include "ddcutilbrightness.h"
#include "gammabrightness.h"
#include "output.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <wlr_all.h>

#include <cmath>
#include <sys/stat.h>
#include <sys/sysmacros.h>

DisplayBrightness::DisplayBrightness(Output *output, qreal brightness)
    : m_output(output)
    , m_brightness(brightness)
{}

std::unique_ptr<DisplayBrightness> DisplayBrightness::createForOutput(Output *output)
{
    if (auto backlight = Backlight::createForOutput(output))
        return backlight;
    if (GammaBrightness::isSupported(output))
        return std::make_unique<GammaBrightness>(output);
    return DdcutilBrightness::createForOutput(output);
}

qreal DisplayBrightness::brightness() const
{
    return m_brightness;
}

Output *DisplayBrightness::output() const
{
    return m_output;
}

void DisplayBrightness::setBrightness(qreal brightness)
{
    if (!std::isfinite(brightness)) {
        Q_EMIT brightnessChangeFailed();
        return;
    }
    brightness = qBound(0.0, brightness, 1.0);
    applyBrightness(brightness);
}

void DisplayBrightness::updateBrightness(qreal brightness)
{
    if (qFuzzyCompare(m_brightness, brightness))
        return;
    m_brightness = brightness;
    Q_EMIT brightnessChanged();
}

QString DisplayBrightness::drmDevicePath(Output *output)
{
    auto *handle = output->output()->handle();
    if (!wlr_output_is_drm(handle))
        return {};

    struct stat info;
    const int fd = wlr_backend_get_drm_fd(handle->backend);
    if (fd < 0 || fstat(fd, &info) != 0)
        return {};
    return QFileInfo(QStringLiteral("/sys/dev/char/%1:%2")
        .arg(major(info.st_rdev)).arg(minor(info.st_rdev))).canonicalFilePath();
}

QString DisplayBrightness::drmConnectorPath(Output *output)
{
    const QString devicePath = drmDevicePath(output);
    if (devicePath.isEmpty())
        return {};
    auto *handle = output->output()->handle();

    const uint32_t connectorId = wlr_drm_connector_get_id(handle);
    const QDir drmDir(QStringLiteral("/sys/class/drm"));
    const auto entries = drmDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &entry : entries) {
        if (QFileInfo(entry.canonicalFilePath() + QStringLiteral("/..")).canonicalFilePath() != devicePath)
            continue;
        QFile idFile(entry.filePath() + QStringLiteral("/connector_id"));
        if (!idFile.open(QIODevice::ReadOnly))
            continue;
        bool ok = false;
        const auto id = idFile.readAll().trimmed().toUInt(&ok);
        if (ok && id == connectorId)
            return entry.filePath();
    }
    return {};
}
