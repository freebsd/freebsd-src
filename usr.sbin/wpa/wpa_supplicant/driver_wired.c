/*
 * WPA Supplicant - wired Ethernet driver interface
 * Copyright (c) 2005-2007, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 *
 * $FreeBSD$
 */

#include "includes.h"
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_dl.h>

#include "common.h"
#include "driver.h"
#include "wpa_supplicant.h"

static const u8 pae_group_addr[ETH_ALEN] =
{ 0x01, 0x80, 0xc2, 0x00, 0x00, 0x03 };

struct wpa_driver_wired_data {
	int	sock;
	char	ifname[IFNAMSIZ + 1];
	int	multi;
	int	flags;
	void	*ctx;
};

static int
getifflags(struct wpa_driver_wired_data *drv, int *flags)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, drv->ifname, sizeof (ifr.ifr_name));
	if (ioctl(drv->sock, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		perror("SIOCGIFFLAGS");
		return errno;
	}
	*flags = (ifr.ifr_flags & 0xffff) | (ifr.ifr_flagshigh << 16);
	return 0;
}

static int
setifflags(struct wpa_driver_wired_data *drv, int flags)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, drv->ifname, sizeof (ifr.ifr_name));
	ifr.ifr_flags = flags & 0xffff;
	ifr.ifr_flagshigh = flags >> 16;
	if (ioctl(drv->sock, SIOCSIFFLAGS, (caddr_t)&ifr) < 0) {
		perror("SIOCSIFFLAGS");
		return errno;
	}
	return 0;
}

static int
wpa_driver_wired_get_ssid(void *priv, u8 *ssid)
{
	ssid[0] = 0;
	return 0;
}

static int
wpa_driver_wired_get_bssid(void *priv, u8 *bssid)
{
	/* Report PAE group address as the "BSSID" for wired connection. */
	os_memcpy(bssid, pae_group_addr, ETH_ALEN);
	return 0;
}

static int
siocmulti(struct wpa_driver_wired_data *drv, int op, const u8 *addr)
{
	struct ifreq ifr;
	struct sockaddr_dl *dlp;

	os_memset(&ifr, 0, sizeof(ifr));
	os_strncpy(ifr.ifr_name, drv->ifname, IFNAMSIZ);
	dlp = (struct sockaddr_dl *) &ifr.ifr_addr;
	dlp->sdl_len = sizeof(struct sockaddr_dl);
	dlp->sdl_family = AF_LINK;
	dlp->sdl_index = 0;
	dlp->sdl_nlen = 0;
	dlp->sdl_alen = ETH_ALEN;
	dlp->sdl_slen = 0;
	os_memcpy(LLADDR(dlp), addr, ETH_ALEN); 
	if (ioctl(drv->sock, op, (caddr_t) &ifr) < 0) {
		wpa_printf(MSG_INFO, "ioctl[%s]: %s", op == SIOCADDMULTI ?
		    "SIOCADDMULTI" : "SIOCDELMULTI", strerror(errno));
		return -1;
	}
	return 0;
}

static void *
wpa_driver_wired_init(void *ctx, const char *ifname)
{
	struct wpa_driver_wired_data *drv;
	int flags;

	drv = os_zalloc(sizeof(*drv));
	if (drv == NULL)
		return NULL;
	os_strncpy(drv->ifname, ifname, sizeof(drv->ifname));
	drv->sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (drv->sock < 0)
		goto fail1;
	drv->ctx = ctx;

	if (getifflags(drv, &drv->flags) < 0) {
		wpa_printf(MSG_INFO, "%s: Unable to get interface flags",
		    __func__);
		goto fail;
	}
	flags = drv->flags | IFF_UP;		/* NB: force interface up */

	/* 
	 * Arrange to receive PAE mcast frames.  Try to add an
	 * explicit mcast address.  If that fails, fallback to
	 * the all multicast mechanism.
	 */
	if (siocmulti(drv, SIOCADDMULTI, pae_group_addr) == 0) {
		wpa_printf(MSG_DEBUG, "%s: Added PAE multicast address",
		    __func__);
		drv->multi = 1;
	} else if ((drv->flags & IFF_ALLMULTI) == 0)
		flags |= IFF_ALLMULTI;

	if (flags != drv->flags) {
		if (setifflags(drv, flags) < 0) {
			wpa_printf(MSG_INFO, "%s: Failed to set interface flags",
			    __func__);
			goto fail;
		}
		if ((flags ^ drv->flags) & IFF_ALLMULTI)
			wpa_printf(MSG_DEBUG, "%s: Enabled all-multi mode",
			    __func__);
	}
	return drv;
fail:
	close(drv->sock);
fail1:
	free(drv);
	return NULL;
}

static void
wpa_driver_wired_deinit(void *priv)
{
	struct wpa_driver_wired_data *drv = priv;

	if (drv->multi) {
		if (siocmulti(drv, SIOCDELMULTI, pae_group_addr) < 0) {
			wpa_printf(MSG_DEBUG, "%s: Failed to remove PAE "
			    "multicast " "group (SIOCDELMULTI)", __func__);
		}
	}
	if (setifflags(drv, drv->flags) < 0) {
		wpa_printf(MSG_INFO, "%s: Failed to restore interface flags",
			   __func__);
	}
	(void) close(drv->sock);
	os_free(drv);
}

const struct wpa_driver_ops wpa_driver_wired_ops = {
	.name		= "wired",
	.desc		= "BSD wired Ethernet driver",
	.get_ssid	= wpa_driver_wired_get_ssid,
	.get_bssid	= wpa_driver_wired_get_bssid,
	.init		= wpa_driver_wired_init,
	.deinit		= wpa_driver_wired_deinit,
};
