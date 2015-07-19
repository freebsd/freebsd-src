/*
 * Copyright (C) 2013-2014 Universita` di Pisa. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


/*
 * This module implements the VALE switch for netmap

--- VALE SWITCH ---

NMG_LOCK() serializes all modifications to switches and ports.
A switch cannot be deleted until all ports are gone.

For each switch, an SX lock (RWlock on linux) protects
deletion of ports. When configuring or deleting a new port, the
lock is acquired in exclusive mode (after holding NMG_LOCK).
When forwarding, the lock is acquired in shared mode (without NMG_LOCK).
The lock is held throughout the entire forwarding cycle,
during which the thread may incur in a page fault.
Hence it is important that sleepable shared locks are used.

On the rx ring, the per-port lock is grabbed initially to reserve
a number of slot in the ring, then the lock is released,
packets are copied from source to destination, and then
the lock is acquired again and the receive ring is updated.
(A similar thing is done on the tx ring for NIC and host stack
ports attached to the switch)

 */

/*
 * OS-specific code that is used only within this file.
 * Other OS-specific code that must be accessed by drivers
 * is present in netmap_kern.h
 */

#if defined(__FreeBSD__)
#include <sys/cdefs.h> /* prerequisite */
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>	/* defines used in kernel.h */
#include <sys/kernel.h>	/* types used in module initialization */
#include <sys/conf.h>	/* cdevsw struct, UID, GID */
#include <sys/sockio.h>
#include <sys/socketvar.h>	/* struct socket */
#include <sys/malloc.h>
#include <sys/poll.h>
#include <sys/rwlock.h>
#include <sys/socket.h> /* sockaddrs */
#include <sys/selinfo.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/bpf.h>		/* BIOCIMMEDIATE */
#include <machine/bus.h>	/* bus_dmamap_* */
#include <sys/endian.h>
#include <sys/refcount.h>


#define BDG_RWLOCK_T		struct rwlock // struct rwlock

#define	BDG_RWINIT(b)		\
	rw_init_flags(&(b)->bdg_lock, "bdg lock", RW_NOWITNESS)
#define BDG_WLOCK(b)		rw_wlock(&(b)->bdg_lock)
#define BDG_WUNLOCK(b)		rw_wunlock(&(b)->bdg_lock)
#define BDG_RLOCK(b)		rw_rlock(&(b)->bdg_lock)
#define BDG_RTRYLOCK(b)		rw_try_rlock(&(b)->bdg_lock)
#define BDG_RUNLOCK(b)		rw_runlock(&(b)->bdg_lock)
#define BDG_RWDESTROY(b)	rw_destroy(&(b)->bdg_lock)


#elif defined(linux)

#include "bsd_glue.h"

#elif defined(__APPLE__)

#warning OSX support is only partial
#include "osx_glue.h"

#else

#error	Unsupported platform

#endif /* unsupported */

/*
 * common headers
 */

#include <net/netmap.h>
#include <dev/netmap/netmap_kern.h>
#include <dev/netmap/netmap_mem2.h>

#ifdef WITH_VALE

/*
 * system parameters (most of them in netmap_kern.h)
 * NM_NAME	prefix for switch port names, default "vale"
 * NM_BDG_MAXPORTS	number of ports
 * NM_BRIDGES	max number of switches in the system.
 *	XXX should become a sysctl or tunable
 *
 * Switch ports are named valeX:Y where X is the switch name and Y
 * is the port. If Y matches a physical interface name, the port is
 * connected to a physical device.
 *
 * Unlike physical interfaces, switch ports use their own memory region
 * for rings and buffers.
 * The virtual interfaces use per-queue lock instead of core lock.
 * In the tx loop, we aggregate traffic in batches to make all operations
 * faster. The batch size is bridge_batch.
 */
#define NM_BDG_MAXRINGS		16	/* XXX unclear how many. */
#define NM_BDG_MAXSLOTS		4096	/* XXX same as above */
#define NM_BRIDGE_RINGSIZE	1024	/* in the device */
#define NM_BDG_HASH		1024	/* forwarding table entries */
#define NM_BDG_BATCH		1024	/* entries in the forwarding buffer */
#define NM_MULTISEG		64	/* max size of a chain of bufs */
/* actual size of the tables */
#define NM_BDG_BATCH_MAX	(NM_BDG_BATCH + NM_MULTISEG)
/* NM_FT_NULL terminates a list of slots in the ft */
#define NM_FT_NULL		NM_BDG_BATCH_MAX
#define	NM_BRIDGES		8	/* number of bridges */


/*
 * bridge_batch is set via sysctl to the max batch size to be
 * used in the bridge. The actual value may be larger as the
 * last packet in the block may overflow the size.
 */
int bridge_batch = NM_BDG_BATCH; /* bridge batch size */
SYSCTL_DECL(_dev_netmap);
SYSCTL_INT(_dev_netmap, OID_AUTO, bridge_batch, CTLFLAG_RW, &bridge_batch, 0 , "");


static int netmap_vp_create(struct nmreq *, struct ifnet *, struct netmap_vp_adapter **);
static int netmap_vp_reg(struct netmap_adapter *na, int onoff);
static int netmap_bwrap_register(struct netmap_adapter *, int onoff);

/*
 * For each output interface, nm_bdg_q is used to construct a list.
 * bq_len is the number of output buffers (we can have coalescing
 * during the copy).
 */
struct nm_bdg_q {
	uint16_t bq_head;
	uint16_t bq_tail;
	uint32_t bq_len;	/* number of buffers */
};

/* XXX revise this */
struct nm_hash_ent {
	uint64_t	mac;	/* the top 2 bytes are the epoch */
	uint64_t	ports;
};

/*
 * nm_bridge is a descriptor for a VALE switch.
 * Interfaces for a bridge are all in bdg_ports[].
 * The array has fixed size, an empty entry does not terminate
 * the search, but lookups only occur on attach/detach so we
 * don't mind if they are slow.
 *
 * The bridge is non blocking on the transmit ports: excess
 * packets are dropped if there is no room on the output port.
 *
 * bdg_lock protects accesses to the bdg_ports array.
 * This is a rw lock (or equivalent).
 */
struct nm_bridge {
	/* XXX what is the proper alignment/layout ? */
	BDG_RWLOCK_T	bdg_lock;	/* protects bdg_ports */
	int		bdg_namelen;
	uint32_t	bdg_active_ports; /* 0 means free */
	char		bdg_basename[IFNAMSIZ];

	/* Indexes of active ports (up to active_ports)
	 * and all other remaining ports.
	 */
	uint8_t		bdg_port_index[NM_BDG_MAXPORTS];

	struct netmap_vp_adapter *bdg_ports[NM_BDG_MAXPORTS];


	/*
	 * The function to decide the destination port.
	 * It returns either of an index of the destination port,
	 * NM_BDG_BROADCAST to broadcast this packet, or NM_BDG_NOPORT not to
	 * forward this packet.  ring_nr is the source ring index, and the
	 * function may overwrite this value to forward this packet to a
	 * different ring index.
	 * This function must be set by netmap_bdgctl().
	 */
	struct netmap_bdg_ops bdg_ops;

	/* the forwarding table, MAC+ports.
	 * XXX should be changed to an argument to be passed to
	 * the lookup function, and allocated on attach
	 */
	struct nm_hash_ent ht[NM_BDG_HASH];

#ifdef CONFIG_NET_NS
	struct net *ns;
#endif /* CONFIG_NET_NS */
};

const char*
netmap_bdg_name(struct netmap_vp_adapter *vp)
{
	struct nm_bridge *b = vp->na_bdg;
	if (b == NULL)
		return NULL;
	return b->bdg_basename;
}


#ifndef CONFIG_NET_NS
/*
 * XXX in principle nm_bridges could be created dynamically
 * Right now we have a static array and deletions are protected
 * by an exclusive lock.
 */
struct nm_bridge *nm_bridges;
#endif /* !CONFIG_NET_NS */


/*
 * this is a slightly optimized copy routine which rounds
 * to multiple of 64 bytes and is often faster than dealing
 * with other odd sizes. We assume there is enough room
 * in the source and destination buffers.
 *
 * XXX only for multiples of 64 bytes, non overlapped.
 */
static inline void
pkt_copy(void *_src, void *_dst, int l)
{
        uint64_t *src = _src;
        uint64_t *dst = _dst;
        if (unlikely(l >= 1024)) {
                memcpy(dst, src, l);
                return;
        }
        for (; likely(l > 0); l-=64) {
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
        }
}


/*
 * locate a bridge among the existing ones.
 * MUST BE CALLED WITH NMG_LOCK()
 *
 * a ':' in the name terminates the bridge name. Otherwise, just NM_NAME.
 * We assume that this is called with a name of at least NM_NAME chars.
 */
static struct nm_bridge *
nm_find_bridge(const char *name, int create)
{
	int i, l, namelen;
	struct nm_bridge *b = NULL, *bridges;
	u_int num_bridges;

	NMG_LOCK_ASSERT();

	netmap_bns_getbridges(&bridges, &num_bridges);

	namelen = strlen(NM_NAME);	/* base length */
	l = name ? strlen(name) : 0;		/* actual length */
	if (l < namelen) {
		D("invalid bridge name %s", name ? name : NULL);
		return NULL;
	}
	for (i = namelen + 1; i < l; i++) {
		if (name[i] == ':') {
			namelen = i;
			break;
		}
	}
	if (namelen >= IFNAMSIZ)
		namelen = IFNAMSIZ;
	ND("--- prefix is '%.*s' ---", namelen, name);

	/* lookup the name, remember empty slot if there is one */
	for (i = 0; i < num_bridges; i++) {
		struct nm_bridge *x = bridges + i;

		if (x->bdg_active_ports == 0) {
			if (create && b == NULL)
				b = x;	/* record empty slot */
		} else if (x->bdg_namelen != namelen) {
			continue;
		} else if (strncmp(name, x->bdg_basename, namelen) == 0) {
			ND("found '%.*s' at %d", namelen, name, i);
			b = x;
			break;
		}
	}
	if (i == num_bridges && b) { /* name not found, can create entry */
		/* initialize the bridge */
		strncpy(b->bdg_basename, name, namelen);
		ND("create new bridge %s with ports %d", b->bdg_basename,
			b->bdg_active_ports);
		b->bdg_namelen = namelen;
		b->bdg_active_ports = 0;
		for (i = 0; i < NM_BDG_MAXPORTS; i++)
			b->bdg_port_index[i] = i;
		/* set the default function */
		b->bdg_ops.lookup = netmap_bdg_learning;
		/* reset the MAC address table */
		bzero(b->ht, sizeof(struct nm_hash_ent) * NM_BDG_HASH);
		NM_BNS_GET(b);
	}
	return b;
}


/*
 * Free the forwarding tables for rings attached to switch ports.
 */
static void
nm_free_bdgfwd(struct netmap_adapter *na)
{
	int nrings, i;
	struct netmap_kring *kring;

	NMG_LOCK_ASSERT();
	nrings = na->num_tx_rings;
	kring = na->tx_rings;
	for (i = 0; i < nrings; i++) {
		if (kring[i].nkr_ft) {
			free(kring[i].nkr_ft, M_DEVBUF);
			kring[i].nkr_ft = NULL; /* protect from freeing twice */
		}
	}
}


/*
 * Allocate the forwarding tables for the rings attached to the bridge ports.
 */
static int
nm_alloc_bdgfwd(struct netmap_adapter *na)
{
	int nrings, l, i, num_dstq;
	struct netmap_kring *kring;

	NMG_LOCK_ASSERT();
	/* all port:rings + broadcast */
	num_dstq = NM_BDG_MAXPORTS * NM_BDG_MAXRINGS + 1;
	l = sizeof(struct nm_bdg_fwd) * NM_BDG_BATCH_MAX;
	l += sizeof(struct nm_bdg_q) * num_dstq;
	l += sizeof(uint16_t) * NM_BDG_BATCH_MAX;

	nrings = netmap_real_rings(na, NR_TX);
	kring = na->tx_rings;
	for (i = 0; i < nrings; i++) {
		struct nm_bdg_fwd *ft;
		struct nm_bdg_q *dstq;
		int j;

		ft = malloc(l, M_DEVBUF, M_NOWAIT | M_ZERO);
		if (!ft) {
			nm_free_bdgfwd(na);
			return ENOMEM;
		}
		dstq = (struct nm_bdg_q *)(ft + NM_BDG_BATCH_MAX);
		for (j = 0; j < num_dstq; j++) {
			dstq[j].bq_head = dstq[j].bq_tail = NM_FT_NULL;
			dstq[j].bq_len = 0;
		}
		kring[i].nkr_ft = ft;
	}
	return 0;
}


/* remove from bridge b the ports in slots hw and sw
 * (sw can be -1 if not needed)
 */
static void
netmap_bdg_detach_common(struct nm_bridge *b, int hw, int sw)
{
	int s_hw = hw, s_sw = sw;
	int i, lim =b->bdg_active_ports;
	uint8_t tmp[NM_BDG_MAXPORTS];

	/*
	New algorithm:
	make a copy of bdg_port_index;
	lookup NA(ifp)->bdg_port and SWNA(ifp)->bdg_port
	in the array of bdg_port_index, replacing them with
	entries from the bottom of the array;
	decrement bdg_active_ports;
	acquire BDG_WLOCK() and copy back the array.
	 */

	if (netmap_verbose)
		D("detach %d and %d (lim %d)", hw, sw, lim);
	/* make a copy of the list of active ports, update it,
	 * and then copy back within BDG_WLOCK().
	 */
	memcpy(tmp, b->bdg_port_index, sizeof(tmp));
	for (i = 0; (hw >= 0 || sw >= 0) && i < lim; ) {
		if (hw >= 0 && tmp[i] == hw) {
			ND("detach hw %d at %d", hw, i);
			lim--; /* point to last active port */
			tmp[i] = tmp[lim]; /* swap with i */
			tmp[lim] = hw;	/* now this is inactive */
			hw = -1;
		} else if (sw >= 0 && tmp[i] == sw) {
			ND("detach sw %d at %d", sw, i);
			lim--;
			tmp[i] = tmp[lim];
			tmp[lim] = sw;
			sw = -1;
		} else {
			i++;
		}
	}
	if (hw >= 0 || sw >= 0) {
		D("XXX delete failed hw %d sw %d, should panic...", hw, sw);
	}

	BDG_WLOCK(b);
	if (b->bdg_ops.dtor)
		b->bdg_ops.dtor(b->bdg_ports[s_hw]);
	b->bdg_ports[s_hw] = NULL;
	if (s_sw >= 0) {
		b->bdg_ports[s_sw] = NULL;
	}
	memcpy(b->bdg_port_index, tmp, sizeof(tmp));
	b->bdg_active_ports = lim;
	BDG_WUNLOCK(b);

	ND("now %d active ports", lim);
	if (lim == 0) {
		ND("marking bridge %s as free", b->bdg_basename);
		bzero(&b->bdg_ops, sizeof(b->bdg_ops));
		NM_BNS_PUT(b);
	}
}

/* nm_bdg_ctl callback for VALE ports */
static int
netmap_vp_bdg_ctl(struct netmap_adapter *na, struct nmreq *nmr, int attach)
{
	struct netmap_vp_adapter *vpna = (struct netmap_vp_adapter *)na;
	struct nm_bridge *b = vpna->na_bdg;

	if (attach)
		return 0; /* nothing to do */
	if (b) {
		netmap_set_all_rings(na, 0 /* disable */);
		netmap_bdg_detach_common(b, vpna->bdg_port, -1);
		vpna->na_bdg = NULL;
		netmap_set_all_rings(na, 1 /* enable */);
	}
	/* I have took reference just for attach */
	netmap_adapter_put(na);
	return 0;
}

/* nm_dtor callback for ephemeral VALE ports */
static void
netmap_vp_dtor(struct netmap_adapter *na)
{
	struct netmap_vp_adapter *vpna = (struct netmap_vp_adapter*)na;
	struct nm_bridge *b = vpna->na_bdg;

	ND("%s has %d references", na->name, na->na_refcount);

	if (b) {
		netmap_bdg_detach_common(b, vpna->bdg_port, -1);
	}
}

/* remove a persistent VALE port from the system */
static int
nm_vi_destroy(const char *name)
{
	struct ifnet *ifp;
	int error;

	ifp = ifunit_ref(name);
	if (!ifp)
		return ENXIO;
	NMG_LOCK();
	/* make sure this is actually a VALE port */
	if (!NETMAP_CAPABLE(ifp) || NA(ifp)->nm_register != netmap_vp_reg) {
		error = EINVAL;
		goto err;
	}

	if (NA(ifp)->na_refcount > 1) {
		error = EBUSY;
		goto err;
	}
	NMG_UNLOCK();

	D("destroying a persistent vale interface %s", ifp->if_xname);
	/* Linux requires all the references are released
	 * before unregister
	 */
	if_rele(ifp);
	netmap_detach(ifp);
	nm_vi_detach(ifp);
	return 0;

err:
	NMG_UNLOCK();
	if_rele(ifp);
	return error;
}

/*
 * Create a virtual interface registered to the system.
 * The interface will be attached to a bridge later.
 */
static int
nm_vi_create(struct nmreq *nmr)
{
	struct ifnet *ifp;
	struct netmap_vp_adapter *vpna;
	int error;

	/* don't include VALE prefix */
	if (!strncmp(nmr->nr_name, NM_NAME, strlen(NM_NAME)))
		return EINVAL;
	ifp = ifunit_ref(nmr->nr_name);
	if (ifp) { /* already exist, cannot create new one */
		if_rele(ifp);
		return EEXIST;
	}
	error = nm_vi_persist(nmr->nr_name, &ifp);
	if (error)
		return error;

	NMG_LOCK();
	/* netmap_vp_create creates a struct netmap_vp_adapter */
	error = netmap_vp_create(nmr, ifp, &vpna);
	if (error) {
		D("error %d", error);
		nm_vi_detach(ifp);
		return error;
	}
	/* persist-specific routines */
	vpna->up.nm_bdg_ctl = netmap_vp_bdg_ctl;
	netmap_adapter_get(&vpna->up);
	NMG_UNLOCK();
	D("created %s", ifp->if_xname);
	return 0;
}

/* Try to get a reference to a netmap adapter attached to a VALE switch.
 * If the adapter is found (or is created), this function returns 0, a
 * non NULL pointer is returned into *na, and the caller holds a
 * reference to the adapter.
 * If an adapter is not found, then no reference is grabbed and the
 * function returns an error code, or 0 if there is just a VALE prefix
 * mismatch. Therefore the caller holds a reference when
 * (*na != NULL && return == 0).
 */
int
netmap_get_bdg_na(struct nmreq *nmr, struct netmap_adapter **na, int create)
{
	char *nr_name = nmr->nr_name;
	const char *ifname;
	struct ifnet *ifp;
	int error = 0;
	struct netmap_vp_adapter *vpna, *hostna = NULL;
	struct nm_bridge *b;
	int i, j, cand = -1, cand2 = -1;
	int needed;

	*na = NULL;     /* default return value */

	/* first try to see if this is a bridge port. */
	NMG_LOCK_ASSERT();
	if (strncmp(nr_name, NM_NAME, sizeof(NM_NAME) - 1)) {
		return 0;  /* no error, but no VALE prefix */
	}

	b = nm_find_bridge(nr_name, create);
	if (b == NULL) {
		D("no bridges available for '%s'", nr_name);
		return (create ? ENOMEM : ENXIO);
	}
	if (strlen(nr_name) < b->bdg_namelen) /* impossible */
		panic("x");

	/* Now we are sure that name starts with the bridge's name,
	 * lookup the port in the bridge. We need to scan the entire
	 * list. It is not important to hold a WLOCK on the bridge
	 * during the search because NMG_LOCK already guarantees
	 * that there are no other possible writers.
	 */

	/* lookup in the local list of ports */
	for (j = 0; j < b->bdg_active_ports; j++) {
		i = b->bdg_port_index[j];
		vpna = b->bdg_ports[i];
		// KASSERT(na != NULL);
		ND("checking %s", vpna->up.name);
		if (!strcmp(vpna->up.name, nr_name)) {
			netmap_adapter_get(&vpna->up);
			ND("found existing if %s refs %d", nr_name)
			*na = &vpna->up;
			return 0;
		}
	}
	/* not found, should we create it? */
	if (!create)
		return ENXIO;
	/* yes we should, see if we have space to attach entries */
	needed = 2; /* in some cases we only need 1 */
	if (b->bdg_active_ports + needed >= NM_BDG_MAXPORTS) {
		D("bridge full %d, cannot create new port", b->bdg_active_ports);
		return ENOMEM;
	}
	/* record the next two ports available, but do not allocate yet */
	cand = b->bdg_port_index[b->bdg_active_ports];
	cand2 = b->bdg_port_index[b->bdg_active_ports + 1];
	ND("+++ bridge %s port %s used %d avail %d %d",
		b->bdg_basename, ifname, b->bdg_active_ports, cand, cand2);

	/*
	 * try see if there is a matching NIC with this name
	 * (after the bridge's name)
	 */
	ifname = nr_name + b->bdg_namelen + 1;
	ifp = ifunit_ref(ifname);
	if (!ifp) {
		/* Create an ephemeral virtual port
		 * This block contains all the ephemeral-specific logics
		 */
		if (nmr->nr_cmd) {
			/* nr_cmd must be 0 for a virtual port */
			return EINVAL;
		}

		/* bdg_netmap_attach creates a struct netmap_adapter */
		error = netmap_vp_create(nmr, NULL, &vpna);
		if (error) {
			D("error %d", error);
			free(ifp, M_DEVBUF);
			return error;
		}
		/* shortcut - we can skip get_hw_na(),
		 * ownership check and nm_bdg_attach()
		 */
	} else {
		struct netmap_adapter *hw;

		error = netmap_get_hw_na(ifp, &hw);
		if (error || hw == NULL)
			goto out;

		/* host adapter might not be created */
		error = hw->nm_bdg_attach(nr_name, hw);
		if (error)
			goto out;
		vpna = hw->na_vp;
		hostna = hw->na_hostvp;
		if_rele(ifp);
		if (nmr->nr_arg1 != NETMAP_BDG_HOST)
			hostna = NULL;
	}

	BDG_WLOCK(b);
	vpna->bdg_port = cand;
	ND("NIC  %p to bridge port %d", vpna, cand);
	/* bind the port to the bridge (virtual ports are not active) */
	b->bdg_ports[cand] = vpna;
	vpna->na_bdg = b;
	b->bdg_active_ports++;
	if (hostna != NULL) {
		/* also bind the host stack to the bridge */
		b->bdg_ports[cand2] = hostna;
		hostna->bdg_port = cand2;
		hostna->na_bdg = b;
		b->bdg_active_ports++;
		ND("host %p to bridge port %d", hostna, cand2);
	}
	ND("if %s refs %d", ifname, vpna->up.na_refcount);
	BDG_WUNLOCK(b);
	*na = &vpna->up;
	netmap_adapter_get(*na);
	return 0;

out:
	if_rele(ifp);

	return error;
}


/* Process NETMAP_BDG_ATTACH */
static int
nm_bdg_ctl_attach(struct nmreq *nmr)
{
	struct netmap_adapter *na;
	int error;

	NMG_LOCK();

	error = netmap_get_bdg_na(nmr, &na, 1 /* create if not exists */);
	if (error) /* no device */
		goto unlock_exit;

	if (na == NULL) { /* VALE prefix missing */
		error = EINVAL;
		goto unlock_exit;
	}

	if (NETMAP_OWNED_BY_ANY(na)) {
		error = EBUSY;
		goto unref_exit;
	}

	if (na->nm_bdg_ctl) {
		/* nop for VALE ports. The bwrap needs to put the hwna
		 * in netmap mode (see netmap_bwrap_bdg_ctl)
		 */
		error = na->nm_bdg_ctl(na, nmr, 1);
		if (error)
			goto unref_exit;
		ND("registered %s to netmap-mode", na->name);
	}
	NMG_UNLOCK();
	return 0;

unref_exit:
	netmap_adapter_put(na);
unlock_exit:
	NMG_UNLOCK();
	return error;
}


/* process NETMAP_BDG_DETACH */
static int
nm_bdg_ctl_detach(struct nmreq *nmr)
{
	struct netmap_adapter *na;
	int error;

	NMG_LOCK();
	error = netmap_get_bdg_na(nmr, &na, 0 /* don't create */);
	if (error) { /* no device, or another bridge or user owns the device */
		goto unlock_exit;
	}

	if (na == NULL) { /* VALE prefix missing */
		error = EINVAL;
		goto unlock_exit;
	}

	if (na->nm_bdg_ctl) {
		/* remove the port from bridge. The bwrap
		 * also needs to put the hwna in normal mode
		 */
		error = na->nm_bdg_ctl(na, nmr, 0);
	}

	netmap_adapter_put(na);
unlock_exit:
	NMG_UNLOCK();
	return error;

}


/* Called by either user's context (netmap_ioctl())
 * or external kernel modules (e.g., Openvswitch).
 * Operation is indicated in nmr->nr_cmd.
 * NETMAP_BDG_OPS that sets configure/lookup/dtor functions to the bridge
 * requires bdg_ops argument; the other commands ignore this argument.
 *
 * Called without NMG_LOCK.
 */
int
netmap_bdg_ctl(struct nmreq *nmr, struct netmap_bdg_ops *bdg_ops)
{
	struct nm_bridge *b, *bridges;
	struct netmap_adapter *na;
	struct netmap_vp_adapter *vpna;
	char *name = nmr->nr_name;
	int cmd = nmr->nr_cmd, namelen = strlen(name);
	int error = 0, i, j;
	u_int num_bridges;

	netmap_bns_getbridges(&bridges, &num_bridges);

	switch (cmd) {
	case NETMAP_BDG_NEWIF:
		error = nm_vi_create(nmr);
		break;

	case NETMAP_BDG_DELIF:
		error = nm_vi_destroy(nmr->nr_name);
		break;

	case NETMAP_BDG_ATTACH:
		error = nm_bdg_ctl_attach(nmr);
		break;

	case NETMAP_BDG_DETACH:
		error = nm_bdg_ctl_detach(nmr);
		break;

	case NETMAP_BDG_LIST:
		/* this is used to enumerate bridges and ports */
		if (namelen) { /* look up indexes of bridge and port */
			if (strncmp(name, NM_NAME, strlen(NM_NAME))) {
				error = EINVAL;
				break;
			}
			NMG_LOCK();
			b = nm_find_bridge(name, 0 /* don't create */);
			if (!b) {
				error = ENOENT;
				NMG_UNLOCK();
				break;
			}

			error = ENOENT;
			for (j = 0; j < b->bdg_active_ports; j++) {
				i = b->bdg_port_index[j];
				vpna = b->bdg_ports[i];
				if (vpna == NULL) {
					D("---AAAAAAAAARGH-------");
					continue;
				}
				/* the former and the latter identify a
				 * virtual port and a NIC, respectively
				 */
				if (!strcmp(vpna->up.name, name)) {
					/* bridge index */
					nmr->nr_arg1 = b - bridges;
					nmr->nr_arg2 = i; /* port index */
					error = 0;
					break;
				}
			}
			NMG_UNLOCK();
		} else {
			/* return the first non-empty entry starting from
			 * bridge nr_arg1 and port nr_arg2.
			 *
			 * Users can detect the end of the same bridge by
			 * seeing the new and old value of nr_arg1, and can
			 * detect the end of all the bridge by error != 0
			 */
			i = nmr->nr_arg1;
			j = nmr->nr_arg2;

			NMG_LOCK();
			for (error = ENOENT; i < NM_BRIDGES; i++) {
				b = bridges + i;
				if (j >= b->bdg_active_ports) {
					j = 0; /* following bridges scan from 0 */
					continue;
				}
				nmr->nr_arg1 = i;
				nmr->nr_arg2 = j;
				j = b->bdg_port_index[j];
				vpna = b->bdg_ports[j];
				strncpy(name, vpna->up.name, (size_t)IFNAMSIZ);
				error = 0;
				break;
			}
			NMG_UNLOCK();
		}
		break;

	case NETMAP_BDG_REGOPS: /* XXX this should not be available from userspace */
		/* register callbacks to the given bridge.
		 * nmr->nr_name may be just bridge's name (including ':'
		 * if it is not just NM_NAME).
		 */
		if (!bdg_ops) {
			error = EINVAL;
			break;
		}
		NMG_LOCK();
		b = nm_find_bridge(name, 0 /* don't create */);
		if (!b) {
			error = EINVAL;
		} else {
			b->bdg_ops = *bdg_ops;
		}
		NMG_UNLOCK();
		break;

	case NETMAP_BDG_VNET_HDR:
		/* Valid lengths for the virtio-net header are 0 (no header),
		   10 and 12. */
		if (nmr->nr_arg1 != 0 &&
			nmr->nr_arg1 != sizeof(struct nm_vnet_hdr) &&
				nmr->nr_arg1 != 12) {
			error = EINVAL;
			break;
		}
		NMG_LOCK();
		error = netmap_get_bdg_na(nmr, &na, 0);
		if (na && !error) {
			vpna = (struct netmap_vp_adapter *)na;
			vpna->virt_hdr_len = nmr->nr_arg1;
			if (vpna->virt_hdr_len)
				vpna->mfs = NETMAP_BUF_SIZE(na);
			D("Using vnet_hdr_len %d for %p", vpna->virt_hdr_len, vpna);
			netmap_adapter_put(na);
		}
		NMG_UNLOCK();
		break;

	default:
		D("invalid cmd (nmr->nr_cmd) (0x%x)", cmd);
		error = EINVAL;
		break;
	}
	return error;
}

int
netmap_bdg_config(struct nmreq *nmr)
{
	struct nm_bridge *b;
	int error = EINVAL;

	NMG_LOCK();
	b = nm_find_bridge(nmr->nr_name, 0);
	if (!b) {
		NMG_UNLOCK();
		return error;
	}
	NMG_UNLOCK();
	/* Don't call config() with NMG_LOCK() held */
	BDG_RLOCK(b);
	if (b->bdg_ops.config != NULL)
		error = b->bdg_ops.config((struct nm_ifreq *)nmr);
	BDG_RUNLOCK(b);
	return error;
}


/* nm_krings_create callback for VALE ports.
 * Calls the standard netmap_krings_create, then adds leases on rx
 * rings and bdgfwd on tx rings.
 */
static int
netmap_vp_krings_create(struct netmap_adapter *na)
{
	u_int tailroom;
	int error, i;
	uint32_t *leases;
	u_int nrx = netmap_real_rings(na, NR_RX);

	/*
	 * Leases are attached to RX rings on vale ports
	 */
	tailroom = sizeof(uint32_t) * na->num_rx_desc * nrx;

	error = netmap_krings_create(na, tailroom);
	if (error)
		return error;

	leases = na->tailroom;

	for (i = 0; i < nrx; i++) { /* Receive rings */
		na->rx_rings[i].nkr_leases = leases;
		leases += na->num_rx_desc;
	}

	error = nm_alloc_bdgfwd(na);
	if (error) {
		netmap_krings_delete(na);
		return error;
	}

	return 0;
}


/* nm_krings_delete callback for VALE ports. */
static void
netmap_vp_krings_delete(struct netmap_adapter *na)
{
	nm_free_bdgfwd(na);
	netmap_krings_delete(na);
}


static int
nm_bdg_flush(struct nm_bdg_fwd *ft, u_int n,
	struct netmap_vp_adapter *na, u_int ring_nr);


/*
 * main dispatch routine for the bridge.
 * Grab packets from a kring, move them into the ft structure
 * associated to the tx (input) port. Max one instance per port,
 * filtered on input (ioctl, poll or XXX).
 * Returns the next position in the ring.
 */
static int
nm_bdg_preflush(struct netmap_kring *kring, u_int end)
{
	struct netmap_vp_adapter *na =
		(struct netmap_vp_adapter*)kring->na;
	struct netmap_ring *ring = kring->ring;
	struct nm_bdg_fwd *ft;
	u_int ring_nr = kring->ring_id;
	u_int j = kring->nr_hwcur, lim = kring->nkr_num_slots - 1;
	u_int ft_i = 0;	/* start from 0 */
	u_int frags = 1; /* how many frags ? */
	struct nm_bridge *b = na->na_bdg;

	/* To protect against modifications to the bridge we acquire a
	 * shared lock, waiting if we can sleep (if the source port is
	 * attached to a user process) or with a trylock otherwise (NICs).
	 */
	ND("wait rlock for %d packets", ((j > end ? lim+1 : 0) + end) - j);
	if (na->up.na_flags & NAF_BDG_MAYSLEEP)
		BDG_RLOCK(b);
	else if (!BDG_RTRYLOCK(b))
		return 0;
	ND(5, "rlock acquired for %d packets", ((j > end ? lim+1 : 0) + end) - j);
	ft = kring->nkr_ft;

	for (; likely(j != end); j = nm_next(j, lim)) {
		struct netmap_slot *slot = &ring->slot[j];
		char *buf;

		ft[ft_i].ft_len = slot->len;
		ft[ft_i].ft_flags = slot->flags;

		ND("flags is 0x%x", slot->flags);
		/* we do not use the buf changed flag, but we still need to reset it */
		slot->flags &= ~NS_BUF_CHANGED;

		/* this slot goes into a list so initialize the link field */
		ft[ft_i].ft_next = NM_FT_NULL;
		buf = ft[ft_i].ft_buf = (slot->flags & NS_INDIRECT) ?
			(void *)(uintptr_t)slot->ptr : NMB(&na->up, slot);
		if (unlikely(buf == NULL)) {
			RD(5, "NULL %s buffer pointer from %s slot %d len %d",
				(slot->flags & NS_INDIRECT) ? "INDIRECT" : "DIRECT",
				kring->name, j, ft[ft_i].ft_len);
			buf = ft[ft_i].ft_buf = NETMAP_BUF_BASE(&na->up);
			ft[ft_i].ft_len = 0;
			ft[ft_i].ft_flags = 0;
		}
		__builtin_prefetch(buf);
		++ft_i;
		if (slot->flags & NS_MOREFRAG) {
			frags++;
			continue;
		}
		if (unlikely(netmap_verbose && frags > 1))
			RD(5, "%d frags at %d", frags, ft_i - frags);
		ft[ft_i - frags].ft_frags = frags;
		frags = 1;
		if (unlikely((int)ft_i >= bridge_batch))
			ft_i = nm_bdg_flush(ft, ft_i, na, ring_nr);
	}
	if (frags > 1) {
		D("truncate incomplete fragment at %d (%d frags)", ft_i, frags);
		// ft_i > 0, ft[ft_i-1].flags has NS_MOREFRAG
		ft[ft_i - 1].ft_frags &= ~NS_MOREFRAG;
		ft[ft_i - frags].ft_frags = frags - 1;
	}
	if (ft_i)
		ft_i = nm_bdg_flush(ft, ft_i, na, ring_nr);
	BDG_RUNLOCK(b);
	return j;
}


/* ----- FreeBSD if_bridge hash function ------- */

/*
 * The following hash function is adapted from "Hash Functions" by Bob Jenkins
 * ("Algorithm Alley", Dr. Dobbs Journal, September 1997).
 *
 * http://www.burtleburtle.net/bob/hash/spooky.html
 */
#define mix(a, b, c)                                                    \
do {                                                                    \
        a -= b; a -= c; a ^= (c >> 13);                                 \
        b -= c; b -= a; b ^= (a << 8);                                  \
        c -= a; c -= b; c ^= (b >> 13);                                 \
        a -= b; a -= c; a ^= (c >> 12);                                 \
        b -= c; b -= a; b ^= (a << 16);                                 \
        c -= a; c -= b; c ^= (b >> 5);                                  \
        a -= b; a -= c; a ^= (c >> 3);                                  \
        b -= c; b -= a; b ^= (a << 10);                                 \
        c -= a; c -= b; c ^= (b >> 15);                                 \
} while (/*CONSTCOND*/0)


static __inline uint32_t
nm_bridge_rthash(const uint8_t *addr)
{
        uint32_t a = 0x9e3779b9, b = 0x9e3779b9, c = 0; // hask key

        b += addr[5] << 8;
        b += addr[4];
        a += addr[3] << 24;
        a += addr[2] << 16;
        a += addr[1] << 8;
        a += addr[0];

        mix(a, b, c);
#define BRIDGE_RTHASH_MASK	(NM_BDG_HASH-1)
        return (c & BRIDGE_RTHASH_MASK);
}

#undef mix


/* nm_register callback for VALE ports */
static int
netmap_vp_reg(struct netmap_adapter *na, int onoff)
{
	struct netmap_vp_adapter *vpna =
		(struct netmap_vp_adapter*)na;

	/* persistent ports may be put in netmap mode
	 * before being attached to a bridge
	 */
	if (vpna->na_bdg)
		BDG_WLOCK(vpna->na_bdg);
	if (onoff) {
		na->na_flags |= NAF_NETMAP_ON;
		 /* XXX on FreeBSD, persistent VALE ports should also
		 * toggle IFCAP_NETMAP in na->ifp (2014-03-16)
		 */
	} else {
		na->na_flags &= ~NAF_NETMAP_ON;
	}
	if (vpna->na_bdg)
		BDG_WUNLOCK(vpna->na_bdg);
	return 0;
}


/*
 * Lookup function for a learning bridge.
 * Update the hash table with the source address,
 * and then returns the destination port index, and the
 * ring in *dst_ring (at the moment, always use ring 0)
 */
u_int
netmap_bdg_learning(struct nm_bdg_fwd *ft, uint8_t *dst_ring,
		struct netmap_vp_adapter *na)
{
	uint8_t *buf = ft->ft_buf;
	u_int buf_len = ft->ft_len;
	struct nm_hash_ent *ht = na->na_bdg->ht;
	uint32_t sh, dh;
	u_int dst, mysrc = na->bdg_port;
	uint64_t smac, dmac;

	/* safety check, unfortunately we have many cases */
	if (buf_len >= 14 + na->virt_hdr_len) {
		/* virthdr + mac_hdr in the same slot */
		buf += na->virt_hdr_len;
		buf_len -= na->virt_hdr_len;
	} else if (buf_len == na->virt_hdr_len && ft->ft_flags & NS_MOREFRAG) {
		/* only header in first fragment */
		ft++;
		buf = ft->ft_buf;
		buf_len = ft->ft_len;
	} else {
		RD(5, "invalid buf format, length %d", buf_len);
		return NM_BDG_NOPORT;
	}
	dmac = le64toh(*(uint64_t *)(buf)) & 0xffffffffffff;
	smac = le64toh(*(uint64_t *)(buf + 4));
	smac >>= 16;

	/*
	 * The hash is somewhat expensive, there might be some
	 * worthwhile optimizations here.
	 */
	if (((buf[6] & 1) == 0) && (na->last_smac != smac)) { /* valid src */
		uint8_t *s = buf+6;
		sh = nm_bridge_rthash(s); // XXX hash of source
		/* update source port forwarding entry */
		na->last_smac = ht[sh].mac = smac;	/* XXX expire ? */
		ht[sh].ports = mysrc;
		if (netmap_verbose)
		    D("src %02x:%02x:%02x:%02x:%02x:%02x on port %d",
			s[0], s[1], s[2], s[3], s[4], s[5], mysrc);
	}
	dst = NM_BDG_BROADCAST;
	if ((buf[0] & 1) == 0) { /* unicast */
		dh = nm_bridge_rthash(buf); // XXX hash of dst
		if (ht[dh].mac == dmac) {	/* found dst */
			dst = ht[dh].ports;
		}
		/* XXX otherwise return NM_BDG_UNKNOWN ? */
	}
	return dst;
}


/*
 * Available space in the ring. Only used in VALE code
 * and only with is_rx = 1
 */
static inline uint32_t
nm_kr_space(struct netmap_kring *k, int is_rx)
{
	int space;

	if (is_rx) {
		int busy = k->nkr_hwlease - k->nr_hwcur;
		if (busy < 0)
			busy += k->nkr_num_slots;
		space = k->nkr_num_slots - 1 - busy;
	} else {
		/* XXX never used in this branch */
		space = k->nr_hwtail - k->nkr_hwlease;
		if (space < 0)
			space += k->nkr_num_slots;
	}
#if 0
	// sanity check
	if (k->nkr_hwlease >= k->nkr_num_slots ||
		k->nr_hwcur >= k->nkr_num_slots ||
		k->nr_tail >= k->nkr_num_slots ||
		busy < 0 ||
		busy >= k->nkr_num_slots) {
		D("invalid kring, cur %d tail %d lease %d lease_idx %d lim %d",			k->nr_hwcur, k->nr_hwtail, k->nkr_hwlease,
			k->nkr_lease_idx, k->nkr_num_slots);
	}
#endif
	return space;
}




/* make a lease on the kring for N positions. return the
 * lease index
 * XXX only used in VALE code and with is_rx = 1
 */
static inline uint32_t
nm_kr_lease(struct netmap_kring *k, u_int n, int is_rx)
{
	uint32_t lim = k->nkr_num_slots - 1;
	uint32_t lease_idx = k->nkr_lease_idx;

	k->nkr_leases[lease_idx] = NR_NOSLOT;
	k->nkr_lease_idx = nm_next(lease_idx, lim);

	if (n > nm_kr_space(k, is_rx)) {
		D("invalid request for %d slots", n);
		panic("x");
	}
	/* XXX verify that there are n slots */
	k->nkr_hwlease += n;
	if (k->nkr_hwlease > lim)
		k->nkr_hwlease -= lim + 1;

	if (k->nkr_hwlease >= k->nkr_num_slots ||
		k->nr_hwcur >= k->nkr_num_slots ||
		k->nr_hwtail >= k->nkr_num_slots ||
		k->nkr_lease_idx >= k->nkr_num_slots) {
		D("invalid kring %s, cur %d tail %d lease %d lease_idx %d lim %d",
			k->na->name,
			k->nr_hwcur, k->nr_hwtail, k->nkr_hwlease,
			k->nkr_lease_idx, k->nkr_num_slots);
	}
	return lease_idx;
}

/*
 *
 * This flush routine supports only unicast and broadcast but a large
 * number of ports, and lets us replace the learn and dispatch functions.
 */
int
nm_bdg_flush(struct nm_bdg_fwd *ft, u_int n, struct netmap_vp_adapter *na,
		u_int ring_nr)
{
	struct nm_bdg_q *dst_ents, *brddst;
	uint16_t num_dsts = 0, *dsts;
	struct nm_bridge *b = na->na_bdg;
	u_int i, j, me = na->bdg_port;

	/*
	 * The work area (pointed by ft) is followed by an array of
	 * pointers to queues , dst_ents; there are NM_BDG_MAXRINGS
	 * queues per port plus one for the broadcast traffic.
	 * Then we have an array of destination indexes.
	 */
	dst_ents = (struct nm_bdg_q *)(ft + NM_BDG_BATCH_MAX);
	dsts = (uint16_t *)(dst_ents + NM_BDG_MAXPORTS * NM_BDG_MAXRINGS + 1);

	/* first pass: find a destination for each packet in the batch */
	for (i = 0; likely(i < n); i += ft[i].ft_frags) {
		uint8_t dst_ring = ring_nr; /* default, same ring as origin */
		uint16_t dst_port, d_i;
		struct nm_bdg_q *d;

		ND("slot %d frags %d", i, ft[i].ft_frags);
		/* Drop the packet if the virtio-net header is not into the first
		   fragment nor at the very beginning of the second. */
		if (unlikely(na->virt_hdr_len > ft[i].ft_len))
			continue;
		dst_port = b->bdg_ops.lookup(&ft[i], &dst_ring, na);
		if (netmap_verbose > 255)
			RD(5, "slot %d port %d -> %d", i, me, dst_port);
		if (dst_port == NM_BDG_NOPORT)
			continue; /* this packet is identified to be dropped */
		else if (unlikely(dst_port > NM_BDG_MAXPORTS))
			continue;
		else if (dst_port == NM_BDG_BROADCAST)
			dst_ring = 0; /* broadcasts always go to ring 0 */
		else if (unlikely(dst_port == me ||
		    !b->bdg_ports[dst_port]))
			continue;

		/* get a position in the scratch pad */
		d_i = dst_port * NM_BDG_MAXRINGS + dst_ring;
		d = dst_ents + d_i;

		/* append the first fragment to the list */
		if (d->bq_head == NM_FT_NULL) { /* new destination */
			d->bq_head = d->bq_tail = i;
			/* remember this position to be scanned later */
			if (dst_port != NM_BDG_BROADCAST)
				dsts[num_dsts++] = d_i;
		} else {
			ft[d->bq_tail].ft_next = i;
			d->bq_tail = i;
		}
		d->bq_len += ft[i].ft_frags;
	}

	/*
	 * Broadcast traffic goes to ring 0 on all destinations.
	 * So we need to add these rings to the list of ports to scan.
	 * XXX at the moment we scan all NM_BDG_MAXPORTS ports, which is
	 * expensive. We should keep a compact list of active destinations
	 * so we could shorten this loop.
	 */
	brddst = dst_ents + NM_BDG_BROADCAST * NM_BDG_MAXRINGS;
	if (brddst->bq_head != NM_FT_NULL) {
		for (j = 0; likely(j < b->bdg_active_ports); j++) {
			uint16_t d_i;
			i = b->bdg_port_index[j];
			if (unlikely(i == me))
				continue;
			d_i = i * NM_BDG_MAXRINGS;
			if (dst_ents[d_i].bq_head == NM_FT_NULL)
				dsts[num_dsts++] = d_i;
		}
	}

	ND(5, "pass 1 done %d pkts %d dsts", n, num_dsts);
	/* second pass: scan destinations */
	for (i = 0; i < num_dsts; i++) {
		struct netmap_vp_adapter *dst_na;
		struct netmap_kring *kring;
		struct netmap_ring *ring;
		u_int dst_nr, lim, j, d_i, next, brd_next;
		u_int needed, howmany;
		int retry = netmap_txsync_retry;
		struct nm_bdg_q *d;
		uint32_t my_start = 0, lease_idx = 0;
		int nrings;
		int virt_hdr_mismatch = 0;

		d_i = dsts[i];
		ND("second pass %d port %d", i, d_i);
		d = dst_ents + d_i;
		// XXX fix the division
		dst_na = b->bdg_ports[d_i/NM_BDG_MAXRINGS];
		/* protect from the lookup function returning an inactive
		 * destination port
		 */
		if (unlikely(dst_na == NULL))
			goto cleanup;
		if (dst_na->up.na_flags & NAF_SW_ONLY)
			goto cleanup;
		/*
		 * The interface may be in !netmap mode in two cases:
		 * - when na is attached but not activated yet;
		 * - when na is being deactivated but is still attached.
		 */
		if (unlikely(!nm_netmap_on(&dst_na->up))) {
			ND("not in netmap mode!");
			goto cleanup;
		}

		/* there is at least one either unicast or broadcast packet */
		brd_next = brddst->bq_head;
		next = d->bq_head;
		/* we need to reserve this many slots. If fewer are
		 * available, some packets will be dropped.
		 * Packets may have multiple fragments, so we may not use
		 * there is a chance that we may not use all of the slots
		 * we have claimed, so we will need to handle the leftover
		 * ones when we regain the lock.
		 */
		needed = d->bq_len + brddst->bq_len;

		if (unlikely(dst_na->virt_hdr_len != na->virt_hdr_len)) {
			RD(3, "virt_hdr_mismatch, src %d dst %d", na->virt_hdr_len, dst_na->virt_hdr_len);
			/* There is a virtio-net header/offloadings mismatch between
			 * source and destination. The slower mismatch datapath will
			 * be used to cope with all the mismatches.
			 */
			virt_hdr_mismatch = 1;
			if (dst_na->mfs < na->mfs) {
				/* We may need to do segmentation offloadings, and so
				 * we may need a number of destination slots greater
				 * than the number of input slots ('needed').
				 * We look for the smallest integer 'x' which satisfies:
				 *	needed * na->mfs + x * H <= x * na->mfs
				 * where 'H' is the length of the longest header that may
				 * be replicated in the segmentation process (e.g. for
				 * TCPv4 we must account for ethernet header, IP header
				 * and TCPv4 header).
				 */
				needed = (needed * na->mfs) /
						(dst_na->mfs - WORST_CASE_GSO_HEADER) + 1;
				ND(3, "srcmtu=%u, dstmtu=%u, x=%u", na->mfs, dst_na->mfs, needed);
			}
		}

		ND(5, "pass 2 dst %d is %x %s",
			i, d_i, is_vp ? "virtual" : "nic/host");
		dst_nr = d_i & (NM_BDG_MAXRINGS-1);
		nrings = dst_na->up.num_rx_rings;
		if (dst_nr >= nrings)
			dst_nr = dst_nr % nrings;
		kring = &dst_na->up.rx_rings[dst_nr];
		ring = kring->ring;
		lim = kring->nkr_num_slots - 1;

retry:

		if (dst_na->retry && retry) {
			/* try to get some free slot from the previous run */
			kring->nm_notify(kring, 0);
			/* actually useful only for bwraps, since there
			 * the notify will trigger a txsync on the hwna. VALE ports
			 * have dst_na->retry == 0
			 */
		}
		/* reserve the buffers in the queue and an entry
		 * to report completion, and drop lock.
		 * XXX this might become a helper function.
		 */
		mtx_lock(&kring->q_lock);
		if (kring->nkr_stopped) {
			mtx_unlock(&kring->q_lock);
			goto cleanup;
		}
		my_start = j = kring->nkr_hwlease;
		howmany = nm_kr_space(kring, 1);
		if (needed < howmany)
			howmany = needed;
		lease_idx = nm_kr_lease(kring, howmany, 1);
		mtx_unlock(&kring->q_lock);

		/* only retry if we need more than available slots */
		if (retry && needed <= howmany)
			retry = 0;

		/* copy to the destination queue */
		while (howmany > 0) {
			struct netmap_slot *slot;
			struct nm_bdg_fwd *ft_p, *ft_end;
			u_int cnt;

			/* find the queue from which we pick next packet.
			 * NM_FT_NULL is always higher than valid indexes
			 * so we never dereference it if the other list
			 * has packets (and if both are empty we never
			 * get here).
			 */
			if (next < brd_next) {
				ft_p = ft + next;
				next = ft_p->ft_next;
			} else { /* insert broadcast */
				ft_p = ft + brd_next;
				brd_next = ft_p->ft_next;
			}
			cnt = ft_p->ft_frags; // cnt > 0
			if (unlikely(cnt > howmany))
			    break; /* no more space */
			if (netmap_verbose && cnt > 1)
				RD(5, "rx %d frags to %d", cnt, j);
			ft_end = ft_p + cnt;
			if (unlikely(virt_hdr_mismatch)) {
				bdg_mismatch_datapath(na, dst_na, ft_p, ring, &j, lim, &howmany);
			} else {
				howmany -= cnt;
				do {
					char *dst, *src = ft_p->ft_buf;
					size_t copy_len = ft_p->ft_len, dst_len = copy_len;

					slot = &ring->slot[j];
					dst = NMB(&dst_na->up, slot);

					ND("send [%d] %d(%d) bytes at %s:%d",
							i, (int)copy_len, (int)dst_len,
							NM_IFPNAME(dst_ifp), j);
					/* round to a multiple of 64 */
					copy_len = (copy_len + 63) & ~63;

					if (unlikely(copy_len > NETMAP_BUF_SIZE(&dst_na->up) ||
						     copy_len > NETMAP_BUF_SIZE(&na->up))) {
						RD(5, "invalid len %d, down to 64", (int)copy_len);
						copy_len = dst_len = 64; // XXX
					}
					if (ft_p->ft_flags & NS_INDIRECT) {
						if (copyin(src, dst, copy_len)) {
							// invalid user pointer, pretend len is 0
							dst_len = 0;
						}
					} else {
						//memcpy(dst, src, copy_len);
						pkt_copy(src, dst, (int)copy_len);
					}
					slot->len = dst_len;
					slot->flags = (cnt << 8)| NS_MOREFRAG;
					j = nm_next(j, lim);
					needed--;
					ft_p++;
				} while (ft_p != ft_end);
				slot->flags = (cnt << 8); /* clear flag on last entry */
			}
			/* are we done ? */
			if (next == NM_FT_NULL && brd_next == NM_FT_NULL)
				break;
		}
		{
		    /* current position */
		    uint32_t *p = kring->nkr_leases; /* shorthand */
		    uint32_t update_pos;
		    int still_locked = 1;

		    mtx_lock(&kring->q_lock);
		    if (unlikely(howmany > 0)) {
			/* not used all bufs. If i am the last one
			 * i can recover the slots, otherwise must
			 * fill them with 0 to mark empty packets.
			 */
			ND("leftover %d bufs", howmany);
			if (nm_next(lease_idx, lim) == kring->nkr_lease_idx) {
			    /* yes i am the last one */
			    ND("roll back nkr_hwlease to %d", j);
			    kring->nkr_hwlease = j;
			} else {
			    while (howmany-- > 0) {
				ring->slot[j].len = 0;
				ring->slot[j].flags = 0;
				j = nm_next(j, lim);
			    }
			}
		    }
		    p[lease_idx] = j; /* report I am done */

		    update_pos = kring->nr_hwtail;

		    if (my_start == update_pos) {
			/* all slots before my_start have been reported,
			 * so scan subsequent leases to see if other ranges
			 * have been completed, and to a selwakeup or txsync.
		         */
			while (lease_idx != kring->nkr_lease_idx &&
				p[lease_idx] != NR_NOSLOT) {
			    j = p[lease_idx];
			    p[lease_idx] = NR_NOSLOT;
			    lease_idx = nm_next(lease_idx, lim);
			}
			/* j is the new 'write' position. j != my_start
			 * means there are new buffers to report
			 */
			if (likely(j != my_start)) {
				kring->nr_hwtail = j;
				still_locked = 0;
				mtx_unlock(&kring->q_lock);
				kring->nm_notify(kring, 0);
				/* this is netmap_notify for VALE ports and
				 * netmap_bwrap_notify for bwrap. The latter will
				 * trigger a txsync on the underlying hwna
				 */
				if (dst_na->retry && retry--) {
					/* XXX this is going to call nm_notify again.
					 * Only useful for bwrap in virtual machines
					 */
					goto retry;
				}
			}
		    }
		    if (still_locked)
			mtx_unlock(&kring->q_lock);
		}
cleanup:
		d->bq_head = d->bq_tail = NM_FT_NULL; /* cleanup */
		d->bq_len = 0;
	}
	brddst->bq_head = brddst->bq_tail = NM_FT_NULL; /* cleanup */
	brddst->bq_len = 0;
	return 0;
}

/* nm_txsync callback for VALE ports */
static int
netmap_vp_txsync(struct netmap_kring *kring, int flags)
{
	struct netmap_vp_adapter *na =
		(struct netmap_vp_adapter *)kring->na;
	u_int done;
	u_int const lim = kring->nkr_num_slots - 1;
	u_int const head = kring->rhead;

	if (bridge_batch <= 0) { /* testing only */
		done = head; // used all
		goto done;
	}
	if (!na->na_bdg) {
		done = head;
		goto done;
	}
	if (bridge_batch > NM_BDG_BATCH)
		bridge_batch = NM_BDG_BATCH;

	done = nm_bdg_preflush(kring, head);
done:
	if (done != head)
		D("early break at %d/ %d, tail %d", done, head, kring->nr_hwtail);
	/*
	 * packets between 'done' and 'cur' are left unsent.
	 */
	kring->nr_hwcur = done;
	kring->nr_hwtail = nm_prev(done, lim);
	if (netmap_verbose)
		D("%s ring %d flags %d", na->up.name, kring->ring_id, flags);
	return 0;
}


/* rxsync code used by VALE ports nm_rxsync callback and also
 * internally by the brwap
 */
static int
netmap_vp_rxsync_locked(struct netmap_kring *kring, int flags)
{
	struct netmap_adapter *na = kring->na;
	struct netmap_ring *ring = kring->ring;
	u_int nm_i, lim = kring->nkr_num_slots - 1;
	u_int head = kring->rhead;
	int n;

	if (head > lim) {
		D("ouch dangerous reset!!!");
		n = netmap_ring_reinit(kring);
		goto done;
	}

	/* First part, import newly received packets. */
	/* actually nothing to do here, they are already in the kring */

	/* Second part, skip past packets that userspace has released. */
	nm_i = kring->nr_hwcur;
	if (nm_i != head) {
		/* consistency check, but nothing really important here */
		for (n = 0; likely(nm_i != head); n++) {
			struct netmap_slot *slot = &ring->slot[nm_i];
			void *addr = NMB(na, slot);

			if (addr == NETMAP_BUF_BASE(kring->na)) { /* bad buf */
				D("bad buffer index %d, ignore ?",
					slot->buf_idx);
			}
			slot->flags &= ~NS_BUF_CHANGED;
			nm_i = nm_next(nm_i, lim);
		}
		kring->nr_hwcur = head;
	}

	n = 0;
done:
	return n;
}

/*
 * nm_rxsync callback for VALE ports
 * user process reading from a VALE switch.
 * Already protected against concurrent calls from userspace,
 * but we must acquire the queue's lock to protect against
 * writers on the same queue.
 */
static int
netmap_vp_rxsync(struct netmap_kring *kring, int flags)
{
	int n;

	mtx_lock(&kring->q_lock);
	n = netmap_vp_rxsync_locked(kring, flags);
	mtx_unlock(&kring->q_lock);
	return n;
}


/* nm_bdg_attach callback for VALE ports
 * The na_vp port is this same netmap_adapter. There is no host port.
 */
static int
netmap_vp_bdg_attach(const char *name, struct netmap_adapter *na)
{
	struct netmap_vp_adapter *vpna = (struct netmap_vp_adapter *)na;

	if (vpna->na_bdg)
		return EBUSY;
	na->na_vp = vpna;
	strncpy(na->name, name, sizeof(na->name));
	na->na_hostvp = NULL;
	return 0;
}

/* create a netmap_vp_adapter that describes a VALE port.
 * Only persistent VALE ports have a non-null ifp.
 */
static int
netmap_vp_create(struct nmreq *nmr, struct ifnet *ifp, struct netmap_vp_adapter **ret)
{
	struct netmap_vp_adapter *vpna;
	struct netmap_adapter *na;
	int error;
	u_int npipes = 0;

	vpna = malloc(sizeof(*vpna), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (vpna == NULL)
		return ENOMEM;

 	na = &vpna->up;

	na->ifp = ifp;
	strncpy(na->name, nmr->nr_name, sizeof(na->name));

	/* bound checking */
	na->num_tx_rings = nmr->nr_tx_rings;
	nm_bound_var(&na->num_tx_rings, 1, 1, NM_BDG_MAXRINGS, NULL);
	nmr->nr_tx_rings = na->num_tx_rings; // write back
	na->num_rx_rings = nmr->nr_rx_rings;
	nm_bound_var(&na->num_rx_rings, 1, 1, NM_BDG_MAXRINGS, NULL);
	nmr->nr_rx_rings = na->num_rx_rings; // write back
	nm_bound_var(&nmr->nr_tx_slots, NM_BRIDGE_RINGSIZE,
			1, NM_BDG_MAXSLOTS, NULL);
	na->num_tx_desc = nmr->nr_tx_slots;
	nm_bound_var(&nmr->nr_rx_slots, NM_BRIDGE_RINGSIZE,
			1, NM_BDG_MAXSLOTS, NULL);
	/* validate number of pipes. We want at least 1,
	 * but probably can do with some more.
	 * So let's use 2 as default (when 0 is supplied)
	 */
	npipes = nmr->nr_arg1;
	nm_bound_var(&npipes, 2, 1, NM_MAXPIPES, NULL);
	nmr->nr_arg1 = npipes;	/* write back */
	/* validate extra bufs */
	nm_bound_var(&nmr->nr_arg3, 0, 0,
			128*NM_BDG_MAXSLOTS, NULL);
	na->num_rx_desc = nmr->nr_rx_slots;
	vpna->virt_hdr_len = 0;
	vpna->mfs = 1514;
	vpna->last_smac = ~0llu;
	/*if (vpna->mfs > netmap_buf_size)  TODO netmap_buf_size is zero??
		vpna->mfs = netmap_buf_size; */
        if (netmap_verbose)
		D("max frame size %u", vpna->mfs);

	na->na_flags |= NAF_BDG_MAYSLEEP;
	/* persistent VALE ports look like hw devices
	 * with a native netmap adapter
	 */
	if (ifp)
		na->na_flags |= NAF_NATIVE;
	na->nm_txsync = netmap_vp_txsync;
	na->nm_rxsync = netmap_vp_rxsync;
	na->nm_register = netmap_vp_reg;
	na->nm_krings_create = netmap_vp_krings_create;
	na->nm_krings_delete = netmap_vp_krings_delete;
	na->nm_dtor = netmap_vp_dtor;
	na->nm_mem = netmap_mem_private_new(na->name,
			na->num_tx_rings, na->num_tx_desc,
			na->num_rx_rings, na->num_rx_desc,
			nmr->nr_arg3, npipes, &error);
	if (na->nm_mem == NULL)
		goto err;
	na->nm_bdg_attach = netmap_vp_bdg_attach;
	/* other nmd fields are set in the common routine */
	error = netmap_attach_common(na);
	if (error)
		goto err;
	*ret = vpna;
	return 0;

err:
	if (na->nm_mem != NULL)
		netmap_mem_delete(na->nm_mem);
	free(vpna, M_DEVBUF);
	return error;
}

/* Bridge wrapper code (bwrap).
 * This is used to connect a non-VALE-port netmap_adapter (hwna) to a
 * VALE switch.
 * The main task is to swap the meaning of tx and rx rings to match the
 * expectations of the VALE switch code (see nm_bdg_flush).
 *
 * The bwrap works by interposing a netmap_bwrap_adapter between the
 * rest of the system and the hwna. The netmap_bwrap_adapter looks like
 * a netmap_vp_adapter to the rest the system, but, internally, it
 * translates all callbacks to what the hwna expects.
 *
 * Note that we have to intercept callbacks coming from two sides:
 *
 *  - callbacks coming from the netmap module are intercepted by
 *    passing around the netmap_bwrap_adapter instead of the hwna
 *
 *  - callbacks coming from outside of the netmap module only know
 *    about the hwna. This, however, only happens in interrupt
 *    handlers, where only the hwna->nm_notify callback is called.
 *    What the bwrap does is to overwrite the hwna->nm_notify callback
 *    with its own netmap_bwrap_intr_notify.
 *    XXX This assumes that the hwna->nm_notify callback was the
 *    standard netmap_notify(), as it is the case for nic adapters.
 *    Any additional action performed by hwna->nm_notify will not be
 *    performed by netmap_bwrap_intr_notify.
 *
 * Additionally, the bwrap can optionally attach the host rings pair
 * of the wrapped adapter to a different port of the switch.
 */


static void
netmap_bwrap_dtor(struct netmap_adapter *na)
{
	struct netmap_bwrap_adapter *bna = (struct netmap_bwrap_adapter*)na;
	struct netmap_adapter *hwna = bna->hwna;

	ND("na %p", na);
	/* drop reference to hwna->ifp.
	 * If we don't do this, netmap_detach_common(na)
	 * will think it has set NA(na->ifp) to NULL
	 */
	na->ifp = NULL;
	/* for safety, also drop the possible reference
	 * in the hostna
	 */
	bna->host.up.ifp = NULL;

	hwna->nm_mem = bna->save_nmd;
	hwna->na_private = NULL;
	hwna->na_vp = hwna->na_hostvp = NULL;
	hwna->na_flags &= ~NAF_BUSY;
	netmap_adapter_put(hwna);

}


/*
 * Intr callback for NICs connected to a bridge.
 * Simply ignore tx interrupts (maybe we could try to recover space ?)
 * and pass received packets from nic to the bridge.
 *
 * XXX TODO check locking: this is called from the interrupt
 * handler so we should make sure that the interface is not
 * disconnected while passing down an interrupt.
 *
 * Note, no user process can access this NIC or the host stack.
 * The only part of the ring that is significant are the slots,
 * and head/cur/tail are set from the kring as needed
 * (part as a receive ring, part as a transmit ring).
 *
 * callback that overwrites the hwna notify callback.
 * Packets come from the outside or from the host stack and are put on an hwna rx ring.
 * The bridge wrapper then sends the packets through the bridge.
 */
static int
netmap_bwrap_intr_notify(struct netmap_kring *kring, int flags)
{
	struct netmap_adapter *na = kring->na;
	struct netmap_bwrap_adapter *bna = na->na_private;
	struct netmap_kring *bkring;
	struct netmap_vp_adapter *vpna = &bna->up;
	u_int ring_nr = kring->ring_id;
	int error = 0;

	if (netmap_verbose)
	    D("%s %s 0x%x", na->name, kring->name, flags);

	if (!nm_netmap_on(na))
		return 0;

	bkring = &vpna->up.tx_rings[ring_nr];

	/* make sure the ring is not disabled */
	if (nm_kr_tryget(kring))
		return 0;

	if (netmap_verbose)
	    D("%s head %d cur %d tail %d",  na->name,
		kring->rhead, kring->rcur, kring->rtail);

	/* simulate a user wakeup on the rx ring
	 * fetch packets that have arrived.
	 */
	error = kring->nm_sync(kring, 0);
	if (error)
		goto put_out;
	if (kring->nr_hwcur == kring->nr_hwtail && netmap_verbose) {
		D("how strange, interrupt with no packets on %s",
			na->name);
		goto put_out;
	}

	/* new packets are kring->rcur to kring->nr_hwtail, and the bkring
	 * had hwcur == bkring->rhead. So advance bkring->rhead to kring->nr_hwtail
	 * to push all packets out.
	 */
	bkring->rhead = bkring->rcur = kring->nr_hwtail;

	netmap_vp_txsync(bkring, flags);

	/* mark all buffers as released on this ring */
	kring->rhead = kring->rcur = kring->rtail = kring->nr_hwtail;
	/* another call to actually release the buffers */
	error = kring->nm_sync(kring, 0);

put_out:
	nm_kr_put(kring);
	return error;
}


/* nm_register callback for bwrap */
static int
netmap_bwrap_register(struct netmap_adapter *na, int onoff)
{
	struct netmap_bwrap_adapter *bna =
		(struct netmap_bwrap_adapter *)na;
	struct netmap_adapter *hwna = bna->hwna;
	struct netmap_vp_adapter *hostna = &bna->host;
	int error;
	enum txrx t;

	ND("%s %s", na->name, onoff ? "on" : "off");

	if (onoff) {
		int i;

		/* netmap_do_regif has been called on the bwrap na.
		 * We need to pass the information about the
		 * memory allocator down to the hwna before
		 * putting it in netmap mode
		 */
		hwna->na_lut = na->na_lut;

		if (hostna->na_bdg) {
			/* if the host rings have been attached to switch,
			 * we need to copy the memory allocator information
			 * in the hostna also
			 */
			hostna->up.na_lut = na->na_lut;
		}

		/* cross-link the netmap rings
		 * The original number of rings comes from hwna,
		 * rx rings on one side equals tx rings on the other.
		 * We need to do this now, after the initialization
		 * of the kring->ring pointers
		 */
		for_rx_tx(t) {
			enum txrx r= nm_txrx_swap(t); /* swap NR_TX <-> NR_RX */
			for (i = 0; i < nma_get_nrings(na, r) + 1; i++) {
				NMR(hwna, t)[i].nkr_num_slots = NMR(na, r)[i].nkr_num_slots;
				NMR(hwna, t)[i].ring = NMR(na, r)[i].ring;
			}
		}
	}

	/* forward the request to the hwna */
	error = hwna->nm_register(hwna, onoff);
	if (error)
		return error;

	/* impersonate a netmap_vp_adapter */
	netmap_vp_reg(na, onoff);
	if (hostna->na_bdg)
		netmap_vp_reg(&hostna->up, onoff);

	if (onoff) {
		u_int i;
		/* intercept the hwna nm_nofify callback on the hw rings */
		for (i = 0; i < hwna->num_rx_rings; i++) {
			hwna->rx_rings[i].save_notify = hwna->rx_rings[i].nm_notify;
			hwna->rx_rings[i].nm_notify = netmap_bwrap_intr_notify;
		}
		i = hwna->num_rx_rings; /* for safety */
		/* save the host ring notify unconditionally */
		hwna->rx_rings[i].save_notify = hwna->rx_rings[i].nm_notify;
		if (hostna->na_bdg) {
			/* also intercept the host ring notify */
			hwna->rx_rings[i].nm_notify = netmap_bwrap_intr_notify;
		}
	} else {
		u_int i;
		/* reset all notify callbacks (including host ring) */
		for (i = 0; i <= hwna->num_rx_rings; i++) {
			hwna->rx_rings[i].nm_notify = hwna->rx_rings[i].save_notify;
			hwna->rx_rings[i].save_notify = NULL;
		}
		hwna->na_lut.lut = NULL;
		hwna->na_lut.objtotal = 0;
		hwna->na_lut.objsize = 0;
	}

	return 0;
}

/* nm_config callback for bwrap */
static int
netmap_bwrap_config(struct netmap_adapter *na, u_int *txr, u_int *txd,
				    u_int *rxr, u_int *rxd)
{
	struct netmap_bwrap_adapter *bna =
		(struct netmap_bwrap_adapter *)na;
	struct netmap_adapter *hwna = bna->hwna;

	/* forward the request */
	netmap_update_config(hwna);
	/* swap the results */
	*txr = hwna->num_rx_rings;
	*txd = hwna->num_rx_desc;
	*rxr = hwna->num_tx_rings;
	*rxd = hwna->num_rx_desc;

	return 0;
}


/* nm_krings_create callback for bwrap */
static int
netmap_bwrap_krings_create(struct netmap_adapter *na)
{
	struct netmap_bwrap_adapter *bna =
		(struct netmap_bwrap_adapter *)na;
	struct netmap_adapter *hwna = bna->hwna;
	struct netmap_adapter *hostna = &bna->host.up;
	int error;

	ND("%s", na->name);

	/* impersonate a netmap_vp_adapter */
	error = netmap_vp_krings_create(na);
	if (error)
		return error;

	/* also create the hwna krings */
	error = hwna->nm_krings_create(hwna);
	if (error) {
		netmap_vp_krings_delete(na);
		return error;
	}
	/* the connection between the bwrap krings and the hwna krings
	 * will be perfomed later, in the nm_register callback, since
	 * now the kring->ring pointers have not been initialized yet
	 */

	if (na->na_flags & NAF_HOST_RINGS) {
		/* the hostna rings are the host rings of the bwrap.
		 * The corresponding krings must point back to the
		 * hostna
		 */
		hostna->tx_rings = &na->tx_rings[na->num_tx_rings];
		hostna->tx_rings[0].na = hostna;
		hostna->rx_rings = &na->rx_rings[na->num_rx_rings];
		hostna->rx_rings[0].na = hostna;
	}

	return 0;
}


static void
netmap_bwrap_krings_delete(struct netmap_adapter *na)
{
	struct netmap_bwrap_adapter *bna =
		(struct netmap_bwrap_adapter *)na;
	struct netmap_adapter *hwna = bna->hwna;

	ND("%s", na->name);

	hwna->nm_krings_delete(hwna);
	netmap_vp_krings_delete(na);
}


/* notify method for the bridge-->hwna direction */
static int
netmap_bwrap_notify(struct netmap_kring *kring, int flags)
{
	struct netmap_adapter *na = kring->na;
	struct netmap_bwrap_adapter *bna = na->na_private;
	struct netmap_adapter *hwna = bna->hwna;
	u_int ring_n = kring->ring_id;
	u_int lim = kring->nkr_num_slots - 1;
	struct netmap_kring *hw_kring;
	int error = 0;

	ND("%s: na %s hwna %s", 
			(kring ? kring->name : "NULL!"),
			(na ? na->name : "NULL!"),
			(hwna ? hwna->name : "NULL!"));
	hw_kring = &hwna->tx_rings[ring_n];

	if (nm_kr_tryget(hw_kring))
		return 0;

	if (!nm_netmap_on(hwna))
		return 0;
	/* first step: simulate a user wakeup on the rx ring */
	netmap_vp_rxsync(kring, flags);
	ND("%s[%d] PRE rx(c%3d t%3d l%3d) ring(h%3d c%3d t%3d) tx(c%3d ht%3d t%3d)",
		na->name, ring_n,
		kring->nr_hwcur, kring->nr_hwtail, kring->nkr_hwlease,
		ring->head, ring->cur, ring->tail,
		hw_kring->nr_hwcur, hw_kring->nr_hwtail, hw_ring->rtail);
	/* second step: the new packets are sent on the tx ring
	 * (which is actually the same ring)
	 */
	hw_kring->rhead = hw_kring->rcur = kring->nr_hwtail;
	error = hw_kring->nm_sync(hw_kring, flags);
	if (error)
		goto out;

	/* third step: now we are back the rx ring */
	/* claim ownership on all hw owned bufs */
	kring->rhead = kring->rcur = nm_next(hw_kring->nr_hwtail, lim); /* skip past reserved slot */

	/* fourth step: the user goes to sleep again, causing another rxsync */
	netmap_vp_rxsync(kring, flags);
	ND("%s[%d] PST rx(c%3d t%3d l%3d) ring(h%3d c%3d t%3d) tx(c%3d ht%3d t%3d)",
		na->name, ring_n,
		kring->nr_hwcur, kring->nr_hwtail, kring->nkr_hwlease,
		ring->head, ring->cur, ring->tail,
		hw_kring->nr_hwcur, hw_kring->nr_hwtail, hw_kring->rtail);
out:
	nm_kr_put(hw_kring);
	return error;
}


/* nm_bdg_ctl callback for the bwrap.
 * Called on bridge-attach and detach, as an effect of vale-ctl -[ahd].
 * On attach, it needs to provide a fake netmap_priv_d structure and
 * perform a netmap_do_regif() on the bwrap. This will put both the
 * bwrap and the hwna in netmap mode, with the netmap rings shared
 * and cross linked. Moroever, it will start intercepting interrupts
 * directed to hwna.
 */
static int
netmap_bwrap_bdg_ctl(struct netmap_adapter *na, struct nmreq *nmr, int attach)
{
	struct netmap_priv_d *npriv;
	struct netmap_bwrap_adapter *bna = (struct netmap_bwrap_adapter*)na;
	int error = 0;

	if (attach) {
		if (NETMAP_OWNED_BY_ANY(na)) {
			return EBUSY;
		}
		if (bna->na_kpriv) {
			/* nothing to do */
			return 0;
		}
		npriv = malloc(sizeof(*npriv), M_DEVBUF, M_NOWAIT|M_ZERO);
		if (npriv == NULL)
			return ENOMEM;
		error = netmap_do_regif(npriv, na, nmr->nr_ringid, nmr->nr_flags);
		if (error) {
			bzero(npriv, sizeof(*npriv));
			free(npriv, M_DEVBUF);
			return error;
		}
		bna->na_kpriv = npriv;
		na->na_flags |= NAF_BUSY;
	} else {
		int last_instance;

		if (na->active_fds == 0) /* not registered */
			return EINVAL;
		last_instance = netmap_dtor_locked(bna->na_kpriv);
		if (!last_instance) {
			D("--- error, trying to detach an entry with active mmaps");
			error = EINVAL;
		} else {
			struct nm_bridge *b = bna->up.na_bdg,
				*bh = bna->host.na_bdg;
			npriv = bna->na_kpriv;
			bna->na_kpriv = NULL;
			D("deleting priv");

			bzero(npriv, sizeof(*npriv));
			free(npriv, M_DEVBUF);
			if (b) {
				/* XXX the bwrap dtor should take care
				 * of this (2014-06-16)
				 */
				netmap_bdg_detach_common(b, bna->up.bdg_port,
				    (bh ? bna->host.bdg_port : -1));
			}
			na->na_flags &= ~NAF_BUSY;
		}
	}
	return error;

}

/* attach a bridge wrapper to the 'real' device */
int
netmap_bwrap_attach(const char *nr_name, struct netmap_adapter *hwna)
{
	struct netmap_bwrap_adapter *bna;
	struct netmap_adapter *na = NULL;
	struct netmap_adapter *hostna = NULL;
	int error = 0;
	enum txrx t;

	/* make sure the NIC is not already in use */
	if (NETMAP_OWNED_BY_ANY(hwna)) {
		D("NIC %s busy, cannot attach to bridge", hwna->name);
		return EBUSY;
	}

	bna = malloc(sizeof(*bna), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (bna == NULL) {
		return ENOMEM;
	}

	na = &bna->up.up;
	na->na_private = bna;
	strncpy(na->name, nr_name, sizeof(na->name));
	/* fill the ring data for the bwrap adapter with rx/tx meanings
	 * swapped. The real cross-linking will be done during register,
	 * when all the krings will have been created.
	 */
	for_rx_tx(t) {
		enum txrx r = nm_txrx_swap(t); /* swap NR_TX <-> NR_RX */
		nma_set_nrings(na, t, nma_get_nrings(hwna, r));
		nma_set_ndesc(na, t, nma_get_ndesc(hwna, r));
	}
	na->nm_dtor = netmap_bwrap_dtor;
	na->nm_register = netmap_bwrap_register;
	// na->nm_txsync = netmap_bwrap_txsync;
	// na->nm_rxsync = netmap_bwrap_rxsync;
	na->nm_config = netmap_bwrap_config;
	na->nm_krings_create = netmap_bwrap_krings_create;
	na->nm_krings_delete = netmap_bwrap_krings_delete;
	na->nm_notify = netmap_bwrap_notify;
	na->nm_bdg_ctl = netmap_bwrap_bdg_ctl;
	na->pdev = hwna->pdev;
	na->nm_mem = netmap_mem_private_new(na->name,
			na->num_tx_rings, na->num_tx_desc,
			na->num_rx_rings, na->num_rx_desc,
			0, 0, &error);
	na->na_flags |= NAF_MEM_OWNER;
	if (na->nm_mem == NULL)
		goto err_put;
	bna->up.retry = 1; /* XXX maybe this should depend on the hwna */

	bna->hwna = hwna;
	netmap_adapter_get(hwna);
	hwna->na_private = bna; /* weak reference */
	hwna->na_vp = &bna->up;

	if (hwna->na_flags & NAF_HOST_RINGS) {
		if (hwna->na_flags & NAF_SW_ONLY)
			na->na_flags |= NAF_SW_ONLY;
		na->na_flags |= NAF_HOST_RINGS;
		hostna = &bna->host.up;
		snprintf(hostna->name, sizeof(hostna->name), "%s^", nr_name);
		hostna->ifp = hwna->ifp;
		for_rx_tx(t) {
			enum txrx r = nm_txrx_swap(t);
			nma_set_nrings(hostna, t, 1);
			nma_set_ndesc(hostna, t, nma_get_ndesc(hwna, r));
		}
		// hostna->nm_txsync = netmap_bwrap_host_txsync;
		// hostna->nm_rxsync = netmap_bwrap_host_rxsync;
		hostna->nm_notify = netmap_bwrap_notify;
		hostna->nm_mem = na->nm_mem;
		hostna->na_private = bna;
		hostna->na_vp = &bna->up;
		na->na_hostvp = hwna->na_hostvp =
			hostna->na_hostvp = &bna->host;
		hostna->na_flags = NAF_BUSY; /* prevent NIOCREGIF */
	}

	ND("%s<->%s txr %d txd %d rxr %d rxd %d",
		na->name, ifp->if_xname,
		na->num_tx_rings, na->num_tx_desc,
		na->num_rx_rings, na->num_rx_desc);

	error = netmap_attach_common(na);
	if (error) {
		goto err_free;
	}
	/* make bwrap ifp point to the real ifp
	 * NOTE: netmap_attach_common() interprets a non-NULL na->ifp
	 * as a request to make the ifp point to the na. Since we
	 * do not want to change the na already pointed to by hwna->ifp,
	 * the following assignment has to be delayed until now
	 */
	na->ifp = hwna->ifp;
	hwna->na_flags |= NAF_BUSY;
	/* make hwna point to the allocator we are actually using,
	 * so that monitors will be able to find it
	 */
	bna->save_nmd = hwna->nm_mem;
	hwna->nm_mem = na->nm_mem;
	return 0;

err_free:
	netmap_mem_delete(na->nm_mem);
err_put:
	hwna->na_vp = hwna->na_hostvp = NULL;
	netmap_adapter_put(hwna);
	free(bna, M_DEVBUF);
	return error;

}

struct nm_bridge *
netmap_init_bridges2(u_int n)
{
	int i;
	struct nm_bridge *b;

	b = malloc(sizeof(struct nm_bridge) * n, M_DEVBUF,
		M_NOWAIT | M_ZERO);
	if (b == NULL)
		return NULL;
	for (i = 0; i < n; i++)
		BDG_RWINIT(&b[i]);
	return b;
}

void
netmap_uninit_bridges2(struct nm_bridge *b, u_int n)
{
	int i;

	if (b == NULL)
		return;

	for (i = 0; i < n; i++)
		BDG_RWDESTROY(&b[i]);
	free(b, M_DEVBUF);
}

int
netmap_init_bridges(void)
{
#ifdef CONFIG_NET_NS
	return netmap_bns_register();
#else
	nm_bridges = netmap_init_bridges2(NM_BRIDGES);
	if (nm_bridges == NULL)
		return ENOMEM;
	return 0;
#endif
}

void
netmap_uninit_bridges(void)
{
#ifdef CONFIG_NET_NS
	netmap_bns_unregister();
#else
	netmap_uninit_bridges2(nm_bridges, NM_BRIDGES);
#endif
}
#endif /* WITH_VALE */
