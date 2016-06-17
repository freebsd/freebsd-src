/*
 *  scsi.c Copyright (C) 1992 Drew Eckhardt
 *         Copyright (C) 1993, 1994, 1995, 1999 Eric Youngdale
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
 *  Modified by Eric Youngdale eric@andante.org or ericy@gnu.ai.mit.edu to
 *  add scatter-gather, multiple outstanding request, and other
 *  enhancements.
 *
 *  Native multichannel, wide scsi, /proc/scsi and hot plugging
 *  support added by Michael Neuffer <mike@i-connect.net>
 *
 *  Added request_module("scsi_hostadapter") for kerneld:
 *  (Put an "alias scsi_hostadapter your_hostadapter" in /etc/modules.conf)
 *  Bjorn Ekwall  <bj0rn@blox.se>
 *  (changed to kmod)
 *
 *  Major improvements to the timeout, abort, and reset processing,
 *  as well as performance modifications for large queue depths by
 *  Leonard N. Zubkoff <lnz@dandelion.com>
 *
 *  Converted cli() code to spinlocks, Ingo Molnar
 *
 *  Jiffies wrap fixes (host->resetting), 3 Dec 1998 Andrea Arcangeli
 *
 *  out_of_space hacks, D. Gilbert (dpg) 990608
 */

#define REVISION	"Revision: 1.00"
#define VERSION		"Id: scsi.c 1.00 2000/09/26"

#include <linux/config.h>
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
#include <linux/init.h>
#include <linux/smp_lock.h>
#include <linux/completion.h>

#define __KERNEL_SYSCALLS__

#include <linux/unistd.h>
#include <linux/spinlock.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/dma.h>
#include <asm/uaccess.h>

#include "scsi.h"
#include "hosts.h"
#include "constants.h"

#ifdef CONFIG_KMOD
#include <linux/kmod.h>
#endif

#undef USE_STATIC_SCSI_MEMORY

struct proc_dir_entry *proc_scsi;

#ifdef CONFIG_PROC_FS
static int scsi_proc_info(char *buffer, char **start, off_t offset, int length);
static void scsi_dump_status(int level);
#endif

/*
   static const char RCSid[] = "$Header: /vger/u4/cvs/linux/drivers/scsi/scsi.c,v 1.38 1997/01/19 23:07:18 davem Exp $";
 */

/*
 * Definitions and constants.
 */

#define MIN_RESET_DELAY (2*HZ)

/* Do not call reset on error if we just did a reset within 15 sec. */
#define MIN_RESET_PERIOD (15*HZ)

/*
 * Macro to determine the size of SCSI command. This macro takes vendor
 * unique commands into account. SCSI commands in groups 6 and 7 are
 * vendor unique and we will depend upon the command length being
 * supplied correctly in cmd_len.
 */
#define CDB_SIZE(SCpnt)	((((SCpnt->cmnd[0] >> 5) & 7) < 6) ? \
				COMMAND_SIZE(SCpnt->cmnd[0]) : SCpnt->cmd_len)

/*
 * Data declarations.
 */
unsigned long scsi_pid;
Scsi_Cmnd *last_cmnd;
/* Command group 3 is reserved and should never be used.  */
const unsigned char scsi_command_size[8] =
{
	6, 10, 10, 12,
	16, 12, 10, 10
};
static unsigned long serial_number;
static Scsi_Cmnd *scsi_bh_queue_head;
static Scsi_Cmnd *scsi_bh_queue_tail;

/*
 * Note - the initial logging level can be set here to log events at boot time.
 * After the system is up, you may enable logging via the /proc interface.
 */
unsigned int scsi_logging_level;

const char *const scsi_device_types[MAX_SCSI_DEVICE_CODE] =
{
	"Direct-Access    ",
	"Sequential-Access",
	"Printer          ",
	"Processor        ",
	"WORM             ",
	"CD-ROM           ",
	"Scanner          ",
	"Optical Device   ",
	"Medium Changer   ",
	"Communications   ",
	"Unknown          ",
	"Unknown          ",
	"Unknown          ",
	"Enclosure        ",
};

/* 
 * Function prototypes.
 */
extern void scsi_times_out(Scsi_Cmnd * SCpnt);
void scsi_build_commandblocks(Scsi_Device * SDpnt);

/*
 * These are the interface to the old error handling code.  It should go away
 * someday soon.
 */
extern void scsi_old_done(Scsi_Cmnd * SCpnt);
extern void scsi_old_times_out(Scsi_Cmnd * SCpnt);
extern int scsi_old_reset(Scsi_Cmnd *SCpnt, unsigned int flag);

/* 
 * Private interface into the new error handling code.
 */
extern int scsi_new_reset(Scsi_Cmnd *SCpnt, unsigned int flag);

/*
 * Function:    scsi_initialize_queue()
 *
 * Purpose:     Selects queue handler function for a device.
 *
 * Arguments:   SDpnt   - device for which we need a handler function.
 *
 * Returns:     Nothing
 *
 * Lock status: No locking assumed or required.
 *
 * Notes:       Most devices will end up using scsi_request_fn for the
 *              handler function (at least as things are done now).
 *              The "block" feature basically ensures that only one of
 *              the blocked hosts is active at one time, mainly to work around
 *              buggy DMA chipsets where the memory gets starved.
 *              For this case, we have a special handler function, which
 *              does some checks and ultimately calls scsi_request_fn.
 *
 *              The single_lun feature is a similar special case.
 *
 *              We handle these things by stacking the handlers.  The
 *              special case handlers simply check a few conditions,
 *              and return if they are not supposed to do anything.
 *              In the event that things are OK, then they call the next
 *              handler in the list - ultimately they call scsi_request_fn
 *              to do the dirty deed.
 */
void  scsi_initialize_queue(Scsi_Device * SDpnt, struct Scsi_Host * SHpnt)
{
	request_queue_t *q = &SDpnt->request_queue;

	blk_init_queue(q, scsi_request_fn);
	blk_queue_headactive(q, 0);
	blk_queue_throttle_sectors(q, 1);
	q->queuedata = (void *) SDpnt;
}

#ifdef MODULE
MODULE_PARM(scsi_logging_level, "i");
MODULE_PARM_DESC(scsi_logging_level, "SCSI logging level; should be zero or nonzero");

#else

static int __init scsi_logging_setup(char *str)
{
	int tmp;

	if (get_option(&str, &tmp) == 1) {
		scsi_logging_level = (tmp ? ~0 : 0);
		return 1;
	} else {
		printk(KERN_INFO "scsi_logging_setup : usage scsi_logging_level=n "
		       "(n should be 0 or non-zero)\n");
		return 0;
	}
}

__setup("scsi_logging=", scsi_logging_setup);

#endif

/*
 *	Issue a command and wait for it to complete
 */
 
static void scsi_wait_done(Scsi_Cmnd * SCpnt)
{
	struct request *req;

	req = &SCpnt->request;
	req->rq_status = RQ_SCSI_DONE;	/* Busy, but indicate request done */

	if (req->waiting != NULL) {
		complete(req->waiting);
	}
}

/*
 * This lock protects the freelist for all devices on the system.
 * We could make this finer grained by having a single lock per
 * device if it is ever found that there is excessive contention
 * on this lock.
 */
static spinlock_t device_request_lock = SPIN_LOCK_UNLOCKED;

/*
 * Used to protect insertion into and removal from the queue of
 * commands to be processed by the bottom half handler.
 */
static spinlock_t scsi_bhqueue_lock = SPIN_LOCK_UNLOCKED;

/*
 * Function:    scsi_allocate_request
 *
 * Purpose:     Allocate a request descriptor.
 *
 * Arguments:   device    - device for which we want a request
 *
 * Lock status: No locks assumed to be held.  This function is SMP-safe.
 *
 * Returns:     Pointer to request block.
 *
 * Notes:       With the new queueing code, it becomes important
 *              to track the difference between a command and a
 *              request.  A request is a pending item in the queue that
 *              has not yet reached the top of the queue.
 */

Scsi_Request *scsi_allocate_request(Scsi_Device * device)
{
  	Scsi_Request *SRpnt = NULL;
  
  	if (!device)
  		panic("No device passed to scsi_allocate_request().\n");
  
	SRpnt = (Scsi_Request *) kmalloc(sizeof(Scsi_Request), GFP_ATOMIC);
	if( SRpnt == NULL )
	{
		return NULL;
	}

	memset(SRpnt, 0, sizeof(Scsi_Request));
	SRpnt->sr_device = device;
	SRpnt->sr_host = device->host;
	SRpnt->sr_magic = SCSI_REQ_MAGIC;
	SRpnt->sr_data_direction = SCSI_DATA_UNKNOWN;

	return SRpnt;
}

/*
 * Function:    scsi_release_request
 *
 * Purpose:     Release a request descriptor.
 *
 * Arguments:   device    - device for which we want a request
 *
 * Lock status: No locks assumed to be held.  This function is SMP-safe.
 *
 * Returns:     Pointer to request block.
 *
 * Notes:       With the new queueing code, it becomes important
 *              to track the difference between a command and a
 *              request.  A request is a pending item in the queue that
 *              has not yet reached the top of the queue.  We still need
 *              to free a request when we are done with it, of course.
 */
void scsi_release_request(Scsi_Request * req)
{
	if( req->sr_command != NULL )
	{
		scsi_release_command(req->sr_command);
		req->sr_command = NULL;
	}

	kfree(req);
}

/*
 * Function:    scsi_allocate_device
 *
 * Purpose:     Allocate a command descriptor.
 *
 * Arguments:   device    - device for which we want a command descriptor
 *              wait      - 1 if we should wait in the event that none
 *                          are available.
 *              interruptible - 1 if we should unblock and return NULL
 *                          in the event that we must wait, and a signal
 *                          arrives.
 *
 * Lock status: No locks assumed to be held.  This function is SMP-safe.
 *
 * Returns:     Pointer to command descriptor.
 *
 * Notes:       Prior to the new queue code, this function was not SMP-safe.
 *
 *              If the wait flag is true, and we are waiting for a free
 *              command block, this function will interrupt and return
 *              NULL in the event that a signal arrives that needs to
 *              be handled.
 *
 *              This function is deprecated, and drivers should be
 *              rewritten to use Scsi_Request instead of Scsi_Cmnd.
 */

Scsi_Cmnd *scsi_allocate_device(Scsi_Device * device, int wait, 
                                int interruptable)
{
 	struct Scsi_Host *host;
  	Scsi_Cmnd *SCpnt = NULL;
	Scsi_Device *SDpnt;
	unsigned long flags;
  
  	if (!device)
  		panic("No device passed to scsi_allocate_device().\n");
  
  	host = device->host;
  
	spin_lock_irqsave(&device_request_lock, flags);
 
	while (1 == 1) {
		SCpnt = NULL;
		if (!device->device_blocked) {
			if (device->single_lun) {
				/*
				 * FIXME(eric) - this is not at all optimal.  Given that
				 * single lun devices are rare and usually slow
				 * (i.e. CD changers), this is good enough for now, but
				 * we may want to come back and optimize this later.
				 *
				 * Scan through all of the devices attached to this
				 * host, and see if any are active or not.  If so,
				 * we need to defer this command.
				 *
				 * We really need a busy counter per device.  This would
				 * allow us to more easily figure out whether we should
				 * do anything here or not.
				 */
				for (SDpnt = host->host_queue;
				     SDpnt;
				     SDpnt = SDpnt->next) {
					/*
					 * Only look for other devices on the same bus
					 * with the same target ID.
					 */
					if (SDpnt->channel != device->channel
					    || SDpnt->id != device->id
					    || SDpnt == device) {
 						continue;
					}
                                        if( atomic_read(&SDpnt->device_active) != 0)
                                        {
                                                break;
                                        }
				}
				if (SDpnt) {
					/*
					 * Some other device in this cluster is busy.
					 * If asked to wait, we need to wait, otherwise
					 * return NULL.
					 */
					SCpnt = NULL;
					goto busy;
				}
			}
			/*
			 * Now we can check for a free command block for this device.
			 */
			for (SCpnt = device->device_queue; SCpnt; SCpnt = SCpnt->next) {
				if (SCpnt->request.rq_status == RQ_INACTIVE)
					break;
			}
		}
		/*
		 * If we couldn't find a free command block, and we have been
		 * asked to wait, then do so.
		 */
		if (SCpnt) {
			break;
		}
      busy:
		/*
		 * If we have been asked to wait for a free block, then
		 * wait here.
		 */
		if (wait) {
                        DECLARE_WAITQUEUE(wait, current);

                        /*
                         * We need to wait for a free commandblock.  We need to
                         * insert ourselves into the list before we release the
                         * lock.  This way if a block were released the same
                         * microsecond that we released the lock, the call
                         * to schedule() wouldn't block (well, it might switch,
                         * but the current task will still be schedulable.
                         */
                        add_wait_queue(&device->scpnt_wait, &wait);
                        if( interruptable ) {
                                set_current_state(TASK_INTERRUPTIBLE);
                        } else {
                                set_current_state(TASK_UNINTERRUPTIBLE);
                        }

                        spin_unlock_irqrestore(&device_request_lock, flags);

			/*
			 * This should block until a device command block
			 * becomes available.
			 */
                        schedule();

			spin_lock_irqsave(&device_request_lock, flags);

                        remove_wait_queue(&device->scpnt_wait, &wait);
                        /*
                         * FIXME - Isn't this redundant??  Someone
                         * else will have forced the state back to running.
                         */
                        set_current_state(TASK_RUNNING);
                        /*
                         * In the event that a signal has arrived that we need
                         * to consider, then simply return NULL.  Everyone
                         * that calls us should be prepared for this
                         * possibility, and pass the appropriate code back
                         * to the user.
                         */
                        if( interruptable ) {
                                if (signal_pending(current)) {
                                        spin_unlock_irqrestore(&device_request_lock, flags);
                                        return NULL;
                                }
                        }
		} else {
                        spin_unlock_irqrestore(&device_request_lock, flags);
			return NULL;
		}
	}

	SCpnt->request.rq_status = RQ_SCSI_BUSY;
	SCpnt->request.waiting = NULL;	/* And no one is waiting for this
					 * to complete */
	atomic_inc(&SCpnt->host->host_active);
	atomic_inc(&SCpnt->device->device_active);

	SCpnt->buffer  = NULL;
	SCpnt->bufflen = 0;
	SCpnt->request_buffer = NULL;
	SCpnt->request_bufflen = 0;

	SCpnt->use_sg = 0;	/* Reset the scatter-gather flag */
	SCpnt->old_use_sg = 0;
	SCpnt->transfersize = 0;	/* No default transfer size */
	SCpnt->cmd_len = 0;

	SCpnt->sc_data_direction = SCSI_DATA_UNKNOWN;
	SCpnt->sc_request = NULL;
	SCpnt->sc_magic = SCSI_CMND_MAGIC;

        SCpnt->result = 0;
	SCpnt->underflow = 0;	/* Do not flag underflow conditions */
	SCpnt->old_underflow = 0;
	SCpnt->resid = 0;
	SCpnt->state = SCSI_STATE_INITIALIZING;
	SCpnt->owner = SCSI_OWNER_HIGHLEVEL;

	spin_unlock_irqrestore(&device_request_lock, flags);

	SCSI_LOG_MLQUEUE(5, printk("Activating command for device %d (%d)\n",
				   SCpnt->target,
				atomic_read(&SCpnt->host->host_active)));

	return SCpnt;
}

inline void __scsi_release_command(Scsi_Cmnd * SCpnt)
{
	unsigned long flags;
        Scsi_Device * SDpnt;

	spin_lock_irqsave(&device_request_lock, flags);

        SDpnt = SCpnt->device;

	SCpnt->request.rq_status = RQ_INACTIVE;
	SCpnt->state = SCSI_STATE_UNUSED;
	SCpnt->owner = SCSI_OWNER_NOBODY;
	atomic_dec(&SCpnt->host->host_active);
	atomic_dec(&SDpnt->device_active);

	SCSI_LOG_MLQUEUE(5, printk("Deactivating command for device %d (active=%d, failed=%d)\n",
				   SCpnt->target,
				   atomic_read(&SCpnt->host->host_active),
				   SCpnt->host->host_failed));
	if (SCpnt->host->host_failed != 0) {
		SCSI_LOG_ERROR_RECOVERY(5, printk("Error handler thread %d %d\n",
						SCpnt->host->in_recovery,
						SCpnt->host->eh_active));
	}
	/*
	 * If the host is having troubles, then look to see if this was the last
	 * command that might have failed.  If so, wake up the error handler.
	 */
	if (SCpnt->host->in_recovery
	    && !SCpnt->host->eh_active
	    && SCpnt->host->host_busy == SCpnt->host->host_failed) {
		SCSI_LOG_ERROR_RECOVERY(5, printk("Waking error handler thread (%d)\n",
			     atomic_read(&SCpnt->host->eh_wait->count)));
		up(SCpnt->host->eh_wait);
	}

	spin_unlock_irqrestore(&device_request_lock, flags);

        /*
         * Wake up anyone waiting for this device.  Do this after we
         * have released the lock, as they will need it as soon as
         * they wake up.  
         */
	wake_up(&SDpnt->scpnt_wait);
}

/*
 * Function:    scsi_release_command
 *
 * Purpose:     Release a command block.
 *
 * Arguments:   SCpnt - command block we are releasing.
 *
 * Notes:       The command block can no longer be used by the caller once
 *              this funciton is called.  This is in effect the inverse
 *              of scsi_allocate_device.  Note that we also must perform
 *              a couple of additional tasks.  We must first wake up any
 *              processes that might have blocked waiting for a command
 *              block, and secondly we must hit the queue handler function
 *              to make sure that the device is busy.  Note - there is an
 *              option to not do this - there were instances where we could
 *              recurse too deeply and blow the stack if this happened
 *              when we were indirectly called from the request function
 *              itself.
 *
 *              The idea is that a lot of the mid-level internals gunk
 *              gets hidden in this function.  Upper level drivers don't
 *              have any chickens to wave in the air to get things to
 *              work reliably.
 *
 *              This function is deprecated, and drivers should be
 *              rewritten to use Scsi_Request instead of Scsi_Cmnd.
 */
void scsi_release_command(Scsi_Cmnd * SCpnt)
{
        request_queue_t *q;
        Scsi_Device * SDpnt;

        SDpnt = SCpnt->device;

        __scsi_release_command(SCpnt);

        /*
         * Finally, hit the queue request function to make sure that
         * the device is actually busy if there are requests present.
         * This won't block - if the device cannot take any more, life
         * will go on.  
         */
        q = &SDpnt->request_queue;
        scsi_queue_next_request(q, NULL);                
}

/*
 * Function:    scsi_dispatch_command
 *
 * Purpose:     Dispatch a command to the low-level driver.
 *
 * Arguments:   SCpnt - command block we are dispatching.
 *
 * Notes:
 */
int scsi_dispatch_cmd(Scsi_Cmnd * SCpnt)
{
#ifdef DEBUG_DELAY
	unsigned long clock;
#endif
	struct Scsi_Host *host;
	int rtn = 0;
	unsigned long flags = 0;
	unsigned long timeout;

	ASSERT_LOCK(&io_request_lock, 0);

#if DEBUG
	unsigned long *ret = 0;
#ifdef __mips__
	__asm__ __volatile__("move\t%0,$31":"=r"(ret));
#else
	ret = __builtin_return_address(0);
#endif
#endif

	host = SCpnt->host;

	/* Assign a unique nonzero serial_number. */
	if (++serial_number == 0)
		serial_number = 1;
	SCpnt->serial_number = serial_number;
	SCpnt->pid = scsi_pid++;

	/*
	 * We will wait MIN_RESET_DELAY clock ticks after the last reset so
	 * we can avoid the drive not being ready.
	 */
	timeout = host->last_reset + MIN_RESET_DELAY;

	if (host->resetting && time_before(jiffies, timeout)) {
		int ticks_remaining = timeout - jiffies;
		/*
		 * NOTE: This may be executed from within an interrupt
		 * handler!  This is bad, but for now, it'll do.  The irq
		 * level of the interrupt handler has been masked out by the
		 * platform dependent interrupt handling code already, so the
		 * sti() here will not cause another call to the SCSI host's
		 * interrupt handler (assuming there is one irq-level per
		 * host).
		 */
		while (--ticks_remaining >= 0)
			mdelay(1 + 999 / HZ);
		host->resetting = 0;
	}
	if (host->hostt->use_new_eh_code) {
		scsi_add_timer(SCpnt, SCpnt->timeout_per_command, scsi_times_out);
	} else {
		scsi_add_timer(SCpnt, SCpnt->timeout_per_command,
			       scsi_old_times_out);
	}

	/*
	 * We will use a queued command if possible, otherwise we will emulate the
	 * queuing and calling of completion function ourselves.
	 */
	SCSI_LOG_MLQUEUE(3, printk("scsi_dispatch_cmnd (host = %d, channel = %d, target = %d, "
	       "command = %p, buffer = %p, \nbufflen = %d, done = %p)\n",
	SCpnt->host->host_no, SCpnt->channel, SCpnt->target, SCpnt->cmnd,
			    SCpnt->buffer, SCpnt->bufflen, SCpnt->done));

	SCpnt->state = SCSI_STATE_QUEUED;
	SCpnt->owner = SCSI_OWNER_LOWLEVEL;
	if (host->can_queue) {
		SCSI_LOG_MLQUEUE(3, printk("queuecommand : routine at %p\n",
					   host->hostt->queuecommand));
		/*
		 * Use the old error handling code if we haven't converted the driver
		 * to use the new one yet.  Note - only the new queuecommand variant
		 * passes a meaningful return value.
		 */
		if (host->hostt->use_new_eh_code) {
			/*
			 * Before we queue this command, check if the command
			 * length exceeds what the host adapter can handle.
			 */
			if (CDB_SIZE(SCpnt) <= SCpnt->host->max_cmd_len) {
				spin_lock_irqsave(&io_request_lock, flags);
				rtn = host->hostt->queuecommand(SCpnt, scsi_done);
				spin_unlock_irqrestore(&io_request_lock, flags);
				if (rtn != 0) {
					scsi_delete_timer(SCpnt);
					scsi_mlqueue_insert(SCpnt, SCSI_MLQUEUE_HOST_BUSY);
					SCSI_LOG_MLQUEUE(3, printk("queuecommand : request rejected\n"));                                
				}
			} else {
				SCSI_LOG_MLQUEUE(3, printk("queuecommand : command too long.\n"));
				SCpnt->result = (DID_ABORT << 16);
				spin_lock_irqsave(&io_request_lock, flags);
				scsi_done(SCpnt);
				spin_unlock_irqrestore(&io_request_lock, flags);
				rtn = 1;
			}
		} else {
			/*
			 * Before we queue this command, check if the command
			 * length exceeds what the host adapter can handle.
			 */
			if (CDB_SIZE(SCpnt) <= SCpnt->host->max_cmd_len) {
				spin_lock_irqsave(&io_request_lock, flags);
				host->hostt->queuecommand(SCpnt, scsi_old_done);
				spin_unlock_irqrestore(&io_request_lock, flags);
			} else {
				SCSI_LOG_MLQUEUE(3, printk("queuecommand : command too long.\n"));
				SCpnt->result = (DID_ABORT << 16);
				spin_lock_irqsave(&io_request_lock, flags);
				scsi_old_done(SCpnt);
				spin_unlock_irqrestore(&io_request_lock, flags);
				rtn = 1;
			}
		}
	} else {
		int temp;

		SCSI_LOG_MLQUEUE(3, printk("command() :  routine at %p\n", host->hostt->command));
                spin_lock_irqsave(&io_request_lock, flags);
		temp = host->hostt->command(SCpnt);
		SCpnt->result = temp;
#ifdef DEBUG_DELAY
                spin_unlock_irqrestore(&io_request_lock, flags);
		clock = jiffies + 4 * HZ;
		while (time_before(jiffies, clock)) {
			barrier();
			cpu_relax();
		}
		printk("done(host = %d, result = %04x) : routine at %p\n",
		       host->host_no, temp, host->hostt->command);
                spin_lock_irqsave(&io_request_lock, flags);
#endif
		if (host->hostt->use_new_eh_code) {
			scsi_done(SCpnt);
		} else {
			scsi_old_done(SCpnt);
		}
                spin_unlock_irqrestore(&io_request_lock, flags);
	}
	SCSI_LOG_MLQUEUE(3, printk("leaving scsi_dispatch_cmnd()\n"));
	return rtn;
}

devfs_handle_t scsi_devfs_handle;

/*
 * scsi_do_cmd sends all the commands out to the low-level driver.  It
 * handles the specifics required for each low level driver - ie queued
 * or non queued.  It also prevents conflicts when different high level
 * drivers go for the same host at the same time.
 */

void scsi_wait_req (Scsi_Request * SRpnt, const void *cmnd ,
 		  void *buffer, unsigned bufflen, 
 		  int timeout, int retries)
{
	DECLARE_COMPLETION(wait);
	request_queue_t *q = &SRpnt->sr_device->request_queue;
	
	SRpnt->sr_request.waiting = &wait;
	SRpnt->sr_request.rq_status = RQ_SCSI_BUSY;
	scsi_do_req (SRpnt, (void *) cmnd,
		buffer, bufflen, scsi_wait_done, timeout, retries);
	generic_unplug_device(q);
	wait_for_completion(&wait);
	SRpnt->sr_request.waiting = NULL;
	if( SRpnt->sr_command != NULL )
	{
		scsi_release_command(SRpnt->sr_command);
		SRpnt->sr_command = NULL;
	}

}
 
/*
 * Function:    scsi_do_req
 *
 * Purpose:     Queue a SCSI request
 *
 * Arguments:   SRpnt     - command descriptor.
 *              cmnd      - actual SCSI command to be performed.
 *              buffer    - data buffer.
 *              bufflen   - size of data buffer.
 *              done      - completion function to be run.
 *              timeout   - how long to let it run before timeout.
 *              retries   - number of retries we allow.
 *
 * Lock status: With the new queueing code, this is SMP-safe, and no locks
 *              need be held upon entry.   The old queueing code the lock was
 *              assumed to be held upon entry.
 *
 * Returns:     Nothing.
 *
 * Notes:       Prior to the new queue code, this function was not SMP-safe.
 *              Also, this function is now only used for queueing requests
 *              for things like ioctls and character device requests - this
 *              is because we essentially just inject a request into the
 *              queue for the device. Normal block device handling manipulates
 *              the queue directly.
 */
void scsi_do_req(Scsi_Request * SRpnt, const void *cmnd,
	      void *buffer, unsigned bufflen, void (*done) (Scsi_Cmnd *),
		 int timeout, int retries)
{
	Scsi_Device * SDpnt = SRpnt->sr_device;
	struct Scsi_Host *host = SDpnt->host;

	ASSERT_LOCK(&io_request_lock, 0);

	SCSI_LOG_MLQUEUE(4,
			 {
			 int i;
			 int target = SDpnt->id;
			 int size = COMMAND_SIZE(((const unsigned char *)cmnd)[0]);
			 printk("scsi_do_req (host = %d, channel = %d target = %d, "
		    "buffer =%p, bufflen = %d, done = %p, timeout = %d, "
				"retries = %d)\n"
				"command : ", host->host_no, SDpnt->channel, target, buffer,
				bufflen, done, timeout, retries);
			 for (i	 = 0; i < size; ++i)
			 	printk("%02x  ", ((unsigned char *) cmnd)[i]);
			 	printk("\n");
			 });

	if (!host) {
		panic("Invalid or not present host.\n");
	}

	/*
	 * If the upper level driver is reusing these things, then
	 * we should release the low-level block now.  Another one will
	 * be allocated later when this request is getting queued.
	 */
	if( SRpnt->sr_command != NULL )
	{
		scsi_release_command(SRpnt->sr_command);
		SRpnt->sr_command = NULL;
	}

	/*
	 * We must prevent reentrancy to the lowlevel host driver.  This prevents
	 * it - we enter a loop until the host we want to talk to is not busy.
	 * Race conditions are prevented, as interrupts are disabled in between the
	 * time we check for the host being not busy, and the time we mark it busy
	 * ourselves.
	 */


	/*
	 * Our own function scsi_done (which marks the host as not busy, disables
	 * the timeout counter, etc) will be called by us or by the
	 * scsi_hosts[host].queuecommand() function needs to also call
	 * the completion function for the high level driver.
	 */

	memcpy((void *) SRpnt->sr_cmnd, (const void *) cmnd, 
	       sizeof(SRpnt->sr_cmnd));
	SRpnt->sr_bufflen = bufflen;
	SRpnt->sr_buffer = buffer;
	SRpnt->sr_allowed = retries;
	SRpnt->sr_done = done;
	SRpnt->sr_timeout_per_command = timeout;

	if (SRpnt->sr_cmd_len == 0)
		SRpnt->sr_cmd_len = COMMAND_SIZE(SRpnt->sr_cmnd[0]);

	/*
	 * At this point, we merely set up the command, stick it in the normal
	 * request queue, and return.  Eventually that request will come to the
	 * top of the list, and will be dispatched.
	 */
	scsi_insert_special_req(SRpnt, 0);

	SCSI_LOG_MLQUEUE(3, printk("Leaving scsi_do_req()\n"));
}
 
/*
 * Function:    scsi_init_cmd_from_req
 *
 * Purpose:     Queue a SCSI command
 * Purpose:     Initialize a Scsi_Cmnd from a Scsi_Request
 *
 * Arguments:   SCpnt     - command descriptor.
 *              SRpnt     - Request from the queue.
 *
 * Lock status: None needed.
 *
 * Returns:     Nothing.
 *
 * Notes:       Mainly transfer data from the request structure to the
 *              command structure.  The request structure is allocated
 *              using the normal memory allocator, and requests can pile
 *              up to more or less any depth.  The command structure represents
 *              a consumable resource, as these are allocated into a pool
 *              when the SCSI subsystem initializes.  The preallocation is
 *              required so that in low-memory situations a disk I/O request
 *              won't cause the memory manager to try and write out a page.
 *              The request structure is generally used by ioctls and character
 *              devices.
 */
void scsi_init_cmd_from_req(Scsi_Cmnd * SCpnt, Scsi_Request * SRpnt)
{
	struct Scsi_Host *host = SCpnt->host;

	ASSERT_LOCK(&io_request_lock, 0);

	SCpnt->owner = SCSI_OWNER_MIDLEVEL;
	SRpnt->sr_command = SCpnt;

	if (!host) {
		panic("Invalid or not present host.\n");
	}

	SCpnt->cmd_len = SRpnt->sr_cmd_len;
	SCpnt->use_sg = SRpnt->sr_use_sg;

	memcpy((void *) &SCpnt->request, (const void *) &SRpnt->sr_request,
	       sizeof(SRpnt->sr_request));
	memcpy((void *) SCpnt->data_cmnd, (const void *) SRpnt->sr_cmnd, 
	       sizeof(SCpnt->data_cmnd));
	SCpnt->reset_chain = NULL;
	SCpnt->serial_number = 0;
	SCpnt->serial_number_at_timeout = 0;
	SCpnt->bufflen = SRpnt->sr_bufflen;
	SCpnt->buffer = SRpnt->sr_buffer;
	SCpnt->flags = 0;
	SCpnt->retries = 0;
	SCpnt->allowed = SRpnt->sr_allowed;
	SCpnt->done = SRpnt->sr_done;
	SCpnt->timeout_per_command = SRpnt->sr_timeout_per_command;

	SCpnt->sc_data_direction = SRpnt->sr_data_direction;

	SCpnt->sglist_len = SRpnt->sr_sglist_len;
	SCpnt->underflow = SRpnt->sr_underflow;

	SCpnt->sc_request = SRpnt;

	memcpy((void *) SCpnt->cmnd, (const void *) SRpnt->sr_cmnd, 
	       sizeof(SCpnt->cmnd));
	/* Zero the sense buffer.  Some host adapters automatically request
	 * sense on error.  0 is not a valid sense code.
	 */
	memset((void *) SCpnt->sense_buffer, 0, sizeof SCpnt->sense_buffer);
	SCpnt->request_buffer = SRpnt->sr_buffer;
	SCpnt->request_bufflen = SRpnt->sr_bufflen;
	SCpnt->old_use_sg = SCpnt->use_sg;
	if (SCpnt->cmd_len == 0)
		SCpnt->cmd_len = COMMAND_SIZE(SCpnt->cmnd[0]);
	SCpnt->old_cmd_len = SCpnt->cmd_len;
	SCpnt->sc_old_data_direction = SCpnt->sc_data_direction;
	SCpnt->old_underflow = SCpnt->underflow;

	/* Start the timer ticking.  */

	SCpnt->internal_timeout = NORMAL_TIMEOUT;
	SCpnt->abort_reason = 0;
	SCpnt->result = 0;

	SCSI_LOG_MLQUEUE(3, printk("Leaving scsi_init_cmd_from_req()\n"));
}

/*
 * Function:    scsi_do_cmd
 *
 * Purpose:     Queue a SCSI command
 *
 * Arguments:   SCpnt     - command descriptor.
 *              cmnd      - actual SCSI command to be performed.
 *              buffer    - data buffer.
 *              bufflen   - size of data buffer.
 *              done      - completion function to be run.
 *              timeout   - how long to let it run before timeout.
 *              retries   - number of retries we allow.
 *
 * Lock status: With the new queueing code, this is SMP-safe, and no locks
 *              need be held upon entry.   The old queueing code the lock was
 *              assumed to be held upon entry.
 *
 * Returns:     Nothing.
 *
 * Notes:       Prior to the new queue code, this function was not SMP-safe.
 *              Also, this function is now only used for queueing requests
 *              for things like ioctls and character device requests - this
 *              is because we essentially just inject a request into the
 *              queue for the device. Normal block device handling manipulates
 *              the queue directly.
 */
void scsi_do_cmd(Scsi_Cmnd * SCpnt, const void *cmnd,
	      void *buffer, unsigned bufflen, void (*done) (Scsi_Cmnd *),
		 int timeout, int retries)
{
	struct Scsi_Host *host = SCpnt->host;

	ASSERT_LOCK(&io_request_lock, 0);

	SCpnt->pid = scsi_pid++;
	SCpnt->owner = SCSI_OWNER_MIDLEVEL;

	SCSI_LOG_MLQUEUE(4,
			 {
			 int i;
			 int target = SCpnt->target;
			 int size = COMMAND_SIZE(((const unsigned char *)cmnd)[0]);
			 printk("scsi_do_cmd (host = %d, channel = %d target = %d, "
		    "buffer =%p, bufflen = %d, done = %p, timeout = %d, "
				"retries = %d)\n"
				"command : ", host->host_no, SCpnt->channel, target, buffer,
				bufflen, done, timeout, retries);
			 for (i = 0; i < size; ++i)
			 	printk("%02x  ", ((unsigned char *) cmnd)[i]);
			 	printk("\n");
			 });

	if (!host) {
		panic("Invalid or not present host.\n");
	}
	/*
	 * We must prevent reentrancy to the lowlevel host driver.  This prevents
	 * it - we enter a loop until the host we want to talk to is not busy.
	 * Race conditions are prevented, as interrupts are disabled in between the
	 * time we check for the host being not busy, and the time we mark it busy
	 * ourselves.
	 */


	/*
	 * Our own function scsi_done (which marks the host as not busy, disables
	 * the timeout counter, etc) will be called by us or by the
	 * scsi_hosts[host].queuecommand() function needs to also call
	 * the completion function for the high level driver.
	 */

	memcpy((void *) SCpnt->data_cmnd, (const void *) cmnd, 
               sizeof(SCpnt->data_cmnd));
	SCpnt->reset_chain = NULL;
	SCpnt->serial_number = 0;
	SCpnt->serial_number_at_timeout = 0;
	SCpnt->bufflen = bufflen;
	SCpnt->buffer = buffer;
	SCpnt->flags = 0;
	SCpnt->retries = 0;
	SCpnt->allowed = retries;
	SCpnt->done = done;
	SCpnt->timeout_per_command = timeout;

	memcpy((void *) SCpnt->cmnd, (const void *) cmnd, 
               sizeof(SCpnt->cmnd));
	/* Zero the sense buffer.  Some host adapters automatically request
	 * sense on error.  0 is not a valid sense code.
	 */
	memset((void *) SCpnt->sense_buffer, 0, sizeof SCpnt->sense_buffer);
	SCpnt->request_buffer = buffer;
	SCpnt->request_bufflen = bufflen;
	SCpnt->old_use_sg = SCpnt->use_sg;
	if (SCpnt->cmd_len == 0)
		SCpnt->cmd_len = COMMAND_SIZE(SCpnt->cmnd[0]);
	SCpnt->old_cmd_len = SCpnt->cmd_len;
	SCpnt->sc_old_data_direction = SCpnt->sc_data_direction;
	SCpnt->old_underflow = SCpnt->underflow;

	/* Start the timer ticking.  */

	SCpnt->internal_timeout = NORMAL_TIMEOUT;
	SCpnt->abort_reason = 0;
	SCpnt->result = 0;

	/*
	 * At this point, we merely set up the command, stick it in the normal
	 * request queue, and return.  Eventually that request will come to the
	 * top of the list, and will be dispatched.
	 */
	scsi_insert_special_cmd(SCpnt, 0);

	SCSI_LOG_MLQUEUE(3, printk("Leaving scsi_do_cmd()\n"));
}

/*
 * This function is the mid-level interrupt routine, which decides how
 *  to handle error conditions.  Each invocation of this function must
 *  do one and *only* one of the following:
 *
 *      1) Insert command in BH queue.
 *      2) Activate error handler for host.
 *
 * FIXME(eric) - I am concerned about stack overflow (still).  An
 * interrupt could come while we are processing the bottom queue,
 * which would cause another command to be stuffed onto the bottom
 * queue, and it would in turn be processed as that interrupt handler
 * is returning.  Given a sufficiently steady rate of returning
 * commands, this could cause the stack to overflow.  I am not sure
 * what is the most appropriate solution here - we should probably
 * keep a depth count, and not process any commands while we still
 * have a bottom handler active higher in the stack.
 *
 * There is currently code in the bottom half handler to monitor
 * recursion in the bottom handler and report if it ever happens.  If
 * this becomes a problem, it won't be hard to engineer something to
 * deal with it so that only the outer layer ever does any real
 * processing.  
 */
void scsi_done(Scsi_Cmnd * SCpnt)
{
	unsigned long flags;
	int tstatus;

	/*
	 * We don't have to worry about this one timing out any more.
	 */
	tstatus = scsi_delete_timer(SCpnt);

	/*
	 * If we are unable to remove the timer, it means that the command
	 * has already timed out.  In this case, we have no choice but to
	 * let the timeout function run, as we have no idea where in fact
	 * that function could really be.  It might be on another processor,
	 * etc, etc.
	 */
	if (!tstatus) {
		SCpnt->done_late = 1;
		return;
	}
	/* Set the serial numbers back to zero */
	SCpnt->serial_number = 0;

	/*
	 * First, see whether this command already timed out.  If so, we ignore
	 * the response.  We treat it as if the command never finished.
	 *
	 * Since serial_number is now 0, the error handler cound detect this
	 * situation and avoid to call the low level driver abort routine.
	 * (DB)
         *
         * FIXME(eric) - I believe that this test is now redundant, due to
         * the test of the return status of del_timer().
	 */
	if (SCpnt->state == SCSI_STATE_TIMEOUT) {
		SCSI_LOG_MLCOMPLETE(1, printk("Ignoring completion of %p due to timeout status", SCpnt));
		return;
	}
	spin_lock_irqsave(&scsi_bhqueue_lock, flags);

	SCpnt->serial_number_at_timeout = 0;
	SCpnt->state = SCSI_STATE_BHQUEUE;
	SCpnt->owner = SCSI_OWNER_BH_HANDLER;
	SCpnt->bh_next = NULL;

	/*
	 * Next, put this command in the BH queue.
	 * 
	 * We need a spinlock here, or compare and exchange if we can reorder incoming
	 * Scsi_Cmnds, as it happens pretty often scsi_done is called multiple times
	 * before bh is serviced. -jj
	 *
	 * We already have the io_request_lock here, since we are called from the
	 * interrupt handler or the error handler. (DB)
	 *
	 * This may be true at the moment, but I would like to wean all of the low
	 * level drivers away from using io_request_lock.   Technically they should
	 * all use their own locking.  I am adding a small spinlock to protect
	 * this datastructure to make it safe for that day.  (ERY)
	 */
	if (!scsi_bh_queue_head) {
		scsi_bh_queue_head = SCpnt;
		scsi_bh_queue_tail = SCpnt;
	} else {
		scsi_bh_queue_tail->bh_next = SCpnt;
		scsi_bh_queue_tail = SCpnt;
	}

	spin_unlock_irqrestore(&scsi_bhqueue_lock, flags);
	/*
	 * Mark the bottom half handler to be run.
	 */
	mark_bh(SCSI_BH);
}

/*
 * Procedure:   scsi_bottom_half_handler
 *
 * Purpose:     Called after we have finished processing interrupts, it
 *              performs post-interrupt handling for commands that may
 *              have completed.
 *
 * Notes:       This is called with all interrupts enabled.  This should reduce
 *              interrupt latency, stack depth, and reentrancy of the low-level
 *              drivers.
 *
 * The io_request_lock is required in all the routine. There was a subtle
 * race condition when scsi_done is called after a command has already
 * timed out but before the time out is processed by the error handler.
 * (DB)
 *
 * I believe I have corrected this.  We simply monitor the return status of
 * del_timer() - if this comes back as 0, it means that the timer has fired
 * and that a timeout is in progress.   I have modified scsi_done() such
 * that in this instance the command is never inserted in the bottom
 * half queue.  Thus the only time we hold the lock here is when
 * we wish to atomically remove the contents of the queue.
 */
void scsi_bottom_half_handler(void)
{
	Scsi_Cmnd *SCpnt;
	Scsi_Cmnd *SCnext;
	unsigned long flags;


	while (1 == 1) {
		spin_lock_irqsave(&scsi_bhqueue_lock, flags);
		SCpnt = scsi_bh_queue_head;
		scsi_bh_queue_head = NULL;
		spin_unlock_irqrestore(&scsi_bhqueue_lock, flags);

		if (SCpnt == NULL) {
			return;
		}
		SCnext = SCpnt->bh_next;

		for (; SCpnt; SCpnt = SCnext) {
			SCnext = SCpnt->bh_next;

			switch (scsi_decide_disposition(SCpnt)) {
			case SUCCESS:
				/*
				 * Add to BH queue.
				 */
				SCSI_LOG_MLCOMPLETE(3, printk("Command finished %d %d 0x%x\n", SCpnt->host->host_busy,
						SCpnt->host->host_failed,
							 SCpnt->result));

				scsi_finish_command(SCpnt);
				break;
			case NEEDS_RETRY:
				/*
				 * We only come in here if we want to retry a command.  The
				 * test to see whether the command should be retried should be
				 * keeping track of the number of tries, so we don't end up looping,
				 * of course.
				 */
				SCSI_LOG_MLCOMPLETE(3, printk("Command needs retry %d %d 0x%x\n", SCpnt->host->host_busy,
				SCpnt->host->host_failed, SCpnt->result));

				scsi_retry_command(SCpnt);
				break;
			case ADD_TO_MLQUEUE:
				/* 
				 * This typically happens for a QUEUE_FULL message -
				 * typically only when the queue depth is only
				 * approximate for a given device.  Adding a command
				 * to the queue for the device will prevent further commands
				 * from being sent to the device, so we shouldn't end up
				 * with tons of things being sent down that shouldn't be.
				 */
				SCSI_LOG_MLCOMPLETE(3, printk("Command rejected as device queue full, put on ml queue %p\n",
                                                              SCpnt));
				scsi_mlqueue_insert(SCpnt, SCSI_MLQUEUE_DEVICE_BUSY);
				break;
			default:
				/*
				 * Here we have a fatal error of some sort.  Turn it over to
				 * the error handler.
				 */
				SCSI_LOG_MLCOMPLETE(3, printk("Command failed %p %x active=%d busy=%d failed=%d\n",
						    SCpnt, SCpnt->result,
				  atomic_read(&SCpnt->host->host_active),
						  SCpnt->host->host_busy,
					      SCpnt->host->host_failed));

				/*
				 * Dump the sense information too.
				 */
				if ((status_byte(SCpnt->result) & CHECK_CONDITION) != 0) {
					SCSI_LOG_MLCOMPLETE(3, print_sense("bh", SCpnt));
				}
				if (SCpnt->host->eh_wait != NULL) {
					SCpnt->host->host_failed++;
					SCpnt->owner = SCSI_OWNER_ERROR_HANDLER;
					SCpnt->state = SCSI_STATE_FAILED;
					SCpnt->host->in_recovery = 1;
					/*
					 * If the host is having troubles, then look to see if this was the last
					 * command that might have failed.  If so, wake up the error handler.
					 */
					if (SCpnt->host->host_busy == SCpnt->host->host_failed) {
						SCSI_LOG_ERROR_RECOVERY(5, printk("Waking error handler thread (%d)\n",
										  atomic_read(&SCpnt->host->eh_wait->count)));
						up(SCpnt->host->eh_wait);
					}
				} else {
					/*
					 * We only get here if the error recovery thread has died.
					 */
					scsi_finish_command(SCpnt);
				}
			}
		}		/* for(; SCpnt...) */

	}			/* while(1==1) */

}

/*
 * Function:    scsi_retry_command
 *
 * Purpose:     Send a command back to the low level to be retried.
 *
 * Notes:       This command is always executed in the context of the
 *              bottom half handler, or the error handler thread. Low
 *              level drivers should not become re-entrant as a result of
 *              this.
 */
int scsi_retry_command(Scsi_Cmnd * SCpnt)
{
	memcpy((void *) SCpnt->cmnd, (void *) SCpnt->data_cmnd,
	       sizeof(SCpnt->data_cmnd));
	SCpnt->request_buffer = SCpnt->buffer;
	SCpnt->request_bufflen = SCpnt->bufflen;
	SCpnt->use_sg = SCpnt->old_use_sg;
	SCpnt->cmd_len = SCpnt->old_cmd_len;
	SCpnt->sc_data_direction = SCpnt->sc_old_data_direction;
	SCpnt->underflow = SCpnt->old_underflow;

        /*
         * Zero the sense information from the last time we tried
         * this command.
         */
	memset((void *) SCpnt->sense_buffer, 0, sizeof SCpnt->sense_buffer);

	return scsi_dispatch_cmd(SCpnt);
}

/*
 * Function:    scsi_finish_command
 *
 * Purpose:     Pass command off to upper layer for finishing of I/O
 *              request, waking processes that are waiting on results,
 *              etc.
 */
void scsi_finish_command(Scsi_Cmnd * SCpnt)
{
	struct Scsi_Host *host;
	Scsi_Device *device;
	Scsi_Request * SRpnt;
	unsigned long flags;

	ASSERT_LOCK(&io_request_lock, 0);

	host = SCpnt->host;
	device = SCpnt->device;

        /*
         * We need to protect the decrement, as otherwise a race condition
         * would exist.  Fiddling with SCpnt isn't a problem as the
         * design only allows a single SCpnt to be active in only
         * one execution context, but the device and host structures are
         * shared.
         */
	spin_lock_irqsave(&io_request_lock, flags);
	host->host_busy--;	/* Indicate that we are free */
	device->device_busy--;	/* Decrement device usage counter. */
	spin_unlock_irqrestore(&io_request_lock, flags);

        /*
         * Clear the flags which say that the device/host is no longer
         * capable of accepting new commands.  These are set in scsi_queue.c
         * for both the queue full condition on a device, and for a
         * host full condition on the host.
         */
        host->host_blocked = FALSE;
        device->device_blocked = FALSE;

	/*
	 * If we have valid sense information, then some kind of recovery
	 * must have taken place.  Make a note of this.
	 */
	if (scsi_sense_valid(SCpnt)) {
		SCpnt->result |= (DRIVER_SENSE << 24);
	}
	SCSI_LOG_MLCOMPLETE(3, printk("Notifying upper driver of completion for device %d %x\n",
				      SCpnt->device->id, SCpnt->result));

	SCpnt->owner = SCSI_OWNER_HIGHLEVEL;
	SCpnt->state = SCSI_STATE_FINISHED;

	/* We can get here with use_sg=0, causing a panic in the upper level (DB) */
	SCpnt->use_sg = SCpnt->old_use_sg;

       /*
	* If there is an associated request structure, copy the data over before we call the
	* completion function.
	*/
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
}

static int scsi_register_host(Scsi_Host_Template *);
static int scsi_unregister_host(Scsi_Host_Template *);

/*
 * Function:    scsi_release_commandblocks()
 *
 * Purpose:     Release command blocks associated with a device.
 *
 * Arguments:   SDpnt   - device
 *
 * Returns:     Nothing
 *
 * Lock status: No locking assumed or required.
 *
 * Notes:
 */
void scsi_release_commandblocks(Scsi_Device * SDpnt)
{
	Scsi_Cmnd *SCpnt, *SCnext;
	unsigned long flags;

 	spin_lock_irqsave(&device_request_lock, flags);
	for (SCpnt = SDpnt->device_queue; SCpnt; SCpnt = SCnext) {
		SDpnt->device_queue = SCnext = SCpnt->next;
		kfree((char *) SCpnt);
	}
	SDpnt->has_cmdblocks = 0;
	SDpnt->queue_depth = 0;
	spin_unlock_irqrestore(&device_request_lock, flags);
}

/*
 * Function:    scsi_build_commandblocks()
 *
 * Purpose:     Allocate command blocks associated with a device.
 *
 * Arguments:   SDpnt   - device
 *
 * Returns:     Nothing
 *
 * Lock status: No locking assumed or required.
 *
 * Notes:
 */
void scsi_build_commandblocks(Scsi_Device * SDpnt)
{
	unsigned long flags;
	struct Scsi_Host *host = SDpnt->host;
	int j;
	Scsi_Cmnd *SCpnt;

	spin_lock_irqsave(&device_request_lock, flags);

	if (SDpnt->queue_depth == 0)
	{
		SDpnt->queue_depth = host->cmd_per_lun;
		if (SDpnt->queue_depth == 0)
			SDpnt->queue_depth = 1; /* live to fight another day */
	}
	SDpnt->device_queue = NULL;

	for (j = 0; j < SDpnt->queue_depth; j++) {
		SCpnt = (Scsi_Cmnd *)
		    kmalloc(sizeof(Scsi_Cmnd),
				     GFP_ATOMIC |
				(host->unchecked_isa_dma ? GFP_DMA : 0));
		if (NULL == SCpnt)
			break;	/* If not, the next line will oops ... */
		memset(SCpnt, 0, sizeof(Scsi_Cmnd));
		SCpnt->host = host;
		SCpnt->device = SDpnt;
		SCpnt->target = SDpnt->id;
		SCpnt->lun = SDpnt->lun;
		SCpnt->channel = SDpnt->channel;
		SCpnt->request.rq_status = RQ_INACTIVE;
		SCpnt->use_sg = 0;
		SCpnt->old_use_sg = 0;
		SCpnt->old_cmd_len = 0;
		SCpnt->underflow = 0;
		SCpnt->old_underflow = 0;
		SCpnt->transfersize = 0;
		SCpnt->resid = 0;
		SCpnt->serial_number = 0;
		SCpnt->serial_number_at_timeout = 0;
		SCpnt->host_scribble = NULL;
		SCpnt->next = SDpnt->device_queue;
		SDpnt->device_queue = SCpnt;
		SCpnt->state = SCSI_STATE_UNUSED;
		SCpnt->owner = SCSI_OWNER_NOBODY;
	}
	if (j < SDpnt->queue_depth) {	/* low on space (D.Gilbert 990424) */
		printk(KERN_WARNING "scsi_build_commandblocks: want=%d, space for=%d blocks\n",
		       SDpnt->queue_depth, j);
		SDpnt->queue_depth = j;
		SDpnt->has_cmdblocks = (0 != j);
	} else {
		SDpnt->has_cmdblocks = 1;
	}
	spin_unlock_irqrestore(&device_request_lock, flags);
}

void __init scsi_host_no_insert(char *str, int n)
{
    Scsi_Host_Name *shn, *shn2;
    int len;
    
    len = strlen(str);
    if (len && (shn = (Scsi_Host_Name *) kmalloc(sizeof(Scsi_Host_Name), GFP_ATOMIC))) {
	if ((shn->name = kmalloc(len+1, GFP_ATOMIC))) {
	    strncpy(shn->name, str, len);
	    shn->name[len] = 0;
	    shn->host_no = n;
	    shn->host_registered = 0;
	    shn->loaded_as_module = 1; /* numbers shouldn't be freed in any case */
	    shn->next = NULL;
	    if (scsi_host_no_list) {
		for (shn2 = scsi_host_no_list;shn2->next;shn2 = shn2->next)
		    ;
		shn2->next = shn;
	    }
	    else
		scsi_host_no_list = shn;
	    max_scsi_hosts = n+1;
	}
	else
	    kfree((char *) shn);
    }
}

#ifdef CONFIG_PROC_FS
static int scsi_proc_info(char *buffer, char **start, off_t offset, int length)
{
	Scsi_Device *scd;
	struct Scsi_Host *HBA_ptr;
	int size, len = 0;
	off_t begin = 0;
	off_t pos = 0;

	/*
	 * First, see if there are any attached devices or not.
	 */
	for (HBA_ptr = scsi_hostlist; HBA_ptr; HBA_ptr = HBA_ptr->next) {
		if (HBA_ptr->host_queue != NULL) {
			break;
		}
	}
	size = sprintf(buffer + len, "Attached devices: %s\n", (HBA_ptr) ? "" : "none");
	len += size;
	pos = begin + len;
	for (HBA_ptr = scsi_hostlist; HBA_ptr; HBA_ptr = HBA_ptr->next) {
#if 0
		size += sprintf(buffer + len, "scsi%2d: %s\n", (int) HBA_ptr->host_no,
				HBA_ptr->hostt->procname);
		len += size;
		pos = begin + len;
#endif
		for (scd = HBA_ptr->host_queue; scd; scd = scd->next) {
			proc_print_scsidevice(scd, buffer, &size, len);
			len += size;
			pos = begin + len;

			if (pos < offset) {
				len = 0;
				begin = pos;
			}
			if (pos > offset + length)
				goto stop_output;
		}
	}

stop_output:
	*start = buffer + (offset - begin);	/* Start of wanted data */
	len -= (offset - begin);	/* Start slop */
	if (len > length)
		len = length;	/* Ending slop */
	return (len);
}

static int proc_scsi_gen_write(struct file * file, const char * buf,
                              unsigned long length, void *data)
{
	struct Scsi_Device_Template *SDTpnt;
	Scsi_Device *scd;
	struct Scsi_Host *HBA_ptr;
	char *p;
	int host, channel, id, lun;
	char * buffer;
	int err;

	if (!buf || length>PAGE_SIZE)
		return -EINVAL;

	if (!(buffer = (char *) __get_free_page(GFP_KERNEL)))
		return -ENOMEM;
	if(copy_from_user(buffer, buf, length))
	{
		err =-EFAULT;
		goto out;
	}

	err = -EINVAL;

	if (length < PAGE_SIZE)
		buffer[length] = '\0';
	else if (buffer[PAGE_SIZE-1])
		goto out;

	if (length < 11 || strncmp("scsi", buffer, 4))
		goto out;

	/*
	 * Usage: echo "scsi dump #N" > /proc/scsi/scsi
	 * to dump status of all scsi commands.  The number is used to specify the level
	 * of detail in the dump.
	 */
	if (!strncmp("dump", buffer + 5, 4)) {
		unsigned int level;

		p = buffer + 10;

		if (*p == '\0')
			goto out;

		level = simple_strtoul(p, NULL, 0);
		scsi_dump_status(level);
	}
	/*
	 * Usage: echo "scsi log token #N" > /proc/scsi/scsi
	 * where token is one of [error,scan,mlqueue,mlcomplete,llqueue,
	 * llcomplete,hlqueue,hlcomplete]
	 */
#ifdef CONFIG_SCSI_LOGGING		/* { */

	if (!strncmp("log", buffer + 5, 3)) {
		char *token;
		unsigned int level;

		p = buffer + 9;
		token = p;
		while (*p != ' ' && *p != '\t' && *p != '\0') {
			p++;
		}

		if (*p == '\0') {
			if (strncmp(token, "all", 3) == 0) {
				/*
				 * Turn on absolutely everything.
				 */
				scsi_logging_level = ~0;
			} else if (strncmp(token, "none", 4) == 0) {
				/*
				 * Turn off absolutely everything.
				 */
				scsi_logging_level = 0;
			} else {
				goto out;
			}
		} else {
			*p++ = '\0';

			level = simple_strtoul(p, NULL, 0);

			/*
			 * Now figure out what to do with it.
			 */
			if (strcmp(token, "error") == 0) {
				SCSI_SET_ERROR_RECOVERY_LOGGING(level);
			} else if (strcmp(token, "timeout") == 0) {
				SCSI_SET_TIMEOUT_LOGGING(level);
			} else if (strcmp(token, "scan") == 0) {
				SCSI_SET_SCAN_BUS_LOGGING(level);
			} else if (strcmp(token, "mlqueue") == 0) {
				SCSI_SET_MLQUEUE_LOGGING(level);
			} else if (strcmp(token, "mlcomplete") == 0) {
				SCSI_SET_MLCOMPLETE_LOGGING(level);
			} else if (strcmp(token, "llqueue") == 0) {
				SCSI_SET_LLQUEUE_LOGGING(level);
			} else if (strcmp(token, "llcomplete") == 0) {
				SCSI_SET_LLCOMPLETE_LOGGING(level);
			} else if (strcmp(token, "hlqueue") == 0) {
				SCSI_SET_HLQUEUE_LOGGING(level);
			} else if (strcmp(token, "hlcomplete") == 0) {
				SCSI_SET_HLCOMPLETE_LOGGING(level);
			} else if (strcmp(token, "ioctl") == 0) {
				SCSI_SET_IOCTL_LOGGING(level);
			} else {
				goto out;
			}
		}

		printk(KERN_INFO "scsi logging level set to 0x%8.8x\n", scsi_logging_level);
	}
#endif	/* CONFIG_SCSI_LOGGING */ /* } */

	/*
	 * Usage: echo "scsi add-single-device 0 1 2 3" >/proc/scsi/scsi
	 * with  "0 1 2 3" replaced by your "Host Channel Id Lun".
	 * Consider this feature BETA.
	 *     CAUTION: This is not for hotplugging your peripherals. As
	 *     SCSI was not designed for this you could damage your
	 *     hardware !
	 * However perhaps it is legal to switch on an
	 * already connected device. It is perhaps not
	 * guaranteed this device doesn't corrupt an ongoing data transfer.
	 */
	if (!strncmp("add-single-device", buffer + 5, 17)) {
		p = buffer + 23;

		host = simple_strtoul(p, &p, 0);
		channel = simple_strtoul(p + 1, &p, 0);
		id = simple_strtoul(p + 1, &p, 0);
		lun = simple_strtoul(p + 1, &p, 0);

		printk(KERN_INFO "scsi singledevice %d %d %d %d\n", host, channel,
		       id, lun);

		for (HBA_ptr = scsi_hostlist; HBA_ptr; HBA_ptr = HBA_ptr->next) {
			if (HBA_ptr->host_no == host) {
				break;
			}
		}
		err = -ENXIO;
		if (!HBA_ptr)
			goto out;

		for (scd = HBA_ptr->host_queue; scd; scd = scd->next) {
			if ((scd->channel == channel
			     && scd->id == id
			     && scd->lun == lun)) {
				break;
			}
		}

		err = -ENOSYS;
		if (scd)
			goto out;	/* We do not yet support unplugging */

		scan_scsis(HBA_ptr, 1, channel, id, lun);

		/* FIXME (DB) This assumes that the queue_depth routines can be used
		   in this context as well, while they were all designed to be
		   called only once after the detect routine. (DB) */
		/* queue_depth routine moved to inside scan_scsis(,1,,,) so
		   it is called before build_commandblocks() */

		err = length;
		goto out;
	}
	/*
	 * Usage: echo "scsi remove-single-device 0 1 2 3" >/proc/scsi/scsi
	 * with  "0 1 2 3" replaced by your "Host Channel Id Lun".
	 *
	 * Consider this feature pre-BETA.
	 *
	 *     CAUTION: This is not for hotplugging your peripherals. As
	 *     SCSI was not designed for this you could damage your
	 *     hardware and thoroughly confuse the SCSI subsystem.
	 *
	 */
	else if (!strncmp("remove-single-device", buffer + 5, 20)) {
		p = buffer + 26;

		host = simple_strtoul(p, &p, 0);
		channel = simple_strtoul(p + 1, &p, 0);
		id = simple_strtoul(p + 1, &p, 0);
		lun = simple_strtoul(p + 1, &p, 0);


		for (HBA_ptr = scsi_hostlist; HBA_ptr; HBA_ptr = HBA_ptr->next) {
			if (HBA_ptr->host_no == host) {
				break;
			}
		}
		err = -ENODEV;
		if (!HBA_ptr)
			goto out;

		for (scd = HBA_ptr->host_queue; scd; scd = scd->next) {
			if ((scd->channel == channel
			     && scd->id == id
			     && scd->lun == lun)) {
				break;
			}
		}

		if (scd == NULL)
			goto out;	/* there is no such device attached */

		err = -EBUSY;
		if (scd->access_count)
			goto out;

		SDTpnt = scsi_devicelist;
		while (SDTpnt != NULL) {
			if (SDTpnt->detach)
				(*SDTpnt->detach) (scd);
			SDTpnt = SDTpnt->next;
		}

		if (scd->attached == 0) {
			/*
			 * Nobody is using this device any more.
			 * Free all of the command structures.
			 */
                        if (HBA_ptr->hostt->revoke)
                                HBA_ptr->hostt->revoke(scd);
			devfs_unregister (scd->de);
			scsi_release_commandblocks(scd);

			/* Now we can remove the device structure */
			if (scd->next != NULL)
				scd->next->prev = scd->prev;

			if (scd->prev != NULL)
				scd->prev->next = scd->next;

			if (HBA_ptr->host_queue == scd) {
				HBA_ptr->host_queue = scd->next;
			}
			blk_cleanup_queue(&scd->request_queue);
			kfree((char *) scd);
		} else {
			goto out;
		}
		err = 0;
	}
out:
	
	free_page((unsigned long) buffer);
	return err;
}
#endif

/*
 * This entry point should be called by a driver if it is trying
 * to add a low level scsi driver to the system.
 */
static int scsi_register_host(Scsi_Host_Template * tpnt)
{
	int pcount;
	struct Scsi_Host *shpnt;
	Scsi_Device *SDpnt;
	struct Scsi_Device_Template *sdtpnt;
	const char *name;
	unsigned long flags;
	int out_of_space = 0;

	if (tpnt->next || !tpnt->detect)
		return 1;	/* Must be already loaded, or
				 * no detect routine available
				 */

	/* If max_sectors isn't set, default to max */
	if (!tpnt->max_sectors)
		tpnt->max_sectors = MAX_SECTORS;

	pcount = next_scsi_host;

	MOD_INC_USE_COUNT;

	/* The detect routine must carefully spinunlock/spinlock if 
	   it enables interrupts, since all interrupt handlers do 
	   spinlock as well.
	   All lame drivers are going to fail due to the following 
	   spinlock. For the time beeing let's use it only for drivers 
	   using the new scsi code. NOTE: the detect routine could
	   redefine the value tpnt->use_new_eh_code. (DB, 13 May 1998) */

	if (tpnt->use_new_eh_code) {
		spin_lock_irqsave(&io_request_lock, flags);
		tpnt->present = tpnt->detect(tpnt);
		spin_unlock_irqrestore(&io_request_lock, flags);
	} else
		tpnt->present = tpnt->detect(tpnt);

	if (tpnt->present) {
		if (pcount == next_scsi_host) {
			if (tpnt->present > 1) {
				printk(KERN_ERR "scsi: Failure to register low-level scsi driver");
				scsi_unregister_host(tpnt);
				return 1;
			}
			/* 
			 * The low-level driver failed to register a driver.
			 * We can do this now.
			 */
			if(scsi_register(tpnt, 0)==NULL)
			{
				printk(KERN_ERR "scsi: register failed.\n");
				scsi_unregister_host(tpnt);
				return 1;
			}
		}
		tpnt->next = scsi_hosts;	/* Add to the linked list */
		scsi_hosts = tpnt;

		/* Add the new driver to /proc/scsi */
#ifdef CONFIG_PROC_FS
		build_proc_dir_entries(tpnt);
#endif


		/*
		 * Add the kernel threads for each host adapter that will
		 * handle error correction.
		 */
		for (shpnt = scsi_hostlist; shpnt; shpnt = shpnt->next) {
			if (shpnt->hostt == tpnt && shpnt->hostt->use_new_eh_code) {
				DECLARE_MUTEX_LOCKED(sem);

				shpnt->eh_notify = &sem;
				kernel_thread((int (*)(void *)) scsi_error_handler,
					      (void *) shpnt, 0);

				/*
				 * Now wait for the kernel error thread to initialize itself
				 * as it might be needed when we scan the bus.
				 */
				down(&sem);
				shpnt->eh_notify = NULL;
			}
		}

		for (shpnt = scsi_hostlist; shpnt; shpnt = shpnt->next) {
			if (shpnt->hostt == tpnt) {
				if (tpnt->info) {
					name = tpnt->info(shpnt);
				} else {
					name = tpnt->name;
				}
				printk(KERN_INFO "scsi%d : %s\n",		/* And print a little message */
				       shpnt->host_no, name);
			}
		}

		/* The next step is to call scan_scsis here.  This generates the
		 * Scsi_Devices entries
		 */
		for (shpnt = scsi_hostlist; shpnt; shpnt = shpnt->next) {
			if (shpnt->hostt == tpnt) {
				scan_scsis(shpnt, 0, 0, 0, 0);
				if (shpnt->select_queue_depths != NULL) {
					(shpnt->select_queue_depths) (shpnt, shpnt->host_queue);
				}
			}
		}

		for (sdtpnt = scsi_devicelist; sdtpnt; sdtpnt = sdtpnt->next) {
			if (sdtpnt->init && sdtpnt->dev_noticed)
				(*sdtpnt->init) ();
		}

		/*
		 * Next we create the Scsi_Cmnd structures for this host 
		 */
		for (shpnt = scsi_hostlist; shpnt; shpnt = shpnt->next) {
			for (SDpnt = shpnt->host_queue; SDpnt; SDpnt = SDpnt->next)
				if (SDpnt->host->hostt == tpnt) {
					for (sdtpnt = scsi_devicelist; sdtpnt; sdtpnt = sdtpnt->next)
						if (sdtpnt->attach)
							(*sdtpnt->attach) (SDpnt);
					if (SDpnt->attached) {
						scsi_build_commandblocks(SDpnt);
						if (0 == SDpnt->has_cmdblocks)
							out_of_space = 1;
					}
				}
		}

		/*
		 * Now that we have all of the devices, resize the DMA pool,
		 * as required.  */
		if (!out_of_space)
			scsi_resize_dma_pool();


		/* This does any final handling that is required. */
		for (sdtpnt = scsi_devicelist; sdtpnt; sdtpnt = sdtpnt->next) {
			if (sdtpnt->finish && sdtpnt->nr_dev) {
				(*sdtpnt->finish) ();
			}
		}
	}
#if defined(USE_STATIC_SCSI_MEMORY)
	printk("SCSI memory: total %ldKb, used %ldKb, free %ldKb.\n",
	       (scsi_memory_upper_value - scsi_memory_lower_value) / 1024,
	       (scsi_init_memory_start - scsi_memory_lower_value) / 1024,
	       (scsi_memory_upper_value - scsi_init_memory_start) / 1024);
#endif

	if (out_of_space) {
		scsi_unregister_host(tpnt);	/* easiest way to clean up?? */
		return 1;
	} else
		return 0;
}

/*
 * Similarly, this entry point should be called by a loadable module if it
 * is trying to remove a low level scsi driver from the system.
 */
static int scsi_unregister_host(Scsi_Host_Template * tpnt)
{
	int online_status;
	int pcount0, pcount;
	Scsi_Cmnd *SCpnt;
	Scsi_Device *SDpnt;
	Scsi_Device *SDpnt1;
	struct Scsi_Device_Template *sdtpnt;
	struct Scsi_Host *sh1;
	struct Scsi_Host *shpnt;
	char name[10];	/* host_no>=10^9? I don't think so. */

	/* get the big kernel lock, so we don't race with open() */
	lock_kernel();

	/*
	 * First verify that this host adapter is completely free with no pending
	 * commands 
	 */
	for (shpnt = scsi_hostlist; shpnt; shpnt = shpnt->next) {
		for (SDpnt = shpnt->host_queue; SDpnt;
		     SDpnt = SDpnt->next) {
			if (SDpnt->host->hostt == tpnt
			    && SDpnt->host->hostt->module
			    && GET_USE_COUNT(SDpnt->host->hostt->module))
				goto err_out;
			/* 
			 * FIXME(eric) - We need to find a way to notify the
			 * low level driver that we are shutting down - via the
			 * special device entry that still needs to get added. 
			 *
			 * Is detach interface below good enough for this?
			 */
		}
	}

	/*
	 * FIXME(eric) put a spinlock on this.  We force all of the devices offline
	 * to help prevent race conditions where other hosts/processors could try and
	 * get in and queue a command.
	 */
	for (shpnt = scsi_hostlist; shpnt; shpnt = shpnt->next) {
		for (SDpnt = shpnt->host_queue; SDpnt;
		     SDpnt = SDpnt->next) {
			if (SDpnt->host->hostt == tpnt)
				SDpnt->online = FALSE;

		}
	}

	for (shpnt = scsi_hostlist; shpnt; shpnt = shpnt->next) {
		if (shpnt->hostt != tpnt) {
			continue;
		}
		for (SDpnt = shpnt->host_queue; SDpnt;
		     SDpnt = SDpnt->next) {
			/*
			 * Loop over all of the commands associated with the device.  If any of
			 * them are busy, then set the state back to inactive and bail.
			 */
			for (SCpnt = SDpnt->device_queue; SCpnt;
			     SCpnt = SCpnt->next) {
				online_status = SDpnt->online;
				SDpnt->online = FALSE;
				if (SCpnt->request.rq_status != RQ_INACTIVE) {
					printk(KERN_ERR "SCSI device not inactive - rq_status=%d, target=%d, pid=%ld, state=%d, owner=%d.\n",
					       SCpnt->request.rq_status, SCpnt->target, SCpnt->pid,
					     SCpnt->state, SCpnt->owner);
					for (SDpnt1 = shpnt->host_queue; SDpnt1;
					     SDpnt1 = SDpnt1->next) {
						for (SCpnt = SDpnt1->device_queue; SCpnt;
						     SCpnt = SCpnt->next)
							if (SCpnt->request.rq_status == RQ_SCSI_DISCONNECTING)
								SCpnt->request.rq_status = RQ_INACTIVE;
					}
					SDpnt->online = online_status;
					printk(KERN_ERR "Device busy???\n");
					goto err_out;
				}
				/*
				 * No, this device is really free.  Mark it as such, and
				 * continue on.
				 */
				SCpnt->state = SCSI_STATE_DISCONNECTING;
				SCpnt->request.rq_status = RQ_SCSI_DISCONNECTING;	/* Mark as busy */
			}
		}
	}
	/* Next we detach the high level drivers from the Scsi_Device structures */

	for (shpnt = scsi_hostlist; shpnt; shpnt = shpnt->next) {
		if (shpnt->hostt != tpnt) {
			continue;
		}
		for (SDpnt = shpnt->host_queue; SDpnt;
		     SDpnt = SDpnt->next) {
			for (sdtpnt = scsi_devicelist; sdtpnt; sdtpnt = sdtpnt->next)
				if (sdtpnt->detach)
					(*sdtpnt->detach) (SDpnt);

			/* If something still attached, punt */
			if (SDpnt->attached) {
				printk(KERN_ERR "Attached usage count = %d\n", SDpnt->attached);
				goto err_out;
			}
			devfs_unregister (SDpnt->de);
		}
	}

	/*
	 * Next, kill the kernel error recovery thread for this host.
	 */
	for (shpnt = scsi_hostlist; shpnt; shpnt = shpnt->next) {
		if (shpnt->hostt == tpnt
		    && shpnt->hostt->use_new_eh_code
		    && shpnt->ehandler != NULL) {
			DECLARE_MUTEX_LOCKED(sem);

			shpnt->eh_notify = &sem;
			send_sig(SIGHUP, shpnt->ehandler, 1);
			down(&sem);
			shpnt->eh_notify = NULL;
		}
	}

	/* Next we free up the Scsi_Cmnd structures for this host */

	for (shpnt = scsi_hostlist; shpnt; shpnt = shpnt->next) {
		if (shpnt->hostt != tpnt) {
			continue;
		}
		for (SDpnt = shpnt->host_queue; SDpnt;
		     SDpnt = shpnt->host_queue) {
			scsi_release_commandblocks(SDpnt);

			blk_cleanup_queue(&SDpnt->request_queue);
			/* Next free up the Scsi_Device structures for this host */
			shpnt->host_queue = SDpnt->next;
			kfree((char *) SDpnt);

		}
	}

	/* Next we go through and remove the instances of the individual hosts
	 * that were detected */

	pcount0 = next_scsi_host;
	for (shpnt = scsi_hostlist; shpnt; shpnt = sh1) {
		sh1 = shpnt->next;
		if (shpnt->hostt != tpnt)
			continue;
		pcount = next_scsi_host;
		/* Remove the /proc/scsi directory entry */
		sprintf(name,"%d",shpnt->host_no);
		remove_proc_entry(name, tpnt->proc_dir);
		if (tpnt->release)
			(*tpnt->release) (shpnt);
		else {
			/* This is the default case for the release function.
			 * It should do the right thing for most correctly
			 * written host adapters.
			 */
			if (shpnt->irq)
				free_irq(shpnt->irq, NULL);
			if (shpnt->dma_channel != 0xff)
				free_dma(shpnt->dma_channel);
			if (shpnt->io_port && shpnt->n_io_port)
				release_region(shpnt->io_port, shpnt->n_io_port);
		}
		if (pcount == next_scsi_host)
			scsi_unregister(shpnt);
		tpnt->present--;
	}

	/*
	 * If there are absolutely no more hosts left, it is safe
	 * to completely nuke the DMA pool.  The resize operation will
	 * do the right thing and free everything.
	 */
	if (!scsi_hosts)
		scsi_resize_dma_pool();

	if (pcount0 != next_scsi_host)
		printk(KERN_INFO "scsi : %d host%s left.\n", next_scsi_host,
		       (next_scsi_host == 1) ? "" : "s");

#if defined(USE_STATIC_SCSI_MEMORY)
	printk("SCSI memory: total %ldKb, used %ldKb, free %ldKb.\n",
	       (scsi_memory_upper_value - scsi_memory_lower_value) / 1024,
	       (scsi_init_memory_start - scsi_memory_lower_value) / 1024,
	       (scsi_memory_upper_value - scsi_init_memory_start) / 1024);
#endif

	/*
	 * Remove it from the linked list and /proc if all
	 * hosts were successfully removed (ie preset == 0)
	 */
	if (!tpnt->present) {
		Scsi_Host_Template **SHTp = &scsi_hosts;
		Scsi_Host_Template *SHT;

		while ((SHT = *SHTp) != NULL) {
			if (SHT == tpnt) {
				*SHTp = SHT->next;
				remove_proc_entry(tpnt->proc_name, proc_scsi);
				break;
			}
			SHTp = &SHT->next;
		}
	}
	MOD_DEC_USE_COUNT;

	unlock_kernel();
	return 0;

err_out:
	unlock_kernel();
	return -1;
}

static int scsi_unregister_device(struct Scsi_Device_Template *tpnt);

/*
 * This entry point should be called by a loadable module if it is trying
 * add a high level scsi driver to the system.
 */
static int scsi_register_device_module(struct Scsi_Device_Template *tpnt)
{
	Scsi_Device *SDpnt;
	struct Scsi_Host *shpnt;
	int out_of_space = 0;

	if (tpnt->next)
		return 1;

	scsi_register_device(tpnt);
	/*
	 * First scan the devices that we know about, and see if we notice them.
	 */

	for (shpnt = scsi_hostlist; shpnt; shpnt = shpnt->next) {
		for (SDpnt = shpnt->host_queue; SDpnt;
		     SDpnt = SDpnt->next) {
			if (tpnt->detect)
				SDpnt->detected = (*tpnt->detect) (SDpnt);
		}
	}

	/*
	 * If any of the devices would match this driver, then perform the
	 * init function.
	 */
	if (tpnt->init && tpnt->dev_noticed) {
		if ((*tpnt->init) ()) {
			for (shpnt = scsi_hostlist; shpnt;
			     shpnt = shpnt->next) {
				for (SDpnt = shpnt->host_queue; SDpnt;
				     SDpnt = SDpnt->next) {
					SDpnt->detected = 0;
				}
			}
			scsi_deregister_device(tpnt);
			return 1;
		}
	}

	/*
	 * Now actually connect the devices to the new driver.
	 */
	for (shpnt = scsi_hostlist; shpnt; shpnt = shpnt->next) {
		for (SDpnt = shpnt->host_queue; SDpnt;
		     SDpnt = SDpnt->next) {
			SDpnt->attached += SDpnt->detected;
			SDpnt->detected = 0;
			if (tpnt->attach)
				(*tpnt->attach) (SDpnt);
			/*
			 * If this driver attached to the device, and don't have any
			 * command blocks for this device, allocate some.
			 */
			if (SDpnt->attached && SDpnt->has_cmdblocks == 0) {
				SDpnt->online = TRUE;
				scsi_build_commandblocks(SDpnt);
				if (0 == SDpnt->has_cmdblocks)
					out_of_space = 1;
			}
		}
	}

	/*
	 * This does any final handling that is required.
	 */
	if (tpnt->finish && tpnt->nr_dev)
		(*tpnt->finish) ();
	if (!out_of_space)
		scsi_resize_dma_pool();
	MOD_INC_USE_COUNT;

	if (out_of_space) {
		scsi_unregister_device(tpnt);	/* easiest way to clean up?? */
		return 1;
	} else
		return 0;
}

static int scsi_unregister_device(struct Scsi_Device_Template *tpnt)
{
	Scsi_Device *SDpnt;
	struct Scsi_Host *shpnt;

	lock_kernel();
	/*
	 * If we are busy, this is not going to fly.
	 */
	if (GET_USE_COUNT(tpnt->module) != 0)
		goto error_out;

	/*
	 * Next, detach the devices from the driver.
	 */

	for (shpnt = scsi_hostlist; shpnt; shpnt = shpnt->next) {
		for (SDpnt = shpnt->host_queue; SDpnt;
		     SDpnt = SDpnt->next) {
			if (tpnt->detach)
				(*tpnt->detach) (SDpnt);
			if (SDpnt->attached == 0) {
				SDpnt->online = FALSE;

				/*
				 * Nobody is using this device any more.  Free all of the
				 * command structures.
				 */
				scsi_release_commandblocks(SDpnt);
			}
		}
	}
	/*
	 * Extract the template from the linked list.
	 */
	scsi_deregister_device(tpnt);

	MOD_DEC_USE_COUNT;
	unlock_kernel();
	/*
	 * Final cleanup for the driver is done in the driver sources in the
	 * cleanup function.
	 */
	return 0;
error_out:
	unlock_kernel();
	return -1;
}


/* This function should be called by drivers which needs to register
 * with the midlevel scsi system. As of 2.4.0-test9pre3 this is our
 * main device/hosts register function	/mathiasen
 */
int scsi_register_module(int module_type, void *ptr)
{
	switch (module_type) {
	case MODULE_SCSI_HA:
		return scsi_register_host((Scsi_Host_Template *) ptr);

		/* Load upper level device handler of some kind */
	case MODULE_SCSI_DEV:
#ifdef CONFIG_KMOD
		if (scsi_hosts == NULL)
			request_module("scsi_hostadapter");
#endif
		return scsi_register_device_module((struct Scsi_Device_Template *) ptr);
		/* The rest of these are not yet implemented */

		/* Load constants.o */
	case MODULE_SCSI_CONST:

		/* Load specialized ioctl handler for some device.  Intended for
		 * cdroms that have non-SCSI2 audio command sets. */
	case MODULE_SCSI_IOCTL:

	default:
		return 1;
	}
}

/* Reverse the actions taken above
 */
int scsi_unregister_module(int module_type, void *ptr)
{
	int retval = 0;

	switch (module_type) {
	case MODULE_SCSI_HA:
		retval = scsi_unregister_host((Scsi_Host_Template *) ptr);
		break;
	case MODULE_SCSI_DEV:
		retval = scsi_unregister_device((struct Scsi_Device_Template *)ptr);
 		break;
		/* The rest of these are not yet implemented. */
	case MODULE_SCSI_CONST:
	case MODULE_SCSI_IOCTL:
		break;
	default:;
	}
	return retval;
}

#ifdef CONFIG_PROC_FS
/*
 * Function:    scsi_dump_status
 *
 * Purpose:     Brain dump of scsi system, used for problem solving.
 *
 * Arguments:   level - used to indicate level of detail.
 *
 * Notes:       The level isn't used at all yet, but we need to find some way
 *              of sensibly logging varying degrees of information.  A quick one-line
 *              display of each command, plus the status would be most useful.
 *
 *              This does depend upon CONFIG_SCSI_LOGGING - I do want some way of turning
 *              it all off if the user wants a lean and mean kernel.  It would probably
 *              also be useful to allow the user to specify one single host to be dumped.
 *              A second argument to the function would be useful for that purpose.
 *
 *              FIXME - some formatting of the output into tables would be very handy.
 */
static void scsi_dump_status(int level)
{
#ifdef CONFIG_SCSI_LOGGING		/* { */
	int i;
	struct Scsi_Host *shpnt;
	Scsi_Cmnd *SCpnt;
	Scsi_Device *SDpnt;
	printk(KERN_INFO "Dump of scsi host parameters:\n");
	i = 0;
	for (shpnt = scsi_hostlist; shpnt; shpnt = shpnt->next) {
		printk(KERN_INFO " %d %d %d : %d %d\n",
		       shpnt->host_failed,
		       shpnt->host_busy,
		       atomic_read(&shpnt->host_active),
		       shpnt->host_blocked,
		       shpnt->host_self_blocked);
	}

	printk(KERN_INFO "\n\n");
	printk(KERN_INFO "Dump of scsi command parameters:\n");
	for (shpnt = scsi_hostlist; shpnt; shpnt = shpnt->next) {
		printk(KERN_INFO "h:c:t:l (dev sect nsect cnumsec sg) (ret all flg) (to/cmd to ito) cmd snse result\n");
		for (SDpnt = shpnt->host_queue; SDpnt; SDpnt = SDpnt->next) {
			for (SCpnt = SDpnt->device_queue; SCpnt; SCpnt = SCpnt->next) {
				/*  (0) h:c:t:l (dev sect nsect cnumsec sg) (ret all flg) (to/cmd to ito) cmd snse result %d %x      */
				printk(KERN_INFO "(%3d) %2d:%1d:%2d:%2d (%6s %4ld %4ld %4ld %4x %1d) (%1d %1d 0x%2x) (%4d %4d %4d) 0x%2.2x 0x%2.2x 0x%8.8x\n",
				       i++,

				       SCpnt->host->host_no,
				       SCpnt->channel,
				       SCpnt->target,
				       SCpnt->lun,

				       kdevname(SCpnt->request.rq_dev),
				       SCpnt->request.sector,
				       SCpnt->request.nr_sectors,
				       SCpnt->request.current_nr_sectors,
				       SCpnt->request.rq_status,
				       SCpnt->use_sg,

				       SCpnt->retries,
				       SCpnt->allowed,
				       SCpnt->flags,

				       SCpnt->timeout_per_command,
				       SCpnt->timeout,
				       SCpnt->internal_timeout,

				       SCpnt->cmnd[0],
				       SCpnt->sense_buffer[2],
				       SCpnt->result);
			}
		}
	}

	for (shpnt = scsi_hostlist; shpnt; shpnt = shpnt->next) {
		for (SDpnt = shpnt->host_queue; SDpnt; SDpnt = SDpnt->next) {
			/* Now dump the request lists for each block device */
			printk(KERN_INFO "Dump of pending block device requests\n");
			for (i = 0; i < MAX_BLKDEV; i++) {
				struct list_head * queue_head;

				queue_head = &blk_dev[i].request_queue.queue_head;
				if (!list_empty(queue_head)) {
					struct request *req;
					struct list_head * entry;

					printk(KERN_INFO "%d: ", i);
					entry = queue_head->next;
					do {
						req = blkdev_entry_to_request(entry);
						printk("(%s %d %ld %ld %ld) ",
						   kdevname(req->rq_dev),
						       req->cmd,
						       req->sector,
						       req->nr_sectors,
						req->current_nr_sectors);
					} while ((entry = entry->next) != queue_head);
					printk("\n");
				}
			}
		}
	}
#endif	/* CONFIG_SCSI_LOGGING */ /* } */
}
#endif				/* CONFIG_PROC_FS */

static int __init scsi_host_no_init (char *str)
{
    static int next_no = 0;
    char *temp;

    while (str) {
	temp = str;
	while (*temp && (*temp != ':') && (*temp != ','))
	    temp++;
	if (!*temp)
	    temp = NULL;
	else
	    *temp++ = 0;
	scsi_host_no_insert(str, next_no);
	str = temp;
	next_no++;
    }
    return 1;
}

static char *scsihosts;

MODULE_PARM(scsihosts, "s");
MODULE_DESCRIPTION("SCSI core");
MODULE_LICENSE("GPL");

#ifndef MODULE
int __init scsi_setup(char *str)
{
	scsihosts = str;
	return 1;
}

__setup("scsihosts=", scsi_setup);
#endif

static int __init init_scsi(void)
{
	struct proc_dir_entry *generic;

	printk(KERN_INFO "SCSI subsystem driver " REVISION "\n");

        if( scsi_init_minimal_dma_pool() != 0 )
        {
                return 1;
        }

	/*
	 * This makes /proc/scsi and /proc/scsi/scsi visible.
	 */
#ifdef CONFIG_PROC_FS
	proc_scsi = proc_mkdir("scsi", 0);
	if (!proc_scsi) {
		printk (KERN_ERR "cannot init /proc/scsi\n");
		return -ENOMEM;
	}
	generic = create_proc_info_entry ("scsi/scsi", 0, 0, scsi_proc_info);
	if (!generic) {
		printk (KERN_ERR "cannot init /proc/scsi/scsi\n");
		remove_proc_entry("scsi", 0);
		return -ENOMEM;
	}
	generic->write_proc = proc_scsi_gen_write;
#endif

        scsi_devfs_handle = devfs_mk_dir (NULL, "scsi", NULL);
        if (scsihosts)
		printk(KERN_INFO "scsi: host order: %s\n", scsihosts);	
	scsi_host_no_init (scsihosts);
	/*
	 * This is where the processing takes place for most everything
	 * when commands are completed.
	 */
	init_bh(SCSI_BH, scsi_bottom_half_handler);

	return 0;
}

static void __exit exit_scsi(void)
{
	Scsi_Host_Name *shn, *shn2 = NULL;

	remove_bh(SCSI_BH);

        devfs_unregister (scsi_devfs_handle);
        for (shn = scsi_host_no_list;shn;shn = shn->next) {
		if (shn->name)
			kfree(shn->name);
                if (shn2)
			kfree (shn2);
                shn2 = shn;
        }
        if (shn2)
		kfree (shn2);

#ifdef CONFIG_PROC_FS
	/* No, we're not here anymore. Don't show the /proc/scsi files. */
	remove_proc_entry ("scsi/scsi", 0);
	remove_proc_entry ("scsi", 0);
#endif
	
	/*
	 * Free up the DMA pool.
	 */
	scsi_resize_dma_pool();

}

module_init(init_scsi);
module_exit(exit_scsi);

/*
 * Function:    scsi_get_host_dev()
 *
 * Purpose:     Create a Scsi_Device that points to the host adapter itself.
 *
 * Arguments:   SHpnt   - Host that needs a Scsi_Device
 *
 * Lock status: None assumed.
 *
 * Returns:     The Scsi_Device or NULL
 *
 * Notes:
 */
Scsi_Device * scsi_get_host_dev(struct Scsi_Host * SHpnt)
{
        Scsi_Device * SDpnt;

        /*
         * Attach a single Scsi_Device to the Scsi_Host - this should
         * be made to look like a "pseudo-device" that points to the
         * HA itself.  For the moment, we include it at the head of
         * the host_queue itself - I don't think we want to show this
         * to the HA in select_queue_depths(), as this would probably confuse
         * matters.
         * Note - this device is not accessible from any high-level
         * drivers (including generics), which is probably not
         * optimal.  We can add hooks later to attach 
         */
        SDpnt = (Scsi_Device *) kmalloc(sizeof(Scsi_Device),
                                        GFP_ATOMIC);
        if(SDpnt == NULL)
        	return NULL;
        	
        memset(SDpnt, 0, sizeof(Scsi_Device));

        SDpnt->host = SHpnt;
        SDpnt->id = SHpnt->this_id;
        SDpnt->type = -1;
        SDpnt->queue_depth = 1;
        
	scsi_build_commandblocks(SDpnt);

	scsi_initialize_queue(SDpnt, SHpnt);

	SDpnt->online = TRUE;

        /*
         * Initialize the object that we will use to wait for command blocks.
         */
	init_waitqueue_head(&SDpnt->scpnt_wait);
        return SDpnt;
}

/*
 * Function:    scsi_free_host_dev()
 *
 * Purpose:     Create a Scsi_Device that points to the host adapter itself.
 *
 * Arguments:   SHpnt   - Host that needs a Scsi_Device
 *
 * Lock status: None assumed.
 *
 * Returns:     Nothing
 *
 * Notes:
 */
void scsi_free_host_dev(Scsi_Device * SDpnt)
{
        if( (unsigned char) SDpnt->id != (unsigned char) SDpnt->host->this_id )
        {
                panic("Attempt to delete wrong device\n");
        }

        blk_cleanup_queue(&SDpnt->request_queue);

        /*
         * We only have a single SCpnt attached to this device.  Free
         * it now.
         */
	scsi_release_commandblocks(SDpnt);
        kfree(SDpnt);
}

/*
 * Function:	scsi_reset_provider_done_command
 *
 * Purpose:	Dummy done routine.
 *
 * Notes:	Some low level drivers will call scsi_done and end up here,
 *		others won't bother.
 *		We don't want the bogus command used for the bus/device
 *		reset to find its way into the mid-layer so we intercept
 *		it here.
 */
static void
scsi_reset_provider_done_command(Scsi_Cmnd *SCpnt)
{
}

/*
 * Function:	scsi_reset_provider
 *
 * Purpose:	Send requested reset to a bus or device at any phase.
 *
 * Arguments:	device	- device to send reset to
 *		flag - reset type (see scsi.h)
 *
 * Returns:	SUCCESS/FAILURE.
 *
 * Notes:	This is used by the SCSI Generic driver to provide
 *		Bus/Device reset capability.
 */
int
scsi_reset_provider(Scsi_Device *dev, int flag)
{
	Scsi_Cmnd SC, *SCpnt = &SC;
	int rtn;

	memset(&SCpnt->eh_timeout, 0, sizeof(SCpnt->eh_timeout));
	SCpnt->host                    	= dev->host;
	SCpnt->device                  	= dev;
	SCpnt->target                  	= dev->id;
	SCpnt->lun                     	= dev->lun;
	SCpnt->channel                 	= dev->channel;
	SCpnt->request.rq_status       	= RQ_SCSI_BUSY;
	SCpnt->request.waiting        	= NULL;
	SCpnt->use_sg                  	= 0;
	SCpnt->old_use_sg              	= 0;
	SCpnt->old_cmd_len             	= 0;
	SCpnt->underflow               	= 0;
	SCpnt->transfersize            	= 0;
	SCpnt->resid			= 0;
	SCpnt->serial_number           	= 0;
	SCpnt->serial_number_at_timeout	= 0;
	SCpnt->host_scribble           	= NULL;
	SCpnt->next                    	= NULL;
	SCpnt->state                   	= SCSI_STATE_INITIALIZING;
	SCpnt->owner	     		= SCSI_OWNER_MIDLEVEL;
    
	memset(&SCpnt->cmnd, '\0', sizeof(SCpnt->cmnd));
    
	SCpnt->scsi_done		= scsi_reset_provider_done_command;
	SCpnt->done			= NULL;
	SCpnt->reset_chain		= NULL;
        
	SCpnt->buffer			= NULL;
	SCpnt->bufflen			= 0;
	SCpnt->request_buffer		= NULL;
	SCpnt->request_bufflen		= 0;

	SCpnt->internal_timeout		= NORMAL_TIMEOUT;
	SCpnt->abort_reason		= DID_ABORT;

	SCpnt->cmd_len			= 0;

	SCpnt->sc_data_direction	= SCSI_DATA_UNKNOWN;
	SCpnt->sc_request		= NULL;
	SCpnt->sc_magic			= SCSI_CMND_MAGIC;

	/*
	 * Sometimes the command can get back into the timer chain,
	 * so use the pid as an identifier.
	 */
	SCpnt->pid			= 0;

	if (dev->host->hostt->use_new_eh_code) {
		rtn = scsi_new_reset(SCpnt, flag);
	} else {
		unsigned long flags;

		spin_lock_irqsave(&io_request_lock, flags);
		rtn = scsi_old_reset(SCpnt, flag);
		spin_unlock_irqrestore(&io_request_lock, flags);
	}

	scsi_delete_timer(SCpnt);
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
