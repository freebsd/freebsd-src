/*
 *	AARP:		An implementation of the AppleTalk AARP protocol for
 *			Ethernet 'ELAP'.
 *
 *		Alan Cox  <Alan.Cox@linux.org>
 *
 *	This doesn't fit cleanly with the IP arp. Potentially we can use
 *	the generic neighbour discovery code to clean this up.
 *
 *	FIXME:
 *		We ought to handle the retransmits with a single list and a 
 *	separate fast timer for when it is needed.
 *		Use neighbour discovery code.
 *		Token Ring Support.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *
 *	References:
 *		Inside AppleTalk (2nd Ed).
 *	Fixes:
 *		Jaume Grau	-	flush caches on AARP_PROBE
 *		Rob Newberry	-	Added proxy AARP and AARP proc fs, 
 *					moved probing from DDP module.
 *		Arnaldo C. Melo -	don't mangle rx packets
 *
 */

#include <linux/config.h>
#if defined(CONFIG_ATALK) || defined(CONFIG_ATALK_MODULE) 
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/in.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/if_ether.h>
#include <linux/inet.h>
#include <linux/notifier.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_arp.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <net/sock.h>
#include <net/datalink.h>
#include <net/psnap.h>
#include <linux/atalk.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/module.h>

int sysctl_aarp_expiry_time = AARP_EXPIRY_TIME;
int sysctl_aarp_tick_time = AARP_TICK_TIME;
int sysctl_aarp_retransmit_limit = AARP_RETRANSMIT_LIMIT;
int sysctl_aarp_resolve_time = AARP_RESOLVE_TIME;

/* Lists of aarp entries */
struct aarp_entry {
	/* These first two are only used for unresolved entries */
	unsigned long last_sent;		/* Last time we xmitted the aarp request */
	struct sk_buff_head packet_queue;	/* Queue of frames wait for resolution */
	int status;				/* Used for proxy AARP */
	unsigned long expires_at;		/* Entry expiry time */
	struct at_addr target_addr;		/* DDP Address */
	struct net_device *dev;			/* Device to use */
	char hwaddr[6];				/* Physical i/f address of target/router */
	unsigned short xmit_count;		/* When this hits 10 we give up */
	struct aarp_entry *next;		/* Next entry in chain */
};

/* Hashed list of resolved, unresolved and proxy entries */
static struct aarp_entry *resolved[AARP_HASH_SIZE];
static struct aarp_entry *unresolved[AARP_HASH_SIZE];
static struct aarp_entry *proxies[AARP_HASH_SIZE];
static int unresolved_count;

/* One lock protects it all. */
static spinlock_t aarp_lock = SPIN_LOCK_UNLOCKED;

/* Used to walk the list and purge/kick entries.  */
static struct timer_list aarp_timer;

/*
 *	Delete an aarp queue
 *
 *	Must run under aarp_lock.
 */
static void __aarp_expire(struct aarp_entry *a)
{
	skb_queue_purge(&a->packet_queue);
	kfree(a);
}

/*
 *	Send an aarp queue entry request
 *
 *	Must run under aarp_lock.
 */
 
static void __aarp_send_query(struct aarp_entry *a)
{
	static char aarp_eth_multicast[ETH_ALEN] =
		{ 0x09, 0x00, 0x07, 0xFF, 0xFF, 0xFF };
	struct net_device *dev = a->dev;
	int len = dev->hard_header_len + sizeof(struct elapaarp) +
		aarp_dl->header_length;
	struct sk_buff *skb = alloc_skb(len, GFP_ATOMIC);
	struct at_addr *sat = atalk_find_dev_addr(dev);
	struct elapaarp *eah;
	
	if (!skb)
		return;

	if (!sat) {
		kfree_skb(skb);
		return;
	}
	
	/* Set up the buffer */		
	skb_reserve(skb, dev->hard_header_len + aarp_dl->header_length);
	eah		=	(struct elapaarp *)skb_put(skb,
						sizeof(struct elapaarp));
	skb->protocol   =       htons(ETH_P_ATALK);
	skb->nh.raw     =       skb->h.raw = (void *) eah;
	skb->dev	=	dev;
	
	/* Set up the ARP */
	eah->hw_type	=	htons(AARP_HW_TYPE_ETHERNET);
	eah->pa_type	=	htons(ETH_P_ATALK);
	eah->hw_len	=	ETH_ALEN;	
	eah->pa_len	=	AARP_PA_ALEN;
	eah->function	=	htons(AARP_REQUEST);
	
	memcpy(eah->hw_src, dev->dev_addr, ETH_ALEN);
	
	eah->pa_src_zero=	0;
	eah->pa_src_net	=	sat->s_net;
	eah->pa_src_node=	sat->s_node;
	
	memset(eah->hw_dst, '\0', ETH_ALEN);
	
	eah->pa_dst_zero=	0;
	eah->pa_dst_net	=	a->target_addr.s_net;
	eah->pa_dst_node=	a->target_addr.s_node;
	
	/* Add ELAP headers and set target to the AARP multicast */
	aarp_dl->datalink_header(aarp_dl, skb, aarp_eth_multicast);	

	/* Send it */	
	dev_queue_xmit(skb);
	/* Update the sending count */
	a->xmit_count++;
}

/* This runs under aarp_lock and in softint context, so only atomic memory
 * allocations can be used. */
static void aarp_send_reply(struct net_device *dev, struct at_addr *us,
			    struct at_addr *them, unsigned char *sha)
{
	int len = dev->hard_header_len + sizeof(struct elapaarp) +
			aarp_dl->header_length;
	struct sk_buff *skb = alloc_skb(len, GFP_ATOMIC);
	struct elapaarp *eah;
	
	if (!skb)
		return;
	
	/* Set up the buffer */
	skb_reserve(skb, dev->hard_header_len + aarp_dl->header_length);
	eah		=	(struct elapaarp *)skb_put(skb,
					sizeof(struct elapaarp));	 
	skb->protocol   =       htons(ETH_P_ATALK);
	skb->nh.raw     =       skb->h.raw = (void *) eah;
	skb->dev	=	dev;
	
	/* Set up the ARP */
	eah->hw_type	=	htons(AARP_HW_TYPE_ETHERNET);
	eah->pa_type	=	htons(ETH_P_ATALK);
	eah->hw_len	=	ETH_ALEN;	
	eah->pa_len	=	AARP_PA_ALEN;
	eah->function	=	htons(AARP_REPLY);
	
	memcpy(eah->hw_src, dev->dev_addr, ETH_ALEN);
	
	eah->pa_src_zero=	0;
	eah->pa_src_net	=	us->s_net;
	eah->pa_src_node=	us->s_node;
	
	if (!sha)
		memset(eah->hw_dst, '\0', ETH_ALEN);
	else
		memcpy(eah->hw_dst, sha, ETH_ALEN);
	
	eah->pa_dst_zero=	0;
	eah->pa_dst_net	=	them->s_net;
	eah->pa_dst_node=	them->s_node;
	
	/* Add ELAP headers and set target to the AARP multicast */
	aarp_dl->datalink_header(aarp_dl, skb, sha);	
	/* Send it */	
	dev_queue_xmit(skb);
}

/*
 *	Send probe frames. Called from aarp_probe_network and
 *	aarp_proxy_probe_network.
 */

void aarp_send_probe(struct net_device *dev, struct at_addr *us)
{
	int len = dev->hard_header_len + sizeof(struct elapaarp) +
			aarp_dl->header_length;
	struct sk_buff *skb = alloc_skb(len, GFP_ATOMIC);
	static char aarp_eth_multicast[ETH_ALEN] =
		{ 0x09, 0x00, 0x07, 0xFF, 0xFF, 0xFF };
	struct elapaarp *eah;

	if (!skb)
		return;

	/* Set up the buffer */
	skb_reserve(skb, dev->hard_header_len + aarp_dl->header_length);
	eah		=	(struct elapaarp *)skb_put(skb,
					sizeof(struct elapaarp));
	skb->protocol   =       htons(ETH_P_ATALK);
	skb->nh.raw     =       skb->h.raw = (void *) eah;
	skb->dev	=	dev;

	/* Set up the ARP */
	eah->hw_type	=	htons(AARP_HW_TYPE_ETHERNET);
	eah->pa_type	=	htons(ETH_P_ATALK);
	eah->hw_len	=	ETH_ALEN;
	eah->pa_len	=	AARP_PA_ALEN;
	eah->function	=	htons(AARP_PROBE);

	memcpy(eah->hw_src, dev->dev_addr, ETH_ALEN);

	eah->pa_src_zero=	0;
	eah->pa_src_net	=	us->s_net;
	eah->pa_src_node=	us->s_node;

	memset(eah->hw_dst, '\0', ETH_ALEN);

	eah->pa_dst_zero=	0;
	eah->pa_dst_net	=	us->s_net;
	eah->pa_dst_node=	us->s_node;

	/* Add ELAP headers and set target to the AARP multicast */
	aarp_dl->datalink_header(aarp_dl, skb, aarp_eth_multicast);
	/* Send it */
	dev_queue_xmit(skb);
}
	
/*
 *	Handle an aarp timer expire
 *
 *	Must run under the aarp_lock.
 */

static void __aarp_expire_timer(struct aarp_entry **n)
{
	struct aarp_entry *t;

	while (*n)
		/* Expired ? */
		if (time_after(jiffies, (*n)->expires_at)) {
			t = *n;
			*n = (*n)->next;
			__aarp_expire(t);
		} else
			n = &((*n)->next);
}

/*
 *	Kick all pending requests 5 times a second.
 *
 *	Must run under the aarp_lock.
 */
 
static void __aarp_kick(struct aarp_entry **n)
{
	struct aarp_entry *t;

	while (*n)
		/* Expired: if this will be the 11th tx, we delete instead. */
		if ((*n)->xmit_count >= sysctl_aarp_retransmit_limit) {
			t = *n;
			*n = (*n)->next;
			__aarp_expire(t);
		} else {
			__aarp_send_query(*n);
			n = &((*n)->next);
		}
}

/*
 *	A device has gone down. Take all entries referring to the device
 *	and remove them.
 *
 *	Must run under the aarp_lock.
 */
 
static void __aarp_expire_device(struct aarp_entry **n, struct net_device *dev)
{
	struct aarp_entry *t;

	while (*n)
		if ((*n)->dev == dev) {
			t = *n;
			*n = (*n)->next;
			__aarp_expire(t);
		} else
			n = &((*n)->next);
}
		
/* Handle the timer event */
static void aarp_expire_timeout(unsigned long unused)
{
	int ct;

	spin_lock_bh(&aarp_lock);

	for (ct = 0; ct < AARP_HASH_SIZE; ct++) {
		__aarp_expire_timer(&resolved[ct]);
		__aarp_kick(&unresolved[ct]);
		__aarp_expire_timer(&unresolved[ct]);
		__aarp_expire_timer(&proxies[ct]);
	}

	spin_unlock_bh(&aarp_lock);
	mod_timer(&aarp_timer, jiffies + 
		  (unresolved_count ? sysctl_aarp_tick_time :
		   sysctl_aarp_expiry_time));
}

/* Network device notifier chain handler. */
static int aarp_device_event(struct notifier_block *this, unsigned long event,
				void *ptr)
{
	int ct;

	if (event == NETDEV_DOWN) {
		spin_lock_bh(&aarp_lock);

		for (ct = 0; ct < AARP_HASH_SIZE; ct++) {
			__aarp_expire_device(&resolved[ct], ptr);
			__aarp_expire_device(&unresolved[ct], ptr);
			__aarp_expire_device(&proxies[ct], ptr);
		}

		spin_unlock_bh(&aarp_lock);
	}
	return NOTIFY_DONE;
}

/*
 *	Create a new aarp entry.  This must use GFP_ATOMIC because it
 *	runs while holding spinlocks.
 */
 
static struct aarp_entry *aarp_alloc(void)
{
	struct aarp_entry *a = kmalloc(sizeof(struct aarp_entry), GFP_ATOMIC);

	if (a)
		skb_queue_head_init(&a->packet_queue);
	return a;
}

/*
 * Find an entry. We might return an expired but not yet purged entry. We
 * don't care as it will do no harm.
 *
 * This must run under the aarp_lock.
 */
static struct aarp_entry *__aarp_find_entry(struct aarp_entry *list,
					    struct net_device *dev,
					    struct at_addr *sat)
{
	while (list) {
		if (list->target_addr.s_net == sat->s_net &&
		    list->target_addr.s_node == sat->s_node &&
		    list->dev == dev)
			break;
		list = list->next;
	}

	return list;
}

/* Called from the DDP code, and thus must be exported. */
void aarp_proxy_remove(struct net_device *dev, struct at_addr *sa)
{
	int hash = sa->s_node % (AARP_HASH_SIZE - 1);
	struct aarp_entry *a;

	spin_lock_bh(&aarp_lock);

	a = __aarp_find_entry(proxies[hash], dev, sa);
	if (a)
		a->expires_at = jiffies - 1;

	spin_unlock_bh(&aarp_lock);
}

/* This must run under aarp_lock. */
static struct at_addr *__aarp_proxy_find(struct net_device *dev,
					 struct at_addr *sa)
{
	int hash = sa->s_node % (AARP_HASH_SIZE - 1);
	struct aarp_entry *a = __aarp_find_entry(proxies[hash], dev, sa);

	return a ? sa : NULL;
}

/*
 * Probe a Phase 1 device or a device that requires its Net:Node to
 * be set via an ioctl.
 */
void aarp_send_probe_phase1(struct atalk_iface *iface)
{
    struct ifreq atreq;
    struct sockaddr_at *sa = (struct sockaddr_at *)&atreq.ifr_addr;

    sa->sat_addr.s_node = iface->address.s_node;
    sa->sat_addr.s_net = ntohs(iface->address.s_net);

    /* We pass the Net:Node to the drivers/cards by a Device ioctl. */
    if (!(iface->dev->do_ioctl(iface->dev, &atreq, SIOCSIFADDR))) {
	    (void)iface->dev->do_ioctl(iface->dev, &atreq, SIOCGIFADDR);
	    if (iface->address.s_net != htons(sa->sat_addr.s_net) ||
		iface->address.s_node != sa->sat_addr.s_node)
		    iface->status |= ATIF_PROBE_FAIL;

	    iface->address.s_net  = htons(sa->sat_addr.s_net);
	    iface->address.s_node = sa->sat_addr.s_node;
    }
}


void aarp_probe_network(struct atalk_iface *atif)
{
	if (atif->dev->type == ARPHRD_LOCALTLK ||
	    atif->dev->type == ARPHRD_PPP) 
		aarp_send_probe_phase1(atif);
	else {
		unsigned int count;

		for (count = 0; count < AARP_RETRANSMIT_LIMIT; count++) {
			aarp_send_probe(atif->dev, &atif->address);

			/* Defer 1/10th */
			current->state = TASK_INTERRUPTIBLE;
			schedule_timeout(HZ/10);
							
			if (atif->status & ATIF_PROBE_FAIL)
				break;
		}
	}
}

int aarp_proxy_probe_network(struct atalk_iface *atif, struct at_addr *sa)
{
	int hash, retval = 1;
	struct aarp_entry *entry;
	unsigned int count;
	
	/*
	 * we don't currently support LocalTalk or PPP for proxy AARP;
	 * if someone wants to try and add it, have fun
	 */
	if (atif->dev->type == ARPHRD_LOCALTLK)
		return -EPROTONOSUPPORT;
		
	if (atif->dev->type == ARPHRD_PPP)
		return -EPROTONOSUPPORT;
		
	/* 
	 * create a new AARP entry with the flags set to be published -- 
	 * we need this one to hang around even if it's in use
	 */
	entry = aarp_alloc();
	if (!entry)
		return -ENOMEM;
	
	entry->expires_at = -1;
	entry->status = ATIF_PROBE;
	entry->target_addr.s_node = sa->s_node;
	entry->target_addr.s_net = sa->s_net;
	entry->dev = atif->dev;

	spin_lock_bh(&aarp_lock);

	hash = sa->s_node % (AARP_HASH_SIZE - 1);
	entry->next = proxies[hash];
	proxies[hash] = entry;
	
	for (count = 0; count < AARP_RETRANSMIT_LIMIT; count++) {
		aarp_send_probe(atif->dev, sa);

		/* Defer 1/10th */
		current->state = TASK_INTERRUPTIBLE;
		spin_unlock_bh(&aarp_lock);
		schedule_timeout(HZ/10);
		spin_lock_bh(&aarp_lock);

		if (entry->status & ATIF_PROBE_FAIL)
			break;
	}
	
	if (entry->status & ATIF_PROBE_FAIL) {
		entry->expires_at = jiffies - 1; /* free the entry */
		retval = -EADDRINUSE; /* return network full */
	} else /* clear the probing flag */
		entry->status &= ~ATIF_PROBE;

	spin_unlock_bh(&aarp_lock);
	return retval;
}

/* Send a DDP frame */
int aarp_send_ddp(struct net_device *dev,struct sk_buff *skb,
			struct at_addr *sa, void *hwaddr)
{
	static char ddp_eth_multicast[ETH_ALEN] =
		{ 0x09, 0x00, 0x07, 0xFF, 0xFF, 0xFF };
	int hash;
	struct aarp_entry *a;
	
	skb->nh.raw = skb->data;
	
	/* Check for LocalTalk first */
	if (dev->type == ARPHRD_LOCALTLK) {
		struct at_addr *at = atalk_find_dev_addr(dev);
		struct ddpehdr *ddp = (struct ddpehdr *)skb->data;
		int ft = 2;
		
		/*
		 *	Compressible ?
		 * 
		 *	IFF: src_net==dest_net==device_net
		 *	(zero matches anything)
		 */
		 
		if ((!ddp->deh_snet || at->s_net == ddp->deh_snet) &&
		    (!ddp->deh_dnet || at->s_net == ddp->deh_dnet)) {
			skb_pull(skb, sizeof(struct ddpehdr) - 4);

			/*
			 *	The upper two remaining bytes are the port 
			 *	numbers	we just happen to need. Now put the 
			 *	length in the lower two.
			 */
			*((__u16 *)skb->data) = htons(skb->len);
			ft = 1;
		}
		/*
		 *	Nice and easy. No AARP type protocols occur here
		 *	so we can just shovel it out with a 3 byte LLAP header
		 */
		 
		skb_push(skb, 3);
		skb->data[0] = sa->s_node;
		skb->data[1] = at->s_node;
		skb->data[2] = ft;
		skb->dev = dev;
		goto sendit;
	}	

	/* On a PPP link we neither compress nor aarp.  */
	if (dev->type == ARPHRD_PPP) {
		skb->protocol = htons(ETH_P_PPPTALK);
		skb->dev = dev;
		goto sendit;
	}
	 
	/* Non ELAP we cannot do. */
	if (dev->type != ARPHRD_ETHER)
		return -1;

	skb->dev = dev;
	skb->protocol = htons(ETH_P_ATALK);
	hash = sa->s_node % (AARP_HASH_SIZE - 1);
	
	/* Do we have a resolved entry? */
	if (sa->s_node == ATADDR_BCAST) {
		ddp_dl->datalink_header(ddp_dl, skb, ddp_eth_multicast);
		goto sendit;
	}

	spin_lock_bh(&aarp_lock);
	a = __aarp_find_entry(resolved[hash], dev, sa);

	if (a) { /* Return 1 and fill in the address */
		a->expires_at = jiffies + (sysctl_aarp_expiry_time * 10);
		ddp_dl->datalink_header(ddp_dl, skb, a->hwaddr);
		spin_unlock_bh(&aarp_lock);
		goto sendit;
	}

	/* Do we have an unresolved entry: This is the less common path */
	a = __aarp_find_entry(unresolved[hash], dev, sa);
	if (a) { /* Queue onto the unresolved queue */
		skb_queue_tail(&a->packet_queue, skb);
		spin_unlock_bh(&aarp_lock);
		return 0;
	}

	/* Allocate a new entry */
	a = aarp_alloc();
	if (!a) {
		/* Whoops slipped... good job it's an unreliable protocol 8) */
		spin_unlock_bh(&aarp_lock);
		return -1;
	}

	/* Set up the queue */
	skb_queue_tail(&a->packet_queue, skb);
	a->expires_at = jiffies + sysctl_aarp_resolve_time;
	a->dev = dev;
	a->next = unresolved[hash];
	a->target_addr = *sa;
	a->xmit_count = 0;
	unresolved[hash] = a;
	unresolved_count++;

	/* Send an initial request for the address */
	__aarp_send_query(a);

	/*
	 *	Switch to fast timer if needed (That is if this is the
	 *	first unresolved entry to get added)
	 */

	if (unresolved_count == 1)
		mod_timer(&aarp_timer, jiffies + sysctl_aarp_tick_time);

	/* Now finally, it is safe to drop the lock. */
	spin_unlock_bh(&aarp_lock);

	/* Tell the ddp layer we have taken over for this frame. */
	return 0;

sendit: if (skb->sk)
		skb->priority = skb->sk->priority;
	dev_queue_xmit(skb);
	return 1;
}

/*
 *	An entry in the aarp unresolved queue has become resolved. Send
 *	all the frames queued under it.
 *
 *	Must run under aarp_lock.
 */
static void __aarp_resolved(struct aarp_entry **list, struct aarp_entry *a,
				int hash)
{
	struct sk_buff *skb;

	while (*list)
		if (*list == a) {
			unresolved_count--;
			*list = a->next;

			/* Move into the resolved list */
			a->next = resolved[hash];
			resolved[hash] = a;

			/* Kick frames off */
			while ((skb = skb_dequeue(&a->packet_queue)) != NULL) {
				a->expires_at = jiffies +
						sysctl_aarp_expiry_time * 10;
				ddp_dl->datalink_header(ddp_dl, skb, a->hwaddr);
				if (skb->sk)
					skb->priority = skb->sk->priority;
				dev_queue_xmit(skb);
			}
		} else 
			list = &((*list)->next);
}

/*
 *	This is called by the SNAP driver whenever we see an AARP SNAP
 *	frame. We currently only support Ethernet.
 */
static int aarp_rcv(struct sk_buff *skb, struct net_device *dev,
			struct packet_type *pt)
{
	struct elapaarp *ea = (struct elapaarp *)skb->h.raw;
	int hash, ret = 0;
	__u16 function;
	struct aarp_entry *a;
	struct at_addr sa, *ma, da;
	struct atalk_iface *ifa;

	/* We only do Ethernet SNAP AARP. */
	if (dev->type != ARPHRD_ETHER)
		goto out0;

	/* Frame size ok? */
	if (!skb_pull(skb, sizeof(*ea)))
		goto out0;

	function = ntohs(ea->function);

	/* Sanity check fields. */
	if (function < AARP_REQUEST || function > AARP_PROBE ||
	    ea->hw_len != ETH_ALEN || ea->pa_len != AARP_PA_ALEN ||
	    ea->pa_src_zero || ea->pa_dst_zero)
		goto out0;

	/* Looks good. */
	hash = ea->pa_src_node % (AARP_HASH_SIZE - 1);

	/* Build an address. */
	sa.s_node = ea->pa_src_node;
	sa.s_net = ea->pa_src_net;

	/* Process the packet. Check for replies of me. */
	ifa = atalk_find_dev(dev);
	if (!ifa)
		goto out1;

	if (ifa->status & ATIF_PROBE &&
	    ifa->address.s_node == ea->pa_dst_node &&
	    ifa->address.s_net == ea->pa_dst_net) {
		ifa->status |= ATIF_PROBE_FAIL; /* Fail the probe (in use) */
		goto out1;
	}

	/* Check for replies of proxy AARP entries */
	da.s_node = ea->pa_dst_node;
	da.s_net = ea->pa_dst_net;

	spin_lock_bh(&aarp_lock);
	a = __aarp_find_entry(proxies[hash], dev, &da);

	if (a && a->status & ATIF_PROBE) {
		a->status |= ATIF_PROBE_FAIL;
		/*
		 * we do not respond to probe or request packets for
		 * this address while we are probing this address
		 */
		goto unlock;
	}

	switch (function) {
		case AARP_REPLY:	
			if (!unresolved_count)	/* Speed up */
				break;

			/* Find the entry.  */
			a = __aarp_find_entry(unresolved[hash],dev,&sa);
			if (!a || dev != a->dev)
				break;

			/* We can fill one in - this is good. */
			memcpy(a->hwaddr,ea->hw_src,ETH_ALEN);
			__aarp_resolved(&unresolved[hash],a,hash);
			if (!unresolved_count)
				mod_timer(&aarp_timer,
					  jiffies + sysctl_aarp_expiry_time);
			break;
			
		case AARP_REQUEST:
		case AARP_PROBE:
			/*
			 *	If it is my address set ma to my address and
			 *	reply. We can treat probe and request the
			 *	same. Probe simply means we shouldn't cache
			 *	the querying host, as in a probe they are
			 *	proposing an address not using one.
			 *	
			 *	Support for proxy-AARP added. We check if the
			 *	address is one of our proxies before we toss
			 *	the packet out.
			 */
			 
			sa.s_node = ea->pa_dst_node;
			sa.s_net = ea->pa_dst_net;

			/* See if we have a matching proxy. */
			ma = __aarp_proxy_find(dev, &sa);
			if (!ma)
				ma = &ifa->address;
			else { /* We need to make a copy of the entry. */
				da.s_node = sa.s_node;
				da.s_net = da.s_net;
				ma = &da;
			}

			if (function == AARP_PROBE) {
				/* A probe implies someone trying to get an
				 * address. So as a precaution flush any
				 * entries we have for this address. */
				struct aarp_entry *a = __aarp_find_entry(
					resolved[sa.s_node%(AARP_HASH_SIZE-1)],
					skb->dev, &sa);
				/* Make it expire next tick - that avoids us
				 * getting into a probe/flush/learn/probe/
				 * flush/learn cycle during probing of a slow
				 * to respond host addr. */
				if (a) {
					a->expires_at = jiffies - 1;
					mod_timer(&aarp_timer, jiffies +
							sysctl_aarp_tick_time);
				}
			}

			if (sa.s_node != ma->s_node)
				break;

			if (sa.s_net && ma->s_net && sa.s_net != ma->s_net)
				break;

			sa.s_node = ea->pa_src_node;
			sa.s_net = ea->pa_src_net;
			
			/* aarp_my_address has found the address to use for us.
			*/
			aarp_send_reply(dev, ma, &sa, ea->hw_src);
			break;
	}

unlock:	spin_unlock_bh(&aarp_lock);
out1:	ret = 1;
out0:	kfree_skb(skb);
	return ret;
}

static struct notifier_block aarp_notifier = {
	notifier_call:	aarp_device_event,
};

static char aarp_snap_id[] = { 0x00, 0x00, 0x00, 0x80, 0xF3 };

void __init aarp_proto_init(void)
{
	aarp_dl = register_snap_client(aarp_snap_id, aarp_rcv);
	if (!aarp_dl)
		printk(KERN_CRIT "Unable to register AARP with SNAP.\n");
	init_timer(&aarp_timer);
	aarp_timer.function = aarp_expire_timeout;
	aarp_timer.data = 0;
	aarp_timer.expires = jiffies + sysctl_aarp_expiry_time;
	add_timer(&aarp_timer);
	register_netdevice_notifier(&aarp_notifier);
}

/* Remove the AARP entries associated with a device. */
void aarp_device_down(struct net_device *dev)
{
	int ct;

	spin_lock_bh(&aarp_lock);

	for (ct = 0; ct < AARP_HASH_SIZE; ct++) {
		__aarp_expire_device(&resolved[ct], dev);
		__aarp_expire_device(&unresolved[ct], dev);
		__aarp_expire_device(&proxies[ct], dev);
	}

	spin_unlock_bh(&aarp_lock);
}

/* Called from proc fs */
static int aarp_get_info(char *buffer, char **start, off_t offset, int length)
{
	/* we should dump all our AARP entries */
	struct aarp_entry *entry;
	int len, ct;

	len = sprintf(buffer,
		"%-10.10s  %-10.10s%-18.18s%12.12s%12.12s xmit_count  status\n",
		"address", "device", "hw addr", "last_sent", "expires");

	spin_lock_bh(&aarp_lock);

	for (ct = 0; ct < AARP_HASH_SIZE; ct++) {
		for (entry = resolved[ct]; entry; entry = entry->next) {
			len+= sprintf(buffer+len,"%6u:%-3u  ",
				(unsigned int)ntohs(entry->target_addr.s_net),
				(unsigned int)(entry->target_addr.s_node));
			len+= sprintf(buffer+len,"%-10.10s",
				entry->dev->name);
			len+= sprintf(buffer+len,"%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
				(int)(entry->hwaddr[0] & 0x000000FF),
				(int)(entry->hwaddr[1] & 0x000000FF),
				(int)(entry->hwaddr[2] & 0x000000FF),
				(int)(entry->hwaddr[3] & 0x000000FF),
				(int)(entry->hwaddr[4] & 0x000000FF),
				(int)(entry->hwaddr[5] & 0x000000FF));
			len+= sprintf(buffer+len,"%12lu ""%12lu ",
				(unsigned long)entry->last_sent,
				(unsigned long)entry->expires_at);
			len+=sprintf(buffer+len,"%10u",
				(unsigned int)entry->xmit_count);

			len+=sprintf(buffer+len,"   resolved\n");
		}
	}

	for (ct = 0; ct < AARP_HASH_SIZE; ct++) {
		for (entry = unresolved[ct]; entry; entry = entry->next) {
			len+= sprintf(buffer+len,"%6u:%-3u  ",
				(unsigned int)ntohs(entry->target_addr.s_net),
				(unsigned int)(entry->target_addr.s_node));
			len+= sprintf(buffer+len,"%-10.10s",
				entry->dev->name);
			len+= sprintf(buffer+len,"%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
				(int)(entry->hwaddr[0] & 0x000000FF),
				(int)(entry->hwaddr[1] & 0x000000FF),
				(int)(entry->hwaddr[2] & 0x000000FF),
				(int)(entry->hwaddr[3] & 0x000000FF),
				(int)(entry->hwaddr[4] & 0x000000FF),
				(int)(entry->hwaddr[5] & 0x000000FF));
			len+= sprintf(buffer+len,"%12lu ""%12lu ",
				(unsigned long)entry->last_sent,
				(unsigned long)entry->expires_at);
			len+=sprintf(buffer+len,"%10u",
				(unsigned int)entry->xmit_count);
			len+=sprintf(buffer+len," unresolved\n");
		}
	}

	for (ct = 0; ct < AARP_HASH_SIZE; ct++) {
		for (entry = proxies[ct]; entry; entry = entry->next) {
			len+= sprintf(buffer+len,"%6u:%-3u  ",
				(unsigned int)ntohs(entry->target_addr.s_net),
				(unsigned int)(entry->target_addr.s_node));
			len+= sprintf(buffer+len,"%-10.10s",
				entry->dev->name);
			len+= sprintf(buffer+len,"%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
				(int)(entry->hwaddr[0] & 0x000000FF),
				(int)(entry->hwaddr[1] & 0x000000FF),
				(int)(entry->hwaddr[2] & 0x000000FF),
				(int)(entry->hwaddr[3] & 0x000000FF),
				(int)(entry->hwaddr[4] & 0x000000FF),
				(int)(entry->hwaddr[5] & 0x000000FF));
			len+= sprintf(buffer+len,"%12lu ""%12lu ",
				(unsigned long)entry->last_sent,
				(unsigned long)entry->expires_at);
			len+=sprintf(buffer+len,"%10u",
				(unsigned int)entry->xmit_count);
			len+=sprintf(buffer+len,"      proxy\n");
		}
	}

	spin_unlock_bh(&aarp_lock);
	return len;
}

#ifdef MODULE
/* General module cleanup. Called from cleanup_module() in ddp.c. */
void aarp_cleanup_module(void)
{
	del_timer(&aarp_timer);
	unregister_netdevice_notifier(&aarp_notifier);
	unregister_snap_client(aarp_snap_id);
}
#endif  /* MODULE */
#ifdef CONFIG_PROC_FS
void aarp_register_proc_fs(void)
{
	proc_net_create("aarp", 0, aarp_get_info);
}

void aarp_unregister_proc_fs(void)
{
	proc_net_remove("aarp");
}
#endif
#endif  /* CONFIG_ATALK || CONFIG_ATALK_MODULE */
MODULE_LICENSE("GPL");
