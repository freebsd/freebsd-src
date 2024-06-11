/*-
 * Copyright 2016 Michal Meloun <mmel@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _DEV_PHY_H_
#define	_DEV_PHY_H_

#include "opt_platform.h"

#include <sys/kobj.h>
#ifdef FDT
#include <dev/ofw/ofw_bus.h>
#endif

#define	PHY_STATUS_ENABLED	0x00000001

typedef enum phy_mode {
	PHY_MODE_INVALID,
	PHY_MODE_USB_HOST,
	PHY_MODE_USB_DEVICE,
	PHY_MODE_USB_OTG,
	PHY_MODE_UFS,
	PHY_MODE_PCIE,
	PHY_MODE_ETHERNET,
	PHY_MODE_MIPI_DPHY,
	PHY_MODE_SATA,
	PHY_MODE_LVDS,
	PHY_MODE_DP
} phy_mode_t ;

typedef enum phy_submode {
	/* Common */
	PHY_SUBMODE_NA = 0,		/* Not applicable */
	PHY_SUBMODE_INTERNAL,

	/* Ethernet  */
	PHY_SUBMODE_ETH_MII = 1000,
	PHY_SUBMODE_ETH_GMII,
	PHY_SUBMODE_ETH_SGMII,
	PHY_SUBMODE_ETH_TBI,
	PHY_SUBMODE_ETH_REVMII,
	PHY_SUBMODE_ETH_RMII,
	PHY_SUBMODE_ETH_RGMII,
	PHY_SUBMODE_ETH_RGMII_ID,
	PHY_SUBMODE_ETH_RGMII_RXID,
	PHY_SUBMODE_ETH_RGMII_TXID,
	PHY_SUBMODE_ETH_RTBI,
	PHY_SUBMODE_ETH_SMII,
	PHY_SUBMODE_ETH_XGMII,
	PHY_SUBMODE_ETH_XLGMII,
	PHY_SUBMODE_ETH_MOCA,
	PHY_SUBMODE_ETH_QSGMII,
	PHY_SUBMODE_ETH_TRGMII,
	PHY_SUBMODE_ETH_1000BASEX,
	PHY_SUBMODE_ETH_2500BASEX,
	PHY_SUBMODE_ETH_RXAUI,
	PHY_SUBMODE_ETH_XAUI,
	PHY_SUBMODE_ETH_10GBASER,
	PHY_SUBMODE_ETH_USXGMII,
	PHY_SUBMODE_ETH_10GKR,

	/* USB */
	PHY_SUBMODE_USB_LS = 2000,
	PHY_SUBMODE_USB_FS,
	PHY_SUBMODE_USB_HS,
	PHY_SUBMODE_USB_SS,

	/* UFS */
	PHY_SUBMODE_UFS_HS_A = 3000,
	PHY_SUBMODE_UFS_HS_B,

} phy_submode_t;

typedef struct phy *phy_t;

/* Initialization parameters. */
struct phynode_init_def {
	intptr_t		id;		/* Phy ID */
#ifdef FDT
	 phandle_t 		ofw_node;	/* OFW node of phy */
#endif
};

#include "phynode_if.h"

/*
 * Shorthands for constructing method tables.
 */
#define	PHYNODEMETHOD		KOBJMETHOD
#define	PHYNODEMETHOD_END	KOBJMETHOD_END
#define phynode_method_t	kobj_method_t
#define phynode_class_t		kobj_class_t
DECLARE_CLASS(phynode_class);

/*
 * Provider interface
 */
struct phynode *phynode_create(device_t pdev, phynode_class_t phynode_class,
    struct phynode_init_def *def);
struct phynode *phynode_register(struct phynode *phynode);
void *phynode_get_softc(struct phynode *phynode);
device_t phynode_get_device(struct phynode *phynode);
intptr_t phynode_get_id(struct phynode *phynode);
int phynode_enable(struct phynode *phynode);
int phynode_disable(struct phynode *phynode);
int phynode_set_mode(struct phynode *phynode, phy_mode_t mode,
    phy_submode_t submode);
int phynode_status(struct phynode *phynode, int *status);
#ifdef FDT
phandle_t phynode_get_ofw_node(struct phynode *phynode);
#endif

/*
 * Consumer interface
 */
int phy_get_by_id(device_t consumer_dev, device_t provider_dev, intptr_t id,
    phy_t *phy);
void phy_release(phy_t phy);
int phy_enable(phy_t phy);
int phy_disable(phy_t phy);
int phy_set_mode(phy_t phy, phy_mode_t mode, phy_submode_t submode);
int phy_status(phy_t phy, int *value);
#ifdef FDT
int phy_get_by_ofw_name(device_t consumer, phandle_t node, char *name,
    phy_t *phy);
int phy_get_by_ofw_idx(device_t consumer, phandle_t node, int idx, phy_t *phy);
int phy_get_by_ofw_property(device_t consumer, phandle_t node, char *name,
    phy_t *phy);
#endif

#endif /* _DEV_PHY_H_ */
