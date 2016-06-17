/*
 * Core I2O structure management 
 * 
 * (C) Copyright 1999   Red Hat Software 
 *
 * Written by Alan Cox, Building Number Three Ltd 
 * 
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License 
 * as published by the Free Software Foundation; either version 
 * 2 of the License, or (at your option) any later version.  
 * 
 * A lot of the I2O message side code from this is taken from the 
 * Red Creek RCPCI45 adapter driver by Red Creek Communications 
 * 
 * Fixes by: 
 *		Philipp Rumpf 
 *		Juha Sievänen <Juha.Sievanen@cs.Helsinki.FI> 
 *		Auvo Häkkinen <Auvo.Hakkinen@cs.Helsinki.FI> 
 *		Deepak Saxena <deepak@plexity.net> 
 *		Boji T Kannanthanam <boji.t.kannanthanam@intel.com>
 * 
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>

#include <linux/i2o.h>

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/smp_lock.h>

#include <linux/bitops.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/tqueue.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <asm/semaphore.h>
#include <linux/completion.h>

#include <asm/io.h>
#include <linux/reboot.h>

#include "i2o_lan.h"

//#define DRIVERDEBUG

#ifdef DRIVERDEBUG
#define dprintk(s, args...) printk(s, ## args)
#else
#define dprintk(s, args...)
#endif

/* OSM table */
static struct i2o_handler *i2o_handlers[MAX_I2O_MODULES];

/* Controller list */
static struct i2o_controller *i2o_controllers[MAX_I2O_CONTROLLERS];
struct i2o_controller *i2o_controller_chain;
int i2o_num_controllers;

/* Initiator Context for Core message */
static int core_context;

/* Initialization && shutdown functions */
void i2o_sys_init(void);
static void i2o_sys_shutdown(void);
static int i2o_reset_controller(struct i2o_controller *);
static int i2o_reboot_event(struct notifier_block *, unsigned long , void *);
static int i2o_online_controller(struct i2o_controller *);
static int i2o_init_outbound_q(struct i2o_controller *);
static int i2o_post_outbound_messages(struct i2o_controller *);

/* Reply handler */
static void i2o_core_reply(struct i2o_handler *, struct i2o_controller *,
			   struct i2o_message *);

/* Various helper functions */
static int i2o_lct_get(struct i2o_controller *);
static int i2o_lct_notify(struct i2o_controller *);
static int i2o_hrt_get(struct i2o_controller *);

static int i2o_build_sys_table(void);
static int i2o_systab_send(struct i2o_controller *c);

/* I2O core event handler */
static int i2o_core_evt(void *);
static int evt_pid;
static int evt_running;

/* Dynamic LCT update handler */
static int i2o_dyn_lct(void *);

void i2o_report_controller_unit(struct i2o_controller *, struct i2o_device *);

/*
 * I2O System Table.  Contains information about
 * all the IOPs in the system.  Used to inform IOPs
 * about each other's existence.
 *
 * sys_tbl_ver is the CurrentChangeIndicator that is
 * used by IOPs to track changes.
 */
static struct i2o_sys_tbl *sys_tbl;
static int sys_tbl_ind;
static int sys_tbl_len;

/*
 * This spin lock is used to keep a device from being
 * added and deleted concurrently across CPUs or interrupts.
 * This can occur when a user creates a device and immediatelly
 * deletes it before the new_dev_notify() handler is called.
 */
static spinlock_t i2o_dev_lock = SPIN_LOCK_UNLOCKED;

/*
 * Structures and definitions for synchronous message posting.
 * See i2o_post_wait() for description.
 */ 
struct i2o_post_wait_data
{
	int *status;		/* Pointer to status block on caller stack */
	int *complete;		/* Pointer to completion flag on caller stack */
	u32 id;			/* Unique identifier */
	wait_queue_head_t *wq;	/* Wake up for caller (NULL for dead) */
	struct i2o_post_wait_data *next;	/* Chain */
	void *mem[2];		/* Memory blocks to recover on failure path */
};
static struct i2o_post_wait_data *post_wait_queue;
static u32 post_wait_id;	// Unique ID for each post_wait
static spinlock_t post_wait_lock = SPIN_LOCK_UNLOCKED;
static void i2o_post_wait_complete(u32, int);

/* OSM descriptor handler */ 
static struct i2o_handler i2o_core_handler =
{
	(void *)i2o_core_reply,
	NULL,
	NULL,
	NULL,
	"I2O core layer",
	0,
	I2O_CLASS_EXECUTIVE
};

/*
 * Used when queueing a reply to be handled later
 */
 
struct reply_info
{
	struct i2o_controller *iop;
	u32 msg[MSG_FRAME_SIZE];
};
static struct reply_info evt_reply;
static struct reply_info events[I2O_EVT_Q_LEN];
static int evt_in;
static int evt_out;
static int evt_q_len;
#define MODINC(x,y) ((x) = ((x) + 1) % (y))

/*
 * I2O configuration spinlock. This isnt a big deal for contention
 * so we have one only
 */

static DECLARE_MUTEX(i2o_configuration_lock);

/* 
 * Event spinlock.  Used to keep event queue sane and from
 * handling multiple events simultaneously.
 */
static spinlock_t i2o_evt_lock = SPIN_LOCK_UNLOCKED;

/*
 * Semaphore used to synchronize event handling thread with 
 * interrupt handler.
 */
 
static DECLARE_MUTEX(evt_sem);
static DECLARE_COMPLETION(evt_dead);
static DECLARE_WAIT_QUEUE_HEAD(evt_wait);

static struct notifier_block i2o_reboot_notifier =
{
        i2o_reboot_event,
        NULL,
        0
};

/*
 *	Config options
 */

static int verbose;
MODULE_PARM(verbose, "i");

/*
 * I2O Core reply handler
 */
static void i2o_core_reply(struct i2o_handler *h, struct i2o_controller *c,
		    struct i2o_message *m)
{
	u32 *msg=(u32 *)m;
	u32 status;
	u32 context = msg[2];

	if (msg[0] & MSG_FAIL) // Fail bit is set
	{
		u32 *preserved_msg = (u32*)(c->mem_offset + msg[7]);

		i2o_report_status(KERN_INFO, "i2o_core", msg);
		i2o_dump_message(preserved_msg);

		/* If the failed request needs special treatment,
		 * it should be done here. */

                /* Release the preserved msg by resubmitting it as a NOP */

		preserved_msg[0] = cpu_to_le32(THREE_WORD_MSG_SIZE | SGL_OFFSET_0);
		preserved_msg[1] = cpu_to_le32(I2O_CMD_UTIL_NOP << 24 | HOST_TID << 12 | 0);
		preserved_msg[2] = 0;
		i2o_post_message(c, msg[7]);

		/* If reply to i2o_post_wait failed, return causes a timeout */

		return;
	}       

#ifdef DRIVERDEBUG
	i2o_report_status(KERN_INFO, "i2o_core", msg);
#endif

	if(msg[2]&0x80000000)	// Post wait message
	{
		if (msg[4] >> 24)
			status = (msg[4] & 0xFFFF);
		else
			status = I2O_POST_WAIT_OK;
	
		i2o_post_wait_complete(context, status);
		return;
	}

	if(m->function == I2O_CMD_UTIL_EVT_REGISTER)
	{
		memcpy(events[evt_in].msg, msg, (msg[0]>>16)<<2);
		events[evt_in].iop = c;

		spin_lock(&i2o_evt_lock);
		MODINC(evt_in, I2O_EVT_Q_LEN);
		if(evt_q_len == I2O_EVT_Q_LEN)
			MODINC(evt_out, I2O_EVT_Q_LEN);
		else
			evt_q_len++;
		spin_unlock(&i2o_evt_lock);

		up(&evt_sem);
		wake_up_interruptible(&evt_wait);
		return;
	}

	if(m->function == I2O_CMD_LCT_NOTIFY)
	{
		up(&c->lct_sem);
		return;
	}

	/*
	 * If this happens, we want to dump the message to the syslog so
	 * it can be sent back to the card manufacturer by the end user
	 * to aid in debugging.
	 * 
	 */
	printk(KERN_WARNING "%s: Unsolicited message reply sent to core!"
			"Message dumped to syslog\n", 
			c->name);
	i2o_dump_message(msg);

	return;
}

/**
 *	i2o_install_handler - install a message handler
 *	@h: Handler structure
 *
 *	Install an I2O handler - these handle the asynchronous messaging
 *	from the card once it has initialised. If the table of handlers is
 *	full then -ENOSPC is returned. On a success 0 is returned and the
 *	context field is set by the function. The structure is part of the
 *	system from this time onwards. It must not be freed until it has
 *	been uninstalled
 */
 
int i2o_install_handler(struct i2o_handler *h)
{
	int i;
	down(&i2o_configuration_lock);
	for(i=0;i<MAX_I2O_MODULES;i++)
	{
		if(i2o_handlers[i]==NULL)
		{
			h->context = i;
			i2o_handlers[i]=h;
			up(&i2o_configuration_lock);
			return 0;
		}
	}
	up(&i2o_configuration_lock);
	return -ENOSPC;
}

/**
 *	i2o_remove_handler - remove an i2o message handler
 *	@h: handler
 *
 *	Remove a message handler previously installed with i2o_install_handler.
 *	After this function returns the handler object can be freed or re-used
 */
 
int i2o_remove_handler(struct i2o_handler *h)
{
	i2o_handlers[h->context]=NULL;
	return 0;
}
	

/*
 *	Each I2O controller has a chain of devices on it.
 * Each device has a pointer to it's LCT entry to be used
 * for fun purposes.
 */

/**
 *	i2o_install_device	-	attach a device to a controller
 *	@c: controller
 *	@d: device
 * 	
 *	Add a new device to an i2o controller. This can be called from
 *	non interrupt contexts only. It adds the device and marks it as
 *	unclaimed. The device memory becomes part of the kernel and must
 *	be uninstalled before being freed or reused. Zero is returned
 *	on success.
 */
 
int i2o_install_device(struct i2o_controller *c, struct i2o_device *d)
{
	int i;

	down(&i2o_configuration_lock);
	d->controller=c;
	d->owner=NULL;
	d->next=c->devices;
	d->prev=NULL;
	if (c->devices != NULL)
		c->devices->prev=d;
	c->devices=d;
	*d->dev_name = 0;

	for(i = 0; i < I2O_MAX_MANAGERS; i++)
		d->managers[i] = NULL;

	up(&i2o_configuration_lock);
	return 0;
}

/* we need this version to call out of i2o_delete_controller */

int __i2o_delete_device(struct i2o_device *d)
{
	struct i2o_device **p;
	int i;

	p=&(d->controller->devices);

	/*
	 *	Hey we have a driver!
	 * Check to see if the driver wants us to notify it of 
	 * device deletion. If it doesn't we assume that it
	 * is unsafe to delete a device with an owner and 
	 * fail.
	 */
	if(d->owner)
	{
		if(d->owner->dev_del_notify)
		{
			dprintk(KERN_INFO "Device has owner, notifying\n");
			d->owner->dev_del_notify(d->controller, d);
			if(d->owner)
			{
				printk(KERN_WARNING 
					"Driver \"%s\" did not release device!\n", d->owner->name);
				return -EBUSY;
			}
		}
		else
			return -EBUSY;
	}

	/*
	 * Tell any other users who are talking to this device
	 * that it's going away.  We assume that everything works.
	 */
	for(i=0; i < I2O_MAX_MANAGERS; i++)
	{
		if(d->managers[i] && d->managers[i]->dev_del_notify)
			d->managers[i]->dev_del_notify(d->controller, d);
	}
	 			
	while(*p!=NULL)
	{
		if(*p==d)
		{
			/*
			 *	Destroy
			 */
			*p=d->next;
			kfree(d);
			return 0;
		}
		p=&((*p)->next);
	}
	printk(KERN_ERR "i2o_delete_device: passed invalid device.\n");
	return -EINVAL;
}

/**
 *	i2o_delete_device	-	remove an i2o device
 *	@d: device to remove
 *
 *	This function unhooks a device from a controller. The device
 *	will not be unhooked if it has an owner who does not wish to free
 *	it, or if the owner lacks a dev_del_notify function. In that case
 *	-EBUSY is returned. On success 0 is returned. Other errors cause
 *	negative errno values to be returned
 */
 
int i2o_delete_device(struct i2o_device *d)
{
	int ret;

	down(&i2o_configuration_lock);

	/*
	 *	Seek, locate
	 */

	ret = __i2o_delete_device(d);

	up(&i2o_configuration_lock);

	return ret;
}

/**
 *	i2o_install_controller	-	attach a controller
 *	@c: controller
 * 	
 *	Add a new controller to the i2o layer. This can be called from
 *	non interrupt contexts only. It adds the controller and marks it as
 *	unused with no devices. If the tables are full or memory allocations
 *	fail then a negative errno code is returned. On success zero is
 *	returned and the controller is bound to the system. The structure
 *	must not be freed or reused until being uninstalled.
 */
 
int i2o_install_controller(struct i2o_controller *c)
{
	int i;
	down(&i2o_configuration_lock);
	for(i=0;i<MAX_I2O_CONTROLLERS;i++)
	{
		if(i2o_controllers[i]==NULL)
		{
			c->dlct = (i2o_lct*)kmalloc(8192, GFP_KERNEL);
			if(c->dlct==NULL)
			{
				up(&i2o_configuration_lock);
				return -ENOMEM;
			}
			i2o_controllers[i]=c;
			c->devices = NULL;
			c->next=i2o_controller_chain;
			i2o_controller_chain=c;
			c->unit = i;
			c->page_frame = NULL;
			c->hrt = NULL;
			c->lct = NULL;
			c->status_block = NULL;
			sprintf(c->name, "i2o/iop%d", i);
			i2o_num_controllers++;
			init_MUTEX_LOCKED(&c->lct_sem);
			up(&i2o_configuration_lock);
			return 0;
		}
	}
	printk(KERN_ERR "No free i2o controller slots.\n");
	up(&i2o_configuration_lock);
	return -EBUSY;
}

/**
 *	i2o_delete_controller	- delete a controller
 *	@c: controller
 *	
 *	Remove an i2o controller from the system. If the controller or its
 *	devices are busy then -EBUSY is returned. On a failure a negative
 *	errno code is returned. On success zero is returned.
 */
  
int i2o_delete_controller(struct i2o_controller *c)
{
	struct i2o_controller **p;
	int users;
	char name[16];
	int stat;

	dprintk(KERN_INFO "Deleting controller %s\n", c->name);

	/*
	 * Clear event registration as this can cause weird behavior
	 */
	if(c->status_block->iop_state == ADAPTER_STATE_OPERATIONAL)
		i2o_event_register(c, core_context, 0, 0, 0);

	down(&i2o_configuration_lock);
	if((users=atomic_read(&c->users)))
	{
		dprintk(KERN_INFO "I2O: %d users for controller %s\n", users,
			c->name);
		up(&i2o_configuration_lock);
		return -EBUSY;
	}
	while(c->devices)
	{
		if(__i2o_delete_device(c->devices)<0)
		{
			/* Shouldnt happen */
			c->bus_disable(c);
			up(&i2o_configuration_lock);
			return -EBUSY;
		}
	}

	/*
	 * If this is shutdown time, the thread's already been killed
	 */
	if(c->lct_running) {
		stat = kill_proc(c->lct_pid, SIGTERM, 1);
		if(!stat) {
			int count = 10 * 100;
			while(c->lct_running && --count) {
				current->state = TASK_INTERRUPTIBLE;
				schedule_timeout(1);
			}
		
			if(!count)
				printk(KERN_ERR 
					"%s: LCT thread still running!\n", 
					c->name);
		}
	}

	p=&i2o_controller_chain;

	while(*p)
	{
		if(*p==c)
		{
 			/* Ask the IOP to switch to RESET state */
			i2o_reset_controller(c);

			/* Release IRQ */
			c->destructor(c);

			*p=c->next;
			up(&i2o_configuration_lock);

			if(c->page_frame)
			{
				pci_unmap_single(c->pdev, c->page_frame_map, MSG_POOL_SIZE, PCI_DMA_FROMDEVICE);
				kfree(c->page_frame);
			}
			if(c->hrt)
				kfree(c->hrt);
			if(c->lct)
				kfree(c->lct);
			if(c->status_block)
				kfree(c->status_block);
			if(c->dlct)
				kfree(c->dlct);

			i2o_controllers[c->unit]=NULL;
			memcpy(name, c->name, strlen(c->name)+1);
			kfree(c);
			dprintk(KERN_INFO "%s: Deleted from controller chain.\n", name);
			
			i2o_num_controllers--;
			return 0;
		}
		p=&((*p)->next);
	}
	up(&i2o_configuration_lock);
	printk(KERN_ERR "i2o_delete_controller: bad pointer!\n");
	return -ENOENT;
}

/**
 *	i2o_unlock_controller	-	unlock a controller
 *	@c: controller to unlock
 *
 *	Take a lock on an i2o controller. This prevents it being deleted.
 *	i2o controllers are not refcounted so a deletion of an in use device
 *	will fail, not take affect on the last dereference.
 */
 
void i2o_unlock_controller(struct i2o_controller *c)
{
	atomic_dec(&c->users);
}

/**
 *	i2o_find_controller - return a locked controller
 *	@n: controller number
 *
 *	Returns a pointer to the controller object. The controller is locked
 *	on return. NULL is returned if the controller is not found.
 */
 
struct i2o_controller *i2o_find_controller(int n)
{
	struct i2o_controller *c;
	
	if(n<0 || n>=MAX_I2O_CONTROLLERS)
		return NULL;
	
	down(&i2o_configuration_lock);
	c=i2o_controllers[n];
	if(c!=NULL)
		atomic_inc(&c->users);
	up(&i2o_configuration_lock);
	return c;
}

/**
 *	i2o_issue_claim	- claim or release a device
 *	@cmd: command
 *	@c: controller to claim for
 *	@tid: i2o task id
 *	@type: type of claim
 *
 *	Issue I2O UTIL_CLAIM and UTIL_RELEASE messages. The message to be sent
 *	is set by cmd. The tid is the task id of the object to claim and the
 *	type is the claim type (see the i2o standard)
 *
 *	Zero is returned on success.
 */
 
static int i2o_issue_claim(u32 cmd, struct i2o_controller *c, int tid, u32 type)
{
	u32 msg[5];

	msg[0] = FIVE_WORD_MSG_SIZE | SGL_OFFSET_0;
	msg[1] = cmd << 24 | HOST_TID<<12 | tid;
	msg[3] = 0;
	msg[4] = type;
	
	return i2o_post_wait(c, msg, sizeof(msg), 60);
}

/*
 * 	i2o_claim_device - claim a device for use by an OSM
 *	@d: device to claim
 *	@h: handler for this device
 *
 *	Do the leg work to assign a device to a given OSM on Linux. The
 *	kernel updates the internal handler data for the device and then
 *	performs an I2O claim for the device, attempting to claim the
 *	device as primary. If the attempt fails a negative errno code
 *	is returned. On success zero is returned.
 */
 
int i2o_claim_device(struct i2o_device *d, struct i2o_handler *h)
{
	down(&i2o_configuration_lock);
	if (d->owner) {
		printk(KERN_INFO "Device claim called, but dev already owned by %s!",
		       h->name);
		up(&i2o_configuration_lock);
		return -EBUSY;
	}
	d->owner=h;

	if(i2o_issue_claim(I2O_CMD_UTIL_CLAIM ,d->controller,d->lct_data.tid, 
			   I2O_CLAIM_PRIMARY))
	{
		d->owner = NULL;
		return -EBUSY;
	}
	up(&i2o_configuration_lock);
	return 0;
}

/**
 *	i2o_release_device - release a device that the OSM is using
 *	@d: device to claim
 *	@h: handler for this device
 *
 *	Drop a claim by an OSM on a given I2O device. The handler is cleared
 *	and 0 is returned on success.
 *
 *	AC - some devices seem to want to refuse an unclaim until they have
 *	finished internal processing. It makes sense since you don't want a
 *	new device to go reconfiguring the entire system until you are done.
 *	Thus we are prepared to wait briefly.
 */

int i2o_release_device(struct i2o_device *d, struct i2o_handler *h)
{
	int err = 0;
	int tries;

	down(&i2o_configuration_lock);
	if (d->owner != h) {
		printk(KERN_INFO "Claim release called, but not owned by %s!\n",
		       h->name);
		up(&i2o_configuration_lock);
		return -ENOENT;
	}	

	for(tries=0;tries<10;tries++)
	{
		d->owner = NULL;

		/*
		 *	If the controller takes a nonblocking approach to
		 *	releases we have to sleep/poll for a few times.
		 */
		 
		if((err=i2o_issue_claim(I2O_CMD_UTIL_RELEASE, d->controller, d->lct_data.tid, I2O_CLAIM_PRIMARY)) )
		{
			err = -ENXIO;
			current->state = TASK_UNINTERRUPTIBLE;
			schedule_timeout(HZ);
		}
		else
		{
			err=0;
			break;
		}
	}
	up(&i2o_configuration_lock);
	return err;
}

/**
 * 	i2o_device_notify_on	-	Enable deletion notifiers
 *	@d: device for notification
 *	@h: handler to install
 *
 *	Called by OSMs to let the core know that they want to be
 *	notified if the given device is deleted from the system.
 */

int i2o_device_notify_on(struct i2o_device *d, struct i2o_handler *h)
{
	int i;

	if(d->num_managers == I2O_MAX_MANAGERS)
		return -ENOSPC;

	for(i = 0; i < I2O_MAX_MANAGERS; i++)
	{
		if(!d->managers[i])
		{
			d->managers[i] = h;
			break;
		}
	}
	
	d->num_managers++;
	
	return 0;
}

/**
 * 	i2o_device_notify_off	-	Remove deletion notifiers
 *	@d: device for notification
 *	@h: handler to remove
 *
 * Called by OSMs to let the core know that they no longer
 * are interested in the fate of the given device.
 */
int i2o_device_notify_off(struct i2o_device *d, struct i2o_handler *h)
{
	int i;

	for(i=0; i < I2O_MAX_MANAGERS; i++)
	{
		if(d->managers[i] == h)
		{
			d->managers[i] = NULL;
			d->num_managers--;
			return 0;
		}
	}

	return -ENOENT;
}

/**
 *	i2o_event_register	-	register interest in an event
 * 	@c: Controller to register interest with
 *	@tid: I2O task id
 *	@init_context: initiator context to use with this notifier
 *	@tr_context: transaction context to use with this notifier
 *	@evt_mask: mask of events
 *
 *	Create and posts an event registration message to the task. No reply
 *	is waited for, or expected. Errors in posting will be reported.
 */
 
int i2o_event_register(struct i2o_controller *c, u32 tid, 
		u32 init_context, u32 tr_context, u32 evt_mask)
{
	u32 msg[5];	// Not performance critical, so we just 
			// i2o_post_this it instead of building it
			// in IOP memory
	
	msg[0] = FIVE_WORD_MSG_SIZE|SGL_OFFSET_0;
	msg[1] = I2O_CMD_UTIL_EVT_REGISTER<<24 | HOST_TID<<12 | tid;
	msg[2] = init_context;
	msg[3] = tr_context;
	msg[4] = evt_mask;

	return i2o_post_this(c, msg, sizeof(msg));
}

/*
 * 	i2o_event_ack	-	acknowledge an event
 *	@c: controller 
 *	@msg: pointer to the UTIL_EVENT_REGISTER reply we received
 *
 *	We just take a pointer to the original UTIL_EVENT_REGISTER reply
 *	message and change the function code since that's what spec
 *	describes an EventAck message looking like.
 */
 
int i2o_event_ack(struct i2o_controller *c, u32 *msg)
{
	struct i2o_message *m = (struct i2o_message *)msg;

	m->function = I2O_CMD_UTIL_EVT_ACK;

	return i2o_post_wait(c, msg, m->size * 4, 2);
}

/*
 * Core event handler.  Runs as a separate thread and is woken
 * up whenever there is an Executive class event.
 */
static int i2o_core_evt(void *reply_data)
{
	struct reply_info *reply = (struct reply_info *) reply_data;
	u32 *msg = reply->msg;
	struct i2o_controller *c = NULL;
	unsigned long flags;

	lock_kernel();
	daemonize();
	unlock_kernel();

	strcpy(current->comm, "i2oevtd");
	evt_running = 1;

	while(1)
	{
		if(down_interruptible(&evt_sem))
		{
			dprintk(KERN_INFO "I2O event thread dead\n");
			printk("exiting...");
			evt_running = 0;
			complete_and_exit(&evt_dead, 0);
		}

		/* 
		 * Copy the data out of the queue so that we don't have to lock
		 * around the whole function and just around the qlen update
		 */
		spin_lock_irqsave(&i2o_evt_lock, flags);
		memcpy(reply, &events[evt_out], sizeof(struct reply_info));
		MODINC(evt_out, I2O_EVT_Q_LEN);
		evt_q_len--;
		spin_unlock_irqrestore(&i2o_evt_lock, flags);
	
		c = reply->iop;
	 	dprintk(KERN_INFO "I2O IRTOS EVENT: iop%d, event %#10x\n", c->unit, msg[4]);

		/* 
		 * We do not attempt to delete/quiesce/etc. the controller if
		 * some sort of error indidication occurs.  We may want to do
		 * so in the future, but for now we just let the user deal with 
		 * it.  One reason for this is that what to do with an error
		 * or when to send what ærror is not really agreed on, so
		 * we get errors that may not be fatal but just look like they
		 * are...so let the user deal with it.
		 */
		switch(msg[4])
		{
			case I2O_EVT_IND_EXEC_RESOURCE_LIMITS:
				printk(KERN_ERR "%s: Out of resources\n", c->name);
				break;

			case I2O_EVT_IND_EXEC_POWER_FAIL:
				printk(KERN_ERR "%s: Power failure\n", c->name);
				break;

			case I2O_EVT_IND_EXEC_HW_FAIL:
			{
				char *fail[] = 
					{ 
						"Unknown Error",
						"Power Lost",
						"Code Violation",
						"Parity Error",
						"Code Execution Exception",
						"Watchdog Timer Expired" 
					};

				if(msg[5] <= 6)
					printk(KERN_ERR "%s: Hardware Failure: %s\n", 
						c->name, fail[msg[5]]);
				else
					printk(KERN_ERR "%s: Unknown Hardware Failure\n", c->name);

				break;
			}

			/*
		 	 * New device created
		 	 * - Create a new i2o_device entry
		 	 * - Inform all interested drivers about this device's existence
		 	 */
			case I2O_EVT_IND_EXEC_NEW_LCT_ENTRY:
			{
				struct i2o_device *d = (struct i2o_device *)
					kmalloc(sizeof(struct i2o_device), GFP_KERNEL);
				int i;

				if (d == NULL) {
					printk(KERN_EMERG "i2oevtd: out of memory\n");
					break;
				}
				memcpy(&d->lct_data, &msg[5], sizeof(i2o_lct_entry));
	
				d->next = NULL;
				d->controller = c;
				d->flags = 0;
	
				i2o_report_controller_unit(c, d);
				i2o_install_device(c,d);
	
				for(i = 0; i < MAX_I2O_MODULES; i++)
				{
					if(i2o_handlers[i] && 
						i2o_handlers[i]->new_dev_notify &&
						(i2o_handlers[i]->class&d->lct_data.class_id))
						{
						spin_lock(&i2o_dev_lock);
						i2o_handlers[i]->new_dev_notify(c,d);
						spin_unlock(&i2o_dev_lock);
						}
				}
			
				break;
			}
	
			/*
 		 	 * LCT entry for a device has been modified, so update it
		 	 * internally.
		 	 */
			case I2O_EVT_IND_EXEC_MODIFIED_LCT:
			{
				struct i2o_device *d;
				i2o_lct_entry *new_lct = (i2o_lct_entry *)&msg[5];

				for(d = c->devices; d; d = d->next)
				{
					if(d->lct_data.tid == new_lct->tid)
					{
						memcpy(&d->lct_data, new_lct, sizeof(i2o_lct_entry));
						break;
					}
				}
				break;
			}
	
			case I2O_EVT_IND_CONFIGURATION_FLAG:
				printk(KERN_WARNING "%s requires user configuration\n", c->name);
				break;
	
			case I2O_EVT_IND_GENERAL_WARNING:
				printk(KERN_WARNING "%s: Warning notification received!"
					"Check configuration for errors!\n", c->name);
				break;
				
			case I2O_EVT_IND_EVT_MASK_MODIFIED:
				/* Well I guess that was us hey .. */
				break;
					
			default:
				printk(KERN_WARNING "%s: No handler for event (0x%08x)\n", c->name, msg[4]);
				break;
		}
	}

	return 0;
}

/*
 * Dynamic LCT update.  This compares the LCT with the currently
 * installed devices to check for device deletions..this needed b/c there
 * is no DELETED_LCT_ENTRY EventIndicator for the Executive class so
 * we can't just have the event handler do this...annoying
 *
 * This is a hole in the spec that will hopefully be fixed someday.
 */
static int i2o_dyn_lct(void *foo)
{
	struct i2o_controller *c = (struct i2o_controller *)foo;
	struct i2o_device *d = NULL;
	struct i2o_device *d1 = NULL;
	int i = 0;
	int found = 0;
	int entries;
	void *tmp;
	char name[16];

	lock_kernel();
	daemonize();
	unlock_kernel();

	sprintf(name, "iop%d_lctd", c->unit);
	strcpy(current->comm, name);	
	
	c->lct_running = 1;

	while(1)
	{
		down_interruptible(&c->lct_sem);
		if(signal_pending(current))
		{
			dprintk(KERN_ERR "%s: LCT thread dead\n", c->name);
			c->lct_running = 0;
			return 0;
		}

		entries = c->dlct->table_size;
		entries -= 3;
		entries /= 9;

		dprintk(KERN_INFO "%s: Dynamic LCT Update\n",c->name);
		dprintk(KERN_INFO "%s: Dynamic LCT contains %d entries\n", c->name, entries);

		if(!entries)
		{
			printk(KERN_INFO "%s: Empty LCT???\n", c->name);
			continue;
		}

		/*
		 * Loop through all the devices on the IOP looking for their
		 * LCT data in the LCT.  We assume that TIDs are not repeated.
		 * as that is the only way to really tell.  It's been confirmed
		 * by the IRTOS vendor(s?) that TIDs are not reused until they 
		 * wrap arround(4096), and I doubt a system will up long enough
		 * to create/delete that many devices.
		 */
		for(d = c->devices; d; )
		{
			found = 0;
			d1 = d->next;
			
			for(i = 0; i < entries; i++) 
			{ 
				if(d->lct_data.tid == c->dlct->lct_entry[i].tid) 
				{ 
					found = 1; 
					break; 
				} 
			} 
			if(!found) 
			{
				dprintk(KERN_INFO "i2o_core: Deleted device!\n"); 
				spin_lock(&i2o_dev_lock);
				i2o_delete_device(d); 
				spin_unlock(&i2o_dev_lock);
			} 
			d = d1; 
		}

		/* 
		 * Tell LCT to renotify us next time there is a change
	 	 */
		i2o_lct_notify(c);

		/*
		 * Copy new LCT into public LCT
		 *
		 * Possible race if someone is reading LCT while  we are copying 
		 * over it. If this happens, we'll fix it then. but I doubt that
		 * the LCT will get updated often enough or will get read by
		 * a user often enough to worry.
		 */
		if(c->lct->table_size < c->dlct->table_size)
		{
			tmp = c->lct;
			c->lct = kmalloc(c->dlct->table_size<<2, GFP_KERNEL);
			if(!c->lct)
			{
				printk(KERN_ERR "%s: No memory for LCT!\n", c->name);
				c->lct = tmp;
				continue;
			}
			kfree(tmp);
		}
		memcpy(c->lct, c->dlct, c->dlct->table_size<<2);
	}

	return 0;
}

/**
 *	i2o_run_queue	-	process pending events on a controller
 *	@c: controller to process
 *
 *	This is called by the bus specific driver layer when an interrupt
 *	or poll of this card interface is desired.
 */
 
void i2o_run_queue(struct i2o_controller *c)
{
	struct i2o_message *m;
	u32 mv;
	u32 *msg;

	/*
	 * Old 960 steppings had a bug in the I2O unit that caused
	 * the queue to appear empty when it wasn't.
	 */
	if((mv=I2O_REPLY_READ32(c))==0xFFFFFFFF)
		mv=I2O_REPLY_READ32(c);

	while(mv!=0xFFFFFFFF)
	{
		struct i2o_handler *i;
		/* Map the message from the page frame map to kernel virtual */
		/* m=(struct i2o_message *)(mv - (unsigned long)c->page_frame_map + (unsigned long)c->page_frame); */
		m=(struct i2o_message *)bus_to_virt(mv);
		msg=(u32*)m;

		/*
	 	 *	Ensure this message is seen coherently but cachably by
		 *	the processor 
	 	 */

		pci_dma_sync_single(c->pdev, c->page_frame_map, MSG_FRAME_SIZE, PCI_DMA_FROMDEVICE);
	
		/*
		 *	Despatch it
	 	 */

		i=i2o_handlers[m->initiator_context&(MAX_I2O_MODULES-1)];
		if(i && i->reply)
			i->reply(i,c,m);
		else
		{
			printk(KERN_WARNING "I2O: Spurious reply to handler %d\n", 
				m->initiator_context&(MAX_I2O_MODULES-1));
		}	
	 	i2o_flush_reply(c,mv);
		mb();

		/* That 960 bug again... */	
		if((mv=I2O_REPLY_READ32(c))==0xFFFFFFFF)
			mv=I2O_REPLY_READ32(c);
	}		
}


/**
 *	i2o_get_class_name - 	do i2o class name lookup
 *	@class: class number
 *
 *	Return a descriptive string for an i2o class
 */
 
const char *i2o_get_class_name(int class)
{
	int idx = 16;
	static char *i2o_class_name[] = {
		"Executive",
		"Device Driver Module",
		"Block Device",
		"Tape Device",
		"LAN Interface",
		"WAN Interface",
		"Fibre Channel Port",
		"Fibre Channel Device",
		"SCSI Device",
		"ATE Port",
		"ATE Device",
		"Floppy Controller",
		"Floppy Device",
		"Secondary Bus Port",
		"Peer Transport Agent",
		"Peer Transport",
		"Unknown"
	};
	
	switch(class&0xFFF)
	{
		case I2O_CLASS_EXECUTIVE:
			idx = 0; break;
		case I2O_CLASS_DDM:
			idx = 1; break;
		case I2O_CLASS_RANDOM_BLOCK_STORAGE:
			idx = 2; break;
		case I2O_CLASS_SEQUENTIAL_STORAGE:
			idx = 3; break;
		case I2O_CLASS_LAN:
			idx = 4; break;
		case I2O_CLASS_WAN:
			idx = 5; break;
		case I2O_CLASS_FIBRE_CHANNEL_PORT:
			idx = 6; break;
		case I2O_CLASS_FIBRE_CHANNEL_PERIPHERAL:
			idx = 7; break;
		case I2O_CLASS_SCSI_PERIPHERAL:
			idx = 8; break;
		case I2O_CLASS_ATE_PORT:
			idx = 9; break;
		case I2O_CLASS_ATE_PERIPHERAL:
			idx = 10; break;
		case I2O_CLASS_FLOPPY_CONTROLLER:
			idx = 11; break;
		case I2O_CLASS_FLOPPY_DEVICE:
			idx = 12; break;
		case I2O_CLASS_BUS_ADAPTER_PORT:
			idx = 13; break;
		case I2O_CLASS_PEER_TRANSPORT_AGENT:
			idx = 14; break;
		case I2O_CLASS_PEER_TRANSPORT:
			idx = 15; break;
	}

	return i2o_class_name[idx];
}


/**
 *	i2o_wait_message	-	obtain an i2o message from the IOP
 *	@c: controller
 *	@why: explanation 
 *
 *	This function waits up to 5 seconds for a message slot to be
 *	available. If no message is available it prints an error message
 *	that is expected to be what the message will be used for (eg
 *	"get_status"). 0xFFFFFFFF is returned on a failure.
 *
 *	On a success the message is returned. This is the physical page
 *	frame offset address from the read port. (See the i2o spec)
 */
 
u32 i2o_wait_message(struct i2o_controller *c, char *why)
{
	long time=jiffies;
	u32 m;
	while((m=I2O_POST_READ32(c))==0xFFFFFFFF)
	{
		if((jiffies-time)>=5*HZ)
		{
			dprintk(KERN_ERR "%s: Timeout waiting for message frame to send %s.\n", 
				c->name, why);
			return 0xFFFFFFFF;
		}
		schedule();
		barrier();
	}
	return m;
}
	
/**
 *	i2o_report_controller_unit - print information about a tid
 *	@c: controller
 *	@d: device
 *	
 *	Dump an information block associated with a given unit (TID). The
 *	tables are read and a block of text is output to printk that is
 *	formatted intended for the user.
 */
 
void i2o_report_controller_unit(struct i2o_controller *c, struct i2o_device *d)
{
	char buf[64];
	char str[22];
	int ret;
	int unit = d->lct_data.tid;

	if(verbose==0)
		return;
		
	printk(KERN_INFO "Target ID %d.\n", unit);
	if((ret=i2o_query_scalar(c, unit, 0xF100, 3, buf, 16))>=0)
	{
		buf[16]=0;
		printk(KERN_INFO "     Vendor: %s\n", buf);
	}
	if((ret=i2o_query_scalar(c, unit, 0xF100, 4, buf, 16))>=0)
	{
		buf[16]=0;
		printk(KERN_INFO "     Device: %s\n", buf);
	}
	if(i2o_query_scalar(c, unit, 0xF100, 5, buf, 16)>=0)
	{
		buf[16]=0;
		printk(KERN_INFO "     Description: %s\n", buf);
	}
	if((ret=i2o_query_scalar(c, unit, 0xF100, 6, buf, 8))>=0)
	{
		buf[8]=0;
		printk(KERN_INFO "        Rev: %s\n", buf);
	}

	printk(KERN_INFO "    Class: ");
	sprintf(str, "%-21s", i2o_get_class_name(d->lct_data.class_id));
	printk("%s\n", str);
		
	printk(KERN_INFO "  Subclass: 0x%04X\n", d->lct_data.sub_class);
	printk(KERN_INFO "     Flags: ");
		
	if(d->lct_data.device_flags&(1<<0))
		printk("C");		// ConfigDialog requested
	if(d->lct_data.device_flags&(1<<1))
		printk("U");		// Multi-user capable
	if(!(d->lct_data.device_flags&(1<<4)))
		printk("P");		// Peer service enabled!
	if(!(d->lct_data.device_flags&(1<<5)))
		printk("M");		// Mgmt service enabled!
	printk("\n");
			
}


/*
 *	Parse the hardware resource table. Right now we print it out
 *	and don't do a lot with it. We should collate these and then
 *	interact with the Linux resource allocation block.
 *
 *	Lets prove we can read it first eh ?
 *
 *	This is full of endianisms!
 */
 
static int i2o_parse_hrt(struct i2o_controller *c)
{
#ifdef DRIVERDEBUG
	u32 *rows=(u32*)c->hrt;
	u8 *p=(u8 *)c->hrt;
	u8 *d;
	int count;
	int length;
	int i;
	int state;
	
	if(p[3]!=0)
	{
		printk(KERN_ERR "%s: HRT table for controller is too new a version.\n",
			c->name);
		return -1;
	}
		
	count=p[0]|(p[1]<<8);
	length = p[2];
	
	printk(KERN_INFO "%s: HRT has %d entries of %d bytes each.\n",
		c->name, count, length<<2);

	rows+=2;
	
	for(i=0;i<count;i++)
	{
		printk(KERN_INFO "Adapter %08X: ", rows[0]);
		p=(u8 *)(rows+1);
		d=(u8 *)(rows+2);
		state=p[1]<<8|p[0];
		
		printk("TID %04X:[", state&0xFFF);
		state>>=12;
		if(state&(1<<0))
			printk("H");		/* Hidden */
		if(state&(1<<2))
		{
			printk("P");		/* Present */
			if(state&(1<<1))
				printk("C");	/* Controlled */
		}
		if(state>9)
			printk("*");		/* Hard */
		
		printk("]:");
		
		switch(p[3]&0xFFFF)
		{
			case 0:
				/* Adapter private bus - easy */
				printk("Local bus %d: I/O at 0x%04X Mem 0x%08X", 
					p[2], d[1]<<8|d[0], *(u32 *)(d+4));
				break;
			case 1:
				/* ISA bus */
				printk("ISA %d: CSN %d I/O at 0x%04X Mem 0x%08X",
					p[2], d[2], d[1]<<8|d[0], *(u32 *)(d+4));
				break;
					
			case 2: /* EISA bus */
				printk("EISA %d: Slot %d I/O at 0x%04X Mem 0x%08X",
					p[2], d[3], d[1]<<8|d[0], *(u32 *)(d+4));
				break;

			case 3: /* MCA bus */
				printk("MCA %d: Slot %d I/O at 0x%04X Mem 0x%08X",
					p[2], d[3], d[1]<<8|d[0], *(u32 *)(d+4));
				break;

			case 4: /* PCI bus */
				printk("PCI %d: Bus %d Device %d Function %d",
					p[2], d[2], d[1], d[0]);
				break;

			case 0x80: /* Other */
			default:
				printk("Unsupported bus type.");
				break;
		}
		printk("\n");
		rows+=length;
	}
#endif
	return 0;
}
	
/*
 *	The logical configuration table tells us what we can talk to
 *	on the board. Most of the stuff isn't interesting to us. 
 */

static int i2o_parse_lct(struct i2o_controller *c)
{
	int i;
	int max;
	int tid;
	struct i2o_device *d;
	i2o_lct *lct = c->lct;

	if (lct == NULL) {
		printk(KERN_ERR "%s: LCT is empty???\n", c->name);
		return -1;
	}

	max = lct->table_size;
	max -= 3;
	max /= 9;
	
	printk(KERN_INFO "%s: LCT has %d entries.\n", c->name, max);
	
	if(lct->iop_flags&(1<<0))
		printk(KERN_WARNING "%s: Configuration dialog desired.\n", c->name);
		
	for(i=0;i<max;i++)
	{
		d = (struct i2o_device *)kmalloc(sizeof(struct i2o_device), GFP_KERNEL);
		if(d==NULL)
		{
			printk(KERN_CRIT "i2o_core: Out of memory for I2O device data.\n");
			return -ENOMEM;
		}
		
		d->controller = c;
		d->next = NULL;

		memcpy(&d->lct_data, &lct->lct_entry[i], sizeof(i2o_lct_entry));

		d->flags = 0;
		tid = d->lct_data.tid;
		
		i2o_report_controller_unit(c, d);
		
		i2o_install_device(c, d);
	}
	return 0;
}


/**
 *	i2o_quiesce_controller - quiesce controller
 *	@c: controller 
 *
 *	Quiesce an IOP. Causes IOP to make external operation quiescent
 *	(i2o 'READY' state). Internal operation of the IOP continues normally.
 */
 
int i2o_quiesce_controller(struct i2o_controller *c)
{
	u32 msg[4];
	int ret;

	i2o_status_get(c);

	/* SysQuiesce discarded if IOP not in READY or OPERATIONAL state */

	if ((c->status_block->iop_state != ADAPTER_STATE_READY) &&
		(c->status_block->iop_state != ADAPTER_STATE_OPERATIONAL))
	{
		return 0;
	}

	msg[0] = FOUR_WORD_MSG_SIZE|SGL_OFFSET_0;
	msg[1] = I2O_CMD_SYS_QUIESCE<<24|HOST_TID<<12|ADAPTER_TID;
	msg[3] = 0;

	/* Long timeout needed for quiesce if lots of devices */

	if ((ret = i2o_post_wait(c, msg, sizeof(msg), 240)))
		printk(KERN_INFO "%s: Unable to quiesce (status=%#x).\n",
			c->name, -ret);
	else
		dprintk(KERN_INFO "%s: Quiesced.\n", c->name);

	i2o_status_get(c); // Entered READY state
	return ret;
}

/**
 *	i2o_enable_controller - move controller from ready to operational
 *	@c: controller
 *
 *	Enable IOP. This allows the IOP to resume external operations and
 *	reverses the effect of a quiesce. In the event of an error a negative
 *	errno code is returned.
 */
 
int i2o_enable_controller(struct i2o_controller *c)
{
	u32 msg[4];
	int ret;

	i2o_status_get(c);
	
	/* Enable only allowed on READY state */	
	if(c->status_block->iop_state != ADAPTER_STATE_READY)
		return -EINVAL;

	msg[0]=FOUR_WORD_MSG_SIZE|SGL_OFFSET_0;
	msg[1]=I2O_CMD_SYS_ENABLE<<24|HOST_TID<<12|ADAPTER_TID;

	/* How long of a timeout do we need? */

	if ((ret = i2o_post_wait(c, msg, sizeof(msg), 240)))
		printk(KERN_ERR "%s: Could not enable (status=%#x).\n",
			c->name, -ret);
	else
		dprintk(KERN_INFO "%s: Enabled.\n", c->name);

	i2o_status_get(c); // entered OPERATIONAL state

	return ret;
}

/**
 *	i2o_clear_controller	-	clear a controller
 *	@c: controller
 *
 *	Clear an IOP to HOLD state, ie. terminate external operations, clear all
 *	input queues and prepare for a system restart. IOP's internal operation
 *	continues normally and the outbound queue is alive.
 *	The IOP is not expected to rebuild its LCT.
 */
 
int i2o_clear_controller(struct i2o_controller *c)
{
	struct i2o_controller *iop;
	u32 msg[4];
	int ret;

	/* Quiesce all IOPs first */

	for (iop = i2o_controller_chain; iop; iop = iop->next)
		i2o_quiesce_controller(iop);

	msg[0]=FOUR_WORD_MSG_SIZE|SGL_OFFSET_0;
	msg[1]=I2O_CMD_ADAPTER_CLEAR<<24|HOST_TID<<12|ADAPTER_TID;
	msg[3]=0;

	if ((ret=i2o_post_wait(c, msg, sizeof(msg), 30)))
		printk(KERN_INFO "%s: Unable to clear (status=%#x).\n",
			c->name, -ret);
	else
		dprintk(KERN_INFO "%s: Cleared.\n",c->name);

	i2o_status_get(c);

	/* Enable other IOPs */

	for (iop = i2o_controller_chain; iop; iop = iop->next)
		if (iop != c)
			i2o_enable_controller(iop);

	return ret;
}


/**
 *	i2o_reset_controller	-	reset an IOP
 *	@c: controller to reset
 *
 *	Reset the IOP into INIT state and wait until IOP gets into RESET state.
 *	Terminate all external operations, clear IOP's inbound and outbound
 *	queues, terminate all DDMs, and reload the IOP's operating environment
 *	and all local DDMs. The IOP rebuilds its LCT.
 */
 
static int i2o_reset_controller(struct i2o_controller *c)
{
	struct i2o_controller *iop;
	u32 m;
	u8 *status;
	u32 *msg;
	long time;

	/* Quiesce all IOPs first */

	for (iop = i2o_controller_chain; iop; iop = iop->next)
	{
		if(iop->type != I2O_TYPE_PCI || !iop->bus.pci.dpt)
			i2o_quiesce_controller(iop);
	}

	m=i2o_wait_message(c, "AdapterReset");
	if(m==0xFFFFFFFF)	
		return -ETIMEDOUT;
	msg=(u32 *)(c->mem_offset+m);
	
	status=(void *)kmalloc(4, GFP_KERNEL);
	if(status==NULL) {
		printk(KERN_ERR "IOP reset failed - no free memory.\n");
		return -ENOMEM;
	}
	memset(status, 0, 4);
	
	msg[0]=EIGHT_WORD_MSG_SIZE|SGL_OFFSET_0;
	msg[1]=I2O_CMD_ADAPTER_RESET<<24|HOST_TID<<12|ADAPTER_TID;
	msg[2]=core_context;
	msg[3]=0;
	msg[4]=0;
	msg[5]=0;
	msg[6]=virt_to_bus(status);
	msg[7]=0;	/* 64bit host FIXME */

	i2o_post_message(c,m);

	/* Wait for a reply */
	time=jiffies;
	while(*status==0)
	{
		if((jiffies-time)>=20*HZ)
		{
			printk(KERN_ERR "IOP reset timeout.\n");
			// Better to leak this for safety: kfree(status);
			return -ETIMEDOUT;
		}
		schedule();
		barrier();
	}

	if (*status==I2O_CMD_IN_PROGRESS)
	{ 
		/* 
		 * Once the reset is sent, the IOP goes into the INIT state 
		 * which is indeterminate.  We need to wait until the IOP 
		 * has rebooted before we can let the system talk to 
		 * it. We read the inbound Free_List until a message is 
		 * available.  If we can't read one in the given ammount of 
		 * time, we assume the IOP could not reboot properly.  
		 */ 

		dprintk(KERN_INFO "%s: Reset in progress, waiting for reboot...\n",
			c->name); 

		time = jiffies; 
		m = I2O_POST_READ32(c); 
		while(m == 0XFFFFFFFF) 
		{ 
			if((jiffies-time) >= 30*HZ)
			{
				printk(KERN_ERR "%s: Timeout waiting for IOP reset.\n", 
						c->name); 
				return -ETIMEDOUT; 
			} 
			schedule(); 
			barrier(); 
			m = I2O_POST_READ32(c); 
		}
		i2o_flush_reply(c,m);
	}

	/* If IopReset was rejected or didn't perform reset, try IopClear */

	i2o_status_get(c);
	if (status[0] == I2O_CMD_REJECTED || 
		c->status_block->iop_state != ADAPTER_STATE_RESET)
	{
		printk(KERN_WARNING "%s: Reset rejected, trying to clear\n",c->name);
		i2o_clear_controller(c);
	}
	else
		dprintk(KERN_INFO "%s: Reset completed.\n", c->name);

	/* Enable other IOPs */

	for (iop = i2o_controller_chain; iop; iop = iop->next)
		if (iop != c)
			i2o_enable_controller(iop);

	kfree(status);
	return 0;
}


/**
 * 	i2o_status_get	-	get the status block for the IOP
 *	@c: controller
 *
 *	Issue a status query on the controller. This updates the
 *	attached status_block. If the controller fails to reply or an
 *	error occurs then a negative errno code is returned. On success
 *	zero is returned and the status_blok is updated.
 */
 
int i2o_status_get(struct i2o_controller *c)
{
	long time;
	u32 m;
	u32 *msg;
	u8 *status_block;

	if (c->status_block == NULL) 
	{
		c->status_block = (i2o_status_block *)
			kmalloc(sizeof(i2o_status_block),GFP_KERNEL);
		if (c->status_block == NULL)
		{
			printk(KERN_CRIT "%s: Get Status Block failed; Out of memory.\n",
				c->name);
			return -ENOMEM;
		}
	}

	status_block = (u8*)c->status_block;
	memset(c->status_block,0,sizeof(i2o_status_block));
	
	m=i2o_wait_message(c, "StatusGet");
	if(m==0xFFFFFFFF)
		return -ETIMEDOUT;	
	msg=(u32 *)(c->mem_offset+m);

	msg[0]=NINE_WORD_MSG_SIZE|SGL_OFFSET_0;
	msg[1]=I2O_CMD_STATUS_GET<<24|HOST_TID<<12|ADAPTER_TID;
	msg[2]=core_context;
	msg[3]=0;
	msg[4]=0;
	msg[5]=0;
	msg[6]=virt_to_bus(c->status_block);
	msg[7]=0;   /* 64bit host FIXME */
	msg[8]=sizeof(i2o_status_block); /* always 88 bytes */

	i2o_post_message(c,m);

	/* Wait for a reply */

	time=jiffies;
	while(status_block[87]!=0xFF)
	{
		if((jiffies-time)>=5*HZ)
		{
			printk(KERN_ERR "%s: Get status timeout.\n",c->name);
			return -ETIMEDOUT;
		}
		schedule();
		barrier();
	}

#ifdef DRIVERDEBUG
	printk(KERN_INFO "%s: State = ", c->name);
	switch (c->status_block->iop_state) {
		case 0x01:  
			printk("INIT\n");
			break;
		case 0x02:
			printk("RESET\n");
			break;
		case 0x04:
			printk("HOLD\n");
			break;
		case 0x05:
			printk("READY\n");
			break;
		case 0x08:
			printk("OPERATIONAL\n");
			break;
		case 0x10:
			printk("FAILED\n");
			break;
		case 0x11:
			printk("FAULTED\n");
			break;
		default: 
			printk("%x (unknown !!)\n",c->status_block->iop_state);
}     
#endif   

	return 0;
}

/*
 * Get the Hardware Resource Table for the device.
 * The HRT contains information about possible hidden devices
 * but is mostly useless to us 
 */
int i2o_hrt_get(struct i2o_controller *c)
{
	u32 msg[6];
	int ret, size = sizeof(i2o_hrt);

	/* First read just the header to figure out the real size */

	do  {
		if (c->hrt == NULL) {
			c->hrt=kmalloc(size, GFP_KERNEL);
			if (c->hrt == NULL) {
				printk(KERN_CRIT "%s: Hrt Get failed; Out of memory.\n", c->name);
				return -ENOMEM;
			}
		}

		msg[0]= SIX_WORD_MSG_SIZE| SGL_OFFSET_4;
		msg[1]= I2O_CMD_HRT_GET<<24 | HOST_TID<<12 | ADAPTER_TID;
		msg[3]= 0;
		msg[4]= (0xD0000000 | size);	/* Simple transaction */
		msg[5]= virt_to_bus(c->hrt);	/* Dump it here */

		ret = i2o_post_wait_mem(c, msg, sizeof(msg), 20, c->hrt, NULL);
		
		if(ret == -ETIMEDOUT)
		{
			/* The HRT block we used is in limbo somewhere. When the iop wakes up
			   we will recover it */
			c->hrt = NULL;
			return ret;
		}
		
		if(ret<0)
		{
			printk(KERN_ERR "%s: Unable to get HRT (status=%#x)\n",
				c->name, -ret);	
			return ret;
		}

		if (c->hrt->num_entries * c->hrt->entry_len << 2 > size) {
			size = c->hrt->num_entries * c->hrt->entry_len << 2;
			kfree(c->hrt);
			c->hrt = NULL;
		}
	} while (c->hrt == NULL);

	i2o_parse_hrt(c); // just for debugging

	return 0;
}

/*
 * Send the I2O System Table to the specified IOP
 *
 * The system table contains information about all the IOPs in the
 * system.  It is build and then sent to each IOP so that IOPs can
 * establish connections between each other.
 *
 */
static int i2o_systab_send(struct i2o_controller *iop)
{
	u32 msg[12];
	int ret;
	u32 *privbuf = kmalloc(16, GFP_KERNEL);
	if(privbuf == NULL)
		return -ENOMEM;
	
	if(iop->type == I2O_TYPE_PCI)
	{
		struct resource *root;
		
		if(iop->status_block->current_mem_size < iop->status_block->desired_mem_size)
		{
			struct resource *res = &iop->mem_resource;
			res->name = iop->pdev->bus->name;
			res->flags = IORESOURCE_MEM;
			res->start = 0;
			res->end = 0;
			printk("%s: requires private memory resources.\n", iop->name);
			root = pci_find_parent_resource(iop->pdev, res);
			if(root==NULL)
				printk("Can't find parent resource!\n");
			if(root && allocate_resource(root, res, 
					iop->status_block->desired_mem_size,
					iop->status_block->desired_mem_size,
					iop->status_block->desired_mem_size,
					1<<20,	/* Unspecified, so use 1Mb and play safe */
					NULL,
					NULL)>=0)
			{
				iop->mem_alloc = 1;
				iop->status_block->current_mem_size = 1 + res->end - res->start;
				iop->status_block->current_mem_base = res->start;
				printk(KERN_INFO "%s: allocated %ld bytes of PCI memory at 0x%08lX.\n", 
					iop->name, 1+res->end-res->start, res->start);
			}
		}
		if(iop->status_block->current_io_size < iop->status_block->desired_io_size)
		{
			struct resource *res = &iop->io_resource;
			res->name = iop->pdev->bus->name;
			res->flags = IORESOURCE_IO;
			res->start = 0;
			res->end = 0;
			printk("%s: requires private memory resources.\n", iop->name);
			root = pci_find_parent_resource(iop->pdev, res);
			if(root==NULL)
				printk("Can't find parent resource!\n");
			if(root &&  allocate_resource(root, res, 
					iop->status_block->desired_io_size,
					iop->status_block->desired_io_size,
					iop->status_block->desired_io_size,
					1<<20,	/* Unspecified, so use 1Mb and play safe */
					NULL,
					NULL)>=0)
			{
				iop->io_alloc = 1;
				iop->status_block->current_io_size = 1 + res->end - res->start;
				iop->status_block->current_mem_base = res->start;
				printk(KERN_INFO "%s: allocated %ld bytes of PCI I/O at 0x%08lX.\n", 
					iop->name, 1+res->end-res->start, res->start);
			}
		}
	}
	else
	{	
		privbuf[0] = iop->status_block->current_mem_base;
		privbuf[1] = iop->status_block->current_mem_size;
		privbuf[2] = iop->status_block->current_io_base;
		privbuf[3] = iop->status_block->current_io_size;
	}

	msg[0] = I2O_MESSAGE_SIZE(12) | SGL_OFFSET_6;
	msg[1] = I2O_CMD_SYS_TAB_SET<<24 | HOST_TID<<12 | ADAPTER_TID;
	msg[3] = 0;
	msg[4] = (0<<16) | ((iop->unit+2) );      /* Host 0 IOP ID (unit + 2) */
	msg[5] = 0;                               /* Segment 0 */

	/* 
 	 * Provide three SGL-elements:
 	 * System table (SysTab), Private memory space declaration and 
 	 * Private i/o space declaration  
 	 *
 	 * FIXME: provide these for controllers needing them
 	 */
	msg[6] = 0x54000000 | sys_tbl_len;
	msg[7] = virt_to_bus(sys_tbl);
	msg[8] = 0x54000000 | privbuf[1];
	msg[9] = privbuf[0];
	msg[10] = 0xD4000000 | privbuf[3];
	msg[11] = privbuf[2];

	ret=i2o_post_wait_mem(iop, msg, sizeof(msg), 120, privbuf, NULL);
	
	if(ret==-ETIMEDOUT)
	{
		printk(KERN_ERR "%s: SysTab setup timed out.\n", iop->name);
	}
	else if(ret<0)
	{
		printk(KERN_ERR "%s: Unable to set SysTab (status=%#x).\n", 
			iop->name, -ret);
		kfree(privbuf);
	}
	else
	{
		dprintk(KERN_INFO "%s: SysTab set.\n", iop->name);
		kfree(privbuf);
	}
	i2o_status_get(iop); // Entered READY state

	return ret;	

 }

/*
 * Initialize I2O subsystem.
 */
void __init i2o_sys_init(void)
{
	struct i2o_controller *iop, *niop = NULL;

	printk(KERN_INFO "Activating I2O controllers...\n");
	printk(KERN_INFO "This may take a few minutes if there are many devices\n");
	
	/* In INIT state, Activate IOPs */
	for (iop = i2o_controller_chain; iop; iop = niop) {
		dprintk(KERN_INFO "Calling i2o_activate_controller for %s...\n", 
			iop->name);
		niop = iop->next;
		if (i2o_activate_controller(iop) < 0)
			i2o_delete_controller(iop);
	}

	/* Active IOPs in HOLD state */

rebuild_sys_tab:
	if (i2o_controller_chain == NULL)
		return;

	/*
	 * If build_sys_table fails, we kill everything and bail
	 * as we can't init the IOPs w/o a system table
	 */	
	dprintk(KERN_INFO "i2o_core: Calling i2o_build_sys_table...\n");
	if (i2o_build_sys_table() < 0) {
		i2o_sys_shutdown();
		return;
	}

	/* If IOP don't get online, we need to rebuild the System table */
	for (iop = i2o_controller_chain; iop; iop = niop) {
		niop = iop->next;
		dprintk(KERN_INFO "Calling i2o_online_controller for %s...\n", iop->name);
		if (i2o_online_controller(iop) < 0) {
			i2o_delete_controller(iop);	
			goto rebuild_sys_tab;
		}
	}
	
	/* Active IOPs now in OPERATIONAL state */

	/*
	 * Register for status updates from all IOPs
	 */
	for(iop = i2o_controller_chain; iop; iop=iop->next) {

		/* Create a kernel thread to deal with dynamic LCT updates */
		iop->lct_pid = kernel_thread(i2o_dyn_lct, iop, CLONE_SIGHAND);
	
		/* Update change ind on DLCT */
		iop->dlct->change_ind = iop->lct->change_ind;

		/* Start dynamic LCT updates */
		i2o_lct_notify(iop);

		/* Register for all events from IRTOS */
		i2o_event_register(iop, core_context, 0, 0, 0xFFFFFFFF);
	}
}

/**
 *	i2o_sys_shutdown - shutdown I2O system
 *
 *	Bring down each i2o controller and then return. Each controller
 *	is taken through an orderly shutdown
 */
 
static void i2o_sys_shutdown(void)
{
	struct i2o_controller *iop, *niop;

	/* Delete all IOPs from the controller chain */
	/* that will reset all IOPs too */

	for (iop = i2o_controller_chain; iop; iop = niop) {
		niop = iop->next;
		i2o_delete_controller(iop);
	}
}

/**
 *	i2o_activate_controller	-	bring controller up to HOLD
 *	@iop: controller
 *
 *	This function brings an I2O controller into HOLD state. The adapter
 *	is reset if neccessary and then the queues and resource table
 *	are read. -1 is returned on a failure, 0 on success.
 *	
 */
 
int i2o_activate_controller(struct i2o_controller *iop)
{
	/* In INIT state, Wait Inbound Q to initialize (in i2o_status_get) */
	/* In READY state, Get status */

	if (i2o_status_get(iop) < 0) {
		printk(KERN_INFO "Unable to obtain status of %s, "
			"attempting a reset.\n", iop->name);
		if (i2o_reset_controller(iop) < 0)
			return -1;
	}

	if(iop->status_block->iop_state == ADAPTER_STATE_FAULTED) {
		printk(KERN_CRIT "%s: hardware fault\n", iop->name);
		return -1;
	}

	if (iop->status_block->i2o_version > I2OVER15) {
		printk(KERN_ERR "%s: Not running vrs. 1.5. of the I2O Specification.\n",
			iop->name);
		return -1;
	}

	if (iop->status_block->iop_state == ADAPTER_STATE_READY ||
	    iop->status_block->iop_state == ADAPTER_STATE_OPERATIONAL ||
	    iop->status_block->iop_state == ADAPTER_STATE_HOLD ||
	    iop->status_block->iop_state == ADAPTER_STATE_FAILED)
	{
		dprintk(KERN_INFO "%s: Already running, trying to reset...\n",
			iop->name);
		if (i2o_reset_controller(iop) < 0)
			return -1;
	}

	if (i2o_init_outbound_q(iop) < 0)
		return -1;

	if (i2o_post_outbound_messages(iop)) 
		return -1;

	/* In HOLD state */
	
	if (i2o_hrt_get(iop) < 0)
		return -1;

	return 0;
}


/**
 *	i2o_init_outbound_queue	- setup the outbound queue
 *	@c: controller
 *
 *	Clear and (re)initialize IOP's outbound queue. Returns 0 on
 *	success or a negative errno code on a failure.
 */
 
int i2o_init_outbound_q(struct i2o_controller *c)
{
	u8 *status;
	u32 m;
	u32 *msg;
	u32 time;

	dprintk(KERN_INFO "%s: Initializing Outbound Queue...\n", c->name);
	m=i2o_wait_message(c, "OutboundInit");
	if(m==0xFFFFFFFF)
		return -ETIMEDOUT;
	msg=(u32 *)(c->mem_offset+m);

	status = kmalloc(4,GFP_KERNEL);
	if (status==NULL) {
		printk(KERN_ERR "%s: Outbound Queue initialization failed - no free memory.\n",
			c->name);
		return -ENOMEM;
	}
	memset(status, 0, 4);

	msg[0]= EIGHT_WORD_MSG_SIZE| TRL_OFFSET_6;
	msg[1]= I2O_CMD_OUTBOUND_INIT<<24 | HOST_TID<<12 | ADAPTER_TID;
	msg[2]= core_context;
	msg[3]= 0x0106;				/* Transaction context */
	msg[4]= 4096;				/* Host page frame size */
	/* Frame size is in words. 256 bytes a frame for now */
	msg[5]= MSG_FRAME_SIZE<<16|0x80;	/* Outbound msg frame size in words and Initcode */
	msg[6]= 0xD0000004;			/* Simple SG LE, EOB */
	msg[7]= virt_to_bus(status);

	i2o_post_message(c,m);
	
	barrier();
	time=jiffies;
	while(status[0] < I2O_CMD_REJECTED)
	{
		if((jiffies-time)>=30*HZ)
		{
			if(status[0]==0x00)
				printk(KERN_ERR "%s: Ignored queue initialize request.\n",
					c->name);
			else  
				printk(KERN_ERR "%s: Outbound queue initialize timeout.\n",
					c->name);
			kfree(status);
			return -ETIMEDOUT;
		}  
		schedule();
		barrier();
	}  

	if(status[0] != I2O_CMD_COMPLETED)
	{
		printk(KERN_ERR "%s: IOP outbound initialise failed.\n", c->name);
		kfree(status);
		return -ETIMEDOUT;
	}

	kfree(status);
	return 0;
}

/**
 *	i2o_post_outbound_messages	-	fill message queue
 *	@c: controller
 *
 *	Allocate a message frame and load the messages into the IOP. The
 *	function returns zero on success or a negative errno code on
 *	failure.
 */

int i2o_post_outbound_messages(struct i2o_controller *c)
{
	int i;
	u32 m;
	/* Alloc space for IOP's outbound queue message frames */

	c->page_frame = kmalloc(MSG_POOL_SIZE, GFP_KERNEL);
	if(c->page_frame==NULL) {
		printk(KERN_ERR "%s: Outbound Q initialize failed; out of memory.\n",
			c->name);
		return -ENOMEM;
	}

	c->page_frame_map = pci_map_single(c->pdev, c->page_frame, MSG_POOL_SIZE, PCI_DMA_FROMDEVICE);

	if(c->page_frame_map == 0)
	{
		kfree(c->page_frame);
		printk(KERN_ERR "%s: Unable to map outbound queue.\n", c->name);
		return -ENOMEM;
	}

	m = c->page_frame_map;

	/* Post frames */

	for(i=0; i< NMBR_MSG_FRAMES; i++) {
		I2O_REPLY_WRITE32(c,m);
		mb();
		m += (MSG_FRAME_SIZE << 2);
	}

	return 0;
}

/*
 * Get the IOP's Logical Configuration Table
 */
int i2o_lct_get(struct i2o_controller *c)
{
	u32 msg[8];
	int ret, size = c->status_block->expected_lct_size;

	do {
		if (c->lct == NULL) {
			c->lct = kmalloc(size, GFP_KERNEL);
			if(c->lct == NULL) {
				printk(KERN_CRIT "%s: Lct Get failed. Out of memory.\n",
					c->name);
				return -ENOMEM;
			}
		}
		memset(c->lct, 0, size);

		msg[0] = EIGHT_WORD_MSG_SIZE|SGL_OFFSET_6;
		msg[1] = I2O_CMD_LCT_NOTIFY<<24 | HOST_TID<<12 | ADAPTER_TID;
		/* msg[2] filled in i2o_post_wait */
		msg[3] = 0;
		msg[4] = 0xFFFFFFFF;	/* All devices */
		msg[5] = 0x00000000;	/* Report now */
		msg[6] = 0xD0000000|size;
		msg[7] = virt_to_bus(c->lct);

		ret=i2o_post_wait_mem(c, msg, sizeof(msg), 120, c->lct, NULL);
		
		if(ret == -ETIMEDOUT)
		{
			c->lct = NULL;
			return ret;
		}
		
		if(ret<0)
		{
			printk(KERN_ERR "%s: LCT Get failed (status=%#x.\n", 
				c->name, -ret);	
			return ret;
		}

		if (c->lct->table_size << 2 > size) {
			size = c->lct->table_size << 2;
			kfree(c->lct);
			c->lct = NULL;
		}
	} while (c->lct == NULL);

        if ((ret=i2o_parse_lct(c)) < 0)
                return ret;

	return 0;
}

/*
 * Like above, but used for async notification.  The main
 * difference is that we keep track of the CurrentChangeIndiicator
 * so that we only get updates when it actually changes.
 *
 */
int i2o_lct_notify(struct i2o_controller *c)
{
	u32 msg[8];

	msg[0] = EIGHT_WORD_MSG_SIZE|SGL_OFFSET_6;
	msg[1] = I2O_CMD_LCT_NOTIFY<<24 | HOST_TID<<12 | ADAPTER_TID;
	msg[2] = core_context;
	msg[3] = 0xDEADBEEF;	
	msg[4] = 0xFFFFFFFF;	/* All devices */
	msg[5] = c->dlct->change_ind+1;	/* Next change */
	msg[6] = 0xD0000000|8192;
	msg[7] = virt_to_bus(c->dlct);

	return i2o_post_this(c, msg, sizeof(msg));
}
		
/*
 *	Bring a controller online into OPERATIONAL state. 
 */
 
int i2o_online_controller(struct i2o_controller *iop)
{
	u32 v;
	
	if (i2o_systab_send(iop) < 0)
		return -1;

	/* In READY state */

	dprintk(KERN_INFO "%s: Attempting to enable...\n", iop->name);
	if (i2o_enable_controller(iop) < 0)
		return -1;

	/* In OPERATIONAL state  */

	dprintk(KERN_INFO "%s: Attempting to get/parse lct...\n", iop->name);
	if (i2o_lct_get(iop) < 0)
		return -1;

	/* Check battery status */
	 
	iop->battery = 0;
	if(i2o_query_scalar(iop, ADAPTER_TID, 0x0000, 4, &v, 4)>=0)
	{
		if(v&16)
			iop->battery = 1;
	}

	return 0;
}

/*
 * Build system table
 *
 * The system table contains information about all the IOPs in the
 * system (duh) and is used by the Executives on the IOPs to establish
 * peer2peer connections.  We're not supporting peer2peer at the moment,
 * but this will be needed down the road for things like lan2lan forwarding.
 */
static int i2o_build_sys_table(void)
{
	struct i2o_controller *iop = NULL;
	struct i2o_controller *niop = NULL;
	int count = 0;

	sys_tbl_len = sizeof(struct i2o_sys_tbl) +	// Header + IOPs
				(i2o_num_controllers) *
					sizeof(struct i2o_sys_tbl_entry);

	if(sys_tbl)
		kfree(sys_tbl);

	sys_tbl = kmalloc(sys_tbl_len, GFP_KERNEL);
	if(!sys_tbl) {
		printk(KERN_CRIT "SysTab Set failed. Out of memory.\n");
		return -ENOMEM;
	}
	memset((void*)sys_tbl, 0, sys_tbl_len);

	sys_tbl->num_entries = i2o_num_controllers;
	sys_tbl->version = I2OVERSION; /* TODO: Version 2.0 */
	sys_tbl->change_ind = sys_tbl_ind++;

	for(iop = i2o_controller_chain; iop; iop = niop)
	{
		niop = iop->next;

		/* 
		 * Get updated IOP state so we have the latest information
		 *
		 * We should delete the controller at this point if it
		 * doesn't respond since  if it's not on the system table 
		 * it is techninically not part of the I2O subsyßtem...
		 */
		if(i2o_status_get(iop)) {
			printk(KERN_ERR "%s: Deleting b/c could not get status while"
				"attempting to build system table\n", iop->name);
			i2o_delete_controller(iop);		
			sys_tbl->num_entries--;
			continue; // try the next one
		}

		sys_tbl->iops[count].org_id = iop->status_block->org_id;
		sys_tbl->iops[count].iop_id = iop->unit + 2;
		sys_tbl->iops[count].seg_num = 0;
		sys_tbl->iops[count].i2o_version = 
				iop->status_block->i2o_version;
		sys_tbl->iops[count].iop_state = 
				iop->status_block->iop_state;
		sys_tbl->iops[count].msg_type = 
				iop->status_block->msg_type;
		sys_tbl->iops[count].frame_size = 
				iop->status_block->inbound_frame_size;
		sys_tbl->iops[count].last_changed = sys_tbl_ind - 1; // ??
		sys_tbl->iops[count].iop_capabilities = 
				iop->status_block->iop_capabilities;
		sys_tbl->iops[count].inbound_low = iop->post_port;
		sys_tbl->iops[count].inbound_high = 0;	// FIXME: 64-bit support

		count++;
	}

#ifdef DRIVERDEBUG
{
	u32 *table;
	table = (u32*)sys_tbl;
	for(count = 0; count < (sys_tbl_len >>2); count++)
		printk(KERN_INFO "sys_tbl[%d] = %0#10x\n", count, table[count]);
}
#endif

	return 0;
}


/*
 *	Run time support routines
 */
 
/*
 *	Generic "post and forget" helpers. This is less efficient - we do
 *	a memcpy for example that isnt strictly needed, but for most uses
 *	this is simply not worth optimising
 */

int i2o_post_this(struct i2o_controller *c, u32 *data, int len)
{
	u32 m;
	u32 *msg;
	unsigned long t=jiffies;

	do
	{
		mb();
		m = I2O_POST_READ32(c);
	}
	while(m==0xFFFFFFFF && (jiffies-t)<HZ);
	
	if(m==0xFFFFFFFF)
	{
		printk(KERN_ERR "%s: Timeout waiting for message frame!\n",
		       c->name);
		return -ETIMEDOUT;
	}
	msg = (u32 *)(c->mem_offset + m);
 	memcpy_toio(msg, data, len);
	i2o_post_message(c,m);
	return 0;
}

/**
 * 	i2o_post_wait_mem	-	I2O query/reply with DMA buffers
 *	@c: controller
 *	@msg: message to send
 *	@len: length of message
 *	@timeout: time in seconds to wait
 *	@mem1: attached memory buffer 1
 *	@mem2: attached memory buffer 2
 *
 * 	This core API allows an OSM to post a message and then be told whether
 *	or not the system received a successful reply. 
 *
 *	If the message times out then the value '-ETIMEDOUT' is returned. This
 *	is a special case. In this situation the message may (should) complete
 *	at an indefinite time in the future. When it completes it will use the
 *	memory buffers attached to the request. If -ETIMEDOUT is returned then
 *	the memory buffers must not be freed. Instead the event completion will
 *	free them for you. In all other cases the buffers are your problem.
 *
 *	Pass NULL for unneeded buffers.
 */
 
int i2o_post_wait_mem(struct i2o_controller *c, u32 *msg, int len, int timeout, void *mem1, void *mem2)
{
	DECLARE_WAIT_QUEUE_HEAD(wq_i2o_post);
	DECLARE_WAITQUEUE(wait, current);
	int complete = 0;
	int status;
	unsigned long flags = 0;
	struct i2o_post_wait_data *wait_data =
		kmalloc(sizeof(struct i2o_post_wait_data), GFP_KERNEL);

	if(!wait_data)
		return -ENOMEM;

	/*
	 *	Create a new notification object
	 */
	wait_data->status = &status;
	wait_data->complete = &complete;
	wait_data->mem[0] = mem1;
	wait_data->mem[1] = mem2;
	/* 
	 *	Queue the event with its unique id
	 */
	spin_lock_irqsave(&post_wait_lock, flags);

	wait_data->next = post_wait_queue;
	post_wait_queue = wait_data;
	wait_data->id = (++post_wait_id) & 0x7fff;
	wait_data->wq = &wq_i2o_post;

	spin_unlock_irqrestore(&post_wait_lock, flags);

	/*
	 *	Fill in the message id
	 */
	 
	msg[2] = 0x80000000|(u32)core_context|((u32)wait_data->id<<16);
	
	/*
	 *	Post the message to the controller. At some point later it 
	 *	will return. If we time out before it returns then
	 *	complete will be zero.  From the point post_this returns
	 *	the wait_data may have been deleted.
	 */

	add_wait_queue(&wq_i2o_post, &wait);
	set_current_state(TASK_INTERRUPTIBLE);
	if ((status = i2o_post_this(c, msg, len))==0) {
		schedule_timeout(HZ * timeout);
	}  
	else
	{
		remove_wait_queue(&wq_i2o_post, &wait);
		return -EIO;
	}
	remove_wait_queue(&wq_i2o_post, &wait);

	if(signal_pending(current))
		status = -EINTR;
		
	spin_lock_irqsave(&post_wait_lock, flags);
	barrier();	/* Be sure we see complete as it is locked */
	if(!complete)
	{
		/* 
		 *	Mark the entry dead. We cannot remove it. This is important.
		 *	When it does terminate (which it must do if the controller hasnt
		 *	died..) then it will otherwise scribble on stuff.
		 *	!complete lets us safely check if the entry is still
		 *	allocated and thus we can write into it
		 */
		wait_data->wq = NULL;
		status = -ETIMEDOUT;
	}
	else
	{
		/* Debugging check - remove me soon */
		if(status == -ETIMEDOUT)
		{
			printk("TIMEDOUT BUG!\n");
			status = -EIO;
		}
	}
	/* And the wait_data is not leaked either! */	 
	spin_unlock_irqrestore(&post_wait_lock, flags);
	return status;
}

/**
 * 	i2o_post_wait		-	I2O query/reply
 *	@c: controller
 *	@msg: message to send
 *	@len: length of message
 *	@timeout: time in seconds to wait
 *
 * 	This core API allows an OSM to post a message and then be told whether
 *	or not the system received a successful reply. 
 */
 
int i2o_post_wait(struct i2o_controller *c, u32 *msg, int len, int timeout)
{
	return i2o_post_wait_mem(c, msg, len, timeout, NULL, NULL);
}

/*
 * i2o_post_wait is completed and we want to wake up the 
 * sleeping proccess. Called by core's reply handler.
 */

static void i2o_post_wait_complete(u32 context, int status)
{
	struct i2o_post_wait_data **p1, *q;
	unsigned long flags;
	
	/* 
	 * We need to search through the post_wait 
	 * queue to see if the given message is still
	 * outstanding.  If not, it means that the IOP 
	 * took longer to respond to the message than we 
	 * had allowed and timer has already expired.  
	 * Not much we can do about that except log
	 * it for debug purposes, increase timeout, and recompile
	 *
	 * Lock needed to keep anyone from moving queue pointers 
	 * around while we're looking through them.
	 */

	spin_lock_irqsave(&post_wait_lock, flags);

	for(p1 = &post_wait_queue; *p1!=NULL; p1 = &((*p1)->next)) 
	{
		q = (*p1);
		if(q->id == ((context >> 16) & 0x7fff)) {
			/*
			 *	Delete it 
			 */
			 
			*p1 = q->next;
			
			/*
			 *	Live or dead ?
			 */
			 
			if(q->wq)
			{
				/* Live entry - wakeup and set status */
				*q->status = status;
				*q->complete = 1;
				wake_up(q->wq);
			}
			else
			{
				/*
				 *	Free resources. Caller is dead
				 */
				if(q->mem[0])
					kfree(q->mem[0]);
				if(q->mem[1])
					kfree(q->mem[1]);
				printk(KERN_WARNING "i2o_post_wait event completed after timeout.\n");
			}
			kfree(q);
			spin_unlock(&post_wait_lock);
			return;
		}
	}
	spin_unlock(&post_wait_lock);

	printk(KERN_DEBUG "i2o_post_wait: Bogus reply!\n");
}

/*	Issue UTIL_PARAMS_GET or UTIL_PARAMS_SET
 *
 *	This function can be used for all UtilParamsGet/Set operations.
 *	The OperationList is given in oplist-buffer, 
 *	and results are returned in reslist-buffer.
 *	Note that the minimum sized reslist is 8 bytes and contains
 *	ResultCount, ErrorInfoSize, BlockStatus and BlockSize.
 */
int i2o_issue_params(int cmd, struct i2o_controller *iop, int tid, 
                void *oplist, int oplen, void *reslist, int reslen)
{
	u32 msg[9]; 
	u32 *res32 = (u32*)reslist;
	u32 *restmp = (u32*)reslist;
	int len = 0;
	int i = 0;
	int wait_status;
	u32 *opmem, *resmem;
	
	/* Get DMAable memory */
	opmem = kmalloc(oplen, GFP_KERNEL);
	if(opmem == NULL)
		return -ENOMEM;
	memcpy(opmem, oplist, oplen);
	
	resmem = kmalloc(reslen, GFP_KERNEL);
	if(resmem == NULL)
	{
		kfree(opmem);
		return -ENOMEM;
	}
	
	msg[0] = NINE_WORD_MSG_SIZE | SGL_OFFSET_5;
	msg[1] = cmd << 24 | HOST_TID << 12 | tid; 
	msg[3] = 0;
	msg[4] = 0;
	msg[5] = 0x54000000 | oplen;	/* OperationList */
	msg[6] = virt_to_bus(opmem);
	msg[7] = 0xD0000000 | reslen;	/* ResultList */
	msg[8] = virt_to_bus(resmem);

	wait_status = i2o_post_wait_mem(iop, msg, sizeof(msg), 10, opmem, resmem);
	
	/*
	 *	This only looks like a memory leak - don't "fix" it.	
	 */
	if(wait_status == -ETIMEDOUT)
		return wait_status;

	/* Query failed */
	if(wait_status != 0)
	{
		kfree(resmem);
		kfree(opmem);
		return wait_status;
	}
	
	memcpy(reslist, resmem, reslen);
	/*
	 * Calculate number of bytes of Result LIST
	 * We need to loop through each Result BLOCK and grab the length
	 */
	restmp = res32 + 1;
	len = 1;
	for(i = 0; i < (res32[0]&0X0000FFFF); i++)
	{
		if(restmp[0]&0x00FF0000)	/* BlockStatus != SUCCESS */
		{
			printk(KERN_WARNING "%s - Error:\n  ErrorInfoSize = 0x%02x, " 
					"BlockStatus = 0x%02x, BlockSize = 0x%04x\n",
					(cmd == I2O_CMD_UTIL_PARAMS_SET) ? "PARAMS_SET"
					: "PARAMS_GET",   
					res32[1]>>24, (res32[1]>>16)&0xFF, res32[1]&0xFFFF);
	
			/*
			 *	If this is the only request,than we return an error
			 */
			if((res32[0]&0x0000FFFF) == 1)
			{
				return -((res32[1] >> 16) & 0xFF); /* -BlockStatus */
			}
		}
		len += restmp[0] & 0x0000FFFF;	/* Length of res BLOCK */
		restmp += restmp[0] & 0x0000FFFF;	/* Skip to next BLOCK */
	}
	return (len << 2);  /* bytes used by result list */
}

/*
 *	 Query one scalar group value or a whole scalar group.
 */                  	
int i2o_query_scalar(struct i2o_controller *iop, int tid, 
                     int group, int field, void *buf, int buflen)
{
	u16 opblk[] = { 1, 0, I2O_PARAMS_FIELD_GET, group, 1, field };
	u8  resblk[8+buflen]; /* 8 bytes for header */
	int size;

	if (field == -1)  		/* whole group */
       		opblk[4] = -1;
              
	size = i2o_issue_params(I2O_CMD_UTIL_PARAMS_GET, iop, tid, 
		opblk, sizeof(opblk), resblk, sizeof(resblk));
		
	memcpy(buf, resblk+8, buflen);  /* cut off header */
	
	if(size>buflen)
		return buflen;
	return size;
}

/*
 *	Set a scalar group value or a whole group.
 */
int i2o_set_scalar(struct i2o_controller *iop, int tid, 
		   int group, int field, void *buf, int buflen)
{
	u16 *opblk;
	u8  resblk[8+buflen]; /* 8 bytes for header */
        int size;

	opblk = kmalloc(buflen+64, GFP_KERNEL);
	if (opblk == NULL)
	{
		printk(KERN_ERR "i2o: no memory for operation buffer.\n");
		return -ENOMEM;
	}

	opblk[0] = 1;                        /* operation count */
	opblk[1] = 0;                        /* pad */
	opblk[2] = I2O_PARAMS_FIELD_SET;
	opblk[3] = group;

	if(field == -1) {               /* whole group */
		opblk[4] = -1;
		memcpy(opblk+5, buf, buflen);
	}
	else                            /* single field */
	{
		opblk[4] = 1;
		opblk[5] = field;
		memcpy(opblk+6, buf, buflen);
	}   

	size = i2o_issue_params(I2O_CMD_UTIL_PARAMS_SET, iop, tid, 
				opblk, 12+buflen, resblk, sizeof(resblk));

	kfree(opblk);
	if(size>buflen)
		return buflen;
	return size;
}

/* 
 * 	if oper == I2O_PARAMS_TABLE_GET, get from all rows 
 * 		if fieldcount == -1 return all fields
 *			ibuf and ibuflen are unused (use NULL, 0)
 * 		else return specific fields
 *  			ibuf contains fieldindexes
 *
 * 	if oper == I2O_PARAMS_LIST_GET, get from specific rows
 * 		if fieldcount == -1 return all fields
 *			ibuf contains rowcount, keyvalues
 * 		else return specific fields
 *			fieldcount is # of fieldindexes
 *  			ibuf contains fieldindexes, rowcount, keyvalues
 *
 *	You could also use directly function i2o_issue_params().
 */
int i2o_query_table(int oper, struct i2o_controller *iop, int tid, int group,
		int fieldcount, void *ibuf, int ibuflen,
		void *resblk, int reslen) 
{
	u16 *opblk;
	int size;

	opblk = kmalloc(10 + ibuflen, GFP_KERNEL);
	if (opblk == NULL)
	{
		printk(KERN_ERR "i2o: no memory for query buffer.\n");
		return -ENOMEM;
	}

	opblk[0] = 1;				/* operation count */
	opblk[1] = 0;				/* pad */
	opblk[2] = oper;
	opblk[3] = group;		
	opblk[4] = fieldcount;
	memcpy(opblk+5, ibuf, ibuflen);		/* other params */

	size = i2o_issue_params(I2O_CMD_UTIL_PARAMS_GET,iop, tid, 
				opblk, 10+ibuflen, resblk, reslen);

	kfree(opblk);
	if(size>reslen)
		return reslen;
	return size;
}

/*
 * 	Clear table group, i.e. delete all rows.
 */
int i2o_clear_table(struct i2o_controller *iop, int tid, int group)
{
	u16 opblk[] = { 1, 0, I2O_PARAMS_TABLE_CLEAR, group };
	u8  resblk[32]; /* min 8 bytes for result header */

	return i2o_issue_params(I2O_CMD_UTIL_PARAMS_SET, iop, tid, 
				opblk, sizeof(opblk), resblk, sizeof(resblk));
}

/*
 * 	Add a new row into a table group.
 *
 * 	if fieldcount==-1 then we add whole rows
 *		buf contains rowcount, keyvalues
 * 	else just specific fields are given, rest use defaults
 *  		buf contains fieldindexes, rowcount, keyvalues
 */	
int i2o_row_add_table(struct i2o_controller *iop, int tid,
		    int group, int fieldcount, void *buf, int buflen)
{
	u16 *opblk;
	u8  resblk[32]; /* min 8 bytes for header */
	int size;

	opblk = kmalloc(buflen+64, GFP_KERNEL);
	if (opblk == NULL)
	{
		printk(KERN_ERR "i2o: no memory for operation buffer.\n");
		return -ENOMEM;
	}

	opblk[0] = 1;			/* operation count */
	opblk[1] = 0;			/* pad */
	opblk[2] = I2O_PARAMS_ROW_ADD;
	opblk[3] = group;	
	opblk[4] = fieldcount;
	memcpy(opblk+5, buf, buflen);

	size = i2o_issue_params(I2O_CMD_UTIL_PARAMS_SET, iop, tid, 
				opblk, 10+buflen, resblk, sizeof(resblk));

	kfree(opblk);
	if(size>buflen)
		return buflen;
	return size;
}


/*
 * Used for error reporting/debugging purposes.
 * Following fail status are common to all classes.
 * The preserved message must be handled in the reply handler. 
 */
void i2o_report_fail_status(u8 req_status, u32* msg)
{
	static char *FAIL_STATUS[] = { 
		"0x80",				/* not used */
		"SERVICE_SUSPENDED", 		/* 0x81 */
		"SERVICE_TERMINATED", 		/* 0x82 */
		"CONGESTION",
		"FAILURE",
		"STATE_ERROR",
		"TIME_OUT",
		"ROUTING_FAILURE",
		"INVALID_VERSION",
		"INVALID_OFFSET",
		"INVALID_MSG_FLAGS",
		"FRAME_TOO_SMALL",
		"FRAME_TOO_LARGE",
		"INVALID_TARGET_ID",
		"INVALID_INITIATOR_ID",
		"INVALID_INITIATOR_CONTEX",	/* 0x8F */
		"UNKNOWN_FAILURE"		/* 0xFF */
	};

	if (req_status == I2O_FSC_TRANSPORT_UNKNOWN_FAILURE)
		printk("TRANSPORT_UNKNOWN_FAILURE (%0#2x)\n.", req_status);
	else
		printk("TRANSPORT_%s.\n", FAIL_STATUS[req_status & 0x0F]);

	/* Dump some details */

	printk(KERN_ERR "  InitiatorId = %d, TargetId = %d\n",
		(msg[1] >> 12) & 0xFFF, msg[1] & 0xFFF); 
	printk(KERN_ERR "  LowestVersion = 0x%02X, HighestVersion = 0x%02X\n",
		(msg[4] >> 8) & 0xFF, msg[4] & 0xFF);
	printk(KERN_ERR "  FailingHostUnit = 0x%04X,  FailingIOP = 0x%03X\n",
		msg[5] >> 16, msg[5] & 0xFFF);

	printk(KERN_ERR "  Severity:  0x%02X ", (msg[4] >> 16) & 0xFF); 
	if (msg[4] & (1<<16))
		printk("(FormatError), "
			"this msg can never be delivered/processed.\n");
	if (msg[4] & (1<<17))
		printk("(PathError), "
			"this msg can no longer be delivered/processed.\n");
	if (msg[4] & (1<<18))
		printk("(PathState), "
			"the system state does not allow delivery.\n");
	if (msg[4] & (1<<19))
		printk("(Congestion), resources temporarily not available;"
			"do not retry immediately.\n");
}

/*
 * Used for error reporting/debugging purposes.
 * Following reply status are common to all classes.
 */
void i2o_report_common_status(u8 req_status)
{
	static char *REPLY_STATUS[] = { 
		"SUCCESS", 
		"ABORT_DIRTY", 
		"ABORT_NO_DATA_TRANSFER",
		"ABORT_PARTIAL_TRANSFER",
		"ERROR_DIRTY",
		"ERROR_NO_DATA_TRANSFER",
		"ERROR_PARTIAL_TRANSFER",
		"PROCESS_ABORT_DIRTY",
		"PROCESS_ABORT_NO_DATA_TRANSFER",
		"PROCESS_ABORT_PARTIAL_TRANSFER",
		"TRANSACTION_ERROR",
		"PROGRESS_REPORT"	
	};

	if (req_status > I2O_REPLY_STATUS_PROGRESS_REPORT)
		printk("RequestStatus = %0#2x", req_status);
	else
		printk("%s", REPLY_STATUS[req_status]);
}

/*
 * Used for error reporting/debugging purposes.
 * Following detailed status are valid  for executive class, 
 * utility class, DDM class and for transaction error replies.
 */
static void i2o_report_common_dsc(u16 detailed_status)
{
	static char *COMMON_DSC[] = { 
		"SUCCESS",
		"0x01",				// not used
		"BAD_KEY",
		"TCL_ERROR",
		"REPLY_BUFFER_FULL",
		"NO_SUCH_PAGE",
		"INSUFFICIENT_RESOURCE_SOFT",
		"INSUFFICIENT_RESOURCE_HARD",
		"0x08",				// not used
		"CHAIN_BUFFER_TOO_LARGE",
		"UNSUPPORTED_FUNCTION",
		"DEVICE_LOCKED",
		"DEVICE_RESET",
		"INAPPROPRIATE_FUNCTION",
		"INVALID_INITIATOR_ADDRESS",
		"INVALID_MESSAGE_FLAGS",
		"INVALID_OFFSET",
		"INVALID_PARAMETER",
		"INVALID_REQUEST",
		"INVALID_TARGET_ADDRESS",
		"MESSAGE_TOO_LARGE",
		"MESSAGE_TOO_SMALL",
		"MISSING_PARAMETER",
		"TIMEOUT",
		"UNKNOWN_ERROR",
		"UNKNOWN_FUNCTION",
		"UNSUPPORTED_VERSION",
		"DEVICE_BUSY",
		"DEVICE_NOT_AVAILABLE"		
	};

	if (detailed_status > I2O_DSC_DEVICE_NOT_AVAILABLE)
		printk(" / DetailedStatus = %0#4x.\n", detailed_status);
	else
		printk(" / %s.\n", COMMON_DSC[detailed_status]);
}

/*
 * Used for error reporting/debugging purposes
 */
static void i2o_report_lan_dsc(u16 detailed_status)
{
	static char *LAN_DSC[] = {	// Lan detailed status code strings
		"SUCCESS",
		"DEVICE_FAILURE",
		"DESTINATION_NOT_FOUND",
		"TRANSMIT_ERROR",
		"TRANSMIT_ABORTED",
		"RECEIVE_ERROR",
		"RECEIVE_ABORTED",
		"DMA_ERROR",
		"BAD_PACKET_DETECTED",
		"OUT_OF_MEMORY",
		"BUCKET_OVERRUN",
		"IOP_INTERNAL_ERROR",
		"CANCELED",
		"INVALID_TRANSACTION_CONTEXT",
		"DEST_ADDRESS_DETECTED",
		"DEST_ADDRESS_OMITTED",
		"PARTIAL_PACKET_RETURNED",
		"TEMP_SUSPENDED_STATE",	// last Lan detailed status code
		"INVALID_REQUEST"	// general detailed status code
	};

	if (detailed_status > I2O_DSC_INVALID_REQUEST)
		printk(" / %0#4x.\n", detailed_status);
	else
		printk(" / %s.\n", LAN_DSC[detailed_status]);
}

/*
 * Used for error reporting/debugging purposes
 */
static void i2o_report_util_cmd(u8 cmd)
{
	switch (cmd) {
	case I2O_CMD_UTIL_NOP:
		printk("UTIL_NOP, ");
		break;			
	case I2O_CMD_UTIL_ABORT:
		printk("UTIL_ABORT, ");
		break;
	case I2O_CMD_UTIL_CLAIM:
		printk("UTIL_CLAIM, ");
		break;
	case I2O_CMD_UTIL_RELEASE:
		printk("UTIL_CLAIM_RELEASE, ");
		break;
	case I2O_CMD_UTIL_CONFIG_DIALOG:
		printk("UTIL_CONFIG_DIALOG, ");
		break;
	case I2O_CMD_UTIL_DEVICE_RESERVE:
		printk("UTIL_DEVICE_RESERVE, ");
		break;
	case I2O_CMD_UTIL_DEVICE_RELEASE:
		printk("UTIL_DEVICE_RELEASE, ");
		break;
	case I2O_CMD_UTIL_EVT_ACK:
		printk("UTIL_EVENT_ACKNOWLEDGE, ");
		break;
	case I2O_CMD_UTIL_EVT_REGISTER:
		printk("UTIL_EVENT_REGISTER, ");
		break;
	case I2O_CMD_UTIL_LOCK:
		printk("UTIL_LOCK, ");
		break;
	case I2O_CMD_UTIL_LOCK_RELEASE:
		printk("UTIL_LOCK_RELEASE, ");
		break;
	case I2O_CMD_UTIL_PARAMS_GET:
		printk("UTIL_PARAMS_GET, ");
		break;
	case I2O_CMD_UTIL_PARAMS_SET:
		printk("UTIL_PARAMS_SET, ");
		break;
	case I2O_CMD_UTIL_REPLY_FAULT_NOTIFY:
		printk("UTIL_REPLY_FAULT_NOTIFY, ");
		break;
	default:
		printk("Cmd = %0#2x, ",cmd);	
	}
}

/*
 * Used for error reporting/debugging purposes
 */
static void i2o_report_exec_cmd(u8 cmd)
{
	switch (cmd) {
	case I2O_CMD_ADAPTER_ASSIGN:
		printk("EXEC_ADAPTER_ASSIGN, ");
		break;
	case I2O_CMD_ADAPTER_READ:
		printk("EXEC_ADAPTER_READ, ");
		break;
	case I2O_CMD_ADAPTER_RELEASE:
		printk("EXEC_ADAPTER_RELEASE, ");
		break;
	case I2O_CMD_BIOS_INFO_SET:
		printk("EXEC_BIOS_INFO_SET, ");
		break;
	case I2O_CMD_BOOT_DEVICE_SET:
		printk("EXEC_BOOT_DEVICE_SET, ");
		break;
	case I2O_CMD_CONFIG_VALIDATE:
		printk("EXEC_CONFIG_VALIDATE, ");
		break;
	case I2O_CMD_CONN_SETUP:
		printk("EXEC_CONN_SETUP, ");
		break;
	case I2O_CMD_DDM_DESTROY:
		printk("EXEC_DDM_DESTROY, ");
		break;
	case I2O_CMD_DDM_ENABLE:
		printk("EXEC_DDM_ENABLE, ");
		break;
	case I2O_CMD_DDM_QUIESCE:
		printk("EXEC_DDM_QUIESCE, ");
		break;
	case I2O_CMD_DDM_RESET:
		printk("EXEC_DDM_RESET, ");
		break;
	case I2O_CMD_DDM_SUSPEND:
		printk("EXEC_DDM_SUSPEND, ");
		break;
	case I2O_CMD_DEVICE_ASSIGN:
		printk("EXEC_DEVICE_ASSIGN, ");
		break;
	case I2O_CMD_DEVICE_RELEASE:
		printk("EXEC_DEVICE_RELEASE, ");
		break;
	case I2O_CMD_HRT_GET:
		printk("EXEC_HRT_GET, ");
		break;
	case I2O_CMD_ADAPTER_CLEAR:
		printk("EXEC_IOP_CLEAR, ");
		break;
	case I2O_CMD_ADAPTER_CONNECT:
		printk("EXEC_IOP_CONNECT, ");
		break;
	case I2O_CMD_ADAPTER_RESET:
		printk("EXEC_IOP_RESET, ");
		break;
	case I2O_CMD_LCT_NOTIFY:
		printk("EXEC_LCT_NOTIFY, ");
		break;
	case I2O_CMD_OUTBOUND_INIT:
		printk("EXEC_OUTBOUND_INIT, ");
		break;
	case I2O_CMD_PATH_ENABLE:
		printk("EXEC_PATH_ENABLE, ");
		break;
	case I2O_CMD_PATH_QUIESCE:
		printk("EXEC_PATH_QUIESCE, ");
		break;
	case I2O_CMD_PATH_RESET:
		printk("EXEC_PATH_RESET, ");
		break;
	case I2O_CMD_STATIC_MF_CREATE:
		printk("EXEC_STATIC_MF_CREATE, ");
		break;
	case I2O_CMD_STATIC_MF_RELEASE:
		printk("EXEC_STATIC_MF_RELEASE, ");
		break;
	case I2O_CMD_STATUS_GET:
		printk("EXEC_STATUS_GET, ");
		break;
	case I2O_CMD_SW_DOWNLOAD:
		printk("EXEC_SW_DOWNLOAD, ");
		break;
	case I2O_CMD_SW_UPLOAD:
		printk("EXEC_SW_UPLOAD, ");
		break;
	case I2O_CMD_SW_REMOVE:
		printk("EXEC_SW_REMOVE, ");
		break;
	case I2O_CMD_SYS_ENABLE:
		printk("EXEC_SYS_ENABLE, ");
		break;
	case I2O_CMD_SYS_MODIFY:
		printk("EXEC_SYS_MODIFY, ");
		break;
	case I2O_CMD_SYS_QUIESCE:
		printk("EXEC_SYS_QUIESCE, ");
		break;
	case I2O_CMD_SYS_TAB_SET:
		printk("EXEC_SYS_TAB_SET, ");
		break;
	default:
		printk("Cmd = %#02x, ",cmd);	
	}
}

/*
 * Used for error reporting/debugging purposes
 */
static void i2o_report_lan_cmd(u8 cmd)
{
	switch (cmd) {
	case LAN_PACKET_SEND:
		printk("LAN_PACKET_SEND, "); 
		break;
	case LAN_SDU_SEND:
		printk("LAN_SDU_SEND, ");
		break;
	case LAN_RECEIVE_POST:
		printk("LAN_RECEIVE_POST, ");
		break;
	case LAN_RESET:
		printk("LAN_RESET, ");
		break;
	case LAN_SUSPEND:
		printk("LAN_SUSPEND, ");
		break;
	default:
		printk("Cmd = %0#2x, ",cmd);	
	}	
}

/*
 * Used for error reporting/debugging purposes.
 * Report Cmd name, Request status, Detailed Status.
 */
void i2o_report_status(const char *severity, const char *str, u32 *msg)
{
	u8 cmd = (msg[1]>>24)&0xFF;
	u8 req_status = (msg[4]>>24)&0xFF;
	u16 detailed_status = msg[4]&0xFFFF;
	struct i2o_handler *h = i2o_handlers[msg[2] & (MAX_I2O_MODULES-1)];

	printk("%s%s: ", severity, str);

	if (cmd < 0x1F) 			// Utility cmd
		i2o_report_util_cmd(cmd);
	
	else if (cmd >= 0xA0 && cmd <= 0xEF) 	// Executive cmd
		i2o_report_exec_cmd(cmd);
	
	else if (h->class == I2O_CLASS_LAN && cmd >= 0x30 && cmd <= 0x3F)
		i2o_report_lan_cmd(cmd);	// LAN cmd
	else
        	printk("Cmd = %0#2x, ", cmd);	// Other cmds

	if (msg[0] & MSG_FAIL) {
		i2o_report_fail_status(req_status, msg);
		return;
	}
	
	i2o_report_common_status(req_status);

	if (cmd < 0x1F || (cmd >= 0xA0 && cmd <= 0xEF))
		i2o_report_common_dsc(detailed_status);	
	else if (h->class == I2O_CLASS_LAN && cmd >= 0x30 && cmd <= 0x3F)
		i2o_report_lan_dsc(detailed_status);
	else
		printk(" / DetailedStatus = %0#4x.\n", detailed_status); 
}

/* Used to dump a message to syslog during debugging */
void i2o_dump_message(u32 *msg)
{
#ifdef DRIVERDEBUG
	int i;
	printk(KERN_INFO "Dumping I2O message size %d @ %p\n", 
		msg[0]>>16&0xffff, msg);
	for(i = 0; i < ((msg[0]>>16)&0xffff); i++)
		printk(KERN_INFO "  msg[%d] = %0#10x\n", i, msg[i]);
#endif
}

/*
 * I2O reboot/shutdown notification.
 *
 * - Call each OSM's reboot notifier (if one exists)
 * - Quiesce each IOP in the system
 *
 * Each IOP has to be quiesced before we can ensure that the system
 * can be properly shutdown as a transaction that has already been
 * acknowledged still needs to be placed in permanent store on the IOP.
 * The SysQuiesce causes the IOP to force all HDMs to complete their
 * transactions before returning, so only at that point is it safe
 * 
 */
static int i2o_reboot_event(struct notifier_block *n, unsigned long code, void
*p)
{
	int i = 0;
	struct i2o_controller *c = NULL;

	if(code != SYS_RESTART && code != SYS_HALT && code != SYS_POWER_OFF)
		return NOTIFY_DONE;

	printk(KERN_INFO "Shutting down I2O system.\n");
	printk(KERN_INFO 
		"   This could take a few minutes if there are many devices attached\n");

	for(i = 0; i < MAX_I2O_MODULES; i++)
	{
		if(i2o_handlers[i] && i2o_handlers[i]->reboot_notify)
			i2o_handlers[i]->reboot_notify();
	}

	for(c = i2o_controller_chain; c; c = c->next)
	{
		if(i2o_quiesce_controller(c))
		{
			printk(KERN_WARNING "i2o: Could not quiesce %s.\n"
			       "Verify setup on next system power up.\n",
			       c->name);
		}
	}

	printk(KERN_INFO "I2O system down.\n");
	return NOTIFY_DONE;
}


EXPORT_SYMBOL(i2o_controller_chain);
EXPORT_SYMBOL(i2o_num_controllers);
EXPORT_SYMBOL(i2o_find_controller);
EXPORT_SYMBOL(i2o_unlock_controller);
EXPORT_SYMBOL(i2o_status_get);

EXPORT_SYMBOL(i2o_install_handler);
EXPORT_SYMBOL(i2o_remove_handler);

EXPORT_SYMBOL(i2o_install_controller);
EXPORT_SYMBOL(i2o_delete_controller);
EXPORT_SYMBOL(i2o_run_queue);

EXPORT_SYMBOL(i2o_claim_device);
EXPORT_SYMBOL(i2o_release_device);
EXPORT_SYMBOL(i2o_device_notify_on);
EXPORT_SYMBOL(i2o_device_notify_off);

EXPORT_SYMBOL(i2o_post_this);
EXPORT_SYMBOL(i2o_post_wait);
EXPORT_SYMBOL(i2o_post_wait_mem);

EXPORT_SYMBOL(i2o_query_scalar);
EXPORT_SYMBOL(i2o_set_scalar);
EXPORT_SYMBOL(i2o_query_table);
EXPORT_SYMBOL(i2o_clear_table);
EXPORT_SYMBOL(i2o_row_add_table);
EXPORT_SYMBOL(i2o_issue_params);

EXPORT_SYMBOL(i2o_event_register);
EXPORT_SYMBOL(i2o_event_ack);

EXPORT_SYMBOL(i2o_report_status);
EXPORT_SYMBOL(i2o_dump_message);

EXPORT_SYMBOL(i2o_get_class_name);

EXPORT_SYMBOL_GPL(i2o_sys_init);

MODULE_AUTHOR("Red Hat Software");
MODULE_DESCRIPTION("I2O Core");
MODULE_LICENSE("GPL");

static int i2o_core_init(void)
{
	printk(KERN_INFO "I2O Core - (C) Copyright 1999 Red Hat Software\n");
	if (i2o_install_handler(&i2o_core_handler) < 0)
	{
		printk(KERN_ERR "i2o_core: Unable to install core handler.\nI2O stack not loaded!");
		return 0;
	}

	core_context = i2o_core_handler.context;

	/*
	 * Initialize event handling thread
	 */	

	init_MUTEX_LOCKED(&evt_sem);
	evt_pid = kernel_thread(i2o_core_evt, &evt_reply, CLONE_SIGHAND);
	if(evt_pid < 0)
	{
		printk(KERN_ERR "I2O: Could not create event handler kernel thread\n");
		i2o_remove_handler(&i2o_core_handler);
		return 0;
	}
	else
		printk(KERN_INFO "I2O: Event thread created as pid %d\n", evt_pid);

	if(i2o_num_controllers)
		i2o_sys_init();

	register_reboot_notifier(&i2o_reboot_notifier);

	return 0;
}

static void i2o_core_exit(void)
{
	int stat;

	unregister_reboot_notifier(&i2o_reboot_notifier);

	if(i2o_num_controllers)
		i2o_sys_shutdown();

	/*
	 * If this is shutdown time, the thread has already been killed
	 */
	if(evt_running) {
		printk("Terminating i2o threads...");
		stat = kill_proc(evt_pid, SIGTERM, 1);
		if(!stat) {
			printk("waiting...");
			wait_for_completion(&evt_dead);
		}
		printk("done.\n");
	}
	i2o_remove_handler(&i2o_core_handler);
	unregister_reboot_notifier(&i2o_reboot_notifier);
}

module_init(i2o_core_init);
module_exit(i2o_core_exit);

