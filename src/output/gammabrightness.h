// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "displaybrightness.h"

class GammaBrightness final : public DisplayBrightness
{
    Q_OBJECT
    Q_PROPERTY(uint colorTemperature READ colorTemperature WRITE setColorTemperature
               NOTIFY colorTemperatureChanged FINAL)

public:
    explicit GammaBrightness(Output *output);
    static bool isSupported(Output *output);
    uint colorTemperature() const;
    void setColorTemperature(uint temperature);

Q_SIGNALS:
    void colorTemperatureChanged();

private:
    void applyBrightness(qreal brightness) override;
    void applyColor();
    void finishGammaCommit(bool applied);
    void scheduleColorUpdate();

    qreal m_pendingBrightness = 1.0;
    uint m_pendingTemperature = 6500;
    qreal m_committingBrightness = 1.0;
    uint m_committingTemperature = 6500;
    uint m_colorTemperature = 6500;
    bool m_scheduled = false;
    bool m_committing = false;
};
