/* linux/drivers/mtd/maps/scx200_docflash.c 

   Copyright (c) 2001,2002 Christer Weinigel <wingel@nano-system.com>

   $Id: scx200_docflash.c,v 1.1 2003/01/24 13:20:40 dwmw2 Exp $ 

   National Semiconductor SCx200 flash mapped with DOCCS
*/

#include <linux/module.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <linux/pci.h>
#include <linux/scx200.h>

#define NAME "scx200_docflash"

MODULE_AUTHOR("Christer Weinigel <wingel@hack.org>");
MODULE_DESCRIPTION("NatSemi SCx200 DOCCS Flash Driver");
MODULE_LICENSE("GPL");

/* Set this to one if you want to partition the flash */
#define PARTITION 1

MODULE_PARM(probe, "i");
MODULE_PARM_DESC(probe, "Probe for a BIOS mapping");
MODULE_PARM(size, "i");
MODULE_PARM_DESC(size, "Size of the flash mapping");
MODULE_PARM(width, "i");
MODULE_PARM_DESC(width, "Data width of the flash mapping (8/16)");
MODULE_PARM(flashtype, "s");
MODULE_PARM_DESC(flashtype, "Type of MTD probe to do");

static int probe = 0;		/* Don't autoprobe */
static unsigned size = 0x1000000; /* 16 MiB the whole ISA address space */
static unsigned width = 8;	/* Default to 8 bits wide */
static char *flashtype = "cfi_probe";

static struct resource docmem = {
	.flags = IORESOURCE_MEM,
	.name  = "NatSemi SCx200 DOCCS Flash",
};

static struct mtd_info *mymtd;

#if PARTITION
static struct mtd_partition partition_info[] = {
	{ 
		.name   = "DOCCS Boot kernel", 
		.offset = 0, 
		.size   = 0xc0000
	},
	{ 
		.name   = "DOCCS Low BIOS", 
		.offset = 0xc0000, 
		.size   = 0x40000
	},
	{ 
		.name   = "DOCCS File system", 
		.offset = 0x100000, 
		.size   = ~0	/* calculate from flash size */
	},
	{ 
		.name   = "DOCCS High BIOS", 
		.offset = ~0, 	/* calculate from flash size */
		.size   = 0x80000
	},
};
#define NUM_PARTITIONS (sizeof(partition_info)/sizeof(partition_info[0]))
#endif

static __u8 scx200_docflash_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + ofs);
}

static __u16 scx200_docflash_read16(struct map_info *map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

static void scx200_docflash_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

static void scx200_docflash_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	__raw_writeb(d, map->map_priv_1 + adr);
	mb();
}

static void scx200_docflash_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	__raw_writew(d, map->map_priv_1 + adr);
	mb();
}

static void scx200_docflash_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio(map->map_priv_1 + to, from, len);
}

static struct map_info scx200_docflash_map = {
	.name      = "NatSemi SCx200 DOCCS Flash",
	.read8     = scx200_docflash_read8,
	.read16    = scx200_docflash_read16,
	.copy_from = scx200_docflash_copy_from,
	.write8    = scx200_docflash_write8,
	.write16   = scx200_docflash_write16,
	.copy_to   = scx200_docflash_copy_to
};

int __init init_scx200_docflash(void)
{
	unsigned u;
	unsigned base;
	unsigned ctrl;
	unsigned pmr;
	struct pci_dev *bridge;

	printk(KERN_DEBUG NAME ": NatSemi SCx200 DOCCS Flash Driver\n");

	if ((bridge = pci_find_device(PCI_VENDOR_ID_NS, 
				      PCI_DEVICE_ID_NS_SCx200_BRIDGE,
				      NULL)) == NULL)
		return -ENODEV;
	
	if (!scx200_cb_probe(SCx200_CB_BASE)) {
		printk(KERN_WARNING NAME ": no configuration block found\n");
		return -ENODEV;
	}

	if (probe) {
		/* Try to use the present flash mapping if any */
		pci_read_config_dword(bridge, SCx200_DOCCS_BASE, &base);
		pci_read_config_dword(bridge, SCx200_DOCCS_CTRL, &ctrl);
		pmr = inl(SCx200_CB_BASE + SCx200_PMR);

		if (base == 0
		    || (ctrl & 0x07000000) != 0x07000000
		    || (ctrl & 0x0007ffff) == 0)
			return -ENODEV;

		size = ((ctrl&0x1fff)<<13) + (1<<13);

		for (u = size; u > 1; u >>= 1)
			;
		if (u != 1)
			return -ENODEV;

		if (pmr & (1<<6))
			width = 16;
		else
			width = 8;

		docmem.start = base;
		docmem.end = base + size;

		if (request_resource(&iomem_resource, &docmem)) {
			printk(KERN_ERR NAME ": unable to allocate memory for flash mapping\n");
			return -ENOMEM;
		}
	} else {
		for (u = size; u > 1; u >>= 1)
			;
		if (u != 1) {
			printk(KERN_ERR NAME ": invalid size for flash mapping\n");
			return -EINVAL;
		}
		
		if (width != 8 && width != 16) {
			printk(KERN_ERR NAME ": invalid bus width for flash mapping\n");
			return -EINVAL;
		}
		
		if (allocate_resource(&iomem_resource, &docmem, 
				      size,
				      0xc0000000, 0xffffffff, 
				      size, NULL, NULL)) {
			printk(KERN_ERR NAME ": unable to allocate memory for flash mapping\n");
			return -ENOMEM;
		}
		
		ctrl = 0x07000000 | ((size-1) >> 13);

		printk(KERN_INFO "DOCCS BASE=0x%08lx, CTRL=0x%08lx\n", (long)docmem.start, (long)ctrl);
		
		pci_write_config_dword(bridge, SCx200_DOCCS_BASE, docmem.start);
		pci_write_config_dword(bridge, SCx200_DOCCS_CTRL, ctrl);
		pmr = inl(SCx200_CB_BASE + SCx200_PMR);
		
		if (width == 8) {
			pmr &= ~(1<<6);
		} else {
			pmr |= (1<<6);
		}
		outl(pmr, SCx200_CB_BASE + SCx200_PMR);
	}
	
       	printk(KERN_INFO NAME ": DOCCS mapped at 0x%lx-0x%lx, width %d\n", 
	       docmem.start, docmem.end, width);

	scx200_docflash_map.size = size;
	if (width == 8)
		scx200_docflash_map.buswidth = 1;
	else
		scx200_docflash_map.buswidth = 2;

	scx200_docflash_map.map_priv_1 = (unsigned long)ioremap(docmem.start, scx200_docflash_map.size);
	if (!scx200_docflash_map.map_priv_1) {
		printk(KERN_ERR NAME ": failed to ioremap the flash\n");
		release_resource(&docmem);
		return -EIO;
	}

	mymtd = do_map_probe(flashtype, &scx200_docflash_map);
	if (!mymtd) {
		printk(KERN_ERR NAME ": unable to detect flash\n");
		iounmap((void *)scx200_docflash_map.map_priv_1);
		release_resource(&docmem);
		return -ENXIO;
	}

	if (size < mymtd->size)
		printk(KERN_WARNING NAME ": warning, flash mapping is smaller than flash size\n");

	mymtd->module = THIS_MODULE;

#if PARTITION
	partition_info[3].offset = mymtd->size-partition_info[3].size;
	partition_info[2].size = partition_info[3].offset-partition_info[2].offset;
	add_mtd_partitions(mymtd, partition_info, NUM_PARTITIONS);
#else
	add_mtd_device(mymtd);
#endif
	return 0;
}

static void __exit cleanup_scx200_docflash(void)
{
	if (mymtd) {
#if PARTITION
		del_mtd_partitions(mymtd);
#else
		del_mtd_device(mymtd);
#endif
		map_destroy(mymtd);
	}
	if (scx200_docflash_map.map_priv_1) {
		iounmap((void *)scx200_docflash_map.map_priv_1);
		release_resource(&docmem);
	}
}

module_init(init_scx200_docflash);
module_exit(cleanup_scx200_docflash);

/*
    Local variables:
        compile-command: "make -k -C ../../.. SUBDIRS=drivers/mtd/maps modules"
        c-basic-offset: 8
    End:
*/
