/*
 *  scsi_error.c Copyright (C) 1997 Eric Youngdale
 *
 *  SCSI error/timeout handling
 *      Initial versions: Eric Youngdale.  Based upon conversations with
 *                        Leonard Zubkoff and David Miller at Linux Expo, 
 *                        ideas originating from all over the place.
 *
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
#include <linux/smp_lock.h>

#define __KERNEL_SYSCALLS__

#include <linux/unistd.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/dma.h>

#include "scsi.h"
#include "hosts.h"
#include "constants.h"

/*
 * We must always allow SHUTDOWN_SIGS.  Even if we are not a module,
 * the host drivers that we are using may be loaded as modules, and
 * when we unload these,  we need to ensure that the error handler thread
 * can be shut down.
 *
 * Note - when we unload a module, we send a SIGHUP.  We mustn't
 * enable SIGTERM, as this is how the init shuts things down when you
 * go to single-user mode.  For that matter, init also sends SIGKILL,
 * so we mustn't enable that one either.  We use SIGHUP instead.  Other
 * options would be SIGPWR, I suppose.
 */
#define SHUTDOWN_SIGS	(sigmask(SIGHUP))

#ifdef DEBUG
#define SENSE_TIMEOUT SCSI_TIMEOUT
#define ABORT_TIMEOUT SCSI_TIMEOUT
#define RESET_TIMEOUT SCSI_TIMEOUT
#else
#define SENSE_TIMEOUT (10*HZ)
#define RESET_TIMEOUT (2*HZ)
#define ABORT_TIMEOUT (15*HZ)
#endif

#define STATIC

/*
 * These should *probably* be handled by the host itself.
 * Since it is allowed to sleep, it probably should.
 */
#define BUS_RESET_SETTLE_TIME   5*HZ
#define HOST_RESET_SETTLE_TIME  10*HZ


static const char RCSid[] = "$Header: /mnt/ide/home/eric/CVSROOT/linux/drivers/scsi/scsi_error.c,v 1.10 1997/12/08 04:50:35 eric Exp $";

STATIC int scsi_check_sense(Scsi_Cmnd * SCpnt);
STATIC int scsi_request_sense(Scsi_Cmnd *);
STATIC void scsi_send_eh_cmnd(Scsi_Cmnd * SCpnt, int timeout);
STATIC int scsi_try_to_abort_command(Scsi_Cmnd *, int);
STATIC int scsi_test_unit_ready(Scsi_Cmnd *);
STATIC int scsi_try_bus_device_reset(Scsi_Cmnd *, int timeout);
STATIC int scsi_try_bus_reset(Scsi_Cmnd *);
STATIC int scsi_try_host_reset(Scsi_Cmnd *);
STATIC int scsi_unit_is_ready(Scsi_Cmnd *);
STATIC void scsi_eh_action_done(Scsi_Cmnd *, int);
STATIC int scsi_eh_retry_command(Scsi_Cmnd *);
STATIC int scsi_eh_completed_normally(Scsi_Cmnd * SCpnt);
STATIC void scsi_restart_operations(struct Scsi_Host *);
STATIC void scsi_eh_finish_command(Scsi_Cmnd ** SClist, Scsi_Cmnd * SCpnt);


/*
 * Function:    scsi_add_timer()
 *
 * Purpose:     Start timeout timer for a single scsi command.
 *
 * Arguments:   SCset   - command that is about to start running.
 *              timeout - amount of time to allow this command to run.
 *              complete - timeout function to call if timer isn't
 *                      canceled.
 *
 * Returns:     Nothing
 *
 * Notes:       This should be turned into an inline function.
 *
 * More Notes:  Each scsi command has it's own timer, and as it is added to
 *              the queue, we set up the timer.  When the command completes,
 *              we cancel the timer.  Pretty simple, really, especially
 *              compared to the old way of handling this crap.
 */
void scsi_add_timer(Scsi_Cmnd * SCset,
		    int timeout,
		    void (*complete) (Scsi_Cmnd *))
{
	SCset->eh_timeout.data = (unsigned long) SCset;
	SCset->eh_timeout.function = (void (*)(unsigned long)) complete;
	mod_timer(&SCset->eh_timeout, jiffies + timeout);

	SCset->done_late = 0;

	SCSI_LOG_ERROR_RECOVERY(5, printk("Adding timer for command %p at %d (%p)\n", SCset, timeout, complete));
}

/*
 * Function:    scsi_delete_timer()
 *
 * Purpose:     Delete/cancel timer for a given function.
 *
 * Arguments:   SCset   - command that we are canceling timer for.
 *
 * Returns:     1 if we were able to detach the timer.  0 if we
 *              blew it, and the timer function has already started
 *              to run.
 *
 * Notes:       This should be turned into an inline function.
 */
int scsi_delete_timer(Scsi_Cmnd * SCset)
{
	int rtn;

	rtn = del_timer(&SCset->eh_timeout);

	SCSI_LOG_ERROR_RECOVERY(5, printk("Clearing timer for command %p %d\n", SCset, rtn));

	SCset->eh_timeout.data = (unsigned long) NULL;
	SCset->eh_timeout.function = NULL;

	return rtn;
}

/*
 * Function:    scsi_times_out()
 *
 * Purpose:     Timeout function for normal scsi commands..
 *
 * Arguments:   SCpnt   - command that is timing out.
 *
 * Returns:     Nothing.
 *
 * Notes:       We do not need to lock this.  There is the potential for
 *              a race only in that the normal completion handling might
 *              run, but if the normal completion function determines
 *              that the timer has already fired, then it mustn't do
 *              anything.
 */
void scsi_times_out(Scsi_Cmnd * SCpnt)
{
	/* 
	 * Notify the low-level code that this operation failed and we are
	 * reposessing the command.  
	 */
#ifdef ERIC_neverdef
	/*
	 * FIXME(eric)
	 * Allow the host adapter to push a queue ordering tag
	 * out to the bus to force the command in question to complete.
	 * If the host wants to do this, then we just restart the timer
	 * for the command.  Before we really do this, some real thought
	 * as to the optimum way to handle this should be done.  We *do*
	 * need to force ordering every so often to ensure that all requests
	 * do eventually complete, but I am not sure if this is the best way
	 * to actually go about it.
	 *
	 * Better yet, force a sync here, but don't block since we are in an
	 * interrupt.
	 */
	if (SCpnt->host->hostt->eh_ordered_queue_tag) {
		if ((*SCpnt->host->hostt->eh_ordered_queue_tag) (SCpnt)) {
			scsi_add_timer(SCpnt, SCpnt->internal_timeout,
				       scsi_times_out);
			return;
		}
	}
	/*
	 * FIXME(eric) - add a second special interface to handle this
	 * case.  Ideally that interface can also be used to request
	 * a queu
	 */
	if (SCpnt->host->can_queue) {
		SCpnt->host->hostt->queuecommand(SCpnt, NULL);
	}
#endif

	/* Set the serial_number_at_timeout to the current serial_number */
	SCpnt->serial_number_at_timeout = SCpnt->serial_number;

	SCpnt->eh_state = FAILED;
	SCpnt->state = SCSI_STATE_TIMEOUT;
	SCpnt->owner = SCSI_OWNER_ERROR_HANDLER;

	SCpnt->host->in_recovery = 1;
	SCpnt->host->host_failed++;

	SCSI_LOG_TIMEOUT(3, printk("Command timed out active=%d busy=%d failed=%d\n",
				   atomic_read(&SCpnt->host->host_active),
				   SCpnt->host->host_busy,
				   SCpnt->host->host_failed));

	/*
	 * If the host is having troubles, then look to see if this was the last
	 * command that might have failed.  If so, wake up the error handler.
	 */
	if( SCpnt->host->eh_wait == NULL ) {
		panic("Error handler thread not present at %p %p %s %d", 
		      SCpnt, SCpnt->host, __FILE__, __LINE__);
	}
	if (SCpnt->host->host_busy == SCpnt->host->host_failed) {
		up(SCpnt->host->eh_wait);
	}
}

/*
 * Function     scsi_block_when_processing_errors
 *
 * Purpose:     Prevent more commands from being queued while error recovery
 *              is taking place.
 *
 * Arguments:   SDpnt - device on which we are performing recovery.
 *
 * Returns:     FALSE   The device was taken offline by error recovery.
 *              TRUE    OK to proceed.
 *
 * Notes:       We block until the host is out of error recovery, and then
 *              check to see whether the host or the device is offline.
 */
int scsi_block_when_processing_errors(Scsi_Device * SDpnt)
{

	SCSI_SLEEP(&SDpnt->host->host_wait, SDpnt->host->in_recovery);

	SCSI_LOG_ERROR_RECOVERY(5, printk("Open returning %d\n", SDpnt->online));

	return SDpnt->online;
}

/*
 * Function:    scsi_eh_times_out()
 *
 * Purpose:     Timeout function for error handling.
 *
 * Arguments:   SCpnt   - command that is timing out.
 *
 * Returns:     Nothing.
 *
 * Notes:       During error handling, the kernel thread will be sleeping
 *              waiting for some action to complete on the device.  Our only
 *              job is to record that it timed out, and to wake up the
 *              thread.
 */
STATIC
void scsi_eh_times_out(Scsi_Cmnd * SCpnt)
{
	SCpnt->eh_state = SCSI_STATE_TIMEOUT;
	SCSI_LOG_ERROR_RECOVERY(5, printk("In scsi_eh_times_out %p\n", SCpnt));

	if (SCpnt->host->eh_action != NULL)
		up(SCpnt->host->eh_action);
	else
		printk("Missing scsi error handler thread\n");
}


/*
 * Function:    scsi_eh_done()
 *
 * Purpose:     Completion function for error handling.
 *
 * Arguments:   SCpnt   - command that is timing out.
 *
 * Returns:     Nothing.
 *
 * Notes:       During error handling, the kernel thread will be sleeping
 *              waiting for some action to complete on the device.  Our only
 *              job is to record that the action completed, and to wake up the
 *              thread.
 */
STATIC
void scsi_eh_done(Scsi_Cmnd * SCpnt)
{
	int     rtn;

	/*
	 * If the timeout handler is already running, then just set the
	 * flag which says we finished late, and return.  We have no
	 * way of stopping the timeout handler from running, so we must
	 * always defer to it.
	 */
	rtn = del_timer(&SCpnt->eh_timeout);
	if (!rtn) {
		SCpnt->done_late = 1;
		return;
	}

	SCpnt->request.rq_status = RQ_SCSI_DONE;

	SCpnt->owner = SCSI_OWNER_ERROR_HANDLER;
	SCpnt->eh_state = SUCCESS;

	SCSI_LOG_ERROR_RECOVERY(5, printk("In eh_done %p result:%x\n", SCpnt,
					  SCpnt->result));

	if (SCpnt->host->eh_action != NULL)
		up(SCpnt->host->eh_action);
}

/*
 * Function:    scsi_eh_action_done()
 *
 * Purpose:     Completion function for error handling.
 *
 * Arguments:   SCpnt   - command that is timing out.
 *              answer  - boolean that indicates whether operation succeeded.
 *
 * Returns:     Nothing.
 *
 * Notes:       This callback is only used for abort and reset operations.
 */
STATIC
void scsi_eh_action_done(Scsi_Cmnd * SCpnt, int answer)
{
	SCpnt->request.rq_status = RQ_SCSI_DONE;

	SCpnt->owner = SCSI_OWNER_ERROR_HANDLER;
	SCpnt->eh_state = (answer ? SUCCESS : FAILED);

	if (SCpnt->host->eh_action != NULL)
		up(SCpnt->host->eh_action);
}

/*
 * Function:  scsi_sense_valid()
 *
 * Purpose:     Determine whether a host has automatically obtained sense
 *              information or not.  If we have it, then give a recommendation
 *              as to what we should do next.
 */
int scsi_sense_valid(Scsi_Cmnd * SCpnt)
{
	if (((SCpnt->sense_buffer[0] & 0x70) >> 4) != 7) {
		return FALSE;
	}
	return TRUE;
}

/*
 * Function:  scsi_eh_retry_command()
 *
 * Purpose:     Retry the original command
 *
 * Returns:     SUCCESS - we were able to get the sense data.
 *              FAILED  - we were not able to get the sense data.
 * 
 * Notes:       This function will *NOT* return until the command either
 *              times out, or it completes.
 */
STATIC int scsi_eh_retry_command(Scsi_Cmnd * SCpnt)
{
	memcpy((void *) SCpnt->cmnd, (void *) SCpnt->data_cmnd,
	       sizeof(SCpnt->data_cmnd));
	SCpnt->request_buffer = SCpnt->buffer;
	SCpnt->request_bufflen = SCpnt->bufflen;
	SCpnt->use_sg = SCpnt->old_use_sg;
	SCpnt->cmd_len = SCpnt->old_cmd_len;
	SCpnt->sc_data_direction = SCpnt->sc_old_data_direction;
	SCpnt->underflow = SCpnt->old_underflow;

	scsi_send_eh_cmnd(SCpnt, SCpnt->timeout_per_command);

	/*
	 * Hey, we are done.  Let's look to see what happened.
	 */
	return SCpnt->eh_state;
}

/*
 * Function:  scsi_request_sense()
 *
 * Purpose:     Request sense data from a particular target.
 *
 * Returns:     SUCCESS - we were able to get the sense data.
 *              FAILED  - we were not able to get the sense data.
 * 
 * Notes:       Some hosts automatically obtain this information, others
 *              require that we obtain it on our own.
 *
 *              This function will *NOT* return until the command either
 *              times out, or it completes.
 */
STATIC int scsi_request_sense(Scsi_Cmnd * SCpnt)
{
	static unsigned char generic_sense[6] =
	{REQUEST_SENSE, 0, 0, 0, 255, 0};
	unsigned char scsi_result0[256], *scsi_result = NULL;
	int saved_result;

	ASSERT_LOCK(&io_request_lock, 0);

	memcpy((void *) SCpnt->cmnd, (void *) generic_sense,
	       sizeof(generic_sense));

	if (SCpnt->device->scsi_level <= SCSI_2)
		SCpnt->cmnd[1] = SCpnt->lun << 5;

	scsi_result = (!SCpnt->host->hostt->unchecked_isa_dma)
	    ? &scsi_result0[0] : kmalloc(512, GFP_ATOMIC | GFP_DMA);

	if (scsi_result == NULL) {
		printk("cannot allocate scsi_result in scsi_request_sense.\n");
		return FAILED;
	}
	/*
	 * Zero the sense buffer.  Some host adapters automatically always request
	 * sense, so it is not a good idea that SCpnt->request_buffer and
	 * SCpnt->sense_buffer point to the same address (DB).
	 * 0 is not a valid sense code. 
	 */
	memset((void *) SCpnt->sense_buffer, 0, sizeof(SCpnt->sense_buffer));
	memset((void *) scsi_result, 0, 256);

	saved_result = SCpnt->result;
	SCpnt->request_buffer = scsi_result;
	SCpnt->request_bufflen = 256;
	SCpnt->use_sg = 0;
	SCpnt->cmd_len = COMMAND_SIZE(SCpnt->cmnd[0]);
	SCpnt->sc_data_direction = SCSI_DATA_READ;
	SCpnt->underflow = 0;

	scsi_send_eh_cmnd(SCpnt, SENSE_TIMEOUT);

	/* Last chance to have valid sense data */
	if (!scsi_sense_valid(SCpnt))
		memcpy((void *) SCpnt->sense_buffer,
		       SCpnt->request_buffer,
		       sizeof(SCpnt->sense_buffer));

	if (scsi_result != &scsi_result0[0] && scsi_result != NULL)
		kfree(scsi_result);

	/*
	 * When we eventually call scsi_finish, we really wish to complete
	 * the original request, so let's restore the original data. (DB)
	 */
	memcpy((void *) SCpnt->cmnd, (void *) SCpnt->data_cmnd,
	       sizeof(SCpnt->data_cmnd));
	SCpnt->result = saved_result;
	SCpnt->request_buffer = SCpnt->buffer;
	SCpnt->request_bufflen = SCpnt->bufflen;
	SCpnt->use_sg = SCpnt->old_use_sg;
	SCpnt->cmd_len = SCpnt->old_cmd_len;
	SCpnt->sc_data_direction = SCpnt->sc_old_data_direction;
	SCpnt->underflow = SCpnt->old_underflow;

	/*
	 * Hey, we are done.  Let's look to see what happened.
	 */
	return SCpnt->eh_state;
}

/*
 * Function:  scsi_test_unit_ready()
 *
 * Purpose:     Run test unit ready command to see if the device is talking to us or not.
 *
 */
STATIC int scsi_test_unit_ready(Scsi_Cmnd * SCpnt)
{
	static unsigned char tur_command[6] =
	{TEST_UNIT_READY, 0, 0, 0, 0, 0};

	memcpy((void *) SCpnt->cmnd, (void *) tur_command,
	       sizeof(tur_command));

	if (SCpnt->device->scsi_level <= SCSI_2)
		SCpnt->cmnd[1] = SCpnt->lun << 5;

	/*
	 * Zero the sense buffer.  The SCSI spec mandates that any
	 * untransferred sense data should be interpreted as being zero.
	 */
	memset((void *) SCpnt->sense_buffer, 0, sizeof(SCpnt->sense_buffer));

	SCpnt->request_buffer = NULL;
	SCpnt->request_bufflen = 0;
	SCpnt->use_sg = 0;
	SCpnt->cmd_len = COMMAND_SIZE(SCpnt->cmnd[0]);
	SCpnt->underflow = 0;
	SCpnt->sc_data_direction = SCSI_DATA_NONE;

	scsi_send_eh_cmnd(SCpnt, SENSE_TIMEOUT);

	/*
	 * When we eventually call scsi_finish, we really wish to complete
	 * the original request, so let's restore the original data. (DB)
	 */
	memcpy((void *) SCpnt->cmnd, (void *) SCpnt->data_cmnd,
	       sizeof(SCpnt->data_cmnd));
	SCpnt->request_buffer = SCpnt->buffer;
	SCpnt->request_bufflen = SCpnt->bufflen;
	SCpnt->use_sg = SCpnt->old_use_sg;
	SCpnt->cmd_len = SCpnt->old_cmd_len;
	SCpnt->sc_data_direction = SCpnt->sc_old_data_direction;
	SCpnt->underflow = SCpnt->old_underflow;

	/*
	 * Hey, we are done.  Let's look to see what happened.
	 */
	SCSI_LOG_ERROR_RECOVERY(3,
		printk("scsi_test_unit_ready: SCpnt %p eh_state %x\n",
		SCpnt, SCpnt->eh_state));
	return SCpnt->eh_state;
}

/*
 * This would normally need to get the IO request lock,
 * but as it doesn't actually touch anything that needs
 * to be locked we can avoid the lock here..
 */
STATIC
void scsi_sleep_done(struct semaphore *sem)
{
	if (sem != NULL) {
		up(sem);
	}
}

void scsi_sleep(int timeout)
{
	DECLARE_MUTEX_LOCKED(sem);
	struct timer_list timer;

	init_timer(&timer);
	timer.data = (unsigned long) &sem;
	timer.expires = jiffies + timeout;
	timer.function = (void (*)(unsigned long)) scsi_sleep_done;

	SCSI_LOG_ERROR_RECOVERY(5, printk("Sleeping for timer tics %d\n", timeout));

	add_timer(&timer);

	down(&sem);
	del_timer(&timer);
}

/*
 * Function:  scsi_send_eh_cmnd
 *
 * Purpose:     Send a command out to a device as part of error recovery.
 *
 * Notes:       The initialization of the structures is quite a bit different
 *              in this case, and furthermore, there is a different completion
 *              handler.
 */
STATIC void scsi_send_eh_cmnd(Scsi_Cmnd * SCpnt, int timeout)
{
	unsigned long flags;
	struct Scsi_Host *host;

	ASSERT_LOCK(&io_request_lock, 0);

	host = SCpnt->host;

      retry:
	/*
	 * We will use a queued command if possible, otherwise we will emulate the
	 * queuing and calling of completion function ourselves.
	 */
	SCpnt->owner = SCSI_OWNER_LOWLEVEL;

	if (host->can_queue) {
		DECLARE_MUTEX_LOCKED(sem);

		SCpnt->eh_state = SCSI_STATE_QUEUED;

		scsi_add_timer(SCpnt, timeout, scsi_eh_times_out);

		/*
		 * Set up the semaphore so we wait for the command to complete.
		 */
		SCpnt->host->eh_action = &sem;
		SCpnt->request.rq_status = RQ_SCSI_BUSY;

		spin_lock_irqsave(&io_request_lock, flags);
		host->hostt->queuecommand(SCpnt, scsi_eh_done);
		spin_unlock_irqrestore(&io_request_lock, flags);

		down(&sem);

		SCpnt->host->eh_action = NULL;

		/*
		 * See if timeout.  If so, tell the host to forget about it.
		 * In other words, we don't want a callback any more.
		 */
		if (SCpnt->eh_state == SCSI_STATE_TIMEOUT) {
                        SCpnt->owner = SCSI_OWNER_LOWLEVEL;

			/*
			 * As far as the low level driver is
			 * concerned, this command is still active, so
			 * we must give the low level driver a chance
			 * to abort it. (DB) 
			 *
			 * FIXME(eric) - we are not tracking whether we could
			 * abort a timed out command or not.  Not sure how
			 * we should treat them differently anyways.
			 */
			spin_lock_irqsave(&io_request_lock, flags);
			if (SCpnt->host->hostt->eh_abort_handler)
				SCpnt->host->hostt->eh_abort_handler(SCpnt);
			spin_unlock_irqrestore(&io_request_lock, flags);
			
			SCpnt->request.rq_status = RQ_SCSI_DONE;
			SCpnt->owner = SCSI_OWNER_ERROR_HANDLER;
			
			SCpnt->eh_state = FAILED;
		}
		SCSI_LOG_ERROR_RECOVERY(5, printk("send_eh_cmnd: %p eh_state:%x\n",
						SCpnt, SCpnt->eh_state));
	} else {
		int temp;

		/*
		 * We damn well had better never use this code.  There is no timeout
		 * protection here, since we would end up waiting in the actual low
		 * level driver, we don't know how to wake it up.
		 */
		spin_lock_irqsave(&io_request_lock, flags);
		temp = host->hostt->command(SCpnt);
		spin_unlock_irqrestore(&io_request_lock, flags);

		SCpnt->result = temp;
		/* Fall through to code below to examine status. */
		SCpnt->eh_state = SUCCESS;
	}

	/*
	 * Now examine the actual status codes to see whether the command actually
	 * did complete normally.
	 */
	if (SCpnt->eh_state == SUCCESS) {
		int ret = scsi_eh_completed_normally(SCpnt);
		SCSI_LOG_ERROR_RECOVERY(3,
			printk("scsi_send_eh_cmnd: scsi_eh_completed_normally %x\n", ret));
		switch (ret) {
		case SUCCESS:
			SCpnt->eh_state = SUCCESS;
			break;
		case NEEDS_RETRY:
			goto retry;
		case FAILED:
		default:
			SCpnt->eh_state = FAILED;
			break;
		}
	} else {
		SCpnt->eh_state = FAILED;
	}
}

/*
 * Function:  scsi_unit_is_ready()
 *
 * Purpose:     Called after TEST_UNIT_READY is run, to test to see if
 *              the unit responded in a way that indicates it is ready.
 */
STATIC int scsi_unit_is_ready(Scsi_Cmnd * SCpnt)
{
	if (SCpnt->result) {
		if (((driver_byte(SCpnt->result) & DRIVER_SENSE) ||
		     (status_byte(SCpnt->result) & CHECK_CONDITION)) &&
		    ((SCpnt->sense_buffer[0] & 0x70) >> 4) == 7) {
			if (((SCpnt->sense_buffer[2] & 0xf) != NOT_READY) &&
			    ((SCpnt->sense_buffer[2] & 0xf) != UNIT_ATTENTION) &&
			    ((SCpnt->sense_buffer[2] & 0xf) != ILLEGAL_REQUEST)) {
				return 0;
			}
		}
	}
	return 1;
}

/*
 * Function:    scsi_eh_finish_command
 *
 * Purpose:     Handle a command that we are finished with WRT error handling.
 *
 * Arguments:   SClist - pointer to list into which we are putting completed commands.
 *              SCpnt  - command that is completing
 *
 * Notes:       We don't want to use the normal command completion while we are
 *              are still handling errors - it may cause other commands to be queued,
 *              and that would disturb what we are doing.  Thus we really want to keep
 *              a list of pending commands for final completion, and once we
 *              are ready to leave error handling we handle completion for real.
 */
STATIC void scsi_eh_finish_command(Scsi_Cmnd ** SClist, Scsi_Cmnd * SCpnt)
{
	SCpnt->state = SCSI_STATE_BHQUEUE;
	SCpnt->bh_next = *SClist;
	/*
	 * Set this back so that the upper level can correctly free up
	 * things.
	 */
	SCpnt->use_sg = SCpnt->old_use_sg;
	SCpnt->sc_data_direction = SCpnt->sc_old_data_direction;
	SCpnt->underflow = SCpnt->old_underflow;
	*SClist = SCpnt;
}

/*
 * Function:  scsi_try_to_abort_command
 *
 * Purpose:     Ask host adapter to abort a running command.
 *
 * Returns:     FAILED          Operation failed or not supported.
 *              SUCCESS         Succeeded.
 *
 * Notes:       This function will not return until the user's completion
 *              function has been called.  There is no timeout on this
 *              operation.  If the author of the low-level driver wishes
 *              this operation to be timed, they can provide this facility
 *              themselves.  Helper functions in scsi_error.c can be supplied
 *              to make this easier to do.
 *
 * Notes:       It may be possible to combine this with all of the reset
 *              handling to eliminate a lot of code duplication.  I don't
 *              know what makes more sense at the moment - this is just a
 *              prototype.
 */
STATIC int scsi_try_to_abort_command(Scsi_Cmnd * SCpnt, int timeout)
{
	int rtn;
	unsigned long flags;

	SCpnt->eh_state = FAILED;	/* Until we come up with something better */

	if (SCpnt->host->hostt->eh_abort_handler == NULL) {
		return FAILED;
	}
	/* 
	 * scsi_done was called just after the command timed out and before
	 * we had a chance to process it. (DB)
	 */
	if (SCpnt->serial_number == 0)
		return SUCCESS;

	SCpnt->owner = SCSI_OWNER_LOWLEVEL;

	spin_lock_irqsave(&io_request_lock, flags);
	rtn = SCpnt->host->hostt->eh_abort_handler(SCpnt);
	spin_unlock_irqrestore(&io_request_lock, flags);
	return rtn;
}

/*
 * Function:  scsi_try_bus_device_reset
 *
 * Purpose:     Ask host adapter to perform a bus device reset for a given
 *              device.
 *
 * Returns:     FAILED          Operation failed or not supported.
 *              SUCCESS         Succeeded.
 *
 * Notes:       There is no timeout for this operation.  If this operation is
 *              unreliable for a given host, then the host itself needs to put a
 *              timer on it, and set the host back to a consistent state prior
 *              to returning.
 */
STATIC int scsi_try_bus_device_reset(Scsi_Cmnd * SCpnt, int timeout)
{
	unsigned long flags;
	int rtn;

	SCpnt->eh_state = FAILED;	/* Until we come up with something better */

	if (SCpnt->host->hostt->eh_device_reset_handler == NULL) {
		return FAILED;
	}
	SCpnt->owner = SCSI_OWNER_LOWLEVEL;

	spin_lock_irqsave(&io_request_lock, flags);
	rtn = SCpnt->host->hostt->eh_device_reset_handler(SCpnt);
	spin_unlock_irqrestore(&io_request_lock, flags);

	if (rtn == SUCCESS)
		SCpnt->eh_state = SUCCESS;

	return SCpnt->eh_state;
}

/*
 * Function:  scsi_try_bus_reset
 *
 * Purpose:     Ask host adapter to perform a bus reset for a host.
 *
 * Returns:     FAILED          Operation failed or not supported.
 *              SUCCESS         Succeeded.
 *
 * Notes:       
 */
STATIC int scsi_try_bus_reset(Scsi_Cmnd * SCpnt)
{
	unsigned long flags;
	int rtn;

	SCpnt->eh_state = FAILED;	/* Until we come up with something better */
	SCpnt->owner = SCSI_OWNER_LOWLEVEL;
	SCpnt->serial_number_at_timeout = SCpnt->serial_number;

	if (SCpnt->host->hostt->eh_bus_reset_handler == NULL) {
		return FAILED;
	}

	spin_lock_irqsave(&io_request_lock, flags);
	rtn = SCpnt->host->hostt->eh_bus_reset_handler(SCpnt);
	spin_unlock_irqrestore(&io_request_lock, flags);

	if (rtn == SUCCESS)
		SCpnt->eh_state = SUCCESS;

	/*
	 * If we had a successful bus reset, mark the command blocks to expect
	 * a condition code of unit attention.
	 */
	scsi_sleep(BUS_RESET_SETTLE_TIME);
	if (SCpnt->eh_state == SUCCESS) {
		Scsi_Device *SDloop;
		for (SDloop = SCpnt->host->host_queue; SDloop; SDloop = SDloop->next) {
			if (SCpnt->channel == SDloop->channel) {
				SDloop->was_reset = 1;
				SDloop->expecting_cc_ua = 1;
			}
		}
	}
	return SCpnt->eh_state;
}

/*
 * Function:  scsi_try_host_reset
 *
 * Purpose:     Ask host adapter to reset itself, and the bus.
 *
 * Returns:     FAILED          Operation failed or not supported.
 *              SUCCESS         Succeeded.
 *
 * Notes:
 */
STATIC int scsi_try_host_reset(Scsi_Cmnd * SCpnt)
{
	unsigned long flags;
	int rtn;

	SCpnt->eh_state = FAILED;	/* Until we come up with something better */
	SCpnt->owner = SCSI_OWNER_LOWLEVEL;
	SCpnt->serial_number_at_timeout = SCpnt->serial_number;

	if (SCpnt->host->hostt->eh_host_reset_handler == NULL) {
		return FAILED;
	}
	spin_lock_irqsave(&io_request_lock, flags);
	rtn = SCpnt->host->hostt->eh_host_reset_handler(SCpnt);
	spin_unlock_irqrestore(&io_request_lock, flags);

	if (rtn == SUCCESS)
		SCpnt->eh_state = SUCCESS;

	/*
	 * If we had a successful host reset, mark the command blocks to expect
	 * a condition code of unit attention.
	 */
	scsi_sleep(HOST_RESET_SETTLE_TIME);
	if (SCpnt->eh_state == SUCCESS) {
		Scsi_Device *SDloop;
		for (SDloop = SCpnt->host->host_queue; SDloop; SDloop = SDloop->next) {
			SDloop->was_reset = 1;
			SDloop->expecting_cc_ua = 1;
		}
	}
	return SCpnt->eh_state;
}

/*
 * Function:  scsi_decide_disposition
 *
 * Purpose:     Examine a command block that has come back from the low-level
 *              and figure out what to do next.
 *
 * Returns:     SUCCESS         - pass on to upper level.
 *              FAILED          - pass on to error handler thread.
 *              RETRY           - command should be retried.
 *              SOFTERR         - command succeeded, but we need to log
 *                                a soft error.
 *
 * Notes:       This is *ONLY* called when we are examining the status
 *              after sending out the actual data command.  Any commands
 *              that are queued for error recovery (i.e. TEST_UNIT_READY)
 *              do *NOT* come through here.
 *
 *              NOTE - When this routine returns FAILED, it means the error
 *              handler thread is woken.  In cases where the error code
 *              indicates an error that doesn't require the error handler
 *              thread (i.e. we don't need to abort/reset), then this function
 *              should return SUCCESS.
 */
int scsi_decide_disposition(Scsi_Cmnd * SCpnt)
{
	int rtn;

	/*
	 * If the device is offline, then we clearly just pass the result back
	 * up to the top level.
	 */
	if (SCpnt->device->online == FALSE) {
		SCSI_LOG_ERROR_RECOVERY(5, printk("scsi_error.c: device offline - report as SUCCESS\n"));
		return SUCCESS;
	}
	/*
	 * First check the host byte, to see if there is anything in there
	 * that would indicate what we need to do.
	 */

	switch (host_byte(SCpnt->result)) {
	case DID_PASSTHROUGH:
		/*
		 * No matter what, pass this through to the upper layer.
		 * Nuke this special code so that it looks like we are saying
		 * DID_OK.
		 */
		SCpnt->result &= 0xff00ffff;
		return SUCCESS;
	case DID_OK:
		/*
		 * Looks good.  Drop through, and check the next byte.
		 */
		break;
	case DID_NO_CONNECT:
	case DID_BAD_TARGET:
	case DID_ABORT:
		/*
		 * Note - this means that we just report the status back to the
		 * top level driver, not that we actually think that it indicates
		 * success.
		 */
		return SUCCESS;
		/*
		 * When the low level driver returns DID_SOFT_ERROR,
		 * it is responsible for keeping an internal retry counter 
		 * in order to avoid endless loops (DB)
		 *
		 * Actually this is a bug in this function here.  We should
		 * be mindful of the maximum number of retries specified
		 * and not get stuck in a loop.
		 */
	case DID_SOFT_ERROR:
		goto maybe_retry;

	case DID_ERROR:
		if (msg_byte(SCpnt->result) == COMMAND_COMPLETE &&
		    status_byte(SCpnt->result) == RESERVATION_CONFLICT)
			/*
			 * execute reservation conflict processing code
			 * lower down
			 */
			break;
		/* FALLTHROUGH */

	case DID_BUS_BUSY:
	case DID_PARITY:
		goto maybe_retry;
	case DID_TIME_OUT:
		/*
		 * When we scan the bus, we get timeout messages for
		 * these commands if there is no device available.
		 * Other hosts report DID_NO_CONNECT for the same thing.
		 */
		if ((SCpnt->cmnd[0] == TEST_UNIT_READY ||
		     SCpnt->cmnd[0] == INQUIRY)) {
			return SUCCESS;
		} else {
			return FAILED;
		}
	case DID_RESET:
		/*
		 * In the normal case where we haven't initiated a reset, this is
		 * a failure.
		 */
		if (SCpnt->flags & IS_RESETTING) {
			SCpnt->flags &= ~IS_RESETTING;
			goto maybe_retry;
		}
		return SUCCESS;
	default:
		return FAILED;
	}

	/*
	 * Next, check the message byte.
	 */
	if (msg_byte(SCpnt->result) != COMMAND_COMPLETE) {
		return FAILED;
	}
	/*
	 * Now, check the status byte to see if this indicates anything special.
	 */
	switch (status_byte(SCpnt->result)) {
	case QUEUE_FULL:
		/*
		 * The case of trying to send too many commands to a tagged queueing
		 * device.
		 */
		return ADD_TO_MLQUEUE;
	case GOOD:
	case COMMAND_TERMINATED:
		return SUCCESS;
	case CHECK_CONDITION:
		rtn = scsi_check_sense(SCpnt);
		if (rtn == NEEDS_RETRY) {
			goto maybe_retry;
		}
		return rtn;
	case CONDITION_GOOD:
	case INTERMEDIATE_GOOD:
	case INTERMEDIATE_C_GOOD:
		/*
		 * Who knows?  FIXME(eric)
		 */
		return SUCCESS;
	case BUSY:
		goto maybe_retry;

	case RESERVATION_CONFLICT:
		printk("scsi%d (%d,%d,%d) : RESERVATION CONFLICT\n", 
		       SCpnt->host->host_no, SCpnt->channel,
		       SCpnt->device->id, SCpnt->device->lun);
		return SUCCESS; /* causes immediate I/O error */
	default:
		return FAILED;
	}
	return FAILED;

      maybe_retry:

	if ((++SCpnt->retries) < SCpnt->allowed) {
		return NEEDS_RETRY;
	} else {
                /*
                 * No more retries - report this one back to upper level.
                 */
		return SUCCESS;
	}
}

/*
 * Function:  scsi_eh_completed_normally
 *
 * Purpose:     Examine a command block that has come back from the low-level
 *              and figure out what to do next.
 *
 * Returns:     SUCCESS         - pass on to upper level.
 *              FAILED          - pass on to error handler thread.
 *              RETRY           - command should be retried.
 *              SOFTERR         - command succeeded, but we need to log
 *                                a soft error.
 *
 * Notes:       This is *ONLY* called when we are examining the status
 *              of commands queued during error recovery.  The main
 *              difference here is that we don't allow for the possibility
 *              of retries here, and we are a lot more restrictive about what
 *              we consider acceptable.
 */
STATIC int scsi_eh_completed_normally(Scsi_Cmnd * SCpnt)
{
	/*
	 * First check the host byte, to see if there is anything in there
	 * that would indicate what we need to do.
	 */
	if (host_byte(SCpnt->result) == DID_RESET) {
		if (SCpnt->flags & IS_RESETTING) {
			/*
			 * OK, this is normal.  We don't know whether in fact the
			 * command in question really needs to be rerun or not - 
			 * if this was the original data command then the answer is yes,
			 * otherwise we just flag it as success.
			 */
			SCpnt->flags &= ~IS_RESETTING;
			return NEEDS_RETRY;
		}
		/*
		 * Rats.  We are already in the error handler, so we now get to try
		 * and figure out what to do next.  If the sense is valid, we have
		 * a pretty good idea of what to do.  If not, we mark it as failed.
		 */
		return scsi_check_sense(SCpnt);
	}
	if (host_byte(SCpnt->result) != DID_OK) {
		return FAILED;
	}
	/*
	 * Next, check the message byte.
	 */
	if (msg_byte(SCpnt->result) != COMMAND_COMPLETE) {
		return FAILED;
	}
	/*
	 * Now, check the status byte to see if this indicates anything special.
	 */
	switch (status_byte(SCpnt->result)) {
	case GOOD:
	case COMMAND_TERMINATED:
		return SUCCESS;
	case CHECK_CONDITION:
		return scsi_check_sense(SCpnt);
	case CONDITION_GOOD:
	case INTERMEDIATE_GOOD:
	case INTERMEDIATE_C_GOOD:
		/*
		 * Who knows?  FIXME(eric)
		 */
		return SUCCESS;
	case BUSY:
	case QUEUE_FULL:
	case RESERVATION_CONFLICT:
	default:
		return FAILED;
	}
	return FAILED;
}

/*
 * Function:  scsi_check_sense
 *
 * Purpose:     Examine sense information - give suggestion as to what
 *              we should do with it.
 */
STATIC int scsi_check_sense(Scsi_Cmnd * SCpnt)
{
	if (!scsi_sense_valid(SCpnt)) {
		return FAILED;
	}
	if (SCpnt->sense_buffer[2] & 0xe0)
		return SUCCESS;

	switch (SCpnt->sense_buffer[2] & 0xf) {
	case NO_SENSE:
		return SUCCESS;
	case RECOVERED_ERROR:
		return /* SOFT_ERROR */ SUCCESS;

	case ABORTED_COMMAND:
		return NEEDS_RETRY;
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
			return NEEDS_RETRY;
		}
		/*
		 * If the device is in the process of becoming ready, we 
		 * should retry.
		 */
		if ((SCpnt->sense_buffer[12] == 0x04) &&
			(SCpnt->sense_buffer[13] == 0x01)) {
			return NEEDS_RETRY;
		}
		return SUCCESS;

		/* these three are not supported */
	case COPY_ABORTED:
	case VOLUME_OVERFLOW:
	case MISCOMPARE:
		return SUCCESS;

	case MEDIUM_ERROR:
		return NEEDS_RETRY;

	case ILLEGAL_REQUEST:
	case BLANK_CHECK:
	case DATA_PROTECT:
	case HARDWARE_ERROR:
	default:
		return SUCCESS;
	}
}


/*
 * Function:  scsi_restart_operations
 *
 * Purpose:     Restart IO operations to the specified host.
 *
 * Arguments:   host  - host that we are restarting
 *
 * Lock status: Assumed that locks are not held upon entry.
 *
 * Returns:     Nothing
 *
 * Notes:       When we entered the error handler, we blocked all further
 *              I/O to this device.  We need to 'reverse' this process.
 */
STATIC void scsi_restart_operations(struct Scsi_Host *host)
{
	Scsi_Device *SDpnt;
	unsigned long flags;

	ASSERT_LOCK(&io_request_lock, 0);

	/*
	 * Next free up anything directly waiting upon the host.  This will be
	 * requests for character device operations, and also for ioctls to queued
	 * block devices.
	 */
	SCSI_LOG_ERROR_RECOVERY(5, printk("scsi_error.c: Waking up host to restart\n"));

	wake_up(&host->host_wait);

	/*
	 * Finally we need to re-initiate requests that may be pending.  We will
	 * have had everything blocked while error handling is taking place, and
	 * now that error recovery is done, we will need to ensure that these
	 * requests are started.
	 */
	spin_lock_irqsave(&io_request_lock, flags);
	for (SDpnt = host->host_queue; SDpnt; SDpnt = SDpnt->next) {
		request_queue_t *q;
		if ((host->can_queue > 0 && (host->host_busy >= host->can_queue))
		    || (host->host_blocked)
		    || (host->host_self_blocked)
		    || (SDpnt->device_blocked)) {
			break;
		}
		q = &SDpnt->request_queue;
		q->request_fn(q);
	}
	spin_unlock_irqrestore(&io_request_lock, flags);
}

/*
 * Function:  scsi_unjam_host
 *
 * Purpose:     Attempt to fix a host which has a command that failed for
 *              some reason.
 *
 * Arguments:   host    - host that needs unjamming.
 * 
 * Returns:     Nothing
 *
 * Notes:       When we come in here, we *know* that all commands on the
 *              bus have either completed, failed or timed out.  We also
 *              know that no further commands are being sent to the host,
 *              so things are relatively quiet and we have freedom to
 *              fiddle with things as we wish.
 *
 * Additional note:  This is only the *default* implementation.  It is possible
 *              for individual drivers to supply their own version of this
 *              function, and if the maintainer wishes to do this, it is
 *              strongly suggested that this function be taken as a template
 *              and modified.  This function was designed to correctly handle
 *              problems for about 95% of the different cases out there, and
 *              it should always provide at least a reasonable amount of error
 *              recovery.
 *
 * Note3:       Any command marked 'FAILED' or 'TIMEOUT' must eventually
 *              have scsi_finish_command() called for it.  We do all of
 *              the retry stuff here, so when we restart the host after we
 *              return it should have an empty queue.
 */
STATIC int scsi_unjam_host(struct Scsi_Host *host)
{
	int devices_failed;
	int numfailed;
	int ourrtn;
	int rtn = FALSE;
	int result;
	Scsi_Cmnd *SCloop;
	Scsi_Cmnd *SCpnt;
	Scsi_Device *SDpnt;
	Scsi_Device *SDloop;
	Scsi_Cmnd *SCdone;
	int timed_out;

	ASSERT_LOCK(&io_request_lock, 0);

	SCdone = NULL;

	/*
	 * First, protect against any sort of race condition.  If any of the outstanding
	 * commands are in states that indicate that we are not yet blocked (i.e. we are
	 * not in a quiet state) then we got woken up in error.  If we ever end up here,
	 * we need to re-examine some of the assumptions.
	 */
	for (SDpnt = host->host_queue; SDpnt; SDpnt = SDpnt->next) {
		for (SCpnt = SDpnt->device_queue; SCpnt; SCpnt = SCpnt->next) {
			if (SCpnt->state == SCSI_STATE_FAILED
			    || SCpnt->state == SCSI_STATE_TIMEOUT
			    || SCpnt->state == SCSI_STATE_INITIALIZING
			    || SCpnt->state == SCSI_STATE_UNUSED) {
				continue;
			}
			/*
			 * Rats.  Something is still floating around out there.  This could
			 * be the result of the fact that the upper level drivers are still frobbing
			 * commands that might have succeeded.  There are two outcomes.  One is that
			 * the command block will eventually be freed, and the other one is that
			 * the command will be queued and will be finished along the way.
			 */
			SCSI_LOG_ERROR_RECOVERY(1, printk("Error handler prematurely woken - commands still active (%p %x %d)\n", SCpnt, SCpnt->state, SCpnt->target));

/*
 *        panic("SCSI Error handler woken too early\n");
 *
 * This is no longer a problem, since now the code cares only about
 * SCSI_STATE_TIMEOUT and SCSI_STATE_FAILED.
 * Other states are useful only to release active commands when devices are
 * set offline. If (host->host_active == host->host_busy) we can safely assume
 * that there are no commands in state other then TIMEOUT od FAILED. (DB)
 *
 * FIXME:
 * It is not easy to release correctly commands according to their state when 
 * devices are set offline, when the state is neither TIMEOUT nor FAILED.
 * When a device is set offline, we can have some command with
 * rq_status=RQ_SCSY_BUSY, owner=SCSI_STATE_HIGHLEVEL, 
 * state=SCSI_STATE_INITIALIZING and the driver module cannot be released.
 * (DB, 17 May 1998)
 */
		}
	}

	/*
	 * Next, see if we need to request sense information.  if so,
	 * then get it now, so we have a better idea of what to do.
	 * FIXME(eric) this has the unfortunate side effect that if a host
	 * adapter does not automatically request sense information, that we end
	 * up shutting it down before we request it.  All hosts should be doing this
	 * anyways, so for now all I have to say is tough noogies if you end up in here.
	 * On second thought, this is probably a good idea.  We *really* want to give
	 * authors an incentive to automatically request this.
	 */
	SCSI_LOG_ERROR_RECOVERY(3, printk("scsi_unjam_host: Checking to see if we need to request sense\n"));

	for (SDpnt = host->host_queue; SDpnt; SDpnt = SDpnt->next) {
		for (SCpnt = SDpnt->device_queue; SCpnt; SCpnt = SCpnt->next) {
			if (SCpnt->state != SCSI_STATE_FAILED || scsi_sense_valid(SCpnt)) {
				continue;
			}
			SCSI_LOG_ERROR_RECOVERY(2, printk("scsi_unjam_host: Requesting sense for %d\n",
							  SCpnt->target));
			rtn = scsi_request_sense(SCpnt);
			if (rtn != SUCCESS) {
				continue;
			}
			SCSI_LOG_ERROR_RECOVERY(3, printk("Sense requested for %p - result %x\n",
						  SCpnt, SCpnt->result));
			SCSI_LOG_ERROR_RECOVERY(3, print_sense("bh", SCpnt));

			result = scsi_decide_disposition(SCpnt);

			/*
			 * If the result was normal, then just pass it along to the
			 * upper level.
			 */
			if (result == SUCCESS) {
				SCpnt->host->host_failed--;
				scsi_eh_finish_command(&SCdone, SCpnt);
			}
			if (result != NEEDS_RETRY) {
				continue;
			}
			/* 
			 * We only come in here if we want to retry a
			 * command.  The test to see whether the command
			 * should be retried should be keeping track of the
			 * number of tries, so we don't end up looping, of
			 * course.  
			 */
			SCpnt->state = NEEDS_RETRY;
			rtn = scsi_eh_retry_command(SCpnt);
			if (rtn != SUCCESS) {
				continue;
			}
			/*
			 * We eventually hand this one back to the top level.
			 */
			SCpnt->host->host_failed--;
			scsi_eh_finish_command(&SCdone, SCpnt);
		}
	}

	/*
	 * Go through the list of commands and figure out where we stand and how bad things
	 * really are.
	 */
	numfailed = 0;
	timed_out = 0;
	devices_failed = 0;
	for (SDpnt = host->host_queue; SDpnt; SDpnt = SDpnt->next) {
		unsigned int device_error = 0;

		for (SCpnt = SDpnt->device_queue; SCpnt; SCpnt = SCpnt->next) {
			if (SCpnt->state == SCSI_STATE_FAILED) {
				SCSI_LOG_ERROR_RECOVERY(5, printk("Command to ID %d failed\n",
							 SCpnt->target));
				numfailed++;
				device_error++;
			}
			if (SCpnt->state == SCSI_STATE_TIMEOUT) {
				SCSI_LOG_ERROR_RECOVERY(5, printk("Command to ID %d timedout\n",
							 SCpnt->target));
				timed_out++;
				device_error++;
			}
		}
		if (device_error > 0) {
			devices_failed++;
		}
	}

	SCSI_LOG_ERROR_RECOVERY(2, printk("Total of %d+%d commands on %d devices require eh work\n",
				  numfailed, timed_out, devices_failed));

	if (host->host_failed == 0) {
		ourrtn = TRUE;
		goto leave;
	}
	/*
	 * Next, try and see whether or not it makes sense to try and abort
	 * the running command.  This only works out to be the case if we have
	 * one command that has timed out.  If the command simply failed, it
	 * makes no sense to try and abort the command, since as far as the
	 * host adapter is concerned, it isn't running.
	 */

	SCSI_LOG_ERROR_RECOVERY(3, printk("scsi_unjam_host: Checking to see if we want to try abort\n"));

	for (SDpnt = host->host_queue; SDpnt; SDpnt = SDpnt->next) {
		for (SCloop = SDpnt->device_queue; SCloop; SCloop = SCloop->next) {
			if (SCloop->state != SCSI_STATE_TIMEOUT) {
				continue;
			}
			rtn = scsi_try_to_abort_command(SCloop, ABORT_TIMEOUT);
			if (rtn == SUCCESS) {
				rtn = scsi_test_unit_ready(SCloop);

				if (rtn == SUCCESS && scsi_unit_is_ready(SCloop)) {
					rtn = scsi_eh_retry_command(SCloop);

					if (rtn == SUCCESS) {
						SCloop->host->host_failed--;
						scsi_eh_finish_command(&SCdone, SCloop);
					}
				}
			}
		}
	}

	/*
	 * If we have corrected all of the problems, then we are done.
	 */
	if (host->host_failed == 0) {
		ourrtn = TRUE;
		goto leave;
	}
	/*
	 * Either the abort wasn't appropriate, or it didn't succeed.
	 * Now try a bus device reset.  Still, look to see whether we have
	 * multiple devices that are jammed or not - if we have multiple devices,
	 * it makes no sense to try BUS_DEVICE_RESET - we really would need
	 * to try a BUS_RESET instead.
	 *
	 * Does this make sense - should we try BDR on each device individually?
	 * Yes, definitely.
	 */
	SCSI_LOG_ERROR_RECOVERY(3, printk("scsi_unjam_host: Checking to see if we want to try BDR\n"));

	for (SDpnt = host->host_queue; SDpnt; SDpnt = SDpnt->next) {
		for (SCloop = SDpnt->device_queue; SCloop; SCloop = SCloop->next) {
			if (SCloop->state == SCSI_STATE_FAILED
			    || SCloop->state == SCSI_STATE_TIMEOUT) {
				break;
			}
		}

		if (SCloop == NULL) {
			continue;
		}
		/*
		 * OK, we have a device that is having problems.  Try and send
		 * a bus device reset to it.
		 *
		 * FIXME(eric) - make sure we handle the case where multiple
		 * commands to the same device have failed. They all must
		 * get properly restarted.
		 */
		rtn = scsi_try_bus_device_reset(SCloop, RESET_TIMEOUT);

		if (rtn == SUCCESS) {
			rtn = scsi_test_unit_ready(SCloop);

			if (rtn == SUCCESS && scsi_unit_is_ready(SCloop)) {
				rtn = scsi_eh_retry_command(SCloop);

				if (rtn == SUCCESS) {
					SCloop->host->host_failed--;
					scsi_eh_finish_command(&SCdone, SCloop);
				}
			}
		}
	}

	if (host->host_failed == 0) {
		ourrtn = TRUE;
		goto leave;
	}
	/*
	 * If we ended up here, we have serious problems.  The only thing left
	 * to try is a full bus reset.  If someone has grabbed the bus and isn't
	 * letting go, then perhaps this will help.
	 */
	SCSI_LOG_ERROR_RECOVERY(3, printk("scsi_unjam_host: Try hard bus reset\n"));

	/* 
	 * We really want to loop over the various channels, and do this on
	 * a channel by channel basis.  We should also check to see if any
	 * of the failed commands are on soft_reset devices, and if so, skip
	 * the reset.  
	 */
	for (SDpnt = host->host_queue; SDpnt; SDpnt = SDpnt->next) {
	      next_device:
		for (SCpnt = SDpnt->device_queue; SCpnt; SCpnt = SCpnt->next) {
			if (SCpnt->state != SCSI_STATE_FAILED
			    && SCpnt->state != SCSI_STATE_TIMEOUT) {
				continue;
			}
			/*
			 * We have a failed command.  Make sure there are no other failed
			 * commands on the same channel that are timed out and implement a
			 * soft reset.
			 */
			for (SDloop = host->host_queue; SDloop; SDloop = SDloop->next) {
				for (SCloop = SDloop->device_queue; SCloop; SCloop = SCloop->next) {
					if (SCloop->channel != SCpnt->channel) {
						continue;
					}
					if (SCloop->state != SCSI_STATE_FAILED
					    && SCloop->state != SCSI_STATE_TIMEOUT) {
						continue;
					}
					if (SDloop->soft_reset && SCloop->state == SCSI_STATE_TIMEOUT) {
						/* 
						 * If this device uses the soft reset option, and this
						 * is one of the devices acting up, then our only
						 * option is to wait a bit, since the command is
						 * supposedly still running.  
						 *
						 * FIXME(eric) - right now we will just end up falling
						 * through to the 'take device offline' case.
						 *
						 * FIXME(eric) - It is possible that the command completed
						 * *after* the error recovery procedure started, and if this
						 * is the case, we are worrying about nothing here.
						 */

						scsi_sleep(1 * HZ);
						goto next_device;
					}
				}
			}

			/*
			 * We now know that we are able to perform a reset for the
			 * bus that SCpnt points to.  There are no soft-reset devices
			 * with outstanding timed out commands.
			 */
			rtn = scsi_try_bus_reset(SCpnt);
			if (rtn == SUCCESS) {
				for (SDloop = host->host_queue; SDloop; SDloop = SDloop->next) {
					for (SCloop = SDloop->device_queue; SCloop; SCloop = SCloop->next) {
						if (SCloop->channel != SCpnt->channel) {
							continue;
						}
						if (SCloop->state != SCSI_STATE_FAILED
						    && SCloop->state != SCSI_STATE_TIMEOUT) {
							continue;
						}
						rtn = scsi_test_unit_ready(SCloop);

						if (rtn == SUCCESS && scsi_unit_is_ready(SCloop)) {
							rtn = scsi_eh_retry_command(SCloop);

							if (rtn == SUCCESS) {
								SCpnt->host->host_failed--;
								scsi_eh_finish_command(&SCdone, SCloop);
							}
						}
						/*
						 * If the bus reset worked, but we are still unable to
						 * talk to the device, take it offline.
						 * FIXME(eric) - is this really the correct thing to do?
						 */
						if (rtn != SUCCESS) {
							printk(KERN_INFO "scsi: device set offline - not ready or command retry failed after bus reset: host %d channel %d id %d lun %d\n", SDloop->host->host_no, SDloop->channel, SDloop->id, SDloop->lun);

							SDloop->online = FALSE;
							SDloop->host->host_failed--;
							scsi_eh_finish_command(&SCdone, SCloop);
						}
					}
				}
			}
		}
	}

	if (host->host_failed == 0) {
		ourrtn = TRUE;
		goto leave;
	}
	/*
	 * If we ended up here, we have serious problems.  The only thing left
	 * to try is a full host reset - perhaps the firmware on the device
	 * crashed, or something like that.
	 *
	 * It is assumed that a succesful host reset will cause *all* information
	 * about the command to be flushed from both the host adapter *and* the
	 * device.
	 *
	 * FIXME(eric) - it isn't clear that devices that implement the soft reset
	 * option can ever be cleared except via cycling the power.  The problem is
	 * that sending the host reset command will cause the host to forget
	 * about the pending command, but the device won't forget.  For now, we
	 * skip the host reset option if any of the failed devices are configured
	 * to use the soft reset option.
	 */
	for (SDpnt = host->host_queue; SDpnt; SDpnt = SDpnt->next) {
	      next_device2:
		for (SCpnt = SDpnt->device_queue; SCpnt; SCpnt = SCpnt->next) {
			if (SCpnt->state != SCSI_STATE_FAILED
			    && SCpnt->state != SCSI_STATE_TIMEOUT) {
				continue;
			}
			if (SDpnt->soft_reset && SCpnt->state == SCSI_STATE_TIMEOUT) {
				/* 
				 * If this device uses the soft reset option, and this
				 * is one of the devices acting up, then our only
				 * option is to wait a bit, since the command is
				 * supposedly still running.  
				 *
				 * FIXME(eric) - right now we will just end up falling
				 * through to the 'take device offline' case.
				 */
				SCSI_LOG_ERROR_RECOVERY(3,
							printk("scsi_unjam_host: Unable to try hard host reset\n"));

				/*
				 * Due to the spinlock, we will never get out of this
				 * loop without a proper wait. (DB)
				 */
				scsi_sleep(1 * HZ);

				goto next_device2;
			}
			SCSI_LOG_ERROR_RECOVERY(3, printk("scsi_unjam_host: Try hard host reset\n"));

			/*
			 * FIXME(eric) - we need to obtain a valid SCpnt to perform this call.
			 */
			rtn = scsi_try_host_reset(SCpnt);
			if (rtn == SUCCESS) {
				/*
				 * FIXME(eric) we assume that all commands are flushed from the
				 * controller.  We should get a DID_RESET for all of the commands
				 * that were pending.  We should ignore these so that we can
				 * guarantee that we are in a consistent state.
				 *
				 * I believe this to be the case right now, but this needs to be
				 * tested.
				 */
				for (SDloop = host->host_queue; SDloop; SDloop = SDloop->next) {
					for (SCloop = SDloop->device_queue; SCloop; SCloop = SCloop->next) {
						if (SCloop->state != SCSI_STATE_FAILED
						    && SCloop->state != SCSI_STATE_TIMEOUT) {
							continue;
						}
						rtn = scsi_test_unit_ready(SCloop);

						if (rtn == SUCCESS && scsi_unit_is_ready(SCloop)) {
							rtn = scsi_eh_retry_command(SCloop);

							if (rtn == SUCCESS) {
								SCpnt->host->host_failed--;
								scsi_eh_finish_command(&SCdone, SCloop);
							}
						}
						if (rtn != SUCCESS) {
							printk(KERN_INFO "scsi: device set offline - not ready or command retry failed after host reset: host %d channel %d id %d lun %d\n", SDloop->host->host_no, SDloop->channel, SDloop->id, SDloop->lun);
							SDloop->online = FALSE;
							SDloop->host->host_failed--;
							scsi_eh_finish_command(&SCdone, SCloop);
						}
					}
				}
			}
		}
	}

	/*
	 * If we solved all of the problems, then let's rev up the engines again.
	 */
	if (host->host_failed == 0) {
		ourrtn = TRUE;
		goto leave;
	}
	/*
	 * If the HOST RESET failed, then for now we assume that the entire host
	 * adapter is too hosed to be of any use.  For our purposes, however, it is
	 * easier to simply take the devices offline that correspond to commands
	 * that failed.
	 */
	SCSI_LOG_ERROR_RECOVERY(1, printk("scsi_unjam_host: Take device offline\n"));

	for (SDpnt = host->host_queue; SDpnt; SDpnt = SDpnt->next) {
		for (SCloop = SDpnt->device_queue; SCloop; SCloop = SCloop->next) {
			if (SCloop->state == SCSI_STATE_FAILED || SCloop->state == SCSI_STATE_TIMEOUT) {
				SDloop = SCloop->device;
				if (SDloop->online == TRUE) {
					printk(KERN_INFO "scsi: device set offline - command error recover failed: host %d channel %d id %d lun %d\n", SDloop->host->host_no, SDloop->channel, SDloop->id, SDloop->lun);
					SDloop->online = FALSE;
				}

				/*
				 * This should pass the failure up to the top level driver, and
				 * it will have to try and do something intelligent with it.
				 */
				SCloop->host->host_failed--;

				if (SCloop->state == SCSI_STATE_TIMEOUT) {
					SCloop->result |= (DRIVER_TIMEOUT << 24);
				}
				SCSI_LOG_ERROR_RECOVERY(3, printk("Finishing command for device %d %x\n",
				    SDloop->id, SCloop->result));

				scsi_eh_finish_command(&SCdone, SCloop);
			}
		}
	}

	if (host->host_failed != 0) {
		panic("scsi_unjam_host: Miscount of number of failed commands.\n");
	}
	SCSI_LOG_ERROR_RECOVERY(3, printk("scsi_unjam_host: Returning\n"));

	ourrtn = FALSE;

      leave:

	/*
	 * We should have a list of commands that we 'finished' during the course of
	 * error recovery.  This should be the same as the list of commands that timed out
	 * or failed.  We are currently holding these things in a linked list - we didn't
	 * put them in the bottom half queue because we wanted to keep things quiet while
	 * we were working on recovery, and passing them up to the top level could easily
	 * cause the top level to try and queue something else again.
	 *
	 * Start by marking that the host is no longer in error recovery.
	 */
	host->in_recovery = 0;

	/*
	 * Take the list of commands, and stick them in the bottom half queue.
	 * The current implementation of scsi_done will do this for us - if need
	 * be we can create a special version of this function to do the
	 * same job for us.
	 */
	for (SCpnt = SCdone; SCpnt != NULL; SCpnt = SCdone) {
		SCdone = SCpnt->bh_next;
		SCpnt->bh_next = NULL;
                /*
                 * Oh, this is a vile hack.  scsi_done() expects a timer
                 * to be running on the command.  If there isn't, it assumes
                 * that the command has actually timed out, and a timer
                 * handler is running.  That may well be how we got into
                 * this fix, but right now things are stable.  We add
                 * a timer back again so that we can report completion.
                 * scsi_done() will immediately remove said timer from
                 * the command, and then process it.
                 */
		scsi_add_timer(SCpnt, 100, scsi_eh_times_out);
		scsi_done(SCpnt);
	}

	return (ourrtn);
}


/*
 * Function:  scsi_error_handler
 *
 * Purpose:     Handle errors/timeouts of scsi commands, try and clean up
 *              and unjam the bus, and restart things.
 *
 * Arguments:   host    - host for which we are running.
 *
 * Returns:     Never returns.
 *
 * Notes:       This is always run in the context of a kernel thread.  The
 *              idea is that we start this thing up when the kernel starts
 *              up (one per host that we detect), and it immediately goes to
 *              sleep and waits for some event (i.e. failure).  When this
 *              takes place, we have the job of trying to unjam the bus
 *              and restarting things.
 *
 */
void scsi_error_handler(void *data)
{
	struct Scsi_Host *host = (struct Scsi_Host *) data;
	int rtn;
	DECLARE_MUTEX_LOCKED(sem);

        /*
         * We only listen to signals if the HA was loaded as a module.
         * If the HA was compiled into the kernel, then we don't listen
         * to any signals.
         */
        if( host->loaded_as_module ) {
	siginitsetinv(&current->blocked, SHUTDOWN_SIGS);
	} else {
	siginitsetinv(&current->blocked, 0);
        }

	lock_kernel();

	/*
	 *    Flush resources
	 */

	daemonize();
	reparent_to_init();

	/*
	 * Set the name of this process.
	 */

	sprintf(current->comm, "scsi_eh_%d", host->host_no);

	host->eh_wait = &sem;
	host->ehandler = current;

	unlock_kernel();

	/*
	 * Wake up the thread that created us.
	 */
	SCSI_LOG_ERROR_RECOVERY(3, printk("Wake up parent %d\n", sem_getcount(host->eh_notify)));

	up(host->eh_notify);

	while (1) {
		/*
		 * If we get a signal, it means we are supposed to go
		 * away and die.  This typically happens if the user is
		 * trying to unload a module.
		 */
		SCSI_LOG_ERROR_RECOVERY(1, printk("Error handler sleeping\n"));

		/*
		 * Note - we always use down_interruptible with the semaphore
		 * even if the module was loaded as part of the kernel.  The
		 * reason is that down() will cause this thread to be counted
		 * in the load average as a running process, and down
		 * interruptible doesn't.  Given that we need to allow this
		 * thread to die if the driver was loaded as a module, using
		 * semaphores isn't unreasonable.
		 */
		down_interruptible(&sem);
		if( host->loaded_as_module ) {
			if (signal_pending(current))
				break;
                }

		SCSI_LOG_ERROR_RECOVERY(1, printk("Error handler waking up\n"));

		host->eh_active = 1;

		/*
		 * We have a host that is failing for some reason.  Figure out
		 * what we need to do to get it up and online again (if we can).
		 * If we fail, we end up taking the thing offline.
		 */
		if (host->hostt->eh_strategy_handler != NULL) {
			rtn = host->hostt->eh_strategy_handler(host);
		} else {
			rtn = scsi_unjam_host(host);
		}

		host->eh_active = 0;

		/*
		 * Note - if the above fails completely, the action is to take
		 * individual devices offline and flush the queue of any
		 * outstanding requests that may have been pending.  When we
		 * restart, we restart any I/O to any other devices on the bus
		 * which are still online.
		 */
		scsi_restart_operations(host);

	}

	SCSI_LOG_ERROR_RECOVERY(1, printk("Error handler exiting\n"));

	/*
	 * Make sure that nobody tries to wake us up again.
	 */
	host->eh_wait = NULL;

	/*
	 * Knock this down too.  From this point on, the host is flying
	 * without a pilot.  If this is because the module is being unloaded,
	 * that's fine.  If the user sent a signal to this thing, we are
	 * potentially in real danger.
	 */
	host->in_recovery = 0;
	host->eh_active = 0;
	host->ehandler = NULL;

	/*
	 * If anyone is waiting for us to exit (i.e. someone trying to unload
	 * a driver), then wake up that process to let them know we are on
	 * the way out the door.  This may be overkill - I *think* that we
	 * could probably just unload the driver and send the signal, and when
	 * the error handling thread wakes up that it would just exit without
	 * needing to touch any memory associated with the driver itself.
	 */
	if (host->eh_notify != NULL)
		up(host->eh_notify);
}

/*
 * Function:	scsi_new_reset
 *
 * Purpose:	Send requested reset to a bus or device at any phase.
 *
 * Arguments:	SCpnt	- command ptr to send reset with (usually a dummy)
 *		flag - reset type (see scsi.h)
 *
 * Returns:	SUCCESS/FAILURE.
 *
 * Notes:	This is used by the SCSI Generic driver to provide
 *		Bus/Device reset capability.
 */
int
scsi_new_reset(Scsi_Cmnd *SCpnt, int flag)
{
	int rtn;

	switch(flag) {
	case SCSI_TRY_RESET_DEVICE:
		rtn = scsi_try_bus_device_reset(SCpnt, 0);
		if (rtn == SUCCESS)
			break;
		/* FALLTHROUGH */
	case SCSI_TRY_RESET_BUS:
		rtn = scsi_try_bus_reset(SCpnt);
		if (rtn == SUCCESS)
			break;
		/* FALLTHROUGH */
	case SCSI_TRY_RESET_HOST:
		rtn = scsi_try_host_reset(SCpnt);
		break;
	default:
		rtn = FAILED;
	}

	return rtn;
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
