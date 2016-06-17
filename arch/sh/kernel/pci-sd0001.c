/*
 *   $Id: pci-sd0001.c,v 1.1.2.1 2003/06/24 08:40:50 dwmw2 Exp $
 *
 *   linux/arch/sh/kernel/pci-sd0001.c
 *
 *   Support Hitachi Semcon SD0001 SH3 PCI Host Bridge .
 *  
 *
 *   Copyright (C) 2000  Hitachi ULSI Systems Co., Ltd.
 *   All Rights Reserved.
 *
 *   Copyright (C) 2001-2003 Red Hat, Inc.
 *
 *   Authors:	Masayuki Okada (macha@adc.hitachi-ul.co.jp)
 *              David Woodhouse (dwmw2@redhat.com)
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/init.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>

#include <asm/pci.h>
#include <asm/io.h>
#include <asm/irq.h>

#include "pci-sd0001.h"

spinlock_t sd0001_indirect_lock = SPIN_LOCK_UNLOCKED;

int remap_area_pages(unsigned long address, unsigned long phys_addr,
                     unsigned long size, unsigned long flags);


#undef DEBUG

#ifdef DEBUG
#define DBG(x...) printk(x)
#else
#define DBG(x...)
#endif
#define SD0001_INDIR_TIME	1000000		/* 間接アクセス完了待ち最大回数 */

static char *err_int_msg [] = {
	"Detect Master Abort",
	"Assert Master Abort",
	"Detect Target Abort",
	"Assert Target Abort",
	"Assert PERR",
	"Detect PERR",
	"Detect SERR",
	"Asster SERR",
	"Bus Timeout",
	"Bus Retry Over",
};

/*
 * PCIバスのバスリセット実行
 */
static void sd0001_bus_reset(void)
{
	sd0001_writel(SD0001_RST_BUSRST, RESET);

	udelay(64);

	sd0001_writel(0, RESET);
}

/*
 * SD0001ソフトリセット
 */
static void sd0001_chip_reset(void)
{
	sd0001_writel(SD0001_RST_SWRST, RESET);
}


#define ROUND_UP(x, a)		(((x) + (a) - 1) & ~((a) - 1))

static void
sd0001_int_pcierr (int irq, void *dummy, struct pt_regs *regs)
{
	static char  errStrings[30*12];
	u32 int_status;
	u32 mask;
	u32 indrct_flg;
	int reset_fatal;
	int   i;
	int   to_cnt = 0;
	unsigned long flags;

	spin_lock_irqsave(&sd0001_indirect_lock, flags);

	if ((int_status = (sd0001_readl(INT_STS1) & SD0001_INT_BUSERR))) {
		DBG("pciIntErrorHandle Called: status 0x%08x\n", int_status, 0, 0, 0, 0, 0);

		for (mask = 1 << 20, i = 0, errStrings[0] = '\0'; i < 11; i++, mask <<= 1) {
			if (int_status & mask) {
/* 				err_int_cnt[i] ++; */
				strcat(errStrings, err_int_msg[i]);
				strcat(errStrings, ", ");
			}
		}
		i = strlen(errStrings);
		errStrings[i-2]= '\0';

		printk(KERN_ERR "PCI Bus 0x%08x(%s) Error\n", int_status, errStrings);

		reset_fatal = 0;

		if (int_status & (SD0001_INT_SSERR|SD0001_INT_RPERR
				  |SD0001_INT_SPERR|SD0001_INT_STABT
				  |SD0001_INT_RTABT|SD0001_INT_RMABT)) {
			/* Clear of Configration Status Bits */
			sd0001_writel(4, INDIRECT_ADR);
			sd0001_writel(0xf9000000, INDIRECT_DATA);
			sd0001_writel(0x000c0002, INDIRECT_CTL);
			to_cnt = 0;
			while (((indrct_flg = sd0001_readl(INDIRECT_STS)) & SD0001_INDRCTF_INDFLG)
			       && (to_cnt++ < SD0001_INDIR_TIME))
				;


			if (indrct_flg & SD0001_INDRCTF_INDFLG) {
				panic("SD0001 Fatal Error 1\n");
			} else {
				if (indrct_flg & SD0001_INDRCTF_MABTRCV) {
					sd0001_writel(SD0001_INDRCTC_FLGRESET, INDIRECT_CTL);
					reset_fatal = -1;
				}
			}
			int_status = sd0001_readl(INT_STS1) & SD0001_INT_BUSERR;
		}

		if (int_status != 0) {
			sd0001_writel(int_status, INT_STS1); /* 割り込みクリア */

			if (reset_fatal || (sd0001_readl(INT_STS2) & SD0001_INT_BUSERR)) {
				printk(KERN_CRIT "Fatal Error:SD0001 PCI Status Can't Clear 0x%08x\n",
				       int_status & 0x7fffffff);
				sd0001_writel(sd0001_readl(INT_ENABLE) & ~int_status, INT_ENABLE);
				/* Masked Error Interrupt */
			}
		}
	}
	spin_unlock_irqrestore(&sd0001_indirect_lock, flags);
}


static inline u32 convert_dev_to_addr (struct pci_dev *dev, u32 reg)
{
	return (SD0001_CONFIG_ADDR_EN
		| (dev->bus->number << 16)
		| ((dev->devfn & 0xff) << 8)
		| (reg & 0xff)
		| ((dev->bus->number)?0x00:0x01));
}


static int sd0001_indirect_RW (u32  addr, u32 cmd, u32  be,
				  u32  rw, u32  *data)
{
	u32 indrct_flg;
	u32 int_sts;
	u32 to_cnt = 0;
	int st = PCIBIOS_SUCCESSFUL;
	unsigned long flags;

	spin_lock_irqsave(&sd0001_indirect_lock, flags);
	if ((cmd & SD0001_INDRCTC_CMD_MASK) == SD0001_INDRCTC_CMD_MEMR
	    || (cmd & SD0001_INDRCTC_CMD_MASK) == SD0001_INDRCTC_CMD_MEMW)
		sd0001_writel(addr & 0xfffffffc, INDIRECT_ADR);
	else
		sd0001_writel(addr, INDIRECT_ADR);

	if (rw == SD0001_INDRCTC_IOWT || rw == SD0001_INDRCTC_COWT)
		sd0001_writel(*data, INDIRECT_DATA);

	sd0001_writel(be | cmd | rw , INDIRECT_CTL);

	while (((indrct_flg = sd0001_readl(INDIRECT_STS)) & SD0001_INDRCTF_INDFLG)
	       && (to_cnt++ < SD0001_INDIR_TIME));

	int_sts = sd0001_readl(INT_STS1) & SD0001_INT_BUSERR;
    
	if (indrct_flg & SD0001_INDRCTF_INDFLG) {	/* タイムアウト */
		printk("SD0001 Fatal Error 2\n");
		spin_unlock_irqrestore(&sd0001_indirect_lock, flags);

		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	if (int_sts != 0 || (indrct_flg & SD0001_INDRCTF_MABTRCV) != 0) {
		if ((st = (indrct_flg & SD0001_INDRCTF_MABTRCV) >> 19) != 0) {
			sd0001_writel(SD0001_INDRCTC_FLGRESET, INDIRECT_CTL);
			st |= 0x80000000;
		}

		st |= 0x80000000 | int_sts;

		if ((int_sts & (SD0001_INT_SSERR|SD0001_INT_RPERR
				|SD0001_INT_SPERR|SD0001_INT_STABT
				|SD0001_INT_RTABT|SD0001_INT_RMABT))
		    || (indrct_flg & SD0001_INDRCTF_MABTRCV)) {
			/* Clear of Configration Status Bits */
			sd0001_writel(4, INDIRECT_ADR);
			sd0001_writel(0xf9000000, INDIRECT_DATA);
			sd0001_writel(0x000c0002, INDIRECT_CTL);

			to_cnt = 0;
			while (((indrct_flg = sd0001_readl(INDIRECT_STS)) & SD0001_INDRCTF_INDFLG)
			       && (to_cnt++ < SD0001_INDIR_TIME));

			if (indrct_flg & SD0001_INDRCTF_INDFLG) {	/* タイムアウト */
				panic("SD0001 Fatal Error 3\n");
			}

			if (indrct_flg & SD0001_INDRCTF_MABTRCV) {
				
				sd0001_writel(SD0001_INDRCTC_FLGRESET, INDIRECT_CTL);
			}
		}
		
		printk(KERN_ERR "PCI Bus Error: status 0x%08x\n", st);

		if ((int_sts = sd0001_readl(INT_STS1) & SD0001_INT_BUSERR) != 0) {
			sd0001_writel(int_sts, INT_STS1);	/* 割り込みクリア */
			
			if (sd0001_readl(INT_STS2) & SD0001_INT_BUSERR) {
				printk(KERN_CRIT "Fatal Error:SD0001 PCI Status Can't Clear 0x%08x\n",
				       sd0001_readl(INT_STS2) & SD0001_INT_BUSERR);
				sd0001_writel(sd0001_readl(INT_ENABLE) & ~int_sts, INT_ENABLE); /* Masked Error Interrupt */
			}
		}

		*data = 0xffffffff;
	} else {
		if (rw != SD0001_INDRCTC_IOWT && rw != SD0001_INDRCTC_COWT)
			*data = sd0001_readl(INDIRECT_DATA);
	}

	spin_unlock_irqrestore(&sd0001_indirect_lock, flags);
	return st;
}

static inline
int sd0001_config_RW (struct pci_dev *dev, u32 reg, u32 be, u32 rw, u32 *data)
{
	u32  reg_addr = convert_dev_to_addr(dev, reg);

	if (reg_addr == 0) {
		*data = 0xffffffff;
		return PCIBIOS_SUCCESSFUL;
	}
    
	return sd0001_indirect_RW (reg_addr, 0, be, rw, data);

}


static int sd0001_read_config_byte(struct pci_dev *dev, int reg, u8 *val)
{
	int  offset;
	u32  be;
	int  re;
	union {
		u32 ldata;
		u8  bdata[4];
	} work;

	be = SD0001_INDRCTC_BE_BYTE << (reg & 0x03);
	re = sd0001_config_RW (dev, reg, be, SD0001_INDRCTC_CORD, &work.ldata);

#if __LITTLE_ENDIAN__
	offset = reg & 0x03;
#else  /* __LITTLE_ENDIAN__ */
	offset = 3 - (reg & 0x03);
#endif /* __LITTLE_ENDIAN__ */
	*val = work.bdata[offset];

	return re;

}


static int sd0001_read_config_word(struct pci_dev *dev, int reg, u16 *val)
{
	int  offset;
	u32 be;
	int  re;
	union {
		u32 ldata;
		u16 wdata[2];
	} work;

	be = SD0001_INDRCTC_BE_WORD << (reg & 0x02);
	
	re = sd0001_config_RW (dev, reg, be, SD0001_INDRCTC_CORD, &work.ldata);
	
#if __LITTLE_ENDIAN__
	offset = (reg >> 1) & 0x01;
#else  /* __LITTLE_ENDIAN__ */
	offset = 1 - ((reg >> 1) & 0x01);
#endif /* __LITTLE_ENDIAN__ */
	
	*val = work.wdata[offset];
	
	return re;
}


static int sd0001_read_config_dword(struct pci_dev *dev, int reg, u32 *val)
{
	return sd0001_config_RW (dev, reg, SD0001_INDRCTC_BE_LONG,
			      SD0001_INDRCTC_CORD, val);
}

static int sd0001_write_config_byte (struct pci_dev *dev, int reg, u8 val)
{
	int  offset;
	u32 be;
	union {
		u32 ldata;
		u8  bdata[4];
	} work;

	be = SD0001_INDRCTC_BE_BYTE << (reg & 0x03);
#if __LITTLE_ENDIAN__
	offset = reg & 0x03;
#else  /* __LITTLE_ENDIAN__ */
	offset = 3 - (reg & 0x03);
#endif /* __LITTLE_ENDIAN__ */
	work.bdata[offset] = val;

	return sd0001_config_RW(dev, reg, be, SD0001_INDRCTC_COWT, &work.ldata);
    
}


static int sd0001_write_config_word (struct pci_dev *dev, int reg, u16 val)
{
	int  offset;
	u32 be;
	union {
		u32  ldata;
		u16  wdata[2];
	} work;


	be = SD0001_INDRCTC_BE_WORD << (reg & 0x02);
#if __LITTLE_ENDIAN__
	offset = (reg >> 1) & 0x01;
#else  /* __LITTLE_ENDIAN__ */
	offset = 1 - ((reg >> 1) & 0x01);
#endif /* __LITTLE_ENDIAN__ */

	work.wdata[offset] = val;

	return sd0001_config_RW (dev, reg, be, SD0001_INDRCTC_COWT, &work.ldata);

}


static int sd0001_write_config_dword (struct pci_dev *dev, int reg, u32 val)
{
	return sd0001_config_RW (dev, reg, SD0001_INDRCTC_BE_LONG, SD0001_INDRCTC_COWT, &val);
}

static struct pci_ops sd0001_pci_ops = 
{
	.read_byte =	sd0001_read_config_byte,
	.read_word =	sd0001_read_config_word,
	.read_dword =	sd0001_read_config_dword,
	.write_byte =	sd0001_write_config_byte,
	.write_word =	sd0001_write_config_word,
	.write_dword =	sd0001_write_config_dword
};

int __init pci_setup_sd0001 (void)
{
	sd0001_writel(0, INT_ENABLE);		/* all Interrupt = Mask */

	sd0001_bus_reset();

	/*
	 * PCIバス制御の動作モード(MCW0レジスタ)設定
	 * ・PCIバスのリトライ回数 : 無限回
	 * ・バスグランドの抑止なし
	 */
#if __LITTLE_ENDIAN__
	sd0001_writel(0x0000, PCI_CTL);
#else
	sd0001_writel(SD0001_CTL_MASTER_SWAP | SD0001_CTL_PCI_EDCONV, PCI_CTL);
#endif
	sd0001_writel(0, PCI_IO_OFFSET);
	sd0001_writel(PCIBIOS_MIN_MEM, PCI_MEM_OFFSET);


	if (request_irq(CONFIG_PCI_SD0001_IRQ, &sd0001_int_pcierr, SA_SHIRQ, "PCI Bus Error", NULL))
		printk(KERN_ERR "Can't Setup PCI Bus Error Interrupt\n");

	/* FIXME: Enable INT[ABCD] only when devices actually want them.
	   We should probably demux them so they appear to the kernel as
	   separate IRQs */
	sd0001_writel(SD0001_INT_INTEN|SD0001_INT_BUSERR
		|SD0001_INT_INTD|SD0001_INT_INTC|SD0001_INT_INTB
		|SD0001_INT_INTA, INT_ENABLE);


	return PCIBIOS_SUCCESSFUL;
}


void __init
pcibios_fixup_pbus_ranges(struct pci_bus * bus,
	struct pbus_set_ranges_data * ranges)
{
	ranges->io_start -= bus->resource[0]->start;
	ranges->io_end -= bus->resource[0]->start;
	ranges->mem_start -= bus->resource[1]->start;
	ranges->mem_end -= bus->resource[1]->start;
}


static u8 __init no_swizzle(struct pci_dev *dev, u8 * pin)
{
        return PCI_SLOT(dev->devfn);
}

static int __init sd0001_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	return CONFIG_PCI_SD0001_IRQ;
}

void __init pcibios_init(void)
{
	printk(KERN_NOTICE "Linux/SH SD0001 PCI Initialise\n");
	
	ioport_resource.end = 0xffff;
	iomem_resource.end = 0xfeffffff;

	pci_setup_sd0001();
	pci_scan_bus(0, &sd0001_pci_ops, NULL);

	pci_assign_unassigned_resources();

	pci_fixup_irqs(no_swizzle, sd0001_map_irq);
}

char * __init pcibios_setup(char *str)
{
        return str;
}

extern unsigned long memory_start, memory_end;

void __init pcibios_fixup_bus(struct pci_bus *bus)
{
	struct list_head *list;

	list_for_each(list, &bus->devices) {
		struct pci_dev *dev = pci_dev_b(list);
		u16 cmd;
		if (dev->class >> 8 == PCI_CLASS_BRIDGE_HOST) {
			memset(&dev->resource[1], 0, sizeof(struct resource));
			dev->resource[1].start = __pa(memory_start);
			dev->resource[1].end = __pa(memory_end)-1;
			dev->resource[1].flags = IORESOURCE_MEM|IORESOURCE_PREFETCH;
			dev->resource[1].name = "PCI Host RAM";
			request_resource(&iomem_resource, &dev->resource[1]);

#if 0
			printk("res1 (@%p) %s %08lx %08lx %x %p %p %p\n", 
			       &dev->resource[1],
			       dev->resource[1].name,
			       dev->resource[1].start,
			       dev->resource[1].end,
			       dev->resource[1].flags,
			       dev->resource[1].parent,
			       dev->resource[1].sibling,
			       dev->resource[1].child);
#endif
			pcibios_update_resource(dev, bus->resource[1], 
						&dev->resource[1], 1);

			cmd = PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;
			pci_write_config_word(dev, PCI_COMMAND, cmd);
			pci_write_config_byte(dev, PCI_LATENCY_TIMER, 0x78);
		}
	}
}



/* Add future fixups here... */
struct pci_fixup pcibios_fixups[] = {
        { 0 }
};


        /*
         * Mapping for PCI Devices.
	 * The SD0001 has a 48MiB window onto the PCI memory space, mapped
	 * into the CPU's address space at 0xb1000000-0xb3ffffff. 
	 * The range of the PCI space which is accessible is controlled
	 * by the SD0001's PCI_MEM_OFFSET register, which has a granularity
	 * of 64MiB. 
	 *
	 * As far as I can tell from the little I can understand of
	 * the SD0001 documentation and from the behaviour of the
	 * device, you have a 48MiB window which can only be moved
	 * with 64MiB granularity. Therefore to the best of my
	 * knowledge, you cannot access any PCI memory address where
	 * (<addr> & 0x03000000) == 0x03000000, except by going 
	 * indirectly through the PCI bridge like we do for configuration
	 * and I/O cycles.
	 *
	 * Hopefully, this is untrue and there's some way of doing it that
	 * I just don't know because I can't read the docs. For now, we
	 * allow only access to the first 48MiB of PCI memory space.
	 *
	 * In addition to that joy, it appears that when accessing PCI
	 * memory space directly, the SD0001 swaps address lines #21
	 * and #22, hence the macro below to swap addresses back
	 * again.
	 *
	 * Me and my baseball bat want a quiet word with someone. 
	 *
	 * dwmw2.
         */

#define unmunge(x) (((x) & ~0x00300000) | ( ((x)&0x00100000) << 1) | ( ((x)&0x00200000) >> 1))

void *sd0001_ioremap(unsigned long phys_addr, unsigned long size)
{
        unsigned long offset;

	if ((phys_addr & 0xFFF00000) == ((phys_addr+size) & 0xFFF00000)) {
		/* It fits within a single mebibyte we can still use the 
		   directly-mapped region... */
		return (void *)(SD0001_MEM_BASE-(PCIBIOS_MIN_MEM&0xfc000000)+unmunge(phys_addr));
	}

	/* It crosses a mebibyte boundary and hence we have to
	   play VM tricks to make the region which is physically
	   contiguous on the PCI bus but not physically contiguous
	   on the SH3 bus appear virtually contiguous to the kernel.
	   Got that? Did I mention my baseball bat yet? */
	void * addr;
	struct vm_struct * area;
		
	offset = phys_addr & ~PAGE_MASK;
	phys_addr &= PAGE_MASK;
	size = PAGE_ALIGN(phys_addr + size - 1) - phys_addr;

	area = get_vm_area(size, VM_IOREMAP);
	if (!area)
		return NULL;
	addr = area->addr;

	phys_addr += SD0001_MEM_BASE;
	while (size) {
		unsigned long this_size;

		this_size = 0x100000 - (phys_addr & 0xfffff);
		this_size = min(this_size, size);

		if (remap_area_pages(VMALLOC_VMADDR(addr),
				     unmunge(phys_addr), this_size, 
				     _PAGE_PRESENT | _PAGE_RW | _PAGE_DIRTY | _PAGE_ACCESSED | _PAGE_FLAGS_HARD)) {
			vfree(area->addr);
			return NULL;
		}
		size -= this_size;
		phys_addr += this_size;
		addr += this_size;
	}
	return (void *) (offset + (char *)area->addr);
}

void sd0001_iounmap(void *addr)
{
	if ((unsigned long)addr >= VMALLOC_START && 
	    (unsigned long)addr < VMALLOC_END)
		vfree((void *)((unsigned long)addr & ~PAGE_MASK));
}


static void sd0001_indirect_access_wait(unsigned long i)
{

	while((sd0001_readl(INDIRECT_STS) & SD0001_INDRCTF_INDFLG)
	      && --i)
		;

	if (i == 0) {
		printk("##### Long Indirect access wait #####\n");
		mdelay(20);
	}
}

static inline void delay(void)
{
	ctrl_inw(0xa0000000);
}

/* PCI IO read long word cycle */
unsigned long sd0001_inl(unsigned long port)
{
	return *(volatile unsigned long *) (SD0001_IO_BASE+port);
}
void sd0001_outl(unsigned long value, unsigned long port)
{
	*(volatile unsigned long *)(SD0001_IO_BASE+port) = value;
}

/* The SD0001 doesn't correctly handle byte enables for sub-word
   accesses, so we have to do it indirectly like we do configuration
   cycles */

/* PCI IO read byte cycle */
unsigned char sd0001_inb(unsigned long port)
{
	unsigned long work, val, byte_e;
	unsigned long flags;

	spin_lock_irqsave(&sd0001_indirect_lock, flags);
	val = 0;
	byte_e = 0x00010000;
	work = (port & 0x00000003);
	byte_e <<= work;

	port &= 0xFFFFFFFC;
	sd0001_writel(port, INDIRECT_ADR);
	sd0001_writel(0x00008204 | byte_e, INDIRECT_CTL); /* I/O read cycle */
	sd0001_indirect_access_wait(100);
	val = sd0001_readl(INDIRECT_DATA);

	val >>= (work*8);
	val &= 0x000000FF;

	spin_unlock_irqrestore(&sd0001_indirect_lock, flags);

	return val;
}

/* PCI IO write byte cycle */
void sd0001_outb(unsigned char value, unsigned long port)
{
	unsigned long work, data, byte_e;
	unsigned long flags;

	spin_lock_irqsave(&sd0001_indirect_lock, flags);
	byte_e = 0x00010000;
	work = (port & 0x00000003);
	byte_e <<= work;

	data = value;
	data <<= (work*8);

	port &= 0xFFFFFFFC;
	sd0001_writel(data, INDIRECT_DATA);
	sd0001_writel(port, INDIRECT_ADR);
	sd0001_writel((0x00008308 | byte_e), INDIRECT_CTL);	/* I/O write cycle */
	sd0001_indirect_access_wait(100);

	spin_unlock_irqrestore(&sd0001_indirect_lock, flags);
}

/* PCI IO read word cycle */
unsigned short sd0001_inw(unsigned long port)
{
	unsigned long work, val, byte_e;
	unsigned long flags;

	spin_lock_irqsave(&sd0001_indirect_lock, flags);
	val = 0;
	byte_e = 0x00030000;
	work = (port & 0x00000003);
	byte_e <<= (work & 0x00000002);
	work >>= 1;

	port &= 0xFFFFFFFC;
	sd0001_writel(port, INDIRECT_ADR);
	sd0001_writel((0x00008204 | byte_e), INDIRECT_CTL);	/* I/O read cycle */
	sd0001_indirect_access_wait(100);
	val = sd0001_readl(INDIRECT_DATA) ;

	val >>= (work*16);
	val &= 0x0000FFFF;

	spin_unlock_irqrestore(&sd0001_indirect_lock, flags);

	return val;
}

/* PCI IO write word cycle */
void sd0001_outw(unsigned short value, unsigned long port)
{
	unsigned long work, data, byte_e;
	unsigned long flags;

	spin_lock_irqsave(&sd0001_indirect_lock, flags);
	byte_e = 0x00030000;
	work = (port & 0x00000003);
	byte_e <<= (work & 0x00000002);
	work >>= 1;

	port &= 0xFFFFFFFC;
	data = value;
	data <<= (work*16);

	sd0001_writel(data, INDIRECT_DATA);
	sd0001_writel(port, INDIRECT_ADR);
	sd0001_writel((0x00008308 | byte_e), INDIRECT_CTL);	/* I/O write cycle */
	sd0001_indirect_access_wait(100);
	spin_unlock_irqrestore(&sd0001_indirect_lock, flags);
}

void sd0001_insb(unsigned long port, void *addr, unsigned long count)
{

	while (count--)
		*((unsigned char *) addr)++ = (unsigned char)sd0001_inb(port);
}

void sd0001_insw(unsigned long port, void *addr, unsigned long count)
{

	while (count--)
		*((unsigned short *) addr)++ = (unsigned short)sd0001_inw(port);
}

void sd0001_insl(unsigned long port, void *addr, unsigned long count)
{

	while (count--)
		*((unsigned long *) addr)++ = sd0001_inl(port);
}

void sd0001_outsb(unsigned long port, const void *addr, unsigned long count)
{

	while (count--)
		sd0001_outb(*((unsigned char *)addr)++, port);
}

void sd0001_outsw(unsigned long port, const void *addr, unsigned long count)
{

	while (count--)
		sd0001_outw(*((unsigned short *)addr)++, port);
}

void sd0001_outsl(unsigned long port, const void *addr, unsigned long count)
{

	while (count--)
		sd0001_outl(*((unsigned long *)addr)++, port);
}

unsigned char sd0001_inb_p(unsigned long port)
{
	unsigned long v;

	v = sd0001_inb(port);
	delay();
	return v;
}

void sd0001_outb_p(unsigned char value, unsigned long port)
{

	sd0001_outb(value, port);
	delay();
}
