// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "gammabrightness.h"

#include "common/treelandlogging.h"
#include "output.h"

#include <QPointer>
#include <QVector>
#include <QTimer>

#include <woutputhelper.h>
#include <woutputrenderwindow.h>
#include <wlr_all.h>

#include <algorithm>
#include <cmath>

namespace {
static inline void kelvinToRGB(double kelvin, double &r, double &g, double &b)
{
    kelvin = std::clamp(kelvin, 1000.0, 20000.0) / 100.0;

    if (kelvin <= 66.0) r = 1.0;
    else r = std::clamp(329.698727446 * std::pow(kelvin - 60.0, -0.1332047592) / 255.0, 0.0, 1.0);

    if (kelvin <= 66.0)
        g = std::clamp((99.4708025861 * std::log(kelvin) - 161.1195681661) / 255.0, 0.0, 1.0);
    else
        g = std::clamp(288.1221695283 * std::pow(kelvin - 60.0, -0.0755148492) / 255.0, 0.0, 1.0);

    if (kelvin >= 66.0) b = 1.0;
    else if (kelvin <= 19.0) b = 0.0;
    else b = std::clamp((138.5177312231 * std::log(kelvin - 10.0) - 305.0447927307) / 255.0, 0.0, 1.0);
}

static inline void generateGammaLUT(uint32_t colorTemperature,
                                    qreal brightness,
                                    size_t gammaSize,
                                    QVector<uint16_t> &r,
                                    QVector<uint16_t> &g,
                                    QVector<uint16_t> &b)
{
    if (gammaSize < 2) {
        return;
    }

    double cr, cg, cb;
    kelvinToRGB(static_cast<double>(colorTemperature), cr, cg, cb);
    for (size_t i = 0; i < gammaSize; ++i) {
        double normalized = static_cast<double>(i) / static_cast<double>(gammaSize - 1);

        double rValue = std::clamp(normalized * cr * brightness, 0.0, 1.0);
        double gValue = std::clamp(normalized * cg * brightness, 0.0, 1.0);
        double bValue = std::clamp(normalized * cb * brightness, 0.0, 1.0);
        r[i] = static_cast<uint16_t>(std::round(rValue * 65535.0));
        g[i] = static_cast<uint16_t>(std::round(gValue * 65535.0));
        b[i] = static_cast<uint16_t>(std::round(bValue * 65535.0));
    }
}

}

GammaBrightness::GammaBrightness(Output *output)
    : DisplayBrightness(output)
{}

bool GammaBrightness::isSupported(Output *output)
{
    return wlr_output_get_gamma_size(output->output()->handle()) > 1;
}

uint GammaBrightness::colorTemperature() const
{
    return m_colorTemperature;
}

void GammaBrightness::setColorTemperature(uint temperature)
{
    m_pendingTemperature = qBound(1000u, temperature, 20000u);
    scheduleColorUpdate();
}

void GammaBrightness::applyBrightness(qreal brightness)
{
    m_pendingBrightness = brightness;
    scheduleColorUpdate();
}

void GammaBrightness::scheduleColorUpdate()
{
    if (m_scheduled || m_committing)
        return;
    m_scheduled = true;
    QTimer::singleShot(0, this, &GammaBrightness::applyColor);
}

void GammaBrightness::applyColor()
{
    m_scheduled = false;
    m_committing = true;
    m_committingBrightness = m_pendingBrightness;
    m_committingTemperature = m_pendingTemperature;
    auto *output = this->output();
    const size_t gammaSize = wlr_output_get_gamma_size(output->output()->handle());
    auto *viewport = output->screenViewport();
    auto *renderWindow = viewport ? viewport->outputRenderWindow() : nullptr;
    auto *helper = renderWindow ? renderWindow->getOutputHelper(viewport) : nullptr;
    if (gammaSize < 2 || !helper) {
        finishGammaCommit(false);
        return;
    }

    QVector<uint16_t> r(gammaSize), g(gammaSize), b(gammaSize);
    generateGammaLUT(m_committingTemperature, m_committingBrightness, gammaSize, r, g, b);
    auto *transform = wlr_color_transform_init_lut_3x1d(gammaSize, r.constData(),
                                                       g.constData(), b.constData());
    if (!transform) {
        qCWarning(lcTlOutput) << "Failed to create gamma color transform for output" << output->output()->name();
        finishGammaCommit(false);
        return;
    }

    WOutputHelper::ExtraState newState;
    wlr_output_state_set_color_transform(newState.get(), transform);
    wlr_color_transform_unref(transform);
    helper->setExtraState(newState);
    const QPointer<GammaBrightness> guard(this);
    helper->scheduleCommitJob([guard, newState](bool success, WOutputHelper::ExtraState state) {
        if (guard)
            guard->finishGammaCommit(success && state == newState);
    }, WOutputHelper::AfterCommitStage);
    renderWindow->update(viewport);
}

void GammaBrightness::finishGammaCommit(bool applied)
{
    m_committing = false;
    const QPointer<GammaBrightness> guard(this);
    if (applied) {
        const bool temperatureChanged = m_colorTemperature != m_committingTemperature;
        m_colorTemperature = m_committingTemperature;
        updateBrightness(m_committingBrightness);
        if (guard && temperatureChanged)
            Q_EMIT colorTemperatureChanged();
    } else {
        qCWarning(lcTlOutput) << "Failed to commit gamma color transform for output" << output()->output()->name();
        Q_EMIT brightnessChangeFailed();
    }
    if (guard && (m_pendingBrightness != m_committingBrightness
                  || m_pendingTemperature != m_committingTemperature))
        scheduleColorUpdate();
}
