/*
 * WPA Supplicant - background scan and roaming interface
 * Copyright (c) 2009-2010, Jouni Malinen <j@w1.fi>
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

#ifndef BGSCAN_H
#define BGSCAN_H

struct wpa_supplicant;
struct wpa_ssid;

struct bgscan_ops {
	const char *name;

	void * (*init)(struct wpa_supplicant *wpa_s, const char *params,
		       const struct wpa_ssid *ssid);
	void (*deinit)(void *priv);

	int (*notify_scan)(void *priv);
	void (*notify_beacon_loss)(void *priv);
	void (*notify_signal_change)(void *priv, int above);
};

#ifdef CONFIG_BGSCAN

int bgscan_init(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid);
void bgscan_deinit(struct wpa_supplicant *wpa_s);
int bgscan_notify_scan(struct wpa_supplicant *wpa_s);
void bgscan_notify_beacon_loss(struct wpa_supplicant *wpa_s);
void bgscan_notify_signal_change(struct wpa_supplicant *wpa_s, int above);

#else /* CONFIG_BGSCAN */

static inline int bgscan_init(struct wpa_supplicant *wpa_s,
			      struct wpa_ssid *ssid)
{
	return 0;
}

static inline void bgscan_deinit(struct wpa_supplicant *wpa_s)
{
}

static inline int bgscan_notify_scan(struct wpa_supplicant *wpa_s)
{
	return 0;
}

static inline void bgscan_notify_beacon_loss(struct wpa_supplicant *wpa_s)
{
}

static inline void bgscan_notify_signal_change(struct wpa_supplicant *wpa_s,
					       int above)
{
}

#endif /* CONFIG_BGSCAN */

#endif /* BGSCAN_H */
