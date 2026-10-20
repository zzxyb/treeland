// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "displaybrightness.h"

#include <QMetaProperty>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>

#include <limits>

class FakeBrightness final : public DisplayBrightness
{
public:
    FakeBrightness() : DisplayBrightness(nullptr) {}
    bool fail = false;

private:
    void applyBrightness(qreal brightness) override
    {
        if (fail)
            Q_EMIT brightnessChangeFailed();
        else
            updateBrightness(brightness);
    }
};

class BrightnessTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesBrightnessProperty()
    {
        FakeBrightness controller;
        const int index = controller.metaObject()->indexOfProperty("brightness");
        QVERIFY(index >= 0);
        const auto property = controller.metaObject()->property(index);
        QVERIFY(property.isReadable());
        QVERIFY(property.isWritable());
        QCOMPARE(property.notifySignal().name(), QByteArray("brightnessChanged"));
        QVERIFY(controller.setProperty("brightness", 0.4));
        QCOMPARE(controller.property("brightness").toReal(), 0.4);
    }

    void notifiesOnlyWhenBrightnessChanges()
    {
        FakeBrightness controller;
        QSignalSpy changed(&controller, &DisplayBrightness::brightnessChanged);
        controller.setBrightness(0.4);
        QCOMPARE(controller.brightness(), 0.4);
        QCOMPARE(changed.size(), 1);
        controller.setBrightness(0.4);
        QCOMPARE(changed.size(), 1);
    }

    void validatesRange()
    {
        FakeBrightness controller;
        controller.setBrightness(-0.5);
        QCOMPARE(controller.brightness(), 0.0);
        controller.setBrightness(2.0);
        QCOMPARE(controller.brightness(), 1.0);
        QSignalSpy failed(&controller, &DisplayBrightness::brightnessChangeFailed);
        controller.setBrightness(std::numeric_limits<qreal>::quiet_NaN());
        QCOMPARE(failed.size(), 1);
        QCOMPARE(controller.brightness(), 1.0);
    }

    void failedWriteDoesNotChangeProperty()
    {
        FakeBrightness controller;
        controller.fail = true;
        QSignalSpy changed(&controller, &DisplayBrightness::brightnessChanged);
        QSignalSpy failed(&controller, &DisplayBrightness::brightnessChangeFailed);
        controller.setBrightness(0.2);
        QCOMPARE(failed.size(), 1);
        QVERIFY(changed.isEmpty());
        QCOMPARE(controller.brightness(), 1.0);
    }

    void notificationCanDestroyController()
    {
        auto *controller = new FakeBrightness;
        QPointer<FakeBrightness> guard(controller);
        connect(controller, &DisplayBrightness::brightnessChanged,
                this, [controller] { delete controller; });
        controller->setBrightness(0.5);
        QVERIFY(guard.isNull());
    }
};

QTEST_GUILESS_MAIN(BrightnessTest)
#include "main.moc"
