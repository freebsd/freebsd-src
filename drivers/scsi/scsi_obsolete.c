/*
 *  scsi_obsolete.c Copyright (C) 1992 Drew Eckhardt
 *         Copyright (C) 1993, 1994, 1995 Eric Youngdale
 *
 *  generic mid-level SCSI driver
 *      Initial versions: Drew Eckhardt
 *      Subsequent revisions: Eric Youngdale
 *
 *  <drew@colorado.edu>
 *
 *  Bug correction thanks go to :
 *      Rik Faith <faith@cs.unc.edu>
 *      Tommy Thorn <tthorn>
 *      Thomas Wuensche <tw@fgb1.fgb.mw.tu-muenchen.de>
 *
 *  Modified by Eric Youngdale eric@andante.org to
 *  add scatter-gather, multiple outstanding request, and other
 *  enhancements.
 *
 *  Native multichannel, wide scsi, /proc/scsi and hot plugging
 *  support added by Michael Neuffer <mike@i-connect.net>
 *
 *  Major improvements to the timeout, abort, and reset processing,
 *  as well as performance modifications for large queue depths by
 *  Leonard N. Zubkoff <lnz@dandelion.com>
 *
 *  Improved compatibility with 2.0 behaviour by Manfred Spraul
 *  <masp0008@stud.uni-sb.de>
 */

/*
 *#########################################################################
 *#########################################################################
 *#########################################################################
 *#########################################################################
 *              NOTE - NOTE - NOTE - NOTE - NOTE - NOTE - NOTE
 *
 *#########################################################################
 *#########################################################################
 *#########################################################################
 *#########################################################################
 *
 * This file contains the 'old' scsi error handling.  It is only present
 * while the new error handling code is being debugged, and while the low
 * level drivers are being converted to use the new code.  Once the last
 * driver uses the new code this *ENTIRE* file will be nuked.
 */

#define __NO_VERSION__
#include <linux/module.h>

#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/stat.h>
#include <linux/blk.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/dma.h>

#include "scsi.h"
#include "hosts.h"
#include "constants.h"

#undef USE_STATIC_SCSI_MEMORY

/*
   static const char RCSid[] = "$Header: /mnt/ide/home/eric/CVSROOT/linux/drivers/scsi/scsi_obsolete.c,v 1.1 1997/05/18 23:27:21 eric Exp $";
 */


#define INTERNAL_ERROR (panic ("Internal error in file %s, line %d.\n", __FILE__, __LINE__))


static int scsi_abort(Scsi_Cmnd *, int code);
static int scsi_reset(Scsi_Cmnd *, unsigned int);

extern void scsi_old_done(Scsi_Cmnd * SCpnt);
int update_timeout(Scsi_Cmnd *, int);
extern void scsi_old_times_out(Scsi_Cmnd * SCpnt);

extern int scsi_dispatch_cmd(Scsi_Cmnd * SCpnt);

#define SCSI_BLOCK(HOST) (HOST->can_queue && HOST->host_busy >= HOST->can_queue)

static unsigned char generic_sense[6] =
{REQUEST_SENSE, 0, 0, 0, 255, 0};

/*
 *  This is the number  of clock ticks we should wait before we time out
 *  and abort the command.  This is for  where the scsi.c module generates
 *  the command, not where it originates from a higher level, in which
 *  case the timeout is specified there.
 *
 *  ABORT_TIMEOUT and RESET_TIMEOUT are the timeouts for RESET and ABORT
 *  respectively.
 */

#ifdef DEBUG_TIMEOUT
static void scsi_dump_status(void);
#endif


#ifdef DEBUG
#define SCSI_TIMEOUT (5*HZ)
#else
#define SCSI_TIMEOUT (2*HZ)
#endif

#ifdef DEBUG
#define SENSE_TIMEOUT SCSI_TIMEOUT
#define ABORT_TIMEOUT SCSI_TIMEOUT
#define RESET_TIMEOUT SCSI_TIMEOUT
#else
#define SENSE_TIMEOUT (5*HZ/10)
#define RESET_TIMEOUT (5*HZ/10)
#define ABORT_TIMEOUT (5*HZ/10)
#endif


/* Do not call reset on error if we just did a reset within 15 sec. */
#define MIN_RESET_PERIOD (15*HZ)



/*
 *  Flag bits for the internal_timeout array
 */
#define IN_ABORT  1
#define IN_RESET  2
#define IN_RESET2 4
#define IN_RESET3 8

/*
 * This is our time out function, called when the timer expires for a
 * given host adapter.  It will attempt to abort the currently executing
 * command, that failing perform a kernel panic.
 */

void scsi_old_times_out(Scsi_Cmnd * SCpnt)
{
	unsigned long flags;

	spin_lock_irqsave(&io_request_lock, flags);

	/* Set the serial_number_at_timeout to the current serial_number */
	SCpnt->serial_number_at_timeout = SCpnt->serial_number;

	switch (SCpnt->internal_timeout & (IN_ABORT | IN_RESET | IN_RESET2 | IN_RESET3)) {
	case NORMAL_TIMEOUT:
		{
#ifdef DEBUG_TIMEOUT
			scsi_dump_status();
#endif
		}

		if (!scsi_abort(SCpnt, DID_TIME_OUT))
			break;
	case IN_ABORT:
		printk("SCSI host %d abort (pid %ld) timed out - resetting\n",
		       SCpnt->host->host_no, SCpnt->pid);
		if (!scsi_reset(SCpnt, SCSI_RESET_ASYNCHRONOUS))
			break;
	case IN_RESET:
	case (IN_ABORT | IN_RESET):
		/* This might be controversial, but if there is a bus hang,
		 * you might conceivably want the machine up and running
		 * esp if you have an ide disk.
		 */
		printk("SCSI host %d channel %d reset (pid %ld) timed out - "
		       "trying harder\n",
		       SCpnt->host->host_no, SCpnt->channel, SCpnt->pid);
		SCpnt->internal_timeout &= ~IN_RESET;
		SCpnt->internal_timeout |= IN_RESET2;
		scsi_reset(SCpnt,
		 SCSI_RESET_ASYNCHRONOUS | SCSI_RESET_SUGGEST_BUS_RESET);
		break;
	case IN_RESET2:
	case (IN_ABORT | IN_RESET2):
		/* Obviously the bus reset didn't work.
		 * Let's try even harder and call for an HBA reset.
		 * Maybe the HBA itself crashed and this will shake it loose.
		 */
		printk("SCSI host %d reset (pid %ld) timed out - trying to shake it loose\n",
		       SCpnt->host->host_no, SCpnt->pid);
		SCpnt->internal_timeout &= ~(IN_RESET | IN_RESET2);
		SCpnt->internal_timeout |= IN_RESET3;
		scsi_reset(SCpnt,
		SCSI_RESET_ASYNCHRONOUS | SCSI_RESET_SUGGEST_HOST_RESET);
		break;

	default:
		printk("SCSI host %d reset (pid %ld) timed out again -\n",
		       SCpnt->host->host_no, SCpnt->pid);
		printk("probably an unrecoverable SCSI bus or device hang.\n");
		break;

	}
	spin_unlock_irqrestore(&io_request_lock, flags);

}

/*
 *  From what I can find in scsi_obsolete.c, this function is only called
 *  by scsi_old_done and scsi_reset.  Both of these functions run with the
 *  io_request_lock already held, so we need do nothing here about grabbing
 *  any locks.
 */
static void scsi_request_sense(Scsi_Cmnd * SCpnt)
{
	SCpnt->flags |= WAS_SENSE | ASKED_FOR_SENSE;
	update_timeout(SCpnt, SENSE_TIMEOUT);


	memcpy((void *) SCpnt->cmnd, (void *) generic_sense,
	       sizeof(generic_sense));
	memset((void *) SCpnt->sense_buffer, 0,
	       sizeof(SCpnt->sense_buffer));

	if (SCpnt->device->scsi_level <= SCSI_2)
		SCpnt->cmnd[1] = SCpnt->lun << 5;
	SCpnt->cmnd[4] = sizeof(SCpnt->sense_buffer);

	SCpnt->request_buffer = &SCpnt->sense_buffer;
	SCpnt->request_bufflen = sizeof(SCpnt->sense_buffer);
	SCpnt->use_sg = 0;
	SCpnt->cmd_len = COMMAND_SIZE(SCpnt->cmnd[0]);
	SCpnt->result = 0;
	SCpnt->sc_data_direction = SCSI_DATA_READ;

        /*
         * Ugly, ugly.  The newer interfaces all assume that the lock
         * isn't held.  Mustn't disappoint, or we deadlock the system.
         */
        spin_unlock_irq(&io_request_lock);
	scsi_dispatch_cmd(SCpnt);
        spin_lock_irq(&io_request_lock);
}




static int check_sense(Scsi_Cmnd * SCpnt)
{
	/* If there is no sense information, request it.  If we have already
	 * requested it, there is no point in asking again - the firmware must
	 * be confused.
	 */
	if (((SCpnt->sense_buffer[0] & 0x70) >> 4) != 7) {
		if (!(SCpnt->flags & ASKED_FOR_SENSE))
			return SUGGEST_SENSE;
		else
			return SUGGEST_RETRY;
	}
	SCpnt->flags &= ~ASKED_FOR_SENSE;

#ifdef DEBUG_INIT
	printk("scsi%d, channel%d : ", SCpnt->host->host_no, SCpnt->channel);
	print_sense("", SCpnt);
	printk("\n");
#endif
	if (SCpnt->sense_buffer[2] & 0xe0)
		return SUGGEST_ABORT;

	switch (SCpnt->sense_buffer[2] & 0xf) {
	case NO_SENSE:
		return 0;
	case RECOVERED_ERROR:
		return SUGGEST_IS_OK;

	case ABORTED_COMMAND:
		return SUGGEST_RETRY;
	case NOT_READY:
	case UNIT_ATTENTION:
		/*
		 * If we are expecting a CC/UA because of a bus reset that we
		 * performed, treat this just as a retry.  Otherwise this is
		 * information that we should pass up to the upper-level driver
		 * so that we can deal with it there.
		 */
		if (SCpnt->device->expecting_cc_ua) {
			SCpnt->device->expecting_cc_ua = 0;
			return SUGGEST_RETRY;
		}
		return SUGGEST_ABORT;

		/* these three are not supported */
	case COPY_ABORTED:
	case VOLUME_OVERFLOW:
	case MISCOMPARE:

	case MEDIUM_ERROR:
		return SUGGEST_REMAP;
	case BLANK_CHECK:
	case DATA_PROTECT:
	case HARDWARE_ERROR:
	case ILLEGAL_REQUEST:
	default:
		return SUGGEST_ABORT;
	}
}

/* This function is the mid-level interrupt routine, which decides how
 *  to handle error conditions.  Each invocation of this function must
 *  do one and *only* one of the following:
 *
 *  (1) Call last_cmnd[host].done.  This is done for fatal errors and
 *      normal completion, and indicates that the handling for this
 *      request is complete.
 *  (2) Call internal_cmnd to requeue the command.  This will result in
 *      scsi_done being called again when the retry is complete.
 *  (3) Call scsi_request_sense.  This asks the host adapter/drive for
 *      more information about the error condition.  When the information
 *      is available, scsi_done will be called again.
 *  (4) Call reset().  This is sort of a last resort, and the idea is that
 *      this may kick things loose and get the drive working again.  reset()
 *      automatically calls scsi_request_sense, and thus scsi_done will be
 *      called again once the reset is complete.
 *
 *      If none of the above actions are taken, the drive in question
 *      will hang. If more than one of the above actions are taken by
 *      scsi_done, then unpredictable behavior will result.
 */
void scsi_old_done(Scsi_Cmnd * SCpnt)
{
	int status = 0;
	int exit = 0;
	int checked;
	int oldto;
	struct Scsi_Host *host = SCpnt->host;
        Scsi_Device * device = SCpnt->device;
	int result = SCpnt->result;
	SCpnt->serial_number = 0;
	SCpnt->serial_number_at_timeout = 0;
	oldto = update_timeout(SCpnt, 0);

#ifdef DEBUG_TIMEOUT
	if (result)
		printk("Non-zero result in scsi_done %x %d:%d\n",
		       result, SCpnt->target, SCpnt->lun);
#endif

	/* If we requested an abort, (and we got it) then fix up the return
	 *  status to say why
	 */
	if (host_byte(result) == DID_ABORT && SCpnt->abort_reason)
		SCpnt->result = result = (result & 0xff00ffff) |
		    (SCpnt->abort_reason << 16);


#define CMD_FINISHED 0
#define MAYREDO  1
#define REDO     3
#define PENDING  4

#ifdef DEBUG
	printk("In scsi_done(host = %d, result = %06x)\n", host->host_no, result);
#endif

	if (SCpnt->flags & SYNC_RESET) {
		/*
		   * The behaviou of scsi_reset(SYNC) was changed in 2.1.? .
		   * The scsi mid-layer does a REDO after every sync reset, the driver
		   * must not do that any more. In order to prevent old drivers from
		   * crashing, all scsi_done() calls during sync resets are ignored.
		 */
		printk("scsi%d: device driver called scsi_done() "
		       "for a synchronous reset.\n", SCpnt->host->host_no);
		return;
	}
	if (SCpnt->flags & WAS_SENSE) {
		SCpnt->use_sg = SCpnt->old_use_sg;
		SCpnt->cmd_len = SCpnt->old_cmd_len;
		SCpnt->sc_data_direction = SCpnt->sc_old_data_direction;
		SCpnt->underflow = SCpnt->old_underflow;
	}
	switch (host_byte(result)) {
	case DID_OK:
		if (status_byte(result) && (SCpnt->flags & WAS_SENSE))
			/* Failed to obtain sense information */
		{
			SCpnt->flags &= ~WAS_SENSE;
#if 0				/* This cannot possibly be correct. */
			SCpnt->internal_timeout &= ~SENSE_TIMEOUT;
#endif

			if (!(SCpnt->flags & WAS_RESET)) {
				printk("scsi%d : channel %d target %d lun %d request sense"
				       " failed, performing reset.\n",
				       SCpnt->host->host_no, SCpnt->channel, SCpnt->target,
				       SCpnt->lun);
				scsi_reset(SCpnt, SCSI_RESET_SYNCHRONOUS);
				status = REDO;
				break;
			} else {
				exit = (DRIVER_HARD | SUGGEST_ABORT);
				status = CMD_FINISHED;
			}
		} else
			switch (msg_byte(result)) {
			case COMMAND_COMPLETE:
				switch (status_byte(result)) {
				case GOOD:
					if (SCpnt->flags & WAS_SENSE) {
#ifdef DEBUG
						printk("In scsi_done, GOOD status, COMMAND COMPLETE, "
						       "parsing sense information.\n");
#endif
						SCpnt->flags &= ~WAS_SENSE;
#if 0				/* This cannot possibly be correct. */
						SCpnt->internal_timeout &= ~SENSE_TIMEOUT;
#endif

						switch (checked = check_sense(SCpnt)) {
						case SUGGEST_SENSE:
						case 0:
#ifdef DEBUG
							printk("NO SENSE.  status = REDO\n");
#endif
							update_timeout(SCpnt, oldto);
							status = REDO;
							break;
						case SUGGEST_IS_OK:
							break;
						case SUGGEST_REMAP:
#ifdef DEBUG
							printk("SENSE SUGGEST REMAP - status = CMD_FINISHED\n");
#endif
							status = CMD_FINISHED;
							exit = DRIVER_SENSE | SUGGEST_ABORT;
							break;
						case SUGGEST_RETRY:
#ifdef DEBUG
							printk("SENSE SUGGEST RETRY - status = MAYREDO\n");
#endif
							status = MAYREDO;
							exit = DRIVER_SENSE | SUGGEST_RETRY;
							break;
						case SUGGEST_ABORT:
#ifdef DEBUG
							printk("SENSE SUGGEST ABORT - status = CMD_FINISHED");
#endif
							status = CMD_FINISHED;
							exit = DRIVER_SENSE | SUGGEST_ABORT;
							break;
						default:
							printk("Internal error %s %d \n", __FILE__,
							       __LINE__);
						}
					}
					/* end WAS_SENSE */
					else {
#ifdef DEBUG
						printk("COMMAND COMPLETE message returned, "
						       "status = CMD_FINISHED. \n");
#endif
						exit = DRIVER_OK;
						status = CMD_FINISHED;
					}
					break;

				case CHECK_CONDITION:
				case COMMAND_TERMINATED:
					switch (check_sense(SCpnt)) {
					case 0:
						update_timeout(SCpnt, oldto);
						status = REDO;
						break;
					case SUGGEST_REMAP:
						status = CMD_FINISHED;
						exit = DRIVER_SENSE | SUGGEST_ABORT;
						break;
					case SUGGEST_RETRY:
						status = MAYREDO;
						exit = DRIVER_SENSE | SUGGEST_RETRY;
						break;
					case SUGGEST_ABORT:
						status = CMD_FINISHED;
						exit = DRIVER_SENSE | SUGGEST_ABORT;
						break;
					case SUGGEST_SENSE:
						scsi_request_sense(SCpnt);
						status = PENDING;
						break;
					}
					break;

				case CONDITION_GOOD:
				case INTERMEDIATE_GOOD:
				case INTERMEDIATE_C_GOOD:
					break;

				case BUSY:
				case QUEUE_FULL:
					update_timeout(SCpnt, oldto);
					status = REDO;
					break;

				case RESERVATION_CONFLICT:
					/*
					 * Most HAs will return an error for
					 * this, so usually reservation
					 * conflicts will  be processed under
					 * DID_ERROR code
					 */
					printk("scsi%d (%d,%d,%d) : RESERVATION CONFLICT\n", 
					       SCpnt->host->host_no, SCpnt->channel,
					       SCpnt->device->id, SCpnt->device->lun);
					status = CMD_FINISHED; /* returns I/O error */
					break;
                                        
				default:
					printk("Internal error %s %d \n"
					 "status byte = %d \n", __FILE__,
					  __LINE__, status_byte(result));

				}
				break;
			default:
				panic("scsi: unsupported message byte %d received\n",
				      msg_byte(result));
			}
		break;
	case DID_TIME_OUT:
#ifdef DEBUG
		printk("Host returned DID_TIME_OUT - ");
#endif

		if (SCpnt->flags & WAS_TIMEDOUT) {
#ifdef DEBUG
			printk("Aborting\n");
#endif
			/*
			   Allow TEST_UNIT_READY and INQUIRY commands to timeout early
			   without causing resets.  All other commands should be retried.
			 */
			if (SCpnt->cmnd[0] != TEST_UNIT_READY &&
			    SCpnt->cmnd[0] != INQUIRY)
				status = MAYREDO;
			exit = (DRIVER_TIMEOUT | SUGGEST_ABORT);
		} else {
#ifdef DEBUG
			printk("Retrying.\n");
#endif
			SCpnt->flags |= WAS_TIMEDOUT;
			SCpnt->internal_timeout &= ~IN_ABORT;
			status = REDO;
		}
		break;
	case DID_BUS_BUSY:
	case DID_PARITY:
		status = REDO;
		break;
	case DID_NO_CONNECT:
#ifdef DEBUG
		printk("Couldn't connect.\n");
#endif
		exit = (DRIVER_HARD | SUGGEST_ABORT);
		break;
	case DID_ERROR:
		if (msg_byte(result) == COMMAND_COMPLETE &&
		    status_byte(result) == RESERVATION_CONFLICT) {
			printk("scsi%d (%d,%d,%d) : RESERVATION CONFLICT\n", 
			       SCpnt->host->host_no, SCpnt->channel,
			       SCpnt->device->id, SCpnt->device->lun);
			status = CMD_FINISHED; /* returns I/O error */
			break;
		}
		status = MAYREDO;
		exit = (DRIVER_HARD | SUGGEST_ABORT);
		break;
	case DID_BAD_TARGET:
	case DID_ABORT:
		exit = (DRIVER_INVALID | SUGGEST_ABORT);
		break;
	case DID_RESET:
		if (SCpnt->flags & IS_RESETTING) {
			SCpnt->flags &= ~IS_RESETTING;
			status = REDO;
			break;
		}
		if (msg_byte(result) == GOOD &&
		    status_byte(result) == CHECK_CONDITION) {
			switch (check_sense(SCpnt)) {
			case 0:
				update_timeout(SCpnt, oldto);
				status = REDO;
				break;
			case SUGGEST_REMAP:
			case SUGGEST_RETRY:
				status = MAYREDO;
				exit = DRIVER_SENSE | SUGGEST_RETRY;
				break;
			case SUGGEST_ABORT:
				status = CMD_FINISHED;
				exit = DRIVER_SENSE | SUGGEST_ABORT;
				break;
			case SUGGEST_SENSE:
				scsi_request_sense(SCpnt);
				status = PENDING;
				break;
			}
		} else {
			status = REDO;
			exit = SUGGEST_RETRY;
		}
		break;
	default:
		exit = (DRIVER_ERROR | SUGGEST_DIE);
	}

	switch (status) {
	case CMD_FINISHED:
	case PENDING:
		break;
	case MAYREDO:
#ifdef DEBUG
		printk("In MAYREDO, allowing %d retries, have %d\n",
		       SCpnt->allowed, SCpnt->retries);
#endif
		if ((++SCpnt->retries) < SCpnt->allowed) {
			if ((SCpnt->retries >= (SCpnt->allowed >> 1))
			    && !(SCpnt->host->resetting && time_before(jiffies, SCpnt->host->last_reset + MIN_RESET_PERIOD))
			    && !(SCpnt->flags & WAS_RESET)) {
				printk("scsi%d channel %d : resetting for second half of retries.\n",
				   SCpnt->host->host_no, SCpnt->channel);
				scsi_reset(SCpnt, SCSI_RESET_SYNCHRONOUS);
				/* fall through to REDO */
			}
		} else {
			status = CMD_FINISHED;
			break;
		}
		/* fall through to REDO */

	case REDO:

		if (SCpnt->flags & WAS_SENSE)
			scsi_request_sense(SCpnt);
		else {
			memcpy((void *) SCpnt->cmnd,
			       (void *) SCpnt->data_cmnd,
			       sizeof(SCpnt->data_cmnd));
			memset((void *) SCpnt->sense_buffer, 0,
			       sizeof(SCpnt->sense_buffer));
			SCpnt->request_buffer = SCpnt->buffer;
			SCpnt->request_bufflen = SCpnt->bufflen;
			SCpnt->use_sg = SCpnt->old_use_sg;
			SCpnt->cmd_len = SCpnt->old_cmd_len;
			SCpnt->sc_data_direction = SCpnt->sc_old_data_direction;
			SCpnt->underflow = SCpnt->old_underflow;
			SCpnt->result = 0;
                        /*
                         * Ugly, ugly.  The newer interfaces all
                         * assume that the lock isn't held.  Mustn't
                         * disappoint, or we deadlock the system.  
                         */
                        spin_unlock_irq(&io_request_lock);
			scsi_dispatch_cmd(SCpnt);
                        spin_lock_irq(&io_request_lock);
		}
		break;
	default:
		INTERNAL_ERROR;
	}

	if (status == CMD_FINISHED) {
		Scsi_Request *SRpnt;
#ifdef DEBUG
		printk("Calling done function - at address %p\n", SCpnt->done);
#endif
		host->host_busy--;	/* Indicate that we are free */
                device->device_busy--;	/* Decrement device usage counter. */

		SCpnt->result = result | ((exit & 0xff) << 24);
		SCpnt->use_sg = SCpnt->old_use_sg;
		SCpnt->cmd_len = SCpnt->old_cmd_len;
		SCpnt->sc_data_direction = SCpnt->sc_old_data_direction;
		SCpnt->underflow = SCpnt->old_underflow;
                /*
                 * The upper layers assume the lock isn't held.  We mustn't
                 * disappoint them.  When the new error handling code is in
                 * use, the upper code is run from a bottom half handler, so
                 * it isn't an issue.
                 */
                spin_unlock_irq(&io_request_lock);
		SRpnt = SCpnt->sc_request;
		if( SRpnt != NULL ) {
			SRpnt->sr_result = SRpnt->sr_command->result;
			if( SRpnt->sr_result != 0 ) {
				memcpy(SRpnt->sr_sense_buffer,
				       SRpnt->sr_command->sense_buffer,
				       sizeof(SRpnt->sr_sense_buffer));
			}
		}

		SCpnt->done(SCpnt);
                spin_lock_irq(&io_request_lock);
	}
#undef CMD_FINISHED
#undef REDO
#undef MAYREDO
#undef PENDING
}

/*
 * The scsi_abort function interfaces with the abort() function of the host
 * we are aborting, and causes the current command to not complete.  The
 * caller should deal with any error messages or status returned on the
 * next call.
 *
 * This will not be called reentrantly for a given host.
 */

/*
 * Since we're nice guys and specified that abort() and reset()
 * can be non-reentrant.  The internal_timeout flags are used for
 * this.
 */


static int scsi_abort(Scsi_Cmnd * SCpnt, int why)
{
	int oldto;
	struct Scsi_Host *host = SCpnt->host;

	while (1) {

		/*
		 * Protect against races here.  If the command is done, or we are
		 * on a different command forget it.
		 */
		if (SCpnt->serial_number != SCpnt->serial_number_at_timeout) {
			return 0;
		}
		if (SCpnt->internal_timeout & IN_ABORT) {
			spin_unlock_irq(&io_request_lock);
			while (SCpnt->internal_timeout & IN_ABORT)
				barrier();
			spin_lock_irq(&io_request_lock);
		} else {
			SCpnt->internal_timeout |= IN_ABORT;
			oldto = update_timeout(SCpnt, ABORT_TIMEOUT);

			if ((SCpnt->flags & IS_RESETTING) && SCpnt->device->soft_reset) {
				/* OK, this command must have died when we did the
				 *  reset.  The device itself must have lied.
				 */
				printk("Stale command on %d %d:%d appears to have died when"
				       " the bus was reset\n",
				       SCpnt->channel, SCpnt->target, SCpnt->lun);
			}
			if (!host->host_busy) {
				SCpnt->internal_timeout &= ~IN_ABORT;
				update_timeout(SCpnt, oldto);
				return 0;
			}
			printk("scsi : aborting command due to timeout : pid %lu, scsi%d,"
			       " channel %d, id %d, lun %d ",
			       SCpnt->pid, SCpnt->host->host_no, (int) SCpnt->channel,
			       (int) SCpnt->target, (int) SCpnt->lun);
			print_command(SCpnt->cmnd);
			if (SCpnt->serial_number != SCpnt->serial_number_at_timeout)
				return 0;
			SCpnt->abort_reason = why;
			switch (host->hostt->abort(SCpnt)) {
				/* We do not know how to abort.  Try waiting another
				 * time increment and see if this helps. Set the
				 * WAS_TIMEDOUT flag set so we do not try this twice
				 */
			case SCSI_ABORT_BUSY:	/* Tough call - returning 1 from
						 * this is too severe
						 */
			case SCSI_ABORT_SNOOZE:
				if (why == DID_TIME_OUT) {
					SCpnt->internal_timeout &= ~IN_ABORT;
					if (SCpnt->flags & WAS_TIMEDOUT) {
						return 1;	/* Indicate we cannot handle this.
								 * We drop down into the reset handler
								 * and try again
								 */
					} else {
						SCpnt->flags |= WAS_TIMEDOUT;
						oldto = SCpnt->timeout_per_command;
						update_timeout(SCpnt, oldto);
					}
				}
				return 0;
			case SCSI_ABORT_PENDING:
				if (why != DID_TIME_OUT) {
					update_timeout(SCpnt, oldto);
				}
				return 0;
			case SCSI_ABORT_SUCCESS:
				/* We should have already aborted this one.  No
				 * need to adjust timeout
				 */
				SCpnt->internal_timeout &= ~IN_ABORT;
				return 0;
			case SCSI_ABORT_NOT_RUNNING:
				SCpnt->internal_timeout &= ~IN_ABORT;
				update_timeout(SCpnt, 0);
				return 0;
			case SCSI_ABORT_ERROR:
			default:
				SCpnt->internal_timeout &= ~IN_ABORT;
				return 1;
			}
		}
	}
}


/* Mark a single SCSI Device as having been reset. */

static inline void scsi_mark_device_reset(Scsi_Device * Device)
{
	Device->was_reset = 1;
	Device->expecting_cc_ua = 1;
}


/* Mark all SCSI Devices on a specific Host as having been reset. */

void scsi_mark_host_reset(struct Scsi_Host *Host)
{
	Scsi_Cmnd *SCpnt;
	Scsi_Device *SDpnt;

	for (SDpnt = Host->host_queue; SDpnt; SDpnt = SDpnt->next) {
		for (SCpnt = SDpnt->device_queue; SCpnt; SCpnt = SCpnt->next)
			scsi_mark_device_reset(SCpnt->device);
	}
}


/* Mark all SCSI Devices on a specific Host Bus as having been reset. */

static void scsi_mark_bus_reset(struct Scsi_Host *Host, int channel)
{
	Scsi_Cmnd *SCpnt;
	Scsi_Device *SDpnt;

	for (SDpnt = Host->host_queue; SDpnt; SDpnt = SDpnt->next) {
		for (SCpnt = SDpnt->device_queue; SCpnt; SCpnt = SCpnt->next)
			if (SCpnt->channel == channel)
				scsi_mark_device_reset(SCpnt->device);
	}
}


static int scsi_reset(Scsi_Cmnd * SCpnt, unsigned int reset_flags)
{
	int temp;
	Scsi_Cmnd *SCpnt1;
	Scsi_Device *SDpnt;
	struct Scsi_Host *host = SCpnt->host;

	printk("SCSI bus is being reset for host %d channel %d.\n",
	       host->host_no, SCpnt->channel);

#if 0
	/*
	 * First of all, we need to make a recommendation to the low-level
	 * driver as to whether a BUS_DEVICE_RESET should be performed,
	 * or whether we should do a full BUS_RESET.  There is no simple
	 * algorithm here - we basically use a series of heuristics
	 * to determine what we should do.
	 */
	SCpnt->host->suggest_bus_reset = FALSE;

	/*
	 * First see if all of the active devices on the bus have
	 * been jammed up so that we are attempting resets.  If so,
	 * then suggest a bus reset.  Forcing a bus reset could
	 * result in some race conditions, but no more than
	 * you would usually get with timeouts.  We will cross
	 * that bridge when we come to it.
	 *
	 * This is actually a pretty bad idea, since a sequence of
	 * commands will often timeout together and this will cause a
	 * Bus Device Reset followed immediately by a SCSI Bus Reset.
	 * If all of the active devices really are jammed up, the
	 * Bus Device Reset will quickly timeout and scsi_times_out
	 * will follow up with a SCSI Bus Reset anyway.
	 */
	SCpnt1 = host->host_queue;
	while (SCpnt1) {
		if (SCpnt1->request.rq_status != RQ_INACTIVE
		    && (SCpnt1->flags & (WAS_RESET | IS_RESETTING)) == 0)
			break;
		SCpnt1 = SCpnt1->next;
	}
	if (SCpnt1 == NULL) {
		reset_flags |= SCSI_RESET_SUGGEST_BUS_RESET;
	}
	/*
	 * If the code that called us is suggesting a hard reset, then
	 * definitely request it.  This usually occurs because a
	 * BUS_DEVICE_RESET times out.
	 *
	 * Passing reset_flags along takes care of this automatically.
	 */
	if (reset_flags & SCSI_RESET_SUGGEST_BUS_RESET) {
		SCpnt->host->suggest_bus_reset = TRUE;
	}
#endif

	while (1) {

		/*
		 * Protect against races here.  If the command is done, or we are
		 * on a different command forget it.
		 */
		if (reset_flags & SCSI_RESET_ASYNCHRONOUS)
			if (SCpnt->serial_number != SCpnt->serial_number_at_timeout) {
				return 0;
			}
		if (SCpnt->internal_timeout & IN_RESET) {
			spin_unlock_irq(&io_request_lock);
			while (SCpnt->internal_timeout & IN_RESET)
				barrier();
			spin_lock_irq(&io_request_lock);
		} else {
			SCpnt->internal_timeout |= IN_RESET;
			update_timeout(SCpnt, RESET_TIMEOUT);

			if (reset_flags & SCSI_RESET_SYNCHRONOUS)
				SCpnt->flags |= SYNC_RESET;
			if (host->host_busy) {
				for (SDpnt = host->host_queue; SDpnt; SDpnt = SDpnt->next) {
					SCpnt1 = SDpnt->device_queue;
					while (SCpnt1) {
						if (SCpnt1->request.rq_status != RQ_INACTIVE) {
#if 0
							if (!(SCpnt1->flags & IS_RESETTING) &&
							    !(SCpnt1->internal_timeout & IN_ABORT))
								scsi_abort(SCpnt1, DID_RESET);
#endif
							SCpnt1->flags |= (WAS_RESET | IS_RESETTING);
						}
						SCpnt1 = SCpnt1->next;
					}
				}

				host->last_reset = jiffies;
				host->resetting = 1;
				/*
				 * I suppose that the host reset callback will not play
				 * with the resetting field. We have just set the resetting
				 * flag here. -arca
				 */
				temp = host->hostt->reset(SCpnt, reset_flags);
				/*
				   This test allows the driver to introduce an additional bus
				   settle time delay by setting last_reset up to 20 seconds in
				   the future.  In the normal case where the driver does not
				   modify last_reset, it must be assumed that the actual bus
				   reset occurred immediately prior to the return to this code,
				   and so last_reset must be updated to the current time, so
				   that the delay in internal_cmnd will guarantee at least a
				   MIN_RESET_DELAY bus settle time.
				 */
				if (host->last_reset - jiffies > 20UL * HZ)
					host->last_reset = jiffies;
			} else {
				host->host_busy++;
				host->last_reset = jiffies;
				host->resetting = 1;
				SCpnt->flags |= (WAS_RESET | IS_RESETTING);
				/*
				 * I suppose that the host reset callback will not play
				 * with the resetting field. We have just set the resetting
				 * flag here. -arca
				 */
				temp = host->hostt->reset(SCpnt, reset_flags);
				if (time_before(host->last_reset, jiffies) ||
				    (time_after(host->last_reset, jiffies + 20 * HZ)))
					host->last_reset = jiffies;
				host->host_busy--;
			}
			if (reset_flags & SCSI_RESET_SYNCHRONOUS)
				SCpnt->flags &= ~SYNC_RESET;

#ifdef DEBUG
			printk("scsi reset function returned %d\n", temp);
#endif

			/*
			 * Now figure out what we need to do, based upon
			 * what the low level driver said that it did.
			 * If the result is SCSI_RESET_SUCCESS, SCSI_RESET_PENDING,
			 * or SCSI_RESET_WAKEUP, then the low level driver did a
			 * bus device reset or bus reset, so we should go through
			 * and mark one or all of the devices on that bus
			 * as having been reset.
			 */
			switch (temp & SCSI_RESET_ACTION) {
			case SCSI_RESET_SUCCESS:
				if (temp & SCSI_RESET_HOST_RESET)
					scsi_mark_host_reset(host);
				else if (temp & SCSI_RESET_BUS_RESET)
					scsi_mark_bus_reset(host, SCpnt->channel);
				else
					scsi_mark_device_reset(SCpnt->device);
				SCpnt->internal_timeout &= ~(IN_RESET | IN_RESET2 | IN_RESET3);
				return 0;
			case SCSI_RESET_PENDING:
				if (temp & SCSI_RESET_HOST_RESET)
					scsi_mark_host_reset(host);
				else if (temp & SCSI_RESET_BUS_RESET)
					scsi_mark_bus_reset(host, SCpnt->channel);
				else
					scsi_mark_device_reset(SCpnt->device);
			case SCSI_RESET_NOT_RUNNING:
				return 0;
			case SCSI_RESET_PUNT:
				SCpnt->internal_timeout &= ~(IN_RESET | IN_RESET2 | IN_RESET3);
				scsi_request_sense(SCpnt);
				return 0;
			case SCSI_RESET_WAKEUP:
				if (temp & SCSI_RESET_HOST_RESET)
					scsi_mark_host_reset(host);
				else if (temp & SCSI_RESET_BUS_RESET)
					scsi_mark_bus_reset(host, SCpnt->channel);
				else
					scsi_mark_device_reset(SCpnt->device);
				SCpnt->internal_timeout &= ~(IN_RESET | IN_RESET2 | IN_RESET3);
				scsi_request_sense(SCpnt);
				/*
				 * If a bus reset was performed, we
				 * need to wake up each and every command
				 * that was active on the bus or if it was a HBA
				 * reset all active commands on all channels
				 */
				if (temp & SCSI_RESET_HOST_RESET) {
					for (SDpnt = host->host_queue; SDpnt; SDpnt = SDpnt->next) {
						SCpnt1 = SDpnt->device_queue;
						while (SCpnt1) {
							if (SCpnt1->request.rq_status != RQ_INACTIVE
							    && SCpnt1 != SCpnt)
								scsi_request_sense(SCpnt1);
							SCpnt1 = SCpnt1->next;
						}
					}
				} else if (temp & SCSI_RESET_BUS_RESET) {
					for (SDpnt = host->host_queue; SDpnt; SDpnt = SDpnt->next) {
						SCpnt1 = SDpnt->device_queue;
						while (SCpnt1) {
							if (SCpnt1->request.rq_status != RQ_INACTIVE
							&& SCpnt1 != SCpnt
							    && SCpnt1->channel == SCpnt->channel)
								scsi_request_sense(SCpnt);
							SCpnt1 = SCpnt1->next;
						}
					}
				}
				return 0;
			case SCSI_RESET_SNOOZE:
				/* In this case, we set the timeout field to 0
				 * so that this command does not time out any more,
				 * and we return 1 so that we get a message on the
				 * screen.
				 */
				SCpnt->internal_timeout &= ~(IN_RESET | IN_RESET2 | IN_RESET3);
				update_timeout(SCpnt, 0);
				/* If you snooze, you lose... */
			case SCSI_RESET_ERROR:
			default:
				return 1;
			}

			return temp;
		}
	}
}

/*
 * The strategy is to cause the timer code to call scsi_times_out()
 * when the soonest timeout is pending.
 * The arguments are used when we are queueing a new command, because
 * we do not want to subtract the time used from this time, but when we
 * set the timer, we want to take this value into account.
 */

int update_timeout(Scsi_Cmnd * SCset, int timeout)
{
	int rtn;

	/*
	 * We are using the new error handling code to actually register/deregister
	 * timers for timeout.
	 */

	if (!timer_pending(&SCset->eh_timeout)) {
		rtn = 0;
	} else {
		rtn = SCset->eh_timeout.expires - jiffies;
	}

	if (timeout == 0) {
		scsi_delete_timer(SCset);
	} else {
		scsi_add_timer(SCset, timeout, scsi_old_times_out);
	}

	return rtn;
}


/*
 * This function exports SCSI Bus, Device or Host reset capability
 * and is for use with the SCSI generic driver.
 */
int
scsi_old_reset(Scsi_Cmnd *SCpnt, unsigned int flag)
{
	unsigned int old_flags = SCSI_RESET_SYNCHRONOUS;

	switch(flag) {
	case SCSI_TRY_RESET_DEVICE:
		/* no suggestion flags to add, device reset is default */
		break;
	case SCSI_TRY_RESET_BUS:
		old_flags |= SCSI_RESET_SUGGEST_BUS_RESET;
		break;
	case SCSI_TRY_RESET_HOST:
		old_flags |= SCSI_RESET_SUGGEST_HOST_RESET;
		break;
	default:
		return FAILED;
	}

	if (scsi_reset(SCpnt, old_flags))
		return FAILED;
	return SUCCESS;
}

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-indent-level: 4
 * c-brace-imaginary-offset: 0
 * c-brace-offset: -4
 * c-argdecl-indent: 4
 * c-label-offset: -4
 * c-continued-statement-offset: 4
 * c-continued-brace-offset: 0
 * indent-tabs-mode: nil
 * tab-width: 8
 * End:
 */
