/*
 * Simulated SCSI driver.
 *
 * Copyright (C) 1999, 2001-2003 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 *	Stephane Eranian <eranian@hpl.hp.com>
 *
 * 02/01/15 David Mosberger	Updated for v2.5.1
 * 99/12/18 David Mosberger	Added support for READ10/WRITE10 needed by linux v2.3.33
 */
#include <linux/config.h>
#include <linux/blk.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/timer.h>

#include <scsi/scsi.h>

#include <asm/irq.h>

#include "../drivers/scsi/scsi.h"
#include "../drivers/scsi/sd.h"
#include "../drivers/scsi/hosts.h"
#include "simscsi.h"

#define DEBUG_SIMSCSI	1

/* Simulator system calls: */

#define SSC_OPEN			50
#define SSC_CLOSE			51
#define SSC_READ			52
#define SSC_WRITE			53
#define SSC_GET_COMPLETION		54
#define SSC_WAIT_COMPLETION		55

#define SSC_WRITE_ACCESS		2
#define SSC_READ_ACCESS			1

#if DEBUG_SIMSCSI
  int simscsi_debug;
# define DBG	simscsi_debug
#else
# define DBG	0
#endif

static void simscsi_interrupt (unsigned long val);
DECLARE_TASKLET(simscsi_tasklet, simscsi_interrupt, 0);

struct disk_req {
	unsigned long addr;
	unsigned len;
};

struct disk_stat {
	int fd;
	unsigned count;
};

extern long ia64_ssc (long arg0, long arg1, long arg2, long arg3, int nr);

static int desc[16] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

static struct queue_entry {
	Scsi_Cmnd *sc;
} queue[SIMSCSI_REQ_QUEUE_LEN];

static int rd, wr;
static atomic_t num_reqs = ATOMIC_INIT(0);

/* base name for default disks */
static char *simscsi_root = DEFAULT_SIMSCSI_ROOT;

#define MAX_ROOT_LEN	128

/*
 * used to setup a new base for disk images
 * to use /foo/bar/disk[a-z] as disk images
 * you have to specify simscsi=/foo/bar/disk on the command line
 */
static int __init
simscsi_setup (char *s)
{
	/* XXX Fix me we may need to strcpy() ? */
	if (strlen(s) > MAX_ROOT_LEN) {
		printk(KERN_ERR "simscsi_setup: prefix too long---using default %s\n",
		       simscsi_root);
	}
	simscsi_root = s;
	return 1;
}

__setup("simscsi=", simscsi_setup);

static void
simscsi_interrupt (unsigned long val)
{
	unsigned long flags;
	Scsi_Cmnd *sc;

	spin_lock_irqsave(&io_request_lock, flags);
	{
		while ((sc = queue[rd].sc) != 0) {
			atomic_dec(&num_reqs);
			queue[rd].sc = 0;
			if (DBG)
				printk("simscsi_interrupt: done with %ld\n", sc->serial_number);
			(*sc->scsi_done)(sc);
			rd = (rd + 1) % SIMSCSI_REQ_QUEUE_LEN;
		}
	}
	spin_unlock_irqrestore(&io_request_lock, flags);
}

int
simscsi_detect (Scsi_Host_Template *templ)
{
	templ->proc_name = "simscsi";
	return 1;	/* fake one SCSI host adapter */
}

int
simscsi_release (struct Scsi_Host *host)
{
	return 0;	/* this is easy...  */
}

const char *
simscsi_info (struct Scsi_Host *host)
{
	return "simulated SCSI host adapter";
}

int
simscsi_biosparam (Disk *disk, kdev_t n, int ip[])
{
	unsigned capacity = disk->capacity;

	ip[0] = 64;		/* heads */
	ip[1] = 32;		/* sectors */
	ip[2] = capacity >> 11;	/* cylinders */
	return 0;
}

static void
simscsi_readwrite (Scsi_Cmnd *sc, int mode, unsigned long offset, unsigned long len)
{
	struct disk_stat stat;
	struct disk_req req;

	req.addr = __pa(sc->request_buffer);
	req.len  = len;			/* # of bytes to transfer */

	if (sc->request_bufflen < req.len)
		return;

	stat.fd = desc[sc->target];
	if (DBG)
		printk("simscsi_%s @ %lx (off %lx)\n",
		       mode == SSC_READ ? "read":"write", req.addr, offset);
	ia64_ssc(stat.fd, 1, __pa(&req), offset, mode);
	ia64_ssc(__pa(&stat), 0, 0, 0, SSC_WAIT_COMPLETION);

	if (stat.count == req.len) {
		sc->result = GOOD;
	} else {
		sc->result = DID_ERROR << 16;
	}
}

static void
simscsi_sg_readwrite (Scsi_Cmnd *sc, int mode, unsigned long offset)
{
	int list_len = sc->use_sg;
	struct scatterlist *sl = (struct scatterlist *)sc->buffer;
	struct disk_stat stat;
	struct disk_req req;

	stat.fd = desc[sc->target];

	while (list_len) {
		req.addr = __pa(sl->address);
		req.len  = sl->length;
		if (DBG)
			printk("simscsi_sg_%s @ %lx (off %lx) use_sg=%d len=%d\n",
			       mode == SSC_READ ? "read":"write", req.addr, offset,
			       list_len, sl->length);
		ia64_ssc(stat.fd, 1, __pa(&req), offset, mode);
		ia64_ssc(__pa(&stat), 0, 0, 0, SSC_WAIT_COMPLETION);

		/* should not happen in our case */
		if (stat.count != req.len) {
			sc->result = DID_ERROR << 16;
			return;
		}
		offset +=  sl->length;
		sl++;
		list_len--;
	}
	sc->result = GOOD;
}

/*
 * function handling both READ_6/WRITE_6 (non-scatter/gather mode)
 * commands.
 * Added 02/26/99 S.Eranian
 */
static void
simscsi_readwrite6 (Scsi_Cmnd *sc, int mode)
{
	unsigned long offset;

	offset = (((sc->cmnd[1] & 0x1f) << 16) | (sc->cmnd[2] << 8) | sc->cmnd[3])*512;
	if (sc->use_sg > 0)
		simscsi_sg_readwrite(sc, mode, offset);
	else
		simscsi_readwrite(sc, mode, offset, sc->cmnd[4]*512);
}

static size_t
simscsi_get_disk_size (int fd)
{
	struct disk_stat stat;
	size_t bit, sectors = 0;
	struct disk_req req;
	char buf[512];

	/*
	 * This is a bit kludgey: the simulator doesn't provide a direct way of determining
	 * the disk size, so we do a binary search, assuming a maximum disk size of 4GB.
	 */
	for (bit = (4UL << 30)/512; bit != 0; bit >>= 1) {
		req.addr = __pa(&buf);
		req.len = sizeof(buf);
		ia64_ssc(fd, 1, __pa(&req), ((sectors | bit) - 1)*512, SSC_READ);
		stat.fd = fd;
		ia64_ssc(__pa(&stat), 0, 0, 0, SSC_WAIT_COMPLETION);
		if (stat.count == sizeof(buf))
			sectors |= bit;
	}
	return sectors - 1;	/* return last valid sector number */
}

static void
simscsi_readwrite10 (Scsi_Cmnd *sc, int mode)
{
	unsigned long offset;

	offset = (  (sc->cmnd[2] << 24) | (sc->cmnd[3] << 16)
		  | (sc->cmnd[4] <<  8) | (sc->cmnd[5] <<  0))*512;
	if (sc->use_sg > 0)
		simscsi_sg_readwrite(sc, mode, offset);
	else
		simscsi_readwrite(sc, mode, offset, ((sc->cmnd[7] << 8) | sc->cmnd[8])*512);
}

int
simscsi_queuecommand (Scsi_Cmnd *sc, void (*done)(Scsi_Cmnd *))
{
	char fname[MAX_ROOT_LEN+16];
	size_t disk_size;
	char *buf;
#if DEBUG_SIMSCSI
	register long sp asm ("sp");

	if (DBG)
		printk("simscsi_queuecommand: target=%d,cmnd=%u,sc=%lu,sp=%lx,done=%p\n",
		       sc->target, sc->cmnd[0], sc->serial_number, sp, done);
#endif

	sc->result = DID_BAD_TARGET << 16;
	sc->scsi_done = done;
	if (sc->target <= 15 && sc->lun == 0) {
		switch (sc->cmnd[0]) {
		      case INQUIRY:
			if (sc->request_bufflen < 35) {
				break;
			}
			sprintf (fname, "%s%c", simscsi_root, 'a' + sc->target);
			desc[sc->target] = ia64_ssc(__pa(fname), SSC_READ_ACCESS|SSC_WRITE_ACCESS,
						    0, 0, SSC_OPEN);
			if (desc[sc->target] < 0) {
				/* disk doesn't exist... */
				break;
			}
			buf = sc->request_buffer;
			buf[0] = 0;	/* magnetic disk */
			buf[1] = 0;	/* not a removable medium */
			buf[2] = 2;	/* SCSI-2 compliant device */
			buf[3] = 2;	/* SCSI-2 response data format */
			buf[4] = 31;	/* additional length (bytes) */
			buf[5] = 0;	/* reserved */
			buf[6] = 0;	/* reserved */
			buf[7] = 0;	/* various flags */
			memcpy(buf + 8, "HP      SIMULATED DISK  0.00",  28);
			sc->result = GOOD;
			break;

		      case TEST_UNIT_READY:
			sc->result = GOOD;
			break;

		      case READ_6:
			if (desc[sc->target] < 0 )
				break;
			simscsi_readwrite6(sc, SSC_READ);
			break;

		      case READ_10:
			if (desc[sc->target] < 0 )
				break;
			simscsi_readwrite10(sc, SSC_READ);
			break;

		      case WRITE_6:
			if (desc[sc->target] < 0)
				break;
			simscsi_readwrite6(sc, SSC_WRITE);
			break;

		      case WRITE_10:
			if (desc[sc->target] < 0)
				break;
			simscsi_readwrite10(sc, SSC_WRITE);
			break;


		      case READ_CAPACITY:
			if (desc[sc->target] < 0 || sc->request_bufflen < 8) {
				break;
			}
			buf = sc->request_buffer;

			disk_size = simscsi_get_disk_size(desc[sc->target]);
			buf[0] = (disk_size >> 24) & 0xff;
			buf[1] = (disk_size >> 16) & 0xff;
			buf[2] = (disk_size >>  8) & 0xff;
			buf[3] = (disk_size >>  0) & 0xff;
			/* set block size of 512 bytes: */
			buf[4] = 0;
			buf[5] = 0;
			buf[6] = 2;
			buf[7] = 0;
			sc->result = GOOD;
			break;

		      case MODE_SENSE:
			/* sd.c uses this to determine whether disk does write-caching. */
			memset(sc->request_buffer, 0, 128);
			sc->result = GOOD;
			break;

		      case START_STOP:
			printk(KERN_ERR "START_STOP\n");
			break;

		      default:
			panic("simscsi: unknown SCSI command %u\n", sc->cmnd[0]);
		}
	}
	if (sc->result == DID_BAD_TARGET) {
		sc->result |= DRIVER_SENSE << 24;
		sc->sense_buffer[0] = 0x70;
		sc->sense_buffer[2] = 0x00;
	}
	if (atomic_read(&num_reqs) >= SIMSCSI_REQ_QUEUE_LEN) {
		panic("Attempt to queue command while command is pending!!");
	}
	atomic_inc(&num_reqs);
	queue[wr].sc = sc;
	wr = (wr + 1) % SIMSCSI_REQ_QUEUE_LEN;

	tasklet_schedule(&simscsi_tasklet);
	return 0;
}

int
simscsi_reset (Scsi_Cmnd *cmd, unsigned int reset_flags)
{
	printk(KERN_ERR "simscsi_reset: unimplemented\n");
	return SCSI_RESET_SUCCESS;
}

int
simscsi_abort (Scsi_Cmnd *cmd)
{
	printk(KERN_ERR "simscsi_abort: unimplemented\n");
	return SCSI_ABORT_SUCCESS;
}

static Scsi_Host_Template driver_template = SIMSCSI;

#include "../drivers/scsi/scsi_module.c"
