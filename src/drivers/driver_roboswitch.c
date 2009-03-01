/*
 * WPA Supplicant - roboswitch driver interface
 * Copyright (c) 2008-2009 Jouke Witteveen
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
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/sockios.h>
#include <linux/mii.h>

#include "common.h"
#include "driver.h"

#define ROBO_PHY_ADDR		0x1E	/* RoboSwitch PHY address */

/* MII access registers */
#define ROBO_MII_PAGE		0x10	/* MII page register */
#define ROBO_MII_ADDR		0x11	/* MII address register */
#define ROBO_MII_DATA_OFFSET	0x18	/* Start of MII data registers */

#define ROBO_MII_PAGE_ENABLE	0x01	/* MII page op code */
#define ROBO_MII_ADDR_WRITE	0x01	/* MII address write op code */
#define ROBO_MII_ADDR_READ	0x02	/* MII address read op code */
#define ROBO_MII_DATA_MAX	   4	/* Consecutive MII data registers */
#define ROBO_MII_RETRY_MAX	  10	/* Read attempts before giving up */

/* Page numbers */
#define ROBO_ARLCTRL_PAGE	0x04	/* ARL control page */
#define ROBO_VLAN_PAGE		0x34	/* VLAN page */

/* ARL control page registers */
#define ROBO_ARLCTRL_CONF	0x00	/* ARL configuration register */
#define ROBO_ARLCTRL_ADDR_1	0x10	/* Multiport address 1 */
#define ROBO_ARLCTRL_VEC_1	0x16	/* Multiport vector 1 */
#define ROBO_ARLCTRL_ADDR_2	0x20	/* Multiport address 2 */
#define ROBO_ARLCTRL_VEC_2	0x26	/* Multiport vector 2 */

/* VLAN page registers */
#define ROBO_VLAN_ACCESS	0x06	/* VLAN table Access register */
#define ROBO_VLAN_ACCESS_5365	0x08	/* VLAN table Access register (5365) */
#define ROBO_VLAN_READ		0x0C	/* VLAN read register */
#define ROBO_VLAN_MAX		0xFF	/* Maximum number of VLANs */


static const u8 pae_group_addr[ETH_ALEN] =
{ 0x01, 0x80, 0xc2, 0x00, 0x00, 0x03 };


struct wpa_driver_roboswitch_data {
	void *ctx;
	char ifname[IFNAMSIZ + 1];
	struct ifreq ifr;
	int fd;
	u16 ports;
};


/* Copied from the kernel-only part of mii.h. */
static inline struct mii_ioctl_data *if_mii(struct ifreq *rq)
{
	return (struct mii_ioctl_data *) &rq->ifr_ifru;
}


static u16 wpa_driver_roboswitch_mdio_read(
	struct wpa_driver_roboswitch_data *drv, u8 reg)
{
	struct mii_ioctl_data *mii = if_mii(&drv->ifr);

	mii->phy_id = ROBO_PHY_ADDR;
	mii->reg_num = reg;

	if (ioctl(drv->fd, SIOCGMIIREG, &drv->ifr) < 0) {
		perror("ioctl[SIOCGMIIREG]");
		return 0x00;
	}
	return mii->val_out;
}


static void wpa_driver_roboswitch_mdio_write(
	struct wpa_driver_roboswitch_data *drv, u8 reg, u16 val)
{
	struct mii_ioctl_data *mii = if_mii(&drv->ifr);

	mii->phy_id = ROBO_PHY_ADDR;
	mii->reg_num = reg;
	mii->val_in = val;

	if (ioctl(drv->fd, SIOCSMIIREG, &drv->ifr) < 0) {
		perror("ioctl[SIOCSMIIREG");
	}
}


static int wpa_driver_roboswitch_reg(struct wpa_driver_roboswitch_data *drv,
				     u8 page, u8 reg, u8 op)
{
	int i;

	/* set page number */
	wpa_driver_roboswitch_mdio_write(drv, ROBO_MII_PAGE,
					 (page << 8) | ROBO_MII_PAGE_ENABLE);
	/* set register address */
	wpa_driver_roboswitch_mdio_write(drv, ROBO_MII_ADDR, (reg << 8) | op);

	/* check if operation completed */
	for (i = 0; i < ROBO_MII_RETRY_MAX; ++i) {
		if ((wpa_driver_roboswitch_mdio_read(drv, ROBO_MII_ADDR) & 3)
		    == 0)
			return 0;
	}
	/* timeout */
	return -1;
}


static int wpa_driver_roboswitch_read(struct wpa_driver_roboswitch_data *drv,
				      u8 page, u8 reg, u16 *val, int len)
{
	int i;

	if (len > ROBO_MII_DATA_MAX ||
	    wpa_driver_roboswitch_reg(drv, page, reg, ROBO_MII_ADDR_READ) < 0)
		return -1;

	for (i = 0; i < len; ++i) {
		val[i] = wpa_driver_roboswitch_mdio_read(
			drv, ROBO_MII_DATA_OFFSET + i);
	}

	return 0;
}


static int wpa_driver_roboswitch_write(struct wpa_driver_roboswitch_data *drv,
				       u8 page, u8 reg, u16 *val, int len)
{
	int i;

	if (len > ROBO_MII_DATA_MAX) return -1;
	for (i = 0; i < len; ++i) {
		wpa_driver_roboswitch_mdio_write(drv, ROBO_MII_DATA_OFFSET + i,
						 val[i]);
	}
	return wpa_driver_roboswitch_reg(drv, page, reg, ROBO_MII_ADDR_WRITE);
}


static int wpa_driver_roboswitch_get_ssid(void *priv, u8 *ssid)
{
	ssid[0] = 0;
	return 0;
}


static int wpa_driver_roboswitch_get_bssid(void *priv, u8 *bssid)
{
	/* Report PAE group address as the "BSSID" for wired connection. */
	os_memcpy(bssid, pae_group_addr, ETH_ALEN);
	return 0;
}


static const char * wpa_driver_roboswitch_get_ifname(void *priv)
{
	struct wpa_driver_roboswitch_data *drv = priv;
	return drv->ifname;
}


static int wpa_driver_roboswitch_join(struct wpa_driver_roboswitch_data *drv,
				      const u8 *addr)
{
	int i;
	u16 _read, zero = 0;
	/* For reasons of simplicity we assume ETH_ALEN is even. */
	u16 addr_word[ETH_ALEN / 2];
	/* RoboSwitch uses 16-bit Big Endian addresses.			*/
	/* The ordering of the words is reversed in the MII registers.	*/
	for (i = 0; i < ETH_ALEN; i += 2)
		addr_word[(ETH_ALEN - i) / 2 - 1] = WPA_GET_BE16(addr + i);

	/* check if multiport addresses are not yet enabled */
	if (wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE,
				       ROBO_ARLCTRL_CONF, &_read, 1) < 0)
		return -1;

	if (!(_read & (1 << 4))) {
		_read |= 1 << 4;
		wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
					    ROBO_ARLCTRL_ADDR_1, addr_word, 3);
		wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
					    ROBO_ARLCTRL_VEC_1, &drv->ports,
					    1);
		wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
					    ROBO_ARLCTRL_VEC_2, &zero, 1);
		wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
					    ROBO_ARLCTRL_CONF, &_read, 1);
		return 0;
	}

	/* check if multiport address 1 is free */
	wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE, ROBO_ARLCTRL_VEC_1,
				   &_read, 1);
	if (_read == 0) {
		wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
					    ROBO_ARLCTRL_ADDR_1, addr_word, 3);
		wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
					    ROBO_ARLCTRL_VEC_1, &drv->ports,
					    1);
		return 0;
	}
	/* check if multiport address 2 is free */
	wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE, ROBO_ARLCTRL_VEC_2,
				   &_read, 1);
	if (_read == 0) {
		wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
					    ROBO_ARLCTRL_ADDR_2, addr_word, 3);
		wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
					    ROBO_ARLCTRL_VEC_2, &drv->ports,
					    1);
		return 0;
	}

	/* out of free multiport addresses */
	return -1;
}


static int wpa_driver_roboswitch_leave(struct wpa_driver_roboswitch_data *drv,
				       const u8 *addr)
{
	int i;
	u16 _read[3], zero = 0;
	u16 addr_word[ETH_ALEN / 2]; /* same as at join */

	for (i = 0; i < ETH_ALEN; i += 2)
		addr_word[(ETH_ALEN - i) / 2 - 1] = WPA_GET_BE16(addr + i);

	/* check if multiport address 1 was used */
	wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE, ROBO_ARLCTRL_VEC_1,
				   _read, 1);
	if (_read[0] == drv->ports) {
		wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE,
					   ROBO_ARLCTRL_ADDR_1, _read, 3);
		if (os_memcmp(_read, addr_word, 6) == 0) {
			wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
						    ROBO_ARLCTRL_VEC_1, &zero,
						    1);
			goto clean_up;
		}
	}

	/* check if multiport address 2 was used */
	wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE, ROBO_ARLCTRL_VEC_2,
				   _read, 1);
	if (_read[0] == drv->ports) {
		wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE,
					   ROBO_ARLCTRL_ADDR_2, _read, 3);
		if (os_memcmp(_read, addr_word, 6) == 0) {
			wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
						    ROBO_ARLCTRL_VEC_2, &zero,
						    1);
			goto clean_up;
		}
	}

	/* used multiport address not found */
	return -1;

clean_up:
	/* leave the multiport registers in a sane state */
	wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE, ROBO_ARLCTRL_VEC_1,
				   _read, 1);
	if (_read[0] == 0) {
		wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE,
					   ROBO_ARLCTRL_VEC_2, _read, 1);
		if (_read[0] == 0) {
			wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE,
						   ROBO_ARLCTRL_CONF, _read,
						   1);
			_read[0] &= ~(1 << 4);
			wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
						    ROBO_ARLCTRL_CONF, _read,
						    1);
		} else {
			wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE,
						   ROBO_ARLCTRL_ADDR_2, _read,
						   3);
			wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
						    ROBO_ARLCTRL_ADDR_1, _read,
						    3);
			wpa_driver_roboswitch_read(drv, ROBO_ARLCTRL_PAGE,
						   ROBO_ARLCTRL_VEC_2, _read,
						   1);
			wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
						    ROBO_ARLCTRL_VEC_1, _read,
						    1);
			wpa_driver_roboswitch_write(drv, ROBO_ARLCTRL_PAGE,
						    ROBO_ARLCTRL_VEC_2, &zero,
						    1);
		}
	}
	return 0;
}


static void * wpa_driver_roboswitch_init(void *ctx, const char *ifname)
{
	struct wpa_driver_roboswitch_data *drv;
	int len = -1, sep = -1;
	u16 vlan_max = ROBO_VLAN_MAX, vlan = 0, vlan_read[2];

	drv = os_zalloc(sizeof(*drv));
	if (drv == NULL) return NULL;
	drv->ctx = ctx;

	while (ifname[++len]) {
		if (ifname[len] == '.')
			sep = len;
	}
	if (sep < 0 || sep >= len - 1) {
		wpa_printf(MSG_INFO, "%s: No <interface>.<vlan> pair in "
			   "interface name %s", __func__, ifname);
		os_free(drv);
		return NULL;
	}
	if (sep > IFNAMSIZ) {
		wpa_printf(MSG_INFO, "%s: Interface name %s is too long",
			   __func__, ifname);
		os_free(drv);
		return NULL;
	}
	os_memcpy(drv->ifname, ifname, sep);
	drv->ifname[sep] = '\0';
	while (++sep < len) {
		if (ifname[sep] < '0' || ifname[sep] > '9') {
			wpa_printf(MSG_INFO, "%s: Invalid vlan specification "
				   "in interface name %s", __func__, ifname);
			os_free(drv);
			return NULL;
		}
		vlan *= 10;
		vlan += ifname[sep] - '0';
		if (vlan > ROBO_VLAN_MAX) {
			wpa_printf(MSG_INFO, "%s: VLAN out of range in "
				   "interface name %s", __func__, ifname);
			os_free(drv);
			return NULL;
		}
	}

	drv->fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (drv->fd < 0) {
		wpa_printf(MSG_INFO, "%s: Unable to create socket", __func__);
		os_free(drv);
		return NULL;
	}

	os_memset(&drv->ifr, 0, sizeof(drv->ifr));
	os_strlcpy(drv->ifr.ifr_name, drv->ifname, IFNAMSIZ);
	if (ioctl(drv->fd, SIOCGMIIPHY, &drv->ifr) < 0) {
		perror("ioctl[SIOCGMIIPHY]");
		os_free(drv);
		return NULL;
	}
	if (if_mii(&drv->ifr)->phy_id != ROBO_PHY_ADDR) {
		wpa_printf(MSG_INFO, "%s: Invalid phy address (not a "
			   "RoboSwitch?)", __func__);
		os_free(drv);
		return NULL;
	}

	/* set the read bit */
	vlan |= 1 << 13;
	/* set and read back to see if the register can be used */
	wpa_driver_roboswitch_write(drv, ROBO_VLAN_PAGE, ROBO_VLAN_ACCESS,
				    &vlan_max, 1);
	wpa_driver_roboswitch_read(drv, ROBO_VLAN_PAGE, ROBO_VLAN_ACCESS,
				   &vlan_max, 1);
	if (vlan_max == ROBO_VLAN_MAX) /* pre-5365 */
		wpa_driver_roboswitch_write(drv, ROBO_VLAN_PAGE,
					    ROBO_VLAN_ACCESS, &vlan, 1);
	else /* 5365 uses a different register */
		wpa_driver_roboswitch_write(drv, ROBO_VLAN_PAGE,
					    ROBO_VLAN_ACCESS_5365, &vlan, 1);
	wpa_driver_roboswitch_read(drv, ROBO_VLAN_PAGE, ROBO_VLAN_READ,
				   vlan_read, 2);
	if (!(vlan_read[1] & (1 << 4))) {
		wpa_printf(MSG_INFO, "%s: Could not get port information for "
				     "VLAN %d", __func__, vlan & ~(1 << 13));
		os_free(drv);
		return NULL;
	}
	drv->ports = vlan_read[0] & 0x001F;
	/* add the MII port */
	drv->ports |= 1 << 8;
	if (wpa_driver_roboswitch_join(drv, pae_group_addr) < 0) {
		wpa_printf(MSG_INFO, "%s: Unable to join PAE group", __func__);
		os_free(drv);
		return NULL;
	} else {
		wpa_printf(MSG_DEBUG, "%s: Added PAE group address to "
			   "RoboSwitch ARL", __func__);
	}

	return drv;
}


static void wpa_driver_roboswitch_deinit(void *priv)
{
	struct wpa_driver_roboswitch_data *drv = priv;

	if (wpa_driver_roboswitch_leave(drv, pae_group_addr) < 0) {
		wpa_printf(MSG_DEBUG, "%s: Unable to leave PAE group",
			   __func__);
	}

	close(drv->fd);
	os_free(drv);
}


const struct wpa_driver_ops wpa_driver_roboswitch_ops = {
	.name = "roboswitch",
	.desc = "wpa_supplicant roboswitch driver",
	.get_ssid = wpa_driver_roboswitch_get_ssid,
	.get_bssid = wpa_driver_roboswitch_get_bssid,
	.init = wpa_driver_roboswitch_init,
	.deinit = wpa_driver_roboswitch_deinit,
	.get_ifname = wpa_driver_roboswitch_get_ifname,
};
