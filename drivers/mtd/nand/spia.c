/*
 *  drivers/mtd/nand/spia.c
 *
 *  Copyright (C) 2000 Steven J. Hill (sjhill@cotw.com)
 *
 *
 *	10-29-2001 TG	change to support hardwarespecific access
 *			to controllines	(due to change in nand.c)
 *			page_cache added
 *
 * $Id: spia.c,v 1.16 2002/03/05 13:50:47 dwmw2 Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Overview:
 *   This is a device driver for the NAND flash device found on the
 *   SPIA board which utilizes the Toshiba TC58V64AFT part. This is
 *   a 64Mibit (8MiB x 8 bits) NAND flash device.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <asm/io.h>

/*
 * MTD structure for SPIA board
 */
static struct mtd_info *spia_mtd = NULL;

/*
 * Values specific to the SPIA board (used with EP7212 processor)
 */
#define SPIA_IO_ADDR	= 0xd0000000	/* Start of EP7212 IO address space */
#define SPIA_FIO_ADDR	= 0xf0000000	/* Address where flash is mapped */
#define SPIA_PEDR	= 0x0080	/*
					 * IO offset to Port E data register
					 * where the CLE, ALE and NCE pins
					 * are wired to.
					 */
#define SPIA_PEDDR	= 0x00c0	/*
					 * IO offset to Port E data direction
					 * register so we can control the IO
					 * lines.
					 */

/*
 * Module stuff
 */

static int spia_io_base = SPIA_IO_BASE;
static int spia_fio_base = SPIA_FIO_BASE;
static int spia_pedr = SPIA_PEDR;
static int spia_peddr = SPIA_PEDDR;

MODULE_PARM(spia_io_base, "i");
MODULE_PARM(spia_fio_base, "i");
MODULE_PARM(spia_pedr, "i");
MODULE_PARM(spia_peddr, "i");

__setup("spia_io_base=",spia_io_base);
__setup("spia_fio_base=",spia_fio_base);
__setup("spia_pedr=",spia_pedr);
__setup("spia_peddr=",spia_peddr);

/*
 * Define partitions for flash device
 */
const static struct mtd_partition partition_info[] = {
	{ name: "SPIA flash partition 1",
	  offset: 0,
	  size: 2*1024*1024 },
	{ name: "SPIA flash partition 2",
	  offset: 2*1024*1024,
	  size: 6*1024*1024 }
};
#define NUM_PARTITIONS 2


/* 
 *	hardware specific access to control-lines
*/
void spia_hwcontrol(int cmd){

    switch(cmd){

	case NAND_CTL_SETCLE: (*(volatile unsigned char *) (spia_io_base + spia_pedr)) |=  0x01; break;
	case NAND_CTL_CLRCLE: (*(volatile unsigned char *) (spia_io_base + spia_pedr)) &= ~0x01; break;

	case NAND_CTL_SETALE: (*(volatile unsigned char *) (spia_io_base + spia_pedr)) |=  0x02; break;
	case NAND_CTL_CLRALE: (*(volatile unsigned char *) (spia_io_base + spia_pedr)) &= ~0x02; break;

	case NAND_CTL_SETNCE: (*(volatile unsigned char *) (spia_io_base + spia_pedr)) &= ~0x04; break;
	case NAND_CTL_CLRNCE: (*(volatile unsigned char *) (spia_io_base + spia_pedr)) |=  0x04; break;
    }
}

/*
 * Main initialization routine
 */
int __init spia_init (void)
{
	struct nand_chip *this;

	/* Allocate memory for MTD device structure and private data */
	spia_mtd = kmalloc (sizeof(struct mtd_info) + sizeof (struct nand_chip),
				GFP_KERNEL);
	if (!spia_mtd) {
		printk ("Unable to allocate SPIA NAND MTD device structure.\n");
		return -ENOMEM;
	}

	/* Get pointer to private data */
	this = (struct nand_chip *) (&spia_mtd[1]);

	/* Initialize structures */
	memset((char *) spia_mtd, 0, sizeof(struct mtd_info));
	memset((char *) this, 0, sizeof(struct nand_chip));

	/* Link the private data with the MTD structure */
	spia_mtd->priv = this;

	/*
	 * Set GPIO Port E control register so that the pins are configured
	 * to be outputs for controlling the NAND flash.
	 */
	(*(volatile unsigned char *) (spia_io_base + spia_peddr)) = 0x07;

	/* Set address of NAND IO lines */
	this->IO_ADDR_R = spia_fio_base;
	this->IO_ADDR_W = spia_fio_base;
	/* Set address of hardware control function */
	this->hwcontrol = spia_hwcontrol;
	/* 15 us command delay time */
	this->chip_delay = 15;		

	/* Scan to find existence of the device */
	if (nand_scan (spia_mtd)) {
		kfree (spia_mtd);
		return -ENXIO;
	}

	/* Allocate memory for internal data buffer */
	this->data_buf = kmalloc (sizeof(u_char) * (spia_mtd->oobblock + spia_mtd->oobsize), GFP_KERNEL);
	if (!this->data_buf) {
		printk ("Unable to allocate NAND data buffer for SPIA.\n");
		kfree (spia_mtd);
		return -ENOMEM;
	}

	/* Allocate memory for internal data buffer */
	this->data_cache = kmalloc (sizeof(u_char) * (spia_mtd->oobblock + spia_mtd->oobsize), GFP_KERNEL);
	if (!this->data_cache) {
		printk ("Unable to allocate NAND data cache for SPIA.\n");
		kfree (this->data_buf);
		kfree (spia_mtd);
		return = -ENOMEM;
	}
	this->cache_page = -1;

	/* Register the partitions */
	add_mtd_partitions(spia_mtd, partition_info, NUM_PARTITIONS);

	/* Return happy */
	return 0;
}
module_init(spia_init);

/*
 * Clean up routine
 */
#ifdef MODULE
static void __exit spia_cleanup (void)
{
	struct nand_chip *this = (struct nand_chip *) &spia_mtd[1];

	/* Unregister the device */
	del_mtd_device (spia_mtd);

	/* Free internal data buffer */
	kfree (this->data_buf);
	kfree (this->page_cache);

	/* Free the MTD device structure */
	kfree (spia_mtd);
}
module_exit(spia_cleanup);
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Steven J. Hill <sjhill@cotw.com");
MODULE_DESCRIPTION("Board-specific glue layer for NAND flash on SPIA board");
