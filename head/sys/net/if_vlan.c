/*-
 * Copyright 1998 Massachusetts Institute of Technology
 * Copyright 2012 ADARA Networks, Inc.
 *
 * Portions of this software were developed by Robert N. M. Watson under
 * contract to ADARA Networks, Inc.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * if_vlan.c - pseudo-device driver for IEEE 802.1Q virtual LANs.
 * This is sort of sneaky in the implementation, since
 * we need to pretend to be enough of an Ethernet implementation
 * to make arp work.  The way we do this is by telling everyone
 * that we are an Ethernet, and then catch the packets that
 * ether_output() sends to us via if_transmit(), rewrite them for
 * use by the real outgoing interface, and ask it to send them.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_vlan.h"

#include <sys/param.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/rmlock.h>
#include <sys/priv.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/sx.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_clone.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>
#include <net/vnet.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#define	VLAN_DEF_HWIDTH	4
#define	VLAN_IFFLAGS	(IFF_BROADCAST | IFF_MULTICAST)

#define	UP_AND_RUNNING(ifp) \
    ((ifp)->if_flags & IFF_UP && (ifp)->if_drv_flags & IFF_DRV_RUNNING)

LIST_HEAD(ifvlanhead, ifvlan);

struct ifvlantrunk {
	struct	ifnet   *parent;	/* parent interface of this trunk */
	struct	rmlock	lock;
#ifdef VLAN_ARRAY
#define	VLAN_ARRAY_SIZE	(EVL_VLID_MASK + 1)
	struct	ifvlan	*vlans[VLAN_ARRAY_SIZE]; /* static table */
#else
	struct	ifvlanhead *hash;	/* dynamic hash-list table */
	uint16_t	hmask;
	uint16_t	hwidth;
#endif
	int		refcnt;
};

struct vlan_mc_entry {
	struct sockaddr_dl		mc_addr;
	SLIST_ENTRY(vlan_mc_entry)	mc_entries;
};

struct	ifvlan {
	struct	ifvlantrunk *ifv_trunk;
	struct	ifnet *ifv_ifp;
#define	TRUNK(ifv)	((ifv)->ifv_trunk)
#define	PARENT(ifv)	((ifv)->ifv_trunk->parent)
	void	*ifv_cookie;
	int	ifv_pflags;	/* special flags we have set on parent */
	struct	ifv_linkmib {
		int	ifvm_encaplen;	/* encapsulation length */
		int	ifvm_mtufudge;	/* MTU fudged by this much */
		int	ifvm_mintu;	/* min transmission unit */
		uint16_t ifvm_proto;	/* encapsulation ethertype */
		uint16_t ifvm_tag;	/* tag to apply on packets leaving if */
              	uint16_t ifvm_vid;	/* VLAN ID */
		uint8_t	ifvm_pcp;	/* Priority Code Point (PCP). */
	}	ifv_mib;
	SLIST_HEAD(, vlan_mc_entry) vlan_mc_listhead;
#ifndef VLAN_ARRAY
	LIST_ENTRY(ifvlan) ifv_list;
#endif
};
#define	ifv_proto	ifv_mib.ifvm_proto
#define	ifv_tag		ifv_mib.ifvm_tag
#define	ifv_vid 	ifv_mib.ifvm_vid
#define	ifv_pcp		ifv_mib.ifvm_pcp
#define	ifv_encaplen	ifv_mib.ifvm_encaplen
#define	ifv_mtufudge	ifv_mib.ifvm_mtufudge
#define	ifv_mintu	ifv_mib.ifvm_mintu

/* Special flags we should propagate to parent. */
static struct {
	int flag;
	int (*func)(struct ifnet *, int);
} vlan_pflags[] = {
	{IFF_PROMISC, ifpromisc},
	{IFF_ALLMULTI, if_allmulti},
	{0, NULL}
};

SYSCTL_DECL(_net_link);
static SYSCTL_NODE(_net_link, IFT_L2VLAN, vlan, CTLFLAG_RW, 0,
    "IEEE 802.1Q VLAN");
static SYSCTL_NODE(_net_link_vlan, PF_LINK, link, CTLFLAG_RW, 0,
    "for consistency");

static VNET_DEFINE(int, soft_pad);
#define	V_soft_pad	VNET(soft_pad)
SYSCTL_INT(_net_link_vlan, OID_AUTO, soft_pad, CTLFLAG_RW | CTLFLAG_VNET,
    &VNET_NAME(soft_pad), 0, "pad short frames before tagging");

/*
 * For now, make preserving PCP via an mbuf tag optional, as it increases
 * per-packet memory allocations and frees.  In the future, it would be
 * preferable to reuse ether_vtag for this, or similar.
 */
static int vlan_mtag_pcp = 0;
SYSCTL_INT(_net_link_vlan, OID_AUTO, mtag_pcp, CTLFLAG_RW, &vlan_mtag_pcp, 0,
	"Retain VLAN PCP information as packets are passed up the stack");

static const char vlanname[] = "vlan";
static MALLOC_DEFINE(M_VLAN, vlanname, "802.1Q Virtual LAN Interface");

static eventhandler_tag ifdetach_tag;
static eventhandler_tag iflladdr_tag;

/*
 * We have a global mutex, that is used to serialize configuration
 * changes and isn't used in normal packet delivery.
 *
 * We also have a per-trunk rmlock(9), that is locked shared on packet
 * processing and exclusive when configuration is changed.
 *
 * The VLAN_ARRAY substitutes the dynamic hash with a static array
 * with 4096 entries. In theory this can give a boost in processing,
 * however on practice it does not. Probably this is because array
 * is too big to fit into CPU cache.
 */
static struct sx ifv_lock;
#define	VLAN_LOCK_INIT()	sx_init(&ifv_lock, "vlan_global")
#define	VLAN_LOCK_DESTROY()	sx_destroy(&ifv_lock)
#define	VLAN_LOCK_ASSERT()	sx_assert(&ifv_lock, SA_LOCKED)
#define	VLAN_LOCK()		sx_xlock(&ifv_lock)
#define	VLAN_UNLOCK()		sx_xunlock(&ifv_lock)
#define	TRUNK_LOCK_INIT(trunk)	rm_init(&(trunk)->lock, vlanname)
#define	TRUNK_LOCK_DESTROY(trunk) rm_destroy(&(trunk)->lock)
#define	TRUNK_LOCK(trunk)	rm_wlock(&(trunk)->lock)
#define	TRUNK_UNLOCK(trunk)	rm_wunlock(&(trunk)->lock)
#define	TRUNK_LOCK_ASSERT(trunk) rm_assert(&(trunk)->lock, RA_WLOCKED)
#define	TRUNK_RLOCK(trunk)	rm_rlock(&(trunk)->lock, &tracker)
#define	TRUNK_RUNLOCK(trunk)	rm_runlock(&(trunk)->lock, &tracker)
#define	TRUNK_LOCK_RASSERT(trunk) rm_assert(&(trunk)->lock, RA_RLOCKED)
#define	TRUNK_LOCK_READER	struct rm_priotracker tracker

#ifndef VLAN_ARRAY
static	void vlan_inithash(struct ifvlantrunk *trunk);
static	void vlan_freehash(struct ifvlantrunk *trunk);
static	int vlan_inshash(struct ifvlantrunk *trunk, struct ifvlan *ifv);
static	int vlan_remhash(struct ifvlantrunk *trunk, struct ifvlan *ifv);
static	void vlan_growhash(struct ifvlantrunk *trunk, int howmuch);
static __inline struct ifvlan * vlan_gethash(struct ifvlantrunk *trunk,
	uint16_t vid);
#endif
static	void trunk_destroy(struct ifvlantrunk *trunk);

static	void vlan_init(void *foo);
static	void vlan_input(struct ifnet *ifp, struct mbuf *m);
static	int vlan_ioctl(struct ifnet *ifp, u_long cmd, caddr_t addr);
static	void vlan_qflush(struct ifnet *ifp);
static	int vlan_setflag(struct ifnet *ifp, int flag, int status,
    int (*func)(struct ifnet *, int));
static	int vlan_setflags(struct ifnet *ifp, int status);
static	int vlan_setmulti(struct ifnet *ifp);
static	int vlan_transmit(struct ifnet *ifp, struct mbuf *m);
static	void vlan_unconfig(struct ifnet *ifp);
static	void vlan_unconfig_locked(struct ifnet *ifp, int departing);
static	int vlan_config(struct ifvlan *ifv, struct ifnet *p, uint16_t tag);
static	void vlan_link_state(struct ifnet *ifp);
static	void vlan_capabilities(struct ifvlan *ifv);
static	void vlan_trunk_capabilities(struct ifnet *ifp);

static	struct ifnet *vlan_clone_match_ethervid(const char *, int *);
static	int vlan_clone_match(struct if_clone *, const char *);
static	int vlan_clone_create(struct if_clone *, char *, size_t, caddr_t);
static	int vlan_clone_destroy(struct if_clone *, struct ifnet *);

static	void vlan_ifdetach(void *arg, struct ifnet *ifp);
static  void vlan_iflladdr(void *arg, struct ifnet *ifp);

static struct if_clone *vlan_cloner;

#ifdef VIMAGE
static VNET_DEFINE(struct if_clone *, vlan_cloner);
#define	V_vlan_cloner	VNET(vlan_cloner)
#endif

#ifndef VLAN_ARRAY
#define HASH(n, m)	((((n) >> 8) ^ ((n) >> 4) ^ (n)) & (m))

static void
vlan_inithash(struct ifvlantrunk *trunk)
{
	int i, n;
	
	/*
	 * The trunk must not be locked here since we call malloc(M_WAITOK).
	 * It is OK in case this function is called before the trunk struct
	 * gets hooked up and becomes visible from other threads.
	 */

	KASSERT(trunk->hwidth == 0 && trunk->hash == NULL,
	    ("%s: hash already initialized", __func__));

	trunk->hwidth = VLAN_DEF_HWIDTH;
	n = 1 << trunk->hwidth;
	trunk->hmask = n - 1;
	trunk->hash = malloc(sizeof(struct ifvlanhead) * n, M_VLAN, M_WAITOK);
	for (i = 0; i < n; i++)
		LIST_INIT(&trunk->hash[i]);
}

static void
vlan_freehash(struct ifvlantrunk *trunk)
{
#ifdef INVARIANTS
	int i;

	KASSERT(trunk->hwidth > 0, ("%s: hwidth not positive", __func__));
	for (i = 0; i < (1 << trunk->hwidth); i++)
		KASSERT(LIST_EMPTY(&trunk->hash[i]),
		    ("%s: hash table not empty", __func__));
#endif
	free(trunk->hash, M_VLAN);
	trunk->hash = NULL;
	trunk->hwidth = trunk->hmask = 0;
}

static int
vlan_inshash(struct ifvlantrunk *trunk, struct ifvlan *ifv)
{
	int i, b;
	struct ifvlan *ifv2;

	TRUNK_LOCK_ASSERT(trunk);
	KASSERT(trunk->hwidth > 0, ("%s: hwidth not positive", __func__));

	b = 1 << trunk->hwidth;
	i = HASH(ifv->ifv_vid, trunk->hmask);
	LIST_FOREACH(ifv2, &trunk->hash[i], ifv_list)
		if (ifv->ifv_vid == ifv2->ifv_vid)
			return (EEXIST);

	/*
	 * Grow the hash when the number of vlans exceeds half of the number of
	 * hash buckets squared. This will make the average linked-list length
	 * buckets/2.
	 */
	if (trunk->refcnt > (b * b) / 2) {
		vlan_growhash(trunk, 1);
		i = HASH(ifv->ifv_vid, trunk->hmask);
	}
	LIST_INSERT_HEAD(&trunk->hash[i], ifv, ifv_list);
	trunk->refcnt++;

	return (0);
}

static int
vlan_remhash(struct ifvlantrunk *trunk, struct ifvlan *ifv)
{
	int i, b;
	struct ifvlan *ifv2;

	TRUNK_LOCK_ASSERT(trunk);
	KASSERT(trunk->hwidth > 0, ("%s: hwidth not positive", __func__));
	
	b = 1 << trunk->hwidth;
	i = HASH(ifv->ifv_vid, trunk->hmask);
	LIST_FOREACH(ifv2, &trunk->hash[i], ifv_list)
		if (ifv2 == ifv) {
			trunk->refcnt--;
			LIST_REMOVE(ifv2, ifv_list);
			if (trunk->refcnt < (b * b) / 2)
				vlan_growhash(trunk, -1);
			return (0);
		}

	panic("%s: vlan not found\n", __func__);
	return (ENOENT); /*NOTREACHED*/
}

/*
 * Grow the hash larger or smaller if memory permits.
 */
static void
vlan_growhash(struct ifvlantrunk *trunk, int howmuch)
{
	struct ifvlan *ifv;
	struct ifvlanhead *hash2;
	int hwidth2, i, j, n, n2;

	TRUNK_LOCK_ASSERT(trunk);
	KASSERT(trunk->hwidth > 0, ("%s: hwidth not positive", __func__));

	if (howmuch == 0) {
		/* Harmless yet obvious coding error */
		printf("%s: howmuch is 0\n", __func__);
		return;
	}

	hwidth2 = trunk->hwidth + howmuch;
	n = 1 << trunk->hwidth;
	n2 = 1 << hwidth2;
	/* Do not shrink the table below the default */
	if (hwidth2 < VLAN_DEF_HWIDTH)
		return;

	/* M_NOWAIT because we're called with trunk mutex held */
	hash2 = malloc(sizeof(struct ifvlanhead) * n2, M_VLAN, M_NOWAIT);
	if (hash2 == NULL) {
		printf("%s: out of memory -- hash size not changed\n",
		    __func__);
		return;		/* We can live with the old hash table */
	}
	for (j = 0; j < n2; j++)
		LIST_INIT(&hash2[j]);
	for (i = 0; i < n; i++)
		while ((ifv = LIST_FIRST(&trunk->hash[i])) != NULL) {
			LIST_REMOVE(ifv, ifv_list);
			j = HASH(ifv->ifv_vid, n2 - 1);
			LIST_INSERT_HEAD(&hash2[j], ifv, ifv_list);
		}
	free(trunk->hash, M_VLAN);
	trunk->hash = hash2;
	trunk->hwidth = hwidth2;
	trunk->hmask = n2 - 1;

	if (bootverbose)
		if_printf(trunk->parent,
		    "VLAN hash table resized from %d to %d buckets\n", n, n2);
}

static __inline struct ifvlan *
vlan_gethash(struct ifvlantrunk *trunk, uint16_t vid)
{
	struct ifvlan *ifv;

	TRUNK_LOCK_RASSERT(trunk);

	LIST_FOREACH(ifv, &trunk->hash[HASH(vid, trunk->hmask)], ifv_list)
		if (ifv->ifv_vid == vid)
			return (ifv);
	return (NULL);
}

#if 0
/* Debugging code to view the hashtables. */
static void
vlan_dumphash(struct ifvlantrunk *trunk)
{
	int i;
	struct ifvlan *ifv;

	for (i = 0; i < (1 << trunk->hwidth); i++) {
		printf("%d: ", i);
		LIST_FOREACH(ifv, &trunk->hash[i], ifv_list)
			printf("%s ", ifv->ifv_ifp->if_xname);
		printf("\n");
	}
}
#endif /* 0 */
#else

static __inline struct ifvlan *
vlan_gethash(struct ifvlantrunk *trunk, uint16_t vid)
{

	return trunk->vlans[vid];
}

static __inline int
vlan_inshash(struct ifvlantrunk *trunk, struct ifvlan *ifv)
{

	if (trunk->vlans[ifv->ifv_vid] != NULL)
		return EEXIST;
	trunk->vlans[ifv->ifv_vid] = ifv;
	trunk->refcnt++;

	return (0);
}

static __inline int
vlan_remhash(struct ifvlantrunk *trunk, struct ifvlan *ifv)
{

	trunk->vlans[ifv->ifv_vid] = NULL;
	trunk->refcnt--;

	return (0);
}

static __inline void
vlan_freehash(struct ifvlantrunk *trunk)
{
}

static __inline void
vlan_inithash(struct ifvlantrunk *trunk)
{
}

#endif /* !VLAN_ARRAY */

static void
trunk_destroy(struct ifvlantrunk *trunk)
{
	VLAN_LOCK_ASSERT();

	TRUNK_LOCK(trunk);
	vlan_freehash(trunk);
	trunk->parent->if_vlantrunk = NULL;
	TRUNK_UNLOCK(trunk);
	TRUNK_LOCK_DESTROY(trunk);
	free(trunk, M_VLAN);
}

/*
 * Program our multicast filter. What we're actually doing is
 * programming the multicast filter of the parent. This has the
 * side effect of causing the parent interface to receive multicast
 * traffic that it doesn't really want, which ends up being discarded
 * later by the upper protocol layers. Unfortunately, there's no way
 * to avoid this: there really is only one physical interface.
 */
static int
vlan_setmulti(struct ifnet *ifp)
{
	struct ifnet		*ifp_p;
	struct ifmultiaddr	*ifma;
	struct ifvlan		*sc;
	struct vlan_mc_entry	*mc;
	int			error;

	/* Find the parent. */
	sc = ifp->if_softc;
	TRUNK_LOCK_ASSERT(TRUNK(sc));
	ifp_p = PARENT(sc);

	CURVNET_SET_QUIET(ifp_p->if_vnet);

	/* First, remove any existing filter entries. */
	while ((mc = SLIST_FIRST(&sc->vlan_mc_listhead)) != NULL) {
		SLIST_REMOVE_HEAD(&sc->vlan_mc_listhead, mc_entries);
		(void)if_delmulti(ifp_p, (struct sockaddr *)&mc->mc_addr);
		free(mc, M_VLAN);
	}

	/* Now program new ones. */
	IF_ADDR_WLOCK(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		mc = malloc(sizeof(struct vlan_mc_entry), M_VLAN, M_NOWAIT);
		if (mc == NULL) {
			IF_ADDR_WUNLOCK(ifp);
			return (ENOMEM);
		}
		bcopy(ifma->ifma_addr, &mc->mc_addr, ifma->ifma_addr->sa_len);
		mc->mc_addr.sdl_index = ifp_p->if_index;
		SLIST_INSERT_HEAD(&sc->vlan_mc_listhead, mc, mc_entries);
	}
	IF_ADDR_WUNLOCK(ifp);
	SLIST_FOREACH (mc, &sc->vlan_mc_listhead, mc_entries) {
		error = if_addmulti(ifp_p, (struct sockaddr *)&mc->mc_addr,
		    NULL);
		if (error)
			return (error);
	}

	CURVNET_RESTORE();
	return (0);
}

/*
 * A handler for parent interface link layer address changes.
 * If the parent interface link layer address is changed we
 * should also change it on all children vlans.
 */
static void
vlan_iflladdr(void *arg __unused, struct ifnet *ifp)
{
	struct ifvlan *ifv;
#ifndef VLAN_ARRAY
	struct ifvlan *next;
#endif
	int i;

	/*
	 * Check if it's a trunk interface first of all
	 * to avoid needless locking.
	 */
	if (ifp->if_vlantrunk == NULL)
		return;

	VLAN_LOCK();
	/*
	 * OK, it's a trunk.  Loop over and change all vlan's lladdrs on it.
	 */
#ifdef VLAN_ARRAY
	for (i = 0; i < VLAN_ARRAY_SIZE; i++)
		if ((ifv = ifp->if_vlantrunk->vlans[i])) {
#else /* VLAN_ARRAY */
	for (i = 0; i < (1 << ifp->if_vlantrunk->hwidth); i++)
		LIST_FOREACH_SAFE(ifv, &ifp->if_vlantrunk->hash[i], ifv_list, next) {
#endif /* VLAN_ARRAY */
			VLAN_UNLOCK();
			if_setlladdr(ifv->ifv_ifp, IF_LLADDR(ifp),
			    ifp->if_addrlen);
			VLAN_LOCK();
		}
	VLAN_UNLOCK();

}

/*
 * A handler for network interface departure events.
 * Track departure of trunks here so that we don't access invalid
 * pointers or whatever if a trunk is ripped from under us, e.g.,
 * by ejecting its hot-plug card.  However, if an ifnet is simply
 * being renamed, then there's no need to tear down the state.
 */
static void
vlan_ifdetach(void *arg __unused, struct ifnet *ifp)
{
	struct ifvlan *ifv;
	int i;

	/*
	 * Check if it's a trunk interface first of all
	 * to avoid needless locking.
	 */
	if (ifp->if_vlantrunk == NULL)
		return;

	/* If the ifnet is just being renamed, don't do anything. */
	if (ifp->if_flags & IFF_RENAMING)
		return;

	VLAN_LOCK();
	/*
	 * OK, it's a trunk.  Loop over and detach all vlan's on it.
	 * Check trunk pointer after each vlan_unconfig() as it will
	 * free it and set to NULL after the last vlan was detached.
	 */
#ifdef VLAN_ARRAY
	for (i = 0; i < VLAN_ARRAY_SIZE; i++)
		if ((ifv = ifp->if_vlantrunk->vlans[i])) {
			vlan_unconfig_locked(ifv->ifv_ifp, 1);
			if (ifp->if_vlantrunk == NULL)
				break;
		}
#else /* VLAN_ARRAY */
restart:
	for (i = 0; i < (1 << ifp->if_vlantrunk->hwidth); i++)
		if ((ifv = LIST_FIRST(&ifp->if_vlantrunk->hash[i]))) {
			vlan_unconfig_locked(ifv->ifv_ifp, 1);
			if (ifp->if_vlantrunk)
				goto restart;	/* trunk->hwidth can change */
			else
				break;
		}
#endif /* VLAN_ARRAY */
	/* Trunk should have been destroyed in vlan_unconfig(). */
	KASSERT(ifp->if_vlantrunk == NULL, ("%s: purge failed", __func__));
	VLAN_UNLOCK();
}

/*
 * Return the trunk device for a virtual interface.
 */
static struct ifnet  *
vlan_trunkdev(struct ifnet *ifp)
{
	struct ifvlan *ifv;

	if (ifp->if_type != IFT_L2VLAN)
		return (NULL);
	ifv = ifp->if_softc;
	ifp = NULL;
	VLAN_LOCK();
	if (ifv->ifv_trunk)
		ifp = PARENT(ifv);
	VLAN_UNLOCK();
	return (ifp);
}

/*
 * Return the 12-bit VLAN VID for this interface, for use by external
 * components such as Infiniband.
 *
 * XXXRW: Note that the function name here is historical; it should be named
 * vlan_vid().
 */
static int
vlan_tag(struct ifnet *ifp, uint16_t *vidp)
{
	struct ifvlan *ifv;

	if (ifp->if_type != IFT_L2VLAN)
		return (EINVAL);
	ifv = ifp->if_softc;
	*vidp = ifv->ifv_vid;
	return (0);
}

/*
 * Return a driver specific cookie for this interface.  Synchronization
 * with setcookie must be provided by the driver. 
 */
static void *
vlan_cookie(struct ifnet *ifp)
{
	struct ifvlan *ifv;

	if (ifp->if_type != IFT_L2VLAN)
		return (NULL);
	ifv = ifp->if_softc;
	return (ifv->ifv_cookie);
}

/*
 * Store a cookie in our softc that drivers can use to store driver
 * private per-instance data in.
 */
static int
vlan_setcookie(struct ifnet *ifp, void *cookie)
{
	struct ifvlan *ifv;

	if (ifp->if_type != IFT_L2VLAN)
		return (EINVAL);
	ifv = ifp->if_softc;
	ifv->ifv_cookie = cookie;
	return (0);
}

/*
 * Return the vlan device present at the specific VID.
 */
static struct ifnet *
vlan_devat(struct ifnet *ifp, uint16_t vid)
{
	struct ifvlantrunk *trunk;
	struct ifvlan *ifv;
	TRUNK_LOCK_READER;

	trunk = ifp->if_vlantrunk;
	if (trunk == NULL)
		return (NULL);
	ifp = NULL;
	TRUNK_RLOCK(trunk);
	ifv = vlan_gethash(trunk, vid);
	if (ifv)
		ifp = ifv->ifv_ifp;
	TRUNK_RUNLOCK(trunk);
	return (ifp);
}

/*
 * Recalculate the cached VLAN tag exposed via the MIB.
 */
static void
vlan_tag_recalculate(struct ifvlan *ifv)
{

       ifv->ifv_tag = EVL_MAKETAG(ifv->ifv_vid, ifv->ifv_pcp, 0);
}

/*
 * VLAN support can be loaded as a module.  The only place in the
 * system that's intimately aware of this is ether_input.  We hook
 * into this code through vlan_input_p which is defined there and
 * set here.  No one else in the system should be aware of this so
 * we use an explicit reference here.
 */
extern	void (*vlan_input_p)(struct ifnet *, struct mbuf *);

/* For if_link_state_change() eyes only... */
extern	void (*vlan_link_state_p)(struct ifnet *);

static int
vlan_modevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		ifdetach_tag = EVENTHANDLER_REGISTER(ifnet_departure_event,
		    vlan_ifdetach, NULL, EVENTHANDLER_PRI_ANY);
		if (ifdetach_tag == NULL)
			return (ENOMEM);
		iflladdr_tag = EVENTHANDLER_REGISTER(iflladdr_event,
		    vlan_iflladdr, NULL, EVENTHANDLER_PRI_ANY);
		if (iflladdr_tag == NULL)
			return (ENOMEM);
		VLAN_LOCK_INIT();
		vlan_input_p = vlan_input;
		vlan_link_state_p = vlan_link_state;
		vlan_trunk_cap_p = vlan_trunk_capabilities;
		vlan_trunkdev_p = vlan_trunkdev;
		vlan_cookie_p = vlan_cookie;
		vlan_setcookie_p = vlan_setcookie;
		vlan_tag_p = vlan_tag;
		vlan_devat_p = vlan_devat;
#ifndef VIMAGE
		vlan_cloner = if_clone_advanced(vlanname, 0, vlan_clone_match,
		    vlan_clone_create, vlan_clone_destroy);
#endif
		if (bootverbose)
			printf("vlan: initialized, using "
#ifdef VLAN_ARRAY
			       "full-size arrays"
#else
			       "hash tables with chaining"
#endif
			
			       "\n");
		break;
	case MOD_UNLOAD:
#ifndef VIMAGE
		if_clone_detach(vlan_cloner);
#endif
		EVENTHANDLER_DEREGISTER(ifnet_departure_event, ifdetach_tag);
		EVENTHANDLER_DEREGISTER(iflladdr_event, iflladdr_tag);
		vlan_input_p = NULL;
		vlan_link_state_p = NULL;
		vlan_trunk_cap_p = NULL;
		vlan_trunkdev_p = NULL;
		vlan_tag_p = NULL;
		vlan_cookie_p = NULL;
		vlan_setcookie_p = NULL;
		vlan_devat_p = NULL;
		VLAN_LOCK_DESTROY();
		if (bootverbose)
			printf("vlan: unloaded\n");
		break;
	default:
		return (EOPNOTSUPP);
	}
	return (0);
}

static moduledata_t vlan_mod = {
	"if_vlan",
	vlan_modevent,
	0
};

DECLARE_MODULE(if_vlan, vlan_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(if_vlan, 3);

#ifdef VIMAGE
static void
vnet_vlan_init(const void *unused __unused)
{

	vlan_cloner = if_clone_advanced(vlanname, 0, vlan_clone_match,
		    vlan_clone_create, vlan_clone_destroy);
	V_vlan_cloner = vlan_cloner;
}
VNET_SYSINIT(vnet_vlan_init, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY,
    vnet_vlan_init, NULL);

static void
vnet_vlan_uninit(const void *unused __unused)
{

	if_clone_detach(V_vlan_cloner);
}
VNET_SYSUNINIT(vnet_vlan_uninit, SI_SUB_INIT_IF, SI_ORDER_FIRST,
    vnet_vlan_uninit, NULL);
#endif

/*
 * Check for <etherif>.<vlan> style interface names.
 */
static struct ifnet *
vlan_clone_match_ethervid(const char *name, int *vidp)
{
	char ifname[IFNAMSIZ];
	char *cp;
	struct ifnet *ifp;
	int vid;

	strlcpy(ifname, name, IFNAMSIZ);
	if ((cp = strchr(ifname, '.')) == NULL)
		return (NULL);
	*cp = '\0';
	if ((ifp = ifunit(ifname)) == NULL)
		return (NULL);
	/* Parse VID. */
	if (*++cp == '\0')
		return (NULL);
	vid = 0;
	for(; *cp >= '0' && *cp <= '9'; cp++)
		vid = (vid * 10) + (*cp - '0');
	if (*cp != '\0')
		return (NULL);
	if (vidp != NULL)
		*vidp = vid;

	return (ifp);
}

static int
vlan_clone_match(struct if_clone *ifc, const char *name)
{
	const char *cp;

	if (vlan_clone_match_ethervid(name, NULL) != NULL)
		return (1);

	if (strncmp(vlanname, name, strlen(vlanname)) != 0)
		return (0);
	for (cp = name + 4; *cp != '\0'; cp++) {
		if (*cp < '0' || *cp > '9')
			return (0);
	}

	return (1);
}

static int
vlan_clone_create(struct if_clone *ifc, char *name, size_t len, caddr_t params)
{
	char *dp;
	int wildcard;
	int unit;
	int error;
	int vid;
	int ethertag;
	struct ifvlan *ifv;
	struct ifnet *ifp;
	struct ifnet *p;
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;
	struct vlanreq vlr;
	static const u_char eaddr[ETHER_ADDR_LEN];	/* 00:00:00:00:00:00 */

	/*
	 * There are 3 (ugh) ways to specify the cloned device:
	 * o pass a parameter block with the clone request.
	 * o specify parameters in the text of the clone device name
	 * o specify no parameters and get an unattached device that
	 *   must be configured separately.
	 * The first technique is preferred; the latter two are
	 * supported for backwards compatibility.
	 *
	 * XXXRW: Note historic use of the word "tag" here.  New ioctls may be
	 * called for.
	 */
	if (params) {
		error = copyin(params, &vlr, sizeof(vlr));
		if (error)
			return error;
		p = ifunit(vlr.vlr_parent);
		if (p == NULL)
			return (ENXIO);
		error = ifc_name2unit(name, &unit);
		if (error != 0)
			return (error);

		ethertag = 1;
		vid = vlr.vlr_tag;
		wildcard = (unit < 0);
	} else if ((p = vlan_clone_match_ethervid(name, &vid)) != NULL) {
		ethertag = 1;
		unit = -1;
		wildcard = 0;
	} else {
		ethertag = 0;

		error = ifc_name2unit(name, &unit);
		if (error != 0)
			return (error);

		wildcard = (unit < 0);
	}

	error = ifc_alloc_unit(ifc, &unit);
	if (error != 0)
		return (error);

	/* In the wildcard case, we need to update the name. */
	if (wildcard) {
		for (dp = name; *dp != '\0'; dp++);
		if (snprintf(dp, len - (dp-name), "%d", unit) >
		    len - (dp-name) - 1) {
			panic("%s: interface name too long", __func__);
		}
	}

	ifv = malloc(sizeof(struct ifvlan), M_VLAN, M_WAITOK | M_ZERO);
	ifp = ifv->ifv_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		ifc_free_unit(ifc, unit);
		free(ifv, M_VLAN);
		return (ENOSPC);
	}
	SLIST_INIT(&ifv->vlan_mc_listhead);
	ifp->if_softc = ifv;
	/*
	 * Set the name manually rather than using if_initname because
	 * we don't conform to the default naming convention for interfaces.
	 */
	strlcpy(ifp->if_xname, name, IFNAMSIZ);
	ifp->if_dname = vlanname;
	ifp->if_dunit = unit;
	/* NB: flags are not set here */
	ifp->if_linkmib = &ifv->ifv_mib;
	ifp->if_linkmiblen = sizeof(ifv->ifv_mib);
	/* NB: mtu is not set here */

	ifp->if_init = vlan_init;
	ifp->if_transmit = vlan_transmit;
	ifp->if_qflush = vlan_qflush;
	ifp->if_ioctl = vlan_ioctl;
	ifp->if_flags = VLAN_IFFLAGS;
	ether_ifattach(ifp, eaddr);
	/* Now undo some of the damage... */
	ifp->if_baudrate = 0;
	ifp->if_type = IFT_L2VLAN;
	ifp->if_hdrlen = ETHER_VLAN_ENCAP_LEN;
	ifa = ifp->if_addr;
	sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	sdl->sdl_type = IFT_L2VLAN;

	if (ethertag) {
		error = vlan_config(ifv, p, vid);
		if (error != 0) {
			/*
			 * Since we've partially failed, we need to back
			 * out all the way, otherwise userland could get
			 * confused.  Thus, we destroy the interface.
			 */
			ether_ifdetach(ifp);
			vlan_unconfig(ifp);
			if_free(ifp);
			ifc_free_unit(ifc, unit);
			free(ifv, M_VLAN);

			return (error);
		}

		/* Update flags on the parent, if necessary. */
		vlan_setflags(ifp, 1);
	}

	return (0);
}

static int
vlan_clone_destroy(struct if_clone *ifc, struct ifnet *ifp)
{
	struct ifvlan *ifv = ifp->if_softc;
	int unit = ifp->if_dunit;

	ether_ifdetach(ifp);	/* first, remove it from system-wide lists */
	vlan_unconfig(ifp);	/* now it can be unconfigured and freed */
	if_free(ifp);
	free(ifv, M_VLAN);
	ifc_free_unit(ifc, unit);

	return (0);
}

/*
 * The ifp->if_init entry point for vlan(4) is a no-op.
 */
static void
vlan_init(void *foo __unused)
{
}

/*
 * The if_transmit method for vlan(4) interface.
 */
static int
vlan_transmit(struct ifnet *ifp, struct mbuf *m)
{
	struct ifvlan *ifv;
	struct ifnet *p;
	struct m_tag *mtag;
	uint16_t tag;
	int error, len, mcast;

	ifv = ifp->if_softc;
	p = PARENT(ifv);
	len = m->m_pkthdr.len;
	mcast = (m->m_flags & (M_MCAST | M_BCAST)) ? 1 : 0;

	BPF_MTAP(ifp, m);

	/*
	 * Do not run parent's if_transmit() if the parent is not up,
	 * or parent's driver will cause a system crash.
	 */
	if (!UP_AND_RUNNING(p)) {
		m_freem(m);
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
		return (ENETDOWN);
	}

	/*
	 * Pad the frame to the minimum size allowed if told to.
	 * This option is in accord with IEEE Std 802.1Q, 2003 Ed.,
	 * paragraph C.4.4.3.b.  It can help to work around buggy
	 * bridges that violate paragraph C.4.4.3.a from the same
	 * document, i.e., fail to pad short frames after untagging.
	 * E.g., a tagged frame 66 bytes long (incl. FCS) is OK, but
	 * untagging it will produce a 62-byte frame, which is a runt
	 * and requires padding.  There are VLAN-enabled network
	 * devices that just discard such runts instead or mishandle
	 * them somehow.
	 */
	if (V_soft_pad && p->if_type == IFT_ETHER) {
		static char pad[8];	/* just zeros */
		int n;

		for (n = ETHERMIN + ETHER_HDR_LEN - m->m_pkthdr.len;
		     n > 0; n -= sizeof(pad))
			if (!m_append(m, min(n, sizeof(pad)), pad))
				break;

		if (n > 0) {
			if_printf(ifp, "cannot pad short frame\n");
			if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
			m_freem(m);
			return (0);
		}
	}

	/*
	 * If underlying interface can do VLAN tag insertion itself,
	 * just pass the packet along. However, we need some way to
	 * tell the interface where the packet came from so that it
	 * knows how to find the VLAN tag to use, so we attach a
	 * packet tag that holds it.
	 */
	if (vlan_mtag_pcp && (mtag = m_tag_locate(m, MTAG_8021Q,
	    MTAG_8021Q_PCP_OUT, NULL)) != NULL)
		tag = EVL_MAKETAG(ifv->ifv_vid, *(uint8_t *)(mtag + 1), 0);
	else
              tag = ifv->ifv_tag;
	if (p->if_capenable & IFCAP_VLAN_HWTAGGING) {
		m->m_pkthdr.ether_vtag = tag;
		m->m_flags |= M_VLANTAG;
	} else {
		m = ether_vlanencap(m, tag);
		if (m == NULL) {
			if_printf(ifp, "unable to prepend VLAN header\n");
			if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
			return (0);
		}
	}

	/*
	 * Send it, precisely as ether_output() would have.
	 */
	error = (p->if_transmit)(p, m);
	if (error == 0) {
		if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);
		if_inc_counter(ifp, IFCOUNTER_OBYTES, len);
		if_inc_counter(ifp, IFCOUNTER_OMCASTS, mcast);
	} else
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
	return (error);
}

/*
 * The ifp->if_qflush entry point for vlan(4) is a no-op.
 */
static void
vlan_qflush(struct ifnet *ifp __unused)
{
}

static void
vlan_input(struct ifnet *ifp, struct mbuf *m)
{
	struct ifvlantrunk *trunk = ifp->if_vlantrunk;
	struct ifvlan *ifv;
	TRUNK_LOCK_READER;
	struct m_tag *mtag;
	uint16_t vid, tag;

	KASSERT(trunk != NULL, ("%s: no trunk", __func__));

	if (m->m_flags & M_VLANTAG) {
		/*
		 * Packet is tagged, but m contains a normal
		 * Ethernet frame; the tag is stored out-of-band.
		 */
		tag = m->m_pkthdr.ether_vtag;
		m->m_flags &= ~M_VLANTAG;
	} else {
		struct ether_vlan_header *evl;

		/*
		 * Packet is tagged in-band as specified by 802.1q.
		 */
		switch (ifp->if_type) {
		case IFT_ETHER:
			if (m->m_len < sizeof(*evl) &&
			    (m = m_pullup(m, sizeof(*evl))) == NULL) {
				if_printf(ifp, "cannot pullup VLAN header\n");
				return;
			}
			evl = mtod(m, struct ether_vlan_header *);
			tag = ntohs(evl->evl_tag);

			/*
			 * Remove the 802.1q header by copying the Ethernet
			 * addresses over it and adjusting the beginning of
			 * the data in the mbuf.  The encapsulated Ethernet
			 * type field is already in place.
			 */
			bcopy((char *)evl, (char *)evl + ETHER_VLAN_ENCAP_LEN,
			      ETHER_HDR_LEN - ETHER_TYPE_LEN);
			m_adj(m, ETHER_VLAN_ENCAP_LEN);
			break;

		default:
#ifdef INVARIANTS
			panic("%s: %s has unsupported if_type %u",
			      __func__, ifp->if_xname, ifp->if_type);
#endif
			m_freem(m);
			if_inc_counter(ifp, IFCOUNTER_NOPROTO, 1);
			return;
		}
	}

	vid = EVL_VLANOFTAG(tag);

	TRUNK_RLOCK(trunk);
	ifv = vlan_gethash(trunk, vid);
	if (ifv == NULL || !UP_AND_RUNNING(ifv->ifv_ifp)) {
		TRUNK_RUNLOCK(trunk);
		m_freem(m);
		if_inc_counter(ifp, IFCOUNTER_NOPROTO, 1);
		return;
	}
	TRUNK_RUNLOCK(trunk);

	if (vlan_mtag_pcp) {
		/*
		 * While uncommon, it is possible that we will find a 802.1q
		 * packet encapsulated inside another packet that also had an
		 * 802.1q header.  For example, ethernet tunneled over IPSEC
		 * arriving over ethernet.  In that case, we replace the
		 * existing 802.1q PCP m_tag value.
		 */
		mtag = m_tag_locate(m, MTAG_8021Q, MTAG_8021Q_PCP_IN, NULL);
		if (mtag == NULL) {
			mtag = m_tag_alloc(MTAG_8021Q, MTAG_8021Q_PCP_IN,
			    sizeof(uint8_t), M_NOWAIT);
			if (mtag == NULL) {
				m_freem(m);
				if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
				return;
			}
			m_tag_prepend(m, mtag);
		}
		*(uint8_t *)(mtag + 1) = EVL_PRIOFTAG(tag);
	}

	m->m_pkthdr.rcvif = ifv->ifv_ifp;
	if_inc_counter(ifv->ifv_ifp, IFCOUNTER_IPACKETS, 1);

	/* Pass it back through the parent's input routine. */
	(*ifp->if_input)(ifv->ifv_ifp, m);
}

static int
vlan_config(struct ifvlan *ifv, struct ifnet *p, uint16_t vid)
{
	struct ifvlantrunk *trunk;
	struct ifnet *ifp;
	int error = 0;

	/*
	 * We can handle non-ethernet hardware types as long as
	 * they handle the tagging and headers themselves.
	 */
	if (p->if_type != IFT_ETHER &&
	    (p->if_capenable & IFCAP_VLAN_HWTAGGING) == 0)
		return (EPROTONOSUPPORT);
	if ((p->if_flags & VLAN_IFFLAGS) != VLAN_IFFLAGS)
		return (EPROTONOSUPPORT);
	/*
	 * Don't let the caller set up a VLAN VID with
	 * anything except VLID bits.
	 * VID numbers 0x0 and 0xFFF are reserved.
	 */
	if (vid == 0 || vid == 0xFFF || (vid & ~EVL_VLID_MASK))
		return (EINVAL);
	if (ifv->ifv_trunk)
		return (EBUSY);

	if (p->if_vlantrunk == NULL) {
		trunk = malloc(sizeof(struct ifvlantrunk),
		    M_VLAN, M_WAITOK | M_ZERO);
		vlan_inithash(trunk);
		VLAN_LOCK();
		if (p->if_vlantrunk != NULL) {
			/* A race that is very unlikely to be hit. */
			vlan_freehash(trunk);
			free(trunk, M_VLAN);
			goto exists;
		}
		TRUNK_LOCK_INIT(trunk);
		TRUNK_LOCK(trunk);
		p->if_vlantrunk = trunk;
		trunk->parent = p;
	} else {
		VLAN_LOCK();
exists:
		trunk = p->if_vlantrunk;
		TRUNK_LOCK(trunk);
	}

	ifv->ifv_vid = vid;	/* must set this before vlan_inshash() */
	ifv->ifv_pcp = 0;       /* Default: best effort delivery. */
	vlan_tag_recalculate(ifv);
	error = vlan_inshash(trunk, ifv);
	if (error)
		goto done;
	ifv->ifv_proto = ETHERTYPE_VLAN;
	ifv->ifv_encaplen = ETHER_VLAN_ENCAP_LEN;
	ifv->ifv_mintu = ETHERMIN;
	ifv->ifv_pflags = 0;

	/*
	 * If the parent supports the VLAN_MTU capability,
	 * i.e. can Tx/Rx larger than ETHER_MAX_LEN frames,
	 * use it.
	 */
	if (p->if_capenable & IFCAP_VLAN_MTU) {
		/*
		 * No need to fudge the MTU since the parent can
		 * handle extended frames.
		 */
		ifv->ifv_mtufudge = 0;
	} else {
		/*
		 * Fudge the MTU by the encapsulation size.  This
		 * makes us incompatible with strictly compliant
		 * 802.1Q implementations, but allows us to use
		 * the feature with other NetBSD implementations,
		 * which might still be useful.
		 */
		ifv->ifv_mtufudge = ifv->ifv_encaplen;
	}

	ifv->ifv_trunk = trunk;
	ifp = ifv->ifv_ifp;
	/*
	 * Initialize fields from our parent.  This duplicates some
	 * work with ether_ifattach() but allows for non-ethernet
	 * interfaces to also work.
	 */
	ifp->if_mtu = p->if_mtu - ifv->ifv_mtufudge;
	ifp->if_baudrate = p->if_baudrate;
	ifp->if_output = p->if_output;
	ifp->if_input = p->if_input;
	ifp->if_resolvemulti = p->if_resolvemulti;
	ifp->if_addrlen = p->if_addrlen;
	ifp->if_broadcastaddr = p->if_broadcastaddr;

	/*
	 * Copy only a selected subset of flags from the parent.
	 * Other flags are none of our business.
	 */
#define VLAN_COPY_FLAGS (IFF_SIMPLEX)
	ifp->if_flags &= ~VLAN_COPY_FLAGS;
	ifp->if_flags |= p->if_flags & VLAN_COPY_FLAGS;
#undef VLAN_COPY_FLAGS

	ifp->if_link_state = p->if_link_state;

	vlan_capabilities(ifv);

	/*
	 * Set up our interface address to reflect the underlying
	 * physical interface's.
	 */
	bcopy(IF_LLADDR(p), IF_LLADDR(ifp), p->if_addrlen);
	((struct sockaddr_dl *)ifp->if_addr->ifa_addr)->sdl_alen =
	    p->if_addrlen;

	/*
	 * Configure multicast addresses that may already be
	 * joined on the vlan device.
	 */
	(void)vlan_setmulti(ifp); /* XXX: VLAN lock held */

	/* We are ready for operation now. */
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
done:
	TRUNK_UNLOCK(trunk);
	if (error == 0)
		EVENTHANDLER_INVOKE(vlan_config, p, ifv->ifv_vid);
	VLAN_UNLOCK();

	return (error);
}

static void
vlan_unconfig(struct ifnet *ifp)
{

	VLAN_LOCK();
	vlan_unconfig_locked(ifp, 0);
	VLAN_UNLOCK();
}

static void
vlan_unconfig_locked(struct ifnet *ifp, int departing)
{
	struct ifvlantrunk *trunk;
	struct vlan_mc_entry *mc;
	struct ifvlan *ifv;
	struct ifnet  *parent;
	int error;

	VLAN_LOCK_ASSERT();

	ifv = ifp->if_softc;
	trunk = ifv->ifv_trunk;
	parent = NULL;

	if (trunk != NULL) {

		TRUNK_LOCK(trunk);
		parent = trunk->parent;

		/*
		 * Since the interface is being unconfigured, we need to
		 * empty the list of multicast groups that we may have joined
		 * while we were alive from the parent's list.
		 */
		while ((mc = SLIST_FIRST(&ifv->vlan_mc_listhead)) != NULL) {
			/*
			 * If the parent interface is being detached,
			 * all its multicast addresses have already
			 * been removed.  Warn about errors if
			 * if_delmulti() does fail, but don't abort as
			 * all callers expect vlan destruction to
			 * succeed.
			 */
			if (!departing) {
				error = if_delmulti(parent,
				    (struct sockaddr *)&mc->mc_addr);
				if (error)
					if_printf(ifp,
		    "Failed to delete multicast address from parent: %d\n",
					    error);
			}
			SLIST_REMOVE_HEAD(&ifv->vlan_mc_listhead, mc_entries);
			free(mc, M_VLAN);
		}

		vlan_setflags(ifp, 0); /* clear special flags on parent */
		vlan_remhash(trunk, ifv);
		ifv->ifv_trunk = NULL;

		/*
		 * Check if we were the last.
		 */
		if (trunk->refcnt == 0) {
			parent->if_vlantrunk = NULL;
			/*
			 * XXXGL: If some ithread has already entered
			 * vlan_input() and is now blocked on the trunk
			 * lock, then it should preempt us right after
			 * unlock and finish its work. Then we will acquire
			 * lock again in trunk_destroy().
			 */
			TRUNK_UNLOCK(trunk);
			trunk_destroy(trunk);
		} else
			TRUNK_UNLOCK(trunk);
	}

	/* Disconnect from parent. */
	if (ifv->ifv_pflags)
		if_printf(ifp, "%s: ifv_pflags unclean\n", __func__);
	ifp->if_mtu = ETHERMTU;
	ifp->if_link_state = LINK_STATE_UNKNOWN;
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;

	/*
	 * Only dispatch an event if vlan was
	 * attached, otherwise there is nothing
	 * to cleanup anyway.
	 */
	if (parent != NULL)
		EVENTHANDLER_INVOKE(vlan_unconfig, parent, ifv->ifv_vid);
}

/* Handle a reference counted flag that should be set on the parent as well */
static int
vlan_setflag(struct ifnet *ifp, int flag, int status,
	     int (*func)(struct ifnet *, int))
{
	struct ifvlan *ifv;
	int error;

	/* XXX VLAN_LOCK_ASSERT(); */

	ifv = ifp->if_softc;
	status = status ? (ifp->if_flags & flag) : 0;
	/* Now "status" contains the flag value or 0 */

	/*
	 * See if recorded parent's status is different from what
	 * we want it to be.  If it is, flip it.  We record parent's
	 * status in ifv_pflags so that we won't clear parent's flag
	 * we haven't set.  In fact, we don't clear or set parent's
	 * flags directly, but get or release references to them.
	 * That's why we can be sure that recorded flags still are
	 * in accord with actual parent's flags.
	 */
	if (status != (ifv->ifv_pflags & flag)) {
		error = (*func)(PARENT(ifv), status);
		if (error)
			return (error);
		ifv->ifv_pflags &= ~flag;
		ifv->ifv_pflags |= status;
	}
	return (0);
}

/*
 * Handle IFF_* flags that require certain changes on the parent:
 * if "status" is true, update parent's flags respective to our if_flags;
 * if "status" is false, forcedly clear the flags set on parent.
 */
static int
vlan_setflags(struct ifnet *ifp, int status)
{
	int error, i;
	
	for (i = 0; vlan_pflags[i].flag; i++) {
		error = vlan_setflag(ifp, vlan_pflags[i].flag,
				     status, vlan_pflags[i].func);
		if (error)
			return (error);
	}
	return (0);
}

/* Inform all vlans that their parent has changed link state */
static void
vlan_link_state(struct ifnet *ifp)
{
	struct ifvlantrunk *trunk = ifp->if_vlantrunk;
	struct ifvlan *ifv;
	int i;

	TRUNK_LOCK(trunk);
#ifdef VLAN_ARRAY
	for (i = 0; i < VLAN_ARRAY_SIZE; i++)
		if (trunk->vlans[i] != NULL) {
			ifv = trunk->vlans[i];
#else
	for (i = 0; i < (1 << trunk->hwidth); i++)
		LIST_FOREACH(ifv, &trunk->hash[i], ifv_list) {
#endif
			ifv->ifv_ifp->if_baudrate = trunk->parent->if_baudrate;
			if_link_state_change(ifv->ifv_ifp,
			    trunk->parent->if_link_state);
		}
	TRUNK_UNLOCK(trunk);
}

static void
vlan_capabilities(struct ifvlan *ifv)
{
	struct ifnet *p = PARENT(ifv);
	struct ifnet *ifp = ifv->ifv_ifp;
	struct ifnet_hw_tsomax hw_tsomax;

	TRUNK_LOCK_ASSERT(TRUNK(ifv));

	/*
	 * If the parent interface can do checksum offloading
	 * on VLANs, then propagate its hardware-assisted
	 * checksumming flags. Also assert that checksum
	 * offloading requires hardware VLAN tagging.
	 */
	if (p->if_capabilities & IFCAP_VLAN_HWCSUM)
		ifp->if_capabilities = p->if_capabilities & IFCAP_HWCSUM;

	if (p->if_capenable & IFCAP_VLAN_HWCSUM &&
	    p->if_capenable & IFCAP_VLAN_HWTAGGING) {
		ifp->if_capenable = p->if_capenable & IFCAP_HWCSUM;
		ifp->if_hwassist = p->if_hwassist & (CSUM_IP | CSUM_TCP |
		    CSUM_UDP | CSUM_SCTP);
	} else {
		ifp->if_capenable = 0;
		ifp->if_hwassist = 0;
	}
	/*
	 * If the parent interface can do TSO on VLANs then
	 * propagate the hardware-assisted flag. TSO on VLANs
	 * does not necessarily require hardware VLAN tagging.
	 */
	memset(&hw_tsomax, 0, sizeof(hw_tsomax));
	if_hw_tsomax_common(p, &hw_tsomax);
	if_hw_tsomax_update(ifp, &hw_tsomax);
	if (p->if_capabilities & IFCAP_VLAN_HWTSO)
		ifp->if_capabilities |= p->if_capabilities & IFCAP_TSO;
	if (p->if_capenable & IFCAP_VLAN_HWTSO) {
		ifp->if_capenable |= p->if_capenable & IFCAP_TSO;
		ifp->if_hwassist |= p->if_hwassist & CSUM_TSO;
	} else {
		ifp->if_capenable &= ~(p->if_capenable & IFCAP_TSO);
		ifp->if_hwassist &= ~(p->if_hwassist & CSUM_TSO);
	}

	/*
	 * If the parent interface can offload TCP connections over VLANs then
	 * propagate its TOE capability to the VLAN interface.
	 *
	 * All TOE drivers in the tree today can deal with VLANs.  If this
	 * changes then IFCAP_VLAN_TOE should be promoted to a full capability
	 * with its own bit.
	 */
#define	IFCAP_VLAN_TOE IFCAP_TOE
	if (p->if_capabilities & IFCAP_VLAN_TOE)
		ifp->if_capabilities |= p->if_capabilities & IFCAP_TOE;
	if (p->if_capenable & IFCAP_VLAN_TOE) {
		TOEDEV(ifp) = TOEDEV(p);
		ifp->if_capenable |= p->if_capenable & IFCAP_TOE;
	}
}

static void
vlan_trunk_capabilities(struct ifnet *ifp)
{
	struct ifvlantrunk *trunk = ifp->if_vlantrunk;
	struct ifvlan *ifv;
	int i;

	TRUNK_LOCK(trunk);
#ifdef VLAN_ARRAY
	for (i = 0; i < VLAN_ARRAY_SIZE; i++)
		if (trunk->vlans[i] != NULL) {
			ifv = trunk->vlans[i];
#else
	for (i = 0; i < (1 << trunk->hwidth); i++) {
		LIST_FOREACH(ifv, &trunk->hash[i], ifv_list)
#endif
			vlan_capabilities(ifv);
	}
	TRUNK_UNLOCK(trunk);
}

static int
vlan_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ifnet *p;
	struct ifreq *ifr;
	struct ifaddr *ifa;
	struct ifvlan *ifv;
	struct ifvlantrunk *trunk;
	struct vlanreq vlr;
	int error = 0;

	ifr = (struct ifreq *)data;
	ifa = (struct ifaddr *) data;
	ifv = ifp->if_softc;

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(ifp, ifa);
#endif
		break;
	case SIOCGIFADDR:
                {
			struct sockaddr *sa;

			sa = (struct sockaddr *)&ifr->ifr_data;
			bcopy(IF_LLADDR(ifp), sa->sa_data, ifp->if_addrlen);
                }
		break;
	case SIOCGIFMEDIA:
		VLAN_LOCK();
		if (TRUNK(ifv) != NULL) {
			p = PARENT(ifv);
			VLAN_UNLOCK();
			error = (*p->if_ioctl)(p, SIOCGIFMEDIA, data);
			/* Limit the result to the parent's current config. */
			if (error == 0) {
				struct ifmediareq *ifmr;

				ifmr = (struct ifmediareq *)data;
				if (ifmr->ifm_count >= 1 && ifmr->ifm_ulist) {
					ifmr->ifm_count = 1;
					error = copyout(&ifmr->ifm_current,
						ifmr->ifm_ulist,
						sizeof(int));
				}
			}
		} else {
			VLAN_UNLOCK();
			error = EINVAL;
		}
		break;

	case SIOCSIFMEDIA:
		error = EINVAL;
		break;

	case SIOCSIFMTU:
		/*
		 * Set the interface MTU.
		 */
		VLAN_LOCK();
		if (TRUNK(ifv) != NULL) {
			if (ifr->ifr_mtu >
			     (PARENT(ifv)->if_mtu - ifv->ifv_mtufudge) ||
			    ifr->ifr_mtu <
			     (ifv->ifv_mintu - ifv->ifv_mtufudge))
				error = EINVAL;
			else
				ifp->if_mtu = ifr->ifr_mtu;
		} else
			error = EINVAL;
		VLAN_UNLOCK();
		break;

	case SIOCSETVLAN:
#ifdef VIMAGE
		/*
		 * XXXRW/XXXBZ: The goal in these checks is to allow a VLAN
		 * interface to be delegated to a jail without allowing the
		 * jail to change what underlying interface/VID it is
		 * associated with.  We are not entirely convinced that this
		 * is the right way to accomplish that policy goal.
		 */
		if (ifp->if_vnet != ifp->if_home_vnet) {
			error = EPERM;
			break;
		}
#endif
		error = copyin(ifr->ifr_data, &vlr, sizeof(vlr));
		if (error)
			break;
		if (vlr.vlr_parent[0] == '\0') {
			vlan_unconfig(ifp);
			break;
		}
		p = ifunit(vlr.vlr_parent);
		if (p == NULL) {
			error = ENOENT;
			break;
		}
		error = vlan_config(ifv, p, vlr.vlr_tag);
		if (error)
			break;

		/* Update flags on the parent, if necessary. */
		vlan_setflags(ifp, 1);
		break;

	case SIOCGETVLAN:
#ifdef VIMAGE
		if (ifp->if_vnet != ifp->if_home_vnet) {
			error = EPERM;
			break;
		}
#endif
		bzero(&vlr, sizeof(vlr));
		VLAN_LOCK();
		if (TRUNK(ifv) != NULL) {
			strlcpy(vlr.vlr_parent, PARENT(ifv)->if_xname,
			    sizeof(vlr.vlr_parent));
			vlr.vlr_tag = ifv->ifv_vid;
		}
		VLAN_UNLOCK();
		error = copyout(&vlr, ifr->ifr_data, sizeof(vlr));
		break;
		
	case SIOCSIFFLAGS:
		/*
		 * We should propagate selected flags to the parent,
		 * e.g., promiscuous mode.
		 */
		if (TRUNK(ifv) != NULL)
			error = vlan_setflags(ifp, 1);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/*
		 * If we don't have a parent, just remember the membership for
		 * when we do.
		 */
		trunk = TRUNK(ifv);
		if (trunk != NULL) {
			TRUNK_LOCK(trunk);
			error = vlan_setmulti(ifp);
			TRUNK_UNLOCK(trunk);
		}
		break;

	case SIOCGVLANPCP:
#ifdef VIMAGE
		if (ifp->if_vnet != ifp->if_home_vnet) {
			error = EPERM;
			break;
		}
#endif
		ifr->ifr_vlan_pcp = ifv->ifv_pcp;
		break;

	case SIOCSVLANPCP:
#ifdef VIMAGE
		if (ifp->if_vnet != ifp->if_home_vnet) {
			error = EPERM;
			break;
		}
#endif
		error = priv_check(curthread, PRIV_NET_SETVLANPCP);
		if (error)
			break;
		if (ifr->ifr_vlan_pcp > 7) {
			error = EINVAL;
			break;
		}
		ifv->ifv_pcp = ifr->ifr_vlan_pcp;
		vlan_tag_recalculate(ifv);
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}
