/*
 * ipmi_kcs_intf.c
 *
 * The interface to the IPMI driver for the KCS.
 *
 * Author: MontaVista Software, Inc.
 *         Corey Minyard <minyard@mvista.com>
 *         source@mvista.com
 *
 * Copyright 2002 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * This file holds the "policy" for the interface to the KCS state
 * machine.  It does the configuration, handles timers and interrupts,
 * and drives the real KCS state machine.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <asm/system.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/ioport.h>
#ifdef CONFIG_HIGH_RES_TIMERS
#include <linux/hrtime.h>
#endif
#include <linux/interrupt.h>
#include <linux/ipmi_smi.h>
#include <asm/io.h>
#include "ipmi_kcs_sm.h"
#include <linux/init.h>

/* Measure times between events in the driver. */
#undef DEBUG_TIMING

/* Timing parameters.  Call every 10 ms when not doing anything,
   otherwise call every KCS_SHORT_TIMEOUT_USEC microseconds. */
#define KCS_TIMEOUT_TIME_USEC	10000
#define KCS_USEC_PER_JIFFY	(1000000/HZ)
#define KCS_TIMEOUT_JIFFIES	(KCS_TIMEOUT_TIME_USEC/KCS_USEC_PER_JIFFY)
#define KCS_SHORT_TIMEOUT_USEC  250 /* .25ms when the SM request a
                                       short timeout */

#ifdef CONFIG_IPMI_KCS
/* This forces a dependency to the config file for this option. */
#endif

enum kcs_intf_state {
	KCS_NORMAL,
	KCS_GETTING_FLAGS,
	KCS_GETTING_EVENTS,
	KCS_CLEARING_FLAGS,
	KCS_CLEARING_FLAGS_THEN_SET_IRQ,
	KCS_GETTING_MESSAGES,
	KCS_ENABLE_INTERRUPTS1,
	KCS_ENABLE_INTERRUPTS2
	/* FIXME - add watchdog stuff. */
};

struct kcs_info
{
	ipmi_smi_t          intf;
	struct kcs_data     *kcs_sm;
	spinlock_t          kcs_lock;
	spinlock_t          msg_lock;
	struct list_head    xmit_msgs;
	struct list_head    hp_xmit_msgs;
	struct ipmi_smi_msg *curr_msg;
	enum kcs_intf_state kcs_state;

	/* Flags from the last GET_MSG_FLAGS command, used when an ATTN
	   is set to hold the flags until we are done handling everything
	   from the flags. */
#define RECEIVE_MSG_AVAIL	0x01
#define EVENT_MSG_BUFFER_FULL	0x02
#define WDT_PRE_TIMEOUT_INT	0x08
	unsigned char       msg_flags;

	/* If set to true, this will request events the next time the
	   state machine is idle. */
	atomic_t            req_events;

	/* If true, run the state machine to completion on every send
	   call.  Generally used after a panic to make sure stuff goes
	   out. */
	int                 run_to_completion;

	/* The I/O port of a KCS interface. */
	int                 port;

	/* zero if no irq; */
	int                 irq;

	/* The physical and remapped memory addresses of a KCS interface. */
	unsigned long	    physaddr;
	unsigned char	    *addr;

	/* The timer for this kcs. */
	struct timer_list   kcs_timer;

	/* The time (in jiffies) the last timeout occurred at. */
	unsigned long       last_timeout_jiffies;

	/* Used to gracefully stop the timer without race conditions. */
	volatile int        stop_operation;
	volatile int        timer_stopped;

	/* The driver will disable interrupts when it gets into a
	   situation where it cannot handle messages due to lack of
	   memory.  Once that situation clears up, it will re-enable
	   interupts. */
	int                 interrupt_disabled;
};

static void kcs_restart_short_timer(struct kcs_info *kcs_info);

static void deliver_recv_msg(struct kcs_info *kcs_info, struct ipmi_smi_msg *msg)
{
	/* Deliver the message to the upper layer with the lock
           released. */
	spin_unlock(&(kcs_info->kcs_lock));
	ipmi_smi_msg_received(kcs_info->intf, msg);
	spin_lock(&(kcs_info->kcs_lock));
}

static void return_hosed_msg(struct kcs_info *kcs_info)
{
	struct ipmi_smi_msg *msg = kcs_info->curr_msg;

	/* Make it a reponse */
	msg->rsp[0] = msg->data[0] | 4;
	msg->rsp[1] = msg->data[1];
	msg->rsp[2] = 0xFF; /* Unknown error. */
	msg->rsp_size = 3;
			
	kcs_info->curr_msg = NULL;
	deliver_recv_msg(kcs_info, msg);
}

static enum kcs_result start_next_msg(struct kcs_info *kcs_info)
{
	int              rv;
	struct list_head *entry = NULL;
#ifdef DEBUG_TIMING
	struct timeval t;
#endif

	/* No need to save flags, we aleady have interrupts off and we
	   already hold the KCS lock. */
	spin_lock(&(kcs_info->msg_lock));
	
	/* Pick the high priority queue first. */
	if (! list_empty(&(kcs_info->hp_xmit_msgs))) {
		entry = kcs_info->hp_xmit_msgs.next;
	} else if (! list_empty(&(kcs_info->xmit_msgs))) {
		entry = kcs_info->xmit_msgs.next;
	}

	if (!entry) {
		kcs_info->curr_msg = NULL;
		rv = KCS_SM_IDLE;
	} else {
		int err;

		list_del(entry);
		kcs_info->curr_msg = list_entry(entry,
						struct ipmi_smi_msg,
						link);
#ifdef DEBUG_TIMING
		do_gettimeofday(&t);
		printk("**Start2: %d.%9.9d\n", t.tv_sec, t.tv_usec);
#endif
		err = start_kcs_transaction(kcs_info->kcs_sm,
					   kcs_info->curr_msg->data,
					   kcs_info->curr_msg->data_size);
		if (err) {
			return_hosed_msg(kcs_info);
		}

		rv = KCS_CALL_WITHOUT_DELAY;
	}
	spin_unlock(&(kcs_info->msg_lock));

	return rv;
}

static void start_enable_irq(struct kcs_info *kcs_info)
{
	unsigned char msg[2];

	/* If we are enabling interrupts, we have to tell the
	   BMC to use them. */
	msg[0] = (IPMI_NETFN_APP_REQUEST << 2);
	msg[1] = IPMI_GET_BMC_GLOBAL_ENABLES_CMD;

	start_kcs_transaction(kcs_info->kcs_sm, msg, 2);
	kcs_info->kcs_state = KCS_ENABLE_INTERRUPTS1;
}

static void start_clear_flags(struct kcs_info *kcs_info)
{
	unsigned char msg[3];

	/* Make sure the watchdog pre-timeout flag is not set at startup. */
	msg[0] = (IPMI_NETFN_APP_REQUEST << 2);
	msg[1] = IPMI_CLEAR_MSG_FLAGS_CMD;
	msg[2] = WDT_PRE_TIMEOUT_INT;

	start_kcs_transaction(kcs_info->kcs_sm, msg, 3);
	kcs_info->kcs_state = KCS_CLEARING_FLAGS;
}

/* When we have a situtaion where we run out of memory and cannot
   allocate messages, we just leave them in the BMC and run the system
   polled until we can allocate some memory.  Once we have some
   memory, we will re-enable the interrupt. */
static inline void disable_kcs_irq(struct kcs_info *kcs_info)
{
	if ((kcs_info->irq) && (!kcs_info->interrupt_disabled)) {
		disable_irq_nosync(kcs_info->irq);
		kcs_info->interrupt_disabled = 1;
	}
}

static inline void enable_kcs_irq(struct kcs_info *kcs_info)
{
	if ((kcs_info->irq) && (kcs_info->interrupt_disabled)) {
		enable_irq(kcs_info->irq);
		kcs_info->interrupt_disabled = 0;
	}
}

static void handle_flags(struct kcs_info *kcs_info)
{
	if (kcs_info->msg_flags & WDT_PRE_TIMEOUT_INT) {
		/* Watchdog pre-timeout */
		start_clear_flags(kcs_info);
		kcs_info->msg_flags &= ~WDT_PRE_TIMEOUT_INT;
		spin_unlock(&(kcs_info->kcs_lock));
		ipmi_smi_watchdog_pretimeout(kcs_info->intf);
		spin_lock(&(kcs_info->kcs_lock));
	} else if (kcs_info->msg_flags & RECEIVE_MSG_AVAIL) {
		/* Messages available. */
		kcs_info->curr_msg = ipmi_alloc_smi_msg();
		if (!kcs_info->curr_msg) {
			disable_kcs_irq(kcs_info);
			kcs_info->kcs_state = KCS_NORMAL;
			return;
		}
		enable_kcs_irq(kcs_info);

		kcs_info->curr_msg->data[0] = (IPMI_NETFN_APP_REQUEST << 2);
		kcs_info->curr_msg->data[1] = IPMI_GET_MSG_CMD;
		kcs_info->curr_msg->data_size = 2;

		start_kcs_transaction(kcs_info->kcs_sm,
				      kcs_info->curr_msg->data,
				      kcs_info->curr_msg->data_size);
		kcs_info->kcs_state = KCS_GETTING_MESSAGES;
	} else if (kcs_info->msg_flags & EVENT_MSG_BUFFER_FULL) {
		/* Events available. */
		kcs_info->curr_msg = ipmi_alloc_smi_msg();
		if (!kcs_info->curr_msg) {
			disable_kcs_irq(kcs_info);
			kcs_info->kcs_state = KCS_NORMAL;
			return;
		}
		enable_kcs_irq(kcs_info);

		kcs_info->curr_msg->data[0] = (IPMI_NETFN_APP_REQUEST << 2);
		kcs_info->curr_msg->data[1] = IPMI_READ_EVENT_MSG_BUFFER_CMD;
		kcs_info->curr_msg->data_size = 2;

		start_kcs_transaction(kcs_info->kcs_sm,
				      kcs_info->curr_msg->data,
				      kcs_info->curr_msg->data_size);
		kcs_info->kcs_state = KCS_GETTING_EVENTS;
	} else {
		kcs_info->kcs_state = KCS_NORMAL;
	}
}

static void handle_transaction_done(struct kcs_info *kcs_info)
{
	struct ipmi_smi_msg *msg;
#ifdef DEBUG_TIMING
	struct timeval t;

	do_gettimeofday(&t);
	printk("**Done: %d.%9.9d\n", t.tv_sec, t.tv_usec);
#endif
	switch (kcs_info->kcs_state) {
	case KCS_NORMAL:
		if (!kcs_info->curr_msg)
			break;
			
		kcs_info->curr_msg->rsp_size
			= kcs_get_result(kcs_info->kcs_sm,
					 kcs_info->curr_msg->rsp,
					 IPMI_MAX_MSG_LENGTH);
		
		/* Do this here becase deliver_recv_msg() releases the
		   lock, and a new message can be put in during the
		   time the lock is released. */
		msg = kcs_info->curr_msg;
		kcs_info->curr_msg = NULL;
		deliver_recv_msg(kcs_info, msg);
		break;
		
	case KCS_GETTING_FLAGS:
	{
		unsigned char msg[4];
		unsigned int  len;

		/* We got the flags from the KCS, now handle them. */
		len = kcs_get_result(kcs_info->kcs_sm, msg, 4);
		if (msg[2] != 0) {
			/* Error fetching flags, just give up for
			   now. */
			kcs_info->kcs_state = KCS_NORMAL;
		} else if (len < 3) {
			/* Hmm, no flags.  That's technically illegal, but
			   don't use uninitialized data. */
			kcs_info->kcs_state = KCS_NORMAL;
		} else {
			kcs_info->msg_flags = msg[3];
			handle_flags(kcs_info);
		}
		break;
	}

	case KCS_CLEARING_FLAGS:
	case KCS_CLEARING_FLAGS_THEN_SET_IRQ:
	{
		unsigned char msg[3];

		/* We cleared the flags. */
		kcs_get_result(kcs_info->kcs_sm, msg, 3);
		if (msg[2] != 0) {
			/* Error clearing flags */
			printk(KERN_WARNING
			       "ipmi_kcs: Error clearing flags: %2.2x\n",
			       msg[2]);
		}
		if (kcs_info->kcs_state == KCS_CLEARING_FLAGS_THEN_SET_IRQ)
			start_enable_irq(kcs_info);
		else
			kcs_info->kcs_state = KCS_NORMAL;
		break;
	}

	case KCS_GETTING_EVENTS:
	{
		kcs_info->curr_msg->rsp_size
			= kcs_get_result(kcs_info->kcs_sm,
					 kcs_info->curr_msg->rsp,
					 IPMI_MAX_MSG_LENGTH);

		/* Do this here becase deliver_recv_msg() releases the
		   lock, and a new message can be put in during the
		   time the lock is released. */
		msg = kcs_info->curr_msg;
		kcs_info->curr_msg = NULL;
		if (msg->rsp[2] != 0) {
			/* Error getting event, probably done. */
			msg->done(msg);

			/* Take off the event flag. */
			kcs_info->msg_flags &= ~EVENT_MSG_BUFFER_FULL;
		} else {
			deliver_recv_msg(kcs_info, msg);
		}
		handle_flags(kcs_info);
		break;
	}

	case KCS_GETTING_MESSAGES:
	{
		kcs_info->curr_msg->rsp_size
			= kcs_get_result(kcs_info->kcs_sm,
					 kcs_info->curr_msg->rsp,
					 IPMI_MAX_MSG_LENGTH);

		/* Do this here becase deliver_recv_msg() releases the
		   lock, and a new message can be put in during the
		   time the lock is released. */
		msg = kcs_info->curr_msg;
		kcs_info->curr_msg = NULL;
		if (msg->rsp[2] != 0) {
			/* Error getting event, probably done. */
			msg->done(msg);

			/* Take off the msg flag. */
			kcs_info->msg_flags &= ~RECEIVE_MSG_AVAIL;
		} else {
			deliver_recv_msg(kcs_info, msg);
		}
		handle_flags(kcs_info);
		break;
	}

	case KCS_ENABLE_INTERRUPTS1:
	{
		unsigned char msg[4];

		/* We got the flags from the KCS, now handle them. */
		kcs_get_result(kcs_info->kcs_sm, msg, 4);
		if (msg[2] != 0) {
			printk(KERN_WARNING
			       "ipmi_kcs: Could not enable interrupts"
			       ", failed get, using polled mode.\n");
			kcs_info->kcs_state = KCS_NORMAL;
		} else {
			msg[0] = (IPMI_NETFN_APP_REQUEST << 2);
			msg[1] = IPMI_SET_BMC_GLOBAL_ENABLES_CMD;
			msg[2] = msg[3] | 1; /* enable msg queue int */
			start_kcs_transaction(kcs_info->kcs_sm, msg,3);
			kcs_info->kcs_state = KCS_ENABLE_INTERRUPTS2;
		}
		break;
	}

	case KCS_ENABLE_INTERRUPTS2:
	{
		unsigned char msg[4];

		/* We got the flags from the KCS, now handle them. */
		kcs_get_result(kcs_info->kcs_sm, msg, 4);
		if (msg[2] != 0) {
			printk(KERN_WARNING
			       "ipmi_kcs: Could not enable interrupts"
			       ", failed set, using polled mode.\n");
		}
		kcs_info->kcs_state = KCS_NORMAL;
		break;
	}
	}
}

/* Called on timeouts and events.  Timeouts should pass the elapsed
   time, interrupts should pass in zero. */
static enum kcs_result kcs_event_handler(struct kcs_info *kcs_info, int time)
{
	enum kcs_result kcs_result;

 restart:
	/* There used to be a loop here that waited a little while
	   (around 25us) before giving up.  That turned out to be
	   pointless, the minimum delays I was seeing were in the 300us
	   range, which is far too long to wait in an interrupt.  So
	   we just run until the state machine tells us something
	   happened or it needs a delay. */
	kcs_result = kcs_event(kcs_info->kcs_sm, time);
	time = 0;
	while (kcs_result == KCS_CALL_WITHOUT_DELAY)
	{
		kcs_result = kcs_event(kcs_info->kcs_sm, 0);
	}

	if (kcs_result == KCS_TRANSACTION_COMPLETE)
	{
		handle_transaction_done(kcs_info);
		kcs_result = kcs_event(kcs_info->kcs_sm, 0);
	}
	else if (kcs_result == KCS_SM_HOSED)
	{
		if (kcs_info->curr_msg != NULL) {
			/* If we were handling a user message, format
                           a response to send to the upper layer to
                           tell it about the error. */
			return_hosed_msg(kcs_info);
		}
		kcs_result = kcs_event(kcs_info->kcs_sm, 0);
		kcs_info->kcs_state = KCS_NORMAL;
	}

	/* We prefer handling attn over new messages. */
	if (kcs_result == KCS_ATTN)
	{
		unsigned char msg[2];

		/* Got a attn, send down a get message flags to see
                   what's causing it.  It would be better to handle
                   this in the upper layer, but due to the way
                   interrupts work with the KCS, that's not really
                   possible. */
		msg[0] = (IPMI_NETFN_APP_REQUEST << 2);
		msg[1] = IPMI_GET_MSG_FLAGS_CMD;

		start_kcs_transaction(kcs_info->kcs_sm, msg, 2);
		kcs_info->kcs_state = KCS_GETTING_FLAGS;
		goto restart;
	}

	/* If we are currently idle, try to start the next message. */
	if (kcs_result == KCS_SM_IDLE) {
		kcs_result = start_next_msg(kcs_info);
		if (kcs_result != KCS_SM_IDLE)
			goto restart;
        }

	if ((kcs_result == KCS_SM_IDLE)
	    && (atomic_read(&kcs_info->req_events)))
	{
		/* We are idle and the upper layer requested that I fetch
		   events, so do so. */
		unsigned char msg[2];

		atomic_set(&kcs_info->req_events, 0);
		msg[0] = (IPMI_NETFN_APP_REQUEST << 2);
		msg[1] = IPMI_GET_MSG_FLAGS_CMD;

		start_kcs_transaction(kcs_info->kcs_sm, msg, 2);
		kcs_info->kcs_state = KCS_GETTING_FLAGS;
		goto restart;
	}

	return kcs_result;
}

static void sender(void                *send_info,
		   struct ipmi_smi_msg *msg,
		   int                 priority)
{
	struct kcs_info *kcs_info = (struct kcs_info *) send_info;
	enum kcs_result result;
	unsigned long   flags;
#ifdef DEBUG_TIMING
	struct timeval t;
#endif

	spin_lock_irqsave(&(kcs_info->msg_lock), flags);
#ifdef DEBUG_TIMING
	do_gettimeofday(&t);
	printk("**Enqueue: %d.%9.9d\n", t.tv_sec, t.tv_usec);
#endif

	if (kcs_info->run_to_completion) {
		/* If we are running to completion, then throw it in
		   the list and run transactions until everything is
		   clear.  Priority doesn't matter here. */
		list_add_tail(&(msg->link), &(kcs_info->xmit_msgs));

		/* We have to release the msg lock and claim the kcs
		   lock in this case, because of race conditions. */
		spin_unlock_irqrestore(&(kcs_info->msg_lock), flags);

		spin_lock_irqsave(&(kcs_info->kcs_lock), flags);
		result = kcs_event_handler(kcs_info, 0);
		while (result != KCS_SM_IDLE) {
			udelay(KCS_SHORT_TIMEOUT_USEC);
			result = kcs_event_handler(kcs_info,
						   KCS_SHORT_TIMEOUT_USEC);
		}
		spin_unlock_irqrestore(&(kcs_info->kcs_lock), flags);
		return;
	} else {
		if (priority > 0) {
			list_add_tail(&(msg->link), &(kcs_info->hp_xmit_msgs));
		} else {
			list_add_tail(&(msg->link), &(kcs_info->xmit_msgs));
		}
	}
	spin_unlock_irqrestore(&(kcs_info->msg_lock), flags);

	spin_lock_irqsave(&(kcs_info->kcs_lock), flags);
	if ((kcs_info->kcs_state == KCS_NORMAL)
	    && (kcs_info->curr_msg == NULL))
	{
		start_next_msg(kcs_info);
		kcs_restart_short_timer(kcs_info);
	}
	spin_unlock_irqrestore(&(kcs_info->kcs_lock), flags);
}

static void set_run_to_completion(void *send_info, int i_run_to_completion)
{
	struct kcs_info *kcs_info = (struct kcs_info *) send_info;
	enum kcs_result result;
	unsigned long   flags;

	spin_lock_irqsave(&(kcs_info->kcs_lock), flags);

	kcs_info->run_to_completion = i_run_to_completion;
	if (i_run_to_completion) {
		result = kcs_event_handler(kcs_info, 0);
		while (result != KCS_SM_IDLE) {
			udelay(KCS_SHORT_TIMEOUT_USEC);
			result = kcs_event_handler(kcs_info,
						   KCS_SHORT_TIMEOUT_USEC);
		}
	}

	spin_unlock_irqrestore(&(kcs_info->kcs_lock), flags);
}

static void request_events(void *send_info)
{
	struct kcs_info *kcs_info = (struct kcs_info *) send_info;

	atomic_set(&kcs_info->req_events, 1);
}

static int new_user(void *send_info)
{
	if (!try_inc_mod_count(THIS_MODULE))
		return -EBUSY;
	return 0;
}

static void user_left(void *send_info)
{
	MOD_DEC_USE_COUNT;
}

static int initialized = 0;

/* Must be called with interrupts off and with the kcs_lock held. */
static void kcs_restart_short_timer(struct kcs_info *kcs_info)
{
#ifdef CONFIG_HIGH_RES_TIMERS
	unsigned long jiffies_now;

	if (del_timer(&(kcs_info->kcs_timer))) {
		/* If we don't delete the timer, then it will go off
		   immediately, anyway.  So we only process if we
		   actually delete the timer. */

		/* We already have irqsave on, so no need for it
                   here. */
		read_lock(&xtime_lock);
		jiffies_now = jiffies;
		kcs_info->kcs_timer.expires = jiffies_now;

		kcs_info->kcs_timer.sub_expires
			= quick_update_jiffies_sub(jiffies_now);
		read_unlock(&xtime_lock);

		kcs_info->kcs_timer.sub_expires
			+= usec_to_arch_cycles(KCS_SHORT_TIMEOUT_USEC);
		while (kcs_info->kcs_timer.sub_expires >= cycles_per_jiffies) {
			kcs_info->kcs_timer.expires++;
			kcs_info->kcs_timer.sub_expires -= cycles_per_jiffies;
		}
		add_timer(&(kcs_info->kcs_timer));
	}
#endif
}

static void kcs_timeout(unsigned long data)
{
	struct kcs_info *kcs_info = (struct kcs_info *) data;
	enum kcs_result kcs_result;
	unsigned long   flags;
	unsigned long   jiffies_now;
	unsigned long   time_diff;
#ifdef DEBUG_TIMING
	struct timeval t;
#endif

	if (kcs_info->stop_operation) {
		kcs_info->timer_stopped = 1;
		return;
	}

	spin_lock_irqsave(&(kcs_info->kcs_lock), flags);
#ifdef DEBUG_TIMING
	do_gettimeofday(&t);
	printk("**Timer: %d.%9.9d\n", t.tv_sec, t.tv_usec);
#endif
	jiffies_now = jiffies;

	time_diff = ((jiffies_now - kcs_info->last_timeout_jiffies)
		     * KCS_USEC_PER_JIFFY);
	kcs_result = kcs_event_handler(kcs_info, time_diff);

	kcs_info->last_timeout_jiffies = jiffies_now;

	if ((kcs_info->irq) && (! kcs_info->interrupt_disabled)) {
		/* Running with interrupts, only do long timeouts. */
		kcs_info->kcs_timer.expires = jiffies + KCS_TIMEOUT_JIFFIES;
		goto do_add_timer;
	}

	/* If the state machine asks for a short delay, then shorten
           the timer timeout. */
#ifdef CONFIG_HIGH_RES_TIMERS
	if (kcs_result == KCS_CALL_WITH_DELAY) {
		kcs_info->kcs_timer.sub_expires
			+= usec_to_arch_cycles(KCS_SHORT_TIMEOUT_USEC);
		while (kcs_info->kcs_timer.sub_expires >= cycles_per_jiffies) {
			kcs_info->kcs_timer.expires++;
			kcs_info->kcs_timer.sub_expires -= cycles_per_jiffies;
		}
	} else {
		kcs_info->kcs_timer.expires = jiffies + KCS_TIMEOUT_JIFFIES;
		kcs_info->kcs_timer.sub_expires = 0;
	}
#else
	/* If requested, take the shortest delay possible */
	if (kcs_result == KCS_CALL_WITH_DELAY) {
		kcs_info->kcs_timer.expires = jiffies + 1;
	} else {
		kcs_info->kcs_timer.expires = jiffies + KCS_TIMEOUT_JIFFIES;
	}
#endif

 do_add_timer:
	add_timer(&(kcs_info->kcs_timer));
	spin_unlock_irqrestore(&(kcs_info->kcs_lock), flags);
}

static void kcs_irq_handler(int irq, void *data, struct pt_regs *regs)
{
	struct kcs_info *kcs_info = (struct kcs_info *) data;
	unsigned long   flags;
#ifdef DEBUG_TIMING
	struct timeval t;
#endif

	spin_lock_irqsave(&(kcs_info->kcs_lock), flags);
	if (kcs_info->stop_operation)
		goto out;

#ifdef DEBUG_TIMING
	do_gettimeofday(&t);
	printk("**Interrupt: %d.%9.9d\n", t.tv_sec, t.tv_usec);
#endif
	kcs_event_handler(kcs_info, 0);
 out:
	spin_unlock_irqrestore(&(kcs_info->kcs_lock), flags);
}

static struct ipmi_smi_handlers handlers =
{
	sender:		       sender,
	request_events:        request_events,
	new_user:	       new_user,
	user_left:	       user_left,
	set_run_to_completion: set_run_to_completion
};

static unsigned char ipmi_kcs_dev_rev;
static unsigned char ipmi_kcs_fw_rev_major;
static unsigned char ipmi_kcs_fw_rev_minor;
static unsigned char ipmi_version_major;
static unsigned char ipmi_version_minor;

extern int kcs_dbg;
static int ipmi_kcs_detect_hardware(unsigned int port,
				    unsigned char *addr,
				    struct kcs_data *data)
{
	unsigned char   msg[2];
	unsigned char   resp[IPMI_MAX_MSG_LENGTH];
	unsigned long   resp_len;
	enum kcs_result kcs_result;

	/* It's impossible for the KCS status register to be all 1's,
	   (assuming a properly functioning, self-initialized BMC)
	   but that's what you get from reading a bogus address, so we
	   test that first. */

	if (port) {
		if (inb(port+1) == 0xff) return -ENODEV; 
	} else { 
		if (readb(addr+1) == 0xff) return -ENODEV; 
	}

	/* Do a Get Device ID command, since it comes back with some
	   useful info. */
	msg[0] = IPMI_NETFN_APP_REQUEST << 2;
	msg[1] = IPMI_GET_DEVICE_ID_CMD;
	start_kcs_transaction(data, msg, 2);
	
	kcs_result = kcs_event(data, 0);
	for (;;)
	{
		if (kcs_result == KCS_CALL_WITH_DELAY) {
			udelay(100);
			kcs_result = kcs_event(data, 100);
		}
		else if (kcs_result == KCS_CALL_WITHOUT_DELAY)
		{
			kcs_result = kcs_event(data, 0);
		}
		else
			break;
	}
	if (kcs_result == KCS_SM_HOSED) {
		/* We couldn't get the state machine to run, so whatever's at
		   the port is probably not an IPMI KCS interface. */
		return -ENODEV;
	}
	/* Otherwise, we got some data. */
	resp_len = kcs_get_result(data, resp, IPMI_MAX_MSG_LENGTH);
	if (resp_len < 6)
		/* That's odd, it should be longer. */
		return -EINVAL;
	
	if ((resp[1] != IPMI_GET_DEVICE_ID_CMD) || (resp[2] != 0))
		/* That's odd, it shouldn't be able to fail. */
		return -EINVAL;
	
	ipmi_kcs_dev_rev = resp[4] & 0xf;
	ipmi_kcs_fw_rev_major = resp[5] & 0x7f;
	ipmi_kcs_fw_rev_minor = resp[6];
	ipmi_version_major = resp[7] & 0xf;
	ipmi_version_minor = resp[7] >> 4;

	return 0;
}

/* There can be 4 IO ports passed in (with or without IRQs), 4 addresses,
   a default IO port, and 1 ACPI/SPMI address.  That sets KCS_MAX_DRIVERS */

#define KCS_MAX_PARMS 4
#define KCS_MAX_DRIVERS ((KCS_MAX_PARMS * 2) + 2)
static struct kcs_info *kcs_infos[KCS_MAX_DRIVERS] =
{ NULL, NULL, NULL, NULL };

#define DEVICE_NAME "ipmi_kcs"

#define DEFAULT_IO_PORT 0xca2

static int kcs_trydefaults = 1;
static unsigned long kcs_addrs[KCS_MAX_PARMS] = { 0, 0, 0, 0 };
static int kcs_ports[KCS_MAX_PARMS] = { 0, 0, 0, 0 };
static int kcs_irqs[KCS_MAX_PARMS] = { 0, 0, 0, 0 };

MODULE_PARM(kcs_trydefaults, "i");
MODULE_PARM(kcs_addrs, "1-4l");
MODULE_PARM(kcs_irqs, "1-4i");
MODULE_PARM(kcs_ports, "1-4i");

/* Returns 0 if initialized, or negative on an error. */
static int init_one_kcs(int kcs_port, 
			int irq, 
			unsigned long kcs_physaddr,
			struct kcs_info **kcs)
{
	int		rv;
	struct kcs_info *new_kcs;

	/* Did anything get passed in at all?  Both == zero disables the
	   driver. */

	if (!(kcs_port || kcs_physaddr)) 
		return -ENODEV;
	
	/* Only initialize a port OR a physical address on this call.
	   Also, IRQs can go with either ports or addresses. */

	if (kcs_port && kcs_physaddr)
		return -EINVAL;

	new_kcs = kmalloc(sizeof(*new_kcs), GFP_KERNEL);
	if (!new_kcs) {
		printk(KERN_ERR "ipmi_kcs: out of memory\n");
		return -ENOMEM;
	}

	/* So we know not to free it unless we have allocated one. */
	new_kcs->kcs_sm = NULL;

	new_kcs->addr = NULL;
	new_kcs->physaddr = kcs_physaddr;
	new_kcs->port = kcs_port;

	if (kcs_port) {
		if (request_region(kcs_port, 2, DEVICE_NAME) == NULL) {
			kfree(new_kcs);
			printk(KERN_ERR 
			       "ipmi_kcs: can't reserve port @ 0x%4.4x\n",
		       	       kcs_port);
			return -EIO;
		}
	} else {
		if (request_mem_region(kcs_physaddr, 2, DEVICE_NAME) == NULL) {
			kfree(new_kcs);
			printk(KERN_ERR 
			       "ipmi_kcs: can't reserve memory @ 0x%lx\n",
		       	       kcs_physaddr);
			return -EIO;
		}
		if ((new_kcs->addr = ioremap(kcs_physaddr, 2)) == NULL) {
			kfree(new_kcs);
			printk(KERN_ERR 
			       "ipmi_kcs: can't remap memory at 0x%lx\n",
		       	       kcs_physaddr);
			return -EIO;
		}
	}

	new_kcs->kcs_sm = kmalloc(kcs_size(), GFP_KERNEL);
	if (!new_kcs->kcs_sm) {
		printk(KERN_ERR "ipmi_kcs: out of memory\n");
		rv = -ENOMEM;
		goto out_err;
	}
	init_kcs_data(new_kcs->kcs_sm, kcs_port, new_kcs->addr);
	spin_lock_init(&(new_kcs->kcs_lock));
	spin_lock_init(&(new_kcs->msg_lock));

	rv = ipmi_kcs_detect_hardware(kcs_port, new_kcs->addr, new_kcs->kcs_sm);
	if (rv) {
		if (kcs_port) 
			printk(KERN_ERR 
			       "ipmi_kcs: No KCS @ port 0x%4.4x\n", 
			       kcs_port);
		else
			printk(KERN_ERR 
			       "ipmi_kcs: No KCS @ addr 0x%lx\n", 
			       kcs_physaddr);
		goto out_err;
	}

	if (irq != 0) {
		rv = request_irq(irq,
				 kcs_irq_handler,
				 SA_INTERRUPT,
				 DEVICE_NAME,
				 new_kcs);
		if (rv) {
			printk(KERN_WARNING
			       "ipmi_kcs: %s unable to claim interrupt %d,"
			       " running polled\n",
			       DEVICE_NAME, irq);
			irq = 0;
		}
	}
	new_kcs->irq = irq;

	INIT_LIST_HEAD(&(new_kcs->xmit_msgs));
	INIT_LIST_HEAD(&(new_kcs->hp_xmit_msgs));
	new_kcs->curr_msg = NULL;
	atomic_set(&new_kcs->req_events, 0);
	new_kcs->run_to_completion = 0;

	start_clear_flags(new_kcs);

	if (irq) {
		new_kcs->kcs_state = KCS_CLEARING_FLAGS_THEN_SET_IRQ;

		printk(KERN_INFO 
		       "ipmi_kcs: Acquiring BMC @ port=0x%x irq=%d\n",
		       kcs_port, irq);

	} else {
		if (kcs_port)
			printk(KERN_INFO 
			       "ipmi_kcs: Acquiring BMC @ port=0x%x\n",
		       	       kcs_port);
		else
			printk(KERN_INFO 
			       "ipmi_kcs: Acquiring BMC @ addr=0x%lx\n",
		       	       kcs_physaddr);
	}

	rv = ipmi_register_smi(&handlers,
			       new_kcs,
			       ipmi_version_major,
			       ipmi_version_minor,
			       &(new_kcs->intf));
	if (rv) {
		free_irq(irq, new_kcs);
		printk(KERN_ERR 
		       "ipmi_kcs: Unable to register device: error %d\n",
		       rv);
		goto out_err;
	}

	new_kcs->interrupt_disabled = 0;
	new_kcs->timer_stopped = 0;
	new_kcs->stop_operation = 0;

	init_timer(&(new_kcs->kcs_timer));
	new_kcs->kcs_timer.data = (long) new_kcs;
	new_kcs->kcs_timer.function = kcs_timeout;
	new_kcs->last_timeout_jiffies = jiffies;
	new_kcs->kcs_timer.expires = jiffies + KCS_TIMEOUT_JIFFIES;
	add_timer(&(new_kcs->kcs_timer));

	*kcs = new_kcs;

	return 0;

 out_err:
	if (kcs_port) 
		release_region (kcs_port, 2);
	if (new_kcs->addr) 
		iounmap(new_kcs->addr);
	if (kcs_physaddr) 
		release_mem_region(kcs_physaddr, 2);
	if (new_kcs->kcs_sm)
		kfree(new_kcs->kcs_sm);
	kfree(new_kcs);
	return rv;
}

#ifdef CONFIG_ACPI_INTERPRETER

/* Retrieve the base physical address from ACPI tables.  Originally
   from Hewlett-Packard simple bmc.c, a GPL KCS driver. */

#include <linux/acpi.h>
/* A real hack, but everything's not there yet in 2.4. */
#include <acpi/acpi.h>
#include <acpi/actypes.h>
#include <acpi/actbl.h>

struct SPMITable {
	s8	Signature[4];
	u32	Length;
	u8	Revision;
	u8	Checksum;
	s8	OEMID[6];
	s8	OEMTableID[8];
	s8	OEMRevision[4];
	s8	CreatorID[4];
	s8	CreatorRevision[4];
	s16	InterfaceType;
	s16	SpecificationRevision;
	u8	InterruptType;
	u8	GPE;
	s16	Reserved;
	u64	GlobalSystemInterrupt;
	u8	BaseAddress[12];
	u8	UID[4];
} __attribute__ ((packed));

static unsigned long acpi_find_bmc(void)
{
	acpi_status       status;
	struct acpi_table_header *spmi;
	static unsigned long io_base = 0;

	if (io_base != 0)
		return io_base;

	status = acpi_get_firmware_table("SPMI", 1,
			ACPI_LOGICAL_ADDRESSING, &spmi);

	if (status != AE_OK) {
		printk(KERN_ERR "ipmi_kcs: SPMI table not found.\n");
		return 0;
	}

	memcpy(&io_base, ((struct SPMITable *)spmi)->BaseAddress,
			sizeof(io_base));
	
	return io_base;
}
#endif

static __init int init_ipmi_kcs(void)
{
	int		rv = 0;
	int		pos = 0;
	int		i = 0;
#ifdef CONFIG_ACPI_INTERPRETER
	unsigned long	physaddr = 0;
#endif

	if (initialized)
		return 0;
	initialized = 1;

	/* First do the "command-line" parameters */

	for (i=0; i < KCS_MAX_PARMS; i++) {
		rv = init_one_kcs(kcs_ports[i], 
				  kcs_irqs[i], 
				  0, 
				  &(kcs_infos[pos]));
		if (rv == 0)
			pos++;

		rv = init_one_kcs(0, 
				  kcs_irqs[i], 
				  kcs_addrs[i], 
				  &(kcs_infos[pos]));
		if (rv == 0)
			pos++;
	}

	/* Only try the defaults if enabled and resources are available
	   (because they weren't already specified above). */

	if (kcs_trydefaults) {
#ifdef CONFIG_ACPI_INTERPRETER
		if ((physaddr = acpi_find_bmc())) {
			if (!check_mem_region(physaddr, 2)) {
				rv = init_one_kcs(0, 
						  0, 
						  physaddr, 
						  &(kcs_infos[pos]));
				if (rv == 0)
					pos++;
			}
		}
#endif
		if (!check_region(DEFAULT_IO_PORT, 2)) {
			rv = init_one_kcs(DEFAULT_IO_PORT, 
					  0, 
					  0, 
					  &(kcs_infos[pos]));
			if (rv == 0)
				pos++;
		}
	}

	if (kcs_infos[0] == NULL) {
		printk("ipmi_kcs: Unable to find any KCS interfaces\n");
		return -ENODEV;
	} 

	return 0;
}
module_init(init_ipmi_kcs);

#ifdef MODULE
void __exit cleanup_one_kcs(struct kcs_info *to_clean)
{
	int           rv;
	unsigned long flags;

	if (! to_clean)
		return;

	/* Tell the timer and interrupt handlers that we are shutting
	   down. */
	spin_lock_irqsave(&(to_clean->kcs_lock), flags);
	spin_lock(&(to_clean->msg_lock));

	to_clean->stop_operation = 1;

	if (to_clean->irq != 0)
		free_irq(to_clean->irq, to_clean);
	if (to_clean->port) {
		printk(KERN_INFO 
		       "ipmi_kcs: Releasing BMC @ port=0x%x\n",
		       to_clean->port);
		release_region (to_clean->port, 2);
	}
	if (to_clean->addr) {
		printk(KERN_INFO 
		       "ipmi_kcs: Releasing BMC @ addr=0x%lx\n",
		       to_clean->physaddr);
		iounmap(to_clean->addr);
		release_mem_region(to_clean->physaddr, 2);
	}

	spin_unlock(&(to_clean->msg_lock));
	spin_unlock_irqrestore(&(to_clean->kcs_lock), flags);

	/* Wait for the timer to stop.  This avoids problems with race
	   conditions removing the timer here.  Hopefully this will be
	   long enough to avoid problems with interrupts still
	   running. */
	schedule_timeout(2);
	while (!to_clean->timer_stopped) {
		schedule_timeout(1);
	}

	rv = ipmi_unregister_smi(to_clean->intf);
	if (rv) {
		printk(KERN_ERR 
		       "ipmi_kcs: Unable to unregister device: errno=%d\n",
		       rv);
	}

	initialized = 0;

	kfree(to_clean->kcs_sm);
	kfree(to_clean);
}

static __exit void cleanup_ipmi_kcs(void)
{
	int i;

	if (!initialized)
		return;

	for (i=0; i<KCS_MAX_DRIVERS; i++) {
		cleanup_one_kcs(kcs_infos[i]);
	}
}
module_exit(cleanup_ipmi_kcs);
#else

/* Unfortunately, cmdline::get_options() only returns integers, not
   longs.  Since we need ulongs (64-bit physical addresses) parse the 
   comma-separated list manually.  Arguments can be one of these forms:
   m0xaabbccddeeff	A physical memory address without an IRQ
   m0xaabbccddeeff:cc	A physical memory address with an IRQ
   p0xaabb		An IO port without an IRQ
   p0xaabb:cc		An IO port with an IRQ
   nodefaults		Suppress trying the default IO port or ACPI address 

   For example, to pass one IO port with an IRQ, one address, and 
   suppress the use of the default IO port and ACPI address,
   use this option string: ipmi_kcs=p0xCA2:5,m0xFF5B0022,nodefaults

   Remember, ipmi_kcs_setup() is passed the string after the equal sign. */

static int __init ipmi_kcs_setup(char *str)
{
	unsigned long val;
	char *cur, *colon;
	int pos;

	pos = 0;
	
	cur = strsep(&str, ",");
	while ((cur) && (*cur) && (pos < KCS_MAX_PARMS)) {
		switch (*cur) {
		case 'n':
			if (strcmp(cur, "nodefaults") == 0)
				kcs_trydefaults = 0;
			else
				printk(KERN_INFO 
				       "ipmi_kcs: bad parameter value %s\n",
				       cur);
			break;
		
		case 'm':
		case 'p':
			val = simple_strtoul(cur + 1,
					     &colon,
					     0);
			if (*cur == 'p')
				kcs_ports[pos] = val;
			else
				kcs_addrs[pos] = val;
			if (*colon == ':') {
				val = simple_strtoul(colon + 1,
						     &colon,
						     0);
				kcs_irqs[pos] = val;
			}
			pos++;
			break;

		default:
			printk(KERN_INFO 
			       "ipmi_kcs: bad parameter value %s\n",
			       cur);
		}
		cur = strsep(&str, ",");
	}

	return 1;
}
__setup("ipmi_kcs=", ipmi_kcs_setup);
#endif

MODULE_LICENSE("GPL");
