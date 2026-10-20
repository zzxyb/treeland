// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "displaybrightness.h"

class Backlight final : public DisplayBrightness
{
public:
    static std::unique_ptr<Backlight> createForOutput(Output *output);

private:
    Backlight(Output *output, const QString &path, qlonglong maximum, qlonglong current);
    void applyBrightness(qreal brightness) override;

    QString m_path;
    qlonglong m_maxBrightness;
};
