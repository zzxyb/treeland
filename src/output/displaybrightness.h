// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QString>

#include <memory>

class Output;

class DisplayBrightness : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal brightness READ brightness WRITE setBrightness NOTIFY brightnessChanged FINAL)

public:
    ~DisplayBrightness() override = default;
    static std::unique_ptr<DisplayBrightness> createForOutput(Output *output);

    qreal brightness() const;
    void setBrightness(qreal brightness);

Q_SIGNALS:
    void brightnessChanged();
    void brightnessChangeFailed();

protected:
    explicit DisplayBrightness(Output *output, qreal brightness = 1.0);
    virtual void applyBrightness(qreal brightness) = 0;
    void updateBrightness(qreal brightness);
    static QString drmDevicePath(Output *output);
    static QString drmConnectorPath(Output *output);
    Output *output() const;

private:
    Output *m_output;
    qreal m_brightness;
};
