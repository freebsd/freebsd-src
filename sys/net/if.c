/*-
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
#include "opt_carp.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/sbuf.h>
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
#include <sys/taskqueue.h>
#include <sys/domain.h>
#include <sys/jail.h>
#include <machine/stdarg.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_clone.h>
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
#ifdef DEV_CARP
#include <netinet/ip_carp.h>
#endif

SYSCTL_NODE(_net, PF_LINK, link, CTLFLAG_RW, 0, "Link layers");
SYSCTL_NODE(_net_link, 0, generic, CTLFLAG_RW, 0, "Generic link-management");

/* Log link state change events */
static int log_link_state_change = 1;

SYSCTL_INT(_net_link, OID_AUTO, log_link_state_change, CTLFLAG_RW,
	&log_link_state_change, 0,
	"log interface link state change events");

void	(*bstp_linkstate_p)(struct ifnet *ifp, int state);
void	(*ng_ether_link_state_p)(struct ifnet *ifp, int state);

struct mbuf *(*tbr_dequeue_ptr)(struct ifaltq *, int) = NULL;

static void	if_attachdomain(void *);
static void	if_attachdomain1(struct ifnet *);
static int	ifconf(u_long, caddr_t);
static void	if_grow(void);
static void	if_init(void *);
static void	if_check(void *);
static int	if_findindex(struct ifnet *);
static void	if_qflush(struct ifaltq *);
static void	if_route(struct ifnet *, int flag, int fam);
static void	if_slowtimo(void *);
static void	if_unroute(struct ifnet *, int flag, int fam);
static void	link_rtrequest(int, struct rtentry *, struct rt_addrinfo *);
static int	if_rtdel(struct radix_node *, void *);
static int	ifhwioctl(u_long, struct ifnet *, caddr_t, struct thread *);
static void	if_start_deferred(void *context, int pending);
static void	do_link_state_change(void *, int);
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
struct	mtx ifnet_lock;
static	if_com_alloc_t *if_com_alloc[256];
static	if_com_free_t *if_com_free[256];

static int	if_indexlim = 8;
static struct	knlist ifklist;

static void	filt_netdetach(struct knote *kn);
static int	filt_netdev(struct knote *kn, long hint);

static struct filterops netdev_filtops =
    { 1, NULL, filt_netdetach, filt_netdev };

/*
 * System initialization
 */
SYSINIT(interfaces, SI_SUB_INIT_IF, SI_ORDER_FIRST, if_init, NULL)
SYSINIT(interface_check, SI_SUB_PROTO_IF, SI_ORDER_FIRST, if_check, NULL)

MALLOC_DEFINE(M_IFNET, "ifnet", "interface internals");
MALLOC_DEFINE(M_IFADDR, "ifaddr", "interface address");
MALLOC_DEFINE(M_IFMADDR, "ether_multi", "link-level multicast address");

static d_open_t		netopen;
static d_close_t	netclose;
static d_ioctl_t	netioctl;
static d_kqfilter_t	netkqfilter;

static struct cdevsw net_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	netopen,
	.d_close =	netclose,
	.d_ioctl =	netioctl,
	.d_name =	"net",
	.d_kqfilter =	netkqfilter,
};

static int
netopen(struct cdev *dev, int flag, int mode, struct thread *td)
{
	return (0);
}

static int
netclose(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	return (0);
}

static int
netioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, struct thread *td)
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
netkqfilter(struct cdev *dev, struct knote *kn)
{
	struct knlist *klist;
	struct ifnet *ifp;
	int idx;

	switch (kn->kn_filter) {
	case EVFILT_NETDEV:
		kn->kn_fop = &netdev_filtops;
		break;
	default:
		return (1);
	}

	idx = minor(dev);
	if (idx == 0) {
		klist = &ifklist;
	} else {
		ifp = ifnet_byindex(idx);
		if (ifp == NULL)
			return (1);
		klist = &ifp->if_klist;
	}

	kn->kn_hook = (caddr_t)klist;

	knlist_add(klist, kn, 0);

	return (0);
}

static void
filt_netdetach(struct knote *kn)
{
	struct knlist *klist = (struct knlist *)kn->kn_hook;

	knlist_remove(klist, kn, 0);
}

static int
filt_netdev(struct knote *kn, long hint)
{
	struct knlist *klist = (struct knlist *)kn->kn_hook;

	/*
	 * Currently NOTE_EXIT is abused to indicate device detach.
	 */
	if (hint == NOTE_EXIT) {
		kn->kn_data = NOTE_LINKINV;
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);
		knlist_remove_inevent(klist, kn);
		return (1);
	}
	if (hint != 0)
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
if_init(void *dummy __unused)
{

	IFNET_LOCK_INIT();
	TAILQ_INIT(&ifnet);
	knlist_init(&ifklist, NULL, NULL, NULL, NULL);
	if_grow();				/* create initial table */
	ifdev_byindex(0) = make_dev(&net_cdevsw, 0,
	    UID_ROOT, GID_WHEEL, 0600, "network");
	if_clone_init();
}

static void
if_grow(void)
{
	u_int n;
	struct ifindex_entry *e;

	if_indexlim <<= 1;
	n = if_indexlim * sizeof(*e);
	e = malloc(n, M_IFNET, M_WAITOK | M_ZERO);
	if (ifindex_table != NULL) {
		memcpy((caddr_t)e, (caddr_t)ifindex_table, n/2);
		free((caddr_t)ifindex_table, M_IFNET);
	}
	ifindex_table = e;
}

/* ARGSUSED*/
static void
if_check(void *dummy __unused)
{
	struct ifnet *ifp;
	int s;

	s = splimp();
	IFNET_RLOCK();	/* could sleep on rare error; mostly okay XXX */
	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		if (ifp->if_snd.ifq_maxlen == 0) {
			if_printf(ifp, "XXX: driver didn't set ifq_maxlen\n");
			ifp->if_snd.ifq_maxlen = ifqmaxlen;
		}
		if (!mtx_initialized(&ifp->if_snd.ifq_mtx)) {
			if_printf(ifp,
			    "XXX: driver didn't initialize queue mtx\n");
			mtx_init(&ifp->if_snd.ifq_mtx, "unknown",
			    MTX_NETWORK_LOCK, MTX_DEF);
		}
	}
	IFNET_RUNLOCK();
	splx(s);
	if_slowtimo(0);
}

/* XXX: should be locked. */
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
	case IFT_BRIDGE:
		snprintf(eaddr, 18, "%6D", IFP2ENADDR(ifp), ":");
		break;
	default:
		eaddr[0] = '\0';
		break;
	}
	strlcpy(devname, ifp->if_xname, sizeof(devname));
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
 * Allocate a struct ifnet and in index for an interface.
 */
struct ifnet*
if_alloc(u_char type)
{
	struct ifnet *ifp;

	ifp = malloc(sizeof(struct ifnet), M_IFNET, M_WAITOK|M_ZERO);

	/* XXX: This should fail if if_index is too big */
	ifp->if_index = if_findindex(ifp);
	if (ifp->if_index > if_index)
		if_index = ifp->if_index;
	if (if_index >= if_indexlim)
		if_grow();

	ifnet_byindex(ifp->if_index) = ifp;

	ifp->if_type = type;

	if (if_com_alloc[type] != NULL) {
		ifp->if_l2com = if_com_alloc[type](type, ifp);
		if (ifp->if_l2com == NULL) {
			free(ifp, M_IFNET);
			return (NULL);
		}
	}

	return (ifp);
}

void
if_free(struct ifnet *ifp)
{

	if_free_type(ifp, ifp->if_type);
}

void
if_free_type(struct ifnet *ifp, u_char type)
{

	if (ifp != ifnet_byindex(ifp->if_index)) {
		if_printf(ifp, "%s: value was not if_alloced, skipping\n",
		    __func__);
		return;
	}

	ifnet_byindex(ifp->if_index) = NULL;

	/* XXX: should be locked with if_findindex() */
	while (if_index > 0 && ifaddr_byindex(if_index) == NULL)
		if_index--;

	if (if_com_free[type] != NULL)
		if_com_free[type](ifp->if_l2com, type);

	free(ifp, M_IFNET);
};

/*
 * Attach an interface to the
 * list of "active" interfaces.
 */
void
if_attach(struct ifnet *ifp)
{
	unsigned socksize, ifasize;
	int namelen, masklen;
	struct sockaddr_dl *sdl;
	struct ifaddr *ifa;

	if (ifp->if_index == 0 || ifp != ifnet_byindex(ifp->if_index))
		panic ("%s: BUG: if_attach called without if_alloc'd input()\n",
		    ifp->if_xname);

	TASK_INIT(&ifp->if_starttask, 0, if_start_deferred, ifp);
	TASK_INIT(&ifp->if_linktask, 0, do_link_state_change, ifp);
	IF_AFDATA_LOCK_INIT(ifp);
	ifp->if_afdata_initialized = 0;
	IFNET_WLOCK();
	TAILQ_INSERT_TAIL(&ifnet, ifp, if_link);
	IFNET_WUNLOCK();
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
	knlist_init(&ifp->if_klist, NULL, NULL, NULL, NULL);
	getmicrotime(&ifp->if_lastchange);
	ifp->if_data.ifi_epoch = time_uptime;
	ifp->if_data.ifi_datalen = sizeof(struct if_data);

#ifdef MAC
	mac_init_ifnet(ifp);
	mac_create_ifnet(ifp);
#endif

	ifdev_byindex(ifp->if_index) = make_dev(&net_cdevsw,
	    unit2minor(ifp->if_index),
	    UID_ROOT, GID_WHEEL, 0600, "%s/%s",
	    net_cdevsw.d_name, ifp->if_xname);
	make_dev_alias(ifdev_byindex(ifp->if_index), "%s%d",
	    net_cdevsw.d_name, ifp->if_index);

	mtx_init(&ifp->if_snd.ifq_mtx, ifp->if_xname, "if send queue", MTX_DEF);

	/*
	 * create a Link Level name for this device
	 */
	namelen = strlen(ifp->if_xname);
	/*
	 * Always save enough space for any possiable name so we can do
	 * a rename in place later.
	 */
	masklen = offsetof(struct sockaddr_dl, sdl_data[0]) + IFNAMSIZ;
	socksize = masklen + ifp->if_addrlen;
	if (socksize < sizeof(*sdl))
		socksize = sizeof(*sdl);
	socksize = roundup2(socksize, sizeof(long));
	ifasize = sizeof(*ifa) + 2 * socksize;
	ifa = malloc(ifasize, M_IFADDR, M_WAITOK | M_ZERO);
	IFA_LOCK_INIT(ifa);
	sdl = (struct sockaddr_dl *)(ifa + 1);
	sdl->sdl_len = socksize;
	sdl->sdl_family = AF_LINK;
	bcopy(ifp->if_xname, sdl->sdl_data, namelen);
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
	ifa->ifa_refcnt = 1;
	TAILQ_INSERT_HEAD(&ifp->if_addrhead, ifa, ifa_link);
	ifp->if_broadcastaddr = NULL; /* reliably crash if used uninitialized */
	ifp->if_snd.altq_type = 0;
	ifp->if_snd.altq_disc = NULL;
	ifp->if_snd.altq_flags &= ALTQF_CANTCHANGE;
	ifp->if_snd.altq_tbr  = NULL;
	ifp->if_snd.altq_ifp  = ifp;

	if (domain_init_status >= 2)
		if_attachdomain1(ifp);

	EVENTHANDLER_INVOKE(ifnet_arrival_event, ifp);

	/* Announce the interface. */
	rt_ifannouncemsg(ifp, IFAN_ARRIVAL);
}

static void
if_attachdomain(void *dummy)
{
	struct ifnet *ifp;
	int s;

	s = splnet();
	TAILQ_FOREACH(ifp, &ifnet, if_link)
		if_attachdomain1(ifp);
	splx(s);
}
SYSINIT(domainifattach, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_SECOND,
    if_attachdomain, NULL);

static void
if_attachdomain1(struct ifnet *ifp)
{
	struct domain *dp;
	int s;

	s = splnet();

	/*
	 * Since dp->dom_ifattach calls malloc() with M_WAITOK, we
	 * cannot lock ifp->if_afdata initialization, entirely.
	 */
	if (IF_AFDATA_TRYLOCK(ifp) == 0) {
		splx(s);
		return;
	}
	if (ifp->if_afdata_initialized >= domain_init_status) {
		IF_AFDATA_UNLOCK(ifp);
		splx(s);
		printf("if_attachdomain called more than once on %s\n",
		    ifp->if_xname);
		return;
	}
	ifp->if_afdata_initialized = domain_init_status;
	IF_AFDATA_UNLOCK(ifp);

	/* address family dependent data region */
	bzero(ifp->if_afdata, sizeof(ifp->if_afdata));
	for (dp = domains; dp; dp = dp->dom_next) {
		if (dp->dom_ifattach)
			ifp->if_afdata[dp->dom_family] =
			    (*dp->dom_ifattach)(ifp);
	}

	splx(s);
}

/*
 * Remove any network addresses from an interface.
 */

void
if_purgeaddrs(struct ifnet *ifp)
{
	struct ifaddr *ifa, *next;

	TAILQ_FOREACH_SAFE(ifa, &ifp->if_addrhead, ifa_link, next) {

		if (ifa->ifa_addr->sa_family == AF_LINK)
			continue;
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
}

/*
 * Detach an interface, removing it from the
 * list of "active" interfaces and freeing the struct ifnet.
 */
void
if_detach(struct ifnet *ifp)
{
	struct ifaddr *ifa;
	struct radix_node_head	*rnh;
	int s;
	int i;
	struct domain *dp;
 	struct ifnet *iter;
 	int found;

	/*
	 * Remove/wait for pending events.
	 */
	taskqueue_drain(taskqueue_swi, &ifp->if_linktask);

#ifdef DEV_CARP
	/* Maybe hook to the generalized departure handler above?!? */
	if (ifp->if_carp)
		carp_ifdetach(ifp);
#endif

	/*
	 * Remove routes and flush queues.
	 */
	s = splnet();
	if_down(ifp);
#ifdef ALTQ
	if (ALTQ_IS_ENABLED(&ifp->if_snd))
		altq_disable(&ifp->if_snd);
	if (ALTQ_IS_ATTACHED(&ifp->if_snd))
		altq_detach(&ifp->if_snd);
#endif

	if_purgeaddrs(ifp);

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
	 * Remove address from ifindex_table[] and maybe decrement if_index.
	 * Clean up all addresses.
	 */
	ifaddr_byindex(ifp->if_index) = NULL;
	destroy_dev(ifdev_byindex(ifp->if_index));
	ifdev_byindex(ifp->if_index) = NULL;

	/* We can now free link ifaddr. */
	if (!TAILQ_EMPTY(&ifp->if_addrhead)) {
		ifa = TAILQ_FIRST(&ifp->if_addrhead);
		TAILQ_REMOVE(&ifp->if_addrhead, ifa, ifa_link);
		IFAFREE(ifa);
	}

	/*
	 * Delete all remaining routes using this interface
	 * Unfortuneatly the only way to do this is to slog through
	 * the entire routing table looking for routes which point
	 * to this interface...oh well...
	 */
	for (i = 1; i <= AF_MAX; i++) {
		if ((rnh = rt_tables[i]) == NULL)
			continue;
		RADIX_NODE_HEAD_LOCK(rnh);
		(void) rnh->rnh_walktree(rnh, if_rtdel, ifp);
		RADIX_NODE_HEAD_UNLOCK(rnh);
	}

	/* Announce that the interface is gone. */
	rt_ifannouncemsg(ifp, IFAN_DEPARTURE);
	EVENTHANDLER_INVOKE(ifnet_departure_event, ifp);

	IF_AFDATA_LOCK(ifp);
	for (dp = domains; dp; dp = dp->dom_next) {
		if (dp->dom_ifdetach && ifp->if_afdata[dp->dom_family])
			(*dp->dom_ifdetach)(ifp,
			    ifp->if_afdata[dp->dom_family]);
	}
	IF_AFDATA_UNLOCK(ifp);

#ifdef MAC
	mac_destroy_ifnet(ifp);
#endif /* MAC */
	KNOTE_UNLOCKED(&ifp->if_klist, NOTE_EXIT);
	knlist_clear(&ifp->if_klist, 0);
	knlist_destroy(&ifp->if_klist);
	IFNET_WLOCK();
 	found = 0;
 	TAILQ_FOREACH(iter, &ifnet, if_link)
 		if (iter == ifp) {
 			found = 1;
 			break;
 		}
 	if (found)
 		TAILQ_REMOVE(&ifnet, ifp, if_link);
	IFNET_WUNLOCK();
	mtx_destroy(&ifp->if_snd.ifq_mtx);
	IF_AFDATA_DESTROY(ifp);
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
if_rtdel(struct radix_node *rn, void *arg)
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

#define	sa_equal(a1, a2)	(bcmp((a1), (a2), ((a1))->sa_len) == 0)

/*
 * Locate an interface based on a complete address.
 */
/*ARGSUSED*/
struct ifaddr *
ifa_ifwithaddr(struct sockaddr *addr)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;

	IFNET_RLOCK();
	TAILQ_FOREACH(ifp, &ifnet, if_link)
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			if (ifa->ifa_addr->sa_family != addr->sa_family)
				continue;
			if (sa_equal(addr, ifa->ifa_addr))
				goto done;
			/* IP6 doesn't have broadcast */
			if ((ifp->if_flags & IFF_BROADCAST) &&
			    ifa->ifa_broadaddr &&
			    ifa->ifa_broadaddr->sa_len != 0 &&
			    sa_equal(ifa->ifa_broadaddr, addr))
				goto done;
		}
	ifa = NULL;
done:
	IFNET_RUNLOCK();
	return (ifa);
}

/*
 * Locate the point to point interface with a given destination address.
 */
/*ARGSUSED*/
struct ifaddr *
ifa_ifwithdstaddr(struct sockaddr *addr)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;

	IFNET_RLOCK();
	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			continue;
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			if (ifa->ifa_addr->sa_family != addr->sa_family)
				continue;
			if (ifa->ifa_dstaddr &&
			    sa_equal(addr, ifa->ifa_dstaddr))
				goto done;
		}
	}
	ifa = NULL;
done:
	IFNET_RUNLOCK();
	return (ifa);
}

/*
 * Find an interface on a specific network.  If many, choice
 * is most specific found.
 */
struct ifaddr *
ifa_ifwithnet(struct sockaddr *addr)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct ifaddr *ifa_maybe = (struct ifaddr *) 0;
	u_int af = addr->sa_family;
	char *addr_data = addr->sa_data, *cplim;

	/*
	 * AF_LINK addresses can be looked up directly by their index number,
	 * so do that if we can.
	 */
	if (af == AF_LINK) {
	    struct sockaddr_dl *sdl = (struct sockaddr_dl *)addr;
	    if (sdl->sdl_index && sdl->sdl_index <= if_index)
		return (ifaddr_byindex(sdl->sdl_index));
	}

	/*
	 * Scan though each interface, looking for ones that have
	 * addresses in this address family.
	 */
	IFNET_RLOCK();
	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			char *cp, *cp2, *cp3;

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
				if (ifa->ifa_dstaddr != 0 &&
				    sa_equal(addr, ifa->ifa_dstaddr))
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
	IFNET_RUNLOCK();
	return (ifa);
}

/*
 * Find an interface address specific to an interface best matching
 * a given address.
 */
struct ifaddr *
ifaof_ifpforaddr(struct sockaddr *addr, struct ifnet *ifp)
{
	struct ifaddr *ifa;
	char *cp, *cp2, *cp3;
	char *cplim;
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
			if (sa_equal(addr, ifa->ifa_addr) ||
			    (ifa->ifa_dstaddr &&
			    sa_equal(addr, ifa->ifa_dstaddr)))
				goto done;
			continue;
		}
		if (ifp->if_flags & IFF_POINTOPOINT) {
			if (sa_equal(addr, ifa->ifa_dstaddr))
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
link_rtrequest(int cmd, struct rtentry *rt, struct rt_addrinfo *info)
{
	struct ifaddr *ifa, *oifa;
	struct sockaddr *dst;
	struct ifnet *ifp;

	RT_LOCK_ASSERT(rt);

	if (cmd != RTM_ADD || ((ifa = rt->rt_ifa) == 0) ||
	    ((ifp = ifa->ifa_ifp) == 0) || ((dst = rt_key(rt)) == 0))
		return;
	ifa = ifaof_ifpforaddr(dst, ifp);
	if (ifa) {
		IFAREF(ifa);		/* XXX */
		oifa = rt->rt_ifa;
		rt->rt_ifa = ifa;
		IFAFREE(oifa);
		if (ifa->ifa_rtrequest && ifa->ifa_rtrequest != link_rtrequest)
			ifa->ifa_rtrequest(cmd, rt, info);
	}
}

/*
 * Mark an interface down and notify protocols of
 * the transition.
 * NOTE: must be called at splnet or eqivalent.
 */
static void
if_unroute(struct ifnet *ifp, int flag, int fam)
{
	struct ifaddr *ifa;

	ifp->if_flags &= ~flag;
	getmicrotime(&ifp->if_lastchange);
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link)
		if (fam == PF_UNSPEC || (fam == ifa->ifa_addr->sa_family))
			pfctlinput(PRC_IFDOWN, ifa->ifa_addr);
	if_qflush(&ifp->if_snd);
#ifdef DEV_CARP
	if (ifp->if_carp)
		carp_carpdev_state(ifp->if_carp);
#endif
	rt_ifmsg(ifp);
}

/*
 * Mark an interface up and notify protocols of
 * the transition.
 * NOTE: must be called at splnet or eqivalent.
 */
static void
if_route(struct ifnet *ifp, int flag, int fam)
{
	struct ifaddr *ifa;

	ifp->if_flags |= flag;
	getmicrotime(&ifp->if_lastchange);
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link)
		if (fam == PF_UNSPEC || (fam == ifa->ifa_addr->sa_family))
			pfctlinput(PRC_IFUP, ifa->ifa_addr);
#ifdef DEV_CARP
	if (ifp->if_carp)
		carp_carpdev_state(ifp->if_carp);
#endif
	rt_ifmsg(ifp);
#ifdef INET6
	in6_if_up(ifp);
#endif
}

void	(*vlan_link_state_p)(struct ifnet *, int);	/* XXX: private from if_vlan */

/*
 * Handle a change in the interface link state. To avoid LORs
 * between driver lock and upper layer locks, as well as possible
 * recursions, we post event to taskqueue, and all job
 * is done in static do_link_state_change().
 */
void
if_link_state_change(struct ifnet *ifp, int link_state)
{
	/* Return if state hasn't changed. */
	if (ifp->if_link_state == link_state)
		return;

	ifp->if_link_state = link_state;

	taskqueue_enqueue(taskqueue_swi, &ifp->if_linktask);
}

static void
do_link_state_change(void *arg, int pending)
{
	struct ifnet *ifp = (struct ifnet *)arg;
	int link_state = ifp->if_link_state;
	int link;

	/* Notify that the link state has changed. */
	rt_ifmsg(ifp);
	if (link_state == LINK_STATE_UP)
		link = NOTE_LINKUP;
	else if (link_state == LINK_STATE_DOWN)
		link = NOTE_LINKDOWN;
	else
		link = NOTE_LINKINV;
	KNOTE_UNLOCKED(&ifp->if_klist, link);
	if (ifp->if_nvlans != 0)
		(*vlan_link_state_p)(ifp, link);

	if ((ifp->if_type == IFT_ETHER || ifp->if_type == IFT_L2VLAN) &&
	    IFP2AC(ifp)->ac_netgraph != NULL)
		(*ng_ether_link_state_p)(ifp, link_state);
#ifdef DEV_CARP
	if (ifp->if_carp)
		carp_carpdev_state(ifp->if_carp);
#endif
	if (ifp->if_bridge) {
		KASSERT(bstp_linkstate_p != NULL,("if_bridge bstp not loaded!"));
		(*bstp_linkstate_p)(ifp, link_state);
	}

	devctl_notify("IFNET", ifp->if_xname,
	    (link_state == LINK_STATE_UP) ? "LINK_UP" : "LINK_DOWN", NULL);
	if (pending > 1)
		if_printf(ifp, "%d link states coalesced\n", pending);
	if (log_link_state_change)
		log(LOG_NOTICE, "%s: link state changed to %s\n", ifp->if_xname,
		    (link_state == LINK_STATE_UP) ? "UP" : "DOWN" );
}

/*
 * Mark an interface down and notify protocols of
 * the transition.
 * NOTE: must be called at splnet or eqivalent.
 */
void
if_down(struct ifnet *ifp)
{

	if_unroute(ifp, IFF_UP, AF_UNSPEC);
}

/*
 * Mark an interface up and notify protocols of
 * the transition.
 * NOTE: must be called at splnet or eqivalent.
 */
void
if_up(struct ifnet *ifp)
{

	if_route(ifp, IFF_UP, AF_UNSPEC);
}

/*
 * Flush an interface queue.
 */
static void
if_qflush(struct ifaltq *ifq)
{
	struct mbuf *m, *n;

	IFQ_LOCK(ifq);
#ifdef ALTQ
	if (ALTQ_IS_ENABLED(ifq))
		ALTQ_PURGE(ifq);
#endif
	n = ifq->ifq_head;
	while ((m = n) != 0) {
		n = m->m_act;
		m_freem(m);
	}
	ifq->ifq_head = 0;
	ifq->ifq_tail = 0;
	ifq->ifq_len = 0;
	IFQ_UNLOCK(ifq);
}

/*
 * Handle interface watchdog timer routines.  Called
 * from softclock, we decrement timers (if set) and
 * call the appropriate interface routine on expiration.
 *
 * XXXRW: Note that because timeouts run with Giant, if_watchdog() is called
 * holding Giant.  If we switch to an MPSAFE callout, we likely need to grab
 * Giant before entering if_watchdog() on an IFF_NEEDSGIANT interface.
 */
static void
if_slowtimo(void *arg)
{
	struct ifnet *ifp;
	int s = splimp();

	IFNET_RLOCK();
	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		if (ifp->if_timer == 0 || --ifp->if_timer)
			continue;
		if (ifp->if_watchdog)
			(*ifp->if_watchdog)(ifp);
	}
	IFNET_RUNLOCK();
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
	struct ifnet *ifp;

	IFNET_RLOCK();
	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		if (strncmp(name, ifp->if_xname, IFNAMSIZ) == 0)
			break;
	}
	IFNET_RUNLOCK();
	return (ifp);
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
	size_t namelen, onamelen;
	char new_name[IFNAMSIZ];
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;

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
		error = mac_ioctl_ifnet_get(td->td_ucred, ifr, ifp);
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
		if (ifp->if_ioctl != NULL) {
			IFF_LOCKGIANT(ifp);
			(void) (*ifp->if_ioctl)(ifp, cmd, data);
			IFF_UNLOCKGIANT(ifp);
		}
		getmicrotime(&ifp->if_lastchange);
		break;

	case SIOCSIFCAP:
		error = suser(td);
		if (error)
			return (error);
		if (ifp->if_ioctl == NULL)
			return (EOPNOTSUPP);
		if (ifr->ifr_reqcap & ~ifp->if_capabilities)
			return (EINVAL);
		IFF_LOCKGIANT(ifp);
		error = (*ifp->if_ioctl)(ifp, cmd, data);
		IFF_UNLOCKGIANT(ifp);
		if (error == 0)
			getmicrotime(&ifp->if_lastchange);
		break;

#ifdef MAC
	case SIOCSIFMAC:
		error = mac_ioctl_ifnet_set(td->td_ucred, ifr, ifp);
		break;
#endif

	case SIOCSIFNAME:
		error = suser(td);
		if (error != 0)
			return (error);
		error = copyinstr(ifr->ifr_data, new_name, IFNAMSIZ, NULL);
		if (error != 0)
			return (error);
		if (new_name[0] == '\0')
			return (EINVAL);
		if (ifunit(new_name) != NULL)
			return (EEXIST);
		
		/* Announce the departure of the interface. */
		rt_ifannouncemsg(ifp, IFAN_DEPARTURE);
		EVENTHANDLER_INVOKE(ifnet_departure_event, ifp);

		log(LOG_INFO, "%s: changing name to '%s'\n",
		    ifp->if_xname, new_name);

		strlcpy(ifp->if_xname, new_name, sizeof(ifp->if_xname));
		ifa = ifaddr_byindex(ifp->if_index);
		IFA_LOCK(ifa);
		sdl = (struct sockaddr_dl *)ifa->ifa_addr;
		namelen = strlen(new_name);
		onamelen = sdl->sdl_nlen;
		/*
		 * Move the address if needed.  This is safe because we
		 * allocate space for a name of length IFNAMSIZ when we
		 * create this in if_attach().
		 */
		if (namelen != onamelen) {
			bcopy(sdl->sdl_data + onamelen,
			    sdl->sdl_data + namelen, sdl->sdl_alen);
		}
		bcopy(new_name, sdl->sdl_data, namelen);
		sdl->sdl_nlen = namelen;
		sdl = (struct sockaddr_dl *)ifa->ifa_netmask;
		bzero(sdl->sdl_data, onamelen);
		while (namelen != 0)
			sdl->sdl_data[--namelen] = 0xff;
		IFA_UNLOCK(ifa);

		EVENTHANDLER_INVOKE(ifnet_arrival_event, ifp);
		/* Announce the return of the interface. */
		rt_ifannouncemsg(ifp, IFAN_ARRIVAL);
		break;

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
			return (error);
		if (ifp->if_ioctl == NULL)
			return (EOPNOTSUPP);
		IFF_LOCKGIANT(ifp);
		error = (*ifp->if_ioctl)(ifp, cmd, data);
		IFF_UNLOCKGIANT(ifp);
		if (error == 0)
			getmicrotime(&ifp->if_lastchange);
		break;

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
		IFF_LOCKGIANT(ifp);
		error = (*ifp->if_ioctl)(ifp, cmd, data);
		IFF_UNLOCKGIANT(ifp);
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
		IFF_LOCKGIANT(ifp);
		error = (*ifp->if_ioctl)(ifp, cmd, data);
		IFF_UNLOCKGIANT(ifp);
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
		if (ifp->if_ioctl == NULL)
			return (EOPNOTSUPP);
		IFF_LOCKGIANT(ifp);
		error = (*ifp->if_ioctl)(ifp, cmd, data);
		IFF_UNLOCKGIANT(ifp);
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
ifioctl(struct socket *so, u_long cmd, caddr_t data, struct thread *td)
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
ifpromisc(struct ifnet *ifp, int pswitch)
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
	IFF_LOCKGIANT(ifp);
	error = (*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, (caddr_t)&ifr);
	IFF_UNLOCKGIANT(ifp);
	if (error == 0) {
		log(LOG_INFO, "%s: promiscuous mode %s\n",
		    ifp->if_xname,
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
ifconf(u_long cmd, caddr_t data)
{
	struct ifconf *ifc = (struct ifconf *)data;
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct ifreq ifr;
	struct sbuf *sb;
	int error, full = 0, valid_len, max_len;

	/* Limit initial buffer size to MAXPHYS to avoid DoS from userspace. */
	max_len = MAXPHYS - 1;

	/* Prevent hostile input from being able to crash the system */
	if (ifc->ifc_len <= 0)
		return (EINVAL);

again:
	if (ifc->ifc_len <= max_len) {
		max_len = ifc->ifc_len;
		full = 1;
	}
	sb = sbuf_new(NULL, NULL, max_len + 1, SBUF_FIXEDLEN);
	max_len = 0;
	valid_len = 0;

	IFNET_RLOCK();		/* could sleep XXX */
	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		int addrs;

		/*
		 * Zero the ifr_name buffer to make sure we don't
		 * disclose the contents of the stack.
		 */
		memset(ifr.ifr_name, 0, sizeof(ifr.ifr_name));

		if (strlcpy(ifr.ifr_name, ifp->if_xname, sizeof(ifr.ifr_name))
		    >= sizeof(ifr.ifr_name))
			return (ENAMETOOLONG);

		addrs = 0;
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			struct sockaddr *sa = ifa->ifa_addr;

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
				sbuf_bcat(sb, &ifr, sizeof(ifr));
				max_len += sizeof(ifr);
			} else
#endif
			if (sa->sa_len <= sizeof(*sa)) {
				ifr.ifr_addr = *sa;
				sbuf_bcat(sb, &ifr, sizeof(ifr));
				max_len += sizeof(ifr);
			} else {
				sbuf_bcat(sb, &ifr,
				    offsetof(struct ifreq, ifr_addr));
				max_len += offsetof(struct ifreq, ifr_addr);
				sbuf_bcat(sb, sa, sa->sa_len);
				max_len += sa->sa_len;
			}

			if (!sbuf_overflowed(sb))
				valid_len = sbuf_len(sb);
		}
		if (addrs == 0) {
			bzero((caddr_t)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
			sbuf_bcat(sb, &ifr, sizeof(ifr));
			max_len += sizeof(ifr);

			if (!sbuf_overflowed(sb))
				valid_len = sbuf_len(sb);
		}
	}
	IFNET_RUNLOCK();

	/*
	 * If we didn't allocate enough space (uncommon), try again.  If
	 * we have already allocated as much space as we are allowed,
	 * return what we've got.
	 */
	if (valid_len != max_len && !full) {
		sbuf_delete(sb);
		goto again;
	}

	ifc->ifc_len = valid_len;
	sbuf_finish(sb);
	error = copyout(sbuf_data(sb), ifc->ifc_req, ifc->ifc_len);
	sbuf_delete(sb);
	return (error);
}

/*
 * Just like ifpromisc(), but for all-multicast-reception mode.
 */
int
if_allmulti(struct ifnet *ifp, int onswitch)
{
	int error = 0;
	int s = splimp();
	struct ifreq ifr;

	if (onswitch) {
		if (ifp->if_amcount++ == 0) {
			ifp->if_flags |= IFF_ALLMULTI;
			ifr.ifr_flags = ifp->if_flags & 0xffff;
			ifr.ifr_flagshigh = ifp->if_flags >> 16;
			IFF_LOCKGIANT(ifp);
			error = ifp->if_ioctl(ifp, SIOCSIFFLAGS, (caddr_t)&ifr);
			IFF_UNLOCKGIANT(ifp);
		}
	} else {
		if (ifp->if_amcount > 1) {
			ifp->if_amcount--;
		} else {
			ifp->if_amcount = 0;
			ifp->if_flags &= ~IFF_ALLMULTI;
			ifr.ifr_flags = ifp->if_flags & 0xffff;;
			ifr.ifr_flagshigh = ifp->if_flags >> 16;
			IFF_LOCKGIANT(ifp);
			error = ifp->if_ioctl(ifp, SIOCSIFFLAGS, (caddr_t)&ifr);
			IFF_UNLOCKGIANT(ifp);
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
if_addmulti(struct ifnet *ifp, struct sockaddr *sa, struct ifmultiaddr **retifma)
{
	struct sockaddr *llsa, *dupsa;
	int error, s;
	struct ifmultiaddr *ifma;

	/*
	 * If the matching multicast address already exists
	 * then don't add a new one, just add a reference
	 */
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (sa_equal(sa, ifma->ifma_addr)) {
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
	if (ifp->if_resolvemulti != NULL) {
		error = ifp->if_resolvemulti(ifp, &llsa, sa);
		if (error) return error;
	} else {
		llsa = NULL;
	}

	MALLOC(ifma, struct ifmultiaddr *, sizeof *ifma, M_IFMADDR, M_WAITOK);
	MALLOC(dupsa, struct sockaddr *, sa->sa_len, M_IFMADDR, M_WAITOK);
	bcopy(sa, dupsa, sa->sa_len);

	ifma->ifma_addr = dupsa;
	ifma->ifma_lladdr = llsa;
	ifma->ifma_ifp = ifp;
	ifma->ifma_refcount = 1;
	ifma->ifma_protospec = NULL;
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

	if (llsa != NULL) {
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
			if (sa_equal(ifma->ifma_addr, llsa))
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
			ifma->ifma_lladdr = NULL;
			ifma->ifma_ifp = ifp;
			ifma->ifma_refcount = 1;
			ifma->ifma_protospec = NULL;
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
	IFF_LOCKGIANT(ifp);
	ifp->if_ioctl(ifp, SIOCADDMULTI, 0);
	IFF_UNLOCKGIANT(ifp);
	splx(s);

	return 0;
}

/*
 * Remove a reference to a multicast address on this interface.  Yell
 * if the request does not match an existing membership.
 */
int
if_delmulti(struct ifnet *ifp, struct sockaddr *sa)
{
	struct ifmultiaddr *ifma;
	int s;

	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link)
		if (sa_equal(sa, ifma->ifma_addr))
			break;
	if (ifma == NULL)
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
	if (ifma->ifma_addr->sa_family == AF_LINK && sa == 0) {
		IFF_LOCKGIANT(ifp);
		ifp->if_ioctl(ifp, SIOCDELMULTI, 0);
		IFF_UNLOCKGIANT(ifp);
	}
	splx(s);
	free(ifma->ifma_addr, M_IFMADDR);
	free(ifma, M_IFMADDR);
	if (sa == NULL)
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
		if (sa_equal(sa, ifma->ifma_addr))
			break;
	if (ifma == NULL)
		return 0;

	if (ifma->ifma_refcount > 1) {
		ifma->ifma_refcount--;
		return 0;
	}

	s = splimp();
	TAILQ_REMOVE(&ifp->if_multiaddrs, ifma, ifma_link);
	IFF_LOCKGIANT(ifp);
	ifp->if_ioctl(ifp, SIOCDELMULTI, 0);
	IFF_UNLOCKGIANT(ifp);
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
	case IFT_BRIDGE:
		bcopy(lladdr, IFP2ENADDR(ifp), len);
		/*
		 * XXX We also need to store the lladdr in LLADDR(sdl),
		 * which is done below. This is a pain because we must
		 * remember to keep the info in sync.
		 */
		/* FALLTHROUGH */
	case IFT_ARCNET:
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
		IFF_LOCKGIANT(ifp);
		ifp->if_flags &= ~IFF_UP;
		ifr.ifr_flags = ifp->if_flags & 0xffff;
		ifr.ifr_flagshigh = ifp->if_flags >> 16;
		(*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, (caddr_t)&ifr);
		ifp->if_flags |= IFF_UP;
		ifr.ifr_flags = ifp->if_flags & 0xffff;
		ifr.ifr_flagshigh = ifp->if_flags >> 16;
		(*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, (caddr_t)&ifr);
		IFF_UNLOCKGIANT(ifp);
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
ifmaof_ifpforaddr(struct sockaddr *sa, struct ifnet *ifp)
{
	struct ifmultiaddr *ifma;

	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link)
		if (sa_equal(ifma->ifma_addr, sa))
			break;

	return ifma;
}

/*
 * The name argument must be a pointer to storage which will last as
 * long as the interface does.  For physical devices, the result of
 * device_get_name(dev) is a good choice and for pseudo-devices a
 * static string works well.
 */
void
if_initname(struct ifnet *ifp, const char *name, int unit)
{
	ifp->if_dname = name;
	ifp->if_dunit = unit;
	if (unit != IF_DUNIT_NONE)
		snprintf(ifp->if_xname, IFNAMSIZ, "%s%d", name, unit);
	else
		strlcpy(ifp->if_xname, name, IFNAMSIZ);
}

int
if_printf(struct ifnet *ifp, const char * fmt, ...)
{
	va_list ap;
	int retval;

	retval = printf("%s: ", ifp->if_xname);
	va_start(ap, fmt);
	retval += vprintf(fmt, ap);
	va_end(ap);
	return (retval);
}

/*
 * When an interface is marked IFF_NEEDSGIANT, its if_start() routine cannot
 * be called without Giant.  However, we often can't acquire the Giant lock
 * at those points; instead, we run it via a task queue that holds Giant via
 * if_start_deferred.
 *
 * XXXRW: We need to make sure that the ifnet isn't fully detached until any
 * outstanding if_start_deferred() tasks that will run after the free.  This
 * probably means waiting in if_detach().
 */
void
if_start(struct ifnet *ifp)
{

	NET_ASSERT_GIANT();

	if ((ifp->if_flags & IFF_NEEDSGIANT) != 0 && debug_mpsafenet != 0) {
		if (mtx_owned(&Giant))
			(*(ifp)->if_start)(ifp);
		else
			taskqueue_enqueue(taskqueue_swi_giant,
			    &ifp->if_starttask);
	} else
		(*(ifp)->if_start)(ifp);
}

static void
if_start_deferred(void *context, int pending)
{
	struct ifnet *ifp;

	/*
	 * This code must be entered with Giant, and should never run if
	 * we're not running with debug.mpsafenet.
	 */
	KASSERT(debug_mpsafenet != 0, ("if_start_deferred: debug.mpsafenet"));
	GIANT_REQUIRED;

	ifp = context;
	(ifp->if_start)(ifp);
}

int
if_handoff(struct ifqueue *ifq, struct mbuf *m, struct ifnet *ifp, int adjust)
{
	int active = 0;

	IF_LOCK(ifq);
	if (_IF_QFULL(ifq)) {
		_IF_DROP(ifq);
		IF_UNLOCK(ifq);
		m_freem(m);
		return (0);
	}
	if (ifp != NULL) {
		ifp->if_obytes += m->m_pkthdr.len + adjust;
		if (m->m_flags & (M_BCAST|M_MCAST))
			ifp->if_omcasts++;
		active = ifp->if_flags & IFF_OACTIVE;
	}
	_IF_ENQUEUE(ifq, m);
	IF_UNLOCK(ifq);
	if (ifp != NULL && !active)
		if_start(ifp);
	return (1);
}

void
if_register_com_alloc(u_char type,
    if_com_alloc_t *a, if_com_free_t *f)
{
	
	KASSERT(if_com_alloc[type] == NULL,
	    ("if_register_com_alloc: %d already registered", type));
	KASSERT(if_com_free[type] == NULL,
	    ("if_register_com_alloc: %d free already registered", type));

	if_com_alloc[type] = a;
	if_com_free[type] = f;
}

void
if_deregister_com_alloc(u_char type)
{
	
	KASSERT(if_com_alloc[type] == NULL,
	    ("if_deregister_com_alloc: %d not registered", type));
	KASSERT(if_com_free[type] == NULL,
	    ("if_deregister_com_alloc: %d free not registered", type));
	if_com_alloc[type] = NULL;
	if_com_free[type] = NULL;
}
