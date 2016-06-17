/*
 *  scsi_queue.c Copyright (C) 1997 Eric Youngdale
 *
 *  generic mid-level SCSI queueing.
 *
 *  The point of this is that we need to track when hosts are unable to
 *  accept a command because they are busy.  In addition, we track devices
 *  that cannot accept a command because of a QUEUE_FULL condition.  In both
 *  of these cases, we enter the command in the queue.  At some later point,
 *  we attempt to remove commands from the queue and retry them.
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
 * TODO:
 *      1) Prevent multiple traversals of list to look for commands to
 *         queue.
 *      2) Protect against multiple insertions of list at the same time.
 * DONE:
 *      1) Set state of scsi command to a new state value for ml queue.
 *      2) Insert into queue when host rejects command.
 *      3) Make sure status code is properly passed from low-level queue func
 *         so that internal_cmnd properly returns the right value.
 *      4) Insert into queue when QUEUE_FULL.
 *      5) Cull queue in bottom half handler.
 *      6) Check usage count prior to queue insertion.  Requeue if usage
 *         count is 0.
 *      7) Don't send down any more commands if the host/device is busy.
 */

static const char RCSid[] = "$Header: /mnt/ide/home/eric/CVSROOT/linux/drivers/scsi/scsi_queue.c,v 1.1 1997/10/21 11:16:38 eric Exp $";


/*
 * Function:    scsi_mlqueue_insert()
 *
 * Purpose:     Insert a command in the midlevel queue.
 *
 * Arguments:   cmd    - command that we are adding to queue.
 *              reason - why we are inserting command to queue.
 *
 * Lock status: Assumed that lock is not held upon entry.
 *
 * Returns:     Nothing.
 *
 * Notes:       We do this for one of two cases.  Either the host is busy
 *              and it cannot accept any more commands for the time being,
 *              or the device returned QUEUE_FULL and can accept no more
 *              commands.
 * Notes:       This could be called either from an interrupt context or a
 *              normal process context.
 */
int scsi_mlqueue_insert(Scsi_Cmnd * cmd, int reason)
{
	struct Scsi_Host *host;
	unsigned long flags;

	SCSI_LOG_MLQUEUE(1, printk("Inserting command %p into mlqueue\n", cmd));

	/*
	 * We are inserting the command into the ml queue.  First, we
	 * cancel the timer, so it doesn't time out.
	 */
	scsi_delete_timer(cmd);

	host = cmd->host;

	/*
	 * Next, set the appropriate busy bit for the device/host.
	 */
	if (reason == SCSI_MLQUEUE_HOST_BUSY) {
		/*
		 * Protect against race conditions.  If the host isn't busy,
		 * assume that something actually completed, and that we should
		 * be able to queue a command now.  Note that there is an implicit
		 * assumption that every host can always queue at least one command.
		 * If a host is inactive and cannot queue any commands, I don't see
		 * how things could possibly work anyways.
		 */
		if (host->host_busy == 0) {
			if (scsi_retry_command(cmd) == 0) {
				return 0;
			}
		}
		host->host_blocked = TRUE;
	} else {
		/*
		 * Protect against race conditions.  If the device isn't busy,
		 * assume that something actually completed, and that we should
		 * be able to queue a command now.  Note that there is an implicit
		 * assumption that every host can always queue at least one command.
		 * If a host is inactive and cannot queue any commands, I don't see
		 * how things could possibly work anyways.
		 */
		if (cmd->device->device_busy == 0) {
			if (scsi_retry_command(cmd) == 0) {
				return 0;
			}
		}
		cmd->device->device_blocked = TRUE;
	}

	/*
	 * Register the fact that we own the thing for now.
	 */
	cmd->state = SCSI_STATE_MLQUEUE;
	cmd->owner = SCSI_OWNER_MIDLEVEL;
	cmd->bh_next = NULL;

	/*
	 * Decrement the counters, since these commands are no longer
	 * active on the host/device.
	 */
	spin_lock_irqsave(&io_request_lock, flags);
	cmd->host->host_busy--;
	cmd->device->device_busy--;
	spin_unlock_irqrestore(&io_request_lock, flags);

	/*
	 * Insert this command at the head of the queue for it's device.
	 * It will go before all other commands that are already in the queue.
	 */
	scsi_insert_special_cmd(cmd, 1);
	return 0;
}
