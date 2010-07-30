/*
 * hostapd / WPS integration
 * Copyright (c) 2008, Jouni Malinen <j@w1.fi>
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

#ifndef WPS_HOSTAPD_H
#define WPS_HOSTAPD_H

#ifdef CONFIG_WPS

int hostapd_init_wps(struct hostapd_data *hapd,
		     struct hostapd_bss_config *conf);
void hostapd_deinit_wps(struct hostapd_data *hapd);
int hostapd_wps_add_pin(struct hostapd_data *hapd, const char *uuid,
			const char *pin, int timeout);
int hostapd_wps_button_pushed(struct hostapd_data *hapd);
void hostapd_wps_probe_req_rx(struct hostapd_data *hapd, const u8 *addr,
			      const u8 *ie, size_t ie_len);

#else /* CONFIG_WPS */

static inline int hostapd_init_wps(struct hostapd_data *hapd,
				   struct hostapd_bss_config *conf)
{
	return 0;
}

static inline void hostapd_deinit_wps(struct hostapd_data *hapd)
{
}

static inline void hostapd_wps_probe_req_rx(struct hostapd_data *hapd,
					    const u8 *addr,
					    const u8 *ie, size_t ie_len)
{
}
#endif /* CONFIG_WPS */

#endif /* WPS_HOSTAPD_H */
