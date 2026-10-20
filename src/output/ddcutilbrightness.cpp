// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "ddcutilbrightness.h"

#include "common/treelandlogging.h"
#include "output.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QRegularExpression>
#include <QStandardPaths>

#include <cmath>
#include <utility>

namespace {
QByteArray readEdid(const QString &connector)
{
    QFile file(connector + QStringLiteral("/edid"));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QString ddcBus(const QString &connector)
{
    const QString ddcPath = QFileInfo(connector + QStringLiteral("/ddc")).canonicalFilePath();
    if (ddcPath.isEmpty())
        return {};
    const QString name = QFileInfo(ddcPath).fileName();
    static const QRegularExpression busPattern(QStringLiteral("^i2c-([0-9]+)$"));
    const auto match = busPattern.match(name);
    if (match.hasMatch())
        return match.captured(1);

    // Some adapters expose i2c-dev below the DDC device instead of naming the
    // DDC device itself i2c-N. Do not guess when several buses are present.
    const auto buses = QDir(ddcPath + QStringLiteral("/i2c-dev"))
        .entryList({QStringLiteral("i2c-*")}, QDir::Dirs | QDir::NoDotAndDotDot);
    if (buses.size() != 1)
        return {};
    const auto childMatch = busPattern.match(buses.first());
    return childMatch.hasMatch() ? childMatch.captured(1) : QString();
}
}

DdcutilBrightness::DdcutilBrightness(Output *output, const QString &program, const QString &bus,
                                     const QString &connector, const QByteArray &edid)
    : DisplayBrightness(output, -1.0) // Unknown until the first successful DDC operation.
    , m_program(program)
    , m_bus(bus)
    , m_connector(connector)
    , m_edid(edid)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(10000);
    connect(&m_timeout, &QTimer::timeout, this, &DdcutilBrightness::onTimeout);
    connect(&m_process, &QProcess::finished, this, &DdcutilBrightness::onProcessFinished);
    connect(&m_process, &QProcess::errorOccurred, this, &DdcutilBrightness::onProcessError);
}

DdcutilBrightness::~DdcutilBrightness()
{
    m_timeout.stop();
    m_process.disconnect(this);
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}

std::unique_ptr<DdcutilBrightness> DdcutilBrightness::createForOutput(Output *output)
{
    const QString program = QStandardPaths::findExecutable(QStringLiteral("ddcutil"));
    const QString connector = drmConnectorPath(output);
    if (program.isEmpty() || connector.isEmpty())
        return nullptr;

    // Bind to this DRM connector's DDC bus and EDID, never the default display.
    const QString bus = ddcBus(connector);
    const QByteArray edid = readEdid(connector);
    if (bus.isEmpty() || edid.size() < 128)
        return nullptr;
    const QFileInfo device(QStringLiteral("/dev/i2c-%1").arg(bus));
    if (!device.isReadable() || !device.isWritable())
        return nullptr;
    return std::unique_ptr<DdcutilBrightness>(new DdcutilBrightness(output, program, bus, connector, edid));
}

bool DdcutilBrightness::matchesDisplay() const
{
    return ddcBus(m_connector) == m_bus && readEdid(m_connector) == m_edid;
}

void DdcutilBrightness::applyBrightness(qreal brightness)
{
    if (m_process.state() != QProcess::NotRunning) {
        m_pendingBrightness = brightness;
        return;
    }
    if (!matchesDisplay()) {
        qCWarning(lcTlOutput) << "DDC display identity changed for output" << output()->output()->name();
        Q_EMIT brightnessChangeFailed();
        return;
    }
    m_requestedBrightness = brightness;
    m_operation = Operation::Read;
    // Query the range for each request: displays need not use a maximum of 100,
    // and their physical controls may have changed brightness since our last write.
    startCommand({QStringLiteral("getvcp"), QStringLiteral("10")});
}

void DdcutilBrightness::startCommand(const QStringList &arguments)
{
    m_timedOut = false;
    m_process.setProgram(m_program);
    m_process.setArguments(QStringList{QStringLiteral("--bus"), m_bus,
                                       QStringLiteral("--brief")} + arguments);
    m_timeout.start();
    m_process.start();
}

void DdcutilBrightness::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_timeout.stop();
    const QByteArray response = m_process.readAllStandardOutput();
    const QByteArray error = m_process.readAllStandardError();
    if (m_timedOut || exitStatus != QProcess::NormalExit || exitCode != 0) {
        qCWarning(lcTlOutput) << "ddcutil brightness command failed for output" << output()->output()->name()
                              << "bus" << m_bus << "exit code" << exitCode
                              << "timed out" << m_timedOut << error.trimmed();
        finish(false);
        return;
    }
    if (!matchesDisplay()) {
        finish(false);
        return;
    }
    if (m_operation == Operation::Write) {
        m_operation = Operation::Verify;
        startCommand({QStringLiteral("getvcp"), QStringLiteral("10")});
        return;
    }

    static const QRegularExpression valuePattern(
        QStringLiteral("^VCP 10 C ([0-9]+) ([0-9]+)\r?$"),
        QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
    const auto match = valuePattern.match(QString::fromUtf8(response));
    bool currentOk = false;
    bool maximumOk = false;
    const uint current = match.captured(1).toUInt(&currentOk);
    const uint maximum = match.captured(2).toUInt(&maximumOk);
    if (!match.hasMatch() || !currentOk || !maximumOk || maximum == 0
        || maximum > 65535 || current > maximum) {
        qCWarning(lcTlOutput) << "Invalid DDC brightness range for output" << output()->output()->name()
                              << "bus" << m_bus;
        finish(false);
        return;
    }
    if (m_operation == Operation::Verify) {
        const bool verified = maximum == m_maxBrightness && current == m_targetLevel;
        if (!verified)
            qCWarning(lcTlOutput) << "DDC brightness readback mismatch for output" << output()->output()->name()
                                  << "requested" << m_targetLevel << "readback" << current;
        finish(verified);
        return;
    }
    m_maxBrightness = maximum;
    m_targetLevel = qMin(static_cast<uint>(std::ceil(m_requestedBrightness * maximum)), maximum);
    if (current == m_targetLevel) {
        finish(true);
        return;
    }
    m_operation = Operation::Write;
    startCommand({QStringLiteral("setvcp"), QStringLiteral("10"), QString::number(m_targetLevel)});
}

void DdcutilBrightness::onProcessError(QProcess::ProcessError error)
{
    if (error != QProcess::FailedToStart)
        return; // Crashes are reported by finished(), avoiding duplicate completion.
    m_timeout.stop();
    qCWarning(lcTlOutput) << "Failed to start ddcutil for output" << output()->output()->name()
                          << m_process.errorString();
    finish(false);
}

void DdcutilBrightness::onTimeout()
{
    m_timedOut = true;
    m_process.kill();
}

void DdcutilBrightness::finish(bool success)
{
    const auto pending = std::exchange(m_pendingBrightness, std::nullopt);
    const QPointer<DdcutilBrightness> guard(this);
    if (success)
        updateBrightness(m_requestedBrightness);
    else
        Q_EMIT brightnessChangeFailed();
    if (guard && pending && m_process.state() == QProcess::NotRunning)
        setBrightness(*pending);
}
