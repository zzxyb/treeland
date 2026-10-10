// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "state.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "ext-transient-seat-v1-client-protocol.h"
#include "treeland-keyboard-state-notify-unstable-v1-client-protocol.h"

struct result { uint32_t global; int denied; };
static void ready(void *data, struct ext_transient_seat_v1 *seat, uint32_t global)
{
    (void)seat;
    ((struct result *)data)->global = global;
}
static void denied(void *data, struct ext_transient_seat_v1 *seat)
{
    (void)seat;
    ((struct result *)data)->denied = 1;
}
static const struct ext_transient_seat_v1_listener listener = { ready, denied };

static int check_state(int count)
{
    struct transient_state state = {0};
    return invoke_on_server_thread(transient_read_state, &state)
        && state.count == count && state.configured == count && state.persisted == 0;
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;
    struct ext_transient_seat_manager_v1 *manager = client_bind(&conn,
        "ext_transient_seat_manager_v1", &ext_transient_seat_manager_v1_interface, 1);
    if (!manager)
        return 1;
    /* Enable the production keyboard observer before transient seats appear. */
    struct treeland_keyboard_state_notify_manager_v1 *observer = client_bind(&conn,
        "treeland_keyboard_state_notify_manager_v1",
        &treeland_keyboard_state_notify_manager_v1_interface, 1);
    if (!observer || wl_display_roundtrip(conn.display) < 0)
        return 1;
    struct result results[2] = {{0}};
    struct ext_transient_seat_v1 *seats[2];
    for (int i = 0; i < 2; ++i) {
        seats[i] = ext_transient_seat_manager_v1_create(manager);
        ext_transient_seat_v1_add_listener(seats[i], &listener, &results[i]);
    }
    if (wl_display_roundtrip(conn.display) < 0 || !results[0].global
        || !results[1].global || results[0].denied || results[1].denied
        || results[0].global == results[1].global || !check_state(2))
        return 1;
    for (int i = 0; i < 2; ++i) {
        struct wl_seat *seat = client_bind_global(&conn, results[i].global, &wl_seat_interface, 7);
        if (!seat)
            return 1;
        wl_seat_release(seat);
    }
    /* Client resource destruction releases only its own seat. */
    ext_transient_seat_v1_destroy(seats[0]);
    if (wl_display_roundtrip(conn.display) < 0 || !check_state(1))
        return 1;
    /* Compositor removal makes the surviving resource inert. */
    if (!invoke_on_server_thread(transient_remove_seats, NULL) || !check_state(0))
        return 1;
    ext_transient_seat_v1_destroy(seats[1]);
    if (wl_display_roundtrip(conn.display) < 0)
        return 1;
    /* Disconnect with a live transient seat must release it as well. */
    struct result disconnected = {0};
    struct ext_transient_seat_v1 *last = ext_transient_seat_manager_v1_create(manager);
    ext_transient_seat_v1_add_listener(last, &listener, &disconnected);
    if (wl_display_roundtrip(conn.display) < 0 || !disconnected.global || !check_state(1))
        return 1;
    wl_proxy_destroy((struct wl_proxy *)last);
    treeland_keyboard_state_notify_manager_v1_destroy(observer);
    ext_transient_seat_manager_v1_destroy(manager);
    client_disconnect(&conn);
    /* A second connection roundtrip synchronises the server event loop. */
    if (!client_connect(&conn, socket_name))
        return 1;
    int ok = check_state(0);
    client_disconnect(&conn);
    return ok ? 0 : 1;
}
