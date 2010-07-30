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
#include <mips/rmi/board.h>
#include <mips/rmi/pic.h>
#include <mips/rmi/shared_structs.h>

static int xlr_rxstn_to_txstn_map[128] = {
	[0 ... 7] = TX_STN_CPU_0,
	[8 ... 15] = TX_STN_CPU_1,
	[16 ... 23] = TX_STN_CPU_2,
	[24 ... 31] = TX_STN_CPU_3,
	[32 ... 39] = TX_STN_CPU_4,
	[40 ... 47] = TX_STN_CPU_5,
	[48 ... 55] = TX_STN_CPU_6,
	[56 ... 63] = TX_STN_CPU_7,
	[64 ... 95] = TX_STN_INVALID,
	[96 ... 103] = TX_STN_GMAC,
	[104 ... 107] = TX_STN_DMA,
	[108 ... 111] = TX_STN_INVALID,
	[112 ... 113] = TX_STN_XGS_0,
	[114 ... 115] = TX_STN_XGS_1,
	[116 ... 119] = TX_STN_INVALID,
	[120 ... 127] = TX_STN_SAE
};

static int xls_rxstn_to_txstn_map[128] = {
	[0 ... 7] = TX_STN_CPU_0,
	[8 ... 15] = TX_STN_CPU_1,
	[16 ... 23] = TX_STN_CPU_2,
	[24 ... 31] = TX_STN_CPU_3,
	[32 ... 63] = TX_STN_INVALID,
	[64 ... 71] = TX_STN_PCIE,
	[72 ... 79] = TX_STN_INVALID,
	[80 ... 87] = TX_STN_GMAC1,
	[88 ... 95] = TX_STN_INVALID,
	[96 ... 103] = TX_STN_GMAC0,
	[104 ... 107] = TX_STN_DMA,
	[108 ... 111] = TX_STN_CDE,
	[112 ... 119] = TX_STN_INVALID,
	[120 ... 127] = TX_STN_SAE
};

struct stn_cc *xlr_core_cc_configs[] = {&cc_table_cpu_0, &cc_table_cpu_1,
	&cc_table_cpu_2, &cc_table_cpu_3,
	&cc_table_cpu_4, &cc_table_cpu_5,
&cc_table_cpu_6, &cc_table_cpu_7};

struct stn_cc *xls_core_cc_configs[] = {&xls_cc_table_cpu_0, &xls_cc_table_cpu_1,
&xls_cc_table_cpu_2, &xls_cc_table_cpu_3};

struct xlr_board_info xlr_board_info;

/*
 * All our knowledge of chip and board that cannot be detected by probing
 * at run-time goes here
 */
int 
xlr_board_info_setup()
{

	if (xlr_is_xls()) {
		xlr_board_info.is_xls = 1;
		xlr_board_info.nr_cpus = 8;
		xlr_board_info.usb = 1;
		/* Board version 8 has NAND flash */
		xlr_board_info.cfi =
		    (xlr_boot1_info.board_major_version != RMI_XLR_BOARD_ARIZONA_VIII);
		xlr_board_info.pci_irq = 0;
		xlr_board_info.credit_configs = xls_core_cc_configs;
		xlr_board_info.bucket_sizes = &xls_bucket_sizes;
		xlr_board_info.msgmap = xls_rxstn_to_txstn_map;
		xlr_board_info.gmacports = 8;

		/* network block 0 */
		xlr_board_info.gmac_block[0].type = XLR_GMAC;
		xlr_board_info.gmac_block[0].enabled = 0xf;
		xlr_board_info.gmac_block[0].credit_config = &xls_cc_table_gmac0;
		xlr_board_info.gmac_block[0].station_txbase = MSGRNG_STNID_GMACTX0;
		xlr_board_info.gmac_block[0].station_rfr = MSGRNG_STNID_GMACRFR_0;
		if (xlr_boot1_info.board_major_version == RMI_XLR_BOARD_ARIZONA_VI ||
		    xlr_boot1_info.board_major_version == RMI_XLR_BOARD_ARIZONA_XI ||
		    xlr_boot1_info.board_major_version == RMI_XLR_BOARD_ARIZONA_XII)
			xlr_board_info.gmac_block[0].mode = XLR_PORT0_RGMII;
		else
			xlr_board_info.gmac_block[0].mode = XLR_SGMII;
		xlr_board_info.gmac_block[0].baseaddr = XLR_IO_GMAC_0_OFFSET;
		xlr_board_info.gmac_block[0].baseirq = PIC_GMAC_0_IRQ;
		xlr_board_info.gmac_block[0].baseinst = 0;

		/* network block 1 */
		xlr_board_info.gmac_block[1].type = XLR_GMAC;
		xlr_board_info.gmac_block[1].enabled = xlr_is_xls1xx() ? 0 : 0xf;
		if (xlr_is_xls4xx_lite()) {
			xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_GPIO_OFFSET);
			uint32_t tmp;

			/* some ports are not enabled on the condor 4xx, figure this
			   out from the GPIO fuse bank */
			tmp = xlr_read_reg(mmio, 35);
			if (tmp & (1<<28))
				xlr_board_info.gmac_block[1].enabled &= ~0x8;
			if (tmp & (1<<29))
				xlr_board_info.gmac_block[1].enabled &= ~0x4;
		}
		xlr_board_info.gmac_block[1].credit_config = &xls_cc_table_gmac1;
		xlr_board_info.gmac_block[1].station_txbase = MSGRNG_STNID_GMAC1_TX0;
		xlr_board_info.gmac_block[1].station_rfr = MSGRNG_STNID_GMAC1_FR_0;
		xlr_board_info.gmac_block[1].mode = XLR_SGMII;
		xlr_board_info.gmac_block[1].baseaddr = XLR_IO_GMAC_4_OFFSET;
		xlr_board_info.gmac_block[1].baseirq = PIC_XGS_0_IRQ;
		xlr_board_info.gmac_block[1].baseinst = 4;

		/* network block 2 */
		xlr_board_info.gmac_block[2].enabled = 0;	/* disabled on XLS */
	} else {
		xlr_board_info.is_xls = 0;
		xlr_board_info.nr_cpus = 32;
		xlr_board_info.usb = 0;
		xlr_board_info.cfi = 1;
		xlr_board_info.pci_irq = 0;
		xlr_board_info.credit_configs = xlr_core_cc_configs;
		xlr_board_info.bucket_sizes = &bucket_sizes;
		xlr_board_info.msgmap = xlr_rxstn_to_txstn_map;
		xlr_board_info.gmacports = 4;

		/* GMAC0 */
		xlr_board_info.gmac_block[0].type = XLR_GMAC;
		xlr_board_info.gmac_block[0].enabled = 0xf;
		xlr_board_info.gmac_block[0].credit_config = &cc_table_gmac;
		xlr_board_info.gmac_block[0].station_txbase = MSGRNG_STNID_GMACTX0;
		xlr_board_info.gmac_block[0].station_rfr = MSGRNG_STNID_GMACRFR_0;
		xlr_board_info.gmac_block[0].mode = XLR_RGMII;
		xlr_board_info.gmac_block[0].baseaddr = XLR_IO_GMAC_0_OFFSET;
		xlr_board_info.gmac_block[0].baseirq = PIC_GMAC_0_IRQ;
		xlr_board_info.gmac_block[0].baseinst = 0;

		/* XGMAC0  */
		xlr_board_info.gmac_block[1].type = XLR_XGMAC;
		xlr_board_info.gmac_block[1].enabled = 1;
		xlr_board_info.gmac_block[1].credit_config = &cc_table_xgs_0;
		xlr_board_info.gmac_block[1].station_txbase = MSGRNG_STNID_XGS0_TX;
		xlr_board_info.gmac_block[1].station_rfr = MSGRNG_STNID_XGS0FR;
		xlr_board_info.gmac_block[1].mode = -1;
		xlr_board_info.gmac_block[1].baseaddr = XLR_IO_XGMAC_0_OFFSET;
		xlr_board_info.gmac_block[1].baseirq = PIC_XGS_0_IRQ;
		xlr_board_info.gmac_block[1].baseinst = 4;

		/* XGMAC1 */
		xlr_board_info.gmac_block[2].type = XLR_XGMAC;
		xlr_board_info.gmac_block[2].enabled = 1;
		xlr_board_info.gmac_block[2].credit_config = &cc_table_xgs_1;
		xlr_board_info.gmac_block[2].station_txbase = MSGRNG_STNID_XGS1_TX;
		xlr_board_info.gmac_block[2].station_rfr = MSGRNG_STNID_XGS1FR;
		xlr_board_info.gmac_block[2].mode = -1;
		xlr_board_info.gmac_block[2].baseaddr = XLR_IO_XGMAC_1_OFFSET;
		xlr_board_info.gmac_block[2].baseirq = PIC_XGS_1_IRQ;
		xlr_board_info.gmac_block[2].baseinst = 5;
	}
	return 0;
}
