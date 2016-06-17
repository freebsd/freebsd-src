/*
 * arch/ppc/platforms/4xx/ibm440gx.c
 *
 * PPC440GX I/O descriptions
 *
 * Matt Porter <mporter@mvista.com>
 * Copyright 2002-2003 MontaVista Software Inc.
 *
 * Eugene Surovegin <eugene.surovegin@zultys.com> or <ebs@ebshome.net>
 * Copyright (c) 2003 Zultys Technologies
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <platforms/ibm440gx.h>
#include <asm/ocp.h>

#if defined(EMAC_NUMS) && EMAC_NUMS > 0
u32 emac_phy_map[EMAC_NUMS];
EXPORT_SYMBOL(emac_phy_map);
#endif

static struct ocp_func_emac_data ibm440gx_emac0_def = {
	.zmii_idx       = 0,            /* ZMII device index */
	.zmii_mux       = 0,            /* ZMII input of this EMAC */
	.mal_idx        = 0,            /* MAL device index */
	.mal_rx_chan    = 0,            /* MAL rx channel number */
	.mal_tx1_chan   = 0,            /* MAL tx channel 1 number */
	.mal_tx2_chan   = -1,           /* MAL tx channel 2 number */
	.wol_irq        = BL_MAC_WOL,   /* WOL interrupt number */
	.mdio_idx       = -1,           /* No shared MDIO */
};

static struct ocp_func_emac_data ibm440gx_emac1_def = {
	.zmii_idx       = 0,            /* ZMII device index */
	.zmii_mux       = 1,            /* ZMII input of this EMAC */
	.mal_idx        = 0,            /* MAL device index */
	.mal_rx_chan    = 1,            /* MAL rx channel number */
	.mal_tx1_chan   = 1,            /* MAL tx channel 1 number */
	.mal_tx2_chan   = -1,           /* MAL tx channel 2 number */
	.wol_irq        = BL_MAC_WOL1,   /* WOL interrupt number */
	.mdio_idx       = -1,           /* No shared MDIO */
};

static struct ocp_func_mal_data ibm440gx_mal0_def = {
	.num_tx_chans   = 2*EMAC_NUMS,  /* Number of TX channels */
	.num_rx_chans   = EMAC_NUMS,    /* Number of RX channels */
};

struct ocp_def core_ocp[] __initdata = {
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_OPB,
	  .index	= 0,
	  .paddr	= PPC440GX_OPB_BASE_START,
	  .irq		= OCP_IRQ_NA,
	  .pm		= OCP_CPM_NA,
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_16550,
	  .index	= 0,
	  .paddr	= PPC440GX_UART0_ADDR,
	  .irq		= UART0_INT,
	  .pm		= IBM_CPM_UART0,
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_16550,
	  .index	= 1,
	  .paddr	= PPC440GX_UART1_ADDR,
	  .irq		= UART1_INT,
	  .pm		= IBM_CPM_UART1,
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_IIC,
	  .index	= 0,
	  .paddr	= PPC440GX_IIC0_ADDR,
	  .irq		= IIC0_IRQ,
	  .pm		= IBM_CPM_IIC0,
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_IIC,
	  .index	= 1,
	  .paddr	= PPC440GX_IIC1_ADDR,
	  .irq		= IIC1_IRQ,
	  .pm		= IBM_CPM_IIC1,
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_GPIO,
	  .index	= 0,
	  .paddr	= PPC440GX_GPIO0_ADDR,
	  .irq		= OCP_IRQ_NA,
	  .pm		= IBM_CPM_GPIO0,
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_MAL,
	  .paddr	= OCP_PADDR_NA,
	  .irq		= OCP_IRQ_NA,
	  .pm		= OCP_CPM_NA,
	  .additions	= &ibm440gx_mal0_def,
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_EMAC,
	  .index	= 0,
	  .paddr	= PPC440GX_EMAC0_ADDR,
	  .irq		= BL_MAC_ETH0,
	  .pm		= OCP_CPM_NA,
	  .additions	= &ibm440gx_emac0_def,
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_EMAC,
	  .index	= 1,
	  .paddr	= PPC440GX_EMAC1_ADDR,
	  .irq		= BL_MAC_ETH1,
	  .pm		= OCP_CPM_NA,
	  .additions	= &ibm440gx_emac1_def,
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_ZMII,
	  .paddr	= PPC440GX_ZMII_ADDR,
	  .irq		= OCP_IRQ_NA,
	  .pm		= OCP_CPM_NA,
	},
	{ .vendor	= OCP_VENDOR_INVALID
	}
};
