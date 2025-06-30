// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>
#include <wseat.h>
#include <wsurface.h>
#include <woutput.h>

#include <QObject>
#include <QSize>

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

class EXTImageCopyCaptureManagerInterfaceV1Private;
class EXTImageCopyCaptureSessionInterfaceV1;
class EXTImageCopyCaptureFrameInterfaceV1;
class EXTImageCopyCaptureCursorSessionInterfaceV1;

class EXTImageCopyCaptureManagerInterfaceV1 : public QObject , public WServerInterface
{
    Q_OBJECT
public:
    enum Options {
        PaintCursors = 1
    };
    Q_ENUM(Options)

    explicit EXTImageCopyCaptureManagerInterfaceV1(QObject *parent = nullptr);
    ~EXTImageCopyCaptureManagerInterfaceV1() override;

Q_SIGNALS:
    void sessionCreated(EXTImageCopyCaptureSessionInterfaceV1 *session);
    void cursorSessionCreated(EXTImageCopyCaptureCursorSessionInterfaceV1 *session);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
    QByteArrayView interfaceName() const override;

private:
    std::unique_ptr<EXTImageCopyCaptureManagerInterfaceV1Private> d;
};

class EXTImageCopyCaptureSessionInterfaceV1Private;
class EXTImageCopyCaptureSessionInterfaceV1 : public QObject
{
    Q_OBJECT
public:
    explicit EXTImageCopyCaptureSessionInterfaceV1(wl_resource *source, uint32_t options, wl_resource *resource);
    ~EXTImageCopyCaptureSessionInterfaceV1() override;

    // Get the capture source (output or surface)
    wl_resource *source() const;
    uint32_t options() const;
    bool paintCursors() const;

    // Send buffer constraints to client
    void sendBufferSize(const QSize &size);
    void sendShmFormat(uint32_t format);
    void sendDmabufDevice(const QByteArray &device);
    void sendDmabufFormat(uint32_t format, const QByteArray &modifiers);
    void sendDone();
    void sendStopped();

Q_SIGNALS:
    void frameCreated(EXTImageCopyCaptureFrameInterfaceV1 *frame);

private:
    friend class EXTImageCopyCaptureManagerInterfaceV1Private;
    std::unique_ptr<EXTImageCopyCaptureSessionInterfaceV1Private> d;
};

class EXTImageCopyCaptureFrameInterfaceV1Private;
class EXTImageCopyCaptureFrameInterfaceV1 : public QObject
{
    Q_OBJECT
public:
    enum FailureReason {
        Unknown = 0,
        BufferConstraints = 1,
        Permanent = 2
    };
    Q_ENUM(FailureReason)

    explicit EXTImageCopyCaptureFrameInterfaceV1(wl_resource *resource);
    ~EXTImageCopyCaptureFrameInterfaceV1() override;

    // Get attached buffer
    wl_resource *buffer() const;
    QRegion damage() const;

    // Send frame events to client
    void sendTransform(uint32_t transform);
    void sendDamage(const QRect &damage);
    void sendPresentationTime(uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec);
    void sendReady();
    void sendFailed(FailureReason reason);

Q_SIGNALS:
    void bufferAttached(wl_resource *buffer);
    void damageChanged(const QRegion &damage);
    void captureRequested();

private:
    friend class EXTImageCopyCaptureSessionInterfaceV1Private;
    std::unique_ptr<EXTImageCopyCaptureFrameInterfaceV1Private> d;
};

class EXTImageCopyCaptureCursorSessionInterfaceV1Private;
class EXTImageCopyCaptureCursorSessionInterfaceV1 : public QObject
{
    Q_OBJECT
public:
    explicit EXTImageCopyCaptureCursorSessionInterfaceV1(wl_resource *source, wl_resource *pointer, wl_resource *resource);
    ~EXTImageCopyCaptureCursorSessionInterfaceV1() override;

    wl_resource *source() const;
    wl_resource *pointer() const;

    // Send cursor events to client
    void sendEnter(uint32_t serial);
    void sendLeave(uint32_t serial);
    void sendPosition(double x, double y);
    void sendHotspot(int32_t x, int32_t y);

Q_SIGNALS:
    void captureSessionRequested(EXTImageCopyCaptureSessionInterfaceV1 *session);

private:
    friend class EXTImageCopyCaptureManagerInterfaceV1Private;
    std::unique_ptr<EXTImageCopyCaptureCursorSessionInterfaceV1Private> d;
};
