/*
 *  drivers/mtd/nand/edb7312.c
 *
 *  Copyright (C) 2002 Marius Gröger (mag@sysgo.de)
 *
 *  Derived from drivers/mtd/nand/autcpu12.c
 *       Copyright (c) 2001 Thomas Gleixner (gleixner@autronix.de)
 *
 * $Id: edb7312.c,v 1.3 2002/06/06 12:58:16 mag Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Overview:
 *   This is a device driver for the NAND flash device found on the
 *   CLEP7312 board which utilizes the Toshiba TC58V64AFT part. This is
 *   a 64Mibit (8MiB x 8 bits) NAND flash device.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <asm/io.h>
#include <asm/arch/hardware.h> /* for CLPS7111_VIRT_BASE */
#include <asm/sizes.h>
#include <asm/hardware/clps7111.h>

/*
 * MTD structure for EDB7312 board
 */
static struct mtd_info *ep7312_mtd = NULL;

/*
 * Values specific to the EDB7312 board (used with EP7312 processor)
 */
#define EP7312_FIO_PBASE 0x10000000	/* Phys address of flash */
#define EP7312_PXDR	0x0001	/*
				 * IO offset to Port B data register
				 * where the CLE, ALE and NCE pins
				 * are wired to.
				 */
#define EP7312_PXDDR	0x0041	/*
				 * IO offset to Port B data direction
				 * register so we can control the IO
				 * lines.
				 */

/*
 * Module stuff
 */

static int ep7312_fio_pbase = EP7312_FIO_PBASE;
static int ep7312_pxdr = EP7312_PXDR;
static int ep7312_pxddr = EP7312_PXDDR;

#ifdef MODULE
MODULE_PARM(ep7312_fio_pbase, "i");
MODULE_PARM(ep7312_pxdr, "i");
MODULE_PARM(ep7312_pxddr, "i");

__setup("ep7312_fio_pbase=",ep7312_fio_pbase);
__setup("ep7312_pxdr=",ep7312_pxdr);
__setup("ep7312_pxddr=",ep7312_pxddr);
#endif

#ifdef CONFIG_MTD_PARTITIONS
/*
 * Define static partitions for flash device
 */
static struct mtd_partition partition_info[] = {
	{ name: "EP7312 Nand Flash",
		  offset: 0,
		  size: 8*1024*1024 }
};
#define NUM_PARTITIONS 1

extern int parse_cmdline_partitions(struct mtd_info *master, 
				    struct mtd_partition **pparts,
				    const char *mtd_id);
#endif


/* 
 *	hardware specific access to control-lines
 */
static void ep7312_hwcontrol(int cmd) 
{
	switch(cmd) {
		
	case NAND_CTL_SETCLE: 
		clps_writeb(clps_readb(ep7312_pxdr) | 0x10, ep7312_pxdr); 
		break;
	case NAND_CTL_CLRCLE: 
		clps_writeb(clps_readb(ep7312_pxdr) & ~0x10, ep7312_pxdr);
		break;
		
	case NAND_CTL_SETALE:
		clps_writeb(clps_readb(ep7312_pxdr) | 0x20, ep7312_pxdr);
		break;
	case NAND_CTL_CLRALE:
		clps_writeb(clps_readb(ep7312_pxdr) & ~0x20, ep7312_pxdr);
		break;
		
	case NAND_CTL_SETNCE:
		clps_writeb((clps_readb(ep7312_pxdr) | 0x80) & ~0x40, ep7312_pxdr);
		break;
	case NAND_CTL_CLRNCE:
		clps_writeb((clps_readb(ep7312_pxdr) | 0x80) | 0x40, ep7312_pxdr);
		break;
	}
}

/*
 *	read device ready pin
 */
static int ep7312_device_ready(void)
{
	return 1;
}

/*
 * Main initialization routine
 */
static int __init ep7312_init (void)
{
	struct nand_chip *this;
	const char *part_type = 0;
	int mtd_parts_nb = 0;
	struct mtd_partition *mtd_parts = 0;
	int ep7312_fio_base;
	
	/* Allocate memory for MTD device structure and private data */
	ep7312_mtd = kmalloc(sizeof(struct mtd_info) + 
			     sizeof(struct nand_chip),
			     GFP_KERNEL);
	if (!ep7312_mtd) {
		printk("Unable to allocate EDB7312 NAND MTD device structure.\n");
		return -ENOMEM;
	}
	
	/* map physical adress */
	ep7312_fio_base = (unsigned long)ioremap(ep7312_fio_pbase, SZ_1K);
	if(!ep7312_fio_base) {
		printk("ioremap EDB7312 NAND flash failed\n");
		kfree(ep7312_mtd);
		return -EIO;
	}
	
	/* Get pointer to private data */
	this = (struct nand_chip *) (&ep7312_mtd[1]);
	
	/* Initialize structures */
	memset((char *) ep7312_mtd, 0, sizeof(struct mtd_info));
	memset((char *) this, 0, sizeof(struct nand_chip));
	
	/* Link the private data with the MTD structure */
	ep7312_mtd->priv = this;
	
	/*
	 * Set GPIO Port B control register so that the pins are configured
	 * to be outputs for controlling the NAND flash.
	 */
	clps_writeb(0xf0, ep7312_pxddr);
	
	/* insert callbacks */
	this->IO_ADDR_R = ep7312_fio_base;
	this->IO_ADDR_W = ep7312_fio_base;
	this->hwcontrol = ep7312_hwcontrol;
	this->dev_ready = ep7312_device_ready;
	/* 15 us command delay time */
	this->chip_delay = 15;
	
	/* Scan to find existence of the device */
	if (nand_scan (ep7312_mtd)) {
		iounmap((void *)ep7312_fio_base);
		kfree (ep7312_mtd);
		return -ENXIO;
	}
	
	/* Allocate memory for internal data buffer */
	this->data_buf = kmalloc (sizeof(u_char) * (ep7312_mtd->oobblock + ep7312_mtd->oobsize), GFP_KERNEL);
	if (!this->data_buf) {
		printk("Unable to allocate NAND data buffer for EDB7312.\n");
		iounmap((void *)ep7312_fio_base);
		kfree (ep7312_mtd);
		return -ENOMEM;
	}
	
	/* Allocate memory for internal data buffer */
	this->data_cache = kmalloc (sizeof(u_char) * (ep7312_mtd->oobblock + ep7312_mtd->oobsize), GFP_KERNEL);
	if (!this->data_cache) {
		printk("Unable to allocate NAND data cache for EDB7312.\n");
		kfree (this->data_buf);
		iounmap((void *)ep7312_fio_base);
		kfree (ep7312_mtd);
		return -ENOMEM;
	}
	this->cache_page = -1;
	
#ifdef CONFIG_MTD_CMDLINE_PARTS
	mtd_parts_nb = parse_cmdline_partitions(ep7312_mtd, &mtd_parts, 
						"edb7312-nand");
	if (mtd_parts_nb > 0)
	  part_type = "command line";
	else
	  mtd_parts_nb = 0;
#endif
	if (mtd_parts_nb == 0)
	{
		mtd_parts = partition_info;
		mtd_parts_nb = NUM_PARTITIONS;
		part_type = "static";
	}
	
	/* Register the partitions */
	printk(KERN_NOTICE "Using %s partition definition\n", part_type);
	add_mtd_partitions(ep7312_mtd, mtd_parts, mtd_parts_nb);
	
	/* Return happy */
	return 0;
}
module_init(ep7312_init);

/*
 * Clean up routine
 */
static void __exit ep7312_cleanup (void)
{
	struct nand_chip *this = (struct nand_chip *) &ep7312_mtd[1];
	
	/* Unregister the device */
	del_mtd_device (ep7312_mtd);
	
	/* Free internal data buffer */
	kfree (this->data_buf);
	kfree (this->data_cache);
	
	/* Free the MTD device structure */
	kfree (ep7312_mtd);
}
module_exit(ep7312_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marius Groeger <mag@sysgo.de>");
MODULE_DESCRIPTION("MTD map driver for Cogent EDB7312 board");
