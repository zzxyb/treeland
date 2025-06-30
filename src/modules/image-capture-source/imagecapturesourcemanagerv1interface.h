// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>
#include <woutput.h>
#include <wsurface.h>

#include <QObject>

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

class EXTImageCaptureSourceInterfaceV1Private;
class EXTOutputImageCaptureSourceManagerInterfaceV1Private;
class EXTForeignToplevelImageCaptureSourceManagerInterfaceV1Private;

// Base image capture source interface
class EXTImageCaptureSourceInterfaceV1 : public QObject
{
    Q_OBJECT
public:
    explicit EXTImageCaptureSourceInterfaceV1(wl_resource *resource);
    ~EXTImageCaptureSourceInterfaceV1() override;

    wl_resource *resource() const;

private:
    std::unique_ptr<EXTImageCaptureSourceInterfaceV1Private> d;
};

// Output image capture source manager
class EXTOutputImageCaptureSourceManagerInterfaceV1 : public QObject, public WServerInterface
{
    Q_OBJECT
public:
    explicit EXTOutputImageCaptureSourceManagerInterfaceV1(QObject *parent = nullptr);
    ~EXTOutputImageCaptureSourceManagerInterfaceV1() override;

Q_SIGNALS:
    void sourceCreated(EXTImageCaptureSourceInterfaceV1 *source, wl_resource *output);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
    QByteArrayView interfaceName() const override;

private:
    std::unique_ptr<EXTOutputImageCaptureSourceManagerInterfaceV1Private> d;
};

// Foreign toplevel image capture source manager
class EXTForeignToplevelImageCaptureSourceManagerInterfaceV1 : public QObject, public WServerInterface
{
    Q_OBJECT
public:
    explicit EXTForeignToplevelImageCaptureSourceManagerInterfaceV1(QObject *parent = nullptr);
    ~EXTForeignToplevelImageCaptureSourceManagerInterfaceV1() override;

Q_SIGNALS:
    void sourceCreated(EXTImageCaptureSourceInterfaceV1 *source, wl_resource *toplevel_handle);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
    QByteArrayView interfaceName() const override;

private:
    std::unique_ptr<EXTForeignToplevelImageCaptureSourceManagerInterfaceV1Private> d;
}; 