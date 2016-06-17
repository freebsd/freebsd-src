/*
 *	AX.25 release 037
 *
 *	This code REQUIRES 2.1.15 or higher/ NET3.038
 *
 *	This module:
 *		This module is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *	Other kernels modules in this kit are generally BSD derived. See the copyright headers.
 *
 *
 *	History
 *	AX.25 020	Jonathan(G4KLX)	First go.
 *	AX.25 022	Jonathan(G4KLX)	Added the actual meat to this - we now have a nice heard list.
 *	AX.25 025	Alan(GW4PTS)	First cut at autobinding by route scan.
 *	AX.25 028b	Jonathan(G4KLX)	Extracted AX25 control block from the
 *					sock structure. Device removal now
 *					removes the heard structure.
 *	AX.25 029	Steven(GW7RRM)	Added /proc information for uid/callsign mapping.
 *			Jonathan(G4KLX)	Handling of IP mode in the routing list and /proc entry.
 *	AX.25 030	Jonathan(G4KLX)	Added digi-peaters to routing table, and
 *					ioctls to manipulate them. Added port
 *					configuration.
 *	AX.25 031	Jonathan(G4KLX)	Added concept of default route.
 *			Joerg(DL1BKE)	ax25_rt_build_path() find digipeater list and device by 
 *					destination call. Needed for IP routing via digipeater
 *			Jonathan(G4KLX)	Added routing for IP datagram packets.
 *			Joerg(DL1BKE)	Changed routing for IP datagram and VC to use a default
 *					route if available. Does not overwrite default routes
 *					on route-table overflow anymore.
 *			Joerg(DL1BKE)	Fixed AX.25 routing of IP datagram and VC, new ioctl()
 *					"SIOCAX25OPTRT" to set IP mode and a 'permanent' flag
 *					on routes.
 *	AX.25 033	Jonathan(G4KLX)	Remove auto-router.
 *			Joerg(DL1BKE)	Moved BPQ Ethernet driver to separate device.
 *	AX.25 035	Frederic(F1OAT)	Support for pseudo-digipeating.
 *			Jonathan(G4KLX)	Support for packet forwarding.
 *			Arnaldo C. Melo s/suser/capable/
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
#include <linux/if_arp.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/init.h>

static ax25_route *ax25_route_list;

static ax25_route *ax25_find_route(ax25_address *, struct net_device *);

/*
 * small macro to drop non-digipeated digipeaters and reverse path
 */
static inline void ax25_route_invert(ax25_digi *in, ax25_digi *out)
{
	int k;

	for (k = 0; k < in->ndigi; k++)
		if (!in->repeated[k])
			break;

	in->ndigi = k;

	ax25_digi_invert(in, out);
}

void ax25_rt_device_down(struct net_device *dev)
{
	ax25_route *s, *t, *ax25_rt = ax25_route_list;
	
	while (ax25_rt != NULL) {
		s       = ax25_rt;
		ax25_rt = ax25_rt->next;

		if (s->dev == dev) {
			if (ax25_route_list == s) {
				ax25_route_list = s->next;
				if (s->digipeat != NULL)
					kfree(s->digipeat);
				kfree(s);
			} else {
				for (t = ax25_route_list; t != NULL; t = t->next) {
					if (t->next == s) {
						t->next = s->next;
						if (s->digipeat != NULL)
							kfree(s->digipeat);
						kfree(s);
						break;
					}
				}
			}
		}
	}
}

int ax25_rt_ioctl(unsigned int cmd, void *arg)
{
	unsigned long flags;
	ax25_route *s, *t, *ax25_rt;
	struct ax25_routes_struct route;
	struct ax25_route_opt_struct rt_option;
	ax25_dev *ax25_dev;
	int i;

	switch (cmd) {
		case SIOCADDRT:
			if (copy_from_user(&route, arg, sizeof(route)))
				return -EFAULT;
			if ((ax25_dev = ax25_addr_ax25dev(&route.port_addr)) == NULL)
				return -EINVAL;
			if (route.digi_count > AX25_MAX_DIGIS)
				return -EINVAL;
			for (ax25_rt = ax25_route_list; ax25_rt != NULL; ax25_rt = ax25_rt->next) {
				if (ax25cmp(&ax25_rt->callsign, &route.dest_addr) == 0 && ax25_rt->dev == ax25_dev->dev) {
					if (ax25_rt->digipeat != NULL) {
						kfree(ax25_rt->digipeat);
						ax25_rt->digipeat = NULL;
					}
					if (route.digi_count != 0) {
						if ((ax25_rt->digipeat = kmalloc(sizeof(ax25_digi), GFP_ATOMIC)) == NULL)
							return -ENOMEM;
						ax25_rt->digipeat->lastrepeat = -1;
						ax25_rt->digipeat->ndigi      = route.digi_count;
						for (i = 0; i < route.digi_count; i++) {
							ax25_rt->digipeat->repeated[i] = 0;
							ax25_rt->digipeat->calls[i]    = route.digi_addr[i];
						}
					}
					return 0;
				}
			}
			if ((ax25_rt = kmalloc(sizeof(ax25_route), GFP_ATOMIC)) == NULL)
				return -ENOMEM;
			ax25_rt->callsign     = route.dest_addr;
			ax25_rt->dev          = ax25_dev->dev;
			ax25_rt->digipeat     = NULL;
			ax25_rt->ip_mode      = ' ';
			if (route.digi_count != 0) {
				if ((ax25_rt->digipeat = kmalloc(sizeof(ax25_digi), GFP_ATOMIC)) == NULL) {
					kfree(ax25_rt);
					return -ENOMEM;
				}
				ax25_rt->digipeat->lastrepeat = -1;
				ax25_rt->digipeat->ndigi      = route.digi_count;
				for (i = 0; i < route.digi_count; i++) {
					ax25_rt->digipeat->repeated[i] = 0;
					ax25_rt->digipeat->calls[i]    = route.digi_addr[i];
				}
			}
			save_flags(flags); cli();
			ax25_rt->next   = ax25_route_list;
			ax25_route_list = ax25_rt;
			restore_flags(flags);
			break;

		case SIOCDELRT:
			if (copy_from_user(&route, arg, sizeof(route)))
				return -EFAULT;
			if ((ax25_dev = ax25_addr_ax25dev(&route.port_addr)) == NULL)
				return -EINVAL;
			ax25_rt = ax25_route_list;
			while (ax25_rt != NULL) {
				s       = ax25_rt;
				ax25_rt = ax25_rt->next;
				if (s->dev == ax25_dev->dev && ax25cmp(&route.dest_addr, &s->callsign) == 0) {
					if (ax25_route_list == s) {
						ax25_route_list = s->next;
						if (s->digipeat != NULL)
							kfree(s->digipeat);
						kfree(s);
					} else {
						for (t = ax25_route_list; t != NULL; t = t->next) {
							if (t->next == s) {
								t->next = s->next;
								if (s->digipeat != NULL)
									kfree(s->digipeat);
								kfree(s);
								break;
							}
						}				
					}
				}
			}
			break;

		case SIOCAX25OPTRT:
			if (copy_from_user(&rt_option, arg, sizeof(rt_option)))
				return -EFAULT;
			if ((ax25_dev = ax25_addr_ax25dev(&rt_option.port_addr)) == NULL)
				return -EINVAL;
			for (ax25_rt = ax25_route_list; ax25_rt != NULL; ax25_rt = ax25_rt->next) {
				if (ax25_rt->dev == ax25_dev->dev && ax25cmp(&rt_option.dest_addr, &ax25_rt->callsign) == 0) {
					switch (rt_option.cmd) {
						case AX25_SET_RT_IPMODE:
							switch (rt_option.arg) {
								case ' ':
								case 'D':
								case 'V':
									ax25_rt->ip_mode = rt_option.arg;
									break;
								default:
									return -EINVAL;
							}
							break;
						default:
							return -EINVAL;
					}
				}
			}
			break;

		default:
			return -EINVAL;
	}

	return 0;
}

int ax25_rt_get_info(char *buffer, char **start, off_t offset, int length)
{
	ax25_route *ax25_rt;
	int len     = 0;
	off_t pos   = 0;
	off_t begin = 0;
	char *callsign;
	int i;
  
	cli();

	len += sprintf(buffer, "callsign  dev  mode digipeaters\n");

	for (ax25_rt = ax25_route_list; ax25_rt != NULL; ax25_rt = ax25_rt->next) {
		if (ax25cmp(&ax25_rt->callsign, &null_ax25_address) == 0)
			callsign = "default";
		else
			callsign = ax2asc(&ax25_rt->callsign);
		len += sprintf(buffer + len, "%-9s %-4s",
			callsign,
			ax25_rt->dev ? ax25_rt->dev->name : "???");

		switch (ax25_rt->ip_mode) {
			case 'V':
				len += sprintf(buffer + len, "   vc");
				break;
			case 'D':
				len += sprintf(buffer + len, "   dg");
				break;
			default:
				len += sprintf(buffer + len, "    *");
				break;
		}

		if (ax25_rt->digipeat != NULL)
			for (i = 0; i < ax25_rt->digipeat->ndigi; i++)
				len += sprintf(buffer + len, " %s", ax2asc(&ax25_rt->digipeat->calls[i]));

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
 *	Find AX.25 route
 */
static ax25_route *ax25_find_route(ax25_address *addr, struct net_device *dev)
{
	ax25_route *ax25_spe_rt = NULL;
	ax25_route *ax25_def_rt = NULL;
	ax25_route *ax25_rt;

	/*
	 *	Bind to the physical interface we heard them on, or the default
	 *	route if none is found;
	 */
	for (ax25_rt = ax25_route_list; ax25_rt != NULL; ax25_rt = ax25_rt->next) {
		if (dev == NULL) {
			if (ax25cmp(&ax25_rt->callsign, addr) == 0 && ax25_rt->dev != NULL)
				ax25_spe_rt = ax25_rt;
			if (ax25cmp(&ax25_rt->callsign, &null_ax25_address) == 0 && ax25_rt->dev != NULL)
				ax25_def_rt = ax25_rt;
		} else {
			if (ax25cmp(&ax25_rt->callsign, addr) == 0 && ax25_rt->dev == dev)
				ax25_spe_rt = ax25_rt;
			if (ax25cmp(&ax25_rt->callsign, &null_ax25_address) == 0 && ax25_rt->dev == dev)
				ax25_def_rt = ax25_rt;
		}
	}

	if (ax25_spe_rt != NULL)
		return ax25_spe_rt;

	return ax25_def_rt;
}

/*
 *	Adjust path: If you specify a default route and want to connect
 *      a target on the digipeater path but w/o having a special route
 *	set before, the path has to be truncated from your target on.
 */
static inline void ax25_adjust_path(ax25_address *addr, ax25_digi *digipeat)
{
	int k;

	for (k = 0; k < digipeat->ndigi; k++) {
		if (ax25cmp(addr, &digipeat->calls[k]) == 0)
			break;
	}

	digipeat->ndigi = k;
}
 

/*
 *	Find which interface to use.
 */
int ax25_rt_autobind(ax25_cb *ax25, ax25_address *addr)
{
	ax25_route *ax25_rt;
	ax25_address *call;

	if ((ax25_rt = ax25_find_route(addr, NULL)) == NULL)
		return -EHOSTUNREACH;

	if ((ax25->ax25_dev = ax25_dev_ax25dev(ax25_rt->dev)) == NULL)
		return -EHOSTUNREACH;

	if ((call = ax25_findbyuid(current->euid)) == NULL) {
		if (ax25_uid_policy && !capable(CAP_NET_BIND_SERVICE))
			return -EPERM;
		call = (ax25_address *)ax25->ax25_dev->dev->dev_addr;
	}

	ax25->source_addr = *call;

	if (ax25_rt->digipeat != NULL) {
		if ((ax25->digipeat = kmalloc(sizeof(ax25_digi), GFP_ATOMIC)) == NULL)
			return -ENOMEM;
		memcpy(ax25->digipeat, ax25_rt->digipeat, sizeof(ax25_digi));
		ax25_adjust_path(addr, ax25->digipeat);
	}

	if (ax25->sk != NULL)
		ax25->sk->zapped = 0;

	return 0;
}

/*
 *	dl1bke 960117: build digipeater path
 *	dl1bke 960301: use the default route if it exists
 */
ax25_route *ax25_rt_find_route(ax25_address *addr, struct net_device *dev)
{
	static ax25_route route;
	ax25_route *ax25_rt;

	if ((ax25_rt = ax25_find_route(addr, dev)) == NULL) {
		route.next     = NULL;
		route.callsign = *addr;
		route.dev      = dev;
		route.digipeat = NULL;
		route.ip_mode  = ' ';
		return &route;
	}

	return ax25_rt;
}

struct sk_buff *ax25_rt_build_path(struct sk_buff *skb, ax25_address *src, ax25_address *dest, ax25_digi *digi)
{
	struct sk_buff *skbn;
	unsigned char *bp;
	int len;

	len = digi->ndigi * AX25_ADDR_LEN;

	if (skb_headroom(skb) < len) {
		if ((skbn = skb_realloc_headroom(skb, len)) == NULL) {
			printk(KERN_CRIT "AX.25: ax25_dg_build_path - out of memory\n");
			return NULL;
		}

		if (skb->sk != NULL)
			skb_set_owner_w(skbn, skb->sk);

		kfree_skb(skb);

		skb = skbn;
	}

	bp = skb_push(skb, len);

	ax25_addr_build(bp, src, dest, digi, AX25_COMMAND, AX25_MODULUS);

	return skb;
}

/*
 *	Free all memory associated with routing structures.
 */
void __exit ax25_rt_free(void)
{
	ax25_route *s, *ax25_rt = ax25_route_list;

	while (ax25_rt != NULL) {
		s       = ax25_rt;
		ax25_rt = ax25_rt->next;

		if (s->digipeat != NULL)
			kfree(s->digipeat);

		kfree(s);
	}
}
