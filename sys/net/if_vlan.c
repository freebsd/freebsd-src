/*-
 * Copyright 1998 Massachusetts Institute of Technology
 * Copyright 2012 ADARA Networks, Inc.
 * Copyright 2017 Dell EMC Isilon
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

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_kern_tls.h"
#include "opt_vlan.h"
#include "opt_ratelimit.h"

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
#include <sys/taskqueue.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_private.h>
#include <net/if_clone.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>
#include <net/route.h>
#include <net/vnet.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_route.h>
#include <netlink/route/route_var.h>

#define	VLAN_DEF_HWIDTH	4
#define	VLAN_IFFLAGS	(IFF_BROADCAST | IFF_MULTICAST)

#define	UP_AND_RUNNING(ifp) \
    ((ifp)->if_flags & IFF_UP && (ifp)->if_drv_flags & IFF_DRV_RUNNING)

CK_SLIST_HEAD(ifvlanhead, ifvlan);

struct ifvlantrunk {
	struct	ifnet   *parent;	/* parent interface of this trunk */
	struct	mtx	lock;
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

#if defined(KERN_TLS) || defined(RATELIMIT)
struct vlan_snd_tag {
	struct m_snd_tag com;
	struct m_snd_tag *tag;
};

static inline struct vlan_snd_tag *
mst_to_vst(struct m_snd_tag *mst)
{

	return (__containerof(mst, struct vlan_snd_tag, com));
}
#endif

/*
 * This macro provides a facility to iterate over every vlan on a trunk with
 * the assumption that none will be added/removed during iteration.
 */
#ifdef VLAN_ARRAY
#define VLAN_FOREACH(_ifv, _trunk) \
	size_t _i; \
	for (_i = 0; _i < VLAN_ARRAY_SIZE; _i++) \
		if (((_ifv) = (_trunk)->vlans[_i]) != NULL)
#else /* VLAN_ARRAY */
#define VLAN_FOREACH(_ifv, _trunk) \
	struct ifvlan *_next; \
	size_t _i; \
	for (_i = 0; _i < (1 << (_trunk)->hwidth); _i++) \
		CK_SLIST_FOREACH_SAFE((_ifv), &(_trunk)->hash[_i], ifv_list, _next)
#endif /* VLAN_ARRAY */

/*
 * This macro provides a facility to iterate over every vlan on a trunk while
 * also modifying the number of vlans on the trunk. The iteration continues
 * until some condition is met or there are no more vlans on the trunk.
 */
#ifdef VLAN_ARRAY
/* The VLAN_ARRAY case is simple -- just a for loop using the condition. */
#define VLAN_FOREACH_UNTIL_SAFE(_ifv, _trunk, _cond) \
	size_t _i; \
	for (_i = 0; !(_cond) && _i < VLAN_ARRAY_SIZE; _i++) \
		if (((_ifv) = (_trunk)->vlans[_i]))
#else /* VLAN_ARRAY */
/*
 * The hash table case is more complicated. We allow for the hash table to be
 * modified (i.e. vlans removed) while we are iterating over it. To allow for
 * this we must restart the iteration every time we "touch" something during
 * the iteration, since removal will resize the hash table and invalidate our
 * current position. If acting on the touched element causes the trunk to be
 * emptied, then iteration also stops.
 */
#define VLAN_FOREACH_UNTIL_SAFE(_ifv, _trunk, _cond) \
	size_t _i; \
	bool _touch = false; \
	for (_i = 0; \
	    !(_cond) && _i < (1 << (_trunk)->hwidth); \
	    _i = (_touch && ((_trunk) != NULL) ? 0 : _i + 1), _touch = false) \
		if (((_ifv) = CK_SLIST_FIRST(&(_trunk)->hash[_i])) != NULL && \
		    (_touch = true))
#endif /* VLAN_ARRAY */

struct vlan_mc_entry {
	struct sockaddr_dl		mc_addr;
	CK_SLIST_ENTRY(vlan_mc_entry)	mc_entries;
	struct epoch_context		mc_epoch_ctx;
};

struct ifvlan {
	struct	ifvlantrunk *ifv_trunk;
	struct	ifnet *ifv_ifp;
#define	TRUNK(ifv)	((ifv)->ifv_trunk)
#define	PARENT(ifv)	(TRUNK(ifv)->parent)
	void	*ifv_cookie;
	int	ifv_pflags;	/* special flags we have set on parent */
	int	ifv_capenable;
  	int	ifv_capenable2;
	int	ifv_encaplen;	/* encapsulation length */
	int	ifv_mtufudge;	/* MTU fudged by this much */
	int	ifv_mintu;	/* min transmission unit */
	struct  ether_8021q_tag ifv_qtag;
#define ifv_proto	ifv_qtag.proto
#define ifv_vid		ifv_qtag.vid
#define ifv_pcp		ifv_qtag.pcp
	struct task lladdr_task;
	CK_SLIST_HEAD(, vlan_mc_entry) vlan_mc_listhead;
#ifndef VLAN_ARRAY
	CK_SLIST_ENTRY(ifvlan) ifv_list;
#endif
};

/* Special flags we should propagate to parent. */
static struct {
	int flag;
	int (*func)(struct ifnet *, int);
} vlan_pflags[] = {
	{IFF_PROMISC, ifpromisc},
	{IFF_ALLMULTI, if_allmulti},
	{0, NULL}
};

VNET_DECLARE(int, vlan_mtag_pcp);
#define	V_vlan_mtag_pcp	VNET(vlan_mtag_pcp)

static const char vlanname[] = "vlan";
static MALLOC_DEFINE(M_VLAN, vlanname, "802.1Q Virtual LAN Interface");

static eventhandler_tag ifdetach_tag;
static eventhandler_tag iflladdr_tag;
static eventhandler_tag ifevent_tag;

/*
 * if_vlan uses two module-level synchronizations primitives to allow concurrent
 * modification of vlan interfaces and (mostly) allow for vlans to be destroyed
 * while they are being used for tx/rx. To accomplish this in a way that has
 * acceptable performance and cooperation with other parts of the network stack
 * there is a non-sleepable epoch(9) and an sx(9).
 *
 * The performance-sensitive paths that warrant using the epoch(9) are
 * vlan_transmit and vlan_input. Both have to check for the vlan interface's
 * existence using if_vlantrunk, and being in the network tx/rx paths the use
 * of an epoch(9) gives a measureable improvement in performance.
 *
 * The reason for having an sx(9) is mostly because there are still areas that
 * must be sleepable and also have safe concurrent access to a vlan interface.
 * Since the sx(9) exists, it is used by default in most paths unless sleeping
 * is not permitted, or if it is not clear whether sleeping is permitted.
 *
 */
#define _VLAN_SX_ID ifv_sx

static struct sx _VLAN_SX_ID;

#define VLAN_LOCKING_INIT() \
	sx_init_flags(&_VLAN_SX_ID, "vlan_sx", SX_RECURSE)

#define VLAN_LOCKING_DESTROY() \
	sx_destroy(&_VLAN_SX_ID)

#define	VLAN_SLOCK()			sx_slock(&_VLAN_SX_ID)
#define	VLAN_SUNLOCK()			sx_sunlock(&_VLAN_SX_ID)
#define	VLAN_XLOCK()			sx_xlock(&_VLAN_SX_ID)
#define	VLAN_XUNLOCK()			sx_xunlock(&_VLAN_SX_ID)
#define	VLAN_SLOCK_ASSERT()		sx_assert(&_VLAN_SX_ID, SA_SLOCKED)
#define	VLAN_XLOCK_ASSERT()		sx_assert(&_VLAN_SX_ID, SA_XLOCKED)
#define	VLAN_SXLOCK_ASSERT()		sx_assert(&_VLAN_SX_ID, SA_LOCKED)

/*
 * We also have a per-trunk mutex that should be acquired when changing
 * its state.
 */
#define	TRUNK_LOCK_INIT(trunk)		mtx_init(&(trunk)->lock, vlanname, NULL, MTX_DEF)
#define	TRUNK_LOCK_DESTROY(trunk)	mtx_destroy(&(trunk)->lock)
#define	TRUNK_WLOCK(trunk)		mtx_lock(&(trunk)->lock)
#define	TRUNK_WUNLOCK(trunk)		mtx_unlock(&(trunk)->lock)
#define	TRUNK_WLOCK_ASSERT(trunk)	mtx_assert(&(trunk)->lock, MA_OWNED);

/*
 * The VLAN_ARRAY substitutes the dynamic hash with a static array
 * with 4096 entries. In theory this can give a boost in processing,
 * however in practice it does not. Probably this is because the array
 * is too big to fit into CPU cache.
 */
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
#if defined(KERN_TLS) || defined(RATELIMIT)
static	int vlan_snd_tag_alloc(struct ifnet *,
    union if_snd_tag_alloc_params *, struct m_snd_tag **);
static	int vlan_snd_tag_modify(struct m_snd_tag *,
    union if_snd_tag_modify_params *);
static	int vlan_snd_tag_query(struct m_snd_tag *,
    union if_snd_tag_query_params *);
static	void vlan_snd_tag_free(struct m_snd_tag *);
static struct m_snd_tag *vlan_next_snd_tag(struct m_snd_tag *);
static void vlan_ratelimit_query(struct ifnet *,
    struct if_ratelimit_query_results *);
#endif
static	void vlan_qflush(struct ifnet *ifp);
static	int vlan_setflag(struct ifnet *ifp, int flag, int status,
    int (*func)(struct ifnet *, int));
static	int vlan_setflags(struct ifnet *ifp, int status);
static	int vlan_setmulti(struct ifnet *ifp);
static	int vlan_transmit(struct ifnet *ifp, struct mbuf *m);
#ifdef ALTQ
static void vlan_altq_start(struct ifnet *ifp);
static	int vlan_altq_transmit(struct ifnet *ifp, struct mbuf *m);
#endif
static	int vlan_output(struct ifnet *ifp, struct mbuf *m,
    const struct sockaddr *dst, struct route *ro);
static	void vlan_unconfig(struct ifnet *ifp);
static	void vlan_unconfig_locked(struct ifnet *ifp, int departing);
static	int vlan_config(struct ifvlan *ifv, struct ifnet *p, uint16_t tag,
	uint16_t proto);
static	void vlan_link_state(struct ifnet *ifp);
static	void vlan_capabilities(struct ifvlan *ifv);
static	void vlan_trunk_capabilities(struct ifnet *ifp);

static	struct ifnet *vlan_clone_match_ethervid(const char *, int *);
static	int vlan_clone_match(struct if_clone *, const char *);
static	int vlan_clone_create(struct if_clone *, char *, size_t,
    struct ifc_data *, struct ifnet **);
static	int vlan_clone_destroy(struct if_clone *, struct ifnet *, uint32_t);

static int vlan_clone_create_nl(struct if_clone *ifc, char *name, size_t len,
    struct ifc_data_nl *ifd);
static int vlan_clone_modify_nl(struct ifnet *ifp, struct ifc_data_nl *ifd);
static void vlan_clone_dump_nl(struct ifnet *ifp, struct nl_writer *nw);

static	void vlan_ifdetach(void *arg, struct ifnet *ifp);
static  void vlan_iflladdr(void *arg, struct ifnet *ifp);
static  void vlan_ifevent(void *arg, struct ifnet *ifp, int event);

static  void vlan_lladdr_fn(void *arg, int pending);

static struct if_clone *vlan_cloner;

#ifdef VIMAGE
VNET_DEFINE_STATIC(struct if_clone *, vlan_cloner);
#define	V_vlan_cloner	VNET(vlan_cloner)
#endif

#ifdef RATELIMIT
static const struct if_snd_tag_sw vlan_snd_tag_ul_sw = {
	.snd_tag_modify = vlan_snd_tag_modify,
	.snd_tag_query = vlan_snd_tag_query,
	.snd_tag_free = vlan_snd_tag_free,
	.next_snd_tag = vlan_next_snd_tag,
	.type = IF_SND_TAG_TYPE_UNLIMITED
};

static const struct if_snd_tag_sw vlan_snd_tag_rl_sw = {
	.snd_tag_modify = vlan_snd_tag_modify,
	.snd_tag_query = vlan_snd_tag_query,
	.snd_tag_free = vlan_snd_tag_free,
	.next_snd_tag = vlan_next_snd_tag,
	.type = IF_SND_TAG_TYPE_RATE_LIMIT
};
#endif

#ifdef KERN_TLS
static const struct if_snd_tag_sw vlan_snd_tag_tls_sw = {
	.snd_tag_modify = vlan_snd_tag_modify,
	.snd_tag_query = vlan_snd_tag_query,
	.snd_tag_free = vlan_snd_tag_free,
	.next_snd_tag = vlan_next_snd_tag,
	.type = IF_SND_TAG_TYPE_TLS
};

#ifdef RATELIMIT
static const struct if_snd_tag_sw vlan_snd_tag_tls_rl_sw = {
	.snd_tag_modify = vlan_snd_tag_modify,
	.snd_tag_query = vlan_snd_tag_query,
	.snd_tag_free = vlan_snd_tag_free,
	.next_snd_tag = vlan_next_snd_tag,
	.type = IF_SND_TAG_TYPE_TLS_RATE_LIMIT
};
#endif
#endif

static void
vlan_mc_free(struct epoch_context *ctx)
{
	struct vlan_mc_entry *mc = __containerof(ctx, struct vlan_mc_entry, mc_epoch_ctx);
	free(mc, M_VLAN);
}

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
		CK_SLIST_INIT(&trunk->hash[i]);
}

static void
vlan_freehash(struct ifvlantrunk *trunk)
{
#ifdef INVARIANTS
	int i;

	KASSERT(trunk->hwidth > 0, ("%s: hwidth not positive", __func__));
	for (i = 0; i < (1 << trunk->hwidth); i++)
		KASSERT(CK_SLIST_EMPTY(&trunk->hash[i]),
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

	VLAN_XLOCK_ASSERT();
	KASSERT(trunk->hwidth > 0, ("%s: hwidth not positive", __func__));

	b = 1 << trunk->hwidth;
	i = HASH(ifv->ifv_vid, trunk->hmask);
	CK_SLIST_FOREACH(ifv2, &trunk->hash[i], ifv_list)
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
	CK_SLIST_INSERT_HEAD(&trunk->hash[i], ifv, ifv_list);
	trunk->refcnt++;

	return (0);
}

static int
vlan_remhash(struct ifvlantrunk *trunk, struct ifvlan *ifv)
{
	int i, b;
	struct ifvlan *ifv2;

	VLAN_XLOCK_ASSERT();
	KASSERT(trunk->hwidth > 0, ("%s: hwidth not positive", __func__));

	b = 1 << (trunk->hwidth - 1);
	i = HASH(ifv->ifv_vid, trunk->hmask);
	CK_SLIST_FOREACH(ifv2, &trunk->hash[i], ifv_list)
		if (ifv2 == ifv) {
			trunk->refcnt--;
			CK_SLIST_REMOVE(&trunk->hash[i], ifv2, ifvlan, ifv_list);
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

	VLAN_XLOCK_ASSERT();
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

	hash2 = malloc(sizeof(struct ifvlanhead) * n2, M_VLAN, M_WAITOK);
	for (j = 0; j < n2; j++)
		CK_SLIST_INIT(&hash2[j]);
	for (i = 0; i < n; i++)
		while ((ifv = CK_SLIST_FIRST(&trunk->hash[i])) != NULL) {
			CK_SLIST_REMOVE(&trunk->hash[i], ifv, ifvlan, ifv_list);
			j = HASH(ifv->ifv_vid, n2 - 1);
			CK_SLIST_INSERT_HEAD(&hash2[j], ifv, ifv_list);
		}
	NET_EPOCH_WAIT();
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

	NET_EPOCH_ASSERT();

	CK_SLIST_FOREACH(ifv, &trunk->hash[HASH(vid, trunk->hmask)], ifv_list)
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
		CK_SLIST_FOREACH(ifv, &trunk->hash[i], ifv_list)
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
	VLAN_XLOCK_ASSERT();

	vlan_freehash(trunk);
	trunk->parent->if_vlantrunk = NULL;
	TRUNK_LOCK_DESTROY(trunk);
	if_rele(trunk->parent);
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

	VLAN_XLOCK_ASSERT();

	/* Find the parent. */
	sc = ifp->if_softc;
	ifp_p = PARENT(sc);

	CURVNET_SET_QUIET(ifp_p->if_vnet);

	/* First, remove any existing filter entries. */
	while ((mc = CK_SLIST_FIRST(&sc->vlan_mc_listhead)) != NULL) {
		CK_SLIST_REMOVE_HEAD(&sc->vlan_mc_listhead, mc_entries);
		(void)if_delmulti(ifp_p, (struct sockaddr *)&mc->mc_addr);
		NET_EPOCH_CALL(vlan_mc_free, &mc->mc_epoch_ctx);
	}

	/* Now program new ones. */
	IF_ADDR_WLOCK(ifp);
	CK_STAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		mc = malloc(sizeof(struct vlan_mc_entry), M_VLAN, M_NOWAIT);
		if (mc == NULL) {
			IF_ADDR_WUNLOCK(ifp);
			CURVNET_RESTORE();
			return (ENOMEM);
		}
		bcopy(ifma->ifma_addr, &mc->mc_addr, ifma->ifma_addr->sa_len);
		mc->mc_addr.sdl_index = ifp_p->if_index;
		CK_SLIST_INSERT_HEAD(&sc->vlan_mc_listhead, mc, mc_entries);
	}
	IF_ADDR_WUNLOCK(ifp);
	CK_SLIST_FOREACH (mc, &sc->vlan_mc_listhead, mc_entries) {
		error = if_addmulti(ifp_p, (struct sockaddr *)&mc->mc_addr,
		    NULL);
		if (error) {
			CURVNET_RESTORE();
			return (error);
		}
	}

	CURVNET_RESTORE();
	return (0);
}

/*
 * A handler for interface ifnet events.
 */
static void
vlan_ifevent(void *arg __unused, struct ifnet *ifp, int event)
{
	struct epoch_tracker et;
	struct ifvlan *ifv;
	struct ifvlantrunk *trunk;

	if (event != IFNET_EVENT_UPDATE_BAUDRATE)
		return;

	NET_EPOCH_ENTER(et);
	trunk = ifp->if_vlantrunk;
	if (trunk == NULL) {
		NET_EPOCH_EXIT(et);
		return;
	}

	TRUNK_WLOCK(trunk);
	VLAN_FOREACH(ifv, trunk) {
		ifv->ifv_ifp->if_baudrate = ifp->if_baudrate;
	}
	TRUNK_WUNLOCK(trunk);
	NET_EPOCH_EXIT(et);
}

/*
 * A handler for parent interface link layer address changes.
 * If the parent interface link layer address is changed we
 * should also change it on all children vlans.
 */
static void
vlan_iflladdr(void *arg __unused, struct ifnet *ifp)
{
	struct epoch_tracker et;
	struct ifvlan *ifv;
	struct ifnet *ifv_ifp;
	struct ifvlantrunk *trunk;
	struct sockaddr_dl *sdl;

	/* Need the epoch since this is run on taskqueue_swi. */
	NET_EPOCH_ENTER(et);
	trunk = ifp->if_vlantrunk;
	if (trunk == NULL) {
		NET_EPOCH_EXIT(et);
		return;
	}

	/*
	 * OK, it's a trunk.  Loop over and change all vlan's lladdrs on it.
	 * We need an exclusive lock here to prevent concurrent SIOCSIFLLADDR
	 * ioctl calls on the parent garbling the lladdr of the child vlan.
	 */
	TRUNK_WLOCK(trunk);
	VLAN_FOREACH(ifv, trunk) {
		/*
		 * Copy new new lladdr into the ifv_ifp, enqueue a task
		 * to actually call if_setlladdr. if_setlladdr needs to
		 * be deferred to a taskqueue because it will call into
		 * the if_vlan ioctl path and try to acquire the global
		 * lock.
		 */
		ifv_ifp = ifv->ifv_ifp;
		bcopy(IF_LLADDR(ifp), IF_LLADDR(ifv_ifp),
		    ifp->if_addrlen);
		sdl = (struct sockaddr_dl *)ifv_ifp->if_addr->ifa_addr;
		sdl->sdl_alen = ifp->if_addrlen;
		taskqueue_enqueue(taskqueue_thread, &ifv->lladdr_task);
	}
	TRUNK_WUNLOCK(trunk);
	NET_EPOCH_EXIT(et);
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
	struct ifvlantrunk *trunk;

	/* If the ifnet is just being renamed, don't do anything. */
	if (ifp->if_flags & IFF_RENAMING)
		return;
	VLAN_XLOCK();
	trunk = ifp->if_vlantrunk;
	if (trunk == NULL) {
		VLAN_XUNLOCK();
		return;
	}

	/*
	 * OK, it's a trunk.  Loop over and detach all vlan's on it.
	 * Check trunk pointer after each vlan_unconfig() as it will
	 * free it and set to NULL after the last vlan was detached.
	 */
	VLAN_FOREACH_UNTIL_SAFE(ifv, ifp->if_vlantrunk,
	    ifp->if_vlantrunk == NULL)
		vlan_unconfig_locked(ifv->ifv_ifp, 1);

	/* Trunk should have been destroyed in vlan_unconfig(). */
	KASSERT(ifp->if_vlantrunk == NULL, ("%s: purge failed", __func__));
	VLAN_XUNLOCK();
}

/*
 * Return the trunk device for a virtual interface.
 */
static struct ifnet  *
vlan_trunkdev(struct ifnet *ifp)
{
	struct ifvlan *ifv;

	NET_EPOCH_ASSERT();

	if (ifp->if_type != IFT_L2VLAN)
		return (NULL);

	ifv = ifp->if_softc;
	ifp = NULL;
	if (ifv->ifv_trunk)
		ifp = PARENT(ifv);
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

static int
vlan_pcp(struct ifnet *ifp, uint16_t *pcpp)
{
	struct ifvlan *ifv;

	if (ifp->if_type != IFT_L2VLAN)
		return (EINVAL);
	ifv = ifp->if_softc;
	*pcpp = ifv->ifv_pcp;
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

	NET_EPOCH_ASSERT();

	trunk = ifp->if_vlantrunk;
	if (trunk == NULL)
		return (NULL);
	ifp = NULL;
	ifv = vlan_gethash(trunk, vid);
	if (ifv)
		ifp = ifv->ifv_ifp;
	return (ifp);
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

static struct if_clone_addreq_v2 vlan_addreq = {
	.version = 2,
	.match_f = vlan_clone_match,
	.create_f = vlan_clone_create,
	.destroy_f = vlan_clone_destroy,
	.create_nl_f = vlan_clone_create_nl,
	.modify_nl_f = vlan_clone_modify_nl,
	.dump_nl_f = vlan_clone_dump_nl,
};

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
		ifevent_tag = EVENTHANDLER_REGISTER(ifnet_event,
		    vlan_ifevent, NULL, EVENTHANDLER_PRI_ANY);
		if (ifevent_tag == NULL)
			return (ENOMEM);
		VLAN_LOCKING_INIT();
		vlan_input_p = vlan_input;
		vlan_link_state_p = vlan_link_state;
		vlan_trunk_cap_p = vlan_trunk_capabilities;
		vlan_trunkdev_p = vlan_trunkdev;
		vlan_cookie_p = vlan_cookie;
		vlan_setcookie_p = vlan_setcookie;
		vlan_tag_p = vlan_tag;
		vlan_pcp_p = vlan_pcp;
		vlan_devat_p = vlan_devat;
#ifndef VIMAGE
		vlan_cloner = ifc_attach_cloner(vlanname, (struct if_clone_addreq *)&vlan_addreq);
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
		ifc_detach_cloner(vlan_cloner);
#endif
		EVENTHANDLER_DEREGISTER(ifnet_departure_event, ifdetach_tag);
		EVENTHANDLER_DEREGISTER(iflladdr_event, iflladdr_tag);
		EVENTHANDLER_DEREGISTER(ifnet_event, ifevent_tag);
		vlan_input_p = NULL;
		vlan_link_state_p = NULL;
		vlan_trunk_cap_p = NULL;
		vlan_trunkdev_p = NULL;
		vlan_tag_p = NULL;
		vlan_cookie_p = NULL;
		vlan_setcookie_p = NULL;
		vlan_devat_p = NULL;
		VLAN_LOCKING_DESTROY();
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
	vlan_cloner = ifc_attach_cloner(vlanname, (struct if_clone_addreq *)&vlan_addreq);
	V_vlan_cloner = vlan_cloner;
}
VNET_SYSINIT(vnet_vlan_init, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY,
    vnet_vlan_init, NULL);

static void
vnet_vlan_uninit(const void *unused __unused)
{

	ifc_detach_cloner(V_vlan_cloner);
}
VNET_SYSUNINIT(vnet_vlan_uninit, SI_SUB_INIT_IF, SI_ORDER_ANY,
    vnet_vlan_uninit, NULL);
#endif

/*
 * Check for <etherif>.<vlan>[.<vlan> ...] style interface names.
 */
static struct ifnet *
vlan_clone_match_ethervid(const char *name, int *vidp)
{
	char ifname[IFNAMSIZ];
	char *cp;
	struct ifnet *ifp;
	int vid;

	strlcpy(ifname, name, IFNAMSIZ);
	if ((cp = strrchr(ifname, '.')) == NULL)
		return (NULL);
	*cp = '\0';
	if ((ifp = ifunit_ref(ifname)) == NULL)
		return (NULL);
	/* Parse VID. */
	if (*++cp == '\0') {
		if_rele(ifp);
		return (NULL);
	}
	vid = 0;
	for(; *cp >= '0' && *cp <= '9'; cp++)
		vid = (vid * 10) + (*cp - '0');
	if (*cp != '\0') {
		if_rele(ifp);
		return (NULL);
	}
	if (vidp != NULL)
		*vidp = vid;

	return (ifp);
}

static int
vlan_clone_match(struct if_clone *ifc, const char *name)
{
	struct ifnet *ifp;
	const char *cp;

	ifp = vlan_clone_match_ethervid(name, NULL);
	if (ifp != NULL) {
		if_rele(ifp);
		return (1);
	}

	if (strncmp(vlanname, name, strlen(vlanname)) != 0)
		return (0);
	for (cp = name + 4; *cp != '\0'; cp++) {
		if (*cp < '0' || *cp > '9')
			return (0);
	}

	return (1);
}

static int
vlan_clone_create(struct if_clone *ifc, char *name, size_t len,
    struct ifc_data *ifd, struct ifnet **ifpp)
{
	char *dp;
	bool wildcard = false;
	bool subinterface = false;
	int unit;
	int error;
	int vid = 0;
	uint16_t proto = ETHERTYPE_VLAN;
	struct ifvlan *ifv;
	struct ifnet *ifp;
	struct ifnet *p = NULL;
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;
	struct vlanreq vlr;
	static const u_char eaddr[ETHER_ADDR_LEN];	/* 00:00:00:00:00:00 */


	/*
	 * There are three ways to specify the cloned device:
	 * o pass a parameter block with the clone request.
	 * o specify parameters in the text of the clone device name
	 * o specify no parameters and get an unattached device that
	 *   must be configured separately.
	 * The first technique is preferred; the latter two are supported
	 * for backwards compatibility.
	 *
	 * XXXRW: Note historic use of the word "tag" here.  New ioctls may be
	 * called for.
	 */

	if (ifd->params != NULL) {
		error = ifc_copyin(ifd, &vlr, sizeof(vlr));
		if (error)
			return error;
		vid = vlr.vlr_tag;
		proto = vlr.vlr_proto;
		if (proto == 0)
			proto = ETHERTYPE_VLAN;
		p = ifunit_ref(vlr.vlr_parent);
		if (p == NULL)
			return (ENXIO);
	}

	if ((error = ifc_name2unit(name, &unit)) == 0) {

		/*
		 * vlanX interface. Set wildcard to true if the unit number
		 * is not fixed (-1)
		 */
		wildcard = (unit < 0);
	} else {
		struct ifnet *p_tmp = vlan_clone_match_ethervid(name, &vid);
		if (p_tmp != NULL) {
			error = 0;
			subinterface = true;
			unit = IF_DUNIT_NONE;
			wildcard = false;
			if (p != NULL) {
				if_rele(p_tmp);
				if (p != p_tmp)
					error = EINVAL;
			} else
				p = p_tmp;
		} else
			error = ENXIO;
	}

	if (error != 0) {
		if (p != NULL)
			if_rele(p);
		return (error);
	}

	if (!subinterface) {
		/* vlanX interface, mark X as busy or allocate new unit # */
		error = ifc_alloc_unit(ifc, &unit);
		if (error != 0) {
			if (p != NULL)
				if_rele(p);
			return (error);
		}
	}

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
	CK_SLIST_INIT(&ifv->vlan_mc_listhead);
	ifp->if_softc = ifv;
	/*
	 * Set the name manually rather than using if_initname because
	 * we don't conform to the default naming convention for interfaces.
	 */
	strlcpy(ifp->if_xname, name, IFNAMSIZ);
	ifp->if_dname = vlanname;
	ifp->if_dunit = unit;

	ifp->if_init = vlan_init;
#ifdef ALTQ
	ifp->if_start = vlan_altq_start;
	ifp->if_transmit = vlan_altq_transmit;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
	ifp->if_snd.ifq_drv_maxlen = 0;
	IFQ_SET_READY(&ifp->if_snd);
#else
	ifp->if_transmit = vlan_transmit;
#endif
	ifp->if_qflush = vlan_qflush;
	ifp->if_ioctl = vlan_ioctl;
#if defined(KERN_TLS) || defined(RATELIMIT)
	ifp->if_snd_tag_alloc = vlan_snd_tag_alloc;
	ifp->if_ratelimit_query = vlan_ratelimit_query;
#endif
	ifp->if_flags = VLAN_IFFLAGS;
	ether_ifattach(ifp, eaddr);
	/* Now undo some of the damage... */
	ifp->if_baudrate = 0;
	ifp->if_type = IFT_L2VLAN;
	ifp->if_hdrlen = ETHER_VLAN_ENCAP_LEN;
	ifa = ifp->if_addr;
	sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	sdl->sdl_type = IFT_L2VLAN;

	if (p != NULL) {
		error = vlan_config(ifv, p, vid, proto);
		if_rele(p);
		if (error != 0) {
			/*
			 * Since we've partially failed, we need to back
			 * out all the way, otherwise userland could get
			 * confused.  Thus, we destroy the interface.
			 */
			ether_ifdetach(ifp);
			vlan_unconfig(ifp);
			if_free(ifp);
			if (!subinterface)
				ifc_free_unit(ifc, unit);
			free(ifv, M_VLAN);

			return (error);
		}
	}
	*ifpp = ifp;

	return (0);
}

/*
 *
 * Parsers of IFLA_INFO_DATA inside IFLA_LINKINFO of RTM_NEWLINK
 *    {{nla_len=8, nla_type=IFLA_LINK}, 2},
 *    {{nla_len=12, nla_type=IFLA_IFNAME}, "xvlan22"},
 *    {{nla_len=24, nla_type=IFLA_LINKINFO},
 *     [
 *      {{nla_len=8, nla_type=IFLA_INFO_KIND}, "vlan"...},
 *      {{nla_len=12, nla_type=IFLA_INFO_DATA}, "\x06\x00\x01\x00\x16\x00\x00\x00"}]}
 */

struct nl_parsed_vlan {
	uint16_t vlan_id;
	uint16_t vlan_proto;
	struct ifla_vlan_flags vlan_flags;
};

#define	_OUT(_field)	offsetof(struct nl_parsed_vlan, _field)
static const struct nlattr_parser nla_p_vlan[] = {
	{ .type = IFLA_VLAN_ID, .off = _OUT(vlan_id), .cb = nlattr_get_uint16 },
	{ .type = IFLA_VLAN_FLAGS, .off = _OUT(vlan_flags), .cb = nlattr_get_nla },
	{ .type = IFLA_VLAN_PROTOCOL, .off = _OUT(vlan_proto), .cb = nlattr_get_uint16 },
};
#undef _OUT
NL_DECLARE_ATTR_PARSER(vlan_parser, nla_p_vlan);

static int
vlan_clone_create_nl(struct if_clone *ifc, char *name, size_t len,
    struct ifc_data_nl *ifd)
{
	struct epoch_tracker et;
        struct ifnet *ifp_parent;
	struct nl_pstate *npt = ifd->npt;
	struct nl_parsed_link *lattrs = ifd->lattrs;
	int error;

	/*
	 * lattrs.ifla_ifname is the new interface name
	 * lattrs.ifi_index contains parent interface index
	 * lattrs.ifla_idata contains un-parsed vlan data
	 */
	struct nl_parsed_vlan attrs = {
		.vlan_id = 0xFEFE,
		.vlan_proto = ETHERTYPE_VLAN
	};

	if (lattrs->ifla_idata == NULL) {
		nlmsg_report_err_msg(npt, "vlan id is required, guessing not supported");
		return (ENOTSUP);
	}

	error = nl_parse_nested(lattrs->ifla_idata, &vlan_parser, npt, &attrs);
	if (error != 0)
		return (error);
	if (attrs.vlan_id > DOT1Q_VID_MAX) {
		nlmsg_report_err_msg(npt, "Invalid VID: %d", attrs.vlan_id);
		return (EINVAL);
	}
	if (attrs.vlan_proto != ETHERTYPE_VLAN && attrs.vlan_proto != ETHERTYPE_QINQ) {
		nlmsg_report_err_msg(npt, "Unsupported ethertype: 0x%04X", attrs.vlan_proto);
		return (ENOTSUP);
	}

	struct vlanreq params = {
		.vlr_tag = attrs.vlan_id,
		.vlr_proto = attrs.vlan_proto,
	};
	struct ifc_data ifd_new = { .flags = IFC_F_SYSSPACE, .unit = ifd->unit, .params = &params };

	NET_EPOCH_ENTER(et);
	ifp_parent = ifnet_byindex(lattrs->ifi_index);
	if (ifp_parent != NULL)
		strlcpy(params.vlr_parent, if_name(ifp_parent), sizeof(params.vlr_parent));
	NET_EPOCH_EXIT(et);

	if (ifp_parent == NULL) {
		nlmsg_report_err_msg(npt, "unable to find parent interface %u", lattrs->ifi_index);
		return (ENOENT);
	}

	error = vlan_clone_create(ifc, name, len, &ifd_new, &ifd->ifp);

	return (error);
}

static int
vlan_clone_modify_nl(struct ifnet *ifp, struct ifc_data_nl *ifd)
{
	struct nl_parsed_link *lattrs = ifd->lattrs;

	if ((lattrs->ifla_idata != NULL) && ((ifd->flags & IFC_F_CREATE) == 0)) {
		struct epoch_tracker et;
		struct nl_parsed_vlan attrs = {
			.vlan_proto = ETHERTYPE_VLAN,
		};
		int error;

		error = nl_parse_nested(lattrs->ifla_idata, &vlan_parser, ifd->npt, &attrs);
		if (error != 0)
			return (error);

		NET_EPOCH_ENTER(et);
		struct ifnet *ifp_parent = ifnet_byindex_ref(lattrs->ifla_link);
		NET_EPOCH_EXIT(et);

		if (ifp_parent == NULL) {
			nlmsg_report_err_msg(ifd->npt, "unable to find parent interface %u",
			    lattrs->ifla_link);
			return (ENOENT);
		}

		struct ifvlan *ifv = ifp->if_softc;
		error = vlan_config(ifv, ifp_parent, attrs.vlan_id, attrs.vlan_proto);

		if_rele(ifp_parent);
		if (error != 0)
			return (error);
	}

	return (nl_modify_ifp_generic(ifp, ifd->lattrs, ifd->bm, ifd->npt));
}

/*
 *    {{nla_len=24, nla_type=IFLA_LINKINFO},
 *     [
 *      {{nla_len=8, nla_type=IFLA_INFO_KIND}, "vlan"...},
 *      {{nla_len=12, nla_type=IFLA_INFO_DATA}, "\x06\x00\x01\x00\x16\x00\x00\x00"}]}
 */
static void
vlan_clone_dump_nl(struct ifnet *ifp, struct nl_writer *nw)
{
	uint32_t parent_index = 0;
	uint16_t vlan_id = 0;
	uint16_t vlan_proto = 0;

	VLAN_SLOCK();
	struct ifvlan *ifv = ifp->if_softc;
	if (TRUNK(ifv) != NULL)
		parent_index = PARENT(ifv)->if_index;
	vlan_id = ifv->ifv_vid;
	vlan_proto = ifv->ifv_proto;
	VLAN_SUNLOCK();

	if (parent_index != 0)
		nlattr_add_u32(nw, IFLA_LINK, parent_index);

	int off = nlattr_add_nested(nw, IFLA_LINKINFO);
	if (off != 0) {
		nlattr_add_string(nw, IFLA_INFO_KIND, "vlan");
		int off2 = nlattr_add_nested(nw, IFLA_INFO_DATA);
		if (off2 != 0) {
			nlattr_add_u16(nw, IFLA_VLAN_ID, vlan_id);
			nlattr_add_u16(nw, IFLA_VLAN_PROTOCOL, vlan_proto);
			nlattr_set_len(nw, off2);
		}
		nlattr_set_len(nw, off);
	}
}

static int
vlan_clone_destroy(struct if_clone *ifc, struct ifnet *ifp, uint32_t flags)
{
	struct ifvlan *ifv = ifp->if_softc;
	int unit = ifp->if_dunit;

	if (ifp->if_vlantrunk)
		return (EBUSY);

#ifdef ALTQ
	IFQ_PURGE(&ifp->if_snd);
#endif
	ether_ifdetach(ifp);	/* first, remove it from system-wide lists */
	vlan_unconfig(ifp);	/* now it can be unconfigured and freed */
	/*
	 * We should have the only reference to the ifv now, so we can now
	 * drain any remaining lladdr task before freeing the ifnet and the
	 * ifvlan.
	 */
	taskqueue_drain(taskqueue_thread, &ifv->lladdr_task);
	NET_EPOCH_WAIT();
	if_free(ifp);
	free(ifv, M_VLAN);
	if (unit != IF_DUNIT_NONE)
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
	int error, len, mcast;

	NET_EPOCH_ASSERT();

	ifv = ifp->if_softc;
	if (TRUNK(ifv) == NULL) {
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
		m_freem(m);
		return (ENETDOWN);
	}
	p = PARENT(ifv);
	len = m->m_pkthdr.len;
	mcast = (m->m_flags & (M_MCAST | M_BCAST)) ? 1 : 0;

	BPF_MTAP(ifp, m);

#if defined(KERN_TLS) || defined(RATELIMIT)
	if (m->m_pkthdr.csum_flags & CSUM_SND_TAG) {
		struct vlan_snd_tag *vst;
		struct m_snd_tag *mst;

		MPASS(m->m_pkthdr.snd_tag->ifp == ifp);
		mst = m->m_pkthdr.snd_tag;
		vst = mst_to_vst(mst);
		if (vst->tag->ifp != p) {
			if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
			m_freem(m);
			return (EAGAIN);
		}

		m->m_pkthdr.snd_tag = m_snd_tag_ref(vst->tag);
		m_snd_tag_rele(mst);
	}
#endif

	/*
	 * Do not run parent's if_transmit() if the parent is not up,
	 * or parent's driver will cause a system crash.
	 */
	if (!UP_AND_RUNNING(p)) {
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
		m_freem(m);
		return (ENETDOWN);
	}

	if (!ether_8021q_frame(&m, ifp, p, &ifv->ifv_qtag)) {
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
		return (0);
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

static int
vlan_output(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst,
    struct route *ro)
{
	struct ifvlan *ifv;
	struct ifnet *p;

	NET_EPOCH_ASSERT();

	/*
	 * Find the first non-VLAN parent interface.
	 */
	ifv = ifp->if_softc;
	do {
		if (TRUNK(ifv) == NULL) {
			m_freem(m);
			return (ENETDOWN);
		}
		p = PARENT(ifv);
		ifv = p->if_softc;
	} while (p->if_type == IFT_L2VLAN);

	return p->if_output(ifp, m, dst, ro);
}

#ifdef ALTQ
static void
vlan_altq_start(if_t ifp)
{
	struct ifaltq *ifq = &ifp->if_snd;
	struct mbuf *m;

	IFQ_LOCK(ifq);
	IFQ_DEQUEUE_NOLOCK(ifq, m);
	while (m != NULL) {
		vlan_transmit(ifp, m);
		IFQ_DEQUEUE_NOLOCK(ifq, m);
	}
	IFQ_UNLOCK(ifq);
}

static int
vlan_altq_transmit(if_t ifp, struct mbuf *m)
{
	int err;

	if (ALTQ_IS_ENABLED(&ifp->if_snd)) {
		IFQ_ENQUEUE(&ifp->if_snd, m, err);
		if (err == 0)
			vlan_altq_start(ifp);
	} else
		err = vlan_transmit(ifp, m);

	return (err);
}
#endif	/* ALTQ */

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
	struct ifvlantrunk *trunk;
	struct ifvlan *ifv;
	struct m_tag *mtag;
	uint16_t vid, tag;

	NET_EPOCH_ASSERT();

	trunk = ifp->if_vlantrunk;
	if (trunk == NULL) {
		m_freem(m);
		return;
	}

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
			if_inc_counter(ifp, IFCOUNTER_NOPROTO, 1);
			m_freem(m);
			return;
		}
	}

	vid = EVL_VLANOFTAG(tag);

	ifv = vlan_gethash(trunk, vid);
	if (ifv == NULL || !UP_AND_RUNNING(ifv->ifv_ifp)) {
		if_inc_counter(ifp, IFCOUNTER_NOPROTO, 1);
		m_freem(m);
		return;
	}

	if (V_vlan_mtag_pcp) {
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
				if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
				m_freem(m);
				return;
			}
			m_tag_prepend(m, mtag);
		}
		*(uint8_t *)(mtag + 1) = EVL_PRIOFTAG(tag);
	}

	m->m_pkthdr.rcvif = ifv->ifv_ifp;
	if_inc_counter(ifv->ifv_ifp, IFCOUNTER_IPACKETS, 1);

	/* Pass it back through the parent's input routine. */
	(*ifv->ifv_ifp->if_input)(ifv->ifv_ifp, m);
}

static void
vlan_lladdr_fn(void *arg, int pending __unused)
{
	struct ifvlan *ifv;
	struct ifnet *ifp;

	ifv = (struct ifvlan *)arg;
	ifp = ifv->ifv_ifp;

	CURVNET_SET(ifp->if_vnet);

	/* The ifv_ifp already has the lladdr copied in. */
	if_setlladdr(ifp, IF_LLADDR(ifp), ifp->if_addrlen);

	CURVNET_RESTORE();
}

static int
vlan_config(struct ifvlan *ifv, struct ifnet *p, uint16_t vid,
	uint16_t proto)
{
	struct epoch_tracker et;
	struct ifvlantrunk *trunk;
	struct ifnet *ifp;
	int error = 0;

	/*
	 * We can handle non-ethernet hardware types as long as
	 * they handle the tagging and headers themselves.
	 */
	if (p->if_type != IFT_ETHER &&
	    p->if_type != IFT_L2VLAN &&
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
	if (ifv->ifv_trunk) {
		trunk = ifv->ifv_trunk;
		if (trunk->parent != p)
			return (EBUSY);

		VLAN_XLOCK();

		ifv->ifv_proto = proto;

		if (ifv->ifv_vid != vid) {
			int oldvid = ifv->ifv_vid;

			/* Re-hash */
			vlan_remhash(trunk, ifv);
			ifv->ifv_vid = vid;
			error = vlan_inshash(trunk, ifv);
			if (error) {
				int ret __diagused;

				ifv->ifv_vid = oldvid;
				/* Re-insert back where we found it. */
				ret = vlan_inshash(trunk, ifv);
				MPASS(ret == 0);
			}
		}
		/* Will unlock */
		goto done;
	}

	VLAN_XLOCK();
	if (p->if_vlantrunk == NULL) {
		trunk = malloc(sizeof(struct ifvlantrunk),
		    M_VLAN, M_WAITOK | M_ZERO);
		vlan_inithash(trunk);
		TRUNK_LOCK_INIT(trunk);
		TRUNK_WLOCK(trunk);
		p->if_vlantrunk = trunk;
		trunk->parent = p;
		if_ref(trunk->parent);
		TRUNK_WUNLOCK(trunk);
	} else {
		trunk = p->if_vlantrunk;
	}

	ifv->ifv_vid = vid;	/* must set this before vlan_inshash() */
	ifv->ifv_pcp = 0;       /* Default: best effort delivery. */
	error = vlan_inshash(trunk, ifv);
	if (error)
		goto done;
	ifv->ifv_proto = proto;
	ifv->ifv_encaplen = ETHER_VLAN_ENCAP_LEN;
	ifv->ifv_mintu = ETHERMIN;
	ifv->ifv_pflags = 0;
	ifv->ifv_capenable = -1;
	ifv->ifv_capenable2 = -1;

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
	ifp->if_input = p->if_input;
	ifp->if_resolvemulti = p->if_resolvemulti;
	ifp->if_addrlen = p->if_addrlen;
	ifp->if_broadcastaddr = p->if_broadcastaddr;
	ifp->if_pcp = ifv->ifv_pcp;

	/*
	 * We wrap the parent's if_output using vlan_output to ensure that it
	 * can't become stale.
	 */
	ifp->if_output = vlan_output;

	/*
	 * Copy only a selected subset of flags from the parent.
	 * Other flags are none of our business.
	 */
#define VLAN_COPY_FLAGS (IFF_SIMPLEX)
	ifp->if_flags &= ~VLAN_COPY_FLAGS;
	ifp->if_flags |= p->if_flags & VLAN_COPY_FLAGS;
#undef VLAN_COPY_FLAGS

	ifp->if_link_state = p->if_link_state;

	NET_EPOCH_ENTER(et);
	vlan_capabilities(ifv);
	NET_EPOCH_EXIT(et);

	/*
	 * Set up our interface address to reflect the underlying
	 * physical interface's.
	 */
	TASK_INIT(&ifv->lladdr_task, 0, vlan_lladdr_fn, ifv);
	((struct sockaddr_dl *)ifp->if_addr->ifa_addr)->sdl_alen =
	    p->if_addrlen;

	/*
	 * Do not schedule link address update if it was the same
	 * as previous parent's. This helps avoid updating for each
	 * associated llentry.
	 */
	if (memcmp(IF_LLADDR(p), IF_LLADDR(ifp), p->if_addrlen) != 0) {
		bcopy(IF_LLADDR(p), IF_LLADDR(ifp), p->if_addrlen);
		taskqueue_enqueue(taskqueue_thread, &ifv->lladdr_task);
	}

	/* We are ready for operation now. */
	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	/* Update flags on the parent, if necessary. */
	vlan_setflags(ifp, 1);

	/*
	 * Configure multicast addresses that may already be
	 * joined on the vlan device.
	 */
	(void)vlan_setmulti(ifp);

done:
	if (error == 0)
		EVENTHANDLER_INVOKE(vlan_config, p, ifv->ifv_vid);
	VLAN_XUNLOCK();

	return (error);
}

static void
vlan_unconfig(struct ifnet *ifp)
{

	VLAN_XLOCK();
	vlan_unconfig_locked(ifp, 0);
	VLAN_XUNLOCK();
}

static void
vlan_unconfig_locked(struct ifnet *ifp, int departing)
{
	struct ifvlantrunk *trunk;
	struct vlan_mc_entry *mc;
	struct ifvlan *ifv;
	struct ifnet  *parent;
	int error;

	VLAN_XLOCK_ASSERT();

	ifv = ifp->if_softc;
	trunk = ifv->ifv_trunk;
	parent = NULL;

	if (trunk != NULL) {
		parent = trunk->parent;

		/*
		 * Since the interface is being unconfigured, we need to
		 * empty the list of multicast groups that we may have joined
		 * while we were alive from the parent's list.
		 */
		while ((mc = CK_SLIST_FIRST(&ifv->vlan_mc_listhead)) != NULL) {
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
			CK_SLIST_REMOVE_HEAD(&ifv->vlan_mc_listhead, mc_entries);
			NET_EPOCH_CALL(vlan_mc_free, &mc->mc_epoch_ctx);
		}

		vlan_setflags(ifp, 0); /* clear special flags on parent */

		vlan_remhash(trunk, ifv);
		ifv->ifv_trunk = NULL;

		/*
		 * Check if we were the last.
		 */
		if (trunk->refcnt == 0) {
			parent->if_vlantrunk = NULL;
			NET_EPOCH_WAIT();
			trunk_destroy(trunk);
		}
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

	VLAN_SXLOCK_ASSERT();

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
	struct epoch_tracker et;
	struct ifvlantrunk *trunk;
	struct ifvlan *ifv;

	NET_EPOCH_ENTER(et);
	trunk = ifp->if_vlantrunk;
	if (trunk == NULL) {
		NET_EPOCH_EXIT(et);
		return;
	}

	TRUNK_WLOCK(trunk);
	VLAN_FOREACH(ifv, trunk) {
		ifv->ifv_ifp->if_baudrate = trunk->parent->if_baudrate;
		if_link_state_change(ifv->ifv_ifp,
		    trunk->parent->if_link_state);
	}
	TRUNK_WUNLOCK(trunk);
	NET_EPOCH_EXIT(et);
}

#ifdef IPSEC_OFFLOAD
#define	VLAN_IPSEC_METHOD(exp)				\
	if_t p;						\
	struct ifvlan *ifv;				\
	int error;					\
							\
	ifv = ifp->if_softc;				\
	VLAN_SLOCK();					\
	if (TRUNK(ifv) != NULL) {			\
		p = PARENT(ifv);			\
		if_ref(p);				\
		error = p->if_ipsec_accel_m->exp;	\
		if_rele(p);				\
	} else {					\
		error = ENXIO;				\
	}						\
	VLAN_SUNLOCK();					\
	return (error);


static int
vlan_if_spdadd(if_t ifp, void *sp, void *inp, void **priv)
{
	VLAN_IPSEC_METHOD(if_spdadd(ifp, sp, inp, priv));
}

static int
vlan_if_spddel(if_t ifp, void *sp, void *priv)
{
	VLAN_IPSEC_METHOD(if_spddel(ifp, sp, priv));
}

static int
vlan_if_sa_newkey(if_t ifp, void *sav, u_int drv_spi, void **privp)
{
	VLAN_IPSEC_METHOD(if_sa_newkey(ifp, sav, drv_spi, privp));
}

static int
vlan_if_sa_deinstall(if_t ifp, u_int drv_spi, void *priv)
{
	VLAN_IPSEC_METHOD(if_sa_deinstall(ifp, drv_spi, priv));
}

static int
vlan_if_sa_cnt(if_t ifp, void *sa, uint32_t drv_spi, void *priv,
    struct seclifetime *lt)
{
	VLAN_IPSEC_METHOD(if_sa_cnt(ifp, sa, drv_spi, priv, lt));
}

static int
vlan_if_ipsec_hwassist(if_t ifp, void *sav, u_int drv_spi,void *priv)
{
	if_t trunk;

	NET_EPOCH_ASSERT();
	trunk = vlan_trunkdev(ifp);
	if (trunk == NULL)
		return (0);
	return (trunk->if_ipsec_accel_m->if_hwassist(trunk, sav,
	    drv_spi, priv));
}

static const struct if_ipsec_accel_methods vlan_if_ipsec_accel_methods = {
	.if_spdadd = vlan_if_spdadd,
	.if_spddel = vlan_if_spddel,
	.if_sa_newkey = vlan_if_sa_newkey,
	.if_sa_deinstall = vlan_if_sa_deinstall,
	.if_sa_cnt = vlan_if_sa_cnt,
	.if_hwassist = vlan_if_ipsec_hwassist,
};

#undef VLAN_IPSEC_METHOD
#endif	/* IPSEC_OFFLOAD */

static void
vlan_capabilities(struct ifvlan *ifv)
{
	struct ifnet *p;
	struct ifnet *ifp;
	struct ifnet_hw_tsomax hw_tsomax;
	int cap = 0, ena = 0, mena, cap2 = 0, ena2 = 0;
	int mena2 __unused;
	u_long hwa = 0;

	NET_EPOCH_ASSERT();
	VLAN_SXLOCK_ASSERT();

	p = PARENT(ifv);
	ifp = ifv->ifv_ifp;

	/* Mask parent interface enabled capabilities disabled by user. */
	mena = p->if_capenable & ifv->ifv_capenable;
	mena2 = p->if_capenable2 & ifv->ifv_capenable2;

	/*
	 * If the parent interface can do checksum offloading
	 * on VLANs, then propagate its hardware-assisted
	 * checksumming flags. Also assert that checksum
	 * offloading requires hardware VLAN tagging.
	 */
	if (p->if_capabilities & IFCAP_VLAN_HWCSUM)
		cap |= p->if_capabilities & (IFCAP_HWCSUM | IFCAP_HWCSUM_IPV6);
	if (p->if_capenable & IFCAP_VLAN_HWCSUM &&
	    p->if_capenable & IFCAP_VLAN_HWTAGGING) {
		ena |= mena & (IFCAP_HWCSUM | IFCAP_HWCSUM_IPV6);
		if (ena & IFCAP_TXCSUM)
			hwa |= p->if_hwassist & (CSUM_IP | CSUM_TCP |
			    CSUM_UDP | CSUM_SCTP);
		if (ena & IFCAP_TXCSUM_IPV6)
			hwa |= p->if_hwassist & (CSUM_TCP_IPV6 |
			    CSUM_UDP_IPV6 | CSUM_SCTP_IPV6);
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
		cap |= p->if_capabilities & IFCAP_TSO;
	if (p->if_capenable & IFCAP_VLAN_HWTSO) {
		ena |= mena & IFCAP_TSO;
		if (ena & IFCAP_TSO)
			hwa |= p->if_hwassist & CSUM_TSO;
	}

	/*
	 * If the parent interface can do LRO and checksum offloading on
	 * VLANs, then guess it may do LRO on VLANs.  False positive here
	 * cost nothing, while false negative may lead to some confusions.
	 */
	if (p->if_capabilities & IFCAP_VLAN_HWCSUM)
		cap |= p->if_capabilities & IFCAP_LRO;
	if (p->if_capenable & IFCAP_VLAN_HWCSUM)
		ena |= mena & IFCAP_LRO;

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
		cap |= p->if_capabilities & IFCAP_TOE;
	if (p->if_capenable & IFCAP_VLAN_TOE) {
		SETTOEDEV(ifp, TOEDEV(p));
		ena |= mena & IFCAP_TOE;
	}

	/*
	 * If the parent interface supports dynamic link state, so does the
	 * VLAN interface.
	 */
	cap |= (p->if_capabilities & IFCAP_LINKSTATE);
	ena |= (mena & IFCAP_LINKSTATE);

#ifdef RATELIMIT
	/*
	 * If the parent interface supports ratelimiting, so does the
	 * VLAN interface.
	 */
	cap |= (p->if_capabilities & IFCAP_TXRTLMT);
	ena |= (mena & IFCAP_TXRTLMT);
#endif

	/*
	 * If the parent interface supports unmapped mbufs, so does
	 * the VLAN interface.  Note that this should be fine even for
	 * interfaces that don't support hardware tagging as headers
	 * are prepended in normal mbufs to unmapped mbufs holding
	 * payload data.
	 */
	cap |= (p->if_capabilities & IFCAP_MEXTPG);
	ena |= (mena & IFCAP_MEXTPG);

	/*
	 * If the parent interface can offload encryption and segmentation
	 * of TLS records over TCP, propagate it's capability to the VLAN
	 * interface.
	 *
	 * All TLS drivers in the tree today can deal with VLANs.  If
	 * this ever changes, then a new IFCAP_VLAN_TXTLS can be
	 * defined.
	 */
	if (p->if_capabilities & (IFCAP_TXTLS | IFCAP_TXTLS_RTLMT))
		cap |= p->if_capabilities & (IFCAP_TXTLS | IFCAP_TXTLS_RTLMT);
	if (p->if_capenable & (IFCAP_TXTLS | IFCAP_TXTLS_RTLMT))
		ena |= mena & (IFCAP_TXTLS | IFCAP_TXTLS_RTLMT);

	ifp->if_capabilities = cap;
	ifp->if_capenable = ena;
	ifp->if_hwassist = hwa;

#ifdef IPSEC_OFFLOAD
	cap2 |= p->if_capabilities2 & IFCAP2_BIT(IFCAP2_IPSEC_OFFLOAD);
	ena2 |= mena2 & IFCAP2_BIT(IFCAP2_IPSEC_OFFLOAD);
	ifp->if_ipsec_accel_m = &vlan_if_ipsec_accel_methods;
#endif

	ifp->if_capabilities2 = cap2;
	ifp->if_capenable2 = ena2;
}

static void
vlan_trunk_capabilities(struct ifnet *ifp)
{
	struct epoch_tracker et;
	struct ifvlantrunk *trunk;
	struct ifvlan *ifv;

	VLAN_SLOCK();
	trunk = ifp->if_vlantrunk;
	if (trunk == NULL) {
		VLAN_SUNLOCK();
		return;
	}
	NET_EPOCH_ENTER(et);
	VLAN_FOREACH(ifv, trunk)
		vlan_capabilities(ifv);
	NET_EPOCH_EXIT(et);
	VLAN_SUNLOCK();
}

static int
vlan_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ifnet *p;
	struct ifreq *ifr;
#ifdef INET
	struct ifaddr *ifa;
#endif
	struct ifvlan *ifv;
	struct ifvlantrunk *trunk;
	struct vlanreq vlr;
	int error = 0, oldmtu;

	ifr = (struct ifreq *)data;
#ifdef INET
	ifa = (struct ifaddr *) data;
#endif
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
		bcopy(IF_LLADDR(ifp), &ifr->ifr_addr.sa_data[0],
		    ifp->if_addrlen);
		break;
	case SIOCGIFMEDIA:
		VLAN_SLOCK();
		if (TRUNK(ifv) != NULL) {
			p = PARENT(ifv);
			if_ref(p);
			error = (*p->if_ioctl)(p, SIOCGIFMEDIA, data);
			if_rele(p);
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
			error = EINVAL;
		}
		VLAN_SUNLOCK();
		break;

	case SIOCSIFMEDIA:
		error = EINVAL;
		break;

	case SIOCSIFMTU:
		/*
		 * Set the interface MTU.
		 */
		VLAN_SLOCK();
		trunk = TRUNK(ifv);
		if (trunk != NULL) {
			TRUNK_WLOCK(trunk);
			if (ifr->ifr_mtu >
			     (PARENT(ifv)->if_mtu - ifv->ifv_mtufudge) ||
			    ifr->ifr_mtu <
			     (ifv->ifv_mintu - ifv->ifv_mtufudge))
				error = EINVAL;
			else
				ifp->if_mtu = ifr->ifr_mtu;
			TRUNK_WUNLOCK(trunk);
		} else
			error = EINVAL;
		VLAN_SUNLOCK();
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
		error = copyin(ifr_data_get_ptr(ifr), &vlr, sizeof(vlr));
		if (error)
			break;
		if (vlr.vlr_parent[0] == '\0') {
			vlan_unconfig(ifp);
			break;
		}
		p = ifunit_ref(vlr.vlr_parent);
		if (p == NULL) {
			error = ENOENT;
			break;
		}
		if (vlr.vlr_proto == 0)
			vlr.vlr_proto = ETHERTYPE_VLAN;
		oldmtu = ifp->if_mtu;
		error = vlan_config(ifv, p, vlr.vlr_tag, vlr.vlr_proto);
		if_rele(p);

		/*
		 * VLAN MTU may change during addition of the vlandev.
		 * If it did, do network layer specific procedure.
		 */
		if (ifp->if_mtu != oldmtu)
			if_notifymtu(ifp);
		break;

	case SIOCGETVLAN:
#ifdef VIMAGE
		if (ifp->if_vnet != ifp->if_home_vnet) {
			error = EPERM;
			break;
		}
#endif
		bzero(&vlr, sizeof(vlr));
		VLAN_SLOCK();
		if (TRUNK(ifv) != NULL) {
			strlcpy(vlr.vlr_parent, PARENT(ifv)->if_xname,
			    sizeof(vlr.vlr_parent));
			vlr.vlr_tag = ifv->ifv_vid;
			vlr.vlr_proto = ifv->ifv_proto;
		}
		VLAN_SUNLOCK();
		error = copyout(&vlr, ifr_data_get_ptr(ifr), sizeof(vlr));
		break;

	case SIOCSIFFLAGS:
		/*
		 * We should propagate selected flags to the parent,
		 * e.g., promiscuous mode.
		 */
		VLAN_SLOCK();
		if (TRUNK(ifv) != NULL)
			error = vlan_setflags(ifp, 1);
		VLAN_SUNLOCK();
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/*
		 * If we don't have a parent, just remember the membership for
		 * when we do.
		 *
		 * XXX We need the rmlock here to avoid sleeping while
		 * holding in6_multi_mtx.
		 */
		VLAN_XLOCK();
		trunk = TRUNK(ifv);
		if (trunk != NULL)
			error = vlan_setmulti(ifp);
		VLAN_XUNLOCK();

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
		if (ifr->ifr_vlan_pcp > VLAN_PCP_MAX) {
			error = EINVAL;
			break;
		}
		ifv->ifv_pcp = ifr->ifr_vlan_pcp;
		ifp->if_pcp = ifv->ifv_pcp;
		/* broadcast event about PCP change */
		EVENTHANDLER_INVOKE(ifnet_event, ifp, IFNET_EVENT_PCP);
		break;

	case SIOCSIFCAP:
		VLAN_SLOCK();
		ifv->ifv_capenable = ifr->ifr_reqcap;
		trunk = TRUNK(ifv);
		if (trunk != NULL) {
			struct epoch_tracker et;

			NET_EPOCH_ENTER(et);
			vlan_capabilities(ifv);
			NET_EPOCH_EXIT(et);
		}
		VLAN_SUNLOCK();
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}

#if defined(KERN_TLS) || defined(RATELIMIT)
static int
vlan_snd_tag_alloc(struct ifnet *ifp,
    union if_snd_tag_alloc_params *params,
    struct m_snd_tag **ppmt)
{
	struct epoch_tracker et;
	const struct if_snd_tag_sw *sw;
	struct vlan_snd_tag *vst;
	struct ifvlan *ifv;
	struct ifnet *parent;
	struct m_snd_tag *mst;
	int error;

	NET_EPOCH_ENTER(et);
	ifv = ifp->if_softc;

	switch (params->hdr.type) {
#ifdef RATELIMIT
	case IF_SND_TAG_TYPE_UNLIMITED:
		sw = &vlan_snd_tag_ul_sw;
		break;
	case IF_SND_TAG_TYPE_RATE_LIMIT:
		sw = &vlan_snd_tag_rl_sw;
		break;
#endif
#ifdef KERN_TLS
	case IF_SND_TAG_TYPE_TLS:
		sw = &vlan_snd_tag_tls_sw;
		break;
	case IF_SND_TAG_TYPE_TLS_RX:
		sw = NULL;
		if (params->tls_rx.vlan_id != 0)
			goto failure;
		params->tls_rx.vlan_id = ifv->ifv_vid;
		break;
#ifdef RATELIMIT
	case IF_SND_TAG_TYPE_TLS_RATE_LIMIT:
		sw = &vlan_snd_tag_tls_rl_sw;
		break;
#endif
#endif
	default:
		goto failure;
	}

	if (ifv->ifv_trunk != NULL)
		parent = PARENT(ifv);
	else
		parent = NULL;
	if (parent == NULL)
		goto failure;
	if_ref(parent);
	NET_EPOCH_EXIT(et);

	if (sw != NULL) {
		vst = malloc(sizeof(*vst), M_VLAN, M_NOWAIT);
		if (vst == NULL) {
			if_rele(parent);
			return (ENOMEM);
		}
	} else
		vst = NULL;

	error = m_snd_tag_alloc(parent, params, &mst);
	if_rele(parent);
	if (error) {
		free(vst, M_VLAN);
		return (error);
	}

	if (sw != NULL) {
		m_snd_tag_init(&vst->com, ifp, sw);
		vst->tag = mst;

		*ppmt = &vst->com;
	} else
		*ppmt = mst;

	return (0);
failure:
	NET_EPOCH_EXIT(et);
	return (EOPNOTSUPP);
}

static struct m_snd_tag *
vlan_next_snd_tag(struct m_snd_tag *mst)
{
	struct vlan_snd_tag *vst;

	vst = mst_to_vst(mst);
	return (vst->tag);
}

static int
vlan_snd_tag_modify(struct m_snd_tag *mst,
    union if_snd_tag_modify_params *params)
{
	struct vlan_snd_tag *vst;

	vst = mst_to_vst(mst);
	return (vst->tag->sw->snd_tag_modify(vst->tag, params));
}

static int
vlan_snd_tag_query(struct m_snd_tag *mst,
    union if_snd_tag_query_params *params)
{
	struct vlan_snd_tag *vst;

	vst = mst_to_vst(mst);
	return (vst->tag->sw->snd_tag_query(vst->tag, params));
}

static void
vlan_snd_tag_free(struct m_snd_tag *mst)
{
	struct vlan_snd_tag *vst;

	vst = mst_to_vst(mst);
	m_snd_tag_rele(vst->tag);
	free(vst, M_VLAN);
}

static void
vlan_ratelimit_query(struct ifnet *ifp __unused, struct if_ratelimit_query_results *q)
{
	/*
	 * For vlan, we have an indirect
	 * interface. The caller needs to
	 * get a ratelimit tag on the actual
	 * interface the flow will go on.
	 */
	q->rate_table = NULL;
	q->flags = RT_IS_INDIRECT;
	q->max_flows = 0;
	q->number_of_rates = 0;
}

#endif
