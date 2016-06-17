/*
 * v320usc.h: Register defines for the V320USC family of devices.
 *
 * Copyright (C) 1999,2000 Dan Aizenstros (dan@vcubed.com)
 *
 * $Id: pci-v320usc.h,v 1.1 2003/02/20 04:26:36 gerg Exp $
 */

#ifndef _V320USC_H_
#define _V320USC_H_

#include <linux/pci.h>

/*
 * General parameters
 */
/* set debug level 4=verbose...1=terse */
#define DEBUG_PCI 3
// #undef DEBUG_PCI

#ifdef DEBUG_PCI
#define PCIDBG(n, x...) { if(DEBUG_PCI>=n) printk(x); }
#else
#define PCIDBG(n, x...)
#endif

#if 0
/* startup values */
#define PCI_PROBE_BIOS 1
#define PCI_PROBE_CONF1 2
#define PCI_PROBE_CONF2 4
#define PCI_NO_SORT 0x100
#define PCI_BIOS_SORT 0x200
#define PCI_NO_CHECKS 0x400
#define PCI_ASSIGN_ROMS 0x1000
#define PCI_BIOS_IRQ_SCAN 0x2000
#endif


/*
 *	platform specific parameters
 */
 
/*
 * PCI Command Register
 * - Offset 04h, Size 16 bits
 */
#define PCI_CMD_W_IO_EN			0x0001		/* I/O access */
#define PCI_CMD_W_MEM_EN		0x0002		/* Memory access */
#define PCI_CMD_W_MASTER_EN		0x0004		/* PCI Master */
#define PCI_CMD_W_MWI_EN		0x0010		/* Memory Write and */
							/* Invalidate enable            */
#define PCI_CMD_W_PAR_EN		0x0040		/* Parity error */
#define PCI_CMD_W_SERR_EN		0x0100		/* System error */
							/* If PAR_EN is enabled then SERR is  */
							/* driven in response to parity error */
#define PCI_CMD_W_FBB_EN		0x0200		/* Fast back to back */
							/* transfers when Bus Master     */

/*
 * PCI Status Register
 * - Offset 06h, Size 16 bits
 */
#define PCI_STAT_W_NEW_CAP		0x0010		/* New Capabilites          */
#define PCI_STAT_W_UDF			0x0040		/* User Defined Feature     */
#define PCI_STAT_W_FAST_BACK		0x0008		/* Fast Back to Back Target */
							/* - Used to indicate ability of this   */
							/* device to other Bus Masters          */
#define PCI_STAT_W_PAR_REP		0x0010		/* Data Parity Report when    */
							/* USC is a Bus Master and PERR is driven */
#define PCI_STAT_W_DEVSEL_MASK		0x0600		/* 10-9 Bits Device Select */
							/* Timing                              */
#define PCI_STAT_W_DEVSEL_SHIFT		9

#define PCI_STAT_W_T_ABORT		0x1000		/* Target Abort - set in */
							/* response to a target abort detected */
							/* while USC was a Bus Master          */
#define PCI_STAT_W_M_ABORT		0x2000		/* Master Abort - set in   */
							/* response to a master abort detected */
							/* while USC was a Bus Master          */
#define PCI_STAT_W_SYS_ERR		0x4000		/* System Error - set in */
							/* response to a system error on the */
							/* SERR pin                          */
#define PCI_STAT_W_PAR_ERR		0x8000		/* Parity Error - set in */
							/* response to a parity error on the */
							/* PCI bus                           */

/*
 * PCI Class and Revision Register
 * - Offset 08h, Size 32 bits
 */
#define PCI_CC_REV_VREV_MASK		0x0000000f	/* 3-0 Bits Stepping ID  */
							/* Rev A = 0,Rev B0 = 1, Rev B1 = 2, */
							/* Rev B2 = 3 */
#define PCI_CC_REV_VREV_SHIFT		0

#define PCI_CC_REV_UREV_MASK		0x000000f0	/* 7-4 Bits User Revision ID */
							/* user definable for system revisions   */
#define PCI_CC_REV_UREV_SHIFT		4

#define PCI_CC_REV_PROG_IF_MASK		0x0000ff00	/* 15-8 Bits PCI Programming */
							/* Interface code                        */
#define PCI_CC_REV_PROG_IF_SHIFT	8

#define PCI_CC_REV_SUB_CLASS_MASK	0x00ff0000	/* 23-16 Bits PCI Sub Class */
#define PCI_CC_REV_SUB_CLASS_SHIFT	16

#define PCI_CC_REV_BASE_CLASS_MASK	0xff000000	/* 32-24 Bits PCI Base Class */
#define PCI_CC_REV_BASE_CLASS_SHIFT 	24

/*
 * PCI Access to local memory map access
 * - Offset 10h, Size 32 bits (I2O mode)
 */
#define PCI_I2O_BASE_IO			0x00000001	/* I/O 1 - I/O space */
							/* 0 - Memory Space              */
#define PCI_I2O_BASE_TYPE_MASK		0x00000006	/* 2-1 Bits Address range */
							/* type                               */
#define PCI_I2O_BASE_TYPE_SHIFT		1		/* 0 - device can be mapped */
							/* any where in a 32 bit address space  */
#define PCI_I2O_BASE_PREFETCH		0x00000008	/* Prefetchable - no effect */
#define PCI_I2O_BASE_ADR_BASE_MASK	0xfff00000	/* 31-20 Bits Base address */
							/* of ATU                              */
#define PCI_I2O_BASE_ADR_BASE_SHIFT	20

/*
 * PCI Access to local memory map access
 * - Offset 14h, Size 32 bits
 */
#define PCI_MEM_BASE_IO			0x00000001	/* I/O 1 - I/O space */
							/* 0 - Memory Space              */
#define PCI_MEM_BASE_TYPE_MASK		0x00000006	/* 2-1 Bits Address range */
							/* type                               */
#define PCI_MEM_BASE_TYPE_SHIFT		1		/* 0 - device can be mapped */
							/* any where in a 32 bit address space  */
#define PCI_MEM_BASE_PREFETCH		0x00000008	/* Prefetchable - no effect */
#define PCI_MEM_BASE_ADR_BASE_MASK	0xfff00000	/* 31-20 Bits Base address */
							/* of ATU                              */
#define PCI_MEM_BASE_ADR_BASE_SHIFT	20

/*
 * PCI Bus Parameters Register
 * - Offset 3ch, Size 32 bits
 */
#define PCI_BPARAM_INT_LINE_MASK	0x000000ff	/* 7-0 Bits Interrupt Line */
#define PCI_BPARAM_INT_LINE_SHIFT	0
#define PCI_BPARAM_INT_PIN_MASK		0x00000700	/* 10-8 Bits Interrupt Pin */
							/* 0 - disable, 1 - INTA, 2 - INT B    */
							/* 3 - INT C, 4 - INT C                */
#define PCI_BPARAM_INT_PIN_SHIFT	8
#define PCI_BPARAM_MIN_GRANT_MASK	0x00ff0000	/* 23-16 Bits Minimum Grant */
#define PCI_BPARAM_MIN_GRANT_SHIFT	16
#define PCI_BPARAM_MAX_LAT_MASK		0xff000000	/* 31-24 Bits Maximum Latency */
#define PCI_BPARAM_MAX_LAT_SHIFT	24

/*
 * LB_PCI_BASEx Registers
 * - Offset 60h, Size 32 bits
 * - Offset 64h, Size 32 bits
 */
#define LB_PCI_BASEX_ALOW_MASK		0x00000003	/* select value AD1:0 */
#define LB_PCI_BASEX_ALOW_SHIFT		0x00000000
#define LB_PCI_BASEX_ERR_EN		0x00000004
#define LB_PCI_BASEX_PREFETCH		0x00000008	/* prefetch */
#define LB_PCI_BASEX_SIZE_DISABLE	0x00000000
#define LB_PCI_BASEX_SIZE_16MB		0x00000010
#define LB_PCI_BASEX_SIZE_32MB		0x00000020
#define LB_PCI_BASEX_SIZE_64MB		0x00000030
#define LB_PCI_BASEX_SIZE_128MB		0x00000040
#define LB_PCI_BASEX_SIZE_256MB		0x00000050
#define LB_PCI_BASEX_SIZE_512MB		0x00000060
#define LB_PCI_BASEX_SIZE_1GB		0x00000070
#define LB_PCI_BASEX_BYTE_SWAP_NO	0x00000000	/* No swap 32 bits */
#define LB_PCI_BASEX_BYTE_SWAP_16	0x00000100	/* 16 bits */
#define LB_PCI_BASEX_BYTE_SWAP_8	0x00000200	/* bits */
#define LB_PCI_BASEX_BYTE_SWAP_AUTO	0x00000300	/* Auto swap use BE[3:0] */
#define LB_PCI_BASEX_COMBINE		0x00000800	/* Burst Write Combine */

#define LB_PCI_BASEX_PCI_CMD_MASK	0x0000e000
#define LB_PCI_BASEX_PCI_CMD_SHIFT	13
#define LB_PCI_BASEX_INT_ACK		0x00000000	/* Interrupt Ack */
#define LB_PCI_BASEX_IO			0x00002000	/* I/O Read/Write */
#define LB_PCI_BASEX_MEMORY		0x00006000	/* Memory Read/Write */
#define LB_PCI_BASEX_CONFIG		0x0000a000	/* Configuration Read/Write */
#define LB_PCI_BASEX_MULTI_MEMORY	0x0000c000	/* Multiple Memory Read/Write */
#define LB_PCI_BASEX_MEMORY_INVALIDATE	0x0000e000	/* Multiple Memory Read/e */
							/* Write Invalidate       */
#define LB_PCI_BASEX_MAP_ADR_MASK	0x00ff0000	/* PCI Address map */
#define LB_PCI_BASEX_MAP_ADR_SHIFT	16
#define LB_PCI_BASEX_BASE		0xff000000	/* Local Address base */
#define LB_PCI_BASEX_BASE_ADR_SHIFT	24


/*
 * SDRAM Local Base Address Register 
 * - Offset 78h, Size 32 bits
 */
#define LB_SDRAM_BASE_ENABLE		0x01		/* must be enabled to access */
#define LB_SDRAM_BASE_SIZE_64M		0x00
#define LB_SDRAM_BASE_SIZE_128M		0x10
#define LB_SDRAM_BASE_SIZE_256M		0x20
#define LB_SDRAM_BASE_SIZE_512M		0x30
#define LB_SDRAM_BASE_SIZE_1G		0x40

#define LB_SDRAM_BASE_MASK		0xfc000000
#define LB_SDRAM_BASE_SHIFT		26


/*
 * Interrupt Configuration Register
 * - Offset e0h, Size 32 bits
 * - Offset e4h, Size 32 bits
 * - Offset e8h, Size 32 bits
 * - Offset 158h, Size 32 bits
 */
#define INT_CFGX_LB_MBI			0x00000001
#define INT_CFGX_PCI_MBI		0x00000002
#define INT_CFGX_I2O_OP_NE		0x00000008
#define INT_CFGX_I2O_IF_NF		0x00000010
#define INT_CFGX_I2O_IP_NE		0x00000020
#define INT_CFGX_I2O_OP_NF		0x00000040
#define INT_CFGX_I2O_OF_NE		0x00000080
#define INT_CFGX_INT0			0x00000100
#define INT_CFGX_INT1			0x00000200
#define INT_CFGX_INT2			0x00000400
#define INT_CFGX_INT3			0x00000800
#define INT_CFGX_TIMER0			0x00001000
#define INT_CFGX_TIMER1			0x00002000
#define INT_CFGX_ENUM			0x00004000
#define INT_CFGX_DMA0			0x00010000
#define INT_CFGX_DMA1			0x00020000
#define INT_CFGX_PWR_STATE		0x00100000
#define INT_CFGX_HBI			0x00200000
#define INT_CFGX_WDI			0x00400000
#define INT_CFGX_BWI			0x00800000
#define INT_CFGX_PSLAVE_PI		0x01000000
#define INT_CFGX_PMASTER_PI		0x02000000
#define INT_CFGX_PCI_T_ABORT		0x04000000
#define INT_CFGX_PCI_M_ABORT		0x08000000
#define INT_CFGX_DRA_PI			0x10000000
#define INT_CFGX_MODE			0x20000000
#define INT_CFGX_DI0			0x40000000
#define INT_CFGX_DI1			0x80000000


/*
 * Interrupt Status Register
 * - Offset ECh, Size 32 bits
 */
#define INT_STAT_BWI			0x00800000
#define INT_STAT_WDI			0x00400000
#define INT_STAT_HBI			0x00200000
#define INT_STAT_DMA1			0x00020000
#define INT_STAT_DMA0			0x00010000
#define INT_STAT_TIMER1			0x00002000
#define INT_STAT_TIMER0			0x00001000
#define INT_STAT_INT3			0x00000800
#define INT_STAT_INT2			0x00000400
#define INT_STAT_INT1			0x00000200
#define INT_STAT_INT0			0x00000100


/*
 * General Purpose Timer Control Register
 * - Offset 150h, Size 16 bits
 * - Offset 152h, Size 16 bits
 */
#define TIMER_CTLX_W_TI_MODE_0		0x0000		/* Timer input event */
#define TIMER_CTLX_W_TI_MODE_1		0x0001
#define TIMER_CTLX_W_TI_MODE_2		0x0002
#define TIMER_CTLX_W_TI_MODE_3		0x0003

#define TIMER_CTLX_W_CNT_EN_0		0x0000		/* Count enable */
#define TIMER_CTLX_W_CNT_EN_1		0x0004
#define TIMER_CTLX_W_CNT_EN_2		0x0008
#define TIMER_CTLX_W_CNT_EN_3		0x000C

#define TIMER_CTLX_W_TRG_MODE_0		0x0000		/* Trigger mode */
#define TIMER_CTLX_W_TRG_MODE_1		0x0010
#define TIMER_CTLX_W_TRG_MODE_2		0x0020
#define TIMER_CTLX_W_TRG_MODE_3		0x0030

#define TIMER_CTLX_W_TO_MODE_0		0x0000		/* Timer output mode */
#define TIMER_CTLX_W_TO_MODE_1		0x0100
#define TIMER_CTLX_W_TO_MODE_2		0x0200
#define TIMER_CTLX_W_TO_MODE_3		0x0300
#define TIMER_CTLX_W_TO_MODE_4		0x0400
#define TIMER_CTLX_W_TO_MODE_5		0x0500

#define TIMER_CTLX_W_DLTCH_0		0x0000		/* Data latch mode */
#define TIMER_CTLX_W_DLTCH_1		0x0800
#define TIMER_CTLX_W_DLTCH_2		0x1000

#define TIMER_CTLX_W_ENABLE		0x8000		/* Timer enable */


/*
 * DMA Delay Register
 * - Offset 16Ch, Size 8 bits
 */
#define DMA_DELAY_MASK			0x000000ff
#define DMA_DELAY_SHIFT			0

/*
 * DMA Command / Status Register
 * - Offset 170h, Size 32 bits
 * - Offset 174h, Size 32 bits
 */
#define DMA_CSR_IPR			0x00000001	/* initiate DMA transfer */
#define DMA_CSR_HALT			0x00000002	/* pause DMA transfer */
#define DMA_CSR_DONE			0x00000004	/* DMA transfer complete */
#define DMA_CSR_DCI			0x00000008	/* DMA control interrupt status	*/
#define DMA_CSR_DPE			0x00000010	/* DMA PCI BUS error status */
#define DMA_CSR_DONE_EN			0x00000400	/* DONE interrupt enable */
#define DMA_CSR_DCI_EN			0x00000800	/* DCI interrupt enable */
#define DMA_CSR_DPE_EN			0x00001000	/* DPE interrupt enable */
#define DMA_CSR_PRIORITY		0x00008000	/* DMA channel priority */
#define DMA_CSR_PCI_CMD0_MASK		0x000E0000	/* PCI Command Type 0 */
#define DMA_CSR_PCI_CMD0_SHIFT		17
#define DMA_CSR_PCI_CMD1_MASK		0x00E00000	/* PCI Command Type 1 */
#define DMA_CSR_PCI_CMD1_SHIFT		21
 
/*
 * DMA Transfer Control Register
 * - Offset 180h, Size 32 bits
 * - Offset 190h, Size 32 bits
 */
#define DMA_XFER_DMA_CNT_MASK		0x000FFFFF	/* DMA transfer count */
#define DMA_XFER_DMA_CNT_SHIFT		0
#define DMA_XFER_DTERM_EN		0x00400000	/* External terminate count enable */
#define DMA_XFER_BLOCK_FILL		0x00800000	/* Block fill feature enable */
#define DMA_XFER_DST_BUS		0x01000000	/* DMA destination BUS */
#define DMA_XFER_SRC_BUS		0x02000000	/* DMA source BUS */
#define DMA_XFER_PDST_TYPE		0x04000000	/* PCI destination command type */
#define DMA_XFER_PSRC_TYPE		0x08000000	/* PCI source command type */
#define DMA_XFER_SWAP_MASK		0x30000000	/* Byte swap control */
#define DMA_XFER_SWAP_SHIFT		28
#define DMA_XFER_UPDT_CNT		0x40000000	/* Update count */
#define DMA_XFER_DREQ_EN		0x80000000	/* External DRQ enable */


/*
 * DMA Control Block Register
 * - Offset 180h, Size 32 bits
 * - Offset 190h, Size 32 bits
 */
#define DMA_CTLB_BUS			0x00000001	/* DMA Control block address space */
#define DMA_CTLB_SA_INC_DIS		0x00000004	/* Source address increment disable */
#define DMA_CTLB_DA_INC_DIS		0x00000008	/* Dest address increment disable */
#define DMA_CTLB_ADDR_MASK		0xFFFFFFF0	/* DMA Control block address mask */
#define DMA_CTLB_ADDR_SHIFT		4


/*
 * V320USC registers offsets
 */
#define	V320USC_PCI_VENDOR		0x00
#define	V320USC_PCI_DEVICE		0x02
#define	V320USC_PCI_CMD_W		0x04
#define	V320USC_PCI_STAT_W		0x06
#define	V320USC_PCI_CC_REV		0x08
#define	V320USC_PCI_HDR_CFG		0x0c
#define	V320USC_PCI_I2O_BASE	0x10
#define	V320USC_PCI_MEM_BASE	0x14
#define	V320USC_PCI_REG_BASE	0x18
#define	V320USC_PCI_PCU_BASE	0x1c
#define	V320USC_PCI_BPARM		0x3c
#define	V320USC_PCI_I2O_MAP		0x50
#define	V320USC_PCI_MEM_MAP		0x54
#define	V320USC_PCI_BUS_CFG		0x5c
#define	V320USC_LB_PCI_BASE0	0x60
#define	V320USC_LB_PCI_BASE1	0x64
#define	V320USC_LB_PCU_BASE		0x6c
#define	V320USC_SYSTEM			0x73
#define	V320USC_LB_SDRAM_BASE	0x78
#define	V320USC_LB_BUS_CFG		0x7c
#define	V320USC_LB_PCI_CTL_W	0x84
#define	V320USC_DRAM_CFG	0x8C
#define	V320USC_DRAM_BLK0	0x90
#define	V320USC_DRAM_BLK1	0x94
#define	V320USC_DRAM_BLK2	0x98
#define	V320USC_DRAM_BLK3	0x9c
#define	V320USC_INT_CFG0		0xe0
#define	V320USC_INT_CFG1		0xe4
#define	V320USC_INT_CFG2		0xe8
#define	V320USC_INT_STAT		0xec
#define	V320USC_WD_HBI_W		0xf4
#define	V320USC_TIMER_DATA0		0x140
#define	V320USC_TIMER_DATA1		0x144
#define	V320USC_TIMER_CTL0_W		0x150
#define	V320USC_TIMER_CTL1_W		0x152
#define	V320USC_INT_CFG3		0x158
#define	V320USC_DMA_DELAY		0x16C
#define	V320USC_DMA_CSR0		0x170
#define	V320USC_DMA_CSR1		0x174
#define	V320USC_DMA_XFER_CTL0		0x180
#define	V320USC_DMA_SRC_ADR0		0x184
#define	V320USC_DMA_DST_ADR0		0x188
#define	V320USC_DMA_CTLB_ADR0		0x18C
#define	V320USC_DMA_XFER_CTL1		0x190
#define	V320USC_DMA_SRC_ADR1		0x194
#define	V320USC_DMA_DST_ADR1		0x198
#define	V320USC_DMA_CTLB_ADR1		0x19C

/*
 * Vendor/Device ID settings
 */

#define	V3USC_PCI_VENDOR		0x11b0
#define	V3USC_PCI_DEVICE_MIPS_9 0x0100
#define	V3USC_PCI_DEVICE_MIPS_5 0x0101
#define	V3USC_PCI_DEVICE_SH3	0x0102
#define	V3USC_PCI_DEVICE_SH4	0x0103

/*
 * Stepping of V3USC320 as read back from V3USC_PCI_CC_REV register
 */
#define V3USC_REV_A0 0
#define V3USC_REV_B0 1
#define V3USC_REV_B1 2

/*
 * PCI Bus Parameters Register
 * - Offset 3ch, Size 32 bits
 */
#define INTERRUPT_PIN_DISABLE		0x0		/* Disabled */
#define INTERRUPT_PIN_INTA		0x1		/* Use INTA */
#define INTERRUPT_PIN_INTB		0x2		/* Use INTB */
#define INTERRUPT_PIN_INTC		0x3		/* Use INTC */
#define INTERRUPT_PIN_INTD		0x4		/* Use INTD */

/*
 * PCI Base Address for Peripheral Access 
 * - Offset 1ch, Size 32 bits
 * PCI Intelligent I/O Address Translation Unit Local Bus Address Map Register
 * - Offset ??h, Size 32 bits
 */
#define BYTE_SWAP_NO			0x0		/* No swap 32 bits */
#define BYTE_SWAP_16			0x1		/* 16 bits */
#define BYTE_SWAP_8			0x2		/* 8 bits */
#define BYTE_SWAP_AUTO			0x3		/* Auto swap use BE[3:0]   */
#define APERTURE_SIZE_1M		0x0		/* Aperture size of 1 MB   */
#define APERTURE_SIZE_2M		0x1		/* Aperture size of 2 MB   */
#define APERTURE_SIZE_4M		0x2		/* Aperture size of 4 MB   */
#define APERTURE_SIZE_8M		0x3		/* Aperture size of 8 MB   */
#define APERTURE_SIZE_16M		0x4		/* Aperture size of 16 MB  */
#define APERTURE_SIZE_32M		0x5		/* Aperture size of 32 MB  */
#define APERTURE_SIZE_64M		0x6		/* Aperture size of 64 MB  */
#define APERTURE_SIZE_128M		0x7		/* Aperture size of 128 MB */
#define APERTURE_SIZE_256M		0x8		/* Aperture size of 256 MB */
#define APERTURE_SIZE_512M		0x9		/* Aperture size of 512 MB */
#define APERTURE_SIZE_1G		0xa		/* Aperture size of 1 GB   */

#endif /* _V320USC_H_ */
