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
#define	_RMI_BOARD_H_

/*
 * Engineering boards have a major/minor number in their EEPROM to 
 * identify their configuration
 */
#define	RMI_XLR_BOARD_ARIZONA_I		1
#define	RMI_XLR_BOARD_ARIZONA_II	2
#define	RMI_XLR_BOARD_ARIZONA_III	3
#define	RMI_XLR_BOARD_ARIZONA_IV	4
#define	RMI_XLR_BOARD_ARIZONA_V		5
#define	RMI_XLR_BOARD_ARIZONA_VI	6
#define	RMI_XLR_BOARD_ARIZONA_VII	7
#define	RMI_XLR_BOARD_ARIZONA_VIII	8
#define	RMI_XLR_BOARD_ARIZONA_XI	11
#define	RMI_XLR_BOARD_ARIZONA_XII	12

/*
 * RMI Chips - Values in Processor ID field
 */
#define	RMI_CHIP_XLR732		0x00
#define	RMI_CHIP_XLR716		0x02
#define	RMI_CHIP_XLR308		0x06
#define	RMI_CHIP_XLR532		0x09

/*
 * XLR C revisions
 */
#define	RMI_CHIP_XLR308_C	0x0F
#define	RMI_CHIP_XLR508_C	0x0b
#define	RMI_CHIP_XLR516_C	0x0a
#define	RMI_CHIP_XLR532_C	0x08

/*
 * XLS processors
 */
#define	RMI_CHIP_XLS408		0x88  /* Lite "Condor" */
#define	RMI_CHIP_XLS608		0x80  /* Internal */
#define	RMI_CHIP_XLS404		0x8c  /* Lite "Condor" */
#define	RMI_CHIP_XLS208		0x8e
#define	RMI_CHIP_XLS204		0x8f
#define	RMI_CHIP_XLS108		0xce
#define	RMI_CHIP_XLS104		0xcf

/*
 * XLS B revision chips
 */
#define	RMI_CHIP_XLS616_B0	0x40
#define	RMI_CHIP_XLS608_B0	0x4a
#define	RMI_CHIP_XLS416_B0	0x44
#define	RMI_CHIP_XLS412_B0	0x4c
#define	RMI_CHIP_XLS408_B0	0x4e
#define	RMI_CHIP_XLS404_B0	0x4f

/* 
 * The XLS product line has chip versions 0x4x and 0x8x
 */
static __inline unsigned int
xlr_is_xls(void)
{
	uint32_t prid = mips_rd_prid();

	return ((prid & 0xf000) == 0x8000 || (prid & 0xf000) == 0x4000 ||
	    (prid & 0xf000) == 0xc000);
}

/*
 * The last byte of the processor id field is revision
 */
static __inline unsigned int
xlr_revision(void)
{

	return (mips_rd_prid() & 0xff);
}

/*
 * The 15:8 byte of the PR Id register is the Processor ID
 */
static __inline unsigned int
xlr_processor_id(void)
{

	return ((mips_rd_prid() & 0xff00) >> 8);
}

/*
 * The processor is XLR and C-Series
 */
static __inline unsigned int
xlr_is_c_revision(void)
{
	int processor_id = xlr_processor_id();
	int revision_id  = xlr_revision();

	switch (processor_id) {
	/* 
	 * These are the relevant PIDs for XLR
	 * steppings (hawk and above). For these,
	 * PIDs, Rev-Ids of [5-9] indicate 'C'.
	 */
	case RMI_CHIP_XLR308_C:
	case RMI_CHIP_XLR508_C:
	case RMI_CHIP_XLR516_C:
	case RMI_CHIP_XLR532_C:
	case RMI_CHIP_XLR716:
	case RMI_CHIP_XLR732:
		if (revision_id >= 5 && revision_id <= 9) 
			return (1);
	default:
		return (0);
	}
	return (0);
}

/*
 * RMI Engineering boards which are PCI cards
 * These should come up in PCI device mode (not yet)
 */
static __inline int
xlr_board_pci(int board_major)
{

	return ((board_major == RMI_XLR_BOARD_ARIZONA_III) ||
		(board_major == RMI_XLR_BOARD_ARIZONA_V));
}

static __inline int
xlr_is_xls1xx(void)
{
	uint32_t chipid = xlr_processor_id();

	return (chipid == 0xce || chipid == 0xcf);
}

static __inline int
xlr_is_xls2xx(void)
{
	uint32_t chipid = xlr_processor_id();

	return (chipid == 0x8e || chipid == 0x8f);
}

static __inline int
xlr_is_xls4xx_lite(void)
{
	uint32_t chipid = xlr_processor_id();

	return (chipid == 0x88 || chipid == 0x8c);
}

static __inline unsigned int
xlr_is_xls_b0(void)
{
	uint32_t chipid = xlr_processor_id();

	return (chipid >= 0x40 && chipid <= 0x4f);
}

/* SPI-4 --> 8 ports, 1G MAC --> 4 ports and 10G MAC --> 1 port */
#define	MAX_NA_PORTS		8

/* all our knowledge of chip and board that cannot be detected run-time goes here */
enum gmac_block_types { XLR_GMAC, XLR_XGMAC, XLR_SPI4};
enum gmac_port_types  { XLR_RGMII, XLR_SGMII, XLR_PORT0_RGMII, XLR_XGMII, XLR_XAUI };
enum i2c_dev_types { I2C_RTC, I2C_THERMAL, I2C_EEPROM };

struct xlr_board_info {
	int is_xls;
	int nr_cpus;
	int usb;                               /* usb enabled ? */
	int cfi;                               /* compact flash driver for NOR? */
	int ata;                               /* ata driver */
	int pci_irq;
	struct stn_cc **credit_configs;        /* pointer to Core station credits */
	struct bucket_size *bucket_sizes;      /* pointer to Core station bucket */
	int *msgmap;                           /* mapping of message station to devices */
	int gmacports;                         /* number of gmac ports on the board */
	struct xlr_i2c_dev_t {
		uint32_t addr;	
		unsigned int enabled;          /* mask of devs enabled */   
		int type;
		int unit;
		char *dev_name;
	} xlr_i2c_device[3];
	struct xlr_gmac_block_t {              /* refers to the set of GMACs controlled by a 
                                                  network accelarator */
		int  type;                     /* see  enum gmac_block_types */
		unsigned int enabled;          /* mask of ports enabled */   
		struct stn_cc *credit_config;  /* credit configuration */
		int station_id;		       /* station id for sending msgs */
		int station_txbase;            /* station id for tx */
		int station_rfr;               /* free desc bucket */
		int  mode;                     /* see gmac_block_modes */
		uint32_t baseaddr;             /* IO base */
		int baseirq;        /* first irq for this block, the rest are in sequence */
		int baseinst;       /* the first rge unit for this block */
		int num_ports;
		struct xlr_gmac_port {
			int valid;
			int type;		/* see enum gmac_port_types */
			uint32_t instance;	/* identifies the GMAC to which
						   this port is bound to. */
			uint32_t phy_addr;
			uint32_t base_addr;
			uint32_t mii_addr;
			uint32_t pcs_addr;
			uint32_t serdes_addr;
			uint32_t tx_bucket_id;
			uint32_t mdint_id;
		} gmac_port[MAX_NA_PORTS];
	} gmac_block [3];
};

extern struct xlr_board_info xlr_board_info;
int xlr_board_info_setup(void);

#endif
