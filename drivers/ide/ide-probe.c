/*
 *  linux/drivers/ide/ide-probe.c	Version 1.11	Mar 05, 2003
 *
 *  Copyright (C) 1994-1998  Linus Torvalds & authors (see below)
 */

/*
 *  Mostly written by Mark Lord <mlord@pobox.com>
 *                and Gadi Oxman <gadio@netvision.net.il>
 *                and Andre Hedrick <andre@linux-ide.org>
 *
 *  See linux/MAINTAINERS for address of current maintainer.
 *
 * This is the IDE probe module, as evolved from hd.c and ide.c.
 *
 * Version 1.00		move drive probing code from ide.c to ide-probe.c
 * Version 1.01		fix compilation problem for m68k
 * Version 1.02		increase WAIT_PIDENTIFY to avoid CD-ROM locking at boot
 *			 by Andrea Arcangeli
 * Version 1.03		fix for (hwif->chipset == ide_4drives)
 * Version 1.04		fixed buggy treatments of known flash memory cards
 *
 * Version 1.05		fix for (hwif->chipset == ide_pdc4030)
 *			added ide6/7/8/9
 *			allowed for secondary flash card to be detectable
 *			 with new flag : drive->ata_flash : 1;
 * Version 1.06		stream line request queue and prep for cascade project.
 * Version 1.07		max_sect <= 255; slower disks would get behind and
 * 			then fall over when they get to 256.	Paul G.
 * Version 1.10		Update set for new IDE. drive->id is now always
 *			valid after probe time even with noprobe
 */

#undef REALLY_SLOW_IO		/* most systems can safely undef this */

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
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/spinlock.h>
#include <linux/pci.h>
#include <linux/kmod.h>

#include <asm/byteorder.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/io.h>

/**
 *	generic_id		-	add a generic drive id
 *	@drive:	drive to make an ID block for
 *	
 *	Add a fake id field to the drive we are passed. This allows
 *	use to skip a ton of NULL checks (which people always miss) 
 *	and make drive properties unconditional outside of this file
 */
 
static void generic_id(ide_drive_t *drive)
{
	drive->id->cyls = drive->cyl;
	drive->id->heads = drive->head;
	drive->id->sectors = drive->sect;
	drive->id->cur_cyls = drive->cyl;
	drive->id->cur_heads = drive->head;
	drive->id->cur_sectors = drive->sect;
}
		
/**
 *	drive_is_flashcard	-	check for compact flash
 *	@drive: drive to check
 *
 *	CompactFlash cards and their brethern pretend to be removable
 *	hard disks, except:
 * 		(1) they never have a slave unit, and
 *		(2) they don't have doorlock mechanisms.
 *	This test catches them, and is invoked elsewhere when setting
 *	appropriate config bits.
 *
 *	FIXME: This treatment is probably applicable for *all* PCMCIA (PC CARD)
 *	devices, so in linux 2.3.x we should change this to just treat all
 *	PCMCIA  drives this way, and get rid of the model-name tests below
 *	(too big of an interface change for 2.4.x).
 *	At that time, we might also consider parameterizing the timeouts and
 *	retries, since these are MUCH faster than mechanical drives. -M.Lord
 */
 
static inline int drive_is_flashcard (ide_drive_t *drive)
{
	struct hd_driveid *id = drive->id;

	if (drive->removable && id != NULL) {
		if (id->config == 0x848a) return 1;	/* CompactFlash */
		if (!strncmp(id->model, "KODAK ATA_FLASH", 15)	/* Kodak */
		 || !strncmp(id->model, "Hitachi CV", 10)	/* Hitachi */
		 || !strncmp(id->model, "SunDisk SDCFB", 13)	/* old SanDisk */
		 || !strncmp(id->model, "SanDisk SDCFB", 13)	/* SanDisk */
		 || !strncmp(id->model, "HAGIWARA HPC", 12)	/* Hagiwara */
		 || !strncmp(id->model, "LEXAR ATA_FLASH", 15)	/* Lexar */
		 || !strncmp(id->model, "ATA_FLASH", 9))	/* Simple Tech */
		{
			return 1;	/* yes, it is a flash memory card */
		}
	}
	return 0;	/* no, it is not a flash memory card */
}

/**
 *	do_identify	-	identify a drive
 *	@drive: drive to identify 
 *	@cmd: command used
 *
 *	Called when we have issued a drive identify command to
 *	read and parse the results. This function is run with
 *	interrupts disabled. 
 */
 
static inline void do_identify (ide_drive_t *drive, u8 cmd)
{
	ide_hwif_t *hwif = HWIF(drive);
	int bswap = 1;
	struct hd_driveid *id;

	/* called with interrupts disabled! */
	id = drive->id;
	/* read 512 bytes of id info */
	hwif->ata_input_data(drive, id, SECTOR_WORDS);
	drive->id_read = 1;
	local_irq_enable();
	
	ide_fix_driveid(id);

	if (!drive->forced_lun)
		drive->last_lun = id->last_lun & 0x7;

#if defined (CONFIG_SCSI_EATA_DMA) || defined (CONFIG_SCSI_EATA_PIO) || defined (CONFIG_SCSI_EATA)
	/*
	 * EATA SCSI controllers do a hardware ATA emulation:
	 * Ignore them if there is a driver for them available.
	 */
	if ((id->model[0] == 'P' && id->model[1] == 'M') ||
	    (id->model[0] == 'S' && id->model[1] == 'K')) {
		printk("%s: EATA SCSI HBA %.10s\n", drive->name, id->model);
		goto err_misc;
	}
#endif /* CONFIG_SCSI_EATA_DMA || CONFIG_SCSI_EATA_PIO */

	/*
	 *  WIN_IDENTIFY returns little-endian info,
	 *  WIN_PIDENTIFY *usually* returns little-endian info.
	 */
	if (cmd == WIN_PIDENTIFY) {
		if ((id->model[0] == 'N' && id->model[1] == 'E') /* NEC */
		 || (id->model[0] == 'F' && id->model[1] == 'X') /* Mitsumi */
		 || (id->model[0] == 'P' && id->model[1] == 'i'))/* Pioneer */
			/* Vertos drives may still be weird */
			bswap ^= 1;	
	}
	ide_fixstring(id->model,     sizeof(id->model),     bswap);
	ide_fixstring(id->fw_rev,    sizeof(id->fw_rev),    bswap);
	ide_fixstring(id->serial_no, sizeof(id->serial_no), bswap);

	if (strstr(id->model, "E X A B Y T E N E S T"))
		goto err_misc;

	/* we depend on this a lot! */
	id->model[sizeof(id->model)-1] = '\0';
	printk("%s: %s, ", drive->name, id->model);
	drive->present = 1;
	drive->dead = 0;

	/*
	 * Check for an ATAPI device
	 */
	if (cmd == WIN_PIDENTIFY) {
		u8 type = (id->config >> 8) & 0x1f;
		printk("ATAPI ");
#ifdef CONFIG_BLK_DEV_PDC4030
		if (hwif->channel == 1 && hwif->chipset == ide_pdc4030) {
			printk(" -- not supported on 2nd Promise port\n");
			goto err_misc;
		}
#endif /* CONFIG_BLK_DEV_PDC4030 */
		switch (type) {
			case ide_floppy:
				if (!strstr(id->model, "CD-ROM")) {
					if (!strstr(id->model, "oppy") &&
					    !strstr(id->model, "poyp") &&
					    !strstr(id->model, "ZIP"))
						printk("cdrom or floppy?, assuming ");
					if (drive->media != ide_cdrom) {
						printk ("FLOPPY");
						drive->removable = 1;
						break;
					}
				}
				/* Early cdrom models used zero */
				type = ide_cdrom;
			case ide_cdrom:
				drive->removable = 1;
#ifdef CONFIG_PPC
				/* kludge for Apple PowerBook internal zip */
				if (!strstr(id->model, "CD-ROM") &&
				    strstr(id->model, "ZIP")) {
					printk ("FLOPPY");
					type = ide_floppy;
					break;
				}
#endif
				printk ("CD/DVD-ROM");
				break;
			case ide_tape:
				printk ("TAPE");
				break;
			case ide_optical:
				printk ("OPTICAL");
				drive->removable = 1;
				break;
			default:
				printk("UNKNOWN (type %d)", type);
				break;
		}
		printk (" drive\n");
		drive->media = type;
		return;
	}

	/*
	 * Not an ATAPI device: looks like a "regular" hard disk
	 */
	if (id->config & (1<<7))
		drive->removable = 1;
		
	/*
	 * Prevent long system lockup probing later for non-existant
	 * slave drive if the hwif is actually a flash memory card of
	 * some variety:
	 */
	drive->is_flash = 0;
	if (drive_is_flashcard(drive)) {
#if 0
		/* The new IDE adapter widgets don't follow this heuristic
		   so we must nowdays just bite the bullet and take the
		   probe hit */	
		ide_drive_t *mate = &hwif->drives[1^drive->select.b.unit];		
		if (!mate->ata_flash) {
			mate->present = 0;
			mate->noprobe = 1;
		}
#endif		
		drive->is_flash = 1;
	}
	drive->media = ide_disk;
	printk("%s DISK drive\n", (drive->is_flash) ? "CFA" : "ATA" );
	QUIRK_LIST(drive);
	return;

err_misc:
	kfree(id);
	drive->present = 0;
	return;
}

/**
 *	actual_try_to_identify	-	send ata/atapi identify
 *	@drive: drive to identify
 *	@cmd: comamnd to use
 *
 *	try_to_identify() sends an ATA(PI) IDENTIFY request to a drive
 *	and waits for a response.  It also monitors irqs while this is
 *	happening, in hope of automatically determining which one is
 *	being used by the interface.
 *
 *	Returns:	0  device was identified
 *			1  device timed-out (no response to identify request)
 *			2  device aborted the command (refused to identify itself)
 */

static int actual_try_to_identify (ide_drive_t *drive, u8 cmd)
{
	ide_hwif_t *hwif = HWIF(drive);
	int rc;
	ide_ioreg_t hd_status;
	unsigned long timeout;
	u8 s = 0, a = 0;

	if (IDE_CONTROL_REG) {
		/* take a deep breath */
		ide_delay_50ms();
		a = hwif->INB(IDE_ALTSTATUS_REG);
		s = hwif->INB(IDE_STATUS_REG);
		if ((a ^ s) & ~INDEX_STAT) {
			printk(KERN_INFO "%s: probing with STATUS(0x%02x) instead of "
				"ALTSTATUS(0x%02x)\n", drive->name, s, a);
			/* ancient Seagate drives, broken interfaces */
			hd_status = IDE_STATUS_REG;
		} else {
			/* use non-intrusive polling */
			hd_status = IDE_ALTSTATUS_REG;
		}
	} else {
		ide_delay_50ms();
		hd_status = IDE_STATUS_REG;
	}

	/* set features register for atapi
	 * identify command to be sure of reply
	 */
	if ((cmd == WIN_PIDENTIFY))
		/* disable dma & overlap */
		hwif->OUTB(0, IDE_FEATURE_REG);

	if (hwif->identify != NULL) {
		if (hwif->identify(drive))
			return 1;
	} else {
		/* ask drive for ID */
		hwif->OUTB(cmd, IDE_COMMAND_REG);
	}
	timeout = ((cmd == WIN_IDENTIFY) ? WAIT_WORSTCASE : WAIT_PIDENTIFY) / 2;
	timeout += jiffies;
	do {
		if (time_after(jiffies, timeout)) {
			/* drive timed-out */
			return 1;
		}
		/* give drive a breather */
		ide_delay_50ms();
	} while ((hwif->INB(hd_status)) & BUSY_STAT);

	/* wait for IRQ and DRQ_STAT */
	ide_delay_50ms();
	if (OK_STAT((hwif->INB(IDE_STATUS_REG)), DRQ_STAT, BAD_R_STAT)) {
		unsigned long flags;

		/* local CPU only; some systems need this */
		local_irq_save(flags);
		/* drive returned ID */
		do_identify(drive, cmd);
		/* drive responded with ID */
		rc = 0;
		/* clear drive IRQ */
		(void) hwif->INB(IDE_STATUS_REG);
		local_irq_restore(flags);
	} else {
		/* drive refused ID */
		rc = 2;
	}
	return rc;
}

/**
 *	try_to_identify	-	try to identify a drive
 *	@drive: drive to probe
 *	@cmd: comamnd to use
 *
 *	Issue the identify command and then do IRQ probing to
 *	complete the identification when needed by finding the
 *	IRQ the drive is attached to
 */
 
static int try_to_identify (ide_drive_t *drive, u8 cmd)
{
	ide_hwif_t *hwif = HWIF(drive);
	int retval;
	int autoprobe = 0;
	unsigned long cookie = 0;

	if (IDE_CONTROL_REG && !hwif->irq) {
		autoprobe = 1;
		cookie = probe_irq_on();
		/* enable device irq */
		hwif->OUTB(drive->ctl, IDE_CONTROL_REG);
	}

	retval = actual_try_to_identify(drive, cmd);

	if (autoprobe) {
		int irq;
		/* mask device irq */
		hwif->OUTB(drive->ctl|2, IDE_CONTROL_REG);
		/* clear drive IRQ */
		(void) hwif->INB(IDE_STATUS_REG);
		udelay(5);
		irq = probe_irq_off(cookie);
		if (!hwif->irq) {
			if (irq > 0) {
				hwif->irq = irq;
			} else {
				/* Mmmm.. multiple IRQs..
				 * don't know which was ours
				 */
				printk("%s: IRQ probe failed (0x%lx)\n",
					drive->name, cookie);
#ifdef CONFIG_BLK_DEV_CMD640
#ifdef CMD640_DUMP_REGS
				if (hwif->chipset == ide_cmd640) {
					printk("%s: Hmmm.. probably a driver "
						"problem.\n", drive->name);
					CMD640_DUMP_REGS;
				}
#endif /* CMD640_DUMP_REGS */
#endif /* CONFIG_BLK_DEV_CMD640 */
			}
		}
	}
	return retval;
}


/**
 *	do_probe		-	probe an IDE device
 *	@drive: drive to probe
 *	@cmd: command to use
 *
 *	do_probe() has the difficult job of finding a drive if it exists,
 *	without getting hung up if it doesn't exist, without trampling on
 *	ethernet cards, and without leaving any IRQs dangling to haunt us later.
 *
 *	If a drive is "known" to exist (from CMOS or kernel parameters),
 *	but does not respond right away, the probe will "hang in there"
 *	for the maximum wait time (about 30 seconds), otherwise it will
 *	exit much more quickly.
 *
 * Returns:	0  device was identified
 *		1  device timed-out (no response to identify request)
 *		2  device aborted the command (refused to identify itself)
 *		3  bad status from device (possible for ATAPI drives)
 *		4  probe was not attempted because failure was obvious
 */

static int do_probe (ide_drive_t *drive, u8 cmd)
{
	int rc;
	ide_hwif_t *hwif = HWIF(drive);

	if (drive->present) {
		/* avoid waiting for inappropriate probes */
		if ((drive->media != ide_disk) && (cmd == WIN_IDENTIFY))
			return 4;
	}
#ifdef DEBUG
	printk("probing for %s: present=%d, media=%d, probetype=%s\n",
		drive->name, drive->present, drive->media,
		(cmd == WIN_IDENTIFY) ? "ATA" : "ATAPI");
#endif

	/* needed for some systems
	 * (e.g. crw9624 as drive0 with disk as slave)
	 */
	ide_delay_50ms();
	SELECT_DRIVE(drive);
	ide_delay_50ms();
	if (hwif->INB(IDE_SELECT_REG) != drive->select.all && !drive->present) {
		if (drive->select.b.unit != 0) {
			/* exit with drive0 selected */
			SELECT_DRIVE(&hwif->drives[0]);
			/* allow BUSY_STAT to assert & clear */
			ide_delay_50ms();
		}
		/* no i/f present: mmm.. this should be a 4 -ml */
		return 3;
	}

	if (OK_STAT((hwif->INB(IDE_STATUS_REG)), READY_STAT, BUSY_STAT) ||
	    drive->present || cmd == WIN_PIDENTIFY) {
		/* send cmd and wait */
		if ((rc = try_to_identify(drive, cmd))) {
			/* failed: try again */
			rc = try_to_identify(drive,cmd);
		}
		if (hwif->INB(IDE_STATUS_REG) == (BUSY_STAT|READY_STAT))
			return 4;

		if ((rc == 1 && cmd == WIN_PIDENTIFY) &&
			((drive->autotune == IDE_TUNE_DEFAULT) ||
			(drive->autotune == IDE_TUNE_AUTO))) {
			unsigned long timeout;
			printk("%s: no response (status = 0x%02x), "
				"resetting drive\n", drive->name,
				hwif->INB(IDE_STATUS_REG));
			ide_delay_50ms();
			hwif->OUTB(drive->select.all, IDE_SELECT_REG);
			ide_delay_50ms();
			hwif->OUTB(WIN_SRST, IDE_COMMAND_REG);
			timeout = jiffies;
			while (((hwif->INB(IDE_STATUS_REG)) & BUSY_STAT) &&
			       time_before(jiffies, timeout + WAIT_WORSTCASE))
				ide_delay_50ms();
			rc = try_to_identify(drive, cmd);
		}
		if (rc == 1)
			printk("%s: no response (status = 0x%02x)\n",
				drive->name, hwif->INB(IDE_STATUS_REG));
		/* ensure drive irq is clear */
		(void) hwif->INB(IDE_STATUS_REG);
	} else {
		/* not present or maybe ATAPI */
		rc = 3;
	}
	if (drive->select.b.unit != 0) {
		/* exit with drive0 selected */
		SELECT_DRIVE(&hwif->drives[0]);
		ide_delay_50ms();
		/* ensure drive irq is clear */
		(void) hwif->INB(IDE_STATUS_REG);
	}
	return rc;
}

/*
 *
 */
static void enable_nest (ide_drive_t *drive)
{
	ide_hwif_t *hwif = HWIF(drive);
	unsigned long timeout;

	printk("%s: enabling %s -- ", hwif->name, drive->id->model);
	SELECT_DRIVE(drive);
	ide_delay_50ms();
	hwif->OUTB(EXABYTE_ENABLE_NEST, IDE_COMMAND_REG);
	timeout = jiffies + WAIT_WORSTCASE;
	do {
		if (time_after(jiffies, timeout)) {
			printk("failed (timeout)\n");
			return;
		}
		ide_delay_50ms();
	} while ((hwif->INB(IDE_STATUS_REG)) & BUSY_STAT);

	ide_delay_50ms();

	if (!OK_STAT((hwif->INB(IDE_STATUS_REG)), 0, BAD_STAT)) {
		printk("failed (status = 0x%02x)\n", hwif->INB(IDE_STATUS_REG));
	} else {
		printk("success\n");
	}

	/* if !(success||timed-out) */
	if (do_probe(drive, WIN_IDENTIFY) >= 2) {
		/* look for ATAPI device */
		(void) do_probe(drive, WIN_PIDENTIFY);
	}
}

/**
 *	ide_probe_for_drives	-	upper level drive probe
 *	@drive: drive to probe for
 *
 *	probe_for_drive() tests for existence of a given drive using do_probe()
 *	and presents things to the user as needed.
 *
 *	Returns:	0  no device was found
 *			1  device was found (note: drive->present might
 *			   still be 0)
 */
 
u8 ide_probe_for_drive (ide_drive_t *drive)
{
	/*
	 *	In order to keep things simple we have an id
	 *	block for all drives at all times. If the device
	 *	is pre ATA or refuses ATA/ATAPI identify we
	 *	will add faked data to this.
	 *
	 *	Also note that 0 everywhere means "can't do X"
	 */
	 
	drive->id = kmalloc(SECTOR_WORDS *4, GFP_KERNEL);
	drive->id_read = 0;
	if(drive->id == NULL)
	{
		printk(KERN_ERR "ide: out of memory for id data.\n");
		return 0;
	}
	memset(drive->id, 0, SECTOR_WORDS * 4);
	strcpy(drive->id->model, "UNKNOWN");
	
	/* skip probing? */
	if (!drive->noprobe)
	{
		/* if !(success||timed-out) */
		if (do_probe(drive, WIN_IDENTIFY) >= 2) {
			/* look for ATAPI device */
			(void) do_probe(drive, WIN_PIDENTIFY);
		}
		if (strstr(drive->id->model, "E X A B Y T E N E S T"))
			enable_nest(drive);
		if (!drive->present)
			/* drive not found */
			return 0;
	
		/* identification failed? */
		if (!drive->id_read) {
			if (drive->media == ide_disk) {
				printk(KERN_INFO "%s: non-IDE drive, CHS=%d/%d/%d\n",
					drive->name, drive->cyl,
					drive->head, drive->sect);
			} else if (drive->media == ide_cdrom) {
				printk(KERN_INFO "%s: ATAPI cdrom (?)\n", drive->name);
			} else {
				/* nuke it */
				printk(KERN_WARNING "%s: Unknown device on bus refused identification. Ignoring.\n", drive->name);
				drive->present = 0;
			}
		}
		/* drive was found */
	}
	if(!drive->present)
		return 0;
	/* The drive wasn't being helpful. Add generic info only */
	if(!drive->id_read)
		generic_id(drive);
	return drive->present;
}

#define hwif_check_region(addr, num) \
	((hwif->mmio) ? check_mem_region((addr),(num)) : check_region((addr),(num)))

/**
 *	hwif_check_regions	-	check resources for IDE
 *	@hwif: interface to use
 *
 *	Checks if all the needed resources for an interface are free
 *	providing the interface is PIO. Right now core IDE code does
 *	this work which is deeply wrong. MMIO leaves it to the controller
 *	driver, PIO will migrate this way over time
 */
 
static int hwif_check_regions (ide_hwif_t *hwif)
{
	u32 i		= 0;
	int addr_errs	= 0;

	if (hwif->mmio == 2)
		return 0;
	addr_errs  = hwif_check_region(hwif->io_ports[IDE_DATA_OFFSET], 1);
	for (i = IDE_ERROR_OFFSET; i <= IDE_STATUS_OFFSET; i++)
		addr_errs += hwif_check_region(hwif->io_ports[i], 1);
	if (hwif->io_ports[IDE_CONTROL_OFFSET])
		addr_errs += hwif_check_region(hwif->io_ports[IDE_CONTROL_OFFSET], 1);
#if defined(CONFIG_AMIGA) || defined(CONFIG_MAC)
	if (hwif->io_ports[IDE_IRQ_OFFSET])
		addr_errs += hwif_check_region(hwif->io_ports[IDE_IRQ_OFFSET], 1);
#endif /* (CONFIG_AMIGA) || (CONFIG_MAC) */
	/* If any errors are return, we drop the hwif interface. */
	hwif->straight8 = 0;
	return(addr_errs);
}

//EXPORT_SYMBOL(hwif_check_regions);

#define hwif_request_region(addr, num, name)	\
	((hwif->mmio) ? request_mem_region((addr),(num),(name)) : request_region((addr),(num),(name)))

static void hwif_register (ide_hwif_t *hwif)
{
	u32 i = 0;

	if (hwif->mmio == 2)
		return;
	if (hwif->io_ports[IDE_CONTROL_OFFSET])
		hwif_request_region(hwif->io_ports[IDE_CONTROL_OFFSET], 1, hwif->name);
#if defined(CONFIG_AMIGA) || defined(CONFIG_MAC)
	if (hwif->io_ports[IDE_IRQ_OFFSET])
		hwif_request_region(hwif->io_ports[IDE_IRQ_OFFSET], 1, hwif->name);
#endif /* (CONFIG_AMIGA) || (CONFIG_MAC) */
	if (((unsigned long)hwif->io_ports[IDE_DATA_OFFSET] | 7) ==
	    ((unsigned long)hwif->io_ports[IDE_STATUS_OFFSET])) {
		hwif_request_region(hwif->io_ports[IDE_DATA_OFFSET], 8, hwif->name);
		hwif->straight8 = 1;
		return;
	}

	for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++)
		hwif_request_region(hwif->io_ports[i], 1, hwif->name);
}

//EXPORT_SYMBOL(hwif_register);

/*
 * This function waits for the hwif to report a non-busy status
 * see comments in probe_hwif()
 */
static int wait_not_busy(ide_hwif_t *hwif, unsigned long timeout)
{
	u8 stat = 0;
	
	while(timeout--) {
		/* Turn this into a schedule() sleep once I'm sure
		 * about locking issues (2.5 work ?)
		 */
		mdelay(1);
		stat = hwif->INB(hwif->io_ports[IDE_STATUS_OFFSET]);
		if ((stat & BUSY_STAT) == 0)
			break;
		/* Assume a value of 0xff means nothing is connected to
		 * the interface and it doesn't implement the pull-down
		 * resistor on D7
		 */
		if (stat == 0xff)
			break;
	}
	return ((stat & BUSY_STAT) == 0) ? 0 : -EBUSY;
}

int ide_wait_hwif_ready(ide_hwif_t *hwif)
{
	int rc;

	printk(KERN_INFO "Probing IDE interface %s...\n", hwif->name);

	/* Let HW settle down a bit from whatever init state we
	 * come from */
	mdelay(2);

	/* Wait for BSY bit to go away, spec timeout is 30 seconds,
	 * I know of at least one disk who takes 31 seconds, I use 35
	 * here to be safe
	 */
	rc = wait_not_busy(hwif, 35000);
	if (rc)
		return rc;

	/* Now make sure both master & slave are ready */
	SELECT_DRIVE(&hwif->drives[0]);
	hwif->OUTB(8, hwif->io_ports[IDE_CONTROL_OFFSET]);
	mdelay(2);
	rc = wait_not_busy(hwif, 10000);
	if (rc)
		return rc;
	SELECT_DRIVE(&hwif->drives[1]);
	hwif->OUTB(8, hwif->io_ports[IDE_CONTROL_OFFSET]);
	mdelay(2);
	rc = wait_not_busy(hwif, 10000);

	/* Exit function with master reselected (let's be sane) */
	SELECT_DRIVE(&hwif->drives[0]);
	
	return rc;
}

void ide_probe_reset(ide_hwif_t *hwif)
{
	if (hwif->io_ports[IDE_CONTROL_OFFSET] && hwif->reset) {
		unsigned long timeout = jiffies + WAIT_WORSTCASE;
		u8 stat;

		printk(KERN_WARNING "%s: reset\n", hwif->name);
		hwif->OUTB(12, hwif->io_ports[IDE_CONTROL_OFFSET]);
		udelay(10);
		hwif->OUTB(8, hwif->io_ports[IDE_CONTROL_OFFSET]);
		do {
			ide_delay_50ms();
			stat = hwif->INB(hwif->io_ports[IDE_STATUS_OFFSET]);
		} while ((stat & BUSY_STAT) && time_after(timeout, jiffies));
	}
}

void ide_tune_drives(ide_hwif_t *hwif)
{
	int unit;
	
	for (unit = 0; unit < MAX_DRIVES; ++unit) {
		ide_drive_t *drive = &hwif->drives[unit];
		int enable_dma = 1;

		if (drive->present) {
			if (hwif->tuneproc != NULL && 
				drive->autotune == IDE_TUNE_AUTO)
				/* auto-tune PIO mode */
				hwif->tuneproc(drive, 255);

#ifdef CONFIG_IDEDMA_ONLYDISK
			if (drive->media != ide_disk)
				enable_dma = 0;
#endif
			/*
			 * MAJOR HACK BARF :-/
			 *
			 * FIXME: chipsets own this cruft!
			 */
			/*
			 * Move here to prevent module loading clashing.
			 */
	//		drive->autodma = hwif->autodma;
			if ((hwif->ide_dma_check) &&
				((drive->autotune == IDE_TUNE_DEFAULT) ||
				(drive->autotune == IDE_TUNE_AUTO))) {
				/*
				 * Force DMAing for the beginning of the check.
				 * Some chipsets appear to do interesting
				 * things, if not checked and cleared.
				 *   PARANOIA!!!
				 */
				hwif->ide_dma_off_quietly(drive);
				if (enable_dma)
					hwif->ide_dma_check(drive);
			}
		}
	}
}

/*
 * This routine only knows how to look for drive units 0 and 1
 * on an interface, so any setting of MAX_DRIVES > 2 won't work here.
 */
void probe_hwif (ide_hwif_t *hwif)
{
	unsigned int unit;
	unsigned long flags;
	unsigned int irqd;

	if (hwif->noprobe)
		return;
#ifdef CONFIG_BLK_DEV_IDE
	if (hwif->io_ports[IDE_DATA_OFFSET] == HD_DATA) {
		extern void probe_cmos_for_drives(ide_hwif_t *);
		probe_cmos_for_drives(hwif);
	}
#endif
	if ((hwif->chipset != ide_4drives || !hwif->mate || !hwif->mate->present) &&
#if CONFIG_BLK_DEV_PDC4030
	    (hwif->chipset != ide_pdc4030 || hwif->channel == 0) &&
#endif /* CONFIG_BLK_DEV_PDC4030 */
	    (hwif_check_regions(hwif))) {
		u16 msgout = 0;
		for (unit = 0; unit < MAX_DRIVES; ++unit) {
			ide_drive_t *drive = &hwif->drives[unit];
			if (drive->present) {
				drive->present = 0;
				printk(KERN_ERR "%s: ERROR, PORTS ALREADY IN USE\n",
					drive->name);
				msgout = 1;
			}
		}
		if (!msgout)
			printk(KERN_ERR "%s: ports already in use, skipping probe\n",
				hwif->name);
		return;	
	}

	/*
	 * We must always disable IRQ, as probe_for_drive will assert IRQ, but
	 * we'll install our IRQ driver much later...
	 */
	irqd = hwif->irq;
	if (irqd)
		disable_irq(hwif->irq);

	local_irq_set(flags);

#ifdef CONFIG_PPC
	/* This is needed on some PPCs and a bunch of BIOS-less embedded
	 * platforms. Typical cases are:
	 * 
	 *  - The firmware hard reset the disk before booting the kernel,
	 *    the drive is still doing it's poweron-reset sequence, that
	 *    can take up to 30 seconds
	 *  - The firmware does nothing (or no firmware), the device is
	 *    still in POST state (same as above actually).
	 *  - Some CD/DVD/Writer combo drives tend to drive the bus during
	 *    their reset sequence even when they are non-selected slave
	 *    devices, thus preventing discovery of the main HD
	 *    
	 *  Doing this wait-for-busy should not harm any existing configuration
	 *  (at least things won't be worse than what current code does, that
	 *  is blindly go & talk to the drive) and fix some issues like the
	 *  above.
	 *  
	 *  BenH.
	 */
	if (ide_wait_hwif_ready(hwif))
		printk(KERN_WARNING "%s: Wait for ready failed before probe !\n", hwif->name);
#endif /* CONFIG_PPC */

	/*
	 * Second drive should only exist if first drive was found,
	 * but a lot of cdrom drives are configured as single slaves.
	 */
	for (unit = 0; unit < MAX_DRIVES; ++unit) {
		ide_drive_t *drive = &hwif->drives[unit];
		drive->dn = ((hwif->channel ? 2 : 0) + unit);
		hwif->drives[unit].dn = ((hwif->channel ? 2 : 0) + unit);
		(void) ide_probe_for_drive(drive);
		if (drive->present && !hwif->present) {
			hwif->present = 1;
			if (hwif->chipset != ide_4drives ||
			    !hwif->mate || 
			    !hwif->mate->present) {
				hwif_register(hwif);
			}
		}
	}
	
	ide_probe_reset(hwif);
	local_irq_restore(flags);
	/*
	 * Use cached IRQ number. It might be (and is...) changed by probe
	 * code above
	 */
	if (irqd)
		enable_irq(irqd);
		
	ide_tune_drives(hwif);

}

EXPORT_SYMBOL(probe_hwif);

#if 0
int hwif_init (ide_hwif_t *hwif);
int probe_hwif_init (ide_hwif_t *hwif)
{
	hwif->initializing = 1;
	probe_hwif(hwif);
	hwif_init(hwif);

#if 1
	if (hwif->present) {
		u16 unit = 0;
		for (unit = 0; unit < MAX_DRIVES; ++unit) {
			ide_drive_t *drive = &hwif->drives[unit];
			if (drive->present) {
				ata_attach(drive);
			}
		}
	}
#endif
	hwif->initializing = 0;
	return 0;
}

EXPORT_SYMBOL(probe_hwif_init);
#endif

#if MAX_HWIFS > 1
/*
 * save_match() is used to simplify logic in init_irq() below.
 *
 * A loophole here is that we may not know about a particular
 * hwif's irq until after that hwif is actually probed/initialized..
 * This could be a problem for the case where an hwif is on a
 * dual interface that requires serialization (eg. cmd640) and another
 * hwif using one of the same irqs is initialized beforehand.
 *
 * This routine detects and reports such situations, but does not fix them.
 */
void save_match (ide_hwif_t *hwif, ide_hwif_t *new, ide_hwif_t **match)
{
	ide_hwif_t *m = *match;

	if (m && m->hwgroup && m->hwgroup != new->hwgroup) {
		if (!new->hwgroup)
			return;
		printk("%s: potential irq problem with %s and %s\n",
			hwif->name, new->name, m->name);
	}
	if (!m || m->irq != hwif->irq) /* don't undo a prior perfect match */
		*match = new;
}
EXPORT_SYMBOL(save_match);
#endif /* MAX_HWIFS > 1 */

/*
 * init request queue
 */
static void ide_init_queue(ide_drive_t *drive)
{
	request_queue_t *q = &drive->queue;

	q->queuedata = HWGROUP(drive);
	blk_init_queue(q, do_ide_request);
	blk_queue_throttle_sectors(q, 1);
}

#undef __IRQ_HELL_SPIN
/*
 * This routine sets up the irq for an ide interface, and creates a new
 * hwgroup for the irq/hwif if none was previously assigned.
 *
 * Much of the code is for correctly detecting/handling irq sharing
 * and irq serialization situations.  This is somewhat complex because
 * it handles static as well as dynamic (PCMCIA) IDE interfaces.
 *
 * The SA_INTERRUPT in sa_flags means ide_intr() is always entered with
 * interrupts completely disabled.  This can be bad for interrupt latency,
 * but anything else has led to problems on some machines.  We re-enable
 * interrupts as much as we can safely do in most places.
 */
int init_irq (ide_hwif_t *hwif)
{
	unsigned long flags;
	unsigned int index;
	ide_hwgroup_t *hwgroup, *new_hwgroup;
	ide_hwif_t *match = NULL;

#if 0
	/* Allocate the buffer and no sleep allowed */
	new_hwgroup = kmalloc(sizeof(ide_hwgroup_t),GFP_ATOMIC);
#else
	/* Allocate the buffer and potentially sleep first */
	new_hwgroup = kmalloc(sizeof(ide_hwgroup_t),GFP_KERNEL);
#endif
	
#ifndef __IRQ_HELL_SPIN
	save_and_cli(flags);
#else
	spin_lock_irqsave(&io_request_lock, flags);
#endif

	hwif->hwgroup = NULL;
#if MAX_HWIFS > 1
	/*
	 * Group up with any other hwifs that share our irq(s).
	 */
	for (index = 0; index < MAX_HWIFS; index++) {
		ide_hwif_t *h = &ide_hwifs[index];
		if (h->hwgroup) {  /* scan only initialized hwif's */
			if (hwif->irq == h->irq) {
				hwif->sharing_irq = h->sharing_irq = 1;
				if (hwif->chipset != ide_pci ||
				    h->chipset != ide_pci) {
					save_match(hwif, h, &match);
				}
			}
			if (hwif->serialized) {
				if (hwif->mate && hwif->mate->irq == h->irq)
					save_match(hwif, h, &match);
			}
			if (h->serialized) {
				if (h->mate && hwif->irq == h->mate->irq)
					save_match(hwif, h, &match);
			}
		}
	}
#endif /* MAX_HWIFS > 1 */
	/*
	 * If we are still without a hwgroup, then form a new one
	 */
	if (match) {
		hwgroup = match->hwgroup;
		if(new_hwgroup)
			kfree(new_hwgroup);
	} else {
		hwgroup = new_hwgroup;
		if (!hwgroup) {
#ifndef __IRQ_HELL_SPIN
			restore_flags(flags);
#else
			spin_unlock_irqrestore(&io_request_lock, flags);
#endif
			return 1;
		}
		memset(hwgroup, 0, sizeof(ide_hwgroup_t));
		hwgroup->hwif     = hwif->next = hwif;
		hwgroup->rq       = NULL;
		hwgroup->handler  = NULL;
		hwgroup->drive    = NULL;
		hwgroup->busy     = 0;
		init_timer(&hwgroup->timer);
		hwgroup->timer.function = &ide_timer_expiry;
		hwgroup->timer.data = (unsigned long) hwgroup;
	}

	/*
	 * Allocate the irq, if not already obtained for another hwif
	 */
	if (!match || match->irq != hwif->irq) {
		int sa = SA_INTERRUPT;
#if defined(__mc68000__) || defined(CONFIG_APUS)
		sa = SA_SHIRQ;
#endif /* __mc68000__ || CONFIG_APUS */

		if (IDE_CHIPSET_IS_PCI(hwif->chipset)) {
			sa = SA_SHIRQ;
#ifndef CONFIG_IDEPCI_SHARE_IRQ
			sa |= SA_INTERRUPT;
#endif /* CONFIG_IDEPCI_SHARE_IRQ */
		}

		if (hwif->io_ports[IDE_CONTROL_OFFSET])
			/* clear nIEN */
			hwif->OUTB(0x08, hwif->io_ports[IDE_CONTROL_OFFSET]);

		if (request_irq(hwif->irq,&ide_intr,sa,hwif->name,hwgroup)) {
			if (!match)
				kfree(hwgroup);
#ifndef __IRQ_HELL_SPIN
			restore_flags(flags);
#else
			spin_unlock_irqrestore(&io_request_lock, flags);
#endif
			return 1;
		}
	}

	/*
	 * Everything is okay, so link us into the hwgroup
	 */
	hwif->hwgroup = hwgroup;
	hwif->next = hwgroup->hwif->next;
	hwgroup->hwif->next = hwif;

	for (index = 0; index < MAX_DRIVES; ++index) {
		ide_drive_t *drive = &hwif->drives[index];
		if (!drive->present)
			continue;
		if (!hwgroup->drive)
			hwgroup->drive = drive;
		drive->next = hwgroup->drive->next;
		hwgroup->drive->next = drive;
		ide_init_queue(drive);
	}
	if (!hwgroup->hwif) {
		hwgroup->hwif = HWIF(hwgroup->drive);
#ifdef DEBUG
		printk("%s : Adding missed hwif to hwgroup!!\n", hwif->name);
#endif
	}

	/* all CPUs; safe now that hwif->hwgroup is set up */
#ifndef __IRQ_HELL_SPIN
	restore_flags(flags);
#else
	spin_unlock_irqrestore(&io_request_lock, flags);
#endif

#if !defined(__mc68000__) && !defined(CONFIG_APUS) && !defined(__sparc__)
	printk("%s at 0x%03lx-0x%03lx,0x%03lx on irq %d", hwif->name,
		hwif->io_ports[IDE_DATA_OFFSET],
		hwif->io_ports[IDE_DATA_OFFSET]+7,
		hwif->io_ports[IDE_CONTROL_OFFSET], hwif->irq);
#elif defined(__sparc__)
	printk("%s at 0x%03lx-0x%03lx,0x%03lx on irq %s", hwif->name,
		hwif->io_ports[IDE_DATA_OFFSET],
		hwif->io_ports[IDE_DATA_OFFSET]+7,
		hwif->io_ports[IDE_CONTROL_OFFSET], __irq_itoa(hwif->irq));
#else
	printk("%s at 0x%08lx on irq %d", hwif->name,
		hwif->io_ports[IDE_DATA_OFFSET], hwif->irq);
#endif /* __mc68000__ && CONFIG_APUS */
	if (match)
		printk(" (%sed with %s)",
			hwif->sharing_irq ? "shar" : "serializ", match->name);
	printk("\n");
	return 0;
}

EXPORT_SYMBOL(init_irq);

/*
 * init_gendisk() (as opposed to ide_geninit) is called for each major device,
 * after probing for drives, to allocate partition tables and other data
 * structures needed for the routines in genhd.c.  ide_geninit() gets called
 * somewhat later, during the partition check.
 */
static void init_gendisk (ide_hwif_t *hwif)
{
	struct gendisk *gd;
	unsigned int unit, units, minors;
	int *bs, *max_sect, *max_ra;
	extern devfs_handle_t ide_devfs_handle;

	units = MAX_DRIVES;

	minors    = units * (1<<PARTN_BITS);
	gd        = kmalloc(sizeof(struct gendisk), GFP_KERNEL);
	if (!gd)
		goto err_kmalloc_gd;
	memset(gd, 0, sizeof(struct gendisk));
	
	gd->sizes = kmalloc(minors * sizeof(int), GFP_KERNEL);
	if (!gd->sizes)
		goto err_kmalloc_gd_sizes;
	gd->part  = kmalloc(minors * sizeof(struct hd_struct), GFP_KERNEL);
	if (!gd->part)
		goto err_kmalloc_gd_part;
	bs        = kmalloc(minors*sizeof(int), GFP_KERNEL);
	if (!bs)
		goto err_kmalloc_bs;
	max_sect  = kmalloc(minors*sizeof(int), GFP_KERNEL);
	if (!max_sect)
		goto err_kmalloc_max_sect;
	max_ra    = kmalloc(minors*sizeof(int), GFP_KERNEL);
	if (!max_ra)
		goto err_kmalloc_max_ra;

	memset(gd->part, 0, minors * sizeof(struct hd_struct));

	/* cdroms and msdos f/s are examples of non-1024 blocksizes */
	blksize_size[hwif->major] = bs;
	max_sectors[hwif->major] = max_sect;
	max_readahead[hwif->major] = max_ra;
	for (unit = 0; unit < minors; ++unit) {
		*bs++ = BLOCK_SIZE;
		/*
		 * IDE can do up to 128K per request == 256
		 */
		*max_sect++ = ((hwif->rqsize) ? hwif->rqsize : 128);
		*max_ra++ = vm_max_readahead;
	}

	for (unit = 0; unit < units; ++unit)
		hwif->drives[unit].part = &gd->part[unit << PARTN_BITS];

	/* our major device number */
	gd->major	= hwif->major;
	/* treated special in genhd.c */
	gd->major_name	= IDE_MAJOR_NAME;
	/* num bits for partitions */
	gd->minor_shift	= PARTN_BITS;
	/* 1 + max partitions / drive */
	gd->max_p	= 1<<PARTN_BITS;
	/* current num real drives */
	gd->nr_real	= units;
	/* ptr to internal data */
	gd->real_devices= hwif;
	/* linked list of major devs */
	gd->next	= NULL;
	/* file operations */
	gd->fops        = ide_fops;
	gd->de_arr	= kmalloc(sizeof *gd->de_arr * units, GFP_KERNEL);
	gd->flags	= kmalloc(sizeof *gd->flags * units, GFP_KERNEL);
	if (gd->de_arr)
		memset(gd->de_arr, 0, sizeof *gd->de_arr * units);
	if (gd->flags)
		memset(gd->flags, 0, sizeof *gd->flags * units);

	hwif->gd = gd;
	add_gendisk(gd);

	for (unit = 0; unit < units; ++unit) {
		char name[64];
		ide_add_generic_settings(hwif->drives + unit);
//		hwif->drives[unit].dn = ((hwif->channel ? 2 : 0) + unit);
		sprintf (name, "host%d/bus%d/target%d/lun%d",
			(hwif->channel && hwif->mate) ?
			hwif->mate->index : hwif->index,
			hwif->channel, unit, hwif->drives[unit].lun);
		if (hwif->drives[unit].present)
			hwif->drives[unit].de = devfs_mk_dir(ide_devfs_handle, name, NULL);
	}
	return;

err_kmalloc_max_ra:
	kfree(max_sect);
err_kmalloc_max_sect:
	kfree(bs);
err_kmalloc_bs:
	kfree(gd->part);
err_kmalloc_gd_part:
	kfree(gd->sizes);
err_kmalloc_gd_sizes:
	kfree(gd);
err_kmalloc_gd:
	printk(KERN_WARNING "(ide::init_gendisk) Out of memory\n");
	return;
}

EXPORT_SYMBOL(init_gendisk);

int hwif_init (ide_hwif_t *hwif)
{
	if (!hwif->present)
		return 0;
	if (!hwif->irq) {
		if (!(hwif->irq = ide_default_irq(hwif->io_ports[IDE_DATA_OFFSET])))
		{
			printk("%s: DISABLED, NO IRQ\n", hwif->name);
			return (hwif->present = 0);
		}
	}
#ifdef CONFIG_BLK_DEV_HD
	if (hwif->irq == HD_IRQ && hwif->io_ports[IDE_DATA_OFFSET] != HD_DATA) {
		printk("%s: CANNOT SHARE IRQ WITH OLD "
			"HARDDISK DRIVER (hd.c)\n", hwif->name);
		return (hwif->present = 0);
	}
#endif /* CONFIG_BLK_DEV_HD */

	/* we set it back to 1 if all is ok below */	
	hwif->present = 0;

	if (devfs_register_blkdev (hwif->major, hwif->name, ide_fops)) {
		printk("%s: UNABLE TO GET MAJOR NUMBER %d\n",
			hwif->name, hwif->major);
		return (hwif->present = 0);
	}
	
	if (init_irq(hwif)) {
		int i = hwif->irq;
		/*
		 *	It failed to initialise. Find the default IRQ for 
		 *	this port and try that.
		 */
		if (!(hwif->irq = ide_default_irq(hwif->io_ports[IDE_DATA_OFFSET]))) {
			printk("%s: Disabled unable to get IRQ %d.\n",
				hwif->name, i);
			(void) unregister_blkdev(hwif->major, hwif->name);
			return (hwif->present = 0);
		}
		if (init_irq(hwif)) {
			printk("%s: probed IRQ %d and default IRQ %d failed.\n",
				hwif->name, i, hwif->irq);
			(void) unregister_blkdev(hwif->major, hwif->name);
			return (hwif->present = 0);
		}
		printk("%s: probed IRQ %d failed, using default.\n",
			hwif->name, hwif->irq);
	}
	
	init_gendisk(hwif);
	blk_dev[hwif->major].data = hwif;
	blk_dev[hwif->major].queue = ide_get_queue;
	read_ahead[hwif->major] = 8;	/* (4kB) */
	hwif->present = 1;	/* success */

#if (DEBUG_SPINLOCK > 0)
{
	static int done = 0;
	if (!done++)
		printk("io_request_lock is %p\n", &io_request_lock);    /* FIXME */
}
#endif
	return hwif->present;
}

EXPORT_SYMBOL(hwif_init);

void export_ide_init_queue (ide_drive_t *drive)
{
	ide_init_queue(drive);
}

EXPORT_SYMBOL(export_ide_init_queue);

#ifndef HWIF_PROBE_CLASSIC_METHOD
int probe_hwif_init (ide_hwif_t *hwif)
{
	hwif->initializing = 1;
	probe_hwif(hwif);
	hwif_init(hwif);

#ifndef CLASSIC_BUILTINS_METHOD
#  ifndef FAKE_CLASSIC_ATTACH_METHOD
#    ifdef DIRECT_HWIF_PROBE_ATTACH_METHOD
	if (hwif->present) {
		u16 unit = 0;
		for (unit = 0; unit < MAX_DRIVES; ++unit) {
			ide_drive_t *drive = &hwif->drives[unit];
			if (drive->present)
				ide_attach_drive(drive);
		}
	}
#    endif /* DIRECT_HWIF_PROBE_ATTACH_METHOD */
#  endif /* FAKE_CLASSIC_ATTACH_METHOD */
#endif /* CLASSIC_BUILTINS_METHOD */
	hwif->initializing = 0;
	return 0;
}

EXPORT_SYMBOL(probe_hwif_init);

#endif /* HWIF_PROBE_CLASSIC_METHOD */

int ideprobe_init (void);
static ide_module_t ideprobe_module = {
	IDE_PROBE_MODULE,
	ideprobe_init,
	NULL
};

int ideprobe_init (void)
{
	unsigned int index;
	int probe[MAX_HWIFS];
	
	MOD_INC_USE_COUNT;
	memset(probe, 0, MAX_HWIFS * sizeof(int));
	for (index = 0; index < MAX_HWIFS; ++index)
		probe[index] = !ide_hwifs[index].present;

	/*
	 * Probe for drives in the usual way.. CMOS/BIOS, then poke at ports
	 */
#ifdef HWIF_PROBE_CLASSIC_METHOD
	for (index = 0; index < MAX_HWIFS; ++index)
		if (probe[index])
			probe_hwif(&ide_hwifs[index]);

	for (index = 0; index < MAX_HWIFS; ++index)
		if (probe[index])
			hwif_init(&ide_hwifs[index]);
#else /* HWIF_PROBE_CLASSIC_METHOD */
	for (index = 0; index < MAX_HWIFS; ++index)
		if (probe[index])
			probe_hwif_init(&ide_hwifs[index]);
#endif /* HWIF_PROBE_CLASSIC_METHOD */

	if (!ide_probe)
		ide_probe = &ideprobe_module;
	MOD_DEC_USE_COUNT;
	return 0;
}

#ifdef MODULE
extern int (*ide_xlate_1024_hook)(kdev_t, int, int, const char *);

static int ideprobe_done = 0;

int ideprobe_init_module (void)
{
	unsigned int index;

	if (ideprobe_done)
    		return -EBUSY;
	
	for (index = 0; index < MAX_HWIFS; ++index)
		ide_unregister(index);
	ideprobe_init();
	create_proc_ide_interfaces();
	ide_xlate_1024_hook = ide_xlate_1024;
	ideprobe_done++;
	return 0;
}

void ideprobe_cleanup_module (void)
{
	ide_probe = NULL;
	ide_xlate_1024_hook = 0;
}
EXPORT_SYMBOL(ideprobe_init_module);
EXPORT_SYMBOL(ideprobe_cleanup_module);
MODULE_LICENSE("GPL");
#endif /* MODULE */
