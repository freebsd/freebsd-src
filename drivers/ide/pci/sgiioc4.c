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

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/hdreg.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/blkdev.h>
#include <asm/io.h>
#include "sgiioc4.h"

extern int dma_timer_expiry(ide_drive_t * drive);

#ifdef CONFIG_PROC_FS
static u8 sgiioc4_proc;
#endif /* CONFIG_PROC_FS */

static int n_sgiioc4_devs ;

static inline void
xide_delay(long ticks)
{
	if (!ticks)
		return;

	current->state = TASK_UNINTERRUPTIBLE;
	schedule_timeout(ticks);
}

static void __init
sgiioc4_ide_setup_pci_device(struct pci_dev *dev, const char *name)
{
	unsigned long base = 0, ctl = 0, dma_base = 0, irqport = 0;
	ide_hwif_t *hwif = NULL;
	int h = 0;

	/*  Get the CmdBlk and CtrlBlk Base Registers */
	base = pci_resource_start(dev, 0) + IOC4_CMD_OFFSET;
	ctl = pci_resource_start(dev, 0) + IOC4_CTRL_OFFSET;
	irqport = pci_resource_start(dev, 0) + IOC4_INTR_OFFSET;
	dma_base = pci_resource_start(dev, 0) + IOC4_DMA_OFFSET;

	for (h = 0; h < MAX_HWIFS; ++h) {
		hwif = &ide_hwifs[h];
		/* Find an empty HWIF */
		if (hwif->chipset == ide_unknown)
			break;
	}

	if (hwif->io_ports[IDE_DATA_OFFSET] != base) {
		/* Initialize the IO registers */
		sgiioc4_init_hwif_ports(&hwif->hw, base, ctl, irqport);
		memcpy(hwif->io_ports, hwif->hw.io_ports, sizeof (hwif->io_ports));
		hwif->noprobe = !hwif->io_ports[IDE_DATA_OFFSET];
	}

	hwif->chipset = ide_pci;
	hwif->pci_dev = dev;
	hwif->channel = 0;	/* Single Channel chip */
	hwif->hw.ack_intr = &sgiioc4_checkirq;	/* MultiFunction Chip */

	/* Initializing chipset IRQ Registers */
	hwif->OUTL(0x03, irqport + IOC4_INTR_SET * 4);

	(void) ide_init_sgiioc4(hwif);

	if (dma_base)
		ide_dma_sgiioc4(hwif, dma_base);
	else
		printk(KERN_INFO "%s: %s Bus-Master DMA disabled \n", hwif->name, name);
}

/* XXX Hack to ensure we can build this for generic kernels without
 * having all the SN2 code sync'd and merged.  For now this is
 * acceptable but this should be resolved ASAP. PV#: 896401 */

pciio_endian_t __attribute__((weak)) snia_pciio_endian_set(struct pci_dev *pci_dev, pciio_endian_t device_end, pciio_endian_t desired_end);

static unsigned int __init
pci_init_sgiioc4(struct pci_dev *dev, const char *name)
{

	if (pci_enable_device(dev)) {
		printk(KERN_INFO "Failed to enable device %s at slot %s \n",name,dev->slot_name);
		return 1;
	}
	pci_set_master(dev);

	/* Enable Byte Swapping in the PIC... */
	if (snia_pciio_endian_set) {
		/* ... if the symbol exists (hack to get this to build
		 * for SuSE before we merge the SN2 code */
		snia_pciio_endian_set(dev, PCIDMA_ENDIAN_LITTLE, PCIDMA_ENDIAN_BIG);
	} else {
		printk(KERN_INFO "Failed to set endianness for device %s at slot %s \n", name, dev->slot_name);
		return 1;
	}

#ifdef CONFIG_PROC_FS
	sgiioc4_devs[n_sgiioc4_devs++] = dev;
	if (!sgiioc4_proc) {
		sgiioc4_proc = 1;
		ide_pci_register_host_proc(&sgiioc4_procs[0]);
	}
#endif
	sgiioc4_ide_setup_pci_device(dev, name);
	return 0;
}

static void
sgiioc4_init_hwif_ports(hw_regs_t * hw, ide_ioreg_t data_port,
			ide_ioreg_t ctrl_port, ide_ioreg_t irq_port)
{
	ide_ioreg_t reg = data_port;
	int i;

	for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++)
		hw->io_ports[i] = reg + i * 4;	/* Registers are word (32 bit) aligned */

	if (ctrl_port)
		hw->io_ports[IDE_CONTROL_OFFSET] = ctrl_port;

	if (irq_port)
		hw->io_ports[IDE_IRQ_OFFSET] = irq_port;
}

static void
sgiioc4_resetproc(ide_drive_t * drive)
{
	sgiioc4_ide_dma_end(drive);
	sgiioc4_clearirq(drive);
}

static void
sgiioc4_maskproc(ide_drive_t * drive, int mask)
{
	ide_hwif_t *hwif = HWIF(drive);
	hwif->OUTB(mask ? (drive->ctl | 2) : (drive->ctl & ~2), IDE_CONTROL_REG);
}

static void __init
ide_init_sgiioc4(ide_hwif_t * hwif)
{
	hwif->autodma = 1;
	hwif->index = 0;	/* Channel 0 */
	hwif->channel = 0;
	hwif->atapi_dma = 1;
	hwif->ultra_mask = 0x0;	/* Disable Ultra DMA */
	hwif->mwdma_mask = 0x2;	/* Multimode-2 DMA  */
	hwif->swdma_mask = 0x2;
	hwif->identify = NULL;
	hwif->tuneproc = NULL;	/* Sets timing for PIO mode */
	hwif->speedproc = NULL;	/* Sets timing for DMA &/or PIO modes */
	hwif->selectproc = NULL;	/* Use the default selection routine to select drive */
	hwif->reset_poll = NULL;	/* No HBA specific reset_poll needed */
	hwif->pre_reset = NULL;	/* No HBA specific pre_set needed */
	hwif->resetproc = &sgiioc4_resetproc;	/* Reset the IOC4 DMA engine, clear interrupts etc */
	hwif->intrproc = NULL;	/* Enable or Disable interrupt from drive */
	hwif->maskproc = &sgiioc4_maskproc;	/* Mask on/off NIEN register */
	hwif->quirkproc = NULL;
	hwif->busproc = NULL;

	hwif->ide_dma_read = &sgiioc4_ide_dma_read;
	hwif->ide_dma_write = &sgiioc4_ide_dma_write;
	hwif->ide_dma_begin = &sgiioc4_ide_dma_begin;
	hwif->ide_dma_end = &sgiioc4_ide_dma_end;
	hwif->ide_dma_check = &sgiioc4_ide_dma_check;
	hwif->ide_dma_on = &sgiioc4_ide_dma_on;
	hwif->ide_dma_off = &sgiioc4_ide_dma_off;
	hwif->ide_dma_off_quietly = &sgiioc4_ide_dma_off_quietly;
	hwif->ide_dma_test_irq = &sgiioc4_ide_dma_test_irq;
	hwif->ide_dma_host_on = &sgiioc4_ide_dma_host_on;
	hwif->ide_dma_host_off = &sgiioc4_ide_dma_host_off;
	hwif->ide_dma_bad_drive = &__ide_dma_bad_drive;
	hwif->ide_dma_good_drive = &__ide_dma_good_drive;
	hwif->ide_dma_count = &sgiioc4_ide_dma_count;
	hwif->ide_dma_verbose = &sgiioc4_ide_dma_verbose;
	hwif->ide_dma_retune = &__ide_dma_retune;
	hwif->ide_dma_lostirq = &sgiioc4_ide_dma_lostirq;
	hwif->ide_dma_timeout = &sgiioc4_ide_dma_timeout;
	hwif->INB = &sgiioc4_INB;
}

static int
sgiioc4_ide_dma_read(ide_drive_t * drive)
{
	struct request *rq = HWGROUP(drive)->rq;
	unsigned int count = 0;

	if (!(count = sgiioc4_build_dma_table(drive, rq, PCI_DMA_FROMDEVICE))) {
		/* try PIO instead of DMA */
		return 1;
	}
	/* Writes FROM the IOC4 TO Main Memory */
	sgiioc4_configure_for_dma(IOC4_DMA_WRITE, drive);

	return 0;
}

static int
sgiioc4_ide_dma_write(ide_drive_t * drive)
{
	struct request *rq = HWGROUP(drive)->rq;
	unsigned int count = 0;

	if (!(count = sgiioc4_build_dma_table(drive, rq, PCI_DMA_TODEVICE))) {
		/* try PIO instead of DMA */
		return 1;
	}

	sgiioc4_configure_for_dma(IOC4_DMA_READ, drive);
	/* Writes TO the IOC4 FROM Main Memory */

	return 0;
}

static int
sgiioc4_ide_dma_begin(ide_drive_t * drive)
{
	ide_hwif_t *hwif = HWIF(drive);
	unsigned int reg = hwif->INL(hwif->dma_base + IOC4_DMA_CTRL * 4);
	unsigned int temp_reg = reg | IOC4_S_DMA_START;

	hwif->OUTL(temp_reg, hwif->dma_base + IOC4_DMA_CTRL * 4);

	return 0;
}

/* Stops the IOC4 DMA Engine */
static int
sgiioc4_ide_dma_end(ide_drive_t * drive)
{
	u32 ioc4_dma, bc_dev, bc_mem, num, valid = 0, cnt = 0;
	ide_hwif_t *hwif = HWIF(drive);
	uint64_t dma_base = hwif->dma_base;
	int dma_stat = 0, count;
	unsigned long *ending_dma = (unsigned long *) hwif->dma_base2;

	hwif->OUTL(IOC4_S_DMA_STOP, dma_base + IOC4_DMA_CTRL * 4);

	count = 0;
	do {
		xide_delay(count);
		ioc4_dma = hwif->INL(dma_base + IOC4_DMA_CTRL * 4);
		count += 10;
	} while ((ioc4_dma & IOC4_S_DMA_STOP) && (count < 100));

	if (ioc4_dma & IOC4_S_DMA_STOP) {
		printk(KERN_ERR "sgiioc4_stopdma(%s): IOC4 DMA STOP bit is still 1 : ioc4_dma_reg 0x%x\n", drive->name, ioc4_dma);
		dma_stat = 1;
	}

	if (ending_dma) {
		do {
			for (num = 0; num < 16; num++) {
				if (ending_dma[num] & (~0ul)) {
					valid = 1;
					break;
				}
			}
			xide_delay(cnt);
		} while ((cnt++ < 100) && (!valid));
	}

	if (!valid)
		printk(KERN_INFO "sgiioc4_ide_dma_end(%s) : Stale DMA Data in Memory\n", drive->name);

	bc_dev = hwif->INL(dma_base + IOC4_BC_DEV * 4);
	bc_mem = hwif->INL(dma_base + IOC4_BC_MEM * 4);

	if ((bc_dev & 0x01FF) || (bc_mem & 0x1FF)) {
		if (bc_dev > bc_mem + 8) {
			printk(KERN_ERR "sgiioc4_ide_dma_end(%s) : WARNING!!! byte_count_at_dev %d != byte_count_at_mem %d\n",
			       drive->name, bc_dev, bc_mem);
		}
	}

	drive->waiting_for_dma = 0;
	ide_destroy_dmatable(drive);

	return dma_stat;
}

static int
sgiioc4_ide_dma_check(ide_drive_t * drive)
{
	if (ide_config_drive_speed(drive,XFER_MW_DMA_2)!=0) {
		printk(KERN_INFO "Couldnot set %s in Multimode-2 DMA mode | Drive %s using PIO instead\n",
				drive->name, drive->name);
		drive->using_dma = 0;
	} else
		drive->using_dma = 1;

	return 0;
}

static int
sgiioc4_ide_dma_on(ide_drive_t * drive)
{
	drive->using_dma = 1;

	return HWIF(drive)->ide_dma_host_on(drive);
}

static int
sgiioc4_ide_dma_off(ide_drive_t * drive)
{
	printk(KERN_INFO "%s: DMA disabled\n", drive->name);

	return HWIF(drive)->ide_dma_off_quietly(drive);
}

static int
sgiioc4_ide_dma_off_quietly(ide_drive_t * drive)
{
	drive->using_dma = 0;

	return HWIF(drive)->ide_dma_host_off(drive);
}

/* returns 1 if dma irq issued, 0 otherwise */
static int
sgiioc4_ide_dma_test_irq(ide_drive_t * drive)
{
	return sgiioc4_checkirq(HWIF(drive));
}

static int
sgiioc4_ide_dma_host_on(ide_drive_t * drive)
{
	if (drive->using_dma)
		return 0;

	return 1;
}

static int
sgiioc4_ide_dma_host_off(ide_drive_t * drive)
{
	sgiioc4_clearirq(drive);

	return 0;
}

static int
sgiioc4_ide_dma_count(ide_drive_t * drive)
{
	return HWIF(drive)->ide_dma_begin(drive);
}

static int
sgiioc4_ide_dma_verbose(ide_drive_t * drive)
{
	if (drive->using_dma == 1)
		printk(", UDMA(16)");
	else
		printk(", PIO");

	return 1;
}

static int
sgiioc4_ide_dma_lostirq(ide_drive_t * drive)
{
	HWIF(drive)->resetproc(drive);

	return __ide_dma_lostirq(drive);
}

static int
sgiioc4_ide_dma_timeout(ide_drive_t * drive)
{
	printk(KERN_ERR "%s: timeout waiting for DMA\n", drive->name);
	if (HWIF(drive)->ide_dma_test_irq(drive))
		return 0;

	return HWIF(drive)->ide_dma_end(drive);
}

static u8
sgiioc4_INB(unsigned long port)
{
	u8 reg = (u8) inb(port);

	if ((port & 0xFFF) == 0x11C) {	/* Status register of IOC4 */
		if (reg & 0x51) {	/* Not busy...check for interrupt */
			unsigned long other_ir = port - 0x110;
			unsigned int intr_reg = (u32) inl(other_ir);

			if (intr_reg & 0x03) {
				/* Clear the Interrupt, Error bits on the IOC4 */
				outl(0x03, other_ir);
				intr_reg = (u32) inl(other_ir);
			}
		}
	}

	return reg;
}

/* Creates a dma map for the scatter-gather list entries */
static void __init
ide_dma_sgiioc4(ide_hwif_t * hwif, unsigned long dma_base)
{
	int num_ports = sizeof (ioc4_dma_regs_t);

	printk(KERN_INFO "%s: BM-DMA at 0x%04lx-0x%04lx\n", hwif->name, dma_base, dma_base + num_ports - 1);

	if (!request_region(dma_base, num_ports, hwif->name)) {
		printk(KERN_ERR "ide_dma_sgiioc4(%s) -- Error, Port Addresses 0x%p to 0x%p ALREADY in use\n",
		       hwif->name, (void *)dma_base, (void *)dma_base + num_ports - 1);
		return;
	}

	hwif->dma_base = dma_base;
	hwif->dmatable_cpu = pci_alloc_consistent(hwif->pci_dev,
						  IOC4_PRD_ENTRIES * IOC4_PRD_BYTES,	/* 1 Page */
						  &hwif->dmatable_dma);

	if (!hwif->dmatable_cpu)
		goto dma_alloc_failure;

	hwif->sg_table = kmalloc(sizeof (struct scatterlist) * IOC4_PRD_ENTRIES, GFP_KERNEL);

	if (!hwif->sg_table) {
		pci_free_consistent(hwif->pci_dev, IOC4_PRD_ENTRIES * IOC4_PRD_BYTES, hwif->dmatable_cpu, hwif->dmatable_dma);
		goto dma_alloc_failure;
	}

	hwif->dma_base2 = (unsigned long) pci_alloc_consistent(hwif->pci_dev, IOC4_IDE_CACHELINE_SIZE,
							       (dma_addr_t*)&(hwif->dma_status));

	if (!hwif->dma_base2) {
		pci_free_consistent(hwif->pci_dev, IOC4_PRD_ENTRIES * IOC4_PRD_BYTES, hwif->dmatable_cpu, hwif->dmatable_dma);
		kfree(hwif->sg_table);
		goto dma_alloc_failure;
	}

	return;

 dma_alloc_failure:
	printk(KERN_INFO "ide_dma_sgiioc4() -- Error! Unable to allocate DMA Maps for drive %s\n", hwif->name);
	printk(KERN_INFO "Changing from DMA to PIO mode for Drive %s \n", hwif->name);

	/* Disable DMA because we couldnot allocate any DMA maps */
	hwif->autodma = 0;
	hwif->atapi_dma = 0;
}

/* Initializes the IOC4 DMA Engine */
static void
sgiioc4_configure_for_dma(int dma_direction, ide_drive_t * drive)
{
	u32 ioc4_dma;
	int count;
	ide_hwif_t *hwif = HWIF(drive);
	uint64_t dma_base = hwif->dma_base;
	uint32_t dma_addr, ending_dma_addr;

	ioc4_dma = hwif->INL(dma_base + IOC4_DMA_CTRL * 4);

	if (ioc4_dma & IOC4_S_DMA_ACTIVE) {
		printk(KERN_WARNING "sgiioc4_configure_for_dma(%s):Warning!! IOC4 DMA from previous transfer was still active\n",
			drive->name);
		hwif->OUTL(IOC4_S_DMA_STOP, dma_base + IOC4_DMA_CTRL * 4);
		count = 0;
		do {
			xide_delay(count);
			ioc4_dma = hwif->INL(dma_base + IOC4_DMA_CTRL * 4);
			count += 10;
		} while ((ioc4_dma & IOC4_S_DMA_STOP) && (count < 100));

		if (ioc4_dma & IOC4_S_DMA_STOP)
			printk(KERN_ERR "sgiioc4_configure_for__dma(%s) : IOC4 Dma STOP bit is still 1\n", drive->name);
	}

	ioc4_dma = hwif->INL(dma_base + IOC4_DMA_CTRL * 4);
	if (ioc4_dma & IOC4_S_DMA_ERROR) {
		printk(KERN_WARNING "sgiioc4_configure_for__dma(%s) : Warning!! - DMA Error during Previous transfer | status 0x%x \n",
		       drive->name, ioc4_dma);
		hwif->OUTL(IOC4_S_DMA_STOP, dma_base + IOC4_DMA_CTRL * 4);
		count = 0;
		do {
			ioc4_dma = hwif->INL(dma_base + IOC4_DMA_CTRL * 4);
			xide_delay(count);
			count += 10;
		} while ((ioc4_dma & IOC4_S_DMA_STOP) && (count < 100));

		if (ioc4_dma & IOC4_S_DMA_STOP)
			printk(KERN_ERR "sgiioc4_configure_for__dma(%s) : IOC4 DMA STOP bit is still 1\n", drive->name);
	}

	/* Address of the Scatter Gather List */
	dma_addr = cpu_to_le32(hwif->dmatable_dma);
	hwif->OUTL(dma_addr, dma_base + IOC4_DMA_PTR_L * 4);

	/* Address of the Ending DMA */
	memset((unsigned int *) hwif->dma_base2, 0,IOC4_IDE_CACHELINE_SIZE);
	ending_dma_addr = cpu_to_le32(hwif->dma_status);
	hwif->OUTL(ending_dma_addr,dma_base + IOC4_DMA_END_ADDR * 4);

	hwif->OUTL(dma_direction, dma_base + IOC4_DMA_CTRL * 4);
	drive->waiting_for_dma = 1;
}

/* IOC4 Scatter Gather list Format 						*/
/* 128 Bit entries to support 64 bit addresses in the future			*/
/* The Scatter Gather list Entry should be in the BIG-ENDIAN Format		*/
/* ---------------------------------------------------------------------------	*/
/* | Upper 32 bits - Zero 		| 	Lower 32 bits- address	     |	*/
/* ---------------------------------------------------------------------------	*/
/* | Upper 32 bits - Zero		|EOL|	 16 Bit Data Length	     |	*/
/* ---------------------------------------------------------------------------	*/

/* Creates the scatter gather list, DMA Table */
static unsigned int
sgiioc4_build_dma_table(ide_drive_t * drive, struct request *rq, int ddir)
{
	ide_hwif_t *hwif = HWIF(drive);
	unsigned int *table = hwif->dmatable_cpu;
	unsigned int count = 0, i = 1;
	struct scatterlist *sg;

	if (rq->cmd == IDE_DRIVE_TASKFILE)
		hwif->sg_nents = i = sgiioc4_ide_raw_build_sglist(hwif, rq);
	else
		hwif->sg_nents = i = sgiioc4_ide_build_sglist(hwif, rq, ddir);

	if (!i)
		return 0;	/* sglist of length Zero */

	sg = hwif->sg_table;
	while (i && sg_dma_len(sg)) {
		dma_addr_t cur_addr;
		int cur_len;
		cur_addr = sg_dma_address(sg);
		cur_len = sg_dma_len(sg);

		while (cur_len) {
			if (count++ >= IOC4_PRD_ENTRIES) {
				printk(KERN_WARNING "%s: DMA table too small\n", drive->name);
				goto use_pio_instead;
			} else {
				uint32_t xcount, bcount = 0x10000 - (cur_addr & 0xffff);

				if (bcount > cur_len)
					bcount = cur_len;

				/* put the addr, length in the IOC4 dma-table format */
				*table = 0x0;
				table++;
				*table = cpu_to_be32(cur_addr);
				table++;
				*table = 0x0;
				table++;

				xcount = bcount & 0xffff;
				*table = cpu_to_be32(xcount);
				table++;

				cur_addr += bcount;
				cur_len -= bcount;
			}
		}

		sg++;
		i--;
	}

	if (count) {
		table--;
		*table |= cpu_to_be32(0x80000000);
		return count;
	}

      use_pio_instead:
	pci_unmap_sg(hwif->pci_dev, hwif->sg_table, hwif->sg_nents, hwif->sg_dma_direction);
	hwif->sg_dma_active = 0;

	return 0;		/* revert to PIO for this request */
}

static int
sgiioc4_checkirq(ide_hwif_t * hwif)
{
	uint8_t intr_reg = hwif->INL(hwif->io_ports[IDE_IRQ_OFFSET] + IOC4_INTR_REG * 4);

	if (intr_reg & 0x03)
		return 1;

	return 0;
}

static int
sgiioc4_clearirq(ide_drive_t * drive)
{
	u32 intr_reg;
	ide_hwif_t *hwif = HWIF(drive);
	ide_ioreg_t other_ir = hwif->io_ports[IDE_IRQ_OFFSET] + (IOC4_INTR_REG << 2);

	/* Code to check for PCI error conditions */
	intr_reg = hwif->INL(other_ir);
	if (intr_reg & 0x03) {
		/* Valid IOC4-IDE interrupt */
		u8 stat = hwif->INB(IDE_STATUS_REG);
		int count = 0;
		do {
			xide_delay(count);
			stat = hwif->INB(IDE_STATUS_REG);	/* Removes Interrupt from IDE Device */
		} while ((stat & 0x80) && (count++ < 1024));

		if (intr_reg & 0x02) {
			/* Error when transferring DMA data on PCI bus */
			uint32_t pci_err_addr_low, pci_err_addr_high, pci_stat_cmd_reg;

			pci_err_addr_low = hwif->INL(hwif->io_ports[IDE_IRQ_OFFSET]);
			pci_err_addr_high = hwif->INL(hwif->io_ports[IDE_IRQ_OFFSET] + 4);
			pci_read_config_dword(hwif->pci_dev, PCI_COMMAND, &pci_stat_cmd_reg);
			printk(KERN_ERR "sgiioc4_clearirq(%s) : PCI Bus Error when doing DMA : status-cmd reg is 0x%x \n", drive->name, pci_stat_cmd_reg);
			printk(KERN_ERR "sgiioc4_clearirq(%s) : PCI Error Address is 0x%x%x \n", drive->name, pci_err_addr_high, pci_err_addr_low);
			/* Clear the PCI Error indicator */
			pci_write_config_dword(hwif->pci_dev, PCI_COMMAND, 0x00000146);
		}

		hwif->OUTL(0x03, other_ir);	/* Clear the Interrupt, Error bits on the IOC4 */

		intr_reg = hwif->INL(other_ir);
	}

	return intr_reg;
}

/* XXX: duplicated code. See PV#: 896400 */

/**
 * 	"Copied from drivers/ide/ide-dma.c"
 *	sgiioc4_ide_build_sglist - map IDE scatter gather for DMA I/O
 *	@hwif: the interface to build the DMA table for
 *	@rq: the request holding the sg list
 *	@ddir: data direction
 *
 *	Perform the PCI mapping magic neccessary to access the source
 *	or target buffers of a request via PCI DMA. The lower layers
 *	of the kernel provide the neccessary cache management so that
 *	we can operate in a portable fashion.
 *
 *	This code is identical to ide_build_sglist in ide-dma.c
 *	however that it not exported and even if it were would create
 *	dependancy problems for modular drivers.
 */
static int
sgiioc4_ide_build_sglist(ide_hwif_t * hwif, struct request *rq, int ddir)
{
	struct buffer_head *bh;
	struct scatterlist *sg = hwif->sg_table;
	unsigned long lastdataend = ~0UL;
	int nents = 0;

	if (hwif->sg_dma_active)
		BUG();

	bh = rq->bh;
	do {
		int contig = 0;

		if (bh->b_page) {
			if (bh_phys(bh) == lastdataend)
				contig = 1;
		} else {
			if ((unsigned long) bh->b_data == lastdataend)
				contig = 1;
		}

		if (contig) {
			sg[nents - 1].length += bh->b_size;
			lastdataend += bh->b_size;
			continue;
		}

		if (nents >= PRD_ENTRIES)
			return 0;

		memset(&sg[nents], 0, sizeof (*sg));

		if (bh->b_page) {
			sg[nents].page = bh->b_page;
			sg[nents].offset = bh_offset(bh);
			lastdataend = bh_phys(bh) + bh->b_size;
		} else {
			if ((unsigned long) bh->b_data < PAGE_SIZE)
				BUG();

			sg[nents].address = bh->b_data;
			lastdataend = (unsigned long) bh->b_data + bh->b_size;
		}

		sg[nents].length = bh->b_size;
		nents++;
	} while ((bh = bh->b_reqnext) != NULL);

	if (nents == 0)
		BUG();

	hwif->sg_dma_direction = ddir;
	return pci_map_sg(hwif->pci_dev, sg, nents, ddir);
}

/* XXX: duplicated code. See PV#: 896400 */

/**
 * 	Copied from drivers/ide/ide-dma.c
 *	sgiioc4_ide_raw_build_sglist	-	map IDE scatter gather for DMA
 *	@hwif: the interface to build the DMA table for
 *	@rq: the request holding the sg list
 *
 *	Perform the PCI mapping magic neccessary to access the source or
 *	target buffers of a taskfile request via PCI DMA. The lower layers
 *	of the  kernel provide the neccessary cache management so that we can
 *	operate in a portable fashion
 *
 *	This code is identical to ide_raw_build_sglist in ide-dma.c
 *	however that it not exported and even if it were would create
 *	dependancy problems for modular drivers.
 */
static int
sgiioc4_ide_raw_build_sglist(ide_hwif_t * hwif, struct request *rq)
{
	struct scatterlist *sg = hwif->sg_table;
	int nents = 0;
	ide_task_t *args = rq->special;
	u8 *virt_addr = rq->buffer;
	int sector_count = rq->nr_sectors;

	if (args->command_type == IDE_DRIVE_TASK_RAW_WRITE)
		hwif->sg_dma_direction = PCI_DMA_TODEVICE;
	else
		hwif->sg_dma_direction = PCI_DMA_FROMDEVICE;
#if 1
	if (sector_count > 128) {
		memset(&sg[nents], 0, sizeof (*sg));
		sg[nents].address = virt_addr;
		sg[nents].length = 128 * SECTOR_SIZE;
		nents++;
		virt_addr = virt_addr + (128 * SECTOR_SIZE);
		sector_count -= 128;
	}
	memset(&sg[nents], 0, sizeof (*sg));
	sg[nents].address = virt_addr;
	sg[nents].length = sector_count * SECTOR_SIZE;
	nents++;
#else
	while (sector_count > 128) {
		memset(&sg[nents], 0, sizeof (*sg));
		sg[nents].address = virt_addr;
		sg[nents].length = 128 * SECTOR_SIZE;
		nents++;
		virt_addr = virt_addr + (128 * SECTOR_SIZE);
		sector_count -= 128;
	};
	memset(&sg[nents], 0, sizeof (*sg));
	sg[nents].address = virt_addr;
	sg[nents].length = sector_count * SECTOR_SIZE;
	nents++;
#endif
	return pci_map_sg(hwif->pci_dev, sg, nents, hwif->sg_dma_direction);
}

#ifdef CONFIG_PROC_FS

static int
sgiioc4_get_info(char *buffer, char **addr, off_t offset, int count)
{
	char *p = buffer;
	unsigned int class_rev;
	int i = 0;

	while (i < n_sgiioc4_devs) {
		pci_read_config_dword(sgiioc4_devs[i], PCI_CLASS_REVISION,
				      &class_rev);
		class_rev &= 0xff;

		if (sgiioc4_devs[i]->device == PCI_DEVICE_ID_SGI_IOC4) {
			p += sprintf(p, "\n	SGI IOC4 Chipset rev %d. ", class_rev);
			p += sprintf(p, "\n	Chipset has 1 IDE channel and supports 2 devices on that channel.");
			p += sprintf(p, "\n	Chipset supports DMA in MultiMode-2 data transfer protocol.\n");
			/* Do we need more info. here? */
		}
		i++;
	}

	return p - buffer;
}

#endif /* CONFIG_PROC_FS */

static int __devinit
sgiioc4_init_one(struct pci_dev *dev, const struct pci_device_id *id)
{
	unsigned int class_rev;
	ide_pci_device_t *d = &sgiioc4_chipsets[id->driver_data];
	if (dev->device != d->device) {
		printk(KERN_ERR "Error in sgiioc4_init_one(dev 0x%p | id 0x%p )\n", (void *) dev, (void *) id);
		BUG();
	}

	pci_read_config_dword(dev, PCI_CLASS_REVISION, &class_rev);
	class_rev &= 0xff;

	if (class_rev < IOC4_SUPPORTED_FIRMWARE_REV) {
		printk(KERN_INFO "Disabling the IOC4 IDE Part due to unsupported Firmware Rev (%d). \n",class_rev);
		printk(KERN_INFO "Please upgrade to Firmware Rev 46 or higher \n");
		return 0;
	}

	printk(KERN_INFO "%s: IDE controller at PCI slot %s\n", d->name, dev->slot_name);

	if (pci_init_sgiioc4(dev, d->name))
		return 0;

	MOD_INC_USE_COUNT;

	return 0;
}

static struct pci_device_id sgiioc4_pci_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_SGI, PCI_DEVICE_ID_SGI_IOC4, PCI_ANY_ID, PCI_ANY_ID, 0x0b4000, 0xFFFFFF, 0 },
	{ 0 }
};

static struct pci_driver driver = {
	.name = "SGI-IOC4 IDE",
	.id_table = sgiioc4_pci_tbl,
	.probe = sgiioc4_init_one,
};

static int
sgiioc4_ide_init(void)
{
	return ide_pci_register_driver(&driver);
}

static void
sgiioc4_ide_exit(void)
{
	ide_pci_unregister_driver(&driver);
}

module_init(sgiioc4_ide_init);
module_exit(sgiioc4_ide_exit);

MODULE_AUTHOR("Aniket Malatpure - Silicon Graphics Inc. (SGI)");
MODULE_DESCRIPTION("PCI driver module for SGI IOC4 Base-IO Card");
MODULE_LICENSE("GPL");

EXPORT_NO_SYMBOLS;
