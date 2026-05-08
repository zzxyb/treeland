// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "radiustexturematerial.h"

#include <rhi/qrhi.h>

inline static bool isPowerOfTwo(int x)
{
    return x == (x & -x);
}

RadiusOpaqueTextureMaterialRhiShader::RadiusOpaqueTextureMaterialRhiShader(int viewCount)
{
    setShaderFileName(VertexStage, QStringLiteral(":/qt-project.org/scenegraph/shaders_ng/opaquetexture.vert.qsb"), viewCount);
    setShaderFileName(FragmentStage, QStringLiteral(":/qt-project.org/scenegraph/shaders_ng/opaquetexture.frag.qsb"), viewCount);
}

bool RadiusOpaqueTextureMaterialRhiShader::updateUniformData(RenderState &state, QSGMaterial *newMaterial, QSGMaterial *)
{
    bool changed = false;
    QByteArray *buf = state.uniformData();
    const int matrixCount = qMin(state.projectionMatrixCount(), newMaterial->viewCount());

    for (int viewIndex = 0; viewIndex < matrixCount; ++viewIndex) {
        if (state.isMatrixDirty()) {
            const QMatrix4x4 m = state.combinedMatrix(viewIndex);
            memcpy(buf->data() + 64 * viewIndex, m.constData(), 64);
            changed = true;
        }
    }

    return changed;
}

void RadiusOpaqueTextureMaterialRhiShader::updateSampledImage(RenderState &state, int binding, QSGTexture **texture,
                                                           QSGMaterial *newMaterial, QSGMaterial *oldMaterial)
{
    if (binding != 1)
        return;

#ifdef QT_NO_DEBUG
    Q_UNUSED(oldMaterial);
#endif
    Q_ASSERT(oldMaterial == nullptr || newMaterial->type() == oldMaterial->type());
    RadiusOpaqueTextureMaterial *tx = static_cast<RadiusOpaqueTextureMaterial *>(newMaterial);
    QSGTexture *t = tx->texture();
    if (!t) {
        *texture = nullptr;
        return;
    }

    t->setFiltering(tx->filtering());
    t->setMipmapFiltering(tx->mipmapFiltering());
    t->setAnisotropyLevel(tx->anisotropyLevel());

    t->setHorizontalWrapMode(tx->horizontalWrapMode());
    t->setVerticalWrapMode(tx->verticalWrapMode());
    if (!state.rhi()->isFeatureSupported(QRhi::NPOTTextureRepeat)) {
        QSize size = t->textureSize();
        const bool isNpot = !isPowerOfTwo(size.width()) || !isPowerOfTwo(size.height());
        if (isNpot) {
            t->setHorizontalWrapMode(QSGTexture::ClampToEdge);
            t->setVerticalWrapMode(QSGTexture::ClampToEdge);
            t->setMipmapFiltering(QSGTexture::None);
        }
    }

    t->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
    *texture = t;
}

RadiusOpaqueTextureMaterial::RadiusOpaqueTextureMaterial()
    : m_texture(nullptr)
    , m_filtering(QSGTexture::Nearest)
    , m_mipmap_filtering(QSGTexture::None)
    , m_horizontal_wrap(QSGTexture::ClampToEdge)
    , m_vertical_wrap(QSGTexture::ClampToEdge)
    , m_anisotropy_level(QSGTexture::AnisotropyNone)
{
}

QSGMaterialType *RadiusOpaqueTextureMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

QSGMaterialShader *RadiusOpaqueTextureMaterial::createShader(QSGRendererInterface::RenderMode renderMode) const
{
    Q_UNUSED(renderMode);
    return new RadiusOpaqueTextureMaterialRhiShader(viewCount());
}

void RadiusOpaqueTextureMaterial::setTexture(QSGTexture *texture)
{
    m_texture = texture;
    setFlag(Blending, m_texture ? m_texture->hasAlphaChannel() : false);
}

int RadiusOpaqueTextureMaterial::compare(const QSGMaterial *o) const
{
    Q_ASSERT(o && type() == o->type());
    const RadiusOpaqueTextureMaterial *other = static_cast<const RadiusOpaqueTextureMaterial *>(o);
    Q_ASSERT(m_texture);
    Q_ASSERT(other->texture());
    const qint64 diff = m_texture->comparisonKey() - other->texture()->comparisonKey();
    if (diff != 0)
        return diff < 0 ? -1 : 1;
    return int(m_filtering) - int(other->m_filtering);
}

QSGMaterialType *RadiusTextureMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

QSGMaterialShader *RadiusTextureMaterial::createShader(QSGRendererInterface::RenderMode renderMode) const
{
    Q_UNUSED(renderMode);
    return new RadiusTextureMaterialRhiShader(viewCount());
}


RadiusTextureMaterialRhiShader::RadiusTextureMaterialRhiShader(int viewCount)
    : RadiusOpaqueTextureMaterialRhiShader(viewCount)
{
    setShaderFileName(VertexStage, QStringLiteral(":/qt-project.org/scenegraph/shaders_ng/texture.vert.qsb"), viewCount);
    setShaderFileName(FragmentStage, QStringLiteral(":/qt-project.org/scenegraph/shaders_ng/texture.frag.qsb"), viewCount);
}

bool RadiusTextureMaterialRhiShader::updateUniformData(RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial)
{
    bool changed = false;
    QByteArray *buf = state.uniformData();
    const int shaderMatrixCount = newMaterial->viewCount();

    if (state.isOpacityDirty()) {
        const float opacity = state.opacity();
        memcpy(buf->data() + 64 * shaderMatrixCount, &opacity, 4);
        changed = true;
    }

    changed |= RadiusOpaqueTextureMaterialRhiShader::updateUniformData(state, newMaterial, oldMaterial);

    return changed;
}
