// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QSGMaterial>
#include <QSGTexture>

class RadiusOpaqueTextureMaterialRhiShader : public QSGMaterialShader
{
public:
    RadiusOpaqueTextureMaterialRhiShader(int viewCount);

    bool updateUniformData(RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial) override;
    void updateSampledImage(RenderState &state, int binding, QSGTexture **texture, QSGMaterial *newMaterial, QSGMaterial *oldMaterial) override;
};

class RadiusTextureMaterialRhiShader : public RadiusOpaqueTextureMaterialRhiShader
{
public:
    RadiusTextureMaterialRhiShader(int viewCount);

    bool updateUniformData(RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial) override;
};

class RadiusOpaqueTextureMaterial : public QSGMaterial
{
public:
    RadiusOpaqueTextureMaterial();

    QSGMaterialType *type() const override;
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode renderMode) const override;
    int compare(const QSGMaterial *other) const override;

    void setTexture(QSGTexture *texture);
    QSGTexture *texture() const { return m_texture; }

    void setMipmapFiltering(QSGTexture::Filtering filteringType) { m_mipmap_filtering = filteringType; }
    QSGTexture::Filtering mipmapFiltering() const { return QSGTexture::Filtering(m_mipmap_filtering); }

    void setFiltering(QSGTexture::Filtering filteringType) { m_filtering = filteringType; }
    QSGTexture::Filtering filtering() const { return QSGTexture::Filtering(m_filtering); }

    void setHorizontalWrapMode(QSGTexture::WrapMode mode) { m_horizontal_wrap = mode; }
    QSGTexture::WrapMode horizontalWrapMode() const { return QSGTexture::WrapMode(m_horizontal_wrap); }

    void setVerticalWrapMode(QSGTexture::WrapMode mode) { m_vertical_wrap = mode; }
    QSGTexture::WrapMode verticalWrapMode() const { return QSGTexture::WrapMode(m_vertical_wrap); }

    void setAnisotropyLevel(QSGTexture::AnisotropyLevel level) { m_anisotropy_level = level; }
    QSGTexture::AnisotropyLevel anisotropyLevel() const { return QSGTexture::AnisotropyLevel(m_anisotropy_level); }

protected:
    QSGTexture *m_texture;

    uint m_filtering: 2;
    uint m_mipmap_filtering: 2;
    uint m_horizontal_wrap : 1;
    uint m_vertical_wrap: 1;
    uint m_anisotropy_level : 3;
    uint m_reserved : 23;
};

class RadiusTextureMaterial : public RadiusOpaqueTextureMaterial
{
public:
    QSGMaterialType *type() const override;
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode renderMode) const override;
};
