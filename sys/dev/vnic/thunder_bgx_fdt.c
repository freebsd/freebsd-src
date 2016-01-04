/*-
 * Copyright (c) 2015 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Semihalf under
 * the sponsorship of the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bitset.h>
#include <sys/bitstring.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/pciio.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/cpuset.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_media.h>

#include <dev/ofw/openfirm.h>
#include <dev/mii/miivar.h>

#include "thunder_bgx.h"
#include "thunder_bgx_var.h"

#define	CONN_TYPE_MAXLEN	16
#define	CONN_TYPE_OFFSET	2

int bgx_fdt_init_phy(struct bgx *);

static void
bgx_fdt_get_macaddr(phandle_t phy, uint8_t *hwaddr)
{
	uint8_t addr[ETHER_ADDR_LEN];

	if (OF_getprop(phy, "local-mac-address", addr, ETHER_ADDR_LEN) == -1) {
		/* Missing MAC address should be marked by clearing it */
		memset(hwaddr, 0, ETHER_ADDR_LEN);
	} else
		memcpy(hwaddr, addr, ETHER_ADDR_LEN);
}

static boolean_t
bgx_fdt_phy_mode_match(struct bgx *bgx, char *qlm_mode, size_t size)
{

	size -= CONN_TYPE_OFFSET;

	switch (bgx->qlm_mode) {
	case QLM_MODE_SGMII:
		if (strncmp(&qlm_mode[CONN_TYPE_OFFSET], "sgmii", size) == 0)
			return (TRUE);
		break;
	case QLM_MODE_XAUI_1X4:
		if (strncmp(&qlm_mode[CONN_TYPE_OFFSET], "xaui", size) == 0)
			return (TRUE);
		if (strncmp(&qlm_mode[CONN_TYPE_OFFSET], "dxaui", size) == 0)
			return (TRUE);
		break;
	case QLM_MODE_RXAUI_2X2:
		if (strncmp(&qlm_mode[CONN_TYPE_OFFSET], "raui", size) == 0)
			return (TRUE);
		break;
	case QLM_MODE_XFI_4X1:
		if (strncmp(&qlm_mode[CONN_TYPE_OFFSET], "xfi", size) == 0)
			return (TRUE);
		break;
	case QLM_MODE_XLAUI_1X4:
		if (strncmp(&qlm_mode[CONN_TYPE_OFFSET], "xlaui", size) == 0)
			return (TRUE);
		break;
	case QLM_MODE_10G_KR_4X1:
		if (strncmp(&qlm_mode[CONN_TYPE_OFFSET], "xfi-10g-kr", size) == 0)
			return (TRUE);
		break;
	case QLM_MODE_40G_KR4_1X4:
		if (strncmp(&qlm_mode[CONN_TYPE_OFFSET], "xlaui-40g-kr", size) == 0)
			return (TRUE);
		break;
	default:
		return (FALSE);
	}

	return (FALSE);
}

int
bgx_fdt_init_phy(struct bgx *bgx)
{
	phandle_t node, child;
	phandle_t phy, mdio;
	uint8_t lmac;
	char bgx_sel[6];
	char qlm_mode[CONN_TYPE_MAXLEN];
	const char *mac;

	(void)mac;

	lmac = 0;
	/* Get BGX node from DT */
	snprintf(bgx_sel, 6, "/bgx%d", bgx->bgx_id);
	node = OF_finddevice(bgx_sel);
	if (node == 0 || node == -1) {
		device_printf(bgx->dev,
		    "Could not find %s node in FDT\n", bgx_sel);
		return (ENXIO);
	}

	for (child = OF_child(node); child > 0; child = OF_peer(child)) {
		if (OF_getprop(child, "qlm-mode", qlm_mode,
		    sizeof(qlm_mode)) <= 0) {
			/* Missing qlm-mode, skipping */
			continue;
		}

		if (!bgx_fdt_phy_mode_match(bgx, qlm_mode, sizeof(qlm_mode))) {
			/*
			 * Connection type not match with BGX mode.
			 */
			continue;
		}

		if (OF_getencprop(child, "phy-handle", &phy,
		    sizeof(phy)) <= 0) {
			if (bootverbose) {
				device_printf(bgx->dev,
				    "No phy-handle in PHY node. Skipping...\n");
			}
			continue;
		}

		/* Acquire PHY address */
		phy = OF_node_from_xref(phy);
		if (OF_getencprop(phy, "reg", &bgx->lmac[lmac].phyaddr,
		    sizeof(bgx->lmac[lmac].phyaddr)) <= 0) {
			if (bootverbose) {
				device_printf(bgx->dev,
				    "Could not retrieve PHY address\n");
			}
			bgx->lmac[lmac].phyaddr = MII_PHY_ANY;
		}

		/*
		 * Get PHY interface (MDIO bus) device.
		 * Driver must be already attached.
		 */
		mdio = OF_parent(phy);
		bgx->lmac[lmac].phy_if_dev =
		    OF_device_from_xref(OF_xref_from_node(mdio));
		if (bgx->lmac[lmac].phy_if_dev == NULL) {
			if (bootverbose) {
				device_printf(bgx->dev,
				    "Could not find interface to PHY\n");
			}
			continue;
		}

		/* Get mac address from FDT */
		bgx_fdt_get_macaddr(phy, bgx->lmac[lmac].mac);

		bgx->lmac[lmac].lmacid = lmac;
		lmac++;
		if (lmac == MAX_LMAC_PER_BGX)
			break;
	}
	if (lmac == 0) {
		device_printf(bgx->dev, "Could not find matching PHY\n");
		return (ENXIO);
	}

	return (0);
}
