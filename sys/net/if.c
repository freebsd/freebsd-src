/*
 * Copyright (c) 1980, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)if.c	8.5 (Berkeley) 1/9/95
 * $FreeBSD$
 */

#include "opt_compat.h"
#include "opt_inet6.h"
#include "opt_inet.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/kernel.h>
#include <sys/sockio.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/jail.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/radix.h>
#include <net/route.h>

#if defined(INET) || defined(INET6)
/*XXX*/
#include <netinet/in.h>
#include <netinet/in_var.h>
#ifdef INET6
#include <netinet6/in6_var.h>
#include <netinet6/in6_ifattach.h>
#endif
#endif
#ifdef INET
#include <netinet/if_ether.h>
#endif

static int	ifconf(u_long, caddr_t);
static void	if_grow(void);
static void	if_init(void *);
static void	if_check(void *);
static int	if_findindex(struct ifnet *);
static void	if_qflush(struct ifqueue *);
static void	if_slowtimo(void *);
static void	link_rtrequest(int, struct rtentry *, struct rt_addrinfo *);
static int	if_rtdel(struct radix_node *, void *);
static struct	if_clone *if_clone_lookup(const char *, int *);
static int	if_clone_list(struct if_clonereq *);
static int	ifhwioctl(u_long, struct ifnet *, caddr_t, struct thread *);
#ifdef INET6
/*
 * XXX: declare here to avoid to include many inet6 related files..
 * should be more generalized?
 */
extern void	nd6_setmtu(struct ifnet *);
#endif

int	if_index = 0;
struct	ifindex_entry *ifindex_table = NULL;
int	ifqmaxlen = IFQ_MAXLEN;
struct	ifnethead ifnet;	/* depend on static init XXX */
int	if_cloners_count;
LIST_HEAD(, if_clone) if_cloners = LIST_HEAD_INITIALIZER(if_cloners);

static int	if_indexlim = 8;
static struct	klist ifklist;

static void	filt_netdetach(struct knote *kn);
static int	filt_netdev(struct knote *kn, long hint);

static struct filterops netdev_filtops =
    { 1, NULL, filt_netdetach, filt_netdev };

/*
 * System initialization
 */
SYSINIT(interfaces, SI_SUB_INIT_IF, SI_ORDER_FIRST, if_init, NULL)
SYSINIT(interface_check, SI_SUB_PROTO_IF, SI_ORDER_FIRST, if_check, NULL)

MALLOC_DEFINE(M_IFADDR, "ifaddr", "interface address");
MALLOC_DEFINE(M_IFMADDR, "ether_multi", "link-level multicast address");
MALLOC_DEFINE(M_CLONE, "clone", "interface cloning framework");

#define CDEV_MAJOR	165

static d_open_t		netopen;
static d_close_t	netclose;
static d_ioctl_t	netioctl;
static d_kqfilter_t	netkqfilter;

static struct cdevsw net_cdevsw = {
	/* open */	netopen,
	/* close */	netclose,
	/* read */	noread,
	/* write */	nowrite,
	/* ioctl */	netioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"net",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_KQFILTER,
	/* kqfilter */	netkqfilter,
};

static int
netopen(dev_t dev, int flag, int mode, struct thread *td)
{
	return (0);
}

static int
netclose(dev_t dev, int flags, int fmt, struct thread *td)
{
	return (0);
}

static int
netioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	struct ifnet *ifp;
	int error, idx;

	/* only support interface specific ioctls */
	if (IOCGROUP(cmd) != 'i')
		return (EOPNOTSUPP);
	idx = minor(dev);
	if (idx == 0) {
		/*
		 * special network device, not interface.
		 */
		if (cmd == SIOCGIFCONF)
			return (ifconf(cmd, data));	/* XXX remove cmd */
		return (EOPNOTSUPP);
	}

	ifp = ifnet_byindex(idx);
	if (ifp == NULL)
		return (ENXIO);

	error = ifhwioctl(cmd, ifp, data, td);
	if (error == ENOIOCTL)
		error = EOPNOTSUPP;
	return (error);
}

static int
netkqfilter(dev_t dev, struct knote *kn)
{
	struct klist *klist;
	struct ifnet *ifp;
	int idx;

	idx = minor(dev);
	if (idx == 0) {
		klist = &ifklist;
	} else {
		ifp = ifnet_byindex(idx);
		if (ifp == NULL)
			return (1);
		klist = &ifp->if_klist;
	}

	switch (kn->kn_filter) {
	case EVFILT_NETDEV:
		kn->kn_fop = &netdev_filtops;
		break;
	default:
		return (1);
	}

	kn->kn_hook = (caddr_t)klist;

	/* XXX locking? */
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);

	return (0);
}

static void
filt_netdetach(struct knote *kn)
{
	struct klist *klist = (struct klist *)kn->kn_hook;

	if (kn->kn_status & KN_DETACHED)
		return;
	SLIST_REMOVE(klist, kn, knote, kn_selnext);
}

static int
filt_netdev(struct knote *kn, long hint)
{

	/*
	 * Currently NOTE_EXIT is abused to indicate device detach.
	 */
	if (hint == NOTE_EXIT) {
		kn->kn_data = NOTE_LINKINV;
                kn->kn_status |= KN_DETACHED;
                kn->kn_flags |= (EV_EOF | EV_ONESHOT); 
                return (1);
        }
	kn->kn_data = hint;			/* current status */
	if (kn->kn_sfflags & hint)
		kn->kn_fflags |= hint;
	return (kn->kn_fflags != 0);
}

/*
 * Network interface utility routines.
 *
 * Routines with ifa_ifwith* names take sockaddr *'s as
 * parameters.
 */
/* ARGSUSED*/
static void
if_init(dummy)
	void *dummy;
{

	TAILQ_INIT(&ifnet);
	SLIST_INIT(&ifklist);
	if_grow();				/* create initial table */
	ifdev_byindex(0) = make_dev(&net_cdevsw, 0,
	    UID_ROOT, GID_WHEEL, 0600, "network");
}

static void
if_grow(void)
{
	u_int n;
	struct ifindex_entry *e;

	if_indexlim <<= 1;
	n = if_indexlim * sizeof(*e);
	e = malloc(n, M_IFADDR, M_WAITOK | M_ZERO);
	if (ifindex_table != NULL) {
		memcpy((caddr_t)e, (caddr_t)ifindex_table, n/2);
		free((caddr_t)ifindex_table, M_IFADDR);
	}
	ifindex_table = e;
}

/* ARGSUSED*/
static void
if_check(dummy)
	void *dummy;
{
	struct ifnet *ifp;
	int s;

	s = splimp();
	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		if (ifp->if_snd.ifq_maxlen == 0) {
			printf("%s%d XXX: driver didn't set ifq_maxlen\n",
			    ifp->if_name, ifp->if_unit);
			ifp->if_snd.ifq_maxlen = ifqmaxlen;
		}
		if (!mtx_initialized(&ifp->if_snd.ifq_mtx)) {
			printf("%s%d XXX: driver didn't initialize queue mtx\n",
			    ifp->if_name, ifp->if_unit);
			mtx_init(&ifp->if_snd.ifq_mtx, "unknown",
			    MTX_NETWORK_LOCK, MTX_DEF);
		}
	}
	splx(s);
	if_slowtimo(0);
}

static int
if_findindex(struct ifnet *ifp)
{
	int i, unit;
	char eaddr[18], devname[32];
	const char *name, *p;

	switch (ifp->if_type) {
	case IFT_ETHER:			/* these types use struct arpcom */
	case IFT_FDDI:
	case IFT_XETHER:
	case IFT_ISO88025:
	case IFT_L2VLAN:
		snprintf(eaddr, 18, "%6D", 
		    ((struct arpcom *)ifp->if_softc)->ac_enaddr, ":");
		break;
	default:
		eaddr[0] = '\0';
		break;
	}
	snprintf(devname, 32, "%s%d", ifp->if_name, ifp->if_unit);
	name = net_cdevsw.d_name;
	i = 0;
	while ((resource_find_dev(&i, name, &unit, NULL, NULL)) == 0) {
		if (resource_string_value(name, unit, "ether", &p) == 0)
			if (strcmp(p, eaddr) == 0)
				goto found;
		if (resource_string_value(name, unit, "dev", &p) == 0)
			if (strcmp(p, devname) == 0)
				goto found;
	}
	unit = 0;
found:
	if (unit != 0) {
		if (ifaddr_byindex(unit) == NULL)
			return (unit);
		printf("%s%d in use, cannot hardwire it to %s.\n",
		    name, unit, devname);
	}
	for (unit = 1; ; unit++) {
		if (unit <= if_index && ifaddr_byindex(unit) != NULL)
			continue;
		if (resource_string_value(name, unit, "ether", &p) == 0 ||
		    resource_string_value(name, unit, "dev", &p) == 0)
			continue;
		break;
	}
	return (unit);
}

/*
 * Attach an interface to the
 * list of "active" interfaces.
 */
void
if_attach(ifp)
	struct ifnet *ifp;
{
	unsigned socksize, ifasize;
	int namelen, masklen;
	char workbuf[64];
	register struct sockaddr_dl *sdl;
	register struct ifaddr *ifa;

	TAILQ_INSERT_TAIL(&ifnet, ifp, if_link);
	/*
	 * XXX -
	 * The old code would work if the interface passed a pre-existing
	 * chain of ifaddrs to this code.  We don't trust our callers to
	 * properly initialize the tailq, however, so we no longer allow
	 * this unlikely case.
	 */
	TAILQ_INIT(&ifp->if_addrhead);
	TAILQ_INIT(&ifp->if_prefixhead);
	TAILQ_INIT(&ifp->if_multiaddrs);
	SLIST_INIT(&ifp->if_klist);
	getmicrotime(&ifp->if_lastchange);

#ifdef MAC
	mac_init_ifnet(ifp);
	mac_create_ifnet(ifp);
#endif

	ifp->if_index = if_findindex(ifp);
	if (ifp->if_index > if_index)
		if_index = ifp->if_index;
	if (if_index >= if_indexlim)
		if_grow();

	ifnet_byindex(ifp->if_index) = ifp;
	ifdev_byindex(ifp->if_index) = make_dev(&net_cdevsw, ifp->if_index,
	    UID_ROOT, GID_WHEEL, 0600, "%s/%s%d",
	    net_cdevsw.d_name, ifp->if_name, ifp->if_unit);
	make_dev_alias(ifdev_byindex(ifp->if_index), "%s%d",
	    net_cdevsw.d_name, ifp->if_index);

	mtx_init(&ifp->if_snd.ifq_mtx, ifp->if_name, "if send queue", MTX_DEF);

	/*
	 * create a Link Level name for this device
	 */
	namelen = snprintf(workbuf, sizeof(workbuf),
	    "%s%d", ifp->if_name, ifp->if_unit);
#define _offsetof(t, m) ((int)((caddr_t)&((t *)0)->m))
	masklen = _offsetof(struct sockaddr_dl, sdl_data[0]) + namelen;
	socksize = masklen + ifp->if_addrlen;
#define ROUNDUP(a) (1 + (((a) - 1) | (sizeof(long) - 1)))
	if (socksize < sizeof(*sdl))
		socksize = sizeof(*sdl);
	socksize = ROUNDUP(socksize);
	ifasize = sizeof(*ifa) + 2 * socksize;
	ifa = (struct ifaddr *)malloc(ifasize, M_IFADDR, M_WAITOK | M_ZERO);
	if (ifa) {
		sdl = (struct sockaddr_dl *)(ifa + 1);
		sdl->sdl_len = socksize;
		sdl->sdl_family = AF_LINK;
		bcopy(workbuf, sdl->sdl_data, namelen);
		sdl->sdl_nlen = namelen;
		sdl->sdl_index = ifp->if_index;
		sdl->sdl_type = ifp->if_type;
		ifaddr_byindex(ifp->if_index) = ifa;
		ifa->ifa_ifp = ifp;
		ifa->ifa_rtrequest = link_rtrequest;
		ifa->ifa_addr = (struct sockaddr *)sdl;
		sdl = (struct sockaddr_dl *)(socksize + (caddr_t)sdl);
		ifa->ifa_netmask = (struct sockaddr *)sdl;
		sdl->sdl_len = masklen;
		while (namelen != 0)
			sdl->sdl_data[--namelen] = 0xff;
		TAILQ_INSERT_HEAD(&ifp->if_addrhead, ifa, ifa_link);
	}
	ifp->if_broadcastaddr = 0; /* reliably crash if used uninitialized */

	/* Announce the interface. */
	rt_ifannouncemsg(ifp, IFAN_ARRIVAL);
}

/*
 * Detach an interface, removing it from the
 * list of "active" interfaces.
 */
void
if_detach(ifp)
	struct ifnet *ifp;
{
	struct ifaddr *ifa;
	struct radix_node_head	*rnh;
	int s;
	int i;

	/*
	 * Remove routes and flush queues.
	 */
	s = splnet();
	if_down(ifp);

	/*
	 * Remove address from ifindex_table[] and maybe decrement if_index.
	 * Clean up all addresses.
	 */
	ifaddr_byindex(ifp->if_index) = NULL;
	revoke_and_destroy_dev(ifdev_byindex(ifp->if_index));
	ifdev_byindex(ifp->if_index) = NULL;

	while (if_index > 0 && ifaddr_byindex(if_index) == NULL)
		if_index--;

	for (ifa = TAILQ_FIRST(&ifp->if_addrhead); ifa;
	     ifa = TAILQ_FIRST(&ifp->if_addrhead)) {
#ifdef INET
		/* XXX: Ugly!! ad hoc just for INET */
		if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
			struct ifaliasreq ifr;

			bzero(&ifr, sizeof(ifr));
			ifr.ifra_addr = *ifa->ifa_addr;
			if (ifa->ifa_dstaddr)
				ifr.ifra_broadaddr = *ifa->ifa_dstaddr;
			if (in_control(NULL, SIOCDIFADDR, (caddr_t)&ifr, ifp,
			    NULL) == 0)
				continue;
		}
#endif /* INET */
#ifdef INET6
		if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET6) {
			in6_purgeaddr(ifa);
			/* ifp_addrhead is already updated */
			continue;
		}
#endif /* INET6 */
		TAILQ_REMOVE(&ifp->if_addrhead, ifa, ifa_link);
		IFAFREE(ifa);
	}

#ifdef INET6
	/*
	 * Remove all IPv6 kernel structs related to ifp.  This should be done
	 * before removing routing entries below, since IPv6 interface direct
	 * routes are expected to be removed by the IPv6-specific kernel API.
	 * Otherwise, the kernel will detect some inconsistency and bark it.
	 */
	in6_ifdetach(ifp);
#endif

	/*
	 * Delete all remaining routes using this interface
	 * Unfortuneatly the only way to do this is to slog through
	 * the entire routing table looking for routes which point
	 * to this interface...oh well...
	 */
	for (i = 1; i <= AF_MAX; i++) {
		if ((rnh = rt_tables[i]) == NULL)
			continue;
		(void) rnh->rnh_walktree(rnh, if_rtdel, ifp);
	}

	/* Announce that the interface is gone. */
	rt_ifannouncemsg(ifp, IFAN_DEPARTURE);

#ifdef MAC
	mac_destroy_ifnet(ifp);
#endif /* MAC */
	KNOTE(&ifp->if_klist, NOTE_EXIT);
	TAILQ_REMOVE(&ifnet, ifp, if_link);
	mtx_destroy(&ifp->if_snd.ifq_mtx);
	splx(s);
}

/*
 * Delete Routes for a Network Interface
 * 
 * Called for each routing entry via the rnh->rnh_walktree() call above
 * to delete all route entries referencing a detaching network interface.
 *
 * Arguments:
 *	rn	pointer to node in the routing table
 *	arg	argument passed to rnh->rnh_walktree() - detaching interface
 *
 * Returns:
 *	0	successful
 *	errno	failed - reason indicated
 *
 */
static int
if_rtdel(rn, arg)
	struct radix_node	*rn;
	void			*arg;
{
	struct rtentry	*rt = (struct rtentry *)rn;
	struct ifnet	*ifp = arg;
	int		err;

	if (rt->rt_ifp == ifp) {

		/*
		 * Protect (sorta) against walktree recursion problems
		 * with cloned routes
		 */
		if ((rt->rt_flags & RTF_UP) == 0)
			return (0);

		err = rtrequest(RTM_DELETE, rt_key(rt), rt->rt_gateway,
				rt_mask(rt), rt->rt_flags,
				(struct rtentry **) NULL);
		if (err) {
			log(LOG_WARNING, "if_rtdel: error %d\n", err);
		}
	}

	return (0);
}

/*
 * Create a clone network interface.
 */
int
if_clone_create(name, len)
	char *name;
	int len;
{
	struct if_clone *ifc;
	char *dp;
	int wildcard, bytoff, bitoff;
	int unit;
	int err;

	ifc = if_clone_lookup(name, &unit);
	if (ifc == NULL)
		return (EINVAL);

	if (ifunit(name) != NULL)
		return (EEXIST);

	bytoff = bitoff = 0;
	wildcard = (unit < 0);
	/*
	 * Find a free unit if none was given.
	 */ 
	if (wildcard) {
		while ((bytoff < ifc->ifc_bmlen)
		    && (ifc->ifc_units[bytoff] == 0xff))
			bytoff++;
		if (bytoff >= ifc->ifc_bmlen)
			return (ENOSPC);
		while ((ifc->ifc_units[bytoff] & (1 << bitoff)) != 0)
			bitoff++;
		unit = (bytoff << 3) + bitoff;
	}

	if (unit > ifc->ifc_maxunit)
		return (ENXIO);

	err = (*ifc->ifc_create)(ifc, unit);
	if (err != 0)
		return (err);

	if (!wildcard) {
		bytoff = unit >> 3;
		bitoff = unit - (bytoff << 3);
	}

	/*
	 * Allocate the unit in the bitmap.
	 */
	KASSERT((ifc->ifc_units[bytoff] & (1 << bitoff)) == 0,
	    ("%s: bit is already set", __func__));
	ifc->ifc_units[bytoff] |= (1 << bitoff);

	/* In the wildcard case, we need to update the name. */
	if (wildcard) {
		for (dp = name; *dp != '\0'; dp++);
		if (snprintf(dp, len - (dp-name), "%d", unit) >
		    len - (dp-name) - 1) {
			/*
			 * This can only be a programmer error and
			 * there's no straightforward way to recover if
			 * it happens.
			 */
			panic("if_clone_create(): interface name too long");
		}
			
	}

	return (0);
}

/*
 * Destroy a clone network interface.
 */
int
if_clone_destroy(name)
	const char *name;
{
	struct if_clone *ifc;
	struct ifnet *ifp;
	int bytoff, bitoff;
	int unit;

	ifc = if_clone_lookup(name, &unit);
	if (ifc == NULL)
		return (EINVAL);

	if (unit < ifc->ifc_minifs)
		return (EINVAL);

	ifp = ifunit(name);
	if (ifp == NULL)
		return (ENXIO);

	if (ifc->ifc_destroy == NULL)
		return (EOPNOTSUPP);

	(*ifc->ifc_destroy)(ifp);

	/*
	 * Compute offset in the bitmap and deallocate the unit.
	 */
	bytoff = unit >> 3;
	bitoff = unit - (bytoff << 3);
	KASSERT((ifc->ifc_units[bytoff] & (1 << bitoff)) != 0,
	    ("%s: bit is already cleared", __func__));
	ifc->ifc_units[bytoff] &= ~(1 << bitoff);
	return (0);
}

/*
 * Look up a network interface cloner.
 */
static struct if_clone *
if_clone_lookup(name, unitp)
	const char *name;
	int *unitp;
{
	struct if_clone *ifc;
	const char *cp;
	int i;

	for (ifc = LIST_FIRST(&if_cloners); ifc != NULL;) {
		for (cp = name, i = 0; i < ifc->ifc_namelen; i++, cp++) {
			if (ifc->ifc_name[i] != *cp)
				goto next_ifc;
		}
		goto found_name;
 next_ifc:
		ifc = LIST_NEXT(ifc, ifc_list);
	}

	/* No match. */
	return ((struct if_clone *)NULL);

 found_name:
	if (*cp == '\0') {
		i = -1;
	} else {
		for (i = 0; *cp != '\0'; cp++) {
			if (*cp < '0' || *cp > '9') {
				/* Bogus unit number. */
				return (NULL);
			}
			i = (i * 10) + (*cp - '0');
		}
	}

	if (unitp != NULL)
		*unitp = i;
	return (ifc);
}

/*
 * Register a network interface cloner.
 */
void
if_clone_attach(ifc)
	struct if_clone *ifc;
{
	int bytoff, bitoff;
	int err;
	int len, maxclone;
	int unit;

	KASSERT(ifc->ifc_minifs - 1 <= ifc->ifc_maxunit,
	    ("%s: %s requested more units then allowed (%d > %d)",
	    __func__, ifc->ifc_name, ifc->ifc_minifs,
	    ifc->ifc_maxunit + 1));
	/*
	 * Compute bitmap size and allocate it.
	 */
	maxclone = ifc->ifc_maxunit + 1;
	len = maxclone >> 3;
	if ((len << 3) < maxclone)
		len++;
	ifc->ifc_units = malloc(len, M_CLONE, M_WAITOK | M_ZERO);
	ifc->ifc_bmlen = len;

	LIST_INSERT_HEAD(&if_cloners, ifc, ifc_list);
	if_cloners_count++;

	for (unit = 0; unit < ifc->ifc_minifs; unit++) {
		err = (*ifc->ifc_create)(ifc, unit);
		KASSERT(err == 0,
		    ("%s: failed to create required interface %s%d",
		    __func__, ifc->ifc_name, unit));

		/* Allocate the unit in the bitmap. */
		bytoff = unit >> 3;
		bitoff = unit - (bytoff << 3);
		ifc->ifc_units[bytoff] |= (1 << bitoff);
	}
}

/*
 * Unregister a network interface cloner.
 */
void
if_clone_detach(ifc)
	struct if_clone *ifc;
{

	LIST_REMOVE(ifc, ifc_list);
	free(ifc->ifc_units, M_CLONE);
	if_cloners_count--;
}

/*
 * Provide list of interface cloners to userspace.
 */
static int
if_clone_list(ifcr)
	struct if_clonereq *ifcr;
{
	char outbuf[IFNAMSIZ], *dst;
	struct if_clone *ifc;
	int count, error = 0;

	ifcr->ifcr_total = if_cloners_count;
	if ((dst = ifcr->ifcr_buffer) == NULL) {
		/* Just asking how many there are. */
		return (0);
	}

	if (ifcr->ifcr_count < 0)
		return (EINVAL);

	count = (if_cloners_count < ifcr->ifcr_count) ?
	    if_cloners_count : ifcr->ifcr_count;

	for (ifc = LIST_FIRST(&if_cloners); ifc != NULL && count != 0;
	     ifc = LIST_NEXT(ifc, ifc_list), count--, dst += IFNAMSIZ) {
		strncpy(outbuf, ifc->ifc_name, IFNAMSIZ);
		outbuf[IFNAMSIZ - 1] = '\0';	/* sanity */
		error = copyout(outbuf, dst, IFNAMSIZ);
		if (error)
			break;
	}

	return (error);
}

/*
 * Locate an interface based on a complete address.
 */
/*ARGSUSED*/
struct ifaddr *
ifa_ifwithaddr(addr)
	struct sockaddr *addr;
{
	struct ifnet *ifp;
	struct ifaddr *ifa;

#define	equal(a1, a2) \
  (bcmp((caddr_t)(a1), (caddr_t)(a2), ((struct sockaddr *)(a1))->sa_len) == 0)
	TAILQ_FOREACH(ifp, &ifnet, if_link)
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			if (ifa->ifa_addr->sa_family != addr->sa_family)
				continue;
			if (equal(addr, ifa->ifa_addr))
				goto done;
			/* IP6 doesn't have broadcast */
			if ((ifp->if_flags & IFF_BROADCAST) &&
			    ifa->ifa_broadaddr &&
			    ifa->ifa_broadaddr->sa_len != 0 &&
			    equal(ifa->ifa_broadaddr, addr))
				goto done;
		}
	ifa = NULL;
done:
	return (ifa);
}

/*
 * Locate the point to point interface with a given destination address.
 */
/*ARGSUSED*/
struct ifaddr *
ifa_ifwithdstaddr(addr)
	struct sockaddr *addr;
{
	struct ifnet *ifp;
	struct ifaddr *ifa;

	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			continue;
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			if (ifa->ifa_addr->sa_family != addr->sa_family)
				continue;
			if (ifa->ifa_dstaddr && equal(addr, ifa->ifa_dstaddr))
				goto done;
		}
	}
	ifa = NULL;
done:
	return (ifa);
}

/*
 * Find an interface on a specific network.  If many, choice
 * is most specific found.
 */
struct ifaddr *
ifa_ifwithnet(addr)
	struct sockaddr *addr;
{
	register struct ifnet *ifp;
	register struct ifaddr *ifa;
	struct ifaddr *ifa_maybe = (struct ifaddr *) 0;
	u_int af = addr->sa_family;
	char *addr_data = addr->sa_data, *cplim;

	/*
	 * AF_LINK addresses can be looked up directly by their index number,
	 * so do that if we can.
	 */
	if (af == AF_LINK) {
	    register struct sockaddr_dl *sdl = (struct sockaddr_dl *)addr;
	    if (sdl->sdl_index && sdl->sdl_index <= if_index)
		return (ifaddr_byindex(sdl->sdl_index));
	}

	/*
	 * Scan though each interface, looking for ones that have
	 * addresses in this address family.
	 */
	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			register char *cp, *cp2, *cp3;

			if (ifa->ifa_addr->sa_family != af)
next:				continue;
			if (af == AF_INET && ifp->if_flags & IFF_POINTOPOINT) {
				/*
				 * This is a bit broken as it doesn't
				 * take into account that the remote end may
				 * be a single node in the network we are
				 * looking for.
				 * The trouble is that we don't know the
				 * netmask for the remote end.
				 */
				if (ifa->ifa_dstaddr != 0
				    && equal(addr, ifa->ifa_dstaddr))
					goto done;
			} else {
				/*
				 * if we have a special address handler,
				 * then use it instead of the generic one.
				 */
	          		if (ifa->ifa_claim_addr) {
					if ((*ifa->ifa_claim_addr)(ifa, addr))
						goto done;
					continue;
				}

				/*
				 * Scan all the bits in the ifa's address.
				 * If a bit dissagrees with what we are
				 * looking for, mask it with the netmask
				 * to see if it really matters.
				 * (A byte at a time)
				 */
				if (ifa->ifa_netmask == 0)
					continue;
				cp = addr_data;
				cp2 = ifa->ifa_addr->sa_data;
				cp3 = ifa->ifa_netmask->sa_data;
				cplim = ifa->ifa_netmask->sa_len
					+ (char *)ifa->ifa_netmask;
				while (cp3 < cplim)
					if ((*cp++ ^ *cp2++) & *cp3++)
						goto next; /* next address! */
				/*
				 * If the netmask of what we just found
				 * is more specific than what we had before
				 * (if we had one) then remember the new one
				 * before continuing to search
				 * for an even better one.
				 */
				if (ifa_maybe == 0 ||
				    rn_refines((caddr_t)ifa->ifa_netmask,
				    (caddr_t)ifa_maybe->ifa_netmask))
					ifa_maybe = ifa;
			}
		}
	}
	ifa = ifa_maybe;
done:
	return (ifa);
}

/*
 * Find an interface address specific to an interface best matching
 * a given address.
 */
struct ifaddr *
ifaof_ifpforaddr(addr, ifp)
	struct sockaddr *addr;
	register struct ifnet *ifp;
{
	register struct ifaddr *ifa;
	register char *cp, *cp2, *cp3;
	register char *cplim;
	struct ifaddr *ifa_maybe = 0;
	u_int af = addr->sa_family;

	if (af >= AF_MAX)
		return (0);
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		if (ifa->ifa_addr->sa_family != af)
			continue;
		if (ifa_maybe == 0)
			ifa_maybe = ifa;
		if (ifa->ifa_netmask == 0) {
			if (equal(addr, ifa->ifa_addr) ||
			    (ifa->ifa_dstaddr && equal(addr, ifa->ifa_dstaddr)))
				goto done;
			continue;
		}
		if (ifp->if_flags & IFF_POINTOPOINT) {
			if (equal(addr, ifa->ifa_dstaddr))
				goto done;
		} else {
			cp = addr->sa_data;
			cp2 = ifa->ifa_addr->sa_data;
			cp3 = ifa->ifa_netmask->sa_data;
			cplim = ifa->ifa_netmask->sa_len + (char *)ifa->ifa_netmask;
			for (; cp3 < cplim; cp3++)
				if ((*cp++ ^ *cp2++) & *cp3)
					break;
			if (cp3 == cplim)
				goto done;
		}
	}
	ifa = ifa_maybe;
done:
	return (ifa);
}

#include <net/route.h>

/*
 * Default action when installing a route with a Link Level gateway.
 * Lookup an appropriate real ifa to point to.
 * This should be moved to /sys/net/link.c eventually.
 */
static void
link_rtrequest(cmd, rt, info)
	int cmd;
	register struct rtentry *rt;
	struct rt_addrinfo *info;
{
	register struct ifaddr *ifa;
	struct sockaddr *dst;
	struct ifnet *ifp;

	if (cmd != RTM_ADD || ((ifa = rt->rt_ifa) == 0) ||
	    ((ifp = ifa->ifa_ifp) == 0) || ((dst = rt_key(rt)) == 0))
		return;
	ifa = ifaof_ifpforaddr(dst, ifp);
	if (ifa) {
		IFAFREE(rt->rt_ifa);
		rt->rt_ifa = ifa;
		ifa->ifa_refcnt++;
		if (ifa->ifa_rtrequest && ifa->ifa_rtrequest != link_rtrequest)
			ifa->ifa_rtrequest(cmd, rt, info);
	}
}

/*
 * Mark an interface down and notify protocols of
 * the transition.
 * NOTE: must be called at splnet or eqivalent.
 */
void
if_unroute(ifp, flag, fam)
	register struct ifnet *ifp;
	int flag, fam;
{
	register struct ifaddr *ifa;

	ifp->if_flags &= ~flag;
	getmicrotime(&ifp->if_lastchange);
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link)
		if (fam == PF_UNSPEC || (fam == ifa->ifa_addr->sa_family))
			pfctlinput(PRC_IFDOWN, ifa->ifa_addr);
	if_qflush(&ifp->if_snd);
	rt_ifmsg(ifp);
}

/*
 * Mark an interface up and notify protocols of
 * the transition.
 * NOTE: must be called at splnet or eqivalent.
 */
void
if_route(ifp, flag, fam)
	register struct ifnet *ifp;
	int flag, fam;
{
	register struct ifaddr *ifa;

	ifp->if_flags |= flag;
	getmicrotime(&ifp->if_lastchange);
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link)
		if (fam == PF_UNSPEC || (fam == ifa->ifa_addr->sa_family))
			pfctlinput(PRC_IFUP, ifa->ifa_addr);
	rt_ifmsg(ifp);
#ifdef INET6
	in6_if_up(ifp);
#endif
}

/*
 * Mark an interface down and notify protocols of
 * the transition.
 * NOTE: must be called at splnet or eqivalent.
 */
void
if_down(ifp)
	register struct ifnet *ifp;
{

	if_unroute(ifp, IFF_UP, AF_UNSPEC);
}

/*
 * Mark an interface up and notify protocols of
 * the transition.
 * NOTE: must be called at splnet or eqivalent.
 */
void
if_up(ifp)
	register struct ifnet *ifp;
{

	if_route(ifp, IFF_UP, AF_UNSPEC);
}

/*
 * Flush an interface queue.
 */
static void
if_qflush(ifq)
	register struct ifqueue *ifq;
{
	register struct mbuf *m, *n;

	n = ifq->ifq_head;
	while ((m = n) != 0) {
		n = m->m_act;
		m_freem(m);
	}
	ifq->ifq_head = 0;
	ifq->ifq_tail = 0;
	ifq->ifq_len = 0;
}

/*
 * Handle interface watchdog timer routines.  Called
 * from softclock, we decrement timers (if set) and
 * call the appropriate interface routine on expiration.
 */
static void
if_slowtimo(arg)
	void *arg;
{
	register struct ifnet *ifp;
	int s = splimp();

	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		if (ifp->if_timer == 0 || --ifp->if_timer)
			continue;
		if (ifp->if_watchdog)
			(*ifp->if_watchdog)(ifp);
	}
	splx(s);
	timeout(if_slowtimo, (void *)0, hz / IFNET_SLOWHZ);
}

/*
 * Map interface name to
 * interface structure pointer.
 */
struct ifnet *
ifunit(const char *name)
{
	char namebuf[IFNAMSIZ + 1];
	struct ifnet *ifp;
	dev_t dev;

	/*
	 * Now search all the interfaces for this name/number
	 */

	/*
	 * XXX
	 * Devices should really be known as /dev/fooN, not /dev/net/fooN.
	 */
	snprintf(namebuf, IFNAMSIZ, "%s/%s", net_cdevsw.d_name, name);
	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		dev = ifdev_byindex(ifp->if_index);
		if (strcmp(devtoname(dev), namebuf) == 0)
			break;
		if (dev_named(dev, name))
			break;
	}
	return (ifp);
}

/*
 * Map interface name in a sockaddr_dl to
 * interface structure pointer.
 */
struct ifnet *
if_withname(sa)
	struct sockaddr *sa;
{
	char ifname[IFNAMSIZ+1];
	struct sockaddr_dl *sdl = (struct sockaddr_dl *)sa;

	if ( (sa->sa_family != AF_LINK) || (sdl->sdl_nlen == 0) ||
	     (sdl->sdl_nlen > IFNAMSIZ) )
		return NULL;

	/*
	 * ifunit wants a NUL-terminated string.  It may not be NUL-terminated
	 * in the sockaddr, and we don't want to change the caller's sockaddr
	 * (there might not be room to add the trailing NUL anyway), so we make
	 * a local copy that we know we can NUL-terminate safely.
	 */

	bcopy(sdl->sdl_data, ifname, sdl->sdl_nlen);
	ifname[sdl->sdl_nlen] = '\0';
	return ifunit(ifname);
}

/*
 * Hardware specific interface ioctls.
 */
static int
ifhwioctl(u_long cmd, struct ifnet *ifp, caddr_t data, struct thread *td)
{
	struct ifreq *ifr;
	struct ifstat *ifs;
	int error = 0;
	int new_flags;

	ifr = (struct ifreq *)data;
	switch (cmd) {
	case SIOCGIFINDEX:
		ifr->ifr_index = ifp->if_index;
		break;

	case SIOCGIFFLAGS:
		ifr->ifr_flags = ifp->if_flags & 0xffff;
		ifr->ifr_flagshigh = ifp->if_flags >> 16;
		break;

	case SIOCGIFCAP:
		ifr->ifr_reqcap = ifp->if_capabilities;
		ifr->ifr_curcap = ifp->if_capenable;
		break;

#ifdef MAC
	case SIOCGIFMAC:
		error = mac_ioctl_ifnet_get(td->td_proc->p_ucred, ifr, ifp);
		break;
#endif

	case SIOCGIFMETRIC:
		ifr->ifr_metric = ifp->if_metric;
		break;

	case SIOCGIFMTU:
		ifr->ifr_mtu = ifp->if_mtu;
		break;

	case SIOCGIFPHYS:
		ifr->ifr_phys = ifp->if_physical;
		break;

	case SIOCSIFFLAGS:
		error = suser(td);
		if (error)
			return (error);
		new_flags = (ifr->ifr_flags & 0xffff) |
		    (ifr->ifr_flagshigh << 16);
		if (ifp->if_flags & IFF_SMART) {
			/* Smart drivers twiddle their own routes */
		} else if (ifp->if_flags & IFF_UP &&
		    (new_flags & IFF_UP) == 0) {
			int s = splimp();
			if_down(ifp);
			splx(s);
		} else if (new_flags & IFF_UP &&
		    (ifp->if_flags & IFF_UP) == 0) {
			int s = splimp();
			if_up(ifp);
			splx(s);
		}
		ifp->if_flags = (ifp->if_flags & IFF_CANTCHANGE) |
			(new_flags &~ IFF_CANTCHANGE);
		if (new_flags & IFF_PPROMISC) {
			/* Permanently promiscuous mode requested */
			ifp->if_flags |= IFF_PROMISC;
		} else if (ifp->if_pcount == 0) {
			ifp->if_flags &= ~IFF_PROMISC;
		}
		if (ifp->if_ioctl)
			(void) (*ifp->if_ioctl)(ifp, cmd, data);
		getmicrotime(&ifp->if_lastchange);
		break;

	case SIOCSIFCAP:
		error = suser(td);
		if (error)
			return (error);
		if (ifr->ifr_reqcap & ~ifp->if_capabilities)
			return (EINVAL);
		(void) (*ifp->if_ioctl)(ifp, cmd, data);
		break;

#ifdef MAC
	case SIOCSIFMAC:
		error = mac_ioctl_ifnet_set(td->td_proc->p_ucred, ifr, ifp);
		break;
#endif

	case SIOCSIFMETRIC:
		error = suser(td);
		if (error)
			return (error);
		ifp->if_metric = ifr->ifr_metric;
		getmicrotime(&ifp->if_lastchange);
		break;

	case SIOCSIFPHYS:
		error = suser(td);
		if (error)
			return error;
		if (!ifp->if_ioctl)
		        return EOPNOTSUPP;
		error = (*ifp->if_ioctl)(ifp, cmd, data);
		if (error == 0)
			getmicrotime(&ifp->if_lastchange);
		return(error);

	case SIOCSIFMTU:
	{
		u_long oldmtu = ifp->if_mtu;

		error = suser(td);
		if (error)
			return (error);
		if (ifr->ifr_mtu < IF_MINMTU || ifr->ifr_mtu > IF_MAXMTU)
			return (EINVAL);
		if (ifp->if_ioctl == NULL)
			return (EOPNOTSUPP);
		error = (*ifp->if_ioctl)(ifp, cmd, data);
		if (error == 0) {
			getmicrotime(&ifp->if_lastchange);
			rt_ifmsg(ifp);
		}
		/*
		 * If the link MTU changed, do network layer specific procedure.
		 */
		if (ifp->if_mtu != oldmtu) {
#ifdef INET6
			nd6_setmtu(ifp);
#endif
		}
		break;
	}

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = suser(td);
		if (error)
			return (error);

		/* Don't allow group membership on non-multicast interfaces. */
		if ((ifp->if_flags & IFF_MULTICAST) == 0)
			return (EOPNOTSUPP);

		/* Don't let users screw up protocols' entries. */
		if (ifr->ifr_addr.sa_family != AF_LINK)
			return (EINVAL);

		if (cmd == SIOCADDMULTI) {
			struct ifmultiaddr *ifma;
			error = if_addmulti(ifp, &ifr->ifr_addr, &ifma);
		} else {
			error = if_delmulti(ifp, &ifr->ifr_addr);
		}
		if (error == 0)
			getmicrotime(&ifp->if_lastchange);
		break;

	case SIOCSIFPHYADDR:
	case SIOCDIFPHYADDR:
#ifdef INET6
	case SIOCSIFPHYADDR_IN6:
#endif
	case SIOCSLIFPHYADDR:
        case SIOCSIFMEDIA:
	case SIOCSIFGENERIC:
		error = suser(td);
		if (error)
			return (error);
		if (ifp->if_ioctl == NULL)
			return (EOPNOTSUPP);
		error = (*ifp->if_ioctl)(ifp, cmd, data);
		if (error == 0)
			getmicrotime(&ifp->if_lastchange);
		break;

	case SIOCGIFSTATUS:
		ifs = (struct ifstat *)data;
		ifs->ascii[0] = '\0';
		
	case SIOCGIFPSRCADDR:
	case SIOCGIFPDSTADDR:
	case SIOCGLIFPHYADDR:
	case SIOCGIFMEDIA:
	case SIOCGIFGENERIC:
		if (ifp->if_ioctl == 0)
			return (EOPNOTSUPP);
		error = (*ifp->if_ioctl)(ifp, cmd, data);
		break;

	case SIOCSIFLLADDR:
		error = suser(td);
		if (error)
			return (error);
		error = if_setlladdr(ifp,
		    ifr->ifr_addr.sa_data, ifr->ifr_addr.sa_len);
		break;

	default:
		error = ENOIOCTL;
		break;
	}
	return (error);
}

/*
 * Interface ioctls.
 */
int
ifioctl(so, cmd, data, td)
	struct socket *so;
	u_long cmd;
	caddr_t data;
	struct thread *td;
{
	struct ifnet *ifp;
	struct ifreq *ifr;
	int error;
	int oif_flags;

	switch (cmd) {
	case SIOCGIFCONF:
	case OSIOCGIFCONF:
		return (ifconf(cmd, data));
	}
	ifr = (struct ifreq *)data;

	switch (cmd) {
	case SIOCIFCREATE:
	case SIOCIFDESTROY:
		if ((error = suser(td)) != 0)
			return (error);
		return ((cmd == SIOCIFCREATE) ?
			if_clone_create(ifr->ifr_name, sizeof(ifr->ifr_name)) :
			if_clone_destroy(ifr->ifr_name));
	
	case SIOCIFGCLONERS:
		return (if_clone_list((struct if_clonereq *)data));
	}

	ifp = ifunit(ifr->ifr_name);
	if (ifp == 0)
		return (ENXIO);

	error = ifhwioctl(cmd, ifp, data, td);
	if (error != ENOIOCTL)
		return (error);

	oif_flags = ifp->if_flags;
	if (so->so_proto == 0)
		return (EOPNOTSUPP);
#ifndef COMPAT_43
	error = ((*so->so_proto->pr_usrreqs->pru_control)(so, cmd,
								 data,
								 ifp, td));
#else
	{
		int ocmd = cmd;

		switch (cmd) {

		case SIOCSIFDSTADDR:
		case SIOCSIFADDR:
		case SIOCSIFBRDADDR:
		case SIOCSIFNETMASK:
#if BYTE_ORDER != BIG_ENDIAN
			if (ifr->ifr_addr.sa_family == 0 &&
			    ifr->ifr_addr.sa_len < 16) {
				ifr->ifr_addr.sa_family = ifr->ifr_addr.sa_len;
				ifr->ifr_addr.sa_len = 16;
			}
#else
			if (ifr->ifr_addr.sa_len == 0)
				ifr->ifr_addr.sa_len = 16;
#endif
			break;

		case OSIOCGIFADDR:
			cmd = SIOCGIFADDR;
			break;

		case OSIOCGIFDSTADDR:
			cmd = SIOCGIFDSTADDR;
			break;

		case OSIOCGIFBRDADDR:
			cmd = SIOCGIFBRDADDR;
			break;

		case OSIOCGIFNETMASK:
			cmd = SIOCGIFNETMASK;
		}
		error =  ((*so->so_proto->pr_usrreqs->pru_control)(so,
								   cmd,
								   data,
								   ifp, td));
		switch (ocmd) {

		case OSIOCGIFADDR:
		case OSIOCGIFDSTADDR:
		case OSIOCGIFBRDADDR:
		case OSIOCGIFNETMASK:
			*(u_short *)&ifr->ifr_addr = ifr->ifr_addr.sa_family;

		}
	}
#endif /* COMPAT_43 */

	if ((oif_flags ^ ifp->if_flags) & IFF_UP) {
#ifdef INET6
		DELAY(100);/* XXX: temporary workaround for fxp issue*/
		if (ifp->if_flags & IFF_UP) {
			int s = splimp();
			in6_if_up(ifp);
			splx(s);
		}
#endif
	}
	return (error);
}

/*
 * Set/clear promiscuous mode on interface ifp based on the truth value
 * of pswitch.  The calls are reference counted so that only the first
 * "on" request actually has an effect, as does the final "off" request.
 * Results are undefined if the "off" and "on" requests are not matched.
 */
int
ifpromisc(ifp, pswitch)
	struct ifnet *ifp;
	int pswitch;
{
	struct ifreq ifr;
	int error;
	int oldflags, oldpcount;

	oldpcount = ifp->if_pcount;
	oldflags = ifp->if_flags;
	if (ifp->if_flags & IFF_PPROMISC) {
		/* Do nothing if device is in permanently promiscuous mode */
		ifp->if_pcount += pswitch ? 1 : -1;
		return (0);
	}
	if (pswitch) {
		/*
		 * If the device is not configured up, we cannot put it in
		 * promiscuous mode.
		 */
		if ((ifp->if_flags & IFF_UP) == 0)
			return (ENETDOWN);
		if (ifp->if_pcount++ != 0)
			return (0);
		ifp->if_flags |= IFF_PROMISC;
	} else {
		if (--ifp->if_pcount > 0)
			return (0);
		ifp->if_flags &= ~IFF_PROMISC;
	}
	ifr.ifr_flags = ifp->if_flags & 0xffff;
	ifr.ifr_flagshigh = ifp->if_flags >> 16;
	error = (*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, (caddr_t)&ifr);
	if (error == 0) {
		log(LOG_INFO, "%s%d: promiscuous mode %s\n",
		    ifp->if_name, ifp->if_unit,
		    (ifp->if_flags & IFF_PROMISC) ? "enabled" : "disabled");
		rt_ifmsg(ifp);
	} else {
		ifp->if_pcount = oldpcount;
		ifp->if_flags = oldflags;
	}
	return error;
}

/*
 * Return interface configuration
 * of system.  List may be used
 * in later ioctl's (above) to get
 * other information.
 */
/*ARGSUSED*/
static int
ifconf(cmd, data)
	u_long cmd;
	caddr_t data;
{
	struct ifconf *ifc = (struct ifconf *)data;
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct ifreq ifr, *ifrp;
	int space = ifc->ifc_len, error = 0;

	ifrp = ifc->ifc_req;
	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		char workbuf[64];
		int ifnlen, addrs;

		if (space < sizeof(ifr))
			break;
		ifnlen = snprintf(workbuf, sizeof(workbuf),
		    "%s%d", ifp->if_name, ifp->if_unit);
		if(ifnlen + 1 > sizeof ifr.ifr_name) {
			error = ENAMETOOLONG;
			break;
		} else {
			strcpy(ifr.ifr_name, workbuf);
		}

		addrs = 0;
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			struct sockaddr *sa = ifa->ifa_addr;

			if (space < sizeof(ifr))
				break;
			if (jailed(curthread->td_ucred) &&
			    prison_if(curthread->td_ucred, sa))
				continue;
			addrs++;
#ifdef COMPAT_43
			if (cmd == OSIOCGIFCONF) {
				struct osockaddr *osa =
					 (struct osockaddr *)&ifr.ifr_addr;
				ifr.ifr_addr = *sa;
				osa->sa_family = sa->sa_family;
				error = copyout((caddr_t)&ifr, (caddr_t)ifrp,
						sizeof (ifr));
				ifrp++;
			} else
#endif
			if (sa->sa_len <= sizeof(*sa)) {
				ifr.ifr_addr = *sa;
				error = copyout((caddr_t)&ifr, (caddr_t)ifrp,
						sizeof (ifr));
				ifrp++;
			} else {
				if (space < sizeof (ifr) + sa->sa_len -
					    sizeof(*sa))
					break;
				space -= sa->sa_len - sizeof(*sa);
				error = copyout((caddr_t)&ifr, (caddr_t)ifrp,
						sizeof (ifr.ifr_name));
				if (error == 0)
				    error = copyout((caddr_t)sa,
				      (caddr_t)&ifrp->ifr_addr, sa->sa_len);
				ifrp = (struct ifreq *)
					(sa->sa_len + (caddr_t)&ifrp->ifr_addr);
			}
			if (error)
				break;
			space -= sizeof (ifr);
		}
		if (error)
			break;
		if (!addrs) {
			bzero((caddr_t)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
			error = copyout((caddr_t)&ifr, (caddr_t)ifrp,
			    sizeof (ifr));
			if (error)
				break;
			space -= sizeof (ifr);
			ifrp++;
		}
	}
	ifc->ifc_len -= space;
	return (error);
}

/*
 * Just like if_promisc(), but for all-multicast-reception mode.
 */
int
if_allmulti(ifp, onswitch)
	struct ifnet *ifp;
	int onswitch;
{
	int error = 0;
	int s = splimp();
	struct ifreq ifr;

	if (onswitch) {
		if (ifp->if_amcount++ == 0) {
			ifp->if_flags |= IFF_ALLMULTI;
			ifr.ifr_flags = ifp->if_flags & 0xffff;
			ifr.ifr_flagshigh = ifp->if_flags >> 16;
			error = ifp->if_ioctl(ifp, SIOCSIFFLAGS, (caddr_t)&ifr);
		}
	} else {
		if (ifp->if_amcount > 1) {
			ifp->if_amcount--;
		} else {
			ifp->if_amcount = 0;
			ifp->if_flags &= ~IFF_ALLMULTI;
			ifr.ifr_flags = ifp->if_flags & 0xffff;;
			ifr.ifr_flagshigh = ifp->if_flags >> 16;
			error = ifp->if_ioctl(ifp, SIOCSIFFLAGS, (caddr_t)&ifr);
		}
	}
	splx(s);

	if (error == 0)
		rt_ifmsg(ifp);
	return error;
}

/*
 * Add a multicast listenership to the interface in question.
 * The link layer provides a routine which converts
 */
int
if_addmulti(ifp, sa, retifma)
	struct ifnet *ifp;	/* interface to manipulate */
	struct sockaddr *sa;	/* address to add */
	struct ifmultiaddr **retifma;
{
	struct sockaddr *llsa, *dupsa;
	int error, s;
	struct ifmultiaddr *ifma;

	/*
	 * If the matching multicast address already exists
	 * then don't add a new one, just add a reference
	 */
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (equal(sa, ifma->ifma_addr)) {
			ifma->ifma_refcount++;
			if (retifma)
				*retifma = ifma;
			return 0;
		}
	}

	/*
	 * Give the link layer a chance to accept/reject it, and also
	 * find out which AF_LINK address this maps to, if it isn't one
	 * already.
	 */
	if (ifp->if_resolvemulti) {
		error = ifp->if_resolvemulti(ifp, &llsa, sa);
		if (error) return error;
	} else {
		llsa = 0;
	}

	MALLOC(ifma, struct ifmultiaddr *, sizeof *ifma, M_IFMADDR, M_WAITOK);
	MALLOC(dupsa, struct sockaddr *, sa->sa_len, M_IFMADDR, M_WAITOK);
	bcopy(sa, dupsa, sa->sa_len);

	ifma->ifma_addr = dupsa;
	ifma->ifma_lladdr = llsa;
	ifma->ifma_ifp = ifp;
	ifma->ifma_refcount = 1;
	ifma->ifma_protospec = 0;
	rt_newmaddrmsg(RTM_NEWMADDR, ifma);

	/*
	 * Some network interfaces can scan the address list at
	 * interrupt time; lock them out.
	 */
	s = splimp();
	TAILQ_INSERT_HEAD(&ifp->if_multiaddrs, ifma, ifma_link);
	splx(s);
	if (retifma != NULL)
		*retifma = ifma;

	if (llsa != 0) {
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
			if (equal(ifma->ifma_addr, llsa))
				break;
		}
		if (ifma) {
			ifma->ifma_refcount++;
		} else {
			MALLOC(ifma, struct ifmultiaddr *, sizeof *ifma,
			       M_IFMADDR, M_WAITOK);
			MALLOC(dupsa, struct sockaddr *, llsa->sa_len,
			       M_IFMADDR, M_WAITOK);
			bcopy(llsa, dupsa, llsa->sa_len);
			ifma->ifma_addr = dupsa;
			ifma->ifma_ifp = ifp;
			ifma->ifma_refcount = 1;
			s = splimp();
			TAILQ_INSERT_HEAD(&ifp->if_multiaddrs, ifma, ifma_link);
			splx(s);
		}
	}
	/*
	 * We are certain we have added something, so call down to the
	 * interface to let them know about it.
	 */
	s = splimp();
	ifp->if_ioctl(ifp, SIOCADDMULTI, 0);
	splx(s);

	return 0;
}

/*
 * Remove a reference to a multicast address on this interface.  Yell
 * if the request does not match an existing membership.
 */
int
if_delmulti(ifp, sa)
	struct ifnet *ifp;
	struct sockaddr *sa;
{
	struct ifmultiaddr *ifma;
	int s;

	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link)
		if (equal(sa, ifma->ifma_addr))
			break;
	if (ifma == 0)
		return ENOENT;

	if (ifma->ifma_refcount > 1) {
		ifma->ifma_refcount--;
		return 0;
	}

	rt_newmaddrmsg(RTM_DELMADDR, ifma);
	sa = ifma->ifma_lladdr;
	s = splimp();
	TAILQ_REMOVE(&ifp->if_multiaddrs, ifma, ifma_link);
	/*
	 * Make sure the interface driver is notified
	 * in the case of a link layer mcast group being left.
	 */
	if (ifma->ifma_addr->sa_family == AF_LINK && sa == 0)
		ifp->if_ioctl(ifp, SIOCDELMULTI, 0);
	splx(s);
	free(ifma->ifma_addr, M_IFMADDR);
	free(ifma, M_IFMADDR);
	if (sa == 0)
		return 0;

	/*
	 * Now look for the link-layer address which corresponds to
	 * this network address.  It had been squirreled away in
	 * ifma->ifma_lladdr for this purpose (so we don't have
	 * to call ifp->if_resolvemulti() again), and we saved that
	 * value in sa above.  If some nasty deleted the
	 * link-layer address out from underneath us, we can deal because
	 * the address we stored was is not the same as the one which was
	 * in the record for the link-layer address.  (So we don't complain
	 * in that case.)
	 */
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link)
		if (equal(sa, ifma->ifma_addr))
			break;
	if (ifma == 0)
		return 0;

	if (ifma->ifma_refcount > 1) {
		ifma->ifma_refcount--;
		return 0;
	}

	s = splimp();
	TAILQ_REMOVE(&ifp->if_multiaddrs, ifma, ifma_link);
	ifp->if_ioctl(ifp, SIOCDELMULTI, 0);
	splx(s);
	free(ifma->ifma_addr, M_IFMADDR);
	free(sa, M_IFMADDR);
	free(ifma, M_IFMADDR);

	return 0;
}

/*
 * Set the link layer address on an interface.
 *
 * At this time we only support certain types of interfaces,
 * and we don't allow the length of the address to change.
 */
int
if_setlladdr(struct ifnet *ifp, const u_char *lladdr, int len)
{
	struct sockaddr_dl *sdl;
	struct ifaddr *ifa;
	struct ifreq ifr;

	ifa = ifaddr_byindex(ifp->if_index);
	if (ifa == NULL)
		return (EINVAL);
	sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	if (sdl == NULL)
		return (EINVAL);
	if (len != sdl->sdl_alen)	/* don't allow length to change */
		return (EINVAL);
	switch (ifp->if_type) {
	case IFT_ETHER:			/* these types use struct arpcom */
	case IFT_FDDI:
	case IFT_XETHER:
	case IFT_ISO88025:
	case IFT_L2VLAN:
		bcopy(lladdr, ((struct arpcom *)ifp->if_softc)->ac_enaddr, len);
		bcopy(lladdr, LLADDR(sdl), len);
		break;
	default:
		return (ENODEV);
	}
	/*
	 * If the interface is already up, we need
	 * to re-init it in order to reprogram its
	 * address filter.
	 */
	if ((ifp->if_flags & IFF_UP) != 0) {
		ifp->if_flags &= ~IFF_UP;
		ifr.ifr_flags = ifp->if_flags & 0xffff;
		ifr.ifr_flagshigh = ifp->if_flags >> 16;
		(*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, (caddr_t)&ifr);
		ifp->if_flags |= IFF_UP;
		ifr.ifr_flags = ifp->if_flags & 0xffff;
		ifr.ifr_flagshigh = ifp->if_flags >> 16;
		(*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, (caddr_t)&ifr);
#ifdef INET
		/*
		 * Also send gratuitous ARPs to notify other nodes about
		 * the address change.
		 */
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			if (ifa->ifa_addr != NULL &&
			    ifa->ifa_addr->sa_family == AF_INET)
				arp_ifinit(ifp, ifa);
		}
#endif
	}
	return (0);
}

struct ifmultiaddr *
ifmaof_ifpforaddr(sa, ifp)
	struct sockaddr *sa;
	struct ifnet *ifp;
{
	struct ifmultiaddr *ifma;
	
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link)
		if (equal(ifma->ifma_addr, sa))
			break;

	return ifma;
}

SYSCTL_NODE(_net, PF_LINK, link, CTLFLAG_RW, 0, "Link layers");
SYSCTL_NODE(_net_link, 0, generic, CTLFLAG_RW, 0, "Generic link-management");
