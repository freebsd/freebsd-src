/*
 * Copyright (c) 2003 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/NoticeExplan
 */

#ifndef SGIIOC4_H
#define SGIIOC4_H

#define IDE_ARCH_ACK_INTR	1
#include <linux/ide.h>

/* IOC4 Specific Definitions */
#define IOC4_CMD_OFFSET		0x100
#define IOC4_CTRL_OFFSET	0x120
#define IOC4_DMA_OFFSET		0x140
#define IOC4_INTR_OFFSET	0x0

#define IOC4_TIMING		0x00
#define IOC4_DMA_PTR_L		0x01
#define IOC4_DMA_PTR_H		0x02
#define IOC4_DMA_ADDR_L		0x03
#define IOC4_DMA_ADDR_H		0x04
#define IOC4_BC_DEV		0x05
#define IOC4_BC_MEM		0x06
#define	IOC4_DMA_CTRL		0x07
#define	IOC4_DMA_END_ADDR	0x08

/* Bits in the IOC4 Control/Status Register */
#define	IOC4_S_DMA_START	0x01
#define	IOC4_S_DMA_STOP		0x02
#define	IOC4_S_DMA_DIR		0x04
#define	IOC4_S_DMA_ACTIVE	0x08
#define	IOC4_S_DMA_ERROR	0x10
#define	IOC4_ATA_MEMERR		0x02

/* Read/Write Directions */
#define	IOC4_DMA_WRITE		0x04
#define	IOC4_DMA_READ		0x00

/* Interrupt Register Offsets */
#define IOC4_INTR_REG		0x03
#define	IOC4_INTR_SET		0x05
#define	IOC4_INTR_CLEAR		0x07

#define IOC4_IDE_CACHELINE_SIZE	128
#define IOC4_SUPPORTED_FIRMWARE_REV 46


/* Weeds out non-IDE interrupts to the IOC4 */
#define ide_ack_intr(hwif)      ((hwif)->hw.ack_intr ? (hwif)->hw.ack_intr(hwif) : 1)

#define SGIIOC4_MAX_DEVS	32

#if  defined(CONFIG_PROC_FS)
#include <linux/stat.h>
#include <linux/proc_fs.h>

static u8 sgiioc4_proc;

static struct pci_dev *sgiioc4_devs[SGIIOC4_MAX_DEVS];
static int sgiioc4_get_info(char *, char **, off_t, int);

static ide_pci_host_proc_t sgiioc4_procs[] __initdata = {
	{
		.name = "sgiioc4",
		.set = 1,
		.get_info = sgiioc4_get_info,
		.parent = NULL,
	}
};
#endif

typedef volatile struct {
	u32 timing_reg0;
	u32 timing_reg1;
	u32 low_mem_ptr;
	u32 high_mem_ptr;
	u32 low_mem_addr;
	u32 high_mem_addr;
	u32 dev_byte_count;
	u32 mem_byte_count;
	u32 status;
} ioc4_dma_regs_t;

/* Each Physical Region Descriptor Entry size is 16 bytes (2 * 64 bits) */
/* IOC4 has only 1 IDE channel */
#define IOC4_PRD_BYTES       16
#define IOC4_PRD_ENTRIES     (PAGE_SIZE /IOC4_PRD_BYTES)

typedef enum pciio_endian_e {
	PCIDMA_ENDIAN_BIG,
	PCIDMA_ENDIAN_LITTLE
} pciio_endian_t;

static void sgiioc4_init_hwif_ports(hw_regs_t * hw, ide_ioreg_t data_port, 
				    ide_ioreg_t ctrl_port, ide_ioreg_t irq_port);
static void sgiioc4_ide_setup_pci_device(struct pci_dev *dev, const char *name);
static void sgiioc4_resetproc(ide_drive_t * drive);
static void sgiioc4_maskproc(ide_drive_t * drive, int mask);
static void sgiioc4_configure_for_dma(int dma_direction, ide_drive_t * drive);
static void __init ide_init_sgiioc4(ide_hwif_t * hwif);
static void __init ide_dma_sgiioc4(ide_hwif_t * hwif, unsigned long dma_base);
static int sgiioc4_checkirq(ide_hwif_t * hwif);
static int sgiioc4_clearirq(ide_drive_t * drive);
static int sgiioc4_get_info(char *buffer, char **addr, off_t offset, int count);
static int sgiioc4_ide_dma_read(ide_drive_t * drive);
static int sgiioc4_ide_dma_write(ide_drive_t * drive);
static int sgiioc4_ide_dma_begin(ide_drive_t * drive);
static int sgiioc4_ide_dma_end(ide_drive_t * drive);
static int sgiioc4_ide_dma_check(ide_drive_t * drive);
static int sgiioc4_ide_dma_on(ide_drive_t * drive);
static int sgiioc4_ide_dma_off(ide_drive_t * drive);
static int sgiioc4_ide_dma_off_quietly(ide_drive_t * drive);
static int sgiioc4_ide_dma_test_irq(ide_drive_t * drive);
static int sgiioc4_ide_dma_host_on(ide_drive_t * drive);
static int sgiioc4_ide_dma_host_off(ide_drive_t * drive);
static int sgiioc4_ide_dma_count(ide_drive_t * drive);
static int sgiioc4_ide_dma_verbose(ide_drive_t * drive);
static int sgiioc4_ide_dma_lostirq(ide_drive_t * drive);
static int sgiioc4_ide_dma_timeout(ide_drive_t * drive);
static int sgiioc4_ide_build_sglist(ide_hwif_t * hwif, struct request *rq,
				    int ddir);
static int sgiioc4_ide_raw_build_sglist(ide_hwif_t * hwif, struct request *rq);

static u8 sgiioc4_INB(unsigned long port);
static inline void xide_delay(long ticks);
extern int (*sgiioc4_display_info) (char *, char **, off_t, int);	/* ide-proc.c */
static unsigned int sgiioc4_build_dma_table(ide_drive_t * drive, struct request *rq,
					    int ddir);
static unsigned int __init pci_init_sgiioc4(struct pci_dev *dev, const char *name);

static ide_pci_device_t sgiioc4_chipsets[] __devinitdata = {
	{		
		/* Channel 0 */
		.vendor = PCI_VENDOR_ID_SGI,
		.device = PCI_DEVICE_ID_SGI_IOC4,
		.name = "SGIIOC4",
		.init_chipset = pci_init_sgiioc4,
		.init_iops = NULL,
		.init_hwif = ide_init_sgiioc4,
		.init_dma = ide_dma_sgiioc4,
		.channels = 1,
		.autodma = AUTODMA,
		.enablebits = { { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00 } },
		.bootable = ON_BOARD,
		.extra = 0,
	}
};

#endif
