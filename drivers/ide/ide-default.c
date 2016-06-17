/*
 *	ide-default		-	Driver for unbound ide devices
 *
 *	This provides a clean way to bind a device to default operations
 *	by having an actual driver class that rather than special casing
 *	"no driver" all over the IDE code
 *
 *	Copyright (C) 2003, Red Hat <alan@redhat.com>
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/genhd.h>
#include <linux/slab.h>
#include <linux/cdrom.h>
#include <linux/ide.h>

#include <asm/byteorder.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/unaligned.h>
#include <asm/bitops.h>

#define IDEDEFAULT_VERSION	"0.9.newide"
/*
 *	Driver initialization.
 */

static void idedefault_setup (ide_drive_t *drive)
{
}

static int idedefault_open(struct inode *inode, struct file *filp, ide_drive_t *drive)
{
	MOD_INC_USE_COUNT;
	if(filp->f_flags & O_NDELAY)
		return 0;
	MOD_DEC_USE_COUNT;
	drive->usage--;
	return -ENXIO;
}

static void idedefault_release(struct inode *inode, struct file *filp, ide_drive_t *drive)
{
	MOD_DEC_USE_COUNT;
}

int idedefault_init (void);
int idedefault_attach(ide_drive_t *drive);

/*
 *	IDE subdriver functions, registered with ide.c
 */

ide_driver_t idedefault_driver = {
	name:			"ide-default",
	version:		IDEDEFAULT_VERSION,
	media:			0,
	busy:			0,
	supports_dma:		1,
	supports_dsc_overlap:	0,
	init:			idedefault_init,
	attach:			idedefault_attach,
	open:			idedefault_open,
	release:		idedefault_release
};

static ide_module_t idedefault_module = {
	IDE_DRIVER_MODULE,
	idedefault_init,
	&idedefault_driver,
	NULL
};

int idedefault_attach (ide_drive_t *drive)
{
	int ret = 0;
	MOD_INC_USE_COUNT;
	if (ide_register_subdriver(drive,
			&idedefault_driver, IDE_SUBDRIVER_VERSION)) {
		printk(KERN_ERR "ide-default: %s: Failed to register the "
			"driver with ide.c\n", drive->name);
		ret = 1;
		goto bye_game_over;
	}
	DRIVER(drive)->busy++;
	idedefault_setup(drive);
	DRIVER(drive)->busy--;

bye_game_over:
	MOD_DEC_USE_COUNT;
	return ret;
}

int idedefault_init (void)
{
	ide_register_module(&idedefault_module);
	return 0;
}

MODULE_LICENSE("GPL");
