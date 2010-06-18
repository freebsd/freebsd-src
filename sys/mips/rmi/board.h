/*-
 * Copyright (c) 2003-2009 RMI Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RMI_BSD
 * $FreeBSD$
 */
#ifndef _RMI_BOARD_H_
#define _RMI_BOARD_H_

/*
 * Engineering boards have a major/minor number in their EEPROM to 
 * identify their configuration
 */
#define RMI_XLR_BOARD_ARIZONA_I        1
#define RMI_XLR_BOARD_ARIZONA_II       2
#define RMI_XLR_BOARD_ARIZONA_III      3
#define RMI_XLR_BOARD_ARIZONA_IV       4
#define RMI_XLR_BOARD_ARIZONA_V        5
#define RMI_XLR_BOARD_ARIZONA_VI       6
#define RMI_XLR_BOARD_ARIZONA_VII      7
#define RMI_XLR_BOARD_ARIZONA_VIII     8
#define RMI_XLR_BOARD_ARIZONA_XI      11
#define RMI_XLR_BOARD_ARIZONA_XII     12

/*
 * RMI Chips - Values in Processor ID field
 */
#define	RMI_CHIP_XLR732		0x00
#define	RMI_CHIP_XLR716		0x02
#define	RMI_CHIP_XLR308		0x06
#define	RMI_CHIP_XLR532		0x09

#define RMI_CHIP_XLS616_B0      0x40
#define RMI_CHIP_XLS608_B0      0x4a
#define RMI_CHIP_XLS608         0x80  /* Internal */
#define RMI_CHIP_XLS416_B0      0x44
#define RMI_CHIP_XLS412_B0      0x4c
#define RMI_CHIP_XLS408_B0      0x4e
#define RMI_CHIP_XLS408         0x88  /* Lite "Condor" */
#define RMI_CHIP_XLS404_B0      0x4f
#define RMI_CHIP_XLS404         0x8c  /* Lite "Condor" */
#define RMI_CHIP_XLS208         0x8e
#define RMI_CHIP_XLS204         0x8f
#define RMI_CHIP_XLS108         0xce
#define RMI_CHIP_XLS104         0xcf

/* 
 * The XLS product line has chip versions 0x4x and 0x8x
 */
static __inline__ unsigned int
xlr_is_xls(void)
{
	uint32_t prid = mips_rd_prid();

	return (prid & 0xf000) == 0x8000 || (prid & 0xf000) == 0x4000;
}

/*
 * The last byte of the processor id field is revision
 */
static __inline__ unsigned int
xlr_revision(void)
{
	return mips_rd_prid() & 0xff;
}

/*
 * The 15:8 byte of the PR Id register is the Processor ID
 */
static __inline__ unsigned int
xlr_processor_id(void)
{
	return ((mips_rd_prid() & 0xff00) >> 8);
}

/*
 * RMI Engineering boards which are PCI cards
 * These should come up in PCI device mode (not yet)
 */
static __inline__ int
xlr_board_pci(void)
{
	return ((xlr_boot1_info.board_major_version == RMI_XLR_BOARD_ARIZONA_III) ||
		(xlr_boot1_info.board_major_version == RMI_XLR_BOARD_ARIZONA_V));
}

static __inline__ int
xlr_is_xls2xx(void)
{
	uint32_t chipid = xlr_processor_id();

	return (chipid == 0x8e || chipid == 0x8f);
}

static __inline__ int
xlr_is_xls4xx_lite(void)
{
	uint32_t chipid = xlr_processor_id();

	return (chipid == 0x88 || chipid == 0x8c);
}

/* all our knowledge of chip and board that cannot be detected run-time goes here */
enum gmac_block_types {
	XLR_GMAC, XLR_XGMAC, XLR_SPI4
};

enum gmac_block_modes {
	XLR_RGMII, XLR_SGMII, XLR_PORT0_RGMII
};

struct xlr_board_info {
	int is_xls;
	int nr_cpus;
	int usb;		/* usb enabled ? */
	int cfi;		/* compact flash driver for NOR? */
	int pci_irq;
	struct stn_cc **credit_configs;	/* pointer to Core station credits */
	struct bucket_size *bucket_sizes;	/* pointer to Core station
						 * bucket */
	int *msgmap;		/* mapping of message station to devices */
	int gmacports;		/* number of gmac ports on the board */
	struct xlr_gmac_block_t {
		int type;	/* see  enum gmac_block_types */
		unsigned int enabled;	/* mask of ports enabled */
		struct stn_cc *credit_config;	/* credit configuration */
		int station_txbase;	/* station id for tx */
		int station_rfr;/* free desc bucket */
		int mode;	/* see gmac_block_modes */
		uint32_t baseaddr;	/* IO base */
		int baseirq;	/* first irq for this block, the rest are in
				 * sequence */
		int baseinst;	/* the first rge unit for this block */
	}  gmac_block[3];
};

extern struct xlr_board_info xlr_board_info;
int xlr_board_info_setup(void);

#endif
