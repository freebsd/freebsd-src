/*
 * ipmi_msghandler.c
 *
 * Incoming and outgoing message routing for an IPMI interface.
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

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <asm/system.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
#include <linux/rwsem.h>
#include <linux/slab.h>
#include <linux/ipmi.h>
#include <linux/ipmi_smi.h>
#include <linux/notifier.h>
#include <linux/init.h>

struct ipmi_recv_msg *ipmi_alloc_recv_msg(void);
static int ipmi_init_msghandler(void);

static int initialized = 0;

#define MAX_EVENTS_IN_QUEUE	25

/* Don't let a message sit in a queue forever, always time it with at lest
   the max message timer. */
#define MAX_MSG_TIMEOUT		60000

struct ipmi_user
{
	struct list_head link;

	/* The upper layer that handles receive messages. */
	struct ipmi_user_hndl *handler;
	void             *handler_data;

	/* The interface this user is bound to. */
	ipmi_smi_t intf;

	/* Does this interface receive IPMI events? */
	int gets_events;
};

struct cmd_rcvr
{
	struct list_head link;

	ipmi_user_t   user;
	unsigned char netfn;
	unsigned char cmd;
};

struct seq_table
{
	int                  inuse : 1;

	unsigned long        timeout;
	unsigned long        orig_timeout;
	unsigned int         retries_left;

	/* To verify on an incoming send message response that this is
           the message that the response is for, we keep a sequence id
           and increment it every time we send a message. */
	long                 seqid;

	/* This is held so we can properly respond to the message on a
           timeout, and it is used to hold the temporary data for
           retransmission, too. */
	struct ipmi_recv_msg *recv_msg;
};

/* Store the information in a msgid (long) to allow us to find a
   sequence table entry from the msgid. */
#define STORE_SEQ_IN_MSGID(seq, seqid) (((seq&0xff)<<26) | (seqid&0x3ffffff))

#define GET_SEQ_FROM_MSGID(msgid, seq, seqid) \
	do {								\
		seq = ((msgid >> 26) & 0x3f);				\
		seqid = (msgid & 0x3fffff);				\
        } while(0)

#define NEXT_SEQID(seqid) (((seqid) + 1) & 0x3fffff)


#define IPMI_IPMB_NUM_SEQ	64
struct ipmi_smi
{
	/* The list of upper layers that are using me.  We read-lock
           this when delivering messages to the upper layer to keep
           the user from going away while we are processing the
           message.  This means that you cannot add or delete a user
           from the receive callback. */
	rwlock_t                users_lock;
	struct list_head        users;

	/* The IPMI version of the BMC on the other end. */
	unsigned char       version_major;
	unsigned char       version_minor;

	/* This is the lower-layer's sender routine. */
	struct ipmi_smi_handlers *handlers;
	void                     *send_info;

	/* A table of sequence numbers for this interface.  We use the
           sequence numbers for IPMB messages that go out of the
           interface to match them up with their responses.  A routine
           is called periodically to time the items in this list. */
	spinlock_t       seq_lock;
	struct seq_table seq_table[IPMI_IPMB_NUM_SEQ];
	int curr_seq;

	/* Messages that were delayed for some reason (out of memory,
           for instance), will go in here to be processed later in a
           periodic timer interrupt. */
	spinlock_t       waiting_msgs_lock;
	struct list_head waiting_msgs;

	/* The list of command receivers that are registered for commands
	   on this interface. */
	rwlock_t	 cmd_rcvr_lock;
	struct list_head cmd_rcvrs;

	/* Events that were queues because no one was there to receive
           them. */
	spinlock_t       events_lock; /* For dealing with event stuff. */
	struct list_head waiting_events;
	unsigned int     waiting_events_count; /* How many events in queue? */

	/* This will be non-null if someone registers to receive all
	   IPMI commands (this is for interface emulation).  There
	   may not be any things in the cmd_rcvrs list above when
	   this is registered. */
	ipmi_user_t all_cmd_rcvr;

	/* My slave address.  This is initialized to IPMI_BMC_SLAVE_ADDR,
	   but may be changed by the user. */
	unsigned char my_address;

	/* My LUN.  This should generally stay the SMS LUN, but just in
	   case... */
	unsigned char my_lun;
};

int
ipmi_register_all_cmd_rcvr(ipmi_user_t user)
{
	unsigned long flags;
	int           rv = -EBUSY;

	write_lock_irqsave(&(user->intf->users_lock), flags);
	write_lock(&(user->intf->cmd_rcvr_lock));
	if ((user->intf->all_cmd_rcvr == NULL)
	    && (list_empty(&(user->intf->cmd_rcvrs))))
	{
		user->intf->all_cmd_rcvr = user;
		rv = 0;
	}
	write_unlock(&(user->intf->cmd_rcvr_lock));
	write_unlock_irqrestore(&(user->intf->users_lock), flags);
	return rv;
}

int
ipmi_unregister_all_cmd_rcvr(ipmi_user_t user)
{
	unsigned long flags;
	int           rv = -EINVAL;

	write_lock_irqsave(&(user->intf->users_lock), flags);
	write_lock(&(user->intf->cmd_rcvr_lock));
	if (user->intf->all_cmd_rcvr == user)
	{
		user->intf->all_cmd_rcvr = NULL;
		rv = 0;
	}
	write_unlock(&(user->intf->cmd_rcvr_lock));
	write_unlock_irqrestore(&(user->intf->users_lock), flags);
	return rv;
}


#define MAX_IPMI_INTERFACES 4
static ipmi_smi_t ipmi_interfaces[MAX_IPMI_INTERFACES];

/* Used to keep interfaces from going away while operations are
   operating on interfaces.  Grab read if you are not modifying the
   interfaces, write if you are. */
static DECLARE_RWSEM(interfaces_sem);

/* Directly protects the ipmi_interfaces data structure.  This is
   claimed in the timer interrupt. */
static spinlock_t interfaces_lock = SPIN_LOCK_UNLOCKED;

/* List of watchers that want to know when smi's are added and
   deleted. */
static struct list_head smi_watchers = LIST_HEAD_INIT(smi_watchers);
static DECLARE_RWSEM(smi_watchers_sem);

int ipmi_smi_watcher_register(struct ipmi_smi_watcher *watcher)
{
	int i;

	down_read(&interfaces_sem);
	down_write(&smi_watchers_sem);
	list_add(&(watcher->link), &smi_watchers);
	for (i=0; i<MAX_IPMI_INTERFACES; i++) {
		if (ipmi_interfaces[i] != NULL) {
			watcher->new_smi(i);
		}
	}
	up_write(&smi_watchers_sem);
	up_read(&interfaces_sem);
	return 0;
}

int ipmi_smi_watcher_unregister(struct ipmi_smi_watcher *watcher)
{
	down_write(&smi_watchers_sem);
	list_del(&(watcher->link));
	up_write(&smi_watchers_sem);
	return 0;
}

int
ipmi_addr_equal(struct ipmi_addr *addr1, struct ipmi_addr *addr2)
{
	if (addr1->addr_type != addr2->addr_type)
		return 0;

	if (addr1->channel != addr2->channel)
		return 0;

	if (addr1->addr_type == IPMI_SYSTEM_INTERFACE_ADDR_TYPE) {
		struct ipmi_system_interface_addr *smi_addr1
		    = (struct ipmi_system_interface_addr *) addr1;
		struct ipmi_system_interface_addr *smi_addr2
		    = (struct ipmi_system_interface_addr *) addr2;
		return (smi_addr1->lun == smi_addr2->lun);
	}

	if ((addr1->addr_type == IPMI_IPMB_ADDR_TYPE)
	    || (addr1->addr_type == IPMI_IPMB_BROADCAST_ADDR_TYPE))
	{
		struct ipmi_ipmb_addr *ipmb_addr1
		    = (struct ipmi_ipmb_addr *) addr1;
		struct ipmi_ipmb_addr *ipmb_addr2
		    = (struct ipmi_ipmb_addr *) addr2;

		return ((ipmb_addr1->slave_addr == ipmb_addr2->slave_addr)
			&& (ipmb_addr1->lun == ipmb_addr2->lun));
	}

	return 1;
}

int ipmi_validate_addr(struct ipmi_addr *addr, int len)
{
	if (len < sizeof(struct ipmi_system_interface_addr)) {
		return -EINVAL;
	}

	if (addr->addr_type == IPMI_SYSTEM_INTERFACE_ADDR_TYPE) {
		if (addr->channel != IPMI_BMC_CHANNEL)
			return -EINVAL;
		return 0;
	}

	if ((addr->channel == IPMI_BMC_CHANNEL)
	    || (addr->channel >= IPMI_NUM_CHANNELS)
	    || (addr->channel < 0))
		return -EINVAL;

	if ((addr->addr_type == IPMI_IPMB_ADDR_TYPE)
	    || (addr->addr_type == IPMI_IPMB_BROADCAST_ADDR_TYPE))
	{
		if (len < sizeof(struct ipmi_ipmb_addr)) {
			return -EINVAL;
		}
		return 0;
	}

	return -EINVAL;
}

unsigned int ipmi_addr_length(int addr_type)
{
	if (addr_type == IPMI_SYSTEM_INTERFACE_ADDR_TYPE)
		return sizeof(struct ipmi_system_interface_addr);

	if ((addr_type == IPMI_IPMB_ADDR_TYPE)
	    || (addr_type == IPMI_IPMB_BROADCAST_ADDR_TYPE))
	{
		return sizeof(struct ipmi_ipmb_addr);
	}

	return 0;
}

static void deliver_response(struct ipmi_recv_msg *msg)
{
    msg->user->handler->ipmi_recv_hndl(msg, msg->user->handler_data);
}

/* Find the next sequence number not being used and add the given
   message with the given timeout to the sequence table.  This must be
   called with the interface's seq_lock held. */
static int intf_next_seq(ipmi_smi_t           intf,
			 struct ipmi_recv_msg *recv_msg,
			 unsigned long        timeout,
			 int                  retries,
			 unsigned char        *seq,
			 long                 *seqid)
{
	int          rv = 0;
	unsigned int i;

	for (i=intf->curr_seq;
	     (i+1)%IPMI_IPMB_NUM_SEQ != intf->curr_seq;
	     i=(i+1)%IPMI_IPMB_NUM_SEQ)
	{
		if (! intf->seq_table[i].inuse)
			break;
	}

	if (! intf->seq_table[i].inuse) {
		intf->seq_table[i].recv_msg = recv_msg;

		/* Start with the maximum timeout, when the send response
		   comes in we will start the real timer. */
		intf->seq_table[i].timeout = MAX_MSG_TIMEOUT;
		intf->seq_table[i].orig_timeout = timeout;
		intf->seq_table[i].retries_left = retries;
		intf->seq_table[i].inuse = 1;
		intf->seq_table[i].seqid = NEXT_SEQID(intf->seq_table[i].seqid);
		*seq = i;
		*seqid = intf->seq_table[i].seqid;
		intf->curr_seq = (i+1)%IPMI_IPMB_NUM_SEQ;
	} else {
		rv = -EAGAIN;
	}
	
	return rv;
}

/* Return the receive message for the given sequence number and
   release the sequence number so it can be reused.  Some other data
   is passed in to be sure the message matches up correctly (to help
   guard against message coming in after their timeout and the
   sequence number being reused). */
static int intf_find_seq(ipmi_smi_t           intf,
			 unsigned char        seq,
			 short                channel,
			 unsigned char        cmd,
			 unsigned char        netfn,
			 struct ipmi_addr     *addr,
			 struct ipmi_recv_msg **recv_msg)
{
	int           rv = -ENODEV;
	unsigned long flags;

	if (seq >= IPMI_IPMB_NUM_SEQ)
		return -EINVAL;

	spin_lock_irqsave(&(intf->seq_lock), flags);
	if (intf->seq_table[seq].inuse) {
		struct ipmi_recv_msg *msg = intf->seq_table[seq].recv_msg;

		if ((msg->addr.channel == channel)
		    && (msg->msg.cmd == cmd)
		    && (msg->msg.netfn == netfn)
		    && (ipmi_addr_equal(addr, &(msg->addr))))
		{
			*recv_msg = msg;
			intf->seq_table[seq].inuse = 0;
			rv = 0;
		}
	}
	spin_unlock_irqrestore(&(intf->seq_lock), flags);

	return rv;
}


/* Start the timer for a specific sequence table entry. */
static int intf_start_seq_timer(ipmi_smi_t           intf,
				long                 msgid)
{
	int           rv = -ENODEV;
	unsigned long flags;
	unsigned char seq;
	unsigned long seqid;


	GET_SEQ_FROM_MSGID(msgid, seq, seqid);

	spin_lock_irqsave(&(intf->seq_lock), flags);
	/* We do this verification because the user can be deleted
           while a message is outstanding. */
	if ((intf->seq_table[seq].inuse)
	    && (intf->seq_table[seq].seqid == seqid))
	{
		struct seq_table *ent = &(intf->seq_table[seq]);
		ent->timeout = ent->orig_timeout;
	}
	spin_unlock_irqrestore(&(intf->seq_lock), flags);

	return rv;
}


int ipmi_create_user(unsigned int          if_num,
		     struct ipmi_user_hndl *handler,
		     void                  *handler_data,
		     ipmi_user_t           *user)
{
	unsigned long flags;
	ipmi_user_t   new_user;
	int           rv = 0;

	/* There is no module usecount here, because it's not
           required.  Since this can only be used by and called from
           other modules, they will implicitly use this module, and
           thus this can't be removed unless the other modules are
           removed. */

	if (handler == NULL)
		return -EINVAL;

	/* Make sure the driver is actually initialized, this handles
	   problems with initialization order. */
	if (!initialized) {
		rv = ipmi_init_msghandler();
		if (rv)
			return rv;

		/* The init code doesn't return an error if it was turned
		   off, but it won't initialize.  Check that. */
		if (!initialized)
			return -ENODEV;
	}

	new_user = kmalloc(sizeof(*new_user), GFP_KERNEL);
	if (! new_user)
		return -ENOMEM;

	down_read(&interfaces_sem);
	if ((if_num > MAX_IPMI_INTERFACES) || ipmi_interfaces[if_num] == NULL)
	{
		rv = -EINVAL;
		goto out_unlock;
	}

	new_user->handler = handler;
	new_user->handler_data = handler_data;
	new_user->intf = ipmi_interfaces[if_num];
	new_user->gets_events = 0;

	rv = new_user->intf->handlers->new_user(new_user->intf->send_info);
	if (rv)
		goto out_unlock;

	write_lock_irqsave(&(new_user->intf->users_lock), flags);
	list_add_tail(&(new_user->link), &(new_user->intf->users));
	write_unlock_irqrestore(&(new_user->intf->users_lock), flags);

 out_unlock:	
	if (rv) {
		kfree(new_user);
	} else {
		*user = new_user;
	}

	up_read(&interfaces_sem);
	return rv;
}

static int ipmi_destroy_user_nolock(ipmi_user_t user)
{
	int              rv = -ENODEV;
	ipmi_user_t      t_user;
	struct list_head *entry, *entry2;
	int              i;
	unsigned long    flags;

	/* Find the user and delete them from the list. */
	list_for_each(entry, &(user->intf->users)) {
		t_user = list_entry(entry, struct ipmi_user, link);
		if (t_user == user) {
			list_del(entry);
			rv = 0;
			break;
		}
	}

	if (rv) {
		goto out_unlock;
	}

	/* Remove the user from the interfaces sequence table. */
	spin_lock_irqsave(&(user->intf->seq_lock), flags);
	for (i=0; i<IPMI_IPMB_NUM_SEQ; i++) {
		if (user->intf->seq_table[i].inuse
		    && (user->intf->seq_table[i].recv_msg->user == user))
		{
			user->intf->seq_table[i].inuse = 0;
		}
	}
	spin_unlock_irqrestore(&(user->intf->seq_lock), flags);

	/* Remove the user from the command receiver's table. */
	write_lock_irqsave(&(user->intf->cmd_rcvr_lock), flags);
	list_for_each_safe(entry, entry2, &(user->intf->cmd_rcvrs)) {
		struct cmd_rcvr *rcvr;
		rcvr = list_entry(entry, struct cmd_rcvr, link);
		if (rcvr->user == user) {
			list_del(entry);
			kfree(rcvr);
		}
	}
	write_unlock_irqrestore(&(user->intf->cmd_rcvr_lock), flags);

	kfree(user);

 out_unlock:

	return rv;
}

int ipmi_destroy_user(ipmi_user_t user)
{
	int           rv;
	ipmi_smi_t    intf = user->intf;
	unsigned long flags;

	down_read(&interfaces_sem);
	write_lock_irqsave(&(intf->users_lock), flags);
	rv = ipmi_destroy_user_nolock(user);
	if (!rv)
		intf->handlers->user_left(intf->send_info);
		
	write_unlock_irqrestore(&(intf->users_lock), flags);
	up_read(&interfaces_sem);
	return rv;
}

void ipmi_get_version(ipmi_user_t   user,
		      unsigned char *major,
		      unsigned char *minor)
{
	*major = user->intf->version_major;
	*minor = user->intf->version_minor;
}

void ipmi_set_my_address(ipmi_user_t   user,
			 unsigned char address)
{
	user->intf->my_address = address;
}

unsigned char ipmi_get_my_address(ipmi_user_t user)
{
	return user->intf->my_address;
}

void ipmi_set_my_LUN(ipmi_user_t   user,
		     unsigned char LUN)
{
	user->intf->my_lun = LUN & 0x3;
}

unsigned char ipmi_get_my_LUN(ipmi_user_t user)
{
	return user->intf->my_lun;
}

int ipmi_set_gets_events(ipmi_user_t user, int val)
{
	unsigned long         flags;
	struct list_head      *e, *e2;
	struct ipmi_recv_msg  *msg;

	read_lock(&(user->intf->users_lock));
	spin_lock_irqsave(&(user->intf->events_lock), flags);
	user->gets_events = val;

	if (val) {
		/* Deliver any queued events. */
		list_for_each_safe(e, e2, &(user->intf->waiting_events)) {
			msg = list_entry(e, struct ipmi_recv_msg, link);
			list_del(e);
			msg->user = user;
			deliver_response(msg);
		}
	}
	
	spin_unlock_irqrestore(&(user->intf->events_lock), flags);
	read_unlock(&(user->intf->users_lock));

	return 0;
}

int ipmi_register_for_cmd(ipmi_user_t   user,
			  unsigned char netfn,
			  unsigned char cmd)
{
	struct list_head *entry;
	unsigned long    flags;
	struct cmd_rcvr  *rcvr;
	int              rv = 0;


	rcvr = kmalloc(sizeof(*rcvr), GFP_KERNEL);
	if (! rcvr)
		return -ENOMEM;

	read_lock(&(user->intf->users_lock));
	write_lock_irqsave(&(user->intf->cmd_rcvr_lock), flags);
	if (user->intf->all_cmd_rcvr != NULL) {
		rv = -EBUSY;
		goto out_unlock;
	}

	/* Make sure the command/netfn is not already registered. */
	list_for_each(entry, &(user->intf->cmd_rcvrs)) {
		struct cmd_rcvr *cmp;
		cmp = list_entry(entry, struct cmd_rcvr, link);
		if ((cmp->netfn == netfn) && (cmp->cmd == cmd)) {
			rv = -EBUSY;
			break;
		}
	}

	if (! rv) {
		rcvr->cmd = cmd;
		rcvr->netfn = netfn;
		rcvr->user = user;
		list_add_tail(&(rcvr->link), &(user->intf->cmd_rcvrs));
	}
 out_unlock:
	write_unlock_irqrestore(&(user->intf->cmd_rcvr_lock), flags);
	read_unlock(&(user->intf->users_lock));

	if (rv)
		kfree(rcvr);

	return rv;
}

int ipmi_unregister_for_cmd(ipmi_user_t   user,
			    unsigned char netfn,
			    unsigned char cmd)
{
	struct list_head *entry;
	unsigned long    flags;
	struct cmd_rcvr  *rcvr;
	int              rv = -ENOENT;

	read_lock(&(user->intf->users_lock));
	write_lock_irqsave(&(user->intf->cmd_rcvr_lock), flags);
	/* Make sure the command/netfn is not already registered. */
	list_for_each(entry, &(user->intf->cmd_rcvrs)) {
		rcvr = list_entry(entry, struct cmd_rcvr, link);
		if ((rcvr->netfn == netfn) && (rcvr->cmd == cmd)) {
			rv = 0;
			list_del(entry);
			kfree(rcvr);
			break;
		}
	}
	write_unlock_irqrestore(&(user->intf->cmd_rcvr_lock), flags);
	read_unlock(&(user->intf->users_lock));

	return rv;
}

static unsigned char
ipmb_checksum(unsigned char *data, int size)
{
	unsigned char csum = 0;
	
	for (; size > 0; size--, data++)
		csum += *data;

	return -csum;
}

static inline void format_ipmb_msg(struct ipmi_smi_msg   *smi_msg,
				   struct ipmi_msg       *msg,
				   struct ipmi_ipmb_addr *ipmb_addr,
				   long                  msgid,
				   unsigned char         ipmb_seq,
				   int                   broadcast,
				   unsigned char         source_address,
				   unsigned char         source_lun)
{
	int i = broadcast;

	/* Format the IPMB header data. */
	smi_msg->data[0] = (IPMI_NETFN_APP_REQUEST << 2);
	smi_msg->data[1] = IPMI_SEND_MSG_CMD;
	smi_msg->data[2] = ipmb_addr->channel;
	if (broadcast)
		smi_msg->data[3] = 0;
	smi_msg->data[i+3] = ipmb_addr->slave_addr;
	smi_msg->data[i+4] = (msg->netfn << 2) | (ipmb_addr->lun & 0x3);
	smi_msg->data[i+5] = ipmb_checksum(&(smi_msg->data[i+3]), 2);
	smi_msg->data[i+6] = source_address;
	smi_msg->data[i+7] = (ipmb_seq << 2) | source_lun;
	smi_msg->data[i+8] = msg->cmd;

	/* Now tack on the data to the message. */
	if (msg->data_len > 0)
		memcpy(&(smi_msg->data[i+9]), msg->data,
		       msg->data_len);
	smi_msg->data_size = msg->data_len + 9;

	/* Now calculate the checksum and tack it on. */
	smi_msg->data[i+smi_msg->data_size]
		= ipmb_checksum(&(smi_msg->data[i+6]),
				smi_msg->data_size-6);

	/* Add on the checksum size and the offset from the
	   broadcast. */
	smi_msg->data_size += 1 + i;

	smi_msg->msgid = msgid;
}

/* Separate from ipmi_request so that the user does not have to be
   supplied in certain circumstances (mainly at panic time).  If
   messages are supplied, they will be freed, even if an error
   occurs. */
static inline int i_ipmi_request(ipmi_user_t          user,
				 ipmi_smi_t           intf,
				 struct ipmi_addr     *addr,
				 long                 msgid,
				 struct ipmi_msg      *msg,
				 void                 *supplied_smi,
				 struct ipmi_recv_msg *supplied_recv,
				 int                  priority,
				 unsigned char        source_address,
				 unsigned char        source_lun)
{
	int                  rv = 0;
	struct ipmi_smi_msg  *smi_msg;
	struct ipmi_recv_msg *recv_msg;
	unsigned long        flags;


	if (supplied_recv) {
		recv_msg = supplied_recv;
	} else {
		recv_msg = ipmi_alloc_recv_msg();
		if (recv_msg == NULL) {
			return -ENOMEM;
		}
	}

	if (supplied_smi) {
		smi_msg = (struct ipmi_smi_msg *) supplied_smi;
	} else {
		smi_msg = ipmi_alloc_smi_msg();
		if (smi_msg == NULL) {
			ipmi_free_recv_msg(recv_msg);
			return -ENOMEM;
		}
	}

	if (addr->channel > IPMI_NUM_CHANNELS) {
	    rv = -EINVAL;
	    goto out_err;
	}

	recv_msg->user = user;
	recv_msg->msgid = msgid;
	/* Store the message to send in the receive message so timeout
	   responses can get the proper response data. */
	recv_msg->msg = *msg;

	if (addr->addr_type == IPMI_SYSTEM_INTERFACE_ADDR_TYPE) {
		struct ipmi_system_interface_addr *smi_addr;


		smi_addr = (struct ipmi_system_interface_addr *) addr;
		if (smi_addr->lun > 3)
			return -EINVAL;

		memcpy(&recv_msg->addr, smi_addr, sizeof(*smi_addr));

		if ((msg->netfn == IPMI_NETFN_APP_REQUEST)
		    && ((msg->cmd == IPMI_SEND_MSG_CMD)
			|| (msg->cmd == IPMI_GET_MSG_CMD)
			|| (msg->cmd == IPMI_READ_EVENT_MSG_BUFFER_CMD)))
		{
			/* We don't let the user do these, since we manage
			   the sequence numbers. */
			rv = -EINVAL;
			goto out_err;
		}

		if ((msg->data_len + 2) > IPMI_MAX_MSG_LENGTH) {
			rv = -EMSGSIZE;
			goto out_err;
		}

		smi_msg->data[0] = (msg->netfn << 2) | (smi_addr->lun & 0x3);
		smi_msg->data[1] = msg->cmd;
		smi_msg->msgid = msgid;
		smi_msg->user_data = recv_msg;
		if (msg->data_len > 0)
			memcpy(&(smi_msg->data[2]), msg->data, msg->data_len);
		smi_msg->data_size = msg->data_len + 2;
	} else if ((addr->addr_type == IPMI_IPMB_ADDR_TYPE)
		   || (addr->addr_type == IPMI_IPMB_BROADCAST_ADDR_TYPE))
	{
		struct ipmi_ipmb_addr *ipmb_addr;
		unsigned char         ipmb_seq;
		long                  seqid;
		int                   broadcast;
		int                   retries;

		if (addr == NULL) {
			rv = -EINVAL;
			goto out_err;
		}

		if (addr->addr_type == IPMI_IPMB_BROADCAST_ADDR_TYPE) {
		    /* Broadcasts add a zero at the beginning of the
		       message, but otherwise is the same as an IPMB
		       address. */
		    addr->addr_type = IPMI_IPMB_ADDR_TYPE;
		    broadcast = 1;
		    retries = 0; /* Don't retry broadcasts. */
		} else {
		    broadcast = 0;
		    retries = 4;
		}

		/* 9 for the header and 1 for the checksum, plus
                   possibly one for the broadcast. */
		if ((msg->data_len + 10 + broadcast) > IPMI_MAX_MSG_LENGTH) {
			rv = -EMSGSIZE;
			goto out_err;
		}

		ipmb_addr = (struct ipmi_ipmb_addr *) addr;
		if (ipmb_addr->lun > 3) {
			rv = -EINVAL;
			goto out_err;
		}

		memcpy(&recv_msg->addr, ipmb_addr, sizeof(*ipmb_addr));

		if (recv_msg->msg.netfn & 0x1) {
			/* It's a response, so use the user's sequence
                           from msgid. */
			format_ipmb_msg(smi_msg, msg, ipmb_addr, msgid,
					msgid, broadcast,
					source_address, source_lun);
		} else {
			/* It's a command, so get a sequence for it. */

			spin_lock_irqsave(&(intf->seq_lock), flags);

			/* Create a sequence number with a 1 second
                           timeout and 4 retries. */
			/* FIXME - magic number for the timeout. */
			rv = intf_next_seq(intf,
					   recv_msg,
					   1000,
					   retries,
					   &ipmb_seq,
					   &seqid);
			if (rv) {
				/* We have used up all the sequence numbers,
				   probably, so abort. */
				spin_unlock_irqrestore(&(intf->seq_lock),
						       flags);
				goto out_err;
			}

			/* Store the sequence number in the message,
                           so that when the send message response
                           comes back we can start the timer. */
			format_ipmb_msg(smi_msg, msg, ipmb_addr,
					STORE_SEQ_IN_MSGID(ipmb_seq, seqid),
					ipmb_seq, broadcast,
					source_address, source_lun);

			/* Copy the message into the recv message data, so we
			   can retransmit it later if necessary. */
			memcpy(recv_msg->msg_data, smi_msg->data,
			       smi_msg->data_size);
			recv_msg->msg.data = recv_msg->msg_data;
			recv_msg->msg.data_len = smi_msg->data_size;

			/* We don't unlock until here, because we need
                           to copy the completed message into the
                           recv_msg before we release the lock.
                           Otherwise, race conditions may bite us.  I
                           know that's pretty paranoid, but I prefer
                           to be correct. */
			spin_unlock_irqrestore(&(intf->seq_lock), flags);
		}
	} else {
	    /* Unknown address type. */
	    rv = -EINVAL;
	    goto out_err;
	}

#if DEBUG_MSGING
	{
	    int m;
	    for (m=0; m<smi_msg->data_size; m++)
		printk(" %2.2x", smi_msg->data[m]);
	    printk("\n");
	}
#endif
	intf->handlers->sender(intf->send_info, smi_msg, priority);

	return 0;

 out_err:
	ipmi_free_smi_msg(smi_msg);
	ipmi_free_recv_msg(recv_msg);
	return rv;
}

int ipmi_request(ipmi_user_t      user,
		 struct ipmi_addr *addr,
		 long             msgid,
		 struct ipmi_msg  *msg,
		 int              priority)
{
	return i_ipmi_request(user,
			      user->intf,
			      addr,
			      msgid,
			      msg,
			      NULL, NULL,
			      priority,
			      user->intf->my_address,
			      user->intf->my_lun);
}

int ipmi_request_supply_msgs(ipmi_user_t          user,
			     struct ipmi_addr     *addr,
			     long                 msgid,
			     struct ipmi_msg      *msg,
			     void                 *supplied_smi,
			     struct ipmi_recv_msg *supplied_recv,
			     int                  priority)
{
	return i_ipmi_request(user,
			      user->intf,
			      addr,
			      msgid,
			      msg,
			      supplied_smi,
			      supplied_recv,
			      priority,
			      user->intf->my_address,
			      user->intf->my_lun);
}

int ipmi_request_with_source(ipmi_user_t      user,
			     struct ipmi_addr *addr,
			     long             msgid,
			     struct ipmi_msg  *msg,
			     int              priority,
			     unsigned char    source_address,
			     unsigned char    source_lun)
{
	return i_ipmi_request(user,
			      user->intf,
			      addr,
			      msgid,
			      msg,
			      NULL, NULL,
			      priority,
			      source_address,
			      source_lun);
}

int ipmi_register_smi(struct ipmi_smi_handlers *handlers,
		      void		       *send_info,
		      unsigned char            version_major,
		      unsigned char            version_minor,
		      ipmi_smi_t               *intf)
{
	int              i, j;
	int              rv;
	ipmi_smi_t       new_intf;
	struct list_head *entry;
	unsigned long    flags;


	/* Make sure the driver is actually initialized, this handles
	   problems with initialization order. */
	if (!initialized) {
		rv = ipmi_init_msghandler();
		if (rv)
			return rv;
		/* The init code doesn't return an error if it was turned
		   off, but it won't initialize.  Check that. */
		if (!initialized)
			return -ENODEV;
	}

	new_intf = kmalloc(sizeof(*new_intf), GFP_KERNEL);
	if (!new_intf)
		return -ENOMEM;

	rv = -ENOMEM;

	down_write(&interfaces_sem);
	for (i=0; i<MAX_IPMI_INTERFACES; i++) {
		if (ipmi_interfaces[i] == NULL) {
			new_intf->version_major = version_major;
			new_intf->version_minor = version_minor;
			new_intf->my_address = IPMI_BMC_SLAVE_ADDR;
			new_intf->my_lun = 2;  /* the SMS LUN. */
			rwlock_init(&(new_intf->users_lock));
			INIT_LIST_HEAD(&(new_intf->users));
			new_intf->handlers = handlers;
			new_intf->send_info = send_info;
			spin_lock_init(&(new_intf->seq_lock));
			for (j=0; j<IPMI_IPMB_NUM_SEQ; j++) {
				new_intf->seq_table[j].inuse = 0;
				new_intf->seq_table[j].seqid = 0;
			}
			new_intf->curr_seq = 0;
			spin_lock_init(&(new_intf->waiting_msgs_lock));
			INIT_LIST_HEAD(&(new_intf->waiting_msgs));
			spin_lock_init(&(new_intf->events_lock));
			INIT_LIST_HEAD(&(new_intf->waiting_events));
			new_intf->waiting_events_count = 0;
			rwlock_init(&(new_intf->cmd_rcvr_lock));
			INIT_LIST_HEAD(&(new_intf->cmd_rcvrs));
			new_intf->all_cmd_rcvr = NULL;

			spin_lock_irqsave(&interfaces_lock, flags);
			ipmi_interfaces[i] = new_intf;
			spin_unlock_irqrestore(&interfaces_lock, flags);

			rv = 0;
			*intf = new_intf;
			break;
		}
	}

	/* We convert to a read semaphore here.  It's possible the
	   interface was removed between the calls, we have to recheck
	   afterwards. */
	up_write(&interfaces_sem);
	down_read(&interfaces_sem);

	if (ipmi_interfaces[i] != new_intf)
		/* Well, it went away.  Just return. */
		goto out;

	if (rv == 0) {
		/* Call all the watcher interfaces to tell them that a
		   new interface is available. */
		down_read(&smi_watchers_sem);
		list_for_each(entry, &smi_watchers) {
			struct ipmi_smi_watcher *w;
			w = list_entry(entry, struct ipmi_smi_watcher, link);
			w->new_smi(i);
		}
		up_read(&smi_watchers_sem);
	}

 out:
	up_read(&interfaces_sem);

	if (rv)
		kfree(new_intf);

	return rv;
}

static void free_recv_msg_list(struct list_head *q)
{
	struct list_head     *entry, *entry2;
	struct ipmi_recv_msg *msg;

	list_for_each_safe(entry, entry2, q) {
		msg = list_entry(entry, struct ipmi_recv_msg, link);
		list_del(entry);
		ipmi_free_recv_msg(msg);
	}
}

static void free_cmd_rcvr_list(struct list_head *q)
{
	struct list_head *entry, *entry2;
	struct cmd_rcvr  *rcvr;

	list_for_each_safe(entry, entry2, q) {
		rcvr = list_entry(entry, struct cmd_rcvr, link);
		list_del(entry);
		kfree(rcvr);
	}
}

static void clean_up_interface_data(ipmi_smi_t intf)
{
	int i;

	free_recv_msg_list(&(intf->waiting_msgs));
	free_recv_msg_list(&(intf->waiting_events));
	free_cmd_rcvr_list(&(intf->cmd_rcvrs));

	for (i=0; i<IPMI_IPMB_NUM_SEQ; i++) {
		if ((intf->seq_table[i].inuse)
		    && (intf->seq_table[i].recv_msg))
		{
			ipmi_free_recv_msg(intf->seq_table[i].recv_msg);
		}	
	}
}

int ipmi_unregister_smi(ipmi_smi_t intf)
{
	int              rv = -ENODEV;
	int              i;
	struct list_head *entry;
	unsigned long    flags;

	down_write(&interfaces_sem);
	if (list_empty(&(intf->users)))
	{
		for (i=0; i<MAX_IPMI_INTERFACES; i++) {
			if (ipmi_interfaces[i] == intf) {
				spin_lock_irqsave(&interfaces_lock, flags);
				ipmi_interfaces[i] = NULL;
				clean_up_interface_data(intf);
				spin_unlock_irqrestore(&interfaces_lock,flags);
				kfree(intf);
				rv = 0;
				goto out_call_watcher;
			}
		}
	} else {
		rv = -EBUSY;
	}
	up_write(&interfaces_sem);

	return rv;

 out_call_watcher:
	/* Convert to a read semaphore so callbacks don't bite us. */
	up_write(&interfaces_sem);
	down_read(&interfaces_sem);

	/* Call all the watcher interfaces to tell them that
	   an interface is gone. */
	down_read(&smi_watchers_sem);
	list_for_each(entry, &smi_watchers) {
		struct ipmi_smi_watcher *w;
		w = list_entry(entry,
			       struct ipmi_smi_watcher,
			       link);
		w->smi_gone(i);
	}
	up_read(&smi_watchers_sem);
	up_read(&interfaces_sem);
	return 0;
}

static int handle_get_msg_rsp(ipmi_smi_t          intf,
			      struct ipmi_smi_msg *msg)
{
	struct ipmi_ipmb_addr ipmb_addr;
	struct ipmi_recv_msg  *recv_msg;

	
	if (msg->rsp_size < 11)
		/* Message not big enough, just ignore it. */
		return 0;

	if (msg->rsp[2] != 0)
		/* An error getting the response, just ignore it. */
		return 0;

	ipmb_addr.addr_type = IPMI_IPMB_ADDR_TYPE;
	ipmb_addr.slave_addr = msg->rsp[6];
	ipmb_addr.channel = msg->rsp[3] & 0x0f;
	ipmb_addr.lun = msg->rsp[7] & 3;

	/* It's a response from a remote entity.  Look up the sequence
	   number and handle the response. */
	if (intf_find_seq(intf,
			  msg->rsp[7] >> 2,
			  msg->rsp[3] & 0x0f,
			  msg->rsp[8],
			  (msg->rsp[4] >> 2) & (~1),
			  (struct ipmi_addr *) &(ipmb_addr),
			  &recv_msg))
	{
		/* We were unable to find the sequence number,
		   so just nuke the message. */
		return 0;
	}

	memcpy(recv_msg->msg_data,
	       &(msg->rsp[9]),
	       msg->rsp_size - 9);
	/* THe other fields matched, so no need to set them, except
           for netfn, which needs to be the response that was
           returned, not the request value. */
	recv_msg->msg.netfn = msg->rsp[4] >> 2;
	recv_msg->msg.data = recv_msg->msg_data;
	recv_msg->msg.data_len = msg->rsp_size - 10;
	recv_msg->recv_type = IPMI_RESPONSE_RECV_TYPE;
	deliver_response(recv_msg);

	return 0;
}

static int handle_get_msg_cmd(ipmi_smi_t          intf,
			      struct ipmi_smi_msg *msg)
{
	struct list_head *entry;
	struct cmd_rcvr       *rcvr;
	int              rv = 0;
	unsigned char    netfn;
	unsigned char    cmd;
	ipmi_user_t      user = NULL;
	struct ipmi_ipmb_addr *ipmb_addr;
	struct ipmi_recv_msg  *recv_msg;

	if (msg->rsp_size < 10)
		/* Message not big enough, just ignore it. */
		return 0;

	if (msg->rsp[2] != 0) {
		/* An error getting the response, just ignore it. */
		return 0;
	}

	netfn = msg->rsp[4] >> 2;
	cmd = msg->rsp[8];

	read_lock(&(intf->cmd_rcvr_lock));
	
	if (intf->all_cmd_rcvr) {
		user = intf->all_cmd_rcvr;
	} else {
		/* Find the command/netfn. */
		list_for_each(entry, &(intf->cmd_rcvrs)) {
			rcvr = list_entry(entry, struct cmd_rcvr, link);
			if ((rcvr->netfn == netfn) && (rcvr->cmd == cmd)) {
				user = rcvr->user;
				break;
			}
		}
	}
	read_unlock(&(intf->cmd_rcvr_lock));

	if (user == NULL) {
		/* We didn't find a user, deliver an error response. */
		msg->data[0] = (IPMI_NETFN_APP_REQUEST << 2);
		msg->data[1] = IPMI_SEND_MSG_CMD;
		msg->data[2] = msg->rsp[3];
		msg->data[3] = msg->rsp[6];
                msg->data[4] = ((netfn + 1) << 2) | (msg->rsp[7] & 0x3);
		msg->data[5] = ipmb_checksum(&(msg->data[3]), 2);
		msg->data[6] = intf->my_address;
                /* rqseq/lun */
                msg->data[7] = (msg->rsp[7] & 0xfc) | (msg->rsp[4] & 0x3);
		msg->data[8] = msg->rsp[8]; /* cmd */
		msg->data[9] = IPMI_INVALID_CMD_COMPLETION_CODE;
		msg->data[10] = ipmb_checksum(&(msg->data[6]), 4);
		msg->data_size = 11;

		intf->handlers->sender(intf->send_info, msg, 0);

		rv = -1; /* We used the message, so return the value that
			    causes it to not be freed or queued. */
	} else {
		/* Deliver the message to the user. */
		recv_msg = ipmi_alloc_recv_msg();
		if (! recv_msg) {
			/* We couldn't allocate memory for the
                           message, so requeue it for handling
                           later. */
			rv = 1;
		} else {
			ipmb_addr = (struct ipmi_ipmb_addr *) &recv_msg->addr;
			ipmb_addr->addr_type = IPMI_IPMB_ADDR_TYPE;
			ipmb_addr->slave_addr = msg->rsp[6];
			ipmb_addr->lun = msg->rsp[7] & 3;
			ipmb_addr->channel = msg->rsp[3];

			recv_msg->user = user;
			recv_msg->recv_type = IPMI_CMD_RECV_TYPE;
			recv_msg->msgid = msg->rsp[7] >> 2;
			recv_msg->msg.netfn = msg->rsp[4] >> 2;
			recv_msg->msg.cmd = msg->rsp[8];
			recv_msg->msg.data = recv_msg->msg_data;
			recv_msg->msg.data_len = msg->rsp_size - 10;
			memcpy(recv_msg->msg_data,
			       &(msg->rsp[9]),
			       msg->rsp_size - 10);
			deliver_response(recv_msg);
		}
	}

	return rv;
}

static void copy_event_into_recv_msg(struct ipmi_recv_msg *recv_msg,
				     struct ipmi_smi_msg  *msg)
{
	struct ipmi_system_interface_addr *smi_addr;
	
	recv_msg->msgid = 0;
	smi_addr = (struct ipmi_system_interface_addr *) &(recv_msg->addr);
	smi_addr->addr_type = IPMI_SYSTEM_INTERFACE_ADDR_TYPE;
	smi_addr->channel = IPMI_BMC_CHANNEL;
	smi_addr->lun = msg->rsp[0] & 3;
	recv_msg->recv_type = IPMI_ASYNC_EVENT_RECV_TYPE;
	recv_msg->msg.netfn = msg->rsp[0] >> 2;
	recv_msg->msg.cmd = msg->rsp[1];
	memcpy(recv_msg->msg_data, &(msg->rsp[3]), msg->rsp_size - 3);
	recv_msg->msg.data = recv_msg->msg_data;
	recv_msg->msg.data_len = msg->rsp_size - 3;
}

/* This will be called with the intf->users_lock read-locked, so no need
   to do that here. */
static int handle_read_event_rsp(ipmi_smi_t          intf,
				 struct ipmi_smi_msg *msg)
{
	struct ipmi_recv_msg *recv_msg;
	struct list_head     msgs;
	struct list_head     *entry, *entry2;
	ipmi_user_t          user;
	int                  rv = 0;
	int                  deliver_count = 0;
	unsigned long        flags;

	if (msg->rsp_size < 19) {
		/* Message is too small to be an IPMB event. */
		return 0;
	}

	if (msg->rsp[2] != 0) {
		/* An error getting the event, just ignore it. */
		return 0;
	}

	INIT_LIST_HEAD(&msgs);

	spin_lock_irqsave(&(intf->events_lock), flags);

	/* Allocate and fill in one message for every user that is getting
	   events. */
	list_for_each(entry, &(intf->users)) {
		user = list_entry(entry, struct ipmi_user, link);

		if (! user->gets_events)
			continue;

		recv_msg = ipmi_alloc_recv_msg();
		if (! recv_msg) {
			list_for_each_safe(entry, entry2, &msgs) {
				recv_msg = list_entry(entry,
						      struct ipmi_recv_msg,
						      link);
				list_del(entry);
				ipmi_free_recv_msg(recv_msg);
			}
			/* We couldn't allocate memory for the
                           message, so requeue it for handling
                           later. */
			rv = 1;
			goto out;
		}

		deliver_count++;

		copy_event_into_recv_msg(recv_msg, msg);
		recv_msg->user = user;
		list_add_tail(&(recv_msg->link), &msgs);
	}

	if (deliver_count) {
		/* Now deliver all the messages. */
		list_for_each_safe(entry, entry2, &msgs) {
			recv_msg = list_entry(entry,
					      struct ipmi_recv_msg,
					      link);
			list_del(entry);
			deliver_response(recv_msg);
		}
	} else if (intf->waiting_events_count < MAX_EVENTS_IN_QUEUE) {
		/* No one to receive the message, put it in queue if there's
		   not already too many things in the queue. */
		recv_msg = ipmi_alloc_recv_msg();
		if (! recv_msg) {
			/* We couldn't allocate memory for the
                           message, so requeue it for handling
                           later. */
			rv = 1;
			goto out;
		}

		copy_event_into_recv_msg(recv_msg, msg);
		list_add_tail(&(recv_msg->link), &(intf->waiting_events));
	} else {
		/* There's too many things in the queue, discard this
		   message. */
		printk(KERN_WARNING "ipmi: Event queue full, discarding an"
		       " incoming event\n");
	}

 out:
	spin_unlock_irqrestore(&(intf->events_lock), flags);

	return rv;
}

static int handle_bmc_rsp(ipmi_smi_t          intf,
			  struct ipmi_smi_msg *msg)
{
	struct ipmi_recv_msg *recv_msg;
	int                  found = 0;
	struct list_head     *entry;

	recv_msg = (struct ipmi_recv_msg *) msg->user_data;

	/* Make sure the user still exists. */
	list_for_each(entry, &(intf->users)) {
		if (list_entry(entry, struct ipmi_user, link)
		    == recv_msg->user)
		{
			/* Found it, so we can deliver it */
			found = 1;
			break;
		}
	}

	if (!found) {
		/* The user for the message went away, so give up. */
		ipmi_free_recv_msg(recv_msg);
	} else {
		struct ipmi_system_interface_addr *smi_addr;

		recv_msg->recv_type = IPMI_RESPONSE_RECV_TYPE;
		recv_msg->msgid = msg->msgid;
		smi_addr = ((struct ipmi_system_interface_addr *)
			    &(recv_msg->addr));
		smi_addr->addr_type = IPMI_SYSTEM_INTERFACE_ADDR_TYPE;
		smi_addr->channel = IPMI_BMC_CHANNEL;
		smi_addr->lun = msg->rsp[0] & 3;
		recv_msg->msg.netfn = msg->rsp[0] >> 2;
		recv_msg->msg.cmd = msg->rsp[1];
		memcpy(recv_msg->msg_data,
		       &(msg->rsp[2]),
		       msg->rsp_size - 2);
		recv_msg->msg.data = recv_msg->msg_data;
		recv_msg->msg.data_len = msg->rsp_size - 2;
		deliver_response(recv_msg);
	}

	return 0;
}

/* Handle a new message.  Return 1 if the message should be requeued,
   0 if the message should be freed, or -1 if the message should not
   be freed or requeued. */
static int handle_new_recv_msg(ipmi_smi_t          intf,
			       struct ipmi_smi_msg *msg)
{
	int requeue;

	if (msg->rsp_size < 2) {
		/* Message is too small to be correct. */
		requeue = 0;
	} else if (msg->rsp[1] == IPMI_GET_MSG_CMD) {
#if DEBUG_MSGING
		int m;
		printk("Response:");
		for (m=0; m<msg->rsp_size; m++)
			printk(" %2.2x", msg->rsp[m]);
		printk("\n");
#endif
		/* It's from the receive queue. */
		if (msg->rsp[4] & 0x04) {
			/* It's a response, so find the
			   requesting message and send it up. */
			requeue = handle_get_msg_rsp(intf, msg);
		} else {
			/* It's a command to the SMS from some other
			   entity.  Handle that. */
			requeue = handle_get_msg_cmd(intf, msg);
		}
	} else if (msg->rsp[1] == IPMI_READ_EVENT_MSG_BUFFER_CMD) {
		/* It's an asyncronous event. */
		requeue = handle_read_event_rsp(intf, msg);
	} else {
		/* It's a response from the local BMC. */
		requeue = handle_bmc_rsp(intf, msg);
	}

	return requeue;
}

/* Handle a new message from the lower layer. */
void ipmi_smi_msg_received(ipmi_smi_t          intf,
			   struct ipmi_smi_msg *msg)
{
	unsigned long flags;
	int           rv;


	/* Lock the user lock so the user can't go away while we are
	   working on it. */
	read_lock(&(intf->users_lock));

	if ((msg->data_size >= 2) && (msg->data[1] == IPMI_SEND_MSG_CMD)) {
		/* This is the local response to a send, start the
                   timer for these. */
		intf_start_seq_timer(intf, msg->msgid);
		ipmi_free_smi_msg(msg);
		goto out_unlock;
	}

	/* To preserve message order, if the list is not empty, we
           tack this message onto the end of the list. */
	spin_lock_irqsave(&(intf->waiting_msgs_lock), flags);
	if (!list_empty(&(intf->waiting_msgs))) {
		list_add_tail(&(msg->link), &(intf->waiting_msgs));
		spin_unlock(&(intf->waiting_msgs_lock));
		goto out_unlock;
	}
	spin_unlock_irqrestore(&(intf->waiting_msgs_lock), flags);
		
	rv = handle_new_recv_msg(intf, msg);
	if (rv > 0) {
		/* Could not handle the message now, just add it to a
                   list to handle later. */
		spin_lock(&(intf->waiting_msgs_lock));
		list_add_tail(&(msg->link), &(intf->waiting_msgs));
		spin_unlock(&(intf->waiting_msgs_lock));
	} else if (rv == 0) {
		ipmi_free_smi_msg(msg);
	}

 out_unlock:
	read_unlock(&(intf->users_lock));
}

void ipmi_smi_watchdog_pretimeout(ipmi_smi_t intf)
{
	struct list_head *entry;
	ipmi_user_t      user;

	read_lock(&(intf->users_lock));
	list_for_each(entry, &(intf->users)) {
		user = list_entry(entry, struct ipmi_user, link);

		if (! user->handler->ipmi_watchdog_pretimeout)
			continue;

		user->handler->ipmi_watchdog_pretimeout(user->handler_data);
	}
	read_unlock(&(intf->users_lock));
}

static void
handle_msg_timeout(struct ipmi_recv_msg *msg)
{
	msg->recv_type = IPMI_RESPONSE_RECV_TYPE;
	msg->msg_data[0] = IPMI_TIMEOUT_COMPLETION_CODE;
	msg->msg.netfn |= 1; /* Convert to a response. */
	msg->msg.data_len = 1;
	msg->msg.data = msg->msg_data;
	deliver_response(msg);
}

static void
send_from_recv_msg(ipmi_smi_t intf, struct ipmi_recv_msg *recv_msg,
		   struct ipmi_smi_msg *smi_msg,
		   unsigned char seq, long seqid)
{
	if (!smi_msg)
		smi_msg = ipmi_alloc_smi_msg();
	if (!smi_msg)
		/* If we can't allocate the message, then just return, we
		   get 4 retries, so this should be ok. */
		return;

	memcpy(smi_msg->data, recv_msg->msg.data, recv_msg->msg.data_len);
	smi_msg->data_size = recv_msg->msg.data_len;
	smi_msg->msgid = STORE_SEQ_IN_MSGID(seq, seqid);
		
	/* Send the new message.  We send with a zero priority.  It
	   timed out, I doubt time is that critical now, and high
	   priority messages are really only for messages to the local
	   MC, which don't get resent. */
	intf->handlers->sender(intf->send_info, smi_msg, 0);

#if DEBUG_MSGING
	{
		int m;
		printk("Resend: ");
		for (m=0; m<smi_msg->data_size; m++)
			printk(" %2.2x", smi_msg->data[m]);
		printk("\n");
	}
#endif
}

static void
ipmi_timeout_handler(long timeout_period)
{
	ipmi_smi_t           intf;
	struct list_head     timeouts;
	struct ipmi_recv_msg *msg;
	struct ipmi_smi_msg  *smi_msg;
	unsigned long        flags;
	struct list_head     *entry, *entry2;
	int                  i, j;

	INIT_LIST_HEAD(&timeouts);

	spin_lock(&interfaces_lock);
	for (i=0; i<MAX_IPMI_INTERFACES; i++) {
		intf = ipmi_interfaces[i];
		if (intf == NULL)
			continue;

		read_lock(&(intf->users_lock));

		/* See if any waiting messages need to be processed. */
		spin_lock_irqsave(&(intf->waiting_msgs_lock), flags);
		list_for_each_safe(entry, entry2, &(intf->waiting_msgs)) {
			smi_msg = list_entry(entry, struct ipmi_smi_msg, link);
			if (! handle_new_recv_msg(intf, smi_msg)) {
				list_del(entry);
				ipmi_free_smi_msg(smi_msg);
			} else {
				/* To preserve message order, quit if we
				   can't handle a message. */
				break;
			}
		}
		spin_unlock_irqrestore(&(intf->waiting_msgs_lock), flags);

		/* Go through the seq table and find any messages that
		   have timed out, putting them in the timeouts
		   list. */
		spin_lock_irqsave(&(intf->seq_lock), flags);
		for (j=0; j<IPMI_IPMB_NUM_SEQ; j++) {
			struct seq_table *ent = &(intf->seq_table[j]);
			if (!ent->inuse)
				continue;

			ent->timeout -= timeout_period;
			if (ent->timeout > 0)
				continue;

			if (ent->retries_left == 0) {
				/* The message has used all its retries. */
				ent->inuse = 0;
				msg = ent->recv_msg;
				list_add_tail(&(msg->link), &timeouts);
			} else {
				/* More retries, send again. */

				/* Start with the max timer, set to normal
				   timer after the message is sent. */
				ent->timeout = MAX_MSG_TIMEOUT;
				ent->retries_left--;
				send_from_recv_msg(intf, ent->recv_msg, NULL,
						   j, ent->seqid);
			}
		}
		spin_unlock_irqrestore(&(intf->seq_lock), flags);

		list_for_each_safe(entry, entry2, &timeouts) {
			msg = list_entry(entry, struct ipmi_recv_msg, link);
			handle_msg_timeout(msg);
		}

		read_unlock(&(intf->users_lock));
	}
	spin_unlock(&interfaces_lock);
}

static void ipmi_request_event(void)
{
	ipmi_smi_t intf;
	int        i;

	spin_lock(&interfaces_lock);
	for (i=0; i<MAX_IPMI_INTERFACES; i++) {
		intf = ipmi_interfaces[i];
		if (intf == NULL)
			continue;

		intf->handlers->request_events(intf->send_info);
	}
	spin_unlock(&interfaces_lock);
}

static struct timer_list ipmi_timer;

/* Call every 100 ms. */
#define IPMI_TIMEOUT_TIME	100
#define IPMI_TIMEOUT_JIFFIES	(IPMI_TIMEOUT_TIME/(1000/HZ))

/* Request events from the queue every second.  Hopefully, in the
   future, IPMI will add a way to know immediately if an event is
   in the queue. */
#define IPMI_REQUEST_EV_TIME	(1000 / (IPMI_TIMEOUT_TIME))

static volatile int stop_operation = 0;
static volatile int timer_stopped = 0;
static unsigned int ticks_to_req_ev = IPMI_REQUEST_EV_TIME;

static void ipmi_timeout(unsigned long data)
{
	if (stop_operation) {
		timer_stopped = 1;
		return;
	}

	ticks_to_req_ev--;
	if (ticks_to_req_ev == 0) {
		ipmi_request_event();
		ticks_to_req_ev = IPMI_REQUEST_EV_TIME;
	}

	ipmi_timeout_handler(IPMI_TIMEOUT_TIME);

	ipmi_timer.expires += IPMI_TIMEOUT_JIFFIES;
	add_timer(&ipmi_timer);
}


static atomic_t smi_msg_inuse_count = ATOMIC_INIT(0);
static atomic_t recv_msg_inuse_count = ATOMIC_INIT(0);

/* FIXME - convert these to slabs. */
static void free_smi_msg(struct ipmi_smi_msg *msg)
{
	atomic_dec(&smi_msg_inuse_count);
	kfree(msg);
}

struct ipmi_smi_msg *ipmi_alloc_smi_msg(void)
{
	struct ipmi_smi_msg *rv;
	rv = kmalloc(sizeof(struct ipmi_smi_msg), GFP_ATOMIC);
	if (rv) {
		rv->done = free_smi_msg;
		atomic_inc(&smi_msg_inuse_count);
	}
	return rv;
}

static void free_recv_msg(struct ipmi_recv_msg *msg)
{
	atomic_dec(&recv_msg_inuse_count);
	kfree(msg);
}

struct ipmi_recv_msg *ipmi_alloc_recv_msg(void)
{
	struct ipmi_recv_msg *rv;

	rv = kmalloc(sizeof(struct ipmi_recv_msg), GFP_ATOMIC);
	if (rv) {
		rv->done = free_recv_msg;
		atomic_inc(&recv_msg_inuse_count);
	}
	return rv;
}

#ifdef CONFIG_IPMI_PANIC_EVENT

static void dummy_smi_done_handler(struct ipmi_smi_msg *msg)
{
}

static void dummy_recv_done_handler(struct ipmi_recv_msg *msg)
{
}

static void send_panic_events(void)
{
	struct ipmi_msg                   msg;
	ipmi_smi_t                        intf;
	unsigned char                     data[8];
	int                               i;
	struct ipmi_system_interface_addr addr;
	struct ipmi_smi_msg               smi_msg;
	struct ipmi_recv_msg              recv_msg;

	addr.addr_type = IPMI_SYSTEM_INTERFACE_ADDR_TYPE;
	addr.channel = IPMI_BMC_CHANNEL;

	/* Fill in an event telling that we have failed. */
	msg.netfn = 0x04; /* Sensor or Event. */
	msg.cmd = 2; /* Platform event command. */
	msg.data = data;
	msg.data_len = 8;
	data[0] = 0x21; /* Kernel generator ID, IPMI table 5-4 */
	data[1] = 0x03; /* This is for IPMI 1.0. */
	data[2] = 0x20; /* OS Critical Stop, IPMI table 36-3 */
	data[4] = 0x6f; /* Sensor specific, IPMI table 36-1 */
	data[5] = 0xa1; /* Runtime stop OEM bytes 2 & 3. */

	/* These used to have the first three bytes of the panic string,
	   but not only is that not terribly useful, it's not available
	   any more. */
	data[3] = 0;
	data[6] = 0;
	data[7] = 0;

	smi_msg.done = dummy_smi_done_handler;
	recv_msg.done = dummy_recv_done_handler;

	/* For every registered interface, send the event. */
	for (i=0; i<MAX_IPMI_INTERFACES; i++) {
		intf = ipmi_interfaces[i];
		if (intf == NULL)
			continue;

		intf->handlers->set_run_to_completion(intf->send_info, 1);
		i_ipmi_request(NULL,
			       intf,
			       (struct ipmi_addr *) &addr,
			       0,
			       &msg,
			       &smi_msg,
			       &recv_msg,
			       0,
			       intf->my_address,
			       intf->my_lun);
	}
}
#endif /* CONFIG_IPMI_PANIC_EVENT */

static int has_paniced = 0;

static int panic_event(struct notifier_block *this,
		       unsigned long         event,
                       void                  *ptr)
{
	int        i;
	ipmi_smi_t intf;

	if (has_paniced)
		return NOTIFY_DONE;
	has_paniced = 1;

	/* For every registered interface, set it to run to completion. */
	for (i=0; i<MAX_IPMI_INTERFACES; i++) {
		intf = ipmi_interfaces[i];
		if (intf == NULL)
			continue;

		intf->handlers->set_run_to_completion(intf->send_info, 1);
	}

#ifdef CONFIG_IPMI_PANIC_EVENT
	send_panic_events();
#endif

	return NOTIFY_DONE;
}

static struct notifier_block panic_block = {
	panic_event,
	NULL,
	200   /* priority: INT_MAX >= x >= 0 */
};


static __init int ipmi_init_msghandler(void)
{
	int i;

	if (initialized)
		return 0;

	for (i=0; i<MAX_IPMI_INTERFACES; i++) {
		ipmi_interfaces[i] = NULL;
	}

	init_timer(&ipmi_timer);
	ipmi_timer.data = 0;
	ipmi_timer.function = ipmi_timeout;
	ipmi_timer.expires = jiffies + IPMI_TIMEOUT_JIFFIES;
	add_timer(&ipmi_timer);

	notifier_chain_register(&panic_notifier_list, &panic_block);

	initialized = 1;

	printk(KERN_INFO "ipmi: message handler initialized\n");

	return 0;
}

static __exit void cleanup_ipmi(void)
{
	int count;

	if (!initialized)
		return;

	notifier_chain_unregister(&panic_notifier_list, &panic_block);

	/* This can't be called if any interfaces exist, so no worry about
	   shutting down the interfaces. */

	/* Tell the timer to stop, then wait for it to stop.  This avoids
	   problems with race conditions removing the timer here. */
	stop_operation = 1;
	while (!timer_stopped) {
		schedule_timeout(1);
	}

	initialized = 0;

	/* Check for buffer leaks. */
	count = atomic_read(&smi_msg_inuse_count);
	if (count != 0)
		printk("ipmi_msghandler: SMI message count %d at exit\n",
		       count);
	count = atomic_read(&recv_msg_inuse_count);
	if (count != 0)
		printk("ipmi_msghandler: recv message count %d at exit\n",
		       count);
}
module_exit(cleanup_ipmi);

module_init(ipmi_init_msghandler);
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(ipmi_alloc_recv_msg);
EXPORT_SYMBOL(ipmi_create_user);
EXPORT_SYMBOL(ipmi_destroy_user);
EXPORT_SYMBOL(ipmi_get_version);
EXPORT_SYMBOL(ipmi_request);
EXPORT_SYMBOL(ipmi_request_supply_msgs);
EXPORT_SYMBOL(ipmi_request_with_source);
EXPORT_SYMBOL(ipmi_register_smi);
EXPORT_SYMBOL(ipmi_unregister_smi);
EXPORT_SYMBOL(ipmi_register_for_cmd);
EXPORT_SYMBOL(ipmi_unregister_for_cmd);
EXPORT_SYMBOL(ipmi_smi_msg_received);
EXPORT_SYMBOL(ipmi_smi_watchdog_pretimeout);
EXPORT_SYMBOL(ipmi_alloc_smi_msg);
EXPORT_SYMBOL(ipmi_register_all_cmd_rcvr);
EXPORT_SYMBOL(ipmi_unregister_all_cmd_rcvr);
EXPORT_SYMBOL(ipmi_addr_length);
EXPORT_SYMBOL(ipmi_validate_addr);
EXPORT_SYMBOL(ipmi_set_gets_events);
EXPORT_SYMBOL(ipmi_addr_equal);
EXPORT_SYMBOL(ipmi_smi_watcher_register);
EXPORT_SYMBOL(ipmi_smi_watcher_unregister);
EXPORT_SYMBOL(ipmi_set_my_address);
EXPORT_SYMBOL(ipmi_get_my_address);
EXPORT_SYMBOL(ipmi_set_my_LUN);
EXPORT_SYMBOL(ipmi_get_my_LUN);
