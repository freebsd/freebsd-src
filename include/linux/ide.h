#ifndef _IDE_H
#define _IDE_H
/*
 *  linux/include/linux/ide.h
 *
 *  Copyright (C) 1994-2002  Linus Torvalds & authors
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/hdreg.h>
#include <linux/hdsmart.h>
#include <linux/blkdev.h>
#include <linux/proc_fs.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/highmem.h>
#include <linux/pci.h>
#include <asm/byteorder.h>
#include <asm/system.h>
#include <asm/hdreg.h>
#include <asm/io.h>

#define CLASSIC_BUILTINS_METHOD				1
//#undef CLASSIC_BUILTINS_METHOD
#define HWIF_PROBE_CLASSIC_METHOD			1
//#undef HWIF_PROBE_CLASSIC_METHOD

#ifdef CLASSIC_BUILTINS_METHOD
#  undef FAKE_CLASSIC_ATTACH_METHOD
#  undef DIRECT_HWIF_PROBE_ATTACH_METHOD
#else /* ! CLASSIC_BUILTINS_METHOD */
#  ifdef HWIF_PROBE_CLASSIC_METHOD
#    undef FAKE_CLASSIC_ATTACH_METHOD
#    undef DIRECT_HWIF_PROBE_ATTACH_METHOD
#  else /* ! HWIF_PROBE_CLASSIC_METHOD */
//#    define FAKE_CLASSIC_ATTACH_METHOD			1
#    undef FAKE_CLASSIC_ATTACH_METHOD
#    ifdef FAKE_CLASSIC_ATTACH_METHOD
#      undef DIRECT_HWIF_PROBE_ATTACH_METHOD
#    else /* ! FAKE_CLASSIC_ATTACH_METHOD */
#      define DIRECT_HWIF_PROBE_ATTACH_METHOD		1
#    endif /* FAKE_CLASSIC_ATTACH_METHOD */
#  endif /* HWIF_PROBE_CLASSIC_METHOD */
#endif /* CLASSIC_BUILTINS_METHOD */

/*
 * This is the multiple IDE interface driver, as evolved from hd.c.
 * It supports up to four IDE interfaces, on one or more IRQs (usually 14 & 15).
 * There can be up to two drives per interface, as per the ATA-2 spec.
 *
 * Primary i/f:    ide0: major=3;  (hda)         minor=0; (hdb)         minor=64
 * Secondary i/f:  ide1: major=22; (hdc or hd1a) minor=0; (hdd or hd1b) minor=64
 * Tertiary i/f:   ide2: major=33; (hde)         minor=0; (hdf)         minor=64
 * Quaternary i/f: ide3: major=34; (hdg)         minor=0; (hdh)         minor=64
 */

/******************************************************************************
 * IDE driver configuration options (play with these as desired):
 *
 * REALLY_SLOW_IO can be defined in ide.c and ide-cd.c, if necessary
 */
#undef REALLY_FAST_IO			/* define if ide ports are perfect */
#define INITIAL_MULT_COUNT	0	/* off=0; on=2,4,8,16,32, etc.. */

#ifndef SUPPORT_SLOW_DATA_PORTS		/* 1 to support slow data ports */
#define SUPPORT_SLOW_DATA_PORTS	1	/* 0 to reduce kernel size */
#endif
#ifndef SUPPORT_VLB_SYNC		/* 1 to support weird 32-bit chips */
#define SUPPORT_VLB_SYNC	1	/* 0 to reduce kernel size */
#endif
#ifndef DISK_RECOVERY_TIME		/* off=0; on=access_delay_time */
#define DISK_RECOVERY_TIME	0	/*  for hardware that needs it */
#endif
#ifndef OK_TO_RESET_CONTROLLER		/* 1 needed for good error recovery */
#define OK_TO_RESET_CONTROLLER	1	/* 0 for use with AH2372A/B interface */
#endif
#ifndef FANCY_STATUS_DUMPS		/* 1 for human-readable drive errors */
#define FANCY_STATUS_DUMPS	1	/* 0 to reduce kernel size */
#endif

#ifdef CONFIG_BLK_DEV_CMD640
#if 0	/* change to 1 when debugging cmd640 problems */
void cmd640_dump_regs (void);
#define CMD640_DUMP_REGS cmd640_dump_regs() /* for debugging cmd640 chipset */
#endif
#endif  /* CONFIG_BLK_DEV_CMD640 */

#ifndef DISABLE_IRQ_NOSYNC
#define DISABLE_IRQ_NOSYNC	0
#endif

/*
 * Used to indicate "no IRQ", should be a value that cannot be an IRQ
 * number.
 */
 
#define IDE_NO_IRQ		(-1)

/*
 * IDE_DRIVE_CMD is used to implement many features of the hdparm utility
 */
#define IDE_DRIVE_CMD			99	/* (magic) undef to reduce kernel size*/

#define IDE_DRIVE_TASK			98

/*
 * IDE_DRIVE_TASKFILE is used to implement many features needed for raw tasks
 */
#define IDE_DRIVE_TASKFILE		97

/*
 *  "No user-serviceable parts" beyond this point  :)
 *****************************************************************************/

typedef unsigned char	byte;	/* used everywhere */

/*
 * Probably not wise to fiddle with these
 */
#define ERROR_MAX	8	/* Max read/write errors per sector */
#define ERROR_RESET	3	/* Reset controller every 4th retry */
#define ERROR_RECAL	1	/* Recalibrate every 2nd retry */

/*
 * Tune flags
 */
#define IDE_TUNE_BIOS		3
#define IDE_TUNE_NOAUTO		2
#define IDE_TUNE_AUTO		1
#define IDE_TUNE_DEFAULT	0

/*
 * state flags
 */

#define DMA_PIO_RETRY	1	/* retrying in PIO */

/*
 * Ensure that various configuration flags have compatible settings
 */
#ifdef REALLY_SLOW_IO
#undef REALLY_FAST_IO
#endif

#define HWIF(drive)		((ide_hwif_t *)((drive)->hwif))
#define HWGROUP(drive)		((ide_hwgroup_t *)(HWIF(drive)->hwgroup))

/*
 * Definitions for accessing IDE controller registers
 */
#define IDE_NR_PORTS		(10)

#define IDE_DATA_OFFSET		(0)
#define IDE_ERROR_OFFSET	(1)
#define IDE_NSECTOR_OFFSET	(2)
#define IDE_SECTOR_OFFSET	(3)
#define IDE_LCYL_OFFSET		(4)
#define IDE_HCYL_OFFSET		(5)
#define IDE_SELECT_OFFSET	(6)
#define IDE_STATUS_OFFSET	(7)
#define IDE_CONTROL_OFFSET	(8)
#define IDE_IRQ_OFFSET		(9)

#define IDE_FEATURE_OFFSET	IDE_ERROR_OFFSET
#define IDE_COMMAND_OFFSET	IDE_STATUS_OFFSET

#define IDE_DATA_OFFSET_HOB	(0)
#define IDE_ERROR_OFFSET_HOB	(1)
#define IDE_NSECTOR_OFFSET_HOB	(2)
#define IDE_SECTOR_OFFSET_HOB	(3)
#define IDE_LCYL_OFFSET_HOB	(4)
#define IDE_HCYL_OFFSET_HOB	(5)
#define IDE_SELECT_OFFSET_HOB	(6)
#define IDE_CONTROL_OFFSET_HOB	(7)

#define IDE_FEATURE_OFFSET_HOB	IDE_ERROR_OFFSET_HOB

#define IDE_DATA_REG		(HWIF(drive)->io_ports[IDE_DATA_OFFSET])
#define IDE_ERROR_REG		(HWIF(drive)->io_ports[IDE_ERROR_OFFSET])
#define IDE_NSECTOR_REG		(HWIF(drive)->io_ports[IDE_NSECTOR_OFFSET])
#define IDE_SECTOR_REG		(HWIF(drive)->io_ports[IDE_SECTOR_OFFSET])
#define IDE_LCYL_REG		(HWIF(drive)->io_ports[IDE_LCYL_OFFSET])
#define IDE_HCYL_REG		(HWIF(drive)->io_ports[IDE_HCYL_OFFSET])
#define IDE_SELECT_REG		(HWIF(drive)->io_ports[IDE_SELECT_OFFSET])
#define IDE_STATUS_REG		(HWIF(drive)->io_ports[IDE_STATUS_OFFSET])
#define IDE_CONTROL_REG		(HWIF(drive)->io_ports[IDE_CONTROL_OFFSET])
#define IDE_IRQ_REG		(HWIF(drive)->io_ports[IDE_IRQ_OFFSET])

#define IDE_DATA_REG_HOB	(HWIF(drive)->io_ports[IDE_DATA_OFFSET])
#define IDE_ERROR_REG_HOB	(HWIF(drive)->io_ports[IDE_ERROR_OFFSET])
#define IDE_NSECTOR_REG_HOB	(HWIF(drive)->io_ports[IDE_NSECTOR_OFFSET])
#define IDE_SECTOR_REG_HOB	(HWIF(drive)->io_ports[IDE_SECTOR_OFFSET])
#define IDE_LCYL_REG_HOB	(HWIF(drive)->io_ports[IDE_LCYL_OFFSET])
#define IDE_HCYL_REG_HOB	(HWIF(drive)->io_ports[IDE_HCYL_OFFSET])
#define IDE_SELECT_REG_HOB	(HWIF(drive)->io_ports[IDE_SELECT_OFFSET])
#define IDE_STATUS_REG_HOB	(HWIF(drive)->io_ports[IDE_STATUS_OFFSET])
#define IDE_CONTROL_REG_HOB	(HWIF(drive)->io_ports[IDE_CONTROL_OFFSET])

#define IDE_FEATURE_REG		IDE_ERROR_REG
#define IDE_COMMAND_REG		IDE_STATUS_REG
#define IDE_ALTSTATUS_REG	IDE_CONTROL_REG
#define IDE_IREASON_REG		IDE_NSECTOR_REG
#define IDE_BCOUNTL_REG		IDE_LCYL_REG
#define IDE_BCOUNTH_REG		IDE_HCYL_REG

#define OK_STAT(stat,good,bad)	(((stat)&((good)|(bad)))==(good))
#define BAD_R_STAT		(BUSY_STAT   | ERR_STAT)
#define BAD_W_STAT		(BAD_R_STAT  | WRERR_STAT)
#define BAD_STAT		(BAD_R_STAT  | DRQ_STAT)
#define DRIVE_READY		(READY_STAT  | SEEK_STAT)
#define DATA_READY		(DRQ_STAT)

#define BAD_CRC			(ABRT_ERR    | ICRC_ERR)

#define SATA_NR_PORTS		(3)	/* 16 possible ?? */

#define SATA_STATUS_OFFSET	(0)
#define SATA_STATUS_REG		(HWIF(drive)->sata_scr[SATA_STATUS_OFFSET])
#define SATA_ERROR_OFFSET	(1)
#define SATA_ERROR_REG		(HWIF(drive)->sata_scr[SATA_ERROR_OFFSET])
#define SATA_CONTROL_OFFSET	(2)
#define SATA_CONTROL_REG	(HWIF(drive)->sata_scr[SATA_CONTROL_OFFSET])

#define SATA_MISC_OFFSET	(0)
#define SATA_MISC_REG		(HWIF(drive)->sata_misc[SATA_MISC_OFFSET])
#define SATA_PHY_OFFSET		(1)
#define SATA_PHY_REG		(HWIF(drive)->sata_misc[SATA_PHY_OFFSET])
#define SATA_IEN_OFFSET		(2)
#define SATA_IEN_REG		(HWIF(drive)->sata_misc[SATA_IEN_OFFSET])

/*
 * Our Physical Region Descriptor (PRD) table should be large enough
 * to handle the biggest I/O request we are likely to see.  Since requests
 * can have no more than 256 sectors, and since the typical blocksize is
 * two or more sectors, we could get by with a limit of 128 entries here for
 * the usual worst case.  Most requests seem to include some contiguous blocks,
 * further reducing the number of table entries required.
 *
 * The driver reverts to PIO mode for individual requests that exceed
 * this limit (possible with 512 byte blocksizes, eg. MSDOS f/s), so handling
 * 100% of all crazy scenarios here is not necessary.
 *
 * As it turns out though, we must allocate a full 4KB page for this,
 * so the two PRD tables (ide0 & ide1) will each get half of that,
 * allowing each to have about 256 entries (8 bytes each) from this.
 */
#define PRD_BYTES       8
#define PRD_ENTRIES     (PAGE_SIZE / (2 * PRD_BYTES))

/*
 * Some more useful definitions
 */
#define IDE_MAJOR_NAME	"hd"	/* the same for all i/f; see also genhd.c */
#define MAJOR_NAME	IDE_MAJOR_NAME
#define PARTN_BITS	6	/* number of minor dev bits for partitions */
#define PARTN_MASK	((1<<PARTN_BITS)-1)	/* a useful bit mask */
#define MAX_DRIVES	2	/* per interface; 2 assumed by lots of code */
#define SECTOR_SIZE	512
#define SECTOR_WORDS	(SECTOR_SIZE / 4)	/* number of 32bit words per sector */
#define IDE_LARGE_SEEK(b1,b2,t)	(((b1) > (b2) + (t)) || ((b2) > (b1) + (t)))
#define IDE_MIN(a,b)	((a)<(b) ? (a):(b))
#define IDE_MAX(a,b)	((a)>(b) ? (a):(b))

/*
 * Timeouts for various operations:
 */
#define WAIT_DRQ	(5*HZ/100)	/* 50msec - spec allows up to 20ms */
#if defined(CONFIG_APM) || defined(CONFIG_APM_MODULE)
#define WAIT_READY	(5*HZ)		/* 5sec - some laptops are very slow */
#else
#define WAIT_READY	(3*HZ/100)	/* 30msec - should be instantaneous */
#endif /* CONFIG_APM || CONFIG_APM_MODULE */
#define WAIT_PIDENTIFY	(10*HZ)	/* 10sec  - should be less than 3ms (?), if all ATAPI CD is closed at boot */
#define WAIT_WORSTCASE	(30*HZ)	/* 30sec  - worst case when spinning up */
#define WAIT_CMD	(10*HZ)	/* 10sec  - maximum wait for an IRQ to happen */
#define WAIT_MIN_SLEEP	(2*HZ/100)	/* 20msec - minimum sleep time */

#define HOST(hwif,chipset)					\
{								\
	return ((hwif)->chipset == chipset) ? 1 : 0;		\
}

/*
 * Check for an interrupt and acknowledge the interrupt status
 */
struct hwif_s;
typedef int (ide_ack_intr_t)(struct hwif_s *);

#ifndef NO_DMA
#define NO_DMA  255
#endif

/*
 * hwif_chipset_t is used to keep track of the specific hardware
 * chipset used by each IDE interface, if known.
 */
typedef enum {	ide_unknown,	ide_generic,	ide_pci,
		ide_cmd640,	ide_dtc2278,	ide_ali14xx,
		ide_qd65xx,	ide_umc8672,	ide_ht6560b,
		ide_pdc4030,	ide_rz1000,	ide_trm290,
		ide_cmd646,	ide_cy82c693,	ide_4drives,
		ide_pmac,	ide_etrax100,	ide_acorn
} hwif_chipset_t;

/*
 * Structure to hold all information about the location of this port
 */
typedef struct hw_regs_s {
	ide_ioreg_t	io_ports[IDE_NR_PORTS];	/* task file registers */
	int		irq;			/* our irq number */
	int		dma;			/* our dma entry */
	ide_ack_intr_t	*ack_intr;		/* acknowledge interrupt */
	void		*priv;			/* interface specific data */
	hwif_chipset_t  chipset;
	sata_ioreg_t	sata_scr[SATA_NR_PORTS];
	sata_ioreg_t	sata_misc[SATA_NR_PORTS];
} hw_regs_t;

/*
 * Register new hardware with ide
 */
int ide_register_hw(hw_regs_t *hw, struct hwif_s **hwifp);

/*
 * Set up hw_regs_t structure before calling ide_register_hw (optional)
 */
void ide_setup_ports(	hw_regs_t *hw,
			ide_ioreg_t base,
			int *offsets,
			ide_ioreg_t ctrl,
			ide_ioreg_t intr,
			ide_ack_intr_t *ack_intr,
#if 0
			ide_io_ops_t *iops,
#endif
			int irq);

#include <asm/ide.h>

/* Currently only m68k, apus and m8xx need it */
#ifndef IDE_ARCH_ACK_INTR
# define ide_ack_intr(hwif) (1)
#endif

/* Currently only Atari needs it */
#ifndef IDE_ARCH_LOCK
# define ide_release_lock()			do {} while (0)
# define ide_get_lock(hdlr, data)		do {} while (0)
#endif /* IDE_ARCH_LOCK */

/*
 * Now for the data we need to maintain per-drive:  ide_drive_t
 */

#define ide_scsi	0x21
#define ide_disk	0x20
#define ide_optical	0x7
#define ide_cdrom	0x5
#define ide_tape	0x1
#define ide_floppy	0x0

/*
 * Special Driver Flags
 *
 * set_geometry	: respecify drive geometry
 * recalibrate	: seek to cyl 0
 * set_multmode	: set multmode count
 * set_tune	: tune interface for drive
 * serviced	: service command
 * reserved	: unused
 */
typedef union {
	unsigned all			: 8;
	struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
		unsigned set_geometry	: 1;
		unsigned recalibrate	: 1;
		unsigned set_multmode	: 1;
		unsigned set_tune	: 1;
		unsigned serviced	: 1;
		unsigned reserved	: 3;
#elif defined(__BIG_ENDIAN_BITFIELD)
		unsigned reserved	: 3;
		unsigned serviced	: 1;
		unsigned set_tune	: 1;
		unsigned set_multmode	: 1;
		unsigned recalibrate	: 1;
		unsigned set_geometry	: 1;
#else
#error "Please fix <asm/byteorder.h>"
#endif
	} b;
} special_t;

/*
 * ATA DATA Register Special.
 * ATA NSECTOR Count Register().
 * ATAPI Byte Count Register.
 * Channel index ordering pairs.
 */
typedef union {
	unsigned all			:16;
	struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
		unsigned low		:8;	/* LSB */
		unsigned high		:8;	/* MSB */
#elif defined(__BIG_ENDIAN_BITFIELD)
		unsigned high		:8;	/* MSB */
		unsigned low		:8;	/* LSB */
#else
#error "Please fix <asm/byteorder.h>"
#endif
	} b;
} ata_nsector_t, ata_data_t, atapi_bcount_t, ata_index_t;

/*
 * ATA-IDE Error Register
 *
 * mark		: Bad address mark
 * tzero	: Couldn't find track 0
 * abrt		: Aborted Command
 * mcr		: Media Change Request
 * id		: ID field not found
 * mce		: Media Change Event
 * ecc		: Uncorrectable ECC error
 * bdd		: dual meaing
 */
typedef union {
	unsigned all			:8;
	struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
		unsigned mark		:1;
		unsigned tzero		:1;
		unsigned abrt		:1;
		unsigned mcr		:1;
		unsigned id		:1;
		unsigned mce		:1;
		unsigned ecc		:1;
		unsigned bdd		:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
		unsigned bdd		:1;
		unsigned ecc		:1;
		unsigned mce		:1;
		unsigned id		:1;
		unsigned mcr		:1;
		unsigned abrt		:1;
		unsigned tzero		:1;
		unsigned mark		:1;
#else
#error "Please fix <asm/byteorder.h>"
#endif
	} b;
} ata_error_t;

/*
 * ATA-IDE Select Register, aka Device-Head
 *
 * head		: always zeros here
 * unit		: drive select number: 0/1
 * bit5		: always 1
 * lba		: using LBA instead of CHS
 * bit7		: always 1
 */
typedef union {
	unsigned all			: 8;
	struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
		unsigned head		: 4;
		unsigned unit		: 1;
		unsigned bit5		: 1;
		unsigned lba		: 1;
		unsigned bit7		: 1;
#elif defined(__BIG_ENDIAN_BITFIELD)
		unsigned bit7		: 1;
		unsigned lba		: 1;
		unsigned bit5		: 1;
		unsigned unit		: 1;
		unsigned head		: 4;
#else
#error "Please fix <asm/byteorder.h>"
#endif
	} b;
} select_t, ata_select_t;

/*
 * The ATA-IDE Status Register.
 * The ATAPI Status Register.
 *
 * check	: Error occurred
 * idx		: Index Error
 * corr		: Correctable error occurred
 * drq		: Data is request by the device
 * dsc		: Disk Seek Complete			: ata
 *		: Media access command finished		: atapi
 * df		: Device Fault				: ata
 *		: Reserved				: atapi
 * drdy		: Ready, Command Mode Capable		: ata
 *		: Ignored for ATAPI commands		: atapi
 * bsy		: Disk is Busy
 *		: The device has access to the command block
 */
typedef union {
	unsigned all			:8;
	struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
		unsigned check		:1;
		unsigned idx		:1;
		unsigned corr		:1;
		unsigned drq		:1;
		unsigned dsc		:1;
		unsigned df		:1;
		unsigned drdy		:1;
		unsigned bsy		:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
		unsigned bsy		:1;
		unsigned drdy		:1;
		unsigned df		:1;
		unsigned dsc		:1;
		unsigned drq		:1;
		unsigned corr           :1;
		unsigned idx		:1;
		unsigned check		:1;
#else
#error "Please fix <asm/byteorder.h>"
#endif
	} b;
} ata_status_t, atapi_status_t;

/*
 * ATA-IDE Control Register
 *
 * bit0		: Should be set to zero
 * nIEN		: device INTRQ to host
 * SRST		: host soft reset bit
 * bit3		: ATA-2 thingy, Should be set to 1
 * reserved456	: Reserved
 * HOB		: 48-bit address ordering, High Ordered Bit
 */
typedef union {
	unsigned all			: 8;
	struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
		unsigned bit0		: 1;
		unsigned nIEN		: 1;
		unsigned SRST		: 1;
		unsigned bit3		: 1;
		unsigned reserved456	: 3;
		unsigned HOB		: 1;
#elif defined(__BIG_ENDIAN_BITFIELD)
		unsigned HOB		: 1;
		unsigned reserved456	: 3;
		unsigned bit3		: 1;
		unsigned SRST		: 1;
		unsigned nIEN		: 1;
		unsigned bit0		: 1;
#else
#error "Please fix <asm/byteorder.h>"
#endif
	} b;
} ata_control_t;

/*
 * ATAPI Feature Register
 *
 * dma		: Using DMA or PIO
 * reserved321	: Reserved
 * reserved654	: Reserved (Tag Type)
 * reserved7	: Reserved
 */
typedef union {
	unsigned all			:8;
	struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
		unsigned dma		:1;
		unsigned reserved321	:3;
		unsigned reserved654	:3;
		unsigned reserved7	:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
		unsigned reserved7	:1;
		unsigned reserved654	:3;
		unsigned reserved321	:3;
		unsigned dma		:1;
#else
#error "Please fix <asm/byteorder.h>"
#endif
	} b;
} atapi_feature_t;

/*
 * ATAPI Interrupt Reason Register.
 *
 * cod		: Information transferred is command (1) or data (0)
 * io		: The device requests us to read (1) or write (0)
 * reserved	: Reserved
 */
typedef union {
	unsigned all			:8;
	struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
		unsigned cod		:1;
		unsigned io		:1;
		unsigned reserved	:6;
#elif defined(__BIG_ENDIAN_BITFIELD)
		unsigned reserved	:6;
		unsigned io		:1;
		unsigned cod		:1;
#else
#error "Please fix <asm/byteorder.h>"
#endif
	} b;
} atapi_ireason_t;

/*
 * The ATAPI error register.
 *
 * ili		: Illegal Length Indication
 * eom		: End Of Media Detected
 * abrt		: Aborted command - As defined by ATA
 * mcr		: Media Change Requested - As defined by ATA
 * sense_key	: Sense key of the last failed packet command
 */
typedef union {
	unsigned all			:8;
	struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
		unsigned ili		:1;
		unsigned eom		:1;
		unsigned abrt		:1;
		unsigned mcr		:1;
		unsigned sense_key	:4;
#elif defined(__BIG_ENDIAN_BITFIELD)
		unsigned sense_key	:4;
		unsigned mcr		:1;
		unsigned abrt		:1;
		unsigned eom		:1;
		unsigned ili		:1;
#else
#error "Please fix <asm/byteorder.h>"
#endif
	} b;
} atapi_error_t;

/*
 * ATAPI floppy Drive Select Register
 *
 * sam_lun	: Logical unit number
 * reserved3	: Reserved
 * drv		: The responding drive will be drive 0 (0) or drive 1 (1)
 * one5		: Should be set to 1
 * reserved6	: Reserved
 * one7		: Should be set to 1
 */
typedef union {
	unsigned all			:8;
	struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
		unsigned sam_lun	:3;
		unsigned reserved3	:1;
		unsigned drv		:1;
		unsigned one5		:1;
		unsigned reserved6	:1;
		unsigned one7		:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
		unsigned one7		:1;
		unsigned reserved6	:1;
		unsigned one5		:1;
		unsigned drv		:1;
		unsigned reserved3	:1;
		unsigned sam_lun	:3;
#else
#error "Please fix <asm/byteorder.h>"
#endif
	} b;
} atapi_select_t;

struct ide_driver_s;
struct ide_settings_s;

/*
 * Status returned from various ide_ functions
 */
typedef enum {
	ide_stopped,	/* no drive operation was started */
	ide_started	/* a drive operation was started, handler was set */
} ide_startstop_t;


typedef struct ide_drive_s {
	char		name[4];	/* drive name, such as "hda" */
        char            driver_req[10];	/* requests specific driver */

	request_queue_t		queue;	/* request queue */

	wait_queue_head_t wqueue;	/* used to wait for drive in open() */
	struct request		*rq;	/* current request */
	struct ide_drive_s 	*next;	/* circular list of hwgroup drives */
	struct ide_driver_s	*driver;/* (ide_driver_t *) */
	void		*driver_data;	/* extra driver data */
	struct hd_driveid	*id;	/* drive model identification info */
	struct hd_struct	*part;	/* drive partition table */
	struct proc_dir_entry *proc;	/* /proc/ide/ directory entry */
	struct ide_settings_s *settings;/* /proc/ide/ drive settings */
	devfs_handle_t		de;	/* directory for device */

	struct hwif_s		*hwif;	/* actually (ide_hwif_t *) */

	unsigned long sleep;		/* sleep until this time */
	unsigned long service_start;	/* time we started last request */
	unsigned long service_time;	/* service time of last request */
	unsigned long timeout;		/* max time to wait for irq */

	special_t	special;	/* special action flags */
	select_t	select;		/* basic drive/head select reg value */

	u8	keep_settings;		/* restore settings after drive reset */
	u8	autodma;		/* device can safely use dma on host */
	u8	using_dma;		/* disk is using dma for read/write */
	u8	using_tcq;		/* disk is using queueing */
	u8	retry_pio;		/* retrying dma capable host in pio */
	u8	state;			/* retry state */
	u8	waiting_for_dma;	/* dma currently in progress */
	u8	unmask;			/* okay to unmask other irqs */
	u8	slow;			/* slow data port */
	u8	bswap;			/* byte swap data */
	u8	dsc_overlap;		/* DSC overlap */
	u8	nice1;			/* give potential excess bandwidth */

	unsigned present	: 1;	/* drive is physically present */
	unsigned noprobe 	: 1;	/* from:  hdx=noprobe */
	unsigned busy		: 1;	/* currently doing revalidate_disk() */
	unsigned removable	: 1;	/* 1 if need to do check_media_change */
	unsigned is_flash	: 1;	/* 1 if probed as flash */
	unsigned forced_geom	: 1;	/* 1 if hdx=c,h,s was given at boot */
	unsigned no_unmask	: 1;	/* disallow setting unmask bit */
	unsigned no_io_32bit	: 1;	/* disallow enabling 32bit I/O */
	unsigned nobios		: 1;	/* do not probe bios for drive */
	unsigned revalidate	: 1;	/* request revalidation */
	unsigned atapi_overlap	: 1;	/* ATAPI overlap (not supported) */
	unsigned nice0		: 1;	/* give obvious excess bandwidth */
	unsigned nice2		: 1;	/* give a share in our own bandwidth */
	unsigned doorlocking	: 1;	/* for removable only: door lock/unlock works */
	unsigned autotune	: 2;	/* 1=autotune, 2=noautotune, 0=default */
	unsigned remap_0_to_1	: 2;	/* 0=remap if ezdrive, 1=remap, 2=noremap */
	unsigned ata_flash	: 1;	/* 1=present, 0=default */
	unsigned dead		: 1;	/* 1=dead, no new attachments */
	unsigned id_read	: 1;	/* 1=id read from disk 0 = synthetic */
	unsigned addressing;		/*      : 3;
					 *  0=28-bit
					 *  1=48-bit
					 *  2=48-bit doing 28-bit
					 *  3=64-bit
					 */

	u8	scsi;		/* 0=default, 1=skip current ide-subdriver for ide-scsi emulation */
        u8	quirk_list;	/* considered quirky, set for a specific host */
        u8	suspend_reset;	/* drive suspend mode flag, soft-reset recovers */
        u8	init_speed;	/* transfer rate set at boot */
        u8	pio_speed;      /* unused by core, used by some drivers for fallback from DMA */
        u8	current_speed;	/* current transfer rate set */
        u8	dn;		/* now wide spread use */
        u8	wcache;		/* status of write cache */
	u8	acoustic;	/* acoustic management */
	u8	media;		/* disk, cdrom, tape, floppy, ... */
	u8	ctl;		/* "normal" value for IDE_CONTROL_REG */
	u8	ready_stat;	/* min status value for drive ready */
	u8	mult_count;	/* current multiple sector setting */
	u8	mult_req;	/* requested multiple sector setting */
	u8	tune_req;	/* requested drive tuning setting */
	u8	io_32bit;	/* 0=16-bit, 1=32-bit, 2/3=32bit+sync */
	u8	bad_wstat;	/* used for ignoring WRERR_STAT */
	u8	nowerr;		/* used for ignoring WRERR_STAT */
	u8	sect0;		/* offset of first sector for DM6:DDO */
	u8	head;		/* "real" number of heads */
	u8	sect;		/* "real" sectors per track */
	u8	bios_head;	/* BIOS/fdisk/LILO number of heads */
	u8	bios_sect;	/* BIOS/fdisk/LILO sectors per track */

	unsigned int	bios_cyl;	/* BIOS/fdisk/LILO number of cyls */
	unsigned int	cyl;		/* "real" number of cyls */
	unsigned int	drive_data;	/* use by tuneproc/selectproc */
	unsigned int	usage;		/* current "open()" count for drive */
	unsigned int	failures;	/* current failure count */
	unsigned int	max_failures;	/* maximum allowed failure count */

	u32		capacity;	/* total number of sectors */
	u64		capacity48;	/* total number of sectors */

	int		last_lun;	/* last logical unit */
	int		forced_lun;	/* if hdxlun was given at boot */
	int		lun;		/* logical unit */
	int		crc_count;	/* crc counter to reduce drive speed */
} ide_drive_t;

typedef struct ide_pio_ops_s {
	void (*ata_input_data)(ide_drive_t *, void *, u32);
	void (*ata_output_data)(ide_drive_t *, void *, u32);

	void (*atapi_input_bytes)(ide_drive_t *, void *, u32);
	void (*atapi_output_bytes)(ide_drive_t *, void *, u32);
} ide_pio_ops_t;

typedef struct ide_dma_ops_s {
	/* insert dma operations here! */
	int (*ide_dma_read)(ide_drive_t *drive);
	int (*ide_dma_write)(ide_drive_t *drive);
	int (*ide_dma_begin)(ide_drive_t *drive);
	int (*ide_dma_end)(ide_drive_t *drive);
	int (*ide_dma_check)(ide_drive_t *drive);
	int (*ide_dma_on)(ide_drive_t *drive);
	int (*ide_dma_off)(ide_drive_t *drive);
	int (*ide_dma_off_quietly)(ide_drive_t *drive);
	int (*ide_dma_test_irq)(ide_drive_t *drive);
	int (*ide_dma_host_on)(ide_drive_t *drive);
	int (*ide_dma_host_off)(ide_drive_t *drive);
	int (*ide_dma_bad_drive)(ide_drive_t *drive);
	int (*ide_dma_good_drive)(ide_drive_t *drive);
	int (*ide_dma_count)(ide_drive_t *drive);
	int (*ide_dma_verbose)(ide_drive_t *drive);
	int (*ide_dma_retune)(ide_drive_t *drive);
	int (*ide_dma_lostirq)(ide_drive_t *drive);
	int (*ide_dma_timeout)(ide_drive_t *drive);
} ide_dma_ops_t;

/*
 * mapping stuff, prepare for highmem...
 *
 * temporarily mapping a (possible) highmem bio for PIO transfer
 */
#define ide_rq_offset(rq) \
	(((rq)->hard_cur_sectors - (rq)->current_nr_sectors) << 9)

static inline void *ide_map_buffer(struct request *rq, unsigned long *flags)
{
	return bh_kmap_irq(rq->bh, flags) + ide_rq_offset(rq);
}

static inline void ide_unmap_buffer(char *buffer, unsigned long *flags)
{
	bh_kunmap_irq(buffer, flags);
}

#define IDE_CHIPSET_PCI_MASK	\
    ((1<<ide_pci)|(1<<ide_cmd646)|(1<<ide_ali14xx))
#define IDE_CHIPSET_IS_PCI(c)	((IDE_CHIPSET_PCI_MASK >> (c)) & 1)

struct ide_pci_device_s;

typedef struct hwif_s {
	struct hwif_s *next;		/* for linked-list in ide_hwgroup_t */
	struct hwif_s *mate;		/* other hwif from same PCI chip */
	struct hwgroup_s *hwgroup;	/* actually (ide_hwgroup_t *) */
	struct proc_dir_entry *proc;	/* /proc/ide/ directory entry */
	struct gendisk *gd;		/* gendisk structure */

	char name[6];			/* name of interface, eg. "ide0" */

		/* task file registers for pata and sata */
	ide_ioreg_t	io_ports[IDE_NR_PORTS];
	sata_ioreg_t	sata_scr[SATA_NR_PORTS];
	sata_ioreg_t	sata_misc[SATA_NR_PORTS];

	hw_regs_t	hw;		/* Hardware info */
	ide_drive_t	drives[MAX_DRIVES];	/* drive info */

	u8 major;	/* our major number */
	u8 index;	/* 0 for ide0; 1 for ide1; ... */
	u8 channel;	/* for dual-port chips: 0=primary, 1=secondary */
	u8 straight8;	/* Alan's straight 8 check */
	u8 bus_state;	/* power state of the IDE bus */

	u8 atapi_dma;	/* host supports atapi_dma */
	u8 ultra_mask;
	u8 mwdma_mask;
	u8 swdma_mask;

	hwif_chipset_t chipset;	/* sub-module for tuning.. */

	struct pci_dev  *pci_dev;	/* for pci chipsets */
	struct ide_pci_device_s	*cds;	/* chipset device struct */

	ide_startstop_t	(*rw_disk)(ide_drive_t *, struct request *, unsigned long);
	
#if 0
	ide_hwif_ops_t	*hwifops;
#else
	/* routine is for HBA specific IDENTITY operations */
	int	(*identify)(ide_drive_t *);
	/* routine to tune PIO mode for drives */
	void	(*tuneproc)(ide_drive_t *, u8);
	/* routine to retune DMA modes for drives */
	int	(*speedproc)(ide_drive_t *, u8);
	/* tweaks hardware to select drive */
	void	(*selectproc)(ide_drive_t *);
	/* chipset polling based on hba specifics */
	int	(*reset_poll)(ide_drive_t *);
	/* chipset specific changes to default for device-hba resets */
	void	(*pre_reset)(ide_drive_t *);
	/* routine to reset controller after a disk reset */
	void	(*resetproc)(ide_drive_t *);
	/* special interrupt handling for shared pci interrupts */
	void	(*intrproc)(ide_drive_t *);
	/* special host masking for drive selection */
	void	(*maskproc)(ide_drive_t *, int);
	/* check host's drive quirk list */
	int	(*quirkproc)(ide_drive_t *);
	/* driver soft-power interface */
	int	(*busproc)(ide_drive_t *, int);
//	/* host rate limiter */
//	u8	(*ratemask)(ide_drive_t *);
//	/* device rate limiter */
//	u8	(*ratefilter)(ide_drive_t *, u8);
#endif

#if 0
	ide_pio_ops_t	*pioops;
#else
	void (*ata_input_data)(ide_drive_t *, void *, u32);
	void (*ata_output_data)(ide_drive_t *, void *, u32);

	void (*atapi_input_bytes)(ide_drive_t *, void *, u32);
	void (*atapi_output_bytes)(ide_drive_t *, void *, u32);
#endif

#if 0
	ide_dma_ops_t	*dmaops;
#else
	int (*ide_dma_read)(ide_drive_t *drive);
	int (*ide_dma_write)(ide_drive_t *drive);
	int (*ide_dma_begin)(ide_drive_t *drive);
	int (*ide_dma_end)(ide_drive_t *drive);
	int (*ide_dma_check)(ide_drive_t *drive);
	int (*ide_dma_on)(ide_drive_t *drive);
	int (*ide_dma_off)(ide_drive_t *drive);
	int (*ide_dma_off_quietly)(ide_drive_t *drive);
	int (*ide_dma_test_irq)(ide_drive_t *drive);
	int (*ide_dma_host_on)(ide_drive_t *drive);
	int (*ide_dma_host_off)(ide_drive_t *drive);
	int (*ide_dma_bad_drive)(ide_drive_t *drive);
	int (*ide_dma_good_drive)(ide_drive_t *drive);
	int (*ide_dma_count)(ide_drive_t *drive);
	int (*ide_dma_verbose)(ide_drive_t *drive);
	int (*ide_dma_retune)(ide_drive_t *drive);
	int (*ide_dma_lostirq)(ide_drive_t *drive);
	int (*ide_dma_timeout)(ide_drive_t *drive);
#endif

	void (*OUTB)(u8 addr, unsigned long port);
	void (*OUTBSYNC)(ide_drive_t *drive, u8 addr, unsigned long port);
	void (*OUTW)(u16 addr, unsigned long port);
	void (*OUTL)(u32 addr, unsigned long port);
	void (*OUTSW)(unsigned long port, void *addr, u32 count);
	void (*OUTSL)(unsigned long port, void *addr, u32 count);

	u8  (*INB)(unsigned long port);
	u16 (*INW)(unsigned long port);
	u32 (*INL)(unsigned long port);
	void (*INSW)(unsigned long port, void *addr, u32 count);
	void (*INSL)(unsigned long port, void *addr, u32 count);

	/* dma physical region descriptor table (cpu view) */
	unsigned int	*dmatable_cpu;
	/* dma physical region descriptor table (dma view) */
	dma_addr_t	dmatable_dma;
	/* Scatter-gather list used to build the above */
	struct scatterlist *sg_table;
	int sg_nents;			/* Current number of entries in it */
	int sg_dma_direction;		/* dma transfer direction */
	int sg_dma_active;		/* is it in use */

	int		mmio;		/* hosts iomio (0), mmio (1) or custom (2) select */
	int		rqsize;		/* max sectors per request */
	int		addressing;	/* hosts addressing */
	int		irq;		/* our irq number */
	int		initializing;	/* set while initializing self */

	unsigned long	dma_master;	/* reference base addr dmabase */
	unsigned long	dma_base;	/* base addr for dma ports */
	unsigned long	dma_command;	/* dma command register */
	unsigned long	dma_vendor1;	/* dma vendor 1 register */
	unsigned long	dma_status;	/* dma status register */
	unsigned long	dma_vendor3;	/* dma vendor 3 register */
	unsigned long	dma_prdtable;	/* actual prd table address */
	unsigned long	dma_base2;	/* extended base addr for dma ports */

	unsigned	dma_extra;	/* extra addr for dma ports */
	unsigned long	config_data;	/* for use by chipset-specific code */
	unsigned long	select_data;	/* for use by chipset-specific code */
#if (DISK_RECOVERY_TIME > 0)
	unsigned long	last_time;	/* time when previous rq was done */
#endif


	unsigned	noprobe    : 1;	/* don't probe for this interface */
	unsigned	present    : 1;	/* this interface exists */
	unsigned	hold       : 1; /* this interface is always present */
	unsigned	serialized : 1;	/* serialized all channel operation */
	unsigned	sharing_irq: 1;	/* 1 = sharing irq with another hwif */
	unsigned	reset      : 1;	/* reset after probe */
	unsigned	autodma    : 1;	/* auto-attempt using DMA at boot */
	unsigned	udma_four  : 1;	/* 1=ATA-66 capable, 0=default */
	unsigned	highmem    : 1;	/* can do full 32-bit dma */
	unsigned	no_dsc     : 1;	/* 0 default, 1 dsc_overlap disabled */
	unsigned	sata	   : 1; /* 0 PATA, 1 SATA */

	void		*hwif_data;	/* extra hwif data */
} ide_hwif_t;

/*
 *  internal ide interrupt handler type
 */
typedef ide_startstop_t (ide_pre_handler_t)(ide_drive_t *, struct request *);
typedef ide_startstop_t (ide_handler_t)(ide_drive_t *);
typedef ide_startstop_t (ide_post_handler_t)(ide_drive_t *);
typedef int (ide_expiry_t)(ide_drive_t *);

typedef struct hwgroup_s {
		/* irq handler, if active */
	ide_startstop_t	(*handler)(ide_drive_t *);
		/* irq handler, suspended if active */
	ide_startstop_t	(*handler_save)(ide_drive_t *);
		/* BOOL: protects all fields below */
	volatile int busy;
		/* BOOL: wake us up on timer expiry */
	int sleeping;
		/* current drive */
	ide_drive_t *drive;
		/* ptr to current hwif in linked-list */
	ide_hwif_t *hwif;

		/* for pci chipsets */
	struct pci_dev *pci_dev;
		/* chipset device struct */
	struct ide_pci_device_s *cds;

		/* current request */
	struct request *rq;
		/* failsafe timer */
	struct timer_list timer;
		/* local copy of current write rq */
	struct request wrq;
		/* timeout value during long polls */
	unsigned long poll_timeout;
		/* queried upon timeouts */
	int (*expiry)(ide_drive_t *);
		/* ide_system_bus_speed */
	int pio_clock;
} ide_hwgroup_t;

/* structure attached to the request for IDE_TASK_CMDS */

/*
 * configurable drive settings
 */

#define TYPE_INT	0
#define TYPE_INTA	1
#define TYPE_BYTE	2
#define TYPE_SHORT	3

#define SETTING_READ	(1 << 0)
#define SETTING_WRITE	(1 << 1)
#define SETTING_RW	(SETTING_READ | SETTING_WRITE)

typedef int (ide_procset_t)(ide_drive_t *, int);
typedef struct ide_settings_s {
	char			*name;
	int			rw;
	int			read_ioctl;
	int			write_ioctl;
	int			data_type;
	int			min;
	int			max;
	int			mul_factor;
	int			div_factor;
	void			*data;
	ide_procset_t		*set;
	int			auto_remove;
	struct ide_settings_s	*next;
} ide_settings_t;

extern struct semaphore ide_setting_sem;
extern int ide_add_setting(ide_drive_t *drive, const char *name, int rw, int read_ioctl, int write_ioctl, int data_type, int min, int max, int mul_factor, int div_factor, void *data, ide_procset_t *set);
extern void ide_remove_setting(ide_drive_t *drive, char *name);
extern ide_settings_t *ide_find_setting_by_name(ide_drive_t *drive, char *name);
extern int ide_read_setting(ide_drive_t *t, ide_settings_t *setting);
extern int ide_write_setting(ide_drive_t *drive, ide_settings_t *setting, int val);
extern void ide_add_generic_settings(ide_drive_t *drive);

/*
 * /proc/ide interface
 */
typedef struct {
	const char	*name;
	mode_t		mode;
	read_proc_t	*read_proc;
	write_proc_t	*write_proc;
} ide_proc_entry_t;

#ifdef CONFIG_PROC_FS
extern void proc_ide_create(void);
extern void proc_ide_destroy(void);
extern void destroy_proc_ide_device(ide_hwif_t *, ide_drive_t *);
extern void destroy_proc_ide_drives(ide_hwif_t *);
extern void create_proc_ide_interfaces(void);
extern void ide_add_proc_entries(struct proc_dir_entry *, ide_proc_entry_t *, void *);
extern void ide_remove_proc_entries(struct proc_dir_entry *, ide_proc_entry_t *);
read_proc_t proc_ide_read_capacity;
read_proc_t proc_ide_read_geometry;

/*
 * Standard exit stuff:
 */
#define PROC_IDE_READ_RETURN(page,start,off,count,eof,len) \
{					\
	len -= off;			\
	if (len < count) {		\
		*eof = 1;		\
		if (len <= 0)		\
			return 0;	\
	} else				\
		len = count;		\
	*start = page + off;		\
	return len;			\
}
#else
#define PROC_IDE_READ_RETURN(page,start,off,count,eof,len) return 0;
#endif

/*
 * Subdrivers support.
 */
#define IDE_SUBDRIVER_VERSION	1

typedef struct ide_driver_s {
	const char			*name;
	const char			*version;
	u8				media;
	unsigned busy			: 1;
	unsigned supports_dma		: 1;
	unsigned supports_dsc_overlap	: 1;
	int		(*cleanup)(ide_drive_t *);
	int		(*shutdown)(ide_drive_t *);
	int		(*standby)(ide_drive_t *);
	int		(*suspend)(ide_drive_t *);
	int		(*resume)(ide_drive_t *);
	int		(*flushcache)(ide_drive_t *);
	ide_startstop_t	(*do_request)(ide_drive_t *, struct request *, unsigned long);
	int		(*end_request)(ide_drive_t *, int);
	u8		(*sense)(ide_drive_t *, const char *, u8);
	ide_startstop_t	(*error)(ide_drive_t *, const char *, u8);
	ide_startstop_t	(*abort)(ide_drive_t *, const char *);
	int		(*ioctl)(ide_drive_t *, struct inode *, struct file *, unsigned int, unsigned long);
	int		(*open)(struct inode *, struct file *, ide_drive_t *);
	void		(*release)(struct inode *, struct file *, ide_drive_t *);
	int		(*media_change)(ide_drive_t *);
	void		(*revalidate)(ide_drive_t *);
	void		(*pre_reset)(ide_drive_t *);
	unsigned long	(*capacity)(ide_drive_t *);
	ide_startstop_t	(*special)(ide_drive_t *);
	ide_proc_entry_t	*proc;
	int		(*init)(void);
	int		(*attach)(ide_drive_t *);
	void		(*ata_prebuilder)(ide_drive_t *);
	void		(*atapi_prebuilder)(ide_drive_t *);
} ide_driver_t;

#define DRIVER(drive)		((drive)->driver)

/*
 * IDE modules.
 */
#define IDE_CHIPSET_MODULE		0	/* not supported yet */
#define IDE_PROBE_MODULE		1
#define IDE_DRIVER_MODULE		2

typedef int	(ide_module_init_proc)(void);

typedef struct ide_module_s {
	int				type;
	ide_module_init_proc		*init;
	void				*info;
	struct ide_module_s		*next;
} ide_module_t;

typedef struct ide_devices_s {
	char			name[4];		/* hdX */
	unsigned		attached	: 1;	/* native */
	unsigned		alttached	: 1;	/* alternate */
	struct ide_devices_s	*next;
} ide_devices_t;

#if 0
typedef enum {
	IDE_MEDIA_FLOPPY	= ide_floppy,
	IDE_MEDIA_TAPE		= ide_tape,
	IDE_MEDIA_CDROM		= ide_cdrom,
	IDE_MEDIA_OPTICAL	= ide_optical,
	IDE_MEDIA_DISK		= ide_disk,
	IDE_MEDIA_SCSI		= ide_scsi,
} ide_media_type;

struct ide_media {
	ide_media_type type;
	char name[4];
	atomic_t refcount;	/* Maybe this should be a struct module*
				   with MOD_INC instead */
	struct list_head devices;
};

struct {
	struct ide_media floppy;
	struct ide_media tape;
	struct ide_media cdrom;
	struct ide_media optical;
	struct ide_media disk;
	struct ide_media scsi;
} ide_medias;

void __init init_media(void)
{
	memset(&ide_medias, 0, sizeof(ide_medias));

	ide_medias.floppy.type	= IDE_MEDIA_FLOPPY;
	ide_medias.tape.type	= IDE_MEDIA_TAPE;
	ide_medias.cdrom.type	= IDE_MEDIA_CDROM;
	ide_medias.optical.type	= IDE_MEDIA_OPTICAL;
	ide_medias.disk.type	= IDE_MEDIA_DISK;
	ide_medias.scsi.type	= IDE_MEDIA_SCSI;
};
#endif

/*
 * ide_hwifs[] is the master data structure used to keep track
 * of just about everything in ide.c.  Whenever possible, routines
 * should be using pointers to a drive (ide_drive_t *) or
 * pointers to a hwif (ide_hwif_t *), rather than indexing this
 * structure directly (the allocation/layout may change!).
 *
 */
#ifndef _IDE_C
extern	ide_hwif_t	ide_hwifs[];		/* master data repository */
extern	ide_module_t	*ide_chipsets;
extern	ide_module_t	*ide_modules;
extern	ide_module_t	*ide_probe;

extern	ide_devices_t	*idedisk;
extern	ide_devices_t	*idecd;
extern	ide_devices_t	*idefloppy;
extern	ide_devices_t	*idetape;
extern	ide_devices_t	*idescsi;

#endif
extern int noautodma;

/*
 * We need blk.h, but we replace its end_request by our own version.
 */
#define IDE_DRIVER		/* Toggle some magic bits in blk.h */
#define LOCAL_END_REQUEST	/* Don't generate end_request in blk.h */
//#define DEVICE_NR(device)	(MINOR(device) >> PARTN_BITS)
#include <linux/blk.h>

extern int ide_end_request(ide_drive_t *, int /* uptodate */);

/*
 * This is used on exit from the driver to designate the next irq handler
 * and also to start the safety timer.
 */
extern void ide_set_handler(ide_drive_t *, ide_handler_t *, unsigned int, ide_expiry_t *);

/*
 * This is used on exit from the driver to designate the next irq handler
 * and start the safety time safely and atomically from the IRQ handler
 * with respect to the command issue (which it also does)
 */
extern void ide_execute_command(ide_drive_t *, task_ioreg_t cmd, ide_handler_t *, unsigned int, ide_expiry_t *);

/*
 * Error reporting, in human readable form (luxurious, but a memory hog).
 *
 * (drive, msg, status)
 */
extern u8 ide_dump_status(ide_drive_t *, const char *, u8);

/*
 * ide_error() takes action based on the error returned by the controller.
 * The caller should return immediately after invoking this.
 *
 * (drive, msg, status)
 */
extern ide_startstop_t ide_error(ide_drive_t *, const char *, u8);
extern ide_startstop_t ide_abort(ide_drive_t *, const char *);

/*
 * Issue a simple drive command
 * The drive must be selected beforehand.
 *
 * (drive, command, nsector, handler)
 */
extern void ide_cmd(ide_drive_t *, u8, u8, ide_handler_t *);

extern void ide_fix_driveid(struct hd_driveid *);
/*
 * ide_fixstring() cleans up and (optionally) byte-swaps a text string,
 * removing leading/trailing blanks and compressing internal blanks.
 * It is primarily used to tidy up the model name/number fields as
 * returned by the WIN_[P]IDENTIFY commands.
 *
 * (s, bytecount, byteswap)
 */
extern void ide_fixstring(u8 *, const int, const int);

/*
 * This routine busy-waits for the drive status to be not "busy".
 * It then checks the status for all of the "good" bits and none
 * of the "bad" bits, and if all is okay it returns 0.  All other
 * cases return 1 after doing "*startstop = ide_error()", and the
 * caller should return the updated value of "startstop" in this case.
 * "startstop" is unchanged when the function returns 0;
 *  (startstop, drive, good, bad, timeout)
 */
extern int ide_wait_stat(ide_startstop_t *, ide_drive_t *, u8, u8, unsigned long);

/*
 * This routine is called from the partition-table code in genhd.c
 * to "convert" a drive to a logical geometry with fewer than 1024 cyls.
 */
extern int ide_xlate_1024(kdev_t, int, int, const char *);

/*
 * Convert kdev_t structure into ide_drive_t * one. If force is set the
 * non present drives can be opened.
 */
extern ide_drive_t *ide_info_ptr(kdev_t i_rdev, int force);

/*
 * Return the current idea about the total capacity of this drive.
 */
extern unsigned long current_capacity(ide_drive_t *);

extern void ide_revalidate_drive(ide_drive_t *);

/*
 * Start a reset operation for an IDE interface.
 * The caller should return immediately after invoking this.
 */
extern ide_startstop_t ide_do_reset(ide_drive_t *);

/*
 * This function is intended to be used prior to invoking ide_do_drive_cmd().
 */
extern void ide_init_drive_cmd(struct request *);

/*
 * "action" parameter type for ide_do_drive_cmd() below.
 */
typedef enum {
	ide_wait,	/* insert rq at end of list, and wait for it */
	ide_next,	/* insert rq immediately after current request */
	ide_preempt,	/* insert rq in front of current request */
	ide_end		/* insert rq at end of list, but don't wait for it */
} ide_action_t;

/*
 * This function issues a special IDE device request
 * onto the request queue.
 *
 * If action is ide_wait, then the rq is queued at the end of the
 * request queue, and the function sleeps until it has been processed.
 * This is for use when invoked from an ioctl handler.
 *
 * If action is ide_preempt, then the rq is queued at the head of
 * the request queue, displacing the currently-being-processed
 * request and this function returns immediately without waiting
 * for the new rq to be completed.  This is VERY DANGEROUS, and is
 * intended for careful use by the ATAPI tape/cdrom driver code.
 *
 * If action is ide_next, then the rq is queued immediately after
 * the currently-being-processed-request (if any), and the function
 * returns without waiting for the new rq to be completed.  As above,
 * This is VERY DANGEROUS, and is intended for careful use by the
 * ATAPI tape/cdrom driver code.
 *
 * If action is ide_end, then the rq is queued at the end of the
 * request queue, and the function returns immediately without waiting
 * for the new rq to be completed. This is again intended for careful
 * use by the ATAPI tape/cdrom driver code.
 */
extern int ide_do_drive_cmd(ide_drive_t *, struct request *, ide_action_t);

/*
 * Clean up after success/failure of an explicit drive cmd.
 * stat/err are used only when (HWGROUP(drive)->rq->cmd == IDE_DRIVE_CMD).
 * stat/err are used only when (HWGROUP(drive)->rq->cmd == IDE_DRIVE_TASK_MASK).
 *
 * (ide_drive_t *drive, u8 stat, u8 err)
 */
extern void ide_end_drive_cmd(ide_drive_t *, u8, u8);

/*
 * Issue ATA command and wait for completion.
 * Use for implementing commands in kernel
 *
 *  (ide_drive_t *drive, u8 cmd, u8 nsect, u8 feature, u8 sectors, u8 *buf)
 */
extern int ide_wait_cmd(ide_drive_t *, u8, u8, u8, u8, u8 *);

/* (ide_drive_t *drive, u8 *buf) */
extern int ide_wait_cmd_task(ide_drive_t *, u8 *);

typedef struct ide_task_s {
/*
 *	struct hd_drive_task_hdr	tf;
 *	task_struct_t		tf;
 *	struct hd_drive_hob_hdr		hobf;
 *	hob_struct_t		hobf;
 */
	task_ioreg_t		tfRegister[8];
	task_ioreg_t		hobRegister[8];
	ide_reg_valid_t		tf_out_flags;
	ide_reg_valid_t		tf_in_flags;
	int			data_phase;
	int			command_type;
	ide_pre_handler_t	*prehandler;
	ide_handler_t		*handler;
	ide_post_handler_t	*posthandler;
	struct request		*rq;		/* copy of request */
	void			*special;	/* valid_t generally */
} ide_task_t;

typedef struct pkt_task_s {
/*
 *	struct hd_drive_task_hdr	pktf;
 *	task_struct_t		pktf;
 *	u8			pkcdb[12];
 */
	task_ioreg_t		tfRegister[8];
	int			data_phase;
	int			command_type;
	ide_handler_t		*handler;
	struct request		*rq;		/* copy of request */
	void			*special;
} pkt_task_t;

extern inline void SELECT_DRIVE(ide_drive_t *);
extern inline void SELECT_INTERRUPT(ide_drive_t *);
extern inline void SELECT_MASK(ide_drive_t *, int);
extern inline void QUIRK_LIST(ide_drive_t *);

extern void ata_input_data(ide_drive_t *, void *, u32);
extern void ata_output_data(ide_drive_t *, void *, u32);
extern void atapi_input_bytes(ide_drive_t *, void *, u32);
extern void atapi_output_bytes(ide_drive_t *, void *, u32);
extern void taskfile_input_data(ide_drive_t *, void *, u32);
extern void taskfile_output_data(ide_drive_t *, void *, u32);

extern int drive_is_ready(ide_drive_t *);
extern int wait_for_ready(ide_drive_t *, int /* timeout */);

/*
 * taskfile io for disks for now...and builds request from ide_ioctl
 */
extern ide_startstop_t do_rw_taskfile(ide_drive_t *, ide_task_t *);

/* (ide_drive_t *drive, u8 stat, u8 err) */
extern void ide_end_taskfile(ide_drive_t *, u8, u8);

/*
 * Special Flagged Register Validation Caller
 */
extern ide_startstop_t flagged_taskfile(ide_drive_t *, ide_task_t *);

extern ide_startstop_t set_multmode_intr(ide_drive_t *);
extern ide_startstop_t set_geometry_intr(ide_drive_t *);
extern ide_startstop_t recal_intr(ide_drive_t *);
extern ide_startstop_t task_no_data_intr(ide_drive_t *);
extern ide_startstop_t task_in_intr(ide_drive_t *);
extern ide_startstop_t task_mulin_intr(ide_drive_t *);
extern ide_startstop_t pre_task_out_intr(ide_drive_t *, struct request *);
extern ide_startstop_t task_out_intr(ide_drive_t *);
extern ide_startstop_t pre_task_mulout_intr(ide_drive_t *, struct request *);
extern ide_startstop_t task_mulout_intr(ide_drive_t *);
extern void ide_init_drive_taskfile(struct request *);

extern int ide_raw_taskfile(ide_drive_t *, ide_task_t *, u8 *);

extern ide_pre_handler_t * ide_pre_handler_parser(struct hd_drive_task_hdr *, struct hd_drive_hob_hdr *);

extern ide_handler_t * ide_handler_parser(struct hd_drive_task_hdr *, struct hd_drive_hob_hdr *);

extern ide_post_handler_t * ide_post_handler_parser(struct hd_drive_task_hdr *, struct hd_drive_hob_hdr *);

/* Expects args is a full set of TF registers and parses the command type */
extern int ide_cmd_type_parser(ide_task_t *);

int ide_taskfile_ioctl(ide_drive_t *, struct inode *, struct file *, unsigned int, unsigned long);
int ide_cmd_ioctl(ide_drive_t *, struct inode *, struct file *, unsigned int, unsigned long);
int ide_task_ioctl(ide_drive_t *, struct inode *, struct file *, unsigned int, unsigned long);

#if 0

#define IDEFLOPPY_PC_BUFFER_SIZE        256
#define IDETAPE_PC_BUFFER_SIZE          256
#define IDE_PC_BUFFER_SIZE          256

typedef struct ide_packet_command_s {
		/* Actual packet bytes */
	u8 c[12];
		/* On each retry, we increment retries */
	int retries;
		/* Error code */
	int error;
		/* Bytes to transfer */
	int request_transfer;
		/* Bytes actually transferred */
	int actually_transferred;
		/* Size of our data buffer */
	int buffer_size;

	struct buffer_head *bh;
	u8 *b_data;
		/* The corresponding request */
	struct request *rq;
# if 0
		/* Scatter gather table */
	struct scatterlist *sg;
# endif
	int b_count;
		/* Data buffer */
	u8 *buffer;
		/* Pointer into the above buffer */
	u8 *current_position;
		/* Called when this packet command is completed */
	ide_startstop_t (*callback) (ide_drive_t *);
		/* Temporary buffer */
	u8 pc_buffer[IDE_PC_BUFFER_SIZE];
		/* Status/Action bit flags: long for set_bit */
	unsigned long flags;
} ide_pc_t;

ide-cd orthoginal :-/
struct packet_command {
        char *buffer;
        int buflen;
        int stat;
        int quiet;
        int timeout;
        struct request_sense *sense;
        unsigned char c[12];
};

#endif

#ifdef CONFIG_PKT_TASK_IOCTL
extern int pkt_taskfile_ioctl(ide_drive_t *, struct inode *, struct file *, unsigned int, unsigned long);
#endif /* CONFIG_PKT_TASK_IOCTL */

extern void ide_delay_50ms(void);
extern int system_bus_clock(void);

extern u8 ide_auto_reduce_xfer(ide_drive_t *);
extern int ide_driveid_update(ide_drive_t *);
extern int ide_ata66_check(ide_drive_t *, ide_task_t *);
extern int ide_config_drive_speed(ide_drive_t *, u8);
extern u8 eighty_ninty_three (ide_drive_t *);
extern int set_transfer(ide_drive_t *, ide_task_t *);
extern int taskfile_lib_get_identify(ide_drive_t *drive, u8 *);

extern ide_startstop_t __ide_do_rw_disk(ide_drive_t *, struct request *, unsigned long);
/*
 * ide_system_bus_speed() returns what we think is the system VESA/PCI
 * bus speed (in MHz).  This is used for calculating interface PIO timings.
 * The default is 40 for known PCI systems, 50 otherwise.
 * The "idebus=xx" parameter can be used to override this value.
 */
extern int ide_system_bus_speed(void);

/*
 * ide_stall_queue() can be used by a drive to give excess bandwidth back
 * to the hwgroup by sleeping for timeout jiffies.
 */
extern void ide_stall_queue(ide_drive_t *, unsigned long);

/*
 * ide_get_queue() returns the queue which corresponds to a given device.
 */
extern request_queue_t *ide_get_queue(kdev_t dev);

extern int ide_spin_wait_hwgroup(ide_drive_t *);
extern void ide_timer_expiry(unsigned long);
extern void ide_intr(int irq, void *dev_id, struct pt_regs *regs);
extern void do_ide_request(request_queue_t *);
extern void ide_init_subdrivers(void);

extern struct block_device_operations ide_fops[];
extern ide_proc_entry_t generic_subdriver_entries[];

extern int ide_attach_drive(ide_drive_t *);

extern int ideprobe_init(void);
extern int idedefault_attach(ide_drive_t *);
extern int idedefault_init(void);
extern int idedisk_attach(ide_drive_t *);
extern int idedisk_init(void);
extern int ide_cdrom_attach(ide_drive_t *);
extern int ide_cdrom_init(void);
extern int idetape_attach(ide_drive_t *);
extern int idetape_init(void);
extern int idefloppy_attach(ide_drive_t *);
extern int idefloppy_init(void);
extern int idescsi_attach(ide_drive_t *);
extern int idescsi_init(void);

extern void ide_scan_pcibus(int scan_direction) __init;
extern int ide_pci_register_driver(struct pci_driver *driver);
extern void ide_pci_unregister_driver(struct pci_driver *driver);

extern void default_hwif_iops(ide_hwif_t *);
extern void default_hwif_mmiops(ide_hwif_t *);
extern void default_hwif_transport(ide_hwif_t *);


extern int ide_register_module(ide_module_t *module);
extern void ide_unregister_module(ide_module_t *module);
extern ide_drive_t *ide_scan_devices(u8, const char *, ide_driver_t *, int);
extern int ide_register_subdriver(ide_drive_t *, ide_driver_t *, int /* ver */);
extern int ide_unregister_subdriver(ide_drive_t *);
extern int ide_replace_subdriver(ide_drive_t *, const char * /* driver*/);

#ifdef CONFIG_PROC_FS
typedef struct ide_pci_host_proc_s {
	char				*name;
        u8				set;
	get_info_t			*get_info;
	struct proc_dir_entry		*parent;
	struct ide_pci_host_proc_s	*next;
} ide_pci_host_proc_t;

void ide_pci_register_host_proc(ide_pci_host_proc_t *);
#endif /* CONFIG_PROC_FS */

#define ON_BOARD		1
#define NEVER_BOARD		0

#ifdef CONFIG_BLK_DEV_OFFBOARD
#  define OFF_BOARD		ON_BOARD
#else /* CONFIG_BLK_DEV_OFFBOARD */
#  define OFF_BOARD		NEVER_BOARD
#endif /* CONFIG_BLK_DEV_OFFBOARD */

#define NODMA 0
#define NOAUTODMA 1
#define AUTODMA 2
#define EOL 255

typedef struct ide_pci_enablebit_s {
	u8	reg;	/* byte pci reg holding the enable-bit */
	u8	mask;	/* mask to isolate the enable-bit */
	u8	val;	/* value of masked reg when "enabled" */
} ide_pci_enablebit_t;

typedef struct ide_pci_device_s {
	u16			vendor;
	u16			device;
	char			*name;
	void			(*init_setup)(struct pci_dev *, struct ide_pci_device_s *);
	unsigned int		(*init_chipset)(struct pci_dev *, const char *);
	void			(*init_iops)(ide_hwif_t *);
	void                    (*init_hwif)(ide_hwif_t *);
	void			(*init_dma)(ide_hwif_t *, unsigned long);
	u8			channels;
	u8			autodma;
	ide_pci_enablebit_t	enablebits[2];
	u8			bootable;
	unsigned int		extra;
	struct ide_pci_device_s	*next;
} ide_pci_device_t;

extern void ide_setup_pci_device(struct pci_dev *, ide_pci_device_t *);
extern void ide_setup_pci_devices(struct pci_dev *, struct pci_dev *, ide_pci_device_t *);

#define BAD_DMA_DRIVE		0
#define GOOD_DMA_DRIVE		1

#ifdef CONFIG_BLK_DEV_IDEDMA_PCI
extern int ide_build_dmatable(ide_drive_t *, struct request *, int);
extern void ide_destroy_dmatable(ide_drive_t *);
extern ide_startstop_t ide_dma_intr(ide_drive_t *);
extern int ide_release_dma(ide_hwif_t *);
extern void ide_setup_dma(ide_hwif_t *, unsigned long, unsigned int);

extern int __ide_dma_host_off(ide_drive_t *);
extern int __ide_dma_off_quietly(ide_drive_t *);
extern int __ide_dma_off(ide_drive_t *);
extern int __ide_dma_host_on(ide_drive_t *);
extern int __ide_dma_on(ide_drive_t *);
extern int __ide_dma_check(ide_drive_t *);
extern int __ide_dma_read(ide_drive_t *);
extern int __ide_dma_write(ide_drive_t *);
extern int __ide_dma_begin(ide_drive_t *);
extern int __ide_dma_end(ide_drive_t *);
extern int __ide_dma_test_irq(ide_drive_t *);
extern int __ide_dma_bad_drive(ide_drive_t *);
extern int __ide_dma_good_drive(ide_drive_t *);
extern int __ide_dma_count(ide_drive_t *);
extern int __ide_dma_verbose(ide_drive_t *);
extern int __ide_dma_retune(ide_drive_t *);
extern int __ide_dma_lostirq(ide_drive_t *);
extern int __ide_dma_timeout(ide_drive_t *);
#else
static inline void ide_setup_dma(ide_hwif_t *x, unsigned long y, unsigned int z) {;}
static inline void ide_release_dma(ide_hwif_t *x) {;}
#endif

extern void hwif_unregister(ide_hwif_t *);

extern void ide_probe_reset(ide_hwif_t *);
extern void ide_tune_drives(ide_hwif_t *);
extern int ide_wait_hwif_ready(ide_hwif_t *);
extern u8 ide_probe_for_drive(ide_drive_t *);


extern void export_ide_init_queue(ide_drive_t *);
extern u8 export_probe_for_drive(ide_drive_t *);
extern int probe_hwif_init(ide_hwif_t *);

static inline void *ide_get_hwifdata (ide_hwif_t * hwif)
{
	return hwif->hwif_data;
}

static inline void ide_set_hwifdata (ide_hwif_t * hwif, void *data)
{
	hwif->hwif_data = data;
}

/* ide-lib.c */
extern u8 ide_dma_speed(ide_drive_t *drive, u8 mode);
extern u8 ide_rate_filter(u8 mode, u8 speed);
extern int ide_dma_enable(ide_drive_t *drive);
extern char *ide_xfer_verbose(u8 xfer_rate);
extern void ide_toggle_bounce(ide_drive_t *drive, int on);
extern int ide_set_xfer_rate(ide_drive_t *drive, u8 rate);

/* ide driver registration */
typedef void (*ide_driver_call)(void);
extern void __init ide_register_driver(ide_driver_call);

/* ide locks for 2.4 */

#define ide_lock		(io_request_lock)
#define DRIVE_LOCK(drive)       ((drive)->queue.queue_lock)


#endif /* _IDE_H */
