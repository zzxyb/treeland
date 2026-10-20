// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "backlight.h"

#include "common/treelandlogging.h"
#include "output.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <wlr_all.h>

#include <cmath>
#include <optional>

namespace {
std::optional<qlonglong> readLevel(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    bool ok = false;
    const auto value = file.readAll().trimmed().toLongLong(&ok);
    return ok ? std::optional<qlonglong>(value) : std::nullopt;
}

QString readAttribute(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()).trimmed() : QString();
}

bool isInternalConnector(const QString &name)
{
    return name.startsWith(QStringLiteral("eDP-")) || name.startsWith(QStringLiteral("LVDS-"))
        || name.startsWith(QStringLiteral("DSI-"));
}

bool isWithinDevice(const QString &path, const QString &device)
{
    return !path.isEmpty() && !device.isEmpty()
        && (path == device || path.startsWith(device + QLatin1Char('/')));
}

int backlightTypePriority(const QString &type)
{
    if (type == QStringLiteral("firmware"))
        return 2;
    if (type == QStringLiteral("platform"))
        return 1;
    if (type == QStringLiteral("raw"))
        return 0;
    return -1;
}

int connectedInternalPanels(const QString &cardPath = {})
{
    int count = 0;
    const auto entries = QDir(QStringLiteral("/sys/class/drm"))
        .entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &entry : entries) {
        const QString name = entry.fileName().section(QLatin1Char('-'), 1);
        if (!isInternalConnector(name))
            continue;
        const QString parent = QFileInfo(entry.canonicalFilePath() + QStringLiteral("/..")).canonicalFilePath();
        if (!cardPath.isEmpty() && parent != cardPath)
            continue;
        if (readAttribute(entry.filePath() + QStringLiteral("/status")) == QStringLiteral("connected"))
            ++count;
    }
    return count;
}
}

Backlight::Backlight(Output *output, const QString &path, qlonglong maximum, qlonglong current)
    : DisplayBrightness(output, current / static_cast<qreal>(maximum))
    , m_path(path)
    , m_maxBrightness(maximum)
{}

std::unique_ptr<Backlight> Backlight::createForOutput(Output *output)
{
    auto *handle = output->output()->handle();
    if (!wlr_output_is_drm(handle))
        return nullptr;

    const QString cardPath = drmDevicePath(output);
    const QString gpuPath = QFileInfo(cardPath + QStringLiteral("/device")).canonicalFilePath();
    const QString connectorPath = QFileInfo(drmConnectorPath(output)).canonicalFilePath();
    if (cardPath.isEmpty() || gpuPath.isEmpty() || connectorPath.isEmpty())
        return nullptr;

    const bool internal = isInternalConnector(output->output()->name());
    const int gpuPanels = connectedInternalPanels(cardPath);
    const int allPanels = connectedInternalPanels();
    const auto entries = QDir(QStringLiteral("/sys/class/backlight"))
        .entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    const uint32_t connectorId = wlr_drm_connector_get_id(handle);
    std::unique_ptr<Backlight> selected;
    int selectedPriority = -1;
    for (const auto &entry : entries) {
        const QString path = entry.filePath();
        const QString device = QFileInfo(path + QStringLiteral("/device")).canonicalFilePath();
        const auto id = readLevel(path + QStringLiteral("/device/connector_id"));
        const int typePriority = backlightTypePriority(readAttribute(path + QStringLiteral("/type")));
        if (typePriority < 0 || (id && *id != connectorId))
            continue;

        // Connector IDs are only unique within a DRM device. An exact match
        // must also belong to this GPU, or to the connector itself.
        const bool sameGpu = isWithinDevice(entry.canonicalFilePath(), gpuPath)
            || isWithinDevice(device, gpuPath);
        const bool sameConnector = isWithinDevice(entry.canonicalFilePath(), connectorPath)
            || isWithinDevice(device, connectorPath);
        const bool exactMatch = sameConnector || (id && sameGpu);
        if (!exactMatch) {
            // Untargeted panel backlights must never control an external
            // monitor or be assigned to several internal panels.
            if (!internal || gpuPanels != 1)
                continue;
            if (typePriority == 1) {
                // Platform devices cannot generally be associated with a GPU.
                if (allPanels != 1)
                    continue;
            } else if (!sameGpu) {
                continue;
            }
        }
        if (!internal && typePriority != 0)
            continue;

        const int priority = (exactMatch ? 3 : 0) + typePriority;
        if (priority <= selectedPriority)
            continue;
        const auto maximum = readLevel(path + QStringLiteral("/max_brightness"));
        auto current = readLevel(path + QStringLiteral("/actual_brightness"));
        if (!current)
            current = readLevel(path + QStringLiteral("/brightness"));
        if (!maximum || !current || *maximum <= 0 || *current < 0 || *current > *maximum)
            continue;
        QFile brightnessFile(path + QStringLiteral("/brightness"));
        if (!brightnessFile.open(QIODevice::WriteOnly))
            continue;
        selected.reset(new Backlight(output, path, *maximum, *current));
        selectedPriority = priority;
    }
    return selected;
}

void Backlight::applyBrightness(qreal brightness)
{
    const auto level = qBound<qlonglong>(0, static_cast<qlonglong>(std::ceil(brightness * m_maxBrightness)),
                                       m_maxBrightness);
    const auto current = readLevel(m_path + QStringLiteral("/brightness"));
    if (!current || level != *current) {
        QFile file(m_path + QStringLiteral("/brightness"));
        const QByteArray value = QByteArray::number(level);
        if (!file.open(QIODevice::WriteOnly) || file.write(value) != value.size() || !file.flush()) {
            qCWarning(lcTlOutput) << "Failed to write backlight brightness" << file.fileName()
                                  << file.errorString();
            Q_EMIT brightnessChangeFailed();
            return;
        }
    }
    // Drivers may clamp or reject a requested level. Like libbacklight, read
    // back the requested-brightness attribute after the write; actual_brightness
    // may still lag behind while the panel performs a hardware transition.
    const auto applied = readLevel(m_path + QStringLiteral("/brightness"));
    if (!applied || *applied < 0 || *applied > m_maxBrightness || *applied != level) {
        qCWarning(lcTlOutput) << "Backlight brightness readback mismatch" << m_path
                              << "requested" << level << "readback" << applied.value_or(-1);
        Q_EMIT brightnessChangeFailed();
        return;
    }
    updateBrightness(brightness);
}
