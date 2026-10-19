// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wradiustexturenode_p.h"

#include <QSGDynamicTexture>
#include <QSGTexture>
#include <QtMath>

WAYLIB_SERVER_BEGIN_NAMESPACE

namespace {
struct CornerRadii {
    qreal topLeft;
    qreal topRight;
    qreal bottomRight;
    qreal bottomLeft;
};

CornerRadii normalizedRadii(const QSizeF &size, CornerRadii radii)
{
    const qreal maximumRadius = qMax<qreal>(0.0, qMin(size.width(), size.height()) * 0.5);
    return { qBound<qreal>(0.0, radii.topLeft, maximumRadius),
             qBound<qreal>(0.0, radii.topRight, maximumRadius),
             qBound<qreal>(0.0, radii.bottomRight, maximumRadius),
             qBound<qreal>(0.0, radii.bottomLeft, maximumRadius) };
}
}

RadiusTextureNode::RadiusTextureNode()
    : m_geometry(QSGGeometry::defaultAttributes_Point2D(), 0)
    , m_textureNode(new QSGSimpleTextureNode)
{
    setFlag(UsePreprocess, true);
    setIsRectangular(false);
    m_geometry.setDrawingMode(QSGGeometry::DrawTriangleFan);
    m_geometry.setVertexDataPattern(QSGGeometry::DynamicPattern);
    setGeometry(&m_geometry);
    appendChildNode(m_textureNode);
}

RadiusTextureNode::~RadiusTextureNode()
{
    if (m_ownsTexture)
        delete m_texture;
}

void RadiusTextureNode::setTexture(QSGTexture *texture)
{
    if (m_provider) {
        disconnect(m_provider, &QSGTextureProvider::textureChanged,
                   this, &RadiusTextureNode::handleTextureChanged);
        m_provider = nullptr;
    }
    setTextureInternal(texture);
}

void RadiusTextureNode::setTextureInternal(QSGTexture *texture)
{
    if (m_texture == texture) {
        // A provider may reuse the wrapper while replacing its native texture.
        if (!texture && m_geometry.vertexCount() != 0)
            m_geometry.allocate(0);
        if (texture)
            m_textureNode->setTexture(texture);
        m_geometryDirty = true;
        markDirty(DirtyGeometry);
        return;
    }

    if (m_ownsTexture)
        delete m_texture;
    m_texture = texture;
    if (texture)
        m_textureNode->setTexture(texture);
    else
        m_textureNode->setRect({});
    m_textureSize = {};
    m_geometryDirty = true;

    if (!m_texture && m_geometry.vertexCount() != 0)
        m_geometry.allocate(0);
    markDirty(DirtyGeometry);
}

QSGTexture *RadiusTextureNode::texture() const
{
    return m_texture;
}

void RadiusTextureNode::setOwnsTexture(bool owns)
{
    m_ownsTexture = owns;
}

bool RadiusTextureNode::ownsTexture() const
{
    return m_ownsTexture;
}

void RadiusTextureNode::setFiltering(QSGTexture::Filtering filtering)
{
    if (m_textureNode->filtering() == filtering)
        return;
    m_textureNode->setFiltering(filtering);
}

QSGTexture::Filtering RadiusTextureNode::filtering() const
{
    return m_textureNode->filtering();
}

void RadiusTextureNode::setMipmapFiltering(QSGTexture::Filtering filtering)
{
    m_mipmapFiltering = filtering;
}

QSGTexture::Filtering RadiusTextureNode::mipmapFiltering() const
{
    return m_mipmapFiltering;
}

void RadiusTextureNode::setAnisotropyLevel(QSGTexture::AnisotropyLevel level)
{
    m_anisotropyLevel = level;
}

QSGTexture::AnisotropyLevel RadiusTextureNode::anisotropyLevel() const
{
    return m_anisotropyLevel;
}

void RadiusTextureNode::setTextureCoordinatesTransform(TextureCoordinatesTransformMode mode)
{
    if (m_texCoordMode == mode)
        return;
    m_texCoordMode = mode;
    m_geometryDirty = true;
    markDirty(DirtyGeometry);
}

RadiusTextureNode::TextureCoordinatesTransformMode
RadiusTextureNode::textureCoordinatesTransform() const
{
    return m_texCoordMode;
}

void RadiusTextureNode::setTextureProvider(QSGTextureProvider *provider)
{
    if (m_provider == provider)
        return;
    if (m_provider) {
        disconnect(m_provider, &QSGTextureProvider::textureChanged,
                   this, &RadiusTextureNode::handleTextureChanged);
    }

    if (m_ownsTexture) {
        setTextureInternal(nullptr);
        setOwnsTexture(false);
    }
    m_provider = provider;
    if (m_provider) {
        connect(m_provider, &QSGTextureProvider::textureChanged,
                this, &RadiusTextureNode::handleTextureChanged, Qt::DirectConnection);
    }
    setTextureInternal(m_provider ? m_provider->texture() : nullptr);
}

void RadiusTextureNode::setRect(const QRectF &rect)
{
    if (m_rect == rect)
        return;
    m_rect = rect;
    m_geometryDirty = true;
    markDirty(DirtyGeometry);
}

void RadiusTextureNode::setViewport(const QRectF &viewport)
{
    if (m_viewport == viewport)
        return;
    m_viewport = viewport;
    m_geometryDirty = true;
    markDirty(DirtyGeometry);
}

void RadiusTextureNode::setSourceRect(const QRectF &rect)
{
    if (m_sourceRect == rect)
        return;
    m_sourceRect = rect;
    m_geometryDirty = true;
    markDirty(DirtyGeometry);
}

void RadiusTextureNode::setSourceOffset(const QPointF &offset)
{
    if (m_sourceOffset == offset)
        return;
    m_sourceOffset = offset;
    m_geometryDirty = true;
    markDirty(DirtyGeometry);
}

void RadiusTextureNode::setRadius(qreal radius)
{
    if (m_radius == radius)
        return;
    m_radius = radius;
    m_geometryDirty = true;
    markDirty(DirtyGeometry);
}

void RadiusTextureNode::setTopLeftRadius(qreal radius)
{
    if (m_topLeftRadius == radius)
        return;
    m_topLeftRadius = radius;
    m_geometryDirty = true;
    markDirty(DirtyGeometry);
}

void RadiusTextureNode::setTopRightRadius(qreal radius)
{
    if (m_topRightRadius == radius)
        return;
    m_topRightRadius = radius;
    m_geometryDirty = true;
    markDirty(DirtyGeometry);
}

void RadiusTextureNode::setBottomLeftRadius(qreal radius)
{
    if (m_bottomLeftRadius == radius)
        return;
    m_bottomLeftRadius = radius;
    m_geometryDirty = true;
    markDirty(DirtyGeometry);
}

void RadiusTextureNode::setBottomRightRadius(qreal radius)
{
    if (m_bottomRightRadius == radius)
        return;
    m_bottomRightRadius = radius;
    m_geometryDirty = true;
    markDirty(DirtyGeometry);
}

void RadiusTextureNode::setAntialiasing(bool antialiasing)
{
    if (m_antialiasing == antialiasing)
        return;
    m_antialiasing = antialiasing;
    m_geometryDirty = true;
    markDirty(DirtyGeometry);
}

void RadiusTextureNode::setSourceSizeScale(qreal widthScale, qreal heightScale)
{
    if (qFuzzyCompare(m_sourceWidthScale, widthScale)
            && qFuzzyCompare(m_sourceHeightScale, heightScale)) {
        return;
    }
    m_sourceWidthScale = widthScale;
    m_sourceHeightScale = heightScale;
    m_geometryDirty = true;
    markDirty(DirtyGeometry);
}

void RadiusTextureNode::preprocess()
{
    if (m_provider && m_texture != m_provider->texture())
        setTextureInternal(m_provider->texture());
    if (!m_texture || !m_rect.isValid()) {
        if (m_geometry.vertexCount() != 0) {
            m_geometry.allocate(0);
            markDirty(DirtyGeometry);
        }
        return;
    }

    if (auto *dynamicTexture = qobject_cast<QSGDynamicTexture *>(m_texture)) {
        if (dynamicTexture->updateTexture())
            m_textureNode->markDirty(DirtyMaterial);
    }

    if (m_textureSize != m_texture->textureSize()) {
        m_textureSize = m_texture->textureSize();
        m_geometryDirty = true;
    }

    if (m_geometryDirty)
        updateGeometry();
}

void RadiusTextureNode::handleTextureChanged()
{
    QSGTexture *providerTexture = m_provider ? m_provider->texture() : nullptr;
    if (m_texture != providerTexture) {
        setTextureInternal(providerTexture);
        return;
    }

    // WSGTextureProvider reuses its QSGTexture wrapper while replacing the
    // underlying QRhiTexture, so a stable pointer still requires rebinding.
    m_textureNode->setTexture(providerTexture);
    m_textureSize = {};
    m_geometryDirty = true;
    markDirty(DirtyGeometry);
}

QRectF RadiusTextureNode::effectiveSourceRect() const
{
    if (m_sourceRect.isValid())
        return m_sourceRect;
    if (!m_texture)
        return {};
    const QSize size = m_texture->textureSize();
    return QRectF(m_sourceOffset,
                  QSizeF(size.width() * m_sourceWidthScale,
                         size.height() * m_sourceHeightScale));
}

void RadiusTextureNode::updateGeometry()
{
    if (!m_texture || !m_rect.isValid())
        return;

    const QRectF viewport = m_viewport.isValid() ? m_viewport : m_rect;
    const auto effective = [this](float radius) {
        return radius >= 0.0f ? radius : m_radius;
    };
    CornerRadii radii { effective(m_topLeftRadius), effective(m_topRightRadius),
                        effective(m_bottomRightRadius), effective(m_bottomLeftRadius) };
    radii = normalizedRadii(viewport.size(), radii);

    constexpr int segmentsPerCorner = 12;
    constexpr int perimeterVertexCount = 4 * (segmentsPerCorner + 1);
    m_geometry.allocate(1 + perimeterVertexCount + 1);
    auto *vertices = m_geometry.vertexDataAsPoint2D();
    int index = 0;
    vertices[index++].set(float(viewport.center().x()), float(viewport.center().y()));

    const auto appendCorner = [&](const QPointF &center, qreal radius, qreal startAngle) {
        for (int segment = 0; segment <= segmentsPerCorner; ++segment) {
            const qreal angle = startAngle + M_PI_2 * segment / segmentsPerCorner;
            vertices[index++].set(float(center.x() + std::cos(angle) * radius),
                                  float(center.y() + std::sin(angle) * radius));
        }
    };
    appendCorner({ viewport.left() + radii.topLeft, viewport.top() + radii.topLeft },
                 radii.topLeft, M_PI);
    appendCorner({ viewport.right() - radii.topRight, viewport.top() + radii.topRight },
                 radii.topRight, M_PI + M_PI_2);
    appendCorner({ viewport.right() - radii.bottomRight, viewport.bottom() - radii.bottomRight },
                 radii.bottomRight, 0.0);
    appendCorner({ viewport.left() + radii.bottomLeft, viewport.bottom() - radii.bottomLeft },
                 radii.bottomLeft, M_PI_2);
    vertices[index] = vertices[1];

    m_textureNode->setRect(m_rect);
    m_textureNode->setSourceRect(effectiveSourceRect());
    m_textureNode->setTextureCoordinatesTransform(
        QSGSimpleTextureNode::TextureCoordinatesTransformMode(m_texCoordMode.toInt()));
    setClipRect(viewport);
    m_geometry.markVertexDataDirty();
    m_geometryDirty = false;
    markDirty(DirtyGeometry);
}

WAYLIB_SERVER_END_NAMESPACE
