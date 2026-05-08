// Copyright (C) 2024 lbwtw <xiaoyaobing@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "tsgradiusimagenode.h"

#include <private/qsgtexturematerial_p.h>

static void qsgsimpletexturenode_update(QSGGeometry *g,
                                        QSGTexture *texture,
                                        const QRectF &rect,
                                        QRectF sourceRect,
                                        QSGSimpleTextureNode::TextureCoordinatesTransformMode texCoordMode)
{
    if (!texture)
        return;

    if (!sourceRect.width() || !sourceRect.height()) {
        QSize ts = texture->textureSize();
        sourceRect = QRectF(0, 0, ts.width(), ts.height());
    }

    if (texCoordMode.testFlag(QSGSimpleTextureNode::MirrorHorizontally)) {
        float tmp = sourceRect.left();
        sourceRect.setLeft(sourceRect.right());
        sourceRect.setRight(tmp);
    }
    if (texCoordMode.testFlag(QSGSimpleTextureNode::MirrorVertically)) {
        float tmp = sourceRect.top();
        sourceRect.setTop(sourceRect.bottom());
        sourceRect.setBottom(tmp);
    }

    QSGGeometry::updateTexturedRectGeometry(g, rect, texture->convertToNormalizedSourceRect(sourceRect));
}

TSGRadiusImageNode::TSGRadiusImageNode()
    : QObject(nullptr)
    , QSGGeometryNode()
    , m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
    , texCoordMode(QSGSimpleTextureNode::NoTransform)
    , isAtlasTexture(false)
    , m_ownsTexture(false)
{
    setGeometry(&m_geometry);
    setMaterial(&m_material);
    setOpaqueMaterial(&m_opaque_material);
    m_material.setMipmapFiltering(QSGTexture::None);
    m_opaque_material.setMipmapFiltering(QSGTexture::None);
#ifdef QSG_RUNTIME_DESCRIPTION
    qsgnode_set_description(this, QLatin1String("tradiusimage"));
#endif
}

TSGRadiusImageNode::~TSGRadiusImageNode()
{
    if (m_ownsTexture)
        delete m_material.texture();
}

void TSGRadiusImageNode::setRect(const QRectF &r)
{
    if (m_rect == r)
        return;

    m_rect = r;
    qsgsimpletexturenode_update(&m_geometry, texture(), m_rect, m_sourceRect, texCoordMode);
    markDirty(DirtyGeometry);
}

QRectF TSGRadiusImageNode::rect() const
{
    return m_rect;
}

void TSGRadiusImageNode::setSourceRect(const QRectF &r)
{
    if (m_sourceRect == r)
        return;
    m_sourceRect = r;
    qsgsimpletexturenode_update(&m_geometry, texture(), m_rect, m_sourceRect, texCoordMode);
    markDirty(DirtyGeometry);
}

QRectF TSGRadiusImageNode::sourceRect() const
{
    return m_sourceRect;
}

void TSGRadiusImageNode::setFiltering(QSGTexture::Filtering filtering)
{
    if (m_material.filtering() == filtering)
        return;

    m_material.setFiltering(filtering);
    m_opaque_material.setFiltering(filtering);
    markDirty(DirtyMaterial);
}

QSGTexture::Filtering TSGRadiusImageNode::filtering() const
{
    return m_material.filtering();
}

void TSGRadiusImageNode::setTextureCoordinatesTransform(QSGSimpleTextureNode::TextureCoordinatesTransformMode mode)
{
    if (texCoordMode == mode)
        return;
    texCoordMode = mode;
    qsgsimpletexturenode_update(&m_geometry, texture(), m_rect, m_sourceRect, texCoordMode);
    markDirty(DirtyGeometry | DirtyMaterial);
}

QSGSimpleTextureNode::TextureCoordinatesTransformMode TSGRadiusImageNode::textureCoordinatesTransform() const
{
    return texCoordMode;
}

void TSGRadiusImageNode::setRadius(qreal radius)
{
    if (radius == m_radius) {
        return;
    }

    m_radius = radius;
    markDirty(DirtyGeometry);
}

void TSGRadiusImageNode::setTopLeftRadius(qreal radius)
{
    if (radius == m_topLeftRadius) {
        return;
    }

    m_topLeftRadius = radius;
    markDirty(DirtyGeometry);
}

void TSGRadiusImageNode::setTopRightRadius(qreal radius)
{
    if (radius == m_topRightRadius) {
        return;
    }

    m_topRightRadius = radius;
    markDirty(DirtyGeometry);
}

void TSGRadiusImageNode::setBottomLeftRadius(qreal radius)
{
    if (radius == m_bottomLeftRadius) {
        return;
    }

    m_bottomLeftRadius = radius;
    markDirty(DirtyGeometry);
}

void TSGRadiusImageNode::setBottomRightRadius(qreal radius)
{
    if (radius == m_bottomRightRadius) {
        return;
    }

    m_bottomRightRadius = radius;
    markDirty(DirtyGeometry);
}

void TSGRadiusImageNode::setTextureProvider(QSGTextureProvider *p)
{
    if (p != m_provider) {
        if (m_provider) {
            disconnect(m_provider.data(),
                       &QSGTextureProvider::textureChanged,
                       this,
                       &TSGRadiusImageNode::handleTextureChange);
        }

        m_provider = p;
        m_ownsTexture = false;
        connect(m_provider.data(),
                &QSGTextureProvider::textureChanged,
                this,
                &TSGRadiusImageNode::handleTextureChange);
    }
}

void TSGRadiusImageNode::setOwnsTexture(bool owns)
{
    m_ownsTexture = owns;
}

bool TSGRadiusImageNode::ownsTexture() const
{
    return m_ownsTexture;
}

void TSGRadiusImageNode::setTexture(QSGTexture *texture)
{
    Q_ASSERT(texture);
    if (m_ownsTexture) {
        delete m_material.texture();
    }

    m_material.setTexture(texture);
    m_opaque_material.setTexture(texture);
    qsgsimpletexturenode_update(&m_geometry, texture, m_rect, m_sourceRect, texCoordMode);

    DirtyState dirty = DirtyMaterial;
    // It would be tempting to skip the extra bit here and instead use
    // m_material.texture to get the old state, but that texture could
    // have been deleted in the mean time.
    bool wasAtlas = isAtlasTexture;
    isAtlasTexture = texture->isAtlasTexture();
    if (wasAtlas || isAtlasTexture)
        dirty |= DirtyGeometry;
    markDirty(dirty);
}

QSGTexture *TSGRadiusImageNode::texture() const
{
    return m_material.texture();
}

void TSGRadiusImageNode::handleTextureChange()
{
    setTexture(m_provider->texture());
}
