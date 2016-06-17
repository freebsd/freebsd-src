/*
 *	DDP:	An implementation of the AppleTalk DDP protocol for
 *		Ethernet 'ELAP'.
 *
 *		Alan Cox  <Alan.Cox@linux.org>
 *
 *		With more than a little assistance from
 *
 *		Wesley Craig <netatalk@umich.edu>
 *
 *	Fixes:
 *		Michael Callahan	:	Made routing work
 *		Wesley Craig		:	Fix probing to listen to a
 *						passed node id.
 *		Alan Cox		:	Added send/recvmsg support
 *		Alan Cox		:	Moved at. to protinfo in
 *						socket.
 *		Alan Cox		:	Added firewall hooks.
 *		Alan Cox		:	Supports new ARPHRD_LOOPBACK
 *		Christer Weinigel	: 	Routing and /proc fixes.
 *		Bradford Johnson	:	LocalTalk.
 *		Tom Dyas		:	Module support.
 *		Alan Cox		:	Hooks for PPP (based on the
 *						LocalTalk hook).
 *		Alan Cox		:	Posix bits
 *		Alan Cox/Mike Freeman	:	Possible fix to NBP problems
 *		Bradford Johnson	:	IP-over-DDP (experimental)
 *		Jay Schulist		:	Moved IP-over-DDP to its own
 *						driver file. (ipddp.c & ipddp.h)
 *		Jay Schulist		:	Made work as module with 
 *						AppleTalk drivers, cleaned it.
 *		Rob Newberry		:	Added proxy AARP and AARP
 *						procfs, moved probing to AARP
 *						module.
 *              Adrian Sun/ 
 *              Michael Zuelsdorff      :       fix for net.0 packets. don't 
 *                                              allow illegal ether/tokentalk
 *                                              port assignment. we lose a 
 *                                              valid localtalk port as a 
 *                                              result.
 *		Arnaldo C. de Melo	:	Cleanup, in preparation for
 *						shared skb support 8)
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 * 
 */

#include <linux/config.h>
#if defined(CONFIG_ATALK) || defined(CONFIG_ATALK_MODULE)
#include <linux/module.h>
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
#include <linux/notifier.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/route.h>
#include <linux/inet.h>
#include <linux/etherdevice.h>
#include <linux/if_arp.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/termios.h>	/* For TIOCOUTQ/INQ */
#include <net/datalink.h>
#include <net/p8022.h>
#include <net/psnap.h>
#include <net/sock.h>
#include <linux/ip.h>
#include <net/route.h>
#include <linux/atalk.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/init.h>

#ifdef CONFIG_PROC_FS
extern void aarp_register_proc_fs(void);
extern void aarp_unregister_proc_fs(void);
#endif

extern void aarp_cleanup_module(void);

extern void aarp_probe_network(struct atalk_iface *atif);
extern int  aarp_proxy_probe_network(struct atalk_iface *atif,
					struct at_addr *sa);
extern void aarp_proxy_remove(struct net_device *dev, struct at_addr *sa);

#undef APPLETALK_DEBUG
#ifdef APPLETALK_DEBUG
#define DPRINT(x)		print(x)
#else
#define DPRINT(x)
#endif /* APPLETALK_DEBUG */

#ifdef CONFIG_SYSCTL
extern inline void atalk_register_sysctl(void);
extern inline void atalk_unregister_sysctl(void);
#endif /* CONFIG_SYSCTL */

struct datalink_proto *ddp_dl, *aarp_dl;
static struct proto_ops atalk_dgram_ops;

/**************************************************************************\
*                                                                          *
* Handlers for the socket list.                                            *
*                                                                          *
\**************************************************************************/

static struct sock *atalk_sockets;
static spinlock_t atalk_sockets_lock = SPIN_LOCK_UNLOCKED;

extern inline void atalk_insert_socket(struct sock *sk)
{
	spin_lock_bh(&atalk_sockets_lock);
	sk->next = atalk_sockets;
	if (sk->next)
		atalk_sockets->pprev = &sk->next;
	atalk_sockets = sk;
	sk->pprev = &atalk_sockets;
	spin_unlock_bh(&atalk_sockets_lock);
}

extern inline void atalk_remove_socket(struct sock *sk)
{
	spin_lock_bh(&atalk_sockets_lock);
	if (sk->pprev) {
		if (sk->next)
			sk->next->pprev = sk->pprev;
		*sk->pprev = sk->next;
		sk->pprev = NULL;
	}
	spin_unlock_bh(&atalk_sockets_lock);
}

static struct sock *atalk_search_socket(struct sockaddr_at *to,
					struct atalk_iface *atif)
{
	struct sock *s;

	spin_lock_bh(&atalk_sockets_lock);
	for (s = atalk_sockets; s; s = s->next) {
		if (to->sat_port != s->protinfo.af_at.src_port)
			continue;

	    	if (to->sat_addr.s_net == ATADDR_ANYNET &&
		    to->sat_addr.s_node == ATADDR_BCAST &&
		    s->protinfo.af_at.src_net == atif->address.s_net)
			break;

	    	if (to->sat_addr.s_net == s->protinfo.af_at.src_net &&
		    (to->sat_addr.s_node == s->protinfo.af_at.src_node ||
		     to->sat_addr.s_node == ATADDR_BCAST ||
		     to->sat_addr.s_node == ATADDR_ANYNODE))
			break;

	    	/* XXXX.0 -- we got a request for this router. make sure
		 * that the node is appropriately set. */
		if (to->sat_addr.s_node == ATADDR_ANYNODE &&
		    to->sat_addr.s_net != ATADDR_ANYNET &&
		    atif->address.s_node == s->protinfo.af_at.src_node) {
			to->sat_addr.s_node = atif->address.s_node;
			break; 
		}
	}
	spin_unlock_bh(&atalk_sockets_lock);
	return s;
}

/*
 * Try to find a socket matching ADDR in the socket list,
 * if found then return it.  If not, insert SK into the
 * socket list.
 *
 * This entire operation must execute atomically.
 */
static struct sock *atalk_find_or_insert_socket(struct sock *sk,
						struct sockaddr_at *sat)
{
	struct sock *s;

	spin_lock_bh(&atalk_sockets_lock);
	for (s = atalk_sockets; s; s = s->next)
		if (s->protinfo.af_at.src_net == sat->sat_addr.s_net &&
		    s->protinfo.af_at.src_node == sat->sat_addr.s_node &&
		    s->protinfo.af_at.src_port == sat->sat_port)
			break;

	if (!s) {
		/* Wheee, it's free, assign and insert. */
		sk->next = atalk_sockets;
		if (sk->next)
			atalk_sockets->pprev = &sk->next;
		atalk_sockets = sk;
		sk->pprev = &atalk_sockets;
	}

	spin_unlock_bh(&atalk_sockets_lock);
	return s;
}

static void atalk_destroy_timer(unsigned long data)
{
	struct sock *sk = (struct sock *) data;

	if (!atomic_read(&sk->wmem_alloc) &&
	    !atomic_read(&sk->rmem_alloc) && sk->dead) {
		sock_put(sk);
		MOD_DEC_USE_COUNT;
	} else {
		sk->timer.expires = jiffies + SOCK_DESTROY_TIME;
		add_timer(&sk->timer);
	}
}

extern inline void atalk_destroy_socket(struct sock *sk)
{
	atalk_remove_socket(sk);
	skb_queue_purge(&sk->receive_queue);

	if (!atomic_read(&sk->wmem_alloc) &&
	    !atomic_read(&sk->rmem_alloc) && sk->dead) {
		sock_put(sk);
		MOD_DEC_USE_COUNT;
	} else {
		init_timer(&sk->timer);
		sk->timer.expires = jiffies + SOCK_DESTROY_TIME;
		sk->timer.function = atalk_destroy_timer;
		sk->timer.data = (unsigned long) sk;
		add_timer(&sk->timer);
	}
}

/* Called from proc fs */
static int atalk_get_info(char *buffer, char **start, off_t offset, int length)
{
	off_t pos = 0;
	off_t begin = 0;
	int len = sprintf(buffer, "Type local_addr  remote_addr tx_queue "
				  "rx_queue st uid\n");
	struct sock *s;
	/* Output the AppleTalk data for the /proc filesystem */

	spin_lock_bh(&atalk_sockets_lock);
	for (s = atalk_sockets; s; s = s->next) {
		len += sprintf(buffer + len,"%02X   ", s->type);
		len += sprintf(buffer + len,"%04X:%02X:%02X  ",
			       ntohs(s->protinfo.af_at.src_net),
			       s->protinfo.af_at.src_node,
			       s->protinfo.af_at.src_port);
		len += sprintf(buffer + len,"%04X:%02X:%02X  ",
			       ntohs(s->protinfo.af_at.dest_net),
			       s->protinfo.af_at.dest_node,
			       s->protinfo.af_at.dest_port);
		len += sprintf(buffer + len,"%08X:%08X ",
			       atomic_read(&s->wmem_alloc),
			       atomic_read(&s->rmem_alloc));
		len += sprintf(buffer + len,"%02X %d\n", s->state, 
			       SOCK_INODE(s->socket)->i_uid);

		/* Are we still dumping unwanted data then discard the record */
		pos = begin + len;

		if (pos < offset) {
			len = 0;	/* Keep dumping into the buffer start */
			begin = pos;
		}
		if (pos > offset + length)	/* We have dumped enough */
			break;
	}
	spin_unlock_bh(&atalk_sockets_lock);

	/* The data in question runs from begin to begin+len */
	*start = buffer + offset - begin;	/* Start of wanted data */
	len -= offset - begin;   /* Remove unwanted header data from length */
	if (len > length)
		len = length;	   /* Remove unwanted tail data from length */

	return len;
}

/**************************************************************************\
*                                                                          *
* Routing tables for the AppleTalk socket layer.                           *
*                                                                          *
\**************************************************************************/

/* Anti-deadlock ordering is router_lock --> iface_lock -DaveM */
static struct atalk_route *atalk_router_list;
static rwlock_t atalk_router_lock = RW_LOCK_UNLOCKED;

static struct atalk_iface *atalk_iface_list;
static spinlock_t atalk_iface_lock = SPIN_LOCK_UNLOCKED;

/* For probing devices or in a routerless network */
static struct atalk_route atrtr_default;

/* AppleTalk interface control */
/*
 * Drop a device. Doesn't drop any of its routes - that is the caller's
 * problem. Called when we down the interface or delete the address.
 */
static void atif_drop_device(struct net_device *dev)
{
	struct atalk_iface **iface = &atalk_iface_list;
	struct atalk_iface *tmp;

	spin_lock_bh(&atalk_iface_lock);
	while ((tmp = *iface) != NULL) {
		if (tmp->dev == dev) {
			*iface = tmp->next;
			kfree(tmp);
			dev->atalk_ptr = NULL;
			MOD_DEC_USE_COUNT;
		} else
			iface = &tmp->next;
	}
	spin_unlock_bh(&atalk_iface_lock);
}

static struct atalk_iface *atif_add_device(struct net_device *dev,
					   struct at_addr *sa)
{
	struct atalk_iface *iface = kmalloc(sizeof(*iface), GFP_KERNEL);

	if (!iface)
		return NULL;

	iface->dev = dev;
	dev->atalk_ptr = iface;
	iface->address = *sa;
	iface->status = 0;

	spin_lock_bh(&atalk_iface_lock);
	iface->next = atalk_iface_list;
	atalk_iface_list = iface;
	spin_unlock_bh(&atalk_iface_lock);

	MOD_INC_USE_COUNT;
	return iface;
}

/* Perform phase 2 AARP probing on our tentative address */
static int atif_probe_device(struct atalk_iface *atif)
{
	int netrange = ntohs(atif->nets.nr_lastnet) -
			ntohs(atif->nets.nr_firstnet) + 1;
	int probe_net = ntohs(atif->address.s_net);
	int probe_node = atif->address.s_node;
	int netct, nodect;

	/* Offset the network we start probing with */
	if (probe_net == ATADDR_ANYNET) {
		probe_net = ntohs(atif->nets.nr_firstnet);
		if (netrange)
			probe_net += jiffies % netrange;
	}
	if (probe_node == ATADDR_ANYNODE)
		probe_node = jiffies & 0xFF;

	/* Scan the networks */
	atif->status |= ATIF_PROBE;
	for (netct = 0; netct <= netrange; netct++) {
		/* Sweep the available nodes from a given start */
		atif->address.s_net = htons(probe_net);
		for (nodect = 0; nodect < 256; nodect++) {
			atif->address.s_node = ((nodect+probe_node) & 0xFF);
			if (atif->address.s_node > 0 &&
			    atif->address.s_node < 254) {
				/* Probe a proposed address */
				aarp_probe_network(atif);

				if (!(atif->status & ATIF_PROBE_FAIL)) {
					atif->status &= ~ATIF_PROBE;
					return 0;
				}
			}
			atif->status &= ~ATIF_PROBE_FAIL;
		}
		probe_net++;
		if (probe_net > ntohs(atif->nets.nr_lastnet))
			probe_net = ntohs(atif->nets.nr_firstnet);
	}
	atif->status &= ~ATIF_PROBE;

	return -EADDRINUSE;	/* Network is full... */
}


/* Perform AARP probing for a proxy address */
static int atif_proxy_probe_device(struct atalk_iface *atif,
				   struct at_addr* proxy_addr)
{
	int netrange = ntohs(atif->nets.nr_lastnet) -
			ntohs(atif->nets.nr_firstnet) + 1;
	/* we probe the interface's network */
	int probe_net = ntohs(atif->address.s_net);
	int probe_node = ATADDR_ANYNODE;	    /* we'll take anything */
	int netct, nodect;

	/* Offset the network we start probing with */
	if (probe_net == ATADDR_ANYNET) {
		probe_net = ntohs(atif->nets.nr_firstnet);
		if (netrange)
			probe_net += jiffies % netrange;
	}

	if (probe_node == ATADDR_ANYNODE)
		probe_node = jiffies & 0xFF;
		
	/* Scan the networks */
	for (netct = 0; netct <= netrange; netct++) {
		/* Sweep the available nodes from a given start */
		proxy_addr->s_net = htons(probe_net);
		for (nodect = 0; nodect < 256; nodect++) {
			proxy_addr->s_node = ((nodect + probe_node) & 0xFF);
			if (proxy_addr->s_node > 0 &&
			    proxy_addr->s_node < 254) {
				/* Tell AARP to probe a proposed address */
				int ret = aarp_proxy_probe_network(atif,
								    proxy_addr);

				if (ret != -EADDRINUSE)
					return ret;
			}
		}
		probe_net++;
		if (probe_net > ntohs(atif->nets.nr_lastnet))
			probe_net = ntohs(atif->nets.nr_firstnet);
	}

	return -EADDRINUSE;	/* Network is full... */
}


struct at_addr *atalk_find_dev_addr(struct net_device *dev)
{
	struct atalk_iface *iface = dev->atalk_ptr;
	return iface ? &iface->address : NULL;
}

static struct at_addr *atalk_find_primary(void)
{
	struct atalk_iface *fiface = NULL;
	struct at_addr *retval;
	struct atalk_iface *iface;

	/*
	 * Return a point-to-point interface only if
	 * there is no non-ptp interface available.
	 */
	spin_lock_bh(&atalk_iface_lock);
	for (iface = atalk_iface_list; iface; iface = iface->next) {
		if (!fiface && !(iface->dev->flags & IFF_LOOPBACK))
			fiface = iface;
		if (!(iface->dev->flags & (IFF_LOOPBACK | IFF_POINTOPOINT))) {
			retval = &iface->address;
			goto out;
		}
	}

	if (fiface)
		retval = &fiface->address;
	else if (atalk_iface_list)
		retval = &atalk_iface_list->address;
	else
		retval = NULL;
out:	spin_unlock_bh(&atalk_iface_lock);
	return retval;
}

/*
 * Find a match for 'any network' - ie any of our interfaces with that
 * node number will do just nicely.
 */
static struct atalk_iface *atalk_find_anynet(int node, struct net_device *dev)
{
	struct atalk_iface *iface = dev->atalk_ptr;

	if (!iface || iface->status & ATIF_PROBE)
		return NULL;

	if (node == ATADDR_BCAST ||
	    iface->address.s_node == node ||
	    node == ATADDR_ANYNODE)
		return iface;

	return NULL;
}

/* Find a match for a specific network:node pair */
static struct atalk_iface *atalk_find_interface(int net, int node)
{
	struct atalk_iface *iface;

	spin_lock_bh(&atalk_iface_lock);
	for (iface = atalk_iface_list; iface; iface = iface->next) {
		if ((node == ATADDR_BCAST ||
		     node == ATADDR_ANYNODE ||
		     iface->address.s_node == node) &&
		    iface->address.s_net == net &&
		    !(iface->status & ATIF_PROBE))
			break;

		/* XXXX.0 -- net.0 returns the iface associated with net */
		if (node == ATADDR_ANYNODE && net != ATADDR_ANYNET &&
		    ntohs(iface->nets.nr_firstnet) <= ntohs(net) &&
		    ntohs(net) <= ntohs(iface->nets.nr_lastnet))
		        break;
	}
	spin_unlock_bh(&atalk_iface_lock);
	return iface;
}


/*
 * Find a route for an AppleTalk packet. This ought to get cached in
 * the socket (later on...). We know about host routes and the fact
 * that a route must be direct to broadcast.
 */
static struct atalk_route *atrtr_find(struct at_addr *target)
{
	/*
	 * we must search through all routes unless we find a 
	 * host route, because some host routes might overlap
	 * network routes
	 */
	struct atalk_route *net_route = NULL;
	struct atalk_route *r;
	
	read_lock_bh(&atalk_router_lock);
	for (r = atalk_router_list; r; r = r->next) {
		if (!(r->flags & RTF_UP))
			continue;

		if (r->target.s_net == target->s_net) {
			if (r->flags & RTF_HOST) {
				/*
				 * if this host route is for the target,
				 * the we're done
				 */
				if (r->target.s_node == target->s_node)
					goto out;
			} else
				/*
				 * this route will work if there isn't a
				 * direct host route, so cache it
				 */
				net_route = r;
		}
	}
	
	/* 
	 * if we found a network route but not a direct host
	 * route, then return it
	 */
	if (net_route)
		r = net_route;
	else if (atrtr_default.dev)
		r = &atrtr_default;
	else /* No route can be found */
		r = NULL;
out:	read_unlock_bh(&atalk_router_lock);
	return r;
}


/*
 * Given an AppleTalk network, find the device to use. This can be
 * a simple lookup.
 */
struct net_device *atrtr_get_dev(struct at_addr *sa)
{
	struct atalk_route *atr = atrtr_find(sa);
	return atr ? atr->dev : NULL;
}

/* Set up a default router */
static void atrtr_set_default(struct net_device *dev)
{
	atrtr_default.dev = dev;
	atrtr_default.flags = RTF_UP;
	atrtr_default.gateway.s_net = htons(0);
	atrtr_default.gateway.s_node = 0;
}

/*
 * Add a router. Basically make sure it looks valid and stuff the
 * entry in the list. While it uses netranges we always set them to one
 * entry to work like netatalk.
 */
static int atrtr_create(struct rtentry *r, struct net_device *devhint)
{
	struct sockaddr_at *ta = (struct sockaddr_at *)&r->rt_dst;
	struct sockaddr_at *ga = (struct sockaddr_at *)&r->rt_gateway;
	struct atalk_route *rt;
	struct atalk_iface *iface, *riface;
	int retval;

	/*
	 * Fixme: Raise/Lower a routing change semaphore for these
	 * operations.
	 */

	/* Validate the request */
	if (ta->sat_family != AF_APPLETALK)
		return -EINVAL;

	if (!devhint && ga->sat_family != AF_APPLETALK)
		return -EINVAL;

	/* Now walk the routing table and make our decisions */
	write_lock_bh(&atalk_router_lock);
	for (rt = atalk_router_list; rt; rt = rt->next) {
		if (r->rt_flags != rt->flags)
			continue;

		if (ta->sat_addr.s_net == rt->target.s_net) {
			if (!(rt->flags & RTF_HOST))
				break;
			if (ta->sat_addr.s_node == rt->target.s_node)
				break;
		}
	}

	if (!devhint) {
		riface = NULL;

		spin_lock_bh(&atalk_iface_lock);
		for (iface = atalk_iface_list; iface; iface = iface->next) {
			if (!riface &&
			    ntohs(ga->sat_addr.s_net) >=
			    		ntohs(iface->nets.nr_firstnet) &&
			    ntohs(ga->sat_addr.s_net) <=
			    		ntohs(iface->nets.nr_lastnet))
				riface = iface;

			if (ga->sat_addr.s_net == iface->address.s_net &&
			    ga->sat_addr.s_node == iface->address.s_node)
				riface = iface;
		}		
		spin_unlock_bh(&atalk_iface_lock);

		retval = -ENETUNREACH;
		if (!riface)
			goto out;

		devhint = riface->dev;
	}

	if (!rt) {
		rt = kmalloc(sizeof(struct atalk_route), GFP_ATOMIC);

		retval = -ENOBUFS;
		if (!rt)
			goto out;

		rt->next = atalk_router_list;
		atalk_router_list = rt;
	}

	/* Fill in the routing entry */
	rt->target  = ta->sat_addr;
	rt->dev     = devhint;
	rt->flags   = r->rt_flags;
	rt->gateway = ga->sat_addr;

	retval = 0;
out:	write_unlock_bh(&atalk_router_lock);
	return retval;
}

/* Delete a route. Find it and discard it */
static int atrtr_delete(struct at_addr * addr)
{
	struct atalk_route **r = &atalk_router_list;
	int retval = 0;
	struct atalk_route *tmp;

	write_lock_bh(&atalk_router_lock);
	while ((tmp = *r) != NULL) {
		if (tmp->target.s_net == addr->s_net &&
		    (!(tmp->flags&RTF_GATEWAY) ||
		     tmp->target.s_node == addr->s_node)) {
			*r = tmp->next;
			kfree(tmp);
			goto out;
		}
		r = &tmp->next;
	}
	retval = -ENOENT;
out:	write_unlock_bh(&atalk_router_lock);
	return retval;
}

/*
 * Called when a device is downed. Just throw away any routes
 * via it.
 */
void atrtr_device_down(struct net_device *dev)
{
	struct atalk_route **r = &atalk_router_list;
	struct atalk_route *tmp;

	write_lock_bh(&atalk_router_lock);
	while ((tmp = *r) != NULL) {
		if (tmp->dev == dev) {
			*r = tmp->next;
			kfree(tmp);
		} else
			r = &tmp->next;
	}
	write_unlock_bh(&atalk_router_lock);

	if (atrtr_default.dev == dev)
		atrtr_set_default(NULL);
}

/* Actually down the interface */
static inline void atalk_dev_down(struct net_device *dev)
{
	atrtr_device_down(dev);	/* Remove all routes for the device */
	aarp_device_down(dev);	/* Remove AARP entries for the device */
	atif_drop_device(dev);	/* Remove the device */
}

/*
 * A device event has occurred. Watch for devices going down and
 * delete our use of them (iface and route).
 */
static int ddp_device_event(struct notifier_block *this, unsigned long event,
				void *ptr)
{
	if (event == NETDEV_DOWN)
		/* Discard any use of this */
	        atalk_dev_down((struct net_device *) ptr);

	return NOTIFY_DONE;
}

/* ioctl calls. Shouldn't even need touching */
/* Device configuration ioctl calls */
static int atif_ioctl(int cmd, void *arg)
{
	static char aarp_mcast[6] = {0x09, 0x00, 0x00, 0xFF, 0xFF, 0xFF};
	struct ifreq atreq;
	struct netrange *nr;
	struct sockaddr_at *sa;
	struct net_device *dev;
	struct atalk_iface *atif;
	int ct;
	int limit;
	struct rtentry rtdef;
	int add_route;

	if (copy_from_user(&atreq, arg, sizeof(atreq)))
		return -EFAULT;

	dev = __dev_get_by_name(atreq.ifr_name);
	if (!dev)
		return -ENODEV;

	sa = (struct sockaddr_at*) &atreq.ifr_addr;
	atif = atalk_find_dev(dev);

	switch (cmd) {
		case SIOCSIFADDR:
			if (!capable(CAP_NET_ADMIN))
				return -EPERM;
			if (sa->sat_family != AF_APPLETALK)
				return -EINVAL;
			if (dev->type != ARPHRD_ETHER &&
			    dev->type != ARPHRD_LOOPBACK &&
			    dev->type != ARPHRD_LOCALTLK &&
			    dev->type != ARPHRD_PPP)
				return -EPROTONOSUPPORT;

			nr = (struct netrange *) &sa->sat_zero[0];
			add_route = 1;

			/*
			 * if this is a point-to-point iface, and we already
			 * have an iface for this AppleTalk address, then we
			 * should not add a route
			 */
			if ((dev->flags & IFF_POINTOPOINT) &&
			    atalk_find_interface(sa->sat_addr.s_net,
				    		 sa->sat_addr.s_node)) {
				printk(KERN_DEBUG "AppleTalk: point-to-point "
						  "interface added with "
						  "existing address\n");
				add_route = 0;
			}
			
			/*
			 * Phase 1 is fine on LocalTalk but we don't do
			 * EtherTalk phase 1. Anyone wanting to add it go ahead.
			 */
			if (dev->type == ARPHRD_ETHER && nr->nr_phase != 2)
				return -EPROTONOSUPPORT;
			if (sa->sat_addr.s_node == ATADDR_BCAST ||
			    sa->sat_addr.s_node == 254)
				return -EINVAL;
			if (atif) {
				/* Already setting address */
				if (atif->status & ATIF_PROBE)
					return -EBUSY;

				atif->address.s_net  = sa->sat_addr.s_net;
				atif->address.s_node = sa->sat_addr.s_node;
				atrtr_device_down(dev);	/* Flush old routes */
			} else {
				atif = atif_add_device(dev, &sa->sat_addr);
				if (!atif)
					return -ENOMEM;
			}
			atif->nets = *nr;

			/*
			 * Check if the chosen address is used. If so we
			 * error and atalkd will try another.
			 */

			if (!(dev->flags & IFF_LOOPBACK) &&
			    !(dev->flags & IFF_POINTOPOINT) &&
			    atif_probe_device(atif) < 0) {
				atif_drop_device(dev);
				return -EADDRINUSE;
			}

			/* Hey it worked - add the direct routes */
			sa = (struct sockaddr_at *) &rtdef.rt_gateway;
			sa->sat_family = AF_APPLETALK;
			sa->sat_addr.s_net  = atif->address.s_net;
			sa->sat_addr.s_node = atif->address.s_node;
			sa = (struct sockaddr_at *) &rtdef.rt_dst;
			rtdef.rt_flags = RTF_UP;
			sa->sat_family = AF_APPLETALK;
			sa->sat_addr.s_node = ATADDR_ANYNODE;
			if (dev->flags & IFF_LOOPBACK ||
			    dev->flags & IFF_POINTOPOINT)
				rtdef.rt_flags |= RTF_HOST;

			/* Routerless initial state */
			if (nr->nr_firstnet == htons(0) &&
			    nr->nr_lastnet == htons(0xFFFE)) {
				sa->sat_addr.s_net = atif->address.s_net;
				atrtr_create(&rtdef, dev);
				atrtr_set_default(dev);
			} else {
				limit = ntohs(nr->nr_lastnet);
				if (limit - ntohs(nr->nr_firstnet) > 4096) {
					printk(KERN_WARNING "Too many routes/"
							    "iface.\n");
					return -EINVAL;
				}
				if (add_route)
					for (ct = ntohs(nr->nr_firstnet);
					     ct <= limit; ct++) {
						sa->sat_addr.s_net = htons(ct);
						atrtr_create(&rtdef, dev);
					}
			}
			dev_mc_add(dev, aarp_mcast, 6, 1);
			return 0;

		case SIOCGIFADDR:
			if (!atif)
				return -EADDRNOTAVAIL;

			sa->sat_family = AF_APPLETALK;
			sa->sat_addr = atif->address;
			break;

		case SIOCGIFBRDADDR:
			if (!atif)
				return -EADDRNOTAVAIL;

			sa->sat_family = AF_APPLETALK;
			sa->sat_addr.s_net = atif->address.s_net;
			sa->sat_addr.s_node = ATADDR_BCAST;
			break;

	        case SIOCATALKDIFADDR:
	        case SIOCDIFADDR:
			if (!capable(CAP_NET_ADMIN))
				return -EPERM;
			if (sa->sat_family != AF_APPLETALK)
				return -EINVAL;
			atalk_dev_down(dev);
			break;			

		case SIOCSARP:
			if (!capable(CAP_NET_ADMIN))
                                return -EPERM;
                        if (sa->sat_family != AF_APPLETALK)
                                return -EINVAL;
                        if (!atif)
                                return -EADDRNOTAVAIL;

                        /*
                         * for now, we only support proxy AARP on ELAP;
                         * we should be able to do it for LocalTalk, too.
                         */
                        if (dev->type != ARPHRD_ETHER)
                                return -EPROTONOSUPPORT;

                        /*
                         * atif points to the current interface on this network;
                         * we aren't concerned about its current status (at
			 * least for now), but it has all the settings about
			 * the network we're going to probe. Consequently, it
			 * must exist.
                         */
                        if (!atif)
                                return -EADDRNOTAVAIL;

                        nr = (struct netrange *) &(atif->nets);
                        /*
                         * Phase 1 is fine on Localtalk but we don't do
                         * Ethertalk phase 1. Anyone wanting to add it go ahead.
                         */
                        if (dev->type == ARPHRD_ETHER && nr->nr_phase != 2)
                                return -EPROTONOSUPPORT;

                        if (sa->sat_addr.s_node == ATADDR_BCAST ||
			    sa->sat_addr.s_node == 254)
                                return -EINVAL;

                        /*
                         * Check if the chosen address is used. If so we
                         * error and ATCP will try another.
                         */
                      	if (atif_proxy_probe_device(atif, &(sa->sat_addr)) < 0)
                      		return -EADDRINUSE;
                      	
			/*
                         * We now have an address on the local network, and
			 * the AARP code will defend it for us until we take it
			 * down. We don't set up any routes right now, because
			 * ATCP will install them manually via SIOCADDRT.
                         */
                        break;

                case SIOCDARP:
                        if (!capable(CAP_NET_ADMIN))
                                return -EPERM;
                        if (sa->sat_family != AF_APPLETALK)
                                return -EINVAL;
                        if (!atif)
                                return -EADDRNOTAVAIL;

                        /* give to aarp module to remove proxy entry */
                        aarp_proxy_remove(atif->dev, &(sa->sat_addr));
                        return 0;
	}

	return copy_to_user(arg, &atreq, sizeof(atreq)) ? -EFAULT : 0;
}

/* Routing ioctl() calls */
static int atrtr_ioctl(unsigned int cmd, void *arg)
{
	struct net_device *dev = NULL;
	struct rtentry rt;

	if (copy_from_user(&rt, arg, sizeof(rt)))
		return -EFAULT;

	switch (cmd) {
		case SIOCDELRT:
			if (rt.rt_dst.sa_family != AF_APPLETALK)
				return -EINVAL;
			return atrtr_delete(&((struct sockaddr_at *)
						&rt.rt_dst)->sat_addr);

		case SIOCADDRT:
			/* FIXME: the name of the device is still in user
			 * space, isn't it? */
			if (rt.rt_dev) {
				dev = __dev_get_by_name(rt.rt_dev);
				if (!dev)
					return -ENODEV;
			}			
			return atrtr_create(&rt, dev);
	}
	return -EINVAL;
}

/* Called from proc fs - just make it print the ifaces neatly */
static int atalk_if_get_info(char *buffer, char **start, off_t offset,
			     int length)
{
	off_t pos = 0;
	off_t begin = 0;
	struct atalk_iface *iface;
	int len = sprintf(buffer, "Interface	  Address   "
				  "Networks   Status\n");

	spin_lock_bh(&atalk_iface_lock);
	for (iface = atalk_iface_list; iface; iface = iface->next) {
		len += sprintf(buffer+len,"%-16s %04X:%02X  %04X-%04X  %d\n",
			       iface->dev->name, ntohs(iface->address.s_net),
			       iface->address.s_node,
			       ntohs(iface->nets.nr_firstnet),
			       ntohs(iface->nets.nr_lastnet), iface->status);
		pos = begin + len;
		if (pos < offset) {
			len   = 0;
			begin = pos;
		}
		if (pos > offset + length)
			break;
	}
	spin_unlock_bh(&atalk_iface_lock);

	*start = buffer + (offset - begin);
	len -= (offset - begin);
	if (len > length)
		len = length;
	return len;
}

/* Called from proc fs - just make it print the routes neatly */
static int atalk_rt_get_info(char *buffer, char **start, off_t offset,
			     int length)
{
	off_t pos = 0;
	off_t begin = 0;
	int len = sprintf(buffer, "Target        Router  Flags Dev\n");
	struct atalk_route *rt;

	if (atrtr_default.dev) {
		rt = &atrtr_default;
		len += sprintf(buffer + len,"Default     %04X:%02X  %-4d  %s\n",
			       ntohs(rt->gateway.s_net), rt->gateway.s_node,
			       rt->flags, rt->dev->name);
	}

	read_lock_bh(&atalk_router_lock);
	for (rt = atalk_router_list; rt; rt = rt->next) {
		len += sprintf(buffer + len,
				"%04X:%02X     %04X:%02X  %-4d  %s\n",
			       ntohs(rt->target.s_net), rt->target.s_node,
			       ntohs(rt->gateway.s_net), rt->gateway.s_node,
			       rt->flags, rt->dev->name);
		pos = begin + len;
		if (pos < offset) {
			len = 0;
			begin = pos;
		}
		if (pos > offset + length)
			break;
	}
	read_unlock_bh(&atalk_router_lock);

	*start = buffer + (offset - begin);
	len -= (offset - begin);
	if (len > length)
		len = length;
	return len;
}

/**************************************************************************\
*                                                                          *
* Handling for system calls applied via the various interfaces to an       *
* AppleTalk socket object.                                                 *
*                                                                          *
\**************************************************************************/

/*
 * Checksum: This is 'optional'. It's quite likely also a good
 * candidate for assembler hackery 8)
 */
unsigned short atalk_checksum(struct ddpehdr *ddp, int len)
{
	unsigned long sum = 0;	/* Assume unsigned long is >16 bits */
	unsigned char *data = (unsigned char *) ddp;

	len  -= 4;		/* skip header 4 bytes */
	data += 4;

	/* This ought to be unwrapped neatly. I'll trust gcc for now */
	while (len--) {
		sum += *data;
		sum <<= 1;
		if (sum & 0x10000) {
			sum++;
			sum &= 0xFFFF;
		}
		data++;
	}
	/* Use 0xFFFF for 0. 0 itself means none */
	return sum ? htons((unsigned short) sum) : 0xFFFF;
}

/*
 * Create a socket. Initialise the socket, blank the addresses
 * set the state.
 */
static int atalk_create(struct socket *sock, int protocol)
{
	struct sock *sk = sk_alloc(PF_APPLETALK, GFP_KERNEL, 1);

	if (!sk)
		return -ENOMEM;

	switch (sock->type) {
		/*
		 * We permit SOCK_DGRAM and RAW is an extension. It is
		 * trivial to do and gives you the full ELAP frame.
		 * Should be handy for CAP 8) 
		 */
		case SOCK_RAW:
		case SOCK_DGRAM:
			sock->ops = &atalk_dgram_ops;
			break;
			
		case SOCK_STREAM:
			/*
			 * TODO: if you want to implement ADSP, here's the
			 * place to start
			 */
			/*
			sock->ops = &atalk_stream_ops;
			break;
			*/
		default:
			sk_free(sk);
			return -ESOCKTNOSUPPORT;
	}

	MOD_INC_USE_COUNT;
	sock_init_data(sock, sk);
	sk->destruct = NULL;
	/* Checksums on by default */
	sk->zapped = 1;
	return 0;
}

/* Free a socket. No work needed */
static int atalk_release(struct socket *sock)
{
	struct sock *sk = sock->sk;

	if (!sk)
		return 0;

	if (!sk->dead)
		sk->state_change(sk);

	sk->dead = 1;
	sock->sk = NULL;
	atalk_destroy_socket(sk);
	return 0;
}

/*
 * Pick a source port when one is not given. If we can
 * find a suitable free one, we insert the socket into
 * the tables using it.
 *
 * This whole operation must be atomic.
 */
static int atalk_pick_and_bind_port(struct sock *sk, struct sockaddr_at *sat)
{
	struct sock *s;
	int retval;

	spin_lock_bh(&atalk_sockets_lock);

	for (sat->sat_port = ATPORT_RESERVED;
	     sat->sat_port < ATPORT_LAST;
	     sat->sat_port++) {
		for (s = atalk_sockets; s; s = s->next) {
			if (s->protinfo.af_at.src_net == sat->sat_addr.s_net &&
			    s->protinfo.af_at.src_node ==
			    	sat->sat_addr.s_node &&
			    s->protinfo.af_at.src_port == sat->sat_port)
				goto try_next_port;
		}

		/* Wheee, it's free, assign and insert. */
		sk->next = atalk_sockets;
		if (sk->next)
			atalk_sockets->pprev = &sk->next;
		atalk_sockets = sk;
		sk->pprev = &atalk_sockets;
		sk->protinfo.af_at.src_port = sat->sat_port;
		retval = 0;
		goto out;

	try_next_port:
		;
	}

	retval = -EBUSY;
out:	spin_unlock_bh(&atalk_sockets_lock);
	return retval;
}

static int atalk_autobind(struct sock *sk)
{
	struct sockaddr_at sat;
	int n;
	struct at_addr *ap = atalk_find_primary();

	if (!ap || ap->s_net == htons(ATADDR_ANYNET))
		return -EADDRNOTAVAIL;

	sk->protinfo.af_at.src_net  = sat.sat_addr.s_net  = ap->s_net;
	sk->protinfo.af_at.src_node = sat.sat_addr.s_node = ap->s_node;

	n = atalk_pick_and_bind_port(sk, &sat);
	if (n < 0)
		return n;

	sk->zapped = 0;
	return 0;
}

/* Set the address 'our end' of the connection */
static int atalk_bind(struct socket *sock, struct sockaddr *uaddr, int addr_len)
{
	struct sockaddr_at *addr = (struct sockaddr_at *)uaddr;
	struct sock *sk = sock->sk;

	if (!sk->zapped || addr_len != sizeof(struct sockaddr_at))
		return -EINVAL;

	if (addr->sat_family != AF_APPLETALK)
		return -EAFNOSUPPORT;

	if (addr->sat_addr.s_net == htons(ATADDR_ANYNET)) {
		struct at_addr *ap = atalk_find_primary();

		if (!ap)
			return -EADDRNOTAVAIL;

		sk->protinfo.af_at.src_net  = addr->sat_addr.s_net = ap->s_net;
		sk->protinfo.af_at.src_node = addr->sat_addr.s_node= ap->s_node;
	} else {
		if (!atalk_find_interface(addr->sat_addr.s_net,
					  addr->sat_addr.s_node))
			return -EADDRNOTAVAIL;

		sk->protinfo.af_at.src_net  = addr->sat_addr.s_net;
		sk->protinfo.af_at.src_node = addr->sat_addr.s_node;
	}

	if (addr->sat_port == ATADDR_ANYPORT) {
		int n = atalk_pick_and_bind_port(sk, addr);

		if (n < 0)
			return n;
	} else {
		sk->protinfo.af_at.src_port = addr->sat_port;

		if (atalk_find_or_insert_socket(sk, addr))
			return -EADDRINUSE;
	}

	sk->zapped = 0;
	return 0;
}

/* Set the address we talk to */
static int atalk_connect(struct socket *sock, struct sockaddr *uaddr,
			 int addr_len, int flags)
{
	struct sock *sk = sock->sk;
	struct sockaddr_at *addr;

	sk->state   = TCP_CLOSE;
	sock->state = SS_UNCONNECTED;

	if (addr_len != sizeof(*addr))
		return -EINVAL;

	addr = (struct sockaddr_at *)uaddr;

	if (addr->sat_family != AF_APPLETALK)
		return -EAFNOSUPPORT;

	if (addr->sat_addr.s_node == ATADDR_BCAST && !sk->broadcast) {
#if 1	
		printk(KERN_WARNING "%s is broken and did not set "
				    "SO_BROADCAST. It will break when 2.2 is "
				    "released.\n",
			current->comm);
#else
		return -EACCES;
#endif			
	}

	if (sk->zapped)
		if (atalk_autobind(sk) < 0)
			return -EBUSY;

	if (!atrtr_get_dev(&addr->sat_addr))
		return -ENETUNREACH;

	sk->protinfo.af_at.dest_port = addr->sat_port;
	sk->protinfo.af_at.dest_net  = addr->sat_addr.s_net;
	sk->protinfo.af_at.dest_node = addr->sat_addr.s_node;

	sock->state = SS_CONNECTED;
	sk->state   = TCP_ESTABLISHED;
	return 0;
}


/*
 * Find the name of an AppleTalk socket. Just copy the right
 * fields into the sockaddr.
 */
static int atalk_getname(struct socket *sock, struct sockaddr *uaddr,
			 int *uaddr_len, int peer)
{
	struct sockaddr_at sat;
	struct sock *sk = sock->sk;

	if (sk->zapped)
		if (atalk_autobind(sk) < 0)
			return -ENOBUFS;

	*uaddr_len = sizeof(struct sockaddr_at);

	if (peer) {
		if (sk->state != TCP_ESTABLISHED)
			return -ENOTCONN;

		sat.sat_addr.s_net  = sk->protinfo.af_at.dest_net;
		sat.sat_addr.s_node = sk->protinfo.af_at.dest_node;
		sat.sat_port = sk->protinfo.af_at.dest_port;
	} else {
		sat.sat_addr.s_net  = sk->protinfo.af_at.src_net;
		sat.sat_addr.s_node = sk->protinfo.af_at.src_node;
		sat.sat_port = sk->protinfo.af_at.src_port;
	}

	sat.sat_family = AF_APPLETALK;
	memcpy(uaddr, &sat, sizeof(sat));
	return 0;
}

/*
 * Receive a packet (in skb) from device dev. This has come from the SNAP
 * decoder, and on entry skb->h.raw is the DDP header, skb->len is the DDP
 * header, skb->len is the DDP length. The physical headers have been
 * extracted. PPP should probably pass frames marked as for this layer.
 * [ie ARPHRD_ETHERTALK]
 */
static int atalk_rcv(struct sk_buff *skb, struct net_device *dev,
			struct packet_type *pt)
{
	struct ddpehdr *ddp = (void *) skb->h.raw;
	struct sock *sock;
	struct atalk_iface *atif;
	struct sockaddr_at tosat;
        int origlen;
        struct ddpebits ddphv;

	/* Size check */
	if (skb->len < sizeof(*ddp))
		goto freeit;

	/*
	 *	Fix up the length field	[Ok this is horrible but otherwise
	 *	I end up with unions of bit fields and messy bit field order
	 *	compiler/endian dependencies..]
	 *
	 *	FIXME: This is a write to a shared object. Granted it
	 *	happens to be safe BUT.. (Its safe as user space will not
	 *	run until we put it back)
	 */
	*((__u16 *)&ddphv) = ntohs(*((__u16 *)ddp));

	/* Trim buffer in case of stray trailing data */
	origlen = skb->len;
	skb_trim(skb, min_t(unsigned int, skb->len, ddphv.deh_len));

	/*
	 * Size check to see if ddp->deh_len was crap
	 * (Otherwise we'll detonate most spectacularly
	 * in the middle of recvmsg()).
	 */
	if (skb->len < sizeof(*ddp))
		goto freeit;

	/*
	 * Any checksums. Note we don't do htons() on this == is assumed to be
	 * valid for net byte orders all over the networking code...
	 */
	if (ddp->deh_sum &&
	    atalk_checksum(ddp, ddphv.deh_len) != ddp->deh_sum)
		/* Not a valid AppleTalk frame - dustbin time */
		goto freeit;

	/* Check the packet is aimed at us */
	if (!ddp->deh_dnet)	/* Net 0 is 'this network' */
		atif = atalk_find_anynet(ddp->deh_dnode, dev);
	else
		atif = atalk_find_interface(ddp->deh_dnet, ddp->deh_dnode);

	/* Not ours, so we route the packet via the correct AppleTalk iface */
	if (!atif) {
		struct atalk_route *rt;
		struct at_addr ta;

		/*
		 * Don't route multicast, etc., packets, or packets
		 * sent to "this network" 
		 */
		if (skb->pkt_type != PACKET_HOST || !ddp->deh_dnet) {
			/* FIXME:
			 * Can it ever happen that a packet is from a PPP
			 * iface and needs to be broadcast onto the default
			 * network? */
			if (dev->type == ARPHRD_PPP)
				printk(KERN_DEBUG "AppleTalk: didn't forward "
						  "broadcast packet received "
						  "from PPP iface\n");
			goto freeit;
		}

		ta.s_net  = ddp->deh_dnet;
		ta.s_node = ddp->deh_dnode;

		/* Route the packet */
		rt = atrtr_find(&ta);
		if (!rt || ddphv.deh_hops == DDP_MAXHOPS)
			goto freeit;
		ddphv.deh_hops++;

		/*
		 * Route goes through another gateway, so
		 * set the target to the gateway instead.
		 */
		if (rt->flags & RTF_GATEWAY) {
			ta.s_net  = rt->gateway.s_net;
			ta.s_node = rt->gateway.s_node;
		}

                /* Fix up skb->len field */
                skb_trim(skb, min_t(unsigned int, origlen, rt->dev->hard_header_len +
			ddp_dl->header_length + ddphv.deh_len));

		/* Mend the byte order */
		*((__u16 *)ddp) = ntohs(*((__u16 *)&ddphv));

		/*
		 * Send the buffer onwards
		 *
		 * Now we must always be careful. If it's come from 
		 * LocalTalk to EtherTalk it might not fit
		 *
		 * Order matters here: If a packet has to be copied
		 * to make a new headroom (rare hopefully) then it
		 * won't need unsharing.
		 *
		 * Note. ddp-> becomes invalid at the realloc.
		 */
		if (skb_headroom(skb) < 22) {
			/* 22 bytes - 12 ether, 2 len, 3 802.2 5 snap */
			struct sk_buff *nskb = skb_realloc_headroom(skb, 32);
			kfree_skb(skb);
			if (!nskb) 
				goto out;
			skb = nskb;
		} else
			skb = skb_unshare(skb, GFP_ATOMIC);
		
		/*
		 * If the buffer didn't vanish into the lack of
		 * space bitbucket we can send it.
		 */
		if (skb && aarp_send_ddp(rt->dev, skb, &ta, NULL) == -1)
			goto freeit;
		goto out;
	}

#if defined(CONFIG_IPDDP) || defined(CONFIG_IPDDP_MODULE)
        /* Check if IP-over-DDP */
        if (skb->data[12] == 22) {
                struct net_device *dev = __dev_get_by_name("ipddp0");
		struct net_device_stats *stats;

		/* This needs to be able to handle ipddp"N" devices */
                if (!dev)
                        return -ENODEV;

                skb->protocol = htons(ETH_P_IP);
                skb_pull(skb, 13);
                skb->dev = dev;
                skb->h.raw = skb->data;

		stats = dev->priv;
                stats->rx_packets++;
                stats->rx_bytes += skb->len + 13;
                netif_rx(skb);  /* Send the SKB up to a higher place. */
		goto out;
        }
#endif
	/*
	 * Which socket - atalk_search_socket() looks for a *full match*
	 * of the <net,node,port> tuple.
	 */
	tosat.sat_addr.s_net  = ddp->deh_dnet;
	tosat.sat_addr.s_node = ddp->deh_dnode;
	tosat.sat_port = ddp->deh_dport;

	sock = atalk_search_socket(&tosat, atif);
	if (!sock) /* But not one of our sockets */
		goto freeit;

	/* Queue packet (standard) */
	skb->sk = sock;

	if (sock_queue_rcv_skb(sock, skb) < 0)
		goto freeit;
	goto out;
freeit:	kfree_skb(skb);
out:	return 0;
}

/*
 * Receive a LocalTalk frame. We make some demands on the caller here.
 * Caller must provide enough headroom on the packet to pull the short
 * header and append a long one.
 */
static int ltalk_rcv(struct sk_buff *skb, struct net_device *dev,
			struct packet_type *pt)
{
	struct ddpehdr *ddp;
	struct at_addr *ap;

	/* Expand any short form frames */
	if (skb->mac.raw[2] == 1) {
		/* Find our address */

		ap = atalk_find_dev_addr(dev);
		if (!ap || skb->len < sizeof(struct ddpshdr)) {
			kfree_skb(skb);
			return 0;
		}

		/*
		 * The push leaves us with a ddephdr not an shdr, and
		 * handily the port bytes in the right place preset.
		 */

		skb_push(skb, sizeof(*ddp) - 4);
		ddp = (struct ddpehdr *)skb->data;

		/* Now fill in the long header */

	 	/*
	 	 * These two first. The mac overlays the new source/dest
	 	 * network information so we MUST copy these before
	 	 * we write the network numbers !
	 	 */

		ddp->deh_dnode = skb->mac.raw[0];     /* From physical header */
		ddp->deh_snode = skb->mac.raw[1];     /* From physical header */

		ddp->deh_dnet  = ap->s_net;	/* Network number */
		ddp->deh_snet  = ap->s_net;
		ddp->deh_sum   = 0;		/* No checksum */
		/*
		 * Not sure about this bit...
		 */
		ddp->deh_len   = skb->len;
		ddp->deh_hops  = DDP_MAXHOPS;	/* Non routable, so force a drop
						   if we slip up later */
		/* Mend the byte order */
		*((__u16 *)ddp) = htons(*((__u16 *)ddp));
	}
	skb->h.raw = skb->data;

	return atalk_rcv(skb, dev, pt);
}

static int atalk_sendmsg(struct socket *sock, struct msghdr *msg, int len,
				struct scm_cookie *scm)
{
	struct sock *sk = sock->sk;
	struct sockaddr_at *usat = (struct sockaddr_at *)msg->msg_name;
	int flags = msg->msg_flags;
	int loopback = 0;
	struct sockaddr_at local_satalk, gsat;
	struct sk_buff *skb;
	struct net_device *dev;
	struct ddpehdr *ddp;
	int size;
	struct atalk_route *rt;
	int err;

	if (flags & ~MSG_DONTWAIT)
		return -EINVAL;

	if (len > DDP_MAXSZ)
		return -EMSGSIZE;

	if (usat) {
		if (sk->zapped)
			if (atalk_autobind(sk) < 0)
				return -EBUSY;

		if (msg->msg_namelen < sizeof(*usat) ||
		    usat->sat_family != AF_APPLETALK)
			return -EINVAL;

		/* netatalk doesn't implement this check */
		if (usat->sat_addr.s_node == ATADDR_BCAST && !sk->broadcast) {
			printk(KERN_INFO "SO_BROADCAST: Fix your netatalk as "
					 "it will break before 2.2\n");
#if 0
			return -EPERM;
#endif
		}
	} else {
		if (sk->state != TCP_ESTABLISHED)
			return -ENOTCONN;
		usat = &local_satalk;
		usat->sat_family = AF_APPLETALK;
		usat->sat_port   = sk->protinfo.af_at.dest_port;
		usat->sat_addr.s_node = sk->protinfo.af_at.dest_node;
		usat->sat_addr.s_net  = sk->protinfo.af_at.dest_net;
	}

	/* Build a packet */
	SOCK_DEBUG(sk, "SK %p: Got address.\n", sk);

	/* For headers */
	size = sizeof(struct ddpehdr) + len + ddp_dl->header_length;

	if (usat->sat_addr.s_net || usat->sat_addr.s_node == ATADDR_ANYNODE) {
		rt = atrtr_find(&usat->sat_addr);
		if (!rt)
			return -ENETUNREACH;

		dev = rt->dev;
	} else {
		struct at_addr at_hint;

		at_hint.s_node = 0;
		at_hint.s_net  = sk->protinfo.af_at.src_net;

		rt = atrtr_find(&at_hint);
		if (!rt)
			return -ENETUNREACH;

		dev = rt->dev;
	}

	SOCK_DEBUG(sk, "SK %p: Size needed %d, device %s\n",
			sk, size, dev->name);

	size += dev->hard_header_len;
	skb = sock_alloc_send_skb(sk, size, (flags & MSG_DONTWAIT), &err);
	if (!skb)
		return err;
	
	skb->sk = sk;
	skb_reserve(skb, ddp_dl->header_length);
	skb_reserve(skb, dev->hard_header_len);
	skb->dev = dev;

	SOCK_DEBUG(sk, "SK %p: Begin build.\n", sk);

	ddp = (struct ddpehdr *)skb_put(skb, sizeof(struct ddpehdr));
	ddp->deh_pad  = 0;
	ddp->deh_hops = 0;
	ddp->deh_len  = len + sizeof(*ddp);
	/*
	 * Fix up the length field [Ok this is horrible but otherwise
	 * I end up with unions of bit fields and messy bit field order
	 * compiler/endian dependencies..
	 */
	*((__u16 *)ddp) = ntohs(*((__u16 *)ddp));

	ddp->deh_dnet  = usat->sat_addr.s_net;
	ddp->deh_snet  = sk->protinfo.af_at.src_net;
	ddp->deh_dnode = usat->sat_addr.s_node;
	ddp->deh_snode = sk->protinfo.af_at.src_node;
	ddp->deh_dport = usat->sat_port;
	ddp->deh_sport = sk->protinfo.af_at.src_port;

	SOCK_DEBUG(sk, "SK %p: Copy user data (%d bytes).\n", sk, len);

	err = memcpy_fromiovec(skb_put(skb, len), msg->msg_iov, len);
	if (err) {
		kfree_skb(skb);
		return -EFAULT;
	}

	if (sk->no_check == 1)
		ddp->deh_sum = 0;
	else
		ddp->deh_sum = atalk_checksum(ddp, len + sizeof(*ddp));

	/*
	 * Loopback broadcast packets to non gateway targets (ie routes
	 * to group we are in)
	 */
	if (ddp->deh_dnode == ATADDR_BCAST &&
	    !(rt->flags & RTF_GATEWAY) && !(dev->flags & IFF_LOOPBACK)) {
		struct sk_buff *skb2 = skb_copy(skb, GFP_KERNEL);

		if (skb2) {
			loopback = 1;
			SOCK_DEBUG(sk, "SK %p: send out(copy).\n", sk);
			if (aarp_send_ddp(dev, skb2,
					  &usat->sat_addr, NULL) == -1)
				kfree_skb(skb2);
				/* else queued/sent above in the aarp queue */
		}
	}

	if (dev->flags & IFF_LOOPBACK || loopback) {
		SOCK_DEBUG(sk, "SK %p: Loop back.\n", sk);
		/* loop back */
		skb_orphan(skb);
		ddp_dl->datalink_header(ddp_dl, skb, dev->dev_addr);
		skb->mac.raw = skb->data;
		skb->h.raw   = skb->data + ddp_dl->header_length +
				dev->hard_header_len;
		skb_pull(skb, dev->hard_header_len);
		skb_pull(skb, ddp_dl->header_length);
		atalk_rcv(skb, dev, NULL);
	} else {
		SOCK_DEBUG(sk, "SK %p: send out.\n", sk);
		if (rt->flags & RTF_GATEWAY) {
		    gsat.sat_addr = rt->gateway;
		    usat = &gsat;
		}

		if (aarp_send_ddp(dev, skb, &usat->sat_addr, NULL) == -1)
			kfree_skb(skb);
		/* else queued/sent above in the aarp queue */
	}
	SOCK_DEBUG(sk, "SK %p: Done write (%d).\n", sk, len);

	return len;
}

static int atalk_recvmsg(struct socket *sock, struct msghdr *msg, int size,
			 int flags, struct scm_cookie *scm)
{
	struct sock *sk = sock->sk;
	struct sockaddr_at *sat = (struct sockaddr_at *)msg->msg_name;
	struct ddpehdr *ddp = NULL;
	int copied = 0;
	int err = 0;
        struct ddpebits ddphv;
	struct sk_buff *skb;

	skb = skb_recv_datagram(sk, flags & ~MSG_DONTWAIT,
				flags & MSG_DONTWAIT, &err);
	if (!skb)
		return err;

	ddp = (struct ddpehdr *)(skb->h.raw);
	*((__u16 *)&ddphv) = ntohs(*((__u16 *)ddp));

	if (sk->type == SOCK_RAW) {
		copied = ddphv.deh_len;
		if (copied > size) {
			copied = size;
			msg->msg_flags |= MSG_TRUNC;
		}

		err = skb_copy_datagram_iovec(skb, 0, msg->msg_iov, copied);
	} else {
		copied = ddphv.deh_len - sizeof(*ddp);
		if (copied > size) {
			copied = size;
			msg->msg_flags |= MSG_TRUNC;
		}
		err = skb_copy_datagram_iovec(skb, sizeof(*ddp),
						msg->msg_iov, copied);
	}

	if (!err) {
		if (sat) {
			sat->sat_family      = AF_APPLETALK;
			sat->sat_port        = ddp->deh_sport;
			sat->sat_addr.s_node = ddp->deh_snode;
			sat->sat_addr.s_net  = ddp->deh_snet;
		}
		msg->msg_namelen = sizeof(*sat);
	}

	skb_free_datagram(sk, skb);	/* Free the datagram. */
	return err ? err : copied;
}


/*
 * AppleTalk ioctl calls.
 */
static int atalk_ioctl(struct socket *sock,unsigned int cmd, unsigned long arg)
{
	long amount = 0;
	struct sock *sk = sock->sk;

	switch (cmd) {
		/* Protocol layer */
		case TIOCOUTQ:
			amount = sk->sndbuf - atomic_read(&sk->wmem_alloc);
			if (amount < 0)
				amount = 0;
			break;
		case TIOCINQ:
		{
			/* These two are safe on a single CPU system as only
			 * user tasks fiddle here */
			struct sk_buff *skb = skb_peek(&sk->receive_queue);

			if (skb)
				amount = skb->len-sizeof(struct ddpehdr);
			break;
		}
		case SIOCGSTAMP:
			if (!sk)
				return -EINVAL;
			if (!sk->stamp.tv_sec)
				return -ENOENT;
			return copy_to_user((void *)arg, &sk->stamp,
					sizeof(struct timeval)) ? -EFAULT : 0;
		/* Routing */
		case SIOCADDRT:
		case SIOCDELRT:
			if (!capable(CAP_NET_ADMIN))
				return -EPERM;
			return atrtr_ioctl(cmd, (void *)arg);
		/* Interface */
		case SIOCGIFADDR:
		case SIOCSIFADDR:
		case SIOCGIFBRDADDR:
		case SIOCATALKDIFADDR:
		case SIOCDIFADDR:
		case SIOCSARP:	/* proxy AARP */
		case SIOCDARP:	/* proxy AARP */
		{
			int ret;

			rtnl_lock();
			ret = atif_ioctl(cmd, (void *)arg);
			rtnl_unlock();

			return ret;
		}
		/* Physical layer ioctl calls */
		case SIOCSIFLINK:
		case SIOCGIFHWADDR:
		case SIOCSIFHWADDR:
		case SIOCGIFFLAGS:
		case SIOCSIFFLAGS:
		case SIOCGIFMTU:
		case SIOCGIFCONF:
		case SIOCADDMULTI:
		case SIOCDELMULTI:
		case SIOCGIFCOUNT:
		case SIOCGIFINDEX:
		case SIOCGIFNAME:
			return dev_ioctl(cmd,(void *) arg);
		case SIOCSIFMETRIC:
		case SIOCSIFBRDADDR:
		case SIOCGIFNETMASK:
		case SIOCSIFNETMASK:
		case SIOCGIFMEM:
		case SIOCSIFMEM:
		case SIOCGIFDSTADDR:
		case SIOCSIFDSTADDR:
			return -EINVAL;
		default:
			return -EINVAL;
	}

	return put_user(amount, (int *)arg);
}

static struct net_proto_family atalk_family_ops =
{
	PF_APPLETALK,
	atalk_create
};

static struct proto_ops SOCKOPS_WRAPPED(atalk_dgram_ops)=
{
	family:		PF_APPLETALK,

	release:	atalk_release,
	bind:		atalk_bind,
	connect:	atalk_connect,
	socketpair:	sock_no_socketpair,
	accept:		sock_no_accept,
	getname:	atalk_getname,
	poll:		datagram_poll,
	ioctl:		atalk_ioctl,
	listen:		sock_no_listen,
	shutdown:	sock_no_shutdown,
	setsockopt:	sock_no_setsockopt,
	getsockopt:	sock_no_getsockopt,
	sendmsg:	atalk_sendmsg,
	recvmsg:	atalk_recvmsg,
	mmap:		sock_no_mmap,
	sendpage:	sock_no_sendpage,
};

#include <linux/smp_lock.h>
SOCKOPS_WRAP(atalk_dgram, PF_APPLETALK);

static struct notifier_block ddp_notifier=
{
	ddp_device_event,
	NULL,
	0
};

struct packet_type ltalk_packet_type=
{
	0,
	NULL,
	ltalk_rcv,
	NULL,
	NULL
};

struct packet_type ppptalk_packet_type=
{
	0,
	NULL,
	atalk_rcv,
	NULL,
	NULL
};

static char ddp_snap_id[] = {0x08, 0x00, 0x07, 0x80, 0x9B};

/* Export symbols for use by drivers when AppleTalk is a module */
EXPORT_SYMBOL(aarp_send_ddp);
EXPORT_SYMBOL(atrtr_get_dev);
EXPORT_SYMBOL(atalk_find_dev_addr);

/* Called by proto.c on kernel start up */
static int __init atalk_init(void)
{
	(void) sock_register(&atalk_family_ops);
	ddp_dl = register_snap_client(ddp_snap_id, atalk_rcv);
	if (!ddp_dl)
		printk(KERN_CRIT "Unable to register DDP with SNAP.\n");

	ltalk_packet_type.type = htons(ETH_P_LOCALTALK);
	dev_add_pack(&ltalk_packet_type);

	ppptalk_packet_type.type = htons(ETH_P_PPPTALK);
	dev_add_pack(&ppptalk_packet_type);

	register_netdevice_notifier(&ddp_notifier);
	aarp_proto_init();

	proc_net_create("appletalk", 0, atalk_get_info);
	proc_net_create("atalk_route", 0, atalk_rt_get_info);
	proc_net_create("atalk_iface", 0, atalk_if_get_info);
#ifdef CONFIG_PROC_FS
	aarp_register_proc_fs();
#endif /* CONFIG_PROC_FS */
#ifdef CONFIG_SYSCTL
	atalk_register_sysctl();
#endif /* CONFIG_SYSCTL */
	printk(KERN_INFO "NET4: AppleTalk 0.18a for Linux NET4.0\n");
	return 0;
}
module_init(atalk_init);

#ifdef MODULE
/*
 * Note on MOD_{INC,DEC}_USE_COUNT:
 *
 * Use counts are incremented/decremented when
 * sockets are created/deleted.
 *
 * AppleTalk interfaces are not incremented until atalkd is run
 * and are only decremented when they are downed.
 *
 * Ergo, before the AppleTalk module can be removed, all AppleTalk
 * sockets be closed from user space.
 */
static void __exit atalk_exit(void)
{
#ifdef CONFIG_SYSCTL
	atalk_unregister_sysctl();
#endif /* CONFIG_SYSCTL */
	proc_net_remove("appletalk");
	proc_net_remove("atalk_route");
	proc_net_remove("atalk_iface");
#ifdef CONFIG_PROC_FS
	aarp_unregister_proc_fs();
#endif /* CONFIG_PROC_FS */
	aarp_cleanup_module();	/* General aarp clean-up. */
	unregister_netdevice_notifier(&ddp_notifier);
	dev_remove_pack(&ltalk_packet_type);
	dev_remove_pack(&ppptalk_packet_type);
	unregister_snap_client(ddp_snap_id);
	sock_unregister(PF_APPLETALK);
}
module_exit(atalk_exit);
#endif  /* MODULE */
#endif  /* CONFIG_ATALK || CONFIG_ATALK_MODULE */
