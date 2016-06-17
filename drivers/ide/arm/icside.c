/*
 * linux/drivers/ide/arm/icside.c
 *
 * Copyright (c) 1996,1997 Russell King.
 *
 * Changelog:
 *  08-Jun-1996	RMK	Created
 *  12-Sep-1997	RMK	Added interrupt enable/disable
 *  17-Apr-1999	RMK	Added support for V6 EASI
 *  22-May-1999	RMK	Added support for V6 DMA
 */

#include <linux/config.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/errno.h>
#include <linux/hdreg.h>
#include <linux/ide.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <asm/dma.h>
#include <asm/ecard.h>
#include <asm/io.h>

#include "ide-noise.h"

/*
 * FIXME: We want to drop the the MACRO CRAP!
 *
 * ec->iops->in{b/w/l}
 * ec->iops->in{b/w/l}_p
 * ec->iops->out{b/w/l}
 * ec->iops->out{b/w/l}_p
 *
 * the new core supports clean MMIO calls and other goodies
 */

/*
 * Maximum number of interfaces per card
 */
#define MAX_IFS	2

#define ICS_IDENT_OFFSET		0x8a0

#define ICS_ARCIN_V5_INTRSTAT		0x000
#define ICS_ARCIN_V5_INTROFFSET		0x001
#define ICS_ARCIN_V5_IDEOFFSET		0xa00
#define ICS_ARCIN_V5_IDEALTOFFSET	0xae0
#define ICS_ARCIN_V5_IDESTEPPING	4

#define ICS_ARCIN_V6_IDEOFFSET_1	0x800
#define ICS_ARCIN_V6_INTROFFSET_1	0x880
#define ICS_ARCIN_V6_INTRSTAT_1		0x8a4
#define ICS_ARCIN_V6_IDEALTOFFSET_1	0x8e0
#define ICS_ARCIN_V6_IDEOFFSET_2	0xc00
#define ICS_ARCIN_V6_INTROFFSET_2	0xc80
#define ICS_ARCIN_V6_INTRSTAT_2		0xca4
#define ICS_ARCIN_V6_IDEALTOFFSET_2	0xce0
#define ICS_ARCIN_V6_IDESTEPPING	4

struct cardinfo {
	unsigned int dataoffset;
	unsigned int ctrloffset;
	unsigned int stepping;
};

static struct cardinfo icside_cardinfo_v5 = {
	ICS_ARCIN_V5_IDEOFFSET,
	ICS_ARCIN_V5_IDEALTOFFSET,
	ICS_ARCIN_V5_IDESTEPPING
};

static struct cardinfo icside_cardinfo_v6_1 = {
	ICS_ARCIN_V6_IDEOFFSET_1,
	ICS_ARCIN_V6_IDEALTOFFSET_1,
	ICS_ARCIN_V6_IDESTEPPING
};

static struct cardinfo icside_cardinfo_v6_2 = {
	ICS_ARCIN_V6_IDEOFFSET_2,
	ICS_ARCIN_V6_IDEALTOFFSET_2,
	ICS_ARCIN_V6_IDESTEPPING
};

static const card_ids icside_cids[] = {
	{ MANU_ICS,  PROD_ICS_IDE  },
	{ MANU_ICS2, PROD_ICS2_IDE },
	{ 0xffff, 0xffff }
};

typedef enum {
	ics_if_unknown,
	ics_if_arcin_v5,
	ics_if_arcin_v6
} iftype_t;

/* ---------------- Version 5 PCB Support Functions --------------------- */
/* Prototype: icside_irqenable_arcin_v5 (struct expansion_card *ec, int irqnr)
 * Purpose  : enable interrupts from card
 */
static void icside_irqenable_arcin_v5 (struct expansion_card *ec, int irqnr)
{
	unsigned int memc_port = (unsigned int)ec->irq_data;
	outb(0, memc_port + ICS_ARCIN_V5_INTROFFSET);
}

/* Prototype: icside_irqdisable_arcin_v5 (struct expansion_card *ec, int irqnr)
 * Purpose  : disable interrupts from card
 */
static void icside_irqdisable_arcin_v5 (struct expansion_card *ec, int irqnr)
{
	unsigned int memc_port = (unsigned int)ec->irq_data;
	inb(memc_port + ICS_ARCIN_V5_INTROFFSET);
}

static const expansioncard_ops_t icside_ops_arcin_v5 = {
	icside_irqenable_arcin_v5,
	icside_irqdisable_arcin_v5,
	NULL,
	NULL,
	NULL,
	NULL
};


/* ---------------- Version 6 PCB Support Functions --------------------- */
/* Prototype: icside_irqenable_arcin_v6 (struct expansion_card *ec, int irqnr)
 * Purpose  : enable interrupts from card
 */
static void icside_irqenable_arcin_v6 (struct expansion_card *ec, int irqnr)
{
	unsigned int ide_base_port = (unsigned int)ec->irq_data;

	outb(0, ide_base_port + ICS_ARCIN_V6_INTROFFSET_1);
	outb(0, ide_base_port + ICS_ARCIN_V6_INTROFFSET_2);
}

/* Prototype: icside_irqdisable_arcin_v6 (struct expansion_card *ec, int irqnr)
 * Purpose  : disable interrupts from card
 */
static void icside_irqdisable_arcin_v6 (struct expansion_card *ec, int irqnr)
{
	unsigned int ide_base_port = (unsigned int)ec->irq_data;

	inb(ide_base_port + ICS_ARCIN_V6_INTROFFSET_1);
	inb(ide_base_port + ICS_ARCIN_V6_INTROFFSET_2);
}

/* Prototype: icside_irqprobe(struct expansion_card *ec)
 * Purpose  : detect an active interrupt from card
 */
static int icside_irqpending_arcin_v6(struct expansion_card *ec)
{
	unsigned int ide_base_port = (unsigned int)ec->irq_data;

	return inb(ide_base_port + ICS_ARCIN_V6_INTRSTAT_1) & 1 ||
	       inb(ide_base_port + ICS_ARCIN_V6_INTRSTAT_2) & 1;
}

static const expansioncard_ops_t icside_ops_arcin_v6 = {
	icside_irqenable_arcin_v6,
	icside_irqdisable_arcin_v6,
	icside_irqpending_arcin_v6,
	NULL,
	NULL,
	NULL
};

/* Prototype: icside_identifyif (struct expansion_card *ec)
 * Purpose  : identify IDE interface type
 * Notes    : checks the description string
 */
static iftype_t __init icside_identifyif (struct expansion_card *ec)
{
	unsigned int addr;
	iftype_t iftype;
	int id = 0;

	iftype = ics_if_unknown;

	addr = ecard_address (ec, ECARD_IOC, ECARD_FAST) + ICS_IDENT_OFFSET;

	id = inb(addr) & 1;
	id |= (inb(addr + 1) & 1) << 1;
	id |= (inb(addr + 2) & 1) << 2;
	id |= (inb(addr + 3) & 1) << 3;

	switch (id) {
	case 0: /* A3IN */
		printk("icside: A3IN unsupported\n");
		break;

	case 1: /* A3USER */
		printk("icside: A3USER unsupported\n");
		break;

	case 3:	/* ARCIN V6 */
		printk(KERN_DEBUG "icside: detected ARCIN V6 in slot %d\n", ec->slot_no);
		iftype = ics_if_arcin_v6;
		break;

	case 15:/* ARCIN V5 (no id) */
		printk(KERN_DEBUG "icside: detected ARCIN V5 in slot %d\n", ec->slot_no);
		iftype = ics_if_arcin_v5;
		break;

	default:/* we don't know - complain very loudly */
		printk("icside: ***********************************\n");
		printk("icside: *** UNKNOWN ICS INTERFACE id=%d ***\n", id);
		printk("icside: ***********************************\n");
		printk("icside: please report this to linux@arm.linux.org.uk\n");
		printk("icside: defaulting to ARCIN V5\n");
		iftype = ics_if_arcin_v5;
		break;
	}

	return iftype;
}

#ifdef CONFIG_BLK_DEV_IDEDMA_ICS
/*
 * SG-DMA support.
 *
 * Similar to the BM-DMA, but we use the RiscPCs IOMD DMA controllers.
 * There is only one DMA controller per card, which means that only
 * one drive can be accessed at one time.  NOTE! We do not enforce that
 * here, but we rely on the main IDE driver spotting that both
 * interfaces use the same IRQ, which should guarantee this.
 */
#define NR_ENTRIES 256
#define TABLE_SIZE (NR_ENTRIES * 8)

static int ide_build_sglist(ide_hwif_t *hwif, struct request *rq)
{
	struct buffer_head *bh;
	struct scatterlist *sg = hwif->sg_table;
	int nents = 0;

	if (rq->cmd == READ)
		hwif->sg_dma_direction = PCI_DMA_FROMDEVICE;
	else
		hwif->sg_dma_direction = PCI_DMA_TODEVICE;
	bh = rq->bh;
	do {
		unsigned char *virt_addr = bh->b_data;
		unsigned int size = bh->b_size;

		while ((bh = bh->b_reqnext) != NULL) {
			if ((virt_addr + size) != (unsigned char *)bh->b_data)
				break;
			size += bh->b_size;
		}
		memset(&sg[nents], 0, sizeof(*sg));
		sg[nents].address = virt_addr;
		sg[nents].length = size;
		nents++;
	} while (bh != NULL);

	return pci_map_sg(NULL, sg, nents, hwif->sg_dma_direction);
}

static int
icside_build_dmatable(ide_drive_t *drive, int ddir)
{
	return HWIF(drive)->sg_nents = ide_build_sglist(HWIF(drive), HWGROUP(drive)->rq, ddir);
}

/* Teardown mappings after DMA has completed.  */
static void icside_destroy_dmatable(ide_drive_t *drive)
{
	struct scatterlist *sg = HWIF(drive)->sg_table;
	int nents = HWIF(drive)->sg_nents;

	pci_unmap_sg(NULL, sg, nents, HWIF(drive)->sg_dma_direction);
}

static int
icside_config_if(ide_drive_t *drive, int xfer_mode)
{
	int func = ide_dma_off;

	switch (xfer_mode) {
	case XFER_MW_DMA_2:
		/*
		 * The cycle time is limited to 250ns by the r/w
		 * pulse width (90ns), however we should still
		 * have a maximum burst transfer rate of 8MB/s.
		 */
		drive->drive_data = 250;
		break;

	case XFER_MW_DMA_1:
		drive->drive_data = 250;
		break;

	case XFER_MW_DMA_0:
		drive->drive_data = 480;
		break;

	default:
		drive->drive_data = 0;
		break;
	}

	if (!drive->init_speed)
		drive->init_speed = (u8) xfer_mode;

	if (drive->drive_data &&
	    ide_config_drive_speed(drive, (u8) xfer_mode) == 0)
		func = ide_dma_on;
	else
		drive->drive_data = 480;

	printk("%s: %s selected (peak %dMB/s)\n", drive->name,
		ide_xfer_verbose(xfer_mode), 2000 / drive->drive_data);

	drive->current_speed = (u8) xfer_mode;

	return func;
}

static int
icside_set_speed(ide_drive_t *drive, u8 speed)
{
	return icside_config_if(drive, speed);
}

/*
 * dma_intr() is the handler for disk read/write DMA interrupts
 */
static ide_startstop_t icside_dmaintr(ide_drive_t *drive)
{
	u8 dma_stat	= HWIF(drive)->ide_dma_end(drive);
	/* get drive status */
	u8 stat		= HWIF(drive)->INB(IDE_STATUS_REG);
	int i;

	if (OK_STAT(stat,DRIVE_READY,drive->bad_wstat|DRQ_STAT)) {
		if (!dma_stat) {
			struct request *rq = HWGROUP(drive)->rq;
			rq = HWGROUP(drive)->rq;
			for (i = rq->nr_sectors; i > 0;) {
				i -= rq->current_nr_sectors;
				DRIVER(drive)->end_request(drive, 1);
			}
			return ide_stopped;
		}
		printk("%s: dma_intr: bad DMA status (dma_stat=%x)\n", 
		       drive->name, dma_stat);
	}
	return DRIVER(drive)->error(drive, "dma_intr", stat);
}

/*
 * The following is a sick duplication from ide-dma.c ;(
 *
 * This should be defined in one place only.
 */
struct drive_list_entry {
	char * id_model;
	char * id_firmware;
};

static struct drive_list_entry drive_whitelist [] = {
	{ "Micropolis 2112A",			"ALL"		},
	{ "CONNER CTMA 4000",			"ALL"		},
	{ "CONNER CTT8000-A",			"ALL"		},
	{ "ST34342A",				"ALL"		},
	{ NULL,					0		}
};

static struct drive_list_entry drive_blacklist [] = {
	{ "WDC AC11000H",			"ALL"		},
	{ "WDC AC22100H",			"ALL"		},
	{ "WDC AC32500H",			"ALL"		},
	{ "WDC AC33100H",			"ALL"		},
	{ "WDC AC31600H",			"ALL"		},
	{ "WDC AC32100H",			"24.09P07"	},
	{ "WDC AC23200L",			"21.10N21"	},
	{ "Compaq CRD-8241B",			"ALL"		},
	{ "CRD-8400B",				"ALL"		},
	{ "CRD-8480B",				"ALL"		},
	{ "CRD-8480C",				"ALL"		},
	{ "CRD-8482B",				"ALL"		},
 	{ "CRD-84",				"ALL"		},
	{ "SanDisk SDP3B",			"ALL"		},
	{ "SanDisk SDP3B-64",			"ALL"		},
	{ "SANYO CD-ROM CRD",			"ALL"		},
	{ "HITACHI CDR-8",			"ALL"		},
	{ "HITACHI CDR-8335",			"ALL"		},
	{ "HITACHI CDR-8435",			"ALL"		},
	{ "Toshiba CD-ROM XM-6202B",		"ALL"		},
	{ "CD-532E-A",				"ALL"		},
	{ "E-IDE CD-ROM CR-840",		"ALL"		},
	{ "CD-ROM Drive/F5A",			"ALL"		},
	{ "RICOH CD-R/RW MP7083A",		"ALL"		},
	{ "WPI CDD-820",			"ALL"		},
	{ "SAMSUNG CD-ROM SC-148C",		"ALL"		},
	{ "SAMSUNG CD-ROM SC-148F",		"ALL"		},
	{ "SAMSUNG CD-ROM SC",			"ALL"		},
	{ "SanDisk SDP3B-64",			"ALL"		},
	{ "SAMSUNG CD-ROM SN-124",		"ALL"		},
	{ "PLEXTOR CD-R PX-W8432T",		"ALL"		},
	{ "ATAPI CD-ROM DRIVE 40X MAXIMUM",	"ALL"		},
	{ "_NEC DV5800A",			"ALL"		},
	{ NULL,					0		}
};

static int in_drive_list(struct hd_driveid *id, struct drive_list_entry * drive_table)
{
	for ( ; drive_table->id_model ; drive_table++)
		if ((!strcmp(drive_table->id_model, id->model)) &&
		    ((!strstr(drive_table->id_firmware, id->fw_rev)) ||
		     (!strcmp(drive_table->id_firmware, "ALL"))))
			return 1;
	return 0;
}

/*
 *  For both Blacklisted and Whitelisted drives.
 *  This is setup to be called as an extern for future support
 *  to other special driver code.
 */
int check_drive_good_lists (ide_drive_t *drive)
{
	struct hd_driveid *id = drive->id;
	return in_drive_list(id, drive_whitelist);
}

int check_drive_bad_lists (ide_drive_t *drive)
{
	struct hd_driveid *id = drive->id;
	int blacklist = in_drive_list(id, drive_blacklist);
	if (blacklist)
		printk("%s: Disabling DMA for %s\n", drive->name, id->model);
	return(blacklist);
}

int icside_dma_check(ide_drive_t *drive)
{
	struct hd_driveid *id = drive->id;
	ide_hwif_t *hwif = HWIF(drive);
	int autodma = hwif->autodma;
	int xfer_mode = XFER_PIO_2;

	if (!id || !(id->capability & 1) || !autodma)
		return hwif->ide_dma_off_quietly(drive);

	/*
	 * Consult the list of known "bad" drives
	 */
	if (check_drive_bad_lists(drive))
		return hwif->ide_dma_off(drive);

	/*
	 * Enable DMA on any drive that has multiword DMA
	 */
	if (id->field_valid & 2) {
		if (id->dma_mword & 4) {
			xfer_mode = XFER_MW_DMA_2;
		} else if (id->dma_mword & 2) {
			xfer_mode = XFER_MW_DMA_1;
		} else if (id->dma_mword & 1) {
			xfer_mode = XFER_MW_DMA_0;
		}
		goto out;
	}

	/*
	 * Consult the list of known "good" drives
	 */
	if (check_drive_good_lists(drive)) {
		if (id->eide_dma_time > 150)
			goto out;
		xfer_mode = XFER_MW_DMA_1;
	}

out:
	if (icside_config_if(drive, xfer_mode))
		return hwif->ide_dma_on(drive);
	return hwif->ide_dma_off(drive);
}

int icside_dma_verbose(ide_drive_t *drive)
{
	printk(", DMA");
	return 1;
}

int icside_dma_test_irq(ide_drive_t *drive)
{
	ide_hwif_t *hwif = HWIF(drive);
	return inb((unsigned long)hwif->hw.priv) & 1;
}

int icside_dma_host_off(ide_drive_t *drive)
{
	return 0;
}

int icside_dma_off_quietly(ide_drive_t *drive)
{
	drive->using_dma = 0;
	return icside_dma_host_off(drive);
}

int icside_dma_off(ide_drive_t *drive)
{
	printk("%s: DMA disabled\n", drive->name);
	return icside_dma_off_quietly(drive);
}

int icside_dma_host_on(ide_drive_t *drive)
{
	return 0;
}

int icside_dma_on(ide_drive_t *drive)
{
	drive->using_dma = 1;
	return icside_dma_host_on(drive);
}

int icside_dma_begin(ide_drive_t *drive)
{
	ide_hwif_t *hwif = HWIF(drive);

	enable_dma(hwif->hw.dma);
	return 0;
}

int icside_dma_end(ide_drive_t *drive)
{
	ide_hwif_t *hwif = HWIF(drive);
 
	drive->waiting_for_dma = 0;
	disable_dma(hwif->hw.dma);
	icside_destroy_dmatable(drive);
	return get_dma_residue(hwif->hw.dma) != 0;
}

int icside_dma_count (ide_drive_t *drive)
{
        return icside_dma_begin(drive);
}

int icside_dma_read(ide_drive_t *drive)
{
	ide_hwif_t *hwif	= HWIF(drive);
//	ide_task_t *args	= HWGROUP(drive)->rq->special;
	int count		= 0;
	u8 lba48		= (drive->addressing == 1) ? 1 : 0;
	task_ioreg_t command	= WIN_NOP;

	count = icside_build_dmatable(drive, PCI_DMA_FROMDEVICE);
	if (!count)
		return 1;
	disable_dma(hwif->hw.dma);

	/* Route the DMA signals to
	 * to the correct interface.
	 */
	HWIF(drive)->OUTB(hwif->select_data, hwif->config_data);

	/* Select the correct timing
	 * for this drive
	 */
	set_dma_speed(hwif->hw.dma, drive->drive_data);

	set_dma_sg(hwif->hw.dma, HWIF(drive)->sg_table, count);
	set_dma_mode(hwif->hw.dma, DMA_MODE_READ);

	drive->waiting_for_dma = 1;
	if (drive->media != ide_disk)
		return 0;

	if (HWGROUP(drive)->handler != NULL)	/* paranoia check */
		BUG();
	ide_set_handler(drive, &icside_dmaintr, WAIT_CMD, NULL);
	/*
	 * FIX ME to use only ACB ide_task_t args Struct
	 */
#if 0
	{
		ide_task_t *args = HWGROUP(drive)->rq->special;
		command = args->tfRegister[IDE_COMMAND_OFFSET];
	}
#else
	command = (lba48) ? WIN_READDMA_EXT : WIN_READDMA;
		if (HWGROUP(drive)->rq->flags & REQ_DRIVE_TASKFILE) {
		ide_task_t *args = HWGROUP(drive)->rq->special;
		command = args->tfRegister[IDE_COMMAND_OFFSET];
	}
#endif
	/* issue cmd to drive */
	HWIF(drive)->OUTB(command, IDE_COMMAND_REG);

	return icside_dma_count(drive);
}

int icside_dma_write(ide_drive_t *drive)
{
	ide_hwif_t *hwif	= HWIF(drive);
//	ide_task_t *args	= HWGROUP(drive)->rq->special;
	int count		= 0;
	u8 lba48		= (drive->addressing == 1) ? 1 : 0;
	task_ioreg_t command	= WIN_NOP;

	count = icside_build_dmatable(drive, PCI_DMA_TODEVICE);
	if (!count)
		return 1;
	disable_dma(hwif->hw.dma);

	/* Route the DMA signals to
	 * to the correct interface.
	 */
	HWIF(drive)->OUTB(hwif->select_data, hwif->config_data);

	/* Select the correct timing
	 * for this drive
	 */
	set_dma_speed(hwif->hw.dma, drive->drive_data);

	set_dma_sg(hwif->hw.dma, HWIF(drive)->sg_table, count);
	set_dma_mode(hwif->hw.dma, DMA_MODE_WRITE);

	drive->waiting_for_dma = 1;
	if (drive->media != ide_disk)
		return 0;

	if (HWGROUP(drive)->handler != NULL)
		BUG();
	ide_set_handler(drive, &icside_dmaintr, WAIT_CMD, NULL);
	/*
	 * FIX ME to use only ACB ide_task_t args Struct
	 */
#if 0
	{
		ide_task_t *args = HWGROUP(drive)->rq->special;
		command = args->tfRegister[IDE_COMMAND_OFFSET];
	}
#else
	command = (lba48) ? WIN_WRITEDMA_EXT : WIN_WRITEDMA;
	if (HWGROUP(drive)->rq->flags & REQ_DRIVE_TASKFILE) {
		ide_task_t *args = HWGROUP(drive)->rq->special;
		command = args->tfRegister[IDE_COMMAND_OFFSET];
	}
#endif
	/* issue cmd to drive */
	HWIF(drive)->OUTB(command, IDE_COMMAND_REG);

	return icside_dma_count(drive);
}

static int
icside_setup_dma(ide_hwif_t *hwif, int autodma)
{
	printk("    %s: SG-DMA", hwif->name);

	hwif->sg_table = kmalloc(sizeof(struct scatterlist) * NR_ENTRIES,
				 GFP_KERNEL);
	if (!hwif->sg_table)
		goto failed;

	hwif->dmatable_cpu = NULL;
	hwif->dmatable_dma = 0;
	hwif->speedproc = icside_set_speed;
	hwif->autodma = autodma;

	hwif->ide_dma_check = icside_dma_check;
	hwif->ide_dma_host_off = icside_dma_host_off;
	hwif->ide_dma_off_quietly = icside_dma_off_quietly;
	hwif->ide_dma_off = icside_dma_off;
	hwif->ide_dma_host_on = icside_dma_host_on;
	hwif->ide_dma_on = icside_dma_on;
	hwif->ide_dma_read = icside_dma_read;
	hwif->ide_dma_write = icside_dma_write;
	hwif->ide_dma_count = icside_dma_count;
	hwif->ide_dma_begin = icside_dma_begin;
	hwif->ide_dma_end = icside_dma_end;
	hwif->ide_dma_verbose = icside_dma_verbose;
	hwif->ide_dma_bad_drive = check_drive_bad_lists;
	hwif->ide_dma_good_drive = check_drive_good_lists;
	hwif->ide_dma_test_irq = icside_dma_test_irq;

	printk(" capable%s\n", autodma ?
		", auto-enable" : "");

	return 1;

failed:
	printk(" -- ERROR, unable to allocate DMA table\n");
	return 0;
}
#endif

static ide_hwif_t *
icside_find_hwif(unsigned long dataport)
{
	ide_hwif_t *hwif;
	int index;

	for (index = 0; index < MAX_HWIFS; ++index) {
		hwif = &ide_hwifs[index];
		if (hwif->io_ports[IDE_DATA_OFFSET] == (ide_ioreg_t)dataport)
			goto found;
	}

	for (index = 0; index < MAX_HWIFS; ++index) {
		hwif = &ide_hwifs[index];
		if (!hwif->io_ports[IDE_DATA_OFFSET])
			goto found;
	}

	return NULL;
found:
	return hwif;
}

static ide_hwif_t *
icside_setup(unsigned long base, struct cardinfo *info, int irq)
{
	unsigned long port = base + info->dataoffset;
	ide_hwif_t *hwif;

	hwif = icside_find_hwif(base);
	if (hwif) {
		int i;

		memset(&hwif->hw, 0, sizeof(hw_regs_t));

		for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++) {
			hwif->hw.io_ports[i] = (ide_ioreg_t)port;
			hwif->io_ports[i] = (ide_ioreg_t)port;
			port += 1 << info->stepping;
		}
		hwif->hw.io_ports[IDE_CONTROL_OFFSET] = base + info->ctrloffset;
		hwif->io_ports[IDE_CONTROL_OFFSET] = base + info->ctrloffset;
		hwif->hw.irq  = irq;
		hwif->irq     = irq;
		hwif->hw.dma  = NO_DMA;
		hwif->noprobe = 0;
		hwif->chipset = ide_acorn;
	}

	return hwif;
}

static int __init icside_register_v5(struct expansion_card *ec, int autodma)
{
	unsigned long slot_port;
	ide_hwif_t *hwif;

	slot_port = ecard_address(ec, ECARD_MEMC, 0);

	ec->irqaddr  = (unsigned char *)ioaddr(slot_port + ICS_ARCIN_V5_INTRSTAT);
	ec->irqmask  = 1;
	ec->irq_data = (void *)slot_port;
	ec->ops      = (expansioncard_ops_t *)&icside_ops_arcin_v5;

	/*
	 * Be on the safe side - disable interrupts
	 */
	inb(slot_port + ICS_ARCIN_V5_INTROFFSET);

	hwif = icside_setup(slot_port, &icside_cardinfo_v5, ec->irq);

	return hwif ? 0 : -1;
}

static int __init icside_register_v6(struct expansion_card *ec, int autodma)
{
	unsigned long slot_port, port;
	ide_hwif_t *hwif, *mate;
	int sel = 0;

	slot_port = ecard_address(ec, ECARD_IOC, ECARD_FAST);
	port      = ecard_address(ec, ECARD_EASI, ECARD_FAST);

	if (port == 0)
		port = slot_port;
	else
		sel = 1 << 5;

	outb(sel, slot_port);

	ec->irq_data = (void *)port;
	ec->ops      = (expansioncard_ops_t *)&icside_ops_arcin_v6;

	/*
	 * Be on the safe side - disable interrupts
	 */
	inb(port + ICS_ARCIN_V6_INTROFFSET_1);
	inb(port + ICS_ARCIN_V6_INTROFFSET_2);

	hwif = icside_setup(port, &icside_cardinfo_v6_1, ec->irq);
	mate = icside_setup(port, &icside_cardinfo_v6_2, ec->irq);

#ifdef CONFIG_BLK_DEV_IDEDMA_ICS
	if (ec->dma != NO_DMA) {
		if (request_dma(ec->dma, hwif->name))
			goto no_dma;

		if (hwif) {
			hwif->config_data = slot_port;
			hwif->select_data = sel;
			hwif->hw.dma  = ec->dma;
			hwif->hw.priv = (void *)
					(port + ICS_ARCIN_V6_INTRSTAT_1);
			hwif->channel = 0;
			icside_setup_dma(hwif, autodma);
			hwif->drives[0].autodma = autodma;
			hwif->drives[1].autodma = autodma;
		}
		if (mate) {
			mate->config_data = slot_port;
			mate->select_data = sel | 1;
			mate->hw.dma  = ec->dma;
			mate->hw.priv = (void *)
					(port + ICS_ARCIN_V6_INTRSTAT_2);
			mate->channel = 1;
			icside_setup_dma(mate, autodma);
			mate->drives[0].autodma = autodma;
			mate->drives[1].autodma = autodma;
		}
	}
no_dma:
#endif
	return hwif || mate ? 0 : -1;
}

int __init icside_init(void)
{
	int autodma = 0;

#ifdef CONFIG_IDEDMA_ICS_AUTO
	autodma = 1;
#endif

	ecard_startfind ();

	do {
		struct expansion_card *ec;
		int result;

		ec = ecard_find(0, icside_cids);
		if (ec == NULL)
			break;

		ecard_claim(ec);

		switch (icside_identifyif(ec)) {
		case ics_if_arcin_v5:
			result = icside_register_v5(ec, autodma);
			break;

		case ics_if_arcin_v6:
			result = icside_register_v6(ec, autodma);
			break;

		default:
			result = -1;
			break;
		}

		if (result)
			ecard_release(ec);
	} while (1);

	return 0;
}
