/*
 *  scsi_scan.c Copyright (C) 2000 Eric Youngdale
 *
 *  Bus scan logic.
 *
 *  This used to live in scsi.c, but that file was just a laundry basket
 *  full of misc stuff.  This got separated out in order to make things
 *  clearer.
 */

#define __NO_VERSION__
#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/blk.h>

#include "scsi.h"
#include "hosts.h"
#include "constants.h"

#ifdef CONFIG_KMOD
#include <linux/kmod.h>
#endif

/* 
 * Flags for irregular SCSI devices that need special treatment 
 */
#define BLIST_NOLUN     	0x001	/* Don't scan for LUNs */
#define BLIST_FORCELUN  	0x002	/* Known to have LUNs, force sanning */
#define BLIST_BORKEN    	0x004	/* Flag for broken handshaking */
#define BLIST_KEY       	0x008	/* Needs to be unlocked by special command */
#define BLIST_SINGLELUN 	0x010	/* LUNs should better not be used in parallel */
#define BLIST_NOTQ		0x020	/* Buggy Tagged Command Queuing */
#define BLIST_SPARSELUN 	0x040	/* Non consecutive LUN numbering */
#define BLIST_MAX5LUN		0x080	/* Avoid LUNS >= 5 */
#define BLIST_ISDISK    	0x100	/* Treat as (removable) disk */
#define BLIST_ISROM     	0x200	/* Treat as (removable) CD-ROM */
#define BLIST_LARGELUN		0x400	/* LUNs larger than 7 despite reporting as SCSI 2 */
#define BLIST_NOSTARTONADD	0x1000	/* do not do automatic start on add */


static void print_inquiry(unsigned char *data);
static int scan_scsis_single(unsigned int channel, unsigned int dev,
		unsigned int lun, int lun0_scsi_level, 
		unsigned int *max_scsi_dev, unsigned int *sparse_lun, 
		Scsi_Device ** SDpnt, struct Scsi_Host *shpnt, 
		char *scsi_result);
static int find_lun0_scsi_level(unsigned int channel, unsigned int dev,
				struct Scsi_Host *shpnt);

struct dev_info {
	const char *vendor;
	const char *model;
	const char *revision;	/* Latest revision known to be bad.  Not used yet */
	unsigned flags;
};

/*
 * This is what was previously known as the blacklist.  The concept
 * has been expanded so that we can specify other types of things we
 * need to be aware of.
 */
static struct dev_info device_list[] =
{
/* The following devices are known not to tolerate a lun != 0 scan for
 * one reason or another.  Some will respond to all luns, others will
 * lock up.
 */
	{"Aashima", "IMAGERY 2400SP", "1.03", BLIST_NOLUN},	/* Locks up if polled for lun != 0 */
	{"CHINON", "CD-ROM CDS-431", "H42", BLIST_NOLUN},	/* Locks up if polled for lun != 0 */
	{"CHINON", "CD-ROM CDS-535", "Q14", BLIST_NOLUN},	/* Locks up if polled for lun != 0 */
	{"DENON", "DRD-25X", "V", BLIST_NOLUN},			/* Locks up if probed for lun != 0 */
	{"HITACHI", "DK312C", "CM81", BLIST_NOLUN},		/* Responds to all lun - dtg */
	{"HITACHI", "DK314C", "CR21", BLIST_NOLUN},		/* responds to all lun */
	{"IMS", "CDD521/10", "2.06", BLIST_NOLUN},		/* Locks-up when LUN>0 polled. */
	{"MAXTOR", "XT-3280", "PR02", BLIST_NOLUN},		/* Locks-up when LUN>0 polled. */
	{"MAXTOR", "XT-4380S", "B3C", BLIST_NOLUN},		/* Locks-up when LUN>0 polled. */
	{"MAXTOR", "MXT-1240S", "I1.2", BLIST_NOLUN},		/* Locks up when LUN>0 polled */
	{"MAXTOR", "XT-4170S", "B5A", BLIST_NOLUN},		/* Locks-up sometimes when LUN>0 polled. */
	{"MAXTOR", "XT-8760S", "B7B", BLIST_NOLUN},		/* guess what? */
	{"MEDIAVIS", "RENO CD-ROMX2A", "2.03", BLIST_NOLUN},	/*Responds to all lun */
	{"NEC", "CD-ROM DRIVE:841", "1.0", BLIST_NOLUN},	/* Locks-up when LUN>0 polled. */
	{"PHILIPS", "PCA80SC", "V4-2", BLIST_NOLUN},		/* Responds to all lun */
	{"RODIME", "RO3000S", "2.33", BLIST_NOLUN},		/* Locks up if polled for lun != 0 */
	{"SANYO", "CRD-250S", "1.20", BLIST_NOLUN},		/* causes failed REQUEST SENSE on lun 1
								 * for aha152x controller, which causes
								 * SCSI code to reset bus.*/
	{"SEAGATE", "ST157N", "\004|j", BLIST_NOLUN},		/* causes failed REQUEST SENSE on lun 1
								 * for aha152x controller, which causes
								 * SCSI code to reset bus.*/
	{"SEAGATE", "ST296", "921", BLIST_NOLUN},		/* Responds to all lun */
	{"SEAGATE", "ST1581", "6538", BLIST_NOLUN},		/* Responds to all lun */
	{"SONY", "CD-ROM CDU-541", "4.3d", BLIST_NOLUN},	
	{"SONY", "CD-ROM CDU-55S", "1.0i", BLIST_NOLUN},
	{"SONY", "CD-ROM CDU-561", "1.7x", BLIST_NOLUN},
	{"SONY", "CD-ROM CDU-8012", "*", BLIST_NOLUN},
	{"TANDBERG", "TDC 3600", "U07", BLIST_NOLUN},		/* Locks up if polled for lun != 0 */
	{"TEAC", "CD-R55S", "1.0H", BLIST_NOLUN},		/* Locks up if polled for lun != 0 */
	{"TEAC", "CD-ROM", "1.06", BLIST_NOLUN},		/* causes failed REQUEST SENSE on lun 1
								 * for seagate controller, which causes
								 * SCSI code to reset bus.*/
	{"TEAC", "MT-2ST/45S2-27", "RV M", BLIST_NOLUN},	/* Responds to all lun */
	{"TEXEL", "CD-ROM", "1.06", BLIST_NOLUN},		/* causes failed REQUEST SENSE on lun 1
								 * for seagate controller, which causes
								 * SCSI code to reset bus.*/
	{"QUANTUM", "LPS525S", "3110", BLIST_NOLUN},		/* Locks sometimes if polled for lun != 0 */
	{"QUANTUM", "PD1225S", "3110", BLIST_NOLUN},		/* Locks sometimes if polled for lun != 0 */
	{"QUANTUM", "FIREBALL ST4.3S", "0F0C", BLIST_NOLUN},	/* Locks up when polled for lun != 0 */
	{"MEDIAVIS", "CDR-H93MV", "1.31", BLIST_NOLUN},		/* Locks up if polled for lun != 0 */
	{"SANKYO", "CP525", "6.64", BLIST_NOLUN},		/* causes failed REQ SENSE, extra reset */
	{"HP", "C1750A", "3226", BLIST_NOLUN},			/* scanjet iic */
	{"HP", "C1790A", "", BLIST_NOLUN},			/* scanjet iip */
	{"HP", "C2500A", "", BLIST_NOLUN},			/* scanjet iicx */
	{"HP", "A6188A", "*", BLIST_SPARSELUN | BLIST_LARGELUN},/* HP Va7100 Array */
	{"HP", "A6189A", "*", BLIST_SPARSELUN | BLIST_LARGELUN},/* HP Va7400 Array */
	{"HP", "A6189B", "*", BLIST_SPARSELUN | BLIST_LARGELUN},/* HP Va7110 Array */
	{"HP", "A6218A", "*", BLIST_SPARSELUN | BLIST_LARGELUN},/* HP Va7410 Array */
	{"YAMAHA", "CDR100", "1.00", BLIST_NOLUN},		/* Locks up if polled for lun != 0 */
	{"YAMAHA", "CDR102", "1.00", BLIST_NOLUN},		/* Locks up if polled for lun != 0  
								 * extra reset */
	{"YAMAHA", "CRW8424S", "1.0", BLIST_NOLUN},		/* Locks up if polled for lun != 0 */
	{"YAMAHA", "CRW6416S", "1.0c", BLIST_NOLUN},		/* Locks up if polled for lun != 0 */
	{"MITSUMI", "CD-R CR-2201CS", "6119", BLIST_NOLUN},	/* Locks up if polled for lun != 0 */
	{"RELISYS", "Scorpio", "*", BLIST_NOLUN},		/* responds to all LUN */
	{"RELISYS", "VM3530+", "*", BLIST_NOLUN},		/* responds to all LUN */
	{"ACROSS", "", "*", BLIST_NOLUN},			/* responds to all LUN */
	{"MICROTEK", "ScanMaker II", "5.61", BLIST_NOLUN},	/* responds to all LUN */

/*
 * Other types of devices that have special flags.
 */
	{"SONY", "CD-ROM CDU-8001", "*", BLIST_BORKEN},
	{"TEXEL", "CD-ROM", "1.06", BLIST_BORKEN},
	{"IOMEGA", "Io20S         *F", "*", BLIST_KEY},
	{"INSITE", "Floptical   F*8I", "*", BLIST_KEY},
	{"INSITE", "I325VM", "*", BLIST_KEY},
	{"LASOUND","CDX7405","3.10", BLIST_MAX5LUN | BLIST_SINGLELUN},
	{"MICROP", "4110", "*", BLIST_NOTQ},			/* Buggy Tagged Queuing */
	{"NRC", "MBR-7", "*", BLIST_FORCELUN | BLIST_SINGLELUN},
	{"NRC", "MBR-7.4", "*", BLIST_FORCELUN | BLIST_SINGLELUN},
	{"REGAL", "CDC-4X", "*", BLIST_MAX5LUN | BLIST_SINGLELUN},
	{"NAKAMICH", "MJ-4.8S", "*", BLIST_FORCELUN | BLIST_SINGLELUN},
	{"NAKAMICH", "MJ-5.16S", "*", BLIST_FORCELUN | BLIST_SINGLELUN},
	{"PIONEER", "CD-ROM DRM-600", "*", BLIST_FORCELUN | BLIST_SINGLELUN},
	{"PIONEER", "CD-ROM DRM-602X", "*", BLIST_FORCELUN | BLIST_SINGLELUN},
	{"PIONEER", "CD-ROM DRM-604X", "*", BLIST_FORCELUN | BLIST_SINGLELUN},
	{"EMULEX", "MD21/S2     ESDI", "*", BLIST_SINGLELUN},
	{"CANON", "IPUBJD", "*", BLIST_SPARSELUN},
	{"nCipher", "Fastness Crypto", "*", BLIST_FORCELUN},
	{"DEC","HSG80","*", BLIST_FORCELUN | BLIST_NOSTARTONADD},
	{"COMPAQ","LOGICAL VOLUME","*", BLIST_FORCELUN},
	{"COMPAQ","CR3500","*", BLIST_FORCELUN},
	{"NEC", "PD-1 ODX654P", "*", BLIST_FORCELUN | BLIST_SINGLELUN},
	{"MATSHITA", "PD-1", "*", BLIST_FORCELUN | BLIST_SINGLELUN},
	{"iomega", "jaz 1GB", "J.86", BLIST_NOTQ | BLIST_NOLUN},
 	{"TOSHIBA","CDROM","*", BLIST_ISROM},
 	{"TOSHIBA","CD-ROM","*", BLIST_ISROM},
	{"MegaRAID", "LD", "*", BLIST_FORCELUN},
	{"DGC",  "RAID",      "*", BLIST_SPARSELUN | BLIST_LARGELUN}, // Dell PV 650F (tgt @ LUN 0)
	{"DGC",  "DISK",      "*", BLIST_SPARSELUN | BLIST_LARGELUN}, // Dell PV 650F (no tgt @ LUN 0) 
	{"DELL", "PV660F",   "*", BLIST_SPARSELUN | BLIST_LARGELUN},
	{"DELL", "PV660F   PSEUDO",   "*", BLIST_SPARSELUN | BLIST_LARGELUN},
	{"DELL", "PSEUDO DEVICE .",   "*", BLIST_SPARSELUN | BLIST_LARGELUN}, // Dell PV 530F
	{"DELL", "PV530F",    "*", BLIST_SPARSELUN | BLIST_LARGELUN}, // Dell PV 530F
	{"EMC", "SYMMETRIX", "*", BLIST_SPARSELUN | BLIST_LARGELUN | BLIST_FORCELUN},
	{"HP", "A6189A", "*", BLIST_SPARSELUN |  BLIST_LARGELUN}, // HP VA7400, by Alar Aun
	{"HP", "OPEN-", "*", BLIST_SPARSELUN | BLIST_LARGELUN},	/* HP XP Arrays */
	{"CMD", "CRA-7280", "*", BLIST_SPARSELUN | BLIST_LARGELUN},   // CMD RAID Controller
	{"CNSI", "G7324", "*", BLIST_SPARSELUN | BLIST_LARGELUN},     // Chaparral G7324 RAID
	{"CNSi", "G8324", "*", BLIST_SPARSELUN | BLIST_LARGELUN},     // Chaparral G8324 RAID
	{"Zzyzx", "RocketStor 500S", "*", BLIST_SPARSELUN},
	{"Zzyzx", "RocketStor 2000", "*", BLIST_SPARSELUN},
	{"SONY", "TSL",       "*", BLIST_FORCELUN},  // DDS3 & DDS4 autoloaders
	{"DELL", "PERCRAID", "*", BLIST_FORCELUN},
	{"HP", "NetRAID-4M", "*", BLIST_FORCELUN},
	{"ADAPTEC", "AACRAID", "*", BLIST_FORCELUN},
	{"ADAPTEC", "Adaptec 5400S", "*", BLIST_FORCELUN},
	{"APPLE", "Xserve", "*", BLIST_SPARSELUN | BLIST_LARGELUN},
	{"COMPAQ", "MSA1000", "*", BLIST_SPARSELUN | BLIST_LARGELUN | BLIST_NOSTARTONADD},
	{"COMPAQ", "MSA1000 VOLUME", "*", BLIST_SPARSELUN | BLIST_LARGELUN | BLIST_NOSTARTONADD},
	{"COMPAQ", "HSV110", "*", BLIST_SPARSELUN | BLIST_LARGELUN | BLIST_NOSTARTONADD},
	{"HP", "HSV100", "*", BLIST_SPARSELUN | BLIST_LARGELUN | BLIST_NOSTARTONADD},
	{"HP", "C1557A", "*", BLIST_FORCELUN},
	{"IBM", "AuSaV1S2", "*", BLIST_FORCELUN},
	{"FSC", "CentricStor", "*", BLIST_SPARSELUN | BLIST_LARGELUN},
	{"DDN", "SAN DataDirector", "*", BLIST_SPARSELUN},
	{"HITACHI", "DF400", "*", BLIST_SPARSELUN},
	{"HITACHI", "DF500", "*", BLIST_SPARSELUN},
	{"HITACHI", "DF600", "*", BLIST_SPARSELUN},
	{"IBM", "ProFibre 4000R", "*", BLIST_SPARSELUN | BLIST_LARGELUN},
	{"HITACHI", "OPEN-", "*", BLIST_SPARSELUN | BLIST_LARGELUN},  /* HITACHI XP Arrays */
	{"HITACHI", "DISK-SUBSYSTEM", "*", BLIST_SPARSELUN | BLIST_LARGELUN},  /* HITACHI 9960 */
	{"WINSYS","FLASHDISK G6", "*", BLIST_SPARSELUN},
	{"DotHill","SANnet RAID X300", "*", BLIST_SPARSELUN},	
	{"SUN", "T300", "*", BLIST_SPARSELUN},
	{"SUN", "T4", "*", BLIST_SPARSELUN},
	{"SGI", "RAID3", "*", BLIST_SPARSELUN | BLIST_LARGELUN},
	{"SGI", "RAID5", "*", BLIST_SPARSELUN | BLIST_LARGELUN},
	{"SGI", "TP9100", "*", BLIST_SPARSELUN | BLIST_LARGELUN},
	{"SGI", "TP9300", "*", BLIST_SPARSELUN | BLIST_LARGELUN},
	{"SGI", "TP9400", "*", BLIST_SPARSELUN | BLIST_LARGELUN},
	{"SGI", "TP9500", "*", BLIST_SPARSELUN | BLIST_LARGELUN},
	{"MYLEX", "DACARMRB", "*", BLIST_SPARSELUN | BLIST_LARGELUN},
	{"PLATYPUS", "CX5", "*", BLIST_SPARSELUN | BLIST_LARGELUN},
	{"Raidtec", "FCR", "*", BLIST_SPARSELUN | BLIST_LARGELUN},
	{"HP", "C7200", "*", BLIST_SPARSELUN},			/* Medium Changer */
	{"SMSC", "USB 2 HS", "*", BLIST_SPARSELUN | BLIST_LARGELUN}, 
	{"XYRATEX", "RS", "*", BLIST_SPARSELUN | BLIST_LARGELUN},
	{"NEC", "iStorage", "*", BLIST_SPARSELUN | BLIST_LARGELUN | BLIST_FORCELUN},

	/*
	 * Must be at end of list...
	 */
	{NULL, NULL, NULL}
};

#define MAX_SCSI_LUNS 0xFFFFFFFF

#ifdef CONFIG_SCSI_MULTI_LUN
static unsigned int max_scsi_luns = MAX_SCSI_LUNS;
#else
static unsigned int max_scsi_luns = 1;
#endif

static unsigned int scsi_allow_ghost_devices = 0;

#ifdef MODULE

MODULE_PARM(max_scsi_luns, "i");
MODULE_PARM_DESC(max_scsi_luns, "last scsi LUN (should be between 1 and 2^32-1)");
MODULE_PARM(scsi_allow_ghost_devices, "i");
MODULE_PARM_DESC(scsi_allow_ghost_devices, "allow devices marked as being offline to be accessed anyway (0 = off, else allow ghosts on lun 0 through scsi_allow_ghost_devices - 1");

#else

static int __init scsi_luns_setup(char *str)
{
	unsigned int tmp;

	if (get_option(&str, &tmp) == 1) {
		max_scsi_luns = tmp;
		return 1;
	} else {
		printk("scsi_luns_setup : usage max_scsi_luns=n "
		       "(n should be between 1 and 2^32-1)\n");
		return 0;
	}
}

__setup("max_scsi_luns=", scsi_luns_setup);

static int __init scsi_allow_ghost_devices_setup(char *str)
{
	unsigned int tmp;

	if (get_option(&str, &tmp) == 1) {
		scsi_allow_ghost_devices = tmp;
		return 1;
	} else {
		printk("scsi_allow_ghost_devices_setup: usage scsi_allow_ghost_devices=n (0: off else\nallow ghost devices (ghost devices are devices that report themselves as\nbeing offline but which we allow access to anyway) on lun 0 through n - 1.\n");
		return 0;
	}
}

__setup("scsi_allow_ghost_devices=", scsi_allow_ghost_devices_setup);

#endif

static void print_inquiry(unsigned char *data)
{
	int i;

	printk("  Vendor: ");
	for (i = 8; i < 16; i++) {
		if (data[i] >= 0x20 && i < data[4] + 5)
			printk("%c", data[i]);
		else
			printk(" ");
	}

	printk("  Model: ");
	for (i = 16; i < 32; i++) {
		if (data[i] >= 0x20 && i < data[4] + 5)
			printk("%c", data[i]);
		else
			printk(" ");
	}

	printk("  Rev: ");
	for (i = 32; i < 36; i++) {
		if (data[i] >= 0x20 && i < data[4] + 5)
			printk("%c", data[i]);
		else
			printk(" ");
	}

	printk("\n");

	i = data[0] & 0x1f;

	printk("  Type:   %s ",
	       i < MAX_SCSI_DEVICE_CODE ? scsi_device_types[i] : "Unknown          ");
	printk("                 ANSI SCSI revision: %02x", data[2] & 0x07);
	if ((data[2] & 0x07) == 1 && (data[3] & 0x0f) == 1)
		printk(" CCS\n");
	else
		printk("\n");
}

static int get_device_flags(unsigned char *response_data)
{
	int i = 0;
	unsigned char *pnt;
	for (i = 0; 1; i++) {
		if (device_list[i].vendor == NULL)
			return 0;
		pnt = &response_data[8];
		while (*pnt && *pnt == ' ')
			pnt++;
		if (memcmp(device_list[i].vendor, pnt,
			   strlen(device_list[i].vendor)))
			continue;
		pnt = &response_data[16];
		while (*pnt && *pnt == ' ')
			pnt++;
		if (memcmp(device_list[i].model, pnt,
			   strlen(device_list[i].model)))
			continue;
		return device_list[i].flags;
	}
	return 0;
}

/*
 *  Detecting SCSI devices :
 *  We scan all present host adapter's busses,  from ID 0 to ID (max_id).
 *  We use the INQUIRY command, determine device type, and pass the ID /
 *  lun address of all sequential devices to the tape driver, all random
 *  devices to the disk driver.
 */
void scan_scsis(struct Scsi_Host *shpnt,
		       uint hardcoded,
		       uint hchannel,
		       uint hid,
		       uint hlun)
{
	uint channel;
	unsigned int dev;
	unsigned int lun;
	unsigned int max_dev_lun;
	unsigned char *scsi_result;
	unsigned char scsi_result0[256];
	Scsi_Device *SDpnt;
	Scsi_Device *SDtail;
	unsigned int sparse_lun;
	int lun0_sl;

	scsi_result = NULL;

	SDpnt = (Scsi_Device *) kmalloc(sizeof(Scsi_Device),
					GFP_ATOMIC);
	if (SDpnt) {
		memset(SDpnt, 0, sizeof(Scsi_Device));
		/*
		 * Register the queue for the device.  All I/O requests will
		 * come in through here.  We also need to register a pointer to
		 * ourselves, since the queue handler won't know what device
		 * the queue actually represents.   We could look it up, but it
		 * is pointless work.
		 */
		scsi_initialize_queue(SDpnt, shpnt);
		SDpnt->request_queue.queuedata = (void *) SDpnt;
		/* Make sure we have something that is valid for DMA purposes */
		scsi_result = ((!shpnt->unchecked_isa_dma)
			       ? &scsi_result0[0] : kmalloc(512, GFP_DMA));
	}

	if (scsi_result == NULL) {
		printk("Unable to obtain scsi_result buffer\n");
		goto leave;
	}
	/*
	 * We must chain ourself in the host_queue, so commands can time out 
	 */
	SDpnt->queue_depth = 1;
	SDpnt->host = shpnt;
	SDpnt->online = TRUE;

	initialize_merge_fn(SDpnt);

        /*
         * Initialize the object that we will use to wait for command blocks.
         */
	init_waitqueue_head(&SDpnt->scpnt_wait);

	/*
	 * Next, hook the device to the host in question.
	 */
	SDpnt->prev = NULL;
	SDpnt->next = NULL;
	if (shpnt->host_queue != NULL) {
		SDtail = shpnt->host_queue;
		while (SDtail->next != NULL)
			SDtail = SDtail->next;

		SDtail->next = SDpnt;
		SDpnt->prev = SDtail;
	} else {
		shpnt->host_queue = SDpnt;
	}

	/*
	 * We need to increment the counter for this one device so we can track
	 * when things are quiet.
	 */
	if (hardcoded == 1) {
		Scsi_Device *oldSDpnt = SDpnt;
		struct Scsi_Device_Template *sdtpnt;
		channel = hchannel;
		if (channel > shpnt->max_channel)
			goto leave;
		dev = hid;
		if (dev >= shpnt->max_id)
			goto leave;
		lun = hlun;
		if (lun >= shpnt->max_lun)
			goto leave;
		if ((0 == lun) || (lun > 7))
			lun0_sl = SCSI_3; /* actually don't care for 0 == lun */
		else
			lun0_sl = find_lun0_scsi_level(channel, dev, shpnt);
		scan_scsis_single(channel, dev, lun, lun0_sl, &max_dev_lun, 
				  &sparse_lun, &SDpnt, shpnt, scsi_result);
		if (SDpnt != oldSDpnt) {

			/* it could happen the blockdevice hasn't yet been inited */
			/* queue_depth() moved from scsi_proc_info() so that
			   it is called before scsi_build_commandblocks() */
			if (shpnt->select_queue_depths != NULL)
				(shpnt->select_queue_depths)(shpnt,
							     shpnt->host_queue);

			for (sdtpnt = scsi_devicelist; sdtpnt; sdtpnt = sdtpnt->next)
				if (sdtpnt->init && sdtpnt->dev_noticed)
					(*sdtpnt->init) ();

			for (sdtpnt = scsi_devicelist; sdtpnt; sdtpnt = sdtpnt->next) {
				if (sdtpnt->attach) {
					(*sdtpnt->attach) (oldSDpnt);
					if (oldSDpnt->attached) {
						scsi_build_commandblocks(oldSDpnt);
						if (0 == oldSDpnt->has_cmdblocks) {
							printk("scan_scsis: DANGER, no command blocks\n");
							/* What to do now ?? */
						}
					}
				}
			}
			scsi_resize_dma_pool();

			for (sdtpnt = scsi_devicelist; sdtpnt; sdtpnt = sdtpnt->next) {
				if (sdtpnt->finish && sdtpnt->nr_dev) {
					(*sdtpnt->finish) ();
				}
			}
		}
	} else {
		/* Actual LUN. PC ordering is 0->n IBM/spec ordering is n->0 */
		int order_dev;

		for (channel = 0; channel <= shpnt->max_channel; channel++) {
			for (dev = 0; dev < shpnt->max_id; ++dev) {
				if (shpnt->reverse_ordering)
					/* Shift to scanning 15,14,13... or 7,6,5,4, */
					order_dev = shpnt->max_id - dev - 1;
				else
					order_dev = dev;

				if (shpnt->this_id != order_dev) {

					/*
					 * We need the for so our continue, etc. work fine. We put this in
					 * a variable so that we can override it during the scan if we
					 * detect a device *KNOWN* to have multiple logical units.
					 */
					max_dev_lun = (max_scsi_luns < shpnt->max_lun ?
					 max_scsi_luns : shpnt->max_lun);
					sparse_lun = 0;
					for (lun = 0, lun0_sl = SCSI_2; lun < max_dev_lun; ++lun) {
						/* don't probe further for luns > 7 for targets <= SCSI_2 */
						if ((lun0_sl < SCSI_3) && (lun > 7))
							break;

						if (!scan_scsis_single(channel, order_dev, lun, lun0_sl,
							 	       &max_dev_lun, &sparse_lun, &SDpnt, shpnt,
								       scsi_result)
						    && !sparse_lun)
							break;	/* break means don't probe further for luns!=0 */
						if (SDpnt && (0 == lun)) {
							int bflags = get_device_flags (scsi_result);
							if (bflags & BLIST_LARGELUN)
								lun0_sl = SCSI_3; /* treat as SCSI 3 */
							else
								lun0_sl = SDpnt->scsi_level;
						}
					}	/* for lun ends */
				}	/* if this_id != id ends */
			}	/* for dev ends */
		}		/* for channel ends */
	}			/* if/else hardcoded */

      leave:

	{			/* Unchain SRpnt from host_queue */
		Scsi_Device *prev, *next;
		Scsi_Device *dqptr;

		for (dqptr = shpnt->host_queue; dqptr != SDpnt; dqptr = dqptr->next)
			continue;
		if (dqptr) {
			prev = dqptr->prev;
			next = dqptr->next;
			if (prev)
				prev->next = next;
			else
				shpnt->host_queue = next;
			if (next)
				next->prev = prev;
		}
	}

	/* Last device block does not exist.  Free memory. */
	if (SDpnt != NULL) {
		blk_cleanup_queue(&SDpnt->request_queue);
		kfree((char *) SDpnt);
	}

	/* If we allocated a buffer so we could do DMA, free it now */
	if (scsi_result != &scsi_result0[0] && scsi_result != NULL) {
		kfree(scsi_result);
	} {
		Scsi_Device *sdev;
		Scsi_Cmnd *scmd;

		SCSI_LOG_SCAN_BUS(4, printk("Host status for host %p:\n", shpnt));
		for (sdev = shpnt->host_queue; sdev; sdev = sdev->next) {
			SCSI_LOG_SCAN_BUS(4, printk("Device %d %p: ", sdev->id, sdev));
			for (scmd = sdev->device_queue; scmd; scmd = scmd->next) {
				SCSI_LOG_SCAN_BUS(4, printk("%p ", scmd));
			}
			SCSI_LOG_SCAN_BUS(4, printk("\n"));
		}
	}
}

/*
 * The worker for scan_scsis.
 * Returning 0 means Please don't ask further for lun!=0, 1 means OK go on.
 * Global variables used : scsi_devices(linked list)
 */
static int scan_scsis_single(unsigned int channel, unsigned int dev,
		unsigned int lun, int lun0_scsi_level,
		unsigned int *max_dev_lun, unsigned int *sparse_lun, 
		Scsi_Device ** SDpnt2, struct Scsi_Host *shpnt, 
		char *scsi_result)
{
	char devname[64];
	unsigned char scsi_cmd[MAX_COMMAND_SIZE];
	struct Scsi_Device_Template *sdtpnt;
	Scsi_Device *SDtail, *SDpnt = *SDpnt2;
	Scsi_Request * SRpnt;
	int bflags, type = -1;
	extern devfs_handle_t scsi_devfs_handle;
	int scsi_level;

	SDpnt->host = shpnt;
	SDpnt->id = dev;
	SDpnt->lun = lun;
	SDpnt->channel = channel;
	SDpnt->online = TRUE;

	scsi_build_commandblocks(SDpnt);
 
	/* Some low level driver could use device->type (DB) */
	SDpnt->type = -1;

	/*
	 * Assume that the device will have handshaking problems, and then fix
	 * this field later if it turns out it doesn't
	 */
	SDpnt->borken = 1;
	SDpnt->was_reset = 0;
	SDpnt->expecting_cc_ua = 0;
	SDpnt->starved = 0;

	if (NULL == (SRpnt = scsi_allocate_request(SDpnt))) {
		printk("scan_scsis_single: no memory\n");
		return 0;
	}

	/*
	 * We used to do a TEST_UNIT_READY before the INQUIRY but that was 
	 * not really necessary.  Spec recommends using INQUIRY to scan for
	 * devices (and TEST_UNIT_READY to poll for media change). - Paul G.
	 */

	SCSI_LOG_SCAN_BUS(3, printk("scsi: performing INQUIRY\n"));
	/*
	 * Build an INQUIRY command block.
	 */
	scsi_cmd[0] = INQUIRY;
	if ((lun > 0) && (lun0_scsi_level <= SCSI_2))
		scsi_cmd[1] = (lun << 5) & 0xe0;
	else	
		scsi_cmd[1] = 0;	/* SCSI_3 and higher, don't touch */
	scsi_cmd[2] = 0;
	scsi_cmd[3] = 0;
	scsi_cmd[4] = 255;
	scsi_cmd[5] = 0;
	SRpnt->sr_cmd_len = 0;
	SRpnt->sr_data_direction = SCSI_DATA_READ;

	scsi_wait_req (SRpnt, (void *) scsi_cmd,
	          (void *) scsi_result,
	          256, SCSI_TIMEOUT+4*HZ, 3);

	SCSI_LOG_SCAN_BUS(3, printk("scsi: INQUIRY %s with code 0x%x\n",
		SRpnt->sr_result ? "failed" : "successful", SRpnt->sr_result));

	/*
	 * Now that we don't do TEST_UNIT_READY anymore, we must be prepared
	 * for media change conditions here, so cannot require zero result.
	 */
	if (SRpnt->sr_result) {
		if ((driver_byte(SRpnt->sr_result) & DRIVER_SENSE) != 0 &&
		    (SRpnt->sr_sense_buffer[2] & 0xf) == UNIT_ATTENTION &&
		    SRpnt->sr_sense_buffer[12] == 0x28 &&
		    SRpnt->sr_sense_buffer[13] == 0) {
			/* not-ready to ready transition - good */
		} else {
			/* assume no peripheral if any other sort of error */
			scsi_release_request(SRpnt);
			scsi_release_commandblocks(SDpnt);
			return 0;
		}
	}

	/*
	 * Check for SPARSELUN before checking the peripheral qualifier,
	 * so sparse lun devices are completely scanned.
	 */

	/*
	 * If we are offline and we are on a LUN != 0, then skip this entry.
	 * If we are on a BLIST_FORCELUN device this will stop the scan at
	 * the first offline LUN (typically the correct thing to do).  If
	 * we are on a BLIST_SPARSELUN device then this won't stop the scan,
	 * but it will keep us from having false entries in our device
	 * array. DL
	 *
	 * NOTE: Need to test this to make sure it doesn't cause problems
	 * with tape autoloaders, multidisc CD changers, and external
	 * RAID chassis that might use sparse luns or multiluns... DL
	 */
	if (lun != 0 && (scsi_result[0] >> 5) == 1) {
		scsi_release_request(SRpnt);
		scsi_release_commandblocks(SDpnt);
		return 0;
	}

	/*
	 * Get any flags for this device.  
	 */
	bflags = get_device_flags (scsi_result);

	if (bflags & BLIST_SPARSELUN) {
	  *sparse_lun = 1;
	}
	/*
	 * Check the peripheral qualifier field - this tells us whether LUNS
	 * are supported here or not.
	 */
	if ((scsi_result[0] >> 5) == 3) {
		scsi_release_request(SRpnt);
		return 0;	/* assume no peripheral if any sort of error */
	}
	 /*   The Toshiba ROM was "gender-changed" here as an inline hack.
	      This is now much more generic.
	      This is a mess: What we really want is to leave the scsi_result
	      alone, and just change the SDpnt structure. And the SDpnt is what
	      we want print_inquiry to print.  -- REW
	 */
	if (bflags & BLIST_ISDISK) {
		scsi_result[0] = TYPE_DISK;                                                
		scsi_result[1] |= 0x80;     /* removable */
	}

	if (bflags & BLIST_ISROM) {
		scsi_result[0] = TYPE_ROM;
		scsi_result[1] |= 0x80;     /* removable */
	}
    
	memcpy(SDpnt->vendor, scsi_result + 8, 8);
	memcpy(SDpnt->model, scsi_result + 16, 16);
	memcpy(SDpnt->rev, scsi_result + 32, 4);

	SDpnt->removable = (0x80 & scsi_result[1]) >> 7;
	/* Use the peripheral qualifier field to determine online/offline */
	if ((((scsi_result[0] >> 5) & 7) == 1) &&
	    (lun >= scsi_allow_ghost_devices))
		SDpnt->online = FALSE;
	else 
		SDpnt->online = TRUE;
	SDpnt->lockable = SDpnt->removable;
	SDpnt->changed = 0;
	SDpnt->access_count = 0;
	SDpnt->busy = 0;
	SDpnt->has_cmdblocks = 0;
	/*
	 * Currently, all sequential devices are assumed to be tapes, all random
	 * devices disk, with the appropriate read only flags set for ROM / WORM
	 * treated as RO.
	 */
	switch (type = (scsi_result[0] & 0x1f)) {
	case TYPE_TAPE:
	case TYPE_DISK:
	case TYPE_PRINTER:
	case TYPE_MOD:
	case TYPE_PROCESSOR:
	case TYPE_SCANNER:
	case TYPE_MEDIUM_CHANGER:
	case TYPE_ENCLOSURE:
	case TYPE_COMM:
		SDpnt->writeable = 1;
		break;
	case TYPE_WORM:
	case TYPE_ROM:
		SDpnt->writeable = 0;
		break;
	default:
		printk("scsi: unknown type %d\n", type);
	}

	SDpnt->device_blocked = FALSE;
	SDpnt->device_busy = 0;
	SDpnt->single_lun = 0;
	SDpnt->soft_reset =
	    (scsi_result[7] & 1) && ((scsi_result[3] & 7) == 2);
	SDpnt->random = (type == TYPE_TAPE) ? 0 : 1;
	SDpnt->type = (type & 0x1f);

	print_inquiry(scsi_result);

        sprintf (devname, "host%d/bus%d/target%d/lun%d",
                 SDpnt->host->host_no, SDpnt->channel, SDpnt->id, SDpnt->lun);
        if (SDpnt->de) printk ("DEBUG: dir: \"%s\" already exists\n", devname);
        else SDpnt->de = devfs_mk_dir (scsi_devfs_handle, devname, NULL);

	for (sdtpnt = scsi_devicelist; sdtpnt;
	     sdtpnt = sdtpnt->next)
		if (sdtpnt->detect)
			SDpnt->attached +=
			    (*sdtpnt->detect) (SDpnt);

	SDpnt->scsi_level = scsi_result[2] & 0x07;
	if (SDpnt->scsi_level >= 2 ||
	    (SDpnt->scsi_level == 1 &&
	     (scsi_result[3] & 0x0f) == 1))
		SDpnt->scsi_level++;
	scsi_level = SDpnt->scsi_level;

	/*
	 * Accommodate drivers that want to sleep when they should be in a polling
	 * loop.
	 */
	SDpnt->disconnect = 0;


	/*
	 * Set the tagged_queue flag for SCSI-II devices that purport to support
	 * tagged queuing in the INQUIRY data.
	 */
	SDpnt->tagged_queue = 0;
	if ((SDpnt->scsi_level >= SCSI_2) &&
	    (scsi_result[7] & 2) &&
	    !(bflags & BLIST_NOTQ)) {
		SDpnt->tagged_supported = 1;
		SDpnt->current_tag = 0;
	}
	/*
	 * Some revisions of the Texel CD ROM drives have handshaking problems when
	 * used with the Seagate controllers.  Before we know what type of device
	 * we're talking to, we assume it's borken and then change it here if it
	 * turns out that it isn't a TEXEL drive.
	 */
	if ((bflags & BLIST_BORKEN) == 0)
		SDpnt->borken = 0;

 	/*
	 * Some devices may not want to have a start command automatically
	 * issued when a device is added.
	 */
	if (bflags & BLIST_NOSTARTONADD)
		SDpnt->no_start_on_add = 1;

	/*
	 * If we want to only allow I/O to one of the luns attached to this device
	 * at a time, then we set this flag.
	 */
	if (bflags & BLIST_SINGLELUN)
		SDpnt->single_lun = 1;

	/*
	 * These devices need this "key" to unlock the devices so we can use it
	 */
	if ((bflags & BLIST_KEY) != 0) {
		printk("Unlocked floptical drive.\n");
		SDpnt->lockable = 0;
		scsi_cmd[0] = MODE_SENSE;
		if (shpnt->max_lun <= 8)
			scsi_cmd[1] = (lun << 5) & 0xe0;
		else	scsi_cmd[1] = 0;	/* any other idea? */
		scsi_cmd[2] = 0x2e;
		scsi_cmd[3] = 0;
		scsi_cmd[4] = 0x2a;
		scsi_cmd[5] = 0;
		SRpnt->sr_cmd_len = 0;
		SRpnt->sr_data_direction = SCSI_DATA_READ;
		scsi_wait_req (SRpnt, (void *) scsi_cmd,
	        	(void *) scsi_result, 0x2a,
	        	SCSI_TIMEOUT, 3);
	}

	scsi_release_request(SRpnt);
	SRpnt = NULL;

	scsi_release_commandblocks(SDpnt);

	/*
	 * This device was already hooked up to the host in question,
	 * so at this point we just let go of it and it should be fine.  We do need to
	 * allocate a new one and attach it to the host so that we can further scan the bus.
	 */
	SDpnt = (Scsi_Device *) kmalloc(sizeof(Scsi_Device), GFP_ATOMIC);
	if (!SDpnt) {
		printk("scsi: scan_scsis_single: Cannot malloc\n");
		return 0;
	}
        memset(SDpnt, 0, sizeof(Scsi_Device));

	*SDpnt2 = SDpnt;
	SDpnt->queue_depth = 1;
	SDpnt->host = shpnt;
	SDpnt->online = TRUE;
	SDpnt->scsi_level = scsi_level;

	/*
	 * Register the queue for the device.  All I/O requests will come
	 * in through here.  We also need to register a pointer to
	 * ourselves, since the queue handler won't know what device
	 * the queue actually represents.   We could look it up, but it
	 * is pointless work.
	 */
	scsi_initialize_queue(SDpnt, shpnt);
	SDpnt->host = shpnt;
	initialize_merge_fn(SDpnt);

	/*
	 * Mark this device as online, or otherwise we won't be able to do much with it.
	 */
	SDpnt->online = TRUE;

        /*
         * Initialize the object that we will use to wait for command blocks.
         */
	init_waitqueue_head(&SDpnt->scpnt_wait);

	/*
	 * Since we just found one device, there had damn well better be one in the list
	 * already.
	 */
	if (shpnt->host_queue == NULL)
		panic("scan_scsis_single: Host queue == NULL\n");

	SDtail = shpnt->host_queue;
	while (SDtail->next) {
		SDtail = SDtail->next;
	}

	/* Add this device to the linked list at the end */
	SDtail->next = SDpnt;
	SDpnt->prev = SDtail;
	SDpnt->next = NULL;

	/*
	 * Some scsi devices cannot be polled for lun != 0 due to firmware bugs
	 */
	if (bflags & BLIST_NOLUN)
		return 0;	/* break; */

	/*
	 * If this device is known to support sparse multiple units, override the
	 * other settings, and scan all of them.
	 */
	if (bflags & BLIST_SPARSELUN) {
		*max_dev_lun = shpnt->max_lun;
		*sparse_lun = 1;
		return 1;
	}
	/*
	 * If this device is known to support multiple units, override the other
	 * settings, and scan all of them.
	 */
	if (bflags & BLIST_FORCELUN) {
		/* 
		 * Scanning MAX_SCSI_LUNS units would be a bad idea.
		 * Any better idea?
		 * I think we need REPORT LUNS in future to avoid scanning
		 * of unused LUNs. But, that is another item.
		 */
		/*
		if (*max_dev_lun < shpnt->max_lun)
			*max_dev_lun = shpnt->max_lun;
		else 	if ((max_scsi_luns >> 1) >= *max_dev_lun)
				*max_dev_lun += shpnt->max_lun;
			else	*max_dev_lun = max_scsi_luns;
		*/
		/*
		 * Blech...the above code is broken.  When you have a device
		 * that is present, and it is a FORCELUN device, then we
		 * need to scan *all* the luns on that device.  Besides,
		 * skipping the scanning of LUNs is a false optimization.
		 * Scanning for a LUN on a present device is a very fast
		 * operation, it's scanning for devices that don't exist that
		 * is expensive and slow (although if you are truly scanning
		 * through MAX_SCSI_LUNS devices that would be bad, I hope
		 * all of the controllers out there set a reasonable value
		 * in shpnt->max_lun).  DL
		 */
		*max_dev_lun = shpnt->max_lun;
		return 1;
	}
	/*
	 * REGAL CDC-4X: avoid hang after LUN 4
	 */
	if (bflags & BLIST_MAX5LUN) {
		*max_dev_lun = 5;
		return 1;
	}

	/*
	 * We assume the device can't handle lun!=0 if: - it reports scsi-0
	 * (ANSI SCSI Revision 0) (old drives like MAXTOR XT-3280) or - it
	 * reports scsi-1 (ANSI SCSI Revision 1) and Response Data Format 0
	 */
	if (((scsi_result[2] & 0x07) == 0)
	    ||
	    ((scsi_result[2] & 0x07) == 1 &&
	     (scsi_result[3] & 0x0f) == 0))
		return 0;
	return 1;
}

/*
 * The worker for scan_scsis.
 * Returns the scsi_level of lun0 on this host, channel and dev (if already
 * known), otherwise returns SCSI_2.
 */
static int find_lun0_scsi_level(unsigned int channel, unsigned int dev,
				struct Scsi_Host *shpnt)
{
	int res = SCSI_2;
	Scsi_Device *SDpnt;

	for (SDpnt = shpnt->host_queue; SDpnt; SDpnt = SDpnt->next)
	{
		if ((0 == SDpnt->lun) && (dev == SDpnt->id) &&
		    (channel == SDpnt->channel))
			return (int)SDpnt->scsi_level;
	}
	/* haven't found lun0, should send INQUIRY but take easy route */
	return res;
}
