// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wradiustexturematerial_p.h"

#include <QMatrix4x4>
#include <QSGMaterialShader>
#include <QSGTexture>
#include <rhi/qrhi.h>

#include <cstring>
#include <cmath>

WAYLIB_SERVER_BEGIN_NAMESPACE

namespace {
constexpr qreal pixelAlignmentTolerance = 0.001;

bool isIntegralPixel(qreal value)
{
    return std::abs(value - std::round(value)) <= pixelAlignmentTolerance;
}

QMatrix4x4 pixelAlignedMatrix(const QMatrix4x4 &matrix, const QRect &viewport,
                             const QRectF &rect, const QRectF &sourceRect)
{
    if (viewport.isEmpty() || !rect.isValid() || !sourceRect.isValid()
        || matrix(3, 0) != 0.0f || matrix(3, 1) != 0.0f || matrix(3, 3) <= 0.0f) {
        return matrix;
    }

    // Fractional source crops intentionally sample between texels.
    if (!isIntegralPixel(sourceRect.left()) || !isIntegralPixel(sourceRect.top())
        || !isIntegralPixel(sourceRect.right()) || !isIntegralPixel(sourceRect.bottom())) {
        return matrix;
    }

    const qreal w = matrix(3, 3);
    const qreal halfWidth = viewport.width() * 0.5;
    const qreal halfHeight = viewport.height() * 0.5;
    const QPointF xAxis(matrix(0, 0) * halfWidth / w, matrix(1, 0) * halfHeight / w);
    const QPointF yAxis(matrix(0, 1) * halfWidth / w, matrix(1, 1) * halfHeight / w);
    const bool axisAligned =
        (std::abs(xAxis.y() * rect.width()) <= pixelAlignmentTolerance
         && std::abs(yAxis.x() * rect.height()) <= pixelAlignmentTolerance)
        || (std::abs(xAxis.x() * rect.width()) <= pixelAlignmentTolerance
            && std::abs(yAxis.y() * rect.height()) <= pixelAlignmentTolerance);
    if (!axisAligned
        || std::abs(std::hypot(xAxis.x(), xAxis.y()) * rect.width() - sourceRect.width())
            > pixelAlignmentTolerance
        || std::abs(std::hypot(yAxis.x(), yAxis.y()) * rect.height() - sourceRect.height())
            > pixelAlignmentTolerance) {
        return matrix;
    }

    // Align the rectangle's origin in the physical render-target viewport.
    // Apply one translation to the whole quad so UVs and the rounded mask
    // retain their relative positions and the quad is never stretched.
    const QVector4D origin = matrix * QVector4D(float(rect.x()), float(rect.y()), 0.0f, 1.0f);
    const qreal pixelX = (origin.x() / w + 1.0) * halfWidth + viewport.x();
    const qreal pixelY = (origin.y() / w + 1.0) * halfHeight + viewport.y();
    QMatrix4x4 aligned = matrix;
    aligned(0, 3) += float((std::round(pixelX) - pixelX) * w / halfWidth);
    aligned(1, 3) += float((std::round(pixelY) - pixelY) * w / halfHeight);
    return aligned;
}

bool isPowerOfTwo(int value)
{
    return value > 0 && (value & (value - 1)) == 0;
}

class RadiusTextureMaterialShader final : public QSGMaterialShader
{
public:
    RadiusTextureMaterialShader()
    {
        setShaderFileName(VertexStage,
                          QStringLiteral(":/waylib/shaders/radiustexture.vert.qsb"));
        setShaderFileName(FragmentStage,
                          QStringLiteral(":/waylib/shaders/radiustexture.frag.qsb"));
    }

    bool updateUniformData(RenderState &state, QSGMaterial *newMaterial,
                           QSGMaterial *oldMaterial) override
    {
        Q_UNUSED(oldMaterial);

        const auto *material = static_cast<RadiusTextureMaterial *>(newMaterial);
        struct UniformData {
            float matrix[16];
            float radii[4];
            float params[4];
            float hasAlphaChannel;
        } uniformData;

        const QMatrix4x4 matrix = pixelAlignedMatrix(state.combinedMatrix(), state.viewportRect(),
                                                    material->rect(), material->sourceRect());
        std::memcpy(uniformData.matrix, matrix.constData(), sizeof(uniformData.matrix));

        const QVector4D radii = material->radii();
        uniformData.radii[0] = radii.x();
        uniformData.radii[1] = radii.y();
        uniformData.radii[2] = radii.z();
        uniformData.radii[3] = radii.w();

        const QVector2D viewportSize = material->viewportSize();
        uniformData.params[0] = viewportSize.x();
        uniformData.params[1] = viewportSize.y();
        uniformData.params[2] = state.opacity();
        uniformData.params[3] = material->antialiasing() ? 1.0f : 0.0f;
        const auto *texture = material->texture();
        uniformData.hasAlphaChannel = texture && texture->hasAlphaChannel() ? 1.0f : 0.0f;

        QByteArray *uniformBuffer = state.uniformData();
        Q_ASSERT(uniformBuffer->size() >= qsizetype(sizeof(uniformData)));
        std::memcpy(uniformBuffer->data(), &uniformData, sizeof(uniformData));
        return true;
    }

    void updateSampledImage(RenderState &state, int binding, QSGTexture **texture,
                            QSGMaterial *newMaterial, QSGMaterial *oldMaterial) override
    {
        Q_UNUSED(oldMaterial);
        if (binding != 1)
            return;

        const auto *material = static_cast<RadiusTextureMaterial *>(newMaterial);
        auto *sourceTexture = material->texture();
        if (!sourceTexture) {
            *texture = nullptr;
            return;
        }

        // QSGTexture is often shared by multiple scene graph materials. Set
        // the complete sampler state at binding time, as QSGTextureMaterial
        // does, so another material cannot leave stale filtering behind.
        sourceTexture->setFiltering(material->filtering());
        sourceTexture->setMipmapFiltering(material->mipmapFiltering());
        sourceTexture->setAnisotropyLevel(material->anisotropyLevel());
        sourceTexture->setHorizontalWrapMode(material->horizontalWrapMode());
        sourceTexture->setVerticalWrapMode(material->verticalWrapMode());
        if (!state.rhi()->isFeatureSupported(QRhi::NPOTTextureRepeat)) {
            const QSize size = sourceTexture->textureSize();
            if (!isPowerOfTwo(size.width()) || !isPowerOfTwo(size.height())) {
                sourceTexture->setHorizontalWrapMode(QSGTexture::ClampToEdge);
                sourceTexture->setVerticalWrapMode(QSGTexture::ClampToEdge);
                sourceTexture->setMipmapFiltering(QSGTexture::None);
            }
        }
        sourceTexture->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
        *texture = sourceTexture;
    }
};
}

RadiusTextureMaterial::RadiusTextureMaterial()
{
    setFlag(Blending);
    setFlag(NoBatching);
    setFlag(RequiresFullMatrix);
}

QSGMaterialType *RadiusTextureMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

QSGMaterialShader *RadiusTextureMaterial::createShader(
    QSGRendererInterface::RenderMode renderMode) const
{
    Q_UNUSED(renderMode);
    return new RadiusTextureMaterialShader;
}

int RadiusTextureMaterial::compare(const QSGMaterial *other) const
{
    Q_ASSERT(other && type() == other->type());
    if (this == other)
        return 0;
    return reinterpret_cast<quintptr>(this) < reinterpret_cast<quintptr>(other) ? -1 : 1;
}

void RadiusTextureMaterial::setTexture(QSGTexture *texture)
{
    QSGTextureMaterial::setTexture(texture);
    // The rounded mask needs blending even when the source texture is opaque.
    setFlag(Blending);
}

QRectF RadiusTextureMaterial::rect() const
{
    return m_rect;
}

void RadiusTextureMaterial::setRect(const QRectF &rect)
{
    m_rect = rect;
}

QRectF RadiusTextureMaterial::sourceRect() const
{
    return m_sourceRect;
}

void RadiusTextureMaterial::setSourceRect(const QRectF &rect)
{
    m_sourceRect = rect;
}

QVector4D RadiusTextureMaterial::radii() const
{
    return m_radii;
}

void RadiusTextureMaterial::setRadii(const QVector4D &radii)
{
    m_radii = radii;
}

QVector2D RadiusTextureMaterial::viewportSize() const
{
    return m_viewportSize;
}

void RadiusTextureMaterial::setViewportSize(const QVector2D &size)
{
    m_viewportSize = size;
}

bool RadiusTextureMaterial::antialiasing() const
{
    return m_antialiasing;
}

void RadiusTextureMaterial::setAntialiasing(bool antialiasing)
{
    m_antialiasing = antialiasing;
}

WAYLIB_SERVER_END_NAMESPACE
