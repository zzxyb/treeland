// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "mpvvideocontroller.h"
#include "wglobal.h"
#include "woutput.h"

#include <mpv/client.h>
#include <mpv/render_gl.h>

#include <QThread>
#include <QQuickItem>
#include <QQuickFramebufferObject>

class MpvVideoItem;

class MpvRenderer : public QQuickFramebufferObject::Renderer
{
public:
    explicit MpvRenderer(MpvVideoItem *item);
    ~MpvRenderer() = default;

    MpvVideoItem *m_item = nullptr;
    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override;
    void render() override;
};

WAYLIB_SERVER_USE_NAMESPACE

class WorkspaceModel;

class MpvVideoItem : public QQuickFramebufferObject
{
    Q_OBJECT
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QString mediaTitle READ mediaTitle NOTIFY mediaTitleChanged)
    Q_PROPERTY(double position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(QString formattedPosition READ formattedPosition NOTIFY positionChanged)
    Q_PROPERTY(QString formattedDuration READ formattedDuration NOTIFY durationChanged)
    Q_PROPERTY(bool pause READ pause WRITE setPause NOTIFY pauseChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool loopFile READ loopFile WRITE setLoopFile NOTIFY loopFileChanged)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(WorkspaceModel* workspace READ workspace WRITE setWorkspace NOTIFY workspaceChanged FINAL)
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WOutput* output READ output WRITE setOutput NOTIFY outputChanged FINAL)

    QML_NAMED_ELEMENT(MpvVideoItem)
    QML_ADDED_IN_VERSION(1, 0)
public:
    explicit MpvVideoItem(QQuickItem *parent = nullptr);
    ~MpvVideoItem() override;

    enum class AsyncIds {
        None,
        SetVolume,
        GetVolume,
        ExpandText,
    };
    Q_ENUM(AsyncIds)

    enum Property {
        MediaTitle,
        Position,
        Duration,
        Pause,
        Volume,
        Mute,
        LoopFile,
        Speed
    };
    Q_ENUM(Property)

    static QString toString(Property p);

    Renderer *createRenderer() const override;

    QString mediaTitle();

    double position();
    void setPosition(double value);

    double duration();

    QString formattedPosition();
    QString formattedDuration();

    bool pause();
    void setPause(bool value);

    bool loopFile();
    void setLoopFile(bool value);

    int volume();
    void setVolume(int value);

    QString source();
    void setSource(const QString &source);

    double speed();
    void setSpeed(double value);

    WorkspaceModel *workspace();
    void setWorkspace(WorkspaceModel *workspace);

    WOutput *output();
    void setOutput(WOutput *output);

    void startSlowDown();

    Q_INVOKABLE void loadFile(const QString &file);
    Q_INVOKABLE int setPropertyBlocking(const QString &property, const QVariant &value);
    Q_INVOKABLE void setPropertyAsync(const QString &property, const QVariant &value, int id = 0);
    Q_INVOKABLE QVariant getProperty(const QString &property);
    Q_INVOKABLE void getPropertyAsync(const QString &property, int id = 0);
    Q_INVOKABLE QVariant commandBlocking(const QVariant &params);
    Q_INVOKABLE void commandAsync(const QStringList &params, int id = 0);
    Q_INVOKABLE QVariant expandText(const QString &text);
    Q_INVOKABLE int unobserveProperty(uint64_t id);

Q_SIGNALS:
    void mediaTitleChanged();
    void currentUrlChanged();
    void positionChanged();
    void durationChanged();
    void pauseChanged();
    void volumeChanged();
    void sourceChanged();

    void fileStarted();
    void fileLoaded();
    void endFile(QString reason);
    void videoReconfig();

    void ready();
    void observeProperty(const QString &property, mpv_format format, uint64_t id = 0);
    void setProperty(const QString &property, const QVariant &value);
    void command(const QStringList &params);

    void outputChanged();
    void workspaceChanged();
    void loopFileChanged();

    void speedChanged();
private Q_SLOTS:
    void onPropertyChanged(const QString &property, const QVariant &value);
    void onAsyncReply(const QVariant &data, mpv_event event);
    void updatePlaybackSpeed();

private:
   void initConnections();
   QString formatTime(const double time);

private:
    friend class MpvRenderer;

    QThread *m_workerThread = nullptr;
    MpvVideoController *m_mpvController = nullptr;
    mpv_handle *m_mpv = nullptr;
    mpv_render_context *m_mpvGL = nullptr;

    QString m_formattedPosition;
    QString m_formattedDuration;
    QUrl m_file;
    QString m_source;
    QPointer<WorkspaceModel> m_workspace;
    QPointer<WOutput> m_output;
    QTimer *m_speedTimer = nullptr;
    QElapsedTimer m_elapsed;
};
