/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 1992-1997,2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_SN_PCI_PCI_DEFS_H
#define _ASM_SN_PCI_PCI_DEFS_H

#include <linux/config.h>

/* defines for the PCI bus architecture */

/* Bit layout of address fields for Type-1
 * Configuration Space cycles.
 */
#define	PCI_TYPE0_SLOT_MASK	0xFFFFF800
#define	PCI_TYPE0_FUNC_MASK	0x00000700
#define	PCI_TYPE0_REG_MASK	0x000000FF

#define	PCI_TYPE0_SLOT_SHFT	11
#define	PCI_TYPE0_FUNC_SHFT	8
#define	PCI_TYPE0_REG_SHFT	0

#define	PCI_TYPE0_FUNC(a)	(((a) & PCI_TYPE0_FUNC_MASK) >> PCI_TYPE0_FUNC_SHFT)
#define	PCI_TYPE0_REG(a)	(((a) & PCI_TYPE0_REG_MASK) >> PCI_TYPE0_REG_SHFT)

#define	PCI_TYPE0(s,f,r)	((((1<<(s)) << PCI_TYPE0_SLOT_SHFT) & PCI_TYPE0_SLOT_MASK) |\
				 (((f) << PCI_TYPE0_FUNC_SHFT) & PCI_TYPE0_FUNC_MASK) |\
				 (((r) << PCI_TYPE0_REG_SHFT) & PCI_TYPE0_REG_MASK))

/* Bit layout of address fields for Type-1
 * Configuration Space cycles.
 * NOTE: I'm including the byte offset within
 * the 32-bit word as part of the register
 * number as an extension of the layout in
 * the PCI spec.
 */
#define	PCI_TYPE1_BUS_MASK	0x00FF0000
#define	PCI_TYPE1_SLOT_MASK	0x0000F800
#define	PCI_TYPE1_FUNC_MASK	0x00000700
#define	PCI_TYPE1_REG_MASK	0x000000FF

#define	PCI_TYPE1_BUS_SHFT	16
#define	PCI_TYPE1_SLOT_SHFT	11
#define	PCI_TYPE1_FUNC_SHFT	8
#define	PCI_TYPE1_REG_SHFT	0

#define	PCI_TYPE1_BUS(a)	(((a) & PCI_TYPE1_BUS_MASK) >> PCI_TYPE1_BUS_SHFT)
#define	PCI_TYPE1_SLOT(a)	(((a) & PCI_TYPE1_SLOT_MASK) >> PCI_TYPE1_SLOT_SHFT)
#define	PCI_TYPE1_FUNC(a)	(((a) & PCI_TYPE1_FUNC_MASK) >> PCI_TYPE1_FUNC_SHFT)
#define	PCI_TYPE1_REG(a)	(((a) & PCI_TYPE1_REG_MASK) >> PCI_TYPE1_REG_SHFT)

#define	PCI_TYPE1(b,s,f,r)	((((b) << PCI_TYPE1_BUS_SHFT) & PCI_TYPE1_BUS_MASK) |\
				 (((s) << PCI_TYPE1_SLOT_SHFT) & PCI_TYPE1_SLOT_MASK) |\
				 (((f) << PCI_TYPE1_FUNC_SHFT) & PCI_TYPE1_FUNC_MASK) |\
				 (((r) << PCI_TYPE1_REG_SHFT) & PCI_TYPE1_REG_MASK))

/* Byte offsets of registers in CFG space
 */
#define	PCI_CFG_VENDOR_ID	0x00		/* Vendor ID (2 bytes) */
#define	PCI_CFG_DEVICE_ID	0x02		/* Device ID (2 bytes) */

#define	PCI_CFG_COMMAND		0x04		/* Command (2 bytes) */
#define	PCI_CFG_STATUS		0x06		/* Status (2 bytes) */

/* NOTE: if you are using a C "switch" statement to
 * differentiate between the Config space registers, be
 * aware that PCI_CFG_CLASS_CODE and PCI_CFG_PROG_IF
 * are the same offset.
 */
#define	PCI_CFG_REV_ID		0x08		/* Revision Id (1 byte) */
#define	PCI_CFG_CLASS_CODE	0x09		/* Class Code (3 bytes) */
#define	PCI_CFG_PROG_IF		0x09		/* Prog Interface (1 byte) */
#define	PCI_CFG_SUB_CLASS	0x0A		/* Sub Class (1 byte) */
#define	PCI_CFG_BASE_CLASS	0x0B		/* Base Class (1 byte) */

#define	PCI_CFG_CACHE_LINE	0x0C		/* Cache line size (1 byte) */
#define	PCI_CFG_LATENCY_TIMER	0x0D		/* Latency Timer (1 byte) */
#define	PCI_CFG_HEADER_TYPE	0x0E		/* Header Type (1 byte) */
#define	PCI_CFG_BIST		0x0F		/* Built In Self Test */

#define	PCI_CFG_BASE_ADDR_0	0x10		/* Base Address (4 bytes) */
#define	PCI_CFG_BASE_ADDR_1	0x14		/* Base Address (4 bytes) */
#define	PCI_CFG_BASE_ADDR_2	0x18		/* Base Address (4 bytes) */
#define	PCI_CFG_BASE_ADDR_3	0x1C		/* Base Address (4 bytes) */
#define	PCI_CFG_BASE_ADDR_4	0x20		/* Base Address (4 bytes) */
#define	PCI_CFG_BASE_ADDR_5	0x24		/* Base Address (4 bytes) */

#define	PCI_CFG_BASE_ADDR_OFF	0x04		/* Base Address Offset (1..5)*/
#define	PCI_CFG_BASE_ADDR(n)	(PCI_CFG_BASE_ADDR_0 + (n)*PCI_CFG_BASE_ADDR_OFF)
#define	PCI_CFG_BASE_ADDRS	6		/* up to this many BASE regs */

#define	PCI_CFG_CARDBUS_CIS	0x28		/* Cardbus CIS Pointer (4B) */

#define	PCI_CFG_SUBSYS_VEND_ID	0x2C		/* Subsystem Vendor ID (2B) */
#define	PCI_CFG_SUBSYS_ID	0x2E		/* Subsystem ID */

#define	PCI_EXPANSION_ROM	0x30		/* Expansion Rom Base (4B) */
#define	PCI_CAPABILITIES_PTR	0x34		/* Capabilities Pointer */

#define	PCI_INTR_LINE		0x3C		/* Interrupt Line (1B) */
#define	PCI_INTR_PIN		0x3D		/* Interrupt Pin (1B) */

#define PCI_CFG_VEND_SPECIFIC	0x40		/* first vendor specific reg */

/* layout for Type 0x01 headers */

#define	PCI_CFG_PPB_BUS_PRI		0x18	/* immediate upstream bus # */
#define	PCI_CFG_PPB_BUS_SEC		0x19	/* immediate downstream bus # */
#define	PCI_CFG_PPB_BUS_SUB		0x1A	/* last downstream bus # */
#define	PCI_CFG_PPB_SEC_LAT		0x1B	/* latency timer for SEC bus */
#define PCI_CFG_PPB_IOBASE		0x1C	/* IO Base Addr bits 12..15 */
#define PCI_CFG_PPB_IOLIM		0x1D	/* IO Limit Addr bits 12..15 */
#define	PCI_CFG_PPB_SEC_STAT		0x1E	/* Secondary Status */
#define PCI_CFG_PPB_MEMBASE		0x20	/* MEM Base Addr bits 16..31 */
#define PCI_CFG_PPB_MEMLIM		0x22	/* MEM Limit Addr bits 16..31 */
#define PCI_CFG_PPB_MEMPFBASE		0x24	/* PfMEM Base Addr bits 16..31 */
#define PCI_CFG_PPB_MEMPFLIM		0x26	/* PfMEM Limit Addr bits 16..31 */
#define PCI_CFG_PPB_MEMPFBASEHI		0x28	/* PfMEM Base Addr bits 32..63 */
#define PCI_CFG_PPB_MEMPFLIMHI		0x2C	/* PfMEM Limit Addr bits 32..63 */
#define PCI_CFG_PPB_IOBASEHI		0x30	/* IO Base Addr bits 16..31 */
#define PCI_CFG_PPB_IOLIMHI		0x32	/* IO Limit Addr bits 16..31 */
#define	PCI_CFG_PPB_SUB_VENDOR		0x34	/* Subsystem Vendor ID */
#define	PCI_CFG_PPB_SUB_DEVICE		0x36	/* Subsystem Device ID */
#define	PCI_CFG_PPB_ROM_BASE		0x38	/* ROM base address */
#define	PCI_CFG_PPB_INT_LINE		0x3C	/* Interrupt Line */
#define	PCI_CFG_PPB_INT_PIN		0x3D	/* Interrupt Pin */
#define	PCI_CFG_PPB_BRIDGE_CTRL		0x3E	/* Bridge Control */
     /* XXX- these might be DEC 21152 specific */
#define	PCI_CFG_PPB_CHIP_CTRL		0x40
#define	PCI_CFG_PPB_DIAG_CTRL		0x41
#define	PCI_CFG_PPB_ARB_CTRL		0x42
#define	PCI_CFG_PPB_SERR_DISABLE	0x64
#define	PCI_CFG_PPB_CLK2_CTRL		0x68
#define	PCI_CFG_PPB_SERR_STATUS		0x6A

/* Command Register layout (0x04) */
#define	PCI_CMD_IO_SPACE	0x001		/* I/O Space device */
#define	PCI_CMD_MEM_SPACE	0x002		/* Memory Space */
#define	PCI_CMD_BUS_MASTER	0x004		/* Bus Master */
#define	PCI_CMD_SPEC_CYCLES	0x008		/* Special Cycles */
#define	PCI_CMD_MEMW_INV_ENAB	0x010		/* Memory Write Inv Enable */
#define	PCI_CMD_VGA_PALETTE_SNP	0x020		/* VGA Palette Snoop */
#define	PCI_CMD_PAR_ERR_RESP	0x040		/* Parity Error Response */
#define	PCI_CMD_WAIT_CYCLE_CTL	0x080		/* Wait Cycle Control */
#define	PCI_CMD_SERR_ENABLE	0x100		/* SERR# Enable */
#define	PCI_CMD_F_BK_BK_ENABLE	0x200		/* Fast Back-to-Back Enable */

/* Status Register Layout (0x06) */
#define	PCI_STAT_PAR_ERR_DET	0x8000		/* Detected Parity Error */
#define	PCI_STAT_SYS_ERR	0x4000		/* Signaled System Error */
#define	PCI_STAT_RCVD_MSTR_ABT	0x2000		/* Received Master Abort */
#define	PCI_STAT_RCVD_TGT_ABT	0x1000		/* Received Target Abort */
#define	PCI_STAT_SGNL_TGT_ABT	0x0800		/* Signaled Target Abort */

#define	PCI_STAT_DEVSEL_TIMING	0x0600		/* DEVSEL Timing Mask */
#define	DEVSEL_TIMING(_x)	(((_x) >> 9) & 3)	/* devsel tim macro */
#define	DEVSEL_FAST		0		/* Fast timing */
#define	DEVSEL_MEDIUM		1		/* Medium timing */
#define	DEVSEL_SLOW		2		/* Slow timing */

#define	PCI_STAT_DATA_PAR_ERR	0x0100		/* Data Parity Err Detected */
#define	PCI_STAT_F_BK_BK_CAP	0x0080		/* Fast Back-to-Back Capable */
#define	PCI_STAT_UDF_SUPP	0x0040		/* UDF Supported */
#define	PCI_STAT_66MHZ_CAP	0x0020		/* 66 MHz Capable */
#define	PCI_STAT_CAP_LIST	0x0010		/* Capabilities List */

/* BIST Register Layout (0x0F) */
#define	PCI_BIST_BIST_CAP	0x80		/* BIST Capable */
#define	PCI_BIST_START_BIST	0x40		/* Start BIST */
#define	PCI_BIST_CMPLTION_MASK	0x0F		/* COMPLETION MASK */
#define	PCI_BIST_CMPL_OK	0x00		/* 0 value is completion OK */

/* Base Address Register 0x10 */
#define PCI_BA_IO_CODEMASK	0x3		/* bottom 2 bits encode I/O BAR type */
#define	PCI_BA_IO_SPACE		0x1		/* I/O Space Marker */

#define PCI_BA_MEM_CODEMASK	0xf		/* bottom 4 bits encode MEM BAR type */
#define	PCI_BA_MEM_LOCATION	0x6		/* 2 bits for location avail */
#define	PCI_BA_MEM_32BIT	0x0		/* Anywhere in 32bit space */
#define	PCI_BA_MEM_1MEG		0x2		/* Locate below 1 Meg */
#define	PCI_BA_MEM_64BIT	0x4		/* Anywhere in 64bit space */
#define	PCI_BA_PREFETCH		0x8		/* Prefetchable, no side effect */

#define PCI_BA_ROM_CODEMASK	0x1		/* bottom bit control expansion ROM enable */
#define PCI_BA_ROM_ENABLE	0x1		/* enable expansion ROM */

/* Bridge Control Register 0x3e */
#define PCI_BCTRL_DTO_SERR	0x0800		/* Discard Timer timeout generates SERR on primary bus */
#define PCI_BCTRL_DTO		0x0400		/* Discard Timer timeout status */
#define PCI_BCTRL_DTO_SEC	0x0200		/* Secondary Discard Timer: 0 => 2^15 PCI clock cycles, 1 => 2^10 */
#define PCI_BCTRL_DTO_PRI	0x0100		/* Primary Discard Timer: 0 => 2^15 PCI clock cycles, 1 => 2^10 */
#define PCI_BCTRL_F_BK_BK_ENABLE 0x0080		/* Enable Fast Back-to-Back on secondary bus */
#define PCI_BCTRL_RESET_SEC	0x0040		/* Reset Secondary bus */
#define PCI_BCTRL_MSTR_ABT_MODE	0x0020		/* Master Abort Mode: 0 => do not report Master-Aborts */
#define PCI_BCTRL_VGA_AF_ENABLE	0x0008		/* Enable VGA Address Forwarding */
#define PCI_BCTRL_ISA_AF_ENABLE	0x0004		/* Enable ISA Address Forwarding */
#define PCI_BCTRL_SERR_ENABLE	0x0002		/* Enable forwarding of SERR from secondary bus to primary bus */
#define PCI_BCTRL_PAR_ERR_RESP	0x0001		/* Enable Parity Error Response reporting on secondary interface */

/*
 * PCI 2.2 introduces the concept of ``capability lists.''  Capability lists
 * provide a flexible mechanism for a device or bridge to advertise one or
 * more standardized capabilities such as the presense of a power management
 * interface, etc.  The presense of a capability list is indicated by
 * PCI_STAT_CAP_LIST being non-zero in the PCI_CFG_STATUS register.  If
 * PCI_STAT_CAP_LIST is set, then PCI_CFG_CAP_PTR is a ``pointer'' into the
 * device-specific portion of the configuration header where the first
 * capability block is stored.  This ``pointer'' is a single byte which
 * contains an offset from the beginning of the configuration header.  The
 * bottom two bits of the pointer are reserved and should be masked off to
 * determine the offset.  Each capability block contains a capability ID, a
 * ``pointer'' to the next capability (another offset where a zero terminates
 * the list) and capability-specific data.  Each capability block starts with
 * the capability ID and the ``next capability pointer.''  All data following
 * this are capability-dependent.
 */
#define PCI_CAP_ID		0x00		/* Capability ID (1B) */
#define PCI_CAP_PTR		0x01		/* Capability ``pointer'' (1B) */

/* PCI Capability IDs */
#define	PCI_CAP_PM		0x01		/* PCI Power Management */
#define	PCI_CAP_AGP		0x02		/* Accelerated Graphics Port */
#define	PCI_CAP_VPD		0x03		/* Vital Product Data (VPD) */
#define	PCI_CAP_SID		0x04		/* Slot Identification */
#define PCI_CAP_MSI		0x05		/* Message Signaled Intr */
#define	PCI_CAP_HS		0x06		/* CompactPCI Hot Swap */
#define	PCI_CAP_PCIX		0x07		/* PCI-X */
#define PCI_CAP_ID_HT		0x08		/* HyperTransport */


/* PIO interface macros */

#ifndef IOC3_EMULATION

#define PCI_INB(x)          (*((volatile char*)x))
#define PCI_INH(x)          (*((volatile short*)x))
#define PCI_INW(x)          (*((volatile int*)x))
#define PCI_OUTB(x,y)       (*((volatile char*)x) = y)
#define PCI_OUTH(x,y)       (*((volatile short*)x) = y)
#define PCI_OUTW(x,y)       (*((volatile int*)x) = y)

#else

extern uint pci_read(void * address, int type);
extern void pci_write(void * address, int data, int type);

#define BYTE   1
#define HALF   2
#define WORD   4

#define PCI_INB(x)          pci_read((void *)(x),BYTE)
#define PCI_INH(x)          pci_read((void *)(x),HALF)
#define PCI_INW(x)          pci_read((void *)(x),WORD)
#define PCI_OUTB(x,y)       pci_write((void *)(x),(y),BYTE)
#define PCI_OUTH(x,y)       pci_write((void *)(x),(y),HALF)
#define PCI_OUTW(x,y)       pci_write((void *)(x),(y),WORD)

#endif /* !IOC3_EMULATION */
						/* effects on reads, merges */

/*
 * Definition of address layouts for PCI Config mechanism #1
 * XXX- These largely duplicate PCI_TYPE1 constants at the top
 * of the file; the two groups should probably be combined.
 */

#define CFG1_ADDR_REGISTER_MASK		0x000000fc
#define CFG1_ADDR_FUNCTION_MASK		0x00000700
#define CFG1_ADDR_DEVICE_MASK		0x0000f800
#define CFG1_ADDR_BUS_MASK		0x00ff0000

#define CFG1_REGISTER_SHIFT		2
#define CFG1_FUNCTION_SHIFT		8
#define CFG1_DEVICE_SHIFT		11
#define CFG1_BUS_SHIFT			16

#ifdef CONFIG_SGI_IP32
 /* Definitions related to IP32 PCI Bridge policy
  * XXX- should probaly be moved to a mace-specific header
  */
#define PCI_CONFIG_BITS			0xfe0085ff
#define	PCI_CONTROL_MRMRA_ENABLE	0x00000800
#define PCI_FIRST_IO_ADDR		0x1000
#define PCI_IO_MAP_INCR			0x1000
#endif /* CONFIG_SGI_IP32 */

/*
 * Class codes
 */
#define PCI_CFG_CLASS_PRE20	0x00
#define PCI_CFG_CLASS_STORAGE	0x01
#define PCI_CFG_CLASS_NETWORK	0x02
#define PCI_CFG_CLASS_DISPLAY	0x03
#define PCI_CFG_CLASS_MMEDIA	0x04
#define PCI_CFG_CLASS_MEMORY	0x05
#define PCI_CFG_CLASS_BRIDGE	0x06
#define PCI_CFG_CLASS_COMM	0x07
#define PCI_CFG_CLASS_BASE	0x08
#define PCI_CFG_CLASS_INPUT	0x09
#define PCI_CFG_CLASS_DOCK	0x0A
#define PCI_CFG_CLASS_PROC	0x0B
#define PCI_CFG_CLASS_SERIALBUS	0x0C
#define PCI_CFG_CLASS_OTHER	0xFF

/*
 * Important Subclasses
 */
#define PCI_CFG_SUBCLASS_BRIDGE_HOST	0x00
#define PCI_CFG_SUBCLASS_BRIDGE_ISA	0x01
#define PCI_CFG_SUBCLASS_BRIDGE_EISA	0x02
#define PCI_CFG_SUBCLASS_BRIDGE_MC	0x03
#define PCI_CFG_SUBCLASS_BRIDGE_PCI	0x04
#define PCI_CFG_SUBCLASS_BRIDGE_PCMCIA	0x05
#define PCI_CFG_SUBCLASS_BRIDGE_NUBUS	0x06
#define PCI_CFG_SUBCLASS_BRIDGE_CARDBUS	0x07
#define PCI_CFG_SUBCLASS_BRIDGE_OTHER	0x80

#ifndef __ASSEMBLY__

#ifdef LITTLE_ENDIAN

/*
 * PCI config space definition
 */
typedef volatile struct pci_cfg_s {
	uint16_t	vendor_id;
	uint16_t	dev_id;
	uint16_t	cmd;
	uint16_t	status;
	uchar_t		rev;
        uchar_t         prog_if;
	uchar_t		sub_class;
	uchar_t		class;
	uchar_t		line_size;
	uchar_t		lt;
	uchar_t		hdr_type;
	uchar_t		bist;
	uint32_t	bar[6];
	uint32_t	cardbus;
	uint16_t	subsys_vendor_id;
	uint16_t	subsys_dev_id;
	uint32_t	exp_rom;
	uint32_t	res[2];
	uchar_t		int_line;
	uchar_t		int_pin;
	uchar_t		min_gnt;
	uchar_t		max_lat;
} pci_cfg_t;

/*
 * PCI Type 1 config space definition for PCI to PCI Bridges (PPBs)
 */
typedef volatile struct pci_cfg1_s {
	uint16_t	vendor_id;
	uint16_t	dev_id;
	uint16_t	cmd;
	uint16_t	status;
	uchar_t		rev;
	uchar_t		prog_if;
	uchar_t		sub_class;
	uchar_t		class;
	uchar_t		line_size;
	uchar_t		lt;
	uchar_t		hdr_type;
	uchar_t		bist;
	uint32_t	bar[2];
	uchar_t		pri_bus_num;
	uchar_t		snd_bus_num;
	uchar_t		sub_bus_num;
	uchar_t		slt;
	uchar_t		io_base;
	uchar_t		io_limit;
	uint16_t	snd_status;
	uint16_t	mem_base;
	uint16_t	mem_limit;
	uint16_t	pmem_base;
	uint16_t	pmem_limit;
	uint32_t	pmem_base_upper;
	uint32_t	pmem_limit_upper;
	uint16_t	io_base_upper;
	uint16_t	io_limit_upper;
	uint32_t	res;
	uint32_t	exp_rom;
	uchar_t		int_line;
	uchar_t		int_pin;
	uint16_t	ppb_control;

} pci_cfg1_t;

/*
 * PCI-X Capability
 */
typedef volatile struct cap_pcix_cmd_reg_s {
	uint16_t	data_parity_enable:	1,
			enable_relaxed_order:	1,
			max_mem_read_cnt:	2,
			max_split:		3,
			reserved1:		9;
} cap_pcix_cmd_reg_t;

typedef volatile struct cap_pcix_stat_reg_s {
	uint32_t	func_num:		3,
			dev_num:		5,
			bus_num:		8,
			bit64_device:		1,
			mhz133_capable:		1,
			split_complt_discard:	1,
			unexpect_split_complt:	1,
			device_complex:		1,
			max_mem_read_cnt:	2,
			max_out_split:		3,
			max_cum_read:		3,
			split_complt_err:	1,
			reserved1:		2;
} cap_pcix_stat_reg_t;

typedef volatile struct cap_pcix_type0_s {
	uchar_t			pcix_cap_id;
	uchar_t			pcix_cap_nxt;
	cap_pcix_cmd_reg_t	pcix_type0_command;
	cap_pcix_stat_reg_t	pcix_type0_status;
} cap_pcix_type0_t;

#else

/*
 * PCI config space definition
 */
typedef volatile struct pci_cfg_s {
	uint16_t	dev_id;
	uint16_t	vendor_id;
	uint16_t	status;
	uint16_t	cmd;
	uchar_t		class;
	uchar_t		sub_class;
	uchar_t		prog_if;
	uchar_t		rev;
	uchar_t		bist;
	uchar_t		hdr_type;
	uchar_t		lt;
	uchar_t		line_size;
	uint32_t	bar[6];
	uint32_t	cardbus;
	uint16_t	subsys_dev_id;
	uint16_t	subsys_vendor_id;
	uint32_t	exp_rom;
	uint32_t	res[2];
	uchar_t		max_lat;
	uchar_t		min_gnt;
	uchar_t		int_pin;
	uchar_t		int_line;
} pci_cfg_t;

/*
 * PCI Type 1 config space definition for PCI to PCI Bridges (PPBs)
 */
typedef volatile struct pci_cfg1_s {
	uint16_t	dev_id;
	uint16_t	vendor_id;
	uint16_t	status;
	uint16_t	cmd;
	uchar_t		class;
	uchar_t		sub_class;
	uchar_t		prog_if;
	uchar_t		rev;
	uchar_t		bist;
	uchar_t		hdr_type;
	uchar_t		lt;
	uchar_t		line_size;
	uint32_t	bar[2];
	uchar_t		slt;
	uchar_t		sub_bus_num;
	uchar_t		snd_bus_num;
	uchar_t		pri_bus_num;
	uint16_t	snd_status;
	uchar_t		io_limit;
	uchar_t		io_base;
	uint16_t	mem_limit;
	uint16_t	mem_base;
	uint16_t	pmem_limit;
	uint16_t	pmem_base;
	uint32_t	pmem_limit_upper;
	uint32_t	pmem_base_upper;
	uint16_t	io_limit_upper;
	uint16_t	io_base_upper;
	uint32_t	res;
	uint32_t	exp_rom;
	uint16_t	ppb_control;
	uchar_t		int_pin;
	uchar_t		int_line;
} pci_cfg1_t;



/*
 * PCI-X Capability
 */
typedef volatile struct cap_pcix_cmd_reg_s {
	uint16_t	reserved1:              9,
			max_split:		3,
			max_mem_read_cnt:	2,
			enable_relaxed_order:	1,
			data_parity_enable:	1;
} cap_pcix_cmd_reg_t;

typedef volatile struct cap_pcix_stat_reg_s {
	uint32_t	reserved1:		2,
			split_complt_err:	1,
			max_cum_read:		3,
			max_out_split:		3,
			max_mem_read_cnt:	2,
			device_complex:		1,
			unexpect_split_complt:	1,
			split_complt_discard:	1,
			mhz133_capable:		1,
			bit64_device:		1,
			bus_num:		8,
			dev_num:		5,
			func_num:		3;
} cap_pcix_stat_reg_t;

typedef volatile struct cap_pcix_type0_s {
	cap_pcix_cmd_reg_t	pcix_type0_command;
	uchar_t			pcix_cap_nxt;
	uchar_t			pcix_cap_id;
	cap_pcix_stat_reg_t	pcix_type0_status;
} cap_pcix_type0_t;

#endif
#endif	/* __ASSEMBLY__ */
#endif /* _ASM_SN_PCI_PCI_DEFS_H */
