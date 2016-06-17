/*
 *	Anycast support for IPv6
 *	Linux INET6 implementation 
 *
 *	Authors:
 *	David L Stevens (dlstevens@us.ibm.com)
 *
 *	based heavily on net/ipv6/mcast.c
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/random.h>
#include <linux/string.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/sched.h>
#include <linux/net.h>
#include <linux/in6.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/route.h>
#include <linux/init.h>
#include <linux/proc_fs.h>

#include <net/sock.h>
#include <net/snmp.h>

#include <net/ipv6.h>
#include <net/protocol.h>
#include <net/if_inet6.h>
#include <net/ndisc.h>
#include <net/addrconf.h>
#include <net/ip6_route.h>

#include <net/checksum.h>

/* Big ac list lock for all the sockets */
static rwlock_t ipv6_sk_ac_lock = RW_LOCK_UNLOCKED;

/* XXX ip6_addr_match() and ip6_onlink() really belong in net/core.c */

static int
ip6_addr_match(struct in6_addr *addr1, struct in6_addr *addr2, int prefix)
{
	__u32	mask;
	int	i;

	if (prefix > 128 || prefix < 0)
		return 0;
	if (prefix == 0)
		return 1;
	for (i=0; i<4; ++i) {
		if (prefix >= 32)
			mask = ~0;
		else
			mask = htonl(~0 << (32 - prefix));
		if ((addr1->s6_addr32[i] ^ addr2->s6_addr32[i]) & mask)
			return 0;
		prefix -= 32;
		if (prefix <= 0)
			break;
	}
	return 1;
}

static int
ip6_onlink(struct in6_addr *addr, struct net_device *dev)
{
	struct inet6_dev	*idev;
	struct inet6_ifaddr	*ifa;
	int	onlink;

	onlink = 0;
	read_lock(&addrconf_lock);
	idev = __in6_dev_get(dev);
	if (idev) {
		read_lock_bh(&idev->lock);
		for (ifa=idev->addr_list; ifa; ifa=ifa->if_next) {
			onlink = ip6_addr_match(addr, &ifa->addr,
					ifa->prefix_len);
			if (onlink)
				break;
		}
		read_unlock_bh(&idev->lock);
	}
	read_unlock(&addrconf_lock);
	return onlink;
}


/*
 *	socket join an anycast group
 */

int ipv6_sock_ac_join(struct sock *sk, int ifindex, struct in6_addr *addr)
{
	struct ipv6_pinfo *np = &sk->net_pinfo.af_inet6;
	struct net_device *dev = NULL;
	struct inet6_dev *idev;
	struct ipv6_ac_socklist *pac;
	int	ishost = !ipv6_devconf.forwarding;
	int	err = 0;

	if (ipv6_addr_type(addr) & IPV6_ADDR_MULTICAST)
		return -EINVAL;

	pac = sock_kmalloc(sk, sizeof(struct ipv6_ac_socklist), GFP_KERNEL);
	if (pac == NULL)
		return -ENOMEM;
	pac->acl_next = NULL;
	ipv6_addr_copy(&pac->acl_addr, addr);

	if (ifindex == 0) {
		struct rt6_info *rt;

		rt = rt6_lookup(addr, NULL, 0, 0);
		if (rt) {
			dev = rt->rt6i_dev;
			dev_hold(dev);
			dst_release(&rt->u.dst);
		} else if (ishost) {
			err = -EADDRNOTAVAIL;
			goto out_free_pac;
		} else {
			/* router, no matching interface: just pick one */

			dev = dev_get_by_flags(IFF_UP, IFF_UP|IFF_LOOPBACK);
		}
	} else
		dev = dev_get_by_index(ifindex);

	if (dev == NULL) {
		err = -ENODEV;
		goto out_free_pac;
	}

	idev = in6_dev_get(dev);
	if (!idev) {
		if (ifindex)
			err = -ENODEV;
		else
			err = -EADDRNOTAVAIL;
		goto out_dev_put;
	}
	/* reset ishost, now that we have a specific device */
	ishost = !idev->cnf.forwarding;
	in6_dev_put(idev);

	pac->acl_ifindex = dev->ifindex;

	/* XXX
	 * For hosts, allow link-local or matching prefix anycasts.
	 * This obviates the need for propagating anycast routes while
	 * still allowing some non-router anycast participation.
	 *
	 * allow anyone to join anycasts that don't require a special route
	 * and can't be spoofs of unicast addresses (reserved anycast only)
	 */
	if (!ip6_onlink(addr, dev)) {
		if (ishost)
			err = -EADDRNOTAVAIL;
		else if (!capable(CAP_NET_ADMIN))
			err = -EPERM;
		if (err)
			goto out_dev_put;
	} else if (!(ipv6_addr_type(addr) & IPV6_ADDR_ANYCAST) &&
		   !capable(CAP_NET_ADMIN)) {
		err = -EPERM;
		goto out_dev_put;
	}

	err = ipv6_dev_ac_inc(dev, addr);
	if (err)
		goto out_dev_put;

	write_lock_bh(&ipv6_sk_ac_lock);
	pac->acl_next = np->ipv6_ac_list;
	np->ipv6_ac_list = pac;
	write_unlock_bh(&ipv6_sk_ac_lock);

	dev_put(dev);

	return 0;

out_dev_put:
	dev_put(dev);
out_free_pac:
	sock_kfree_s(sk, pac, sizeof(*pac));
	return err;
}

/*
 *	socket leave an anycast group
 */
int ipv6_sock_ac_drop(struct sock *sk, int ifindex, struct in6_addr *addr)
{
	struct ipv6_pinfo *np = &sk->net_pinfo.af_inet6;
	struct net_device *dev;
	struct ipv6_ac_socklist *pac, *prev_pac;

	write_lock_bh(&ipv6_sk_ac_lock);
	prev_pac = 0;
	for (pac = np->ipv6_ac_list; pac; pac = pac->acl_next) {
		if ((ifindex == 0 || pac->acl_ifindex == ifindex) &&
		     ipv6_addr_cmp(&pac->acl_addr, addr) == 0)
			break;
		prev_pac = pac;
	}
	if (!pac) {
		write_unlock_bh(&ipv6_sk_ac_lock);
		return -ENOENT;
	}
	if (prev_pac)
		prev_pac->acl_next = pac->acl_next;
	else
		np->ipv6_ac_list = pac->acl_next;

	write_unlock_bh(&ipv6_sk_ac_lock);

	dev = dev_get_by_index(pac->acl_ifindex);
	if (dev) {
		ipv6_dev_ac_dec(dev, &pac->acl_addr);
		dev_put(dev);
	}
	sock_kfree_s(sk, pac, sizeof(*pac));
	return 0;
}

void ipv6_sock_ac_close(struct sock *sk)
{
	struct ipv6_pinfo *np = &sk->net_pinfo.af_inet6;
	struct net_device *dev = 0;
	struct ipv6_ac_socklist *pac;
	int	prev_index;

	write_lock_bh(&ipv6_sk_ac_lock);
	pac = np->ipv6_ac_list;
	np->ipv6_ac_list = 0;
	write_unlock_bh(&ipv6_sk_ac_lock);

	prev_index = 0;
	while (pac) {
		struct ipv6_ac_socklist *next = pac->acl_next;

		if (pac->acl_ifindex != prev_index) {
			if (dev)
				dev_put(dev);
			dev = dev_get_by_index(pac->acl_ifindex);
			prev_index = pac->acl_ifindex;
		}
		if (dev)
			ipv6_dev_ac_dec(dev, &pac->acl_addr);
		sock_kfree_s(sk, pac, sizeof(*pac));
		pac = next;
	}
	if (dev)
		dev_put(dev);
}

int inet6_ac_check(struct sock *sk, struct in6_addr *addr, int ifindex)
{
	struct ipv6_ac_socklist *pac;
	struct ipv6_pinfo *np = &sk->net_pinfo.af_inet6;
	int	found;

	found = 0;
	read_lock(&ipv6_sk_ac_lock);
	for (pac=np->ipv6_ac_list; pac; pac=pac->acl_next) {
		if (ifindex && pac->acl_ifindex != ifindex)
			continue;
		found = ipv6_addr_cmp(&pac->acl_addr, addr) == 0;
		if (found)
			break;
	}
	read_unlock(&ipv6_sk_ac_lock);

	return found;
}

static void aca_put(struct ifacaddr6 *ac)
{
	if (atomic_dec_and_test(&ac->aca_refcnt)) {
		in6_dev_put(ac->aca_idev);
		kfree(ac);
	}
}

/*
 *	device anycast group inc (add if not found)
 */
int ipv6_dev_ac_inc(struct net_device *dev, struct in6_addr *addr)
{
	struct ifacaddr6 *aca;
	struct inet6_dev *idev;

	idev = in6_dev_get(dev);

	if (idev == NULL)
		return -EINVAL;

	write_lock_bh(&idev->lock);
	if (idev->dead) {
		write_unlock_bh(&idev->lock);
		in6_dev_put(idev);
		return -ENODEV;
	}

	for (aca = idev->ac_list; aca; aca = aca->aca_next) {
		if (ipv6_addr_cmp(&aca->aca_addr, addr) == 0) {
			aca->aca_users++;
			write_unlock_bh(&idev->lock);
			in6_dev_put(idev);
			return 0;
		}
	}

	/*
	 *	not found: create a new one.
	 */

	aca = kmalloc(sizeof(struct ifacaddr6), GFP_ATOMIC);

	if (aca == NULL) {
		write_unlock_bh(&idev->lock);
		in6_dev_put(idev);
		return -ENOMEM;
	}

	memset(aca, 0, sizeof(struct ifacaddr6));

	ipv6_addr_copy(&aca->aca_addr, addr);
	aca->aca_idev = idev;
	aca->aca_users = 1;
	atomic_set(&aca->aca_refcnt, 2);
	aca->aca_lock = SPIN_LOCK_UNLOCKED;

	aca->aca_next = idev->ac_list;
	idev->ac_list = aca;
	write_unlock_bh(&idev->lock);

	ip6_rt_addr_add(&aca->aca_addr, dev);

	addrconf_join_solict(dev, &aca->aca_addr);

	aca_put(aca);
	return 0;
}

/*
 *	device anycast group decrement
 */
int ipv6_dev_ac_dec(struct net_device *dev, struct in6_addr *addr)
{
	struct inet6_dev *idev;
	struct ifacaddr6 *aca, *prev_aca;

	idev = in6_dev_get(dev);
	if (idev == NULL)
		return -ENODEV;

	write_lock_bh(&idev->lock);
	prev_aca = 0;
	for (aca = idev->ac_list; aca; aca = aca->aca_next) {
		if (ipv6_addr_cmp(&aca->aca_addr, addr) == 0)
			break;
		prev_aca = aca;
	}
	if (!aca) {
		write_unlock_bh(&idev->lock);
		in6_dev_put(idev);
		return -ENOENT;
	}
	if (--aca->aca_users > 0) {
		write_unlock_bh(&idev->lock);
		in6_dev_put(idev);
		return 0;
	}
	if (prev_aca)
		prev_aca->aca_next = aca->aca_next;
	else
		idev->ac_list = aca->aca_next;
	write_unlock_bh(&idev->lock);
	addrconf_leave_solict(dev, &aca->aca_addr);

	ip6_rt_addr_del(&aca->aca_addr, dev);

	aca_put(aca);
	in6_dev_put(idev);
	return 0;
}

/*
 *	check if the interface has this anycast address
 */
static int ipv6_chk_acast_dev(struct net_device *dev, struct in6_addr *addr)
{
	struct inet6_dev *idev;
	struct ifacaddr6 *aca;

	idev = in6_dev_get(dev);
	if (idev) {
		read_lock_bh(&idev->lock);
		for (aca = idev->ac_list; aca; aca = aca->aca_next)
			if (ipv6_addr_cmp(&aca->aca_addr, addr) == 0)
				break;
		read_unlock_bh(&idev->lock);
		in6_dev_put(idev);
		return aca != 0;
	}
	return 0;
}

/*
 *	check if given interface (or any, if dev==0) has this anycast address
 */
int ipv6_chk_acast_addr(struct net_device *dev, struct in6_addr *addr)
{
	if (dev)
		return ipv6_chk_acast_dev(dev, addr);
	read_lock(&dev_base_lock);
	for (dev=dev_base; dev; dev=dev->next)
		if (ipv6_chk_acast_dev(dev, addr))
			break;
	read_unlock(&dev_base_lock);
	return dev != 0;
}


#ifdef CONFIG_PROC_FS
int anycast6_get_info(char *buffer, char **start, off_t offset, int length)
{
	off_t pos=0, begin=0;
	struct ifacaddr6 *im;
	int len=0;
	struct net_device *dev;
	
	read_lock(&dev_base_lock);
	for (dev = dev_base; dev; dev = dev->next) {
		struct inet6_dev *idev;

		if ((idev = in6_dev_get(dev)) == NULL)
			continue;

		read_lock_bh(&idev->lock);
		for (im = idev->ac_list; im; im = im->aca_next) {
			int i;

			len += sprintf(buffer+len,"%-4d %-15s ", dev->ifindex, dev->name);

			for (i=0; i<16; i++)
				len += sprintf(buffer+len, "%02x", im->aca_addr.s6_addr[i]);

			len += sprintf(buffer+len, " %5d\n", im->aca_users);

			pos=begin+len;
			if (pos < offset) {
				len=0;
				begin=pos;
			}
			if (pos > offset+length) {
				read_unlock_bh(&idev->lock);
				in6_dev_put(idev);
				goto done;
			}
		}
		read_unlock_bh(&idev->lock);
		in6_dev_put(idev);
	}

done:
	read_unlock(&dev_base_lock);

	*start=buffer+(offset-begin);
	len-=(offset-begin);
	if(len>length)
		len=length;
	if (len<0)
		len=0;
	return len;
}

#endif
