/*
 * wpa_supplicant D-Bus control interface - common definitions
 * Copyright (c) 2006, Dan Williams <dcbw@redhat.com> and Red Hat, Inc.
 * Copyright (c) 2009, Witold Sowa <witold.sowa@gmail.com>
 * Copyright (c) 2009, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#ifndef DBUS_COMMON_H
#define DBUS_COMMON_H

struct wpas_dbus_priv;
struct wpa_global;

struct wpas_dbus_priv * wpas_dbus_init(struct wpa_global *global);
void wpas_dbus_deinit(struct wpas_dbus_priv *priv);

#endif /* DBUS_COMMON_H */
