/*********************************************************************
 *
 * Copyright 2003-2006 Raza Microelectronics, Inc. (RMI). All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Raza Microelectronics, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL RMI OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES, LOSS OF USE, DATA, OR PROFITS, OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * *****************************RMI_2**********************************/
#include <sys/cdefs.h>		/* RCS ID & Copyright macro defns */
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/cpufunc.h>
#include <mips/rmi/msgring.h>
#include <mips/rmi/rmi_boot_info.h>
#include <mips/rmi/board.h>
#include <mips/rmi/pic.h>

#define XLR_I2C_RTC_ADDR 		0xd0
#define XLR_I2C_EEPROM_ADDR 		0xa0
#define XLR_I2C_TEMPSENSOR_ADDR     	0x98 
#define XLR_I2C_ATX8_TEMPSENSOR_ADDR	0x9a

struct stn_cc *xlr_core_cc_configs[] = { &cc_table_cpu_0, &cc_table_cpu_1,
    &cc_table_cpu_2, &cc_table_cpu_3, &cc_table_cpu_4, &cc_table_cpu_5,
    &cc_table_cpu_6, &cc_table_cpu_7};

struct stn_cc *xls_core_cc_configs[] = { &xls_cc_table_cpu_0, &xls_cc_table_cpu_1,
   &xls_cc_table_cpu_2, &xls_cc_table_cpu_3 };

struct xlr_board_info xlr_board_info;

static int
xlr_pcmcia_present(void)
{
	xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_GPIO_OFFSET);
	uint32_t resetconf;

	resetconf = xlr_read_reg(mmio, 21);
	return ((resetconf & 0x4000) != 0);
}

static void
xlr_chip_specific_overrides(struct xlr_board_info* board)
{
	struct xlr_gmac_block_t *blk0, *blk1, *blk2;
	uint32_t chipid;
	uint32_t revision;

	blk0 = &board->gmac_block[0];
	blk1 = &board->gmac_block[1];
	blk2 = &board->gmac_block[2];

	chipid = xlr_processor_id();
	revision = xlr_revision();
	
	if (revision == 0x04) { /* B2 */
		switch (chipid) { 
		case 0x07:  /* XLR 508 */
		case 0x08:  /* XLR 516 */
		case 0x09:  /* XLR 532 */
			/* NA[12] not available */
			memset(blk1, 0, sizeof(*blk1));
			memset(blk2, 0, sizeof(*blk2));
			break;
		case 0x06:  /* XLR 308 */
			/* NA0 has 3 ports */
			blk0->gmac_port[3].valid = 0;
			blk0->num_ports--;
			/* NA[12] not available */
			memset(blk1, 0, sizeof(*blk1));
			memset(blk2, 0, sizeof(*blk2));
			break;
		default:
			break;
		}
	} else if (revision == 0x91) { /* C4 */
		switch (chipid) { 
		case 0x0B:  /* XLR 508 */
		case 0x0A:  /* XLR 516 */
		case 0x08:  /* XLR 532 */
			/* NA[12] not available */
			memset(blk1, 0, sizeof(*blk1));
			memset(blk2, 0, sizeof(*blk2));
			break;
		case 0x0F:  /* XLR 308 */
			/* NA0 has 3 ports */
			blk0->gmac_port[3].valid = 0;
			blk0->num_ports--;
			/* NA[12] not available */
			memset(blk1, 0, sizeof(*blk1));
			memset(blk2, 0, sizeof(*blk2));
			break;
		default:
			break;
		}
	} else { /* other pre-production silicon */
		switch (chipid) { 
			/* XLR 5xx */
		case 0x0B:
		case 0x0A:
		case 0x07:
		case 0x08:
		case 0x09:
			/* NA[12] not available */
			memset(blk1, 0, sizeof(*blk1));
			memset(blk2, 0, sizeof(*blk2));
			break;
			/* XLR 3xx */
		case 0x0F:
		case 0x06:
			/* NA0 has 3 ports */
			blk0->gmac_port[3].valid = 0;
			blk0->num_ports--;
			/* NA[12] not available */
			memset(blk1, 0, sizeof(*blk1));
			memset(blk2, 0, sizeof(*blk2));
			break;
		default:
			break;
		}
	}
}

static void
xlr_board_specific_overrides(struct xlr_board_info* board)
{
	struct xlr_gmac_block_t *blk1, *blk2;

	blk1 = &board->gmac_block[1];
	blk2 = &board->gmac_block[2];

	switch (xlr_boot1_info.board_major_version) {
	case RMI_XLR_BOARD_ARIZONA_I: 
		/* ATX-I has SPI-4, not XGMAC */
		blk1->type = XLR_SPI4;
		blk1->enabled = 0;     /* nlge does not
							 support SPI-4 */
		blk2->type = XLR_SPI4;
		blk2->enabled = 0;
		break;

	case RMI_XLR_BOARD_ARIZONA_II:
		/* XGMII_A --> VSC7281, XGMII_B --> VSC7281 */
		blk1->enabled = 1;
		blk1->num_ports = 1;
		blk1->gmac_port[0].valid = 1;

		blk2->enabled = 1;
		blk2->num_ports = 1;
		blk2->gmac_port[0].valid = 1;
	default:
		break;
	}
}

static int
quad0_xaui(void)
{
	xlr_reg_t *gpio_mmio =
	    (unsigned int *)(DEFAULT_XLR_IO_BASE + XLR_IO_GPIO_OFFSET);
	uint32_t bit24;

	bit24 = (xlr_read_reg(gpio_mmio, 0x15) >> 24) & 0x1;
	return (bit24);
}

static int
quad1_xaui(void)
{
	xlr_reg_t *gpio_mmio =
	    (unsigned int *)(DEFAULT_XLR_IO_BASE + XLR_IO_GPIO_OFFSET);
	uint32_t bit25;

	bit25 = (xlr_read_reg(gpio_mmio, 0x15) >> 25) & 0x1;
	return (bit25);
}

static void
xls_chip_specific_overrides(struct xlr_board_info* board)
{
	struct xlr_gmac_block_t *blk0, *blk1;
	uint32_t chipid;

	blk0 = &board->gmac_block[0];
	blk1 = &board->gmac_block[1];
	chipid = xlr_processor_id();
	
	switch (chipid) { 
	case 0x8E: 	/* XLS208 */
	case 0x8F: 	/* XLS204 */
		/* NA1 is not available */
		memset(blk1, 0, sizeof(*blk1));
		break;
	case 0xCE:	/* XLS108 */
	case 0xCF:	/* XLS104 */
		/* NA0 has 3 ports */
		blk0->gmac_port[3].valid = 0;
		blk0->num_ports--;
		/* NA1 is not available */
		memset(blk1, 0, sizeof(*blk1));
		break;
	default:
		break;
	}
}

static void
xls_board_specific_overrides(struct xlr_board_info* board)
{
	struct xlr_gmac_block_t *blk0, *blk1;
	int i;
	struct xlr_i2c_dev_t* iic_blk;

	blk0 = &board->gmac_block[0];
	blk1 = &board->gmac_block[1];

	switch (xlr_boot1_info.board_major_version) {
	case RMI_XLR_BOARD_ARIZONA_VI:
		blk0->mode = XLR_PORT0_RGMII;
		blk0->gmac_port[0].type = XLR_RGMII;
		blk0->gmac_port[0].phy_addr = 0;
		blk0->gmac_port[0].mii_addr = XLR_IO_GMAC_4_OFFSET;
		/* Because of the Octal PHY, SGMII Quad1 is MII is also bound
		 * to the PHY attached to SGMII0_MDC/MDIO/MDINT. */
		for (i = 0; i < 4; i++) {
			blk1->gmac_port[i].mii_addr = XLR_IO_GMAC_0_OFFSET;
			blk1->gmac_port[i].serdes_addr = XLR_IO_GMAC_0_OFFSET;
		}
		blk1->gmac_port[1].mii_addr = XLR_IO_GMAC_0_OFFSET;
		blk1->gmac_port[2].mii_addr = XLR_IO_GMAC_0_OFFSET;
		blk1->gmac_port[3].mii_addr = XLR_IO_GMAC_0_OFFSET;

		blk1->gmac_port[1].serdes_addr = XLR_IO_GMAC_0_OFFSET;
		blk1->gmac_port[2].serdes_addr = XLR_IO_GMAC_0_OFFSET;
		blk1->gmac_port[3].serdes_addr = XLR_IO_GMAC_0_OFFSET;

		/* RGMII MDIO interrupt is thru NA1 and SGMII MDIO 
		 * interrupts for ports in blk1 are from NA0 */
		blk0->gmac_port[0].mdint_id = 1;

		blk1->gmac_port[0].mdint_id = 0;
		blk1->gmac_port[1].mdint_id = 0;
		blk1->gmac_port[2].mdint_id = 0;
		blk1->gmac_port[3].mdint_id = 0;

		/* If we have a 4xx lite chip, don't enable the 
		 * GMACs which are disabled in hardware */
		if (xlr_is_xls4xx_lite()) {
			xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_GPIO_OFFSET);
			uint32_t tmp;

			/* Port 6 & 7 are not enabled on the condor 4xx, figure
			 * this out from the GPIO fuse bank */
			tmp = xlr_read_reg(mmio, 35);
			if ((tmp & (3 << 28)) != 0) {
				blk1->enabled = 0x3;
				blk1->gmac_port[2].valid = 0;
				blk1->gmac_port[3].valid = 0;
				blk1->num_ports = 2;
			}
		}
		break;

	case RMI_XLR_BOARD_ARIZONA_VIII:
		iic_blk = &xlr_board_info.xlr_i2c_device[I2C_THERMAL];
		if (iic_blk->enabled) {
			iic_blk->addr = XLR_I2C_ATX8_TEMPSENSOR_ADDR;
		}	
		if (blk1->enabled) { 
			/* There is just one Octal PHY on the board and it is 
			 * connected to the MII interface for NA Quad 0. */
			for (i = 0; i < 4; i++) { 
				blk1->gmac_port[i].mii_addr =
				    XLR_IO_GMAC_0_OFFSET; 
				blk1->gmac_port[i].mdint_id = 0; 
			} 
		}
		break;

	case RMI_XLR_BOARD_ARIZONA_XI:
	case RMI_XLR_BOARD_ARIZONA_XII:
		if (quad0_xaui()) { /* GMAC ports 0-3 are set to XAUI */
			/* only GMAC0 is active i.e, the 0-th port on this quad.
			 * Disable all the other 7 possible ports. */
			for (i = 1; i < MAX_NA_PORTS; i++) {
				memset(&blk0->gmac_port[i], 0,
				    sizeof(blk0->gmac_port[i]));
			}
			/* Setup for XAUI on N/w Acc0: gmac0 */
			blk0->type 		= XLR_XGMAC;
			blk0->mode 		= XLR_XAUI;
			blk0->num_ports 	= 1;
			blk0->gmac_port[0].type = XLR_XAUI;
			blk1->gmac_port[0].phy_addr = 16;
			blk0->gmac_port[0].tx_bucket_id = blk0->station_txbase;
			/* Other addresses etc need not be modified as XAUI_0
			 * shares its addresses with SGMII GMAC_0, which was 
			 * set in the caller. */
		}
		else {
			blk0->num_ports 	= 1;  /* only 1 RGMII port */ 
			blk0->mode = XLR_PORT0_RGMII;
			blk0->gmac_port[0].type = XLR_RGMII;
			blk0->gmac_port[0].phy_addr = 0;
			blk0->gmac_port[0].mii_addr = XLR_IO_GMAC_0_OFFSET;
		}

		if (quad1_xaui()) { /* GMAC ports 4-7 are used for XAUI */
			/* only GMAC4 is active i.e, the 0-th port on this quad.
			 * Disable all the other 7 possible ports. */
			for (i = 1; i < MAX_NA_PORTS; i++) {
				memset(&blk1->gmac_port[i], 0,
				    sizeof(blk1->gmac_port[i]));
			}
			/* Setup for XAUI on N/w Acc1: gmac4 */
			blk1->type 		= XLR_XGMAC;
			blk1->mode 		= XLR_XAUI;
			blk1->num_ports 	= 1;
			/* XAUI and SGMII ports share FMN buckets on N/w Acc 1;
			   so, station_txbase, station_rfr need not be
			   patched up. */
			blk1->gmac_port[0].type = XLR_XAUI;
			blk1->gmac_port[0].phy_addr = 16;
			blk1->gmac_port[0].tx_bucket_id = blk1->station_txbase;
			/* Other addresses etc need not be modified as XAUI_1
			 * shares its addresses with SGMII GMAC_4, which was 
			 * set in the caller. */
		}
		break;

	default:
		break;
	}
}

/*
 * All our knowledge of chip and board that cannot be detected by probing
 * at run-time goes here
 */
int 
xlr_board_info_setup()
{
	struct xlr_gmac_block_t *blk0, *blk1, *blk2;
	struct xlr_i2c_dev_t* iic_blk;
	int i;

	/* This setup code is long'ish because the same base driver
	 * (if_nlge.c) is used for different: 
	 *    - CPUs (XLR/XLS)
	 *    - boards (for each CPU, multiple board configs are possible
	 *	        and available).
	 *
	 * At the time of writing, there are atleast 12 boards, 4 with XLR 
	 * and 8 with XLS. This means that the base driver needs to work with
	 * 12 different configurations, with varying levels of differences. 
	 * To accomodate the different configs, the xlr_board_info struct
	 * has various attributes for paramters that could be different. 
	 * These attributes are setup here and can be used directly in the 
	 * base driver. 
	 * It was seen that the setup code is not entirely trivial and
	 * it is possible to organize it in different ways. In the following,
	 * we choose an approach that sacrifices code-compactness/speed for
	 * readability. This is because configuration code executes once
	 * per reboot and hence has a minimal performance impact.
	 * On the other hand, driver debugging/enhancements require 
	 * that different engineers can quickly comprehend the setup 
	 * sequence. Hence, readability is seen as the key requirement for
	 * this code. It is for the reader to decide how much of this
	 * requirement is met with the current code organization !!
	 *
	 * The initialization is organized thus: 
	 *
	 * if (CPU is XLS) {
	 *    // initialize per XLS architecture
	 *       // default inits (per chip spec)
	 *       // chip-specific overrides
	 *       // board-specific overrides
	 * } else if (CPU is XLR) {
	 *    // initialize per XLR architecture
	 *       // default inits (per chip spec)
	 *       // chip-specific overrides
	 *       // board-specific overrides
	 * }
	 * 
	 * For each CPU family, all the default initializations
	 * are done for a fully-loaded device of that family. 
	 * This configuration is then adjusted for the actual
	 * chip id. This is followed up with board specific
	 * overrides. 
	 */

	/* start with a clean slate */
	memset(&xlr_board_info, 0, sizeof(xlr_board_info));
	xlr_board_info.ata =  xlr_pcmcia_present();

	blk0 = &xlr_board_info.gmac_block[0];
	blk1 = &xlr_board_info.gmac_block[1];
	blk2 = &xlr_board_info.gmac_block[2];
	
	iic_blk = xlr_board_info.xlr_i2c_device;
	iic_blk[I2C_RTC].enabled = 1;
	iic_blk[I2C_RTC].addr = XLR_I2C_RTC_ADDR;
	iic_blk[I2C_THERMAL].enabled = 1;
	iic_blk[I2C_THERMAL].addr = XLR_I2C_TEMPSENSOR_ADDR;
	iic_blk[I2C_EEPROM].enabled = 1;
	iic_blk[I2C_EEPROM].addr = XLR_I2C_EEPROM_ADDR;

	if (xlr_is_xls()) {
		xlr_board_info.is_xls = 1;
		xlr_board_info.nr_cpus = 8;
		xlr_board_info.usb = 1;
		/* Board version 8 has NAND flash */
		xlr_board_info.cfi =
		    (xlr_boot1_info.board_major_version != RMI_XLR_BOARD_ARIZONA_VIII);
		xlr_board_info.pci_irq = 0;
		xlr_board_info.credit_configs = xls_core_cc_configs;
		xlr_board_info.bucket_sizes   = &xls_bucket_sizes;
		xlr_board_info.gmacports      = MAX_NA_PORTS;

		/* ---------------- Network Acc 0 ---------------- */

		blk0->type 		= XLR_GMAC;
		blk0->enabled 		= 0xf;
		blk0->credit_config 	= &xls_cc_table_gmac0;
		blk0->station_id 	= MSGRNG_STNID_GMAC;
		blk0->station_txbase 	= MSGRNG_STNID_GMACTX0;
		blk0->station_rfr 	= MSGRNG_STNID_GMACRFR_0;
		blk0->mode 		= XLR_SGMII;
		blk0->baseaddr 		= XLR_IO_GMAC_0_OFFSET;
		blk0->baseirq 		= PIC_GMAC_0_IRQ;
		blk0->baseinst 		= 0;

		/* By default, assume SGMII is setup. But this can change based 
		   on board-specific or setting-specific info. */
		for (i = 0; i < 4; i++) {
			blk0->gmac_port[i].valid = 1;
			blk0->gmac_port[i].instance = i + blk0->baseinst;
			blk0->gmac_port[i].type = XLR_SGMII;
			blk0->gmac_port[i].phy_addr = i + 16;
			blk0->gmac_port[i].tx_bucket_id = 
			    blk0->station_txbase + i;
			blk0->gmac_port[i].mdint_id = 0;
			blk0->num_ports++;
			blk0->gmac_port[i].base_addr = XLR_IO_GMAC_0_OFFSET + i * 0x1000;
			blk0->gmac_port[i].mii_addr = XLR_IO_GMAC_0_OFFSET;
			blk0->gmac_port[i].pcs_addr = XLR_IO_GMAC_0_OFFSET;
			blk0->gmac_port[i].serdes_addr = XLR_IO_GMAC_0_OFFSET;
		}

		/* ---------------- Network Acc 1 ---------------- */
		blk1->type 		= XLR_GMAC;
		blk1->enabled 		= 0xf;
		blk1->credit_config 	= &xls_cc_table_gmac1;
		blk1->station_id 	= MSGRNG_STNID_GMAC1;
		blk1->station_txbase 	= MSGRNG_STNID_GMAC1_TX0;
		blk1->station_rfr 	= MSGRNG_STNID_GMAC1_FR_0;
		blk1->mode 		= XLR_SGMII;
		blk1->baseaddr 		= XLR_IO_GMAC_4_OFFSET;
		blk1->baseirq 		= PIC_XGS_0_IRQ;
		blk1->baseinst 		= 4;

		for (i = 0; i < 4; i++) {
			blk1->gmac_port[i].valid = 1;
			blk1->gmac_port[i].instance = i + blk1->baseinst;
			blk1->gmac_port[i].type = XLR_SGMII;
			blk1->gmac_port[i].phy_addr = i + 20;
			blk1->gmac_port[i].tx_bucket_id = 
			    blk1->station_txbase + i;
			blk1->gmac_port[i].mdint_id = 1;
			blk1->num_ports++;
			blk1->gmac_port[i].base_addr = XLR_IO_GMAC_4_OFFSET +  i * 0x1000;
			blk1->gmac_port[i].mii_addr = XLR_IO_GMAC_4_OFFSET;
			blk1->gmac_port[i].pcs_addr = XLR_IO_GMAC_4_OFFSET;
			blk1->gmac_port[i].serdes_addr = XLR_IO_GMAC_0_OFFSET;
		}

		/* ---------------- Network Acc 2 ---------------- */
		xlr_board_info.gmac_block[2].enabled = 0;  /* disabled on XLS */

		xls_chip_specific_overrides(&xlr_board_info);
		xls_board_specific_overrides(&xlr_board_info);

	} else {	/* XLR */
		xlr_board_info.is_xls = 0;
		xlr_board_info.nr_cpus = 32;
		xlr_board_info.usb = 0;
		xlr_board_info.cfi = 1;
		xlr_board_info.pci_irq = 0;
		xlr_board_info.credit_configs = xlr_core_cc_configs;
		xlr_board_info.bucket_sizes   = &bucket_sizes;
		xlr_board_info.gmacports         = 4;

		/* ---------------- GMAC0 ---------------- */
		blk0->type 		= XLR_GMAC;
		blk0->enabled 		= 0xf;
		blk0->credit_config 	= &cc_table_gmac;
		blk0->station_id 	= MSGRNG_STNID_GMAC;
		blk0->station_txbase 	= MSGRNG_STNID_GMACTX0;
		blk0->station_rfr 	= MSGRNG_STNID_GMACRFR_0;
		blk0->mode 		= XLR_RGMII;
		blk0->baseaddr 		= XLR_IO_GMAC_0_OFFSET;
		blk0->baseirq 		= PIC_GMAC_0_IRQ;
		blk0->baseinst 		= 0;

		/* first, do the common/easy stuff for all the ports */
		for (i = 0; i < 4; i++) {
			blk0->gmac_port[i].valid = 1;
			blk0->gmac_port[i].instance = i + blk0->baseinst;
			blk0->gmac_port[i].type = XLR_RGMII;
			blk0->gmac_port[i].phy_addr = i;
			blk0->gmac_port[i].tx_bucket_id = 
			    blk0->station_txbase + i;
			blk0->gmac_port[i].mdint_id = 0;
			blk0->gmac_port[i].base_addr = XLR_IO_GMAC_0_OFFSET + i * 0x1000;
			blk0->gmac_port[i].mii_addr = XLR_IO_GMAC_0_OFFSET;
			/* RGMII ports, no PCS/SERDES */
			blk0->num_ports++;
		}

		/* ---------------- XGMAC0 ---------------- */
		blk1->type 		= XLR_XGMAC;
		blk1->mode 		= XLR_XGMII;
		blk1->enabled 		= 0;
		blk1->credit_config 	= &cc_table_xgs_0;
		blk1->station_txbase 	= MSGRNG_STNID_XGS0_TX;
		blk1->station_rfr 	= MSGRNG_STNID_XMAC0RFR;
		blk1->station_id 	= MSGRNG_STNID_XGS0FR;
		blk1->baseaddr 		= XLR_IO_XGMAC_0_OFFSET;
		blk1->baseirq 		= PIC_XGS_0_IRQ;
		blk1->baseinst 		= 4;

		blk1->gmac_port[0].type 	= XLR_XGMII;
		blk1->gmac_port[0].instance 	= 0;
		blk1->gmac_port[0].phy_addr 	= 0;
		blk1->gmac_port[0].base_addr 	= XLR_IO_XGMAC_0_OFFSET;
		blk1->gmac_port[0].mii_addr 	= XLR_IO_XGMAC_0_OFFSET;
		blk1->gmac_port[0].tx_bucket_id = blk1->station_txbase;
		blk1->gmac_port[0].mdint_id 	= 1;

		/* ---------------- XGMAC1 ---------------- */
		blk2->type 		= XLR_XGMAC;
		blk2->mode 		= XLR_XGMII;
		blk2->enabled 		= 0;
		blk2->credit_config 	= &cc_table_xgs_1;
		blk2->station_txbase 	= MSGRNG_STNID_XGS1_TX;
		blk2->station_rfr 	= MSGRNG_STNID_XMAC1RFR;
		blk2->station_id 	= MSGRNG_STNID_XGS1FR;
		blk2->baseaddr 		= XLR_IO_XGMAC_1_OFFSET;
		blk2->baseirq 		= PIC_XGS_1_IRQ;
		blk2->baseinst 		= 5;

		blk2->gmac_port[0].type 	= XLR_XGMII;
		blk2->gmac_port[0].instance 	= 0;
		blk2->gmac_port[0].phy_addr 	= 0;
		blk2->gmac_port[0].base_addr 	= XLR_IO_XGMAC_1_OFFSET;
		blk2->gmac_port[0].mii_addr 	= XLR_IO_XGMAC_1_OFFSET;
		blk2->gmac_port[0].tx_bucket_id = blk2->station_txbase;
		blk2->gmac_port[0].mdint_id 	= 2;

		/* Done with default setup. Now handle chip and board-specific
		   variations. */
		xlr_chip_specific_overrides(&xlr_board_info);
		xlr_board_specific_overrides(&xlr_board_info);
  	}
  	return 0;
}
