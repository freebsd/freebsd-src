/*
 *  linux/drivers/ide/ide.c		Version 7.00beta3	Apr 22 2003
 *
 *  Copyright (C) 1994-1998  Linus Torvalds & authors (see below)
 */

/*
 *  Mostly written by Mark Lord  <mlord@pobox.com>
 *                and Gadi Oxman <gadio@netvision.net.il>
 *                and Andre Hedrick <andre@linux-ide.org>
 *
 *  See linux/MAINTAINERS for address of current maintainer.
 *
 * This is the multiple IDE interface driver, as evolved from hd.c.
 * It supports up to MAX_HWIFS IDE interfaces, on one or more IRQs
 *   (usually 14 & 15).
 * There can be up to two drives per interface, as per the ATA-2 spec.
 *
 * Primary:    ide0, port 0x1f0; major=3;  hda is minor=0; hdb is minor=64
 * Secondary:  ide1, port 0x170; major=22; hdc is minor=0; hdd is minor=64
 * Tertiary:   ide2, port 0x???; major=33; hde is minor=0; hdf is minor=64
 * Quaternary: ide3, port 0x???; major=34; hdg is minor=0; hdh is minor=64
 * ...
 *
 *  From hd.c:
 *  |
 *  | It traverses the request-list, using interrupts to jump between functions.
 *  | As nearly all functions can be called within interrupts, we may not sleep.
 *  | Special care is recommended.  Have Fun!
 *  |
 *  | modified by Drew Eckhardt to check nr of hd's from the CMOS.
 *  |
 *  | Thanks to Branko Lankester, lankeste@fwi.uva.nl, who found a bug
 *  | in the early extended-partition checks and added DM partitions.
 *  |
 *  | Early work on error handling by Mika Liljeberg (liljeber@cs.Helsinki.FI).
 *  |
 *  | IRQ-unmask, drive-id, multiple-mode, support for ">16 heads",
 *  | and general streamlining by Mark Lord (mlord@pobox.com).
 *
 *  October, 1994 -- Complete line-by-line overhaul for linux 1.1.x, by:
 *
 *	Mark Lord	(mlord@pobox.com)		(IDE Perf.Pkg)
 *	Delman Lee	(delman@ieee.org)		("Mr. atdisk2")
 *	Scott Snyder	(snyder@fnald0.fnal.gov)	(ATAPI IDE cd-rom)
 *
 *  This was a rewrite of just about everything from hd.c, though some original
 *  code is still sprinkled about.  Think of it as a major evolution, with
 *  inspiration from lots of linux users, esp.  hamish@zot.apana.org.au
 *
 *  Version 1.0 ALPHA	initial code, primary i/f working okay
 *  Version 1.3 BETA	dual i/f on shared irq tested & working!
 *  Version 1.4 BETA	added auto probing for irq(s)
 *  Version 1.5 BETA	added ALPHA (untested) support for IDE cd-roms,
 *  ...
 * Version 5.50		allow values as small as 20 for idebus=
 * Version 5.51		force non io_32bit in drive_cmd_intr()
 *			change delay_10ms() to delay_50ms() to fix problems
 * Version 5.52		fix incorrect invalidation of removable devices
 *			add "hdx=slow" command line option
 * Version 5.60		start to modularize the driver; the disk and ATAPI
 *			 drivers can be compiled as loadable modules.
 *			move IDE probe code to ide-probe.c
 *			move IDE disk code to ide-disk.c
 *			add support for generic IDE device subdrivers
 *			add m68k code from Geert Uytterhoeven
 *			probe all interfaces by default
 *			add ioctl to (re)probe an interface
 * Version 6.00		use per device request queues
 *			attempt to optimize shared hwgroup performance
 *			add ioctl to manually adjust bandwidth algorithms
 *			add kerneld support for the probe module
 *			fix bug in ide_error()
 *			fix bug in the first ide_get_lock() call for Atari
 *			don't flush leftover data for ATAPI devices
 * Version 6.01		clear hwgroup->active while the hwgroup sleeps
 *			support HDIO_GETGEO for floppies
 * Version 6.02		fix ide_ack_intr() call
 *			check partition table on floppies
 * Version 6.03		handle bad status bit sequencing in ide_wait_stat()
 * Version 6.10		deleted old entries from this list of updates
 *			replaced triton.c with ide-dma.c generic PCI DMA
 *			added support for BIOS-enabled UltraDMA
 *			rename all "promise" things to "pdc4030"
 *			fix EZ-DRIVE handling on small disks
 * Version 6.11		fix probe error in ide_scan_devices()
 *			fix ancient "jiffies" polling bugs
 *			mask all hwgroup interrupts on each irq entry
 * Version 6.12		integrate ioctl and proc interfaces
 *			fix parsing of "idex=" command line parameter
 * Version 6.13		add support for ide4/ide5 courtesy rjones@orchestream.com
 * Version 6.14		fixed IRQ sharing among PCI devices
 * Version 6.15		added SMP awareness to IDE drivers
 * Version 6.16		fixed various bugs; even more SMP friendly
 * Version 6.17		fix for newest EZ-Drive problem
 * Version 6.18		default unpartitioned-disk translation now "BIOS LBA"
 * Version 6.19		Re-design for a UNIFORM driver for all platforms,
 *			  model based on suggestions from Russell King and
 *			  Geert Uytterhoeven
 *			Promise DC4030VL now supported.
 *			add support for ide6/ide7
 *			delay_50ms() changed to ide_delay_50ms() and exported.
 * Version 6.20		Added/Fixed Generic ATA-66 support and hwif detection.
 *			Added hdx=flash to allow for second flash disk
 *			  detection w/o the hang loop.
 *			Added support for ide8/ide9
 *			Added idex=ata66 for the quirky chipsets that are
 *			  ATA-66 compliant, but have yet to determine a method
 *			  of verification of the 80c cable presence.
 *			  Specifically Promise's PDC20262 chipset.
 * Version 6.21		Fixing/Fixed SMP spinlock issue with insight from an old
 *			  hat that clarified original low level driver design.
 * Version 6.30		Added SMP support; fixed multmode issues.  -ml
 * Version 6.31		Debug Share INTR's and request queue streaming
 *			Native ATA-100 support
 *			Prep for Cascades Project
 * Version 7.00alpha	First named revision of ide rearrange
 * Version 7.00beta	(2.4 backport)
 *
 *  Some additional driver compile-time options are in ./include/linux/ide.h
 *
 */

#define	REVISION	"Revision: 7.00beta4-2.4"
#define	VERSION		"Id: ide.c 7.00b4 20030520"

#undef REALLY_SLOW_IO		/* most systems can safely undef this */

#define _IDE_C			/* Tell ide.h it's really us */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/genhd.h>
#include <linux/blkpg.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/completion.h>
#include <linux/reboot.h>

#include <asm/byteorder.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/bitops.h>

#include "ide_modes.h"

#include <linux/kmod.h>

/* default maximum number of failures */
#define IDE_DEFAULT_MAX_FAILURES 	1

static const u8 ide_hwif_to_major[] = { IDE0_MAJOR, IDE1_MAJOR,
					IDE2_MAJOR, IDE3_MAJOR,
					IDE4_MAJOR, IDE5_MAJOR,
					IDE6_MAJOR, IDE7_MAJOR,
					IDE8_MAJOR, IDE9_MAJOR };

static int idebus_parameter;	/* holds the "idebus=" parameter */
static int system_bus_speed;	/* holds what we think is VESA/PCI bus speed */
static int initializing;	/* set while initializing built-in drivers */

static int ide_scan_direction;	/* THIS was formerly 2.2.x pci=reverse */

#ifdef CONFIG_IDEDMA_AUTO
int noautodma = 0;
#else
int noautodma = 1;
#endif

EXPORT_SYMBOL(noautodma);


/*
 * ide_modules keeps track of the available IDE chipset/probe/driver modules.
 */
ide_module_t *ide_chipsets;
ide_module_t *ide_modules;
ide_module_t *ide_probe;

/*
 * This is declared extern in ide.h, for access by other IDE modules:
 */
ide_hwif_t ide_hwifs[MAX_HWIFS];	/* master data repository */

EXPORT_SYMBOL(ide_hwifs);

ide_devices_t *idedisk;
ide_devices_t *idecd;
ide_devices_t *idefloppy;
ide_devices_t *idetape;
ide_devices_t *idescsi;

EXPORT_SYMBOL(idedisk);
EXPORT_SYMBOL(idecd);
EXPORT_SYMBOL(idefloppy);
EXPORT_SYMBOL(idetape);
EXPORT_SYMBOL(idescsi);

extern ide_driver_t idedefault_driver;
static void setup_driver_defaults (ide_drive_t *drive);

/*
 * Do not even *think* about calling this!
 */
static void init_hwif_data (unsigned int index)
{
	unsigned int unit;
	hw_regs_t hw;
	ide_hwif_t *hwif = &ide_hwifs[index];

	/* bulk initialize hwif & drive info with zeros */
	memset(hwif, 0, sizeof(ide_hwif_t));
	memset(&hw, 0, sizeof(hw_regs_t));

	/* fill in any non-zero initial values */
	hwif->index     = index;
	ide_init_hwif_ports(&hw, ide_default_io_base(index), 0, &hwif->irq);
	memcpy(&hwif->hw, &hw, sizeof(hw));
	memcpy(hwif->io_ports, hw.io_ports, sizeof(hw.io_ports));
	hwif->noprobe	= !hwif->io_ports[IDE_DATA_OFFSET];
#ifdef CONFIG_BLK_DEV_HD
	if (hwif->io_ports[IDE_DATA_OFFSET] == HD_DATA)
		hwif->noprobe = 1; /* may be overridden by ide_setup() */
#endif /* CONFIG_BLK_DEV_HD */
	hwif->major	= ide_hwif_to_major[index];
	hwif->name[0]	= 'i';
	hwif->name[1]	= 'd';
	hwif->name[2]	= 'e';
	hwif->name[3]	= '0' + index;
	hwif->bus_state = BUSSTATE_ON;
	hwif->reset_poll= NULL;
	hwif->pre_reset = NULL;

	hwif->atapi_dma = 0;		/* disable all atapi dma */ 
	hwif->ultra_mask = 0x80;	/* disable all ultra */
	hwif->mwdma_mask = 0x80;	/* disable all mwdma */
	hwif->swdma_mask = 0x80;	/* disable all swdma */
	hwif->sata = 0;			/* assume PATA */

	default_hwif_iops(hwif);
	default_hwif_transport(hwif);
	for (unit = 0; unit < MAX_DRIVES; ++unit) {
		ide_drive_t *drive = &hwif->drives[unit];

		drive->media			= ide_disk;
		drive->select.all		= (unit<<4)|0xa0;
		drive->hwif			= hwif;
		drive->ctl			= 0x08;
		drive->ready_stat		= READY_STAT;
		drive->bad_wstat		= BAD_W_STAT;
		drive->special.b.recalibrate	= 1;
		drive->special.b.set_geometry	= 1;
		drive->name[0]			= 'h';
		drive->name[1]			= 'd';
		drive->name[2]			= 'a' + (index * MAX_DRIVES) + unit;
		drive->max_failures		= IDE_DEFAULT_MAX_FAILURES;
		drive->using_dma		= 0;
		drive->is_flash			= 0;
		drive->driver			= &idedefault_driver;
		setup_driver_defaults(drive);
		init_waitqueue_head(&drive->wqueue);
	}
}

/*
 * init_ide_data() sets reasonable default values into all fields
 * of all instances of the hwifs and drives, but only on the first call.
 * Subsequent calls have no effect (they don't wipe out anything).
 *
 * This routine is normally called at driver initialization time,
 * but may also be called MUCH earlier during kernel "command-line"
 * parameter processing.  As such, we cannot depend on any other parts
 * of the kernel (such as memory allocation) to be functioning yet.
 *
 * This is too bad, as otherwise we could dynamically allocate the
 * ide_drive_t structs as needed, rather than always consuming memory
 * for the max possible number (MAX_HWIFS * MAX_DRIVES) of them.
 *
 * FIXME: We should stuff the setup data into __init and copy the
 * relevant hwifs/allocate them properly during boot.
 */
#define MAGIC_COOKIE 0x12345678
static void __init init_ide_data (void)
{
	unsigned int index;
	static unsigned long magic_cookie = MAGIC_COOKIE;

	if (magic_cookie != MAGIC_COOKIE)
		return;		/* already initialized */
	magic_cookie = 0;

	/* Initialise all interface structures */
	for (index = 0; index < MAX_HWIFS; ++index)
		init_hwif_data(index);

	/* Add default hw interfaces */
	ide_init_default_hwifs();

	idebus_parameter = 0;
	system_bus_speed = 0;
}

/*
 * ide_system_bus_speed() returns what we think is the system VESA/PCI
 * bus speed (in MHz).  This is used for calculating interface PIO timings.
 * The default is 40 for known PCI systems, 50 otherwise.
 * The "idebus=xx" parameter can be used to override this value.
 * The actual value to be used is computed/displayed the first time through.
 */
int ide_system_bus_speed (void)
{
	if (!system_bus_speed) {
		if (idebus_parameter) {
			/* user supplied value */
			system_bus_speed = idebus_parameter;
		} else if (pci_present()) {
			/* safe default value for PCI */
			system_bus_speed = 33;
		} else {
			/* safe default value for VESA and PCI */
			system_bus_speed = 50;
		}
		printk(KERN_INFO "ide: Assuming %dMHz system bus speed "
			"for PIO modes%s\n", system_bus_speed,
			idebus_parameter ? "" : "; override with idebus=xx");
	}
	return system_bus_speed;
}

/*
 * current_capacity() returns the capacity (in sectors) of a drive
 * according to its current geometry/LBA settings.
 */
unsigned long current_capacity (ide_drive_t *drive)
{
	if (!drive->present)
		return 0;
	return DRIVER(drive)->capacity(drive);
}

EXPORT_SYMBOL(current_capacity);

static inline u32 read_24 (ide_drive_t *drive)
{
	return  (HWIF(drive)->INB(IDE_HCYL_REG)<<16) |
		(HWIF(drive)->INB(IDE_LCYL_REG)<<8) |
		 HWIF(drive)->INB(IDE_SECTOR_REG);
}

/*
 * Error reporting, in human readable form (luxurious, but a memory hog).
 */
u8 ide_dump_status (ide_drive_t *drive, const char *msg, u8 stat)
{
	ide_hwif_t *hwif = HWIF(drive);
	unsigned long flags;
	u8 err = 0;

	local_irq_set(flags);
	printk(KERN_WARNING "%s: %s: status=0x%02x", drive->name, msg, stat);
#if FANCY_STATUS_DUMPS
	printk(" { ");
	if (stat & BUSY_STAT) {
		printk("Busy ");
	} else {
		if (stat & READY_STAT)	printk("DriveReady ");
		if (stat & WRERR_STAT)	printk("DeviceFault ");
		if (stat & SEEK_STAT)	printk("SeekComplete ");
		if (stat & DRQ_STAT)	printk("DataRequest ");
		if (stat & ECC_STAT)	printk("CorrectedError ");
		if (stat & INDEX_STAT)	printk("Index ");
		if (stat & ERR_STAT)	printk("Error ");
	}
	printk("}");
#endif	/* FANCY_STATUS_DUMPS */
	printk("\n");
	if ((stat & (BUSY_STAT|ERR_STAT)) == ERR_STAT) {
		err = hwif->INB(IDE_ERROR_REG);
		printk("%s: %s: error=0x%02x", drive->name, msg, err);
#if FANCY_STATUS_DUMPS
		if (drive->media == ide_disk) {
			printk(" { ");
			if (err & ABRT_ERR)	printk("DriveStatusError ");
			if (err & ICRC_ERR)	printk("Bad%s ", (err & ABRT_ERR) ? "CRC" : "Sector");
			if (err & ECC_ERR)	printk("UncorrectableError ");
			if (err & ID_ERR)	printk("SectorIdNotFound ");
			if (err & TRK0_ERR)	printk("TrackZeroNotFound ");
			if (err & MARK_ERR)	printk("AddrMarkNotFound ");
			printk("}");
			if ((err & (BBD_ERR | ABRT_ERR)) == BBD_ERR || (err & (ECC_ERR|ID_ERR|MARK_ERR))) {
				if ((drive->id->command_set_2 & 0x0400) &&
				    (drive->id->cfs_enable_2 & 0x0400) &&
				    (drive->addressing == 1)) {
					u64 sectors = 0;
					u32 high = 0;
					u32 low = read_24(drive);
					hwif->OUTB(drive->ctl|0x80, IDE_CONTROL_REG);
					high = read_24(drive);

					sectors = ((u64)high << 24) | low;
					printk(", LBAsect=%llu, high=%d, low=%d",
					       (u64) sectors,
					       high, low);
				} else {
					u8 cur = hwif->INB(IDE_SELECT_REG);
					if (cur & 0x40) {	/* using LBA? */
						printk(", LBAsect=%ld", (unsigned long)
						 ((cur&0xf)<<24)
						 |(hwif->INB(IDE_HCYL_REG)<<16)
						 |(hwif->INB(IDE_LCYL_REG)<<8)
						 | hwif->INB(IDE_SECTOR_REG));
					} else {
						printk(", CHS=%d/%d/%d",
						 (hwif->INB(IDE_HCYL_REG)<<8) +
						  hwif->INB(IDE_LCYL_REG),
						  cur & 0xf,
						  hwif->INB(IDE_SECTOR_REG));
					}
				}
				if (HWGROUP(drive) && HWGROUP(drive)->rq)
					printk(", sector=%ld", HWGROUP(drive)->rq->sector);
			}
		}
#endif	/* FANCY_STATUS_DUMPS */
		printk("\n");
	}
	local_irq_restore(flags);
	return err;
}

EXPORT_SYMBOL(ide_dump_status);


/*
 * This routine is called to flush all partitions and partition tables
 * for a changed disk, and then re-read the new partition table.
 * If we are revalidating a disk because of a media change, then we
 * enter with usage == 0.  If we are using an ioctl, we automatically have
 * usage == 1 (we need an open channel to use an ioctl :-), so this
 * is our limit.
 */
int ide_revalidate_disk (kdev_t i_rdev)
{
	ide_drive_t *drive;
	ide_hwgroup_t *hwgroup;
	unsigned int p, major, minor;
	unsigned long flags;

	if ((drive = ide_info_ptr(i_rdev, 0)) == NULL)
		return -ENODEV;
	major = MAJOR(i_rdev);
	minor = drive->select.b.unit << PARTN_BITS;
	hwgroup = HWGROUP(drive);
	spin_lock_irqsave(&io_request_lock, flags);
	if (drive->busy || (drive->usage > 1)) {
		spin_unlock_irqrestore(&io_request_lock, flags);
		return -EBUSY;
	};
	drive->busy = 1;
	MOD_INC_USE_COUNT;
	spin_unlock_irqrestore(&io_request_lock, flags);

	for (p = 0; p < (1<<PARTN_BITS); ++p) {
		if (drive->part[p].nr_sects > 0) {
			kdev_t devp = MKDEV(major, minor+p);
			invalidate_device(devp, 1);
		}
		drive->part[p].start_sect = 0;
		drive->part[p].nr_sects   = 0;
	};

	if (DRIVER(drive)->revalidate)
		DRIVER(drive)->revalidate(drive);

	drive->busy = 0;
	wake_up(&drive->wqueue);
	MOD_DEC_USE_COUNT;
	return 0;
}

EXPORT_SYMBOL(ide_revalidate_disk);

static void revalidate_drives (int revaldiate)
{
	ide_hwif_t *hwif;
	ide_drive_t *drive;
	int index, unit;

	for (index = 0; index < MAX_HWIFS; ++index) {
		hwif = &ide_hwifs[index];
		for (unit = 0; unit < MAX_DRIVES; ++unit) {
			drive = &ide_hwifs[index].drives[unit];
			if (drive->revalidate) {
				drive->revalidate = 0;
				if ((!initializing) && (revaldiate))
					(void) ide_revalidate_disk(MKDEV(hwif->major, unit<<PARTN_BITS));
			}
		}
	}
}

void ide_probe_module (int revaldiate)
{
	if (!ide_probe) {
#if  defined(CONFIG_BLK_DEV_IDE_MODULE)
		(void) request_module("ide-probe-mod");
#endif
	} else {
		(void) ide_probe->init();
	}
	revalidate_drives(revaldiate);
}

EXPORT_SYMBOL(ide_probe_module);

void ide_driver_module (int revaldiate)
{
	int index;
	ide_module_t *module = ide_modules;

	for (index = 0; index < MAX_HWIFS; ++index)
		if (ide_hwifs[index].present)
			goto search;
	ide_probe_module(revaldiate);
search:
	while (module) {
		(void) module->init();
		module = module->next;
	}
	revalidate_drives(revaldiate);
}

EXPORT_SYMBOL(ide_driver_module);

static int ide_open (struct inode * inode, struct file * filp)
{
	ide_drive_t *drive;
	int force = 1/*FIXME 0*/;
	
	if(capable(CAP_SYS_ADMIN) && (filp->f_flags & O_NDELAY))
		force = 1;

	if ((drive = ide_info_ptr(inode->i_rdev, force)) == NULL)
		return -ENXIO;
	
	/*
	 *	If the device is present make sure that we attach any
	 *	needed driver
	 */	

	if (drive->present)
	{
		if (drive->driver == &idedefault_driver)
			ide_driver_module(1);
		if (drive->driver == &idedefault_driver) {
			if (drive->media == ide_disk)
				(void) request_module("ide-disk");
			if (drive->scsi)
				(void) request_module("ide-scsi");
			if (drive->media == ide_cdrom)
				(void) request_module("ide-cd");
			if (drive->media == ide_tape)
				(void) request_module("ide-tape");
			if (drive->media == ide_floppy)
				(void) request_module("ide-floppy");
		}
	
		/* The locking here isnt enough, but this is hard to fix
		   in the 2.4 cases */
		while (drive->busy)
			sleep_on(&drive->wqueue);
	}
	
	/*
	 *	Now do the actual open
	 */
	 
	drive->usage++;
	if (!drive->dead || force)
		return DRIVER(drive)->open(inode, filp, drive);
	printk(KERN_WARNING "%s: driver not present\n", drive->name);
	drive->usage--;
	return -ENXIO;
}

/*
 * Releasing a block device means we sync() it, so that it can safely
 * be forgotten about...
 */
static int ide_release (struct inode * inode, struct file * file)
{
	ide_drive_t *drive;

	if ((drive = ide_info_ptr(inode->i_rdev, 1)) != NULL) {
		drive->usage--;
		DRIVER(drive)->release(inode, file, drive);
	}
	return 0;
}

#ifdef CONFIG_PROC_FS
ide_proc_entry_t generic_subdriver_entries[] = {
	{ "capacity",	S_IFREG|S_IRUGO,	proc_ide_read_capacity,	NULL },
	{ NULL, 0, NULL, NULL }
};
#endif


#define hwif_release_region(addr, num) \
	((hwif->mmio) ? release_mem_region((addr),(num)) : release_region((addr),(num)))

/*
 * Note that we only release the standard ports,
 * and do not even try to handle any extra ports
 * allocated for weird IDE interface chipsets.
 */
void hwif_unregister (ide_hwif_t *hwif)
{
	u32 i = 0;

	if (hwif->mmio == 2)
		return;
	if (hwif->io_ports[IDE_CONTROL_OFFSET])
		hwif_release_region(hwif->io_ports[IDE_CONTROL_OFFSET], 1);
#if defined(CONFIG_AMIGA) || defined(CONFIG_MAC)
	if (hwif->io_ports[IDE_IRQ_OFFSET])
		hwif_release_region(hwif->io_ports[IDE_IRQ_OFFSET], 1);
#endif /* (CONFIG_AMIGA) || (CONFIG_MAC) */

	if (hwif->straight8) {
		hwif_release_region(hwif->io_ports[IDE_DATA_OFFSET], 8);
		return;
	}
	for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++) {
		if (hwif->io_ports[i]) {
			hwif_release_region(hwif->io_ports[i], 1);
		}
	}
}

EXPORT_SYMBOL(hwif_unregister);

extern void init_hwif_data(unsigned int index);

/**
 *	ide_prepare_tristate	-	prepare interface for warm unplug
 *	@drive: drive on this hwif we are using
 *
 *	Prepares a drive for shutdown after a bus tristate. The
 *	drives must be quiescent and the only user the calling ioctl
 */
 
static int ide_prepare_tristate(ide_drive_t *our_drive)
{
	ide_drive_t *drive;
	int unit;
	unsigned long flags;
	int minor;
	int p;
	int i;
	ide_hwif_t *hwif = HWIF(our_drive);
		
	if(our_drive->busy)
		printk("HUH? We are busy.\n");
		
	if (!hwif->present)
		BUG();
	spin_lock_irqsave(&io_request_lock, flags);
	
	/* Abort if anything is busy */
	for (unit = 0; unit < MAX_DRIVES; ++unit) {
		drive = &hwif->drives[unit];
		if (!drive->present)
			continue;
		if (drive == our_drive && drive->usage != 1)
			goto abort;
		if (drive != our_drive && drive->usage)
			goto abort;
		if (drive->busy)
			goto abort;
	}
	/* Commit to shutdown sequence */
	for (unit = 0; unit < MAX_DRIVES; ++unit) {
		drive = &hwif->drives[unit];
		if (!drive->present)
			continue;
		if (drive != our_drive && DRIVER(drive)->shutdown(drive))
			goto abort;
	}
	/* We hold the lock here.. which is important as we need to play
	   with usage counts beyond the scenes */
	   
	our_drive->usage--;
	i = DRIVER(our_drive)->shutdown(our_drive);
	if(i)
		goto abort_fix;
	/* Drive shutdown sequence done */
	/* Prevent new opens ?? */
	spin_unlock_irqrestore(&io_request_lock, flags);
	/*
	 * Flush kernel side caches, and dump the /proc files
	 */
	spin_unlock_irqrestore(&io_request_lock, flags);
	for (unit = 0; unit < MAX_DRIVES; ++unit) {
		drive = &hwif->drives[unit];
		if (!drive->present)
			continue;
		DRIVER(drive)->cleanup(drive);
		minor = drive->select.b.unit << PARTN_BITS;
		for (p = 0; p < (1<<PARTN_BITS); ++p) {
			if (drive->part[p].nr_sects > 0) {
				kdev_t devp = MKDEV(hwif->major, minor+p);
				invalidate_device(devp, 0);
			}
		}
#ifdef CONFIG_PROC_FS
		destroy_proc_ide_drives(hwif);
#endif
	}
	spin_lock_irqsave(&io_request_lock, flags);
	our_drive->usage++;
	for (i = 0; i < MAX_DRIVES; ++i) {
		drive = &hwif->drives[i];
		if (drive->de) {
			devfs_unregister(drive->de);
			drive->de = NULL;
		}
		if (!drive->present)
			continue;
		if (drive->id != NULL) {
			kfree(drive->id);
			drive->id = NULL;
		}
		drive->present = 0;
		/* Safe to clear now */
		drive->dead = 0;
	}
	spin_unlock_irqrestore(&io_request_lock, flags);
	return 0;

abort_fix:
	our_drive->usage++;
abort:
	spin_unlock_irqrestore(&io_request_lock, flags);
	return -EBUSY;
}


/**
 *	ide_resume_hwif		-	return a hwif to active mode
 *	@hwif: interface to resume
 *	
 *	Restore a dead interface from tristate back to normality. At this
 *	point the hardware driver busproc has reconnected the bus, but
 *	nothing else has happened
 */
 
static int ide_resume_hwif(ide_drive_t *our_drive)
{
	ide_hwif_t *hwif = HWIF(our_drive);
	int err = ide_wait_hwif_ready(hwif);
	int irqd;
	int present = 0;
	int unit;
		
	if(err)
	{
		printk(KERN_ERR "%s: drives not ready.\n", our_drive->name);
		return err;
	}
		
	/* The drives are now taking commands */
	
	irqd = hwif->irq;
	if(irqd)
		disable_irq(irqd);
		
	/* Identify and probe the drives */
	
	for (unit = 0; unit < MAX_DRIVES; ++unit) {
		ide_drive_t *drive = &hwif->drives[unit];
		drive->dn = ((hwif->channel ? 2 : 0) + unit);
		drive->usage = 0;
		drive->busy = 0;
		hwif->drives[unit].dn = ((hwif->channel ? 2 : 0) + unit);
		(void) ide_probe_for_drive(drive);
		if (drive->present)
			present = 1;
	}
	ide_probe_reset(hwif);
	if(irqd)
		enable_irq(irqd);
	
	if(present)
		printk(KERN_INFO "ide: drives found on hot-added interface.\n");
			
	/*
	 *	Set up the drive modes (Even if we didnt swap drives
	 *	we may have lost settings when we disconnected the bus)
	 */
	 
	ide_tune_drives(hwif);
	if(present)
		hwif->present = 1;
		
	/*
	 *	Reattach the devices to drivers
	 */
	for (unit = 0; unit < MAX_DRIVES; ++unit) {
		ide_drive_t *drive = &hwif->drives[unit];
		if(drive->present && !drive->dead)
			ide_attach_drive(drive);
	}
	our_drive->usage++;
	return 0;
}

int ide_unregister (unsigned int index)
{
	struct gendisk *gd;
	ide_drive_t *drive, *d;
	ide_hwif_t *hwif, *g;
	ide_hwgroup_t *hwgroup;
	int irq_count = 0, unit, i;
	unsigned long flags;
	unsigned int p, minor;
	ide_hwif_t old_hwif;

	if (index >= MAX_HWIFS)
		BUG();
		
	spin_lock_irqsave(&io_request_lock, flags);
	hwif = &ide_hwifs[index];
	if (!hwif->present)
		goto abort;
	for (unit = 0; unit < MAX_DRIVES; ++unit) {
		drive = &hwif->drives[unit];
		if (!drive->present)
			continue;
		if (drive->busy || drive->usage)
			goto abort;
		if (DRIVER(drive)->shutdown(drive))
			goto abort;
	}
	hwif->present = 0;
	
	/*
	 * All clear?  Then blow away the buffer cache
	 */
	spin_unlock_irqrestore(&io_request_lock, flags);
	for (unit = 0; unit < MAX_DRIVES; ++unit) {
		drive = &hwif->drives[unit];
		if (!drive->present)
			continue;
		DRIVER(drive)->cleanup(drive);
		minor = drive->select.b.unit << PARTN_BITS;
		for (p = 0; p < (1<<PARTN_BITS); ++p) {
			if (drive->part[p].nr_sects > 0) {
				kdev_t devp = MKDEV(hwif->major, minor+p);
				invalidate_device(devp, 0);
			}
		}
#ifdef CONFIG_PROC_FS
		destroy_proc_ide_drives(hwif);
#endif
	}

	spin_lock_irqsave(&io_request_lock, flags);
	hwgroup = hwif->hwgroup;

	/*
	 * free the irq if we were the only hwif using it
	 */
	g = hwgroup->hwif;
	do {
		if (g->irq == hwif->irq)
			++irq_count;
		g = g->next;
	} while (g != hwgroup->hwif);
	if (irq_count == 1)
		free_irq(hwif->irq, hwgroup);

	/*
	 * Note that we only release the standard ports,
	 * and do not even try to handle any extra ports
	 * allocated for weird IDE interface chipsets.
	 */
	hwif_unregister(hwif);

	/*
	 * Remove us from the hwgroup, and free
	 * the hwgroup if we were the only member
	 */
	d = hwgroup->drive;
	for (i = 0; i < MAX_DRIVES; ++i) {
		drive = &hwif->drives[i];
		if (drive->de) {
			devfs_unregister(drive->de);
			drive->de = NULL;
		}
		if (!drive->present)
			continue;
		while (hwgroup->drive->next != drive)
			hwgroup->drive = hwgroup->drive->next;
		hwgroup->drive->next = drive->next;
		if (hwgroup->drive == drive)
			hwgroup->drive = NULL;
		if (drive->id != NULL) {
			kfree(drive->id);
			drive->id = NULL;
		}
		drive->present = 0;
		blk_cleanup_queue(&drive->queue);
	}
	if (d->present)
		hwgroup->drive = d;
	while (hwgroup->hwif->next != hwif)
		hwgroup->hwif = hwgroup->hwif->next;
	hwgroup->hwif->next = hwif->next;
	if (hwgroup->hwif == hwif)
		kfree(hwgroup);
	else
		hwgroup->hwif = HWIF(hwgroup->drive);

#if !defined(CONFIG_DMA_NONPCI)
	if (hwif->dma_base) {
		(void) ide_release_dma(hwif);

		hwif->dma_base = 0;
		hwif->dma_master = 0;
		hwif->dma_command = 0;
		hwif->dma_vendor1 = 0;
		hwif->dma_status = 0;
		hwif->dma_vendor3 = 0;
		hwif->dma_prdtable = 0;
	}
#endif /* !(CONFIG_DMA_NONPCI) */

	/*
	 * Remove us from the kernel's knowledge
	 */
	unregister_blkdev(hwif->major, hwif->name);
	kfree(blksize_size[hwif->major]);
	kfree(max_sectors[hwif->major]);
	kfree(max_readahead[hwif->major]);
	blk_dev[hwif->major].data = NULL;
	blk_dev[hwif->major].queue = NULL;
	blksize_size[hwif->major] = NULL;
	gd = hwif->gd;
	if (gd) {
		del_gendisk(gd);
		kfree(gd->sizes);
		kfree(gd->part);
		if (gd->de_arr)
			kfree(gd->de_arr);
		if (gd->flags)
			kfree(gd->flags);
		kfree(gd);
		hwif->gd = NULL;
	}

	old_hwif			= *hwif;
	init_hwif_data(index);	/* restore hwif data to pristine status */
	hwif->hwgroup			= old_hwif.hwgroup;

	hwif->proc			= old_hwif.proc;

	hwif->major			= old_hwif.major;
//	hwif->index			= old_hwif.index;
//	hwif->channel			= old_hwif.channel;
	hwif->straight8			= old_hwif.straight8;
	hwif->bus_state			= old_hwif.bus_state;

	hwif->atapi_dma			= old_hwif.atapi_dma;
	hwif->ultra_mask		= old_hwif.ultra_mask;
	hwif->mwdma_mask		= old_hwif.mwdma_mask;
	hwif->swdma_mask		= old_hwif.swdma_mask;

	hwif->chipset			= old_hwif.chipset;
	hwif->hold			= old_hwif.hold;

#ifdef CONFIG_BLK_DEV_IDEPCI
	hwif->pci_dev			= old_hwif.pci_dev;
	hwif->cds			= old_hwif.cds;
#endif /* CONFIG_BLK_DEV_IDEPCI */

#if 0
	hwif->hwifops			= old_hwif.hwifops;
#else
	hwif->identify			= old_hwif.identify;
	hwif->tuneproc			= old_hwif.tuneproc;
	hwif->speedproc			= old_hwif.speedproc;
	hwif->selectproc		= old_hwif.selectproc;
	hwif->reset_poll		= old_hwif.reset_poll;
	hwif->pre_reset			= old_hwif.pre_reset;
	hwif->resetproc			= old_hwif.resetproc;
	hwif->intrproc			= old_hwif.intrproc;
	hwif->maskproc			= old_hwif.maskproc;
	hwif->quirkproc			= old_hwif.quirkproc;
	hwif->busproc			= old_hwif.busproc;
#endif

#if 0
	hwif->pioops			= old_hwif.pioops;
#else
	hwif->ata_input_data		= old_hwif.ata_input_data;
	hwif->ata_output_data		= old_hwif.ata_output_data;
	hwif->atapi_input_bytes		= old_hwif.atapi_input_bytes;
	hwif->atapi_output_bytes	= old_hwif.atapi_output_bytes;
#endif

#if 0
	hwif->dmaops			= old_hwif.dmaops;
#else
	hwif->ide_dma_read		= old_hwif.ide_dma_read;
	hwif->ide_dma_write		= old_hwif.ide_dma_write;
	hwif->ide_dma_begin		= old_hwif.ide_dma_begin;
	hwif->ide_dma_end		= old_hwif.ide_dma_end;
	hwif->ide_dma_check		= old_hwif.ide_dma_check;
	hwif->ide_dma_on		= old_hwif.ide_dma_on;
	hwif->ide_dma_off		= old_hwif.ide_dma_off;
	hwif->ide_dma_off_quietly	= old_hwif.ide_dma_off_quietly;
	hwif->ide_dma_test_irq		= old_hwif.ide_dma_test_irq;
	hwif->ide_dma_host_on		= old_hwif.ide_dma_host_on;
	hwif->ide_dma_host_off		= old_hwif.ide_dma_host_off;
	hwif->ide_dma_bad_drive		= old_hwif.ide_dma_bad_drive;
	hwif->ide_dma_good_drive	= old_hwif.ide_dma_good_drive;
	hwif->ide_dma_count		= old_hwif.ide_dma_count;
	hwif->ide_dma_verbose		= old_hwif.ide_dma_verbose;
	hwif->ide_dma_retune		= old_hwif.ide_dma_retune;
	hwif->ide_dma_lostirq		= old_hwif.ide_dma_lostirq;
	hwif->ide_dma_timeout		= old_hwif.ide_dma_timeout;
#endif

#if 0
	hwif->iops			= old_hwif.iops;
#else
	hwif->OUTB		= old_hwif.OUTB;
	hwif->OUTBSYNC		= old_hwif.OUTBSYNC;
	hwif->OUTW		= old_hwif.OUTW;
	hwif->OUTL		= old_hwif.OUTL;
	hwif->OUTSW		= old_hwif.OUTSW;
	hwif->OUTSL		= old_hwif.OUTSL;

	hwif->INB		= old_hwif.INB;
	hwif->INW		= old_hwif.INW;
	hwif->INL		= old_hwif.INL;
	hwif->INSW		= old_hwif.INSW;
	hwif->INSL		= old_hwif.INSL;
#endif

	hwif->mmio			= old_hwif.mmio;
	hwif->rqsize			= old_hwif.rqsize;
	hwif->addressing		= old_hwif.addressing;
#ifndef CONFIG_BLK_DEV_IDECS
	hwif->irq			= old_hwif.irq;
#endif /* CONFIG_BLK_DEV_IDECS */
	hwif->initializing		= old_hwif.initializing;

	hwif->dma_base			= old_hwif.dma_base;
	hwif->dma_master		= old_hwif.dma_master;
	hwif->dma_command		= old_hwif.dma_command;
	hwif->dma_vendor1		= old_hwif.dma_vendor1;
	hwif->dma_status		= old_hwif.dma_status;
	hwif->dma_vendor3		= old_hwif.dma_vendor3;
	hwif->dma_prdtable		= old_hwif.dma_prdtable;

	hwif->dma_extra			= old_hwif.dma_extra;
	hwif->config_data		= old_hwif.config_data;
	hwif->select_data		= old_hwif.select_data;
	hwif->autodma			= old_hwif.autodma;
	hwif->udma_four			= old_hwif.udma_four;
	hwif->no_dsc			= old_hwif.no_dsc;

	hwif->hwif_data			= old_hwif.hwif_data;
	spin_unlock_irqrestore(&io_request_lock, flags);
	return 0;

abort:
	spin_unlock_irqrestore(&io_request_lock, flags);
	return 1;
	
}

EXPORT_SYMBOL(ide_unregister);

/*
 * Setup hw_regs_t structure described by parameters.  You
 * may set up the hw structure yourself OR use this routine to
 * do it for you.
 */
void ide_setup_ports (	hw_regs_t *hw,
			ide_ioreg_t base, int *offsets,
			ide_ioreg_t ctrl, ide_ioreg_t intr,
			ide_ack_intr_t *ack_intr,
/*
 *			ide_io_ops_t *iops,
 */
			int irq)
{
	int i;

	for (i = 0; i < IDE_NR_PORTS; i++) {
		if (offsets[i] == -1) {
			switch(i) {
				case IDE_CONTROL_OFFSET:
					hw->io_ports[i] = ctrl;
					break;
#if defined(CONFIG_AMIGA) || defined(CONFIG_MAC)
				case IDE_IRQ_OFFSET:
					hw->io_ports[i] = intr;
					break;
#endif /* (CONFIG_AMIGA) || (CONFIG_MAC) */
				default:
					hw->io_ports[i] = 0;
					break;
			}
		} else {
			hw->io_ports[i] = base + offsets[i];
		}
	}
	hw->irq = irq;
	hw->dma = NO_DMA;
	hw->ack_intr = ack_intr;
/*
 *	hw->iops = iops;
 */
}

EXPORT_SYMBOL(ide_setup_ports);

/*
 * Register an IDE interface, specifing exactly the registers etc
 * Set init=1 iff calling before probes have taken place.
 */
int ide_register_hw (hw_regs_t *hw, ide_hwif_t **hwifp)
{
	int index, retry = 1;
	ide_hwif_t *hwif;

	do {
		for (index = 0; index < MAX_HWIFS; ++index) {
			hwif = &ide_hwifs[index];
			if (hwif->hw.io_ports[IDE_DATA_OFFSET] == hw->io_ports[IDE_DATA_OFFSET])
				goto found;
		}
		for (index = 0; index < MAX_HWIFS; ++index) {
			hwif = &ide_hwifs[index];
			if (!hwif->hold && ((!hwif->present && !hwif->mate && !initializing) ||
			    (!hwif->hw.io_ports[IDE_DATA_OFFSET] && initializing)))
				goto found;
		}
		for (index = 0; index < MAX_HWIFS; index++)
			ide_unregister(index);
	} while (retry--);
	return -1;
found:
	if (hwif->present)
		ide_unregister(index);
	else if (!hwif->hold)
		init_hwif_data(index);
	if (hwif->present)
		return -1;
	memcpy(&hwif->hw, hw, sizeof(*hw));
	memcpy(hwif->io_ports, hwif->hw.io_ports, sizeof(hwif->hw.io_ports));
	hwif->irq = hw->irq;
	hwif->noprobe = 0;
	hwif->chipset = hw->chipset;

	if (!initializing) {
		ide_probe_module(1);
#ifdef CONFIG_PROC_FS
		create_proc_ide_interfaces();
#endif
		ide_driver_module(1);
	}

	if (hwifp)
		*hwifp = hwif;

	return (initializing || hwif->present) ? index : -1;
}

EXPORT_SYMBOL(ide_register_hw);

/*
 * Compatability function with existing drivers.  If you want
 * something different, use the function above.
 */
int ide_register (int arg1, int arg2, int irq)
{
	hw_regs_t hw;
	ide_init_hwif_ports(&hw, (ide_ioreg_t) arg1, (ide_ioreg_t) arg2, NULL);
	hw.irq = irq;
	return ide_register_hw(&hw, NULL);
}

EXPORT_SYMBOL(ide_register);


/*
 *	Locks for IDE setting functionality
 */

DECLARE_MUTEX(ide_setting_sem);
EXPORT_SYMBOL(ide_setting_sem);

/**
 *	ide_add_setting	-	add an ide setting option
 *	@drive: drive to use
 *	@name: setting name
 *	@rw: true if the function is read write
 *	@read_ioctl: function to call on read
 *	@write_ioctl: function to call on write
 *	@data_type: type of data
 *	@min: range minimum
 *	@max: range maximum
 *	@mul_factor: multiplication scale
 *	@div_factor: divison scale
 *	@data: private data field
 *	@set: setting
 *
 *	Removes the setting named from the device if it is present.
 *	The function takes the settings_lock to protect against 
 *	parallel changes. This function must not be called from IRQ
 *	context. Returns 0 on success or -1 on failure.
 *
 *	BUGS: This code is seriously over-engineered. There is also
 *	magic about how the driver specific features are setup. If
 *	a driver is attached we assume the driver settings are auto
 *	remove.
 */
 
int ide_add_setting (ide_drive_t *drive, const char *name, int rw, int read_ioctl, int write_ioctl, int data_type, int min, int max, int mul_factor, int div_factor, void *data, ide_procset_t *set)
{
	ide_settings_t **p = (ide_settings_t **) &drive->settings, *setting = NULL;

	down(&ide_setting_sem);
	while ((*p) && strcmp((*p)->name, name) < 0)
		p = &((*p)->next);
	if ((setting = kmalloc(sizeof(*setting), GFP_KERNEL)) == NULL)
		goto abort;
	memset(setting, 0, sizeof(*setting));
	if ((setting->name = kmalloc(strlen(name) + 1, GFP_KERNEL)) == NULL)
		goto abort;
	strcpy(setting->name, name);
	setting->rw = rw;
	setting->read_ioctl = read_ioctl;
	setting->write_ioctl = write_ioctl;
	setting->data_type = data_type;
	setting->min = min;
	setting->max = max;
	setting->mul_factor = mul_factor;
	setting->div_factor = div_factor;
	setting->data = data;
	setting->set = set;
	
	setting->next = *p;
	if (drive->driver != &idedefault_driver)
		setting->auto_remove = 1;
	*p = setting;
	up(&ide_setting_sem);
	return 0;
abort:
	up(&ide_setting_sem);
	if (setting)
		kfree(setting);
	return -1;
}

EXPORT_SYMBOL(ide_add_setting);

/**
 *	__ide_remove_setting	-	remove an ide setting option
 *	@drive: drive to use
 *	@name: setting name
 *
 *	Removes the setting named from the device if it is present.
 *	The caller must hold the setting semaphore.
 */
 
static void __ide_remove_setting (ide_drive_t *drive, char *name)
{
	ide_settings_t **p, *setting;

	p = (ide_settings_t **) &drive->settings;

	while ((*p) && strcmp((*p)->name, name))
		p = &((*p)->next);
	if ((setting = (*p)) == NULL)
		return;

	(*p) = setting->next;
	
	kfree(setting->name);
	kfree(setting);
}

/**
 *	ide_remove_setting	-	remove an ide setting option
 *	@drive: drive to use
 *	@name: setting name
 *
 *	Removes the setting named from the device if it is present.
 *	The function takes the settings_lock to protect against 
 *	parallel changes. This function must not be called from IRQ
 *	context.
 */
 
void ide_remove_setting (ide_drive_t *drive, char *name)
{
	down(&ide_setting_sem);
	__ide_remove_setting(drive, name);
	up(&ide_setting_sem);
}

EXPORT_SYMBOL(ide_remove_setting);

/**
 *	ide_find_setting_by_ioctl	-	find a drive specific ioctl
 *	@drive: drive to scan
 *	@cmd: ioctl command to handle
 *
 *	Scan's the device setting table for a matching entry and returns
 *	this or NULL if no entry is found. The caller must hold the
 *	setting semaphore
 */
 
static ide_settings_t *ide_find_setting_by_ioctl (ide_drive_t *drive, int cmd)
{
	ide_settings_t *setting = drive->settings;

	while (setting) {
		if (setting->read_ioctl == cmd || setting->write_ioctl == cmd)
			break;
		setting = setting->next;
	}
	
	return setting;
}

/**
 *	ide_find_setting_by_name	-	find a drive specific setting
 *	@drive: drive to scan
 *	@name: setting name
 *
 *	Scan's the device setting table for a matching entry and returns
 *	this or NULL if no entry is found. The caller must hold the
 *	setting semaphore
 */
 
ide_settings_t *ide_find_setting_by_name (ide_drive_t *drive, char *name)
{
	ide_settings_t *setting = drive->settings;

	while (setting) {
		if (strcmp(setting->name, name) == 0)
			break;
		setting = setting->next;
	}
	return setting;
}

/**
 *	auto_remove_settings	-	remove driver specific settings
 *	@drive: drive
 *
 *	Automatically remove all the driver specific settings for this
 *	drive. This function may sleep and must not be called from IRQ
 *	context. Caller must hold the setting lock.
 */
 
static void auto_remove_settings (ide_drive_t *drive)
{
	ide_settings_t *setting;
repeat:
	setting = drive->settings;
	while (setting) {
		if (setting->auto_remove) {
			__ide_remove_setting(drive, setting->name);
			goto repeat;
		}
		setting = setting->next;
	}
}

/**
 *	ide_read_setting	-	read an IDE setting
 *	@drive: drive to read from
 *	@setting: drive setting
 *
 *	Read a drive setting and return the value. The caller
 *	must hold the ide_setting_sem when making this call.
 *
 *	BUGS: the data return and error are the same return value
 *	so an error -EINVAL and true return of the same value cannot
 *	be told apart
 */
 
int ide_read_setting (ide_drive_t *drive, ide_settings_t *setting)
{
	int		val = -EINVAL;
	unsigned long	flags;

	if ((setting->rw & SETTING_READ)) {
		spin_lock_irqsave(&io_request_lock, flags);
		switch(setting->data_type) {
			case TYPE_BYTE:
				val = *((u8 *) setting->data);
				break;
			case TYPE_SHORT:
				val = *((u16 *) setting->data);
				break;
			case TYPE_INT:
			case TYPE_INTA:
				val = *((u32 *) setting->data);
				break;
		}
		spin_unlock_irqrestore(&io_request_lock, flags);
	}
	return val;
}

int ide_spin_wait_hwgroup (ide_drive_t *drive)
{
	ide_hwgroup_t *hwgroup = HWGROUP(drive);
	unsigned long timeout = jiffies + (3 * HZ);

	spin_lock_irq(&io_request_lock);

	while (hwgroup->busy) {
		unsigned long lflags;
		spin_unlock_irq(&io_request_lock);
		local_irq_set(lflags);
		if (time_after(jiffies, timeout)) {
			local_irq_restore(lflags);
			printk(KERN_ERR "%s: channel busy\n", drive->name);
			return -EBUSY;
		}
		local_irq_restore(lflags);
		spin_lock_irq(&io_request_lock);
	}
	return 0;
}

EXPORT_SYMBOL(ide_spin_wait_hwgroup);

/**
 *	ide_write_setting	-	read an IDE setting
 *	@drive: drive to read from
 *	@setting: drive setting
 *	@val: value
 *
 *	Write a drive setting if it is possible. The caller
 *	must hold the ide_setting_sem when making this call.
 *
 *	BUGS: the data return and error are the same return value
 *	so an error -EINVAL and true return of the same value cannot
 *	be told apart
 *
 *	FIXME:  This should be changed to enqueue a special request
 *	to the driver to change settings, and then wait on a sema for completion.
 *	The current scheme of polling is kludgy, though safe enough.
 */
int ide_write_setting (ide_drive_t *drive, ide_settings_t *setting, int val)
{
	int i;
	u32 *p;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;
	if (!(setting->rw & SETTING_WRITE))
		return -EPERM;
	if (val < setting->min || val > setting->max)
		return -EINVAL;
	if (setting->set)
		return setting->set(drive, val);
	if (ide_spin_wait_hwgroup(drive))
		return -EBUSY;
	switch (setting->data_type) {
		case TYPE_BYTE:
			*((u8 *) setting->data) = val;
			break;
		case TYPE_SHORT:
			*((u16 *) setting->data) = val;
			break;
		case TYPE_INT:
			*((u32 *) setting->data) = val;
			break;
		case TYPE_INTA:
			p = (u32 *) setting->data;
			for (i = 0; i < 1 << PARTN_BITS; i++, p++)
				*p = val;
			break;
	}
	spin_unlock_irq(&io_request_lock);
	return 0;
}

EXPORT_SYMBOL(ide_write_setting);

static int set_io_32bit(ide_drive_t *drive, int arg)
{
	drive->io_32bit = arg;
#ifdef CONFIG_BLK_DEV_DTC2278
	if (HWIF(drive)->chipset == ide_dtc2278)
		HWIF(drive)->drives[!drive->select.b.unit].io_32bit = arg;
#endif /* CONFIG_BLK_DEV_DTC2278 */
	return 0;
}

static int set_using_dma (ide_drive_t *drive, int arg)
{
	if (!DRIVER(drive)->supports_dma)
		return -EPERM;
	if (!(drive->id->capability & 1))
		return -EPERM;
	if (HWIF(drive)->ide_dma_check == NULL)
		return -EPERM;
	if (HWIF(drive)->ide_dma_check(drive) != 0)
		return -EIO;
	if (arg) {
		if (HWIF(drive)->ide_dma_check(drive) != 0)
			return -EIO;
		if (HWIF(drive)->ide_dma_on(drive)) return -EIO;
	} else {
		if (HWIF(drive)->ide_dma_off(drive)) return -EIO;
	}
	return 0;
}

static int set_pio_mode (ide_drive_t *drive, int arg)
{
	struct request rq;

	if (!HWIF(drive)->tuneproc)
		return -ENOSYS;
	if (drive->special.b.set_tune)
		return -EBUSY;
	ide_init_drive_cmd(&rq);
	drive->tune_req = (u8) arg;
	drive->special.b.set_tune = 1;
	(void) ide_do_drive_cmd(drive, &rq, ide_wait);
	return 0;
}

static int set_xfer_rate (ide_drive_t *drive, int arg)
{
	int err = ide_wait_cmd(drive,
			WIN_SETFEATURES, (u8) arg,
			SETFEATURES_XFER, 0, NULL);

	if (!err && arg) {
		ide_set_xfer_rate(drive, (u8) arg);
		ide_driveid_update(drive);
	}
	return err;
}

int ide_atapi_to_scsi (ide_drive_t *drive, int arg)
{
	if (drive->media == ide_disk) {
		drive->scsi = 0;
		return 0;
	}
	if (DRIVER(drive)->cleanup(drive)) {
		drive->scsi = 0;
		return 0;
	}
	drive->scsi = (u8) arg;
	ide_attach_drive(drive);
	return 0;
}

void ide_add_generic_settings (ide_drive_t *drive)
{
/*
 *			drive	setting name		read/write access				read ioctl		write ioctl		data type	min	max				mul_factor	div_factor	data pointer			set function
 */
	ide_add_setting(drive,	"io_32bit",		drive->no_io_32bit ? SETTING_READ : SETTING_RW,	HDIO_GET_32BIT,		HDIO_SET_32BIT,		TYPE_BYTE,	0,	1 + (SUPPORT_VLB_SYNC << 1),	1,		1,		&drive->io_32bit,		set_io_32bit);
	ide_add_setting(drive,	"keepsettings",		SETTING_RW,					HDIO_GET_KEEPSETTINGS,	HDIO_SET_KEEPSETTINGS,	TYPE_BYTE,	0,	1,				1,		1,		&drive->keep_settings,		NULL);
	ide_add_setting(drive,	"nice1",		SETTING_RW,					-1,			-1,			TYPE_BYTE,	0,	1,				1,		1,		&drive->nice1,			NULL);
	ide_add_setting(drive,	"pio_mode",		SETTING_WRITE,					-1,			HDIO_SET_PIO_MODE,	TYPE_BYTE,	0,	255,				1,		1,		NULL,				set_pio_mode);
	ide_add_setting(drive,	"slow",			SETTING_RW,					-1,			-1,			TYPE_BYTE,	0,	1,				1,		1,		&drive->slow,			NULL);
	ide_add_setting(drive,	"unmaskirq",		drive->no_unmask ? SETTING_READ : SETTING_RW,	HDIO_GET_UNMASKINTR,	HDIO_SET_UNMASKINTR,	TYPE_BYTE,	0,	1,				1,		1,		&drive->unmask,			NULL);
	ide_add_setting(drive,	"using_dma",		SETTING_RW,					HDIO_GET_DMA,		HDIO_SET_DMA,		TYPE_BYTE,	0,	1,				1,		1,		&drive->using_dma,		set_using_dma);
	ide_add_setting(drive,	"init_speed",		SETTING_RW,					-1,			-1,			TYPE_BYTE,	0,	70,				1,		1,		&drive->init_speed,		NULL);
	ide_add_setting(drive,	"current_speed",	SETTING_RW,					-1,			-1,			TYPE_BYTE,	0,	70,				1,		1,		&drive->current_speed,		set_xfer_rate);
	ide_add_setting(drive,	"number",		SETTING_RW,					-1,			-1,			TYPE_BYTE,	0,	3,				1,		1,		&drive->dn,			NULL);
#if 0
	/* Experimental, but this needs the setting/register locking rewritten to be used */	
	if (drive->media != ide_disk)
		ide_add_setting(drive,	"ide-scsi",		SETTING_RW,					-1,		HDIO_SET_IDE_SCSI,		TYPE_BYTE,	0,	1,				1,		1,		&drive->scsi,			ide_atapi_to_scsi);
#endif		
}

EXPORT_SYMBOL_GPL(ide_add_generic_settings);

/*
 * Delay for *at least* 50ms.  As we don't know how much time is left
 * until the next tick occurs, we wait an extra tick to be safe.
 * This is used only during the probing/polling for drives at boot time.
 *
 * However, its usefullness may be needed in other places, thus we export it now.
 * The future may change this to a millisecond setable delay.
 */
void ide_delay_50ms (void)
{
#ifndef CONFIG_BLK_DEV_IDECS
	mdelay(50);
#else
	__set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(1+HZ/20);
#endif /* CONFIG_BLK_DEV_IDECS */
}

EXPORT_SYMBOL(ide_delay_50ms);

int system_bus_clock (void)
{
	return((int) ((!system_bus_speed) ? ide_system_bus_speed() : system_bus_speed ));
}

EXPORT_SYMBOL(system_bus_clock);

int ide_replace_subdriver (ide_drive_t *drive, const char *driver)
{
	if (!drive->present || drive->busy || drive->usage || drive->dead)
		goto abort;
	if (DRIVER(drive)->cleanup(drive))
		goto abort;
	strncpy(drive->driver_req, driver, 9);
	ide_driver_module(0);
	drive->driver_req[0] = 0;
	ide_driver_module(0);
	if (!strcmp(DRIVER(drive)->name, driver))
		return 0;
abort:
	return 1;
}

EXPORT_SYMBOL(ide_replace_subdriver);

int ide_attach_drive (ide_drive_t *drive)
{
	/* Someone unplugged the device on us */
	if(drive->dead)
		return 1;
		
#ifdef CONFIG_BLK_DEV_IDESCSI
	if (drive->scsi) {
		extern int idescsi_attach(ide_drive_t *drive);
		if (idescsi_attach(drive))
			return 0;
	}
#endif /* CONFIG_BLK_DEV_IDESCSI */

	switch (drive->media) {
#ifdef CONFIG_BLK_DEV_IDECD
		case ide_cdrom:
		{
			extern int ide_cdrom_attach(ide_drive_t *drive);
			if (ide_cdrom_attach(drive))
				return 1;
			break;
		}
#endif /* CONFIG_BLK_DEV_IDECD */
#ifdef CONFIG_BLK_DEV_IDEDISK
		case ide_disk:
		{
			extern int idedisk_attach(ide_drive_t *drive);
			if (idedisk_attach(drive))
				return 1;
			break;
		}
#endif /* CONFIG_BLK_DEV_IDEDISK */
#ifdef CONFIG_BLK_DEV_IDEFLOPPY
		case ide_floppy:
		{
			extern int idefloppy_attach(ide_drive_t *drive);
			if (idefloppy_attach(drive))
				return 1;
			break;
		}
#endif /* CONFIG_BLK_DEV_IDEFLOPPY */
#ifdef CONFIG_BLK_DEV_IDETAPE
		case ide_tape:
		{
			extern int idetape_attach(ide_drive_t *drive);
			if (idetape_attach(drive))
				return 1;
			break;
		}
#endif /* CONFIG_BLK_DEV_IDETAPE */
		default:
		{
			extern int idedefault_attach(ide_drive_t *drive);
			if(idedefault_attach(drive))
				printk(KERN_CRIT "ide: failed to attach default driver.\n");
			return 1;
		}
	}
	return 0;
}

EXPORT_SYMBOL(ide_attach_drive);

static int ide_ioctl (struct inode *inode, struct file *file,
			unsigned int cmd, unsigned long arg)
{
	int err = 0, major, minor;
	ide_drive_t *drive;
	struct request rq;
	kdev_t dev;
	ide_settings_t *setting;
	int force = 0;
	
	if (!inode || !(dev = inode->i_rdev))
		return -EINVAL;
		
	switch(cmd)
	{
		case HDIO_GET_BUSSTATE:
		case HDIO_SET_BUSSTATE:
		case HDIO_SCAN_HWIF:
		case HDIO_UNREGISTER_HWIF:
			force = 1;
	}
	
	major = MAJOR(dev); minor = MINOR(dev);
	if ((drive = ide_info_ptr(inode->i_rdev, force)) == NULL)
		return -ENODEV;

	down(&ide_setting_sem);
	if ((setting = ide_find_setting_by_ioctl(drive, cmd)) != NULL) {
		if (cmd == setting->read_ioctl) {
			err = ide_read_setting(drive, setting);
			up(&ide_setting_sem);
			return err >= 0 ? put_user(err, (long *) arg) : err;
		} else {
			if ((MINOR(inode->i_rdev) & PARTN_MASK))
				err = -EINVAL;
			else 
				err = ide_write_setting(drive, setting, arg);
			up(&ide_setting_sem);
			return err;
		}
	}
	up(&ide_setting_sem);

	ide_init_drive_cmd (&rq);
	switch (cmd) {
		case HDIO_GETGEO:
		{
			struct hd_geometry *loc = (struct hd_geometry *) arg;
			u16 bios_cyl = drive->bios_cyl; /* truncate */
			if (!loc || (drive->media != ide_disk && drive->media != ide_floppy)) return -EINVAL;
			if (put_user(drive->bios_head, (u8 *) &loc->heads)) return -EFAULT;
			if (put_user(drive->bios_sect, (u8 *) &loc->sectors)) return -EFAULT;
			if (put_user(bios_cyl, (u16 *) &loc->cylinders)) return -EFAULT;
			if (put_user((unsigned)drive->part[MINOR(inode->i_rdev)&PARTN_MASK].start_sect,
				(unsigned long *) &loc->start)) return -EFAULT;
			return 0;
		}

		case HDIO_GETGEO_BIG:
		{
			struct hd_big_geometry *loc = (struct hd_big_geometry *) arg;
			if (!loc || (drive->media != ide_disk && drive->media != ide_floppy)) return -EINVAL;
			if (put_user(drive->bios_head, (u8 *) &loc->heads)) return -EFAULT;
			if (put_user(drive->bios_sect, (u8 *) &loc->sectors)) return -EFAULT;
			if (put_user(drive->bios_cyl, (unsigned int *) &loc->cylinders)) return -EFAULT;
			if (put_user((unsigned)drive->part[MINOR(inode->i_rdev)&PARTN_MASK].start_sect,
				(unsigned long *) &loc->start)) return -EFAULT;
			return 0;
		}

		case HDIO_GETGEO_BIG_RAW:
		{
			struct hd_big_geometry *loc = (struct hd_big_geometry *) arg;
			if (!loc || (drive->media != ide_disk && drive->media != ide_floppy)) return -EINVAL;
			if (put_user(drive->head, (u8 *) &loc->heads)) return -EFAULT;
			if (put_user(drive->sect, (u8 *) &loc->sectors)) return -EFAULT;
			if (put_user(drive->cyl, (unsigned int *) &loc->cylinders)) return -EFAULT;
			if (put_user((unsigned)drive->part[MINOR(inode->i_rdev)&PARTN_MASK].start_sect,
				(unsigned long *) &loc->start)) return -EFAULT;
			return 0;
		}

	 	case BLKGETSIZE:   /* Return device size */
			return put_user(drive->part[MINOR(inode->i_rdev)&PARTN_MASK].nr_sects, (unsigned long *) arg);
	 	case BLKGETSIZE64:
			return put_user((u64)drive->part[MINOR(inode->i_rdev)&PARTN_MASK].nr_sects << 9, (u64 *) arg);

		case BLKRRPART: /* Re-read partition tables */
			if (!capable(CAP_SYS_ADMIN)) return -EACCES;
			return ide_revalidate_disk(inode->i_rdev);

		case HDIO_OBSOLETE_IDENTITY:
		case HDIO_GET_IDENTITY:
			if (MINOR(inode->i_rdev) & PARTN_MASK)
				return -EINVAL;
			if (drive->id_read == 0)
				return -ENOMSG;
			if (copy_to_user((char *)arg, (char *)drive->id, (cmd == HDIO_GET_IDENTITY) ? sizeof(*drive->id) : 142))
				return -EFAULT;
			return 0;

		case HDIO_GET_NICE:
			return put_user(drive->dsc_overlap	<<	IDE_NICE_DSC_OVERLAP	|
					drive->atapi_overlap	<<	IDE_NICE_ATAPI_OVERLAP	|
					drive->nice0		<< 	IDE_NICE_0		|
					drive->nice1		<<	IDE_NICE_1		|
					drive->nice2		<<	IDE_NICE_2,
					(long *) arg);

#ifdef CONFIG_IDE_TASK_IOCTL
		case HDIO_DRIVE_TASKFILE:
		        if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RAWIO))
				return -EACCES;
			switch(drive->media) {
				case ide_disk:
					return ide_taskfile_ioctl(drive, inode, file, cmd, arg);
#ifdef CONFIG_PKT_TASK_IOCTL
				case ide_cdrom:
				case ide_tape:
				case ide_floppy:
					return pkt_taskfile_ioctl(drive, inode, file, cmd, arg);
#endif /* CONFIG_PKT_TASK_IOCTL */
				default:
					return -ENOMSG;
			}
#endif /* CONFIG_IDE_TASK_IOCTL */

		case HDIO_DRIVE_CMD:
			if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RAWIO))
				return -EACCES;
			return ide_cmd_ioctl(drive, inode, file, cmd, arg);

		case HDIO_DRIVE_TASK:
			if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RAWIO))
				return -EACCES;
			return ide_task_ioctl(drive, inode, file, cmd, arg);

		case HDIO_SCAN_HWIF:
		{
			int args[3];
			if (!capable(CAP_SYS_RAWIO)) return -EACCES;
			if (copy_from_user(args, (void *)arg, 3 * sizeof(int)))
				return -EFAULT;
			if (ide_register(args[0], args[1], args[2]) == -1)
				return -EIO;
			return 0;
		}
	        case HDIO_UNREGISTER_HWIF:
			if (!capable(CAP_SYS_RAWIO)) return -EACCES;
			/* (arg > MAX_HWIFS) checked in function */
			ide_unregister(arg);
			return 0;
		case HDIO_SET_NICE:
			if (!capable(CAP_SYS_ADMIN)) return -EACCES;
			if (arg != (arg & ((1 << IDE_NICE_DSC_OVERLAP) | (1 << IDE_NICE_1))))
				return -EPERM;
			drive->dsc_overlap = (arg >> IDE_NICE_DSC_OVERLAP) & 1;
			if (drive->dsc_overlap && !DRIVER(drive)->supports_dsc_overlap) {
				drive->dsc_overlap = 0;
				return -EPERM;
			}
			drive->nice1 = (arg >> IDE_NICE_1) & 1;
			return 0;
		case HDIO_DRIVE_RESET:
		{
			unsigned long flags;
			if (!capable(CAP_SYS_ADMIN)) return -EACCES;
			
			/*
			 *	Abort the current command on the
			 *	group if there is one, taking
			 *	care not to allow anything else
			 *	to be queued and to die on the
			 *	spot if we miss one somehow
			 */

			spin_lock_irqsave(&io_request_lock, flags);
			
			DRIVER(drive)->abort(drive, "drive reset");
			if(HWGROUP(drive)->handler)
				BUG();
				
			/* Ensure nothing gets queued after we
			   drop the lock. Reset will clear the busy */
			   
			HWGROUP(drive)->busy = 1;
			spin_unlock_irqrestore(&io_request_lock, flags);

			(void) ide_do_reset(drive);
			if (drive->suspend_reset) {
/*
 *				APM WAKE UP todo !!
 *				int nogoodpower = 1;
 *				while(nogoodpower) {
 *					check_power1() or check_power2()
 *					nogoodpower = 0;
 *				} 
 *				HWIF(drive)->multiproc(drive);
 */
				return ide_revalidate_disk(inode->i_rdev);
			}
			return 0;
		}
		case BLKROSET:
		case BLKROGET:
		case BLKFLSBUF:
		case BLKSSZGET:
		case BLKPG:
		case BLKELVGET:
		case BLKELVSET:
		case BLKBSZGET:
		case BLKBSZSET:
			return blk_ioctl(inode->i_rdev, cmd, arg);

		case HDIO_GET_BUSSTATE:
			if (!capable(CAP_SYS_ADMIN))
				return -EACCES;
			if (put_user(HWIF(drive)->bus_state, (long *)arg))
				return -EFAULT;
			return 0;

		case HDIO_SET_BUSSTATE:
		{
			ide_hwif_t *hwif =  HWIF(drive);
			
			if (!capable(CAP_SYS_ADMIN))
				return -EACCES;
#ifdef OLD_STUFF
			if (hwif->busproc)
				return HWIF(drive)->busproc(drive, (int)arg);
			return -EOPNOTSUPP;
#else
			if(hwif->bus_state == arg)
				return 0;
				
			if(hwif->bus_state == BUSSTATE_ON)
			{
				/* "drive" may vanish beyond here */
				if((err = ide_prepare_tristate(drive)) != 0)
					return err;
				hwif->bus_state = arg;
			}
			if (hwif->busproc)
			{
				err = hwif->busproc(drive, (int)arg);
				if(err)
					return err;
			}
			if(arg != BUSSTATE_OFF)
			{
				err = ide_resume_hwif(drive);
				hwif->bus_state = arg;
				if(err)
					return err;
			}
			return 0;
#endif			
		}				

		default:
			return DRIVER(drive)->ioctl(drive, inode, file, cmd, arg);
			return -EPERM;
	}
}

static int ide_check_media_change (kdev_t i_rdev)
{
	ide_drive_t *drive;

	if ((drive = ide_info_ptr(i_rdev, 0)) == NULL)
		return -ENODEV;
	return DRIVER(drive)->media_change(drive);
}

/*
 * stridx() returns the offset of c within s,
 * or -1 if c is '\0' or not found within s.
 */
static int __init stridx (const char *s, char c)
{
	char *i = strchr(s, c);
	return (i && c) ? i - s : -1;
}

/*
 * match_parm() does parsing for ide_setup():
 *
 * 1. the first char of s must be '='.
 * 2. if the remainder matches one of the supplied keywords,
 *     the index (1 based) of the keyword is negated and returned.
 * 3. if the remainder is a series of no more than max_vals numbers
 *     separated by commas, the numbers are saved in vals[] and a
 *     count of how many were saved is returned.  Base10 is assumed,
 *     and base16 is allowed when prefixed with "0x".
 * 4. otherwise, zero is returned.
 */
static int __init match_parm (char *s, const char *keywords[], int vals[], int max_vals)
{
	static const char *decimal = "0123456789";
	static const char *hex = "0123456789abcdef";
	int i, n;

	if (*s++ == '=') {
		/*
		 * Try matching against the supplied keywords,
		 * and return -(index+1) if we match one
		 */
		if (keywords != NULL) {
			for (i = 0; *keywords != NULL; ++i) {
				if (!strcmp(s, *keywords++))
					return -(i+1);
			}
		}
		/*
		 * Look for a series of no more than "max_vals"
		 * numeric values separated by commas, in base10,
		 * or base16 when prefixed with "0x".
		 * Return a count of how many were found.
		 */
		for (n = 0; (i = stridx(decimal, *s)) >= 0;) {
			vals[n] = i;
			while ((i = stridx(decimal, *++s)) >= 0)
				vals[n] = (vals[n] * 10) + i;
			if (*s == 'x' && !vals[n]) {
				while ((i = stridx(hex, *++s)) >= 0)
					vals[n] = (vals[n] * 0x10) + i;
			}
			if (++n == max_vals)
				break;
			if (*s == ',' || *s == ';')
				++s;
		}
		if (!*s)
			return n;
	}
	return 0;	/* zero = nothing matched */
}

/*
 * ide_setup() gets called VERY EARLY during initialization,
 * to handle kernel "command line" strings beginning with "hdx="
 * or "ide".  Here is the complete set currently supported:
 *
 * "hdx="  is recognized for all "x" from "a" to "h", such as "hdc".
 * "idex=" is recognized for all "x" from "0" to "3", such as "ide1".
 *
 * "hdx=noprobe"	: drive may be present, but do not probe for it
 * "hdx=none"		: drive is NOT present, ignore cmos and do not probe
 * "hdx=nowerr"		: ignore the WRERR_STAT bit on this drive
 * "hdx=cdrom"		: drive is present, and is a cdrom drive
 * "hdx=cyl,head,sect"	: disk drive is present, with specified geometry
 * "hdx=noremap"	: do not remap 0->1 even though EZD was detected
 * "hdx=autotune"	: driver will attempt to tune interface speed
 *				to the fastest PIO mode supported,
 *				if possible for this drive only.
 *				Not fully supported by all chipset types,
 *				and quite likely to cause trouble with
 *				older/odd IDE drives.
 *
 * "hdx=slow"		: insert a huge pause after each access to the data
 *				port. Should be used only as a last resort.
 *
 * "hdx=swapdata"	: when the drive is a disk, byte swap all data
 * "hdx=bswap"		: same as above..........
 * "hdxlun=xx"          : set the drive last logical unit.
 * "hdx=flash"		: allows for more than one ata_flash disk to be
 *				registered. In most cases, only one device
 *				will be present.
 * "hdx=scsi"		: the return of the ide-scsi flag, this is useful for
 *				allowwing ide-floppy, ide-tape, and ide-cdrom|writers
 *				to use ide-scsi emulation on a device specific option.
 * "idebus=xx"		: inform IDE driver of VESA/PCI bus speed in MHz,
 *				where "xx" is between 20 and 66 inclusive,
 *				used when tuning chipset PIO modes.
 *				For PCI bus, 25 is correct for a P75 system,
 *				30 is correct for P90,P120,P180 systems,
 *				and 33 is used for P100,P133,P166 systems.
 *				If in doubt, use idebus=33 for PCI.
 *				As for VLB, it is safest to not specify it.
 *
 * "idex=noprobe"	: do not attempt to access/use this interface
 * "idex=base"		: probe for an interface at the addr specified,
 *				where "base" is usually 0x1f0 or 0x170
 *				and "ctl" is assumed to be "base"+0x206
 * "idex=base,ctl"	: specify both base and ctl
 * "idex=base,ctl,irq"	: specify base, ctl, and irq number
 * "idex=autotune"	: driver will attempt to tune interface speed
 *				to the fastest PIO mode supported,
 *				for all drives on this interface.
 *				Not fully supported by all chipset types,
 *				and quite likely to cause trouble with
 *				older/odd IDE drives.
 * "idex=noautotune"	: driver will NOT attempt to tune interface speed
 *				This is the default for most chipsets,
 *				except the cmd640.
 * "idex=serialize"	: do not overlap operations on idex and ide(x^1)
 * "idex=four"		: four drives on idex and ide(x^1) share same ports
 * "idex=reset"		: reset interface before first use
 * "idex=dma"		: enable DMA by default on both drives if possible
 * "idex=ata66"		: informs the interface that it has an 80c cable
 *				for chipsets that are ATA-66 capable, but
 *				the ablity to bit test for detection is
 *				currently unknown.
 * "ide=reverse"	: Formerly called to pci sub-system, but now local.
 *
 * The following are valid ONLY on ide0, (except dc4030)
 * and the defaults for the base,ctl ports must not be altered.
 *
 * "ide0=dtc2278"	: probe/support DTC2278 interface
 * "ide0=ht6560b"	: probe/support HT6560B interface
 * "ide0=cmd640_vlb"	: *REQUIRED* for VLB cards with the CMD640 chip
 *			  (not for PCI -- automatically detected)
 * "ide0=qd65xx"	: probe/support qd65xx interface
 * "ide0=ali14xx"	: probe/support ali14xx chipsets (ALI M1439, M1443, M1445)
 * "ide0=umc8672"	: probe/support umc8672 chipsets
 * "idex=dc4030"	: probe/support Promise DC4030VL interface
 * "ide=doubler"	: probe/support IDE doublers on Amiga
 */
int __init ide_setup (char *s)
{
	int i, vals[3];
	ide_hwif_t *hwif;
	ide_drive_t *drive;
	unsigned int hw, unit;
	const char max_drive = 'a' + ((MAX_HWIFS * MAX_DRIVES) - 1);
	const char max_hwif  = '0' + (MAX_HWIFS - 1);

	
	if (strncmp(s,"hd",2) == 0 && s[2] == '=')	/* hd= is for hd.c   */
		return 0;				/* driver and not us */

	if (strncmp(s,"ide",3) &&
	    strncmp(s,"idebus",6) &&
	    strncmp(s,"hd",2))		/* hdx= & hdxlun= */
		return 0;

	printk(KERN_INFO "ide_setup: %s", s);
	init_ide_data ();

#ifdef CONFIG_BLK_DEV_IDEDOUBLER
	if (!strcmp(s, "ide=doubler")) {
		extern int ide_doubler;

		printk(" : Enabled support for IDE doublers\n");
		ide_doubler = 1;
		return 1;
	}
#endif /* CONFIG_BLK_DEV_IDEDOUBLER */

	if (!strcmp(s, "ide=nodma")) {
		printk(" : Prevented DMA\n");
		noautodma = 1;
		return 1;
	}

#ifdef CONFIG_BLK_DEV_IDEPCI
	if (!strcmp(s, "ide=reverse")) {
		ide_scan_direction = 1;
		printk(" : Enabled support for IDE inverse scan order.\n");
		return 1;
	}
#endif /* CONFIG_BLK_DEV_IDEPCI */

	/*
	 * Look for drive options:  "hdx="
	 */
	if (s[0] == 'h' && s[1] == 'd' && s[2] >= 'a' && s[2] <= max_drive) {
		const char *hd_words[] = {"none", "noprobe", "nowerr", "cdrom",
				"serialize", "autotune", "noautotune",
				"slow", "swapdata", "bswap", "flash",
				"remap", "noremap", "scsi", NULL};
		unit = s[2] - 'a';
		hw   = unit / MAX_DRIVES;
		unit = unit % MAX_DRIVES;
		hwif = &ide_hwifs[hw];
		drive = &hwif->drives[unit];
		if (strncmp(s + 4, "ide-", 4) == 0) {
			strncpy(drive->driver_req, s + 4, 9);
			goto done;
		}
		/*
		 * Look for last lun option:  "hdxlun="
		 */
		if (s[3] == 'l' && s[4] == 'u' && s[5] == 'n') {
			if (match_parm(&s[6], NULL, vals, 1) != 1)
				goto bad_option;
			if (vals[0] >= 0 && vals[0] <= 7) {
				drive->last_lun = vals[0];
				drive->forced_lun = 1;
			} else
				printk(" -- BAD LAST LUN! Expected value from 0 to 7");
			goto done;
		}
		switch (match_parm(&s[3], hd_words, vals, 3)) {
			case -1: /* "none" */
				drive->nobios = 1;  /* drop into "noprobe" */
			case -2: /* "noprobe" */
				drive->noprobe = 1;
				goto done;
			case -3: /* "nowerr" */
				drive->bad_wstat = BAD_R_STAT;
				hwif->noprobe = 0;
				goto done;
			case -4: /* "cdrom" */
				drive->present = 1;
				drive->media = ide_cdrom;
				hwif->noprobe = 0;
				goto done;
			case -5: /* "serialize" */
				printk(" -- USE \"ide%d=serialize\" INSTEAD", hw);
				goto do_serialize;
			case -6: /* "autotune" */
				drive->autotune = 1;
				goto done;
			case -7: /* "noautotune" */
				drive->autotune = 2;
				goto done;
			case -8: /* "slow" */
				drive->slow = 1;
				goto done;
			case -9: /* "swapdata" or "bswap" */
			case -10:
				drive->bswap = 1;
				goto done;
			case -11: /* "flash" */
				drive->ata_flash = 1;
				goto done;
			case -12: /* "remap" */
				drive->remap_0_to_1 = 1;
				goto done;
			case -13: /* "noremap" */
				drive->remap_0_to_1 = 2;
				goto done;
			case -14: /* "scsi" */
				drive->scsi = 1;
				goto done;
			case 3: /* cyl,head,sect */
				drive->media	= ide_disk;
				drive->cyl	= drive->bios_cyl  = vals[0];
				drive->head	= drive->bios_head = vals[1];
				drive->sect	= drive->bios_sect = vals[2];
				drive->present	= 1;
				drive->forced_geom = 1;
				hwif->noprobe = 0;
				goto done;
			default:
				goto bad_option;
		}
	}

	if (s[0] != 'i' || s[1] != 'd' || s[2] != 'e')
		goto bad_option;
	/*
	 * Look for bus speed option:  "idebus="
	 */
	if (s[3] == 'b' && s[4] == 'u' && s[5] == 's') {
		if (match_parm(&s[6], NULL, vals, 1) != 1)
			goto bad_option;
		if (vals[0] >= 20 && vals[0] <= 66) {
			idebus_parameter = vals[0];
		} else
			printk(" -- BAD BUS SPEED! Expected value from 20 to 66");
		goto done;
	}
	/*
	 * Look for interface options:  "idex="
	 */
	if (s[3] >= '0' && s[3] <= max_hwif) {
		/*
		 * Be VERY CAREFUL changing this: note hardcoded indexes below
		 * -8,-9,-10 : are reserved for future idex calls to ease the hardcoding.
		 */
		const char *ide_words[] = {
			"noprobe", "serialize", "autotune", "noautotune", "reset", "dma", "ata66",
			"minus8", "minus9", "minus10",
			"four", "qd65xx", "ht6560b", "cmd640_vlb", "dtc2278", "umc8672", "ali14xx", "dc4030", NULL };
		hw = s[3] - '0';
		hwif = &ide_hwifs[hw];
		i = match_parm(&s[4], ide_words, vals, 3);

		/*
		 * Cryptic check to ensure chipset not already set for hwif:
		 */
		if (i > 0 || i <= -11) {			/* is parameter a chipset name? */
			if (hwif->chipset != ide_unknown)
				goto bad_option;	/* chipset already specified */
			if (i <= -11 && i != -18 && hw != 0)
				goto bad_hwif;		/* chipset drivers are for "ide0=" only */
			if (i <= -11 && i != -18 && ide_hwifs[hw+1].chipset != ide_unknown)
				goto bad_option;	/* chipset for 2nd port already specified */
			printk("\n");
		}

		switch (i) {
#ifdef CONFIG_BLK_DEV_PDC4030
			case -18: /* "dc4030" */
			{
				extern void init_pdc4030(void);
				init_pdc4030();
				goto done;
			}
#endif /* CONFIG_BLK_DEV_PDC4030 */
#ifdef CONFIG_BLK_DEV_ALI14XX
			case -17: /* "ali14xx" */
			{
				extern void init_ali14xx (void);
				init_ali14xx();
				goto done;
			}
#endif /* CONFIG_BLK_DEV_ALI14XX */
#ifdef CONFIG_BLK_DEV_UMC8672
			case -16: /* "umc8672" */
			{
				extern void init_umc8672 (void);
				init_umc8672();
				goto done;
			}
#endif /* CONFIG_BLK_DEV_UMC8672 */
#ifdef CONFIG_BLK_DEV_DTC2278
			case -15: /* "dtc2278" */
			{
				extern void init_dtc2278 (void);
				init_dtc2278();
				goto done;
			}
#endif /* CONFIG_BLK_DEV_DTC2278 */
#ifdef CONFIG_BLK_DEV_CMD640
			case -14: /* "cmd640_vlb" */
			{
				extern void init_cmd640_vlb (void);
				init_cmd640_vlb();
				goto done;
			}
#endif /* CONFIG_BLK_DEV_CMD640 */
#ifdef CONFIG_BLK_DEV_HT6560B
			case -13: /* "ht6560b" */
			{
				extern void init_ht6560b (void);
				init_ht6560b();
				goto done;
			}
#endif /* CONFIG_BLK_DEV_HT6560B */
#if CONFIG_BLK_DEV_QD65XX
			case -12: /* "qd65xx" */
			{
				extern void init_qd65xx (void);
				init_qd65xx();
				goto done;
			}
#endif /* CONFIG_BLK_DEV_QD65XX */
#ifdef CONFIG_BLK_DEV_4DRIVES
			case -11: /* "four" drives on one set of ports */
			{
				ide_hwif_t *mate = &ide_hwifs[hw^1];
				mate->drives[0].select.all ^= 0x20;
				mate->drives[1].select.all ^= 0x20;
				hwif->chipset = mate->chipset = ide_4drives;
				mate->irq = hwif->irq;
				memcpy(mate->io_ports, hwif->io_ports, sizeof(hwif->io_ports));
				goto do_serialize;
			}
#endif /* CONFIG_BLK_DEV_4DRIVES */
			case -10: /* minus10 */
			case -9: /* minus9 */
			case -8: /* minus8 */
				goto bad_option;
			case -7: /* ata66 */
#ifdef CONFIG_BLK_DEV_IDEPCI
				hwif->udma_four = 1;
				goto done;
#else /* !CONFIG_BLK_DEV_IDEPCI */
				hwif->udma_four = 0;
				goto bad_hwif;
#endif /* CONFIG_BLK_DEV_IDEPCI */
			case -6: /* dma */
				hwif->autodma = 1;
				goto done;
			case -5: /* "reset" */
				hwif->reset = 1;
				goto done;
			case -4: /* "noautotune" */
				hwif->drives[0].autotune = 2;
				hwif->drives[1].autotune = 2;
				goto done;
			case -3: /* "autotune" */
				hwif->drives[0].autotune = 1;
				hwif->drives[1].autotune = 1;
				goto done;
			case -2: /* "serialize" */
			do_serialize:
				hwif->mate = &ide_hwifs[hw^1];
				hwif->mate->mate = hwif;
				hwif->serialized = hwif->mate->serialized = 1;
				goto done;

			case -1: /* "noprobe" */
				hwif->noprobe = 1;
				goto done;

			case 1:	/* base */
				vals[1] = vals[0] + 0x206; /* default ctl */
			case 2: /* base,ctl */
				vals[2] = 0;	/* default irq = probe for it */
			case 3: /* base,ctl,irq */
				hwif->hw.irq = vals[2];
				ide_init_hwif_ports(&hwif->hw, (ide_ioreg_t) vals[0], (ide_ioreg_t) vals[1], &hwif->irq);
				memcpy(hwif->io_ports, hwif->hw.io_ports, sizeof(hwif->io_ports));
				hwif->irq      = vals[2];
				hwif->noprobe  = 0;
				hwif->chipset  = ide_generic;
				goto done;

			case 0: goto bad_option;
			default:
				printk(" -- SUPPORT NOT CONFIGURED IN THIS KERNEL\n");
				return 1;
		}
	}
bad_option:
	printk(" -- BAD OPTION\n");
	return 1;
bad_hwif:
	printk("-- NOT SUPPORTED ON ide%d", hw);
done:
	printk("\n");
	return 1;
}

/*
 *	Legacy interface registration
 */
#define NUM_DRIVER		32

/*
 *	Yes this is moronically simple, but why not - it works!
 */

static __initdata ide_driver_call ide_scan[NUM_DRIVER];
static int drivers_run = 0;

void __init ide_register_driver(ide_driver_call scan)
{
	static int ide_scans = 0;
	if(ide_scans == NUM_DRIVER)
		panic("Too many IDE drivers");
	ide_scan[ide_scans++]=scan;
	if(drivers_run)
	{
		printk(KERN_ERR "ide: late registration of driver.\n");
		scan();
	}
}

EXPORT_SYMBOL(ide_register_driver);

static void __init ide_scan_drivers(void)
{
	int i = 0;
	while(ide_scan[i])
	{
		(ide_scan[i])();
		i++;
	}
	drivers_run = 1;
}

/*
 * probe_for_hwifs() finds/initializes "known" IDE interfaces
 */
static void __init probe_for_hwifs (void)
{
#ifdef CONFIG_BLK_DEV_IDEPCI
	if (pci_present())
	{
		ide_scan_pcibus(ide_scan_direction);
	}
#endif /* CONFIG_BLK_DEV_IDEPCI */
	ide_scan_drivers();

	/*
	 *	Unconverted drivers
	 */	
#ifdef CONFIG_ETRAX_IDE
	{
		extern void init_e100_ide(void);
		init_e100_ide();
	}
#endif /* CONFIG_ETRAX_IDE */
#ifdef CONFIG_BLK_DEV_IDE_PMAC
	{
		extern void pmac_ide_probe(void);
		pmac_ide_probe();
	}
#endif /* CONFIG_BLK_DEV_IDE_PMAC */
#ifdef CONFIG_BLK_DEV_IDE_SIBYTE
	{
		extern void sibyte_ide_probe(void);
		sibyte_ide_probe();
	}
#endif /* CONFIG_BLK_DEV_IDE_SIBYTE */
#ifdef CONFIG_BLK_DEV_IDE_ICSIDE
	{
		extern void icside_init(void);
		icside_init();
	}
#endif /* CONFIG_BLK_DEV_IDE_ICSIDE */
#ifdef CONFIG_BLK_DEV_IDE_RAPIDE
	{
		extern void rapide_init(void);
		rapide_init();
	}
#endif /* CONFIG_BLK_DEV_IDE_RAPIDE */
#ifdef CONFIG_BLK_DEV_GAYLE
	{
		extern void gayle_init(void);
		gayle_init();
	}
#endif /* CONFIG_BLK_DEV_GAYLE */
#ifdef CONFIG_BLK_DEV_FALCON_IDE
	{
		extern void falconide_init(void);
		falconide_init();
	}
#endif /* CONFIG_BLK_DEV_FALCON_IDE */
#ifdef CONFIG_BLK_DEV_MAC_IDE
	{
		extern void macide_init(void);
		macide_init();
	}
#endif /* CONFIG_BLK_DEV_MAC_IDE */
#ifdef CONFIG_BLK_DEV_CPCI405_IDE
	{
		extern void cpci405ide_init(void);
		cpci405ide_init();
	}
#endif /* CONFIG_BLK_DEV_CPCI405_IDE */
#ifdef CONFIG_BLK_DEV_Q40IDE
	{
		extern void q40ide_init(void);
		q40ide_init();
	}
#endif /* CONFIG_BLK_DEV_Q40IDE */
#ifdef CONFIG_BLK_DEV_BUDDHA
	{
		extern void buddha_init(void);
		buddha_init();
	}
#endif /* CONFIG_BLK_DEV_BUDDHA */
}

void __init ide_init_builtin_subdrivers (void)
{
	idedefault_init();
#ifdef CONFIG_BLK_DEV_IDEDISK
	(void) idedisk_init();
#endif /* CONFIG_BLK_DEV_IDEDISK */
#ifdef CONFIG_BLK_DEV_IDECD
	(void) ide_cdrom_init();
#endif /* CONFIG_BLK_DEV_IDECD */
#ifdef CONFIG_BLK_DEV_IDETAPE
	(void) idetape_init();
#endif /* CONFIG_BLK_DEV_IDETAPE */
#ifdef CONFIG_BLK_DEV_IDEFLOPPY
	(void) idefloppy_init();
#endif /* CONFIG_BLK_DEV_IDEFLOPPY */
#ifdef CONFIG_BLK_DEV_IDESCSI
 #ifdef CONFIG_SCSI
	(void) idescsi_init();
 #else
    #warning ide scsi-emulation selected but no SCSI-subsystem in kernel
 #endif
#endif /* CONFIG_BLK_DEV_IDESCSI */
}

#ifndef CLASSIC_BUILTINS_METHOD
#  ifndef DIRECT_HWIF_PROBE_ATTACH_METHOD
#    ifdef FAKE_CLASSIC_ATTACH_METHOD
/*
 * Attempt to match drivers for the available drives
 */
void ide_attach_devices (void)
{
	ide_hwif_t *hwif;
	ide_drive_t *drive;
	int i, unit;

	for (i = 0; i < MAX_HWIFS; i++) {
		hwif = &ide_hwifs[i];
		if (!hwif->present)
			continue;
		for (unit = 0; unit < MAX_DRIVES; ++unit) {
			drive = &hwif->drives[unit];
			if (drive->present && !drive->dead)
				ide_attach_drive(drive);
		}
	}
}
#    endif /* FAKE_CLASSIC_ATTACH_METHOD */
#  endif /* DIRECT_HWIF_PROBE_ATTACH_METHOD */
#endif /* CLASSIC_BUILTINS_METHOD */

void __init ide_init_builtin_drivers (void)
{
	/*
	 * Attach null driver
	 */
	idedefault_init();
	/*
	 * Probe for special PCI and other "known" interface chipsets
	 */
	probe_for_hwifs ();

#ifdef CONFIG_BLK_DEV_IDE
	if (ide_hwifs[0].io_ports[IDE_DATA_OFFSET])
		ide_get_lock(NULL, NULL); /* for atari only */

	(void) ideprobe_init();

	if (ide_hwifs[0].io_ports[IDE_DATA_OFFSET])
		ide_release_lock();	/* for atari only */
#endif /* CONFIG_BLK_DEV_IDE */

#ifdef CONFIG_PROC_FS
	proc_ide_create();
#endif

#ifdef CLASSIC_BUILTINS_METHOD
	ide_init_builtin_subdrivers();
#else /* ! CLASSIC_BUILTINS_METHOD */
#  ifndef DIRECT_HWIF_PROBE_ATTACH_METHOD
#    ifdef FAKE_CLASSIC_ATTACH_METHOD
	ide_attach_devices();
#    endif /* FAKE_CLASSIC_ATTACH_METHOD */
#  endif /* DIRECT_HWIF_PROBE_ATTACH_METHOD */
#endif /* CLASSIC_BUILTINS_METHOD */
}

/*
 *	Actually unregister the subdriver. Called with the
 *	request lock dropped.
 */
 
static int default_cleanup (ide_drive_t *drive)
{
	return ide_unregister_subdriver(drive);
}

/*
 *	Check if we can unregister the subdriver. Called with the
 *	request lock held.
 */
 
static int default_shutdown(ide_drive_t *drive)
{
	if (drive->usage || drive->busy || DRIVER(drive)->busy) {
		return 1;
	}
	drive->dead = 1;
	return 0;
}

static int default_standby (ide_drive_t *drive)
{
	return 0;
}

static int default_suspend (ide_drive_t *drive)
{
	return 0;
}

static int default_resume (ide_drive_t *drive)
{
	return 0;
}

static int default_flushcache (ide_drive_t *drive)
{
	return 0;
}

static ide_startstop_t default_do_request (ide_drive_t *drive, struct request *rq, unsigned long block)
{
	ide_end_request(drive, 0);
	return ide_stopped;
}

static int default_end_request (ide_drive_t *drive, int uptodate)
{
	return ide_end_request(drive, uptodate);
}

static u8 default_sense (ide_drive_t *drive, const char *msg, u8 stat)
{
	return ide_dump_status(drive, msg, stat);
}

static ide_startstop_t default_error (ide_drive_t *drive, const char *msg, u8 stat)
{
	return ide_error(drive, msg, stat);
}

static ide_startstop_t default_abort (ide_drive_t *drive, const char *msg)
{
	return ide_abort(drive, msg);
}

static int default_ioctl (ide_drive_t *drive, struct inode *inode, struct file *file,
			  unsigned int cmd, unsigned long arg)
{
	return -EIO;
}

static int default_open (struct inode *inode, struct file *filp, ide_drive_t *drive)
{
	drive->usage--;
	return -EIO;
}

static void default_release (struct inode *inode, struct file *filp, ide_drive_t *drive)
{
}

static int default_check_media_change (ide_drive_t *drive)
{
	return 1;
}

static void default_pre_reset (ide_drive_t *drive)
{
}

static unsigned long default_capacity (ide_drive_t *drive)
{
	return 0x7fffffff;
}

static ide_startstop_t default_special (ide_drive_t *drive)
{
	special_t *s = &drive->special;

	s->all = 0;
	drive->mult_req = 0;
	return ide_stopped;
}

static int default_init (void)
{
	return 0;
}

static int default_attach (ide_drive_t *drive)
{
	printk(KERN_ERR "%s: does not support hotswap of device class !\n",
		drive->name);

	return 0;
}

static void setup_driver_defaults (ide_drive_t *drive)
{
	ide_driver_t *d = drive->driver;

	if (d->cleanup == NULL)		d->cleanup = default_cleanup;
	if (d->shutdown == NULL)	d->shutdown = default_shutdown;
	if (d->standby == NULL)		d->standby = default_standby;
	if (d->suspend == NULL)		d->suspend = default_suspend;
	if (d->resume == NULL)		d->resume = default_resume;
	if (d->flushcache == NULL)	d->flushcache = default_flushcache;
	if (d->do_request == NULL)	d->do_request = default_do_request;
	if (d->end_request == NULL)	d->end_request = default_end_request;
	if (d->sense == NULL)		d->sense = default_sense;
	if (d->error == NULL)		d->error = default_error;
	if (d->error == NULL)		d->abort = default_abort;
	if (d->ioctl == NULL)		d->ioctl = default_ioctl;
	if (d->open == NULL)		d->open = default_open;
	if (d->release == NULL)		d->release = default_release;
	if (d->media_change == NULL)	d->media_change = default_check_media_change;
	if (d->pre_reset == NULL)	d->pre_reset = default_pre_reset;
	if (d->capacity == NULL)	d->capacity = default_capacity;
	if (d->special == NULL)		d->special = default_special;
	if (d->init == NULL)		d->init = default_init;
	if (d->attach == NULL)		d->attach = default_attach;
}

ide_drive_t *ide_scan_devices (u8 media, const char *name, ide_driver_t *driver, int n)
{
	unsigned int unit, index, i;
	
	if(driver == NULL)	/* No driver is actually &idedefault_driver */
		driver = &idedefault_driver;

	for (index = 0, i = 0; index < MAX_HWIFS; ++index) {
		ide_hwif_t *hwif = &ide_hwifs[index];
		if (!hwif->present)
			continue;
		for (unit = 0; unit < MAX_DRIVES; ++unit) {
			ide_drive_t *drive = &hwif->drives[unit];
			char *req = drive->driver_req;
			if (*req && !strstr(name, req))
				continue;
			if (drive->present && drive->media == media &&
			    drive->driver == driver && ++i > n)
				return drive;
		}
	}
	return NULL;
}

EXPORT_SYMBOL(ide_scan_devices);

int ide_register_subdriver (ide_drive_t *drive, ide_driver_t *driver, int version)
{
	unsigned long flags;
	
	BUG_ON(drive->driver == NULL);
	
	spin_lock_irqsave(&io_request_lock, flags);
	if (version != IDE_SUBDRIVER_VERSION || !drive->present ||
	    drive->driver != &idedefault_driver || drive->busy || drive->usage) {
		spin_unlock_irqrestore(&io_request_lock, flags);
		return 1;
	}
	drive->driver = driver;
	setup_driver_defaults(drive);
	printk("%s: attached %s driver.\n", drive->name, driver->name);
	spin_unlock_irqrestore(&io_request_lock, flags);
	if (drive->autotune != 2) {
		/* DMA timings and setup moved to ide-probe.c */
		if (!driver->supports_dma && HWIF(drive)->ide_dma_off_quietly)
//			HWIF(drive)->ide_dma_off_quietly(drive);
			HWIF(drive)->ide_dma_off(drive);
		drive->dsc_overlap = (drive->next != drive && driver->supports_dsc_overlap);
		drive->nice1 = 1;
	}
	drive->revalidate = 1;
	drive->suspend_reset = 0;
#ifdef CONFIG_PROC_FS
	if (drive->driver != &idedefault_driver) {
		ide_add_proc_entries(drive->proc, generic_subdriver_entries, drive);
		ide_add_proc_entries(drive->proc, driver->proc, drive);
	}
#endif
	return 0;
}

EXPORT_SYMBOL(ide_register_subdriver);

int ide_unregister_subdriver (ide_drive_t *drive)
{
	unsigned long flags;

	down(&ide_setting_sem);	
	spin_lock_irqsave(&io_request_lock, flags);
	if (drive->usage || drive->busy || DRIVER(drive)->busy) {
		spin_unlock_irqrestore(&io_request_lock, flags);
		up(&ide_setting_sem);
		return 1;
	}
#ifdef CONFIG_PROC_FS
	ide_remove_proc_entries(drive->proc, DRIVER(drive)->proc);
	ide_remove_proc_entries(drive->proc, generic_subdriver_entries);
#endif
	drive->driver = &idedefault_driver;
	setup_driver_defaults(drive);
	auto_remove_settings(drive);
	spin_unlock_irqrestore(&io_request_lock, flags);
	up(&ide_setting_sem);
	return 0;
}

EXPORT_SYMBOL(ide_unregister_subdriver);

int ide_register_module (ide_module_t *module)
{
	ide_module_t *p = ide_modules;

	while (p) {
		if (p == module)
			return 1;
		p = p->next;
	}
	module->next = ide_modules;
	ide_modules = module;
	revalidate_drives(1);
	return 0;
}

EXPORT_SYMBOL(ide_register_module);

void ide_unregister_module (ide_module_t *module)
{
	ide_module_t **p;

	for (p = &ide_modules; (*p) && (*p) != module; p = &((*p)->next));
	if (*p)
		*p = (*p)->next;
}

EXPORT_SYMBOL(ide_unregister_module);

struct block_device_operations ide_fops[] = {{
	owner:			THIS_MODULE,
	open:			ide_open,
	release:		ide_release,
	ioctl:			ide_ioctl,
	check_media_change:	ide_check_media_change,
	revalidate:		ide_revalidate_disk
}};

EXPORT_SYMBOL(ide_fops);

/*
 * ide_geninit() is called exactly *once* for each interface.
 */
void ide_geninit (ide_hwif_t *hwif)
{
	unsigned int unit;
	struct gendisk *gd = hwif->gd;

	for (unit = 0; unit < MAX_DRIVES; ++unit) {
		ide_drive_t *drive = &hwif->drives[unit];

		if (!drive->present)
			continue;
		if (drive->media!=ide_disk && drive->media!=ide_floppy)
			continue;
		register_disk(gd,MKDEV(hwif->major,unit<<PARTN_BITS),
#ifdef CONFIG_BLK_DEV_ISAPNP
			(drive->forced_geom && drive->noprobe) ? 1 :
#endif /* CONFIG_BLK_DEV_ISAPNP */
			1<<PARTN_BITS, ide_fops,
			current_capacity(drive));
	}
}

EXPORT_SYMBOL(ide_geninit);


/*
 * Probe module
 */
devfs_handle_t ide_devfs_handle;

EXPORT_SYMBOL(ide_probe);
EXPORT_SYMBOL(ide_devfs_handle);

static int ide_notify_reboot (struct notifier_block *this, unsigned long event, void *x)
{
	ide_hwif_t *hwif;
	ide_drive_t *drive;
	int i, unit;

	switch (event) {
		case SYS_HALT:
		case SYS_POWER_OFF:
		case SYS_RESTART:
			break;
		default:
			return NOTIFY_DONE;
	}

	printk(KERN_INFO "flushing ide devices: ");

	for (i = 0; i < MAX_HWIFS; i++) {
		hwif = &ide_hwifs[i];
		if (!hwif->present)
			continue;
		for (unit = 0; unit < MAX_DRIVES; ++unit) {
			drive = &hwif->drives[unit];
			if (!drive->present)
				continue;

			/* set the drive to standby */
			printk("%s ", drive->name);
			if (event != SYS_RESTART)
				if (DRIVER(drive)->standby(drive))
					continue;

			DRIVER(drive)->cleanup(drive);
			continue;
		}
	}
	printk("\n");
	return NOTIFY_DONE;
}

static struct notifier_block ide_notifier = {
	ide_notify_reboot,
	NULL,
	5
};

/*
 * This is gets invoked once during initialization, to set *everything* up
 */
int __init ide_init (void)
{
	static char banner_printed;
	int i;

	if (!banner_printed) {
		printk(KERN_INFO "Uniform Multi-Platform E-IDE driver " REVISION "\n");
		ide_devfs_handle = devfs_mk_dir(NULL, "ide", NULL);
		system_bus_speed = ide_system_bus_speed();
		banner_printed = 1;
	}

	init_ide_data();

#ifndef CLASSIC_BUILTINS_METHOD
	ide_init_builtin_subdrivers();
#endif /* CLASSIC_BUILTINS_METHOD */

	initializing = 1;
	ide_init_builtin_drivers();
	initializing = 0;

	for (i = 0; i < MAX_HWIFS; ++i) {
		ide_hwif_t  *hwif = &ide_hwifs[i];
		if (hwif->present)
			ide_geninit(hwif);
	}

	register_reboot_notifier(&ide_notifier);
	return 0;
}

#ifdef MODULE
char *options = NULL;
MODULE_PARM(options,"s");
MODULE_LICENSE("GPL");

static void __init parse_options (char *line)
{
	char *next = line;

	if (line == NULL || !*line)
		return;
	while ((line = next) != NULL) {
 		if ((next = strchr(line,' ')) != NULL)
			*next++ = 0;
		if (!ide_setup(line))
			printk ("Unknown option '%s'\n", line);
	}
}

int init_module (void)
{
	parse_options(options);
	return ide_init();
}

void cleanup_module (void)
{
	int index;

	unregister_reboot_notifier(&ide_notifier);
	for (index = 0; index < MAX_HWIFS; ++index) {
		ide_unregister(index);
#if !defined(CONFIG_DMA_NONPCI)
		if (ide_hwifs[index].dma_base)
			(void) ide_release_dma(&ide_hwifs[index]);
#endif /* !(CONFIG_DMA_NONPCI) */
	}

#ifdef CONFIG_PROC_FS
	proc_ide_destroy();
#endif
	devfs_unregister (ide_devfs_handle);
}

#else /* !MODULE */

__setup("", ide_setup);
module_init(ide_init);

#endif /* MODULE */
