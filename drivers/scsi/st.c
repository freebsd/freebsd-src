/*
   SCSI Tape Driver for Linux version 1.1 and newer. See the accompanying
   file README.st for more information.

   History:
   Rewritten from Dwayne Forsyth's SCSI tape driver by Kai Makisara.
   Contribution and ideas from several people including (in alphabetical
   order) Klaus Ehrenfried, Eugene Exarevsky, Eric Lee Green, Wolfgang Denk,
   Steve Hirsch, Andreas Koppenh"ofer, Michael Leodolter, Eyal Lebedinsky,
   Michael Schaefer, J"org Weule, and Eric Youngdale.

   Copyright 1992 - 2004 Kai Makisara
   email Kai.Makisara@kolumbus.fi

   Last modified: Fri Jan  2 17:50:08 2004 by makisara
   Some small formal changes - aeb, 950809

   Last modified: 18-JAN-1998 Richard Gooch <rgooch@atnf.csiro.au> Devfs support

   Reminder: write_lock_irqsave() can be replaced by write_lock() when the old SCSI
   error handling will be discarded.
 */

static char *verstr = "20040102";

#include <linux/module.h>

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/mtio.h>
#include <linux/ioctl.h>
#include <linux/fcntl.h>
#include <linux/spinlock.h>
#include <linux/smp_lock.h>
#include <asm/uaccess.h>
#include <asm/dma.h>
#include <asm/system.h>

/* The driver prints some debugging information on the console if DEBUG
   is defined and non-zero. */
#define DEBUG 0

#if DEBUG
/* The message level for the debug messages is currently set to KERN_NOTICE
   so that people can easily see the messages. Later when the debugging messages
   in the drivers are more widely classified, this may be changed to KERN_DEBUG. */
#define ST_DEB_MSG  KERN_NOTICE
#define DEB(a) a
#define DEBC(a) if (debugging) { a ; }
#else
#define DEB(a)
#define DEBC(a)
#endif

#define MAJOR_NR SCSI_TAPE_MAJOR
#include <linux/blk.h>

#include "scsi.h"
#include "hosts.h"
#include <scsi/scsi_ioctl.h>

#define ST_KILOBYTE 1024

#include "st_options.h"
#include "st.h"

#include "constants.h"

static int buffer_kbs;
static int max_buffers = (-1);
static int max_sg_segs;
static int blocking_open = ST_BLOCKING_OPEN;


MODULE_AUTHOR("Kai Makisara");
MODULE_DESCRIPTION("SCSI Tape Driver");
MODULE_LICENSE("GPL");

MODULE_PARM(buffer_kbs, "i");
MODULE_PARM_DESC(buffer_kbs, "Default driver buffer size (KB; 32)");
MODULE_PARM(max_buffers, "i");
MODULE_PARM_DESC(max_buffers, "Maximum number of buffer allocated at initialisation (4)");
MODULE_PARM(max_sg_segs, "i");
MODULE_PARM_DESC(max_sg_segs, "Maximum number of scatter/gather segments to use (32)");
MODULE_PARM(blocking_open, "i");
MODULE_PARM_DESC(blocking_open, "Block in open if not ready an no O_NONBLOCK (0)");

EXPORT_NO_SYMBOLS;

#ifndef MODULE
static struct st_dev_parm {
	char *name;
	int *val;
} parms[] __initdata = {
	{
		"buffer_kbs", &buffer_kbs
	},
	{	/* Retained for compatibility */
		"write_threshold_kbs", NULL
	},
	{
		"max_buffers", &max_buffers
	},
	{
		"max_sg_segs", &max_sg_segs
	},
	{
		"blocking_open", &blocking_open
	}
};
#endif


/* The default definitions have been moved to st_options.h */

#define ST_BUFFER_SIZE (ST_BUFFER_BLOCKS * ST_KILOBYTE)

/* The buffer size should fit into the 24 bits for length in the
   6-byte SCSI read and write commands. */
#if ST_BUFFER_SIZE >= (2 << 24 - 1)
#error "Buffer size should not exceed (2 << 24 - 1) bytes!"
#endif

DEB( static int debugging = DEBUG; )

#define MAX_RETRIES 0
#define MAX_WRITE_RETRIES 0
#define MAX_READY_RETRIES 5
#define NO_TAPE  NOT_READY

#define ST_TIMEOUT (900 * HZ)
#define ST_LONG_TIMEOUT (14000 * HZ)

#define TAPE_NR(x) (MINOR(x) & ~(128 | ST_MODE_MASK))
#define TAPE_MODE(x) ((MINOR(x) & ST_MODE_MASK) >> ST_MODE_SHIFT)

/* Internal ioctl to set both density (uppermost 8 bits) and blocksize (lower
   24 bits) */
#define SET_DENS_AND_BLK 0x10001

#define ST_DEV_ARR_LUMP  6
static rwlock_t st_dev_arr_lock = RW_LOCK_UNLOCKED;

static int st_nbr_buffers;
static ST_buffer **st_buffers = NULL;
static int st_buffer_size = ST_BUFFER_SIZE;
static int st_max_buffers = ST_MAX_BUFFERS;
static int st_max_sg_segs = ST_MAX_SG;

static Scsi_Tape **scsi_tapes = NULL;

static int modes_defined;

static ST_buffer *new_tape_buffer(int, int, int);
static int enlarge_buffer(ST_buffer *, int, int);
static void normalize_buffer(ST_buffer *);
static int set_sg_lengths(ST_buffer *, unsigned int);
static int append_to_buffer(const char *, ST_buffer *, int);
static int from_buffer(ST_buffer *, char *, int);
static void move_buffer_data(ST_buffer *, int);

static int st_init(void);
static int st_attach(Scsi_Device *);
static int st_detect(Scsi_Device *);
static void st_detach(Scsi_Device *);

static struct Scsi_Device_Template st_template =
{
	name:"tape", 
	tag:"st", 
	scsi_type:TYPE_TAPE,
	major:SCSI_TAPE_MAJOR, 
	detect:st_detect, 
	init:st_init,
	attach:st_attach, 
	detach:st_detach
};

static int st_compression(Scsi_Tape *, int);

static int find_partition(Scsi_Tape *);
static int update_partition(Scsi_Tape *);

static int st_int_ioctl(Scsi_Tape *, unsigned int, unsigned long);


#include "osst_detect.h"
#ifndef SIGS_FROM_OSST
#define SIGS_FROM_OSST \
	{"OnStream", "SC-", "", "osst"}, \
	{"OnStream", "DI-", "", "osst"}, \
	{"OnStream", "DP-", "", "osst"}, \
	{"OnStream", "USB", "", "osst"}, \
	{"OnStream", "FW-", "", "osst"}
#endif

struct st_reject_data {
	char *vendor;
	char *model;
	char *rev;
	char *driver_hint; /* Name of the correct driver, NULL if unknown */
};

static struct st_reject_data reject_list[] = {
	/* {"XXX", "Yy-", "", NULL},  example */
	SIGS_FROM_OSST,
	{NULL, }};

/* If the device signature is on the list of incompatible drives, the
   function returns a pointer to the name of the correct driver (if known) */
static char * st_incompatible(Scsi_Device* SDp)
{
	struct st_reject_data *rp;

	for (rp=&(reject_list[0]); rp->vendor != NULL; rp++)
		if (!strncmp(rp->vendor, SDp->vendor, strlen(rp->vendor)) &&
		    !strncmp(rp->model, SDp->model, strlen(rp->model)) &&
		    !strncmp(rp->rev, SDp->rev, strlen(rp->rev))) {
			if (rp->driver_hint)
				return rp->driver_hint;
			else
				return "unknown";
		}
	return NULL;
}


/* Convert the result to success code */
static int st_chk_result(Scsi_Tape *STp, Scsi_Request * SRpnt)
{
	int dev;
	int result = SRpnt->sr_result;
	unsigned char *sense = SRpnt->sr_sense_buffer, scode;
	DEB(const char *stp;)

	if (!result) {
		sense[0] = 0;	/* We don't have sense data if this byte is zero */
		return 0;
	}

	if ((driver_byte(result) & DRIVER_SENSE) == DRIVER_SENSE)
		scode = sense[2] & 0x0f;
	else {
		sense[0] = 0;
		scode = 0;
	}

	dev = TAPE_NR(SRpnt->sr_request.rq_dev);
        DEB(
        if (debugging) {
                printk(ST_DEB_MSG "st%d: Error: %x, cmd: %x %x %x %x %x %x Len: %d\n",
		       dev, result,
		       SRpnt->sr_cmnd[0], SRpnt->sr_cmnd[1], SRpnt->sr_cmnd[2],
		       SRpnt->sr_cmnd[3], SRpnt->sr_cmnd[4], SRpnt->sr_cmnd[5],
		       SRpnt->sr_bufflen);
		if (driver_byte(result) & DRIVER_SENSE)
			print_req_sense("st", SRpnt);
	} else ) /* end DEB */
		if (!(driver_byte(result) & DRIVER_SENSE) ||
		    ((sense[0] & 0x70) == 0x70 &&
		     scode != NO_SENSE &&
		     scode != RECOVERED_ERROR &&
                     /* scode != UNIT_ATTENTION && */
		     scode != BLANK_CHECK &&
		     scode != VOLUME_OVERFLOW &&
		     SRpnt->sr_cmnd[0] != MODE_SENSE &&
		     SRpnt->sr_cmnd[0] != TEST_UNIT_READY)) {	/* Abnormal conditions for tape */
		if (driver_byte(result) & DRIVER_SENSE) {
			printk(KERN_WARNING "st%d: Error with sense data: ", dev);
			print_req_sense("st", SRpnt);
		} else
			printk(KERN_WARNING
			       "st%d: Error %x (sugg. bt 0x%x, driver bt 0x%x, host bt 0x%x).\n",
			       dev, result, suggestion(result),
                               driver_byte(result) & DRIVER_MASK, host_byte(result));
	}

	if (STp->cln_mode >= EXTENDED_SENSE_START) {
		if (STp->cln_sense_value)
			STp->cleaning_req |= ((SRpnt->sr_sense_buffer[STp->cln_mode] &
					       STp->cln_sense_mask) == STp->cln_sense_value);
		else
			STp->cleaning_req |= ((SRpnt->sr_sense_buffer[STp->cln_mode] &
					       STp->cln_sense_mask) != 0);
	}
	if (sense[12] == 0 && sense[13] == 0x17) /* ASC and ASCQ => cleaning requested */
		STp->cleaning_req = 1;

	if ((sense[0] & 0x70) == 0x70 &&
	    scode == RECOVERED_ERROR
#if ST_RECOVERED_WRITE_FATAL
	    && SRpnt->sr_cmnd[0] != WRITE_6
	    && SRpnt->sr_cmnd[0] != WRITE_FILEMARKS
#endif
	    ) {
		STp->recover_count++;
		STp->recover_reg++;

                DEB(
		if (debugging) {
			if (SRpnt->sr_cmnd[0] == READ_6)
				stp = "read";
			else if (SRpnt->sr_cmnd[0] == WRITE_6)
				stp = "write";
			else
				stp = "ioctl";
			printk(ST_DEB_MSG "st%d: Recovered %s error (%d).\n", dev, stp,
			       STp->recover_count);
		} ) /* end DEB */

		if ((sense[2] & 0xe0) == 0)
			return 0;
	}
	return (-EIO);
}


/* Wakeup from interrupt */
static void st_sleep_done(Scsi_Cmnd * SCpnt)
{
	unsigned int st_nbr;
	int remainder;
	Scsi_Tape *STp;

	st_nbr = TAPE_NR(SCpnt->request.rq_dev);
	read_lock(&st_dev_arr_lock);
	STp = scsi_tapes[st_nbr];
	read_unlock(&st_dev_arr_lock);
	if ((STp->buffer)->writing &&
	    (SCpnt->sense_buffer[0] & 0x70) == 0x70 &&
	    (SCpnt->sense_buffer[2] & 0x40)) {
		/* EOM at write-behind, has all been written? */
		if ((SCpnt->sense_buffer[0] & 0x80) != 0)
			remainder = (SCpnt->sense_buffer[3] << 24) |
				(SCpnt->sense_buffer[4] << 16) |
				(SCpnt->sense_buffer[5] << 8) |
				SCpnt->sense_buffer[6];
		else
			remainder = 0;
		if ((SCpnt->sense_buffer[2] & 0x0f) == VOLUME_OVERFLOW ||
		    remainder > 0)
			(STp->buffer)->midlevel_result = SCpnt->result; /* Error */
		else
			(STp->buffer)->midlevel_result = INT_MAX;	/* OK */
	} else
		(STp->buffer)->midlevel_result = SCpnt->result;
	SCpnt->request.rq_status = RQ_SCSI_DONE;
	(STp->buffer)->last_SRpnt = SCpnt->sc_request;
	DEB( STp->write_pending = 0; )

	complete(SCpnt->request.waiting);
}


/* Do the scsi command. Waits until command performed if do_wait is true.
   Otherwise write_behind_check() is used to check that the command
   has finished. */
static Scsi_Request *
 st_do_scsi(Scsi_Request * SRpnt, Scsi_Tape * STp, unsigned char *cmd, int bytes,
	    int direction, int timeout, int retries, int do_wait)
{
	unsigned char *bp;

	if (SRpnt == NULL) {
		SRpnt = scsi_allocate_request(STp->device);
		if (SRpnt == NULL) {
			DEBC( printk(KERN_ERR "st%d: Can't get SCSI request.\n",
				     TAPE_NR(STp->devt)); );
			if (signal_pending(current))
				(STp->buffer)->syscall_result = (-EINTR);
			else
				(STp->buffer)->syscall_result = (-EBUSY);
			return NULL;
		}
	}

	if (SRpnt->sr_device->scsi_level <= SCSI_2)
		cmd[1] |= (SRpnt->sr_device->lun << 5) & 0xe0;
	init_completion(&STp->wait);
	SRpnt->sr_use_sg = (bytes > (STp->buffer)->sg_lengths[0]) ?
	    (STp->buffer)->use_sg : 0;
	if (SRpnt->sr_use_sg) {
		bp = (char *) &((STp->buffer)->sg[0]);
		SRpnt->sr_use_sg = set_sg_lengths(STp->buffer, bytes);
	} else
		bp = (STp->buffer)->b_data;
	SRpnt->sr_data_direction = direction;
	SRpnt->sr_cmd_len = 0;
	SRpnt->sr_request.waiting = &(STp->wait);
	SRpnt->sr_request.rq_status = RQ_SCSI_BUSY;
	SRpnt->sr_request.rq_dev = STp->devt;

	scsi_do_req(SRpnt, (void *) cmd, bp, bytes,
		    st_sleep_done, timeout, retries);

	if (do_wait) {
		wait_for_completion(SRpnt->sr_request.waiting);
		SRpnt->sr_request.waiting = NULL;
		(STp->buffer)->syscall_result = st_chk_result(STp, SRpnt);
	}
	return SRpnt;
}


/* Handle the write-behind checking (downs the semaphore) */
static void write_behind_check(Scsi_Tape * STp)
{
	ST_buffer *STbuffer;
	ST_partstat *STps;

	STbuffer = STp->buffer;

        DEB(
	if (STp->write_pending)
		STp->nbr_waits++;
	else
		STp->nbr_finished++;
        ) /* end DEB */

	wait_for_completion(&(STp->wait));
	(STp->buffer)->last_SRpnt->sr_request.waiting = NULL;

	(STp->buffer)->syscall_result = st_chk_result(STp, (STp->buffer)->last_SRpnt);
	scsi_release_request((STp->buffer)->last_SRpnt);

	STbuffer->buffer_bytes -= STbuffer->writing;
	STps = &(STp->ps[STp->partition]);
	if (STps->drv_block >= 0) {
		if (STp->block_size == 0)
			STps->drv_block++;
		else
			STps->drv_block += STbuffer->writing / STp->block_size;
	}
	STbuffer->writing = 0;

	return;
}


/* Step over EOF if it has been inadvertently crossed (ioctl not used because
   it messes up the block number). */
static int cross_eof(Scsi_Tape * STp, int forward)
{
	Scsi_Request *SRpnt;
	unsigned char cmd[MAX_COMMAND_SIZE];

	cmd[0] = SPACE;
	cmd[1] = 0x01;		/* Space FileMarks */
	if (forward) {
		cmd[2] = cmd[3] = 0;
		cmd[4] = 1;
	} else
		cmd[2] = cmd[3] = cmd[4] = 0xff;	/* -1 filemarks */
	cmd[5] = 0;

        DEBC(printk(ST_DEB_MSG "st%d: Stepping over filemark %s.\n",
		   TAPE_NR(STp->devt), forward ? "forward" : "backward"));

	SRpnt = st_do_scsi(NULL, STp, cmd, 0, SCSI_DATA_NONE,
			   STp->timeout, MAX_RETRIES, TRUE);
	if (!SRpnt)
		return (STp->buffer)->syscall_result;

	scsi_release_request(SRpnt);
	SRpnt = NULL;

	if ((STp->buffer)->midlevel_result != 0)
		printk(KERN_ERR "st%d: Stepping over filemark %s failed.\n",
		   TAPE_NR(STp->devt), forward ? "forward" : "backward");

	return (STp->buffer)->syscall_result;
}


/* Flush the write buffer (never need to write if variable blocksize). */
static int flush_write_buffer(Scsi_Tape * STp)
{
	int offset, transfer, blks;
	int result;
	unsigned char cmd[MAX_COMMAND_SIZE];
	Scsi_Request *SRpnt;
	ST_partstat *STps;

	if ((STp->buffer)->writing) {
		write_behind_check(STp);
		if ((STp->buffer)->syscall_result) {
                        DEBC(printk(ST_DEB_MSG
                                       "st%d: Async write error (flush) %x.\n",
				       TAPE_NR(STp->devt), (STp->buffer)->midlevel_result))
			if ((STp->buffer)->midlevel_result == INT_MAX)
				return (-ENOSPC);
			return (-EIO);
		}
	}
	if (STp->block_size == 0)
		return 0;

	result = 0;
	if (STp->dirty == 1) {

		offset = (STp->buffer)->buffer_bytes;
		transfer = ((offset + STp->block_size - 1) /
			    STp->block_size) * STp->block_size;
                DEBC(printk(ST_DEB_MSG "st%d: Flushing %d bytes.\n",
                               TAPE_NR(STp->devt), transfer));

		memset((STp->buffer)->b_data + offset, 0, transfer - offset);

		memset(cmd, 0, MAX_COMMAND_SIZE);
		cmd[0] = WRITE_6;
		cmd[1] = 1;
		blks = transfer / STp->block_size;
		cmd[2] = blks >> 16;
		cmd[3] = blks >> 8;
		cmd[4] = blks;

		SRpnt = st_do_scsi(NULL, STp, cmd, transfer, SCSI_DATA_WRITE,
				   STp->timeout, MAX_WRITE_RETRIES, TRUE);
		if (!SRpnt)
			return (STp->buffer)->syscall_result;

		STps = &(STp->ps[STp->partition]);
		if ((STp->buffer)->syscall_result != 0) {
			if ((SRpnt->sr_sense_buffer[0] & 0x70) == 0x70 &&
			    (SRpnt->sr_sense_buffer[2] & 0x40) &&
			    (SRpnt->sr_sense_buffer[2] & 0x0f) == NO_SENSE) {
				STp->dirty = 0;
				(STp->buffer)->buffer_bytes = 0;
				result = (-ENOSPC);
			} else {
				printk(KERN_ERR "st%d: Error on flush.\n",
                                       TAPE_NR(STp->devt));
				result = (-EIO);
			}
			STps->drv_block = (-1);
		} else {
			if (STps->drv_block >= 0)
				STps->drv_block += blks;
			STp->dirty = 0;
			(STp->buffer)->buffer_bytes = 0;
		}
		scsi_release_request(SRpnt);
		SRpnt = NULL;
	}
	return result;
}


/* Flush the tape buffer. The tape will be positioned correctly unless
   seek_next is true. */
static int flush_buffer(Scsi_Tape *STp, int seek_next)
{
	int backspace, result;
	ST_buffer *STbuffer;
	ST_partstat *STps;

	STbuffer = STp->buffer;

	/*
	 * If there was a bus reset, block further access
	 * to this device.
	 */
	if (STp->device->was_reset)
		return (-EIO);

	if (STp->ready != ST_READY)
		return 0;

	STps = &(STp->ps[STp->partition]);
	if (STps->rw == ST_WRITING)	/* Writing */
		return flush_write_buffer(STp);

	if (STp->block_size == 0)
		return 0;

	backspace = ((STp->buffer)->buffer_bytes +
		     (STp->buffer)->read_pointer) / STp->block_size -
	    ((STp->buffer)->read_pointer + STp->block_size - 1) /
	    STp->block_size;
	(STp->buffer)->buffer_bytes = 0;
	(STp->buffer)->read_pointer = 0;
	result = 0;
	if (!seek_next) {
		if (STps->eof == ST_FM_HIT) {
			result = cross_eof(STp, FALSE);	/* Back over the EOF hit */
			if (!result)
				STps->eof = ST_NOEOF;
			else {
				if (STps->drv_file >= 0)
					STps->drv_file++;
				STps->drv_block = 0;
			}
		}
		if (!result && backspace > 0)
			result = st_int_ioctl(STp, MTBSR, backspace);
	} else if (STps->eof == ST_FM_HIT) {
		if (STps->drv_file >= 0)
			STps->drv_file++;
		STps->drv_block = 0;
		STps->eof = ST_NOEOF;
	}
	return result;

}

/* Set the mode parameters */
static int set_mode_densblk(Scsi_Tape * STp, ST_mode * STm)
{
	int set_it = FALSE;
	unsigned long arg;
	int dev = TAPE_NR(STp->devt);

	if (!STp->density_changed &&
	    STm->default_density >= 0 &&
	    STm->default_density != STp->density) {
		arg = STm->default_density;
		set_it = TRUE;
	} else
		arg = STp->density;
	arg <<= MT_ST_DENSITY_SHIFT;
	if (!STp->blksize_changed &&
	    STm->default_blksize >= 0 &&
	    STm->default_blksize != STp->block_size) {
		arg |= STm->default_blksize;
		set_it = TRUE;
	} else
		arg |= STp->block_size;
	if (set_it &&
	    st_int_ioctl(STp, SET_DENS_AND_BLK, arg)) {
		printk(KERN_WARNING
		       "st%d: Can't set default block size to %d bytes and density %x.\n",
		       dev, STm->default_blksize, STm->default_density);
		if (modes_defined)
			return (-EINVAL);
	}
	return 0;
}

/* Test if the drive is ready. Returns either one of the codes below or a negative system
   error code. */
#define CHKRES_READY       0
#define CHKRES_NEW_SESSION 1
#define CHKRES_NOT_READY   2
#define CHKRES_NO_TAPE     3

#define MAX_ATTENTIONS    10

static int test_ready(Scsi_Tape *STp, int do_wait)
{
	int attentions, waits, max_wait, scode;
	int retval = CHKRES_READY, new_session = FALSE;
	unsigned char cmd[MAX_COMMAND_SIZE];
	Scsi_Request *SRpnt = NULL;

	max_wait = do_wait ? ST_BLOCK_SECONDS : 0;

	for (attentions=waits=0; ; ) {
		memset((void *) &cmd[0], 0, MAX_COMMAND_SIZE);
		cmd[0] = TEST_UNIT_READY;
		SRpnt = st_do_scsi(SRpnt, STp, cmd, 0, SCSI_DATA_NONE,
				   STp->long_timeout, MAX_READY_RETRIES, TRUE);

		if (!SRpnt) {
			retval = (STp->buffer)->syscall_result;
			break;
		}

		if ((SRpnt->sr_sense_buffer[0] & 0x70) == 0x70) {

			scode = (SRpnt->sr_sense_buffer[2] & 0x0f);

			if (scode == UNIT_ATTENTION) { /* New media? */
				new_session = TRUE;
				if (attentions < MAX_ATTENTIONS) {
					attentions++;
					continue;
				}
				else {
					retval = (-EIO);
					break;
				}
			}

			if (scode == NOT_READY) {
				if (waits < max_wait) {
					set_current_state(TASK_INTERRUPTIBLE);
					schedule_timeout(HZ);
					if (signal_pending(current)) {
						retval = (-EINTR);
						break;
					}
					waits++;
					continue;
				}
				else {
					if ((STp->device)->scsi_level >= SCSI_2 &&
					    SRpnt->sr_sense_buffer[12] == 0x3a)	/* Check ASC */
						retval = CHKRES_NO_TAPE;
					else
						retval = CHKRES_NOT_READY;
					break;
				}
			}
		}

		retval = (STp->buffer)->syscall_result;
		if (!retval)
			retval = new_session ? CHKRES_NEW_SESSION : CHKRES_READY;
		break;
	}

	if (SRpnt != NULL)
		scsi_release_request(SRpnt);
	return retval;
}


/* See if the drive is ready and gather information about the tape. Return values:
   < 0   negative error code from errno.h
   0     drive ready
   1     drive not ready (possibly no tape)
*/

static int check_tape(Scsi_Tape *STp, struct file *filp)
{
	int i, retval, new_session = FALSE, do_wait;
	unsigned char cmd[MAX_COMMAND_SIZE], saved_cleaning;
	unsigned short st_flags = filp->f_flags;
	Scsi_Request *SRpnt = NULL;
	ST_mode *STm;
	ST_partstat *STps;
	int dev = TAPE_NR(STp->devt);
	struct inode *inode = filp->f_dentry->d_inode;
	int mode = TAPE_MODE(inode->i_rdev);

	STp->ready = ST_READY;

	if (mode != STp->current_mode) {
                DEBC(printk(ST_DEB_MSG "st%d: Mode change from %d to %d.\n",
			       dev, STp->current_mode, mode));
		new_session = TRUE;
		STp->current_mode = mode;
	}
	STm = &(STp->modes[STp->current_mode]);

	saved_cleaning = STp->cleaning_req;
	STp->cleaning_req = 0;

	do_wait = (blocking_open && (filp->f_flags & O_NONBLOCK) == 0);
	retval = test_ready(STp, do_wait);

	if (retval < 0)
		goto err_out;

	if (retval == CHKRES_NEW_SESSION) {
		(STp->device)->was_reset = 0;
		STp->partition = STp->new_partition = 0;
		if (STp->can_partitions)
			STp->nbr_partitions = 1; /* This guess will be updated later
                                                    if necessary */
		for (i = 0; i < ST_NBR_PARTITIONS; i++) {
			STps = &(STp->ps[i]);
			STps->rw = ST_IDLE;
			STps->eof = ST_NOEOF;
			STps->at_sm = 0;
			STps->last_block_valid = FALSE;
			STps->drv_block = 0;
			STps->drv_file = 0;
		}
		new_session = TRUE;
	}
	else {
		STp->cleaning_req |= saved_cleaning;

		if (retval == CHKRES_NOT_READY || retval == CHKRES_NO_TAPE) {
			if (retval == CHKRES_NO_TAPE)
				STp->ready = ST_NO_TAPE;
			else
				STp->ready = ST_NOT_READY;

			STp->density = 0;	/* Clear the erroneous "residue" */
			STp->write_prot = 0;
			STp->block_size = 0;
			STp->ps[0].drv_file = STp->ps[0].drv_block = (-1);
			STp->partition = STp->new_partition = 0;
			STp->door_locked = ST_UNLOCKED;
			return CHKRES_NOT_READY;
		}
	}

	if (STp->omit_blklims)
		STp->min_block = STp->max_block = (-1);
	else {
		memset((void *) &cmd[0], 0, MAX_COMMAND_SIZE);
		cmd[0] = READ_BLOCK_LIMITS;

		SRpnt = st_do_scsi(SRpnt, STp, cmd, 6, SCSI_DATA_READ, STp->timeout,
				   MAX_READY_RETRIES, TRUE);
		if (!SRpnt) {
			retval = (STp->buffer)->syscall_result;
			goto err_out;
		}

		if (!SRpnt->sr_result && !SRpnt->sr_sense_buffer[0]) {
			STp->max_block = ((STp->buffer)->b_data[1] << 16) |
			    ((STp->buffer)->b_data[2] << 8) | (STp->buffer)->b_data[3];
			STp->min_block = ((STp->buffer)->b_data[4] << 8) |
			    (STp->buffer)->b_data[5];
			if ( DEB( debugging || ) !STp->inited)
				printk(KERN_WARNING
                                       "st%d: Block limits %d - %d bytes.\n", dev,
                                       STp->min_block, STp->max_block);
		} else {
			STp->min_block = STp->max_block = (-1);
                        DEBC(printk(ST_DEB_MSG "st%d: Can't read block limits.\n",
                                       dev));
		}
	}

	memset((void *) &cmd[0], 0, MAX_COMMAND_SIZE);
	cmd[0] = MODE_SENSE;
	cmd[4] = 12;

	SRpnt = st_do_scsi(SRpnt, STp, cmd, 12, SCSI_DATA_READ, STp->timeout,
			   MAX_READY_RETRIES, TRUE);
	if (!SRpnt) {
		retval = (STp->buffer)->syscall_result;
		goto err_out;
	}

	if ((STp->buffer)->syscall_result != 0) {
                DEBC(printk(ST_DEB_MSG "st%d: No Mode Sense.\n", dev));
		STp->block_size = ST_DEFAULT_BLOCK;	/* Educated guess (?) */
		(STp->buffer)->syscall_result = 0;	/* Prevent error propagation */
		STp->drv_write_prot = 0;
	} else {
                DEBC(printk(ST_DEB_MSG
                            "st%d: Mode sense. Length %d, medium %x, WBS %x, BLL %d\n",
                            dev,
                            (STp->buffer)->b_data[0], (STp->buffer)->b_data[1],
                            (STp->buffer)->b_data[2], (STp->buffer)->b_data[3]));

		if ((STp->buffer)->b_data[3] >= 8) {
			STp->drv_buffer = ((STp->buffer)->b_data[2] >> 4) & 7;
			STp->density = (STp->buffer)->b_data[4];
			STp->block_size = (STp->buffer)->b_data[9] * 65536 +
			    (STp->buffer)->b_data[10] * 256 + (STp->buffer)->b_data[11];
                        DEBC(printk(ST_DEB_MSG
                                    "st%d: Density %x, tape length: %x, drv buffer: %d\n",
                                    dev, STp->density, (STp->buffer)->b_data[5] * 65536 +
                                    (STp->buffer)->b_data[6] * 256 + (STp->buffer)->b_data[7],
                                    STp->drv_buffer));
		}
		STp->drv_write_prot = ((STp->buffer)->b_data[2] & 0x80) != 0;
	}
	scsi_release_request(SRpnt);
	SRpnt = NULL;
        STp->inited = TRUE;

	if (STp->block_size > 0)
		(STp->buffer)->buffer_blocks =
                        (STp->buffer)->buffer_size / STp->block_size;
	else
		(STp->buffer)->buffer_blocks = 1;
	(STp->buffer)->buffer_bytes = (STp->buffer)->read_pointer = 0;

        DEBC(printk(ST_DEB_MSG
                       "st%d: Block size: %d, buffer size: %d (%d blocks).\n", dev,
		       STp->block_size, (STp->buffer)->buffer_size,
		       (STp->buffer)->buffer_blocks));

	if (STp->drv_write_prot) {
		STp->write_prot = 1;

                DEBC(printk(ST_DEB_MSG "st%d: Write protected\n", dev));

		if ((st_flags & O_ACCMODE) == O_WRONLY ||
		    (st_flags & O_ACCMODE) == O_RDWR) {
			retval = (-EROFS);
			goto err_out;
		}
	}

	if (STp->can_partitions && STp->nbr_partitions < 1) {
		/* This code is reached when the device is opened for the first time
		   after the driver has been initialized with tape in the drive and the
		   partition support has been enabled. */
                DEBC(printk(ST_DEB_MSG
                            "st%d: Updating partition number in status.\n", dev));
		if ((STp->partition = find_partition(STp)) < 0) {
			retval = STp->partition;
			goto err_out;
		}
		STp->new_partition = STp->partition;
		STp->nbr_partitions = 1; /* This guess will be updated when necessary */
	}

	if (new_session) {	/* Change the drive parameters for the new mode */
		STp->density_changed = STp->blksize_changed = FALSE;
		STp->compression_changed = FALSE;
		if (!(STm->defaults_for_writes) &&
		    (retval = set_mode_densblk(STp, STm)) < 0)
		    goto err_out;

		if (STp->default_drvbuffer != 0xff) {
			if (st_int_ioctl(STp, MTSETDRVBUFFER, STp->default_drvbuffer))
				printk(KERN_WARNING
                                       "st%d: Can't set default drive buffering to %d.\n",
				       dev, STp->default_drvbuffer);
		}
	}

	return CHKRES_READY;

 err_out:
	return retval;
}


/* Open the device. Needs to be called with BKL only because of incrementing the SCSI host
   module count. */
static int st_open(struct inode *inode, struct file *filp)
{
	int i, need_dma_buffer;
	int retval = (-EIO);
	Scsi_Tape *STp;
	ST_partstat *STps;
	int dev = TAPE_NR(inode->i_rdev);
	unsigned long flags;

	write_lock_irqsave(&st_dev_arr_lock, flags);
	STp = scsi_tapes[dev];
	if (dev >= st_template.dev_max || STp == NULL) {
		write_unlock_irqrestore(&st_dev_arr_lock, flags);
		return (-ENXIO);
	}

	if (STp->in_use) {
		write_unlock_irqrestore(&st_dev_arr_lock, flags);
		DEB( printk(ST_DEB_MSG "st%d: Device already in use.\n", dev); )
		return (-EBUSY);
	}
	STp->in_use = 1;
	write_unlock_irqrestore(&st_dev_arr_lock, flags);
	STp->rew_at_close = STp->autorew_dev = (MINOR(inode->i_rdev) & 0x80) == 0;

	if (STp->device->host->hostt->module)
		__MOD_INC_USE_COUNT(STp->device->host->hostt->module);
	STp->device->access_count++;

	if (!scsi_block_when_processing_errors(STp->device)) {
		retval = (-ENXIO);
		goto err_out;
	}

	/* Allocate a buffer for this user */
	need_dma_buffer = STp->restr_dma;
	write_lock_irqsave(&st_dev_arr_lock, flags);
	for (i = 0; i < st_nbr_buffers; i++)
		if (!st_buffers[i]->in_use &&
		    (!need_dma_buffer || st_buffers[i]->dma)) {
			STp->buffer = st_buffers[i];
			(STp->buffer)->in_use = 1;
			break;
		}
	write_unlock_irqrestore(&st_dev_arr_lock, flags);
	if (i >= st_nbr_buffers) {
		STp->buffer = new_tape_buffer(FALSE, need_dma_buffer, TRUE);
		if (STp->buffer == NULL) {
			printk(KERN_WARNING "st%d: Can't allocate tape buffer.\n", dev);
			retval = (-EBUSY);
			goto err_out;
		}
	}

	(STp->buffer)->writing = 0;
	(STp->buffer)->syscall_result = 0;
	(STp->buffer)->use_sg = STp->device->host->sg_tablesize;

	/* Compute the usable buffer size for this SCSI adapter */
	if (!(STp->buffer)->use_sg)
		(STp->buffer)->buffer_size = (STp->buffer)->sg_lengths[0];
	else {
		for (i = 0, (STp->buffer)->buffer_size = 0; i < (STp->buffer)->use_sg &&
		     i < (STp->buffer)->sg_segs; i++)
			(STp->buffer)->buffer_size += (STp->buffer)->sg_lengths[i];
	}

	STp->write_prot = ((filp->f_flags & O_ACCMODE) == O_RDONLY);

	STp->dirty = 0;
	for (i = 0; i < ST_NBR_PARTITIONS; i++) {
		STps = &(STp->ps[i]);
		STps->rw = ST_IDLE;
	}
	STp->recover_count = 0;
	DEB( STp->nbr_waits = STp->nbr_finished = 0; )

	retval = check_tape(STp, filp);
	if (retval < 0)
		goto err_out;
	if (blocking_open &&
	    (filp->f_flags & O_NONBLOCK) == 0 &&
	    retval != CHKRES_READY) {
		retval = (-EIO);
		goto err_out;
	}
	return 0;

 err_out:
	if (STp->buffer != NULL) {
		(STp->buffer)->in_use = 0;
		STp->buffer = NULL;
	}
	STp->in_use = 0;
	STp->device->access_count--;
	if (STp->device->host->hostt->module)
	    __MOD_DEC_USE_COUNT(STp->device->host->hostt->module);
	return retval;

}


/* Flush the tape buffer before close */
static int st_flush(struct file *filp)
{
	int result = 0, result2;
	unsigned char cmd[MAX_COMMAND_SIZE];
	Scsi_Request *SRpnt;
	Scsi_Tape *STp;
	ST_mode *STm;
	ST_partstat *STps;

	struct inode *inode = filp->f_dentry->d_inode;
	kdev_t devt = inode->i_rdev;
	int dev;

	if (file_count(filp) > 1)
		return 0;

	dev = TAPE_NR(devt);
	read_lock(&st_dev_arr_lock);
	STp = scsi_tapes[dev];
	read_unlock(&st_dev_arr_lock);
	STm = &(STp->modes[STp->current_mode]);
	STps = &(STp->ps[STp->partition]);

	if (STps->rw == ST_WRITING && !(STp->device)->was_reset) {
		result = flush_write_buffer(STp);
		if (result != 0 && result != (-ENOSPC))
			goto out;
	}

	if (STp->can_partitions &&
	    (result2 = update_partition(STp)) < 0) {
                DEBC(printk(ST_DEB_MSG
                               "st%d: update_partition at close failed.\n", dev));
		if (result == 0)
			result = result2;
		goto out;
	}

	if (STps->rw == ST_WRITING && !(STp->device)->was_reset) {

                DEBC(printk(ST_DEB_MSG "st%d: File length %ld bytes.\n",
                            dev, (long) (filp->f_pos));
                     printk(ST_DEB_MSG "st%d: Async write waits %d, finished %d.\n",
                            dev, STp->nbr_waits, STp->nbr_finished);
		)

		memset(cmd, 0, MAX_COMMAND_SIZE);
		cmd[0] = WRITE_FILEMARKS;
		cmd[4] = 1 + STp->two_fm;

		SRpnt = st_do_scsi(NULL, STp, cmd, 0, SCSI_DATA_NONE,
				   STp->timeout, MAX_WRITE_RETRIES, TRUE);
		if (!SRpnt) {
			result = (STp->buffer)->syscall_result;
			goto out;
		}

		if ((STp->buffer)->syscall_result != 0 &&
		    ((SRpnt->sr_sense_buffer[0] & 0x70) != 0x70 ||
		     (SRpnt->sr_sense_buffer[2] & 0x4f) != 0x40 ||
		     ((SRpnt->sr_sense_buffer[0] & 0x80) != 0 &&
		      (SRpnt->sr_sense_buffer[3] | SRpnt->sr_sense_buffer[4] |
		       SRpnt->sr_sense_buffer[5] |
		       SRpnt->sr_sense_buffer[6]) != 0))) {
			/* Filter out successful write at EOM */
			scsi_release_request(SRpnt);
			SRpnt = NULL;
			printk(KERN_ERR "st%d: Error on write filemark.\n", dev);
			if (result == 0)
				result = (-EIO);
		} else {
			scsi_release_request(SRpnt);
			SRpnt = NULL;
			if (STps->drv_file >= 0)
				STps->drv_file++;
			STps->drv_block = 0;
			if (STp->two_fm)
				cross_eof(STp, FALSE);
			STps->eof = ST_FM;
		}

                DEBC(printk(ST_DEB_MSG "st%d: Buffer flushed, %d EOF(s) written\n",
                            dev, cmd[4]));
	} else if (!STp->rew_at_close) {
		STps = &(STp->ps[STp->partition]);
		if (!STm->sysv || STps->rw != ST_READING) {
			if (STp->can_bsr)
				result = flush_buffer(STp, 0);
			else if (STps->eof == ST_FM_HIT) {
				result = cross_eof(STp, FALSE);
				if (result) {
					if (STps->drv_file >= 0)
						STps->drv_file++;
					STps->drv_block = 0;
					STps->eof = ST_FM;
				} else
					STps->eof = ST_NOEOF;
			}
		} else if ((STps->eof == ST_NOEOF &&
			    !(result = cross_eof(STp, TRUE))) ||
			   STps->eof == ST_FM_HIT) {
			if (STps->drv_file >= 0)
				STps->drv_file++;
			STps->drv_block = 0;
			STps->eof = ST_FM;
		}
	}

      out:
	if (STp->rew_at_close) {
		result2 = st_int_ioctl(STp, MTREW, 1);
		if (result == 0)
			result = result2;
	}
	return result;
}


/* Close the device and release it. BKL is not needed: this is the only thread
   accessing this tape. */
static int st_release(struct inode *inode, struct file *filp)
{
	int result = 0;
	Scsi_Tape *STp;
	unsigned long flags;

	kdev_t devt = inode->i_rdev;
	int dev;

	dev = TAPE_NR(devt);
	read_lock(&st_dev_arr_lock);
	STp = scsi_tapes[dev];
	read_unlock(&st_dev_arr_lock);

	if (STp->door_locked == ST_LOCKED_AUTO)
		st_int_ioctl(STp, MTUNLOCK, 0);

	if (STp->buffer != NULL) {
		normalize_buffer(STp->buffer);
		write_lock_irqsave(&st_dev_arr_lock, flags);
		(STp->buffer)->in_use = 0;
		STp->buffer = NULL;
	}
	else {
		write_lock_irqsave(&st_dev_arr_lock, flags);
	}

	STp->in_use = 0;
	write_unlock_irqrestore(&st_dev_arr_lock, flags);
	STp->device->access_count--;
	if (STp->device->host->hostt->module)
		__MOD_DEC_USE_COUNT(STp->device->host->hostt->module);

	return result;
}


/* Write command */
static ssize_t
 st_write(struct file *filp, const char *buf, size_t count, loff_t * ppos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	ssize_t total;
	ssize_t i, do_count, blks, transfer;
	ssize_t retval = 0;
	int residual, retry_eot = 0, scode;
	int write_threshold;
	int doing_write = 0;
	unsigned char cmd[MAX_COMMAND_SIZE];
	const char *b_point;
	Scsi_Request *SRpnt = NULL;
	Scsi_Tape *STp;
	ST_mode *STm;
	ST_partstat *STps;
	int dev = TAPE_NR(inode->i_rdev);

	read_lock(&st_dev_arr_lock);
	STp = scsi_tapes[dev];
	read_unlock(&st_dev_arr_lock);

	if (down_interruptible(&STp->lock))
		return -ERESTARTSYS;

	/*
	 * If we are in the middle of error recovery, don't let anyone
	 * else try and use this device.  Also, if error recovery fails, it
	 * may try and take the device offline, in which case all further
	 * access to the device is prohibited.
	 */
	if (!scsi_block_when_processing_errors(STp->device)) {
		retval = (-ENXIO);
		goto out;
	}

	if (ppos != &filp->f_pos) {
		/* "A request was outside the capabilities of the device." */
		retval = (-ENXIO);
		goto out;
	}

	if (STp->ready != ST_READY) {
		if (STp->ready == ST_NO_TAPE)
			retval = (-ENOMEDIUM);
		else
			retval = (-EIO);
		goto out;
	}

	STm = &(STp->modes[STp->current_mode]);
	if (!STm->defined) {
		retval = (-ENXIO);
		goto out;
	}
	if (count == 0)
		goto out;

	/*
	 * If there was a bus reset, block further access
	 * to this device.
	 */
	if (STp->device->was_reset) {
		retval = (-EIO);
		goto out;
	}

        DEB(
	if (!STp->in_use) {
		printk(ST_DEB_MSG "st%d: Incorrect device.\n", dev);
		retval = (-EIO);
		goto out;
	} ) /* end DEB */

	/* Write must be integral number of blocks */
	if (STp->block_size != 0 && (count % STp->block_size) != 0) {
		printk(KERN_WARNING "st%d: Write not multiple of tape block size.\n",
		       dev);
		retval = (-EINVAL);
		goto out;
	}

	if (STp->can_partitions &&
	    (retval = update_partition(STp)) < 0)
		goto out;
	STps = &(STp->ps[STp->partition]);

	if (STp->write_prot) {
		retval = (-EACCES);
		goto out;
	}

	if ((STp->buffer)->writing) {
		write_behind_check(STp);
		if ((STp->buffer)->syscall_result) {
                        DEBC(printk(ST_DEB_MSG "st%d: Async write error (write) %x.\n",
                                    dev, (STp->buffer)->midlevel_result));
			if ((STp->buffer)->midlevel_result == INT_MAX)
				STps->eof = ST_EOM_OK;
			else
				STps->eof = ST_EOM_ERROR;
		}
	}

	if (STp->block_size == 0) {
		if (STp->max_block > 0 &&
		    (count < STp->min_block || count > STp->max_block)) {
			retval = (-EINVAL);
			goto out;
		}
		if (count > (STp->buffer)->buffer_size &&
		    !enlarge_buffer(STp->buffer, count, STp->restr_dma)) {
			retval = (-EOVERFLOW);
			goto out;
		}
	}
	if ((STp->buffer)->buffer_blocks < 1) {
		/* Fixed block mode with too small buffer */
		if (!enlarge_buffer(STp->buffer, STp->block_size, STp->restr_dma)) {
			retval = (-EOVERFLOW);
			goto out;
		}
		(STp->buffer)->buffer_blocks = 1;
	}

	if (STp->do_auto_lock && STp->door_locked == ST_UNLOCKED &&
	    !st_int_ioctl(STp, MTLOCK, 0))
		STp->door_locked = ST_LOCKED_AUTO;

	if (STps->rw == ST_READING) {
		retval = flush_buffer(STp, 0);
		if (retval)
			goto out;
		STps->rw = ST_WRITING;
	} else if (STps->rw != ST_WRITING &&
		   STps->drv_file == 0 && STps->drv_block == 0) {
		if ((retval = set_mode_densblk(STp, STm)) < 0)
			goto out;
		if (STm->default_compression != ST_DONT_TOUCH &&
		    !(STp->compression_changed)) {
			if (st_compression(STp, (STm->default_compression == ST_YES))) {
				printk(KERN_WARNING "st%d: Can't set default compression.\n",
				       dev);
				if (modes_defined) {
					retval = (-EINVAL);
					goto out;
				}
			}
		}
	}

	if (STps->eof == ST_EOM_OK) {
		retval = (-ENOSPC);
		goto out;
	}
	else if (STps->eof == ST_EOM_ERROR) {
		retval = (-EIO);
		goto out;
	}

	/* Check the buffer readability in cases where copy_user might catch
	   the problems after some tape movement. */
	if (STp->block_size != 0 &&
	    (copy_from_user(&i, buf, 1) != 0 ||
	     copy_from_user(&i, buf + count - 1, 1) != 0)) {
		retval = (-EFAULT);
		goto out;
	}

	if (!STm->do_buffer_writes) {
#if 0
		if (STp->block_size != 0 && (count % STp->block_size) != 0) {
			retval = (-EINVAL);	/* Write must be integral number of blocks */
			goto out;
		}
#endif
		write_threshold = 1;
	} else
		write_threshold = (STp->buffer)->buffer_blocks * STp->block_size;
	if (!STm->do_async_writes || STp->block_size > 0)
		write_threshold--;

	total = count;

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = WRITE_6;
	cmd[1] = (STp->block_size != 0);

	STps->rw = ST_WRITING;

	b_point = buf;
	while ((STp->block_size == 0 && !STm->do_async_writes && count > 0) ||
	       (STp->block_size != 0 &&
		(STp->buffer)->buffer_bytes + count > write_threshold && !retry_eot)) {
		doing_write = 1;
		if (STp->block_size == 0)
			do_count = count;
		else {
			do_count = (STp->buffer)->buffer_blocks * STp->block_size -
			    (STp->buffer)->buffer_bytes;
			if (do_count > count)
				do_count = count;
		}

		i = append_to_buffer(b_point, STp->buffer, do_count);
		if (i) {
			retval = i;
			goto out;
		}

	retry_write:
		if (STp->block_size == 0)
			blks = transfer = do_count;
		else {
			blks = (STp->buffer)->buffer_bytes /
			    STp->block_size;
			transfer = blks * STp->block_size;
		}
		cmd[2] = blks >> 16;
		cmd[3] = blks >> 8;
		cmd[4] = blks;

		SRpnt = st_do_scsi(SRpnt, STp, cmd, transfer, SCSI_DATA_WRITE,
				   STp->timeout, MAX_WRITE_RETRIES, TRUE);
		if (!SRpnt) {
			retval = (STp->buffer)->syscall_result;
			goto out;
		}

		if ((STp->buffer)->syscall_result != 0) {
                        DEBC(printk(ST_DEB_MSG "st%d: Error on write:\n", dev));
			if ((SRpnt->sr_sense_buffer[0] & 0x70) == 0x70 &&
			    (SRpnt->sr_sense_buffer[2] & 0x40)) {
				scode = SRpnt->sr_sense_buffer[2] & 0x0f;
				if ((SRpnt->sr_sense_buffer[0] & 0x80) != 0)
					residual = (SRpnt->sr_sense_buffer[3] << 24) |
					    (SRpnt->sr_sense_buffer[4] << 16) |
					    (SRpnt->sr_sense_buffer[5] << 8) |
                                                SRpnt->sr_sense_buffer[6];
				else if (STp->block_size == 0 &&
					 scode == VOLUME_OVERFLOW)
					residual = do_count;
				else
					residual = 0;
				if (STp->block_size != 0)
					residual *= STp->block_size;
				if (residual <= do_count) {
					/* Within the data in this write() */
					filp->f_pos += do_count - residual;
					count -= do_count - residual;
					if (STps->drv_block >= 0) {
						if (STp->block_size == 0 &&
						    residual < do_count)
							STps->drv_block++;
						else if (STp->block_size != 0)
							STps->drv_block +=
								(transfer - residual) /
                                                                STp->block_size;
					}
					STps->eof = ST_EOM_OK;
					retval = (-ENOSPC); /* EOM within current request */
                                        DEBC(printk(ST_DEB_MSG
                                                       "st%d: EOM with %d bytes unwritten.\n",
						       dev, count));
				} else {
					/* EOT in within data buffered earlier */
					if (!retry_eot && (SRpnt->sr_sense_buffer[0] & 1) == 0 &&
					    (scode == NO_SENSE || scode == RECOVERED_ERROR)) {
						move_buffer_data(STp->buffer, transfer - residual);
						retry_eot = TRUE;
						if (STps->drv_block >= 0) {
							STps->drv_block += (transfer - residual) /
								STp->block_size;
						}
						STps->eof = ST_EOM_OK;
						DEBC(printk(ST_DEB_MSG
							    "st%d: Retry write of %d bytes at EOM.\n",
							    dev, do_count));
						goto retry_write;
					}
					else {
						/* Either error within data buffered by driver or failed retry */
						STps->eof = ST_EOM_ERROR;
						STps->drv_block = (-1); /* Too cautious? */
						retval = (-EIO);	/* EOM for old data */
						DEBC(printk(ST_DEB_MSG
							    "st%d: EOM with lost data.\n",
							    dev));
					}
				}
			} else {
				STps->drv_block = (-1);		/* Too cautious? */
				retry_eot = FALSE;
				retval = (-EIO);
			}

			scsi_release_request(SRpnt);
			SRpnt = NULL;
			(STp->buffer)->buffer_bytes = 0;
			STp->dirty = 0;
			if (count < total)
				retval = total - count;
			goto out;
		}
		filp->f_pos += do_count;
		b_point += do_count;
		count -= do_count;
		if (STps->drv_block >= 0) {
			if (STp->block_size == 0)
				STps->drv_block++;
			else
				STps->drv_block += blks;
		}
		(STp->buffer)->buffer_bytes = 0;
		STp->dirty = 0;
	}
	if (count != 0 && !retry_eot) {
		STp->dirty = 1;
		i = append_to_buffer(b_point, STp->buffer, count);
		if (i) {
			retval = i;
			goto out;
		}
		filp->f_pos += count;
		count = 0;
	}

	if (doing_write && (STp->buffer)->syscall_result != 0) {
		retval = (STp->buffer)->syscall_result;
		goto out;
	}

	if (STm->do_async_writes && STp->block_size == 0) {
		/* Schedule an asynchronous write */
		(STp->buffer)->writing = (STp->buffer)->buffer_bytes;
		STp->dirty = FALSE;
		residual = (STp->buffer)->writing;
		cmd[2] = residual >> 16;
		cmd[3] = residual >> 8;
		cmd[4] = residual;
		DEB( STp->write_pending = 1; )

		SRpnt = st_do_scsi(SRpnt, STp, cmd, (STp->buffer)->writing,
				   SCSI_DATA_WRITE, STp->timeout,
				   MAX_WRITE_RETRIES, FALSE);
		if (SRpnt == NULL) {
			retval = (STp->buffer)->syscall_result;
			goto out;
		}
		SRpnt = NULL;  /* Prevent releasing this request! */

	}
	STps->at_sm &= (total == 0);
	if (total > 0 && !retry_eot)
		STps->eof = ST_NOEOF;
	retval = total - count;

 out:
	if (SRpnt != NULL)
		scsi_release_request(SRpnt);
	up(&STp->lock);

	return retval;
}

/* Read data from the tape. Returns zero in the normal case, one if the
   eof status has changed, and the negative error code in case of a
   fatal error. Otherwise updates the buffer and the eof state. */
static long read_tape(Scsi_Tape *STp, long count, Scsi_Request ** aSRpnt)
{
	int transfer, blks, bytes;
	unsigned char cmd[MAX_COMMAND_SIZE];
	Scsi_Request *SRpnt;
	ST_mode *STm;
	ST_partstat *STps;
	int dev = TAPE_NR(STp->devt);
	int retval = 0;

	if (count == 0)
		return 0;

	STm = &(STp->modes[STp->current_mode]);
	STps = &(STp->ps[STp->partition]);
	if (STps->eof == ST_FM_HIT)
		return 1;

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = READ_6;
	cmd[1] = (STp->block_size != 0);
	if (STp->block_size == 0)
		blks = bytes = count;
	else {
		if (STm->do_read_ahead) {
			blks = (STp->buffer)->buffer_blocks;
			bytes = blks * STp->block_size;
		} else {
			bytes = count;
			if (bytes > (STp->buffer)->buffer_size)
				bytes = (STp->buffer)->buffer_size;
			blks = bytes / STp->block_size;
			bytes = blks * STp->block_size;
		}
	}
	cmd[2] = blks >> 16;
	cmd[3] = blks >> 8;
	cmd[4] = blks;

	SRpnt = *aSRpnt;
	SRpnt = st_do_scsi(SRpnt, STp, cmd, bytes, SCSI_DATA_READ,
			   STp->timeout, MAX_RETRIES, TRUE);
	*aSRpnt = SRpnt;
	if (!SRpnt)
		return (STp->buffer)->syscall_result;

	(STp->buffer)->read_pointer = 0;
	STps->at_sm = 0;

	/* Something to check */
	if ((STp->buffer)->syscall_result) {
		retval = 1;
		DEBC(printk(ST_DEB_MSG "st%d: Sense: %2x %2x %2x %2x %2x %2x %2x %2x\n",
                            dev,
                            SRpnt->sr_sense_buffer[0], SRpnt->sr_sense_buffer[1],
                            SRpnt->sr_sense_buffer[2], SRpnt->sr_sense_buffer[3],
                            SRpnt->sr_sense_buffer[4], SRpnt->sr_sense_buffer[5],
                            SRpnt->sr_sense_buffer[6], SRpnt->sr_sense_buffer[7]));
		if ((SRpnt->sr_sense_buffer[0] & 0x70) == 0x70) {	/* extended sense */

			if ((SRpnt->sr_sense_buffer[2] & 0x0f) == BLANK_CHECK)
				SRpnt->sr_sense_buffer[2] &= 0xcf;	/* No need for EOM in this case */

			if ((SRpnt->sr_sense_buffer[2] & 0xe0) != 0) { /* EOF, EOM, or ILI */
				/* Compute the residual count */
				if ((SRpnt->sr_sense_buffer[0] & 0x80) != 0)
					transfer = (SRpnt->sr_sense_buffer[3] << 24) |
					    (SRpnt->sr_sense_buffer[4] << 16) |
					    (SRpnt->sr_sense_buffer[5] << 8) |
					    SRpnt->sr_sense_buffer[6];
				else
					transfer = 0;
				if (STp->block_size == 0 &&
				    (SRpnt->sr_sense_buffer[2] & 0x0f) == MEDIUM_ERROR)
					transfer = bytes;

				if (SRpnt->sr_sense_buffer[2] & 0x20) {	/* ILI */
					if (STp->block_size == 0) {
						if (transfer <= 0) {
							if (transfer < 0)
								printk(KERN_NOTICE
								       "st%d: Failed to read %d byte block with %d byte read.\n",
								       dev, bytes - transfer, bytes);
							if (STps->drv_block >= 0)
								STps->drv_block += 1;
							(STp->buffer)->buffer_bytes = 0;
							return (-ENOMEM);
						}
						(STp->buffer)->buffer_bytes = bytes - transfer;
					} else {
						scsi_release_request(SRpnt);
						SRpnt = *aSRpnt = NULL;
						if (transfer == blks) {	/* We did not get anything, error */
							printk(KERN_NOTICE "st%d: Incorrect block size.\n", dev);
							if (STps->drv_block >= 0)
								STps->drv_block += blks - transfer + 1;
							st_int_ioctl(STp, MTBSR, 1);
							return (-EIO);
						}
						/* We have some data, deliver it */
						(STp->buffer)->buffer_bytes = (blks - transfer) *
						    STp->block_size;
                                                DEBC(printk(ST_DEB_MSG
                                                            "st%d: ILI but enough data received %ld %d.\n",
                                                            dev, count, (STp->buffer)->buffer_bytes));
						if (STps->drv_block >= 0)
							STps->drv_block += 1;
						if (st_int_ioctl(STp, MTBSR, 1))
							return (-EIO);
					}
				} else if (SRpnt->sr_sense_buffer[2] & 0x80) {	/* FM overrides EOM */
					if (STps->eof != ST_FM_HIT)
						STps->eof = ST_FM_HIT;
					else
						STps->eof = ST_EOD_2;
					if (STp->block_size == 0)
						(STp->buffer)->buffer_bytes = 0;
					else
						(STp->buffer)->buffer_bytes =
						    bytes - transfer * STp->block_size;
                                        DEBC(printk(ST_DEB_MSG
                                                    "st%d: EOF detected (%d bytes read).\n",
                                                    dev, (STp->buffer)->buffer_bytes));
				} else if (SRpnt->sr_sense_buffer[2] & 0x40) {
					if (STps->eof == ST_FM)
						STps->eof = ST_EOD_1;
					else
						STps->eof = ST_EOM_OK;
					if (STp->block_size == 0)
						(STp->buffer)->buffer_bytes = bytes - transfer;
					else
						(STp->buffer)->buffer_bytes =
						    bytes - transfer * STp->block_size;

                                        DEBC(printk(ST_DEB_MSG "st%d: EOM detected (%d bytes read).\n",
                                                    dev, (STp->buffer)->buffer_bytes));
				}
			}
			/* end of EOF, EOM, ILI test */ 
			else {	/* nonzero sense key */
                                DEBC(printk(ST_DEB_MSG
                                            "st%d: Tape error while reading.\n", dev));
				STps->drv_block = (-1);
				if (STps->eof == ST_FM &&
				    (SRpnt->sr_sense_buffer[2] & 0x0f) == BLANK_CHECK) {
                                        DEBC(printk(ST_DEB_MSG
                                                    "st%d: Zero returned for first BLANK CHECK after EOF.\n",
                                                    dev));
					STps->eof = ST_EOD_2;	/* First BLANK_CHECK after FM */
				} else	/* Some other extended sense code */
					retval = (-EIO);
			}

			if ((STp->buffer)->buffer_bytes < 0) /* Caused by bogus sense data */
				(STp->buffer)->buffer_bytes = 0;
		}
		/* End of extended sense test */ 
		else {		/* Non-extended sense */
			retval = (STp->buffer)->syscall_result;
		}

	}
	/* End of error handling */ 
	else			/* Read successful */
		(STp->buffer)->buffer_bytes = bytes;

	if (STps->drv_block >= 0) {
		if (STp->block_size == 0)
			STps->drv_block++;
		else
			STps->drv_block += (STp->buffer)->buffer_bytes / STp->block_size;
	}
	return retval;
}


/* Read command */
static ssize_t
 st_read(struct file *filp, char *buf, size_t count, loff_t * ppos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	ssize_t total;
	ssize_t retval = 0;
	ssize_t i, transfer;
	int special;
	Scsi_Request *SRpnt = NULL;
	Scsi_Tape *STp;
	ST_mode *STm;
	ST_partstat *STps;
	int dev = TAPE_NR(inode->i_rdev);

	read_lock(&st_dev_arr_lock);
	STp = scsi_tapes[dev];
	read_unlock(&st_dev_arr_lock);

	if (down_interruptible(&STp->lock))
		return -ERESTARTSYS;

	/*
	 * If we are in the middle of error recovery, don't let anyone
	 * else try and use this device.  Also, if error recovery fails, it
	 * may try and take the device offline, in which case all further
	 * access to the device is prohibited.
	 */
	if (!scsi_block_when_processing_errors(STp->device)) {
		retval = (-ENXIO);
		goto out;
	}

	if (ppos != &filp->f_pos) {
		/* "A request was outside the capabilities of the device." */
		retval = (-ENXIO);
		goto out;
	}

	if (STp->ready != ST_READY) {
		if (STp->ready == ST_NO_TAPE)
			retval = (-ENOMEDIUM);
		else
			retval = (-EIO);
		goto out;
	}
	STm = &(STp->modes[STp->current_mode]);
	if (!STm->defined) {
		retval = (-ENXIO);
		goto out;
	}
        DEB(
	if (!STp->in_use) {
		printk(ST_DEB_MSG "st%d: Incorrect device.\n", dev);
		retval = (-EIO);
		goto out;
	} ) /* end DEB */

	if (STp->can_partitions &&
	    (retval = update_partition(STp)) < 0)
		goto out;

	if (STp->block_size == 0) {
		if (STp->max_block > 0 &&
		    (count < STp->min_block || count > STp->max_block)) {
			retval = (-EINVAL);
			goto out;
		}
		if (count > (STp->buffer)->buffer_size &&
		    !enlarge_buffer(STp->buffer, count, STp->restr_dma)) {
			retval = (-EOVERFLOW);
			goto out;
		}
	}
	if ((STp->buffer)->buffer_blocks < 1) {
		/* Fixed block mode with too small buffer */
		if (!enlarge_buffer(STp->buffer, STp->block_size, STp->restr_dma)) {
			retval = (-EOVERFLOW);
			goto out;
		}
		(STp->buffer)->buffer_blocks = 1;
	}

	if (!(STm->do_read_ahead) && STp->block_size != 0 &&
	    (count % STp->block_size) != 0) {
		retval = (-EINVAL);	/* Read must be integral number of blocks */
		goto out;
	}

	if (STp->do_auto_lock && STp->door_locked == ST_UNLOCKED &&
	    !st_int_ioctl(STp, MTLOCK, 0))
		STp->door_locked = ST_LOCKED_AUTO;

	STps = &(STp->ps[STp->partition]);
	if (STps->rw == ST_WRITING) {
		retval = flush_buffer(STp, 0);
		if (retval)
			goto out;
		STps->rw = ST_READING;
	}
        DEB(
	if (debugging && STps->eof != ST_NOEOF)
		printk(ST_DEB_MSG "st%d: EOF/EOM flag up (%d). Bytes %d\n", dev,
		       STps->eof, (STp->buffer)->buffer_bytes);
        ) /* end DEB */

	if ((STp->buffer)->buffer_bytes == 0 &&
	    STps->eof >= ST_EOD_1) {
		if (STps->eof < ST_EOD) {
			STps->eof += 1;
			retval = 0;
			goto out;
		}
		retval = (-EIO);	/* EOM or Blank Check */
		goto out;
	}

	/* Check the buffer writability before any tape movement. Don't alter
	   buffer data. */
	if (copy_from_user(&i, buf, 1) != 0 ||
	    copy_to_user(buf, &i, 1) != 0 ||
	    copy_from_user(&i, buf + count - 1, 1) != 0 ||
	    copy_to_user(buf + count - 1, &i, 1) != 0) {
		retval = (-EFAULT);
		goto out;
	}

	STps->rw = ST_READING;


	/* Loop until enough data in buffer or a special condition found */
	for (total = 0, special = 0; total < count && !special;) {

		/* Get new data if the buffer is empty */
		if ((STp->buffer)->buffer_bytes == 0) {
			special = read_tape(STp, count - total, &SRpnt);
			if (special < 0) {	/* No need to continue read */
				retval = special;
				goto out;
			}
		}

		/* Move the data from driver buffer to user buffer */
		if ((STp->buffer)->buffer_bytes > 0) {
                        DEB(
			if (debugging && STps->eof != ST_NOEOF)
				printk(ST_DEB_MSG
                                       "st%d: EOF up (%d). Left %d, needed %d.\n", dev,
				       STps->eof, (STp->buffer)->buffer_bytes,
                                       count - total);
                        ) /* end DEB */
			transfer = (STp->buffer)->buffer_bytes < count - total ?
			    (STp->buffer)->buffer_bytes : count - total;
			i = from_buffer(STp->buffer, buf, transfer);
			if (i) {
				retval = i;
				goto out;
			}
			filp->f_pos += transfer;
			buf += transfer;
			total += transfer;
		}

		if (STp->block_size == 0)
			break;	/* Read only one variable length block */

	}			/* for (total = 0, special = 0;
                                   total < count && !special; ) */

	/* Change the eof state if no data from tape or buffer */
	if (total == 0) {
		if (STps->eof == ST_FM_HIT) {
			STps->eof = ST_FM;
			STps->drv_block = 0;
			if (STps->drv_file >= 0)
				STps->drv_file++;
		} else if (STps->eof == ST_EOD_1) {
			STps->eof = ST_EOD_2;
			STps->drv_block = 0;
			if (STps->drv_file >= 0)
				STps->drv_file++;
		} else if (STps->eof == ST_EOD_2)
			STps->eof = ST_EOD;
	} else if (STps->eof == ST_FM)
		STps->eof = ST_NOEOF;
	retval = total;

 out:
	if (SRpnt != NULL) {
		scsi_release_request(SRpnt);
		SRpnt = NULL;
	}
	up(&STp->lock);

	return retval;
}



/* Set the driver options */
static void st_log_options(Scsi_Tape * STp, ST_mode * STm, int dev)
{
	printk(KERN_INFO
	       "st%d: Mode %d options: buffer writes: %d, async writes: %d, read ahead: %d\n",
	       dev, STp->current_mode, STm->do_buffer_writes, STm->do_async_writes,
	       STm->do_read_ahead);
	printk(KERN_INFO
	       "st%d:    can bsr: %d, two FMs: %d, fast mteom: %d, auto lock: %d,\n",
	       dev, STp->can_bsr, STp->two_fm, STp->fast_mteom, STp->do_auto_lock);
	printk(KERN_INFO
	       "st%d:    defs for wr: %d, no block limits: %d, partitions: %d, s2 log: %d\n",
	       dev, STm->defaults_for_writes, STp->omit_blklims, STp->can_partitions,
	       STp->scsi2_logical);
	printk(KERN_INFO
	       "st%d:    sysv: %d nowait: %d\n", dev, STm->sysv, STp->immediate);
        DEB(printk(KERN_INFO
                   "st%d:    debugging: %d\n",
                   dev, debugging);)
}


static int st_set_options(Scsi_Tape *STp, long options)
{
	int value;
	long code;
	ST_mode *STm;
	int dev = TAPE_NR(STp->devt);

	STm = &(STp->modes[STp->current_mode]);
	if (!STm->defined) {
		memcpy(STm, &(STp->modes[0]), sizeof(ST_mode));
		modes_defined = TRUE;
                DEBC(printk(ST_DEB_MSG
                            "st%d: Initialized mode %d definition from mode 0\n",
                            dev, STp->current_mode));
	}

	code = options & MT_ST_OPTIONS;
	if (code == MT_ST_BOOLEANS) {
		STm->do_buffer_writes = (options & MT_ST_BUFFER_WRITES) != 0;
		STm->do_async_writes = (options & MT_ST_ASYNC_WRITES) != 0;
		STm->defaults_for_writes = (options & MT_ST_DEF_WRITES) != 0;
		STm->do_read_ahead = (options & MT_ST_READ_AHEAD) != 0;
		STp->two_fm = (options & MT_ST_TWO_FM) != 0;
		STp->fast_mteom = (options & MT_ST_FAST_MTEOM) != 0;
		STp->do_auto_lock = (options & MT_ST_AUTO_LOCK) != 0;
		STp->can_bsr = (options & MT_ST_CAN_BSR) != 0;
		STp->omit_blklims = (options & MT_ST_NO_BLKLIMS) != 0;
		if ((STp->device)->scsi_level >= SCSI_2)
			STp->can_partitions = (options & MT_ST_CAN_PARTITIONS) != 0;
		STp->scsi2_logical = (options & MT_ST_SCSI2LOGICAL) != 0;
		STp->immediate = (options & MT_ST_NOWAIT) != 0;
		STm->sysv = (options & MT_ST_SYSV) != 0;
		DEB( debugging = (options & MT_ST_DEBUGGING) != 0; )
		st_log_options(STp, STm, dev);
	} else if (code == MT_ST_SETBOOLEANS || code == MT_ST_CLEARBOOLEANS) {
		value = (code == MT_ST_SETBOOLEANS);
		if ((options & MT_ST_BUFFER_WRITES) != 0)
			STm->do_buffer_writes = value;
		if ((options & MT_ST_ASYNC_WRITES) != 0)
			STm->do_async_writes = value;
		if ((options & MT_ST_DEF_WRITES) != 0)
			STm->defaults_for_writes = value;
		if ((options & MT_ST_READ_AHEAD) != 0)
			STm->do_read_ahead = value;
		if ((options & MT_ST_TWO_FM) != 0)
			STp->two_fm = value;
		if ((options & MT_ST_FAST_MTEOM) != 0)
			STp->fast_mteom = value;
		if ((options & MT_ST_AUTO_LOCK) != 0)
			STp->do_auto_lock = value;
		if ((options & MT_ST_CAN_BSR) != 0)
			STp->can_bsr = value;
		if ((options & MT_ST_NO_BLKLIMS) != 0)
			STp->omit_blklims = value;
		if ((STp->device)->scsi_level >= SCSI_2 &&
		    (options & MT_ST_CAN_PARTITIONS) != 0)
			STp->can_partitions = value;
		if ((options & MT_ST_SCSI2LOGICAL) != 0)
			STp->scsi2_logical = value;
		if ((options & MT_ST_NOWAIT) != 0)
			STp->immediate = value;
		if ((options & MT_ST_SYSV) != 0)
			STm->sysv = value;
                DEB(
		if ((options & MT_ST_DEBUGGING) != 0)
			debugging = value; )
		st_log_options(STp, STm, dev);
	} else if (code == MT_ST_WRITE_THRESHOLD) {
		/* Retained for compatibility */
	} else if (code == MT_ST_DEF_BLKSIZE) {
		value = (options & ~MT_ST_OPTIONS);
		if (value == ~MT_ST_OPTIONS) {
			STm->default_blksize = (-1);
			printk(KERN_INFO "st%d: Default block size disabled.\n", dev);
		} else {
			STm->default_blksize = value;
			printk(KERN_INFO "st%d: Default block size set to %d bytes.\n",
			       dev, STm->default_blksize);
			if (STp->ready == ST_READY) {
				STp->blksize_changed = FALSE;
				set_mode_densblk(STp, STm);
			}
		}
	} else if (code == MT_ST_TIMEOUTS) {
		value = (options & ~MT_ST_OPTIONS);
		if ((value & MT_ST_SET_LONG_TIMEOUT) != 0) {
			STp->long_timeout = (value & ~MT_ST_SET_LONG_TIMEOUT) * HZ;
			printk(KERN_INFO "st%d: Long timeout set to %d seconds.\n", dev,
			       (value & ~MT_ST_SET_LONG_TIMEOUT));
		} else {
			STp->timeout = value * HZ;
			printk(KERN_INFO "st%d: Normal timeout set to %d seconds.\n",
                               dev, value);
		}
	} else if (code == MT_ST_SET_CLN) {
		value = (options & ~MT_ST_OPTIONS) & 0xff;
		if (value != 0 &&
		    value < EXTENDED_SENSE_START && value >= SCSI_SENSE_BUFFERSIZE)
			return (-EINVAL);
		STp->cln_mode = value;
		STp->cln_sense_mask = (options >> 8) & 0xff;
		STp->cln_sense_value = (options >> 16) & 0xff;
		printk(KERN_INFO
		       "st%d: Cleaning request mode %d, mask %02x, value %02x\n",
		       dev, value, STp->cln_sense_mask, STp->cln_sense_value);
	} else if (code == MT_ST_DEF_OPTIONS) {
		code = (options & ~MT_ST_CLEAR_DEFAULT);
		value = (options & MT_ST_CLEAR_DEFAULT);
		if (code == MT_ST_DEF_DENSITY) {
			if (value == MT_ST_CLEAR_DEFAULT) {
				STm->default_density = (-1);
				printk(KERN_INFO "st%d: Density default disabled.\n",
                                       dev);
			} else {
				STm->default_density = value & 0xff;
				printk(KERN_INFO "st%d: Density default set to %x\n",
				       dev, STm->default_density);
				if (STp->ready == ST_READY) {
					STp->density_changed = FALSE;
					set_mode_densblk(STp, STm);
				}
			}
		} else if (code == MT_ST_DEF_DRVBUFFER) {
			if (value == MT_ST_CLEAR_DEFAULT) {
				STp->default_drvbuffer = 0xff;
				printk(KERN_INFO
                                       "st%d: Drive buffer default disabled.\n", dev);
			} else {
				STp->default_drvbuffer = value & 7;
				printk(KERN_INFO
                                       "st%d: Drive buffer default set to %x\n",
				       dev, STp->default_drvbuffer);
				if (STp->ready == ST_READY)
					st_int_ioctl(STp, MTSETDRVBUFFER, STp->default_drvbuffer);
			}
		} else if (code == MT_ST_DEF_COMPRESSION) {
			if (value == MT_ST_CLEAR_DEFAULT) {
				STm->default_compression = ST_DONT_TOUCH;
				printk(KERN_INFO
                                       "st%d: Compression default disabled.\n", dev);
			} else {
				if ((value & 0xff00) != 0) {
					STp->c_algo = (value & 0xff00) >> 8;
					printk(KERN_INFO "st%d: Compression algorithm set to 0x%x.\n",
					       dev, STp->c_algo);
				}
				if ((value & 0xff) != 0xff) {
					STm->default_compression = (value & 1 ? ST_YES : ST_NO);
					printk(KERN_INFO "st%d: Compression default set to %x\n",
					       dev, (value & 1));
					if (STp->ready == ST_READY) {
						STp->compression_changed = FALSE;
						st_compression(STp, (STm->default_compression == ST_YES));
					}
				}
			}
		}
	} else
		return (-EIO);

	return 0;
}

#define MODE_HEADER_LENGTH  4

/* Mode header and page byte offsets */
#define MH_OFF_DATA_LENGTH     0
#define MH_OFF_MEDIUM_TYPE     1
#define MH_OFF_DEV_SPECIFIC    2
#define MH_OFF_BDESCS_LENGTH   3
#define MP_OFF_PAGE_NBR        0
#define MP_OFF_PAGE_LENGTH     1

/* Mode header and page bit masks */
#define MH_BIT_WP              0x80
#define MP_MSK_PAGE_NBR        0x3f

/* Don't return block descriptors */
#define MODE_SENSE_OMIT_BDESCS 0x08

#define MODE_SELECT_PAGE_FORMAT 0x10

/* Read a mode page into the tape buffer. The block descriptors are included
   if incl_block_descs is true. The page control is ored to the page number
   parameter, if necessary. */
static int read_mode_page(Scsi_Tape *STp, int page, int omit_block_descs)
{
	unsigned char cmd[MAX_COMMAND_SIZE];
	Scsi_Request *SRpnt = NULL;

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = MODE_SENSE;
	if (omit_block_descs)
		cmd[1] = MODE_SENSE_OMIT_BDESCS;
	cmd[2] = page;
	cmd[4] = 255;

	SRpnt = st_do_scsi(SRpnt, STp, cmd, cmd[4], SCSI_DATA_READ,
			   STp->timeout, 0, TRUE);
	if (SRpnt == NULL)
		return (STp->buffer)->syscall_result;

	scsi_release_request(SRpnt);

	return (STp->buffer)->syscall_result;
}


/* Send the mode page in the tape buffer to the drive. Assumes that the mode data
   in the buffer is correctly formatted. */
static int write_mode_page(Scsi_Tape *STp, int page, int slow)
{
	int pgo;
	unsigned char cmd[MAX_COMMAND_SIZE];
	Scsi_Request *SRpnt = NULL;

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = MODE_SELECT;
	cmd[1] = MODE_SELECT_PAGE_FORMAT;
	pgo = MODE_HEADER_LENGTH + (STp->buffer)->b_data[MH_OFF_BDESCS_LENGTH];
	cmd[4] = pgo + (STp->buffer)->b_data[pgo + MP_OFF_PAGE_LENGTH] + 2;

	/* Clear reserved fields */
	(STp->buffer)->b_data[MH_OFF_DATA_LENGTH] = 0;
	(STp->buffer)->b_data[MH_OFF_MEDIUM_TYPE] = 0;
	(STp->buffer)->b_data[MH_OFF_DEV_SPECIFIC] &= ~MH_BIT_WP;
	(STp->buffer)->b_data[pgo + MP_OFF_PAGE_NBR] &= MP_MSK_PAGE_NBR;

	SRpnt = st_do_scsi(SRpnt, STp, cmd, cmd[4], SCSI_DATA_WRITE,
			   (slow ? STp->long_timeout : STp->timeout), 0, TRUE);
	if (SRpnt == NULL)
		return (STp->buffer)->syscall_result;

	scsi_release_request(SRpnt);

	return (STp->buffer)->syscall_result;
}


#define COMPRESSION_PAGE        0x0f
#define COMPRESSION_PAGE_LENGTH 16

#define CP_OFF_DCE_DCC          2
#define CP_OFF_C_ALGO           7

#define DCE_MASK  0x80
#define DCC_MASK  0x40
#define RED_MASK  0x60


/* Control the compression with mode page 15. Algorithm not changed if zero.

   The block descriptors are read and written because Sony SDT-7000 does not
   work without this (suggestion from Michael Schaefer <Michael.Schaefer@dlr.de>).
   Including block descriptors should not cause any harm to other drives. */

static int st_compression(Scsi_Tape * STp, int state)
{
	int retval;
	int mpoffs;  /* Offset to mode page start */
	unsigned char *b_data = (STp->buffer)->b_data;
	DEB( int dev = TAPE_NR(STp->devt); )

	if (STp->ready != ST_READY)
		return (-EIO);

	/* Read the current page contents */
	retval = read_mode_page(STp, COMPRESSION_PAGE, FALSE);
	if (retval) {
                DEBC(printk(ST_DEB_MSG "st%d: Compression mode page not supported.\n",
                            dev));
		return (-EIO);
	}

	mpoffs = MODE_HEADER_LENGTH + b_data[MH_OFF_BDESCS_LENGTH];
        DEBC(printk(ST_DEB_MSG "st%d: Compression state is %d.\n", dev,
                    (b_data[mpoffs + CP_OFF_DCE_DCC] & DCE_MASK ? 1 : 0)));

	/* Check if compression can be changed */
	if ((b_data[mpoffs + CP_OFF_DCE_DCC] & DCC_MASK) == 0) {
                DEBC(printk(ST_DEB_MSG "st%d: Compression not supported.\n", dev));
		return (-EIO);
	}

	/* Do the change */
	if (state) {
		b_data[mpoffs + CP_OFF_DCE_DCC] |= DCE_MASK;
		if (STp->c_algo != 0)
			b_data[mpoffs + CP_OFF_C_ALGO] = STp->c_algo;
	}
	else {
		b_data[mpoffs + CP_OFF_DCE_DCC] &= ~DCE_MASK;
		if (STp->c_algo != 0)
			b_data[mpoffs + CP_OFF_C_ALGO] = 0; /* no compression */
	}

	retval = write_mode_page(STp, COMPRESSION_PAGE, FALSE);
	if (retval) {
                DEBC(printk(ST_DEB_MSG "st%d: Compression change failed.\n", dev));
		return (-EIO);
	}
        DEBC(printk(ST_DEB_MSG "st%d: Compression state changed to %d.\n",
		       dev, state));

	STp->compression_changed = TRUE;
	return 0;
}


/* Process the load and unload commands (does unload if the load code is zero) */
static int do_load_unload(Scsi_Tape *STp, struct file *filp, int load_code)
{
	int retval = (-EIO), timeout;
	DEB(int dev = TAPE_NR(STp->devt);)
	unsigned char cmd[MAX_COMMAND_SIZE];
	ST_partstat *STps;
	Scsi_Request *SRpnt;

	if (STp->ready != ST_READY && !load_code) {
		if (STp->ready == ST_NO_TAPE)
			return (-ENOMEDIUM);
		else
			return (-EIO);
	}

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = START_STOP;
	if (load_code)
		cmd[4] |= 1;
	/*
	 * If arg >= 1 && arg <= 6 Enhanced load/unload in HP C1553A
	 */
	if (load_code >= 1 + MT_ST_HPLOADER_OFFSET
	    && load_code <= 6 + MT_ST_HPLOADER_OFFSET) {
		DEBC(printk(ST_DEB_MSG "st%d: Enhanced %sload slot %2d.\n",
			    dev, (cmd[4]) ? "" : "un",
			    load_code - MT_ST_HPLOADER_OFFSET));
		cmd[3] = load_code - MT_ST_HPLOADER_OFFSET; /* MediaID field of C1553A */
	}
	if (STp->immediate) {
		cmd[1] = 1;	/* Don't wait for completion */
		timeout = STp->timeout;
	}
	else
		timeout = STp->long_timeout;

	DEBC(
		if (!load_code)
		printk(ST_DEB_MSG "st%d: Unloading tape.\n", dev);
		else
		printk(ST_DEB_MSG "st%d: Loading tape.\n", dev);
		);

	SRpnt = st_do_scsi(NULL, STp, cmd, 0, SCSI_DATA_NONE,
			   timeout, MAX_RETRIES, TRUE);
	if (!SRpnt)
		return (STp->buffer)->syscall_result;

	retval = (STp->buffer)->syscall_result;
	scsi_release_request(SRpnt);

	if (!retval) {	/* SCSI command successful */

		if (!load_code) {
			STp->rew_at_close = 0;
			STp->ready = ST_NO_TAPE;
		}
		else {
			STp->rew_at_close = STp->autorew_dev;
			retval = check_tape(STp, filp);
			if (retval > 0)
				retval = 0;
		}
	}
	else {
		STps = &(STp->ps[STp->partition]);
		STps->drv_file = STps->drv_block = (-1);
	}

	return retval;
}


/* Internal ioctl function */
static int st_int_ioctl(Scsi_Tape *STp, unsigned int cmd_in, unsigned long arg)
{
	int timeout;
	long ltmp;
	int ioctl_result;
	int chg_eof = TRUE;
	unsigned char cmd[MAX_COMMAND_SIZE];
	Scsi_Request *SRpnt;
	ST_partstat *STps;
	int fileno, blkno, at_sm, undone;
	int datalen = 0, direction = SCSI_DATA_NONE;
	int dev = TAPE_NR(STp->devt);

	if (STp->ready != ST_READY) {
		if (STp->ready == ST_NO_TAPE)
			return (-ENOMEDIUM);
		else
			return (-EIO);
	}
	timeout = STp->long_timeout;
	STps = &(STp->ps[STp->partition]);
	fileno = STps->drv_file;
	blkno = STps->drv_block;
	at_sm = STps->at_sm;

	memset(cmd, 0, MAX_COMMAND_SIZE);
	switch (cmd_in) {
	case MTFSFM:
		chg_eof = FALSE;	/* Changed from the FSF after this */
	case MTFSF:
		cmd[0] = SPACE;
		cmd[1] = 0x01;	/* Space FileMarks */
		cmd[2] = (arg >> 16);
		cmd[3] = (arg >> 8);
		cmd[4] = arg;
                DEBC(printk(ST_DEB_MSG "st%d: Spacing tape forward over %d filemarks.\n",
			    dev, cmd[2] * 65536 + cmd[3] * 256 + cmd[4]));
		if (fileno >= 0)
			fileno += arg;
		blkno = 0;
		at_sm &= (arg == 0);
		break;
	case MTBSFM:
		chg_eof = FALSE;	/* Changed from the FSF after this */
	case MTBSF:
		cmd[0] = SPACE;
		cmd[1] = 0x01;	/* Space FileMarks */
		ltmp = (-arg);
		cmd[2] = (ltmp >> 16);
		cmd[3] = (ltmp >> 8);
		cmd[4] = ltmp;
                DEBC(
                     if (cmd[2] & 0x80)
                     	ltmp = 0xff000000;
                     ltmp = ltmp | (cmd[2] << 16) | (cmd[3] << 8) | cmd[4];
                     printk(ST_DEB_MSG
                            "st%d: Spacing tape backward over %ld filemarks.\n",
                            dev, (-ltmp));
		)
		if (fileno >= 0)
			fileno -= arg;
		blkno = (-1);	/* We can't know the block number */
		at_sm &= (arg == 0);
		break;
	case MTFSR:
		cmd[0] = SPACE;
		cmd[1] = 0x00;	/* Space Blocks */
		cmd[2] = (arg >> 16);
		cmd[3] = (arg >> 8);
		cmd[4] = arg;
                DEBC(printk(ST_DEB_MSG "st%d: Spacing tape forward %d blocks.\n", dev,
			       cmd[2] * 65536 + cmd[3] * 256 + cmd[4]));
		if (blkno >= 0)
			blkno += arg;
		at_sm &= (arg == 0);
		break;
	case MTBSR:
		cmd[0] = SPACE;
		cmd[1] = 0x00;	/* Space Blocks */
		ltmp = (-arg);
		cmd[2] = (ltmp >> 16);
		cmd[3] = (ltmp >> 8);
		cmd[4] = ltmp;
                DEBC(
                     if (cmd[2] & 0x80)
                          ltmp = 0xff000000;
                     ltmp = ltmp | (cmd[2] << 16) | (cmd[3] << 8) | cmd[4];
                     printk(ST_DEB_MSG
                            "st%d: Spacing tape backward %ld blocks.\n", dev, (-ltmp));
		)
		if (blkno >= 0)
			blkno -= arg;
		at_sm &= (arg == 0);
		break;
	case MTFSS:
		cmd[0] = SPACE;
		cmd[1] = 0x04;	/* Space Setmarks */
		cmd[2] = (arg >> 16);
		cmd[3] = (arg >> 8);
		cmd[4] = arg;
                DEBC(printk(ST_DEB_MSG "st%d: Spacing tape forward %d setmarks.\n", dev,
                            cmd[2] * 65536 + cmd[3] * 256 + cmd[4]));
		if (arg != 0) {
			blkno = fileno = (-1);
			at_sm = 1;
		}
		break;
	case MTBSS:
		cmd[0] = SPACE;
		cmd[1] = 0x04;	/* Space Setmarks */
		ltmp = (-arg);
		cmd[2] = (ltmp >> 16);
		cmd[3] = (ltmp >> 8);
		cmd[4] = ltmp;
                DEBC(
                     if (cmd[2] & 0x80)
				ltmp = 0xff000000;
                     ltmp = ltmp | (cmd[2] << 16) | (cmd[3] << 8) | cmd[4];
                     printk(ST_DEB_MSG "st%d: Spacing tape backward %ld setmarks.\n",
                            dev, (-ltmp));
		)
		if (arg != 0) {
			blkno = fileno = (-1);
			at_sm = 1;
		}
		break;
	case MTWEOF:
	case MTWSM:
		if (STp->write_prot)
			return (-EACCES);
		cmd[0] = WRITE_FILEMARKS;
		if (cmd_in == MTWSM)
			cmd[1] = 2;
		cmd[2] = (arg >> 16);
		cmd[3] = (arg >> 8);
		cmd[4] = arg;
		timeout = STp->timeout;
                DEBC(
                     if (cmd_in == MTWEOF)
                               printk(ST_DEB_MSG "st%d: Writing %d filemarks.\n", dev,
				 cmd[2] * 65536 + cmd[3] * 256 + cmd[4]);
                     else
				printk(ST_DEB_MSG "st%d: Writing %d setmarks.\n", dev,
				 cmd[2] * 65536 + cmd[3] * 256 + cmd[4]);
		)
		if (fileno >= 0)
			fileno += arg;
		blkno = 0;
		at_sm = (cmd_in == MTWSM);
		break;
	case MTREW:
		cmd[0] = REZERO_UNIT;
		if (STp->immediate) {
			cmd[1] = 1;	/* Don't wait for completion */
			timeout = STp->timeout;
		}
                DEBC(printk(ST_DEB_MSG "st%d: Rewinding tape.\n", dev));
		fileno = blkno = at_sm = 0;
		break;
	case MTNOP:
                DEBC(printk(ST_DEB_MSG "st%d: No op on tape.\n", dev));
		return 0;	/* Should do something ? */
		break;
	case MTRETEN:
		cmd[0] = START_STOP;
		if (STp->immediate) {
			cmd[1] = 1;	/* Don't wait for completion */
			timeout = STp->timeout;
		}
		cmd[4] = 3;
                DEBC(printk(ST_DEB_MSG "st%d: Retensioning tape.\n", dev));
		fileno = blkno = at_sm = 0;
		break;
	case MTEOM:
		if (!STp->fast_mteom) {
			/* space to the end of tape */
			ioctl_result = st_int_ioctl(STp, MTFSF, 0x7fffff);
			fileno = STps->drv_file;
			if (STps->eof >= ST_EOD_1)
				return 0;
			/* The next lines would hide the number of spaced FileMarks
			   That's why I inserted the previous lines. I had no luck
			   with detecting EOM with FSF, so we go now to EOM.
			   Joerg Weule */
		} else
			fileno = (-1);
		cmd[0] = SPACE;
		cmd[1] = 3;
                DEBC(printk(ST_DEB_MSG "st%d: Spacing to end of recorded medium.\n",
                            dev));
		blkno = 0;
		at_sm = 0;
		break;
	case MTERASE:
		if (STp->write_prot)
			return (-EACCES);
		cmd[0] = ERASE;
		cmd[1] = 1;	/* To the end of tape */
		if (STp->immediate) {
			cmd[1] |= 2;	/* Don't wait for completion */
			timeout = STp->timeout;
		}
		else
			timeout = STp->long_timeout * 8;

                DEBC(printk(ST_DEB_MSG "st%d: Erasing tape.\n", dev));
		fileno = blkno = at_sm = 0;
		break;
	case MTLOCK:
		chg_eof = FALSE;
		cmd[0] = ALLOW_MEDIUM_REMOVAL;
		cmd[4] = SCSI_REMOVAL_PREVENT;
                DEBC(printk(ST_DEB_MSG "st%d: Locking drive door.\n", dev));
		break;
	case MTUNLOCK:
		chg_eof = FALSE;
		cmd[0] = ALLOW_MEDIUM_REMOVAL;
		cmd[4] = SCSI_REMOVAL_ALLOW;
                DEBC(printk(ST_DEB_MSG "st%d: Unlocking drive door.\n", dev));
		break;
	case MTSETBLK:		/* Set block length */
	case MTSETDENSITY:	/* Set tape density */
	case MTSETDRVBUFFER:	/* Set drive buffering */
	case SET_DENS_AND_BLK:	/* Set density and block size */
		chg_eof = FALSE;
		if (STp->dirty || (STp->buffer)->buffer_bytes != 0)
			return (-EIO);	/* Not allowed if data in buffer */
		if ((cmd_in == MTSETBLK || cmd_in == SET_DENS_AND_BLK) &&
		    (arg & MT_ST_BLKSIZE_MASK) != 0 &&
		    STp->max_block > 0 &&
		    ((arg & MT_ST_BLKSIZE_MASK) < STp->min_block ||
		     (arg & MT_ST_BLKSIZE_MASK) > STp->max_block)) {
			printk(KERN_WARNING "st%d: Illegal block size.\n", dev);
			return (-EINVAL);
		}
		cmd[0] = MODE_SELECT;
		if ((STp->use_pf & USE_PF))
			cmd[1] = MODE_SELECT_PAGE_FORMAT;
		cmd[4] = datalen = 12;
		direction = SCSI_DATA_WRITE;

		memset((STp->buffer)->b_data, 0, 12);
		if (cmd_in == MTSETDRVBUFFER)
			(STp->buffer)->b_data[2] = (arg & 7) << 4;
		else
			(STp->buffer)->b_data[2] =
			    STp->drv_buffer << 4;
		(STp->buffer)->b_data[3] = 8;	/* block descriptor length */
		if (cmd_in == MTSETDENSITY) {
			(STp->buffer)->b_data[4] = arg;
			STp->density_changed = TRUE;	/* At least we tried ;-) */
		} else if (cmd_in == SET_DENS_AND_BLK)
			(STp->buffer)->b_data[4] = arg >> 24;
		else
			(STp->buffer)->b_data[4] = STp->density;
		if (cmd_in == MTSETBLK || cmd_in == SET_DENS_AND_BLK) {
			ltmp = arg & MT_ST_BLKSIZE_MASK;
			if (cmd_in == MTSETBLK)
				STp->blksize_changed = TRUE; /* At least we tried ;-) */
		} else
			ltmp = STp->block_size;
		(STp->buffer)->b_data[9] = (ltmp >> 16);
		(STp->buffer)->b_data[10] = (ltmp >> 8);
		(STp->buffer)->b_data[11] = ltmp;
		timeout = STp->timeout;
                DEBC(
			if (cmd_in == MTSETBLK || cmd_in == SET_DENS_AND_BLK)
				printk(ST_DEB_MSG
                                       "st%d: Setting block size to %d bytes.\n", dev,
				       (STp->buffer)->b_data[9] * 65536 +
				       (STp->buffer)->b_data[10] * 256 +
				       (STp->buffer)->b_data[11]);
			if (cmd_in == MTSETDENSITY || cmd_in == SET_DENS_AND_BLK)
				printk(ST_DEB_MSG
                                       "st%d: Setting density code to %x.\n", dev,
				       (STp->buffer)->b_data[4]);
			if (cmd_in == MTSETDRVBUFFER)
				printk(ST_DEB_MSG
                                       "st%d: Setting drive buffer code to %d.\n", dev,
				    ((STp->buffer)->b_data[2] >> 4) & 7);
		)
		break;
	default:
		return (-ENOSYS);
	}

	SRpnt = st_do_scsi(NULL, STp, cmd, datalen, direction,
			   timeout, MAX_RETRIES, TRUE);
	if (!SRpnt)
		return (STp->buffer)->syscall_result;

	ioctl_result = (STp->buffer)->syscall_result;

	if (!ioctl_result) {	/* SCSI command successful */
		scsi_release_request(SRpnt);
		SRpnt = NULL;
		STps->drv_block = blkno;
		STps->drv_file = fileno;
		STps->at_sm = at_sm;

		if (cmd_in == MTLOCK)
			STp->door_locked = ST_LOCKED_EXPLICIT;
		else if (cmd_in == MTUNLOCK)
			STp->door_locked = ST_UNLOCKED;

		if (cmd_in == MTBSFM)
			ioctl_result = st_int_ioctl(STp, MTFSF, 1);
		else if (cmd_in == MTFSFM)
			ioctl_result = st_int_ioctl(STp, MTBSF, 1);

		if (cmd_in == MTSETBLK || cmd_in == SET_DENS_AND_BLK) {
			STp->block_size = arg & MT_ST_BLKSIZE_MASK;
			if (STp->block_size != 0)
				(STp->buffer)->buffer_blocks =
				    (STp->buffer)->buffer_size / STp->block_size;
			(STp->buffer)->buffer_bytes = (STp->buffer)->read_pointer = 0;
			if (cmd_in == SET_DENS_AND_BLK)
				STp->density = arg >> MT_ST_DENSITY_SHIFT;
		} else if (cmd_in == MTSETDRVBUFFER)
			STp->drv_buffer = (arg & 7);
		else if (cmd_in == MTSETDENSITY)
			STp->density = arg;

		if (cmd_in == MTEOM)
			STps->eof = ST_EOD;
		else if (cmd_in == MTFSF)
			STps->eof = ST_FM;
		else if (chg_eof)
			STps->eof = ST_NOEOF;

	} else { /* SCSI command was not completely successful. Don't return
                    from this block without releasing the SCSI command block! */

		if (SRpnt->sr_sense_buffer[2] & 0x40) {
			if (cmd_in != MTBSF && cmd_in != MTBSFM &&
			    cmd_in != MTBSR && cmd_in != MTBSS)
				STps->eof = ST_EOM_OK;
			STps->drv_block = 0;
		}

		undone = ((SRpnt->sr_sense_buffer[3] << 24) +
			  (SRpnt->sr_sense_buffer[4] << 16) +
			  (SRpnt->sr_sense_buffer[5] << 8) +
			  SRpnt->sr_sense_buffer[6]);

		if (cmd_in == MTWEOF &&
		    (SRpnt->sr_sense_buffer[0] & 0x70) == 0x70 &&
		    (SRpnt->sr_sense_buffer[2] & 0x4f) == 0x40 &&
		 ((SRpnt->sr_sense_buffer[0] & 0x80) == 0 || undone == 0)) {
			ioctl_result = 0;	/* EOF written succesfully at EOM */
			if (fileno >= 0)
				fileno++;
			STps->drv_file = fileno;
			STps->eof = ST_NOEOF;
		} else if ((cmd_in == MTFSF) || (cmd_in == MTFSFM)) {
			if (fileno >= 0)
				STps->drv_file = fileno - undone;
			else
				STps->drv_file = fileno;
			STps->drv_block = 0;
			STps->eof = ST_NOEOF;
		} else if ((cmd_in == MTBSF) || (cmd_in == MTBSFM)) {
			if (arg > 0 && undone < 0)  /* Some drives get this wrong */
				undone = (-undone);
			if (STps->drv_file >= 0)
				STps->drv_file = fileno + undone;
			STps->drv_block = 0;
			STps->eof = ST_NOEOF;
		} else if (cmd_in == MTFSR) {
			if (SRpnt->sr_sense_buffer[2] & 0x80) {	/* Hit filemark */
				if (STps->drv_file >= 0)
					STps->drv_file++;
				STps->drv_block = 0;
				STps->eof = ST_FM;
			} else {
				if (blkno >= undone)
					STps->drv_block = blkno - undone;
				else
					STps->drv_block = (-1);
				STps->eof = ST_NOEOF;
			}
		} else if (cmd_in == MTBSR) {
			if (SRpnt->sr_sense_buffer[2] & 0x80) {	/* Hit filemark */
				STps->drv_file--;
				STps->drv_block = (-1);
			} else {
				if (arg > 0 && undone < 0)  /* Some drives get this wrong */
					undone = (-undone);
				if (STps->drv_block >= 0)
					STps->drv_block = blkno + undone;
			}
			STps->eof = ST_NOEOF;
		} else if (cmd_in == MTEOM) {
			STps->drv_file = (-1);
			STps->drv_block = (-1);
			STps->eof = ST_EOD;
		} else if (cmd_in == MTSETBLK ||
			   cmd_in == MTSETDENSITY ||
			   cmd_in == MTSETDRVBUFFER ||
			   cmd_in == SET_DENS_AND_BLK) {
			if ((SRpnt->sr_sense_buffer[2] & 0x0f) == ILLEGAL_REQUEST &&
			    !(STp->use_pf & PF_TESTED)) {
				/* Try the other possible state of Page Format if not
				   already tried */
				STp->use_pf = !STp->use_pf | PF_TESTED;
				scsi_release_request(SRpnt);
				SRpnt = NULL;
				return st_int_ioctl(STp, cmd_in, arg);
			}
		} else if (chg_eof)
			STps->eof = ST_NOEOF;

		if ((SRpnt->sr_sense_buffer[2] & 0x0f) == BLANK_CHECK)
			STps->eof = ST_EOD;

		if (cmd_in == MTLOCK)
			STp->door_locked = ST_LOCK_FAILS;

		scsi_release_request(SRpnt);
		SRpnt = NULL;
	}

	return ioctl_result;
}


/* Get the tape position. If bt == 2, arg points into a kernel space mt_loc
   structure. */

static int get_location(Scsi_Tape *STp, unsigned int *block, int *partition,
			int logical)
{
	int result;
	unsigned char scmd[MAX_COMMAND_SIZE];
	Scsi_Request *SRpnt;
	DEB( int dev = TAPE_NR(STp->devt); )

	if (STp->ready != ST_READY)
		return (-EIO);

	memset(scmd, 0, MAX_COMMAND_SIZE);
	if ((STp->device)->scsi_level < SCSI_2) {
		scmd[0] = QFA_REQUEST_BLOCK;
		scmd[4] = 3;
	} else {
		scmd[0] = READ_POSITION;
		if (!logical && !STp->scsi2_logical)
			scmd[1] = 1;
	}
	SRpnt = st_do_scsi(NULL, STp, scmd, 20, SCSI_DATA_READ, STp->timeout,
			   MAX_READY_RETRIES, TRUE);
	if (!SRpnt)
		return (STp->buffer)->syscall_result;

	if ((STp->buffer)->syscall_result != 0 ||
	    (STp->device->scsi_level >= SCSI_2 &&
	     ((STp->buffer)->b_data[0] & 4) != 0)) {
		*block = *partition = 0;
                DEBC(printk(ST_DEB_MSG "st%d: Can't read tape position.\n", dev));
		result = (-EIO);
	} else {
		result = 0;
		if ((STp->device)->scsi_level < SCSI_2) {
			*block = ((STp->buffer)->b_data[0] << 16)
			    + ((STp->buffer)->b_data[1] << 8)
			    + (STp->buffer)->b_data[2];
			*partition = 0;
		} else {
			*block = ((STp->buffer)->b_data[4] << 24)
			    + ((STp->buffer)->b_data[5] << 16)
			    + ((STp->buffer)->b_data[6] << 8)
			    + (STp->buffer)->b_data[7];
			*partition = (STp->buffer)->b_data[1];
			if (((STp->buffer)->b_data[0] & 0x80) &&
			    (STp->buffer)->b_data[1] == 0)	/* BOP of partition 0 */
				STp->ps[0].drv_block = STp->ps[0].drv_file = 0;
		}
                DEBC(printk(ST_DEB_MSG "st%d: Got tape pos. blk %d part %d.\n", dev,
                            *block, *partition));
	}
	scsi_release_request(SRpnt);
	SRpnt = NULL;

	return result;
}


/* Set the tape block and partition. Negative partition means that only the
   block should be set in vendor specific way. */
static int set_location(Scsi_Tape *STp, unsigned int block, int partition,
			int logical)
{
	ST_partstat *STps;
	int result, p;
	unsigned int blk;
	int timeout;
	unsigned char scmd[MAX_COMMAND_SIZE];
	Scsi_Request *SRpnt;
	DEB( int dev = TAPE_NR(STp->devt); )

	if (STp->ready != ST_READY)
		return (-EIO);
	timeout = STp->long_timeout;
	STps = &(STp->ps[STp->partition]);

        DEBC(printk(ST_DEB_MSG "st%d: Setting block to %d and partition to %d.\n",
                    dev, block, partition));
	DEB(if (partition < 0)
		return (-EIO); )

	/* Update the location at the partition we are leaving */
	if ((!STp->can_partitions && partition != 0) ||
	    partition >= ST_NBR_PARTITIONS)
		return (-EINVAL);
	if (partition != STp->partition) {
		if (get_location(STp, &blk, &p, 1))
			STps->last_block_valid = FALSE;
		else {
			STps->last_block_valid = TRUE;
			STps->last_block_visited = blk;
                        DEBC(printk(ST_DEB_MSG
                                    "st%d: Visited block %d for partition %d saved.\n",
                                    dev, blk, STp->partition));
		}
	}

	memset(scmd, 0, MAX_COMMAND_SIZE);
	if ((STp->device)->scsi_level < SCSI_2) {
		scmd[0] = QFA_SEEK_BLOCK;
		scmd[2] = (block >> 16);
		scmd[3] = (block >> 8);
		scmd[4] = block;
		scmd[5] = 0;
	} else {
		scmd[0] = SEEK_10;
		scmd[3] = (block >> 24);
		scmd[4] = (block >> 16);
		scmd[5] = (block >> 8);
		scmd[6] = block;
		if (!logical && !STp->scsi2_logical)
			scmd[1] = 4;
		if (STp->partition != partition) {
			scmd[1] |= 2;
			scmd[8] = partition;
                        DEBC(printk(ST_DEB_MSG
                                    "st%d: Trying to change partition from %d to %d\n",
                                    dev, STp->partition, partition));
		}
	}
	if (STp->immediate) {
		scmd[1] |= 1;		/* Don't wait for completion */
		timeout = STp->timeout;
	}

	SRpnt = st_do_scsi(NULL, STp, scmd, 0, SCSI_DATA_NONE,
			   timeout, MAX_READY_RETRIES, TRUE);
	if (!SRpnt)
		return (STp->buffer)->syscall_result;

	STps->drv_block = STps->drv_file = (-1);
	STps->eof = ST_NOEOF;
	if ((STp->buffer)->syscall_result != 0) {
		result = (-EIO);
		if (STp->can_partitions &&
		    (STp->device)->scsi_level >= SCSI_2 &&
		    (p = find_partition(STp)) >= 0)
			STp->partition = p;
	} else {
		if (STp->can_partitions) {
			STp->partition = partition;
			STps = &(STp->ps[partition]);
			if (!STps->last_block_valid ||
			    STps->last_block_visited != block) {
				STps->at_sm = 0;
				STps->rw = ST_IDLE;
			}
		} else
			STps->at_sm = 0;
		if (block == 0)
			STps->drv_block = STps->drv_file = 0;
		result = 0;
	}

	scsi_release_request(SRpnt);
	SRpnt = NULL;

	return result;
}


/* Find the current partition number for the drive status. Called from open and
   returns either partition number of negative error code. */
static int find_partition(Scsi_Tape *STp)
{
	int i, partition;
	unsigned int block;

	if ((i = get_location(STp, &block, &partition, 1)) < 0)
		return i;
	if (partition >= ST_NBR_PARTITIONS)
		return (-EIO);
	return partition;
}


/* Change the partition if necessary */
static int update_partition(Scsi_Tape *STp)
{
	ST_partstat *STps;

	if (STp->partition == STp->new_partition)
		return 0;
	STps = &(STp->ps[STp->new_partition]);
	if (!STps->last_block_valid)
		STps->last_block_visited = 0;
	return set_location(STp, STps->last_block_visited, STp->new_partition, 1);
}

/* Functions for reading and writing the medium partition mode page. */

#define PART_PAGE   0x11
#define PART_PAGE_FIXED_LENGTH 8

#define PP_OFF_MAX_ADD_PARTS   2
#define PP_OFF_NBR_ADD_PARTS   3
#define PP_OFF_FLAGS           4
#define PP_OFF_PART_UNITS      6
#define PP_OFF_RESERVED        7

#define PP_BIT_IDP             0x20
#define PP_MSK_PSUM_MB         0x10

/* Get the number of partitions on the tape. As a side effect reads the
   mode page into the tape buffer. */
static int nbr_partitions(Scsi_Tape *STp)
{
	int result;
	DEB( int dev = TAPE_NR(STp->devt) );

	if (STp->ready != ST_READY)
		return (-EIO);

	result = read_mode_page(STp, PART_PAGE, TRUE);

	if (result) {
                DEBC(printk(ST_DEB_MSG "st%d: Can't read medium partition page.\n",
                            dev));
		result = (-EIO);
	} else {
		result = (STp->buffer)->b_data[MODE_HEADER_LENGTH +
					      PP_OFF_NBR_ADD_PARTS] + 1;
                DEBC(printk(ST_DEB_MSG "st%d: Number of partitions %d.\n", dev, result));
	}

	return result;
}


/* Partition the tape into two partitions if size > 0 or one partition if
   size == 0.

   The block descriptors are read and written because Sony SDT-7000 does not
   work without this (suggestion from Michael Schaefer <Michael.Schaefer@dlr.de>).

   My HP C1533A drive returns only one partition size field. This is used to
   set the size of partition 1. There is no size field for the default partition.
   Michael Schaefer's Sony SDT-7000 returns two descriptors and the second is
   used to set the size of partition 1 (this is what the SCSI-3 standard specifies).
   The following algorithm is used to accomodate both drives: if the number of
   partition size fields is greater than the maximum number of additional partitions
   in the mode page, the second field is used. Otherwise the first field is used.

   For Seagate DDS drives the page length must be 8 when no partitions is defined
   and 10 when 1 partition is defined (information from Eric Lee Green). This is
   is acceptable also to some other old drives and enforced if the first partition
   size field is used for the first additional partition size.
 */
static int partition_tape(Scsi_Tape *STp, int size)
{
	int dev = TAPE_NR(STp->devt), result;
	int pgo, psd_cnt, psdo;
	unsigned char *bp;

	result = read_mode_page(STp, PART_PAGE, FALSE);
	if (result) {
		DEBC(printk(ST_DEB_MSG "st%d: Can't read partition mode page.\n", dev));
		return result;
	}
	/* The mode page is in the buffer. Let's modify it and write it. */
	bp = (STp->buffer)->b_data;
	pgo = MODE_HEADER_LENGTH + bp[MH_OFF_BDESCS_LENGTH];
	DEBC(printk(ST_DEB_MSG "st%d: Partition page length is %d bytes.\n",
		    dev, bp[pgo + MP_OFF_PAGE_LENGTH] + 2));

	psd_cnt = (bp[pgo + MP_OFF_PAGE_LENGTH] + 2 - PART_PAGE_FIXED_LENGTH) / 2;
	psdo = pgo + PART_PAGE_FIXED_LENGTH;
	if (psd_cnt > bp[pgo + PP_OFF_MAX_ADD_PARTS]) {
		bp[psdo] = bp[psdo + 1] = 0xff;  /* Rest of the tape */
		psdo += 2;
	}
	memset(bp + psdo, 0, bp[pgo + PP_OFF_NBR_ADD_PARTS] * 2);

	DEBC(printk("st%d: psd_cnt %d, max.parts %d, nbr_parts %d\n", dev,
		    psd_cnt, bp[pgo + PP_OFF_MAX_ADD_PARTS],
		    bp[pgo + PP_OFF_NBR_ADD_PARTS]));

	if (size <= 0) {
		bp[pgo + PP_OFF_NBR_ADD_PARTS] = 0;
		if (psd_cnt <= bp[pgo + PP_OFF_MAX_ADD_PARTS])
		    bp[pgo + MP_OFF_PAGE_LENGTH] = 6;
                DEBC(printk(ST_DEB_MSG "st%d: Formatting tape with one partition.\n",
                            dev));
	} else {
		bp[psdo] = (size >> 8) & 0xff;
		bp[psdo + 1] = size & 0xff;
		bp[pgo + 3] = 1;
		if (bp[pgo + MP_OFF_PAGE_LENGTH] < 8)
		    bp[pgo + MP_OFF_PAGE_LENGTH] = 8;
                DEBC(printk(ST_DEB_MSG
                            "st%d: Formatting tape with two partitions (1 = %d MB).\n",
                            dev, size));
	}
	bp[pgo + PP_OFF_PART_UNITS] = 0;
	bp[pgo + PP_OFF_RESERVED] = 0;
	bp[pgo + PP_OFF_FLAGS] = PP_BIT_IDP | PP_MSK_PSUM_MB;

	result = write_mode_page(STp, PART_PAGE, TRUE);
	if (result) {
		printk(KERN_INFO "st%d: Partitioning of tape failed.\n", dev);
		result = (-EIO);
	}

	return result;
}



/* The ioctl command */
static int st_ioctl(struct inode *inode, struct file *file,
		    unsigned int cmd_in, unsigned long arg)
{
	int i, cmd_nr, cmd_type, bt;
	int retval = 0;
	unsigned int blk;
	Scsi_Tape *STp;
	ST_mode *STm;
	ST_partstat *STps;
	int dev = TAPE_NR(inode->i_rdev);

	read_lock(&st_dev_arr_lock);
	STp = scsi_tapes[dev];
	read_unlock(&st_dev_arr_lock);

	if (down_interruptible(&STp->lock))
		return -ERESTARTSYS;

        DEB(
	if (debugging && !STp->in_use) {
		printk(ST_DEB_MSG "st%d: Incorrect device.\n", dev);
		retval = (-EIO);
		goto out;
	} ) /* end DEB */

	STm = &(STp->modes[STp->current_mode]);
	STps = &(STp->ps[STp->partition]);

	/*
	 * If we are in the middle of error recovery, don't let anyone
	 * else try and use this device.  Also, if error recovery fails, it
	 * may try and take the device offline, in which case all further
	 * access to the device is prohibited.
	 */
	if (!scsi_block_when_processing_errors(STp->device)) {
		retval = (-ENXIO);
		goto out;
	}
	cmd_type = _IOC_TYPE(cmd_in);
	cmd_nr = _IOC_NR(cmd_in);

	if (cmd_type == _IOC_TYPE(MTIOCTOP) && cmd_nr == _IOC_NR(MTIOCTOP)) {
		struct mtop mtc;

		if (_IOC_SIZE(cmd_in) != sizeof(mtc)) {
			retval = (-EINVAL);
			goto out;
		}

		i = copy_from_user((char *) &mtc, (char *) arg, sizeof(struct mtop));
		if (i) {
			retval = (-EFAULT);
			goto out;
		}

		if (mtc.mt_op == MTSETDRVBUFFER && !capable(CAP_SYS_ADMIN)) {
			printk(KERN_WARNING
                               "st%d: MTSETDRVBUFFER only allowed for root.\n", dev);
			retval = (-EPERM);
			goto out;
		}
		if (!STm->defined &&
		    (mtc.mt_op != MTSETDRVBUFFER &&
		     (mtc.mt_count & MT_ST_OPTIONS) == 0)) {
			retval = (-ENXIO);
			goto out;
		}

		if (!(STp->device)->was_reset) {

			if (STps->eof == ST_FM_HIT) {
				if (mtc.mt_op == MTFSF || mtc.mt_op == MTFSFM ||
                                    mtc.mt_op == MTEOM) {
					mtc.mt_count -= 1;
					if (STps->drv_file >= 0)
						STps->drv_file += 1;
				} else if (mtc.mt_op == MTBSF || mtc.mt_op == MTBSFM) {
					mtc.mt_count += 1;
					if (STps->drv_file >= 0)
						STps->drv_file += 1;
				}
			}

			if (mtc.mt_op == MTSEEK) {
				/* Old position must be restored if partition will be
                                   changed */
				i = !STp->can_partitions ||
				    (STp->new_partition != STp->partition);
			} else {
				i = mtc.mt_op == MTREW || mtc.mt_op == MTOFFL ||
				    mtc.mt_op == MTRETEN || mtc.mt_op == MTEOM ||
				    mtc.mt_op == MTLOCK || mtc.mt_op == MTLOAD ||
				    mtc.mt_op == MTFSF || mtc.mt_op == MTFSFM ||
				    mtc.mt_op == MTBSF || mtc.mt_op == MTBSFM ||
				    mtc.mt_op == MTCOMPRESSION;
			}
			i = flush_buffer(STp, i);
			if (i < 0) {
				retval = i;
				goto out;
			}
		} else {
			/*
			 * If there was a bus reset, block further access
			 * to this device.  If the user wants to rewind the tape,
			 * then reset the flag and allow access again.
			 */
			if (mtc.mt_op != MTREW &&
			    mtc.mt_op != MTOFFL &&
			    mtc.mt_op != MTRETEN &&
			    mtc.mt_op != MTERASE &&
			    mtc.mt_op != MTSEEK &&
			    mtc.mt_op != MTEOM) {
				retval = (-EIO);
				goto out;
			}
			STp->device->was_reset = 0;
			if (STp->door_locked != ST_UNLOCKED &&
			    STp->door_locked != ST_LOCK_FAILS) {
				if (st_int_ioctl(STp, MTLOCK, 0)) {
					printk(KERN_NOTICE
                                               "st%d: Could not relock door after bus reset.\n",
					       dev);
					STp->door_locked = ST_UNLOCKED;
				}
			}
		}

		if (mtc.mt_op != MTNOP && mtc.mt_op != MTSETBLK &&
		    mtc.mt_op != MTSETDENSITY && mtc.mt_op != MTWSM &&
		    mtc.mt_op != MTSETDRVBUFFER && mtc.mt_op != MTSETPART)
			STps->rw = ST_IDLE;	/* Prevent automatic WEOF and fsf */

		if (mtc.mt_op == MTOFFL && STp->door_locked != ST_UNLOCKED)
			st_int_ioctl(STp, MTUNLOCK, 0);	/* Ignore result! */

		if (mtc.mt_op == MTSETDRVBUFFER &&
		    (mtc.mt_count & MT_ST_OPTIONS) != 0) {
			retval = st_set_options(STp, mtc.mt_count);
			goto out;
		}

		if (mtc.mt_op == MTSETPART) {
			if (!STp->can_partitions ||
			    mtc.mt_count < 0 || mtc.mt_count >= ST_NBR_PARTITIONS) {
				retval = (-EINVAL);
				goto out;
			}
			if (mtc.mt_count >= STp->nbr_partitions &&
			    (STp->nbr_partitions = nbr_partitions(STp)) < 0) {
				retval = (-EIO);
				goto out;
			}
			if (mtc.mt_count >= STp->nbr_partitions) {
				retval = (-EINVAL);
				goto out;
			}
			STp->new_partition = mtc.mt_count;
			retval = 0;
			goto out;
		}

		if (mtc.mt_op == MTMKPART) {
			if (!STp->can_partitions) {
				retval = (-EINVAL);
				goto out;
			}
			if ((i = st_int_ioctl(STp, MTREW, 0)) < 0 ||
			    (i = partition_tape(STp, mtc.mt_count)) < 0) {
				retval = i;
				goto out;
			}
			for (i = 0; i < ST_NBR_PARTITIONS; i++) {
				STp->ps[i].rw = ST_IDLE;
				STp->ps[i].at_sm = 0;
				STp->ps[i].last_block_valid = FALSE;
			}
			STp->partition = STp->new_partition = 0;
			STp->nbr_partitions = 1;	/* Bad guess ?-) */
			STps->drv_block = STps->drv_file = 0;
			retval = 0;
			goto out;
		}

		if (mtc.mt_op == MTSEEK) {
			i = set_location(STp, mtc.mt_count, STp->new_partition, 0);
			if (!STp->can_partitions)
				STp->ps[0].rw = ST_IDLE;
			retval = i;
			goto out;
		}

		if (mtc.mt_op == MTUNLOAD || mtc.mt_op == MTOFFL) {
			retval = do_load_unload(STp, file, 0);
			goto out;
		}

		if (mtc.mt_op == MTLOAD) {
			retval = do_load_unload(STp, file, max(1, mtc.mt_count));
			goto out;
		}

		if (STp->can_partitions && STp->ready == ST_READY &&
		    (i = update_partition(STp)) < 0) {
			retval = i;
			goto out;
		}

		if (mtc.mt_op == MTCOMPRESSION)
			retval = st_compression(STp, (mtc.mt_count & 1));
		else
			retval = st_int_ioctl(STp, mtc.mt_op, mtc.mt_count);
		goto out;
	}
	if (!STm->defined) {
		retval = (-ENXIO);
		goto out;
	}

	if ((i = flush_buffer(STp, FALSE)) < 0) {
		retval = i;
		goto out;
	}
	if (STp->can_partitions &&
	    (i = update_partition(STp)) < 0) {
		retval = i;
		goto out;
	}

	if (cmd_type == _IOC_TYPE(MTIOCGET) && cmd_nr == _IOC_NR(MTIOCGET)) {
		struct mtget mt_status;

		if (_IOC_SIZE(cmd_in) != sizeof(struct mtget)) {
			 retval = (-EINVAL);
			 goto out;
		}

		mt_status.mt_type = STp->tape_type;
		mt_status.mt_dsreg =
		    ((STp->block_size << MT_ST_BLKSIZE_SHIFT) & MT_ST_BLKSIZE_MASK) |
		    ((STp->density << MT_ST_DENSITY_SHIFT) & MT_ST_DENSITY_MASK);
		mt_status.mt_blkno = STps->drv_block;
		mt_status.mt_fileno = STps->drv_file;
		if (STp->block_size != 0) {
			if (STps->rw == ST_WRITING)
				mt_status.mt_blkno +=
				    (STp->buffer)->buffer_bytes / STp->block_size;
			else if (STps->rw == ST_READING)
				mt_status.mt_blkno -=
                                        ((STp->buffer)->buffer_bytes +
                                         STp->block_size - 1) / STp->block_size;
		}

		mt_status.mt_gstat = 0;
		if (STp->drv_write_prot)
			mt_status.mt_gstat |= GMT_WR_PROT(0xffffffff);
		if (mt_status.mt_blkno == 0) {
			if (mt_status.mt_fileno == 0)
				mt_status.mt_gstat |= GMT_BOT(0xffffffff);
			else
				mt_status.mt_gstat |= GMT_EOF(0xffffffff);
		}
		mt_status.mt_erreg = (STp->recover_reg << MT_ST_SOFTERR_SHIFT);
		mt_status.mt_resid = STp->partition;
		if (STps->eof == ST_EOM_OK || STps->eof == ST_EOM_ERROR)
			mt_status.mt_gstat |= GMT_EOT(0xffffffff);
		else if (STps->eof >= ST_EOM_OK)
			mt_status.mt_gstat |= GMT_EOD(0xffffffff);
		if (STp->density == 1)
			mt_status.mt_gstat |= GMT_D_800(0xffffffff);
		else if (STp->density == 2)
			mt_status.mt_gstat |= GMT_D_1600(0xffffffff);
		else if (STp->density == 3)
			mt_status.mt_gstat |= GMT_D_6250(0xffffffff);
		if (STp->ready == ST_READY)
			mt_status.mt_gstat |= GMT_ONLINE(0xffffffff);
		if (STp->ready == ST_NO_TAPE)
			mt_status.mt_gstat |= GMT_DR_OPEN(0xffffffff);
		if (STps->at_sm)
			mt_status.mt_gstat |= GMT_SM(0xffffffff);
		if (STm->do_async_writes ||
                    (STm->do_buffer_writes && STp->block_size != 0) ||
		    STp->drv_buffer != 0)
			mt_status.mt_gstat |= GMT_IM_REP_EN(0xffffffff);
		if (STp->cleaning_req)
			mt_status.mt_gstat |= GMT_CLN(0xffffffff);

		i = copy_to_user((char *) arg, (char *) &(mt_status),
				 sizeof(struct mtget));
		if (i) {
			retval = (-EFAULT);
			goto out;
		}

		STp->recover_reg = 0;		/* Clear after read */
		retval = 0;
		goto out;
	}			/* End of MTIOCGET */
	if (cmd_type == _IOC_TYPE(MTIOCPOS) && cmd_nr == _IOC_NR(MTIOCPOS)) {
		struct mtpos mt_pos;
		if (_IOC_SIZE(cmd_in) != sizeof(struct mtpos)) {
			 retval = (-EINVAL);
			 goto out;
		}
		if ((i = get_location(STp, &blk, &bt, 0)) < 0) {
			retval = i;
			goto out;
		}
		mt_pos.mt_blkno = blk;
		i = copy_to_user((char *) arg, (char *) (&mt_pos), sizeof(struct mtpos));
		if (i)
			retval = (-EFAULT);
		goto out;
	}
	up(&STp->lock);
	return scsi_ioctl(STp->device, cmd_in, (void *) arg);

 out:
	up(&STp->lock);
	return retval;
}


/* Try to allocate a new tape buffer. Calling function must not hold
   dev_arr_lock. */
static ST_buffer *
 new_tape_buffer(int from_initialization, int need_dma, int in_use)
{
	int i, priority, b_size, order, got = 0, segs = 0;
	unsigned long flags;
	ST_buffer *tb;

	read_lock(&st_dev_arr_lock);
	if (st_nbr_buffers >= st_template.dev_max) {
		read_unlock(&st_dev_arr_lock);
		return NULL;	/* Should never happen */
	}
	read_unlock(&st_dev_arr_lock);

	if (from_initialization)
		priority = GFP_ATOMIC;
	else
		priority = GFP_KERNEL;

	i = sizeof(ST_buffer) + (st_max_sg_segs - 1) * sizeof(struct scatterlist) +
		st_max_sg_segs * sizeof(unsigned int);
	tb = kmalloc(i, priority);
	if (tb) {
		tb->sg_lengths = (unsigned int *)(&tb->sg[0] + st_max_sg_segs);

		if (need_dma)
			priority |= GFP_DMA;

		/* Try to allocate the first segment up to ST_FIRST_ORDER and the
		   others big enough to reach the goal */
		for (b_size = PAGE_SIZE, order=0;
		     b_size < st_buffer_size && order < ST_FIRST_ORDER;
		     order++, b_size *= 2)
			;
		for ( ; b_size >= PAGE_SIZE; order--, b_size /= 2) {
			tb->sg[0].address =
			    (unsigned char *) __get_free_pages(priority, order);
			if (tb->sg[0].address != NULL) {
				tb->sg_lengths[0] = b_size;
				break;
			}
		}
		tb->sg[0].page = NULL;
		if (tb->sg[segs].address == NULL) {
			kfree(tb);
			tb = NULL;
		} else {	/* Got something, continue */

			for (b_size = PAGE_SIZE, order=0;
			     st_buffer_size >
                                     tb->sg_lengths[0] + (ST_FIRST_SG - 1) * b_size;
			     order++, b_size *= 2)
				;
			for (segs = 1, got = tb->sg_lengths[0];
			     got < st_buffer_size && segs < ST_FIRST_SG;) {
				tb->sg[segs].address =
					(unsigned char *) __get_free_pages(priority,
									   order);
				if (tb->sg[segs].address == NULL) {
					if (st_buffer_size - got <=
					    (ST_FIRST_SG - segs) * b_size / 2) {
						b_size /= 2; /* Large enough for the
                                                                rest of the buffers */
						order--;
						continue;
					}
					tb->sg_segs = segs;
					tb->orig_sg_segs = 0;
					DEB(tb->buffer_size = got);
					normalize_buffer(tb);
					kfree(tb);
					tb = NULL;
					break;
				}
				tb->sg[segs].page = NULL;
				tb->sg_lengths[segs] = b_size;
				got += b_size;
				segs++;
			}
		}
	}

	if (!tb) {
		printk(KERN_NOTICE "st: Can't allocate new tape buffer (nbr %d).\n",
		       st_nbr_buffers);
		return NULL;
	}
	tb->sg_segs = tb->orig_sg_segs = segs;
	tb->b_data = tb->sg[0].address;

        DEBC(printk(ST_DEB_MSG
                    "st: Allocated tape buffer %d (%d bytes, %d segments, dma: %d, a: %p).\n",
                    st_nbr_buffers, got, tb->sg_segs, need_dma, tb->b_data);
             printk(ST_DEB_MSG
                    "st: segment sizes: first %d, last %d bytes.\n",
                    tb->sg_lengths[0], tb->sg_lengths[segs - 1]);
	)
	tb->in_use = in_use;
	tb->dma = need_dma;
	tb->buffer_size = got;
	tb->writing = 0;

	write_lock_irqsave(&st_dev_arr_lock, flags);
	st_buffers[st_nbr_buffers++] = tb;
	write_unlock_irqrestore(&st_dev_arr_lock, flags);

	return tb;
}


/* Try to allocate a temporary enlarged tape buffer */
static int enlarge_buffer(ST_buffer * STbuffer, int new_size, int need_dma)
{
	int segs, nbr, max_segs, b_size, priority, order, got;

	normalize_buffer(STbuffer);

	max_segs = STbuffer->use_sg;
	if (max_segs > st_max_sg_segs)
		max_segs = st_max_sg_segs;
	nbr = max_segs - STbuffer->sg_segs;
	if (nbr <= 0)
		return FALSE;

	priority = GFP_KERNEL;
	if (need_dma)
		priority |= GFP_DMA;
	for (b_size = PAGE_SIZE, order=0;
	     b_size * nbr < new_size - STbuffer->buffer_size;
	     order++, b_size *= 2)
		;  /* empty */

	for (segs = STbuffer->sg_segs, got = STbuffer->buffer_size;
	     segs < max_segs && got < new_size;) {
		STbuffer->sg[segs].address =
			(unsigned char *) __get_free_pages(priority, order);
		if (STbuffer->sg[segs].address == NULL) {
			if (new_size - got <= (max_segs - segs) * b_size / 2) {
				b_size /= 2; /* Large enough for the rest of the buffers */
				order--;
				continue;
			}
			printk(KERN_NOTICE "st: failed to enlarge buffer to %d bytes.\n",
			       new_size);
			DEB(STbuffer->buffer_size = got);
			normalize_buffer(STbuffer);
			return FALSE;
		}
		STbuffer->sg[segs].page = NULL;
		STbuffer->sg_lengths[segs] = b_size;
		STbuffer->sg_segs += 1;
		got += b_size;
		STbuffer->buffer_size = got;
		segs++;
	}
        DEBC(printk(ST_DEB_MSG
                    "st: Succeeded to enlarge buffer to %d bytes (segs %d->%d, %d).\n",
                    got, STbuffer->orig_sg_segs, STbuffer->sg_segs, b_size));

	return TRUE;
}


/* Release the extra buffer */
static void normalize_buffer(ST_buffer * STbuffer)
{
	int i, order, b_size;

	for (i = STbuffer->orig_sg_segs; i < STbuffer->sg_segs; i++) {
		for (b_size=PAGE_SIZE, order=0; b_size < STbuffer->sg_lengths[i];
		     order++, b_size *= 2)
			; /* empty */
		free_pages((unsigned long)(STbuffer->sg[i].address), order);
		STbuffer->buffer_size -= STbuffer->sg_lengths[i];
	}
        DEB(
	if (debugging && STbuffer->orig_sg_segs < STbuffer->sg_segs)
		printk(ST_DEB_MSG "st: Buffer at %p normalized to %d bytes (segs %d).\n",
		       STbuffer->sg[0].address, STbuffer->buffer_size,
		       STbuffer->sg_segs);
        ) /* end DEB */
	STbuffer->sg_segs = STbuffer->orig_sg_segs;
}


/* Move data from the user buffer to the tape buffer. Returns zero (success) or
   negative error code. */
static int append_to_buffer(const char *ubp, ST_buffer * st_bp, int do_count)
{
	int i, cnt, res, offset;

	for (i = 0, offset = st_bp->buffer_bytes;
	     i < st_bp->sg_segs && offset >= st_bp->sg_lengths[i]; i++)
		offset -= st_bp->sg_lengths[i];
	if (i == st_bp->sg_segs) {	/* Should never happen */
		printk(KERN_WARNING "st: append_to_buffer offset overflow.\n");
		return (-EIO);
	}
	for (; i < st_bp->sg_segs && do_count > 0; i++) {
		cnt = st_bp->sg_lengths[i] - offset < do_count ?
		    st_bp->sg_lengths[i] - offset : do_count;
		res = copy_from_user(st_bp->sg[i].address + offset, ubp, cnt);
		if (res)
			return (-EFAULT);
		do_count -= cnt;
		st_bp->buffer_bytes += cnt;
		ubp += cnt;
		offset = 0;
	}
	if (do_count) {		/* Should never happen */
		printk(KERN_WARNING "st: append_to_buffer overflow (left %d).\n",
		       do_count);
		return (-EIO);
	}
	return 0;
}


/* Move data from the tape buffer to the user buffer. Returns zero (success) or
   negative error code. */
static int from_buffer(ST_buffer * st_bp, char *ubp, int do_count)
{
	int i, cnt, res, offset;

	for (i = 0, offset = st_bp->read_pointer;
	     i < st_bp->sg_segs && offset >= st_bp->sg_lengths[i]; i++)
		offset -= st_bp->sg_lengths[i];
	if (i == st_bp->sg_segs) {	/* Should never happen */
		printk(KERN_WARNING "st: from_buffer offset overflow.\n");
		return (-EIO);
	}
	for (; i < st_bp->sg_segs && do_count > 0; i++) {
		cnt = st_bp->sg_lengths[i] - offset < do_count ?
		    st_bp->sg_lengths[i] - offset : do_count;
		res = copy_to_user(ubp, st_bp->sg[i].address + offset, cnt);
		if (res)
			return (-EFAULT);
		do_count -= cnt;
		st_bp->buffer_bytes -= cnt;
		st_bp->read_pointer += cnt;
		ubp += cnt;
		offset = 0;
	}
	if (do_count) {		/* Should never happen */
		printk(KERN_WARNING "st: from_buffer overflow (left %d).\n",
		       do_count);
		return (-EIO);
	}
	return 0;
}


/* Move data towards start of buffer */
static void move_buffer_data(ST_buffer * st_bp, int offset)
{
	int src_seg, dst_seg, src_offset = 0, dst_offset;
	int count, total;

	if (offset == 0)
		return;

	total=st_bp->buffer_bytes - offset;
	for (src_seg=0; src_seg < st_bp->sg_segs; src_seg++) {
		src_offset = offset;
		if (src_offset < st_bp->sg_lengths[src_seg])
			break;
		offset -= st_bp->sg_lengths[src_seg];
	}
	if (src_seg == st_bp->sg_segs) {	/* Should never happen */
		printk(KERN_WARNING "st: zap_buffer offset overflow.\n");
		return;
	}

	st_bp->buffer_bytes = st_bp->read_pointer = total;
	for (dst_seg=dst_offset=0; total > 0; ) {
		count = min(st_bp->sg_lengths[dst_seg] - dst_offset,
			    st_bp->sg_lengths[src_seg] - src_offset);
		memmove(st_bp->sg[dst_seg].address + dst_offset,
			st_bp->sg[src_seg].address + src_offset, count);
		printk("st: move (%d,%d) -> (%d,%d) count %d\n",
		       src_seg, src_offset, dst_seg, dst_offset, count);
		src_offset += count;
		if (src_offset >= st_bp->sg_lengths[src_seg]) {
			src_seg++;
			src_offset = 0;
		}
		dst_offset += count;
		if (dst_offset >= st_bp->sg_lengths[dst_seg]) {
			dst_seg++;
			dst_offset = 0;
		}
		total -= count;
	}
}


/* Set the scatter/gather list length fields to sum up to the transfer length.
   Return the number of segments being used. */
static int set_sg_lengths(ST_buffer *st_bp, unsigned int length)
{
	int i;

	for (i=0; i < st_bp->sg_segs; i++) {
		if (length > st_bp->sg_lengths[i])
			st_bp->sg[i].length = st_bp->sg_lengths[i];
		else {
			st_bp->sg[i].length = length;
			break;
		}
		length -= st_bp->sg_lengths[i];
	}
	return i + 1;
}


/* Validate the options from command line or module parameters */
static void validate_options(void)
{
	if (buffer_kbs > 0)
		st_buffer_size = buffer_kbs * ST_KILOBYTE;
	if (max_buffers >= 0)
		st_max_buffers = max_buffers;
	if (max_sg_segs >= ST_FIRST_SG)
		st_max_sg_segs = max_sg_segs;
}

#ifndef MODULE
/* Set the boot options. Syntax is defined in README.st.
 */
static int __init st_setup(char *str)
{
	int i, len, ints[5];
	char *stp;

	stp = get_options(str, ARRAY_SIZE(ints), ints);

	if (ints[0] > 0) {
		for (i = 0; i < ints[0] && i < ARRAY_SIZE(parms); i++)
			if (parms[i].val)
				*parms[i].val = ints[i + 1];
	} else {
		while (stp != NULL) {
			for (i = 0; i < ARRAY_SIZE(parms); i++) {
				len = strlen(parms[i].name);
				if (!strncmp(stp, parms[i].name, len) &&
				    (*(stp + len) == ':' || *(stp + len) == '=') &&
				    parms[i].val) {
					*parms[i].val =
                                                simple_strtoul(stp + len + 1, NULL, 0);
					break;
				}
			}
			if (i >= sizeof(parms) / sizeof(struct st_dev_parm))
				 printk(KERN_WARNING "st: illegal parameter in '%s'\n",
					stp);
			stp = strchr(stp, ',');
			if (stp)
				stp++;
		}
	}

	validate_options();

	return 1;
}

__setup("st=", st_setup);

#endif


static struct file_operations st_fops =
{
	owner:		THIS_MODULE,
	read:		st_read,
	write:		st_write,
	ioctl:		st_ioctl,
	open:		st_open,
	flush:		st_flush,
	release:	st_release,
};

static int st_attach(Scsi_Device * SDp)
{
	Scsi_Tape *tpnt;
	ST_mode *STm;
	ST_partstat *STps;
	int i, mode, target_nbr, dev_num;
	unsigned long flags = 0;
	char *stp;

	if (SDp->type != TYPE_TAPE)
		return 1;
	if ((stp = st_incompatible(SDp))) {
		printk(KERN_INFO
		       "st: Found incompatible tape at scsi%d, channel %d, id %d, lun %d\n",
		       SDp->host->host_no, SDp->channel, SDp->id, SDp->lun);
		printk(KERN_INFO "st: The suggested driver is %s.\n", stp);
		return 1;
	}

	write_lock_irqsave(&st_dev_arr_lock, flags);
	if (st_template.nr_dev >= st_template.dev_max) {
		Scsi_Tape **tmp_da;
		ST_buffer **tmp_ba;
		int tmp_dev_max;

		tmp_dev_max = st_template.nr_dev + ST_DEV_ARR_LUMP;
		if (tmp_dev_max > ST_MAX_TAPES)
			tmp_dev_max = ST_MAX_TAPES;
		if (tmp_dev_max <= st_template.nr_dev) {
			SDp->attached--;
			write_unlock_irqrestore(&st_dev_arr_lock, flags);
			printk(KERN_ERR "st: Too many tape devices (max. %d).\n",
			       ST_MAX_TAPES);
			return 1;
		}

		tmp_da = kmalloc(tmp_dev_max * sizeof(Scsi_Tape *), GFP_ATOMIC);
		tmp_ba = kmalloc(tmp_dev_max * sizeof(ST_buffer *), GFP_ATOMIC);
		if (tmp_da == NULL || tmp_ba == NULL) {
			if (tmp_da != NULL)
				kfree(tmp_da);
			if (tmp_ba != NULL)
				kfree(tmp_ba);
			SDp->attached--;
			write_unlock_irqrestore(&st_dev_arr_lock, flags);
			printk(KERN_ERR "st: Can't extend device array.\n");
			return 1;
		}

		memset(tmp_da, 0, tmp_dev_max * sizeof(Scsi_Tape *));
		if (scsi_tapes != NULL) {
			memcpy(tmp_da, scsi_tapes,
			       st_template.dev_max * sizeof(Scsi_Tape *));
			kfree(scsi_tapes);
		}
		scsi_tapes = tmp_da;

		memset(tmp_ba, 0, tmp_dev_max * sizeof(ST_buffer *));
		if (st_buffers != NULL) {
			memcpy(tmp_ba, st_buffers,
			       st_template.dev_max * sizeof(ST_buffer *));
			kfree(st_buffers);
		}
		st_buffers = tmp_ba;

		st_template.dev_max = tmp_dev_max;
	}

	for (i = 0; i < st_template.dev_max; i++)
		if (scsi_tapes[i] == NULL)
			break;
	if (i >= st_template.dev_max)
		panic("scsi_devices corrupt (st)");

	tpnt = kmalloc(sizeof(Scsi_Tape), GFP_ATOMIC);
	if (tpnt == NULL) {
		SDp->attached--;
		write_unlock_irqrestore(&st_dev_arr_lock, flags);
		printk(KERN_ERR "st: Can't allocate device descriptor.\n");
		return 1;
	}
	memset(tpnt, 0, sizeof(Scsi_Tape));
	scsi_tapes[i] = tpnt;
	dev_num = i;

	for (mode = 0; mode < ST_NBR_MODES; ++mode) {
	    char name[8];
	    static char *formats[ST_NBR_MODES] ={"", "l", "m", "a"};

	    /*  Rewind entry  */
	    sprintf (name, "mt%s", formats[mode]);
	    tpnt->de_r[mode] =
		devfs_register (SDp->de, name, DEVFS_FL_DEFAULT,
				MAJOR_NR, i + (mode << 5),
				S_IFCHR | S_IRUGO | S_IWUGO,
				&st_fops, NULL);
	    /*  No-rewind entry  */
	    sprintf (name, "mt%sn", formats[mode]);
	    tpnt->de_n[mode] =
		devfs_register (SDp->de, name, DEVFS_FL_DEFAULT,
				MAJOR_NR, i + (mode << 5) + 128,
				S_IFCHR | S_IRUGO | S_IWUGO,
				&st_fops, NULL);
	}
	devfs_register_tape (tpnt->de_r[0]);
	tpnt->device = SDp;
	if (SDp->scsi_level <= 2)
		tpnt->tape_type = MT_ISSCSI1;
	else
		tpnt->tape_type = MT_ISSCSI2;

        tpnt->inited = 0;
	tpnt->devt = MKDEV(SCSI_TAPE_MAJOR, i);
	tpnt->dirty = 0;
	tpnt->in_use = 0;
	tpnt->drv_buffer = 1;	/* Try buffering if no mode sense */
	tpnt->restr_dma = (SDp->host)->unchecked_isa_dma;
	tpnt->use_pf = (SDp->scsi_level >= SCSI_2);
	tpnt->density = 0;
	tpnt->do_auto_lock = ST_AUTO_LOCK;
	tpnt->can_bsr = ST_IN_FILE_POS;
	tpnt->can_partitions = 0;
	tpnt->two_fm = ST_TWO_FM;
	tpnt->fast_mteom = ST_FAST_MTEOM;
	tpnt->scsi2_logical = ST_SCSI2LOGICAL;
	tpnt->immediate = ST_NOWAIT;
	tpnt->default_drvbuffer = 0xff;		/* No forced buffering */
	tpnt->partition = 0;
	tpnt->new_partition = 0;
	tpnt->nbr_partitions = 0;
	tpnt->timeout = ST_TIMEOUT;
	tpnt->long_timeout = ST_LONG_TIMEOUT;

	for (i = 0; i < ST_NBR_MODES; i++) {
		STm = &(tpnt->modes[i]);
		STm->defined = FALSE;
		STm->sysv = ST_SYSV;
		STm->defaults_for_writes = 0;
		STm->do_async_writes = ST_ASYNC_WRITES;
		STm->do_buffer_writes = ST_BUFFER_WRITES;
		STm->do_read_ahead = ST_READ_AHEAD;
		STm->default_compression = ST_DONT_TOUCH;
		STm->default_blksize = (-1);	/* No forced size */
		STm->default_density = (-1);	/* No forced density */
	}

	for (i = 0; i < ST_NBR_PARTITIONS; i++) {
		STps = &(tpnt->ps[i]);
		STps->rw = ST_IDLE;
		STps->eof = ST_NOEOF;
		STps->at_sm = 0;
		STps->last_block_valid = FALSE;
		STps->drv_block = (-1);
		STps->drv_file = (-1);
	}

	tpnt->current_mode = 0;
	tpnt->modes[0].defined = TRUE;

	tpnt->density_changed = tpnt->compression_changed =
	    tpnt->blksize_changed = FALSE;
	init_MUTEX(&tpnt->lock);

	st_template.nr_dev++;
	write_unlock_irqrestore(&st_dev_arr_lock, flags);
	printk(KERN_WARNING
	"Attached scsi tape st%d at scsi%d, channel %d, id %d, lun %d\n",
	       dev_num, SDp->host->host_no, SDp->channel, SDp->id, SDp->lun);

	/* See if we need to allocate more static buffers */
	target_nbr = st_template.nr_dev;
	if (target_nbr > st_max_buffers)
		target_nbr = st_max_buffers;
	for (i=st_nbr_buffers; i < target_nbr; i++)
		if (!new_tape_buffer(TRUE, TRUE, FALSE)) {
			printk(KERN_INFO "st: Unable to allocate new static buffer.\n");
			break;
		}
	/* If the previous allocation fails, we will try again when the buffer is
	   really needed. */

	return 0;
};

static int st_detect(Scsi_Device * SDp)
{
	if (SDp->type != TYPE_TAPE || st_incompatible(SDp))
		return 0;
        st_template.dev_noticed++;
	return 1;
}

static int st_registered = 0;

/* Driver initialization (not __init because may be called later) */
static int st_init()
{
	unsigned long flags;

	if (st_template.dev_noticed == 0 || st_registered)
		return 0;

	printk(KERN_INFO
	       "st: Version %s, bufsize %d, max init. bufs %d, s/g segs %d\n",
	       verstr, st_buffer_size, st_max_buffers, st_max_sg_segs);

	write_lock_irqsave(&st_dev_arr_lock, flags);
	if (!st_registered) {
		if (devfs_register_chrdev(SCSI_TAPE_MAJOR, "st", &st_fops)) {
			write_unlock_irqrestore(&st_dev_arr_lock, flags);
			printk(KERN_ERR "Unable to get major %d for SCSI tapes\n",
                               MAJOR_NR);
			st_template.dev_noticed = 0;
			return 1;
		}
		st_registered++;
	}

	st_template.dev_max = 0;
	st_nbr_buffers = 0;
	write_unlock_irqrestore(&st_dev_arr_lock, flags);

	return 0;
}

static void st_detach(Scsi_Device * SDp)
{
	Scsi_Tape *tpnt;
	int i, mode;
	unsigned long flags;

	write_lock_irqsave(&st_dev_arr_lock, flags);
	for (i = 0; i < st_template.dev_max; i++) {
		tpnt = scsi_tapes[i];
		if (tpnt != NULL && tpnt->device == SDp) {
			tpnt->device = NULL;
			for (mode = 0; mode < ST_NBR_MODES; ++mode) {
				devfs_unregister (tpnt->de_r[mode]);
				tpnt->de_r[mode] = NULL;
				devfs_unregister (tpnt->de_n[mode]);
				tpnt->de_n[mode] = NULL;
			}
			kfree(tpnt);
			scsi_tapes[i] = 0;
			SDp->attached--;
			st_template.nr_dev--;
			st_template.dev_noticed--;
			write_unlock_irqrestore(&st_dev_arr_lock, flags);
			return;
		}
	}

	write_unlock_irqrestore(&st_dev_arr_lock, flags);
	return;
}


static int __init init_st(void)
{
	validate_options();

	st_template.module = THIS_MODULE;
        return scsi_register_module(MODULE_SCSI_DEV, &st_template);
}

static void __exit exit_st(void)
{
	int i;

	scsi_unregister_module(MODULE_SCSI_DEV, &st_template);
	devfs_unregister_chrdev(SCSI_TAPE_MAJOR, "st");
	st_registered--;
	if (scsi_tapes != NULL) {
		for (i=0; i < st_template.dev_max; ++i)
			if (scsi_tapes[i])
				kfree(scsi_tapes[i]);
		kfree(scsi_tapes);
		if (st_buffers != NULL) {
			for (i = 0; i < st_nbr_buffers; i++) {
				if (st_buffers[i] != NULL) {
					st_buffers[i]->orig_sg_segs = 0;
					normalize_buffer(st_buffers[i]);
					kfree(st_buffers[i]);
				}
			}	
			kfree(st_buffers);
		}
	}
	st_template.dev_max = 0;
	printk(KERN_INFO "st: Unloaded.\n");
}

module_init(init_st);
module_exit(exit_st);
