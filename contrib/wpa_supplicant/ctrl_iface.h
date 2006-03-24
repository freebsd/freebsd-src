/*
 * WPA Supplicant / UNIX domain socket -based control interface
 * Copyright (c) 2004-2005, Jouni Malinen <jkmaline@cc.hut.fi>
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

#ifndef CTRL_IFACE_H
#define CTRL_IFACE_H

#ifdef CONFIG_CTRL_IFACE

int wpa_supplicant_ctrl_iface_init(struct wpa_supplicant *wpa_s);
void wpa_supplicant_ctrl_iface_deinit(struct wpa_supplicant *wpa_s);
void wpa_supplicant_ctrl_iface_send(struct wpa_supplicant *wpa_s, int level,
				    char *buf, size_t len);
void wpa_supplicant_ctrl_iface_wait(struct wpa_supplicant *wpa_s);
int wpa_supplicant_global_ctrl_iface_init(struct wpa_global *global);
void wpa_supplicant_global_ctrl_iface_deinit(struct wpa_global *global);

#else /* CONFIG_CTRL_IFACE */

static inline int wpa_supplicant_ctrl_iface_init(struct wpa_supplicant *wpa_s)
{
	return 0;
}

static inline void
wpa_supplicant_ctrl_iface_deinit(struct wpa_supplicant *wpa_s)
{
}

static inline void
wpa_supplicant_ctrl_iface_send(struct wpa_supplicant *wpa_s, int level,
			       char *buf, size_t len)
{
}

static inline void
wpa_supplicant_ctrl_iface_wait(struct wpa_supplicant *wpa_s)
{
}

static inline int
wpa_supplicant_global_ctrl_iface_init(struct wpa_global *global)
{
	return 0;
}

static inline void
wpa_supplicant_global_ctrl_iface_deinit(struct wpa_global *global)
{
}

#endif /* CONFIG_CTRL_IFACE */

#endif /* CTRL_IFACE_H */
