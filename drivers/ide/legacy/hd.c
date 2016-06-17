/*
 *  linux/drivers/ide/legacy/hd.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/*
 * This is the low-level hd interrupt support. It traverses the
 * request-list, using interrupts to jump between functions. As
 * all the functions are called within interrupts, we may not
 * sleep. Special care is recommended.
 * 
 *  modified by Drew Eckhardt to check nr of hd's from the CMOS.
 *
 *  Thanks to Branko Lankester, lankeste@fwi.uva.nl, who found a bug
 *  in the early extended-partition checks and added DM partitions
 *
 *  IRQ-unmask, drive-id, multiple-mode, support for ">16 heads",
 *  and general streamlining by Mark Lord.
 *
 *  Removed 99% of above. Use Mark's ide driver for those options.
 *  This is now a lightweight ST-506 driver. (Paul Gortmaker)
 *
 *  Modified 1995 Russell King for ARM processor.
 *
 *  Bugfix: max_sectors must be <= 255 or the wheels tend to come
 *  off in a hurry once you queue things up - Paul G. 02/2001
 */
  
/* Uncomment the following if you want verbose error reports. */
/* #define VERBOSE_ERRORS */
  
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/kernel.h>
#include <linux/hdreg.h>
#include <linux/genhd.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/mc146818rtc.h> /* CMOS defines */
#include <linux/init.h>
#include <linux/blkpg.h>

#define REALLY_SLOW_IO
#include <asm/system.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#define MAJOR_NR HD_MAJOR
#include <linux/blk.h>

#ifdef __arm__
#undef  HD_IRQ
#endif
#include <asm/irq.h>
#ifdef __arm__
#define HD_IRQ IRQ_HARDDISK
#endif

static int revalidate_hddisk(kdev_t, int);

#define	HD_DELAY	0

#define MAX_ERRORS     16	/* Max read/write errors/sector */
#define RESET_FREQ      8	/* Reset controller every 8th retry */
#define RECAL_FREQ      4	/* Recalibrate every 4th retry */
#define MAX_HD		2

#define STAT_OK		(READY_STAT|SEEK_STAT)
#define OK_STATUS(s)	(((s)&(STAT_OK|(BUSY_STAT|WRERR_STAT|ERR_STAT)))==STAT_OK)

static void recal_intr(void);
static void bad_rw_intr(void);

static char recalibrate[MAX_HD];
static char special_op[MAX_HD];
static int access_count[MAX_HD];
static char busy[MAX_HD];
static DECLARE_WAIT_QUEUE_HEAD(busy_wait);

static int reset;
static int hd_error;

#define SUBSECTOR(block) (CURRENT->current_nr_sectors > 0)

/*
 *  This struct defines the HD's and their types.
 */
struct hd_i_struct {
	unsigned int head,sect,cyl,wpcom,lzone,ctl;
};
	
#ifdef HD_TYPE
static struct hd_i_struct hd_info[] = { HD_TYPE };
static int NR_HD = ((sizeof (hd_info))/(sizeof (struct hd_i_struct)));
#else
static struct hd_i_struct hd_info[MAX_HD];
static int NR_HD;
#endif

static struct hd_struct hd[MAX_HD<<6];
static int hd_sizes[MAX_HD<<6];
static int hd_blocksizes[MAX_HD<<6];
static int hd_hardsectsizes[MAX_HD<<6];
static int hd_maxsect[MAX_HD<<6];

static struct timer_list device_timer;

#define SET_TIMER 							\
	do {								\
		mod_timer(&device_timer, jiffies + TIMEOUT_VALUE);	\
	} while (0)

#define CLEAR_TIMER del_timer(&device_timer);

#undef SET_INTR

#define SET_INTR(x) \
if ((DEVICE_INTR = (x)) != NULL) \
	SET_TIMER; \
else \
	CLEAR_TIMER;


#if (HD_DELAY > 0)
unsigned long last_req;

unsigned long read_timer(void)
{
	unsigned long t, flags;
	int i;

	spin_lock_irqsave(&io_request_lock, flags);
	t = jiffies * 11932;
    	outb_p(0, 0x43);
	i = inb_p(0x40);
	i |= inb(0x40) << 8;
	spin_unlock_irqrestore(&io_request_lock, flags);
	return(t - i);
}
#endif

void __init hd_setup(char *str, int *ints)
{
	int hdind = 0;

	if (ints[0] != 3)
		return;
	if (hd_info[0].head != 0)
		hdind=1;
	hd_info[hdind].head = ints[2];
	hd_info[hdind].sect = ints[3];
	hd_info[hdind].cyl = ints[1];
	hd_info[hdind].wpcom = 0;
	hd_info[hdind].lzone = ints[1];
	hd_info[hdind].ctl = (ints[2] > 8 ? 8 : 0);
	NR_HD = hdind+1;
}

static void dump_status (const char *msg, unsigned int stat)
{
	unsigned long flags;
	char devc;

	devc = !QUEUE_EMPTY ? 'a' + DEVICE_NR(CURRENT->rq_dev) : '?';
	save_flags (flags);
	sti();
#ifdef VERBOSE_ERRORS
	printk("hd%c: %s: status=0x%02x { ", devc, msg, stat & 0xff);
	if (stat & BUSY_STAT)	printk("Busy ");
	if (stat & READY_STAT)	printk("DriveReady ");
	if (stat & WRERR_STAT)	printk("WriteFault ");
	if (stat & SEEK_STAT)	printk("SeekComplete ");
	if (stat & DRQ_STAT)	printk("DataRequest ");
	if (stat & ECC_STAT)	printk("CorrectedError ");
	if (stat & INDEX_STAT)	printk("Index ");
	if (stat & ERR_STAT)	printk("Error ");
	printk("}\n");
	if ((stat & ERR_STAT) == 0) {
		hd_error = 0;
	} else {
		hd_error = inb(HD_ERROR);
		printk("hd%c: %s: error=0x%02x { ", devc, msg, hd_error & 0xff);
		if (hd_error & BBD_ERR)		printk("BadSector ");
		if (hd_error & ECC_ERR)		printk("UncorrectableError ");
		if (hd_error & ID_ERR)		printk("SectorIdNotFound ");
		if (hd_error & ABRT_ERR)	printk("DriveStatusError ");
		if (hd_error & TRK0_ERR)	printk("TrackZeroNotFound ");
		if (hd_error & MARK_ERR)	printk("AddrMarkNotFound ");
		printk("}");
		if (hd_error & (BBD_ERR|ECC_ERR|ID_ERR|MARK_ERR)) {
			printk(", CHS=%d/%d/%d",
				(inb(HD_HCYL)<<8) + inb(HD_LCYL),
				inb(HD_CURRENT) & 0xf, inb(HD_SECTOR));
			if (!QUEUE_EMPTY)
				printk(", sector=%ld", CURRENT->sector);
		}
		printk("\n");
	}
#else
	printk("hd%c: %s: status=0x%02x.\n", devc, msg, stat & 0xff);
	if ((stat & ERR_STAT) == 0) {
		hd_error = 0;
	} else {
		hd_error = inb(HD_ERROR);
		printk("hd%c: %s: error=0x%02x.\n", devc, msg, hd_error & 0xff);
	}
#endif	/* verbose errors */
	restore_flags (flags);
}

void check_status(void)
{
	int i = inb_p(HD_STATUS);

	if (!OK_STATUS(i)) {
		dump_status("check_status", i);
		bad_rw_intr();
	}
}

static int controller_busy(void)
{
	int retries = 100000;
	unsigned char status;

	do {
		status = inb_p(HD_STATUS);
	} while ((status & BUSY_STAT) && --retries);
	return status;
}

static int status_ok(void)
{
	unsigned char status = inb_p(HD_STATUS);

	if (status & BUSY_STAT)
		return 1;	/* Ancient, but does it make sense??? */
	if (status & WRERR_STAT)
		return 0;
	if (!(status & READY_STAT))
		return 0;
	if (!(status & SEEK_STAT))
		return 0;
	return 1;
}

static int controller_ready(unsigned int drive, unsigned int head)
{
	int retry = 100;

	do {
		if (controller_busy() & BUSY_STAT)
			return 0;
		outb_p(0xA0 | (drive<<4) | head, HD_CURRENT);
		if (status_ok())
			return 1;
	} while (--retry);
	return 0;
}

static void hd_out(unsigned int drive,unsigned int nsect,unsigned int sect,
		unsigned int head,unsigned int cyl,unsigned int cmd,
		void (*intr_addr)(void))
{
	unsigned short port;

#if (HD_DELAY > 0)
	while (read_timer() - last_req < HD_DELAY)
		/* nothing */;
#endif
	if (reset)
		return;
	if (!controller_ready(drive, head)) {
		reset = 1;
		return;
	}
	SET_INTR(intr_addr);
	outb_p(hd_info[drive].ctl,HD_CMD);
	port=HD_DATA;
	outb_p(hd_info[drive].wpcom>>2,++port);
	outb_p(nsect,++port);
	outb_p(sect,++port);
	outb_p(cyl,++port);
	outb_p(cyl>>8,++port);
	outb_p(0xA0|(drive<<4)|head,++port);
	outb_p(cmd,++port);
}

static void hd_request (void);

static int drive_busy(void)
{
	unsigned int i;
	unsigned char c;

	for (i = 0; i < 500000 ; i++) {
		c = inb_p(HD_STATUS);
		if ((c & (BUSY_STAT | READY_STAT | SEEK_STAT)) == STAT_OK)
			return 0;
	}
	dump_status("reset timed out", c);
	return 1;
}

static void reset_controller(void)
{
	int	i;

	outb_p(4,HD_CMD);
	for(i = 0; i < 1000; i++) barrier();
	outb_p(hd_info[0].ctl & 0x0f,HD_CMD);
	for(i = 0; i < 1000; i++) barrier();
	if (drive_busy())
		printk("hd: controller still busy\n");
	else if ((hd_error = inb(HD_ERROR)) != 1)
		printk("hd: controller reset failed: %02x\n",hd_error);
}

static void reset_hd(void)
{
	static int i;

repeat:
	if (reset) {
		reset = 0;
		i = -1;
		reset_controller();
	} else {
		check_status();
		if (reset)
			goto repeat;
	}
	if (++i < NR_HD) {
		special_op[i] = recalibrate[i] = 1;
		hd_out(i,hd_info[i].sect,hd_info[i].sect,hd_info[i].head-1,
			hd_info[i].cyl,WIN_SPECIFY,&reset_hd);
		if (reset)
			goto repeat;
	} else
		hd_request();
}

void do_reset_hd(void)
{
	DEVICE_INTR = NULL;
	reset = 1;
	reset_hd();
}

/*
 * Ok, don't know what to do with the unexpected interrupts: on some machines
 * doing a reset and a retry seems to result in an eternal loop. Right now I
 * ignore it, and just set the timeout.
 *
 * On laptops (and "green" PCs), an unexpected interrupt occurs whenever the
 * drive enters "idle", "standby", or "sleep" mode, so if the status looks
 * "good", we just ignore the interrupt completely.
 */
void unexpected_hd_interrupt(void)
{
	unsigned int stat = inb_p(HD_STATUS);

	if (stat & (BUSY_STAT|DRQ_STAT|ECC_STAT|ERR_STAT)) {
		dump_status ("unexpected interrupt", stat);
		SET_TIMER;
	}
}

/*
 * bad_rw_intr() now tries to be a bit smarter and does things
 * according to the error returned by the controller.
 * -Mika Liljeberg (liljeber@cs.Helsinki.FI)
 */
static void bad_rw_intr(void)
{
	int dev;

	if (QUEUE_EMPTY)
		return;
	dev = DEVICE_NR(CURRENT->rq_dev);
	if (++CURRENT->errors >= MAX_ERRORS || (hd_error & BBD_ERR)) {
		end_request(0);
		special_op[dev] = recalibrate[dev] = 1;
	} else if (CURRENT->errors % RESET_FREQ == 0)
		reset = 1;
	else if ((hd_error & TRK0_ERR) || CURRENT->errors % RECAL_FREQ == 0)
		special_op[dev] = recalibrate[dev] = 1;
	/* Otherwise just retry */
}

static inline int wait_DRQ(void)
{
	int retries = 100000, stat;

	while (--retries > 0)
		if ((stat = inb_p(HD_STATUS)) & DRQ_STAT)
			return 0;
	dump_status("wait_DRQ", stat);
	return -1;
}

static void read_intr(void)
{
	int i, retries = 100000;

	do {
		i = (unsigned) inb_p(HD_STATUS);
		if (i & BUSY_STAT)
			continue;
		if (!OK_STATUS(i))
			break;
		if (i & DRQ_STAT)
			goto ok_to_read;
	} while (--retries > 0);
	dump_status("read_intr", i);
	bad_rw_intr();
	hd_request();
	return;
ok_to_read:
	insw(HD_DATA,CURRENT->buffer,256);
	CURRENT->sector++;
	CURRENT->buffer += 512;
	CURRENT->errors = 0;
	i = --CURRENT->nr_sectors;
	--CURRENT->current_nr_sectors;
#ifdef DEBUG
	printk("hd%c: read: sector %ld, remaining = %ld, buffer=0x%08lx\n",
		dev+'a', CURRENT->sector, CURRENT->nr_sectors,
		(unsigned long) CURRENT->buffer+512));
#endif
	if (CURRENT->current_nr_sectors <= 0)
		end_request(1);
	if (i > 0) {
		SET_INTR(&read_intr);
		return;
	}
	(void) inb_p(HD_STATUS);
#if (HD_DELAY > 0)
	last_req = read_timer();
#endif
	if (!QUEUE_EMPTY)
		hd_request();
	return;
}

static void write_intr(void)
{
	int i;
	int retries = 100000;

	do {
		i = (unsigned) inb_p(HD_STATUS);
		if (i & BUSY_STAT)
			continue;
		if (!OK_STATUS(i))
			break;
		if ((CURRENT->nr_sectors <= 1) || (i & DRQ_STAT))
			goto ok_to_write;
	} while (--retries > 0);
	dump_status("write_intr", i);
	bad_rw_intr();
	hd_request();
	return;
ok_to_write:
	CURRENT->sector++;
	i = --CURRENT->nr_sectors;
	--CURRENT->current_nr_sectors;
	CURRENT->buffer += 512;
	if (!i || (CURRENT->bh && !SUBSECTOR(i)))
		end_request(1);
	if (i > 0) {
		SET_INTR(&write_intr);
		outsw(HD_DATA,CURRENT->buffer,256);
		sti();
	} else {
#if (HD_DELAY > 0)
		last_req = read_timer();
#endif
		hd_request();
	}
	return;
}

static void recal_intr(void)
{
	check_status();
#if (HD_DELAY > 0)
	last_req = read_timer();
#endif
	hd_request();
}

/*
 * This is another of the error-routines I don't know what to do with. The
 * best idea seems to just set reset, and start all over again.
 */
static void hd_times_out(unsigned long dummy)
{
	unsigned int dev;

	DEVICE_INTR = NULL;
	if (QUEUE_EMPTY)
		return;
	disable_irq(HD_IRQ);
	sti();
	reset = 1;
	dev = DEVICE_NR(CURRENT->rq_dev);
	printk("hd%c: timeout\n", dev+'a');
	if (++CURRENT->errors >= MAX_ERRORS) {
#ifdef DEBUG
		printk("hd%c: too many errors\n", dev+'a');
#endif
		end_request(0);
	}
	cli();
	hd_request();
	enable_irq(HD_IRQ);
}

int do_special_op (unsigned int dev)
{
	if (recalibrate[dev]) {
		recalibrate[dev] = 0;
		hd_out(dev,hd_info[dev].sect,0,0,0,WIN_RESTORE,&recal_intr);
		return reset;
	}
	if (hd_info[dev].head > 16) {
		printk ("hd%c: cannot handle device with more than 16 heads - giving up\n", dev+'a');
		end_request(0);
	}
	special_op[dev] = 0;
	return 1;
}

/*
 * The driver enables interrupts as much as possible.  In order to do this,
 * (a) the device-interrupt is disabled before entering hd_request(),
 * and (b) the timeout-interrupt is disabled before the sti().
 *
 * Interrupts are still masked (by default) whenever we are exchanging
 * data/cmds with a drive, because some drives seem to have very poor
 * tolerance for latency during I/O. The IDE driver has support to unmask
 * interrupts for non-broken hardware, so use that driver if required.
 */
static void hd_request(void)
{
	unsigned int dev, block, nsect, sec, track, head, cyl;

	if (!QUEUE_EMPTY && CURRENT->rq_status == RQ_INACTIVE) return;
	if (DEVICE_INTR)
		return;
repeat:
	del_timer(&device_timer);
	sti();
	INIT_REQUEST;
	if (reset) {
		cli();
		reset_hd();
		return;
	}
	dev = MINOR(CURRENT->rq_dev);
	block = CURRENT->sector;
	nsect = CURRENT->nr_sectors;
	if (dev >= (NR_HD<<6) || block >= hd[dev].nr_sects || ((block+nsect) > hd[dev].nr_sects)) {
#ifdef DEBUG
		if (dev >= (NR_HD<<6))
			printk("hd: bad minor number: device=%s\n",
			       kdevname(CURRENT->rq_dev));
		else
			printk("hd%c: bad access: block=%d, count=%d\n",
				(MINOR(CURRENT->rq_dev)>>6)+'a', block, nsect);
#endif
		end_request(0);
		goto repeat;
	}
	block += hd[dev].start_sect;
	dev >>= 6;
	if (special_op[dev]) {
		if (do_special_op(dev))
			goto repeat;
		return;
	}
	sec   = block % hd_info[dev].sect + 1;
	track = block / hd_info[dev].sect;
	head  = track % hd_info[dev].head;
	cyl   = track / hd_info[dev].head;
#ifdef DEBUG
	printk("hd%c: %sing: CHS=%d/%d/%d, sectors=%d, buffer=0x%08lx\n",
		dev+'a', (CURRENT->cmd == READ)?"read":"writ",
		cyl, head, sec, nsect, (unsigned long) CURRENT->buffer);
#endif
	if (CURRENT->cmd == READ) {
		hd_out(dev,nsect,sec,head,cyl,WIN_READ,&read_intr);
		if (reset)
			goto repeat;
		return;
	}
	if (CURRENT->cmd == WRITE) {
		hd_out(dev,nsect,sec,head,cyl,WIN_WRITE,&write_intr);
		if (reset)
			goto repeat;
		if (wait_DRQ()) {
			bad_rw_intr();
			goto repeat;
		}
		outsw(HD_DATA,CURRENT->buffer,256);
		return;
	}
	panic("unknown hd-command");
}

static void do_hd_request (request_queue_t * q)
{
	disable_irq(HD_IRQ);
	hd_request();
	enable_irq(HD_IRQ);
}

static int hd_ioctl(struct inode * inode, struct file * file,
	unsigned int cmd, unsigned long arg)
{
	struct hd_geometry *loc = (struct hd_geometry *) arg;
	int dev;

	if ((!inode) || !(inode->i_rdev))
		return -EINVAL;
	dev = DEVICE_NR(inode->i_rdev);
	if (dev >= NR_HD)
		return -EINVAL;
	switch (cmd) {
		case HDIO_GETGEO:
		{
			struct hd_geometry g; 
			if (!loc)  return -EINVAL;
			g.heads = hd_info[dev].head;
			g.sectors = hd_info[dev].sect;
			g.cylinders = hd_info[dev].cyl;
			g.start = hd[MINOR(inode->i_rdev)].start_sect;
			return copy_to_user(loc, &g, sizeof g) ? -EFAULT : 0; 
		}

         	case BLKGETSIZE:   /* Return device size */
			return put_user(hd[MINOR(inode->i_rdev)].nr_sects, 
					(unsigned long *) arg);
         	case BLKGETSIZE64:
			return put_user((u64)hd[MINOR(inode->i_rdev)].nr_sects << 9, 
					(u64 *) arg);

		case BLKRRPART: /* Re-read partition tables */
			if (!capable(CAP_SYS_ADMIN))
				return -EACCES;
			return revalidate_hddisk(inode->i_rdev, 1);

		case BLKROSET:
		case BLKROGET:
		case BLKRASET:
		case BLKRAGET:
		case BLKFLSBUF:
		case BLKPG:
			return blk_ioctl(inode->i_rdev, cmd, arg);

		default:
			return -EINVAL;
	}
}

static int hd_open(struct inode * inode, struct file * filp)
{
	int target;
	target =  DEVICE_NR(inode->i_rdev);

	if (target >= NR_HD)
		return -ENODEV;
	while (busy[target])
		sleep_on(&busy_wait);
	access_count[target]++;
	return 0;
}

/*
 * Releasing a block device means we sync() it, so that it can safely
 * be forgotten about...
 */
static int hd_release(struct inode * inode, struct file * file)
{
        int target =  DEVICE_NR(inode->i_rdev);
	access_count[target]--;
	return 0;
}

extern struct block_device_operations hd_fops;

static struct gendisk hd_gendisk = {
	major:		MAJOR_NR,
	major_name:	"hd",
	minor_shift:	6,
	max_p:		1 << 6,
	part:		hd,
	sizes:		hd_sizes,
	fops:		&hd_fops,
};
	
static void hd_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	void (*handler)(void) = DEVICE_INTR;

	DEVICE_INTR = NULL;
	del_timer(&device_timer);
	if (!handler)
		handler = unexpected_hd_interrupt;
	handler();
	sti();
}

static struct block_device_operations hd_fops = {
	open:		hd_open,
	release:	hd_release,
	ioctl:		hd_ioctl,
};

/*
 * This is the hard disk IRQ description. The SA_INTERRUPT in sa_flags
 * means we run the IRQ-handler with interrupts disabled:  this is bad for
 * interrupt latency, but anything else has led to problems on some
 * machines.
 *
 * We enable interrupts in some of the routines after making sure it's
 * safe.
 */
static void __init hd_geninit(void)
{
	int drive;

	for(drive=0; drive < (MAX_HD << 6); drive++) {
		hd_blocksizes[drive] = 1024;
		hd_hardsectsizes[drive] = 512;
		hd_maxsect[drive]=255;
	}
	blksize_size[MAJOR_NR] = hd_blocksizes;
	hardsect_size[MAJOR_NR] = hd_hardsectsizes;
	max_sectors[MAJOR_NR] = hd_maxsect;

#ifdef __i386__
	if (!NR_HD) {
		extern struct drive_info drive_info;
		unsigned char *BIOS = (unsigned char *) &drive_info;
		unsigned long flags;
		int cmos_disks;

		for (drive=0 ; drive<2 ; drive++) {
			hd_info[drive].cyl = *(unsigned short *) BIOS;
			hd_info[drive].head = *(2+BIOS);
			hd_info[drive].wpcom = *(unsigned short *) (5+BIOS);
			hd_info[drive].ctl = *(8+BIOS);
			hd_info[drive].lzone = *(unsigned short *) (12+BIOS);
			hd_info[drive].sect = *(14+BIOS);
#ifdef does_not_work_for_everybody_with_scsi_but_helps_ibm_vp
			if (hd_info[drive].cyl && NR_HD == drive)
				NR_HD++;
#endif
			BIOS += 16;
		}

	/*
		We query CMOS about hard disks : it could be that 
		we have a SCSI/ESDI/etc controller that is BIOS
		compatible with ST-506, and thus showing up in our
		BIOS table, but not register compatible, and therefore
		not present in CMOS.

		Furthermore, we will assume that our ST-506 drives
		<if any> are the primary drives in the system, and 
		the ones reflected as drive 1 or 2.

		The first drive is stored in the high nibble of CMOS
		byte 0x12, the second in the low nibble.  This will be
		either a 4 bit drive type or 0xf indicating use byte 0x19 
		for an 8 bit type, drive 1, 0x1a for drive 2 in CMOS.

		Needless to say, a non-zero value means we have 
		an AT controller hard disk for that drive.

		Currently the rtc_lock is a bit academic since this
		driver is non-modular, but someday... ?         Paul G.
	*/

		spin_lock_irqsave(&rtc_lock, flags);
		cmos_disks = CMOS_READ(0x12);
		spin_unlock_irqrestore(&rtc_lock, flags);

		if (cmos_disks & 0xf0) {
			if (cmos_disks & 0x0f)
				NR_HD = 2;
			else
				NR_HD = 1;
		}
	}
#endif /* __i386__ */
#ifdef __arm__
	if (!NR_HD) {
		/* We don't know anything about the drive.  This means
		 * that you *MUST* specify the drive parameters to the
		 * kernel yourself.
		 */
		printk("hd: no drives specified - use hd=cyl,head,sectors"
			" on kernel command line\n");
	}
#endif

	for (drive=0 ; drive < NR_HD ; drive++) {
		hd[drive<<6].nr_sects = hd_info[drive].head *
			hd_info[drive].sect * hd_info[drive].cyl;
		printk ("hd%c: %ldMB, CHS=%d/%d/%d\n", drive+'a',
			hd[drive<<6].nr_sects / 2048, hd_info[drive].cyl,
			hd_info[drive].head, hd_info[drive].sect);
	}
	if (!NR_HD)
		return;

	if (request_irq(HD_IRQ, hd_interrupt, SA_INTERRUPT, "hd", NULL)) {
		printk("hd: unable to get IRQ%d for the hard disk driver\n",
			HD_IRQ);
		NR_HD = 0;
		return;
	}
	request_region(HD_DATA, 8, "hd");
	request_region(HD_CMD, 1, "hd(cmd)");

	hd_gendisk.nr_real = NR_HD;

	for(drive=0; drive < NR_HD; drive++)
		register_disk(&hd_gendisk, MKDEV(MAJOR_NR,drive<<6), 1<<6,
			&hd_fops, hd_info[drive].head * hd_info[drive].sect *
			hd_info[drive].cyl);
}

int __init hd_init(void)
{
	if (devfs_register_blkdev(MAJOR_NR,"hd",&hd_fops)) {
		printk("hd: unable to get major %d for hard disk\n",MAJOR_NR);
		return -1;
	}
	blk_init_queue(BLK_DEFAULT_QUEUE(MAJOR_NR), DEVICE_REQUEST);
	read_ahead[MAJOR_NR] = 8;		/* 8 sector (4kB) read-ahead */
	add_gendisk(&hd_gendisk);
	init_timer(&device_timer);
	device_timer.function = hd_times_out;
	hd_geninit();
	return 0;
}

#define DEVICE_BUSY busy[target]
#define USAGE access_count[target]
#define CAPACITY (hd_info[target].head*hd_info[target].sect*hd_info[target].cyl)
/* We assume that the BIOS parameters do not change, so the disk capacity
   will not change */
#undef MAYBE_REINIT
#define GENDISK_STRUCT hd_gendisk

/*
 * This routine is called to flush all partitions and partition tables
 * for a changed disk, and then re-read the new partition table.
 * If we are revalidating a disk because of a media change, then we
 * enter with usage == 0.  If we are using an ioctl, we automatically have
 * usage == 1 (we need an open channel to use an ioctl :-), so this
 * is our limit.
 */
static int revalidate_hddisk(kdev_t dev, int maxusage)
{
	int target;
	struct gendisk * gdev;
	int max_p;
	int start;
	int i;
	long flags;

	target = DEVICE_NR(dev);
	gdev = &GENDISK_STRUCT;

	spin_lock_irqsave(&io_request_lock, flags);
	if (DEVICE_BUSY || USAGE > maxusage) {
		spin_unlock_irqrestore(&io_request_lock, flags);
		return -EBUSY;
	}
	DEVICE_BUSY = 1;
	spin_unlock_irqrestore(&io_request_lock, flags);

	max_p = gdev->max_p;
	start = target << gdev->minor_shift;

	for (i=max_p - 1; i >=0 ; i--) {
		int minor = start + i;
		invalidate_device(MKDEV(MAJOR_NR, minor), 1);
		gdev->part[minor].start_sect = 0;
		gdev->part[minor].nr_sects = 0;
	}

#ifdef MAYBE_REINIT
	MAYBE_REINIT;
#endif

	grok_partitions(gdev, target, 1<<6, CAPACITY);

	DEVICE_BUSY = 0;
	wake_up(&busy_wait);
	return 0;
}

static int parse_hd_setup (char *line) {
	int ints[6];

	(void) get_options(line, ARRAY_SIZE(ints), ints);
	hd_setup(NULL, ints);

	return 1;
}
__setup("hd=", parse_hd_setup);

module_init(hd_init);
