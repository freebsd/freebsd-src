/*
 * hostapd / Driver interface for RADIUS server only (no driver)
 * Copyright (c) 2008, Atheros Communications
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

#include "includes.h"

#include "hostapd.h"
#include "driver.h"


struct none_driver_data {
	struct hostapd_data *hapd;
};


static void * none_driver_init(struct hostapd_data *hapd)
{
	struct none_driver_data *drv;

	drv = os_zalloc(sizeof(struct none_driver_data));
	if (drv == NULL) {
		wpa_printf(MSG_ERROR, "Could not allocate memory for none "
			   "driver data");
		return NULL;
	}
	drv->hapd = hapd;

	return drv;
}


static void none_driver_deinit(void *priv)
{
	struct none_driver_data *drv = priv;

	os_free(drv);
}


static int none_driver_send_ether(void *priv, const u8 *dst, const u8 *src,
				  u16 proto, const u8 *data, size_t data_len)
{
	return 0;
}


const struct wpa_driver_ops wpa_driver_none_ops = {
	.name = "none",
	.init = none_driver_init,
	.deinit = none_driver_deinit,
	.send_ether = none_driver_send_ether,
};
