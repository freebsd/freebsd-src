/*
 *	Multicast support for IPv6
 *	Linux INET6 implementation 
 *
 *	Authors:
 *	Pedro Roque		<roque@di.fc.ul.pt>	
 *
 *	$Id: mcast.c,v 1.38 2001/08/15 07:36:31 davem Exp $
 *
 *	Based on linux/ipv4/igmp.c and linux/ipv4/ip_sockglue.c 
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

/* Changes:
 *
 *	yoshfuji	: fix format of router-alert option
 *	YOSHIFUJI Hideaki @USAGI:
 *		Fixed source address for MLD message based on
 *		<draft-ietf-magma-mld-source-02.txt>.
 *	YOSHIFUJI Hideaki @USAGI:
 *		- Ignore Queries for invalid addresses.
 *		- MLD for link-local addresses.
 *	David L Stevens <dlstevens@us.ibm.com>:
		- MLDv2 support
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/sched.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/route.h>
#include <linux/init.h>
#include <linux/proc_fs.h>

#include <linux/netfilter.h>
#include <linux/netfilter_ipv6.h>

#include <net/sock.h>
#include <net/snmp.h>

#include <net/ipv6.h>
#include <net/protocol.h>
#include <net/if_inet6.h>
#include <net/ndisc.h>
#include <net/addrconf.h>
#include <net/ip6_route.h>

#include <net/checksum.h>

/* Set to 3 to get tracing... */
#define MCAST_DEBUG 2

#if MCAST_DEBUG >= 3
#define MDBG(x) printk x
#else
#define MDBG(x)
#endif

/*
 *  These header formats should be in a separate include file, but icmpv6.h
 *  doesn't have in6_addr defined in all cases, there is no __u128, and no
 *  other files reference these.
 *
 *  			+-DLS 4/14/03
 */

/* Multicast Listener Discovery version 2 headers */

struct mld2_grec {
	__u8		grec_type;
	__u8		grec_auxwords;
	__u16		grec_nsrcs;
	struct in6_addr	grec_mca;
	struct in6_addr	grec_src[0];
};

struct mld2_report {
	__u8	type;
	__u8	resv1;
	__u16	csum;
	__u16	resv2;
	__u16	ngrec;
	struct mld2_grec grec[0];
};

struct mld2_query {
	__u8 type;
	__u8 code;
	__u16 csum;
	__u16 mrc;
	__u16 resv1;
	struct in6_addr mca;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8 qrv:3,
	     suppress:1,
	     resv2:4;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8 resv2:4,
	     suppress:1,
	     qrv:3;
#else
#error "Please fix <asm/byteorder.h>"
#endif
	__u8 qqic;
	__u16 nsrcs;
	struct in6_addr srcs[0];
};

struct in6_addr mld2_all_mcr = MLD2_ALL_MCR_INIT;
struct in6_addr all_nodes_addr = {{{0xff,0x02,0,0,0,0,0,0,0,0,0,0,0,0,0,0x1}}};

/* Big mc list lock for all the sockets */
static rwlock_t ipv6_sk_mc_lock = RW_LOCK_UNLOCKED;

static struct socket *igmp6_socket;

static void igmp6_join_group(struct ifmcaddr6 *ma);
static void igmp6_leave_group(struct ifmcaddr6 *ma);
static void igmp6_timer_handler(unsigned long data);

static void mld_gq_timer_expire(unsigned long data);
static void mld_ifc_timer_expire(unsigned long data);
static void mld_ifc_event(struct inet6_dev *idev);
static void mld_add_delrec(struct inet6_dev *idev, struct ifmcaddr6 *pmc);
static void mld_del_delrec(struct inet6_dev *idev, struct in6_addr *addr);
static void mld_clear_delrec(struct inet6_dev *idev);
static int sf_setstate(struct ifmcaddr6 *pmc);
static void sf_markstate(struct ifmcaddr6 *pmc);
static void ip6_mc_clear_src(struct ifmcaddr6 *pmc);
int ip6_mc_del_src(struct inet6_dev *idev, struct in6_addr *pmca, int sfmode,
	int sfcount, struct in6_addr *psfsrc, int delta);
int ip6_mc_add_src(struct inet6_dev *idev, struct in6_addr *pmca, int sfmode,
	int sfcount, struct in6_addr *psfsrc, int delta);
int ip6_mc_leave_src(struct sock *sk, struct ipv6_mc_socklist *iml,
	struct inet6_dev *idev);


#define IGMP6_UNSOLICITED_IVAL	(10*HZ)
#define MLD_QRV_DEFAULT		2

#define MLD_V1_SEEN(idev) ((idev)->mc_v1_seen && \
		time_before(jiffies, (idev)->mc_v1_seen))

#define MLDV2_MASK(value, nb) ((nb)>=32 ? (value) : ((1<<(nb))-1) & (value))
#define MLDV2_EXP(thresh, nbmant, nbexp, value) \
	((value) < (thresh) ? (value) : \
	((MLDV2_MASK(value, nbmant) | (1<<(nbmant+nbexp))) << \
	(MLDV2_MASK((value) >> (nbmant), nbexp) + (nbexp))))

#define MLDV2_QQIC(value) MLDV2_EXP(0x80, 4, 3, value)
#define MLDV2_MRC(value) MLDV2_EXP(0x8000, 12, 3, value)

#define IPV6_MLD_MAX_MSF	10

int sysctl_mld_max_msf = IPV6_MLD_MAX_MSF;

/*
 *	socket join on multicast group
 */

int ipv6_sock_mc_join(struct sock *sk, int ifindex, struct in6_addr *addr)
{
	struct net_device *dev = NULL;
	struct ipv6_mc_socklist *mc_lst;
	struct ipv6_pinfo *np = &sk->net_pinfo.af_inet6;
	int err;

	if (!(ipv6_addr_type(addr) & IPV6_ADDR_MULTICAST))
		return -EINVAL;

	mc_lst = sock_kmalloc(sk, sizeof(struct ipv6_mc_socklist), GFP_KERNEL);

	if (mc_lst == NULL)
		return -ENOMEM;

	mc_lst->next = NULL;
	memcpy(&mc_lst->addr, addr, sizeof(struct in6_addr));

	if (ifindex == 0) {
		struct rt6_info *rt;
		rt = rt6_lookup(addr, NULL, 0, 0);
		if (rt) {
			dev = rt->rt6i_dev;
			dev_hold(dev);
			dst_release(&rt->u.dst);
		}
	} else
		dev = dev_get_by_index(ifindex);

	if (dev == NULL) {
		sock_kfree_s(sk, mc_lst, sizeof(*mc_lst));
		return -ENODEV;
	}

	mc_lst->ifindex = dev->ifindex;
	mc_lst->sfmode = MCAST_EXCLUDE;
	mc_lst->sflist = 0;

	/*
	 *	now add/increase the group membership on the device
	 */

	err = ipv6_dev_mc_inc(dev, addr);

	if (err) {
		sock_kfree_s(sk, mc_lst, sizeof(*mc_lst));
		dev_put(dev);
		return err;
	}

	write_lock_bh(&ipv6_sk_mc_lock);
	mc_lst->next = np->ipv6_mc_list;
	np->ipv6_mc_list = mc_lst;
	write_unlock_bh(&ipv6_sk_mc_lock);

	dev_put(dev);

	return 0;
}

/*
 *	socket leave on multicast group
 */
int ipv6_sock_mc_drop(struct sock *sk, int ifindex, struct in6_addr *addr)
{
	struct ipv6_pinfo *np = &sk->net_pinfo.af_inet6;
	struct ipv6_mc_socklist *mc_lst, **lnk;

	write_lock_bh(&ipv6_sk_mc_lock);
	for (lnk = &np->ipv6_mc_list; (mc_lst = *lnk) !=NULL ; lnk = &mc_lst->next) {
		if (mc_lst->ifindex == ifindex &&
		    ipv6_addr_cmp(&mc_lst->addr, addr) == 0) {
			struct net_device *dev;

			*lnk = mc_lst->next;
			write_unlock_bh(&ipv6_sk_mc_lock);

			if ((dev = dev_get_by_index(ifindex)) != NULL) {
				struct inet6_dev *idev = in6_dev_get(dev);

				if (idev) {
					(void) ip6_mc_leave_src(sk,mc_lst,idev);
					in6_dev_put(idev);
				}
				ipv6_dev_mc_dec(dev, &mc_lst->addr);
				dev_put(dev);
			}
			sock_kfree_s(sk, mc_lst, sizeof(*mc_lst));
			return 0;
		}
	}
	write_unlock_bh(&ipv6_sk_mc_lock);

	return -ENOENT;
}

struct inet6_dev *ip6_mc_find_dev(struct in6_addr *group, int ifindex)
{
	struct net_device *dev = 0;
	struct inet6_dev *idev = 0;

	if (ifindex == 0) {
		struct rt6_info *rt;

		rt = rt6_lookup(group, NULL, 0, 0);
		if (rt) {
			dev = rt->rt6i_dev;
			dev_hold(dev);
			dst_release(&rt->u.dst);
		}
	} else
		dev = dev_get_by_index(ifindex);

	if (!dev)
		return 0;
	idev = in6_dev_get(dev);
	if (!idev) {
		dev_put(dev);
		return 0;
	}
	read_lock_bh(&idev->lock);
	if (idev->dead) {
		read_unlock_bh(&idev->lock);
		in6_dev_put(idev);
		dev_put(dev);
		return 0;
	}
	return idev;
}

void ipv6_sock_mc_close(struct sock *sk)
{
	struct ipv6_pinfo *np = &sk->net_pinfo.af_inet6;
	struct ipv6_mc_socklist *mc_lst;

	write_lock_bh(&ipv6_sk_mc_lock);
	while ((mc_lst = np->ipv6_mc_list) != NULL) {
		struct net_device *dev;

		np->ipv6_mc_list = mc_lst->next;
		write_unlock_bh(&ipv6_sk_mc_lock);

		dev = dev_get_by_index(mc_lst->ifindex);
		if (dev) {
			struct inet6_dev *idev = in6_dev_get(dev);

			if (idev) {
				(void) ip6_mc_leave_src(sk, mc_lst, idev);
				in6_dev_put(idev);
			}
			ipv6_dev_mc_dec(dev, &mc_lst->addr);
			dev_put(dev);
		}

		sock_kfree_s(sk, mc_lst, sizeof(*mc_lst));

		write_lock_bh(&ipv6_sk_mc_lock);
	}
	write_unlock_bh(&ipv6_sk_mc_lock);
}

int ip6_mc_source(int add, int omode, struct sock *sk,
	struct group_source_req *pgsr)
{
	struct in6_addr *source, *group;
	struct ipv6_mc_socklist *pmc;
	struct net_device *dev;
	struct inet6_dev *idev;
	struct ipv6_pinfo *inet6 = &sk->net_pinfo.af_inet6;
	struct ip6_sf_socklist *psl;
	int i, j, rv;
	int err;

	if (pgsr->gsr_group.ss_family != AF_INET6 ||
	    pgsr->gsr_source.ss_family != AF_INET6)
		return -EINVAL;

	source = &((struct sockaddr_in6 *)&pgsr->gsr_source)->sin6_addr;
	group = &((struct sockaddr_in6 *)&pgsr->gsr_group)->sin6_addr;

	if (!(ipv6_addr_type(group) & IPV6_ADDR_MULTICAST))
		return -EINVAL;

	idev = ip6_mc_find_dev(group, pgsr->gsr_interface);
	if (!idev)
		return -ENODEV;
	dev = idev->dev;

	err = -EADDRNOTAVAIL;

	for (pmc=inet6->ipv6_mc_list; pmc; pmc=pmc->next) {
		if (pmc->ifindex != pgsr->gsr_interface)
			continue;
		if (ipv6_addr_cmp(&pmc->addr, group) == 0)
			break;
	}
	if (!pmc)		/* must have a prior join */
		goto done;
	/* if a source filter was set, must be the same mode as before */
	if (pmc->sflist) {
		if (pmc->sfmode != omode)
			goto done;
	} else if (pmc->sfmode != omode) {
		/* allow mode switches for empty-set filters */
		ip6_mc_add_src(idev, group, omode, 0, 0, 0);
		ip6_mc_del_src(idev, group, pmc->sfmode, 0, 0, 0);
		pmc->sfmode = omode;
	}

	psl = pmc->sflist;
	if (!add) {
		if (!psl)
			goto done;
		rv = !0;
		for (i=0; i<psl->sl_count; i++) {
			rv = memcmp(&psl->sl_addr, group,
				sizeof(struct in6_addr));
			if (rv >= 0)
				break;
		}
		if (!rv)	/* source not found */
			goto done;

		/* update the interface filter */
		ip6_mc_del_src(idev, group, omode, 1, source, 1);

		for (j=i+1; j<psl->sl_count; j++)
			psl->sl_addr[j-1] = psl->sl_addr[j];
		psl->sl_count--;
		err = 0;
		goto done;
	}
	/* else, add a new source to the filter */

	if (psl && psl->sl_count >= sysctl_mld_max_msf) {
		err = -ENOBUFS;
		goto done;
	}
	if (!psl || psl->sl_count == psl->sl_max) {
		struct ip6_sf_socklist *newpsl;
		int count = IP6_SFBLOCK;

		if (psl)
			count += psl->sl_max;
		newpsl = (struct ip6_sf_socklist *)sock_kmalloc(sk,
			IP6_SFLSIZE(count), GFP_ATOMIC);
		if (!newpsl) {
			err = -ENOBUFS;
			goto done;
		}
		newpsl->sl_max = count;
		newpsl->sl_count = count - IP6_SFBLOCK;
		if (psl) {
			for (i=0; i<psl->sl_count; i++)
				newpsl->sl_addr[i] = psl->sl_addr[i];
			sock_kfree_s(sk, psl, IP6_SFLSIZE(psl->sl_max));
		}
		pmc->sflist = psl = newpsl;
	}
	rv = 1;	/* > 0 for insert logic below if sl_count is 0 */
	for (i=0; i<psl->sl_count; i++) {
		rv = memcmp(&psl->sl_addr, group, sizeof(struct in6_addr));
		if (rv >= 0)
			break;
	}
	if (rv == 0)		/* address already there is an error */
		goto done;
	for (j=psl->sl_count-1; j>=i; j--)
		psl->sl_addr[j+1] = psl->sl_addr[j];
	psl->sl_addr[i] = *source;
	psl->sl_count++;
	err = 0;
	/* update the interface list */
	ip6_mc_add_src(idev, group, omode, 1, source, 1);
done:
	read_unlock_bh(&idev->lock);
	in6_dev_put(idev);
	dev_put(dev);
	return err;
}

int ip6_mc_msfilter(struct sock *sk, struct group_filter *gsf)
{
	struct in6_addr *group;
	struct ipv6_mc_socklist *pmc;
	struct net_device *dev;
	struct inet6_dev *idev;
	struct ipv6_pinfo *inet6 = &sk->net_pinfo.af_inet6;
	struct ip6_sf_socklist *newpsl, *psl;
	int i, err;

	group = &((struct sockaddr_in6 *)&gsf->gf_group)->sin6_addr;

	if (!(ipv6_addr_type(group) & IPV6_ADDR_MULTICAST))
		return -EINVAL;
	if (gsf->gf_fmode != MCAST_INCLUDE &&
	    gsf->gf_fmode != MCAST_EXCLUDE)
		return -EINVAL;

	idev = ip6_mc_find_dev(group, gsf->gf_interface);

	if (!idev)
		return -ENODEV;
	dev = idev->dev;
	err = -EADDRNOTAVAIL;

	for (pmc=inet6->ipv6_mc_list; pmc; pmc=pmc->next) {
		if (pmc->ifindex != gsf->gf_interface)
			continue;
		if (ipv6_addr_cmp(&pmc->addr, group) == 0)
			break;
	}
	if (!pmc)		/* must have a prior join */
		goto done;
	if (gsf->gf_numsrc) {
		newpsl = (struct ip6_sf_socklist *)sock_kmalloc(sk,
				IP6_SFLSIZE(gsf->gf_numsrc), GFP_ATOMIC);
		if (!newpsl) {
			err = -ENOBUFS;
			goto done;
		}
		newpsl->sl_max = newpsl->sl_count = gsf->gf_numsrc;
		for (i=0; i<newpsl->sl_count; ++i) {
			struct sockaddr_in6 *psin6;

			psin6 = (struct sockaddr_in6 *)&gsf->gf_slist[i];
			newpsl->sl_addr[i] = psin6->sin6_addr;
		}
		err = ip6_mc_add_src(idev, group, gsf->gf_fmode,
			newpsl->sl_count, newpsl->sl_addr, 0);
		if (err) {
			sock_kfree_s(sk, newpsl, IP6_SFLSIZE(newpsl->sl_max));
			goto done;
		}
	} else
		newpsl = 0;
	psl = pmc->sflist;
	if (psl) {
		(void) ip6_mc_del_src(idev, group, pmc->sfmode,
			psl->sl_count, psl->sl_addr, 0);
		sock_kfree_s(sk, psl, IP6_SFLSIZE(psl->sl_max));
	} else
		(void) ip6_mc_del_src(idev, group, pmc->sfmode, 0, 0, 0);
	pmc->sflist = newpsl;
	pmc->sfmode = gsf->gf_fmode;
done:
	read_unlock_bh(&idev->lock);
	in6_dev_put(idev);
	dev_put(dev);
	return err;
}

int ip6_mc_msfget(struct sock *sk, struct group_filter *gsf,
	struct group_filter *optval, int *optlen)
{
	int err, i, count, copycount;
	struct in6_addr *group;
	struct ipv6_mc_socklist *pmc;
	struct inet6_dev *idev;
	struct net_device *dev;
	struct ipv6_pinfo *inet6 = &sk->net_pinfo.af_inet6;
	struct ip6_sf_socklist *psl;

	group = &((struct sockaddr_in6 *)&gsf->gf_group)->sin6_addr;

	if (!(ipv6_addr_type(group) & IPV6_ADDR_MULTICAST))
		return -EINVAL;

	idev = ip6_mc_find_dev(group, gsf->gf_interface);

	if (!idev)
		return -ENODEV;

	dev = idev->dev;

	err = -EADDRNOTAVAIL;

	for (pmc=inet6->ipv6_mc_list; pmc; pmc=pmc->next) {
		if (pmc->ifindex != gsf->gf_interface)
			continue;
		if (ipv6_addr_cmp(group, &pmc->addr) == 0)
			break;
	}
	if (!pmc)		/* must have a prior join */
		goto done;
	gsf->gf_fmode = pmc->sfmode;
	psl = pmc->sflist;
	count = psl ? psl->sl_count : 0;
	read_unlock_bh(&idev->lock);
	in6_dev_put(idev);
	dev_put(dev);

	copycount = count < gsf->gf_numsrc ? count : gsf->gf_numsrc;
	gsf->gf_numsrc = count;
	if (put_user(GROUP_FILTER_SIZE(copycount), optlen) ||
	    copy_to_user((void *)optval, gsf, GROUP_FILTER_SIZE(0))) {
		return -EFAULT;
	}
	for (i=0; i<copycount; i++) {
		struct sockaddr_in6 *psin6;
		struct sockaddr_storage ss;

		psin6 = (struct sockaddr_in6 *)&ss;
		memset(&ss, 0, sizeof(ss));
		psin6->sin6_family = AF_INET6;
		psin6->sin6_addr = psl->sl_addr[i];
	    	if (copy_to_user((void *)&optval->gf_slist[i], &ss, sizeof(ss)))
			return -EFAULT;
	}
	return 0;
done:
	read_unlock_bh(&idev->lock);
	in6_dev_put(idev);
	dev_put(dev);
	return err;
}

int inet6_mc_check(struct sock *sk, struct in6_addr *mc_addr,
	struct in6_addr *src_addr)
{
	struct ipv6_pinfo *np = &sk->net_pinfo.af_inet6;
	struct ipv6_mc_socklist *mc;
	struct ip6_sf_socklist *psl;
	int rv = 1;

	read_lock(&ipv6_sk_mc_lock);
	for (mc = np->ipv6_mc_list; mc; mc = mc->next) {
		if (ipv6_addr_cmp(&mc->addr, mc_addr) == 0)
			break;
	}
	if (!mc) {
		read_unlock(&ipv6_sk_mc_lock);
		return 1;
	}
	psl = mc->sflist;
	if (!psl) {
		rv = mc->sfmode == MCAST_EXCLUDE;
	} else {
		int i;

		for (i=0; i<psl->sl_count; i++) {
			if (ipv6_addr_cmp(&psl->sl_addr[i], src_addr) == 0)
				break;
		}
		if (mc->sfmode == MCAST_INCLUDE && i >= psl->sl_count)
			rv = 0;
		if (mc->sfmode == MCAST_EXCLUDE && i < psl->sl_count)
			rv = 0;
	}
	read_unlock(&ipv6_sk_mc_lock);

	return rv;
}

static void ma_put(struct ifmcaddr6 *mc)
{
	if (atomic_dec_and_test(&mc->mca_refcnt)) {
		in6_dev_put(mc->idev);
		kfree(mc);
	}
}

static void igmp6_group_added(struct ifmcaddr6 *mc)
{
	struct net_device *dev = mc->idev->dev;
	char buf[MAX_ADDR_LEN];

	spin_lock_bh(&mc->mca_lock);
	if (!(mc->mca_flags&MAF_LOADED)) {
		mc->mca_flags |= MAF_LOADED;
		if (ndisc_mc_map(&mc->mca_addr, buf, dev, 0) == 0)
			dev_mc_add(dev, buf, dev->addr_len, 0);
	}
	spin_unlock_bh(&mc->mca_lock);

	if (!(dev->flags & IFF_UP) || (mc->mca_flags & MAF_NOREPORT))
		return;

	if (MLD_V1_SEEN(mc->idev)) {
		igmp6_join_group(mc);
		return;
	}
	/* else v2 */

	mc->mca_crcount = mc->idev->mc_qrv;
	mld_ifc_event(mc->idev);
}

static void igmp6_group_dropped(struct ifmcaddr6 *mc)
{
	struct net_device *dev = mc->idev->dev;
	char buf[MAX_ADDR_LEN];

	spin_lock_bh(&mc->mca_lock);
	if (mc->mca_flags&MAF_LOADED) {
		mc->mca_flags &= ~MAF_LOADED;
		if (ndisc_mc_map(&mc->mca_addr, buf, dev, 0) == 0)
			dev_mc_delete(dev, buf, dev->addr_len, 0);
	}

	if (mc->mca_flags & MAF_NOREPORT)
		goto done;
	spin_unlock_bh(&mc->mca_lock);

	if (!mc->idev->dead)
		igmp6_leave_group(mc);

	spin_lock_bh(&mc->mca_lock);
	if (del_timer(&mc->mca_timer))
		atomic_dec(&mc->mca_refcnt);
done:
	ip6_mc_clear_src(mc);
	spin_unlock_bh(&mc->mca_lock);
}

/*
 * deleted ifmcaddr6 manipulation
 */
static void mld_add_delrec(struct inet6_dev *idev, struct ifmcaddr6 *im)
{
	struct ifmcaddr6 *pmc;

	/* this is an "ifmcaddr6" for convenience; only the fields below
	 * are actually used. In particular, the refcnt and users are not
	 * used for management of the delete list. Using the same structure
	 * for deleted items allows change reports to use common code with
	 * non-deleted or query-response MCA's.
	 */
	pmc = (struct ifmcaddr6 *)kmalloc(sizeof(*pmc), GFP_ATOMIC);
	if (!pmc)
		return;
	memset(pmc, 0, sizeof(*pmc));
	spin_lock_bh(&im->mca_lock);
	pmc->mca_lock = SPIN_LOCK_UNLOCKED;
	pmc->idev = im->idev;
	in6_dev_hold(idev);
	pmc->mca_addr = im->mca_addr;
	pmc->mca_crcount = idev->mc_qrv;
	pmc->mca_sfmode = im->mca_sfmode;
	if (pmc->mca_sfmode == MCAST_INCLUDE) {
		struct ip6_sf_list *psf;

		pmc->mca_tomb = im->mca_tomb;
		pmc->mca_sources = im->mca_sources;
		im->mca_tomb = im->mca_sources = 0;
		for (psf=pmc->mca_sources; psf; psf=psf->sf_next)
			psf->sf_crcount = pmc->mca_crcount;
	}
	spin_unlock_bh(&im->mca_lock);

	write_lock_bh(&idev->mc_lock);
	pmc->next = idev->mc_tomb;
	idev->mc_tomb = pmc;
	write_unlock_bh(&idev->mc_lock);
}

static void mld_del_delrec(struct inet6_dev *idev, struct in6_addr *pmca)
{
	struct ifmcaddr6 *pmc, *pmc_prev;
	struct ip6_sf_list *psf, *psf_next;

	write_lock_bh(&idev->mc_lock);
	pmc_prev = 0;
	for (pmc=idev->mc_tomb; pmc; pmc=pmc->next) {
		if (ipv6_addr_cmp(&pmc->mca_addr, pmca) == 0)
			break;
		pmc_prev = pmc;
	}
	if (pmc) {
		if (pmc_prev)
			pmc_prev->next = pmc->next;
		else
			idev->mc_tomb = pmc->next;
	}
	write_unlock_bh(&idev->mc_lock);
	if (pmc) {
		for (psf=pmc->mca_tomb; psf; psf=psf_next) {
			psf_next = psf->sf_next;
			kfree(psf);
		}
		in6_dev_put(pmc->idev);
		kfree(pmc);
	}
}

static void mld_clear_delrec(struct inet6_dev *idev)
{
	struct ifmcaddr6 *pmc, *nextpmc;

	write_lock_bh(&idev->mc_lock);
	pmc = idev->mc_tomb;
	idev->mc_tomb = 0;
	write_unlock_bh(&idev->mc_lock);

	for (; pmc; pmc = nextpmc) {
		nextpmc = pmc->next;
		ip6_mc_clear_src(pmc);
		in6_dev_put(pmc->idev);
		kfree(pmc);
	}

	/* clear dead sources, too */
	read_lock_bh(&idev->lock);
	for (pmc=idev->mc_list; pmc; pmc=pmc->next) {
		struct ip6_sf_list *psf, *psf_next;

		spin_lock_bh(&pmc->mca_lock);
		psf = pmc->mca_tomb;
		pmc->mca_tomb = 0;
		spin_unlock_bh(&pmc->mca_lock);
		for (; psf; psf=psf_next) {
			psf_next = psf->sf_next;
			kfree(psf);
		}
	}
	read_unlock_bh(&idev->lock);
}


/*
 *	device multicast group inc (add if not found)
 */
int ipv6_dev_mc_inc(struct net_device *dev, struct in6_addr *addr)
{
	struct ifmcaddr6 *mc;
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

	for (mc = idev->mc_list; mc; mc = mc->next) {
		if (ipv6_addr_cmp(&mc->mca_addr, addr) == 0) {
			mc->mca_users++;
			write_unlock_bh(&idev->lock);
			ip6_mc_add_src(idev, &mc->mca_addr, MCAST_EXCLUDE, 0,
				0, 0);
			in6_dev_put(idev);
			return 0;
		}
	}

	/*
	 *	not found: create a new one.
	 */

	mc = kmalloc(sizeof(struct ifmcaddr6), GFP_ATOMIC);

	if (mc == NULL) {
		write_unlock_bh(&idev->lock);
		in6_dev_put(idev);
		return -ENOMEM;
	}

	memset(mc, 0, sizeof(struct ifmcaddr6));
	mc->mca_timer.function = igmp6_timer_handler;
	mc->mca_timer.data = (unsigned long) mc;

	memcpy(&mc->mca_addr, addr, sizeof(struct in6_addr));
	mc->idev = idev;
	mc->mca_users = 1;
	atomic_set(&mc->mca_refcnt, 2);
	mc->mca_lock = SPIN_LOCK_UNLOCKED;

	/* initial mode is (EX, empty) */
	mc->mca_sfmode = MCAST_EXCLUDE;
	mc->mca_sfcount[MCAST_EXCLUDE] = 1;

	if (ipv6_addr_cmp(&mc->mca_addr, &all_nodes_addr) == 0 ||
	    IPV6_ADDR_MC_SCOPE(&mc->mca_addr) < IPV6_ADDR_SCOPE_LINKLOCAL)
		mc->mca_flags |= MAF_NOREPORT;

	mc->next = idev->mc_list;
	idev->mc_list = mc;
	write_unlock_bh(&idev->lock);

	mld_del_delrec(idev, &mc->mca_addr);
	igmp6_group_added(mc);
	ma_put(mc);
	return 0;
}

/*
 *	device multicast group del
 */
static int __ipv6_dev_mc_dec(struct net_device *dev, struct inet6_dev *idev, struct in6_addr *addr)
{
	struct ifmcaddr6 *ma, **map;

	write_lock_bh(&idev->lock);
	for (map = &idev->mc_list; (ma=*map) != NULL; map = &ma->next) {
		if (ipv6_addr_cmp(&ma->mca_addr, addr) == 0) {
			if (--ma->mca_users == 0) {
				*map = ma->next;
				write_unlock_bh(&idev->lock);

				igmp6_group_dropped(ma);

				ma_put(ma);
				return 0;
			}
			write_unlock_bh(&idev->lock);
			return 0;
		}
	}
	write_unlock_bh(&idev->lock);

	return -ENOENT;
}

int ipv6_dev_mc_dec(struct net_device *dev, struct in6_addr *addr)
{
	struct inet6_dev *idev = in6_dev_get(dev);
	int err;

	if (!idev)
		return -ENODEV;

	err = __ipv6_dev_mc_dec(dev, idev, addr);

	in6_dev_put(idev);

	return err;
}

/*
 *	check if the interface/address pair is valid
 */
int ipv6_chk_mcast_addr(struct net_device *dev, struct in6_addr *group,
	struct in6_addr *src_addr)
{
	struct inet6_dev *idev;
	struct ifmcaddr6 *mc;
	int rv = 0;

	idev = in6_dev_get(dev);
	if (idev) {
		read_lock_bh(&idev->lock);
		for (mc = idev->mc_list; mc; mc=mc->next) {
			if (ipv6_addr_cmp(&mc->mca_addr, group) == 0)
				break;
		}
		if (mc) {
			if (!ipv6_addr_any(src_addr)) {
				struct ip6_sf_list *psf;

				spin_lock_bh(&mc->mca_lock);
				for (psf=mc->mca_sources;psf;psf=psf->sf_next) {
					if (ipv6_addr_cmp(&psf->sf_addr,
					    src_addr) == 0)
						break;
				}
				if (psf)
					rv = psf->sf_count[MCAST_INCLUDE] ||
						psf->sf_count[MCAST_EXCLUDE] !=
						mc->mca_sfcount[MCAST_EXCLUDE];
				else
					rv = mc->mca_sfcount[MCAST_EXCLUDE] !=0;
				spin_unlock_bh(&mc->mca_lock);
			} else
				rv = 1; /* don't filter unspecified source */
		}
		read_unlock_bh(&idev->lock);
		in6_dev_put(idev);
	}
	return rv;
}

static void mld_gq_start_timer(struct inet6_dev *idev)
{
	int tv = net_random() % idev->mc_maxdelay;

	idev->mc_gq_running = 1;
	if (!mod_timer(&idev->mc_gq_timer, jiffies+tv+2))
		in6_dev_hold(idev);
}

static void mld_ifc_start_timer(struct inet6_dev *idev, int delay)
{
	int tv = net_random() % delay;

	if (!mod_timer(&idev->mc_ifc_timer, jiffies+tv+2))
		in6_dev_hold(idev);
}

/*
 *	IGMP handling (alias multicast ICMPv6 messages)
 */

static void igmp6_group_queried(struct ifmcaddr6 *ma, unsigned long resptime)
{
	unsigned long delay = resptime;

	/* Do not start timer for these addresses */
	if (ipv6_addr_is_ll_all_nodes(&ma->mca_addr) ||
	    IPV6_ADDR_MC_SCOPE(&ma->mca_addr) < IPV6_ADDR_SCOPE_LINKLOCAL)
		return;

	if (del_timer(&ma->mca_timer)) {
		atomic_dec(&ma->mca_refcnt);
		delay = ma->mca_timer.expires - jiffies;
	}

	if (delay >= resptime) {
		if (resptime)
			delay = net_random() % resptime;
		else
			delay = 1;
	}

	ma->mca_timer.expires = jiffies + delay;
	if (!mod_timer(&ma->mca_timer, jiffies + delay))
		atomic_inc(&ma->mca_refcnt);
	ma->mca_flags |= MAF_TIMER_RUNNING;
	spin_unlock(&ma->mca_lock);
}

static void mld_marksources(struct ifmcaddr6 *pmc, int nsrcs,
	struct in6_addr *srcs)
{
	struct ip6_sf_list *psf;
	int i, scount;

	scount = 0;
	for (psf=pmc->mca_sources; psf; psf=psf->sf_next) {
		if (scount == nsrcs)
			break;
		for (i=0; i<nsrcs; i++)
			if (ipv6_addr_cmp(&srcs[i], &psf->sf_addr) == 0) {
				psf->sf_gsresp = 1;
				scount++;
				break;
			}
	}
}

int igmp6_event_query(struct sk_buff *skb)
{
	struct mld2_query *mlh2 = (struct mld2_query *) skb->h.raw;
	struct ifmcaddr6 *ma;
	struct in6_addr *group;
	unsigned long max_delay;
	struct inet6_dev *idev;
	struct icmp6hdr *hdr;
	int group_type;
	int mark = 0;
	int len;

	if (!pskb_may_pull(skb, sizeof(struct in6_addr)))
		return -EINVAL;

	len = ntohs(skb->nh.ipv6h->payload_len);

	/* Drop queries with not link local source */
	if (!(ipv6_addr_type(&skb->nh.ipv6h->saddr)&IPV6_ADDR_LINKLOCAL))
		return -EINVAL;

	idev = in6_dev_get(skb->dev);

	if (idev == NULL)
		return 0;

	hdr = (struct icmp6hdr *) skb->h.raw;
	group = (struct in6_addr *) (hdr + 1);
	group_type = ipv6_addr_type(group);

	if (group_type != IPV6_ADDR_ANY &&
	    !(group_type&IPV6_ADDR_MULTICAST)) {
		in6_dev_put(idev);
		return -EINVAL;
	}

	if (len == 24) {
		int switchback;
		/* MLDv1 router present */

		/* Translate milliseconds to jiffies */
		max_delay = (ntohs(hdr->icmp6_maxdelay)*HZ)/1000;

		switchback = (idev->mc_qrv + 1) * max_delay;
		idev->mc_v1_seen = jiffies + switchback;

		/* cancel the interface change timer */
		idev->mc_ifc_count = 0;
		if (del_timer(&idev->mc_ifc_timer))
			__in6_dev_put(idev);
		/* clear deleted report items */
		mld_clear_delrec(idev);
	} else if (len >= 28) {
		max_delay = (MLDV2_MRC(ntohs(mlh2->mrc))*HZ)/1000;
		if (!max_delay)
			max_delay = 1;
		idev->mc_maxdelay = max_delay;
		if (mlh2->qrv)
			idev->mc_qrv = mlh2->qrv;
		if (group_type == IPV6_ADDR_ANY) { /* general query */
			if (mlh2->nsrcs) {
				in6_dev_put(idev);
				return -EINVAL; /* no sources allowed */
			}
			mld_gq_start_timer(idev);
			in6_dev_put(idev);
			return 0;
		}
		/* mark sources to include, if group & source-specific */
		mark = mlh2->nsrcs != 0;
	} else {
		in6_dev_put(idev);
		return -EINVAL;
	}

	read_lock_bh(&idev->lock);
	if (group_type == IPV6_ADDR_ANY) {
		for (ma = idev->mc_list; ma; ma=ma->next) {
			spin_lock_bh(&ma->mca_lock);
			igmp6_group_queried(ma, max_delay);
			spin_unlock_bh(&ma->mca_lock);
		}
	} else {
		for (ma = idev->mc_list; ma; ma=ma->next) {
			if (group_type != IPV6_ADDR_ANY &&
			    ipv6_addr_cmp(group, &ma->mca_addr) != 0)
				continue;
			spin_lock_bh(&ma->mca_lock);
			if (ma->mca_flags & MAF_TIMER_RUNNING) {
				/* gsquery <- gsquery && mark */
				if (!mark)
					ma->mca_flags &= ~MAF_GSQUERY;
			} else {
				/* gsquery <- mark */
				if (mark)
					ma->mca_flags |= MAF_GSQUERY;
				else
					ma->mca_flags &= ~MAF_GSQUERY;
			}
			if (ma->mca_flags & MAF_GSQUERY)
				mld_marksources(ma, ntohs(mlh2->nsrcs),
					mlh2->srcs);
			igmp6_group_queried(ma, max_delay);
			spin_unlock_bh(&ma->mca_lock);
			if (group_type != IPV6_ADDR_ANY)
				break;
		}
	}
	read_unlock_bh(&idev->lock);
	in6_dev_put(idev);

	return 0;
}


int igmp6_event_report(struct sk_buff *skb)
{
	struct ifmcaddr6 *ma;
	struct in6_addr *addrp;
	struct inet6_dev *idev;
	struct icmp6hdr *hdr;
	int addr_type;

	/* Our own report looped back. Ignore it. */
	if (skb->pkt_type == PACKET_LOOPBACK)
		return 0;

	if (!pskb_may_pull(skb, sizeof(struct in6_addr)))
		return -EINVAL;

	hdr = (struct icmp6hdr*) skb->h.raw;

	/* Drop reports with not link local source */
	addr_type = ipv6_addr_type(&skb->nh.ipv6h->saddr);
	if (addr_type != IPV6_ADDR_ANY && 
	    !(addr_type&IPV6_ADDR_LINKLOCAL))
		return -EINVAL;

	addrp = (struct in6_addr *) (hdr + 1);

	idev = in6_dev_get(skb->dev);
	if (idev == NULL)
		return -ENODEV;

	/*
	 *	Cancel the timer for this group
	 */

	read_lock_bh(&idev->lock);
	for (ma = idev->mc_list; ma; ma=ma->next) {
		if (ipv6_addr_cmp(&ma->mca_addr, addrp) == 0) {
			spin_lock(&ma->mca_lock);
			if (del_timer(&ma->mca_timer))
				atomic_dec(&ma->mca_refcnt);
			ma->mca_flags &= ~(MAF_LAST_REPORTER|MAF_TIMER_RUNNING);
			spin_unlock(&ma->mca_lock);
			break;
		}
	}
	read_unlock_bh(&idev->lock);
	in6_dev_put(idev);
	return 0;
}

static int is_in(struct ifmcaddr6 *pmc, struct ip6_sf_list *psf, int type,
	int gdeleted, int sdeleted)
{
	switch (type) {
	case MLD2_MODE_IS_INCLUDE:
	case MLD2_MODE_IS_EXCLUDE:
		if (gdeleted || sdeleted)
			return 0;
		return !((pmc->mca_flags & MAF_GSQUERY) && !psf->sf_gsresp);
	case MLD2_CHANGE_TO_INCLUDE:
		if (gdeleted || sdeleted)
			return 0;
		return psf->sf_count[MCAST_INCLUDE] != 0;
	case MLD2_CHANGE_TO_EXCLUDE:
		if (gdeleted || sdeleted)
			return 0;
		if (pmc->mca_sfcount[MCAST_EXCLUDE] == 0 ||
		    psf->sf_count[MCAST_INCLUDE])
			return 0;
		return pmc->mca_sfcount[MCAST_EXCLUDE] ==
			psf->sf_count[MCAST_EXCLUDE];
	case MLD2_ALLOW_NEW_SOURCES:
		if (gdeleted || !psf->sf_crcount)
			return 0;
		return (pmc->mca_sfmode == MCAST_INCLUDE) ^ sdeleted;
	case MLD2_BLOCK_OLD_SOURCES:
		if (pmc->mca_sfmode == MCAST_INCLUDE)
			return gdeleted || (psf->sf_crcount && sdeleted);
		return psf->sf_crcount && !gdeleted && !sdeleted;
	}
	return 0;
}

static int
mld_scount(struct ifmcaddr6 *pmc, int type, int gdeleted, int sdeleted)
{
	struct ip6_sf_list *psf;
	int scount = 0;

	for (psf=pmc->mca_sources; psf; psf=psf->sf_next) {
		if (!is_in(pmc, psf, type, gdeleted, sdeleted))
			continue;
		scount++;
	}
	return scount;
}

static struct sk_buff *mld_newpack(struct net_device *dev, int size)
{
	struct sock *sk = igmp6_socket->sk;
	struct sk_buff *skb;
	struct mld2_report *pmr;
	struct in6_addr addr_buf;
	int err;
	u8 ra[8] = { IPPROTO_ICMPV6, 0,
		     IPV6_TLV_ROUTERALERT, 2, 0, 0,
		     IPV6_TLV_PADN, 0 };

	skb = sock_alloc_send_skb(sk, size + dev->hard_header_len+15, 1, &err);

	if (skb == 0)
		return 0;

	skb_reserve(skb, (dev->hard_header_len + 15) & ~15);
	if (dev->hard_header) {
		unsigned char ha[MAX_ADDR_LEN];

		ndisc_mc_map(&mld2_all_mcr, ha, dev, 1);
		if (dev->hard_header(skb, dev, ETH_P_IPV6,ha,NULL,size) < 0) {
			kfree_skb(skb);
			return 0;
		}
	}

	if (ipv6_get_lladdr(dev, &addr_buf)) {
		/* <draft-ietf-magma-mld-source-02.txt>:
		 * use unspecified address as the source address 
		 * when a valid link-local address is not available.
		 */
		memset(&addr_buf, 0, sizeof(addr_buf));
	}

	ip6_nd_hdr(sk, skb, dev, &addr_buf, &mld2_all_mcr, NEXTHDR_HOP, 0);

	memcpy(skb_put(skb, sizeof(ra)), ra, sizeof(ra));

	pmr =(struct mld2_report *)skb_put(skb, sizeof(*pmr));
	skb->h.raw = (unsigned char *)pmr;
	pmr->type = ICMPV6_MLD2_REPORT;
	pmr->resv1 = 0;
	pmr->csum = 0;
	pmr->resv2 = 0;
	pmr->ngrec = 0;
	return skb;
}

static void mld_sendpack(struct sk_buff *skb)
{
	struct ipv6hdr *pip6 = skb->nh.ipv6h;
	struct mld2_report *pmr = (struct mld2_report *)skb->h.raw;
	int payload_len, mldlen, err;

	payload_len = skb->tail - (unsigned char *)skb->nh.ipv6h -
		sizeof(struct ipv6hdr);
	mldlen = skb->tail - skb->h.raw;
	pip6->payload_len = htons(payload_len);

	pmr->csum = csum_ipv6_magic(&pip6->saddr, &pip6->daddr, mldlen,
		IPPROTO_ICMPV6, csum_partial(skb->h.raw, mldlen, 0));
	err = NF_HOOK(PF_INET6, NF_IP6_LOCAL_OUT, skb, NULL, skb->dev,
		      dev_queue_xmit);
	if (!err)
		ICMP6_INC_STATS(Icmp6OutMsgs);
}

static int grec_size(struct ifmcaddr6 *pmc, int type, int gdel, int sdel)
{
	return sizeof(struct mld2_grec) + 4*mld_scount(pmc,type,gdel,sdel);
}

static struct sk_buff *add_grhead(struct sk_buff *skb, struct ifmcaddr6 *pmc,
	int type, struct mld2_grec **ppgr)
{
	struct net_device *dev = pmc->idev->dev;
	struct mld2_report *pmr;
	struct mld2_grec *pgr;

	if (!skb)
		skb = mld_newpack(dev, dev->mtu);
	if (!skb)
		return 0;
	pgr = (struct mld2_grec *)skb_put(skb, sizeof(struct mld2_grec));
	pgr->grec_type = type;
	pgr->grec_auxwords = 0;
	pgr->grec_nsrcs = 0;
	pgr->grec_mca = pmc->mca_addr;	/* structure copy */
	pmr = (struct mld2_report *)skb->h.raw;
	pmr->ngrec = htons(ntohs(pmr->ngrec)+1);
	*ppgr = pgr;
	return skb;
}

#define AVAILABLE(skb) ((skb) ? ((skb)->dev ? (skb)->dev->mtu - (skb)->len : \
	skb_tailroom(skb)) : 0)

static struct sk_buff *add_grec(struct sk_buff *skb, struct ifmcaddr6 *pmc,
	int type, int gdeleted, int sdeleted)
{
	struct net_device *dev = pmc->idev->dev;
	struct mld2_report *pmr;
	struct mld2_grec *pgr = 0;
	struct ip6_sf_list *psf, *psf_next, *psf_prev, **psf_list;
	int scount, first, isquery, truncate;

	if (pmc->mca_flags & MAF_NOREPORT)
		return skb;

	isquery = type == MLD2_MODE_IS_INCLUDE ||
		  type == MLD2_MODE_IS_EXCLUDE;
	truncate = type == MLD2_MODE_IS_EXCLUDE ||
		    type == MLD2_CHANGE_TO_EXCLUDE;

	psf_list = sdeleted ? &pmc->mca_tomb : &pmc->mca_sources;

	if (!*psf_list) {
		if (type == MLD2_ALLOW_NEW_SOURCES ||
		    type == MLD2_BLOCK_OLD_SOURCES)
			return skb;
		if (pmc->mca_crcount || isquery) {
			/* make sure we have room for group header and at
			 * least one source.
			 */
			if (skb && AVAILABLE(skb) < sizeof(struct mld2_grec)+
			    sizeof(struct in6_addr)) {
				mld_sendpack(skb);
				skb = 0; /* add_grhead will get a new one */
			}
			skb = add_grhead(skb, pmc, type, &pgr);
		}
		return skb;
	}
	pmr = skb ? (struct mld2_report *)skb->h.raw : 0;

	/* EX and TO_EX get a fresh packet, if needed */
	if (truncate) {
		if (pmr && pmr->ngrec &&
		    AVAILABLE(skb) < grec_size(pmc, type, gdeleted, sdeleted)) {
			if (skb)
				mld_sendpack(skb);
			skb = mld_newpack(dev, dev->mtu);
		}
	}
	first = 1;
	scount = 0;
	psf_prev = 0;
	for (psf=*psf_list; psf; psf=psf_next) {
		struct in6_addr *psrc;

		psf_next = psf->sf_next;

		if (!is_in(pmc, psf, type, gdeleted, sdeleted)) {
			psf_prev = psf;
			continue;
		}

		/* clear marks on query responses */
		if (isquery)
			psf->sf_gsresp = 0;

		if (AVAILABLE(skb) < sizeof(*psrc) +
		    first*sizeof(struct mld2_grec)) {
			if (truncate && !first)
				break;	 /* truncate these */
			if (pgr)
				pgr->grec_nsrcs = htons(scount);
			if (skb)
				mld_sendpack(skb);
			skb = mld_newpack(dev, dev->mtu);
			first = 1;
			scount = 0;
		}
		if (first) {
			skb = add_grhead(skb, pmc, type, &pgr);
			first = 0;
		}
		psrc = (struct in6_addr *)skb_put(skb, sizeof(*psrc));
		*psrc = psf->sf_addr;
		scount++;
		if ((type == MLD2_ALLOW_NEW_SOURCES ||
		     type == MLD2_BLOCK_OLD_SOURCES) && psf->sf_crcount) {
			psf->sf_crcount--;
			if ((sdeleted || gdeleted) && psf->sf_crcount == 0) {
				if (psf_prev)
					psf_prev->sf_next = psf->sf_next;
				else
					*psf_list = psf->sf_next;
				kfree(psf);
				continue;
			}
		}
		psf_prev = psf;
	}
	if (pgr)
		pgr->grec_nsrcs = htons(scount);

	if (isquery)
		pmc->mca_flags &= ~MAF_GSQUERY;	/* clear query state */
	return skb;
}

static void mld_send_report(struct inet6_dev *idev, struct ifmcaddr6 *pmc)
{
	struct sk_buff *skb = 0;
	int type;

	if (!pmc) {
		read_lock_bh(&idev->lock);
		for (pmc=idev->mc_list; pmc; pmc=pmc->next) {
			if (pmc->mca_flags & MAF_NOREPORT)
				continue;
			spin_lock_bh(&pmc->mca_lock);
			if (pmc->mca_sfcount[MCAST_EXCLUDE])
				type = MLD2_MODE_IS_EXCLUDE;
			else
				type = MLD2_MODE_IS_INCLUDE;
			skb = add_grec(skb, pmc, type, 0, 0);
			spin_unlock_bh(&pmc->mca_lock);
		}
		read_unlock_bh(&idev->lock);
	} else {
		spin_lock_bh(&pmc->mca_lock);
		if (pmc->mca_sfcount[MCAST_EXCLUDE])
			type = MLD2_MODE_IS_EXCLUDE;
		else
			type = MLD2_MODE_IS_INCLUDE;
		skb = add_grec(skb, pmc, type, 0, 0);
		spin_unlock_bh(&pmc->mca_lock);
	}
	if (skb)
		mld_sendpack(skb);
}

/*
 * remove zero-count source records from a source filter list
 */
static void mld_clear_zeros(struct ip6_sf_list **ppsf)
{
	struct ip6_sf_list *psf_prev, *psf_next, *psf;

	psf_prev = 0;
	for (psf=*ppsf; psf; psf = psf_next) {
		psf_next = psf->sf_next;
		if (psf->sf_crcount == 0) {
			if (psf_prev)
				psf_prev->sf_next = psf->sf_next;
			else
				*ppsf = psf->sf_next;
			kfree(psf);
		} else
			psf_prev = psf;
	}
}

static void mld_send_cr(struct inet6_dev *idev)
{
	struct ifmcaddr6 *pmc, *pmc_prev, *pmc_next;
	struct sk_buff *skb = 0;
	int type, dtype;

	read_lock_bh(&idev->lock);
	write_lock_bh(&idev->mc_lock);

	/* deleted MCA's */
	pmc_prev = 0;
	for (pmc=idev->mc_tomb; pmc; pmc=pmc_next) {
		pmc_next = pmc->next;
		if (pmc->mca_sfmode == MCAST_INCLUDE) {
			type = MLD2_BLOCK_OLD_SOURCES;
			dtype = MLD2_BLOCK_OLD_SOURCES;
			skb = add_grec(skb, pmc, type, 1, 0);
			skb = add_grec(skb, pmc, dtype, 1, 1);
		}
		if (pmc->mca_crcount) {
			pmc->mca_crcount--;
			if (pmc->mca_sfmode == MCAST_EXCLUDE) {
				type = MLD2_CHANGE_TO_INCLUDE;
				skb = add_grec(skb, pmc, type, 1, 0);
			}
			if (pmc->mca_crcount == 0) {
				mld_clear_zeros(&pmc->mca_tomb);
				mld_clear_zeros(&pmc->mca_sources);
			}
		}
		if (pmc->mca_crcount == 0 && !pmc->mca_tomb &&
		    !pmc->mca_sources) {
			if (pmc_prev)
				pmc_prev->next = pmc_next;
			else
				idev->mc_tomb = pmc_next;
			in6_dev_put(pmc->idev);
			kfree(pmc);
		} else
			pmc_prev = pmc;
	}
	write_unlock_bh(&idev->mc_lock);

	/* change recs */
	for (pmc=idev->mc_list; pmc; pmc=pmc->next) {
		spin_lock_bh(&pmc->mca_lock);
		if (pmc->mca_sfcount[MCAST_EXCLUDE]) {
			type = MLD2_BLOCK_OLD_SOURCES;
			dtype = MLD2_ALLOW_NEW_SOURCES;
		} else {
			type = MLD2_ALLOW_NEW_SOURCES;
			dtype = MLD2_BLOCK_OLD_SOURCES;
		}
		skb = add_grec(skb, pmc, type, 0, 0);
		skb = add_grec(skb, pmc, dtype, 0, 1);	/* deleted sources */

		/* filter mode changes */
		if (pmc->mca_crcount) {
			pmc->mca_crcount--;
			if (pmc->mca_sfmode == MCAST_EXCLUDE)
				type = MLD2_CHANGE_TO_EXCLUDE;
			else
				type = MLD2_CHANGE_TO_INCLUDE;
			skb = add_grec(skb, pmc, type, 0, 0);
		}
		spin_unlock_bh(&pmc->mca_lock);
	}
	read_unlock_bh(&idev->lock);
	if (!skb)
		return;
	(void) mld_sendpack(skb);
}

void igmp6_send(struct in6_addr *addr, struct net_device *dev, int type)
{
	struct sock *sk = igmp6_socket->sk;
        struct sk_buff *skb;
        struct icmp6hdr *hdr;
	struct in6_addr *snd_addr;
	struct in6_addr *addrp;
	struct in6_addr addr_buf;
	struct in6_addr all_routers;
	int err, len, payload_len, full_len;
	u8 ra[8] = { IPPROTO_ICMPV6, 0,
		     IPV6_TLV_ROUTERALERT, 2, 0, 0,
		     IPV6_TLV_PADN, 0 };

	snd_addr = addr;
	if (type == ICMPV6_MGM_REDUCTION) {
		snd_addr = &all_routers;
		ipv6_addr_all_routers(&all_routers);
	}

	len = sizeof(struct icmp6hdr) + sizeof(struct in6_addr);
	payload_len = len + sizeof(ra);
	full_len = sizeof(struct ipv6hdr) + payload_len;

	skb = sock_alloc_send_skb(sk, dev->hard_header_len + full_len + 15, 1, &err);

	if (skb == NULL)
		return;

	skb_reserve(skb, (dev->hard_header_len + 15) & ~15);
	if (dev->hard_header) {
		unsigned char ha[MAX_ADDR_LEN];
		ndisc_mc_map(snd_addr, ha, dev, 1);
		if (dev->hard_header(skb, dev, ETH_P_IPV6, ha, NULL, full_len) < 0)
			goto out;
	}

	if (ipv6_get_lladdr(dev, &addr_buf)) {
		/* <draft-ietf-magma-mld-source-02.txt>:
		 * use unspecified address as the source address 
		 * when a valid link-local address is not available.
		 */
		memset(&addr_buf, 0, sizeof(addr_buf));
	}

	ip6_nd_hdr(sk, skb, dev, &addr_buf, snd_addr, NEXTHDR_HOP, payload_len);

	memcpy(skb_put(skb, sizeof(ra)), ra, sizeof(ra));

	hdr = (struct icmp6hdr *) skb_put(skb, sizeof(struct icmp6hdr));
	memset(hdr, 0, sizeof(struct icmp6hdr));
	hdr->icmp6_type = type;

	addrp = (struct in6_addr *) skb_put(skb, sizeof(struct in6_addr));
	ipv6_addr_copy(addrp, addr);

	hdr->icmp6_cksum = csum_ipv6_magic(&addr_buf, snd_addr, len,
					   IPPROTO_ICMPV6,
					   csum_partial((__u8 *) hdr, len, 0));

	err = NF_HOOK(PF_INET6, NF_IP6_LOCAL_OUT, skb, NULL, skb->dev,
		      dev_queue_xmit);
	if (!err) {
		if (type == ICMPV6_MGM_REDUCTION)
			ICMP6_INC_STATS(Icmp6OutGroupMembReductions);
		else
			ICMP6_INC_STATS(Icmp6OutGroupMembResponses);
		ICMP6_INC_STATS(Icmp6OutMsgs);
	}

	return;

out:
	kfree_skb(skb);
}

static int ip6_mc_del1_src(struct ifmcaddr6 *pmc, int sfmode,
	struct in6_addr *psfsrc)
{
	struct ip6_sf_list *psf, *psf_prev;
	int rv = 0;

	psf_prev = 0;
	for (psf=pmc->mca_sources; psf; psf=psf->sf_next) {
		if (ipv6_addr_cmp(&psf->sf_addr, psfsrc) == 0)
			break;
		psf_prev = psf;
	}
	if (!psf || psf->sf_count[sfmode] == 0) {
		/* source filter not found, or count wrong =>  bug */
		return -ESRCH;
	}
	psf->sf_count[sfmode]--;
	if (!psf->sf_count[MCAST_INCLUDE] && !psf->sf_count[MCAST_EXCLUDE]) {
		struct inet6_dev *idev = pmc->idev;

		/* no more filters for this source */
		if (psf_prev)
			psf_prev->sf_next = psf->sf_next;
		else
			pmc->mca_sources = psf->sf_next;
		if (psf->sf_oldin && !(pmc->mca_flags & MAF_NOREPORT) &&
		    !MLD_V1_SEEN(idev)) {
			psf->sf_crcount = idev->mc_qrv;
			psf->sf_next = pmc->mca_tomb;
			pmc->mca_tomb = psf;
			rv = 1;
		} else
			kfree(psf);
	}
	return rv;
}

int ip6_mc_del_src(struct inet6_dev *idev, struct in6_addr *pmca, int sfmode,
	int sfcount, struct in6_addr *psfsrc, int delta)
{
	struct ifmcaddr6 *pmc;
	int	changerec = 0;
	int	i, err;

	if (!idev)
		return -ENODEV;
	read_lock_bh(&idev->lock);
	for (pmc=idev->mc_list; pmc; pmc=pmc->next) {
		if (ipv6_addr_cmp(pmca, &pmc->mca_addr) == 0)
			break;
	}
	if (!pmc) {
		/* MCA not found?? bug */
		read_unlock_bh(&idev->lock);
		return -ESRCH;
	}
	spin_lock_bh(&pmc->mca_lock);
	sf_markstate(pmc);
	if (!delta) {
		if (!pmc->mca_sfcount[sfmode]) {
			spin_unlock_bh(&pmc->mca_lock);
			read_unlock_bh(&idev->lock);
			return -EINVAL;
		}
		pmc->mca_sfcount[sfmode]--;
	}
	err = 0;
	for (i=0; i<sfcount; i++) {
		int rv = ip6_mc_del1_src(pmc, sfmode, &psfsrc[i]);

		changerec |= rv > 0;
		if (!err && rv < 0)
			err = rv;
	}
	if (pmc->mca_sfmode == MCAST_EXCLUDE &&
	    pmc->mca_sfcount[MCAST_EXCLUDE] == 0 &&
	    pmc->mca_sfcount[MCAST_INCLUDE]) {
		struct ip6_sf_list *psf;

		/* filter mode change */
		pmc->mca_sfmode = MCAST_INCLUDE;
		pmc->mca_crcount = idev->mc_qrv;
		idev->mc_ifc_count = pmc->mca_crcount;
		for (psf=pmc->mca_sources; psf; psf = psf->sf_next)
			psf->sf_crcount = 0;
		mld_ifc_event(pmc->idev);
	} else if (sf_setstate(pmc) || changerec)
		mld_ifc_event(pmc->idev);
	spin_unlock_bh(&pmc->mca_lock);
	read_unlock_bh(&idev->lock);
	return err;
}

/*
 * Add multicast single-source filter to the interface list
 */
static int ip6_mc_add1_src(struct ifmcaddr6 *pmc, int sfmode,
	struct in6_addr *psfsrc, int delta)
{
	struct ip6_sf_list *psf, *psf_prev;

	psf_prev = 0;
	for (psf=pmc->mca_sources; psf; psf=psf->sf_next) {
		if (ipv6_addr_cmp(&psf->sf_addr, psfsrc) == 0)
			break;
		psf_prev = psf;
	}
	if (!psf) {
		psf = (struct ip6_sf_list *)kmalloc(sizeof(*psf), GFP_ATOMIC);
		if (!psf)
			return -ENOBUFS;
		memset(psf, 0, sizeof(*psf));
		psf->sf_addr = *psfsrc;
		if (psf_prev) {
			psf_prev->sf_next = psf;
		} else
			pmc->mca_sources = psf;
	}
	psf->sf_count[sfmode]++;
	return 0;
}

static void sf_markstate(struct ifmcaddr6 *pmc)
{
	struct ip6_sf_list *psf;
	int mca_xcount = pmc->mca_sfcount[MCAST_EXCLUDE];

	for (psf=pmc->mca_sources; psf; psf=psf->sf_next)
		if (pmc->mca_sfcount[MCAST_EXCLUDE]) {
			psf->sf_oldin = mca_xcount ==
				psf->sf_count[MCAST_EXCLUDE] &&
				!psf->sf_count[MCAST_INCLUDE];
		} else
			psf->sf_oldin = psf->sf_count[MCAST_INCLUDE] != 0;
}

static int sf_setstate(struct ifmcaddr6 *pmc)
{
	struct ip6_sf_list *psf;
	int mca_xcount = pmc->mca_sfcount[MCAST_EXCLUDE];
	int qrv = pmc->idev->mc_qrv;
	int new_in, rv;

	rv = 0;
	for (psf=pmc->mca_sources; psf; psf=psf->sf_next) {
		if (pmc->mca_sfcount[MCAST_EXCLUDE]) {
			new_in = mca_xcount == psf->sf_count[MCAST_EXCLUDE] &&
				!psf->sf_count[MCAST_INCLUDE];
		} else
			new_in = psf->sf_count[MCAST_INCLUDE] != 0;
		if (new_in != psf->sf_oldin) {
			psf->sf_crcount = qrv;
			rv++;
		}
	}
	return rv;
}

/*
 * Add multicast source filter list to the interface list
 */
int ip6_mc_add_src(struct inet6_dev *idev, struct in6_addr *pmca, int sfmode,
	int sfcount, struct in6_addr *psfsrc, int delta)
{
	struct ifmcaddr6 *pmc;
	int	isexclude;
	int	i, err;

	if (!idev)
		return -ENODEV;
	read_lock_bh(&idev->lock);
	for (pmc=idev->mc_list; pmc; pmc=pmc->next) {
		if (ipv6_addr_cmp(pmca, &pmc->mca_addr) == 0)
			break;
	}
	if (!pmc) {
		/* MCA not found?? bug */
		read_unlock_bh(&idev->lock);
		return -ESRCH;
	}
	spin_lock_bh(&pmc->mca_lock);

	sf_markstate(pmc);
	isexclude = pmc->mca_sfmode == MCAST_EXCLUDE;
	if (!delta)
		pmc->mca_sfcount[sfmode]++;
	err = 0;
	for (i=0; i<sfcount; i++) {
		err = ip6_mc_add1_src(pmc, sfmode, &psfsrc[i], delta);
		if (err)
			break;
	}
	if (err) {
		int j;

		pmc->mca_sfcount[sfmode]--;
		for (j=0; j<i; j++)
			(void) ip6_mc_del1_src(pmc, sfmode, &psfsrc[i]);
	} else if (isexclude != (pmc->mca_sfcount[MCAST_EXCLUDE] != 0)) {
		struct inet6_dev *idev = pmc->idev;
		struct ip6_sf_list *psf;

		/* filter mode change */
		if (pmc->mca_sfcount[MCAST_EXCLUDE])
			pmc->mca_sfmode = MCAST_EXCLUDE;
		else if (pmc->mca_sfcount[MCAST_INCLUDE])
			pmc->mca_sfmode = MCAST_INCLUDE;
		/* else no filters; keep old mode for reports */

		pmc->mca_crcount = idev->mc_qrv;
		idev->mc_ifc_count = pmc->mca_crcount;
		for (psf=pmc->mca_sources; psf; psf = psf->sf_next)
			psf->sf_crcount = 0;
		mld_ifc_event(idev);
	} else if (sf_setstate(pmc))
		mld_ifc_event(idev);
	spin_unlock_bh(&pmc->mca_lock);
	read_unlock_bh(&idev->lock);
	return err;
}

static void ip6_mc_clear_src(struct ifmcaddr6 *pmc)
{
	struct ip6_sf_list *psf, *nextpsf;

	for (psf=pmc->mca_tomb; psf; psf=nextpsf) {
		nextpsf = psf->sf_next;
		kfree(psf);
	}
	pmc->mca_tomb = 0;
	for (psf=pmc->mca_sources; psf; psf=nextpsf) {
		nextpsf = psf->sf_next;
		kfree(psf);
	}
	pmc->mca_sources = 0;
	pmc->mca_sfmode = MCAST_EXCLUDE;
	pmc->mca_sfcount[MCAST_EXCLUDE] = 0;
	pmc->mca_sfcount[MCAST_EXCLUDE] = 1;
}

static void igmp6_join_group(struct ifmcaddr6 *ma)
{
	unsigned long delay;

	if (ma->mca_flags & MAF_NOREPORT)
		return;

	igmp6_send(&ma->mca_addr, ma->idev->dev, ICMPV6_MGM_REPORT);

	delay = net_random() % IGMP6_UNSOLICITED_IVAL;

	spin_lock_bh(&ma->mca_lock);
	if (del_timer(&ma->mca_timer)) {
		atomic_dec(&ma->mca_refcnt);
		delay = ma->mca_timer.expires - jiffies;
	}

	if (!mod_timer(&ma->mca_timer, jiffies + delay))
		atomic_inc(&ma->mca_refcnt);
	ma->mca_flags |= MAF_TIMER_RUNNING | MAF_LAST_REPORTER;
	spin_unlock_bh(&ma->mca_lock);
}

int ip6_mc_leave_src(struct sock *sk, struct ipv6_mc_socklist *iml,
	struct inet6_dev *idev)
{
	int err;

	if (iml->sflist == 0) {
		/* any-source empty exclude case */
		return ip6_mc_del_src(idev, &iml->addr, iml->sfmode, 0, 0, 0);
	}
	err = ip6_mc_del_src(idev, &iml->addr, iml->sfmode,
		iml->sflist->sl_count, iml->sflist->sl_addr, 0);
	sock_kfree_s(sk, iml->sflist, IP6_SFLSIZE(iml->sflist->sl_max));
	iml->sflist = 0;
	return err;
}

static void igmp6_leave_group(struct ifmcaddr6 *ma)
{
	if (MLD_V1_SEEN(ma->idev)) {
		if (ma->mca_flags & MAF_LAST_REPORTER)
			igmp6_send(&ma->mca_addr, ma->idev->dev,
				ICMPV6_MGM_REDUCTION);
	} else {
		mld_add_delrec(ma->idev, ma);
		mld_ifc_event(ma->idev);
	}
}

static void mld_gq_timer_expire(unsigned long data)
{
	struct inet6_dev *idev = (struct inet6_dev *)data;

	idev->mc_gq_running = 0;
	mld_send_report(idev, 0);
	__in6_dev_put(idev);
}

static void mld_ifc_timer_expire(unsigned long data)
{
	struct inet6_dev *idev = (struct inet6_dev *)data;

	mld_send_cr(idev);
	if (idev->mc_ifc_count) {
		idev->mc_ifc_count--;
		if (idev->mc_ifc_count)
			mld_ifc_start_timer(idev, idev->mc_maxdelay);
	}
	__in6_dev_put(idev);
}

static void mld_ifc_event(struct inet6_dev *idev)
{
	if (MLD_V1_SEEN(idev))
		return;
	idev->mc_ifc_count = idev->mc_qrv;
	mld_ifc_start_timer(idev, 1);
}


static void igmp6_timer_handler(unsigned long data)
{
	struct ifmcaddr6 *ma = (struct ifmcaddr6 *) data;

	if (MLD_V1_SEEN(ma->idev))
		igmp6_send(&ma->mca_addr, ma->idev->dev, ICMPV6_MGM_REPORT);
	else
		mld_send_report(ma->idev, ma);

	spin_lock(&ma->mca_lock);
	ma->mca_flags |=  MAF_LAST_REPORTER;
	ma->mca_flags &= ~MAF_TIMER_RUNNING;
	spin_unlock(&ma->mca_lock);
	ma_put(ma);
}

/* Device going down */

void ipv6_mc_down(struct inet6_dev *idev)
{
	struct ifmcaddr6 *i;

	/* Withdraw multicast list */

	read_lock_bh(&idev->lock);
	idev->mc_ifc_count = 0;
	if (del_timer(&idev->mc_ifc_timer))
		__in6_dev_put(idev);
	idev->mc_gq_running = 0;
	if (del_timer(&idev->mc_gq_timer))
		__in6_dev_put(idev);

	for (i = idev->mc_list; i; i=i->next)
		igmp6_group_dropped(i);
	read_unlock_bh(&idev->lock);

	mld_clear_delrec(idev);
}


/* Device going up */

void ipv6_mc_up(struct inet6_dev *idev)
{
	struct ifmcaddr6 *i;

	/* Install multicast list, except for all-nodes (already installed) */

	read_lock_bh(&idev->lock);
	for (i = idev->mc_list; i; i=i->next)
		igmp6_group_added(i);
	read_unlock_bh(&idev->lock);
}

/* IPv6 device initialization. */

void ipv6_mc_init_dev(struct inet6_dev *idev)
{
	struct in6_addr maddr;

	write_lock_bh(&idev->lock);
	idev->mc_lock = RW_LOCK_UNLOCKED;
	idev->mc_gq_running = 0;
	init_timer(&idev->mc_gq_timer);
	idev->mc_gq_timer.data = (unsigned long) idev;
	idev->mc_gq_timer.function = &mld_gq_timer_expire;
	idev->mc_tomb = 0;
	idev->mc_ifc_count = 0;
	init_timer(&idev->mc_ifc_timer);
	idev->mc_ifc_timer.data = (unsigned long) idev;
	idev->mc_ifc_timer.function = &mld_ifc_timer_expire;
	idev->mc_qrv = MLD_QRV_DEFAULT;
	idev->mc_maxdelay = IGMP6_UNSOLICITED_IVAL;
	idev->mc_v1_seen = 0;
	write_unlock_bh(&idev->lock);

	/* Add all-nodes address. */
	ipv6_addr_all_nodes(&maddr);
	ipv6_dev_mc_inc(idev->dev, &maddr);
}

/*
 *	Device is about to be destroyed: clean up.
 */

void ipv6_mc_destroy_dev(struct inet6_dev *idev)
{
	struct ifmcaddr6 *i;
	struct in6_addr maddr;

	/* Deactivate timers */
	ipv6_mc_down(idev);

	/* Delete all-nodes address. */
	ipv6_addr_all_nodes(&maddr);

	/* We cannot call ipv6_dev_mc_dec() directly, our caller in
	 * addrconf.c has NULL'd out dev->ip6_ptr so in6_dev_get() will
	 * fail.
	 */
	__ipv6_dev_mc_dec(idev->dev, idev, &maddr);

	write_lock_bh(&idev->lock);
	while ((i = idev->mc_list) != NULL) {
		idev->mc_list = i->next;
		write_unlock_bh(&idev->lock);

		igmp6_group_dropped(i);
		ma_put(i);

		write_lock_bh(&idev->lock);
	}
	write_unlock_bh(&idev->lock);
}

#ifdef CONFIG_PROC_FS
static int igmp6_read_proc(char *buffer, char **start, off_t offset,
			   int length, int *eof, void *data)
{
	off_t pos=0, begin=0;
	struct ifmcaddr6 *im;
	int len=0;
	struct net_device *dev;
	
	read_lock(&dev_base_lock);
	for (dev = dev_base; dev; dev = dev->next) {
		struct inet6_dev *idev;

		if ((idev = in6_dev_get(dev)) == NULL)
			continue;

		read_lock_bh(&idev->lock);
		for (im = idev->mc_list; im; im = im->next) {
			int i;

			len += sprintf(buffer+len,"%-4d %-15s ", dev->ifindex, dev->name);

			for (i=0; i<16; i++)
				len += sprintf(buffer+len, "%02x", im->mca_addr.s6_addr[i]);

			len+=sprintf(buffer+len,
				     " %5d %08X %ld\n",
				     im->mca_users,
				     im->mca_flags,
				     (im->mca_flags&MAF_TIMER_RUNNING) ? im->mca_timer.expires-jiffies : 0);

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
	*eof = 1;

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

static int ip6_mcf_read_proc(char *buffer, char **start, off_t offset,
			   int length, int *eof, void *data)
{
	off_t pos=0, begin=0;
	int len=0;
	int first=1;
	struct net_device *dev;
	
	read_lock(&dev_base_lock);
	for (dev=dev_base; dev; dev=dev->next) {
		struct inet6_dev *idev = in6_dev_get(dev);
		struct ifmcaddr6 *imc;

		if (idev == NULL)
			continue;

		read_lock_bh(&idev->lock);

		for (imc=idev->mc_list; imc; imc=imc->next) {
			struct ip6_sf_list *psf;
			unsigned long i;

			spin_lock_bh(&imc->mca_lock);
			for (psf=imc->mca_sources; psf; psf=psf->sf_next) {
				if (first) {
					len += sprintf(buffer+len, "%3s %6s "
						"%32s %32s %6s %6s\n", "Idx",
						"Device", "Multicast Address",
						"Source Address", "INC", "EXC");
					first = 0;
				}
				len += sprintf(buffer+len,"%3d %6.6s ",
					dev->ifindex, dev->name);

				for (i=0; i<16; i++)
					len += sprintf(buffer+len, "%02x",
						imc->mca_addr.s6_addr[i]);
				buffer[len++] = ' ';
				for (i=0; i<16; i++)
					len += sprintf(buffer+len, "%02x",
						psf->sf_addr.s6_addr[i]);
				len += sprintf(buffer+len, " %6lu %6lu\n",
					psf->sf_count[MCAST_INCLUDE],
					psf->sf_count[MCAST_EXCLUDE]);
				pos = begin+len;
				if (pos < offset) {
					len=0;
					begin=pos;
				}
				if (pos > offset+length) {
					spin_unlock_bh(&imc->mca_lock);
					read_unlock_bh(&idev->lock);
					in6_dev_put(idev);
					goto done;
				}
			}
			spin_unlock_bh(&imc->mca_lock);
		}
		read_unlock_bh(&idev->lock);
		in6_dev_put(idev);
	}
	*eof = 1;

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

int __init igmp6_init(struct net_proto_family *ops)
{
	struct sock *sk;
	int err;

	igmp6_socket = sock_alloc();
	if (igmp6_socket == NULL) {
		printk(KERN_ERR
		       "Failed to create the IGMP6 control socket.\n");
		return -1;
	}
	igmp6_socket->inode->i_uid = 0;
	igmp6_socket->inode->i_gid = 0;
	igmp6_socket->type = SOCK_RAW;

	if((err = ops->create(igmp6_socket, IPPROTO_ICMPV6)) < 0) {
		printk(KERN_DEBUG 
		       "Failed to initialize the IGMP6 control socket (err %d).\n",
		       err);
		sock_release(igmp6_socket);
		igmp6_socket = NULL; /* For safety. */
		return err;
	}

	sk = igmp6_socket->sk;
	sk->allocation = GFP_ATOMIC;
	sk->prot->unhash(sk);

	sk->net_pinfo.af_inet6.hop_limit = 1;
#ifdef CONFIG_PROC_FS
	create_proc_read_entry("net/igmp6", 0, 0, igmp6_read_proc, NULL);
	create_proc_read_entry("net/mcfilter6", 0, 0, ip6_mcf_read_proc, NULL);
#endif

	return 0;
}

void igmp6_cleanup(void)
{
	sock_release(igmp6_socket);
	igmp6_socket = NULL; /* for safety */
#ifdef CONFIG_PROC_FS
	remove_proc_entry("net/igmp6", 0);
#endif
}
