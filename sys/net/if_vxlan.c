/*-
 * Copyright (c) 2014, Bryan Venteicher <bryanv@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/hash.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/refcount.h>
#include <sys/rwlock.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_clone.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_vxlan.h>
#include <net/netisr.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

struct vxlan_softc;
struct vxlan_ftable_entry;

LIST_HEAD(vxlan_ftable_head, vxlan_ftable_entry);
LIST_HEAD(vxlan_softc_head, vxlan_softc);

#define VXLAN_SO_VNI_HASH_SHIFT		5
#define VXLAN_SO_VNI_HASH_SIZE		(1 << VXLAN_SO_VNI_HASH_SHIFT)
#define VXLAN_SO_VNI_HASH(_vni)		((_vni) % VXLAN_SO_VNI_HASH_SIZE)

struct vxlan_socket {
	struct socket			*vxlso_sock;
	struct rwlock			 vxlso_lock;
	u_int				 vxlso_refcnt;
	union vxlan_sockaddr		 vxlso_laddr;
	LIST_ENTRY(vxlan_socket)	 vxlso_entry;
	/* Lookup hash from VNI to vxlan softc. */
	struct vxlan_softc_head		 vxlso_vni_hash[VXLAN_SO_VNI_HASH_SIZE];
};

#define VXLAN_SO_RLOCK(_vso)		rw_rlock(&(_vso)->vxlso_lock)
#define VXLAN_SO_RUNLOCK(_vso)		rw_runlock(&(_vso)->vxlso_lock)
#define VXLAN_SO_WLOCK(_vso)		rw_wlock(&(_vso)->vxlso_lock)
#define VXLAN_SO_WUNLOCK(_vso)		rw_wunlock(&(_vso)->vxlso_lock)
#define VXLAN_SO_UNLOCK(_vso)		rw_unlock(&(_vso)->vxlso_lock)
#define VXLAN_SO_LOCK_ASSERT(_vso) \
    rw_assert(&(_vso)->vxlso_lock, RA_LOCKED)
#define VXLAN_SO_LOCK_WASSERT(_vso) \
    rw_assert(&(_vso)->vxlso_lock, RA_WLOCKED)

#define VXLAN_SO_ACQUIRE(_vso)		refcount_acquire(&(_vso)->vxlso_refcnt)
#define VXLAN_SO_RELEASE(_vso)		refcount_release(&(_vso)->vxlso_refcnt)

struct vxlan_ftable_entry {
	LIST_ENTRY(vxlan_ftable_entry)	 vxlfe_hash;
	uint16_t			 vxlfe_flags;
	uint8_t				 vxlfe_mac[ETHER_ADDR_LEN];
	union vxlan_sockaddr		 vxlfe_raddr;
	time_t				 vxlfe_expire;
};

#define VXLAN_FE_FLAG_DYNAMIC		0x01
#define VXLAN_FE_FLAG_STATIC		0x02

#define VXLAN_FE_IS_DYNAMIC(_fe) \
    ((_fe)->vxlfe_flags & VXLAN_FE_FLAG_DYNAMIC)

#define VXLAN_SC_FTABLE_SHIFT		8
#define VXLAN_SC_FTABLE_SIZE		(1 << VXLAN_SC_FTABLE_SHIFT)
#define VXLAN_SC_FTABLE_MASK		(VXLAN_SC_FTABLE_SIZE - 1)
#define VXLAN_SC_FTABLE_HASH(_sc, _mac)	\
    (vxlan_mac_hash(_sc, _mac) % VXLAN_SC_FTABLE_SIZE)

struct vxlan_statistics {
	uint32_t	ftable_lock_upgrade;

};

struct vxlan_softc {
	struct ifnet			*vxl_ifp;
	struct vxlan_socket		*vxl_sock;
	struct rwlock			 vxl_lock;
	volatile u_int			 vxl_refcnt;
	union vxlan_sockaddr		 vxl_src_addr;
	union vxlan_sockaddr		 vxl_dst_addr;
	uint32_t			 vxl_vni;
	uint32_t			 vxl_flags;
#define VXLAN_FLAG_INITING	0x0001
#define VXLAN_FLAG_RELEASED	0x0002
#define VXLAN_FLAG_NOLEARN	0x0004

	uint32_t			 vxl_last_port_hash;
	uint16_t			 vxl_min_port;
	uint16_t			 vxl_max_port;
	uint8_t				 vxl_ttl;

	/* Lookup table from MAC address to forwarding entry. */
	uint32_t			 vxl_ftable_cnt;
	uint32_t			 vxl_ftable_max;
	uint32_t			 vxl_ftable_nospace;
	uint32_t			 vxl_ftable_timeout;
	uint32_t			 vxl_ftable_hash_key;
	struct vxlan_ftable_head	*vxl_ftable;

	/* Derived from vxl_dst_addr. */
	struct vxlan_ftable_entry	 vxl_default_fe;

	struct vxlan_statistics		 vxl_stats;
	int				 vxl_unit;
	struct callout			 vxl_callout;
	uint8_t				 vxl_hwaddr[ETHER_ADDR_LEN];
	LIST_ENTRY(vxlan_softc)		 vxl_entry;
};

#define VXLAN_RLOCK(_sc)	rw_rlock(&(_sc)->vxl_lock)
#define VXLAN_RUNLOCK(_sc)	rw_runlock(&(_sc)->vxl_lock)
#define VXLAN_WLOCK(_sc)	rw_wlock(&(_sc)->vxl_lock)
#define VXLAN_WUNLOCK(_sc)	rw_wunlock(&(_sc)->vxl_lock)
#define VXLAN_UNLOCK(_sc)	rw_unlock(&(_sc)->vxl_lock)
#define VXLAN_LOCK_UPGRADE(_sc)	rw_try_upgrade(&(_sc)->vxl_lock)
#define VXLAN_LOCK_WOWNED(_sc)	rw_wowned(&(_sc)->vxl_lock)
#define VXLAN_LOCK_ASSERT(_sc)	rw_assert(&(_sc)->vxl_lock, RA_LOCKED)
#define VXLAN_LOCK_WASSERT(_sc) rw_assert(&(_sc)->vxl_lock, RA_WLOCKED)

#define VXLAN_ACQUIRE(_sc)	refcount_acquire(&(_sc)->vxl_refcnt)
#define VXLAN_RELEASE(_sc)	refcount_release(&(_sc)->vxl_refcnt)

struct vxlanudphdr {
	struct udphdr		vxlh_udp;
	struct vxlan_header	vxlh_hdr;
} __packed;

static int	vxlan_ftable_addr_cmp(const uint8_t *, const uint8_t *);
static void	vxlan_ftable_init(struct vxlan_softc *);
static void	vxlan_ftable_fini(struct vxlan_softc *);
static void	vxlan_ftable_flush(struct vxlan_softc *, int);
static struct vxlan_ftable_entry *
		vxlan_ftable_entry_alloc(void);
static void	vxlan_ftable_entry_init(struct vxlan_softc *,
		    struct vxlan_ftable_entry *, const uint8_t *,
		    const struct sockaddr *, uint32_t);
static void	vxlan_ftable_entry_free(struct vxlan_ftable_entry *);
static struct vxlan_ftable_entry *
		vxlan_ftable_entry_lookup(struct vxlan_softc *,
		    const uint8_t *);
static int	vxlan_ftable_entry_insert(struct vxlan_softc *,
		    struct vxlan_ftable_entry *);
static void	vxlan_ftable_entry_destroy(struct vxlan_softc *,
		    struct vxlan_ftable_entry *fe);
static int	vxlan_ftable_update(struct vxlan_softc *,
		    const struct sockaddr *, const uint8_t *);
static void	vxlan_ftable_expire(struct vxlan_softc *);

static struct vxlan_socket *
		vxlan_socket_alloc(const union vxlan_sockaddr *);
static void	vxlan_socket_destroy(struct vxlan_socket *);
static int	vxlan_socket_create(struct ifnet *,
		    const union vxlan_sockaddr *, struct vxlan_socket **);
static int	vxlan_socket_insert_softc(struct vxlan_socket *,
		    struct vxlan_softc *);
static struct vxlan_softc *
		vxlan_socket_lookup_softc_locked(struct vxlan_socket *,
		    uint32_t);
static struct vxlan_softc *
		vxlan_socket_lookup_softc(struct vxlan_socket *, uint32_t);

static int	vxlan_valid_init_config(struct vxlan_softc *);
static void	vxlan_init(void *);
static void	vxlan_stop(struct vxlan_softc *);
static void	vxlan_release_socket(struct vxlan_softc *);
static void	vxlan_release(struct vxlan_softc *);
static void	vxlan_timer(void *);
static uint16_t vxlan_pick_source_port(struct vxlan_softc *,
		    const struct ether_header *);
static void	vxlan_encap_header(struct vxlan_softc *, struct mbuf *,
		    int, uint16_t, uint16_t);
static int	vxlan_encap4(struct vxlan_softc *,
		    const union vxlan_sockaddr *, struct mbuf *);
static int	vxlan_transmit(struct ifnet *, struct mbuf *);
static void	vxlan_qflush(struct ifnet *);

static void	vxlan_rcv_udp_packet(struct mbuf *, int, struct inpcb *,
		    const struct sockaddr *);
static int	vxlan_input(struct vxlan_socket *, uint32_t, struct mbuf **,
		    const struct sockaddr *);

static int	vxlan_ctrl_get_config(struct vxlan_softc *, void *);
static int	vxlan_ctrl_set_vni(struct vxlan_softc *, void *);
static int	vxlan_ctrl_set_local_addr(struct vxlan_softc *, void *);
static int	vxlan_ctrl_set_remote_addr(struct vxlan_softc *, void *);
static int	vxlan_ctrl_set_local_port(struct vxlan_softc *, void *);
static int	vxlan_ctrl_set_remote_port(struct vxlan_softc *, void *);
static int	vxlan_ctrl_flush(struct vxlan_softc *, void *);
static int	vxlan_ctrl_ftable_timeout(struct vxlan_softc *, void *);
static int	vxlan_ctrl_ftable_max(struct vxlan_softc *, void *);
static int	vxlan_ctrl_ftable_entry_add(struct vxlan_softc *, void *);
static int	vxlan_ctrl_ftable_entry_rem(struct vxlan_softc *, void *);
static int	vxlan_ctrl_ttl(struct vxlan_softc *, void *);
static int	vxlan_ctrl_learn(struct vxlan_softc *, void *);
static int	vxlan_ioctl_drvspec(struct vxlan_softc *,
		    struct ifdrv *, int);
static int	vxlan_ioctl_ifflags(struct vxlan_softc *);
static int	vxlan_ioctl(struct ifnet *, u_long, caddr_t);

static void	vxlan_set_default_config(struct vxlan_softc *);
static int	vxlan_set_user_config(struct vxlan_softc *,
		     struct ifvxlanparam *);
static int	vxlan_clone_create(struct if_clone *, int, caddr_t);
static void	vxlan_clone_destroy(struct ifnet *);

static uint32_t vxlan_mac_hash(struct vxlan_softc *, const uint8_t *);
static void	vxlan_fakeaddr(struct vxlan_softc *);
static int	vxlan_sockaddr_cmp(const union vxlan_sockaddr *,
		    const struct sockaddr *);
static void	vxlan_sockaddr_copy(union vxlan_sockaddr *,
		    const struct sockaddr *);
static int	vxlan_sockaddr_in_equal(const union vxlan_sockaddr *,
		    const struct sockaddr *);
static void	vxlan_sockaddr_in_copy(union vxlan_sockaddr *,
		    const struct sockaddr *);
static int	vxlan_sockaddr_in_any(const union vxlan_sockaddr *);
static int	vxlan_sockaddr_in_multicast(const union vxlan_sockaddr *);

static int	vxlan_initing_or_running(struct vxlan_softc *);
static int	vxlan_check_vni(uint32_t);
static int	vxlan_check_ttl(int);
static int	vxlan_check_ftable_timeout(uint32_t);
static int	vxlan_check_ftable_max(uint32_t);

static int	vxlan_tunable_int(struct vxlan_softc *, const char *, int);

SYSCTL_DECL(_net_link);
SYSCTL_NODE(_net_link, OID_AUTO, vxlan, CTLFLAG_RW, 0,
    "Virtual eXtensible Local Area Network");

static int vxlan_legacy_port = 0;
TUNABLE_INT("net.link.vxlan.legacy_port", &vxlan_legacy_port);

static const char vxlan_name[] = "vxlan";
static MALLOC_DEFINE(M_VXLAN, vxlan_name,
    "Virtual eXtensible LAN Interface");
static struct if_clone *vxlan_cloner;
static struct mtx vxlan_list_mtx;
static LIST_HEAD(, vxlan_socket) vxlan_socket_list;

/* Default maximum number of addresses in the forwarding table. */
#ifndef VXLAN_FTABLE_MAX
#define VXLAN_FTABLE_MAX	2000
#endif

/* Timeout (in seconds) of addresses learned in the forwarding table. */
#ifndef VXLAN_FTABLE_TIMEOUT
#define VXLAN_FTABLE_TIMEOUT	(20 * 60)
#endif

/* Number of seconds between pruning attempts of the forwarding table. */
#ifndef VXLAN_FTABLE_PRUNE
#define VXLAN_FTABLE_PRUNE	(5 * 60)
#endif

static int vxlan_ftable_prune_period = VXLAN_FTABLE_PRUNE;

struct vxlan_control {
	int	(*vxlc_func)(struct vxlan_softc *, void *);
	int	vxlc_argsize;
	int	vxlc_flags;
#define VXLAN_CTRL_FLAG_COPYIN	0x01
#define VXLAN_CTRL_FLAG_COPYOUT	0x02
#define VXLAN_CTRL_FLAG_SUSER	0x04
};

static const struct vxlan_control vxlan_control_table[] = {
	[VXLAN_CMD_GET_CONFIG] =
	    {	vxlan_ctrl_get_config,
		sizeof(struct ifvxlancfg),
		VXLAN_CTRL_FLAG_COPYOUT
	    },

	[VXLAN_CMD_SET_VNI] =
	    {   vxlan_ctrl_set_vni,
		sizeof(struct ifvxlancfg),
		VXLAN_CTRL_FLAG_COPYIN | VXLAN_CTRL_FLAG_SUSER,
	    },

	[VXLAN_CMD_SET_LOCAL_ADDR] =
	    {   vxlan_ctrl_set_local_addr,
		sizeof(struct ifvxlancfg),
		VXLAN_CTRL_FLAG_COPYIN | VXLAN_CTRL_FLAG_SUSER,
	    },

	[VXLAN_CMD_SET_REMOTE_ADDR] =
	    {   vxlan_ctrl_set_remote_addr,
		sizeof(struct ifvxlancfg),
		VXLAN_CTRL_FLAG_COPYIN | VXLAN_CTRL_FLAG_SUSER,
	    },

	[VXLAN_CMD_SET_LOCAL_PORT] =
	    {   vxlan_ctrl_set_local_port,
		sizeof(struct ifvxlancfg),
		VXLAN_CTRL_FLAG_COPYIN | VXLAN_CTRL_FLAG_SUSER,
	    },

	[VXLAN_CMD_SET_REMOTE_PORT] =
	    {   vxlan_ctrl_set_remote_port,
		sizeof(struct ifvxlancfg),
		VXLAN_CTRL_FLAG_COPYIN | VXLAN_CTRL_FLAG_SUSER,
	    },

	[VXLAN_CMD_FLUSH] =
	    {   vxlan_ctrl_flush,
		sizeof(struct ifvxlancmd),
		VXLAN_CTRL_FLAG_COPYIN | VXLAN_CTRL_FLAG_SUSER,
	    },

	[VXLAN_CMD_FTABLE_TIMEOUT] =
	    {	vxlan_ctrl_ftable_timeout,
		sizeof(struct ifvxlancmd),
		VXLAN_CTRL_FLAG_COPYIN | VXLAN_CTRL_FLAG_SUSER,
	    },

	[VXLAN_CMD_FTABLE_MAX] =
	    {	vxlan_ctrl_ftable_max,
		sizeof(struct ifvxlancmd),
		VXLAN_CTRL_FLAG_COPYIN | VXLAN_CTRL_FLAG_SUSER,
	    },

	[VXLAN_CMD_FTABLE_ENTRY_ADD] =
	    {	vxlan_ctrl_ftable_entry_add,
		sizeof(struct ifvxlancmd),
		VXLAN_CTRL_FLAG_COPYIN | VXLAN_CTRL_FLAG_SUSER,
	    },

	[VXLAN_CMD_FTABLE_ENTRY_REM] =
	    {	vxlan_ctrl_ftable_entry_rem,
		sizeof(struct ifvxlancmd),
		VXLAN_CTRL_FLAG_COPYIN | VXLAN_CTRL_FLAG_SUSER,
	    },

	[VXLAN_CMD_TTL] =
	    {	vxlan_ctrl_ttl,
		sizeof(struct ifvxlancmd),
		VXLAN_CTRL_FLAG_COPYIN | VXLAN_CTRL_FLAG_SUSER,
	    },

	[VXLAN_CMD_LEARN] =
	    {	vxlan_ctrl_learn,
		sizeof(struct ifvxlancmd),
		VXLAN_CTRL_FLAG_COPYIN | VXLAN_CTRL_FLAG_SUSER,
	    },
};

static const int vxlan_control_table_size = nitems(vxlan_control_table);

static int
vxlan_ftable_addr_cmp(const uint8_t *a, const uint8_t *b)
{
	int i, d;

	/* Same MAC comparison as done in if_bridge. */
	for (i = 0, d = 0; i < ETHER_ADDR_LEN && d == 0; i++)
		d = ((int)a[i]) - ((int)b[i]);

	return (d);
}

static void
vxlan_ftable_init(struct vxlan_softc *sc)
{
	static const uint8_t mac[ETHER_ADDR_LEN];
	int i;

	sc->vxl_ftable = malloc(sizeof(struct vxlan_ftable_head) *
	    VXLAN_SC_FTABLE_SIZE, M_VXLAN, M_ZERO | M_WAITOK);

	for (i = 0; i < VXLAN_SC_FTABLE_SIZE; i++)
		LIST_INIT(&sc->vxl_ftable[i]);
	sc->vxl_ftable_hash_key = arc4random();

	vxlan_ftable_entry_init(sc, &sc->vxl_default_fe, mac,
	    &sc->vxl_dst_addr.sa, VXLAN_FE_FLAG_STATIC);
}

static void
vxlan_ftable_fini(struct vxlan_softc *sc)
{
	int i;

	for (i = 0; i < VXLAN_SC_FTABLE_SIZE; i++)
		KASSERT(LIST_EMPTY(&sc->vxl_ftable[i]),
		    ("%s: vxlan %p ftable[%d] not empty", __func__, sc, i));
	MPASS(sc->vxl_ftable_cnt == 0);

	free(sc->vxl_ftable, M_VXLAN);
	sc->vxl_ftable = NULL;
}

static void
vxlan_ftable_flush(struct vxlan_softc *sc, int all)
{
	struct vxlan_ftable_entry *fe, *tfe;
	int i;

	for (i = 0; i < VXLAN_SC_FTABLE_SIZE; i++) {
		LIST_FOREACH_SAFE(fe, &sc->vxl_ftable[i], vxlfe_hash, tfe) {
			if (all || VXLAN_FE_IS_DYNAMIC(fe))
				vxlan_ftable_entry_destroy(sc, fe);
		}
	}
}

static void
vxlan_ftable_expire(struct vxlan_softc *sc)
{
	struct vxlan_ftable_entry *fe, *tfe;
	int i;

	VXLAN_LOCK_WASSERT(sc);

	for (i = 0; i < VXLAN_SC_FTABLE_SIZE; i++) {
		LIST_FOREACH_SAFE(fe, &sc->vxl_ftable[i], vxlfe_hash, tfe) {
			if (VXLAN_FE_IS_DYNAMIC(fe) &&
			    time_uptime >= fe->vxlfe_expire)
				vxlan_ftable_entry_destroy(sc, fe);
		}
	}
}

static int
vxlan_ftable_update_locked(struct vxlan_softc *sc, const struct sockaddr *sa,
    const uint8_t *mac)
{
	union vxlan_sockaddr vxlsa;
	struct vxlan_ftable_entry *fe;
	int error;

	VXLAN_LOCK_ASSERT(sc);

again:
	/*
	 * A forwarding entry for this MAC address might already exist. If
	 * so, update it, otherwise create a new one. We may have to upgrade
	 * the lock if we have to change or create an entry.
	 */
	fe = vxlan_ftable_entry_lookup(sc, mac);
	if (fe != NULL) {
		/* Accept the race if we only hold the read lock. */
		fe->vxlfe_expire = time_uptime + sc->vxl_ftable_timeout;

		if (!VXLAN_FE_IS_DYNAMIC(fe))
			return (0);
		if (vxlan_sockaddr_in_equal(&fe->vxlfe_raddr, sa))
			return (0);
		if (!VXLAN_LOCK_WOWNED(sc) && VXLAN_LOCK_UPGRADE(sc) == 0) {
			VXLAN_RUNLOCK(sc);
			VXLAN_WLOCK(sc);
			sc->vxl_stats.ftable_lock_upgrade++;
			goto again;
		}
		vxlan_sockaddr_in_copy(&fe->vxlfe_raddr, sa);
		return (0);
	}

	if (!VXLAN_LOCK_WOWNED(sc) && VXLAN_LOCK_UPGRADE(sc) == 0) {
		VXLAN_RUNLOCK(sc);
		VXLAN_WLOCK(sc);
		sc->vxl_stats.ftable_lock_upgrade++;
		goto again;
	}

	if (sc->vxl_ftable_cnt >= sc->vxl_ftable_max) {
		sc->vxl_ftable_nospace++;
		return (ENOSPC);
	}

	fe = vxlan_ftable_entry_alloc();
	if (fe == NULL)
		return (ENOMEM);

	/*
	 * The source port may be random, so always use the port of the
	 * default destination address.
	 */
	vxlan_sockaddr_copy(&vxlsa, sa);
	vxlsa.in4.sin_port = sc->vxl_dst_addr.in4.sin_port;

	vxlan_ftable_entry_init(sc, fe, mac, &vxlsa.sa,
	    VXLAN_FE_FLAG_DYNAMIC);

	/* The prior lookup failed, so the insert should not. */
	error = vxlan_ftable_entry_insert(sc, fe);
	MPASS(error == 0);

	return (0);
}

static int
vxlan_ftable_update(struct vxlan_softc *sc, const struct sockaddr *sa,
    const uint8_t *mac)
{
	int error;

	VXLAN_RLOCK(sc);
	error = vxlan_ftable_update_locked(sc, sa, mac);
	VXLAN_UNLOCK(sc);

	return (error);
}

static struct vxlan_ftable_entry *
vxlan_ftable_entry_alloc(void)
{
	struct vxlan_ftable_entry *fe;

	fe = malloc(sizeof(*fe), M_VXLAN, M_ZERO | M_NOWAIT);

	return (fe);
}

static void
vxlan_ftable_entry_free(struct vxlan_ftable_entry *fe)
{

	free(fe, M_VXLAN);
}

static void
vxlan_ftable_entry_init(struct vxlan_softc *sc, struct vxlan_ftable_entry *fe,
    const uint8_t *mac, const struct sockaddr *sa, uint32_t flags)
{

	fe->vxlfe_flags = flags;
	fe->vxlfe_expire = time_uptime + sc->vxl_ftable_timeout;
	memcpy(fe->vxlfe_mac, mac, ETHER_HDR_LEN);
	vxlan_sockaddr_copy(&fe->vxlfe_raddr, sa);
}

static void
vxlan_ftable_entry_destroy(struct vxlan_softc *sc,
    struct vxlan_ftable_entry *fe)
{

	sc->vxl_ftable_cnt--;
	LIST_REMOVE(fe, vxlfe_hash);
	vxlan_ftable_entry_free(fe);
}

static struct vxlan_ftable_entry *
vxlan_ftable_entry_lookup(struct vxlan_softc *sc, const uint8_t *mac)
{
	struct vxlan_ftable_entry *fe;
	uint32_t hash;
	int dir;

	VXLAN_LOCK_ASSERT(sc);
	hash = VXLAN_SC_FTABLE_HASH(sc, mac);

	LIST_FOREACH(fe, &sc->vxl_ftable[hash], vxlfe_hash) {
		dir = vxlan_ftable_addr_cmp(fe->vxlfe_mac, mac);
		if (dir == 0)
			return (fe);
		if (dir > 0)
			break;
	}

	return (NULL);
}

static int
vxlan_ftable_entry_insert(struct vxlan_softc *sc,
    struct vxlan_ftable_entry *fe)
{
	struct vxlan_ftable_entry *lfe;
	uint32_t hash;
	int dir;

	VXLAN_LOCK_WASSERT(sc);
	hash = VXLAN_SC_FTABLE_HASH(sc, fe->vxlfe_mac);

	lfe = LIST_FIRST(&sc->vxl_ftable[hash]);
	if (lfe == NULL) {
		LIST_INSERT_HEAD(&sc->vxl_ftable[hash], fe, vxlfe_hash);
		goto out;
	}

	do {
		dir = vxlan_ftable_addr_cmp(fe->vxlfe_mac, lfe->vxlfe_mac);
		if (dir == 0)
			return (EEXIST);
		if (dir > 0) {
			LIST_INSERT_BEFORE(lfe, fe, vxlfe_hash);
			goto out;
		} else if (LIST_NEXT(lfe, vxlfe_hash) == NULL) {
			LIST_INSERT_AFTER(lfe, fe, vxlfe_hash);
			goto out;
		} else
			lfe = LIST_NEXT(lfe, vxlfe_hash);
	} while (lfe != NULL);

out:
	sc->vxl_ftable_cnt++;
	return (0);
}

static struct vxlan_socket *
vxlan_socket_alloc(const union vxlan_sockaddr *vxlsa)
{
	struct vxlan_socket *vso;
	int i;

	vso = malloc(sizeof(*vso), M_VXLAN, M_WAITOK | M_ZERO);
	rw_init(&vso->vxlso_lock, "vxlansorw");
	refcount_init(&vso->vxlso_refcnt, 0);
	for (i = 0; i < VXLAN_SO_VNI_HASH_SIZE; i++)
		LIST_INIT(&vso->vxlso_vni_hash[i]);
	vso->vxlso_laddr = *vxlsa;

	return (vso);
}

static void
vxlan_socket_destroy(struct vxlan_socket *vso)
{
	struct socket *so;
	struct inpcb *inp;
	int i;

	for (i = 0; i < VXLAN_SO_VNI_HASH_SIZE; i++) {
		KASSERT(LIST_EMPTY(&vso->vxlso_vni_hash[i]),
		    ("%s: socket %p vni_hash[%d] not empty", __func__, vso, i));
	}

	so = vso->vxlso_sock;
	if (so != NULL) {
		vso->vxlso_sock = NULL;
		inp = sotoinpcb(so);
		MPASS(inp->inp_pspare[0] == vso);
		inp->inp_pspare[0] = NULL;
		soclose(so);
	}

	rw_destroy(&vso->vxlso_lock);
	free(vso, M_VXLAN);
}

static void
vxlan_socket_release(struct vxlan_socket *vso)
{
	int destroy;

	mtx_lock(&vxlan_list_mtx);
	destroy = VXLAN_SO_RELEASE(vso);
	if (destroy != 0)
		LIST_REMOVE(vso, vxlso_entry);
	mtx_unlock(&vxlan_list_mtx);

	if (destroy != 0)
		vxlan_socket_destroy(vso);
}

static struct vxlan_socket *
vxlan_socket_lookup(union vxlan_sockaddr *vxlsa)
{
	struct vxlan_socket *vso;

	mtx_lock(&vxlan_list_mtx);
	LIST_FOREACH(vso, &vxlan_socket_list, vxlso_entry) {
		if (vxlan_sockaddr_cmp(&vso->vxlso_laddr, &vxlsa->sa) == 0) {
			VXLAN_SO_ACQUIRE(vso);
			break;
		}
	}
	mtx_unlock(&vxlan_list_mtx);

	return (vso);
}

static void
vxlan_socket_insert(struct vxlan_socket *vso)
{

	mtx_lock(&vxlan_list_mtx);
	VXLAN_SO_ACQUIRE(vso);
	LIST_INSERT_HEAD(&vxlan_socket_list, vso, vxlso_entry);
	mtx_unlock(&vxlan_list_mtx);
}

static int
vxlan_socket_create(struct ifnet *ifp, const union vxlan_sockaddr *vxlsa,
    struct vxlan_socket **vsop)
{
	union vxlan_sockaddr baddr;
	struct vxlan_socket *vso;
	struct thread *td;
	struct inpcb *inp;
	int error;

	td = curthread;
	*vsop = NULL;

	vso = vxlan_socket_alloc(vxlsa);
	if (vso == NULL)
		return (ENOMEM);

	error = socreate(vso->vxlso_laddr.sa.sa_family, &vso->vxlso_sock,
	    SOCK_DGRAM, IPPROTO_UDP, td->td_ucred, td);
	if (error) {
		if_printf(ifp, "cannot create socket: %d\n", error);
		goto fail;
	}

	inp = sotoinpcb(vso->vxlso_sock);

	/*
	 * XXX: Use a spare field in the inpcb to obtain the vxlan socket
	 * in the tunneling callback. We instead should be able to pass a
	 * context to the tunneling callback.
	 */
	INP_WLOCK(inp);
	MPASS(inp->inp_pspare[0] == NULL);
	inp->inp_pspare[0] = vso;
	INP_WUNLOCK(inp);

	error = udp_set_kernel_tunneling(vso->vxlso_sock, vxlan_rcv_udp_packet);
	if (error) {
		if_printf(ifp, "cannot set tunneling function: %d\n", error);
		goto fail;
	}

	baddr = vso->vxlso_laddr;
	MPASS(baddr.in4.sin_port != 0);

	error = sobind(vso->vxlso_sock, &baddr.sa, td);
	if (error) {
		if (error != EADDRINUSE)
			if_printf(ifp, "cannot bind socket: %d\n", error);
		goto fail;
	}

	vxlan_socket_insert(vso);
	*vsop = vso;

	return (0);

fail:
	vxlan_socket_destroy(vso);
	return (error);
}

static struct vxlan_softc *
vxlan_socket_lookup_softc_locked(struct vxlan_socket *vso, uint32_t vni)
{
	struct vxlan_softc *sc;
	uint32_t hash;

	VXLAN_SO_LOCK_ASSERT(vso);
	hash = VXLAN_SO_VNI_HASH(vni);

	LIST_FOREACH(sc, &vso->vxlso_vni_hash[hash], vxl_entry) {
		if (sc->vxl_vni == vni) {
			VXLAN_ACQUIRE(sc);
			break;
		}
	}

	return (sc);
}

static struct vxlan_softc *
vxlan_socket_lookup_softc(struct vxlan_socket *vso, uint32_t vni)
{
	struct vxlan_softc *sc;

	VXLAN_SO_RLOCK(vso);
	sc = vxlan_socket_lookup_softc_locked(vso, vni);
	VXLAN_SO_RUNLOCK(vso);

	return (sc);
}

static int
vxlan_socket_insert_softc(struct vxlan_socket *vso, struct vxlan_softc *sc)
{
	struct vxlan_softc *tsc;
	uint32_t vni, hash;

	vni = sc->vxl_vni;
	hash = VXLAN_SO_VNI_HASH(vni);

	VXLAN_SO_WLOCK(vso);
	tsc = vxlan_socket_lookup_softc_locked(vso, vni);
	if (tsc != NULL) {
		VXLAN_SO_WUNLOCK(vso);
		vxlan_release(tsc);
		return (EEXIST);
	}

	VXLAN_ACQUIRE(sc);
	LIST_INSERT_HEAD(&vso->vxlso_vni_hash[hash], sc, vxl_entry);
	VXLAN_SO_WUNLOCK(vso);

	return (0);
}

static void
vxlan_socket_remove_softc(struct vxlan_socket *vso, struct vxlan_softc *sc)
{

	VXLAN_SO_WLOCK(vso);
	LIST_REMOVE(sc, vxl_entry);
	VXLAN_SO_WUNLOCK(vso);

	vxlan_release(sc);
}

static struct vxlan_socket *
vxlan_setup_socket(struct vxlan_softc *sc)
{
	struct vxlan_socket *vso;
	struct ifnet *ifp;
	union vxlan_sockaddr *saddr;
	int error;

	vso = NULL;
	ifp = sc->vxl_ifp;
	saddr = &sc->vxl_src_addr;

	error = vxlan_socket_create(ifp, saddr, &vso);
	if (error == 0)
		return (vso);

	/* Assume the bind failed. Try to use an existing socket. */
	vso = vxlan_socket_lookup(saddr);
	if (vso != NULL)
		return (vso);

	if_printf(ifp, "%s: cannot create socket, and no existing "
	    "socket found\n", __func__);

	return (NULL);
}

static int
vxlan_valid_init_config(struct vxlan_softc *sc)
{
	const char *reason;

	if (vxlan_check_vni(sc->vxl_vni) != 0) {
		reason = "no valid virtual network identifier specified";
		goto fail;
	}

	if (vxlan_sockaddr_in_any(&sc->vxl_dst_addr) != 0) {
		reason = "no valid destination address specified";
		goto fail;
	}

	if (!vxlan_sockaddr_in_any(&sc->vxl_src_addr)) {
		if (VXLAN_SOCKADDR_IS_IPV4(&sc->vxl_src_addr) ^
		    VXLAN_SOCKADDR_IS_IPV4(&sc->vxl_dst_addr)) {
			reason = "source and destination address must be "
			    "both IPv4 or IPv6";
			goto fail;
		}
	}

	if (sc->vxl_src_addr.in4.sin_port == 0) {
		reason = "local port not specified";
		goto fail;
	}

	if (sc->vxl_dst_addr.in4.sin_port == 0) {
		reason = "remote port not specified";
		goto fail;
	}

	return (0);

fail:
	if_printf(sc->vxl_ifp, "cannot init interface: %s\n", reason);

	return (EINVAL);
}

static void
vxlan_init(void *xsc)
{
	struct vxlan_softc *sc;
	struct ifnet *ifp;
	struct vxlan_socket *vso;

	sc = xsc;
	vso = NULL;
	ifp = sc->vxl_ifp;

	VXLAN_WLOCK(sc);
	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		VXLAN_WUNLOCK(sc);
		return;
	}
	sc->vxl_flags |= VXLAN_FLAG_INITING;
	VXLAN_WUNLOCK(sc);

	if (vxlan_valid_init_config(sc) != 0)
		goto out;

	vso = vxlan_setup_socket(sc);
	if (vso == NULL)
		goto out;

	sc->vxl_sock = vso;

	if (vxlan_socket_insert_softc(vso, sc) != 0) {
		sc->vxl_sock = NULL;
		goto out;
	}

	VXLAN_WLOCK(sc);
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	callout_reset(&sc->vxl_callout, vxlan_ftable_prune_period * hz,
	    vxlan_timer, sc);
	sc->vxl_flags &= ~VXLAN_FLAG_INITING;
	VXLAN_WUNLOCK(sc);

	return;

out:
	if (vso != NULL)
		vxlan_socket_release(vso);

	VXLAN_WLOCK(sc);
	sc->vxl_flags &= ~VXLAN_FLAG_INITING;
	VXLAN_WUNLOCK(sc);
}

static void
vxlan_stop(struct vxlan_softc *sc)
{
	struct ifnet *ifp;

	ifp = sc->vxl_ifp;

	VXLAN_WLOCK(sc);
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	callout_stop(&sc->vxl_callout);
	VXLAN_WUNLOCK(sc);
}

static void
vxlan_release_socket(struct vxlan_softc *sc)
{
	struct vxlan_socket *vso;

	VXLAN_WLOCK(sc);
	vso = sc->vxl_sock;
	sc->vxl_sock = NULL;
	VXLAN_WUNLOCK(sc);

	if (vso != NULL)
		vxlan_socket_release(vso);
}

static void
vxlan_release(struct vxlan_softc *sc)
{

	if (VXLAN_RELEASE(sc) != 0) {
		VXLAN_WLOCK(sc);
		sc->vxl_flags |= VXLAN_FLAG_RELEASED;
		wakeup(sc);
		VXLAN_WUNLOCK(sc);
	}
}

static void
vxlan_drain(struct vxlan_softc *sc)
{

	if (sc->vxl_sock != NULL)
		vxlan_socket_remove_softc(sc->vxl_sock, sc);

	VXLAN_WLOCK(sc);
	while (sc->vxl_refcnt != 0)
		rw_sleep(sc, &sc->vxl_lock, 0, "vxldrn", hz);
	VXLAN_WUNLOCK(sc);
}

static void
vxlan_timer(void *xsc)
{
	struct vxlan_softc *sc;

	sc = xsc;
	VXLAN_LOCK_WASSERT(sc);

	vxlan_ftable_expire(sc);
	callout_schedule(&sc->vxl_callout, vxlan_ftable_prune_period * hz);
}

static int
vxlan_ioctl_ifflags(struct vxlan_softc *sc)
{
	struct ifnet *ifp;

	ifp = sc->vxl_ifp;

	if ((ifp->if_flags & IFF_UP) == 0) {
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			vxlan_stop(sc);
			callout_drain(&sc->vxl_callout);
			vxlan_drain(sc);
			vxlan_release_socket(sc);
		}
	} else if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		vxlan_init(sc);

	return (0);
}

static int
vxlan_ctrl_get_config(struct vxlan_softc *sc, void *arg)
{
	struct ifvxlancfg *cfg;

	cfg = arg;
	bzero(cfg, sizeof(*cfg));

	VXLAN_RLOCK(sc);
	cfg->vxlc_vni = sc->vxl_vni;
	memcpy(&cfg->vxlc_local_sa, &sc->vxl_src_addr,
	    sizeof(union vxlan_sockaddr));
	memcpy(&cfg->vxlc_remote_sa, &sc->vxl_dst_addr,
	    sizeof(union vxlan_sockaddr));
	cfg->vxlc_ftable_cnt = sc->vxl_ftable_cnt;
	cfg->vxlc_ftable_max = sc->vxl_ftable_max;
	cfg->vxlc_ftable_timeout = sc->vxl_ftable_timeout;
	cfg->vxlc_nolearn = (sc->vxl_flags & VXLAN_FLAG_NOLEARN) != 0;
	cfg->vxlc_ttl = sc->vxl_ttl;
	VXLAN_RUNLOCK(sc);

	return (0);
}

static int
vxlan_ctrl_set_vni(struct vxlan_softc *sc, void *arg)
{
	struct ifvxlancmd *cmd;
	int error;

	cmd = arg;

	if (vxlan_check_vni(cmd->vxlcmd_vni) != 0)
		return (EINVAL);

	VXLAN_WLOCK(sc);
	if (!vxlan_initing_or_running(sc)) {
		sc->vxl_vni = cmd->vxlcmd_vni;
		error = 0;
	} else
		error = EBUSY;
	VXLAN_WUNLOCK(sc);

	return (error);
}

static int
vxlan_ctrl_set_local_addr(struct vxlan_softc *sc, void *arg)
{
	struct ifvxlancmd *cmd;
	union vxlan_sockaddr *vxlsa;
	int error;

	cmd = arg;
	vxlsa = &cmd->vxlcmd_sa;

	if (!VXLAN_SOCKADDR_IS_IPV4(vxlsa) && !VXLAN_SOCKADDR_IS_IPV6(vxlsa))
		return (EINVAL);
	if (vxlan_sockaddr_in_multicast(vxlsa) != 0)
		return (EINVAL);

	VXLAN_WLOCK(sc);
	if (!vxlan_initing_or_running(sc)) {
		vxlan_sockaddr_in_copy(&sc->vxl_src_addr, &vxlsa->sa);
		error = 0;
	} else
		error = EBUSY;
	VXLAN_WUNLOCK(sc);

	return (error);
}

static int
vxlan_ctrl_set_remote_addr(struct vxlan_softc *sc, void *arg)
{
	struct ifvxlancmd *cmd;
	union vxlan_sockaddr *vxlsa;
	int error;

	cmd = arg;
	vxlsa = &cmd->vxlcmd_sa;

	if (!VXLAN_SOCKADDR_IS_IPV4(vxlsa) &&
	    !VXLAN_SOCKADDR_IS_IPV6(vxlsa))
		return (EINVAL);

	VXLAN_WLOCK(sc);
	if (!vxlan_initing_or_running(sc)) {
		vxlan_sockaddr_in_copy(&sc->vxl_dst_addr, &vxlsa->sa);
		error = 0;
	} else
		error = EBUSY;
	VXLAN_WUNLOCK(sc);

	return (error);
}

static int
vxlan_ctrl_set_local_port(struct vxlan_softc *sc, void *arg)
{
	struct ifvxlancmd *cmd;
	int error;

	cmd = arg;

	if (cmd->vxlcmd_port == 0)
		return (EINVAL);

	VXLAN_WLOCK(sc);
	if (!vxlan_initing_or_running(sc)) {
		sc->vxl_src_addr.in4.sin_port = htons(cmd->vxlcmd_port);
		error = 0;
	} else
		error = EBUSY;
	VXLAN_WUNLOCK(sc);

	return (error);
}

static int
vxlan_ctrl_set_remote_port(struct vxlan_softc *sc, void *arg)
{
	struct ifvxlancmd *cmd;
	int error;

	cmd = arg;

	if (cmd->vxlcmd_port == 0)
		return (EINVAL);

	VXLAN_WLOCK(sc);
	if (!vxlan_initing_or_running(sc)) {
		sc->vxl_dst_addr.in4.sin_port = htons(cmd->vxlcmd_port);
		error = 0;
	} else
		error = EBUSY;
	VXLAN_WUNLOCK(sc);

	return (error);
}

static int
vxlan_ctrl_flush(struct vxlan_softc *sc, void *arg)
{
	struct ifvxlancmd *cmd;
	int all;

	cmd = arg;
	all = cmd->vxlcmd_flags & VXLAN_CMD_FLAG_FLUSH_ALL;

	VXLAN_WLOCK(sc);
	vxlan_ftable_flush(sc, all);
	VXLAN_WUNLOCK(sc);

	return (0);
}

static int
vxlan_ctrl_ftable_timeout(struct vxlan_softc *sc, void *arg)
{
	struct ifvxlancmd *cmd;
	int error;

	cmd = arg;

	VXLAN_WLOCK(sc);
	if (vxlan_check_ftable_timeout(cmd->vxlcmd_ftable_timeout) == 0) {
		sc->vxl_ftable_timeout = cmd->vxlcmd_ftable_timeout;
		error = 0;
	} else
		error = EINVAL;
	VXLAN_WUNLOCK(sc);

	return (error);
}

static int
vxlan_ctrl_ftable_max(struct vxlan_softc *sc, void *arg)
{
	struct ifvxlancmd *cmd;
	int error;

	cmd = arg;

	VXLAN_WLOCK(sc);
	if (vxlan_check_ftable_max(cmd->vxlcmd_ftable_max) == 0) {
		sc->vxl_ftable_max = cmd->vxlcmd_ftable_max;
		error = 0;
	} else
		error = EINVAL;
	VXLAN_WUNLOCK(sc);

	return (error);
}

static int
vxlan_ctrl_ftable_entry_add(struct vxlan_softc *sc, void *arg)
{
	union vxlan_sockaddr vxlsa;
	struct ifvxlancmd *cmd;
	struct vxlan_ftable_entry *fe;
	int error;

	cmd = arg;
	vxlsa = cmd->vxlcmd_sa;

	if (!VXLAN_SOCKADDR_IS_IPV4(&vxlsa) &&
	    !VXLAN_SOCKADDR_IS_IPV6(&vxlsa))
		return (EINVAL);
	if (vxlan_sockaddr_in_any(&vxlsa) ||
	    vxlan_sockaddr_in_multicast(&vxlsa))
		return (EINVAL);

	/*
	 * BMV: For now, force the address family of this forwarding entry to
	 * match our default destination. We can support both IPv4 and IPv6
	 * later.
	 */
	if (vxlsa.sa.sa_family != sc->vxl_dst_addr.sa.sa_family)
		return (ENOTSUP);

	fe = vxlan_ftable_entry_alloc();
	if (fe == NULL)
		return (ENOMEM);

	if (vxlsa.in4.sin_port == 0)
		vxlsa.in4.sin_port = sc->vxl_dst_addr.in4.sin_port;

	vxlan_ftable_entry_init(sc, fe, cmd->vxlcmd_mac, &vxlsa.sa,
	    VXLAN_FE_FLAG_STATIC);

	VXLAN_WLOCK(sc);
	error = vxlan_ftable_entry_insert(sc, fe);
	VXLAN_WUNLOCK(sc);

	if (error)
		vxlan_ftable_entry_free(fe);

	return (error);
}

static int
vxlan_ctrl_ftable_entry_rem(struct vxlan_softc *sc, void *arg)
{
	struct ifvxlancmd *cmd;
	struct vxlan_ftable_entry *fe;
	int error;

	cmd = arg;

	VXLAN_WLOCK(sc);
	fe = vxlan_ftable_entry_lookup(sc, cmd->vxlcmd_mac);
	if (fe != NULL) {
		vxlan_ftable_entry_destroy(sc, fe);
		error = 0;
	} else
		error = ENOENT;
	VXLAN_WUNLOCK(sc);

	return (error);
}

static int
vxlan_ctrl_ttl(struct vxlan_softc *sc, void *arg)
{
	struct ifvxlancmd *cmd;
	int error;

	cmd = arg;

	VXLAN_WLOCK(sc);
	if (vxlan_check_ttl(cmd->vxlcmd_ttl) == 0) {
		sc->vxl_ttl = cmd->vxlcmd_ttl;
		error = 0;
	} else
		error = EINVAL;
	VXLAN_WUNLOCK(sc);

	return (error);
}

static int
vxlan_ctrl_learn(struct vxlan_softc *sc, void *arg)
{
	struct ifvxlancmd *cmd;

	cmd = arg;

	VXLAN_WLOCK(sc);
	if (cmd->vxlcmd_flags & VXLAN_CMD_FLAG_LEARN)
		sc->vxl_flags &= ~VXLAN_FLAG_NOLEARN;
	else
		sc->vxl_flags |= VXLAN_FLAG_NOLEARN;
	VXLAN_WUNLOCK(sc);

	return (0);
}

#define PRIV_NET_VXLAN PRIV_NET_BRIDGE

static int
vxlan_ioctl_drvspec(struct vxlan_softc *sc, struct ifdrv *ifd, int get)
{
	const struct vxlan_control *vc;
	union {
		struct ifvxlancfg	cfg;
		struct ifvxlancmd	cmd;
	} args;
	int out, error;

	if (ifd->ifd_cmd >= vxlan_control_table_size)
		return (EINVAL);

	bzero(&args, sizeof(args));
	vc = &vxlan_control_table[ifd->ifd_cmd];
	out = (vc->vxlc_flags & VXLAN_CTRL_FLAG_COPYOUT) != 0;

	if ((get != 0 && out == 0) || (get == 0 && out != 0))
		return (EINVAL);

	if (vc->vxlc_flags & VXLAN_CTRL_FLAG_SUSER) {
		error = priv_check(curthread, PRIV_NET_VXLAN);
		if (error)
			return (error);
	}

	if (ifd->ifd_len != vc->vxlc_argsize ||
	    ifd->ifd_len > sizeof(args))
		return (EINVAL);

	if (vc->vxlc_flags & VXLAN_CTRL_FLAG_COPYIN) {
		error = copyin(ifd->ifd_data, &args, ifd->ifd_len);
		if (error)
			return (error);
	}

	error = vc->vxlc_func(sc, &args);
	if (error)
		return (error);

	if (vc->vxlc_flags & VXLAN_CTRL_FLAG_COPYOUT) {
		error = copyout(&args, ifd->ifd_data, ifd->ifd_len);
		if (error)
			return (error);
	}

	return (0);
}

#undef PRIV_NET_VXLAN

static int
vxlan_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct vxlan_softc *sc;
	struct ifreq *ifr;
	struct ifdrv *ifd;
	int error;

	sc = ifp->if_softc;
	ifr = (struct ifreq *) data;
	ifd = (struct ifdrv *) data;

	switch (cmd) {
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = 0;
		break;

	case SIOCGDRVSPEC:
	case SIOCSDRVSPEC:
		error = vxlan_ioctl_drvspec(sc, ifd, cmd == SIOCGDRVSPEC);
		break;

	case SIOCSIFFLAGS:
		error = vxlan_ioctl_ifflags(sc);
		break;
	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	return (error);
}

static uint16_t
vxlan_pick_source_port(struct vxlan_softc *sc, const struct ether_header *eh)
{
	uint32_t hash;
	int range;

	range = sc->vxl_max_port - sc->vxl_min_port + 1;

	/*
	 * The specification recommends the source port be based on a hash
	 * of the inner frame's Ethernet header.
	 */
	hash = jenkins_hash(eh, ETHER_HDR_LEN, sc->vxl_last_port_hash);
	sc->vxl_last_port_hash = hash;

	return (sc->vxl_min_port + (hash % range));
}

static void
vxlan_encap_header(struct vxlan_softc *sc, struct mbuf *m, int ipoff,
    uint16_t srcport, uint16_t dstport)
{
	struct vxlanudphdr *hdr;
	struct udphdr *udph;
	struct vxlan_header *vxh;
	int len;

	len = m->m_pkthdr.len - ipoff;
	MPASS(len >= sizeof(struct vxlanudphdr));
	hdr = mtodo(m, ipoff);

	udph = &hdr->vxlh_udp;
	udph->uh_sport = srcport;
	udph->uh_dport = dstport;
	udph->uh_ulen = htons(len);
	udph->uh_sum = 0;

	vxh = &hdr->vxlh_hdr;
	vxh->vxlh_flags = htonl(VXLAN_HDR_FLAGS_VALID_VNI);
	vxh->vxlh_vni = htonl(sc->vxl_vni << VXLAN_HDR_VNI_SHIFT);
}

static int
vxlan_encap4(struct vxlan_softc *sc, const union vxlan_sockaddr *fvxlsa,
    struct mbuf *m)
{
	struct ifnet *ifp;
	struct ip *ip;
	struct in_addr srcaddr, dstaddr;
	uint16_t srcport, dstport;
	int len, mcast, error;

	ifp = sc->vxl_ifp;
	srcaddr = sc->vxl_src_addr.in4.sin_addr;
	srcport = vxlan_pick_source_port(sc, mtod(m, struct ether_header *));
	dstaddr = fvxlsa->in4.sin_addr;
	dstport = fvxlsa->in4.sin_port;

	M_PREPEND(m, sizeof(struct ip) + sizeof(struct vxlanudphdr), M_NOWAIT);
	if (m == NULL) {
		ifp->if_oerrors++;
		return (ENOBUFS);
	}

	len = m->m_pkthdr.len;

	ip = mtod(m, struct ip *);
	ip->ip_tos = 0;
	ip->ip_len = htons(len);
	ip->ip_off = 0;
	ip->ip_ttl = sc->vxl_ttl;
	ip->ip_p = IPPROTO_UDP;
	ip->ip_sum = 0;
	ip->ip_src = srcaddr;
	ip->ip_dst = dstaddr;

	vxlan_encap_header(sc, m, sizeof(struct ip), srcport, dstport);

	mcast = (m->m_flags & (M_MCAST | M_BCAST)) ? 1 : 0;
	m->m_flags &= ~(M_MCAST | M_BCAST);

	error = ip_output(m, NULL, NULL, 0, NULL, NULL);
	if (error == 0) {
		ifp->if_opackets++;
		ifp->if_obytes += len;
		ifp->if_omcasts += mcast;
	} else
		ifp->if_oerrors++;

	return (error);
}

static int
vxlan_transmit(struct ifnet *ifp, struct mbuf *m)
{
	struct vxlan_softc *sc;
	struct vxlan_ftable_entry *fe;
	struct ether_header *eh;
	union vxlan_sockaddr vxlsa;
	int error;

	sc = ifp->if_softc;
	eh = mtod(m, struct ether_header *);
	fe = NULL;

	ETHER_BPF_MTAP(ifp, m);

	VXLAN_RLOCK(sc);
	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		VXLAN_RUNLOCK(sc);
		m_freem(m);
		return (ENETDOWN);
	}

	if ((m->m_flags & (M_BCAST | M_MCAST)) == 0)
		fe = vxlan_ftable_entry_lookup(sc, eh->ether_dhost);
	if (fe == NULL)
		fe = &sc->vxl_default_fe;
	/*
	 * Copy the remote address. Determine later if we want to
	 * refcount the ftable entries instead.
	 */
	vxlan_sockaddr_copy(&vxlsa, &fe->vxlfe_raddr.sa);

	VXLAN_ACQUIRE(sc);
	VXLAN_RUNLOCK(sc);

	error = vxlan_encap4(sc, &vxlsa, m);
	vxlan_release(sc);

	return (error);
}

static void
vxlan_qflush(struct ifnet *ifp __unused)
{
}

static void
vxlan_rcv_udp_packet(struct mbuf *m, int offset, struct inpcb *inpcb,
    const struct sockaddr *srcsa)
{
	struct vxlan_socket *vso;
	struct vxlan_header *vxh, vxlanhdr;
	uint32_t vni;
	int error;

	vso = inpcb->inp_pspare[0];
	if (vso == NULL)
		goto out;

	M_ASSERTPKTHDR(m);
	offset += sizeof(struct udphdr);

	if (m->m_pkthdr.len < offset + sizeof(struct vxlan_header))
		goto out;

	if (__predict_false(m->m_len < offset + sizeof(struct vxlan_header))) {
		m_copydata(m, offset, sizeof(struct vxlan_header),
		    (caddr_t) &vxlanhdr);
		vxh = &vxlanhdr;
	} else
		vxh = mtodo(m, offset);

	/* The flag MUST be set for a valid VNI. Ignore reserved fields. */
	if ((ntohl(vxh->vxlh_flags) & VXLAN_HDR_FLAGS_VALID_VNI) == 0)
		goto out;

	vni = ntohl(vxh->vxlh_vni) >> VXLAN_HDR_VNI_SHIFT;
	/* Adjust to the start of the inner Ethernet frame. */
	m_adj(m, offset + sizeof(struct vxlan_header));

	error = vxlan_input(vso, vni, &m, srcsa);
	MPASS(error != 0 || m == NULL);

out:
	if (m != NULL)
		m_freem(m);
}

static int
vxlan_input(struct vxlan_socket *vso, uint32_t vni, struct mbuf **m0,
    const struct sockaddr *sa)
{
	struct vxlan_softc *sc;
	struct ifnet *ifp;
	struct mbuf *m;
	struct ether_header *eh;
	int error;

	sc = vxlan_socket_lookup_softc(vso, vni);
	if (sc == NULL)
		return (ENOENT);

	ifp = sc->vxl_ifp;
	m = *m0;
	eh = mtod(m, struct ether_header *);

	if (ifp == m->m_pkthdr.rcvif) {
		error = EDEADLK;
		goto out;
	}

	if ((sc->vxl_flags & VXLAN_FLAG_NOLEARN) == 0)
		vxlan_ftable_update(sc, sa, eh->ether_shost);

	m_clrprotoflags(m);
	m->m_pkthdr.rcvif = ifp;
	M_SETFIB(m, ifp->if_fib);

	error = netisr_dispatch(NETISR_ETHER, m);
	*m0 = NULL;

out:
	vxlan_release(sc);
	return (error);
}

static void
vxlan_set_default_config(struct vxlan_softc *sc)
{

	sc->vxl_vni = VXLAN_VNI_MAX;
	sc->vxl_ttl = IPDEFTTL;

	if (!vxlan_tunable_int(sc, "legacy_port", vxlan_legacy_port)) {
		sc->vxl_src_addr.in4.sin_port = htons(VXLAN_PORT);
		sc->vxl_dst_addr.in4.sin_port = htons(VXLAN_PORT);
	} else {
		sc->vxl_src_addr.in4.sin_port = htons(VXLAN_LEGACY_PORT);
		sc->vxl_dst_addr.in4.sin_port = htons(VXLAN_LEGACY_PORT);
	}

	sc->vxl_min_port = V_ipport_firstauto;
	sc->vxl_max_port = V_ipport_lastauto;

	sc->vxl_ftable_max = VXLAN_FTABLE_MAX;
	sc->vxl_ftable_timeout = VXLAN_FTABLE_TIMEOUT;
}

static int
vxlan_set_user_config(struct vxlan_softc *sc, struct ifvxlanparam *vxlp)
{

#ifndef INET
	if (vxlp->vxlp_with & (VXLAN_PARAM_WITH_LOCAL_ADDR4 |
	    VXLAN_PARAM_WITH_REMOTE_ADDR4))
		return (ENOTSUP);
#endif

#ifndef INET6
	if (vxlp->vxlp_with & (VXLAN_PARAM_WITH_LOCAL_ADDR6 |
	    VXLAN_PARAM_WITH_REMOTE_ADDR6))
		return (ENOTSUP);
#endif

	if (vxlp->vxlp_with & VXLAN_PARAM_WITH_VNI) {
		if (vxlan_check_vni(vxlp->vxlp_vni) != 0)
			return (EINVAL);
		sc->vxl_vni = vxlp->vxlp_vni;
	}

	if (vxlp->vxlp_with & VXLAN_PARAM_WITH_LOCAL_ADDR4) {
		sc->vxl_src_addr.in4.sin_len = sizeof(struct sockaddr_in);
		sc->vxl_src_addr.in4.sin_family = AF_INET;
		sc->vxl_src_addr.in4.sin_addr = vxlp->vxlp_local_in4;
	} else if (vxlp->vxlp_with & VXLAN_PARAM_WITH_LOCAL_ADDR6) {
		sc->vxl_src_addr.in6.sin6_len = sizeof(struct sockaddr_in6);
		sc->vxl_src_addr.in6.sin6_family = AF_INET6;
		sc->vxl_src_addr.in6.sin6_addr = vxlp->vxlp_local_in6;
	}

	if (vxlp->vxlp_with & VXLAN_PARAM_WITH_REMOTE_ADDR4) {
		sc->vxl_dst_addr.in4.sin_len = sizeof(struct sockaddr_in);
		sc->vxl_dst_addr.in4.sin_family = AF_INET;
		sc->vxl_dst_addr.in4.sin_addr = vxlp->vxlp_remote_in4;
	} else if (vxlp->vxlp_with & VXLAN_PARAM_WITH_REMOTE_ADDR6) {
		sc->vxl_dst_addr.in6.sin6_len = sizeof(struct sockaddr_in6);
		sc->vxl_dst_addr.in6.sin6_family = AF_INET6;
		sc->vxl_dst_addr.in6.sin6_addr = vxlp->vxlp_remote_in6;
	}

	if (vxlp->vxlp_with & VXLAN_PARAM_WITH_LOCAL_PORT)
		sc->vxl_src_addr.in4.sin_port = htons(vxlp->vxlp_local_port);
	if (vxlp->vxlp_with & VXLAN_PARAM_WITH_REMOTE_PORT)
		sc->vxl_dst_addr.in4.sin_port = htons(vxlp->vxlp_remote_port);

	if (vxlp->vxlp_with & VXLAN_PARAM_WITH_PORT_RANGE) {
		if (vxlp->vxlp_min_port <= vxlp->vxlp_max_port) {
			sc->vxl_min_port = vxlp->vxlp_min_port;
			sc->vxl_max_port = vxlp->vxlp_max_port;
		}
	}

	if (vxlp->vxlp_with & VXLAN_PARAM_WITH_FTABLE_TIMEOUT) {
		if (vxlan_check_ftable_timeout(vxlp->vxlp_ftable_timeout) == 0)
			sc->vxl_ftable_timeout = vxlp->vxlp_ftable_timeout;
	}

	if (vxlp->vxlp_with & VXLAN_PARAM_WITH_FTABLE_MAX) {
		if (vxlan_check_ftable_max(vxlp->vxlp_ftable_max) == 0)
			sc->vxl_ftable_max = vxlp->vxlp_ftable_max;
	}

	if (vxlp->vxlp_with & VXLAN_PARAM_WITH_TTL) {
		if (vxlan_check_ttl(vxlp->vxlp_ttl) == 0)
			sc->vxl_ttl = vxlp->vxlp_ttl;
	}

	if (vxlp->vxlp_with & VXLAN_PARAM_WITH_NOLEARN &&
	    vxlp->vxlp_nolearn != 0)
		sc->vxl_flags |= VXLAN_FLAG_NOLEARN;

	return (0);
}

static int
vxlan_clone_create(struct if_clone *ifc, int unit, caddr_t params)
{
	struct vxlan_softc *sc;
	struct ifnet *ifp;
	struct ifvxlanparam vxlp;
	int error;

	sc = malloc(sizeof(struct vxlan_softc), M_VXLAN, M_WAITOK | M_ZERO);
	sc->vxl_unit = unit;
	vxlan_set_default_config(sc);

	if (params != 0) {
		error = copyin(params, &vxlp, sizeof(vxlp));
		if (error)
			goto fail;

		error = vxlan_set_user_config(sc, &vxlp);
		if (error)
			goto fail;
	}

	ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		error = ENOSPC;
		goto fail;
	}

	sc->vxl_ifp = ifp;
	rw_init(&sc->vxl_lock, "vxlanrw");
	callout_init_rw(&sc->vxl_callout, &sc->vxl_lock, 0);
	sc->vxl_last_port_hash = arc4random();
	vxlan_ftable_init(sc);

	ifp->if_softc = sc;
	if_initname(ifp, vxlan_name, unit);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = vxlan_init;
	ifp->if_ioctl = vxlan_ioctl;
	ifp->if_transmit = vxlan_transmit;
	ifp->if_qflush = vxlan_qflush;

	vxlan_fakeaddr(sc);
	ether_ifattach(ifp, sc->vxl_hwaddr);
	/* Now undo some of the damage... */
	ifp->if_baudrate = 0;
	ifp->if_hdrlen = sizeof(struct vxlanudphdr); /* + sizeof(struct ip) */

	return (0);

fail:
	free(sc, M_VXLAN);
	return (error);
}

static void
vxlan_clone_destroy(struct ifnet *ifp)
{
	struct vxlan_softc *sc;

	sc = ifp->if_softc;

	vxlan_stop(sc);

	callout_drain(&sc->vxl_callout);
	vxlan_drain(sc);

	vxlan_release_socket(sc);
	vxlan_ftable_flush(sc, 1);

	ether_ifdetach(ifp);
	if_free(ifp);

	vxlan_ftable_fini(sc);

	rw_destroy(&sc->vxl_lock);
	free(sc, M_VXLAN);
}

/* BMV: Taken from if_bridge. */
static uint32_t
vxlan_mac_hash(struct vxlan_softc *sc, const uint8_t *addr)
{
	uint32_t a = 0x9e3779b9, b = 0x9e3779b9, c = sc->vxl_ftable_hash_key;

	b += addr[5] << 8;
	b += addr[4];
	a += addr[3] << 24;
	a += addr[2] << 16;
	a += addr[1] << 8;
	a += addr[0];

/*
 * The following hash function is adapted from "Hash Functions" by Bob Jenkins
 * ("Algorithm Alley", Dr. Dobbs Journal, September 1997).
 */
#define	mix(a, b, c)							\
do {									\
	a -= b; a -= c; a ^= (c >> 13);					\
	b -= c; b -= a; b ^= (a << 8);					\
	c -= a; c -= b; c ^= (b >> 13);					\
	a -= b; a -= c; a ^= (c >> 12);					\
	b -= c; b -= a; b ^= (a << 16);					\
	c -= a; c -= b; c ^= (b >> 5);					\
	a -= b; a -= c; a ^= (c >> 3);					\
	b -= c; b -= a; b ^= (a << 10);					\
	c -= a; c -= b; c ^= (b >> 15);					\
} while (0)

	mix(a, b, c);

#undef mix

	return (c);
}

static void
vxlan_fakeaddr(struct vxlan_softc *sc)
{

	/* Generate a non-multicast, locally administered address. */
	arc4rand(sc->vxl_hwaddr, ETHER_ADDR_LEN, 1);
	sc->vxl_hwaddr[0] &= ~1;
	sc->vxl_hwaddr[0] |= 2;
}

static int
vxlan_sockaddr_cmp(const union vxlan_sockaddr *vxladdr,
    const struct sockaddr *sa)
{

	return (bcmp(&vxladdr->sa, sa, vxladdr->sa.sa_len));
}

static void
vxlan_sockaddr_copy(union vxlan_sockaddr *vxladdr,
    const struct sockaddr *sa)
{

	MPASS(sa->sa_family == AF_INET || sa->sa_family == AF_INET6);

	if (sa->sa_family == AF_INET)
		vxladdr->in4 = *satosin(sa);
	else
		vxladdr->in6 = *satosin6(sa);
}

static int
vxlan_sockaddr_in_equal(const union vxlan_sockaddr *vxladdr,
    const struct sockaddr *sa)
{
	int equal;

	MPASS(sa->sa_family == AF_INET || sa->sa_family == AF_INET6);

	if (sa->sa_family == AF_INET) {
		struct in_addr *in4 = &satosin(sa)->sin_addr;
		equal = in4->s_addr == vxladdr->in4.sin_addr.s_addr;
	} else {
		struct in6_addr *in6 = &satosin6(sa)->sin6_addr;
		equal = IN6_ARE_ADDR_EQUAL(in6, &vxladdr->in6.sin6_addr);
	}

	return (equal);
}

static void
vxlan_sockaddr_in_copy(union vxlan_sockaddr *vxladdr,
    const struct sockaddr *sa)
{

	MPASS(sa->sa_family == AF_INET || sa->sa_family == AF_INET6);

	if (sa->sa_family == AF_INET) {
		struct in_addr *in4 = &satosin(sa)->sin_addr;
		vxladdr->in4.sin_family = AF_INET;
		vxladdr->in4.sin_addr = *in4;
	} else {
		struct in6_addr *in6 = &satosin6(sa)->sin6_addr;
		vxladdr->in6.sin6_family = AF_INET6;
		vxladdr->in6.sin6_addr = *in6;
	}
}

static int
vxlan_sockaddr_in_any(const union vxlan_sockaddr *vxladdr)
{
	const struct sockaddr *sa;
	int any;

	sa = &vxladdr->sa;

	if (sa->sa_family == AF_INET) {
		const struct in_addr *in4 = &satosin(sa)->sin_addr;
		any = in4->s_addr == INADDR_ANY;
	} else if (sa->sa_family == AF_INET6) {
		const struct in6_addr *in6 = &satosin6(sa)->sin6_addr;
		any = IN6_IS_ADDR_UNSPECIFIED(in6);
	} else
		any = 1;

	return (any);
}

static int
vxlan_sockaddr_in_multicast(const union vxlan_sockaddr *vxladdr)
{
	const struct sockaddr *sa;
	int mc;

	sa = &vxladdr->sa;

	if (sa->sa_family == AF_INET) {
		const struct in_addr *in4 = &satosin(sa)->sin_addr;
		mc = IN_MULTICAST(in4->s_addr);
	} else if (sa->sa_family == AF_INET6) {
		const struct in6_addr *in6 = &satosin6(sa)->sin6_addr;
		mc = IN6_IS_ADDR_MULTICAST(in6);
	} else
		mc = 1;

	return (mc);
}

static int
vxlan_initing_or_running(struct vxlan_softc *sc)
{
	struct ifnet *ifp;

	ifp = sc->vxl_ifp;
	VXLAN_LOCK_WASSERT(sc);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING ||
	    sc->vxl_flags & VXLAN_FLAG_INITING)
		return (1);

	return (0);
}

static int
vxlan_check_vni(uint32_t vni)
{

	return (vni >= VXLAN_VNI_MAX);
}

static int
vxlan_check_ttl(int ttl)
{

	return (ttl <= MAXTTL ? 0 : EINVAL);
}

static int
vxlan_check_ftable_timeout(uint32_t timeout)
{

	/* BMV Fix me. */
	return (0);
}

static int
vxlan_check_ftable_max(uint32_t max)
{

	return (max <= VXLAN_FTABLE_MAX);
}

static int
vxlan_tunable_int(struct vxlan_softc *sc, const char *knob, int def)
{
	char path[64];

	snprintf(path, sizeof(path), "net.link.vxlan.%d.%s",
	    sc->vxl_unit, knob);
	TUNABLE_INT_FETCH(path, &def);

	return (def);
}

static void
vxlan_load(void)
{

	mtx_init(&vxlan_list_mtx, "vxlan list", NULL, MTX_DEF);
	LIST_INIT(&vxlan_socket_list);
	vxlan_cloner = if_clone_simple(vxlan_name, vxlan_clone_create,
	    vxlan_clone_destroy, 0);
}

static void
vxlan_unload(void)
{

	if_clone_detach(vxlan_cloner);
	mtx_destroy(&vxlan_list_mtx);
	MPASS(LIST_EMPTY(&vxlan_socket_list));
}

static int
vxlan_modevent(module_t mod, int type, void *unused)
{
	int error;

	error = 0;

	switch (type) {
	case MOD_LOAD:
		vxlan_load();
		break;
	case MOD_UNLOAD:
		vxlan_unload();
		break;
	default:
		error = ENOTSUP;
		break;
	}

	return (error);
}

static moduledata_t vxlan_mod = {
	"if_vxlan",
	vxlan_modevent,
	0
};

DECLARE_MODULE(if_vxlan, vxlan_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(if_vxlan, 1);
