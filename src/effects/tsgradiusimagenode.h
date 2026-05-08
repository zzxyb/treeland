// Copyright (C) 2024 lbwtw <xiaoyaobing@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "radiustexturematerial.h"

#include <QPointer>
#include <QSGGeometryNode>
#include <QSGTextureProvider>
#include <QSGSimpleTextureNode>

class TSGRadiusImageNode
    : public QObject
    , public QSGGeometryNode
{
    Q_OBJECT
public:
    TSGRadiusImageNode();
    ~TSGRadiusImageNode() override;

    void setRect(const QRectF &rect);
    inline void setRect(qreal x, qreal y, qreal w, qreal h) { setRect(QRectF(x, y, w, h)); }
    QRectF rect() const;

    void setSourceRect(const QRectF &r);
    inline void setSourceRect(qreal x, qreal y, qreal w, qreal h) { setSourceRect(QRectF(x, y, w, h)); }
    QRectF sourceRect() const;

    void setFiltering(QSGTexture::Filtering filtering);
    QSGTexture::Filtering filtering() const;

    void setTextureCoordinatesTransform(QSGSimpleTextureNode::TextureCoordinatesTransformMode mode);
    QSGSimpleTextureNode::TextureCoordinatesTransformMode textureCoordinatesTransform() const;

    void setRadius(qreal radius);
    void setTopLeftRadius(qreal radius);
    void setTopRightRadius(qreal radius);
    void setBottomLeftRadius(qreal radius);
    void setBottomRightRadius(qreal radius);

    void setTextureProvider(QSGTextureProvider *p);
    void setOwnsTexture(bool owns);
    bool ownsTexture() const;
    void setTexture(QSGTexture *texture);
    QSGTexture *texture() const;

public Q_SLOTS:
    void handleTextureChange();

private:
    QSGGeometry m_geometry;
    RadiusOpaqueTextureMaterial m_opaque_material;
    RadiusTextureMaterial m_material;
    QPointer<QSGTextureProvider> m_provider;

    float m_radius = 0.0f;
    float m_topLeftRadius = -1.0f;
    float m_topRightRadius = -1.0f;
    float m_bottomLeftRadius = -1.0f;
    float m_bottomRightRadius = -1.0f;

    QRectF m_rect;
    QRectF m_sourceRect;
    QSGSimpleTextureNode::TextureCoordinatesTransformMode texCoordMode;
    uint isAtlasTexture : 1;
    uint m_ownsTexture : 1;
};
