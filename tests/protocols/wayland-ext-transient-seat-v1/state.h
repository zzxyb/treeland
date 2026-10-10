// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
struct transient_state { int count; int configured; int persisted; };
void transient_read_state(void *data);
void transient_remove_seats(void *data);
#ifdef __cplusplus
}
#endif
