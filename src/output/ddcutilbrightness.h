// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "displaybrightness.h"

#include <QProcess>
#include <QTimer>

#include <optional>

class DdcutilBrightness final : public DisplayBrightness
{
public:
    static std::unique_ptr<DdcutilBrightness> createForOutput(Output *output);
    ~DdcutilBrightness() override;

private:
    DdcutilBrightness(Output *output, const QString &program, const QString &bus,
                      const QString &connector, const QByteArray &edid);
    void applyBrightness(qreal brightness) override;
    bool matchesDisplay() const;
    void startCommand(const QStringList &arguments);
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onTimeout();
    void finish(bool success);

    enum class Operation { Read, Write, Verify };
    QProcess m_process;
    QTimer m_timeout;
    QString m_program;
    QString m_bus;
    QString m_connector;
    QByteArray m_edid;
    Operation m_operation = Operation::Read;
    qreal m_requestedBrightness = 1.0;
    std::optional<qreal> m_pendingBrightness;
    uint m_maxBrightness = 0;
    uint m_targetLevel = 0;
    bool m_timedOut = false;
};
