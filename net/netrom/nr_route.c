/*
 *	NET/ROM release 007
 *
 *	This code REQUIRES 2.1.15 or higher/ NET3.038
 *
 *	This module:
 *		This module is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *	History
 *	NET/ROM 001	Jonathan(G4KLX)	First attempt.
 *	NET/ROM	003	Jonathan(G4KLX)	Use SIOCADDRT/SIOCDELRT ioctl values
 *					for NET/ROM routes.
 *					Use '*' for a blank mnemonic in /proc/net/nr_nodes.
 *					Change default quality for new neighbour when same
 *					as node callsign.
 *			Alan Cox(GW4PTS) Added the firewall hooks.
 *	NET/ROM 006	Jonathan(G4KLX)	Added the setting of digipeated neighbours.
 *			Tomi(OH2BNS)	Routing quality and link failure changes.
 *					Device refcnt fixes.
 */

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <net/ax25.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <net/arp.h>
#include <linux/if_arp.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/fcntl.h>
#include <linux/termios.h>	/* For TIOCINQ/OUTQ */
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>
#include <linux/netfilter.h>
#include <linux/init.h>
#include <net/netrom.h>

static unsigned int nr_neigh_no = 1;

static struct nr_node  *nr_node_list;
static struct nr_neigh *nr_neigh_list;

static void nr_remove_neigh(struct nr_neigh *);

/*
 *	Add a new route to a node, and in the process add the node and the
 *	neighbour if it is new.
 */
static int nr_add_node(ax25_address *nr, const char *mnemonic, ax25_address *ax25,
	ax25_digi *ax25_digi, struct net_device *dev, int quality, int obs_count)
{
	struct nr_node  *nr_node;
	struct nr_neigh *nr_neigh;
	struct nr_route nr_route;
	struct net_device *tdev;
	unsigned long flags;
	int i, found;

	/* Can't add routes to ourself */
	if ((tdev = nr_dev_get(nr)) != NULL) {
		dev_put(tdev);
		return -EINVAL;
	}

	for (nr_node = nr_node_list; nr_node != NULL; nr_node = nr_node->next)
		if (ax25cmp(nr, &nr_node->callsign) == 0)
			break;

	for (nr_neigh = nr_neigh_list; nr_neigh != NULL; nr_neigh = nr_neigh->next)
		if (ax25cmp(ax25, &nr_neigh->callsign) == 0 && nr_neigh->dev == dev)
			break;

	/*
	 * The L2 link to a neighbour has failed in the past
	 * and now a frame comes from this neighbour. We assume
	 * it was a temporary trouble with the link and reset the
	 * routes now (and not wait for a node broadcast).
	 */
	if (nr_neigh != NULL && nr_neigh->failed != 0 && quality == 0) {
		struct nr_node *node;

		for (node = nr_node_list; node != NULL; node = node->next)
			for (i = 0; i < node->count; i++)
				if (node->routes[i].neighbour == nr_neigh)
					if (i < node->which)
						node->which = i;
	}

	if (nr_neigh != NULL)
		nr_neigh->failed = 0;

	if (quality == 0 && nr_neigh != NULL && nr_node != NULL)
		return 0;

	if (nr_neigh == NULL) {
		if ((nr_neigh = kmalloc(sizeof(*nr_neigh), GFP_ATOMIC)) == NULL)
			return -ENOMEM;

		nr_neigh->callsign = *ax25;
		nr_neigh->digipeat = NULL;
		nr_neigh->ax25     = NULL;
		nr_neigh->dev      = dev;
		nr_neigh->quality  = sysctl_netrom_default_path_quality;
		nr_neigh->locked   = 0;
		nr_neigh->count    = 0;
		nr_neigh->number   = nr_neigh_no++;
		nr_neigh->failed   = 0;

		if (ax25_digi != NULL && ax25_digi->ndigi > 0) {
			if ((nr_neigh->digipeat = kmalloc(sizeof(*ax25_digi), GFP_KERNEL)) == NULL) {
				kfree(nr_neigh);
				return -ENOMEM;
			}
			memcpy(nr_neigh->digipeat, ax25_digi, sizeof(ax25_digi));
		}

		dev_hold(nr_neigh->dev);

		save_flags(flags);
		cli();

		nr_neigh->next = nr_neigh_list;
		nr_neigh_list  = nr_neigh;

		restore_flags(flags);
	}

	if (quality != 0 && ax25cmp(nr, ax25) == 0 && !nr_neigh->locked)
		nr_neigh->quality = quality;

	if (nr_node == NULL) {
		if ((nr_node = kmalloc(sizeof(*nr_node), GFP_ATOMIC)) == NULL)
			return -ENOMEM;

		nr_node->callsign = *nr;
		strcpy(nr_node->mnemonic, mnemonic);

		nr_node->which = 0;
		nr_node->count = 1;

		nr_node->routes[0].quality   = quality;
		nr_node->routes[0].obs_count = obs_count;
		nr_node->routes[0].neighbour = nr_neigh;

		save_flags(flags);
		cli();

		nr_node->next = nr_node_list;
		nr_node_list  = nr_node;

		restore_flags(flags);

		nr_neigh->count++;

		return 0;
	}

	if (quality != 0)
		strcpy(nr_node->mnemonic, mnemonic);

	for (found = 0, i = 0; i < nr_node->count; i++) {
		if (nr_node->routes[i].neighbour == nr_neigh) {
			nr_node->routes[i].quality   = quality;
			nr_node->routes[i].obs_count = obs_count;
			found = 1;
			break;
		}
	}

	if (!found) {
		/* We have space at the bottom, slot it in */
		if (nr_node->count < 3) {
			nr_node->routes[2] = nr_node->routes[1];
			nr_node->routes[1] = nr_node->routes[0];

			nr_node->routes[0].quality   = quality;
			nr_node->routes[0].obs_count = obs_count;
			nr_node->routes[0].neighbour = nr_neigh;

			nr_node->which++;
			nr_node->count++;
			nr_neigh->count++;
		} else {
			/* It must be better than the worst */
			if (quality > nr_node->routes[2].quality) {
				nr_node->routes[2].neighbour->count--;

				if (nr_node->routes[2].neighbour->count == 0 && !nr_node->routes[2].neighbour->locked)
					nr_remove_neigh(nr_node->routes[2].neighbour);

				nr_node->routes[2].quality   = quality;
				nr_node->routes[2].obs_count = obs_count;
				nr_node->routes[2].neighbour = nr_neigh;

				nr_neigh->count++;
			}
		}
	}

	/* Now re-sort the routes in quality order */
	switch (nr_node->count) {
		case 3:
			if (nr_node->routes[1].quality > nr_node->routes[0].quality) {
				switch (nr_node->which) {
					case 0:  nr_node->which = 1; break;
					case 1:  nr_node->which = 0; break;
					default: break;
				}
				nr_route           = nr_node->routes[0];
				nr_node->routes[0] = nr_node->routes[1];
				nr_node->routes[1] = nr_route;
			}
			if (nr_node->routes[2].quality > nr_node->routes[1].quality) {
				switch (nr_node->which) {
					case 1:  nr_node->which = 2; break;
					case 2:  nr_node->which = 1; break;
					default: break;
				}
				nr_route           = nr_node->routes[1];
				nr_node->routes[1] = nr_node->routes[2];
				nr_node->routes[2] = nr_route;
			}
		case 2:
			if (nr_node->routes[1].quality > nr_node->routes[0].quality) {
				switch (nr_node->which) {
					case 0:  nr_node->which = 1; break;
					case 1:  nr_node->which = 0; break;
					default: break;
				}
				nr_route           = nr_node->routes[0];
				nr_node->routes[0] = nr_node->routes[1];
				nr_node->routes[1] = nr_route;
			}
		case 1:
			break;
	}

	for (i = 0; i < nr_node->count; i++) {
		if (nr_node->routes[i].neighbour == nr_neigh) {
			if (i < nr_node->which)
				nr_node->which = i;
			break;
		}
	}

	return 0;
}

static void nr_remove_node(struct nr_node *nr_node)
{
	struct nr_node *s;
	unsigned long flags;

	save_flags(flags);
	cli();

	if ((s = nr_node_list) == nr_node) {
		nr_node_list = nr_node->next;
		restore_flags(flags);
		kfree(nr_node);
		return;
	}

	while (s != NULL && s->next != NULL) {
		if (s->next == nr_node) {
			s->next = nr_node->next;
			restore_flags(flags);
			kfree(nr_node);
			return;
		}

		s = s->next;
	}

	restore_flags(flags);
}

static void nr_remove_neigh(struct nr_neigh *nr_neigh)
{
	struct nr_neigh *s;
	unsigned long flags;
	
	save_flags(flags);
	cli();

	if ((s = nr_neigh_list) == nr_neigh) {
		nr_neigh_list = nr_neigh->next;
		restore_flags(flags);
		dev_put(nr_neigh->dev);
		if (nr_neigh->digipeat != NULL)
			kfree(nr_neigh->digipeat);
		kfree(nr_neigh);
		return;
	}

	while (s != NULL && s->next != NULL) {
		if (s->next == nr_neigh) {
			s->next = nr_neigh->next;
			restore_flags(flags);
			dev_put(nr_neigh->dev);
			if (nr_neigh->digipeat != NULL)
				kfree(nr_neigh->digipeat);
			kfree(nr_neigh);
			return;
		}

		s = s->next;
	}

	restore_flags(flags);
}

/*
 *	"Delete" a node. Strictly speaking remove a route to a node. The node
 *	is only deleted if no routes are left to it.
 */
static int nr_del_node(ax25_address *callsign, ax25_address *neighbour, struct net_device *dev)
{
	struct nr_node  *nr_node;
	struct nr_neigh *nr_neigh;
	int i;

	for (nr_node = nr_node_list; nr_node != NULL; nr_node = nr_node->next)
		if (ax25cmp(callsign, &nr_node->callsign) == 0)
			break;

	if (nr_node == NULL) return -EINVAL;

	for (nr_neigh = nr_neigh_list; nr_neigh != NULL; nr_neigh = nr_neigh->next)
		if (ax25cmp(neighbour, &nr_neigh->callsign) == 0 && nr_neigh->dev == dev)
			break;

	if (nr_neigh == NULL) return -EINVAL;

	for (i = 0; i < nr_node->count; i++) {
		if (nr_node->routes[i].neighbour == nr_neigh) {
			nr_neigh->count--;

			if (nr_neigh->count == 0 && !nr_neigh->locked)
				nr_remove_neigh(nr_neigh);

			nr_node->count--;

			if (nr_node->count == 0) {
				nr_remove_node(nr_node);
			} else {
				switch (i) {
					case 0:
						nr_node->routes[0] = nr_node->routes[1];
					case 1:
						nr_node->routes[1] = nr_node->routes[2];
					case 2:
						break;
				}
			}

			return 0;
		}
	}

	return -EINVAL;
}

/*
 *	Lock a neighbour with a quality.
 */
static int nr_add_neigh(ax25_address *callsign, ax25_digi *ax25_digi, struct net_device *dev, unsigned int quality)
{
	struct nr_neigh *nr_neigh;
	unsigned long flags;

	for (nr_neigh = nr_neigh_list; nr_neigh != NULL; nr_neigh = nr_neigh->next) {
		if (ax25cmp(callsign, &nr_neigh->callsign) == 0 && nr_neigh->dev == dev) {
			nr_neigh->quality = quality;
			nr_neigh->locked  = 1;
			return 0;
		}
	}

	if ((nr_neigh = kmalloc(sizeof(*nr_neigh), GFP_ATOMIC)) == NULL)
		return -ENOMEM;

	nr_neigh->callsign = *callsign;
	nr_neigh->digipeat = NULL;
	nr_neigh->ax25     = NULL;
	nr_neigh->dev      = dev;
	nr_neigh->quality  = quality;
	nr_neigh->locked   = 1;
	nr_neigh->count    = 0;
	nr_neigh->number   = nr_neigh_no++;
	nr_neigh->failed   = 0;

	if (ax25_digi != NULL && ax25_digi->ndigi > 0) {
		if ((nr_neigh->digipeat = kmalloc(sizeof(*ax25_digi), GFP_KERNEL)) == NULL) {
			kfree(nr_neigh);
			return -ENOMEM;
		}
		memcpy(nr_neigh->digipeat, ax25_digi, sizeof(ax25_digi));
	}

	dev_hold(nr_neigh->dev);

	save_flags(flags);
	cli();

	nr_neigh->next = nr_neigh_list;
	nr_neigh_list  = nr_neigh;

	restore_flags(flags);

	return 0;	
}

/*
 *	"Delete" a neighbour. The neighbour is only removed if the number
 *	of nodes that may use it is zero.
 */
static int nr_del_neigh(ax25_address *callsign, struct net_device *dev, unsigned int quality)
{
	struct nr_neigh *nr_neigh;

	for (nr_neigh = nr_neigh_list; nr_neigh != NULL; nr_neigh = nr_neigh->next)
		if (ax25cmp(callsign, &nr_neigh->callsign) == 0 && nr_neigh->dev == dev)
			break;

	if (nr_neigh == NULL) return -EINVAL;

	nr_neigh->quality = quality;
	nr_neigh->locked  = 0;

	if (nr_neigh->count == 0)
		nr_remove_neigh(nr_neigh);

	return 0;
}

/*
 *	Decrement the obsolescence count by one. If a route is reduced to a
 *	count of zero, remove it. Also remove any unlocked neighbours with
 *	zero nodes routing via it.
 */
static int nr_dec_obs(void)
{
	struct nr_neigh *nr_neigh;
	struct nr_node  *s, *nr_node;
	int i;

	nr_node = nr_node_list;

	while (nr_node != NULL) {
		s       = nr_node;
		nr_node = nr_node->next;

		for (i = 0; i < s->count; i++) {
			switch (s->routes[i].obs_count) {

			case 0:		/* A locked entry */
				break;

			case 1:		/* From 1 -> 0 */
				nr_neigh = s->routes[i].neighbour;

				nr_neigh->count--;

				if (nr_neigh->count == 0 && !nr_neigh->locked)
					nr_remove_neigh(nr_neigh);

				s->count--;

				switch (i) {
					case 0:
						s->routes[0] = s->routes[1];
					case 1:
						s->routes[1] = s->routes[2];
					case 2:
						break;
				}
				break;

			default:
				s->routes[i].obs_count--;
				break;

			}
		}

		if (s->count <= 0)
			nr_remove_node(s);
	}

	return 0;
}

/*
 *	A device has been removed. Remove its routes and neighbours.
 */
void nr_rt_device_down(struct net_device *dev)
{
	struct nr_neigh *s, *nr_neigh = nr_neigh_list;
	struct nr_node  *t, *nr_node;
	int i;

	while (nr_neigh != NULL) {
		s        = nr_neigh;
		nr_neigh = nr_neigh->next;

		if (s->dev == dev) {
			nr_node = nr_node_list;

			while (nr_node != NULL) {
				t       = nr_node;
				nr_node = nr_node->next;

				for (i = 0; i < t->count; i++) {
					if (t->routes[i].neighbour == s) {
						t->count--;

						switch (i) {
							case 0:
								t->routes[0] = t->routes[1];
							case 1:
								t->routes[1] = t->routes[2];
							case 2:
								break;
						}
					}
				}

				if (t->count <= 0)
					nr_remove_node(t);
			}

			nr_remove_neigh(s);
		}
	}
}

/*
 *	Check that the device given is a valid AX.25 interface that is "up".
 *	Or a valid ethernet interface with an AX.25 callsign binding.
 */
static struct net_device *nr_ax25_dev_get(char *devname)
{
	struct net_device *dev;

	if ((dev = dev_get_by_name(devname)) == NULL)
		return NULL;

	if ((dev->flags & IFF_UP) && dev->type == ARPHRD_AX25)
		return dev;

	dev_put(dev);
	return NULL;
}

/*
 *	Find the first active NET/ROM device, usually "nr0".
 */
struct net_device *nr_dev_first(void)
{
	struct net_device *dev, *first = NULL;

	read_lock(&dev_base_lock);
	for (dev = dev_base; dev != NULL; dev = dev->next) {
		if ((dev->flags & IFF_UP) && dev->type == ARPHRD_NETROM)
			if (first == NULL || strncmp(dev->name, first->name, 3) < 0)
				first = dev;
	}

	if (first != NULL)
		dev_hold(first);

	read_unlock(&dev_base_lock);

	return first;
}

/*
 *	Find the NET/ROM device for the given callsign.
 */
struct net_device *nr_dev_get(ax25_address *addr)
{
	struct net_device *dev;

	read_lock(&dev_base_lock);
	for (dev = dev_base; dev != NULL; dev = dev->next) {
		if ((dev->flags & IFF_UP) && dev->type == ARPHRD_NETROM && ax25cmp(addr, (ax25_address *)dev->dev_addr) == 0) {
			dev_hold(dev);
			goto out;
		}
	}
out:
	read_unlock(&dev_base_lock);
	return dev;
}

static ax25_digi *nr_call_to_digi(int ndigis, ax25_address *digipeaters)
{
	static ax25_digi ax25_digi;
	int i;

	if (ndigis == 0)
		return NULL;

	for (i = 0; i < ndigis; i++) {
		ax25_digi.calls[i]    = digipeaters[i];
		ax25_digi.repeated[i] = 0;
	}

	ax25_digi.ndigi      = ndigis;
	ax25_digi.lastrepeat = -1;

	return &ax25_digi;
}

/*
 *	Handle the ioctls that control the routing functions.
 */
int nr_rt_ioctl(unsigned int cmd, void *arg)
{
	struct nr_route_struct nr_route;
	struct net_device *dev;
	int ret;

	switch (cmd) {

		case SIOCADDRT:
			if (copy_from_user(&nr_route, arg, sizeof(struct nr_route_struct)))
				return -EFAULT;
			if ((dev = nr_ax25_dev_get(nr_route.device)) == NULL)
				return -EINVAL;
			if (nr_route.ndigis < 0 || nr_route.ndigis > AX25_MAX_DIGIS) {
				dev_put(dev);
				return -EINVAL;
			}
			switch (nr_route.type) {
				case NETROM_NODE:
					ret = nr_add_node(&nr_route.callsign,
						nr_route.mnemonic,
						&nr_route.neighbour,
						nr_call_to_digi(nr_route.ndigis, nr_route.digipeaters),
						dev, nr_route.quality,
						nr_route.obs_count);
					break;
				case NETROM_NEIGH:
					ret = nr_add_neigh(&nr_route.callsign,
						nr_call_to_digi(nr_route.ndigis, nr_route.digipeaters),
						dev, nr_route.quality);
					break;
				default:
					ret = -EINVAL;
					break;
			}
			dev_put(dev);
			return ret;

		case SIOCDELRT:
			if (copy_from_user(&nr_route, arg, sizeof(struct nr_route_struct)))
				return -EFAULT;
			if ((dev = nr_ax25_dev_get(nr_route.device)) == NULL)
				return -EINVAL;
			switch (nr_route.type) {
				case NETROM_NODE:
					ret = nr_del_node(&nr_route.callsign,
						&nr_route.neighbour, dev);
					break;
				case NETROM_NEIGH:
					ret = nr_del_neigh(&nr_route.callsign,
						dev, nr_route.quality);
					break;
				default:
					ret = -EINVAL;
					break;
			}
			dev_put(dev);
			return ret;

		case SIOCNRDECOBS:
			return nr_dec_obs();

		default:
			return -EINVAL;
	}

	return 0;
}

/*
 * 	A level 2 link has timed out, therefore it appears to be a poor link,
 *	then don't use that neighbour until it is reset.
 */
void nr_link_failed(ax25_cb *ax25, int reason)
{
	struct nr_neigh *nr_neigh;
	struct nr_node  *nr_node;

	for (nr_neigh = nr_neigh_list; nr_neigh != NULL; nr_neigh = nr_neigh->next)
		if (nr_neigh->ax25 == ax25)
			break;

	if (nr_neigh == NULL) return;

	nr_neigh->ax25 = NULL;

	if (++nr_neigh->failed < sysctl_netrom_link_fails_count) return;

	for (nr_node = nr_node_list; nr_node != NULL; nr_node = nr_node->next)
		if (nr_node->which < nr_node->count && nr_node->routes[nr_node->which].neighbour == nr_neigh)
			nr_node->which++;
}

/*
 *	Route a frame to an appropriate AX.25 connection. A NULL ax25_cb
 *	indicates an internally generated frame.
 */
int nr_route_frame(struct sk_buff *skb, ax25_cb *ax25)
{
	ax25_address *nr_src, *nr_dest;
	struct nr_neigh *nr_neigh;
	struct nr_node  *nr_node;
	struct net_device *dev;
	unsigned char *dptr;


	nr_src  = (ax25_address *)(skb->data + 0);
	nr_dest = (ax25_address *)(skb->data + 7);

	if (ax25 != NULL)
		nr_add_node(nr_src, "", &ax25->dest_addr, ax25->digipeat,
			    ax25->ax25_dev->dev, 0, sysctl_netrom_obsolescence_count_initialiser);

	if ((dev = nr_dev_get(nr_dest)) != NULL) {	/* Its for me */
		int ret;

		if (ax25 == NULL)			/* Its from me */
			ret = nr_loopback_queue(skb);
		else
			ret = nr_rx_frame(skb, dev);

		dev_put(dev);
		return ret;
	}

	if (!sysctl_netrom_routing_control && ax25 != NULL)
		return 0;

	/* Its Time-To-Live has expired */
	if (--skb->data[14] == 0)
		return 0;

	for (nr_node = nr_node_list; nr_node != NULL; nr_node = nr_node->next)
		if (ax25cmp(nr_dest, &nr_node->callsign) == 0)
			break;

	if (nr_node == NULL || nr_node->which >= nr_node->count)
		return 0;

	nr_neigh = nr_node->routes[nr_node->which].neighbour;

	if ((dev = nr_dev_first()) == NULL)
		return 0;

	dptr  = skb_push(skb, 1);
	*dptr = AX25_P_NETROM;

	nr_neigh->ax25 = ax25_send_frame(skb, 256, (ax25_address *)dev->dev_addr, &nr_neigh->callsign, nr_neigh->digipeat, nr_neigh->dev);

	dev_put(dev);

	return (nr_neigh->ax25 != NULL);
}

int nr_nodes_get_info(char *buffer, char **start, off_t offset, int length)
{
	struct nr_node *nr_node;
	int len     = 0;
	off_t pos   = 0;
	off_t begin = 0;
	int i;

	cli();

	len += sprintf(buffer, "callsign  mnemonic w n qual obs neigh qual obs neigh qual obs neigh\n");

	for (nr_node = nr_node_list; nr_node != NULL; nr_node = nr_node->next) {
		len += sprintf(buffer + len, "%-9s %-7s  %d %d",
			ax2asc(&nr_node->callsign),
			(nr_node->mnemonic[0] == '\0') ? "*" : nr_node->mnemonic,
			nr_node->which + 1,
			nr_node->count);			

		for (i = 0; i < nr_node->count; i++) {
			len += sprintf(buffer + len, "  %3d   %d %05d",
				nr_node->routes[i].quality,
				nr_node->routes[i].obs_count,
				nr_node->routes[i].neighbour->number);
		}

		len += sprintf(buffer + len, "\n");

		pos = begin + len;

		if (pos < offset) {
			len   = 0;
			begin = pos;
		}

		if (pos > offset + length)
			break;
	}

	sti();

	*start = buffer + (offset - begin);
	len   -= (offset - begin);

	if (len > length) len = length;

	return len;
} 

int nr_neigh_get_info(char *buffer, char **start, off_t offset, int length)
{
	struct nr_neigh *nr_neigh;
	int len     = 0;
	off_t pos   = 0;
	off_t begin = 0;
	int i;

	cli();

	len += sprintf(buffer, "addr  callsign  dev  qual lock count failed digipeaters\n");

	for (nr_neigh = nr_neigh_list; nr_neigh != NULL; nr_neigh = nr_neigh->next) {
		len += sprintf(buffer + len, "%05d %-9s %-4s  %3d    %d   %3d    %3d",
			nr_neigh->number,
			ax2asc(&nr_neigh->callsign),
			nr_neigh->dev ? nr_neigh->dev->name : "???",
			nr_neigh->quality,
			nr_neigh->locked,
			nr_neigh->count,
			nr_neigh->failed);

		if (nr_neigh->digipeat != NULL) {
			for (i = 0; i < nr_neigh->digipeat->ndigi; i++)
				len += sprintf(buffer + len, " %s", ax2asc(&nr_neigh->digipeat->calls[i]));
		}

		len += sprintf(buffer + len, "\n");

		pos = begin + len;

		if (pos < offset) {
			len   = 0;
			begin = pos;
		}

		if (pos > offset + length)
			break;
	}

	sti();

	*start = buffer + (offset - begin);
	len   -= (offset - begin);

	if (len > length) len = length;

	return len;
} 

/*
 *	Free all memory associated with the nodes and routes lists.
 */
void __exit nr_rt_free(void)
{
	struct nr_neigh *s, *nr_neigh = nr_neigh_list;
	struct nr_node  *t, *nr_node  = nr_node_list;

	while (nr_node != NULL) {
		t       = nr_node;
		nr_node = nr_node->next;

		nr_remove_node(t);
	}

	while (nr_neigh != NULL) {
		s        = nr_neigh;
		nr_neigh = nr_neigh->next;

		nr_remove_neigh(s);
	}
}
