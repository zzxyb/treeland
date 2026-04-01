// Copyright (C) 2023 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "personalizationmanager.h"
#include "surfacewrapper.h"

#include "modules/personalization/qwayland-server-treeland-personalization-manager-v1.h"

#include "seat/helper.h"
#include "common/treelandlogging.h"
#include "treelanduserconfig.hpp"

#include <wlayersurface.h>
#include <wxdgpopupsurface.h>
#include <wxdgshell.h>
#include <wxdgsurface.h>

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwlayershellv1.h>
#include <qwsignalconnector.h>
#include <qwxdgshell.h>

#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>

#include <optional>
#include <sys/socket.h>
#include <unistd.h>

extern "C" {
#define static
#include <wlr/types/wlr_compositor.h>
#undef static
}

DCORE_USE_NAMESPACE

static PersonalizationV1 *PERSONALIZATION_MANAGER = nullptr;

#define DEFAULT_WALLPAPER "qrc:/desktop.webp"
#define DEFAULT_WALLPAPER_ISDARK false

static QString defaultBackground()
{
    static QString defaultBg = [] {
        const QString configDefaultBg = Helper::instance()->config()->defaultBackground();
        return QFile::exists(configDefaultBg) ? configDefaultBg : DEFAULT_WALLPAPER;
    }();
    return defaultBg;
}

static std::optional<int32_t> protocolWindowThemeTypeToDConfig(uint32_t type)
{
    switch (type) {
    case TREELAND_PERSONALIZATION_APPEARANCE_CONTEXT_V1_THEME_TYPE_LIGHT:
        return 1;
    case TREELAND_PERSONALIZATION_APPEARANCE_CONTEXT_V1_THEME_TYPE_DARK:
        return 2;
    case TREELAND_PERSONALIZATION_APPEARANCE_CONTEXT_V1_THEME_TYPE_AUTO:
        qCCritical(treelandConfig)
            << "Protocol window theme type AUTO is not supported by dconfig.";
        return std::nullopt;
    default:
        qCWarning(treelandConfig) << "Unknown protocol window theme type:" << type;
        return std::nullopt;
    }
}

static uint32_t dconfigWindowThemeTypeToProtocol(int32_t type)
{
    switch (type) {
    case 1:
        return TREELAND_PERSONALIZATION_APPEARANCE_CONTEXT_V1_THEME_TYPE_LIGHT;
    case 2:
        return TREELAND_PERSONALIZATION_APPEARANCE_CONTEXT_V1_THEME_TYPE_DARK;
    default:
        qCWarning(treelandConfig)
            << "Unknown dconfig windowThemeType:" << type << ", fallback to light.";
        return TREELAND_PERSONALIZATION_APPEARANCE_CONTEXT_V1_THEME_TYPE_LIGHT;
    }
}

// ============================================================================
// PersonalizationManagerV1Private
// ============================================================================

class PersonalizationManagerV1Private
    : public QtWaylandServer::treeland_personalization_manager_v1
{
public:
    PersonalizationManagerV1Private(PersonalizationV1 *_q);
    ~PersonalizationManagerV1Private() override = default;

    wl_global *global() const;
    PersonalizationV1 *q;

protected:
    void bind_resource(Resource *resource) override;
    void destroy_resource(Resource *resource) override;
    void destroy_global() override;
    void get_window_context(Resource *resource, struct ::wl_resource *surface, uint32_t id) override;
    void get_wallpaper_context(Resource *resource, uint32_t id) override;
    void get_cursor_context(Resource *resource, uint32_t id) override;
    void get_font_context(Resource *resource, uint32_t id) override;
    void get_appearance_context(Resource *resource, uint32_t id) override;
};

PersonalizationManagerV1Private::PersonalizationManagerV1Private(PersonalizationV1 *_q)
    : q(_q)
{
}

wl_global *PersonalizationManagerV1Private::global() const
{
    return m_global;
}

void PersonalizationManagerV1Private::bind_resource(Resource *resource)
{
}

void PersonalizationManagerV1Private::destroy_resource(Resource *resource)
{
}

void PersonalizationManagerV1Private::destroy_global()
{
    delete q;
}

void PersonalizationManagerV1Private::get_window_context(Resource *resource,
                                                          struct ::wl_resource *surface,
                                                          uint32_t id)
{
    auto *wlrSurface = wlr_surface_from_resource(surface);
    auto *context = new PersonalizationWindowContextV1(this,
                                                       resource->client(),
                                                       id,
                                                       resource->version(),
                                                       wlrSurface);
    Q_EMIT q->windowContextCreated(context);
}

void PersonalizationManagerV1Private::get_wallpaper_context(Resource *resource, uint32_t id)
{
    auto *context = new PersonalizationWallpaperContextV1(this,
                                                          resource->client(),
                                                          id,
                                                          resource->version());
    Q_EMIT q->onWallpaperContextCreated(context);
}

void PersonalizationManagerV1Private::get_cursor_context(Resource *resource, uint32_t id)
{
    auto *context = new PersonalizationCursorContextV1(this,
                                                       resource->client(),
                                                       id,
                                                       resource->version());
    Q_EMIT q->onCursorContextCreated(context);
}

void PersonalizationManagerV1Private::get_font_context(Resource *resource, uint32_t id)
{
    auto *context = new PersonalizationFontContextV1(this,
                                                     resource->client(),
                                                     id,
                                                     resource->version());
    Q_EMIT q->onFontContextCreated(context);
}

void PersonalizationManagerV1Private::get_appearance_context(Resource *resource, uint32_t id)
{
    auto *context = new PersonalizationAppearanceContextV1(this,
                                                           resource->client(),
                                                           id,
                                                           resource->version());
    Q_EMIT q->onAppearanceContextCreated(context);
}

// ============================================================================
// PersonalizationWindowContextV1Private
// ============================================================================

class PersonalizationWindowContextV1Private
    : public QtWaylandServer::treeland_personalization_window_context_v1
{
public:
    PersonalizationWindowContextV1Private(PersonalizationWindowContextV1 *_q,
                                          PersonalizationManagerV1Private *_manager,
                                          wl_client *client,
                                          uint32_t id,
                                          int version,
                                          wlr_surface *surface);
    ~PersonalizationWindowContextV1Private() override = default;

    PersonalizationWindowContextV1 *q = nullptr;
    PersonalizationManagerV1Private *manager = nullptr;
    wlr_surface *m_surface = nullptr;

    int32_t m_backgroundType = 0;
    int32_t m_cornerRadius = 0;
    Shadow m_shadow;
    Border m_border;
    PersonalizationWindowContextV1::WindowStates m_states;

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void set_blend_mode(Resource *resource, int32_t type) override;
    void set_round_corner_radius(Resource *resource, int32_t radius) override;
    void set_shadow(Resource *resource, int32_t radius, int32_t offset_x, int32_t offset_y, int32_t r, int32_t g, int32_t b, int32_t a) override;
    void set_border(Resource *resource, int32_t width, int32_t r, int32_t g, int32_t b, int32_t a) override;
    void set_titlebar(Resource *resource, int32_t mode) override;
};

PersonalizationWindowContextV1Private::PersonalizationWindowContextV1Private(
    PersonalizationWindowContextV1 *_q,
    PersonalizationManagerV1Private *_manager,
    wl_client *client,
    uint32_t id,
    int version,
    wlr_surface *surface)
    : q(_q)
    , manager(_manager)
    , m_surface(surface)
{
    auto *resource = wl_resource_create(client,
                                        &treeland_personalization_window_context_v1_interface,
                                        version,
                                        id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    init(resource);
}

void PersonalizationWindowContextV1Private::destroy_resource(Resource *resource)
{
    Q_EMIT q->beforeDestroy();
    delete q;
}

void PersonalizationWindowContextV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PersonalizationWindowContextV1Private::set_blend_mode(Resource *resource, int32_t type)
{
    m_backgroundType = type;
    Q_EMIT q->backgroundTypeChanged();
}

void PersonalizationWindowContextV1Private::set_round_corner_radius(Resource *resource, int32_t radius)
{
    m_cornerRadius = radius;
    Q_EMIT q->cornerRadiusChanged();
}

void PersonalizationWindowContextV1Private::set_shadow(Resource *resource,
                                                        int32_t radius,
                                                        int32_t offset_x,
                                                        int32_t offset_y,
                                                        int32_t r,
                                                        int32_t g,
                                                        int32_t b,
                                                        int32_t a)
{
    m_shadow = Shadow{ radius, QPoint{ offset_x, offset_y }, QColor{ r, g, b, a } };
    Q_EMIT q->shadowChanged();
}

void PersonalizationWindowContextV1Private::set_border(Resource *resource,
                                                        int32_t width,
                                                        int32_t r,
                                                        int32_t g,
                                                        int32_t b,
                                                        int32_t a)
{
    m_border = Border{ width, QColor{ r, g, b, a } };
    Q_EMIT q->borderChanged();
}

void PersonalizationWindowContextV1Private::set_titlebar(Resource *resource, int32_t mode)
{
    m_states.setFlag(
        PersonalizationWindowContextV1::WindowState::NoTitleBar,
        mode == TREELAND_PERSONALIZATION_WINDOW_CONTEXT_V1_ENABLE_MODE_DISABLE);
    Q_EMIT q->windowStateChanged();
}

// ============================================================================
// PersonalizationWindowContextV1
// ============================================================================

PersonalizationWindowContextV1::PersonalizationWindowContextV1(
    PersonalizationManagerV1Private *manager,
    wl_client *client,
    uint32_t id,
    int version,
    wlr_surface *surface)
    : QObject(nullptr)
    , d(new PersonalizationWindowContextV1Private(this, manager, client, id, version, surface))
{
}

PersonalizationWindowContextV1::~PersonalizationWindowContextV1() = default;

wlr_surface *PersonalizationWindowContextV1::surface() const
{
    return d->m_surface;
}

int32_t PersonalizationWindowContextV1::backgroundType() const
{
    return d->m_backgroundType;
}

int32_t PersonalizationWindowContextV1::cornerRadius() const
{
    return d->m_cornerRadius;
}

Shadow PersonalizationWindowContextV1::shadow() const
{
    return d->m_shadow;
}

Border PersonalizationWindowContextV1::border() const
{
    return d->m_border;
}

PersonalizationWindowContextV1::WindowStates PersonalizationWindowContextV1::states() const
{
    return d->m_states;
}

PersonalizationWindowContextV1 *PersonalizationWindowContextV1::get(wl_resource *resource)
{
    auto *r = QtWaylandServer::treeland_personalization_window_context_v1::Resource::fromResource(resource);
    if (r && r->object()) {
        return static_cast<PersonalizationWindowContextV1Private *>(r->object())->q;
    }
    return nullptr;
}

// ============================================================================
// PersonalizationWallpaperContextV1Private
// ============================================================================

class PersonalizationWallpaperContextV1Private
    : public QtWaylandServer::treeland_personalization_wallpaper_context_v1
{
public:
    PersonalizationWallpaperContextV1Private(PersonalizationWallpaperContextV1 *_q,
                                             PersonalizationManagerV1Private *_manager,
                                             wl_client *client,
                                             uint32_t id,
                                             int version);
    ~PersonalizationWallpaperContextV1Private() override = default;

    PersonalizationWallpaperContextV1 *q = nullptr;
    PersonalizationManagerV1Private *manager = nullptr;

    int32_t m_fd = -1;
    uint32_t m_uid = 0;
    uint32_t m_options = 0;
    bool m_isDark = false;
    QString m_metaData;
    QString m_identifier;
    QString m_outputName;

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void set_fd(Resource *resource, int32_t fd, const QString &metadata) override;
    void set_identifier(Resource *resource, const QString &identifier) override;
    void set_output(Resource *resource, const QString &output) override;
    void set_on(Resource *resource, uint32_t options) override;
    void set_isdark(Resource *resource, uint32_t isdark) override;
    void commit(Resource *resource) override;
    void get_metadata(Resource *resource) override;
};

PersonalizationWallpaperContextV1Private::PersonalizationWallpaperContextV1Private(
    PersonalizationWallpaperContextV1 *_q,
    PersonalizationManagerV1Private *_manager,
    wl_client *client,
    uint32_t id,
    int version)
    : q(_q)
    , manager(_manager)
{
    auto *resource = wl_resource_create(client,
                                        &treeland_personalization_wallpaper_context_v1_interface,
                                        version,
                                        id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    init(resource);

    wl_client_get_credentials(client, nullptr, &m_uid, nullptr);
}

void PersonalizationWallpaperContextV1Private::destroy_resource(Resource *resource)
{
    Q_EMIT q->beforeDestroy();
    delete q;
}

void PersonalizationWallpaperContextV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PersonalizationWallpaperContextV1Private::set_fd(Resource *resource, int32_t fd, const QString &metadata)
{
    m_fd = fd;
    m_metaData = metadata;
}

void PersonalizationWallpaperContextV1Private::set_identifier(Resource *resource, const QString &identifier)
{
    m_identifier = identifier;
}

void PersonalizationWallpaperContextV1Private::set_output(Resource *resource, const QString &output)
{
    m_outputName = output;
}

void PersonalizationWallpaperContextV1Private::set_on(Resource *resource, uint32_t options)
{
    m_options = options;
}

void PersonalizationWallpaperContextV1Private::set_isdark(Resource *resource, uint32_t isdark)
{
    m_isDark = isdark;
}

void PersonalizationWallpaperContextV1Private::commit(Resource *resource)
{
    Q_EMIT q->commit();
}

void PersonalizationWallpaperContextV1Private::get_metadata(Resource *resource)
{
    Q_EMIT q->getWallpapers();
}

// ============================================================================
// PersonalizationWallpaperContextV1
// ============================================================================

PersonalizationWallpaperContextV1::PersonalizationWallpaperContextV1(
    PersonalizationManagerV1Private *manager,
    wl_client *client,
    uint32_t id,
    int version)
    : QObject(nullptr)
    , d(new PersonalizationWallpaperContextV1Private(this, manager, client, id, version))
{
}

PersonalizationWallpaperContextV1::~PersonalizationWallpaperContextV1() = default;

int32_t PersonalizationWallpaperContextV1::fd() const
{
    return d->m_fd;
}

uint32_t PersonalizationWallpaperContextV1::uid() const
{
    return d->m_uid;
}

uint32_t PersonalizationWallpaperContextV1::options() const
{
    return d->m_options;
}

bool PersonalizationWallpaperContextV1::isDark() const
{
    return d->m_isDark;
}

QString PersonalizationWallpaperContextV1::metaData() const
{
    return d->m_metaData;
}

QString PersonalizationWallpaperContextV1::identifier() const
{
    return d->m_identifier;
}

QString PersonalizationWallpaperContextV1::outputName() const
{
    return d->m_outputName;
}

void PersonalizationWallpaperContextV1::setMetaData(const QString &data)
{
    d->m_metaData = data;
    d->send_metadata(data);
}

PersonalizationWallpaperContextV1 *PersonalizationWallpaperContextV1::get(wl_resource *resource)
{
    auto *r = QtWaylandServer::treeland_personalization_wallpaper_context_v1::Resource::fromResource(resource);
    if (r && r->object()) {
        return static_cast<PersonalizationWallpaperContextV1Private *>(r->object())->q;
    }
    return nullptr;
}

// ============================================================================
// PersonalizationCursorContextV1Private
// ============================================================================

class PersonalizationCursorContextV1Private
    : public QtWaylandServer::treeland_personalization_cursor_context_v1
{
public:
    PersonalizationCursorContextV1Private(PersonalizationCursorContextV1 *_q,
                                          PersonalizationManagerV1Private *_manager,
                                          wl_client *client,
                                          uint32_t id,
                                          int version);
    ~PersonalizationCursorContextV1Private() override = default;

    PersonalizationCursorContextV1 *q = nullptr;
    PersonalizationManagerV1Private *manager = nullptr;

    QSize m_size;
    QString m_theme;

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void set_theme(Resource *resource, const QString &name) override;
    void get_theme(Resource *resource) override;
    void set_size(Resource *resource, uint32_t size) override;
    void get_size(Resource *resource) override;
    void commit(Resource *resource) override;
};

PersonalizationCursorContextV1Private::PersonalizationCursorContextV1Private(
    PersonalizationCursorContextV1 *_q,
    PersonalizationManagerV1Private *_manager,
    wl_client *client,
    uint32_t id,
    int version)
    : q(_q)
    , manager(_manager)
{
    auto *resource = wl_resource_create(client,
                                        &treeland_personalization_cursor_context_v1_interface,
                                        version,
                                        id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    init(resource);
}

void PersonalizationCursorContextV1Private::destroy_resource(Resource *resource)
{
    Q_EMIT q->beforeDestroy();
    delete q;
}

void PersonalizationCursorContextV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PersonalizationCursorContextV1Private::set_theme(Resource *resource, const QString &name)
{
    m_theme = name;
}

void PersonalizationCursorContextV1Private::get_theme(Resource *resource)
{
    Q_EMIT q->getTheme();
}

void PersonalizationCursorContextV1Private::set_size(Resource *resource, uint32_t size)
{
    m_size = QSize(size, size);
}

void PersonalizationCursorContextV1Private::get_size(Resource *resource)
{
    Q_EMIT q->getSize();
}

void PersonalizationCursorContextV1Private::commit(Resource *resource)
{
    Q_EMIT q->commit();
}

// ============================================================================
// PersonalizationCursorContextV1
// ============================================================================

PersonalizationCursorContextV1::PersonalizationCursorContextV1(
    PersonalizationManagerV1Private *manager,
    wl_client *client,
    uint32_t id,
    int version)
    : QObject(nullptr)
    , d(new PersonalizationCursorContextV1Private(this, manager, client, id, version))
{
}

PersonalizationCursorContextV1::~PersonalizationCursorContextV1() = default;

QSize PersonalizationCursorContextV1::size() const
{
    return d->m_size;
}

QString PersonalizationCursorContextV1::theme() const
{
    return d->m_theme;
}

void PersonalizationCursorContextV1::setTheme(const QString &theme)
{
    d->m_theme = theme;
}

void PersonalizationCursorContextV1::setSize(const QSize &size)
{
    d->m_size = size;
}

void PersonalizationCursorContextV1::verify(bool verified)
{
    d->send_verfity(verified);
}

void PersonalizationCursorContextV1::sendTheme()
{
    d->send_theme(d->m_theme);
}

void PersonalizationCursorContextV1::sendSize()
{
    d->send_size(d->m_size.width());
}

PersonalizationCursorContextV1 *PersonalizationCursorContextV1::get(wl_resource *resource)
{
    auto *r = QtWaylandServer::treeland_personalization_cursor_context_v1::Resource::fromResource(resource);
    if (r && r->object()) {
        return static_cast<PersonalizationCursorContextV1Private *>(r->object())->q;
    }
    return nullptr;
}

// ============================================================================
// PersonalizationAppearanceContextV1Private
// ============================================================================

class PersonalizationAppearanceContextV1Private
    : public QtWaylandServer::treeland_personalization_appearance_context_v1
{
public:
    PersonalizationAppearanceContextV1Private(PersonalizationAppearanceContextV1 *_q,
                                              PersonalizationManagerV1Private *_manager,
                                              wl_client *client,
                                              uint32_t id,
                                              int version);
    ~PersonalizationAppearanceContextV1Private() override = default;

    PersonalizationAppearanceContextV1 *q = nullptr;
    PersonalizationManagerV1Private *manager = nullptr;

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void set_round_corner_radius(Resource *resource, int32_t radius) override;
    void get_round_corner_radius(Resource *resource) override;
    void set_icon_theme(Resource *resource, const QString &theme_name) override;
    void get_icon_theme(Resource *resource) override;
    void set_active_color(Resource *resource, const QString &color) override;
    void get_active_color(Resource *resource) override;
    void set_window_opacity(Resource *resource, uint32_t opacity) override;
    void get_window_opacity(Resource *resource) override;
    void set_window_theme_type(Resource *resource, uint32_t type) override;
    void get_window_theme_type(Resource *resource) override;
    void set_window_titlebar_height(Resource *resource, uint32_t height) override;
    void get_window_titlebar_height(Resource *resource) override;
};

PersonalizationAppearanceContextV1Private::PersonalizationAppearanceContextV1Private(
    PersonalizationAppearanceContextV1 *_q,
    PersonalizationManagerV1Private *_manager,
    wl_client *client,
    uint32_t id,
    int version)
    : q(_q)
    , manager(_manager)
{
    auto *resource = wl_resource_create(client,
                                        &treeland_personalization_appearance_context_v1_interface,
                                        version,
                                        id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    init(resource);
}

void PersonalizationAppearanceContextV1Private::destroy_resource(Resource *resource)
{
    Q_EMIT q->beforeDestroy();
    delete q;
}

void PersonalizationAppearanceContextV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PersonalizationAppearanceContextV1Private::set_round_corner_radius(Resource *resource, int32_t radius)
{
    Q_EMIT q->roundCornerRadiusChanged(radius);
}

void PersonalizationAppearanceContextV1Private::get_round_corner_radius(Resource *resource)
{
    Q_EMIT q->requestRoundCornerRadius();
}

void PersonalizationAppearanceContextV1Private::set_icon_theme(Resource *resource, const QString &theme_name)
{
    Q_EMIT q->iconThemeChanged(theme_name);
}

void PersonalizationAppearanceContextV1Private::get_icon_theme(Resource *resource)
{
    Q_EMIT q->requestIconTheme();
}

void PersonalizationAppearanceContextV1Private::set_active_color(Resource *resource, const QString &color)
{
    Q_EMIT q->activeColorChanged(color);
}

void PersonalizationAppearanceContextV1Private::get_active_color(Resource *resource)
{
    Q_EMIT q->requestActiveColor();
}

void PersonalizationAppearanceContextV1Private::set_window_opacity(Resource *resource, uint32_t opacity)
{
    Q_EMIT q->windowOpacityChanged(opacity);
}

void PersonalizationAppearanceContextV1Private::get_window_opacity(Resource *resource)
{
    Q_EMIT q->requestWindowOpacity();
}

void PersonalizationAppearanceContextV1Private::set_window_theme_type(Resource *resource, uint32_t type)
{
    Q_EMIT q->windowThemeTypeChanged(type);
}

void PersonalizationAppearanceContextV1Private::get_window_theme_type(Resource *resource)
{
    Q_EMIT q->requestWindowThemeType();
}

void PersonalizationAppearanceContextV1Private::set_window_titlebar_height(Resource *resource, uint32_t height)
{
    Q_EMIT q->titlebarHeightChanged(height);
}

void PersonalizationAppearanceContextV1Private::get_window_titlebar_height(Resource *resource)
{
    Q_EMIT q->requestWindowTitlebarHeight();
}

// ============================================================================
// PersonalizationAppearanceContextV1
// ============================================================================

PersonalizationAppearanceContextV1::PersonalizationAppearanceContextV1(
    PersonalizationManagerV1Private *manager,
    wl_client *client,
    uint32_t id,
    int version)
    : QObject(nullptr)
    , d(new PersonalizationAppearanceContextV1Private(this, manager, client, id, version))
{
}

PersonalizationAppearanceContextV1::~PersonalizationAppearanceContextV1() = default;

void PersonalizationAppearanceContextV1::setRoundCornerRadius(int32_t radius)
{
    Q_EMIT roundCornerRadiusChanged(radius);
}

void PersonalizationAppearanceContextV1::sendRoundCornerRadius(int32_t radius)
{
    d->send_round_corner_radius(radius);
}

void PersonalizationAppearanceContextV1::setIconTheme(const QString &theme)
{
    Q_EMIT iconThemeChanged(theme);
}

void PersonalizationAppearanceContextV1::sendIconTheme(const QString &theme)
{
    d->send_icon_theme(theme);
}

void PersonalizationAppearanceContextV1::setActiveColor(const QString &color)
{
    Q_EMIT activeColorChanged(color);
}

void PersonalizationAppearanceContextV1::sendActiveColor(const QString &color)
{
    d->send_active_color(color);
}

void PersonalizationAppearanceContextV1::setWindowOpacity(uint32_t opacity)
{
    Q_EMIT windowOpacityChanged(opacity);
}

void PersonalizationAppearanceContextV1::sendWindowOpacity(uint32_t opacity)
{
    d->send_window_opacity(opacity);
}

void PersonalizationAppearanceContextV1::setWindowThemeType(uint32_t type)
{
    Q_EMIT windowThemeTypeChanged(type);
}

void PersonalizationAppearanceContextV1::sendWindowThemeType(uint32_t type)
{
    d->send_window_theme_type(type);
}

void PersonalizationAppearanceContextV1::setWindowTitlebarHeight(uint32_t height)
{
    Q_EMIT titlebarHeightChanged(height);
}

void PersonalizationAppearanceContextV1::sendWindowTitlebarHeight(uint32_t height)
{
    d->send_window_titlebar_height(height);
}

PersonalizationAppearanceContextV1 *PersonalizationAppearanceContextV1::get(wl_resource *resource)
{
    auto *r = QtWaylandServer::treeland_personalization_appearance_context_v1::Resource::fromResource(resource);
    if (r && r->object()) {
        return static_cast<PersonalizationAppearanceContextV1Private *>(r->object())->q;
    }
    return nullptr;
}

// ============================================================================
// PersonalizationFontContextV1Private
// ============================================================================

class PersonalizationFontContextV1Private
    : public QtWaylandServer::treeland_personalization_font_context_v1
{
public:
    PersonalizationFontContextV1Private(PersonalizationFontContextV1 *_q,
                                        PersonalizationManagerV1Private *_manager,
                                        wl_client *client,
                                        uint32_t id,
                                        int version);
    ~PersonalizationFontContextV1Private() override = default;

    PersonalizationFontContextV1 *q = nullptr;
    PersonalizationManagerV1Private *manager = nullptr;

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void set_font(Resource *resource, const QString &font) override;
    void get_font(Resource *resource) override;
    void set_monospace_font(Resource *resource, const QString &font) override;
    void get_monospace_font(Resource *resource) override;
    void set_font_size(Resource *resource, uint32_t size) override;
    void get_font_size(Resource *resource) override;
};

PersonalizationFontContextV1Private::PersonalizationFontContextV1Private(
    PersonalizationFontContextV1 *_q,
    PersonalizationManagerV1Private *_manager,
    wl_client *client,
    uint32_t id,
    int version)
    : q(_q)
    , manager(_manager)
{
    auto *resource = wl_resource_create(client,
                                        &treeland_personalization_font_context_v1_interface,
                                        version,
                                        id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    init(resource);
}

void PersonalizationFontContextV1Private::destroy_resource(Resource *resource)
{
    Q_EMIT q->beforeDestroy();
    delete q;
}

void PersonalizationFontContextV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PersonalizationFontContextV1Private::set_font(Resource *resource, const QString &font)
{
    Q_EMIT q->fontChanged(font);
}

void PersonalizationFontContextV1Private::get_font(Resource *resource)
{
    Q_EMIT q->requestFont();
}

void PersonalizationFontContextV1Private::set_monospace_font(Resource *resource, const QString &font)
{
    Q_EMIT q->monoFontChanged(font);
}

void PersonalizationFontContextV1Private::get_monospace_font(Resource *resource)
{
    Q_EMIT q->requestMonoFont();
}

void PersonalizationFontContextV1Private::set_font_size(Resource *resource, uint32_t size)
{
    Q_EMIT q->fontSizeChanged(size);
}

void PersonalizationFontContextV1Private::get_font_size(Resource *resource)
{
    Q_EMIT q->requestFontSize();
}

// ============================================================================
// PersonalizationFontContextV1
// ============================================================================

PersonalizationFontContextV1::PersonalizationFontContextV1(
    PersonalizationManagerV1Private *manager,
    wl_client *client,
    uint32_t id,
    int version)
    : QObject(nullptr)
    , d(new PersonalizationFontContextV1Private(this, manager, client, id, version))
{
}

PersonalizationFontContextV1::~PersonalizationFontContextV1() = default;

void PersonalizationFontContextV1::sendFont(const QString &font)
{
    d->send_font(font);
}

void PersonalizationFontContextV1::sendMonospaceFont(const QString &font)
{
    d->send_monospace_font(font);
}

void PersonalizationFontContextV1::sendFontSize(uint32_t size)
{
    d->send_font_size(size);
}

PersonalizationFontContextV1 *PersonalizationFontContextV1::get(wl_resource *resource)
{
    auto *r = QtWaylandServer::treeland_personalization_font_context_v1::Resource::fromResource(resource);
    if (r && r->object()) {
        return static_cast<PersonalizationFontContextV1Private *>(r->object())->q;
    }
    return nullptr;
}

// ============================================================================
// PersonalizationV1
// ============================================================================

void PersonalizationV1::updateCacheWallpaperPath(uid_t uid)
{
    QString cache_location = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    m_cacheDirectory = cache_location + QString("/wallpaper/%1/").arg(uid);
    m_settingFile = m_cacheDirectory + "wallpaper.ini";

    QSettings settings(m_settingFile, QSettings::IniFormat);
    m_iniMetaData = settings.value("metadata").toString();
}

QString PersonalizationV1::readWallpaperSettings(const QString &group,
                                                  const QString &output,
                                                  int workspaceId)
{
    if (m_settingFile.isEmpty() || output.isEmpty() || workspaceId < 1)
        return defaultBackground();

    QSettings settings(m_settingFile, QSettings::IniFormat);
    settings.beginGroup(QString("%1.%2.%3").arg(group).arg(output).arg(workspaceId));
    return settings.value("path", defaultBackground()).toString();
}

PersonalizationV1::PersonalizationV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(new PersonalizationManagerV1Private(this))
{
    if (PERSONALIZATION_MANAGER) {
        qFatal("There are multiple instances of QuickPersonalizationManager");
    }

    Q_INIT_RESOURCE(default_background);

    PERSONALIZATION_MANAGER = this;

    if (qgetenv("TREELAND_RUN_MODE") == "user") {
        setUserId(getgid());
    }
}

PersonalizationV1::~PersonalizationV1()
{
    PERSONALIZATION_MANAGER = nullptr;

    Q_CLEANUP_RESOURCE(default_background);
}

void PersonalizationV1::onWindowContextCreated(PersonalizationWindowContextV1 *context)
{
    connect(context, &PersonalizationWindowContextV1::beforeDestroy, this, [this, context] {
        m_windowContexts.removeAll(context);
    });

    m_windowContexts.append(context);

    Q_EMIT windowContextCreated(context);
}

void PersonalizationV1::onWallpaperContextCreated(PersonalizationWallpaperContextV1 *context)
{
    connect(context,
            &PersonalizationWallpaperContextV1::commit,
            this,
            &PersonalizationV1::onWallpaperCommit);
    connect(context,
            &PersonalizationWallpaperContextV1::getWallpapers,
            this,
            &PersonalizationV1::onGetWallpapers);
}

void PersonalizationV1::onCursorContextCreated(PersonalizationCursorContextV1 *context)
{
    connect(context,
            &PersonalizationCursorContextV1::commit,
            this,
            &PersonalizationV1::onCursorCommit);
    connect(context,
            &PersonalizationCursorContextV1::getTheme,
            context,
            &PersonalizationCursorContextV1::sendTheme);
    connect(context,
            &PersonalizationCursorContextV1::getSize,
            context,
            &PersonalizationCursorContextV1::sendSize);

    context->blockSignals(true);
    context->setTheme(Helper::instance()->config()->cursorThemeName());
    auto size = Helper::instance()->config()->cursorSize();
    context->setSize(QSize(size, size));
    connect(Helper::instance()->config(),
            &TreelandUserConfig::cursorThemeNameChanged,
            context,
            [context]() {
                context->setTheme(Helper::instance()->config()->cursorThemeName());
            });
    connect(Helper::instance()->config(),
            &TreelandUserConfig::cursorSizeChanged,
            context,
            [context]() {
                auto size = Helper::instance()->config()->cursorSize();
                context->setSize(QSize(size, size));
            });
    context->blockSignals(false);
}

void PersonalizationV1::onAppearanceContextCreated(PersonalizationAppearanceContextV1 *context)
{
    m_appearanceContexts.push_back(context);

    connect(context, &PersonalizationAppearanceContextV1::roundCornerRadiusChanged, this, [this](int32_t radius) {
        Helper::instance()->config()->setWindowRadius(radius);
        for (auto *c : m_appearanceContexts) {
            c->sendRoundCornerRadius(radius);
        }
    });
    connect(context, &PersonalizationAppearanceContextV1::iconThemeChanged, this, [this](const QString &theme) {
        Helper::instance()->config()->setIconThemeName(theme);
        for (auto *c : m_appearanceContexts) {
            c->sendIconTheme(theme);
        }
    });
    connect(context, &PersonalizationAppearanceContextV1::activeColorChanged, this, [this](const QString &color) {
        Helper::instance()->config()->setActiveColor(color);
        for (auto *c : m_appearanceContexts) {
            c->sendActiveColor(color);
        }
    });
    connect(context, &PersonalizationAppearanceContextV1::windowOpacityChanged, this, [this](uint32_t opacity) {
        Helper::instance()->config()->setWindowOpacity(opacity);
        for (auto *c : m_appearanceContexts) {
            c->sendWindowOpacity(opacity);
        }
    });
    connect(context, &PersonalizationAppearanceContextV1::windowThemeTypeChanged, this, [this](uint32_t type) {
        const auto dconfigType = protocolWindowThemeTypeToDConfig(type);
        if (dconfigType.has_value()) {
            Helper::instance()->config()->setWindowThemeType(*dconfigType);
        }
        for (auto *c : m_appearanceContexts) {
            c->sendWindowThemeType(type);
        }
    });
    connect(context, &PersonalizationAppearanceContextV1::titlebarHeightChanged, this, [this](uint32_t height) {
        Helper::instance()->config()->setWindowTitlebarHeight(height);
        for (auto *c : m_appearanceContexts) {
            c->sendWindowTitlebarHeight(height);
        }
    });

    connect(context, &PersonalizationAppearanceContextV1::requestRoundCornerRadius, context, [this, context] {
        context->setRoundCornerRadius(windowRadius());
    });

    connect(context, &PersonalizationAppearanceContextV1::requestIconTheme, context, [this, context] {
        context->setIconTheme(iconTheme());
    });

    connect(context, &PersonalizationAppearanceContextV1::requestActiveColor, context, [context] {
        context->setActiveColor(Helper::instance()->config()->activeColor());
    });

    connect(context, &PersonalizationAppearanceContextV1::requestWindowOpacity, context, [context] {
        context->setWindowOpacity(Helper::instance()->config()->windowOpacity());
    });

    connect(context, &PersonalizationAppearanceContextV1::requestWindowThemeType, context, [context] {
        const auto protocolType = dconfigWindowThemeTypeToProtocol(
            Helper::instance()->config()->windowThemeType());
        context->setWindowThemeType(protocolType);
    });

    connect(context, &PersonalizationAppearanceContextV1::requestWindowTitlebarHeight, context, [context] {
        context->setWindowTitlebarHeight(Helper::instance()->config()->windowTitlebarHeight());
    });

    connect(context, &PersonalizationAppearanceContextV1::beforeDestroy, this, [this, context] {
        for (auto it = m_appearanceContexts.begin(); it != m_appearanceContexts.end(); ++it) {
            if (*it == context) {
                m_appearanceContexts.erase(it);
                break;
            }
        }
    });

    context->blockSignals(true);

    context->setRoundCornerRadius(Helper::instance()->config()->windowRadius());
    context->setIconTheme(Helper::instance()->config()->iconThemeName());
    context->setActiveColor(Helper::instance()->config()->activeColor());
    context->setWindowOpacity(Helper::instance()->config()->windowOpacity());
    context->setWindowThemeType(dconfigWindowThemeTypeToProtocol(
        Helper::instance()->config()->windowThemeType()));
    context->setWindowTitlebarHeight(Helper::instance()->config()->windowTitlebarHeight());

    context->blockSignals(false);
}

void PersonalizationV1::onFontContextCreated(PersonalizationFontContextV1 *context)
{
    connect(Helper::instance()->config(), &TreelandUserConfig::fontChanged, context, [context] {
        context->sendFont(Helper::instance()->config()->font());
    });
    connect(Helper::instance()->config(), &TreelandUserConfig::monoFontChanged, context, [context] {
        context->sendMonospaceFont(Helper::instance()->config()->monoFont());
    });
    connect(Helper::instance()->config(), &TreelandUserConfig::fontSizeChanged, context, [context] {
        context->sendFontSize(Helper::instance()->config()->fontSize());
    });

    connect(context, &PersonalizationFontContextV1::requestFont, context, [context] {
        context->sendFont(Helper::instance()->config()->font());
    });
    connect(context, &PersonalizationFontContextV1::requestMonoFont, context, [context] {
        context->sendMonospaceFont(Helper::instance()->config()->monoFont());
    });
    connect(context, &PersonalizationFontContextV1::requestFontSize, context, [context] {
        context->sendFontSize(Helper::instance()->config()->fontSize());
    });

    connect(context, &PersonalizationFontContextV1::fontChanged, Helper::instance()->config(), &TreelandUserConfig::setFont);
    connect(context, &PersonalizationFontContextV1::monoFontChanged, Helper::instance()->config(), &TreelandUserConfig::setMonoFont);
    connect(context, &PersonalizationFontContextV1::fontSizeChanged, Helper::instance()->config(), &TreelandUserConfig::setFontSize);

    connect(context, &PersonalizationFontContextV1::beforeDestroy, this, [this, context] {
        for (auto it = m_fontContexts.begin(); it != m_fontContexts.end(); ++it) {
            if (*it == context) {
                m_fontContexts.erase(it);
                break;
            }
        }
    });

    context->blockSignals(true);

    context->sendFont(Helper::instance()->config()->font());
    context->sendMonospaceFont(Helper::instance()->config()->monoFont());
    context->sendFontSize(Helper::instance()->config()->fontSize());

    context->blockSignals(false);

    m_fontContexts.push_back(context);
}

void PersonalizationV1::saveImage(PersonalizationWallpaperContextV1 *context,
                                   const QString &prefix)
{
    if (!context || context->fd() == -1 || m_settingFile.isEmpty()) {
        return;
    }

    QDir dir(m_cacheDirectory);
    if (!dir.exists()) {
        dir.mkpath(m_cacheDirectory);
    }

    QString output = context->outputName();
    if (output.isEmpty()) {
        for (QScreen *screen : QGuiApplication::screens()) {
            output = screen->name();
            break;
        }
    }

    QString dest = m_cacheDirectory + prefix + "_" + output + "_"
        + QDateTime::currentDateTime().toString("yyyyMMddhhmmss");

    QFile src_file;
    if (!src_file.open(context->fd(), QIODevice::ReadOnly))
        return;

    QByteArray data = src_file.readAll();
    src_file.close();

    QFile dest_file(dest);
    if (dest_file.open(QIODevice::WriteOnly)) {
        dest_file.write(data);
        dest_file.close();
    }

    QSettings settings(m_settingFile, QSettings::IniFormat);

    int workspaceId = 1;
    settings.beginGroup(QString("%1.%2.%3").arg(prefix).arg(output).arg(workspaceId));

    const QString &old_path = settings.value("path").toString();
    QFile::remove(old_path);

    settings.setValue("path", dest);
    settings.setValue("isdark", context->isDark());
    settings.endGroup();

    settings.setValue("metadata", context->metaData());
    m_iniMetaData = context->metaData();
}

void PersonalizationV1::onWallpaperCommit(PersonalizationWallpaperContextV1 *context)
{
    if (context->options() & TREELAND_PERSONALIZATION_WALLPAPER_CONTEXT_V1_OPTIONS_BACKGROUND) {
        saveImage(context, "background");
        Q_EMIT backgroundChanged(context->outputName(), context->isDark());
    }

    if (context->options() & TREELAND_PERSONALIZATION_WALLPAPER_CONTEXT_V1_OPTIONS_LOCKSCREEN) {
        saveImage(context, "lockscreen");
        Q_EMIT lockscreenChanged();
    }
}

void PersonalizationV1::onCursorCommit(PersonalizationCursorContextV1 *context)
{
    if (!context->size().isValid() || context->theme().isEmpty()) {
        context->verify(false);
    }

    setCursorTheme(context->theme());
    setCursorSize(context->size());

    context->verify(true);
}

void PersonalizationV1::onGetWallpapers(PersonalizationWallpaperContextV1 *context)
{
    QDir dir(m_cacheDirectory);
    if (!dir.exists())
        return;

    context->setMetaData(m_iniMetaData);
}

uid_t PersonalizationV1::userId()
{
    return m_userId;
}

void PersonalizationV1::setUserId(uid_t uid)
{
    m_userId = uid;
    updateCacheWallpaperPath(uid);
    Q_EMIT userIdChanged(uid);
}

QString PersonalizationV1::cursorTheme()
{
    return Helper::instance()->config()->cursorThemeName();
}

void PersonalizationV1::setCursorTheme(const QString &name)
{
    Helper::instance()->config()->setCursorThemeName(name);

    Q_EMIT cursorThemeChanged(name);
}

QSize PersonalizationV1::cursorSize()
{
    int size = Helper::instance()->config()->cursorSize();

    return QSize(size, size);
}

void PersonalizationV1::setCursorSize(const QSize &size)
{
    Helper::instance()->config()->setCursorSize(size.width());

    Q_EMIT cursorSizeChanged(size);
}

int32_t PersonalizationV1::windowRadius() const
{
    return Helper::instance()->config()->windowRadius();
}

QString PersonalizationV1::iconTheme() const
{
    return Helper::instance()->config()->iconThemeName();
}

QString PersonalizationV1::background(const QString &output, int workspaceId)
{
    return readWallpaperSettings("background", output, workspaceId);
}

QString PersonalizationV1::lockscreen(const QString &output, int workspaceId)
{
    return readWallpaperSettings("lockscreen", output, workspaceId);
}

bool PersonalizationV1::backgroundIsDark(const QString &output, int workspaceId)
{
    if (m_settingFile.isEmpty())
        return DEFAULT_WALLPAPER_ISDARK;

    QSettings settings(m_settingFile, QSettings::IniFormat);
    settings.beginGroup(QString("background.%2.%3").arg(output).arg(workspaceId));
    return settings.value("isdark", DEFAULT_WALLPAPER_ISDARK).toBool();
}

bool PersonalizationV1::isAnimagedImage(const QString &source)
{
    QImageReader reader(source);
    return reader.imageCount() > 1;
}

// ============================================================================
// Personalization
// ============================================================================

Personalization::Personalization(WToplevelSurface *target,
                                 PersonalizationV1 *manager,
                                 SurfaceWrapper *parent)
    : QObject(parent)
    , m_target(target)
    , m_manager(manager)
{
    connect(target, &WToplevelSurface::aboutToBeInvalidated, this, [this] {
        disconnect(m_connection);
    });

    auto update = [this](PersonalizationWindowContextV1 *context) {
        assert(context);

        if (WSurface::fromHandle(context->surface()) != m_target->surface()) {
            return;
        }

        disconnect(m_connection);

        connect(context,
                &PersonalizationWindowContextV1::backgroundTypeChanged,
                this,
                [this, context] {
                    m_backgroundType = context->backgroundType();
                    Q_EMIT backgroundTypeChanged();
                });
        connect(context,
                &PersonalizationWindowContextV1::cornerRadiusChanged,
                this,
                [this, context] {
                    m_cornerRadius = context->cornerRadius();
                    Q_EMIT cornerRadiusChanged();
                });

        connect(context, &PersonalizationWindowContextV1::shadowChanged, this, [this, context] {
            m_shadow = context->shadow();
            Q_EMIT shadowChanged();
        });

        connect(context, &PersonalizationWindowContextV1::borderChanged, this, [this, context] {
            m_border = context->border();
            Q_EMIT borderChanged();
        });

        connect(context,
                &PersonalizationWindowContextV1::windowStateChanged,
                this,
                [this, context] {
                    m_states = context->states();
                    Q_EMIT windowStateChanged();
                });

        m_backgroundType = context->backgroundType();
        m_cornerRadius = context->cornerRadius();
        m_shadow = context->shadow();
        m_border = context->border();
        m_states = context->states();
    };

    m_connection = connect(m_manager, &PersonalizationV1::windowContextCreated, this, update);

    if (auto *context = m_manager->getWindowContext(m_target->surface())) {
        update(context);
    }
}

SurfaceWrapper *Personalization::surfaceWrapper() const
{
    return qobject_cast<SurfaceWrapper*>(parent());
}

Personalization::BackgroundType Personalization::backgroundType() const
{
    return static_cast<Personalization::BackgroundType>(m_backgroundType);
}

bool Personalization::noTitlebar() const
{
    if (qobject_cast<WXdgPopupSurface *>(m_target)) {
        return true;
    }

    return m_states.testFlag(PersonalizationWindowContextV1::NoTitleBar);
}

void PersonalizationV1::create(WServer *server)
{
    d->init(server->handle()->handle(), 1);
}

void PersonalizationV1::destroy([[maybe_unused]] WServer *server)
{
    d = nullptr;
}

wl_global *PersonalizationV1::global() const
{
    return d->global();
}

QByteArrayView PersonalizationV1::interfaceName() const
{
    return d->interfaceName();
}

PersonalizationWindowContextV1 *PersonalizationV1::getWindowContext(WSurface *surface)
{
    for (auto *context : m_windowContexts) {
        if (context->surface() == surface->handle()->handle()) {
            return context;
        }
    }

    return nullptr;
}

QString PersonalizationV1::defaultWallpaper() const
{
    return DEFAULT_WALLPAPER;
}
