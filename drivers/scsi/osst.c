/*
  SCSI Tape Driver for Linux version 1.1 and newer. See the accompanying
  file README.st for more information.

  History:

  OnStream SCSI Tape support (osst) cloned from st.c by
  Willem Riede (osst@riede.org) Feb 2000
  Fixes ... Kurt Garloff <garloff@suse.de> Mar 2000

  Rewritten from Dwayne Forsyth's SCSI tape driver by Kai Makisara.
  Contribution and ideas from several people including (in alphabetical
  order) Klaus Ehrenfried, Wolfgang Denk, Steve Hirsch, Andreas Koppenh"ofer,
  Michael Leodolter, Eyal Lebedinsky, J"org Weule, and Eric Youngdale.

  Copyright 1992 - 2000 Kai Makisara
		 email Kai.Makisara@metla.fi

  $Header: /cvsroot/osst/Driver/osst.c,v 1.67.2.1 2003/06/28 00:21:28 riede Exp $

  Microscopic alterations - Rik Ling, 2000/12/21
  Last modified: Wed Feb  2 22:04:05 2000 by makisara@kai.makisara.local
  Some small formal changes - aeb, 950809
*/

static const char * cvsid = "$Id: osst.c,v 1.67.2.2 2003/12/14 14:28:46 wriede Exp $";
const char * osst_version = "0.9.14";

/* The "failure to reconnect" firmware bug */
#define OSST_FW_NEED_POLL_MIN 10601 /*(107A)*/
#define OSST_FW_NEED_POLL_MAX 10704 /*(108D)*/
#define OSST_FW_NEED_POLL(x,d) ((x) >= OSST_FW_NEED_POLL_MIN && (x) <= OSST_FW_NEED_POLL_MAX && d->host->this_id != 7)

#include <linux/module.h>

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/mtio.h>
#include <linux/ioctl.h>
#include <linux/fcntl.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <asm/uaccess.h>
#include <asm/dma.h>
#include <asm/system.h>

/* The driver prints some debugging information on the console if DEBUG
   is defined and non-zero. */
#define DEBUG 0

/* The message level for the debug messages is currently set to KERN_NOTICE
   so that people can easily see the messages. Later when the debugging messages
   in the drivers are more widely classified, this may be changed to KERN_DEBUG. */
#define OSST_DEB_MSG  KERN_NOTICE

#define MAJOR_NR OSST_MAJOR
#include <linux/blk.h>

#include "scsi.h"
#include "hosts.h"
#include <scsi/scsi_ioctl.h>

#define ST_KILOBYTE 1024

#include "st.h"
#include "osst.h"
#include "osst_options.h"
#include "osst_detect.h"

#include "constants.h"

static int max_dev = 0;
static int write_threshold_kbs = 0;
static int max_sg_segs = 0;

#ifdef MODULE
MODULE_AUTHOR("Willem Riede");
MODULE_DESCRIPTION("OnStream SCSI Tape Driver");
MODULE_LICENSE("GPL");

MODULE_PARM(max_dev, "i");
MODULE_PARM(write_threshold_kbs, "i");
MODULE_PARM(max_sg_segs, "i");
#else
static struct osst_dev_parm {
       char   *name;
       int    *val;
} parms[] __initdata = {
       { "max_dev",             &max_dev             },
       { "write_threshold_kbs", &write_threshold_kbs },
       { "max_sg_segs",         &max_sg_segs         }
       };
#endif

/* Some default definitions have been moved to osst_options.h */
#define OSST_BUFFER_SIZE (OSST_BUFFER_BLOCKS * ST_KILOBYTE)
#define OSST_WRITE_THRESHOLD (OSST_WRITE_THRESHOLD_BLOCKS * ST_KILOBYTE)

/* The buffer size should fit into the 24 bits for length in the
   6-byte SCSI read and write commands. */
#if OSST_BUFFER_SIZE >= (2 << 24 - 1)
#error "Buffer size should not exceed (2 << 24 - 1) bytes!"
#endif

#if DEBUG
static int debugging = 1;
/* uncomment define below to test error recovery */
// #define OSST_INJECT_ERRORS 1 
#endif

#define MAX_RETRIES 2
#define MAX_READ_RETRIES 0
#define MAX_WRITE_RETRIES 0
#define MAX_READY_RETRIES 0
#define NO_TAPE  NOT_READY

#define OSST_WAIT_POSITION_COMPLETE   (HZ > 200 ? HZ / 200 : 1)
#define OSST_WAIT_WRITE_COMPLETE      (HZ / 12)
#define OSST_WAIT_LONG_WRITE_COMPLETE (HZ / 2)

#define OSST_TIMEOUT (200 * HZ)
#define OSST_LONG_TIMEOUT (1800 * HZ)

#define TAPE_NR(x) (MINOR(x) & ~(128 | ST_MODE_MASK))
#define TAPE_MODE(x) ((MINOR(x) & ST_MODE_MASK) >> ST_MODE_SHIFT)

/* Internal ioctl to set both density (uppermost 8 bits) and blocksize (lower
   24 bits) */
#define SET_DENS_AND_BLK 0x10001

static int osst_nbr_buffers;
static int osst_buffer_size       = OSST_BUFFER_SIZE;
static int osst_write_threshold   = OSST_WRITE_THRESHOLD;
static int osst_max_buffers       = OSST_MAX_BUFFERS;
static int osst_max_sg_segs       = OSST_MAX_SG;
static int osst_max_dev           = OSST_MAX_TAPES;

static OS_Scsi_Tape **os_scsi_tapes = NULL;
static OSST_buffer  **osst_buffers  = NULL;

static int modes_defined = FALSE;

static OSST_buffer *new_tape_buffer(int, int);
static int enlarge_buffer(OSST_buffer *, int, int);
static void normalize_buffer(OSST_buffer *);
static int append_to_buffer(const char *, OSST_buffer *, int);
static int from_buffer(OSST_buffer *, char *, int);
static int osst_zero_buffer_tail(OSST_buffer *);
static int osst_copy_to_buffer(OSST_buffer *, unsigned char *);
static int osst_copy_from_buffer(OSST_buffer *, unsigned char *);

static int osst_init(void);
static int osst_attach(Scsi_Device *);
static int osst_detect(Scsi_Device *);
static void osst_detach(Scsi_Device *);

struct Scsi_Device_Template osst_template =
{
       name:		"OnStream tape",
       tag:		"osst",
       scsi_type:	TYPE_TAPE,
       major:		OSST_MAJOR,
       detect:		osst_detect,
       init:		osst_init,
       attach:		osst_attach,
       detach:		osst_detach
};

static int osst_int_ioctl(OS_Scsi_Tape *STp, Scsi_Request ** aSRpnt, unsigned int cmd_in,unsigned long arg);

static int osst_set_frame_position(OS_Scsi_Tape *STp, Scsi_Request ** aSRpnt, int frame, int skip);

static int osst_get_frame_position(OS_Scsi_Tape *STp, Scsi_Request ** aSRpnt);

static int osst_flush_write_buffer(OS_Scsi_Tape *STp, Scsi_Request ** aSRpnt);

static int osst_write_error_recovery(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt, int pending);


/* Routines that handle the interaction with mid-layer SCSI routines */

/* Convert the result to success code */
static int osst_chk_result(OS_Scsi_Tape * STp, Scsi_Request * SRpnt)
{
	int dev = TAPE_NR(STp->devt);
	int result = SRpnt->sr_result;
	unsigned char * sense = SRpnt->sr_sense_buffer, scode;
#if DEBUG
	const char *stp;
#endif

	if (!result) {
		sense[0] = 0;    /* We don't have sense data if this byte is zero */
		return 0;
	}
	if (driver_byte(result) & DRIVER_SENSE)
		scode = sense[2] & 0x0f;
	else {
		sense[0] = 0;    /* We don't have sense data if this byte is zero */
		scode = 0;
	}

#if DEBUG
	if (debugging) {
		printk(OSST_DEB_MSG "osst%d:D: Error: %x, cmd: %x %x %x %x %x %x Len: %d\n",
		   dev, result,
		   SRpnt->sr_cmnd[0], SRpnt->sr_cmnd[1], SRpnt->sr_cmnd[2],
		   SRpnt->sr_cmnd[3], SRpnt->sr_cmnd[4], SRpnt->sr_cmnd[5],
		   SRpnt->sr_bufflen);
		if (driver_byte(result) & DRIVER_SENSE)
			print_req_sense("osst", SRpnt);
	}
	else
#endif
	if (!(driver_byte(result) & DRIVER_SENSE) ||
		((sense[0] & 0x70) == 0x70 &&
		 scode != NO_SENSE &&
		 scode != RECOVERED_ERROR &&
/*      	 scode != UNIT_ATTENTION && */
		 scode != BLANK_CHECK &&
		 scode != VOLUME_OVERFLOW &&
		 SRpnt->sr_cmnd[0] != MODE_SENSE &&
		 SRpnt->sr_cmnd[0] != TEST_UNIT_READY)) { /* Abnormal conditions for tape */
		if (driver_byte(result) & DRIVER_SENSE) {
			printk(KERN_WARNING "osst%d:W: Command with sense data: ", dev);
			print_req_sense("osst:", SRpnt);
		}
		else {
			static	int	notyetprinted = 1;

			printk(KERN_WARNING
			     "osst%d:W: Warning %x (sugg. bt 0x%x, driver bt 0x%x, host bt 0x%x).\n",
			     dev, result, suggestion(result), driver_byte(result) & DRIVER_MASK,
			     host_byte(result));
			if (notyetprinted) {
				notyetprinted = 0;
				printk(KERN_INFO
					"osst%d:I: This warning may be caused by your scsi controller,\n", dev);
				printk(KERN_INFO
					"osst%d:I: it has been reported with some Buslogic cards.\n", dev);
			}
		}
	}
	if ((sense[0] & 0x70) == 0x70 &&
	     scode == RECOVERED_ERROR) {
		STp->recover_count++;
		STp->recover_erreg++;
#if DEBUG
		if (debugging) {
			if (SRpnt->sr_cmnd[0] == READ_6)
				stp = "read";
			else if (SRpnt->sr_cmnd[0] == WRITE_6)
				stp = "write";
			else
				stp = "ioctl";
			printk(OSST_DEB_MSG "osst%d:D: Recovered %s error (%d).\n", dev, stp,
					     os_scsi_tapes[dev]->recover_count);
		}
#endif
		if ((sense[2] & 0xe0) == 0)
			return 0;
	}
	return (-EIO);
}


/* Wakeup from interrupt */
static void osst_sleep_done (Scsi_Cmnd * SCpnt)
{
	unsigned int dev = TAPE_NR(SCpnt->request.rq_dev);
	OS_Scsi_Tape * STp;

	if (os_scsi_tapes && (STp = os_scsi_tapes[dev])) {
		if ((STp->buffer)->writing &&
		    (SCpnt->sense_buffer[0] & 0x70) == 0x70 &&
		    (SCpnt->sense_buffer[2] & 0x40)) {
			/* EOM at write-behind, has all been written? */
			if ((SCpnt->sense_buffer[2] & 0x0f) == VOLUME_OVERFLOW)
				(STp->buffer)->midlevel_result = SCpnt->result; /* Error */
			else
				(STp->buffer)->midlevel_result = INT_MAX;       /* OK */
		}
		else
			(STp->buffer)->midlevel_result = SCpnt->result;
		SCpnt->request.rq_status = RQ_SCSI_DONE;
		(STp->buffer)->last_SRpnt = SCpnt->sc_request;

#if DEBUG
		STp->write_pending = 0;
#endif
		complete(SCpnt->request.waiting);
	}
#if DEBUG
	else if (debugging)
		printk(OSST_DEB_MSG "osst?:D: Illegal interrupt device %x\n", dev);
#endif
}


/* Do the scsi command. Waits until command performed if do_wait is true.
   Otherwise osst_write_behind_check() is used to check that the command
   has finished. */
static	Scsi_Request * osst_do_scsi(Scsi_Request *SRpnt, OS_Scsi_Tape *STp, 
	unsigned char *cmd, int bytes, int direction, int timeout, int retries, int do_wait)
{
	unsigned char *bp;
#ifdef OSST_INJECT_ERRORS
	static   int   inject = 0;
	static   int   repeat = 0;
#endif
	if (SRpnt == NULL) {
		if ((SRpnt = scsi_allocate_request(STp->device)) == NULL) {
			printk(KERN_ERR "osst%d:E: Can't get SCSI request.\n", TAPE_NR(STp->devt));
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
	SRpnt->sr_use_sg = (bytes > (STp->buffer)->sg[0].length) ?
				    (STp->buffer)->use_sg : 0;
	if (SRpnt->sr_use_sg) {
		bp = (char *)&(STp->buffer->sg[0]);
		if (STp->buffer->sg_segs < SRpnt->sr_use_sg)
			SRpnt->sr_use_sg = STp->buffer->sg_segs;
	}
	else
		bp = (STp->buffer)->b_data;
	SRpnt->sr_data_direction = direction;
	SRpnt->sr_cmd_len = 0;
	SRpnt->sr_request.waiting = &(STp->wait);
	SRpnt->sr_request.rq_status = RQ_SCSI_BUSY;
	SRpnt->sr_request.rq_dev = STp->devt;

	scsi_do_req(SRpnt, (void *)cmd, bp, bytes, osst_sleep_done, timeout, retries);

	if (do_wait) {
		wait_for_completion(SRpnt->sr_request.waiting);
		SRpnt->sr_request.waiting = NULL;
		STp->buffer->syscall_result = osst_chk_result(STp, SRpnt);
#ifdef OSST_INJECT_ERRORS
		if (STp->buffer->syscall_result == 0 &&
		    cmd[0] == READ_6 &&
		    cmd[4] && 
		    ( (++ inject % 83) == 29  ||
		      (STp->first_frame_position == 240 
			         /* or STp->read_error_frame to fail again on the block calculated above */ &&
				 ++repeat < 3))) {
			printk(OSST_DEB_MSG "osst%d:D: Injecting read error\n", TAPE_NR(STp->devt));
			STp->buffer->last_result_fatal = 1;
		}
#endif
	}
	return SRpnt;
}


/* Handle the write-behind checking (downs the semaphore) */
static void osst_write_behind_check(OS_Scsi_Tape *STp)
{
	OSST_buffer * STbuffer;

	STbuffer = STp->buffer;

#if DEBUG
	if (STp->write_pending)
		STp->nbr_waits++;
	else
		STp->nbr_finished++;
#endif
	wait_for_completion(&(STp->wait));
	(STp->buffer)->last_SRpnt->sr_request.waiting = NULL;

	STp->buffer->syscall_result = osst_chk_result(STp, STp->buffer->last_SRpnt);

	if ((STp->buffer)->syscall_result)
		(STp->buffer)->syscall_result =
			osst_write_error_recovery(STp, &((STp->buffer)->last_SRpnt), 1);
	else
		STp->first_frame_position++;

	scsi_release_request((STp->buffer)->last_SRpnt);

	if (STbuffer->writing < STbuffer->buffer_bytes)
		printk(KERN_WARNING "osst:A: write_behind_check: something left in buffer!\n");

	STbuffer->buffer_bytes -= STbuffer->writing;
	STbuffer->writing = 0;

	return;
}



/* Onstream specific Routines */
/*
 * Initialize the OnStream AUX
 */
static void osst_init_aux(OS_Scsi_Tape * STp, int frame_type, int frame_seq_number,
					 int logical_blk_num, int blk_sz, int blk_cnt)
{
	os_aux_t       *aux = STp->buffer->aux;
	os_partition_t *par = &aux->partition;
	os_dat_t       *dat = &aux->dat;

	if (STp->raw) return;

	memset(aux, 0, sizeof(*aux));
	aux->format_id = htonl(0);
	memcpy(aux->application_sig, "LIN4", 4);
	aux->hdwr = htonl(0);
	aux->frame_type = frame_type;

	switch (frame_type) {
	  case	OS_FRAME_TYPE_HEADER:
		aux->update_frame_cntr    = htonl(STp->update_frame_cntr);
		par->partition_num        = OS_CONFIG_PARTITION;
		par->par_desc_ver         = OS_PARTITION_VERSION;
		par->wrt_pass_cntr        = htons(0xffff);
		/* 0-4 = reserved, 5-9 = header, 2990-2994 = header, 2995-2999 = reserved */
		par->first_frame_ppos     = htonl(0);
		par->last_frame_ppos      = htonl(0xbb7);
		aux->frame_seq_num        = htonl(0);
		aux->logical_blk_num_high = htonl(0);
		aux->logical_blk_num      = htonl(0);
		aux->next_mark_ppos       = htonl(STp->first_mark_ppos);
		break;
	  case	OS_FRAME_TYPE_DATA:
	  case	OS_FRAME_TYPE_MARKER:
		dat->dat_sz = 8;
		dat->reserved1 = 0;
		dat->entry_cnt = 1;
		dat->reserved3 = 0;
		dat->dat_list[0].blk_sz   = htonl(blk_sz);
		dat->dat_list[0].blk_cnt  = htons(blk_cnt);
		dat->dat_list[0].flags    = frame_type==OS_FRAME_TYPE_MARKER?
							OS_DAT_FLAGS_MARK:OS_DAT_FLAGS_DATA;
		dat->dat_list[0].reserved = 0;
	  case	OS_FRAME_TYPE_EOD:
		aux->update_frame_cntr    = htonl(0);
		par->partition_num        = OS_DATA_PARTITION;
		par->par_desc_ver         = OS_PARTITION_VERSION;
		par->wrt_pass_cntr        = htons(STp->wrt_pass_cntr);
		par->first_frame_ppos     = htonl(STp->first_data_ppos);
		par->last_frame_ppos      = htonl(STp->capacity);
		aux->frame_seq_num        = htonl(frame_seq_number);
		aux->logical_blk_num_high = htonl(0);
		aux->logical_blk_num      = htonl(logical_blk_num);
		break;
	  default: ; /* probably FILL */
	}
	aux->filemark_cnt = ntohl(STp->filemark_cnt);
	aux->phys_fm = ntohl(0xffffffff);
	aux->last_mark_ppos = ntohl(STp->last_mark_ppos);
	aux->last_mark_lbn  = ntohl(STp->last_mark_lbn);
}

/*
 * Verify that we have the correct tape frame
 */
static int osst_verify_frame(OS_Scsi_Tape * STp, int frame_seq_number, int quiet)
{
	os_aux_t       * aux  = STp->buffer->aux;
	os_partition_t * par  = &(aux->partition);
	ST_partstat    * STps = &(STp->ps[STp->partition]);
	int		 dev  = TAPE_NR(STp->devt);
	int		 blk_cnt, blk_sz, i;

	if (STp->raw) {
		if (STp->buffer->syscall_result) {
			for (i=0; i < STp->buffer->sg_segs; i++)
				memset(STp->buffer->sg[i].address, 0, STp->buffer->sg[i].length);
			strcpy(STp->buffer->b_data, "READ ERROR ON FRAME");
                } else
			STp->buffer->buffer_bytes = OS_FRAME_SIZE;
		return 1;
	}
	if (STp->buffer->syscall_result) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Skipping frame, read error\n", dev);
#endif
		return 0;
	}
	if (ntohl(aux->format_id) != 0) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Skipping frame, format_id %u\n", dev, ntohl(aux->format_id));
#endif
		goto err_out;
	}
	if (memcmp(aux->application_sig, STp->application_sig, 4) != 0 &&
	    (memcmp(aux->application_sig, "LIN3", 4) != 0 || STp->linux_media_version != 4)) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Skipping frame, incorrect application signature\n", dev);
#endif
		goto err_out;
	}
	if (par->partition_num != OS_DATA_PARTITION) {
		if (!STp->linux_media || STp->linux_media_version != 2) {
#if DEBUG
			printk(OSST_DEB_MSG "osst%d:D: Skipping frame, partition num %d\n",
					    dev, par->partition_num);
#endif
			goto err_out;
		}
	}
	if (par->par_desc_ver != OS_PARTITION_VERSION) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Skipping frame, partition version %d\n", dev, par->par_desc_ver);
#endif
		goto err_out;
	}
	if (ntohs(par->wrt_pass_cntr) != STp->wrt_pass_cntr) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Skipping frame, wrt_pass_cntr %d (expected %d)\n", 
				    dev, ntohs(par->wrt_pass_cntr), STp->wrt_pass_cntr);
#endif
		goto err_out;
	}
	if (aux->frame_type != OS_FRAME_TYPE_DATA &&
	    aux->frame_type != OS_FRAME_TYPE_EOD &&
	    aux->frame_type != OS_FRAME_TYPE_MARKER) {
		if (!quiet)
#if DEBUG
			printk(OSST_DEB_MSG "osst%d:D: Skipping frame, frame type %x\n", dev, aux->frame_type);
#endif
		goto err_out;
	}
	if (aux->frame_type == OS_FRAME_TYPE_EOD &&
	    STp->first_frame_position < STp->eod_frame_ppos) {
		printk(KERN_INFO "osst%d:I: Skipping premature EOD frame %d\n", dev,
				 STp->first_frame_position);
		goto err_out;
	}
        if (frame_seq_number != -1 && ntohl(aux->frame_seq_num) != frame_seq_number) {
		if (!quiet)
#if DEBUG
			printk(OSST_DEB_MSG "osst%d:D: Skipping frame, sequence number %u (expected %d)\n", 
					    dev, ntohl(aux->frame_seq_num), frame_seq_number);
#endif
		goto err_out;
	}
	if (aux->frame_type == OS_FRAME_TYPE_MARKER) {
		STps->eof = ST_FM_HIT;

		i = ntohl(aux->filemark_cnt);
		if (STp->header_cache != NULL && i < OS_FM_TAB_MAX && (i > STp->filemark_cnt ||
		    STp->first_frame_position - 1 != ntohl(STp->header_cache->dat_fm_tab.fm_tab_ent[i]))) {
#if DEBUG
			printk(OSST_DEB_MSG "osst%d:D: %s filemark %d at frame pos %d\n", dev,
				  STp->header_cache->dat_fm_tab.fm_tab_ent[i] == 0?"Learned":"Corrected",
				  i, STp->first_frame_position - 1);
#endif
			STp->header_cache->dat_fm_tab.fm_tab_ent[i] = htonl(STp->first_frame_position - 1);
			if (i >= STp->filemark_cnt)
				 STp->filemark_cnt = i+1;
		}
	}
	if (aux->frame_type == OS_FRAME_TYPE_EOD) {
		STps->eof = ST_EOD_1;
		STp->frame_in_buffer = 1;
	}
	if (aux->frame_type == OS_FRAME_TYPE_DATA) {
                blk_cnt = ntohs(aux->dat.dat_list[0].blk_cnt);
		blk_sz  = ntohl(aux->dat.dat_list[0].blk_sz);
		STp->buffer->buffer_bytes = blk_cnt * blk_sz;
		STp->buffer->read_pointer = 0;
		STp->frame_in_buffer = 1;

		/* See what block size was used to write file */
		if (STp->block_size != blk_sz && blk_sz > 0) {
			printk(KERN_INFO
	    	"osst%d:I: File was written with block size %d%c, currently %d%c, adjusted to match.\n",
       				dev, blk_sz<1024?blk_sz:blk_sz/1024,blk_sz<1024?'b':'k',
				STp->block_size<1024?STp->block_size:STp->block_size/1024,
				STp->block_size<1024?'b':'k');
			STp->block_size            = blk_sz;
			STp->buffer->buffer_blocks = OS_DATA_SIZE / blk_sz;
		}
		STps->eof = ST_NOEOF;
	}
        STp->frame_seq_number = ntohl(aux->frame_seq_num);
	STp->logical_blk_num  = ntohl(aux->logical_blk_num);
	return 1;

err_out:
	if (STp->read_error_frame == 0)
		STp->read_error_frame = STp->first_frame_position - 1;
	return 0;
}

/*
 * Wait for the unit to become Ready
 */
static int osst_wait_ready(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt, unsigned timeout, int initial_delay)
{
	unsigned char	cmd[MAX_COMMAND_SIZE];
	Scsi_Request  * SRpnt;
	long		startwait = jiffies;
#if DEBUG
	int		dbg = debugging;
	int		dev = TAPE_NR(STp->devt);

	printk(OSST_DEB_MSG "osst%d:D: Reached onstream wait ready\n", dev);
#endif

	if (initial_delay > 0) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(initial_delay);
	}

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = TEST_UNIT_READY;

	SRpnt = osst_do_scsi(*aSRpnt, STp, cmd, 0, SCSI_DATA_NONE, STp->timeout, MAX_READY_RETRIES, TRUE);
	*aSRpnt = SRpnt;
	if (!SRpnt) return (-EBUSY);

	while ( STp->buffer->syscall_result && time_before(jiffies, startwait + timeout*HZ) &&
	       (( SRpnt->sr_sense_buffer[2]  == 2 && SRpnt->sr_sense_buffer[12] == 4    &&
		 (SRpnt->sr_sense_buffer[13] == 1 || SRpnt->sr_sense_buffer[13] == 8)    ) ||
		( SRpnt->sr_sense_buffer[2]  == 6 && SRpnt->sr_sense_buffer[12] == 0x28 &&
		  SRpnt->sr_sense_buffer[13] == 0                                        )  )) {
#if DEBUG
	    if (debugging) {
		printk(OSST_DEB_MSG "osst%d:D: Sleeping in onstream wait ready\n", dev);
		printk(OSST_DEB_MSG "osst%d:D: Turning off debugging for a while\n", dev);
		debugging = 0;
	    }
#endif
	    set_current_state(TASK_INTERRUPTIBLE);
	    schedule_timeout(HZ / 10);

	    memset(cmd, 0, MAX_COMMAND_SIZE);
	    cmd[0] = TEST_UNIT_READY;

	    SRpnt = osst_do_scsi(SRpnt, STp, cmd, 0, SCSI_DATA_NONE, STp->timeout, MAX_READY_RETRIES, TRUE);
	}
	*aSRpnt = SRpnt;
#if DEBUG
	debugging = dbg;
#endif
	if ( STp->buffer->syscall_result &&
	     osst_write_error_recovery(STp, aSRpnt, 0) ) {
#if DEBUG
	    printk(OSST_DEB_MSG "osst%d:D: Abnormal exit from onstream wait ready\n", dev);
	    printk(OSST_DEB_MSG "osst%d:D: Result = %d, Sense: 0=%02x, 2=%02x, 12=%02x, 13=%02x\n", dev,
			STp->buffer->syscall_result, SRpnt->sr_sense_buffer[0], SRpnt->sr_sense_buffer[2],
			SRpnt->sr_sense_buffer[12], SRpnt->sr_sense_buffer[13]);
#endif
	    return (-EIO);
	}
#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: Normal exit from onstream wait ready\n", dev);
#endif
	return 0;
}

/*
 * Wait for a tape to be inserted in the unit
 */
static int osst_wait_for_medium(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt, unsigned timeout)
{
	unsigned char	cmd[MAX_COMMAND_SIZE];
	Scsi_Request  * SRpnt;
	long		startwait = jiffies;
#if DEBUG
	int		dbg = debugging;
	int		dev  = TAPE_NR(STp->devt);

	printk(OSST_DEB_MSG "osst%d:D: Reached onstream wait for medium\n", dev);
#endif

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = TEST_UNIT_READY;

	SRpnt = osst_do_scsi(*aSRpnt, STp, cmd, 0, SCSI_DATA_NONE, STp->timeout, MAX_READY_RETRIES, TRUE);
	*aSRpnt = SRpnt;
	if (!SRpnt) return (-EBUSY);

	while ( STp->buffer->syscall_result && time_before(jiffies, startwait + timeout*HZ) &&
		SRpnt->sr_sense_buffer[2]  == 2 && SRpnt->sr_sense_buffer[12] == 0x3a       &&
	        SRpnt->sr_sense_buffer[13] == 0                                             ) {
#if DEBUG
	    if (debugging) {
		printk(OSST_DEB_MSG "osst%d:D: Sleeping in onstream wait medium\n", dev);
		printk(OSST_DEB_MSG "osst%d:D: Turning off debugging for a while\n", dev);
		debugging = 0;
	    }
#endif
	    set_current_state(TASK_INTERRUPTIBLE);
	    schedule_timeout(HZ / 10);

	    memset(cmd, 0, MAX_COMMAND_SIZE);
	    cmd[0] = TEST_UNIT_READY;

	    SRpnt = osst_do_scsi(SRpnt, STp, cmd, 0, SCSI_DATA_NONE, STp->timeout, MAX_READY_RETRIES, TRUE);
	}
	*aSRpnt = SRpnt;
#if DEBUG
	debugging = dbg;
#endif
	if ( STp->buffer->syscall_result     && SRpnt->sr_sense_buffer[2]  != 2 &&
	     SRpnt->sr_sense_buffer[12] != 4 && SRpnt->sr_sense_buffer[13] == 1) {
#if DEBUG
	    printk(OSST_DEB_MSG "osst%d:D: Abnormal exit from onstream wait medium\n", dev);
	    printk(OSST_DEB_MSG "osst%d:D: Result = %d, Sense: 0=%02x, 2=%02x, 12=%02x, 13=%02x\n", dev,
			STp->buffer->syscall_result, SRpnt->sr_sense_buffer[0], SRpnt->sr_sense_buffer[2],
			SRpnt->sr_sense_buffer[12], SRpnt->sr_sense_buffer[13]);
#endif
	    return 0;
	}
#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: Normal exit from onstream wait medium\n", dev);
#endif
	return 1;
}

static int osst_position_tape_and_confirm(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt, int frame)
{
	int	retval;

	osst_wait_ready(STp, aSRpnt, 15 * 60, 0);			/* TODO - can this catch a write error? */
	retval = osst_set_frame_position(STp, aSRpnt, frame, 0);
	if (retval) return (retval);
	osst_wait_ready(STp, aSRpnt, 15 * 60, OSST_WAIT_POSITION_COMPLETE);
	return (osst_get_frame_position(STp, aSRpnt));
}

/*
 * Wait for write(s) to complete
 */
static int osst_flush_drive_buffer(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt)
{
	unsigned char	cmd[MAX_COMMAND_SIZE];
	Scsi_Request  * SRpnt;

	int             result = 0;
	int		delay  = OSST_WAIT_WRITE_COMPLETE;
#if DEBUG
	int		dev  = TAPE_NR(STp->devt);

	printk(OSST_DEB_MSG "osst%d:D: Reached onstream flush drive buffer (write filemark)\n", dev);
#endif

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = WRITE_FILEMARKS;
	cmd[1] = 1;

	SRpnt = osst_do_scsi(*aSRpnt, STp, cmd, 0, SCSI_DATA_NONE, STp->timeout, MAX_WRITE_RETRIES, TRUE);
	*aSRpnt = SRpnt;
	if (!SRpnt) return (-EBUSY);
	if (STp->buffer->syscall_result) {
		if ((SRpnt->sr_sense_buffer[2] & 0x0f) == 2 && SRpnt->sr_sense_buffer[12] == 4) {
			if (SRpnt->sr_sense_buffer[13] == 8) {
				delay = OSST_WAIT_LONG_WRITE_COMPLETE;
			}
		} else
			result = osst_write_error_recovery(STp, aSRpnt, 0);
	}
	result |= osst_wait_ready(STp, aSRpnt, 5 * 60, delay);
	STp->ps[STp->partition].rw = OS_WRITING_COMPLETE;

	return (result);
}

#define OSST_POLL_PER_SEC 10
static int osst_wait_frame(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt, int curr, int minlast, int to)
{
	long	startwait     = jiffies;
	int	dev	      = TAPE_NR(STp->devt);
#if DEBUG
	char	notyetprinted = 1;
#endif
	if (minlast >= 0 && STp->ps[STp->partition].rw != ST_READING)
		printk(KERN_ERR "osst%i:A: Waiting for frame without having initialized read!\n", dev);

	while (time_before (jiffies, startwait + to*HZ))
	{ 
		int result;
		result = osst_get_frame_position (STp, aSRpnt);
		if (result == -EIO)
			if ((result = osst_write_error_recovery(STp, aSRpnt, 0)) == 0)
				return 0;	/* successful recovery leaves drive ready for frame */
		if (result < 0) break;
		if (STp->first_frame_position == curr &&
		    ((minlast < 0 &&
		      (signed)STp->last_frame_position > (signed)curr + minlast) ||
		     (minlast >= 0 && STp->cur_frames > minlast)
		    ) && result >= 0)
		{
#if DEBUG			
			if (debugging || jiffies - startwait >= 2*HZ/OSST_POLL_PER_SEC)
				printk (OSST_DEB_MSG
					"osst%d:D: Succ wait f fr %i (>%i): %i-%i %i (%i): %3li.%li s\n",
					dev, curr, curr+minlast, STp->first_frame_position,
					STp->last_frame_position, STp->cur_frames,
					result, (jiffies-startwait)/HZ, 
					(((jiffies-startwait)%HZ)*10)/HZ);
#endif
			return 0;
		}
#if DEBUG
		if (jiffies - startwait >= 2*HZ/OSST_POLL_PER_SEC && notyetprinted)
		{
			printk (OSST_DEB_MSG "osst%d:D: Wait for frame %i (>%i): %i-%i %i (%i)\n",
				dev, curr, curr+minlast, STp->first_frame_position,
				STp->last_frame_position, STp->cur_frames, result);
			notyetprinted--;
		}
#endif
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout (HZ / OSST_POLL_PER_SEC);
	}
#if DEBUG
	printk (OSST_DEB_MSG "osst%d:D: Fail wait f fr %i (>%i): %i-%i %i: %3li.%li s\n",
		dev, curr, curr+minlast, STp->first_frame_position,
		STp->last_frame_position, STp->cur_frames,
		(jiffies-startwait)/HZ, (((jiffies-startwait)%HZ)*10)/HZ);
#endif	
	return -EBUSY;
}

/*
 * Read the next OnStream tape frame at the current location
 */
static int osst_read_frame(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt, int timeout)
{
	unsigned char	cmd[MAX_COMMAND_SIZE];
	Scsi_Request  * SRpnt;
	int		retval = 0;
#if DEBUG
	os_aux_t      * aux    = STp->buffer->aux;
	int		dev    = TAPE_NR(STp->devt);
#endif

	/* TODO: Error handling */
	if (STp->poll)
		retval = osst_wait_frame (STp, aSRpnt, STp->first_frame_position, 0, timeout);
	
	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = READ_6;
	cmd[1] = 1;
	cmd[4] = 1;

#if DEBUG
	if (debugging)
	    printk(OSST_DEB_MSG "osst%d:D: Reading frame from OnStream tape\n", dev);
#endif
	SRpnt = osst_do_scsi(*aSRpnt, STp, cmd, OS_FRAME_SIZE, SCSI_DATA_READ,
				      STp->timeout, MAX_READ_RETRIES, TRUE);
	*aSRpnt = SRpnt;
	if (!SRpnt)
	    return (-EBUSY);

	if ((STp->buffer)->syscall_result) {
	    retval = 1;
	    if (STp->read_error_frame == 0) {
		STp->read_error_frame = STp->first_frame_position;
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Recording read error at %d\n", dev, STp->read_error_frame);
#endif
	    }
#if DEBUG
	    if (debugging)
		printk(OSST_DEB_MSG "osst%d:D: Sense: %2x %2x %2x %2x %2x %2x %2x %2x\n",
		   dev,
		   SRpnt->sr_sense_buffer[0], SRpnt->sr_sense_buffer[1],
		   SRpnt->sr_sense_buffer[2], SRpnt->sr_sense_buffer[3],
		   SRpnt->sr_sense_buffer[4], SRpnt->sr_sense_buffer[5],
		   SRpnt->sr_sense_buffer[6], SRpnt->sr_sense_buffer[7]);
#endif
	}
	else
	    STp->first_frame_position++;
#if DEBUG
	if (debugging) {
	   printk(OSST_DEB_MSG 
		"osst%d:D: AUX: %c%c%c%c UpdFrCt#%d Wpass#%d %s FrSeq#%d LogBlk#%d Qty=%d Sz=%d\n", dev,
			aux->application_sig[0], aux->application_sig[1],
			aux->application_sig[2], aux->application_sig[3], 
			ntohl(aux->update_frame_cntr), ntohs(aux->partition.wrt_pass_cntr),
			aux->frame_type==1?"EOD":aux->frame_type==2?"MARK":
			aux->frame_type==8?"HEADR":aux->frame_type==0x80?"DATA":"FILL", 
			ntohl(aux->frame_seq_num), ntohl(aux->logical_blk_num),
			ntohs(aux->dat.dat_list[0].blk_cnt), ntohl(aux->dat.dat_list[0].blk_sz) );
	   if (aux->frame_type==2)
		printk(OSST_DEB_MSG "osst%d:D: mark_cnt=%d, last_mark_ppos=%d, last_mark_lbn=%d\n", dev,
			ntohl(aux->filemark_cnt), ntohl(aux->last_mark_ppos), ntohl(aux->last_mark_lbn));
	   printk(OSST_DEB_MSG "osst%d:D: Exit read frame from OnStream tape with code %d\n", dev, retval);
	}
#endif
	return (retval);
}

static int osst_initiate_read(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt)
{
	ST_partstat   * STps   = &(STp->ps[STp->partition]);
	Scsi_Request  * SRpnt  ;
	unsigned char	cmd[MAX_COMMAND_SIZE];
	int		retval = 0;
#if DEBUG
	int		dev    = TAPE_NR(STp->devt);
#endif

	if (STps->rw != ST_READING) {         /* Initialize read operation */
		if (STps->rw == ST_WRITING || STp->dirty) {
			STp->write_type = OS_WRITE_DATA;
                        osst_flush_write_buffer(STp, aSRpnt);
			osst_flush_drive_buffer(STp, aSRpnt);
		}
		STps->rw = ST_READING;
		STp->frame_in_buffer = 0;

		/*
		 *      Issue a read 0 command to get the OnStream drive
                 *      read frames into its buffer.
		 */
		memset(cmd, 0, MAX_COMMAND_SIZE);
		cmd[0] = READ_6;
		cmd[1] = 1;

#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Start Read Ahead on OnStream tape\n", dev);
#endif
		SRpnt   = osst_do_scsi(*aSRpnt, STp, cmd, 0, SCSI_DATA_NONE, STp->timeout, MAX_READ_RETRIES, TRUE);
		*aSRpnt = SRpnt;
		retval  = STp->buffer->syscall_result;
	}

	return retval;
}

static int osst_get_logical_frame(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt, int frame_seq_number, int quiet)
{
	ST_partstat * STps  = &(STp->ps[STp->partition]);
	int           dev   = TAPE_NR(STp->devt);
	int           cnt   = 0,
		      bad   = 0,
		      past  = 0,
		      x,
		      position;

	/*
	 * If we want just any frame (-1) and there is a frame in the buffer, return it
	 */
	if (frame_seq_number == -1 && STp->frame_in_buffer) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Frame %d still in buffer\n", dev, STp->frame_seq_number);
#endif
		return (STps->eof);
	}
	/*
         * Search and wait for the next logical tape frame
	 */
	while (1) {
		if (cnt++ > 400) {
                        printk(KERN_ERR "osst%d:E: Couldn't find logical frame %d, aborting\n",
					    dev, frame_seq_number);
			if (STp->read_error_frame) {
				osst_set_frame_position(STp, aSRpnt, STp->read_error_frame, 0);
#if DEBUG
                        	printk(OSST_DEB_MSG "osst%d:D: Repositioning tape to bad frame %d\n",
						    dev, STp->read_error_frame);
#endif
				STp->read_error_frame = 0;
			}
			return (-EIO);
		}
#if DEBUG
		if (debugging)
			printk(OSST_DEB_MSG "osst%d:D: Looking for frame %d, attempt %d\n",
					  dev, frame_seq_number, cnt);
#endif
		if ( osst_initiate_read(STp, aSRpnt)
                || ( (!STp->frame_in_buffer) && osst_read_frame(STp, aSRpnt, 30) ) ) {
			if (STp->raw)
				return (-EIO);
			position = osst_get_frame_position(STp, aSRpnt);
			if (position >= 0xbae && position < 0xbb8)
				position = 0xbb8;
			else if (position > STp->eod_frame_ppos || ++bad == 10) {
				position = STp->read_error_frame - 1;
			}
			else {
				position += 39;
				cnt += 20;
			}
#if DEBUG
			printk(OSST_DEB_MSG "osst%d:D: Bad frame detected, positioning tape to block %d\n",
					 dev, position);
#endif
			osst_set_frame_position(STp, aSRpnt, position, 0);
			continue;
		}
		if (osst_verify_frame(STp, frame_seq_number, quiet))
			break;
		if (osst_verify_frame(STp, -1, quiet)) {
			x = ntohl(STp->buffer->aux->frame_seq_num);
			if (STp->fast_open) {
				printk(KERN_WARNING
				       "osst%d:W: Found logical frame %d instead of %d after fast open\n",
				       dev, x, frame_seq_number);
				STp->header_ok = 0;
				STp->read_error_frame = 0;
				return (-EIO);
			}
			if (x > frame_seq_number) {
				if (++past > 3) {
					/* positioning backwards did not bring us to the desired frame */
					position = STp->read_error_frame - 1;
				}
				else {
			        	position = osst_get_frame_position(STp, aSRpnt)
					         + frame_seq_number - x - 1;

					if (STp->first_frame_position >= 3000 && position < 3000)
						position -= 10;
				}
#if DEBUG
                                printk(OSST_DEB_MSG
				       "osst%d:D: Found logical frame %d while looking for %d: back up %d\n",
						dev, x, frame_seq_number,
					       	STp->first_frame_position - position);
#endif
                        	osst_set_frame_position(STp, aSRpnt, position, 0);
				cnt += 10;
			}
			else
				past = 0;
		}
		if (osst_get_frame_position(STp, aSRpnt) == 0xbaf) {
#if DEBUG
			printk(OSST_DEB_MSG "osst%d:D: Skipping config partition\n", dev);
#endif
			osst_set_frame_position(STp, aSRpnt, 0xbb8, 0);
			cnt--;
		}
		STp->frame_in_buffer = 0;
	}
	if (cnt > 1) {
		STp->recover_count++;
		STp->recover_erreg++;
		printk(KERN_WARNING "osst%d:I: Don't worry, Read error at position %d recovered\n", 
					dev, STp->read_error_frame);
 	}
	STp->read_count++;

#if DEBUG
	if (debugging || STps->eof)
		printk(OSST_DEB_MSG
			"osst%d:D: Exit get logical frame (%d=>%d) from OnStream tape with code %d\n",
			dev, frame_seq_number, STp->frame_seq_number, STps->eof);
#endif
	STp->fast_open = FALSE;
	STp->read_error_frame = 0;
	return (STps->eof);
}

static int osst_seek_logical_blk(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt, int logical_blk_num)
{
        ST_partstat * STps = &(STp->ps[STp->partition]);
	int	dev        = TAPE_NR(STp->devt);
	int	retries    = 0;
	int	frame_seq_estimate, ppos_estimate, move;
	
	if (logical_blk_num < 0) logical_blk_num = 0;
#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: Seeking logical block %d (now at %d, size %d%c)\n",
				dev, logical_blk_num, STp->logical_blk_num, 
				STp->block_size<1024?STp->block_size:STp->block_size/1024,
				STp->block_size<1024?'b':'k');
#endif
	/* Do we know where we are? */
	if (STps->drv_block >= 0) {
		move                = logical_blk_num - STp->logical_blk_num;
		if (move < 0) move -= (OS_DATA_SIZE / STp->block_size) - 1;
		move               /= (OS_DATA_SIZE / STp->block_size);
		frame_seq_estimate  = STp->frame_seq_number + move;
	} else
		frame_seq_estimate  = logical_blk_num * STp->block_size / OS_DATA_SIZE;

	if (frame_seq_estimate < 2980) ppos_estimate = frame_seq_estimate + 10;
	else			       ppos_estimate = frame_seq_estimate + 20;
	while (++retries < 10) {
	   if (ppos_estimate > STp->eod_frame_ppos-2) {
	       frame_seq_estimate += STp->eod_frame_ppos - 2 - ppos_estimate;
	       ppos_estimate       = STp->eod_frame_ppos - 2;
	   }
	   if (frame_seq_estimate < 0) {
	       frame_seq_estimate = 0;
	       ppos_estimate      = 10;
	   }
	   osst_set_frame_position(STp, aSRpnt, ppos_estimate, 0);
	   if (osst_get_logical_frame(STp, aSRpnt, frame_seq_estimate, 1) >= 0) {
	      /* we've located the estimated frame, now does it have our block? */
	      if (logical_blk_num <  STp->logical_blk_num ||
	          logical_blk_num >= STp->logical_blk_num + ntohs(STp->buffer->aux->dat.dat_list[0].blk_cnt)) {
		 if (STps->eof == ST_FM_HIT)
		    move = logical_blk_num < STp->logical_blk_num? -2 : 1;
		 else {
		    move                = logical_blk_num - STp->logical_blk_num;
		    if (move < 0) move -= (OS_DATA_SIZE / STp->block_size) - 1;
		    move               /= (OS_DATA_SIZE / STp->block_size);
		 }
		 if (!move) move = logical_blk_num > STp->logical_blk_num ? 1 : -1;
#if DEBUG
		 printk(OSST_DEB_MSG
			"osst%d:D: Seek retry %d at ppos %d fsq %d (est %d) lbn %d (need %d) move %d\n",
				dev, retries, ppos_estimate, STp->frame_seq_number, frame_seq_estimate, 
				STp->logical_blk_num, logical_blk_num, move);
#endif
		 frame_seq_estimate += move;
		 ppos_estimate      += move;
		 continue;
	      } else {
		 STp->buffer->read_pointer  = (logical_blk_num - STp->logical_blk_num) * STp->block_size;
		 STp->buffer->buffer_bytes -= STp->buffer->read_pointer;
		 STp->logical_blk_num       =  logical_blk_num;
#if DEBUG
		 printk(OSST_DEB_MSG 
			"osst%d:D: Seek success at ppos %d fsq %d in_buf %d, bytes %d, ptr %d*%d\n",
				dev, ppos_estimate, STp->frame_seq_number, STp->frame_in_buffer, 
				STp->buffer->buffer_bytes, STp->buffer->read_pointer / STp->block_size, 
				STp->block_size);
#endif
		 STps->drv_file = ntohl(STp->buffer->aux->filemark_cnt);
		 if (STps->eof == ST_FM_HIT) {
		     STps->drv_file++;
		     STps->drv_block = 0;
		 } else {
		     STps->drv_block = ntohl(STp->buffer->aux->last_mark_lbn)?
					  STp->logical_blk_num -
					     (STps->drv_file ? ntohl(STp->buffer->aux->last_mark_lbn) + 1 : 0):
					-1;
		 }
		 STps->eof = (STp->first_frame_position >= STp->eod_frame_ppos)?ST_EOD:ST_NOEOF;
		 return 0;
	      }
	   }
	   if (osst_get_logical_frame(STp, aSRpnt, -1, 1) < 0)
	      goto error;
	   /* we are not yet at the estimated frame, adjust our estimate of its physical position */
#if DEBUG
	   printk(OSST_DEB_MSG "osst%d:D: Seek retry %d at ppos %d fsq %d (est %d) lbn %d (need %d)\n", 
			   dev, retries, ppos_estimate, STp->frame_seq_number, frame_seq_estimate, 
			   STp->logical_blk_num, logical_blk_num);
#endif
	   if (frame_seq_estimate != STp->frame_seq_number)
	      ppos_estimate += frame_seq_estimate - STp->frame_seq_number;
	   else
	      break;
	}
error:
	printk(KERN_ERR "osst%d:E: Couldn't seek to logical block %d (at %d), %d retries\n", 
			    dev, logical_blk_num, STp->logical_blk_num, retries);
	return (-EIO);
}

/* The values below are based on the OnStream frame payload size of 32K == 2**15,
 * that is, OSST_FRAME_SHIFT + OSST_SECTOR_SHIFT must be 15. With a minimum block
 * size of 512 bytes, we need to be able to resolve 32K/512 == 64 == 2**6 positions
 * inside each frame. Finaly, OSST_SECTOR_MASK == 2**OSST_FRAME_SHIFT - 1.
 */
#define OSST_FRAME_SHIFT  6
#define OSST_SECTOR_SHIFT 9
#define OSST_SECTOR_MASK  0x03F

static int osst_get_sector(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt)
{
	int	sector;
#if DEBUG
	int	dev = TAPE_NR(STp->devt);
	
	printk(OSST_DEB_MSG 
		"osst%d:D: Positioned at ppos %d, frame %d, lbn %d, file %d, blk %d, %cptr %d, eof %d\n",
		dev, STp->first_frame_position, STp->frame_seq_number, STp->logical_blk_num,
		STp->ps[STp->partition].drv_file, STp->ps[STp->partition].drv_block, 
		STp->ps[STp->partition].rw == ST_WRITING?'w':'r',
		STp->ps[STp->partition].rw == ST_WRITING?STp->buffer->buffer_bytes:
		STp->buffer->read_pointer, STp->ps[STp->partition].eof);
#endif
	/* do we know where we are inside a file? */
	if (STp->ps[STp->partition].drv_block >= 0) {
		sector = (STp->frame_in_buffer ? STp->first_frame_position-1 :
				STp->first_frame_position) << OSST_FRAME_SHIFT;
		if (STp->ps[STp->partition].rw == ST_WRITING)
		       	sector |= (STp->buffer->buffer_bytes >> OSST_SECTOR_SHIFT) & OSST_SECTOR_MASK;
		else
	       		sector |= (STp->buffer->read_pointer >> OSST_SECTOR_SHIFT) & OSST_SECTOR_MASK;
	} else {
		sector = osst_get_frame_position(STp, aSRpnt);
		if (sector > 0)
			sector <<= OSST_FRAME_SHIFT;
	}
	return sector;
}

static int osst_seek_sector(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt, int sector)
{
        ST_partstat   * STps   = &(STp->ps[STp->partition]);
	int		frame  = sector >> OSST_FRAME_SHIFT,
			offset = (sector & OSST_SECTOR_MASK) << OSST_SECTOR_SHIFT, 
			r;
#if DEBUG
	int		dev    = TAPE_NR(STp->devt);

	printk(OSST_DEB_MSG "osst%d:D: Seeking sector %d in frame %d at offset %d\n",
				dev, sector, frame, offset);
#endif
	if (frame < 0 || frame >= STp->capacity) return (-ENXIO);

	if (frame <= STp->first_data_ppos) {
		STp->frame_seq_number = STp->logical_blk_num = STps->drv_file = STps->drv_block = 0;
		return (osst_set_frame_position(STp, aSRpnt, frame, 0));
	}
	r = osst_set_frame_position(STp, aSRpnt, offset?frame:frame-1, 0);
	if (r < 0) return r;

	r = osst_get_logical_frame(STp, aSRpnt, -1, 1);
	if (r < 0) return r;

	if (osst_get_frame_position(STp, aSRpnt) != (offset?frame+1:frame)) return (-EIO);

	if (offset) {
		STp->logical_blk_num      += offset / STp->block_size;
		STp->buffer->read_pointer  = offset;
		STp->buffer->buffer_bytes -= offset;
	} else {
		STp->frame_seq_number++;
		STp->frame_in_buffer       = 0;
		STp->logical_blk_num      += ntohs(STp->buffer->aux->dat.dat_list[0].blk_cnt);
		STp->buffer->buffer_bytes  = STp->buffer->read_pointer = 0;
	}
	STps->drv_file = ntohl(STp->buffer->aux->filemark_cnt);
	if (STps->eof == ST_FM_HIT) {
		STps->drv_file++;
		STps->drv_block = 0;
	} else {
		STps->drv_block = ntohl(STp->buffer->aux->last_mark_lbn)?
				    STp->logical_blk_num -
					(STps->drv_file ? ntohl(STp->buffer->aux->last_mark_lbn) + 1 : 0):
				  -1;
	}
	STps->eof       = (STp->first_frame_position >= STp->eod_frame_ppos)?ST_EOD:ST_NOEOF;
#if DEBUG
	printk(OSST_DEB_MSG 
		"osst%d:D: Now positioned at ppos %d, frame %d, lbn %d, file %d, blk %d, rptr %d, eof %d\n",
		dev, STp->first_frame_position, STp->frame_seq_number, STp->logical_blk_num,
		STps->drv_file, STps->drv_block, STp->buffer->read_pointer, STps->eof);
#endif
	return 0;
}

/*
 * Read back the drive's internal buffer contents, as a part
 * of the write error recovery mechanism for old OnStream
 * firmware revisions.
 * Precondition for this function to work: all frames in the
 * drive's buffer must be of one type (DATA, MARK or EOD)!
 */
static int osst_read_back_buffer_and_rewrite(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt,
					unsigned int frame, unsigned int skip, int pending)
{
	Scsi_Request  * SRpnt = * aSRpnt;
	unsigned char * buffer, * p;
	unsigned char	cmd[MAX_COMMAND_SIZE];
	int             flag, new_frame, i;
	int             nframes          = STp->cur_frames;
	int             blks_per_frame   = ntohs(STp->buffer->aux->dat.dat_list[0].blk_cnt);
	int             frame_seq_number = ntohl(STp->buffer->aux->frame_seq_num)
						- (nframes + pending - 1);
	int             logical_blk_num  = ntohl(STp->buffer->aux->logical_blk_num) 
						- (nframes + pending - 1) * blks_per_frame;
	int		dev              = TAPE_NR(STp->devt);
	long		startwait        = jiffies;
#if DEBUG
	int		dbg              = debugging;
#endif

	if ((buffer = (unsigned char *)vmalloc((nframes + 1) * OS_DATA_SIZE)) == NULL)
		return (-EIO);

	printk(KERN_INFO "osst%d:I: Reading back %d frames from drive buffer%s\n",
			 dev, nframes, pending?" and one that was pending":"");

	osst_copy_from_buffer(STp->buffer, (p = &buffer[nframes * OS_DATA_SIZE]));
#if DEBUG
	if (pending && debugging)
		printk(OSST_DEB_MSG "osst%d:D: Pending frame %d (lblk %d), data %02x %02x %02x %02x\n",
				dev, frame_seq_number + nframes,
			       	logical_blk_num + nframes * blks_per_frame,
			       	p[0], p[1], p[2], p[3]);
#endif
	for (i = 0, p = buffer; i < nframes; i++, p += OS_DATA_SIZE) {

		memset(cmd, 0, MAX_COMMAND_SIZE);
		cmd[0] = 0x3C;		/* Buffer Read           */
		cmd[1] = 6;		/* Retrieve Faulty Block */
		cmd[7] = 32768 >> 8;
		cmd[8] = 32768 & 0xff;

		SRpnt = osst_do_scsi(SRpnt, STp, cmd, OS_FRAME_SIZE, SCSI_DATA_READ,
					    STp->timeout, MAX_READ_RETRIES, TRUE);
	
		if ((STp->buffer)->syscall_result || !SRpnt) {
			printk(KERN_ERR "osst%d:E: Failed to read frame back from OnStream buffer\n", dev);
			vfree((void *)buffer);
			*aSRpnt = SRpnt;
			return (-EIO);
		}
		osst_copy_from_buffer(STp->buffer, p);
#if DEBUG
		if (debugging)
			printk(OSST_DEB_MSG "osst%d:D: Read back logical frame %d, data %02x %02x %02x %02x\n",
					  dev, frame_seq_number + i, p[0], p[1], p[2], p[3]);
#endif
	}
	*aSRpnt = SRpnt;
	osst_get_frame_position(STp, aSRpnt);

#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: Frames left in buffer: %d\n", dev, STp->cur_frames);
#endif
	/* Write synchronously so we can be sure we're OK again and don't have to recover recursively */
	/* In the header we don't actually re-write the frames that fail, just the ones after them */

	for (flag=1, new_frame=frame, p=buffer, i=0; i < nframes + pending; ) {

		if (flag) {
			if (STp->write_type == OS_WRITE_HEADER) {
				i += skip;
				p += skip * OS_DATA_SIZE;
			}
			else if (new_frame < 2990 && new_frame+skip+nframes+pending >= 2990)
				new_frame = 3000-i;
			else
				new_frame += skip;
#if DEBUG
			printk(OSST_DEB_MSG "osst%d:D: Position to frame %d, write fseq %d\n",
						dev, new_frame+i, frame_seq_number+i);
#endif
			osst_set_frame_position(STp, aSRpnt, new_frame + i, 0);
			osst_wait_ready(STp, aSRpnt, 60, OSST_WAIT_POSITION_COMPLETE);
			osst_get_frame_position(STp, aSRpnt);
			SRpnt = * aSRpnt;

			if (new_frame > frame + 1000) {
				printk(KERN_ERR "osst%d:E: Failed to find writable tape media\n", dev);
				vfree((void *)buffer);
				return (-EIO);
			}
			flag = 0;
			if ( i >= nframes + pending ) break;
		}
		osst_copy_to_buffer(STp->buffer, p);
		/*
		 * IMPORTANT: for error recovery to work, _never_ queue frames with mixed frame type!
		 */
		osst_init_aux(STp, STp->buffer->aux->frame_type, frame_seq_number+i,
			       	logical_blk_num + i*blks_per_frame,
			       	ntohl(STp->buffer->aux->dat.dat_list[0].blk_sz), blks_per_frame);
		memset(cmd, 0, MAX_COMMAND_SIZE);
		cmd[0] = WRITE_6;
		cmd[1] = 1;
		cmd[4] = 1;

#if DEBUG
		if (debugging)
			printk(OSST_DEB_MSG
				"osst%d:D: About to write frame %d, seq %d, lbn %d, data %02x %02x %02x %02x\n",
				dev, new_frame+i, frame_seq_number+i, logical_blk_num + i*blks_per_frame,
				p[0], p[1], p[2], p[3]);
#endif
		SRpnt = osst_do_scsi(SRpnt, STp, cmd, OS_FRAME_SIZE, SCSI_DATA_WRITE,
					    STp->timeout, MAX_WRITE_RETRIES, TRUE);

		if (STp->buffer->syscall_result)
			flag = 1;
		else {
			p += OS_DATA_SIZE; i++;

			/* if we just sent the last frame, wait till all successfully written */
			if ( i == nframes + pending ) {
#if DEBUG
				printk(OSST_DEB_MSG "osst%d:D: Check re-write successful\n", dev);
#endif
				memset(cmd, 0, MAX_COMMAND_SIZE);
				cmd[0] = WRITE_FILEMARKS;
				cmd[1] = 1;
				SRpnt = osst_do_scsi(SRpnt, STp, cmd, 0, SCSI_DATA_NONE,
							    STp->timeout, MAX_WRITE_RETRIES, TRUE);
#if DEBUG
				if (debugging) {
					printk(OSST_DEB_MSG "osst%d:D: Sleeping in re-write wait ready\n", dev);
					printk(OSST_DEB_MSG "osst%d:D: Turning off debugging for a while\n", dev);
					debugging = 0;
				}
#endif
				flag = STp->buffer->syscall_result;
				while ( !flag && time_before(jiffies, startwait + 60*HZ) ) {

					memset(cmd, 0, MAX_COMMAND_SIZE);
					cmd[0] = TEST_UNIT_READY;

					SRpnt = osst_do_scsi(SRpnt, STp, cmd, 0, SCSI_DATA_NONE, STp->timeout,
									 MAX_READY_RETRIES, TRUE);

					if (SRpnt->sr_sense_buffer[2] == 2 && SRpnt->sr_sense_buffer[12] == 4 &&
					    (SRpnt->sr_sense_buffer[13] == 1 || SRpnt->sr_sense_buffer[13] == 8)) {
						/* in the process of becoming ready */
						set_current_state(TASK_INTERRUPTIBLE);
						schedule_timeout(HZ / 10);
						continue;
					}
					if (STp->buffer->syscall_result)
						flag = 1;
					break;
				}
#if DEBUG
				debugging = dbg;
				printk(OSST_DEB_MSG "osst%d:D: Wait re-write finished\n", dev);
#endif
			}
		}
		*aSRpnt = SRpnt;
		if (flag) {
			if ((SRpnt->sr_sense_buffer[ 2] & 0x0f) == 13 &&
			     SRpnt->sr_sense_buffer[12]         ==  0 &&
			     SRpnt->sr_sense_buffer[13]         ==  2) {
				printk(KERN_ERR "osst%d:E: Volume overflow in write error recovery\n", dev);
				vfree((void *)buffer);
				return (-EIO);			/* hit end of tape = fail */
			}
			i = ((SRpnt->sr_sense_buffer[3] << 24) |
			     (SRpnt->sr_sense_buffer[4] << 16) |
			     (SRpnt->sr_sense_buffer[5] <<  8) |
			      SRpnt->sr_sense_buffer[6]        ) - new_frame;
			p = &buffer[i * OS_DATA_SIZE];
#if DEBUG
			printk(OSST_DEB_MSG "osst%d:D: Additional write error at %d\n", dev, new_frame+i);
#endif
			osst_get_frame_position(STp, aSRpnt);
#if DEBUG
			printk(OSST_DEB_MSG "osst%d:D: reported frame positions: host = %d, tape = %d\n",
					  dev, STp->first_frame_position, STp->last_frame_position);
#endif
		}
	}    
	if (!pending)
		osst_copy_to_buffer(STp->buffer, p);	/* so buffer content == at entry in all cases */
	vfree((void *)buffer);
	return 0;
}

static int osst_reposition_and_retry(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt,
					unsigned int frame, unsigned int skip, int pending)
{
	unsigned char	cmd[MAX_COMMAND_SIZE];
	Scsi_Request  * SRpnt;
	int		dev       = TAPE_NR(STp->devt);
	int		expected  = 0;
	int		attempts  = 1000 / skip;
	int		flag      = 1;
	long		startwait = jiffies;
#if DEBUG
	int		dbg       = debugging;
#endif

	while (attempts && time_before(jiffies, startwait + 60*HZ)) {
		if (flag) {
#if DEBUG
			debugging = dbg;
#endif
			if (frame < 2990 && frame+skip+STp->cur_frames+pending >= 2990)
				frame = 3000-skip;
			expected = frame+skip+STp->cur_frames+pending;
#if DEBUG
			printk(OSST_DEB_MSG "osst%d:D: Position to fppos %d, re-write from fseq %d\n",
					  dev, frame+skip, STp->frame_seq_number-STp->cur_frames-pending);
#endif
			osst_set_frame_position(STp, aSRpnt, frame + skip, 1);
			flag = 0;
			attempts--;
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(HZ / 10);
		}
		if (osst_get_frame_position(STp, aSRpnt) < 0) {		/* additional write error */
#if DEBUG
			printk(OSST_DEB_MSG "osst%d:D: Addl error, host %d, tape %d, buffer %d\n",
					  dev, STp->first_frame_position,
					  STp->last_frame_position, STp->cur_frames);
#endif
			frame = STp->last_frame_position;
			flag = 1;
			continue;
		}
		if (pending && STp->cur_frames < 50) {

			memset(cmd, 0, MAX_COMMAND_SIZE);
			cmd[0] = WRITE_6;
			cmd[1] = 1;
			cmd[4] = 1;
#if DEBUG
			printk(OSST_DEB_MSG "osst%d:D: About to write pending fseq %d at fppos %d\n",
					  dev, STp->frame_seq_number-1, STp->first_frame_position);
#endif
			SRpnt = osst_do_scsi(*aSRpnt, STp, cmd, OS_FRAME_SIZE, SCSI_DATA_WRITE,
						      STp->timeout, MAX_WRITE_RETRIES, TRUE);
			*aSRpnt = SRpnt;

			if (STp->buffer->syscall_result) {		/* additional write error */
				if ((SRpnt->sr_sense_buffer[ 2] & 0x0f) == 13 &&
				     SRpnt->sr_sense_buffer[12]         ==  0 &&
				     SRpnt->sr_sense_buffer[13]         ==  2) {
					printk(KERN_ERR
					       "osst%d:E: Volume overflow in write error recovery\n",
					       dev);
					break;				/* hit end of tape = fail */
				}
				flag = 1;
			}
			else
				pending = 0;

			continue;
		}
		if (STp->cur_frames == 0) {
#if DEBUG
			debugging = dbg;
			printk(OSST_DEB_MSG "osst%d:D: Wait re-write finished\n", dev);
#endif
			if (STp->first_frame_position != expected) {
				printk(KERN_ERR "osst%d:A: Actual position %d - expected %d\n", 
						dev, STp->first_frame_position, expected);
				return (-EIO);
			}
			return 0;
		}
#if DEBUG
		if (debugging) {
			printk(OSST_DEB_MSG "osst%d:D: Sleeping in re-write wait ready\n", dev);
			printk(OSST_DEB_MSG "osst%d:D: Turning off debugging for a while\n", dev);
			debugging = 0;
		}
#endif
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(HZ / 10);
	}
	printk(KERN_ERR "osst%d:E: Failed to find valid tape media\n", dev);
#if DEBUG
	debugging = dbg;
#endif
	return (-EIO);
}

/*
 * Error recovery algorithm for the OnStream tape.
 */

static int osst_write_error_recovery(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt, int pending)
{
	Scsi_Request * SRpnt  = * aSRpnt;
	ST_partstat  * STps   = & STp->ps[STp->partition];
	int            dev    = TAPE_NR(STp->devt);
	int            retval = 0;
	int            rw_state;
	unsigned int  frame, skip;

	rw_state = STps->rw;

	if ((SRpnt->sr_sense_buffer[ 2] & 0x0f) != 3
	  || SRpnt->sr_sense_buffer[12]         != 12
	  || SRpnt->sr_sense_buffer[13]         != 0) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Write error recovery cannot handle %02x:%02x:%02x\n", dev,
			SRpnt->sr_sense_buffer[2], SRpnt->sr_sense_buffer[12], SRpnt->sr_sense_buffer[13]);
#endif
		return (-EIO);
	}
	frame =	(SRpnt->sr_sense_buffer[3] << 24) |
		(SRpnt->sr_sense_buffer[4] << 16) |
		(SRpnt->sr_sense_buffer[5] <<  8) |
		 SRpnt->sr_sense_buffer[6];
	skip  =  SRpnt->sr_sense_buffer[9];
 
#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: Detected physical bad frame at %u, advised to skip %d\n", dev, frame, skip);
#endif
	osst_get_frame_position(STp, aSRpnt);
#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: reported frame positions: host = %d, tape = %d\n",
			dev, STp->first_frame_position, STp->last_frame_position);
#endif
	switch (STp->write_type) {
	   case OS_WRITE_DATA:
	   case OS_WRITE_EOD:
	   case OS_WRITE_NEW_MARK:
		printk(KERN_WARNING 
			"osst%d:I: Relocating %d buffered logical frames from position %u to %u\n",
			dev, STp->cur_frames, frame, (frame + skip > 3000 && frame < 3000)?3000:frame + skip);
		if (STp->os_fw_rev >= 10600)
			retval = osst_reposition_and_retry(STp, aSRpnt, frame, skip, pending);
		else
			retval = osst_read_back_buffer_and_rewrite(STp, aSRpnt, frame, skip, pending);
		printk(KERN_WARNING "osst%d:%s: %sWrite error%srecovered\n", dev,
			       	retval?"E"    :"I",
			       	retval?""     :"Don't worry, ",
			       	retval?" not ":" ");
		break;
	   case OS_WRITE_LAST_MARK:
		printk(KERN_ERR "osst%d:E: Bad frame in update last marker, fatal\n", dev);
		osst_set_frame_position(STp, aSRpnt, frame + STp->cur_frames + pending, 0);
		retval = -EIO;
		break;
	   case OS_WRITE_HEADER:
		printk(KERN_WARNING "osst%d:I: Bad frame in header partition, skipped\n", dev);
		retval = osst_read_back_buffer_and_rewrite(STp, aSRpnt, frame, 1, pending);
		break;
	   default:
		printk(KERN_INFO "osst%d:I: Bad frame in filler, ignored\n", dev);
		osst_set_frame_position(STp, aSRpnt, frame + STp->cur_frames + pending, 0);
	}
	osst_get_frame_position(STp, aSRpnt);
#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: Positioning complete, cur_frames %d, pos %d, tape pos %d\n", 
			dev, STp->cur_frames, STp->first_frame_position, STp->last_frame_position);
	printk(OSST_DEB_MSG "osst%d:D: next logical frame to write: %d\n", dev, STp->logical_blk_num);
#endif
	if (retval == 0) {
		STp->recover_count++;
		STp->recover_erreg++;
	}
	STps->rw = rw_state;
	return retval;
}

static int osst_space_over_filemarks_backward(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt,
								 int mt_op, int mt_count)
{
	int     dev = TAPE_NR(STp->devt);
	int     cnt;
	int     last_mark_ppos = -1;

#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: Reached space_over_filemarks_backwards %d %d\n", dev, mt_op, mt_count);
#endif
	if (osst_get_logical_frame(STp, aSRpnt, -1, 0) < 0) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Couldn't get logical blk num in space_filemarks_bwd\n", dev);
#endif
		return -EIO;
	}
	if (STp->linux_media_version >= 4) {
		/*
		 * direct lookup in header filemark list
		 */
		cnt = ntohl(STp->buffer->aux->filemark_cnt);
		if (STp->header_ok                         && 
		    STp->header_cache != NULL              &&
		    (cnt - mt_count)  >= 0                 &&
		    (cnt - mt_count)   < OS_FM_TAB_MAX     &&
		    (cnt - mt_count)   < STp->filemark_cnt &&
		    STp->header_cache->dat_fm_tab.fm_tab_ent[cnt-1] == STp->buffer->aux->last_mark_ppos)

			last_mark_ppos = ntohl(STp->header_cache->dat_fm_tab.fm_tab_ent[cnt - mt_count]);
#if DEBUG
		if (STp->header_cache == NULL || (cnt - mt_count) < 0 || (cnt - mt_count) >= OS_FM_TAB_MAX)
			printk(OSST_DEB_MSG "osst%d:D: Filemark lookup fail due to %s\n", dev,
			       STp->header_cache == NULL?"lack of header cache":"count out of range");
		else
			printk(OSST_DEB_MSG "osst%d:D: Filemark lookup: prev mark %d (%s), skip %d to %d\n",
				dev, cnt,
				((cnt == -1 && ntohl(STp->buffer->aux->last_mark_ppos) == -1) ||
				 (STp->header_cache->dat_fm_tab.fm_tab_ent[cnt-1] ==
					 STp->buffer->aux->last_mark_ppos))?"match":"error",
			       mt_count, last_mark_ppos);
#endif
		if (last_mark_ppos > 10 && last_mark_ppos < STp->eod_frame_ppos) {
			osst_position_tape_and_confirm(STp, aSRpnt, last_mark_ppos);
			if (osst_get_logical_frame(STp, aSRpnt, -1, 0) < 0) {
#if DEBUG
				printk(OSST_DEB_MSG 
					"osst%d:D: Couldn't get logical blk num in space_filemarks\n", dev);
#endif
				return (-EIO);
			}
			if (STp->buffer->aux->frame_type != OS_FRAME_TYPE_MARKER) {
				printk(KERN_WARNING "osst%d:W: Expected to find marker at ppos %d, not found\n",
						 dev, last_mark_ppos);
				return (-EIO);
			}
			goto found;
		}
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Reverting to scan filemark backwards\n", dev);
#endif
	}
	cnt = 0;
	while (cnt != mt_count) {
		last_mark_ppos = ntohl(STp->buffer->aux->last_mark_ppos);
		if (last_mark_ppos == -1)
			return (-EIO);
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Positioning to last mark at %d\n", dev, last_mark_ppos);
#endif
		osst_position_tape_and_confirm(STp, aSRpnt, last_mark_ppos);
		cnt++;
		if (osst_get_logical_frame(STp, aSRpnt, -1, 0) < 0) {
#if DEBUG
			printk(OSST_DEB_MSG "osst%d:D: Couldn't get logical blk num in space_filemarks\n", dev);
#endif
			return (-EIO);
		}
		if (STp->buffer->aux->frame_type != OS_FRAME_TYPE_MARKER) {
			printk(KERN_WARNING "osst%d:W: Expected to find marker at ppos %d, not found\n",
					 dev, last_mark_ppos);
			return (-EIO);
		}
	}
found:
	if (mt_op == MTBSFM) {
		STp->frame_seq_number++;
		STp->frame_in_buffer      = 0;
		STp->buffer->buffer_bytes = 0;
		STp->buffer->read_pointer = 0;
		STp->logical_blk_num     += ntohs(STp->buffer->aux->dat.dat_list[0].blk_cnt);
	}
	return 0;
}

/*
 * ADRL 1.1 compatible "slow" space filemarks fwd version
 *
 * Just scans for the filemark sequentially.
 */
static int osst_space_over_filemarks_forward_slow(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt,
								     int mt_op, int mt_count)
{
	int	cnt = 0;
#if DEBUG
	int	dev = TAPE_NR(STp->devt);

	printk(OSST_DEB_MSG "osst%d:D: Reached space_over_filemarks_forward_slow %d %d\n", dev, mt_op, mt_count);
#endif
	if (osst_get_logical_frame(STp, aSRpnt, -1, 0) < 0) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Couldn't get logical blk num in space_filemarks_fwd\n", dev);
#endif
		return (-EIO);
	}
	while (1) {
		if (osst_get_logical_frame(STp, aSRpnt, -1, 0) < 0) {
#if DEBUG
			printk(OSST_DEB_MSG "osst%d:D: Couldn't get logical blk num in space_filemarks\n", dev);
#endif
			return (-EIO);
		}
		if (STp->buffer->aux->frame_type == OS_FRAME_TYPE_MARKER)
			cnt++;
		if (STp->buffer->aux->frame_type == OS_FRAME_TYPE_EOD) {
#if DEBUG
			printk(OSST_DEB_MSG "osst%d:D: space_fwd: EOD reached\n", dev);
#endif
			if (STp->first_frame_position > STp->eod_frame_ppos+1) {
#if DEBUG
				printk(OSST_DEB_MSG "osst%d:D: EOD position corrected (%d=>%d)\n",
					       	dev, STp->eod_frame_ppos, STp->first_frame_position-1);
#endif
				STp->eod_frame_ppos = STp->first_frame_position-1;
			}
			return (-EIO);
		}
		if (cnt == mt_count)
			break;
		STp->frame_in_buffer = 0;
	}
	if (mt_op == MTFSF) {
		STp->frame_seq_number++;
		STp->frame_in_buffer      = 0;
		STp->buffer->buffer_bytes = 0;
		STp->buffer->read_pointer = 0;
		STp->logical_blk_num     += ntohs(STp->buffer->aux->dat.dat_list[0].blk_cnt);
	}
	return 0;
}

/*
 * Fast linux specific version of OnStream FSF
 */
static int osst_space_over_filemarks_forward_fast(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt,
								     int mt_op, int mt_count)
{
	int	dev = TAPE_NR(STp->devt);
	int	cnt = 0,
		next_mark_ppos = -1;

#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: Reached space_over_filemarks_forward_fast %d %d\n", dev, mt_op, mt_count);
#endif
	if (osst_get_logical_frame(STp, aSRpnt, -1, 0) < 0) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Couldn't get logical blk num in space_filemarks_fwd\n", dev);
#endif
		return (-EIO);
	}

	if (STp->linux_media_version >= 4) {
		/*
		 * direct lookup in header filemark list
		 */
		cnt = ntohl(STp->buffer->aux->filemark_cnt) - 1;
		if (STp->header_ok                         && 
		    STp->header_cache != NULL              &&
		    (cnt + mt_count)   < OS_FM_TAB_MAX     &&
		    (cnt + mt_count)   < STp->filemark_cnt &&
		    ((cnt == -1 && ntohl(STp->buffer->aux->last_mark_ppos) == -1) ||
		     (STp->header_cache->dat_fm_tab.fm_tab_ent[cnt] == STp->buffer->aux->last_mark_ppos)))

			next_mark_ppos = ntohl(STp->header_cache->dat_fm_tab.fm_tab_ent[cnt + mt_count]);
#if DEBUG
		if (STp->header_cache == NULL || (cnt + mt_count) >= OS_FM_TAB_MAX)
			printk(OSST_DEB_MSG "osst%d:D: Filemark lookup fail due to %s\n", dev,
			       STp->header_cache == NULL?"lack of header cache":"count out of range");
		else
			printk(OSST_DEB_MSG "osst%d:D: Filemark lookup: prev mark %d (%s), skip %d to %d\n",
			       dev, cnt,
			       ((cnt == -1 && ntohl(STp->buffer->aux->last_mark_ppos) == -1) ||
				(STp->header_cache->dat_fm_tab.fm_tab_ent[cnt] ==
					 STp->buffer->aux->last_mark_ppos))?"match":"error",
			       mt_count, next_mark_ppos);
#endif
		if (next_mark_ppos <= 10 || next_mark_ppos > STp->eod_frame_ppos) {
#if DEBUG
			printk(OSST_DEB_MSG "osst%d:D: Reverting to slow filemark space\n", dev);
#endif
			return osst_space_over_filemarks_forward_slow(STp, aSRpnt, mt_op, mt_count);
		} else {
			osst_position_tape_and_confirm(STp, aSRpnt, next_mark_ppos);
			if (osst_get_logical_frame(STp, aSRpnt, -1, 0) < 0) {
#if DEBUG
				printk(OSST_DEB_MSG "osst%d:D: Couldn't get logical blk num in space_filemarks\n",
						 dev);
#endif
				return (-EIO);
			}
			if (STp->buffer->aux->frame_type != OS_FRAME_TYPE_MARKER) {
				printk(KERN_WARNING "osst%d:W: Expected to find marker at ppos %d, not found\n",
						 dev, next_mark_ppos);
				return (-EIO);
			}
			if (ntohl(STp->buffer->aux->filemark_cnt) != cnt + mt_count) {
				printk(KERN_WARNING "osst%d:W: Expected to find marker %d at ppos %d, not %d\n",
						 dev, cnt+mt_count, next_mark_ppos,
						 ntohl(STp->buffer->aux->filemark_cnt));
       				return (-EIO);
			}
		}
	} else {
		/*
		 * Find nearest (usually previous) marker, then jump from marker to marker
		 */
		while (1) {
			if (STp->buffer->aux->frame_type == OS_FRAME_TYPE_MARKER)
				break;
			if (STp->buffer->aux->frame_type == OS_FRAME_TYPE_EOD) {
#if DEBUG
				printk(OSST_DEB_MSG "osst%d:D: space_fwd: EOD reached\n", dev);
#endif
				return (-EIO);
			}
			if (ntohl(STp->buffer->aux->filemark_cnt) == 0) {
				if (STp->first_mark_ppos == -1) {
#if DEBUG
					printk(OSST_DEB_MSG "osst%d:D: Reverting to slow filemark space\n", dev);
#endif
					return osst_space_over_filemarks_forward_slow(STp, aSRpnt, mt_op, mt_count);
				}
				osst_position_tape_and_confirm(STp, aSRpnt, STp->first_mark_ppos);
				if (osst_get_logical_frame(STp, aSRpnt, -1, 0) < 0) {
#if DEBUG
					printk(OSST_DEB_MSG
					       "osst%d:D: Couldn't get logical blk num in space_filemarks_fwd_fast\n",
					       dev);
#endif
					return (-EIO);
				}
				if (STp->buffer->aux->frame_type != OS_FRAME_TYPE_MARKER) {
					printk(KERN_WARNING "osst%d:W: Expected to find filemark at %d\n",
							 dev, STp->first_mark_ppos);
					return (-EIO);
				}
			} else {
				if (osst_space_over_filemarks_backward(STp, aSRpnt, MTBSF, 1) < 0)
					return (-EIO);
				mt_count++;
			}
		}
		cnt++;
		while (cnt != mt_count) {
			next_mark_ppos = ntohl(STp->buffer->aux->next_mark_ppos);
			if (!next_mark_ppos || next_mark_ppos > STp->eod_frame_ppos) {
#if DEBUG
				printk(OSST_DEB_MSG "osst%d:D: Reverting to slow filemark space\n", dev);
#endif
				return osst_space_over_filemarks_forward_slow(STp, aSRpnt, mt_op, mt_count - cnt);
			}
#if DEBUG
			else printk(OSST_DEB_MSG "osst%d:D: Positioning to next mark at %d\n", dev, next_mark_ppos);
#endif
			osst_position_tape_and_confirm(STp, aSRpnt, next_mark_ppos);
			cnt++;
			if (osst_get_logical_frame(STp, aSRpnt, -1, 0) < 0) {
#if DEBUG
				printk(OSST_DEB_MSG "osst%d:D: Couldn't get logical blk num in space_filemarks\n",
						 dev);
#endif
				return (-EIO);
			}
			if (STp->buffer->aux->frame_type != OS_FRAME_TYPE_MARKER) {
				printk(KERN_WARNING "osst%d:W: Expected to find marker at ppos %d, not found\n",
						 dev, next_mark_ppos);
				return (-EIO);
			}
		}
	}
	if (mt_op == MTFSF) {
		STp->frame_seq_number++;
		STp->frame_in_buffer      = 0;
		STp->buffer->buffer_bytes = 0;
		STp->buffer->read_pointer = 0;
		STp->logical_blk_num     += ntohs(STp->buffer->aux->dat.dat_list[0].blk_cnt);
	}
	return 0;
}

/*
 * In debug mode, we want to see as many errors as possible
 * to test the error recovery mechanism.
 */
#if DEBUG
static void osst_set_retries(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt, int retries)
{
	unsigned char	cmd[MAX_COMMAND_SIZE];
	Scsi_Request     * SRpnt  = * aSRpnt;
	int		dev  = TAPE_NR(STp->devt);

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = MODE_SELECT;
	cmd[1] = 0x10;
	cmd[4] = NUMBER_RETRIES_PAGE_LENGTH + MODE_HEADER_LENGTH;

	(STp->buffer)->b_data[0] = cmd[4] - 1;
	(STp->buffer)->b_data[1] = 0;			/* Medium Type - ignoring */
	(STp->buffer)->b_data[2] = 0;			/* Reserved */
	(STp->buffer)->b_data[3] = 0;			/* Block Descriptor Length */
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 0] = NUMBER_RETRIES_PAGE | (1 << 7);
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 1] = 2;
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 2] = 4;
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 3] = retries;

	if (debugging)
	    printk(OSST_DEB_MSG "osst%d:D: Setting number of retries on OnStream tape to %d\n", dev, retries);

	SRpnt = osst_do_scsi(SRpnt, STp, cmd, cmd[4], SCSI_DATA_WRITE, STp->timeout, 0, TRUE);
	*aSRpnt = SRpnt;

	if ((STp->buffer)->syscall_result)
	    printk (KERN_ERR "osst%d:D: Couldn't set retries to %d\n", dev, retries);
}
#endif


static int osst_write_filemark(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt)
{
	int	result;
	int	this_mark_ppos = STp->first_frame_position;
	int	this_mark_lbn  = STp->logical_blk_num;
#if DEBUG
	int	dev = TAPE_NR(STp->devt);
#endif

	if (STp->raw) return 0;

	STp->write_type = OS_WRITE_NEW_MARK;
#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: Writing Filemark %i at fppos %d (fseq %d, lblk %d)\n", 
	       dev, STp->filemark_cnt, this_mark_ppos, STp->frame_seq_number, this_mark_lbn);
#endif
	STp->dirty = 1;
	result  = osst_flush_write_buffer(STp, aSRpnt);
	result |= osst_flush_drive_buffer(STp, aSRpnt);
	STp->last_mark_ppos = this_mark_ppos;
	STp->last_mark_lbn  = this_mark_lbn;
	if (STp->header_cache != NULL && STp->filemark_cnt < OS_FM_TAB_MAX)
		STp->header_cache->dat_fm_tab.fm_tab_ent[STp->filemark_cnt] = htonl(this_mark_ppos);
	if (STp->filemark_cnt++ == 0)
		STp->first_mark_ppos = this_mark_ppos;
	return result;
}

static int osst_write_eod(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt)
{
	int	result;
#if DEBUG
	int	dev = TAPE_NR(STp->devt);
#endif

	if (STp->raw) return 0;

	STp->write_type = OS_WRITE_EOD;
	STp->eod_frame_ppos = STp->first_frame_position;
#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: Writing EOD at fppos %d (fseq %d, lblk %d)\n", dev,
			STp->eod_frame_ppos, STp->frame_seq_number, STp->logical_blk_num);
#endif
	STp->dirty = 1;

	result  = osst_flush_write_buffer(STp, aSRpnt);	
	result |= osst_flush_drive_buffer(STp, aSRpnt);
	STp->eod_frame_lfa = --(STp->frame_seq_number);
	return result;
}

static int osst_write_filler(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt, int where, int count)
{
	int	dev = TAPE_NR(STp->devt);

#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: Reached onstream write filler group %d\n", dev, where);
#endif
	osst_wait_ready(STp, aSRpnt, 60 * 5, 0);
	osst_set_frame_position(STp, aSRpnt, where, 0);
	STp->write_type = OS_WRITE_FILLER;
	while (count--) {
		memcpy(STp->buffer->b_data, "Filler", 6);
		STp->buffer->buffer_bytes = 6;
		STp->dirty = 1;
		if (osst_flush_write_buffer(STp, aSRpnt)) {
			printk(KERN_INFO "osst%i:I: Couldn't write filler frame\n", dev);
			return (-EIO);
		}
	}
#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: Exiting onstream write filler group\n", dev);
#endif
	return osst_flush_drive_buffer(STp, aSRpnt);
}

static int __osst_write_header(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt, int where, int count)
{
	int	dev   = TAPE_NR(STp->devt);
	int     result;

#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: Reached onstream write header group %d\n", dev, where);
#endif
	osst_wait_ready(STp, aSRpnt, 60 * 5, 0);
	osst_set_frame_position(STp, aSRpnt, where, 0);
	STp->write_type = OS_WRITE_HEADER;
	while (count--) {
		osst_copy_to_buffer(STp->buffer, (unsigned char *)STp->header_cache);
		STp->buffer->buffer_bytes = sizeof(os_header_t);
		STp->dirty = 1;
		if (osst_flush_write_buffer(STp, aSRpnt)) {
			printk(KERN_INFO "osst%i:I: Couldn't write header frame\n", dev);
			return (-EIO);
		}
	}
	result = osst_flush_drive_buffer(STp, aSRpnt);
#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: Write onstream header group %s\n", dev, result?"failed":"done");
#endif
	return result;
}

static int osst_write_header(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt, int locate_eod)
{
	os_header_t * header;
	int	      result;
	int	      dev   = TAPE_NR(STp->devt);

#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: Writing tape header\n", dev);
#endif
	if (STp->raw) return 0;

	if (STp->header_cache == NULL) {
		if ((STp->header_cache = (os_header_t *)vmalloc(sizeof(os_header_t))) == NULL) {
			printk(KERN_ERR "osst%i:E: Failed to allocate header cache\n", dev);
			return (-ENOMEM);
		}
		memset(STp->header_cache, 0, sizeof(os_header_t));
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Allocated and cleared memory for header cache\n", dev);
#endif
	}
	if (STp->header_ok) STp->update_frame_cntr++;
	else                STp->update_frame_cntr = 0;

	header = STp->header_cache;
	strcpy(header->ident_str, "ADR_SEQ");
	header->major_rev      = 1;
	header->minor_rev      = 4;
	header->ext_trk_tb_off = htons(17192);
	header->pt_par_num     = 1;
	header->partition[0].partition_num              = OS_DATA_PARTITION;
	header->partition[0].par_desc_ver               = OS_PARTITION_VERSION;
	header->partition[0].wrt_pass_cntr              = htons(STp->wrt_pass_cntr);
	header->partition[0].first_frame_ppos           = htonl(STp->first_data_ppos);
	header->partition[0].last_frame_ppos            = htonl(STp->capacity);
	header->partition[0].eod_frame_ppos             = htonl(STp->eod_frame_ppos);
	header->cfg_col_width                           = htonl(20);
	header->dat_col_width                           = htonl(1500);
	header->qfa_col_width                           = htonl(0);
	header->ext_track_tb.nr_stream_part             = 1;
	header->ext_track_tb.et_ent_sz                  = 32;
	header->ext_track_tb.dat_ext_trk_ey.et_part_num = 0;
	header->ext_track_tb.dat_ext_trk_ey.fmt         = 1;
	header->ext_track_tb.dat_ext_trk_ey.fm_tab_off  = htons(17736);
	header->ext_track_tb.dat_ext_trk_ey.last_hlb_hi = 0;
	header->ext_track_tb.dat_ext_trk_ey.last_hlb    = htonl(STp->eod_frame_lfa);
	header->ext_track_tb.dat_ext_trk_ey.last_pp	= htonl(STp->eod_frame_ppos);
	header->dat_fm_tab.fm_part_num                  = 0;
	header->dat_fm_tab.fm_tab_ent_sz                = 4;
	header->dat_fm_tab.fm_tab_ent_cnt               = htons(STp->filemark_cnt<OS_FM_TAB_MAX?
								STp->filemark_cnt:OS_FM_TAB_MAX);

	result  = __osst_write_header(STp, aSRpnt, 0xbae, 5);
	if (STp->update_frame_cntr == 0)
		    osst_write_filler(STp, aSRpnt, 0xbb3, 5);
	result &= __osst_write_header(STp, aSRpnt,     5, 5);

	if (locate_eod) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Locating back to eod frame addr %d\n", dev, STp->eod_frame_ppos);
#endif
		osst_set_frame_position(STp, aSRpnt, STp->eod_frame_ppos, 0);
	}
	if (result)
		printk(KERN_ERR "osst%i:E: Write header failed\n", dev);
	else {
		memcpy(STp->application_sig, "LIN4", 4);
		STp->linux_media         = 1;
		STp->linux_media_version = 4;
		STp->header_ok           = 1;
	}
	return result;
}

static int osst_reset_header(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt)
{
	if (STp->header_cache != NULL)
		memset(STp->header_cache, 0, sizeof(os_header_t));

	STp->logical_blk_num = STp->frame_seq_number = 0;
	STp->frame_in_buffer = 0;
	STp->eod_frame_ppos = STp->first_data_ppos = 0x0000000A;
	STp->filemark_cnt = 0;
	STp->first_mark_ppos = STp->last_mark_ppos = STp->last_mark_lbn = -1;
	return osst_write_header(STp, aSRpnt, 1);
}

static int __osst_analyze_headers(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt, int ppos)
{
	int           dev = TAPE_NR(STp->devt);
	os_header_t * header;
	os_aux_t    * aux;
	char          id_string[8];
	int	      linux_media_version,
		      update_frame_cntr;

	if (STp->raw)
		return 1;

	if (ppos == 5 || ppos == 0xbae || STp->buffer->syscall_result) {
		if (osst_set_frame_position(STp, aSRpnt, ppos, 0))
			printk(KERN_WARNING "osst%i:W: Couldn't position tape\n", dev);
		osst_wait_ready(STp, aSRpnt, 60 * 15, 0);
		if (osst_initiate_read(STp, aSRpnt)) {
			printk(KERN_WARNING "osst%i:W: Couldn't initiate read\n", dev);
			return 0;
		}
	}
	if (osst_read_frame(STp, aSRpnt, 180)) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Couldn't read header frame\n", dev);
#endif
		return 0;
	}
	header = (os_header_t *) STp->buffer->b_data;	/* warning: only first segment addressable */
	aux = STp->buffer->aux;
	if (aux->frame_type != OS_FRAME_TYPE_HEADER) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Skipping non-header frame (%d)\n", dev, ppos);
#endif
		return 0;
	}
	if (ntohl(aux->frame_seq_num)              != 0                   ||
	    ntohl(aux->logical_blk_num)            != 0                   ||
	          aux->partition.partition_num     != OS_CONFIG_PARTITION ||
	    ntohl(aux->partition.first_frame_ppos) != 0                   ||
	    ntohl(aux->partition.last_frame_ppos)  != 0xbb7               ) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Invalid header frame (%d,%d,%d,%d,%d)\n", dev,
				ntohl(aux->frame_seq_num), ntohl(aux->logical_blk_num),
			       	aux->partition.partition_num, ntohl(aux->partition.first_frame_ppos),
			       	ntohl(aux->partition.last_frame_ppos));
#endif
		return 0;
	}
	if (strncmp(header->ident_str, "ADR_SEQ", 7) != 0 &&
	    strncmp(header->ident_str, "ADR-SEQ", 7) != 0) {
		strncpy(id_string, header->ident_str, 7);
		id_string[7] = 0;
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Invalid header identification string %s\n", dev, id_string);
#endif
		return 0;
	}
	update_frame_cntr = ntohl(aux->update_frame_cntr);
	if (update_frame_cntr < STp->update_frame_cntr) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Skipping frame %d with update_frame_counter %d<%d\n",
				   dev, ppos, update_frame_cntr, STp->update_frame_cntr);
#endif
		return 0;
	}
	if (header->major_rev != 1 || header->minor_rev != 4 ) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: %s revision %d.%d detected (1.4 supported)\n", 
				 dev, (header->major_rev != 1 || header->minor_rev < 2 || 
				       header->minor_rev  > 4 )? "Invalid" : "Warning:",
				 header->major_rev, header->minor_rev);
#endif
		if (header->major_rev != 1 || header->minor_rev < 2 || header->minor_rev > 4)
			return 0;
	}
#if DEBUG
	if (header->pt_par_num != 1)
		printk(KERN_INFO "osst%i:W: %d partitions defined, only one supported\n", 
				 dev, header->pt_par_num);
#endif
	memcpy(id_string, aux->application_sig, 4);
	id_string[4] = 0;
	if (memcmp(id_string, "LIN", 3) == 0) {
		STp->linux_media = 1;
		linux_media_version = id_string[3] - '0';
		if (linux_media_version != 4)
			printk(KERN_INFO "osst%i:I: Linux media version %d detected (current 4)\n",
					 dev, linux_media_version);
	} else {
		printk(KERN_WARNING "osst%i:W: Non Linux media detected (%s)\n", dev, id_string);
		return 0;
	}
	if (linux_media_version < STp->linux_media_version) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Skipping frame %d with linux_media_version %d\n",
				  dev, ppos, linux_media_version);
#endif
		return 0;
	}
	if (linux_media_version > STp->linux_media_version) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Frame %d sets linux_media_version to %d\n",
				   dev, ppos, linux_media_version);
#endif
		memcpy(STp->application_sig, id_string, 5);
		STp->linux_media_version = linux_media_version;
		STp->update_frame_cntr = -1;
	}
	if (update_frame_cntr > STp->update_frame_cntr) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Frame %d sets update_frame_counter to %d\n",
				   dev, ppos, update_frame_cntr);
#endif
		if (STp->header_cache == NULL) {
			if ((STp->header_cache = (os_header_t *)vmalloc(sizeof(os_header_t))) == NULL) {
				printk(KERN_ERR "osst%i:E: Failed to allocate header cache\n", dev);
				return 0;
			}
#if DEBUG
			printk(OSST_DEB_MSG "osst%d:D: Allocated memory for header cache\n", dev);
#endif
		}
		osst_copy_from_buffer(STp->buffer, (unsigned char *)STp->header_cache);
		header = STp->header_cache;	/* further accesses from cached (full) copy */

		STp->wrt_pass_cntr     = ntohs(header->partition[0].wrt_pass_cntr);
		STp->first_data_ppos   = ntohl(header->partition[0].first_frame_ppos);
		STp->eod_frame_ppos    = ntohl(header->partition[0].eod_frame_ppos);
		STp->eod_frame_lfa     = ntohl(header->ext_track_tb.dat_ext_trk_ey.last_hlb);
		STp->filemark_cnt      = ntohl(aux->filemark_cnt);
		STp->first_mark_ppos   = ntohl(aux->next_mark_ppos);
		STp->last_mark_ppos    = ntohl(aux->last_mark_ppos);
		STp->last_mark_lbn     = ntohl(aux->last_mark_lbn);
		STp->update_frame_cntr = update_frame_cntr;
#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: Detected write pass %d, update frame counter %d, filemark counter %d\n",
			  dev, STp->wrt_pass_cntr, STp->update_frame_cntr, STp->filemark_cnt);
	printk(OSST_DEB_MSG "osst%d:D: first data frame on tape = %d, last = %d, eod frame = %d\n", dev,
			  STp->first_data_ppos,
			  ntohl(header->partition[0].last_frame_ppos),
			  ntohl(header->partition[0].eod_frame_ppos));
	printk(OSST_DEB_MSG "osst%d:D: first mark on tape = %d, last = %d, eod frame = %d\n", 
			  dev, STp->first_mark_ppos, STp->last_mark_ppos, STp->eod_frame_ppos);
#endif
		if (header->minor_rev < 4 && STp->linux_media_version == 4) {
#if DEBUG
			printk(OSST_DEB_MSG "osst%i:D: Moving filemark list to ADR 1.4 location\n", dev);
#endif
			memcpy((void *)header->dat_fm_tab.fm_tab_ent, 
			       (void *)header->old_filemark_list, sizeof(header->dat_fm_tab.fm_tab_ent));
			memset((void *)header->old_filemark_list, 0, sizeof(header->old_filemark_list));
		}
		if (header->minor_rev == 4   &&
		    (header->ext_trk_tb_off                          != htons(17192)               ||
		     header->partition[0].partition_num              != OS_DATA_PARTITION          ||
		     header->partition[0].par_desc_ver               != OS_PARTITION_VERSION       ||
		     header->partition[0].last_frame_ppos            != htonl(STp->capacity)       ||
		     header->cfg_col_width                           != htonl(20)                  ||
		     header->dat_col_width                           != htonl(1500)                ||
		     header->qfa_col_width                           != htonl(0)                   ||
		     header->ext_track_tb.nr_stream_part             != 1                          ||
		     header->ext_track_tb.et_ent_sz                  != 32                         ||
		     header->ext_track_tb.dat_ext_trk_ey.et_part_num != OS_DATA_PARTITION          ||
		     header->ext_track_tb.dat_ext_trk_ey.fmt         != 1                          ||
		     header->ext_track_tb.dat_ext_trk_ey.fm_tab_off  != htons(17736)               ||
		     header->ext_track_tb.dat_ext_trk_ey.last_hlb_hi != 0                          ||
		     header->ext_track_tb.dat_ext_trk_ey.last_pp     != htonl(STp->eod_frame_ppos) ||
		     header->dat_fm_tab.fm_part_num                  != OS_DATA_PARTITION          ||
		     header->dat_fm_tab.fm_tab_ent_sz                != 4                          ||
		     header->dat_fm_tab.fm_tab_ent_cnt               !=
			     htons(STp->filemark_cnt<OS_FM_TAB_MAX?STp->filemark_cnt:OS_FM_TAB_MAX)))
			printk(KERN_WARNING "osst%i:W: Failed consistency check ADR 1.4 format\n", dev);

	}

	return 1;
}

static int osst_analyze_headers(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt)
{
	int position, ppos;
	int	first, last;
	int	valid = 0;
	int	dev = TAPE_NR(STp->devt);

	position = osst_get_frame_position(STp, aSRpnt);

	if (STp->raw) {
		STp->header_ok = STp->linux_media = 1;
		STp->linux_media_version = 0;
		return 1;
	}
	STp->header_ok = STp->linux_media = STp->linux_media_version = 0;
	STp->wrt_pass_cntr = STp->update_frame_cntr = -1;
	STp->eod_frame_ppos = STp->first_data_ppos = -1;
	STp->first_mark_ppos = STp->last_mark_ppos = STp->last_mark_lbn = -1;
#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: Reading header\n", dev);
#endif

	/* optimization for speed - if we are positioned at ppos 10, read second group first  */	
	/* TODO try the ADR 1.1 locations for the second group if we have no valid one yet... */

	first = position==10?0xbae: 5;
	last  = position==10?0xbb3:10;

	for (ppos = first; ppos < last; ppos++)
		if (__osst_analyze_headers(STp, aSRpnt, ppos))
			valid = 1;

	first = position==10? 5:0xbae;
	last  = position==10?10:0xbb3;

	for (ppos = first; ppos < last; ppos++)
		if (__osst_analyze_headers(STp, aSRpnt, ppos))
			valid = 1;

	if (!valid) {
		printk(KERN_ERR "osst%i:E: Failed to find valid ADRL header, new media?\n", dev);
		STp->eod_frame_ppos = STp->first_data_ppos = 0;
		osst_set_frame_position(STp, aSRpnt, 10, 0);
		return 0;
	}
	if (position <= STp->first_data_ppos) {
		position = STp->first_data_ppos;
		STp->ps[0].drv_file = STp->ps[0].drv_block = STp->frame_seq_number = STp->logical_blk_num = 0;
	}
	osst_set_frame_position(STp, aSRpnt, position, 0);
	STp->header_ok = 1;

	return 1;
}

static int osst_verify_position(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt)
{
	int	write_pass      = STp->wrt_pass_cntr;
	int	frame_position  = STp->first_frame_position;
	int	frame_seq_numbr = STp->frame_seq_number;
	int	logical_blk_num = STp->logical_blk_num;
	int	halfway_frame   = STp->frame_in_buffer;
	int	read_pointer    = STp->buffer->read_pointer;
	int	prev_mark_ppos  = -1;
	int	actual_mark_ppos, i, n;
#if DEBUG
	int	dev = TAPE_NR(STp->devt);

	printk(OSST_DEB_MSG "osst%d:D: Verify that the tape is really the one we think before writing\n", dev);
#endif
	if (frame_position <= STp->first_data_ppos) {
		/* check header match */
		if (!osst_analyze_headers(STp, aSRpnt) ||
		    (write_pass != STp->wrt_pass_cntr)) {
#if DEBUG
                        printk(OSST_DEB_MSG "osst%d:D: Couldn't match header in verify_position\n", dev);
#endif
                        return (-EIO);
		}
	} else {
		/* find preceding data frame of current write pass */
		osst_set_frame_position(STp, aSRpnt, frame_position - 1, 0);
		if (osst_get_logical_frame(STp, aSRpnt, -1, 0) < 0) {
#if DEBUG
			printk(OSST_DEB_MSG "osst%d:D: Couldn't get logical blk num in verify_position\n", dev);
#endif
			return (-EIO);
		}
	}
	if (STp->linux_media_version >= 4) {
		for (i=0; i<STp->filemark_cnt; i++)
			if ((n=ntohl(STp->header_cache->dat_fm_tab.fm_tab_ent[i])) < frame_position)
				prev_mark_ppos = n;
	} else
		prev_mark_ppos = frame_position - 1;  /* usually - we don't really know */
	actual_mark_ppos = STp->buffer->aux->frame_type == OS_FRAME_TYPE_MARKER ?
				frame_position - 1 : ntohl(STp->buffer->aux->last_mark_ppos);
	if (frame_position  != STp->first_frame_position                   ||
	    frame_seq_numbr != STp->frame_seq_number + (halfway_frame?0:1) ||
	    prev_mark_ppos  != actual_mark_ppos                            ) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Block mismatch: fppos %d-%d, fseq %d-%d, mark %d-%d\n", dev,
				  STp->first_frame_position, frame_position, 
				  STp->frame_seq_number + (halfway_frame?0:1),
				  frame_seq_numbr, actual_mark_ppos, prev_mark_ppos);
#endif
		return (-EIO);
	}
	if (halfway_frame) {
		/* prepare buffer for append and rewrite on top of original */
		osst_set_frame_position(STp, aSRpnt, frame_position - 1, 0);
		STp->buffer->buffer_bytes  = read_pointer;
		STp->ps[STp->partition].rw = ST_WRITING;
		STp->dirty                 = 1;
	}
	STp->frame_in_buffer  = halfway_frame;
	STp->frame_seq_number = frame_seq_numbr;
	STp->logical_blk_num  = logical_blk_num;
	return 0;
}

/* Acc. to OnStream, the vers. numbering is the following:
 * X.XX for released versions (X=digit), 
 * XXXY for unreleased versions (Y=letter)
 * Ordering 1.05 < 106A < 106B < ...  < 106a < ... < 1.06
 * This fn makes monoton numbers out of this scheme ...
 */
static unsigned int osst_parse_firmware_rev (const char * str)
{
	if (str[1] == '.') {
		return (str[0]-'0')*10000
			+(str[2]-'0')*1000
			+(str[3]-'0')*100;
	} else {
		return (str[0]-'0')*10000
			+(str[1]-'0')*1000
			+(str[2]-'0')*100 - 100
			+(str[3]-'@');
	}
}

/*
 * Configure the OnStream SCII tape drive for default operation
 */
static int osst_configure_onstream(OS_Scsi_Tape *STp, Scsi_Request ** aSRpnt)
{
	unsigned char                  cmd[MAX_COMMAND_SIZE];
	int                            dev   = TAPE_NR(STp->devt);
	Scsi_Request                    * SRpnt = * aSRpnt;
	osst_mode_parameter_header_t * header;
	osst_block_size_page_t       * bs;
	osst_capabilities_page_t     * cp;
	osst_tape_paramtr_page_t     * prm;
	int                            drive_buffer_size;

	if (STp->ready != ST_READY) {
#if DEBUG
	    printk(OSST_DEB_MSG "osst%d:D: Not Ready\n", dev);
#endif
	    return (-EIO);
	}
	
	if (STp->os_fw_rev < 10600) {
	    printk(KERN_INFO "osst%i:I: Old OnStream firmware revision detected (%s),\n", dev, STp->device->rev);
	    printk(KERN_INFO "osst%d:I: an upgrade to version 1.06 or above is recommended\n", dev);
	}

	/*
	 * Configure 32.5KB (data+aux) frame size.
         * Get the current frame size from the block size mode page
	 */
	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = MODE_SENSE;
	cmd[1] = 8;
	cmd[2] = BLOCK_SIZE_PAGE;
	cmd[4] = BLOCK_SIZE_PAGE_LENGTH + MODE_HEADER_LENGTH;

	SRpnt = osst_do_scsi(SRpnt, STp, cmd, cmd[4], SCSI_DATA_READ, STp->timeout, 0, TRUE);
	if (SRpnt == NULL) {
#if DEBUG
 	    printk(OSST_DEB_MSG "osst :D: Busy\n");
#endif
	    return (-EBUSY);
	}
	*aSRpnt = SRpnt;
	if ((STp->buffer)->syscall_result != 0) {
	    printk (KERN_ERR "osst%i:E: Can't get tape block size mode page\n", dev);
	    return (-EIO);
	}

	header = (osst_mode_parameter_header_t *) (STp->buffer)->b_data;
	bs = (osst_block_size_page_t *) ((STp->buffer)->b_data + sizeof(osst_mode_parameter_header_t) + header->bdl);

#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: 32KB play back: %s\n",   dev, bs->play32     ? "Yes" : "No");
	printk(OSST_DEB_MSG "osst%d:D: 32.5KB play back: %s\n", dev, bs->play32_5   ? "Yes" : "No");
	printk(OSST_DEB_MSG "osst%d:D: 32KB record: %s\n",      dev, bs->record32   ? "Yes" : "No");
	printk(OSST_DEB_MSG "osst%d:D: 32.5KB record: %s\n",    dev, bs->record32_5 ? "Yes" : "No");
#endif

	/*
	 * Configure default auto columns mode, 32.5KB transfer mode
	 */ 
	bs->one = 1;
	bs->play32 = 0;
	bs->play32_5 = 1;
	bs->record32 = 0;
	bs->record32_5 = 1;

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = MODE_SELECT;
	cmd[1] = 0x10;
	cmd[4] = BLOCK_SIZE_PAGE_LENGTH + MODE_HEADER_LENGTH;

	SRpnt = osst_do_scsi(SRpnt, STp, cmd, cmd[4], SCSI_DATA_WRITE, STp->timeout, 0, TRUE);
	*aSRpnt = SRpnt;
	if ((STp->buffer)->syscall_result != 0) {
	    printk (KERN_ERR "osst%i:E: Couldn't set tape block size mode page\n", dev);
	    return (-EIO);
	}

#if DEBUG
	printk(KERN_INFO "osst%d:D: Drive Block Size changed to 32.5K\n", dev);
	 /*
	 * In debug mode, we want to see as many errors as possible
	 * to test the error recovery mechanism.
	 */
	osst_set_retries(STp, aSRpnt, 0);
	SRpnt = * aSRpnt;
#endif

	/*
	 * Set vendor name to 'LIN4' for "Linux support version 4".
	 */

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = MODE_SELECT;
	cmd[1] = 0x10;
	cmd[4] = VENDOR_IDENT_PAGE_LENGTH + MODE_HEADER_LENGTH;

	header->mode_data_length = VENDOR_IDENT_PAGE_LENGTH + MODE_HEADER_LENGTH - 1;
	header->medium_type      = 0;	/* Medium Type - ignoring */
	header->dsp              = 0;	/* Reserved */
	header->bdl              = 0;	/* Block Descriptor Length */
	
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 0] = VENDOR_IDENT_PAGE | (1 << 7);
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 1] = 6;
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 2] = 'L';
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 3] = 'I';
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 4] = 'N';
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 5] = '4';
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 6] = 0;
	(STp->buffer)->b_data[MODE_HEADER_LENGTH + 7] = 0;

	SRpnt = osst_do_scsi(SRpnt, STp, cmd, cmd[4], SCSI_DATA_WRITE, STp->timeout, 0, TRUE);
	*aSRpnt = SRpnt;

	if ((STp->buffer)->syscall_result != 0) {
	    printk (KERN_ERR "osst%i:E: Couldn't set vendor name to %s\n", dev, 
			(char *) ((STp->buffer)->b_data + MODE_HEADER_LENGTH + 2));
	    return (-EIO);
	}

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = MODE_SENSE;
	cmd[1] = 8;
	cmd[2] = CAPABILITIES_PAGE;
	cmd[4] = CAPABILITIES_PAGE_LENGTH + MODE_HEADER_LENGTH;

	SRpnt = osst_do_scsi(SRpnt, STp, cmd, cmd[4], SCSI_DATA_READ, STp->timeout, 0, TRUE);
	*aSRpnt = SRpnt;

	if ((STp->buffer)->syscall_result != 0) {
	    printk (KERN_ERR "osst%i:E: Can't get capabilities page\n", dev);
	    return (-EIO);
	}

	header = (osst_mode_parameter_header_t *) (STp->buffer)->b_data;
	cp     = (osst_capabilities_page_t    *) ((STp->buffer)->b_data +
		 sizeof(osst_mode_parameter_header_t) + header->bdl);

	drive_buffer_size = ntohs(cp->buffer_size) / 2;

	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = MODE_SENSE;
	cmd[1] = 8;
	cmd[2] = TAPE_PARAMTR_PAGE;
	cmd[4] = TAPE_PARAMTR_PAGE_LENGTH + MODE_HEADER_LENGTH;

	SRpnt = osst_do_scsi(SRpnt, STp, cmd, cmd[4], SCSI_DATA_READ, STp->timeout, 0, TRUE);
	*aSRpnt = SRpnt;

	if ((STp->buffer)->syscall_result != 0) {
	    printk (KERN_ERR "osst%i:E: Can't get tape parameter page\n", dev);
	    return (-EIO);
	}

	header = (osst_mode_parameter_header_t *) (STp->buffer)->b_data;
	prm    = (osst_tape_paramtr_page_t    *) ((STp->buffer)->b_data +
		 sizeof(osst_mode_parameter_header_t) + header->bdl);

	STp->density  = prm->density;
	STp->capacity = ntohs(prm->segtrk) * ntohs(prm->trks);
#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: Density %d, tape length: %dMB, drive buffer size: %dKB\n",
			  dev, STp->density, STp->capacity / 32, drive_buffer_size);
#endif

	return 0;
	
}


/* Step over EOF if it has been inadvertently crossed (ioctl not used because
   it messes up the block number). */
static int cross_eof(OS_Scsi_Tape *STp, Scsi_Request ** aSRpnt, int forward)
{
	int	result;
	int	dev   = TAPE_NR(STp->devt);

#if DEBUG
	if (debugging)
		printk(OSST_DEB_MSG "osst%d:D: Stepping over filemark %s.\n",
	   			  dev, forward ? "forward" : "backward");
#endif

	if (forward) {
	   /* assumes that the filemark is already read by the drive, so this is low cost */
	   result = osst_space_over_filemarks_forward_slow(STp, aSRpnt, MTFSF, 1);
	}
	else
	   /* assumes this is only called if we just read the filemark! */
	   result = osst_seek_logical_blk(STp, aSRpnt, STp->logical_blk_num - 1);

	if (result < 0)
	   printk(KERN_WARNING "osst%d:W: Stepping over filemark %s failed.\n",
				dev, forward ? "forward" : "backward");

	return result;
}


/* Get the tape position. */

static int osst_get_frame_position(OS_Scsi_Tape *STp, Scsi_Request ** aSRpnt)
{
	unsigned char	scmd[MAX_COMMAND_SIZE];
	Scsi_Request  * SRpnt;
	int		result = 0;

	/* KG: We want to be able to use it for checking Write Buffer availability
	 *  and thus don't want to risk to overwrite anything. Exchange buffers ... */
	char		mybuf[24];
	char	      * olddata = STp->buffer->b_data;
	int		oldsize = STp->buffer->buffer_size;
	int		dev     = TAPE_NR(STp->devt);

	if (STp->ready != ST_READY) return (-EIO);

	memset (scmd, 0, MAX_COMMAND_SIZE);
	scmd[0] = READ_POSITION;

	STp->buffer->b_data = mybuf; STp->buffer->buffer_size = 24;
	SRpnt = osst_do_scsi(*aSRpnt, STp, scmd, 20, SCSI_DATA_READ,
				      STp->timeout, MAX_RETRIES, TRUE);
	if (!SRpnt) {
		STp->buffer->b_data = olddata; STp->buffer->buffer_size = oldsize;
		return (-EBUSY);
	}
	*aSRpnt = SRpnt;

	if (STp->buffer->syscall_result)
		result = ((SRpnt->sr_sense_buffer[2] & 0x0f) == 3) ? -EIO : -EINVAL;

	if (result == -EINVAL)
		printk(KERN_ERR "osst%d:E: Can't read tape position.\n", dev);
	else {

		if (result == -EIO) {	/* re-read position */
			unsigned char mysense[16];
			memcpy (mysense, SRpnt->sr_sense_buffer, 16);
			memset (scmd, 0, MAX_COMMAND_SIZE);
			scmd[0] = READ_POSITION;
			STp->buffer->b_data = mybuf; STp->buffer->buffer_size = 24;
			SRpnt = osst_do_scsi(SRpnt, STp, scmd, 20, SCSI_DATA_READ,
						    STp->timeout, MAX_RETRIES, TRUE);
			if (!STp->buffer->syscall_result)
				memcpy (SRpnt->sr_sense_buffer, mysense, 16);
		}
		STp->first_frame_position = ((STp->buffer)->b_data[4] << 24)
					  + ((STp->buffer)->b_data[5] << 16)
					  + ((STp->buffer)->b_data[6] << 8)
					  +  (STp->buffer)->b_data[7];
		STp->last_frame_position  = ((STp->buffer)->b_data[ 8] << 24)
					  + ((STp->buffer)->b_data[ 9] << 16)
					  + ((STp->buffer)->b_data[10] <<  8)
					  +  (STp->buffer)->b_data[11];
		STp->cur_frames           =  (STp->buffer)->b_data[15];
#if DEBUG
		if (debugging) {
			printk(OSST_DEB_MSG "osst%d:D: Drive Positions: host %d, tape %d%s, buffer %d\n", dev,
					    STp->first_frame_position, STp->last_frame_position,
					    ((STp->buffer)->b_data[0]&0x80)?" (BOP)":
					    ((STp->buffer)->b_data[0]&0x40)?" (EOP)":"",
					    STp->cur_frames);
		}
#endif
		if (STp->cur_frames == 0 && STp->first_frame_position != STp->last_frame_position) {
#if DEBUG
			printk(KERN_WARNING "osst%d:D: Correcting read position %d, %d, %d\n", dev,
					STp->first_frame_position, STp->last_frame_position, STp->cur_frames);
#endif
			STp->first_frame_position = STp->last_frame_position;
		}
	}
	STp->buffer->b_data = olddata; STp->buffer->buffer_size = oldsize;

	return (result == 0 ? STp->first_frame_position : result);
}


/* Set the tape block */
static int osst_set_frame_position(OS_Scsi_Tape *STp, Scsi_Request ** aSRpnt, int ppos, int skip)
{
	unsigned char	scmd[MAX_COMMAND_SIZE];
	Scsi_Request  * SRpnt;
	ST_partstat   * STps;
	int		result = 0;
	int		pp  = (ppos == 3000 && !skip)? 0 : ppos;
	int		dev = TAPE_NR(STp->devt);

	if (STp->ready != ST_READY) return (-EIO);

	STps = &(STp->ps[STp->partition]);

	if (ppos < 0 || ppos > STp->capacity) {
		printk(KERN_WARNING "osst%d:W: Reposition request %d out of range\n", dev, ppos);
		pp = ppos = ppos < 0 ? 0 : (STp->capacity - 1);
		result = (-EINVAL);
	}

	do {
#if DEBUG
		if (debugging)
			printk(OSST_DEB_MSG "osst%d:D: Setting ppos to %d.\n", dev, pp);
#endif
		memset (scmd, 0, MAX_COMMAND_SIZE);
		scmd[0] = SEEK_10;
		scmd[1] = 1;
		scmd[3] = (pp >> 24);
		scmd[4] = (pp >> 16);
		scmd[5] = (pp >> 8);
		scmd[6] =  pp;
		if (skip)
			scmd[9] = 0x80;

		SRpnt = osst_do_scsi(*aSRpnt, STp, scmd, 0, SCSI_DATA_NONE, STp->long_timeout,
								MAX_RETRIES, TRUE);
		if (!SRpnt)
			return (-EBUSY);
		*aSRpnt  = SRpnt;

		if ((STp->buffer)->syscall_result != 0) {
#if DEBUG
			printk(OSST_DEB_MSG "osst%d:D: SEEK command from %d to %d failed.\n",
					dev, STp->first_frame_position, pp);
#endif
			result = (-EIO);
		}
		if (pp != ppos)
			osst_wait_ready(STp, aSRpnt, 5 * 60, OSST_WAIT_POSITION_COMPLETE);
	} while ((pp != ppos) && (pp = ppos));
	STp->first_frame_position = STp->last_frame_position = ppos;
	STps->eof = ST_NOEOF;
	STps->at_sm = 0;
	STps->rw = ST_IDLE;
	STp->frame_in_buffer = 0;
	return result;
}

static int osst_write_trailer(OS_Scsi_Tape *STp, Scsi_Request ** aSRpnt, int leave_at_EOT)
{
	ST_partstat * STps = &(STp->ps[STp->partition]);
	int result = 0;

	if (STp->write_type != OS_WRITE_NEW_MARK) {
		/* true unless the user wrote the filemark for us */
		result = osst_flush_drive_buffer(STp, aSRpnt);
		if (result < 0) goto out;
		result = osst_write_filemark(STp, aSRpnt);
		if (result < 0) goto out;

		if (STps->drv_file >= 0)
			STps->drv_file++ ;
		STps->drv_block = 0;
	}
	result = osst_write_eod(STp, aSRpnt);
	osst_write_header(STp, aSRpnt, leave_at_EOT);

	STps->eof = ST_FM;
out:
	return result;
}

/* osst versions of st functions - augmented and stripped to suit OnStream only */

/* Flush the write buffer (never need to write if variable blocksize). */
static int osst_flush_write_buffer(OS_Scsi_Tape *STp, Scsi_Request ** aSRpnt)
{
	int offset, transfer, blks = 0;
	int result = 0;
	unsigned char cmd[MAX_COMMAND_SIZE];
	Scsi_Request * SRpnt = *aSRpnt;
	ST_partstat * STps;
	int dev = TAPE_NR(STp->devt);

	if ((STp->buffer)->writing) {
		if (SRpnt == (STp->buffer)->last_SRpnt)
#if DEBUG
			{ printk(OSST_DEB_MSG
	 "osst%d:D: aSRpnt points to Scsi_Request that write_behind_check will release -- cleared\n", dev);
#endif
			*aSRpnt = SRpnt = NULL;
#if DEBUG
			} else if (SRpnt)
				printk(OSST_DEB_MSG
	 "osst%d:D: aSRpnt does not point to Scsi_Request that write_behind_check will release -- strange\n", dev);
#endif	
		osst_write_behind_check(STp);
		if ((STp->buffer)->syscall_result) {
#if DEBUG
			if (debugging)
				printk(OSST_DEB_MSG "osst%d:D: Async write error (flush) %x.\n",
				       dev, (STp->buffer)->midlevel_result);
#endif
			if ((STp->buffer)->midlevel_result == INT_MAX)
				return (-ENOSPC);
			return (-EIO);
		}
	}

	result = 0;
	if (STp->dirty == 1) {

		STp->write_count++;
		STps     = &(STp->ps[STp->partition]);
		STps->rw = ST_WRITING;
		offset   = STp->buffer->buffer_bytes;
		blks     = (offset + STp->block_size - 1) / STp->block_size;
		transfer = OS_FRAME_SIZE;
		
		if (offset < OS_DATA_SIZE)
			osst_zero_buffer_tail(STp->buffer);

		/* TODO: Error handling! */
		if (STp->poll)
			result = osst_wait_frame (STp, aSRpnt, STp->first_frame_position, -50, 120);

		memset(cmd, 0, MAX_COMMAND_SIZE);
		cmd[0] = WRITE_6;
		cmd[1] = 1;
		cmd[4] = 1;

		switch	(STp->write_type) {
		   case OS_WRITE_DATA:
#if DEBUG
   			if (debugging)
				printk(OSST_DEB_MSG "osst%d:D: Writing %d blocks to frame %d, lblks %d-%d\n",
					dev, blks, STp->frame_seq_number, 
					STp->logical_blk_num - blks, STp->logical_blk_num - 1);
#endif
			osst_init_aux(STp, OS_FRAME_TYPE_DATA, STp->frame_seq_number++,
				      STp->logical_blk_num - blks, STp->block_size, blks);
			break;
		   case OS_WRITE_EOD:
			osst_init_aux(STp, OS_FRAME_TYPE_EOD, STp->frame_seq_number++,
				      STp->logical_blk_num, 0, 0);
			break;
		   case OS_WRITE_NEW_MARK:
			osst_init_aux(STp, OS_FRAME_TYPE_MARKER, STp->frame_seq_number++,
				      STp->logical_blk_num++, 0, blks=1);
			break;
		   case OS_WRITE_HEADER:
			osst_init_aux(STp, OS_FRAME_TYPE_HEADER, 0, 0, 0, blks=0);
			break;
		default: /* probably FILLER */
			osst_init_aux(STp, OS_FRAME_TYPE_FILL, 0, 0, 0, 0);
		}
#if DEBUG
		if (debugging)
			printk(OSST_DEB_MSG "osst%d:D: Flushing %d bytes, Transfering %d bytes in %d lblocks.\n",
			  			 dev, offset, transfer, blks);
#endif

		SRpnt = osst_do_scsi(*aSRpnt, STp, cmd, transfer, SCSI_DATA_WRITE,
					  STp->timeout, MAX_WRITE_RETRIES, TRUE);
		*aSRpnt = SRpnt;
		if (!SRpnt)
			return (-EBUSY);

		if ((STp->buffer)->syscall_result != 0) {
#if DEBUG
			printk(OSST_DEB_MSG
				"osst%d:D: write sense [0]=0x%02x [2]=%02x [12]=%02x [13]=%02x\n",
				dev, SRpnt->sr_sense_buffer[0], SRpnt->sr_sense_buffer[2],
				SRpnt->sr_sense_buffer[12], SRpnt->sr_sense_buffer[13]);
#endif
			if ((SRpnt->sr_sense_buffer[0] & 0x70) == 0x70 &&
			    (SRpnt->sr_sense_buffer[2] & 0x40) && /* FIXME - SC-30 drive doesn't assert EOM bit */
			    (SRpnt->sr_sense_buffer[2] & 0x0f) == NO_SENSE) {
				STp->dirty = 0;
				(STp->buffer)->buffer_bytes = 0;
				result = (-ENOSPC);
			}
			else {
				if (osst_write_error_recovery(STp, aSRpnt, 1)) {
					printk(KERN_ERR "osst%d:E: Error on flush write.\n", dev);
					result = (-EIO);
				}
			}
			STps->drv_block = (-1);		/* FIXME - even if write recovery succeeds? */
		}
		else {
			STp->first_frame_position++;
			STp->dirty = 0;
			(STp->buffer)->buffer_bytes = 0;
		}
	}
#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: Exit flush write buffer with code %d\n", dev, result);
#endif
	return result;
}


/* Flush the tape buffer. The tape will be positioned correctly unless
   seek_next is true. */
static int osst_flush_buffer(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt, int seek_next)
{
	ST_partstat   * STps;
	int backspace = 0, result = 0;
#if DEBUG
	int dev = TAPE_NR(STp->devt);
#endif

	/*
	 * If there was a bus reset, block further access
	 * to this device.
	 */
	if( STp->device->was_reset )
		return (-EIO);

	if (STp->ready != ST_READY)
		return 0;

	STps = &(STp->ps[STp->partition]);
	if (STps->rw == ST_WRITING || STp->dirty) {	/* Writing */
		STp->write_type = OS_WRITE_DATA;
		return osst_flush_write_buffer(STp, aSRpnt);
	}
	if (STp->block_size == 0)
		return 0;

#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: Reached flush (read) buffer\n", dev);
#endif

	if (!STp->can_bsr) {
		backspace = ((STp->buffer)->buffer_bytes + (STp->buffer)->read_pointer) / STp->block_size -
			    ((STp->buffer)->read_pointer + STp->block_size - 1        ) / STp->block_size ;
		(STp->buffer)->buffer_bytes = 0;
		(STp->buffer)->read_pointer = 0;
		STp->frame_in_buffer = 0;		/* FIXME is this relevant w. OSST? */
	}

	if (!seek_next) {
		if (STps->eof == ST_FM_HIT) {
			result = cross_eof(STp, aSRpnt, FALSE); /* Back over the EOF hit */
			if (!result)
				STps->eof = ST_NOEOF;
			else {
				if (STps->drv_file >= 0)
					STps->drv_file++;
				STps->drv_block = 0;
			}
		}
		if (!result && backspace > 0)	/* TODO -- design and run a test case for this */
			result = osst_seek_logical_blk(STp, aSRpnt, STp->logical_blk_num - backspace);
	}
	else if (STps->eof == ST_FM_HIT) {
		if (STps->drv_file >= 0)
			STps->drv_file++;
		STps->drv_block = 0;
		STps->eof = ST_NOEOF;
	}

	return result;
}

static int osst_write_frame(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt, int synchronous)
{
	unsigned char	cmd[MAX_COMMAND_SIZE];
	Scsi_Request     * SRpnt;
	int		blks;
#if DEBUG
	int		dev = TAPE_NR(STp->devt);
#endif

	if ((!STp-> raw) && (STp->first_frame_position == 0xbae)) { /* _must_ preserve buffer! */
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Reaching config partition.\n", dev);
#endif
		if (osst_flush_drive_buffer(STp, aSRpnt) < 0) {
			return (-EIO);
		}
		/* error recovery may have bumped us past the header partition */
		if (osst_get_frame_position(STp, aSRpnt) < 0xbb8) {
#if DEBUG
			printk(OSST_DEB_MSG "osst%d:D: Skipping over config partition.\n", dev);
#endif
		osst_position_tape_and_confirm(STp, aSRpnt, 0xbb8);
		}
	}

	if (STp->poll)
		osst_wait_frame (STp, aSRpnt, STp->first_frame_position, -50, 60);
	/* TODO: Check for an error ! */

//	osst_build_stats(STp, &SRpnt);

	STp->ps[STp->partition].rw = ST_WRITING;
	STp->write_type            = OS_WRITE_DATA;
			
	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0]   = WRITE_6;
	cmd[1]   = 1;
	cmd[4]   = 1;						/* one frame at a time... */
	blks     = STp->buffer->buffer_bytes / STp->block_size;
#if DEBUG
	if (debugging)
		printk(OSST_DEB_MSG "osst%d:D: Writing %d blocks to frame %d, lblks %d-%d\n", dev, blks, 
			STp->frame_seq_number, STp->logical_blk_num - blks, STp->logical_blk_num - 1);
#endif
	osst_init_aux(STp, OS_FRAME_TYPE_DATA, STp->frame_seq_number++,
		      STp->logical_blk_num - blks, STp->block_size, blks);

#if DEBUG
	if (!synchronous)
		STp->write_pending = 1;
#endif
	SRpnt = osst_do_scsi(*aSRpnt, STp, cmd, OS_FRAME_SIZE, SCSI_DATA_WRITE, STp->timeout,
							MAX_WRITE_RETRIES, synchronous);
	if (!SRpnt)
		return (-EBUSY);
	*aSRpnt = SRpnt;

	if (synchronous) {
		if (STp->buffer->syscall_result != 0) {
#if DEBUG
			if (debugging)
				printk(OSST_DEB_MSG "osst%d:D: Error on write:\n", dev);
#endif
			if ((SRpnt->sr_sense_buffer[0] & 0x70) == 0x70 &&
			    (SRpnt->sr_sense_buffer[2] & 0x40)) {
				if ((SRpnt->sr_sense_buffer[2] & 0x0f) == VOLUME_OVERFLOW)
					return (-ENOSPC);
			}
			else {
				if (osst_write_error_recovery(STp, aSRpnt, 1))
					return (-EIO);
			}
		}
		else
			STp->first_frame_position++;
	}

	STp->write_count++;

	return 0;
}


/* Entry points to osst */

/* Write command */
static ssize_t osst_write(struct file * filp, const char * buf, size_t count, loff_t *ppos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	ssize_t total, retval = 0;
	ssize_t i, do_count, blks, transfer;
	int write_threshold;
	int doing_write = 0;
	const char *b_point;
	Scsi_Request * SRpnt = NULL;
	OS_Scsi_Tape * STp;
	ST_mode * STm;
	ST_partstat * STps;
	int dev = TAPE_NR(inode->i_rdev);

	STp = os_scsi_tapes[dev];

	if (down_interruptible(&STp->lock))
		return (-ERESTARTSYS);

	/*
	 * If we are in the middle of error recovery, don't let anyone
	 * else try and use this device.  Also, if error recovery fails, it
	 * may try and take the device offline, in which case all further
	 * access to the device is prohibited.
	 */
	if( !scsi_block_when_processing_errors(STp->device) ) {
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

#if DEBUG
	if (!STp->in_use) {
		printk(OSST_DEB_MSG "osst%d:D: Incorrect device.\n", dev);
		retval = (-EIO);
		goto out;
	}
#endif

	if (STp->write_prot) {
		retval = (-EACCES);
		goto out;
	}

	/* Write must be integral number of blocks */
	if (STp->block_size != 0 && (count % STp->block_size) != 0) {
		printk(KERN_ERR "osst%d:E: Write (%ld bytes) not multiple of tape block size (%d%c).\n",
				       dev, (unsigned long)count, STp->block_size<1024?
				       STp->block_size:STp->block_size/1024, STp->block_size<1024?'b':'k');
		retval = (-EINVAL);
		goto out;
	}

	if (STp->first_frame_position >= STp->capacity - OSST_EOM_RESERVE) {
		printk(KERN_ERR "osst%d:E: Write truncated at EOM early warning (frame %d).\n",
				       dev, STp->first_frame_position);
		retval = (-ENOSPC);
		goto out;
	}

	STps = &(STp->ps[STp->partition]);

	if (STp->do_auto_lock && STp->door_locked == ST_UNLOCKED &&
	    !osst_int_ioctl(STp, &SRpnt, MTLOCK, 0))
		STp->door_locked = ST_LOCKED_AUTO;


	if (STps->rw == ST_READING) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Switching from read to write at file %d, block %d\n", dev, 
					STps->drv_file, STps->drv_block);
#endif
		retval = osst_flush_buffer(STp, &SRpnt, 0);
		if (retval)
			goto out;
		STps->rw = ST_IDLE;
	}
	if (STps->rw != ST_WRITING) {
		/* Are we totally rewriting this tape? */
		if (!STp->header_ok ||
		    (STp->first_frame_position == STp->first_data_ppos && STps->drv_block < 0) ||
		    (STps->drv_file == 0 && STps->drv_block == 0)) {
			STp->wrt_pass_cntr++;
#if DEBUG
			printk(OSST_DEB_MSG "osst%d:D: Allocating next write pass counter: %d\n",
						  dev, STp->wrt_pass_cntr);
#endif
			osst_reset_header(STp, &SRpnt);
			STps->drv_file = STps->drv_block = 0;
		}
		/* Do we know where we'll be writing on the tape? */
		else {
			if ((STp->fast_open && osst_verify_position(STp, &SRpnt)) ||
			  		STps->drv_file < 0 || STps->drv_block < 0) {
				if (STp->first_frame_position == STp->eod_frame_ppos) {	/* at EOD */
			  		STps->drv_file = STp->filemark_cnt;
			  		STps->drv_block = 0;
				}
				else {
					/* We have no idea where the tape is positioned - give up */
#if DEBUG
					printk(OSST_DEB_MSG
						"osst%d:D: Cannot write at indeterminate position.\n", dev);
#endif
					retval = (-EIO);
					goto out;
				}
      			}	  
			if ((STps->drv_file + STps->drv_block) > 0 && STps->drv_file < STp->filemark_cnt) {
				STp->filemark_cnt = STps->drv_file;
				STp->last_mark_ppos =
				       	ntohl(STp->header_cache->dat_fm_tab.fm_tab_ent[STp->filemark_cnt-1]);
				printk(KERN_WARNING
					"osst%d:W: Overwriting file %d with old write pass counter %d\n",
						dev, STps->drv_file, STp->wrt_pass_cntr);
				printk(KERN_WARNING
					"osst%d:W: may lead to stale data being accepted on reading back!\n",
						dev);
#if DEBUG
				printk(OSST_DEB_MSG
				  "osst%d:D: resetting filemark count to %d and last mark ppos,lbn to %d,%d\n",
					dev, STp->filemark_cnt, STp->last_mark_ppos, STp->last_mark_lbn);
#endif
			}
		}
		STp->fast_open = FALSE;
	}
	if (!STp->header_ok) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Write cannot proceed without valid headers\n", dev);
#endif
		retval = (-EIO);
		goto out;
	}

	if ((STp->buffer)->writing) {
if (SRpnt) printk(KERN_ERR "osst%d:A: Not supposed to have SRpnt at line %d\n", dev, __LINE__);
		osst_write_behind_check(STp);
		if ((STp->buffer)->syscall_result) {
#if DEBUG
		if (debugging)
			printk(OSST_DEB_MSG "osst%d:D: Async write error (write) %x.\n", dev,
						 (STp->buffer)->midlevel_result);
#endif
		if ((STp->buffer)->midlevel_result == INT_MAX)
			STps->eof = ST_EOM_OK;
		else
			STps->eof = ST_EOM_ERROR;
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
	if ((copy_from_user(&i, buf, 1) != 0 ||
	     copy_from_user(&i, buf + count - 1, 1) != 0)) {
		retval = (-EFAULT);
		goto out;
	}

	if (!STm->do_buffer_writes) {
		write_threshold = 1;
	}
	else
		write_threshold = (STp->buffer)->buffer_blocks * STp->block_size;
	if (!STm->do_async_writes)
		write_threshold--;

	total = count;
#if DEBUG
	if (debugging)
		printk(OSST_DEB_MSG "osst%d:D: Writing %d bytes to file %d block %d lblk %d fseq %d fppos %d\n",
				dev, count, STps->drv_file, STps->drv_block,
				STp->logical_blk_num, STp->frame_seq_number, STp->first_frame_position);
#endif
	b_point = buf;
	while ((STp->buffer)->buffer_bytes + count > write_threshold)
	{
		doing_write = 1;
		do_count = (STp->buffer)->buffer_blocks * STp->block_size -
			   (STp->buffer)->buffer_bytes;
		if (do_count > count)
			do_count = count;

		i = append_to_buffer(b_point, STp->buffer, do_count);
		if (i) {
			retval = i;
			goto out;
		}

		blks = do_count / STp->block_size;
		STp->logical_blk_num += blks;  /* logical_blk_num is incremented as data is moved from user */
  
		i = osst_write_frame(STp, &SRpnt, TRUE);

		if (i == (-ENOSPC)) {
			transfer = STp->buffer->writing;	/* FIXME -- check this logic */
			if (transfer <= do_count) {
				filp->f_pos += do_count - transfer;
				count -= do_count - transfer;
				if (STps->drv_block >= 0) {
					STps->drv_block += (do_count - transfer) / STp->block_size;
				}
				STps->eof = ST_EOM_OK;
				retval = (-ENOSPC);		/* EOM within current request */
#if DEBUG
				if (debugging)
				      printk(OSST_DEB_MSG "osst%d:D: EOM with %d bytes unwritten.\n",
							     dev, transfer);
#endif
			}
			else {
				STps->eof = ST_EOM_ERROR;
				STps->drv_block = (-1);		/* Too cautious? */
				retval = (-EIO);		/* EOM for old data */
#if DEBUG
				if (debugging)
				      printk(OSST_DEB_MSG "osst%d:D: EOM with lost data.\n", dev);
#endif
			}
		}
		else
			retval = i;
			
		if (retval < 0) {
			if (SRpnt != NULL) {
				scsi_release_request(SRpnt);
				SRpnt = NULL;
			}
			STp->buffer->buffer_bytes = 0;
			STp->dirty = 0;
			if (count < total)
				retval = total - count;
			goto out;
		}

		filp->f_pos += do_count;
		b_point += do_count;
		count -= do_count;
		if (STps->drv_block >= 0) {
			STps->drv_block += blks;
		}
		STp->buffer->buffer_bytes = 0;
		STp->dirty = 0;
	}  /* end while write threshold exceeded */

	if (count != 0) {
		STp->dirty = 1;
		i = append_to_buffer(b_point, STp->buffer, count);
		if (i) {
			retval = i;
			goto out;
		}
		blks = count / STp->block_size;
		STp->logical_blk_num += blks;
		if (STps->drv_block >= 0) {
			STps->drv_block += blks;
		}
		filp->f_pos += count;
		count = 0;
	}

	if (doing_write && (STp->buffer)->syscall_result != 0) {
		retval = (STp->buffer)->syscall_result;
		goto out;
	}

	if (STm->do_async_writes && ((STp->buffer)->buffer_bytes >= STp->write_threshold)) { 
		/* Schedule an asynchronous write */
		(STp->buffer)->writing = ((STp->buffer)->buffer_bytes /
					   STp->block_size) * STp->block_size;
		STp->dirty = !((STp->buffer)->writing ==
				          (STp->buffer)->buffer_bytes);

		i = osst_write_frame(STp, &SRpnt, FALSE);
		if (i < 0) {
			retval = (-EIO);
			goto out;
		}
		SRpnt = NULL;			/* Prevent releasing this request! */
	}
	STps->at_sm &= (total == 0);
	if (total > 0)
		STps->eof = ST_NOEOF;

	retval = total;

out:
	if (SRpnt != NULL) scsi_release_request(SRpnt);

	up(&STp->lock);

	return retval;
}


/* Read command */
static ssize_t osst_read(struct file * filp, char * buf, size_t count, loff_t *ppos)
{
	struct inode * inode = filp->f_dentry->d_inode;
	ssize_t total, retval = 0;
	ssize_t i, transfer;
	int special;
	OS_Scsi_Tape * STp;
	ST_mode * STm;
	ST_partstat * STps;
	Scsi_Request *SRpnt = NULL;
	int dev = TAPE_NR(inode->i_rdev);

	STp = os_scsi_tapes[dev];

	if (down_interruptible(&STp->lock))
		return (-ERESTARTSYS);

	/*
	 * If we are in the middle of error recovery, don't let anyone
	 * else try and use this device.  Also, if error recovery fails, it
	 * may try and take the device offline, in which case all further
	 * access to the device is prohibited.
	 */
	if( !scsi_block_when_processing_errors(STp->device) ) {
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
#if DEBUG
	if (!STp->in_use) {
		printk(OSST_DEB_MSG "osst%d:D: Incorrect device.\n", dev);
		retval = (-EIO);
		goto out;
	}
#endif
	/* Must have initialized medium */
	if (!STp->header_ok) {
		retval = (-EIO);
		goto out;
	}

	if (STp->do_auto_lock && STp->door_locked == ST_UNLOCKED &&
	!osst_int_ioctl(STp, &SRpnt, MTLOCK, 0))
		STp->door_locked = ST_LOCKED_AUTO;

	STps = &(STp->ps[STp->partition]);
	if (STps->rw == ST_WRITING) {
		retval = osst_flush_buffer(STp, &SRpnt, 0);
		if (retval)
			goto out;
		STps->rw = ST_IDLE;
		/* FIXME -- this may leave the tape without EOD and up2date headers */
	}

	if ((count % STp->block_size) != 0) {
		printk(KERN_WARNING
		    "osst%d:W: Read (%Zd bytes) not multiple of tape block size (%d%c).\n", dev, count,
		    STp->block_size<1024?STp->block_size:STp->block_size/1024, STp->block_size<1024?'b':'k');
	}

#if DEBUG
	if (debugging && STps->eof != ST_NOEOF)
		printk(OSST_DEB_MSG "osst%d:D: EOF/EOM flag up (%d). Bytes %d\n", dev,
				     STps->eof, (STp->buffer)->buffer_bytes);
#endif
	if ((STp->buffer)->buffer_bytes == 0 &&
	     STps->eof >= ST_EOD_1) {
		if (STps->eof < ST_EOD) {
			STps->eof += 1;
			retval = 0;
			goto out;
		}
		retval = (-EIO);  /* EOM or Blank Check */
		goto out;
	}

	/* Check the buffer writability before any tape movement. Don't alter
		 buffer data. */
	if (copy_from_user(&i, buf, 1)             != 0 ||
	    copy_to_user  (buf, &i, 1)             != 0 ||
	    copy_from_user(&i, buf + count - 1, 1) != 0 ||
	    copy_to_user  (buf + count - 1, &i, 1) != 0) {
		retval = (-EFAULT);
		goto out;
	}

	/* Loop until enough data in buffer or a special condition found */
	for (total = 0, special = 0; total < count - STp->block_size + 1 && !special; ) {

		/* Get new data if the buffer is empty */
		if ((STp->buffer)->buffer_bytes == 0) {
			if (STps->eof == ST_FM_HIT)
				break;
			special = osst_get_logical_frame(STp, &SRpnt, STp->frame_seq_number, 0);
			if (special < 0) { 			/* No need to continue read */
				STp->frame_in_buffer = 0;
				retval = special;
				goto out;
			}
		}

		/* Move the data from driver buffer to user buffer */
		if ((STp->buffer)->buffer_bytes > 0) {
#if DEBUG
			if (debugging && STps->eof != ST_NOEOF)
			    printk(OSST_DEB_MSG "osst%d:D: EOF up (%d). Left %d, needed %d.\n", dev,
						 STps->eof, (STp->buffer)->buffer_bytes, count - total);
#endif
		       	/* force multiple of block size, note block_size may have been adjusted */
			transfer = (((STp->buffer)->buffer_bytes < count - total ?
				     (STp->buffer)->buffer_bytes : count - total)/
					STp->block_size) * STp->block_size;

			if (transfer == 0) {
				printk(KERN_WARNING
			    "osst%d:W: Nothing can be transfered, requested %d, tape block size (%d%c).\n",
			   		dev, count, STp->block_size < 1024?
					STp->block_size:STp->block_size/1024,
				       	STp->block_size<1024?'b':'k');
				break;
			}
			i = from_buffer(STp->buffer, buf, transfer);
			if (i)  {
				retval = i;
				goto out;
			}
			STp->logical_blk_num += transfer / STp->block_size;
			STps->drv_block      += transfer / STp->block_size;
			filp->f_pos          += transfer;
			buf                  += transfer;
			total                += transfer;
		}
 
		if ((STp->buffer)->buffer_bytes == 0) {
#if DEBUG
			if (debugging)
				printk(OSST_DEB_MSG "osst%d:D: Finished with frame %d\n",
					       	dev, STp->frame_seq_number);
#endif
			STp->frame_in_buffer = 0;
			STp->frame_seq_number++;              /* frame to look for next time */
		}
	} /* for (total = 0, special = 0; total < count && !special; ) */

	/* Change the eof state if no data from tape or buffer */
	if (total == 0) {
		if (STps->eof == ST_FM_HIT) {
			STps->eof = (STp->first_frame_position >= STp->eod_frame_ppos)?ST_EOD_2:ST_FM;
			STps->drv_block = 0;
			if (STps->drv_file >= 0)
				STps->drv_file++;
		}
		else if (STps->eof == ST_EOD_1) {
			STps->eof = ST_EOD_2;
			if (STps->drv_block > 0 && STps->drv_file >= 0)
				STps->drv_file++;
			STps->drv_block = 0;
		}
		else if (STps->eof == ST_EOD_2)
			STps->eof = ST_EOD;
	}
	else if (STps->eof == ST_FM)
		STps->eof = ST_NOEOF;

	retval = total;

out:
	if (SRpnt != NULL) scsi_release_request(SRpnt);

	up(&STp->lock);

	return retval;
}


/* Set the driver options */
static void osst_log_options(OS_Scsi_Tape *STp, ST_mode *STm, int dev)
{
  printk(KERN_INFO
"osst%d:I: Mode %d options: buffer writes: %d, async writes: %d, read ahead: %d\n",
	 dev, STp->current_mode, STm->do_buffer_writes, STm->do_async_writes,
	 STm->do_read_ahead);
  printk(KERN_INFO
"osst%d:I:    can bsr: %d, two FMs: %d, fast mteom: %d, auto lock: %d,\n",
	 dev, STp->can_bsr, STp->two_fm, STp->fast_mteom, STp->do_auto_lock);
  printk(KERN_INFO
"osst%d:I:    defs for wr: %d, no block limits: %d, partitions: %d, s2 log: %d\n",
	 dev, STm->defaults_for_writes, STp->omit_blklims, STp->can_partitions,
	 STp->scsi2_logical);
  printk(KERN_INFO
"osst%d:I:    sysv: %d\n", dev, STm->sysv);
#if DEBUG
  printk(KERN_INFO
	 "osst%d:D:    debugging: %d\n",
	 dev, debugging);
#endif
}


static int osst_set_options(OS_Scsi_Tape *STp, long options)
{
	int value;
	long code;
	ST_mode *STm;
	int dev = TAPE_NR(STp->devt);

	STm = &(STp->modes[STp->current_mode]);
	if (!STm->defined) {
		memcpy(STm, &(STp->modes[0]), sizeof(ST_mode));
		modes_defined = TRUE;
#if DEBUG
		if (debugging)
			printk(OSST_DEB_MSG "osst%d:D: Initialized mode %d definition from mode 0\n",
					     dev, STp->current_mode);
#endif
	}

	code = options & MT_ST_OPTIONS;
	if (code == MT_ST_BOOLEANS) {
		STm->do_buffer_writes = (options & MT_ST_BUFFER_WRITES) != 0;
		STm->do_async_writes  = (options & MT_ST_ASYNC_WRITES) != 0;
		STm->defaults_for_writes = (options & MT_ST_DEF_WRITES) != 0;
		STm->do_read_ahead    = (options & MT_ST_READ_AHEAD) != 0;
		STp->two_fm	      = (options & MT_ST_TWO_FM) != 0;
		STp->fast_mteom	      = (options & MT_ST_FAST_MTEOM) != 0;
		STp->do_auto_lock     = (options & MT_ST_AUTO_LOCK) != 0;
		STp->can_bsr          = (options & MT_ST_CAN_BSR) != 0;
		STp->omit_blklims     = (options & MT_ST_NO_BLKLIMS) != 0;
		if ((STp->device)->scsi_level >= SCSI_2)
			STp->can_partitions = (options & MT_ST_CAN_PARTITIONS) != 0;
		STp->scsi2_logical    = (options & MT_ST_SCSI2LOGICAL) != 0;
		STm->sysv	      = (options & MT_ST_SYSV) != 0;
#if DEBUG
		debugging = (options & MT_ST_DEBUGGING) != 0;
#endif
		osst_log_options(STp, STm, dev);
	}
	else if (code == MT_ST_SETBOOLEANS || code == MT_ST_CLEARBOOLEANS) {
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
		if ((options & MT_ST_SYSV) != 0)
			STm->sysv = value;
#if DEBUG
		if ((options & MT_ST_DEBUGGING) != 0)
			debugging = value;
#endif
		osst_log_options(STp, STm, dev);
	}
	else if (code == MT_ST_WRITE_THRESHOLD) {
		value = (options & ~MT_ST_OPTIONS) * ST_KILOBYTE;
		if (value < 1 || value > osst_buffer_size) {
			printk(KERN_WARNING "osst%d:W: Write threshold %d too small or too large.\n",
					     dev, value);
			return (-EIO);
		}
		STp->write_threshold = value;
		printk(KERN_INFO "osst%d:I: Write threshold set to %d bytes.\n",
				  dev, value);
	}
	else if (code == MT_ST_DEF_BLKSIZE) {
		value = (options & ~MT_ST_OPTIONS);
		if (value == ~MT_ST_OPTIONS) {
			STm->default_blksize = (-1);
			printk(KERN_INFO "osst%d:I: Default block size disabled.\n", dev);
		}
		else {
			if (value < 512 || value > OS_DATA_SIZE || OS_DATA_SIZE % value) {
				printk(KERN_WARNING "osst%d:W: Default block size cannot be set to %d.\n",
							 dev, value);
				return (-EINVAL);
			}
			STm->default_blksize = value;
			printk(KERN_INFO "osst%d:I: Default block size set to %d bytes.\n",
					  dev, STm->default_blksize);
		}
	}
	else if (code == MT_ST_TIMEOUTS) {
		value = (options & ~MT_ST_OPTIONS);
		if ((value & MT_ST_SET_LONG_TIMEOUT) != 0) {
			STp->long_timeout = (value & ~MT_ST_SET_LONG_TIMEOUT) * HZ;
			printk(KERN_INFO "osst%d:I: Long timeout set to %d seconds.\n", dev,
					     (value & ~MT_ST_SET_LONG_TIMEOUT));
		}
		else {
			STp->timeout = value * HZ;
			printk(KERN_INFO "osst%d:I: Normal timeout set to %d seconds.\n", dev, value);
		}
	}
	else if (code == MT_ST_DEF_OPTIONS) {
		code = (options & ~MT_ST_CLEAR_DEFAULT);
		value = (options & MT_ST_CLEAR_DEFAULT);
		if (code == MT_ST_DEF_DENSITY) {
			if (value == MT_ST_CLEAR_DEFAULT) {
				STm->default_density = (-1);
				printk(KERN_INFO "osst%d:I: Density default disabled.\n", dev);
			}
			else {
				STm->default_density = value & 0xff;
				printk(KERN_INFO "osst%d:I: Density default set to %x\n",
						  dev, STm->default_density);
			}
		}
		else if (code == MT_ST_DEF_DRVBUFFER) {
			if (value == MT_ST_CLEAR_DEFAULT) {
				STp->default_drvbuffer = 0xff;
				printk(KERN_INFO "osst%d:I: Drive buffer default disabled.\n", dev);
			}
			else {
				STp->default_drvbuffer = value & 7;
				printk(KERN_INFO "osst%d:I: Drive buffer default set to %x\n",
						  dev, STp->default_drvbuffer);
			}
		}
		else if (code == MT_ST_DEF_COMPRESSION) {
			if (value == MT_ST_CLEAR_DEFAULT) {
				STm->default_compression = ST_DONT_TOUCH;
				printk(KERN_INFO "osst%d:I: Compression default disabled.\n", dev);
			}
			else {
				STm->default_compression = (value & 1 ? ST_YES : ST_NO);
				printk(KERN_INFO "osst%d:I: Compression default set to %x\n",
						  dev, (value & 1));
			}
		}
	}
	else
		return (-EIO);

	return 0;
}


/* Internal ioctl function */
static int osst_int_ioctl(OS_Scsi_Tape * STp, Scsi_Request ** aSRpnt, unsigned int cmd_in, unsigned long arg)
{
	int timeout;
	long ltmp;
	int i, ioctl_result;
	int chg_eof = TRUE;
	unsigned char cmd[MAX_COMMAND_SIZE];
	Scsi_Request * SRpnt = * aSRpnt;
	ST_partstat * STps;
	int fileno, blkno, at_sm, frame_seq_numbr, logical_blk_num;
	int datalen = 0, direction = SCSI_DATA_NONE;
	int dev = TAPE_NR(STp->devt);

	if (STp->ready != ST_READY && cmd_in != MTLOAD) {
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
	frame_seq_numbr = STp->frame_seq_number;
	logical_blk_num = STp->logical_blk_num;

	memset(cmd, 0, MAX_COMMAND_SIZE);
	switch (cmd_in) {
	 case MTFSFM:
		chg_eof = FALSE; /* Changed from the FSF after this */
	 case MTFSF:
		if (STp->raw)
		   return (-EIO);
		if (STp->linux_media)
		   ioctl_result = osst_space_over_filemarks_forward_fast(STp, &SRpnt, cmd_in, arg);
		else
		   ioctl_result = osst_space_over_filemarks_forward_slow(STp, &SRpnt, cmd_in, arg);
		if (fileno >= 0)
		   fileno += arg;
		blkno = 0;
		at_sm &= (arg == 0);
		goto os_bypass;

	 case MTBSF:
		chg_eof = FALSE; /* Changed from the FSF after this */
	 case MTBSFM:
		if (STp->raw)
		   return (-EIO);
		ioctl_result = osst_space_over_filemarks_backward(STp, &SRpnt, cmd_in, arg);
		if (fileno >= 0)
		   fileno -= arg;
		blkno = (-1);  /* We can't know the block number */
		at_sm &= (arg == 0);
		goto os_bypass;

	 case MTFSR:
	 case MTBSR:
#if DEBUG
		if (debugging)
		   printk(OSST_DEB_MSG "osst%d:D: Skipping %lu blocks %s from logical block %d\n",
				dev, arg, cmd_in==MTFSR?"forward":"backward", logical_blk_num);
#endif
		if (cmd_in == MTFSR) {
		   logical_blk_num += arg;
		   if (blkno >= 0) blkno += arg;
		}
		else {
		   logical_blk_num -= arg;
		   if (blkno >= 0) blkno -= arg;
		}
		ioctl_result = osst_seek_logical_blk(STp, &SRpnt, logical_blk_num);
		fileno = STps->drv_file;
		blkno  = STps->drv_block;
		at_sm &= (arg == 0);
		goto os_bypass;

	 case MTFSS:
		cmd[0] = SPACE;
		cmd[1] = 0x04; /* Space Setmarks */   /* FIXME -- OS can't do this? */
		cmd[2] = (arg >> 16);
		cmd[3] = (arg >> 8);
		cmd[4] = arg;
#if DEBUG
		if (debugging)
			printk(OSST_DEB_MSG "osst%d:D: Spacing tape forward %d setmarks.\n", dev,
		cmd[2] * 65536 + cmd[3] * 256 + cmd[4]);
#endif
		if (arg != 0) {
			blkno = fileno = (-1);
			at_sm = 1;
		}
		break;
	 case MTBSS:
		cmd[0] = SPACE;
		cmd[1] = 0x04; /* Space Setmarks */   /* FIXME -- OS can't do this? */
		ltmp = (-arg);
		cmd[2] = (ltmp >> 16);
		cmd[3] = (ltmp >> 8);
		cmd[4] = ltmp;
#if DEBUG
		if (debugging) {
			if (cmd[2] & 0x80)
			   ltmp = 0xff000000;
			ltmp = ltmp | (cmd[2] << 16) | (cmd[3] << 8) | cmd[4];
			printk(OSST_DEB_MSG "osst%d:D: Spacing tape backward %ld setmarks.\n",
						dev, (-ltmp));
		 }
#endif
		 if (arg != 0) {
			blkno = fileno = (-1);
			at_sm = 1;
		 }
		 break;
	 case MTWEOF:
		 if ((STps->rw == ST_WRITING || STp->dirty) && !(STp->device)->was_reset) {
			STp->write_type = OS_WRITE_DATA;
			ioctl_result = osst_flush_write_buffer(STp, &SRpnt);
		 } else
			ioctl_result = 0;
#if DEBUG
		 if (debugging) 
			   printk(OSST_DEB_MSG "osst%d:D: Writing %ld filemark(s).\n", dev, arg);
#endif
		 for (i=0; i<arg; i++)
			ioctl_result |= osst_write_filemark(STp, &SRpnt);
		 if (fileno >= 0) fileno += arg;
		 if (blkno  >= 0) blkno   = 0;
		 goto os_bypass;

	 case MTWSM:
		 if (STp->write_prot)
			return (-EACCES);
		 if (!STp->raw)
			return 0;
		 cmd[0] = WRITE_FILEMARKS;   /* FIXME -- need OS version */
		 if (cmd_in == MTWSM)
			 cmd[1] = 2;
		 cmd[2] = (arg >> 16);
		 cmd[3] = (arg >> 8);
		 cmd[4] = arg;
		 timeout = STp->timeout;
#if DEBUG
		 if (debugging) 
			   printk(OSST_DEB_MSG "osst%d:D: Writing %d setmark(s).\n", dev,
				  cmd[2] * 65536 + cmd[3] * 256 + cmd[4]);
#endif
		 if (fileno >= 0)
			fileno += arg;
		 blkno = 0;
		 at_sm = (cmd_in == MTWSM);
		 break;
	 case MTOFFL:
	 case MTLOAD:
	 case MTUNLOAD:
	 case MTRETEN:
		 cmd[0] = START_STOP;
		 cmd[1] = 1;			/* Don't wait for completion */
		 if (cmd_in == MTLOAD) {
		     if (STp->ready == ST_NO_TAPE)
			 cmd[4] = 4;		/* open tray */
		      else
			 cmd[4] = 1;		/* load */
		 }
		 if (cmd_in == MTRETEN)
			 cmd[4] = 3;		/* retension then mount */
		 if (cmd_in == MTOFFL)
			 cmd[4] = 4;		/* rewind then eject */
		 timeout = STp->timeout;
#if DEBUG
		 if (debugging) {
			 switch (cmd_in) {
				 case MTUNLOAD:
					 printk(OSST_DEB_MSG "osst%d:D: Unloading tape.\n", dev);
					 break;
				 case MTLOAD:
					 printk(OSST_DEB_MSG "osst%d:D: Loading tape.\n", dev);
					 break;
				 case MTRETEN:
					 printk(OSST_DEB_MSG "osst%d:D: Retensioning tape.\n", dev);
					 break;
				 case MTOFFL:
					 printk(OSST_DEB_MSG "osst%d:D: Ejecting tape.\n", dev);
					 break;
			 }
		 }
#endif
       fileno = blkno = at_sm = frame_seq_numbr = logical_blk_num = 0 ;
		 break;
	 case MTNOP:
#if DEBUG
		 if (debugging)
			 printk(OSST_DEB_MSG "osst%d:D: No-op on tape.\n", dev);
#endif
		 return 0;  /* Should do something ? */
		 break;
	 case MTEOM:
#if DEBUG
		if (debugging)
		   printk(OSST_DEB_MSG "osst%d:D: Spacing to end of recorded medium.\n", dev);
#endif
		if ((osst_position_tape_and_confirm(STp, &SRpnt, STp->eod_frame_ppos) < 0) ||
			    (osst_get_logical_frame(STp, &SRpnt, -1, 0)               < 0)) {
		   ioctl_result = -EIO;
		   goto os_bypass;
		}
		if (STp->buffer->aux->frame_type != OS_FRAME_TYPE_EOD) {
#if DEBUG
		   printk(OSST_DEB_MSG "osst%d:D: No EOD frame found where expected.\n", dev);
#endif
		   ioctl_result = -EIO;
		   goto os_bypass;
		}
		ioctl_result = osst_set_frame_position(STp, &SRpnt, STp->eod_frame_ppos, 0);
		fileno = STp->filemark_cnt;
		blkno  = at_sm = 0;
		goto os_bypass;

	 case MTERASE:
		if (STp->write_prot)
		   return (-EACCES);
		ioctl_result = osst_reset_header(STp, &SRpnt);
		i = osst_write_eod(STp, &SRpnt);
		if (i < ioctl_result) ioctl_result = i;
		i = osst_position_tape_and_confirm(STp, &SRpnt, STp->eod_frame_ppos);
		if (i < ioctl_result) ioctl_result = i;
		fileno = blkno = at_sm = 0 ;
		goto os_bypass;

	 case MTREW:
		cmd[0] = REZERO_UNIT; /* rewind */
		cmd[1] = 1;
#if DEBUG
		if (debugging)
		   printk(OSST_DEB_MSG "osst%d:D: Rewinding tape, Immed=%d.\n", dev, cmd[1]);
#endif
		fileno = blkno = at_sm = frame_seq_numbr = logical_blk_num = 0 ;
		break;

	 case MTLOCK:
		chg_eof = FALSE;
		cmd[0] = ALLOW_MEDIUM_REMOVAL;
		cmd[4] = SCSI_REMOVAL_PREVENT;
#if DEBUG
		if (debugging)
		    printk(OSST_DEB_MSG "osst%d:D: Locking drive door.\n", dev);
#endif
		break;

	 case MTUNLOCK:
		chg_eof = FALSE;
		cmd[0] = ALLOW_MEDIUM_REMOVAL;
		cmd[4] = SCSI_REMOVAL_ALLOW;
#if DEBUG
		if (debugging)
		   printk(OSST_DEB_MSG "osst%d:D: Unlocking drive door.\n", dev);
#endif
	break;

	 case MTSETBLK:           /* Set block length */
		 if ((STps->drv_block == 0 )			  &&
		     !STp->dirty				  &&
		     ((STp->buffer)->buffer_bytes == 0)		  &&
		     ((arg & MT_ST_BLKSIZE_MASK) >= 512 )	  && 
		     ((arg & MT_ST_BLKSIZE_MASK) <= OS_DATA_SIZE) &&
		     !(OS_DATA_SIZE % (arg & MT_ST_BLKSIZE_MASK))  ) {
			 /*
			  * Only allowed to change the block size if you opened the
			  * device at the beginning of a file before writing anything.
			  * Note, that when reading, changing block_size is futile,
			  * as the size used when writing overrides it.
			  */
			 STp->block_size = (arg & MT_ST_BLKSIZE_MASK);
			 printk(KERN_INFO "osst%d:I: Block size set to %d bytes.\n",
					   dev, STp->block_size);
			 return 0;
		 }
	 case MTSETDENSITY:       /* Set tape density */
	 case MTSETDRVBUFFER:     /* Set drive buffering */
	 case SET_DENS_AND_BLK:   /* Set density and block size */
		 chg_eof = FALSE;
		 if (STp->dirty || (STp->buffer)->buffer_bytes != 0)
			 return (-EIO);       /* Not allowed if data in buffer */
		 if ((cmd_in == MTSETBLK || cmd_in == SET_DENS_AND_BLK) &&
		     (arg & MT_ST_BLKSIZE_MASK) != 0                    &&
		     (arg & MT_ST_BLKSIZE_MASK) != STp->block_size       ) {
			 printk(KERN_WARNING "osst%d:W: Illegal to set block size to %d%s.\n",
					      dev, (int)(arg & MT_ST_BLKSIZE_MASK),
					      (OS_DATA_SIZE % (arg & MT_ST_BLKSIZE_MASK))?"":" now");
			 return (-EINVAL);
		 }
		 return 0;  /* FIXME silently ignore if block size didn't change */

	 default:
		return (-ENOSYS);
	}

	SRpnt = osst_do_scsi(SRpnt, STp, cmd, datalen, direction, timeout, MAX_RETRIES, TRUE);

	ioctl_result = (STp->buffer)->syscall_result;

	if (!SRpnt) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Couldn't exec scsi cmd for IOCTL\n", dev);
#endif
		return ioctl_result;
	}

	if (!ioctl_result) {  /* SCSI command successful */
		STp->frame_seq_number = frame_seq_numbr;
		STp->logical_blk_num  = logical_blk_num;
	}

os_bypass:
#if DEBUG
	if (debugging)
		printk(OSST_DEB_MSG "osst%d:D: IOCTL (%d) Result=%d\n", dev, cmd_in, ioctl_result);
#endif

	if (!ioctl_result) {				/* success */

		if (cmd_in == MTFSFM) {
			 fileno--;
			 blkno--;
		}
		if (cmd_in == MTBSFM) {
			 fileno++;
			 blkno++;
		}
		STps->drv_block = blkno;
		STps->drv_file = fileno;
		STps->at_sm = at_sm;

		if (cmd_in == MTLOCK)
			 STp->door_locked = ST_LOCKED_EXPLICIT;
		else if (cmd_in == MTUNLOCK)
			STp->door_locked = ST_UNLOCKED;

		if (cmd_in == MTEOM)
			STps->eof = ST_EOD;
		else if ((cmd_in == MTFSFM || cmd_in == MTBSF) && STps->eof == ST_FM_HIT) {
			ioctl_result = osst_seek_logical_blk(STp, &SRpnt, STp->logical_blk_num-1);
			STps->drv_block++;
			STp->logical_blk_num++;
			STp->frame_seq_number++;
			STp->frame_in_buffer = 0;
			STp->buffer->read_pointer = 0;
		}
		else if (cmd_in == MTFSF)
			STps->eof = (STp->first_frame_position >= STp->eod_frame_ppos)?ST_EOD:ST_FM;
		else if (chg_eof)
			STps->eof = ST_NOEOF;

		if (cmd_in == MTOFFL || cmd_in == MTUNLOAD)
			STp->rew_at_close = 0;
		else if (cmd_in == MTLOAD) {
			for (i=0; i < ST_NBR_PARTITIONS; i++) {
			    STp->ps[i].rw = ST_IDLE;
			    STp->ps[i].last_block_valid = FALSE;/* FIXME - where else is this field maintained? */
			}
			STp->partition = 0;
		}

		if (cmd_in == MTREW) {
			ioctl_result = osst_position_tape_and_confirm(STp, &SRpnt, STp->first_data_ppos); 
			if (ioctl_result > 0)
				ioctl_result = 0;
		}

	} else if (cmd_in == MTBSF || cmd_in == MTBSFM ) {
		if (osst_position_tape_and_confirm(STp, &SRpnt, STp->first_data_ppos) < 0)
			STps->drv_file = STps->drv_block = -1;
		else
			STps->drv_file = STps->drv_block = 0;
		STps->eof = ST_NOEOF;
	} else if (cmd_in == MTFSF || cmd_in == MTFSFM) {
		if (osst_position_tape_and_confirm(STp, &SRpnt, STp->eod_frame_ppos) < 0)
			STps->drv_file = STps->drv_block = -1;
		else {
			STps->drv_file  = STp->filemark_cnt;
			STps->drv_block = 0;
		}
		STps->eof = ST_EOD;
	} else if (cmd_in == MTBSR || cmd_in == MTFSR || cmd_in == MTWEOF || cmd_in == MTEOM) {
		STps->drv_file = STps->drv_block = (-1);
		STps->eof = ST_NOEOF;
		STp->header_ok = 0;
	} else if (cmd_in == MTERASE) {
		STp->header_ok = 0;
	} else if (SRpnt) {  /* SCSI command was not completely successful. */
		if (SRpnt->sr_sense_buffer[2] & 0x40) {
			STps->eof = ST_EOM_OK;
			STps->drv_block = 0;
		}
		if (chg_eof)
			STps->eof = ST_NOEOF;

		if ((SRpnt->sr_sense_buffer[2] & 0x0f) == BLANK_CHECK)
			STps->eof = ST_EOD;

		if (cmd_in == MTLOCK)
			STp->door_locked = ST_LOCK_FAILS;

		if (cmd_in == MTLOAD && osst_wait_for_medium(STp, &SRpnt, 60))
			ioctl_result = osst_wait_ready(STp, &SRpnt, 5 * 60, OSST_WAIT_POSITION_COMPLETE);
	}
	*aSRpnt = SRpnt;

#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: Ioctl %s, ppos %d fseq %d lblk %d bytes %d file %d blk %d\n", dev,
			ioctl_result?"fail":"success", STp->first_frame_position, STp->frame_seq_number,
		       	STp->logical_blk_num, STp->buffer->buffer_bytes, STps->drv_file, STps->drv_block);
#endif
	return ioctl_result;
}


/* Open the device */
static int os_scsi_tape_open(struct inode * inode, struct file * filp)
{
	unsigned short flags;
	int i, b_size, need_dma_buffer, new_session = FALSE, retval = 0;
	unsigned char cmd[MAX_COMMAND_SIZE];
	Scsi_Request * SRpnt;
	OS_Scsi_Tape * STp;
	ST_mode * STm;
	ST_partstat * STps;
	int dev = TAPE_NR(inode->i_rdev);
	int mode = TAPE_MODE(inode->i_rdev);

	if (dev >= osst_template.dev_max || (STp = os_scsi_tapes[dev]) == NULL || !STp->device)
		return (-ENXIO);

	if( !scsi_block_when_processing_errors(STp->device) ) {
		return -ENXIO;
	}

	if (STp->in_use) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Device already in use.\n", dev);
#endif
		return (-EBUSY);
	}
	STp->in_use       = 1;
	STp->rew_at_close = (MINOR(inode->i_rdev) & 0x80) == 0;

	if (STp->device->host->hostt->module)
		 __MOD_INC_USE_COUNT(STp->device->host->hostt->module);
	if (osst_template.module)
		 __MOD_INC_USE_COUNT(osst_template.module);
	STp->device->access_count++;

	if (mode != STp->current_mode) {
#if DEBUG
		if (debugging)
			printk(OSST_DEB_MSG "osst%d:D: Mode change from %d to %d.\n",
					       dev, STp->current_mode, mode);
#endif
		new_session = TRUE;
		STp->current_mode = mode;
	}
	STm = &(STp->modes[STp->current_mode]);

	flags = filp->f_flags;
	STp->write_prot = ((flags & O_ACCMODE) == O_RDONLY);

	STp->raw = (MINOR(inode->i_rdev) & 0x40) != 0;
	if (STp->raw)
		STp->header_ok = 0;

	/* Allocate a buffer for this user */
	need_dma_buffer = STp->restr_dma;
	for (i=0; i < osst_nbr_buffers; i++)
		if (!osst_buffers[i]->in_use &&
		   (!need_dma_buffer || osst_buffers[i]->dma))
			break;
	if (i >= osst_nbr_buffers) {
		STp->buffer = new_tape_buffer(FALSE, need_dma_buffer);
		if (STp->buffer == NULL) {
			printk(KERN_WARNING "osst%d:W: Can't allocate tape buffer.\n", dev);
			retval = (-EBUSY);
			goto err_out;
		}
	}
	else
		STp->buffer = osst_buffers[i];
	(STp->buffer)->in_use = 1;
	(STp->buffer)->writing = 0;
	(STp->buffer)->syscall_result = 0;
	(STp->buffer)->use_sg = STp->device->host->sg_tablesize;

	/* Compute the usable buffer size for this SCSI adapter */
	if (!(STp->buffer)->use_sg)
	(STp->buffer)->buffer_size = (STp->buffer)->sg[0].length;
	else {
		for (i=0, (STp->buffer)->buffer_size = 0; i < (STp->buffer)->use_sg &&
		     i < (STp->buffer)->sg_segs; i++)
			(STp->buffer)->buffer_size += (STp->buffer)->sg[i].length;
	}

	STp->dirty = 0;
	for (i=0; i < ST_NBR_PARTITIONS; i++) {
		STps = &(STp->ps[i]);
		STps->rw = ST_IDLE;
	}
	STp->ready = ST_READY;
#if DEBUG
	STp->nbr_waits = STp->nbr_finished = 0;
#endif

	memset (cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = TEST_UNIT_READY;

	SRpnt = osst_do_scsi(NULL, STp, cmd, 0, SCSI_DATA_NONE, STp->timeout, MAX_READY_RETRIES, TRUE);
	if (!SRpnt) {
		retval = (STp->buffer)->syscall_result;
		goto err_out;
	}
	if ((SRpnt->sr_sense_buffer[0] & 0x70) == 0x70      &&
	    (SRpnt->sr_sense_buffer[2] & 0x0f) == NOT_READY &&
	     SRpnt->sr_sense_buffer[12]        == 4         ) {
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Unit not ready, cause %x\n", dev, SRpnt->sr_sense_buffer[13]);
#endif
		if (SRpnt->sr_sense_buffer[13] == 2) {	/* initialize command required (LOAD) */
			memset (cmd, 0, MAX_COMMAND_SIZE);
        		cmd[0] = START_STOP;
			cmd[1] = 1;
			cmd[4] = 1;
			SRpnt = osst_do_scsi(SRpnt, STp, cmd, 0, SCSI_DATA_NONE,
					     STp->timeout, MAX_READY_RETRIES, TRUE);
		}
		osst_wait_ready(STp, &SRpnt, (SRpnt->sr_sense_buffer[13]==1?15:3) * 60, 0);
	}
	if ((SRpnt->sr_sense_buffer[0] & 0x70) == 0x70 &&
	    (SRpnt->sr_sense_buffer[2] & 0x0f) == UNIT_ATTENTION) { /* New media? */
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Unit wants attention\n", dev);
#endif
		STp->header_ok = 0;

		for (i=0; i < 10; i++) {

			memset (cmd, 0, MAX_COMMAND_SIZE);
			cmd[0] = TEST_UNIT_READY;

			SRpnt = osst_do_scsi(SRpnt, STp, cmd, 0, SCSI_DATA_NONE,
					     STp->timeout, MAX_READY_RETRIES, TRUE);
			if ((SRpnt->sr_sense_buffer[0] & 0x70) != 0x70 ||
			    (SRpnt->sr_sense_buffer[2] & 0x0f) != UNIT_ATTENTION)
				break;
		}

		STp->device->was_reset = 0;
		STp->partition = STp->new_partition = 0;
		if (STp->can_partitions)
			STp->nbr_partitions = 1;  /* This guess will be updated later if necessary */
		for (i=0; i < ST_NBR_PARTITIONS; i++) {
			STps = &(STp->ps[i]);
			STps->rw = ST_IDLE;		/* FIXME - seems to be redundant... */
			STps->eof = ST_NOEOF;
			STps->at_sm = 0;
			STps->last_block_valid = FALSE;
			STps->drv_block = 0;
			STps->drv_file = 0 ;
		}
		new_session = TRUE;
		STp->recover_count = 0;
	}
	/*
	 * if we have valid headers from before, and the drive/tape seem untouched,
	 * open without reconfiguring and re-reading the headers
	 */
	if (!STp->buffer->syscall_result && STp->header_ok &&
	    !SRpnt->sr_result && SRpnt->sr_sense_buffer[0] == 0) {

		memset(cmd, 0, MAX_COMMAND_SIZE);
		cmd[0] = MODE_SENSE;
		cmd[1] = 8;
		cmd[2] = VENDOR_IDENT_PAGE;
		cmd[4] = VENDOR_IDENT_PAGE_LENGTH + MODE_HEADER_LENGTH;

		SRpnt = osst_do_scsi(SRpnt, STp, cmd, cmd[4], SCSI_DATA_READ, STp->timeout, 0, TRUE);

		if (STp->buffer->syscall_result                     ||
		    STp->buffer->b_data[MODE_HEADER_LENGTH + 2] != 'L' ||
		    STp->buffer->b_data[MODE_HEADER_LENGTH + 3] != 'I' ||
		    STp->buffer->b_data[MODE_HEADER_LENGTH + 4] != 'N' ||
		    STp->buffer->b_data[MODE_HEADER_LENGTH + 5] != '4'  ) {
#if DEBUG
			printk(OSST_DEB_MSG "osst%d:D: Signature was changed to %c%c%c%c\n", dev,
			  STp->buffer->b_data[MODE_HEADER_LENGTH + 2],
			  STp->buffer->b_data[MODE_HEADER_LENGTH + 3],
			  STp->buffer->b_data[MODE_HEADER_LENGTH + 4],
			  STp->buffer->b_data[MODE_HEADER_LENGTH + 5]);
#endif
			STp->header_ok = 0;
		}
		i = STp->first_frame_position;
		if (STp->header_ok && i == osst_get_frame_position(STp, &SRpnt)) {
			if (STp->door_locked == ST_UNLOCKED) {
				if (osst_int_ioctl(STp, &SRpnt, MTLOCK, 0))
					printk(KERN_INFO "osst%d:I: Can't lock drive door\n", dev);
				else
					STp->door_locked = ST_LOCKED_AUTO;
			}
			if (!STp->frame_in_buffer) {
				STp->block_size = (STm->default_blksize > 0) ?
							STm->default_blksize : OS_DATA_SIZE;
				STp->buffer->buffer_bytes = STp->buffer->read_pointer = 0;
			}
			STp->buffer->buffer_blocks = OS_DATA_SIZE / STp->block_size;
			STp->fast_open = TRUE;
			scsi_release_request(SRpnt);
			return 0;
		}
#if DEBUG
		if (i != STp->first_frame_position)
			printk(OSST_DEB_MSG "osst%d:D: Tape position changed from %d to %d\n",
						dev, i, STp->first_frame_position);
#endif
		STp->header_ok = 0;
	}
	STp->fast_open = FALSE;

	if ((STp->buffer)->syscall_result != 0 &&   /* in all error conditions except no medium */ 
	    (SRpnt->sr_sense_buffer[2] != 2 || SRpnt->sr_sense_buffer[12] != 0x3A) ) {

		memset(cmd, 0, MAX_COMMAND_SIZE);
		cmd[0] = MODE_SELECT;
		cmd[1] = 0x10;
		cmd[4] = 4 + MODE_HEADER_LENGTH;

		(STp->buffer)->b_data[0] = cmd[4] - 1;
		(STp->buffer)->b_data[1] = 0;			/* Medium Type - ignoring */
		(STp->buffer)->b_data[2] = 0;			/* Reserved */
		(STp->buffer)->b_data[3] = 0;			/* Block Descriptor Length */
		(STp->buffer)->b_data[MODE_HEADER_LENGTH + 0] = 0x3f;
		(STp->buffer)->b_data[MODE_HEADER_LENGTH + 1] = 1;
		(STp->buffer)->b_data[MODE_HEADER_LENGTH + 2] = 2;
		(STp->buffer)->b_data[MODE_HEADER_LENGTH + 3] = 3;

#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: Applying soft reset\n", dev);
#endif
		SRpnt = osst_do_scsi(SRpnt, STp, cmd, cmd[4], SCSI_DATA_WRITE, STp->timeout, 0, TRUE);

		STp->header_ok = 0;

		for (i=0; i < 10; i++) {

			memset (cmd, 0, MAX_COMMAND_SIZE);
			cmd[0] = TEST_UNIT_READY;

			SRpnt = osst_do_scsi(SRpnt, STp, cmd, 0, SCSI_DATA_NONE,
					     STp->timeout, MAX_READY_RETRIES, TRUE);
			if ((SRpnt->sr_sense_buffer[0] & 0x70) != 0x70 ||
			    (SRpnt->sr_sense_buffer[2] & 0x0f) == NOT_READY)
			break;

			if ((SRpnt->sr_sense_buffer[2] & 0x0f) == UNIT_ATTENTION) {
				STp->device->was_reset = 0;
				STp->partition = STp->new_partition = 0;
				if (STp->can_partitions)
					STp->nbr_partitions = 1;  /* This guess will be updated later if necessary */
				for (i=0; i < ST_NBR_PARTITIONS; i++) {
					STps = &(STp->ps[i]);
					STps->rw = ST_IDLE;
					STps->eof = ST_NOEOF;
					STps->at_sm = 0;
					STps->last_block_valid = FALSE;
					STps->drv_block = 0;
					STps->drv_file = 0 ;
				}
				new_session = TRUE;
			}
		}
	}

	if (osst_wait_ready(STp, &SRpnt, 15 * 60, 0))		/* FIXME - not allowed with NOBLOCK */
		 printk(KERN_INFO "osst%i:I: Device did not become Ready in open\n",dev);

	if ((STp->buffer)->syscall_result != 0) {
		if ((STp->device)->scsi_level >= SCSI_2 &&
		    (SRpnt->sr_sense_buffer[0] & 0x70) == 0x70 &&
		    (SRpnt->sr_sense_buffer[2] & 0x0f) == NOT_READY &&
		     SRpnt->sr_sense_buffer[12] == 0x3a) { /* Check ASC */
			STp->ready = ST_NO_TAPE;
		} else
			STp->ready = ST_NOT_READY;
		scsi_release_request(SRpnt);
		SRpnt = NULL;
		STp->density = 0;   	/* Clear the erroneous "residue" */
		STp->write_prot = 0;
		STp->block_size = 0;
		STp->ps[0].drv_file = STp->ps[0].drv_block = (-1);
		STp->partition = STp->new_partition = 0;
		STp->door_locked = ST_UNLOCKED;
		return 0;
	}

	osst_configure_onstream(STp, &SRpnt);

/*	STp->drv_write_prot = ((STp->buffer)->b_data[2] & 0x80) != 0; FIXME */

	if (OS_FRAME_SIZE > (STp->buffer)->buffer_size &&
	    !enlarge_buffer(STp->buffer, OS_FRAME_SIZE, STp->restr_dma)) {
		printk(KERN_NOTICE "osst%d:A: Framesize %d too large for buffer.\n", dev,
				     OS_FRAME_SIZE);
		retval = (-EIO);
		goto err_out;
	}

	if ((STp->buffer)->buffer_size >= OS_FRAME_SIZE) {
		for (i = 0, b_size = 0; 
		     i < STp->buffer->sg_segs && (b_size + STp->buffer->sg[i].length) <= OS_DATA_SIZE; 
		     b_size += STp->buffer->sg[i++].length);
		STp->buffer->aux = (os_aux_t *) (STp->buffer->sg[i].address + OS_DATA_SIZE - b_size);
#if DEBUG
		printk(OSST_DEB_MSG "osst%d:D: b_data points to %p in segment 0 at %p\n", dev,
			STp->buffer->b_data, STp->buffer->sg[0].address);
		printk(OSST_DEB_MSG "osst%d:D: AUX points to %p in segment %d at %p\n", dev,
			 STp->buffer->aux, i, STp->buffer->sg[i].address);
#endif
	} else
		STp->buffer->aux = NULL; /* this had better never happen! */

	STp->block_size = STp->raw ? OS_FRAME_SIZE : (
			     (STm->default_blksize > 0) ? STm->default_blksize : OS_DATA_SIZE);
	STp->buffer->buffer_blocks = STp->raw ? 1 : OS_DATA_SIZE / STp->block_size;
	STp->buffer->buffer_bytes  =
	STp->buffer->read_pointer  =
	STp->frame_in_buffer       = 0;

#if DEBUG
	if (debugging)
		printk(OSST_DEB_MSG "osst%d:D: Block size: %d, frame size: %d, buffer size: %d (%d blocks).\n",
		     dev, STp->block_size, OS_FRAME_SIZE, (STp->buffer)->buffer_size,
		     (STp->buffer)->buffer_blocks);
#endif

	if (STp->drv_write_prot) {
		STp->write_prot = 1;
#if DEBUG
		if (debugging)
			printk(OSST_DEB_MSG "osst%d:D: Write protected\n", dev);
#endif
		if ((flags & O_ACCMODE) == O_WRONLY || (flags & O_ACCMODE) == O_RDWR) {
			retval = (-EROFS);
			goto err_out;
		}
	}

	if (new_session) {  /* Change the drive parameters for the new mode */
#if DEBUG
		if (debugging)
	printk(OSST_DEB_MSG "osst%d:D: New Session\n", dev);
#endif
		STp->density_changed = STp->blksize_changed = FALSE;
		STp->compression_changed = FALSE;
	}

	/*
	 * properly position the tape and check the ADR headers
	 */
	if (STp->door_locked == ST_UNLOCKED) {
		 if (osst_int_ioctl(STp, &SRpnt, MTLOCK, 0))
			printk(KERN_INFO "osst%d:I: Can't lock drive door\n", dev);
		 else
			STp->door_locked = ST_LOCKED_AUTO;
	}

	osst_analyze_headers(STp, &SRpnt);

	scsi_release_request(SRpnt);
	SRpnt = NULL;

	return 0;

err_out:
	if (SRpnt != NULL)
		scsi_release_request(SRpnt);
	if (STp->buffer != NULL) {
		STp->buffer->in_use = 0;
		STp->buffer = NULL;
	}
	STp->in_use = 0;
	STp->header_ok = 0;
	STp->device->access_count--;

	if (STp->device->host->hostt->module)
	    __MOD_DEC_USE_COUNT(STp->device->host->hostt->module);
	if (osst_template.module)
	    __MOD_DEC_USE_COUNT(osst_template.module);

	return retval;
}


/* Flush the tape buffer before close */
static int os_scsi_tape_flush(struct file * filp)
{
	int result = 0, result2;
	OS_Scsi_Tape * STp;
	ST_mode * STm;
	ST_partstat * STps;
	Scsi_Request *SRpnt = NULL;

	struct inode *inode = filp->f_dentry->d_inode;
	kdev_t devt = inode->i_rdev;
	int dev;

	if (file_count(filp) > 1)
	return 0;

	dev = TAPE_NR(devt);
	STp = os_scsi_tapes[dev];
	STm = &(STp->modes[STp->current_mode]);
	STps = &(STp->ps[STp->partition]);

	if ((STps->rw == ST_WRITING || STp->dirty) && !(STp->device)->was_reset) {
		STp->write_type = OS_WRITE_DATA;
		result = osst_flush_write_buffer(STp, &SRpnt);
		if (result != 0 && result != (-ENOSPC))
			goto out;
	}
	if ( STps->rw >= ST_WRITING && !(STp->device)->was_reset) {

#if DEBUG
		if (debugging) {
			printk(OSST_DEB_MSG "osst%d:D: File length %ld bytes.\n",
					       dev, (long)(filp->f_pos));
			printk(OSST_DEB_MSG "osst%d:D: Async write waits %d, finished %d.\n",
					       dev, STp->nbr_waits, STp->nbr_finished);
		}
#endif
		result = osst_write_trailer(STp, &SRpnt, !(STp->rew_at_close));
#if DEBUG
		if (debugging)
			printk(OSST_DEB_MSG "osst%d:D: Buffer flushed, %d EOF(s) written\n",
					       dev, 1+STp->two_fm);
#endif
	}
	else if (!STp->rew_at_close) {
		STps = &(STp->ps[STp->partition]);
		if (!STm->sysv || STps->rw != ST_READING) {
			if (STp->can_bsr)
				result = osst_flush_buffer(STp, &SRpnt, 0); /* this is the default path */
			else if (STps->eof == ST_FM_HIT) {
				result = cross_eof(STp, &SRpnt, FALSE);
					if (result) {
						if (STps->drv_file >= 0)
							STps->drv_file++;
						STps->drv_block = 0;
						STps->eof = ST_FM;
					}
					else
						STps->eof = ST_NOEOF;
			}
		}
		else if ((STps->eof == ST_NOEOF &&
			  !(result = cross_eof(STp, &SRpnt, TRUE))) ||
			 STps->eof == ST_FM_HIT) {
			if (STps->drv_file >= 0)
				STps->drv_file++;
			STps->drv_block = 0;
			STps->eof = ST_FM;
		}
	}

out:
	if (STp->rew_at_close) {
		result2 = osst_position_tape_and_confirm(STp, &SRpnt, STp->first_data_ppos);
		STps->drv_file = STps->drv_block = STp->frame_seq_number = STp->logical_blk_num = 0;
		if (result == 0 && result2 < 0)
			result = result2;
	}
	if (SRpnt) scsi_release_request(SRpnt);

	if (STp->recover_count) {
		printk(KERN_INFO "osst%d:I: %d recovered errors in", dev, STp->recover_count);
		if (STp->write_count)
			printk(" %d frames written", STp->write_count);
		if (STp->read_count)
			printk(" %d frames read", STp->read_count);
		printk("\n");
		STp->recover_count = 0;
	}
	STp->write_count = 0;
	STp->read_count  = 0;

	return result;
}


/* Close the device and release it */
static int os_scsi_tape_close(struct inode * inode, struct file * filp)
{
	int result = 0;
	OS_Scsi_Tape * STp;
	Scsi_Request * SRpnt = NULL;

	kdev_t devt = inode->i_rdev;
	int dev;

	dev = TAPE_NR(devt);
	STp = os_scsi_tapes[dev];

	if (STp->door_locked == ST_LOCKED_AUTO)
		osst_int_ioctl(STp, &SRpnt, MTUNLOCK, 0);
	if (SRpnt) scsi_release_request(SRpnt);

	if (STp->buffer != NULL)
		STp->buffer->in_use = 0;

	if (STp->raw)
		STp->header_ok = 0;

	STp->in_use = 0;
	STp->device->access_count--;

	if (STp->device->host->hostt->module)
		__MOD_DEC_USE_COUNT(STp->device->host->hostt->module);
	if(osst_template.module)
		__MOD_DEC_USE_COUNT(osst_template.module);

	return result;
}


/* The ioctl command */
static int osst_ioctl(struct inode * inode,struct file * file,
	 unsigned int cmd_in, unsigned long arg)
{
	int i, cmd_nr, cmd_type, retval = 0;
	unsigned int blk;
	OS_Scsi_Tape *STp;
	ST_mode *STm;
	ST_partstat *STps;
	Scsi_Request *SRpnt = NULL;
	int dev = TAPE_NR(inode->i_rdev);

	STp = os_scsi_tapes[dev];

	if (down_interruptible(&STp->lock))
		return -ERESTARTSYS;

#if DEBUG
	if (debugging && !STp->in_use) {
		printk(OSST_DEB_MSG "osst%d:D: Incorrect device.\n", dev);
		retval = (-EIO);
		goto out;
	}
#endif
	STm = &(STp->modes[STp->current_mode]);
	STps = &(STp->ps[STp->partition]);

	/*
	 * If we are in the middle of error recovery, don't let anyone
	 * else try and use this device.  Also, if error recovery fails, it
	 * may try and take the device offline, in which case all further
	 * access to the device is prohibited.
	 */
	if( !scsi_block_when_processing_errors(STp->device) ) {
		retval = (-ENXIO);
		goto out;
	}

	cmd_type = _IOC_TYPE(cmd_in);
	cmd_nr   = _IOC_NR(cmd_in);
#if DEBUG
	printk(OSST_DEB_MSG "osst%d:D: Ioctl %d,%d in %s mode\n", dev,
			    cmd_type, cmd_nr, STp->raw?"raw":"normal");
#endif
	if (cmd_type == _IOC_TYPE(MTIOCTOP) && cmd_nr == _IOC_NR(MTIOCTOP)) {
		struct mtop mtc;
		int    auto_weof = 0;

		if (_IOC_SIZE(cmd_in) != sizeof(mtc)) {
			retval = (-EINVAL);
			goto out;
		}

		i = copy_from_user((char *) &mtc, (char *)arg, sizeof(struct mtop));
		if (i) {
			retval = (-EFAULT);
			goto out;
		}

		if (mtc.mt_op == MTSETDRVBUFFER && !capable(CAP_SYS_ADMIN)) {
			printk(KERN_WARNING "osst%d:W: MTSETDRVBUFFER only allowed for root.\n", dev);
			retval = (-EPERM);
			goto out;
		}

		if (!STm->defined && (mtc.mt_op != MTSETDRVBUFFER && (mtc.mt_count & MT_ST_OPTIONS) == 0)) {
			retval = (-ENXIO);
			goto out;
		}

		if (!(STp->device)->was_reset) {

			if (STps->eof == ST_FM_HIT) {
				if (mtc.mt_op == MTFSF || mtc.mt_op == MTFSFM|| mtc.mt_op == MTEOM) {
					mtc.mt_count -= 1;
					if (STps->drv_file >= 0)
						STps->drv_file += 1;
				}
				else if (mtc.mt_op == MTBSF || mtc.mt_op == MTBSFM) {
					mtc.mt_count += 1;
					if (STps->drv_file >= 0)
						STps->drv_file += 1;
				}
			}

			if (mtc.mt_op == MTSEEK) {
				/* Old position must be restored if partition will be changed */
				i = !STp->can_partitions || (STp->new_partition != STp->partition);
			}
			else {
				i = mtc.mt_op == MTREW   || mtc.mt_op == MTOFFL ||
				    mtc.mt_op == MTRETEN || mtc.mt_op == MTEOM  ||
				    mtc.mt_op == MTLOCK  || mtc.mt_op == MTLOAD ||
				    mtc.mt_op == MTCOMPRESSION;
			}
			i = osst_flush_buffer(STp, &SRpnt, i);
			if (i < 0) {
				retval = i;
				goto out;
			}
		}
		else {
			/*
			 * If there was a bus reset, block further access
			 * to this device.  If the user wants to rewind the tape,
			 * then reset the flag and allow access again.
			 */
			if(mtc.mt_op != MTREW   &&
			   mtc.mt_op != MTOFFL  &&
			   mtc.mt_op != MTRETEN &&
			   mtc.mt_op != MTERASE &&
			   mtc.mt_op != MTSEEK  &&
			   mtc.mt_op != MTEOM)   {
				retval = (-EIO);
				goto out;
			}
			STp->device->was_reset = 0;
			if (STp->door_locked != ST_UNLOCKED &&
			    STp->door_locked != ST_LOCK_FAILS) {
				if (osst_int_ioctl(STp, &SRpnt, MTLOCK, 0)) {
					printk(KERN_NOTICE "osst%d:I: Could not relock door after bus reset.\n",
								  dev);
					STp->door_locked = ST_UNLOCKED;
				}
			}
		}

		if (mtc.mt_op != MTCOMPRESSION  && mtc.mt_op != MTLOCK         &&
		    mtc.mt_op != MTNOP          && mtc.mt_op != MTSETBLK       &&
		    mtc.mt_op != MTSETDENSITY   && mtc.mt_op != MTSETDRVBUFFER && 
		    mtc.mt_op != MTMKPART       && mtc.mt_op != MTSETPART      &&
		    mtc.mt_op != MTWEOF         && mtc.mt_op != MTWSM           ) {

			/*
			 * The user tells us to move to another position on the tape.
			 * If we were appending to the tape content, that would leave
			 * the tape without proper end, in that case write EOD and
			 * update the header to reflect its position.
			 */
#if DEBUG
			printk(KERN_WARNING "osst%d:D: auto_weod %s at ffp=%d,efp=%d,fsn=%d,lbn=%d,fn=%d,bn=%d\n",dev,
					STps->rw >= ST_WRITING ? "write" : STps->rw == ST_READING ? "read" : "idle",
					STp->first_frame_position, STp->eod_frame_ppos, STp->frame_seq_number,
					STp->logical_blk_num, STps->drv_file, STps->drv_block );
#endif
			if (STps->rw >= ST_WRITING && STp->first_frame_position >= STp->eod_frame_ppos) {
				auto_weof = ((STp->write_type != OS_WRITE_NEW_MARK) &&
							!(mtc.mt_op == MTREW || mtc.mt_op == MTOFFL));
				i = osst_write_trailer(STp, &SRpnt,
							!(mtc.mt_op == MTREW || mtc.mt_op == MTOFFL));
#if DEBUG
				printk(KERN_WARNING "osst%d:D: post trailer xeof=%d,ffp=%d,efp=%d,fsn=%d,lbn=%d,fn=%d,bn=%d\n",
						dev, auto_weof, STp->first_frame_position, STp->eod_frame_ppos,
						STp->frame_seq_number, STp->logical_blk_num, STps->drv_file, STps->drv_block );
#endif
				if (i < 0) {
					retval = i;
					goto out;
				}
			}
			STps->rw = ST_IDLE;
		}

		if (mtc.mt_op == MTOFFL && STp->door_locked != ST_UNLOCKED)
			osst_int_ioctl(STp, &SRpnt, MTUNLOCK, 0);  /* Ignore result! */

		if (mtc.mt_op == MTSETDRVBUFFER &&
		   (mtc.mt_count & MT_ST_OPTIONS) != 0) {
			retval = osst_set_options(STp, mtc.mt_count);
			goto out;
		}

		if (mtc.mt_op == MTSETPART) {
			if (mtc.mt_count >= STp->nbr_partitions)
				retval = -EINVAL;
			else {
				STp->new_partition = mtc.mt_count;
				retval = 0;
			}
			goto out;
		}

		if (mtc.mt_op == MTMKPART) {
			if (!STp->can_partitions) {
				retval = (-EINVAL);
				goto out;
			}
			if ((i = osst_int_ioctl(STp, &SRpnt, MTREW, 0)) < 0 /*||
			    (i = partition_tape(inode, mtc.mt_count)) < 0*/) {
				retval = i;
				goto out;
			}
			for (i=0; i < ST_NBR_PARTITIONS; i++) {
				STp->ps[i].rw = ST_IDLE;
				STp->ps[i].at_sm = 0;
				STp->ps[i].last_block_valid = FALSE;
			}
			STp->partition = STp->new_partition = 0;
			STp->nbr_partitions = 1;  /* Bad guess ?-) */
			STps->drv_block = STps->drv_file = 0;
			retval = 0;
			goto out;
	 	}

		if (mtc.mt_op == MTSEEK) {
			if (STp->raw)
				i = osst_set_frame_position(STp, &SRpnt, mtc.mt_count, 0);
			else
				i = osst_seek_sector(STp, &SRpnt, mtc.mt_count);
			if (!STp->can_partitions)
				STp->ps[0].rw = ST_IDLE;
			retval = i;
			goto out;
		}

		if (auto_weof)
			cross_eof(STp, &SRpnt, FALSE);

		if (mtc.mt_op == MTCOMPRESSION)
			retval = -EINVAL;	/* OnStream drives don't have compression hardware */
		else
			/* MTBSF MTBSFM MTBSR MTBSS MTEOM MTERASE MTFSF MTFSFB MTFSR MTFSS MTLOAD
			 * MTLOCK MTOFFL MTRESET MTRETEN MTREW MTUNLOAD MTUNLOCK MTWEOF MTWSM */
			retval = osst_int_ioctl(STp, &SRpnt, mtc.mt_op, mtc.mt_count);
		goto out;
	}

	if (!STm->defined) {
		retval = (-ENXIO);
		goto out;
	}

	if ((i = osst_flush_buffer(STp, &SRpnt, FALSE)) < 0) {
		retval = i;
		goto out;
	}

	if (cmd_type == _IOC_TYPE(MTIOCGET) && cmd_nr == _IOC_NR(MTIOCGET)) {
		struct mtget mt_status;

		if (_IOC_SIZE(cmd_in) != sizeof(struct mtget)) {
			 retval = (-EINVAL);
			 goto out;
		}

		mt_status.mt_type = MT_ISONSTREAM_SC;
		mt_status.mt_erreg = STp->recover_erreg << MT_ST_SOFTERR_SHIFT;
		mt_status.mt_dsreg =
			((STp->block_size << MT_ST_BLKSIZE_SHIFT) & MT_ST_BLKSIZE_MASK) |
			((STp->density    << MT_ST_DENSITY_SHIFT) & MT_ST_DENSITY_MASK);
		mt_status.mt_blkno = STps->drv_block;
		mt_status.mt_fileno = STps->drv_file;
		if (STp->block_size != 0) {
			if (STps->rw == ST_WRITING)
				mt_status.mt_blkno += (STp->buffer)->buffer_bytes / STp->block_size;
			else if (STps->rw == ST_READING)
				mt_status.mt_blkno -= ((STp->buffer)->buffer_bytes +
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
		if (STm->do_async_writes || (STm->do_buffer_writes && STp->block_size != 0) ||
		    STp->drv_buffer != 0)
			mt_status.mt_gstat |= GMT_IM_REP_EN(0xffffffff);

		i = copy_to_user((char *)arg, (char *)&mt_status,
		      sizeof(struct mtget));
		if (i) {
			retval = (-EFAULT);
			goto out;
		}

		STp->recover_erreg = 0;  /* Clear after read */
		retval = 0;
		goto out;
	} /* End of MTIOCGET */

	if (cmd_type == _IOC_TYPE(MTIOCPOS) && cmd_nr == _IOC_NR(MTIOCPOS)) {
		struct mtpos mt_pos;

		if (_IOC_SIZE(cmd_in) != sizeof(struct mtpos)) {
			retval = (-EINVAL);
			goto out;
		}
		if (STp->raw)
			blk = osst_get_frame_position(STp, &SRpnt);
		else
			blk = osst_get_sector(STp, &SRpnt);
		if (blk < 0) {
			retval = blk;
			goto out;
		}
		mt_pos.mt_blkno = blk;
		i = copy_to_user((char *)arg, (char *) (&mt_pos), sizeof(struct mtpos));
		if (i)
			retval = -EFAULT;
		goto out;
	}
	if (SRpnt) scsi_release_request(SRpnt);

	up(&STp->lock);

	return scsi_ioctl(STp->device, cmd_in, (void *) arg);

out:
	if (SRpnt) scsi_release_request(SRpnt);

	up(&STp->lock);

	return retval;
}


/* Memory handling routines */

/* Try to allocate a new tape buffer */
static OSST_buffer * new_tape_buffer( int from_initialization, int need_dma )
{
	int i, priority, b_size, order, got = 0, segs = 0;
	OSST_buffer *tb;

	if (osst_nbr_buffers >= osst_template.dev_max)
		return NULL;  /* Should never happen */

	if (from_initialization)
		priority = GFP_ATOMIC;
	else
		priority = GFP_KERNEL;

	i = sizeof(OSST_buffer) + (osst_max_sg_segs - 1) * sizeof(struct scatterlist);
	tb = (OSST_buffer *)kmalloc(i, priority);
	if (tb) {
//    tb->this_size = i;
		if (need_dma)
			priority |= GFP_DMA;

		/* Try to allocate the first segment up to OSST_FIRST_ORDER and the
		 others big enough to reach the goal */
		for (b_size = PAGE_SIZE,          order = 0;
		     b_size < osst_buffer_size && order < OSST_FIRST_ORDER;
		     b_size *= 2,                 order++ );

		for ( ; b_size >= PAGE_SIZE; order--, b_size /= 2) {
			tb->sg[0].address =
			    (unsigned char *)__get_free_pages(priority, order);
			if (tb->sg[0].address != NULL) {
			    tb->sg[0].page = NULL;
			    tb->sg[0].length = b_size;
			    break;
			}
		}
		if (tb->sg[segs].address == NULL) {
			kfree(tb);
			tb = NULL;
		}
		else {  /* Got something, continue */

			for (b_size = PAGE_SIZE, order = 0;
			     osst_buffer_size > tb->sg[0].length + (OSST_FIRST_SG - 1) * b_size;
			     b_size *= 2, order++ );

			for (segs=1, got=tb->sg[0].length;
			     got < osst_buffer_size && segs < OSST_FIRST_SG; ) {
			    tb->sg[segs].address =
				(unsigned char *)__get_free_pages(priority, order);
			    if (tb->sg[segs].address == NULL) {
				if (osst_buffer_size - got <=
				    (OSST_FIRST_SG - segs) * b_size / 2) {
				    b_size /= 2; /* Large enough for the rest of the buffers */
				    order--;
				    continue;
				}
				tb->sg_segs = segs;
				tb->orig_sg_segs = 0;
#if DEBUG
				tb->buffer_size = got;
#endif
				normalize_buffer(tb);
				kfree(tb);
				tb = NULL;
				break;
			    }
                            tb->sg[segs].page = NULL;
			    tb->sg[segs].length = b_size;
			    got += b_size;
			    segs++;
			}
		}
	}
	if (!tb) {
		printk(KERN_NOTICE "osst :I: Can't allocate new tape buffer (nbr %d).\n",
				   osst_nbr_buffers);
		return NULL;
	}
	tb->sg_segs = tb->orig_sg_segs = segs;
	tb->b_data = tb->sg[0].address;

#if DEBUG
	if (debugging) {
		printk(OSST_DEB_MSG
			"osst :D: Allocated tape buffer %d (%d bytes, %d segments, dma: %d, a: %p).\n",
			   osst_nbr_buffers, got, tb->sg_segs, need_dma, tb->b_data);
		printk(OSST_DEB_MSG
			"osst :D: segment sizes: first %d, last %d bytes.\n",
			   tb->sg[0].length, tb->sg[segs-1].length);
	}
#endif
	tb->in_use = 0;
	tb->dma = need_dma;
	tb->buffer_size = got;
	tb->writing = 0;
	osst_buffers[osst_nbr_buffers++] = tb;

	return tb;
}


/* Try to allocate a temporary enlarged tape buffer */
static int enlarge_buffer(OSST_buffer *STbuffer, int new_size, int need_dma)
{
	int segs, nbr, max_segs, b_size, priority, order, got;

	normalize_buffer(STbuffer);

	max_segs = STbuffer->use_sg;
	if (max_segs > osst_max_sg_segs)
		max_segs = osst_max_sg_segs;
	nbr = max_segs - STbuffer->sg_segs;
	if (nbr <= 0)
		return FALSE;

	priority = GFP_KERNEL;
	if (need_dma)
		priority |= GFP_DMA;
	for (b_size = PAGE_SIZE, order = 0;
		 b_size * nbr < new_size - STbuffer->buffer_size;
		 b_size *= 2, order++);

	for (segs=STbuffer->sg_segs, got=STbuffer->buffer_size;
	     segs < max_segs && got < new_size; ) {
		STbuffer->sg[segs].address =
			  (unsigned char *)__get_free_pages(priority,
				(new_size - got <= PAGE_SIZE) ? 0 : order);
		if (STbuffer->sg[segs].address == NULL) {
			if (new_size - got <= (max_segs - segs) * b_size / 2 && order) {
				b_size /= 2;  /* Large enough for the rest of the buffers */
				order--;
				continue;
			}
			printk(KERN_WARNING "osst :W: Failed to enlarge buffer to %d bytes.\n",
						new_size);
#if DEBUG
			STbuffer->buffer_size = got;
#endif
			normalize_buffer(STbuffer);
			return FALSE;
		}
		STbuffer->sg[segs].page = NULL;
		STbuffer->sg[segs].length = (new_size - got <= PAGE_SIZE / 2) ? (new_size - got) : b_size;
		STbuffer->sg_segs += 1;
		got += STbuffer->sg[segs].length;
		STbuffer->buffer_size = got;
		segs++;
	}
#if DEBUG
	if (debugging) {
		for (nbr=0; osst_buffers[nbr] != STbuffer && nbr < osst_nbr_buffers; nbr++);
			printk(OSST_DEB_MSG
			   "osst :D: Expanded tape buffer %d (%d bytes, %d->%d segments, dma: %d, a: %p).\n",
			   nbr, got, STbuffer->orig_sg_segs, STbuffer->sg_segs, need_dma, STbuffer->b_data);
			printk(OSST_DEB_MSG
			   "osst :D: segment sizes: first %d, last %d bytes.\n",
			   STbuffer->sg[0].length, STbuffer->sg[segs-1].length);
	}
#endif

	return TRUE;
}


/* Release the extra buffer */
static void normalize_buffer(OSST_buffer *STbuffer)
{
  int i, order, b_size;

	for (i=STbuffer->orig_sg_segs; i < STbuffer->sg_segs; i++) {

		for (b_size = PAGE_SIZE, order = 0;
		     b_size < STbuffer->sg[i].length;
		     b_size *= 2, order++);

		free_pages((unsigned long)STbuffer->sg[i].address, order);
		STbuffer->buffer_size -= STbuffer->sg[i].length;
	}
#if DEBUG
	if (debugging && STbuffer->orig_sg_segs < STbuffer->sg_segs)
		printk(OSST_DEB_MSG "osst :D: Buffer at %p normalized to %d bytes (segs %d).\n",
			     STbuffer->b_data, STbuffer->buffer_size, STbuffer->sg_segs);
#endif
	STbuffer->sg_segs = STbuffer->orig_sg_segs;
}


/* Move data from the user buffer to the tape buffer. Returns zero (success) or
   negative error code. */
static int append_to_buffer(const char *ubp, OSST_buffer *st_bp, int do_count)
{
	int i, cnt, res, offset;

	for (i=0, offset=st_bp->buffer_bytes;
	     i < st_bp->sg_segs && offset >= st_bp->sg[i].length; i++)
	offset -= st_bp->sg[i].length;
	if (i == st_bp->sg_segs) {  /* Should never happen */
		printk(KERN_WARNING "osst :A: Append_to_buffer offset overflow.\n");
		return (-EIO);
	}
	for ( ; i < st_bp->sg_segs && do_count > 0; i++) {
		cnt = st_bp->sg[i].length - offset < do_count ?
		      st_bp->sg[i].length - offset : do_count;
		res = copy_from_user(st_bp->sg[i].address + offset, ubp, cnt);
		if (res)
			return (-EFAULT);
		do_count -= cnt;
		st_bp->buffer_bytes += cnt;
		ubp += cnt;
		offset = 0;
	}
	if (do_count) {  /* Should never happen */
		printk(KERN_WARNING "osst :A: Append_to_buffer overflow (left %d).\n",
		       do_count);
		return (-EIO);
	}
	return 0;
}


/* Move data from the tape buffer to the user buffer. Returns zero (success) or
   negative error code. */
static int from_buffer(OSST_buffer *st_bp, char *ubp, int do_count)
{
	int i, cnt, res, offset;

	for (i=0, offset=st_bp->read_pointer;
	     i < st_bp->sg_segs && offset >= st_bp->sg[i].length; i++)
		offset -= st_bp->sg[i].length;
	if (i == st_bp->sg_segs) {  /* Should never happen */
		printk(KERN_WARNING "osst :A: From_buffer offset overflow.\n");
		return (-EIO);
	}
	for ( ; i < st_bp->sg_segs && do_count > 0; i++) {
		cnt = st_bp->sg[i].length - offset < do_count ?
		      st_bp->sg[i].length - offset : do_count;
		res = copy_to_user(ubp, st_bp->sg[i].address + offset, cnt);
		if (res)
			return (-EFAULT);
		do_count -= cnt;
		st_bp->buffer_bytes -= cnt;
		st_bp->read_pointer += cnt;
		ubp += cnt;
		offset = 0;
	}
	if (do_count) {  /* Should never happen */
		printk(KERN_WARNING "osst :A: From_buffer overflow (left %d).\n", do_count);
		return (-EIO);
	}
	return 0;
}

/* Sets the tail of the buffer after fill point to zero.
   Returns zero (success) or negative error code.        */
static int osst_zero_buffer_tail(OSST_buffer *st_bp)
{
	int	i, offset, do_count, cnt;

	for (i = 0, offset = st_bp->buffer_bytes;
	     i < st_bp->sg_segs && offset >= st_bp->sg[i].length; i++)
		offset -= st_bp->sg[i].length;
	if (i == st_bp->sg_segs) {  /* Should never happen */
		printk(KERN_WARNING "osst :A: Zero_buffer offset overflow.\n");
		return (-EIO);
	}
	for (do_count = OS_DATA_SIZE - st_bp->buffer_bytes;
	     i < st_bp->sg_segs && do_count > 0; i++) {
		cnt = st_bp->sg[i].length - offset < do_count ?
		      st_bp->sg[i].length - offset : do_count ;
		memset(st_bp->sg[i].address + offset, 0, cnt);
		do_count -= cnt;
		offset = 0;
	}
	if (do_count) {  /* Should never happen */
		printk(KERN_WARNING "osst :A: Zero_buffer overflow (left %d).\n", do_count);
		return (-EIO);
	}
	return 0;
}

/* Copy a osst 32K chunk of memory into the buffer.
   Returns zero (success) or negative error code.  */
static int osst_copy_to_buffer(OSST_buffer *st_bp, unsigned char *ptr)
{
	int	i, cnt, do_count = OS_DATA_SIZE;

	for (i = 0; i < st_bp->sg_segs && do_count > 0; i++) {
		cnt = st_bp->sg[i].length < do_count ?
		      st_bp->sg[i].length : do_count ;
		memcpy(st_bp->sg[i].address, ptr, cnt);
		do_count -= cnt;
		ptr      += cnt;
	}
	if (do_count || i != st_bp->sg_segs-1) {  /* Should never happen */
		printk(KERN_WARNING "osst :A: Copy_to_buffer overflow (left %d at sg %d).\n",
					 do_count, i);
		return (-EIO);
	}
	return 0;
}

/* Copy a osst 32K chunk of memory from the buffer.
   Returns zero (success) or negative error code.  */
static int osst_copy_from_buffer(OSST_buffer *st_bp, unsigned char *ptr)
{
	int	i, cnt, do_count = OS_DATA_SIZE;

	for (i = 0; i < st_bp->sg_segs && do_count > 0; i++) {
		cnt = st_bp->sg[i].length < do_count ?
		      st_bp->sg[i].length : do_count ;
		memcpy(ptr, st_bp->sg[i].address, cnt);
		do_count -= cnt;
		ptr      += cnt;
	}
	if (do_count || i != st_bp->sg_segs-1) {  /* Should never happen */
		printk(KERN_WARNING "osst :A: Copy_from_buffer overflow (left %d at sg %d).\n",
					 do_count, i);
		return (-EIO);
	}
	return 0;
}


/* Module housekeeping */

static void validate_options (void)
{
  if (max_dev > 0)
		osst_max_dev = osst_max_buffers = max_dev;
  if (write_threshold_kbs > 0)
		osst_write_threshold = write_threshold_kbs * ST_KILOBYTE;
  if (osst_write_threshold > osst_buffer_size)
		osst_write_threshold = osst_buffer_size;
  if (max_sg_segs >= OSST_FIRST_SG)
		osst_max_sg_segs = max_sg_segs;
#if DEBUG
  printk(OSST_DEB_MSG "osst :D: bufsize %d, wrt %d, max devices %d, s/g segs %d.\n",
	 osst_buffer_size, osst_write_threshold, osst_max_buffers, osst_max_sg_segs);
#endif
}
	
#ifndef MODULE
/* Set the boot options. Syntax: osst=xxx,yyy,zzz
 * where xxx is maximum nr of devices to attach,
 * yyy is write threshold in 1024 byte blocks
 * and zzz the maximum nr of s/g segments to handle.
 */
static int __init osst_setup (char *str)
{
  int i, ints[5];
  char *stp;

  stp = get_options(str, ARRAY_SIZE(ints), ints);
	
  if (ints[0] > 0) {
	for (i = 0; i < ints[0] && i < ARRAY_SIZE(parms); i++)
		  *parms[i].val = ints[i + 1];
  } else {
	while (stp != NULL) {
		for (i = 0; i < ARRAY_SIZE(parms); i++) {
			int len = strlen(parms[i].name);
			if (!strncmp(stp, parms[i].name, len) &&
			    (*(stp + len) == ':' || *(stp + len) == '=')) {
				*parms[i].val =
					simple_strtoul(stp + len + 1, NULL, 0);
				break;
			}
		}
		if (i >= sizeof(parms) / sizeof(struct osst_dev_parm))
			printk(KERN_INFO "osst :I: Illegal parameter in '%s'\n",
			       stp);
		stp = strchr(stp, ',');
		if (stp)
			stp++;
	}
  }

  return 1;
}

__setup("osst=", osst_setup);

#endif


static struct file_operations osst_fops = {
	read:		osst_read,
	write:		osst_write,
	ioctl:		osst_ioctl,
	open:		os_scsi_tape_open,
	flush:		os_scsi_tape_flush,
	release:	os_scsi_tape_close,
};

static int osst_supports(Scsi_Device * SDp)
{
	struct	osst_support_data {
		char *vendor;
		char *model;
		char *rev;
		char *driver_hint; /* Name of the correct driver, NULL if unknown */
	};

static	struct	osst_support_data support_list[] = {
		/* {"XXX", "Yy-", "", NULL},  example */
		SIGS_FROM_OSST,
		{NULL, }};

	struct	osst_support_data *rp;

	/* We are willing to drive OnStream SC-x0 as well as the
	 * 	 * IDE, ParPort, FireWire, USB variants, if accessible by
	 * 	 	 * emulation layer (ide-scsi, usb-storage, ...) */

	for (rp=&(support_list[0]); rp->vendor != NULL; rp++)
		if (!strncmp(rp->vendor, SDp->vendor, strlen(rp->vendor)) &&
		    !strncmp(rp->model, SDp->model, strlen(rp->model)) &&
		    !strncmp(rp->rev, SDp->rev, strlen(rp->rev))) 
			return 1;
	return 0;
}

/*
 * /proc support for accessing ADR header information
 */
static struct proc_dir_entry * osst_proc_dir = NULL;
static char   osst_proc_dirname[] = "osst";

static int osst_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int l = 0;
	OS_Scsi_Tape * STp = (OS_Scsi_Tape *) data;

	if (!osst_proc_dir) return 0;

	if (STp->header_ok && STp->linux_media)
		l = sprintf(page, "%d.%d LIN%d %8d %8d %8d \n",
				  STp->header_cache->major_rev,
				  STp->header_cache->minor_rev,
				  STp->linux_media_version,
				  STp->first_data_ppos,
				  STp->eod_frame_ppos,
				  STp->filemark_cnt );
	return l;
}

static void osst_proc_init(void)
{
	if (!proc_scsi) return;

	osst_proc_dir = proc_mkdir(osst_proc_dirname, proc_scsi);
	osst_proc_dir->owner = THIS_MODULE;
}

static void osst_proc_create(OS_Scsi_Tape * STp, int dev)
{
	char s[16];
	struct proc_dir_entry * p_entry;

	if (!osst_proc_dir) return;

	sprintf(s, "osst%d", dev);
	p_entry = create_proc_read_entry(s, 0444, osst_proc_dir, osst_proc_read, (void *) STp);
	p_entry->owner = THIS_MODULE;
}

static void osst_proc_destroy(int dev)
{
	char s[16];

	if (!osst_proc_dir) return; 

	sprintf(s, "osst%d", dev);
	remove_proc_entry(s, osst_proc_dir);
}

static void osst_proc_cleanup(void)
{
	if ((! proc_scsi) || (!osst_proc_dir)) return;

	remove_proc_entry(osst_proc_dirname, proc_scsi);
	osst_proc_dir = NULL;
}

/*
 * osst startup / cleanup code
 */

static int osst_attach(Scsi_Device * SDp)
{
	OS_Scsi_Tape * tpnt;
	ST_mode * STm;
	ST_partstat * STps;
	int i, dev;
#ifdef CONFIG_DEVFS_FS
	int mode;
#endif

	if (SDp->type != TYPE_TAPE || !osst_supports(SDp))
		 return 1;

	if (osst_template.nr_dev >= osst_template.dev_max) {
		 SDp->attached--;
		 return 1;
	}
	
	/* find a free minor number */
	for (i=0; os_scsi_tapes[i] && i<osst_template.dev_max; i++);
	if(i >= osst_template.dev_max) panic ("Scsi_devices corrupt (osst)");

	/* allocate a OS_Scsi_Tape for this device */
	tpnt = (OS_Scsi_Tape *)kmalloc(sizeof(OS_Scsi_Tape), GFP_ATOMIC);
	if (tpnt == NULL) {
		 SDp->attached--;
		 printk(KERN_WARNING "osst :W: Can't allocate device descriptor.\n");
		 return 1;
	}
	memset(tpnt, 0, sizeof(OS_Scsi_Tape));
	os_scsi_tapes[i] = tpnt;
	dev = i;
	tpnt->capacity = 0xfffff;

	/* allocate a buffer for this device */
	if (!new_tape_buffer(TRUE, TRUE)) 
		 printk(KERN_ERR "osst :W: Unable to allocate a tape buffer.\n");

#ifdef CONFIG_DEVFS_FS
	for (mode = 0; mode < ST_NBR_MODES; ++mode) {
		 char name[8];
		 static char *formats[ST_NBR_MODES] ={"", "l", "m", "a"};

		 /*  Rewind entry  */
		 sprintf (name, "mt%s", formats[mode]);
		 tpnt->de_r[mode] =
			devfs_register (SDp->de, name, DEVFS_FL_DEFAULT,
			   MAJOR_NR, i + (mode << 5),
			   S_IFCHR | S_IRUGO | S_IWUGO,
			   &osst_fops, NULL);
		 /*  No-rewind entry  */
		 sprintf (name, "mt%sn", formats[mode]);
		 tpnt->de_n[mode] =
			devfs_register (SDp->de, name, DEVFS_FL_DEFAULT,
			   MAJOR_NR, i + (mode << 5) + 128,
			   S_IFCHR | S_IRUGO | S_IWUGO,
			   &osst_fops, NULL);
	}
	devfs_register_tape (tpnt->de_r[0]);
#endif

	tpnt->device = SDp;
	tpnt->devt = MKDEV(MAJOR_NR, i);
	tpnt->dirty = 0;
	tpnt->in_use = 0;
	tpnt->drv_buffer = 1;  /* Try buffering if no mode sense */
	tpnt->restr_dma = (SDp->host)->unchecked_isa_dma;
	tpnt->density = 0;
	tpnt->do_auto_lock = OSST_AUTO_LOCK;
	tpnt->can_bsr = OSST_IN_FILE_POS;
	tpnt->can_partitions = 0;
	tpnt->two_fm = OSST_TWO_FM;
	tpnt->fast_mteom = OSST_FAST_MTEOM;
	tpnt->scsi2_logical = OSST_SCSI2LOGICAL; /* FIXME */
	tpnt->write_threshold = osst_write_threshold;
	tpnt->default_drvbuffer = 0xff; /* No forced buffering */
	tpnt->partition = 0;
	tpnt->new_partition = 0;
	tpnt->nbr_partitions = 0;
	tpnt->min_block = 512;
	tpnt->max_block = OS_DATA_SIZE;
	tpnt->timeout = OSST_TIMEOUT;
	tpnt->long_timeout = OSST_LONG_TIMEOUT;

	/* Recognize OnStream tapes */
	/* We don't need to test for OnStream, as this has been done in detect () */
	tpnt->os_fw_rev = osst_parse_firmware_rev (SDp->rev);
	tpnt->omit_blklims = 1;

	tpnt->poll = (strncmp(SDp->model, "DI-", 3) == 0) || 
		     (strncmp(SDp->model, "FW-", 3) == 0) || OSST_FW_NEED_POLL(tpnt->os_fw_rev,SDp);
	tpnt->frame_in_buffer = 0;
	tpnt->header_ok = 0;
	tpnt->linux_media = 0;
	tpnt->header_cache = NULL;

	for (i=0; i < ST_NBR_MODES; i++) {
		STm = &(tpnt->modes[i]);
		STm->defined = FALSE;
		STm->sysv = OSST_SYSV;
		STm->defaults_for_writes = 0;
		STm->do_async_writes = OSST_ASYNC_WRITES;
		STm->do_buffer_writes = OSST_BUFFER_WRITES;
		STm->do_read_ahead = OSST_READ_AHEAD;
		STm->default_compression = ST_DONT_TOUCH;
		STm->default_blksize = 512;
		STm->default_density = (-1);  /* No forced density */
	}

	for (i=0; i < ST_NBR_PARTITIONS; i++) {
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
	tpnt->modes[2].defined = TRUE;
	tpnt->density_changed = tpnt->compression_changed = tpnt->blksize_changed = FALSE;
	init_MUTEX(&tpnt->lock);

	osst_template.nr_dev++;

	osst_proc_create(tpnt, dev);

	printk(KERN_INFO
		"osst :I: Attached OnStream %.5s tape at scsi%d, channel %d, id %d, lun %d as osst%d\n",
		SDp->model, SDp->host->host_no, SDp->channel, SDp->id, SDp->lun, dev);

	return 0;
};

static int osst_detect(Scsi_Device * SDp)
{
	if (SDp->type != TYPE_TAPE) return 0;
	if ( ! osst_supports(SDp) ) return 0;
	
	osst_template.dev_noticed++;
	return 1;
}

static int osst_registered = 0;

/* Driver initialization (not __initfunc because may be called later) */
static int osst_init()
{
  int i;

  if (osst_template.dev_noticed == 0) return 0;

  if(!osst_registered) {
#ifdef CONFIG_DEVFS_FS
	if (devfs_register_chrdev(MAJOR_NR,"osst",&osst_fops)) {
#else
	if (register_chrdev(MAJOR_NR,"osst",&osst_fops)) {
#endif
		printk(KERN_ERR "osst :W: Unable to get major %d for OnStream tapes\n",MAJOR_NR);
		return 1;
	}
	osst_registered++;
  }
  
  if (os_scsi_tapes) return 0;
  osst_template.dev_max = osst_max_dev;
  if (osst_template.dev_max > 128 / ST_NBR_MODES)
	printk(KERN_INFO "osst :I: Only %d tapes accessible.\n", 128 / ST_NBR_MODES);
  os_scsi_tapes =
	(OS_Scsi_Tape **)kmalloc(osst_template.dev_max * sizeof(OS_Scsi_Tape *),
				   GFP_ATOMIC);
  if (os_scsi_tapes == NULL) {
	printk(KERN_ERR "osst :W: Unable to allocate array for OnStream SCSI tapes.\n");
#ifdef CONFIG_DEVFS_FS
	devfs_unregister_chrdev(MAJOR_NR, "osst");
#else
	unregister_chrdev(MAJOR_NR, "osst");
#endif
	return 1;
  }

  for (i=0; i < osst_template.dev_max; ++i) os_scsi_tapes[i] = NULL;

  /* Allocate the buffer pointers */
  osst_buffers =
	(OSST_buffer **)kmalloc(osst_template.dev_max * sizeof(OSST_buffer *),
				    GFP_ATOMIC);
  if (osst_buffers == NULL) {
	printk(KERN_ERR "osst :W: Unable to allocate tape buffer pointers.\n");
#ifdef CONFIG_DEVFS_FS
	devfs_unregister_chrdev(MAJOR_NR, "osst");
#else
	unregister_chrdev(MAJOR_NR, "osst");
#endif
	kfree(os_scsi_tapes);
	return 1;
  }
  osst_nbr_buffers = 0;

  printk(KERN_INFO "osst :I: Tape driver with OnStream support version %s\nosst :I: %s\n", osst_version, cvsid);

#if DEBUG
  printk(OSST_DEB_MSG "osst :D: Buffer size %d bytes, write threshold %d bytes.\n",
	 osst_buffer_size, osst_write_threshold);
#endif
  osst_proc_init();
  return 0;
}


static void osst_detach(Scsi_Device * SDp)
{
	OS_Scsi_Tape * tpnt;
	int i;
#ifdef CONFIG_DEVFS_FS
	int mode;
#endif
	for(i=0; i<osst_template.dev_max; i++) {
		tpnt = os_scsi_tapes[i];
		if(tpnt != NULL && tpnt->device == SDp) {
			osst_proc_destroy(i);
			tpnt->device = NULL;
#ifdef CONFIG_DEVFS_FS
			for (mode = 0; mode < ST_NBR_MODES; ++mode) {
				devfs_unregister (tpnt->de_r[mode]);
				tpnt->de_r[mode] = NULL;
				devfs_unregister (tpnt->de_n[mode]);
				tpnt->de_n[mode] = NULL;
			}
#endif
			if (tpnt->header_cache != NULL) {
				vfree(tpnt->header_cache);
			}
			kfree(tpnt);
			os_scsi_tapes[i] = NULL;
			SDp->attached--;
			osst_template.nr_dev--;
			osst_template.dev_noticed--;
			return;
		}
	}
}

static int __init init_osst(void) 
{
	validate_options();
	osst_template.module = THIS_MODULE;
	return scsi_register_module(MODULE_SCSI_DEV, &osst_template);
}

static void __exit exit_osst (void)
{
	int i;

	scsi_unregister_module(MODULE_SCSI_DEV, &osst_template);
#ifdef CONFIG_DEVFS_FS
	devfs_unregister_chrdev(MAJOR_NR, "osst");
#else
	unregister_chrdev(MAJOR_NR, "osst");
#endif
	osst_registered--;

	osst_proc_cleanup();

	if(os_scsi_tapes != NULL) {
		kfree(os_scsi_tapes);
	}
	if (osst_buffers != NULL) {
		for (i=0; i < osst_nbr_buffers; i++)
			if (osst_buffers[i] != NULL) {
				osst_buffers[i]->orig_sg_segs = 0;
				normalize_buffer(osst_buffers[i]);
				kfree(osst_buffers[i]);
			}
		kfree(osst_buffers);
	}
	osst_template.dev_max = 0;
	printk(KERN_INFO "osst :I: Unloaded.\n");
}

module_init(init_osst);
module_exit(exit_osst);
