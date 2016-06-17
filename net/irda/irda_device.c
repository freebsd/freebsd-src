/*********************************************************************
 *                
 * Filename:      irda_device.c
 * Version:       0.9
 * Description:   Utility functions used by the device drivers
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Sat Oct  9 09:22:27 1999
 * Modified at:   Sun Jan 23 17:41:24 2000
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * 
 *     Copyright (c) 1999-2000 Dag Brattli, All Rights Reserved.
 *     Copyright (c) 2000-2001 Jean Tourrilhes <jt@hpl.hp.com>
 *     
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License 
 *     along with this program; if not, write to the Free Software 
 *     Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 *     MA 02111-1307 USA
 *     
 ********************************************************************/

#include <linux/config.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/skbuff.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <linux/netdevice.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/kmod.h>
#include <linux/wireless.h>
#include <linux/spinlock.h>

#include <asm/ioctls.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <asm/dma.h>
#include <asm/io.h>

#include <net/pkt_sched.h>

#include <net/irda/irda_device.h>
#include <net/irda/irlap.h>
#include <net/irda/timer.h>
#include <net/irda/wrapper.h>

extern int irtty_init(void);
extern int nsc_ircc_init(void);
extern int ircc_init(void);
extern int toshoboe_init(void);
extern int litelink_init(void);
extern int w83977af_init(void);
extern int esi_init(void);
extern int tekram_init(void);
extern int actisys_init(void);
extern int girbil_init(void);
extern int sa1100_irda_init(void);
extern int ep7211_ir_init(void);
extern int mcp2120_init(void);

static void __irda_task_delete(struct irda_task *task);

static hashbin_t *dongles = NULL;
static hashbin_t *tasks = NULL;

const char *infrared_mode[] = {
	"IRDA_IRLAP",
	"IRDA_RAW",
	"SHARP_ASK",
	"TV_REMOTE",
};

#ifdef CONFIG_IRDA_DEBUG
static const char *task_state[] = {
	"IRDA_TASK_INIT",
	"IRDA_TASK_DONE", 
	"IRDA_TASK_WAIT",
	"IRDA_TASK_WAIT1",
	"IRDA_TASK_WAIT2",
	"IRDA_TASK_WAIT3",
	"IRDA_TASK_CHILD_INIT",
	"IRDA_TASK_CHILD_WAIT",
	"IRDA_TASK_CHILD_DONE",
};
#endif	/* CONFIG_IRDA_DEBUG */

static void irda_task_timer_expired(void *data);

#ifdef CONFIG_PROC_FS
int irda_device_proc_read(char *buf, char **start, off_t offset, int len, 
			  int unused);

#endif /* CONFIG_PROC_FS */

int __init irda_device_init( void)
{
	dongles = hashbin_new(HB_GLOBAL);
	if (dongles == NULL) {
		printk(KERN_WARNING 
		       "IrDA: Can't allocate dongles hashbin!\n");
		return -ENOMEM;
	}

	tasks = hashbin_new(HB_GLOBAL);
	if (tasks == NULL) {
		printk(KERN_WARNING 
		       "IrDA: Can't allocate tasks hashbin!\n");
		return -ENOMEM;
	}

	/* 
	 * Call the init function of the device drivers that has not been
	 * compiled as a module 
	 * Note : non-modular IrDA is not supported in 2.4.X, so don't
	 * waste too much time fixing this code. If you require it, please
	 * upgrade to the IrDA stack in 2.5.X. Jean II
	 */
#ifdef CONFIG_IRTTY_SIR
	irtty_init();
#endif
#ifdef CONFIG_WINBOND_FIR
	w83977af_init();
#endif
#ifdef CONFIG_SA1100_FIR
	sa1100_irda_init();
#endif
#ifdef CONFIG_NSC_FIR
	nsc_ircc_init();
#endif
#ifdef CONFIG_TOSHIBA_OLD
	toshoboe_init();
#endif
#ifdef CONFIG_SMC_IRCC_FIR
	ircc_init();
#endif
#ifdef CONFIG_ESI_DONGLE
	esi_init();
#endif
#ifdef CONFIG_TEKRAM_DONGLE
	tekram_init();
#endif
#ifdef CONFIG_ACTISYS_DONGLE
	actisys_init();
#endif
#ifdef CONFIG_GIRBIL_DONGLE
	girbil_init();
#endif
#ifdef CONFIG_LITELINK_DONGLE
	litelink_init();
#endif
#ifdef CONFIG_OLD_BELKIN
 	old_belkin_init();
#endif
#ifdef CONFIG_EP7211_IR
 	ep7211_ir_init();
#endif
#ifdef CONFIG_MCP2120_DONGLE
	mcp2120_init();
#endif
	return 0;
}

void irda_device_cleanup(void)
{
	IRDA_DEBUG(4, "%s()\n", __FUNCTION__);

	hashbin_delete(tasks, (FREE_FUNC) __irda_task_delete);
	hashbin_delete(dongles, NULL);
}

/*
 * Function irda_device_set_media_busy (self, status)
 *
 *    Called when we have detected that another station is transmiting
 *    in contention mode.
 */
void irda_device_set_media_busy(struct net_device *dev, int status) 
{
	struct irlap_cb *self;

	IRDA_DEBUG(4, "%s(%s)\n", __FUNCTION__, status ? "TRUE" : "FALSE");

	self = (struct irlap_cb *) dev->atalk_ptr;

	ASSERT(self != NULL, return;);
	ASSERT(self->magic == LAP_MAGIC, return;);

	if (status) {
		self->media_busy = TRUE;
		if (status == SMALL)
			irlap_start_mbusy_timer(self, SMALLBUSY_TIMEOUT);
		else
			irlap_start_mbusy_timer(self, MEDIABUSY_TIMEOUT);
		IRDA_DEBUG( 4, "Media busy!\n");
	} else {
		self->media_busy = FALSE;
		irlap_stop_mbusy_timer(self);
	}
}

int irda_device_set_dtr_rts(struct net_device *dev, int dtr, int rts)
{	
	struct if_irda_req req;
	int ret;

	IRDA_DEBUG(2, "%s()\n", __FUNCTION__);

	if (!dev->do_ioctl) {
		ERROR("%s(), do_ioctl not impl. by "
		      "device driver\n", __FUNCTION__);
		return -1;
	}

	req.ifr_dtr = dtr;
	req.ifr_rts = rts;

	ret = dev->do_ioctl(dev, (struct ifreq *) &req, SIOCSDTRRTS);

	return ret;
}

int irda_device_change_speed(struct net_device *dev, __u32 speed)
{	
	struct if_irda_req req;
	int ret;

	IRDA_DEBUG(2, "%s()\n", __FUNCTION__);

	if (!dev->do_ioctl) {
		ERROR("%s(), do_ioctl not impl. by "
		      "device driver\n", __FUNCTION__);
		return -1;
	}

	req.ifr_baudrate = speed;

	ret = dev->do_ioctl(dev, (struct ifreq *) &req, SIOCSBANDWIDTH);

	return ret;
}

/*
 * Function irda_device_is_receiving (dev)
 *
 *    Check if the device driver is currently receiving data
 *
 */
int irda_device_is_receiving(struct net_device *dev)
{
	struct if_irda_req req;
	int ret;

	IRDA_DEBUG(2, "%s()\n", __FUNCTION__);

	if (!dev->do_ioctl) {
		ERROR("%s(), do_ioctl not impl. by "
		      "device driver\n", __FUNCTION__);
		return -1;
	}

	ret = dev->do_ioctl(dev, (struct ifreq *) &req, SIOCGRECEIVING);
	if (ret < 0)
		return ret;

	return req.ifr_receiving;
}

void irda_task_next_state(struct irda_task *task, IRDA_TASK_STATE state)
{
	IRDA_DEBUG(2, "%s(), state = %s\n", __FUNCTION__, task_state[state]);

	task->state = state;
}

static void __irda_task_delete(struct irda_task *task)
{
	del_timer(&task->timer);
	
	kfree(task);
}

void irda_task_delete(struct irda_task *task)
{
	/* Unregister task */
	hashbin_remove(tasks, (int) task, NULL);

	__irda_task_delete(task);
}

/*
 * Function irda_task_kick (task)
 *
 *    Tries to execute a task possible multiple times until the task is either
 *    finished, or askes for a timeout. When a task is finished, we do post
 *    processing, and notify the parent task, that is waiting for this task
 *    to complete.
 */
int irda_task_kick(struct irda_task *task)
{
	int finished = TRUE;
	int count = 0;
	int timeout;

	IRDA_DEBUG(2, "%s()\n", __FUNCTION__);

	ASSERT(task != NULL, return -1;);
	ASSERT(task->magic == IRDA_TASK_MAGIC, return -1;);

	/* Execute task until it's finished, or askes for a timeout */
	do {
		timeout = task->function(task);
		if (count++ > 100) {
			ERROR("%s(), error in task handler!\n", __FUNCTION__);
			irda_task_delete(task);
			return TRUE;
		}			
	} while ((timeout == 0) && (task->state != IRDA_TASK_DONE));

	if (timeout < 0) {
		ERROR("%s(), Error executing task!\n", __FUNCTION__);
		irda_task_delete(task);
		return TRUE;
	}

	/* Check if we are finished */
	if (task->state == IRDA_TASK_DONE) {
		del_timer(&task->timer);

		/* Do post processing */
		if (task->finished)
			task->finished(task);

		/* Notify parent */
		if (task->parent) {
			/* Check if parent is waiting for us to complete */
			if (task->parent->state == IRDA_TASK_CHILD_WAIT) {
				task->parent->state = IRDA_TASK_CHILD_DONE;

				/* Stop timer now that we are here */
				del_timer(&task->parent->timer);

				/* Kick parent task */
				irda_task_kick(task->parent);
			}
		}		
		irda_task_delete(task);
	} else if (timeout > 0) {
		irda_start_timer(&task->timer, timeout, (void *) task, 
				 irda_task_timer_expired);
		finished = FALSE;
	} else {
		IRDA_DEBUG(0, "%s(), not finished, and no timeout!\n", __FUNCTION__);
		finished = FALSE;
	}

	return finished;
}

/*
 * Function irda_task_execute (instance, function, finished)
 *
 *    This function registers and tries to execute tasks that may take some
 *    time to complete. We do it this hairy way since we may have been
 *    called from interrupt context, so it's not possible to use
 *    schedule_timeout() 
 * Two important notes :
 *	o Make sure you irda_task_delete(task); in case you delete the
 *	  calling instance.
 *	o No real need to lock when calling this function, but you may
 *	  want to lock within the task handler.
 * Jean II
 */
struct irda_task *irda_task_execute(void *instance, 
				    IRDA_TASK_CALLBACK function, 
				    IRDA_TASK_CALLBACK finished, 
				    struct irda_task *parent, void *param)
{
	struct irda_task *task;
	int ret;

	IRDA_DEBUG(2, "%s()\n", __FUNCTION__);

	task = kmalloc(sizeof(struct irda_task), GFP_ATOMIC);
	if (!task)
		return NULL;

	task->state    = IRDA_TASK_INIT;
	task->instance = instance;
	task->function = function;
	task->finished = finished;
	task->parent   = parent;
	task->param    = param;	
	task->magic    = IRDA_TASK_MAGIC;

	init_timer(&task->timer);

	/* Register task */
	hashbin_insert(tasks, (irda_queue_t *) task, (int) task, NULL);

	/* No time to waste, so lets get going! */
	ret = irda_task_kick(task);
	if (ret)
		return NULL;
	else
		return task;
}

/*
 * Function irda_task_timer_expired (data)
 *
 *    Task time has expired. We now try to execute task (again), and restart
 *    the timer if the task has not finished yet
 */
static void irda_task_timer_expired(void *data)
{
	struct irda_task *task;

	IRDA_DEBUG(2, "%s()\n", __FUNCTION__);

	task = (struct irda_task *) data;

	irda_task_kick(task);
}

/*
 * Function irda_device_setup (dev)
 *
 *    This function should be used by low level device drivers in a similar way
 *    as ether_setup() is used by normal network device drivers
 */
int irda_device_setup(struct net_device *dev) 
{
	ASSERT(dev != NULL, return -1;);

        dev->hard_header_len = 0;
        dev->addr_len        = 0;

	dev->features        |= NETIF_F_DYNALLOC;
	/* dev->destructor      = irda_device_destructor; */

        dev->type            = ARPHRD_IRDA;
        dev->tx_queue_len    = 8; /* Window size + 1 s-frame */
 
	memset(dev->broadcast, 0xff, 4);

	dev->mtu = 2048;
	dev->flags = IFF_NOARP;
	return 0;
}

/*
 * Function irda_device_txqueue_empty (dev)
 *
 *    Check if there is still some frames in the transmit queue for this
 *    device. Maybe we should use: q->q.qlen == 0.
 *
 */
int irda_device_txqueue_empty(struct net_device *dev)
{
	if (skb_queue_len(&dev->qdisc->q))
		return FALSE;

	return TRUE;
}

/*
 * Function irda_device_init_dongle (self, type, qos)
 *
 *    Initialize attached dongle.
 *
 * Important : request_module require us to call this function with
 * a process context and irq enabled. - Jean II
 */
dongle_t *irda_device_dongle_init(struct net_device *dev, int type)
{
	struct dongle_reg *reg;
	dongle_t *dongle;

	ASSERT(dev != NULL, return NULL;);

#ifdef CONFIG_KMOD
	{
	char modname[32];
	ASSERT(!in_interrupt(), return NULL;);
	/* Try to load the module needed */
	sprintf(modname, "irda-dongle-%d", type);
	request_module(modname);
	}
#endif /* CONFIG_KMOD */

	if (!(reg = hashbin_find(dongles, type, NULL))) {
		ERROR("IrDA: Unable to find requested dongle\n");
		return NULL;
	}

	/* Allocate dongle info for this instance */
	dongle = kmalloc(sizeof(dongle_t), GFP_KERNEL);
	if (!dongle)
		return NULL;

	memset(dongle, 0, sizeof(dongle_t));

	/* Bind the registration info to this particular instance */
	dongle->issue = reg;
	dongle->dev = dev;

	return dongle;
}

/*
 * Function irda_device_dongle_cleanup (dongle)
 *
 *    
 *
 */
int irda_device_dongle_cleanup(dongle_t *dongle)
{
	ASSERT(dongle != NULL, return -1;);

	dongle->issue->close(dongle);

	kfree(dongle);

	return 0;
}

/*
 * Function irda_device_register_dongle (dongle)
 *
 *    
 *
 */
int irda_device_register_dongle(struct dongle_reg *new)
{
	/* Check if this dongle has been registred before */
	if (hashbin_find(dongles, new->type, NULL)) {
		MESSAGE("%s(), Dongle already registered\n", __FUNCTION__);
                return 0;
        }
	
	/* Insert IrDA dongle into hashbin */
	hashbin_insert(dongles, (irda_queue_t *) new, new->type, NULL);
	
        return 0;
}

/*
 * Function irda_device_unregister_dongle (dongle)
 *
 *    Unregister dongle, and remove dongle from list of registred dongles
 *
 */
void irda_device_unregister_dongle(struct dongle_reg *dongle)
{
	struct dongle *node;

	node = hashbin_remove(dongles, dongle->type, NULL);
	if (!node) {
		ERROR("%s(), dongle not found!\n", __FUNCTION__);
		return;
	}
}

/*
 * Function irda_device_set_mode (self, mode)
 *
 *    Set the Infrared device driver into mode where it sends and receives
 *    data without using IrLAP framing. Check out the particular device
 *    driver to find out which modes it support.
 */
int irda_device_set_mode(struct net_device* dev, int mode)
{	
	struct if_irda_req req;
	int ret;

	IRDA_DEBUG(0, "%s()\n", __FUNCTION__);

	if (!dev->do_ioctl) {
		ERROR("%s(), set_raw_mode not impl. by "
		      "device driver\n", __FUNCTION__);
		return -1;
	}
	
	req.ifr_mode = mode;

	ret = dev->do_ioctl(dev, (struct ifreq *) &req, SIOCSMODE);
	
	return ret;
}

/*
 * Function setup_dma (idev, buffer, count, mode)
 *
 *    Setup the DMA channel. Commonly used by ISA FIR drivers
 *
 */
void setup_dma(int channel, char *buffer, int count, int mode)
{
	unsigned long flags;
	
	flags = claim_dma_lock();
	
	disable_dma(channel);
	clear_dma_ff(channel);
	set_dma_mode(channel, mode);
	set_dma_addr(channel, virt_to_bus(buffer));
	set_dma_count(channel, count);
	enable_dma(channel);

	release_dma_lock(flags);
}
