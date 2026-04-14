/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025-2026 Pouria Mousavizadeh Tehrani <pouria@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/hash.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/refcount.h>
#include <sys/rmlock.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sdt.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/sx.h>
#include <sys/systm.h>
#include <sys/counter.h>
#include <sys/jail.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_private.h>
#include <net/if_arp.h>
#include <net/if_clone.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/route/nhop.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_var.h>
#include <netinet6/scope6_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/in_fib.h>
#include <netinet6/in6_fib.h>
#include <netinet/ip_ecn.h>
#include <net/if_geneve.h>

#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_var.h>
#include <netlink/netlink_route.h>
#include <netlink/route/route_var.h>

#include <security/mac/mac_framework.h>

SDT_PROVIDER_DEFINE(if_geneve);

struct geneve_softc;
LIST_HEAD(geneve_softc_head, geneve_softc);

static struct sx geneve_sx;
SX_SYSINIT(geneve, &geneve_sx, "GENEVE global start/stop lock");

static unsigned geneve_osd_jail_slot;

union sockaddr_union {
	struct sockaddr		sa;
	struct sockaddr_in	sin;
	struct sockaddr_in6	sin6;
};

struct geneve_socket_mc_info {
	union sockaddr_union	gnvsomc_saddr;
	union sockaddr_union	gnvsomc_gaddr;
	int			gnvsomc_ifidx;
	int			gnvsomc_users;
};

/* The maximum MTU of encapsulated geneve packet. */
#define GENEVE_MAX_L3MTU	(IP_MAXPACKET - \
	    60 /* Maximum IPv4 header len */ - \
	    sizeof(struct udphdr) - \
	    sizeof(struct genevehdr))
#define GENEVE_MAX_MTU		(GENEVE_MAX_L3MTU - \
	    ETHER_HDR_LEN - ETHER_VLAN_ENCAP_LEN)

#define GENEVE_BASIC_IFCAPS (IFCAP_LINKSTATE | IFCAP_JUMBO_MTU | IFCAP_NV)

#define GENEVE_VERSION	0
#define GENEVE_VNI_MASK	(GENEVE_VNI_MAX - 1)

#define GENEVE_HDR_VNI_SHIFT	8

#define GENEVE_SO_MC_MAX_GROUPS		32

#define GENEVE_SO_VNI_HASH_SHIFT	6
#define GENEVE_SO_VNI_HASH_SIZE		(1 << GENEVE_SO_VNI_HASH_SHIFT)
#define GENEVE_SO_VNI_HASH(_vni)	((_vni) % GENEVE_SO_VNI_HASH_SIZE)

struct geneve_socket {
	struct socket			*gnvso_sock;
	struct rmlock			gnvso_lock;
	u_int				gnvso_refcnt;
	union sockaddr_union		gnvso_laddr;
	LIST_ENTRY(geneve_socket)	gnvso_entry;
	struct geneve_softc_head	gnvso_vni_hash[GENEVE_SO_VNI_HASH_SIZE];
	struct geneve_socket_mc_info	gnvso_mc[GENEVE_SO_MC_MAX_GROUPS];
};

#define GENEVE_SO_RLOCK(_gnvso, _p)	rm_rlock(&(_gnvso)->gnvso_lock, (_p))
#define GENEVE_SO_RUNLOCK(_gnvso, _p)	rm_runlock(&(_gnvso)->gnvso_lock, (_p))
#define GENEVE_SO_WLOCK(_gnvso)		rm_wlock(&(_gnvso)->gnvso_lock)
#define GENEVE_SO_WUNLOCK(_gnvso)		rm_wunlock(&(_gnvso)->gnvso_lock)
#define GENEVE_SO_LOCK_ASSERT(_gnvso) \
    rm_assert(&(_gnvso)->gnvso_lock, RA_LOCKED)
#define GENEVE_SO_LOCK_WASSERT(_gnvso) \
    rm_assert(&(_gnvso)->gnvso_lock, RA_WLOCKED)

#define GENEVE_SO_ACQUIRE(_gnvso)		refcount_acquire(&(_gnvso)->gnvso_refcnt)
#define GENEVE_SO_RELEASE(_gnvso)		refcount_release(&(_gnvso)->gnvso_refcnt)

struct gnv_ftable_entry {
	LIST_ENTRY(gnv_ftable_entry)	gnvfe_hash;
	uint16_t			gnvfe_flags;
	uint8_t				gnvfe_mac[ETHER_ADDR_LEN];
	union sockaddr_union		gnvfe_raddr;
	time_t				gnvfe_expire;
};

#define GENEVE_FE_FLAG_DYNAMIC		0x01
#define GENEVE_FE_FLAG_STATIC		0x02

#define GENEVE_FE_IS_DYNAMIC(_fe) \
    ((_fe)->gnvfe_flags & GENEVE_FE_FLAG_DYNAMIC)

#define GENEVE_SC_FTABLE_SHIFT		9
#define GENEVE_SC_FTABLE_SIZE		(1 << GENEVE_SC_FTABLE_SHIFT)
#define GENEVE_SC_FTABLE_MASK		(GENEVE_SC_FTABLE_SIZE - 1)
#define GENEVE_SC_FTABLE_HASH(_sc, _mac)	\
    (geneve_mac_hash(_sc, _mac) % GENEVE_SC_FTABLE_SIZE)

LIST_HEAD(geneve_ftable_head, gnv_ftable_entry);

struct geneve_statistics {
	uint32_t	ftable_nospace;
	uint32_t	ftable_lock_upgrade_failed;
	counter_u64_t	txcsum;
	counter_u64_t	tso;
	counter_u64_t	rxcsum;
};

struct geneve_softc {
	LIST_ENTRY(geneve_softc)	gnv_entry;

	struct ifnet			*gnv_ifp;
	uint32_t			gnv_flags;
#define GENEVE_FLAG_INIT		0x0001
#define GENEVE_FLAG_RUNNING		0x0002
#define GENEVE_FLAG_TEARDOWN		0x0004
#define GENEVE_FLAG_LEARN		0x0008
#define GENEVE_FLAG_USER_MTU		0x0010
#define GENEVE_FLAG_TTL_INHERIT		0x0020
#define GENEVE_FLAG_DSCP_INHERIT	0x0040
#define GENEVE_FLAG_COLLECT_METADATA	0x0080

	int				gnv_reqcap;
	int				gnv_reqcap2;
	struct geneve_socket		*gnv_sock;
	union sockaddr_union		gnv_src_addr;
	union sockaddr_union		gnv_dst_addr;
	uint32_t			gnv_fibnum;
	uint32_t			gnv_vni;
	uint32_t			gnv_port_hash_key;
	uint16_t			gnv_proto;
	uint16_t			gnv_min_port;
	uint16_t			gnv_max_port;
	uint8_t				gnv_ttl;
	enum ifla_geneve_df		gnv_df;

	/* Lookup table from MAC address to forwarding entry. */
	uint32_t			gnv_ftable_cnt;
	uint32_t			gnv_ftable_max;
	uint32_t			gnv_ftable_timeout;
	uint32_t			gnv_ftable_hash_key;
	struct geneve_ftable_head	*gnv_ftable;

	/* Derived from gnv_dst_addr. */
	struct gnv_ftable_entry		gnv_default_fe;

	struct ip_moptions		*gnv_im4o;
	struct ip6_moptions		*gnv_im6o;

	struct rmlock			gnv_lock;
	volatile u_int			gnv_refcnt;

	int				gnv_so_mc_index;
	struct geneve_statistics	gnv_stats;
	struct callout			gnv_callout;
	struct ether_addr		gnv_hwaddr;
	int				gnv_mc_ifindex;
	struct ifnet			*gnv_mc_ifp;
	struct ifmedia			gnv_media;
	char				gnv_mc_ifname[IFNAMSIZ];

	/* For rate limiting errors on the tx fast path. */
	struct timeval			err_time;
	int				err_pps;
};

#define GENEVE_RLOCK(_sc, _p)	rm_rlock(&(_sc)->gnv_lock, (_p))
#define GENEVE_RUNLOCK(_sc, _p)	rm_runlock(&(_sc)->gnv_lock, (_p))
#define GENEVE_WLOCK(_sc)	rm_wlock(&(_sc)->gnv_lock)
#define GENEVE_WUNLOCK(_sc)	rm_wunlock(&(_sc)->gnv_lock)
#define GENEVE_LOCK_WOWNED(_sc)	rm_wowned(&(_sc)->gnv_lock)
#define GENEVE_LOCK_ASSERT(_sc)	rm_assert(&(_sc)->gnv_lock, RA_LOCKED)
#define GENEVE_LOCK_WASSERT(_sc) rm_assert(&(_sc)->gnv_lock, RA_WLOCKED)
#define GENEVE_UNLOCK(_sc, _p) do {		\
    if (GENEVE_LOCK_WOWNED(_sc))		\
	GENEVE_WUNLOCK(_sc);			\
    else					\
	GENEVE_RUNLOCK(_sc, _p);		\
} while (0)

#define GENEVE_ACQUIRE(_sc)	refcount_acquire(&(_sc)->gnv_refcnt)
#define GENEVE_RELEASE(_sc)	refcount_release(&(_sc)->gnv_refcnt)

#define	SATOCONSTSIN(sa)	((const struct sockaddr_in *)(sa))
#define	SATOCONSTSIN6(sa)	((const struct sockaddr_in6 *)(sa))

struct geneve_pkt_info {
	u_int		isr;
	uint16_t	ethertype;
	uint8_t		ecn;
	uint8_t		ttl;
};

struct nl_parsed_geneve {
	/* essential */
	uint32_t			ifla_vni;
	uint16_t			ifla_proto;
	struct sockaddr			*ifla_local;
	struct sockaddr			*ifla_remote;
	uint16_t			ifla_local_port;
	uint16_t			ifla_remote_port;

	/* optional */
	struct ifla_geneve_port_range	ifla_port_range;
	enum ifla_geneve_df		ifla_df;
	uint8_t				ifla_ttl;
	bool				ifla_ttl_inherit;
	bool				ifla_dscp_inherit;
	bool				ifla_external;

	/* l2 specific */
	bool				ifla_ftable_learn;
	bool				ifla_ftable_flush;
	uint32_t			ifla_ftable_max;
	uint32_t			ifla_ftable_timeout;
	uint32_t			ifla_ftable_count;	/* read-only */

	/* multicast specific */
	char				*ifla_mc_ifname;
	uint32_t			ifla_mc_ifindex;	/* read-only */
};

/* The multicast-based learning parts of the code are taken from if_vxlan */
static int	geneve_ftable_addr_cmp(const uint8_t *, const uint8_t *);
static void	geneve_ftable_init(struct geneve_softc *);
static void	geneve_ftable_fini(struct geneve_softc *);
static void	geneve_ftable_flush(struct geneve_softc *, int);
static void	geneve_ftable_expire(struct geneve_softc *);
static int	geneve_ftable_update_locked(struct geneve_softc *,
		    const union sockaddr_union *, const uint8_t *,
		    struct rm_priotracker *);
static int	geneve_ftable_learn(struct geneve_softc *,
		    const struct sockaddr *, const uint8_t *);

static struct gnv_ftable_entry *
		geneve_ftable_entry_alloc(void);
static void	geneve_ftable_entry_free(struct gnv_ftable_entry *);
static void	geneve_ftable_entry_init(struct geneve_softc *,
		    struct gnv_ftable_entry *, const uint8_t *,
		    const struct sockaddr *, uint32_t);
static void	geneve_ftable_entry_destroy(struct geneve_softc *,
		    struct gnv_ftable_entry *);
static int	geneve_ftable_entry_insert(struct geneve_softc *,
		    struct gnv_ftable_entry *);
static struct gnv_ftable_entry *
		geneve_ftable_entry_lookup(struct geneve_softc *,
		    const uint8_t *);

static struct geneve_socket *
		geneve_socket_alloc(union sockaddr_union *laddr);
static void	geneve_socket_destroy(struct geneve_socket *);
static void	geneve_socket_release(struct geneve_socket *);
static struct geneve_socket *
		geneve_socket_lookup(union sockaddr_union *);
static void	geneve_socket_insert(struct geneve_socket *);
static int	geneve_socket_init(struct geneve_socket *, struct ifnet *);
static int	geneve_socket_bind(struct geneve_socket *, struct ifnet *);
static int	geneve_socket_create(struct ifnet *, int,
		    const union sockaddr_union *, struct geneve_socket **);
static int	geneve_socket_set_df(struct geneve_socket *, bool);

static struct geneve_socket *
		geneve_socket_mc_lookup(const union sockaddr_union *);
static int	geneve_sockaddr_mc_info_match(
		    const struct geneve_socket_mc_info *,
		    const union sockaddr_union *,
		    const union sockaddr_union *, int);
static int	geneve_socket_mc_join_group(struct geneve_socket *,
		    const union sockaddr_union *, const union sockaddr_union *,
		    int *, union sockaddr_union *);
static int	geneve_socket_mc_leave_group(struct geneve_socket *,
		    const union sockaddr_union *,
		    const union sockaddr_union *, int);
static int	geneve_socket_mc_add_group(struct geneve_socket *,
		    const union sockaddr_union *,
		    const union sockaddr_union *, int, int *);
static void	geneve_socket_mc_release_group(struct geneve_socket *, int);

static struct geneve_softc *
		geneve_socket_lookup_softc_locked(struct geneve_socket *,
		    uint32_t);
static struct geneve_softc *
		geneve_socket_lookup_softc(struct geneve_socket *, uint32_t);
static int	geneve_socket_insert_softc(struct geneve_socket *,
		    struct geneve_softc *);
static void	geneve_socket_remove_softc(struct geneve_socket *,
		    struct geneve_softc *);

static struct ifnet *
		geneve_multicast_if_ref(struct geneve_softc *, uint32_t);
static void	geneve_free_multicast(struct geneve_softc *);
static int	geneve_setup_multicast_interface(struct geneve_softc *);

static int	geneve_setup_multicast(struct geneve_softc *);
static int	geneve_setup_socket(struct geneve_softc *);
static void	geneve_setup_interface_hdrlen(struct geneve_softc *);
static int	geneve_valid_init_config(struct geneve_softc *);
static void	geneve_init_complete(struct geneve_softc *);
static void	geneve_init(void *);
static void	geneve_release(struct geneve_softc *);
static void	geneve_teardown_wait(struct geneve_softc *);
static void	geneve_teardown_locked(struct geneve_softc *);
static void	geneve_teardown(struct geneve_softc *);
static void	geneve_timer(void *);

static int	geneve_flush_ftable(struct geneve_softc *, bool);
static uint16_t	geneve_get_local_port(struct geneve_softc *);
static uint16_t	geneve_get_remote_port(struct geneve_softc *);

static int	geneve_set_vni_nl(struct geneve_softc *, struct nl_pstate *,
		    uint32_t);
static int	geneve_set_local_addr_nl(struct geneve_softc *, struct nl_pstate *,
		    struct sockaddr *);
static int	geneve_set_remote_addr_nl(struct geneve_softc *, struct nl_pstate *,
		    struct sockaddr *);
static int	geneve_set_local_port_nl(struct geneve_softc *, struct nl_pstate *,
		    uint16_t);
static int	geneve_set_remote_port_nl(struct geneve_softc *, struct nl_pstate *,
		    uint16_t);
static int	geneve_set_port_range_nl(struct geneve_softc *, struct nl_pstate *,
		    struct ifla_geneve_port_range);
static int	geneve_set_df_nl(struct geneve_softc *, struct nl_pstate *,
		    enum ifla_geneve_df);
static int	geneve_set_ttl_nl(struct geneve_softc *, struct nl_pstate *,
		    uint8_t);
static int	geneve_set_ttl_inherit_nl(struct geneve_softc *, struct nl_pstate *,
		    bool);
static int	geneve_set_dscp_inherit_nl(struct geneve_softc *, struct nl_pstate *,
		    bool);
static int	geneve_set_collect_metadata_nl(struct geneve_softc *,
		    struct nl_pstate *, bool);
static int	geneve_set_learn_nl(struct geneve_softc *, struct nl_pstate *,
		    bool);
static int	geneve_set_ftable_max_nl(struct geneve_softc *, struct nl_pstate *,
		    uint32_t);
static int	geneve_set_ftable_timeout_nl(struct geneve_softc *,
		    struct nl_pstate *, uint32_t);
static int	geneve_set_mc_if_nl(struct geneve_softc *, struct nl_pstate *,
		    char *);
static int	geneve_flush_ftable_nl(struct geneve_softc *, struct nl_pstate *,
		    bool);
static void	geneve_get_local_addr_nl(struct geneve_softc *, struct nl_writer *);
static void	geneve_get_remote_addr_nl(struct geneve_softc *, struct nl_writer *);

static int	geneve_ioctl_ifflags(struct geneve_softc *);
static int	geneve_ioctl(struct ifnet *, u_long, caddr_t);

static uint16_t geneve_pick_source_port(struct geneve_softc *, struct mbuf *);
static void	geneve_encap_header(struct geneve_softc *, struct mbuf *,
		    int, uint16_t, uint16_t, uint16_t);
static uint16_t	geneve_get_ethertype(struct mbuf *);
static int	geneve_inherit_l3_hdr(struct mbuf *, struct geneve_softc *,
		    uint16_t, uint8_t *, uint8_t *, u_short *);
#ifdef INET
static int	geneve_encap4(struct geneve_softc *,
		    const union sockaddr_union *, struct mbuf *);
#endif
#ifdef INET6
static int	geneve_encap6(struct geneve_softc *,
		    const union sockaddr_union *, struct mbuf *);
#endif
static int	geneve_transmit(struct ifnet *, struct mbuf *);
static void	geneve_qflush(struct ifnet *);
static int	geneve_output(struct ifnet *, struct mbuf *,
		    const struct sockaddr *, struct route *);
static uint32_t	geneve_map_etype_to_af(uint32_t);
static bool	geneve_udp_input(struct mbuf *, int, struct inpcb *,
		    const struct sockaddr *, void *);
static int	geneve_input_ether(struct geneve_softc *, struct mbuf **,
		    const struct sockaddr *, struct geneve_pkt_info *);
static int	geneve_input_inherit(struct geneve_softc *,
		    struct mbuf **, int, struct geneve_pkt_info *);
static int	geneve_next_option(struct geneve_socket *, struct genevehdr *,
		    struct mbuf **);
static void	geneve_input_csum(struct mbuf *m, struct ifnet *ifp,
		    counter_u64_t rxcsum);

static void	geneve_stats_alloc(struct geneve_softc *);
static void	geneve_stats_free(struct geneve_softc *);
static void	geneve_set_default_config(struct geneve_softc *);
static int	geneve_set_reqcap(struct geneve_softc *, struct ifnet *, int,
		    int);
static void	geneve_set_hwcaps(struct geneve_softc *);
static int	geneve_clone_create(struct if_clone *, char *, size_t,
		    struct ifc_data *, struct ifnet **);
static int	geneve_clone_destroy(struct if_clone *, struct ifnet *,
		    uint32_t);
static int	geneve_clone_create_nl(struct if_clone *, char *, size_t,
		    struct ifc_data_nl *);
static int	geneve_clone_modify_nl(struct ifnet *, struct ifc_data_nl *);
static void	geneve_clone_dump_nl(struct ifnet *, struct nl_writer *);

static uint32_t geneve_mac_hash(struct geneve_softc *, const uint8_t *);
static int	geneve_media_change(struct ifnet *);
static void	geneve_media_status(struct ifnet *, struct ifmediareq *);

static int	geneve_sockaddr_cmp(const union sockaddr_union *,
		    const struct sockaddr *);
static void	geneve_sockaddr_copy(union sockaddr_union *,
		    const struct sockaddr *);
static int	geneve_sockaddr_in_equal(const union sockaddr_union *,
		    const struct sockaddr *);
static void	geneve_sockaddr_in_copy(union sockaddr_union *,
		    const struct sockaddr *);
static int	geneve_sockaddr_supported(const union sockaddr_union *, int);
static int	geneve_sockaddr_in_any(const union sockaddr_union *);

static int	geneve_can_change_config(struct geneve_softc *);
static int	geneve_check_proto(uint16_t);
static int	geneve_check_multicast_addr(const union sockaddr_union *);
static int	geneve_check_sockaddr(const union sockaddr_union *, const int);

static int	geneve_prison_remove(void *, void *);
static void	vnet_geneve_load(void);
static void	vnet_geneve_unload(void);
static void	geneve_module_init(void);
static void	geneve_module_deinit(void);
static int	geneve_modevent(module_t, int, void *);


static const char geneve_name[] = "geneve";
static MALLOC_DEFINE(M_GENEVE, geneve_name,
    "Generic Network Virtualization Encapsulation Interface");
#define MTAG_GENEVE_LOOP	0x93d66dc0 /* geneve mtag */

VNET_DEFINE_STATIC(struct if_clone *, geneve_cloner);
#define	V_geneve_cloner	VNET(geneve_cloner)

static struct mtx geneve_list_mtx;
#define GENEVE_LIST_LOCK()	mtx_lock(&geneve_list_mtx)
#define GENEVE_LIST_UNLOCK()	mtx_unlock(&geneve_list_mtx)

static LIST_HEAD(, geneve_socket) geneve_socket_list = LIST_HEAD_INITIALIZER(geneve_socket_list);

/* Default maximum number of addresses in the forwarding table. */
#define GENEVE_FTABLE_MAX	2000

/* Timeout (in seconds) of addresses learned in the forwarding table. */
#define GENEVE_FTABLE_TIMEOUT	(20 * 60)

/* Maximum timeout (in seconds) of addresses learned in the forwarding table. */
#define GENEVE_FTABLE_MAX_TIMEOUT	(60 * 60 * 24)

/* Number of seconds between pruning attempts of the forwarding table. */
#define GENEVE_FTABLE_PRUNE	(5 * 60)

static int geneve_ftable_prune_period = GENEVE_FTABLE_PRUNE;

#define _OUT(_field)	offsetof(struct nl_parsed_geneve, _field)
static const struct nlattr_parser nla_p_geneve_create[] = {
	{ .type = IFLA_GENEVE_PROTOCOL, .off = _OUT(ifla_proto), .cb = nlattr_get_uint16 },
};
#undef _OUT
NL_DECLARE_ATTR_PARSER(geneve_create_parser, nla_p_geneve_create);

#define _OUT(_field)	offsetof(struct nl_parsed_geneve, _field)
static const struct nlattr_parser nla_p_geneve[] = {
	{ .type = IFLA_GENEVE_ID, .off = _OUT(ifla_vni), .cb = nlattr_get_uint32 },
	{ .type = IFLA_GENEVE_PROTOCOL, .off = _OUT(ifla_proto), .cb = nlattr_get_uint16 },
	{ .type = IFLA_GENEVE_LOCAL, .off = _OUT(ifla_local), .cb = nlattr_get_ip },
	{ .type = IFLA_GENEVE_REMOTE, .off = _OUT(ifla_remote), .cb = nlattr_get_ip },
	{ .type = IFLA_GENEVE_LOCAL_PORT, .off = _OUT(ifla_local_port), .cb = nlattr_get_uint16 },
	{ .type = IFLA_GENEVE_PORT, .off = _OUT(ifla_remote_port), .cb = nlattr_get_uint16 },
	{ .type = IFLA_GENEVE_PORT_RANGE, .off = _OUT(ifla_port_range),
		.arg = (void *)sizeof(struct ifla_geneve_port_range), .cb = nlattr_get_bytes },
	{ .type = IFLA_GENEVE_DF, .off = _OUT(ifla_df), .cb = nlattr_get_uint8 },
	{ .type = IFLA_GENEVE_TTL, .off = _OUT(ifla_ttl), .cb = nlattr_get_uint8 },
	{ .type = IFLA_GENEVE_TTL_INHERIT, .off = _OUT(ifla_ttl_inherit), .cb = nlattr_get_bool },
	{ .type = IFLA_GENEVE_DSCP_INHERIT, .off = _OUT(ifla_dscp_inherit), .cb = nlattr_get_bool },
	{ .type = IFLA_GENEVE_COLLECT_METADATA, .off = _OUT(ifla_external), .cb = nlattr_get_bool },
	{ .type = IFLA_GENEVE_FTABLE_LEARN, .off = _OUT(ifla_ftable_learn), .cb = nlattr_get_bool },
	{ .type = IFLA_GENEVE_FTABLE_FLUSH, .off = _OUT(ifla_ftable_flush), .cb = nlattr_get_bool },
	{ .type = IFLA_GENEVE_FTABLE_MAX, .off = _OUT(ifla_ftable_max), .cb = nlattr_get_uint32 },
	{ .type = IFLA_GENEVE_FTABLE_TIMEOUT, .off = _OUT(ifla_ftable_timeout), .cb = nlattr_get_uint32 },
	{ .type = IFLA_GENEVE_MC_IFNAME, .off = _OUT(ifla_mc_ifname), .cb = nlattr_get_string },
};
#undef _OUT
NL_DECLARE_ATTR_PARSER(geneve_modify_parser, nla_p_geneve);

static const struct nlhdr_parser *all_parsers[] = {
	&geneve_create_parser, &geneve_modify_parser,
};

static int
geneve_ftable_addr_cmp(const uint8_t *a, const uint8_t *b)
{
	int i, d;

	for (i = 0, d = 0; i < ETHER_ADDR_LEN && d == 0; i++)
		d = (int)a[i] - (int)b[i];

	return (d);
}

static void
geneve_ftable_init(struct geneve_softc *sc)
{
	int i;

	sc->gnv_ftable = malloc(sizeof(struct geneve_ftable_head) *
	    GENEVE_SC_FTABLE_SIZE, M_GENEVE, M_ZERO | M_WAITOK);

	for (i = 0; i < GENEVE_SC_FTABLE_SIZE; i++)
		LIST_INIT(&sc->gnv_ftable[i]);
	sc->gnv_ftable_hash_key = arc4random();
}

static void
geneve_ftable_fini(struct geneve_softc *sc)
{
	int i;

	for (i = 0; i < GENEVE_SC_FTABLE_SIZE; i++) {
		KASSERT(LIST_EMPTY(&sc->gnv_ftable[i]),
		    ("%s: geneve %p ftable[%d] not empty", __func__, sc, i));
	}
	MPASS(sc->gnv_ftable_cnt == 0);

	free(sc->gnv_ftable, M_GENEVE);
	sc->gnv_ftable = NULL;
}

static void
geneve_ftable_flush(struct geneve_softc *sc, int all)
{
	struct gnv_ftable_entry *fe, *tfe;

	for (int i = 0; i < GENEVE_SC_FTABLE_SIZE; i++) {
		LIST_FOREACH_SAFE(fe, &sc->gnv_ftable[i], gnvfe_hash, tfe) {
			if (all || GENEVE_FE_IS_DYNAMIC(fe))
				geneve_ftable_entry_destroy(sc, fe);
		}
	}
}

static void
geneve_ftable_expire(struct geneve_softc *sc)
{
	struct gnv_ftable_entry *fe, *tfe;

	GENEVE_LOCK_WASSERT(sc);

	for (int i = 0; i < GENEVE_SC_FTABLE_SIZE; i++) {
		LIST_FOREACH_SAFE(fe, &sc->gnv_ftable[i], gnvfe_hash, tfe) {
			if (GENEVE_FE_IS_DYNAMIC(fe) &&
			    time_uptime >= fe->gnvfe_expire)
				geneve_ftable_entry_destroy(sc, fe);
		}
	}
}

static int
geneve_ftable_update_locked(struct geneve_softc *sc,
    const union sockaddr_union *unsa, const uint8_t *mac,
    struct rm_priotracker *tracker)
{
	struct gnv_ftable_entry *fe;
	int error;

	GENEVE_LOCK_ASSERT(sc);

again:
	/*
	 * A forwarding entry for this MAC address might already exist. If
	 * so, update it, otherwise create a new one. We may have to upgrade
	 * the lock if we have to change or create an entry.
	 */
	fe = geneve_ftable_entry_lookup(sc, mac);
	if (fe != NULL) {
		fe->gnvfe_expire = time_uptime + sc->gnv_ftable_timeout;

		if (!GENEVE_FE_IS_DYNAMIC(fe) ||
		    geneve_sockaddr_in_equal(&fe->gnvfe_raddr, &unsa->sa))
			return (0);
		if (!GENEVE_LOCK_WOWNED(sc)) {
			GENEVE_RUNLOCK(sc, tracker);
			GENEVE_WLOCK(sc);
			sc->gnv_stats.ftable_lock_upgrade_failed++;
			goto again;
		}
		geneve_sockaddr_in_copy(&fe->gnvfe_raddr, &unsa->sa);
		return (0);
	}

	if (!GENEVE_LOCK_WOWNED(sc)) {
		GENEVE_RUNLOCK(sc, tracker);
		GENEVE_WLOCK(sc);
		sc->gnv_stats.ftable_lock_upgrade_failed++;
		goto again;
	}

	if (sc->gnv_ftable_cnt >= sc->gnv_ftable_max) {
		sc->gnv_stats.ftable_nospace++;
		return (ENOSPC);
	}

	fe = geneve_ftable_entry_alloc();
	if (fe == NULL)
		return (ENOMEM);

	geneve_ftable_entry_init(sc, fe, mac, &unsa->sa, GENEVE_FE_FLAG_DYNAMIC);

	/* The prior lookup failed, so the insert should not. */
	error = geneve_ftable_entry_insert(sc, fe);
	MPASS(error == 0);

	return (error);
}

static int
geneve_ftable_learn(struct geneve_softc *sc, const struct sockaddr *sa,
    const uint8_t *mac)
{
	struct rm_priotracker tracker;
	union sockaddr_union unsa;
	int error;

	/*
	 * The source port may be randomly selected by the remote host, so
	 * use the port of the default destination address.
	 */
	geneve_sockaddr_copy(&unsa, sa);
	unsa.sin.sin_port = sc->gnv_dst_addr.sin.sin_port;

#ifdef INET6
	if (unsa.sa.sa_family == AF_INET6) {
		error = sa6_embedscope(&unsa.sin6, V_ip6_use_defzone);
		if (error)
			return (error);
	}
#endif

	GENEVE_RLOCK(sc, &tracker);
	error = geneve_ftable_update_locked(sc, &unsa, mac, &tracker);
	GENEVE_UNLOCK(sc, &tracker);

	return (error);
}

static struct gnv_ftable_entry *
geneve_ftable_entry_alloc(void)
{
	struct gnv_ftable_entry *fe;

	fe = malloc(sizeof(*fe), M_GENEVE, M_ZERO | M_NOWAIT);

	return (fe);
}

static void
geneve_ftable_entry_free(struct gnv_ftable_entry *fe)
{

	free(fe, M_GENEVE);
}

static void
geneve_ftable_entry_init(struct geneve_softc *sc, struct gnv_ftable_entry *fe,
    const uint8_t *mac, const struct sockaddr *sa, uint32_t flags)
{

	fe->gnvfe_flags = flags;
	fe->gnvfe_expire = time_uptime + sc->gnv_ftable_timeout;
	memcpy(fe->gnvfe_mac, mac, ETHER_ADDR_LEN);
	geneve_sockaddr_copy(&fe->gnvfe_raddr, sa);
}

static void
geneve_ftable_entry_destroy(struct geneve_softc *sc,
    struct gnv_ftable_entry *fe)
{

	sc->gnv_ftable_cnt--;
	LIST_REMOVE(fe, gnvfe_hash);
	geneve_ftable_entry_free(fe);
}

static int
geneve_ftable_entry_insert(struct geneve_softc *sc,
    struct gnv_ftable_entry *fe)
{
	struct gnv_ftable_entry *lfe;
	uint32_t hash;
	int dir;

	GENEVE_LOCK_WASSERT(sc);
	hash = GENEVE_SC_FTABLE_HASH(sc, fe->gnvfe_mac);

	lfe = LIST_FIRST(&sc->gnv_ftable[hash]);
	if (lfe == NULL) {
		LIST_INSERT_HEAD(&sc->gnv_ftable[hash], fe, gnvfe_hash);
		goto out;
	}

	do {
		dir = geneve_ftable_addr_cmp(fe->gnvfe_mac, lfe->gnvfe_mac);
		if (dir == 0)
			return (EEXIST);
		if (dir > 0) {
			LIST_INSERT_BEFORE(lfe, fe, gnvfe_hash);
			goto out;
		} else if (LIST_NEXT(lfe, gnvfe_hash) == NULL) {
			LIST_INSERT_AFTER(lfe, fe, gnvfe_hash);
			goto out;
		} else
			lfe = LIST_NEXT(lfe, gnvfe_hash);
	} while (lfe != NULL);

out:
	sc->gnv_ftable_cnt++;

	return (0);
}

static struct gnv_ftable_entry *
geneve_ftable_entry_lookup(struct geneve_softc *sc, const uint8_t *mac)
{
	struct gnv_ftable_entry *fe;
	uint32_t hash;
	int dir;

	GENEVE_LOCK_ASSERT(sc);

	hash = GENEVE_SC_FTABLE_HASH(sc, mac);
	LIST_FOREACH(fe, &sc->gnv_ftable[hash], gnvfe_hash) {
		dir = geneve_ftable_addr_cmp(mac, fe->gnvfe_mac);
		if (dir == 0)
			return (fe);
		if (dir > 0)
			break;
	}

	return (NULL);
}

static struct geneve_socket *
geneve_socket_alloc(union sockaddr_union *laddr)
{
	struct geneve_socket *gnvso;

	gnvso = malloc(sizeof(*gnvso), M_GENEVE, M_WAITOK | M_ZERO);
	rm_init(&gnvso->gnvso_lock, "genevesorm");
	refcount_init(&gnvso->gnvso_refcnt, 0);
	for (int i = 0; i < GENEVE_SO_VNI_HASH_SIZE; i++)
		LIST_INIT(&gnvso->gnvso_vni_hash[i]);
	gnvso->gnvso_laddr = *laddr;

	return (gnvso);
}

static void
geneve_socket_destroy(struct geneve_socket *gnvso)
{
	struct socket *so;

	so = gnvso->gnvso_sock;
	if (so != NULL) {
		gnvso->gnvso_sock = NULL;
		soclose(so);
	}

	rm_destroy(&gnvso->gnvso_lock);
	free(gnvso, M_GENEVE);
}

static void
geneve_socket_release(struct geneve_socket *gnvso)
{
	int destroy;

	GENEVE_LIST_LOCK();
	destroy = GENEVE_SO_RELEASE(gnvso);
	if (destroy != 0)
		LIST_REMOVE(gnvso, gnvso_entry);
	GENEVE_LIST_UNLOCK();

	if (destroy != 0)
		geneve_socket_destroy(gnvso);
}

static struct geneve_socket *
geneve_socket_lookup(union sockaddr_union *unsa)
{
	struct geneve_socket *gnvso;

	GENEVE_LIST_LOCK();
	LIST_FOREACH(gnvso, &geneve_socket_list, gnvso_entry) {
		if (geneve_sockaddr_cmp(&gnvso->gnvso_laddr, &unsa->sa) == 0) {
			GENEVE_SO_ACQUIRE(gnvso);
			break;
		}
	}
	GENEVE_LIST_UNLOCK();

	return (gnvso);
}

static void
geneve_socket_insert(struct geneve_socket *gnvso)
{

	GENEVE_LIST_LOCK();
	GENEVE_SO_ACQUIRE(gnvso);
	LIST_INSERT_HEAD(&geneve_socket_list, gnvso, gnvso_entry);
	GENEVE_LIST_UNLOCK();
}

static int
geneve_socket_init(struct geneve_socket *gnvso, struct ifnet *ifp)
{
	struct thread *td;
	int error;

	td = curthread;
	error = socreate(gnvso->gnvso_laddr.sa.sa_family, &gnvso->gnvso_sock,
	    SOCK_DGRAM, IPPROTO_UDP, td->td_ucred, td);
	if (error) {
		if_printf(ifp, "cannot create socket: %d\n", error);
		return (error);
	}

	/*
	 * XXX: If Geneve traffic is shared with other UDP listeners on
	 * the same IP address, tunnel endpoints SHOULD implement a mechanism
	 * to ensure ICMP return traffic arising from network errors is
	 * directed to the correct listener. Unfortunately,
	 * udp_set_kernel_tunneling does not handle icmp errors from transit
	 * devices other than specified source.
	 */
	error = udp_set_kernel_tunneling(gnvso->gnvso_sock,
	    geneve_udp_input, NULL, gnvso);
	if (error)
		if_printf(ifp, "cannot set tunneling function: %d\n", error);

	return (error);
}

static int
geneve_socket_bind(struct geneve_socket *gnvso, struct ifnet *ifp)
{
	union sockaddr_union laddr;
	int error;

	laddr = gnvso->gnvso_laddr;
	error = sobind(gnvso->gnvso_sock, &laddr.sa, curthread);
	if (error)
		return (error);

	return (0);
}

static int
geneve_socket_create(struct ifnet *ifp, int multicast,
    const union sockaddr_union *unsa, struct geneve_socket **xgnvso)
{
	union sockaddr_union laddr;
	struct geneve_socket *gnvso;
	int error;

	laddr = *unsa;

	/*
	 * If this socket will be multicast, then only the local port
	 * must be specified when binding.
	 */
	if (multicast != 0) {
		switch (laddr.sa.sa_family) {
#ifdef INET
		case AF_INET:
			laddr.sin.sin_addr.s_addr = INADDR_ANY;
			break;
#endif
#ifdef INET6
		case AF_INET6:
			laddr.sin6.sin6_addr = in6addr_any;
			break;
#endif
		default:
			return (EAFNOSUPPORT);
		}
	}
	gnvso = geneve_socket_alloc(&laddr);
	if (gnvso == NULL)
		return (ENOMEM);

	error = geneve_socket_init(gnvso, ifp);
	if (error)
		goto fail;

	error = geneve_socket_bind(gnvso, ifp);
	if (error)
		goto fail;

	/*
	 * There is a small window between the bind completing and
	 * inserting the socket, so that a concurrent create may fail.
	 * Let's not worry about that for now.
	 */
	if_printf(ifp, "new geneve socket inserted to socket list\n");
	geneve_socket_insert(gnvso);
	*xgnvso = gnvso;

	return (0);

fail:
	if_printf(ifp, "can't create new socket (error: %d)\n", error);
	geneve_socket_destroy(gnvso);

	return (error);
}

static struct geneve_socket *
geneve_socket_mc_lookup(const union sockaddr_union *unsa)
{
	union sockaddr_union laddr;

	laddr = *unsa;

	switch (laddr.sa.sa_family) {
#ifdef INET
	case AF_INET:
		laddr.sin.sin_addr.s_addr = INADDR_ANY;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		laddr.sin6.sin6_addr = in6addr_any;
		break;
#endif
	default:
		return (NULL);
	}

	return (geneve_socket_lookup(&laddr));
}

static int
geneve_sockaddr_mc_info_match(const struct geneve_socket_mc_info *mc,
    const union sockaddr_union *group, const union sockaddr_union *local,
    int ifidx)
{

	if (!geneve_sockaddr_in_any(local) &&
	    !geneve_sockaddr_in_equal(&mc->gnvsomc_saddr, &local->sa))
		return (0);
	if (!geneve_sockaddr_in_equal(&mc->gnvsomc_gaddr, &group->sa))
		return (0);
	if (ifidx != 0 && ifidx != mc->gnvsomc_ifidx)
		return (0);

	return (1);
}

static int
geneve_socket_mc_join_group(struct geneve_socket *gnvso,
    const union sockaddr_union *group, const union sockaddr_union *local,
    int *ifidx, union sockaddr_union *source)
{
	struct sockopt sopt;
	int error;

	*source = *local;

	if (group->sa.sa_family == AF_INET) {
		struct ip_mreq mreq;

		mreq.imr_multiaddr = group->sin.sin_addr;
		mreq.imr_interface = local->sin.sin_addr;

		memset(&sopt, 0, sizeof(sopt));
		sopt.sopt_dir = SOPT_SET;
		sopt.sopt_level = IPPROTO_IP;
		sopt.sopt_name = IP_ADD_MEMBERSHIP;
		sopt.sopt_val = &mreq;
		sopt.sopt_valsize = sizeof(mreq);
		error = sosetopt(gnvso->gnvso_sock, &sopt);
		if (error)
			return (error);

		/*
		 * BMV: Ideally, there would be a formal way for us to get
		 * the local interface that was selected based on the
		 * imr_interface address. We could then update *ifidx so
		 * geneve_sockaddr_mc_info_match() would return a match for
		 * later creates that explicitly set the multicast interface.
		 *
		 * If we really need to, we can of course look in the INP's
		 * membership list:
		 *     sotoinpcb(gnvso->gnvso_sock)->inp_moptions->
		 *         imo_head[]->imf_inm->inm_ifp
		 * similarly to imo_match_group().
		 */
		source->sin.sin_addr = local->sin.sin_addr;

	} else if (group->sa.sa_family == AF_INET6) {
		struct ipv6_mreq mreq;

		mreq.ipv6mr_multiaddr = group->sin6.sin6_addr;
		mreq.ipv6mr_interface = *ifidx;

		memset(&sopt, 0, sizeof(sopt));
		sopt.sopt_dir = SOPT_SET;
		sopt.sopt_level = IPPROTO_IPV6;
		sopt.sopt_name = IPV6_JOIN_GROUP;
		sopt.sopt_val = &mreq;
		sopt.sopt_valsize = sizeof(mreq);
		error = sosetopt(gnvso->gnvso_sock, &sopt);

		/*
		 * BMV: As with IPv4, we would really like to know what
		 * interface in6p_lookup_mcast_ifp() selected.
		 */
	} else
		error = EAFNOSUPPORT;

	return (error);
}

static int
geneve_socket_mc_leave_group(struct geneve_socket *gnvso,
    const union sockaddr_union *group, const union sockaddr_union *source,
    int ifidx)
{
	struct sockopt sopt;
	int error;

	memset(&sopt, 0, sizeof(sopt));
	sopt.sopt_dir = SOPT_SET;

	if (group->sa.sa_family == AF_INET) {
		struct ip_mreq mreq;

		mreq.imr_multiaddr = group->sin.sin_addr;
		mreq.imr_interface = source->sin.sin_addr;

		sopt.sopt_level = IPPROTO_IP;
		sopt.sopt_name = IP_DROP_MEMBERSHIP;
		sopt.sopt_val = &mreq;
		sopt.sopt_valsize = sizeof(mreq);
		error = sosetopt(gnvso->gnvso_sock, &sopt);
	} else if (group->sa.sa_family == AF_INET6) {
		struct ipv6_mreq mreq;

		mreq.ipv6mr_multiaddr = group->sin6.sin6_addr;
		mreq.ipv6mr_interface = ifidx;

		sopt.sopt_level = IPPROTO_IPV6;
		sopt.sopt_name = IPV6_LEAVE_GROUP;
		sopt.sopt_val = &mreq;
		sopt.sopt_valsize = sizeof(mreq);
		error = sosetopt(gnvso->gnvso_sock, &sopt);
	} else
		error = EAFNOSUPPORT;

	return (error);
}

static int
geneve_socket_mc_add_group(struct geneve_socket *gnvso,
    const union sockaddr_union *group, const union sockaddr_union *local,
    int ifidx, int *idx)
{
	union sockaddr_union source;
	struct geneve_socket_mc_info *mc;
	int i, empty, error;

	/*
	 * Within a socket, the same multicast group may be used by multiple
	 * interfaces, each with a different network identifier. But a socket
	 * may only join a multicast group once, so keep track of the users
	 * here.
	 */

	GENEVE_SO_WLOCK(gnvso);
	for (empty = 0, i = 0; i < GENEVE_SO_MC_MAX_GROUPS; i++) {
		mc = &gnvso->gnvso_mc[i];

		if (mc->gnvsomc_gaddr.sa.sa_family == AF_UNSPEC) {
			empty++;
			continue;
		}
		if (geneve_sockaddr_mc_info_match(mc, group, local, ifidx))
			goto out;
	}
	GENEVE_SO_WUNLOCK(gnvso);

	if (empty == 0)
		return (ENOSPC);

	error = geneve_socket_mc_join_group(gnvso, group, local, &ifidx, &source);
	if (error)
		return (error);

	GENEVE_SO_WLOCK(gnvso);
	for (i = 0; i < GENEVE_SO_MC_MAX_GROUPS; i++) {
		mc = &gnvso->gnvso_mc[i];

		if (mc->gnvsomc_gaddr.sa.sa_family == AF_UNSPEC) {
			geneve_sockaddr_copy(&mc->gnvsomc_gaddr, &group->sa);
			geneve_sockaddr_copy(&mc->gnvsomc_saddr, &source.sa);
			mc->gnvsomc_ifidx = ifidx;
			goto out;
		}
	}
	GENEVE_SO_WUNLOCK(gnvso);

	error = geneve_socket_mc_leave_group(gnvso, group, &source, ifidx);
	MPASS(error == 0);

	return (ENOSPC);

out:
	mc->gnvsomc_users++;
	GENEVE_SO_WUNLOCK(gnvso);
	*idx = i;

	return (0);
}

static void
geneve_socket_mc_release_group(struct geneve_socket *vso, int idx)
{
	union sockaddr_union group, source;
	struct geneve_socket_mc_info *mc;
	int ifidx, leave;

	KASSERT(idx >= 0 && idx < GENEVE_SO_MC_MAX_GROUPS,
	    ("%s: vso %p idx %d out of bounds", __func__, vso, idx));

	leave = 0;
	mc = &vso->gnvso_mc[idx];

	GENEVE_SO_WLOCK(vso);
	mc->gnvsomc_users--;
	if (mc->gnvsomc_users == 0) {
		group = mc->gnvsomc_gaddr;
		source = mc->gnvsomc_saddr;
		ifidx = mc->gnvsomc_ifidx;
		memset(mc, 0, sizeof(*mc));
		leave = 1;
	}
	GENEVE_SO_WUNLOCK(vso);

	if (leave != 0) {
		/*
		 * Our socket's membership in this group may have already
		 * been removed if we joined through an interface that's
		 * been detached.
		 */
		geneve_socket_mc_leave_group(vso, &group, &source, ifidx);
	}
}

static struct geneve_softc *
geneve_socket_lookup_softc_locked(struct geneve_socket *gnvso, uint32_t vni)
{
	struct geneve_softc *sc;
	uint32_t hash;

	GENEVE_SO_LOCK_ASSERT(gnvso);
	hash = GENEVE_SO_VNI_HASH(vni);

	LIST_FOREACH(sc, &gnvso->gnvso_vni_hash[hash], gnv_entry) {
		if (sc->gnv_vni == vni) {
			GENEVE_ACQUIRE(sc);
			break;
		}
	}

	return (sc);
}

static struct geneve_softc *
geneve_socket_lookup_softc(struct geneve_socket *gnvso, uint32_t vni)
{
	struct rm_priotracker tracker;
	struct geneve_softc *sc;

	GENEVE_SO_RLOCK(gnvso, &tracker);
	sc = geneve_socket_lookup_softc_locked(gnvso, vni);
	GENEVE_SO_RUNLOCK(gnvso, &tracker);

	return (sc);
}

static int
geneve_socket_insert_softc(struct geneve_socket *gnvso, struct geneve_softc *sc)
{
	struct geneve_softc *tsc;
	uint32_t vni, hash;

	vni = sc->gnv_vni;
	hash = GENEVE_SO_VNI_HASH(vni);

	GENEVE_SO_WLOCK(gnvso);
	tsc = geneve_socket_lookup_softc_locked(gnvso, vni);
	if (tsc != NULL) {
		GENEVE_SO_WUNLOCK(gnvso);
		geneve_release(tsc);
		return (EEXIST);
	}

	GENEVE_ACQUIRE(sc);
	LIST_INSERT_HEAD(&gnvso->gnvso_vni_hash[hash], sc, gnv_entry);
	GENEVE_SO_WUNLOCK(gnvso);

	return (0);
}

static void
geneve_socket_remove_softc(struct geneve_socket *gnvso, struct geneve_softc *sc)
{

	GENEVE_SO_WLOCK(gnvso);
	LIST_REMOVE(sc, gnv_entry);
	GENEVE_SO_WUNLOCK(gnvso);

	geneve_release(sc);
}

static struct ifnet *
geneve_multicast_if_ref(struct geneve_softc *sc, uint32_t af)
{
	struct ifnet *ifp;

	GENEVE_LOCK_ASSERT(sc);

	ifp = NULL;
	if (af == AF_INET && sc->gnv_im4o != NULL)
		ifp = sc->gnv_im4o->imo_multicast_ifp;
	else if (af == AF_INET6 && sc->gnv_im6o != NULL)
		ifp = sc->gnv_im6o->im6o_multicast_ifp;

	if (ifp != NULL)
		if_ref(ifp);

	return (ifp);
}

static void
geneve_free_multicast(struct geneve_softc *sc)
{

	if (sc->gnv_mc_ifp != NULL) {
		if_rele(sc->gnv_mc_ifp);
		sc->gnv_mc_ifp = NULL;
		sc->gnv_mc_ifindex = 0;
	}

	if (sc->gnv_im4o != NULL) {
		free(sc->gnv_im4o, M_GENEVE);
		sc->gnv_im4o = NULL;
	}

	if (sc->gnv_im6o != NULL) {
		free(sc->gnv_im6o, M_GENEVE);
		sc->gnv_im6o = NULL;
	}
}

static int
geneve_setup_multicast_interface(struct geneve_softc *sc)
{
	struct ifnet *ifp;

	ifp = ifunit_ref(sc->gnv_mc_ifname);
	if (ifp == NULL) {
		if_printf(sc->gnv_ifp, "multicast interface %s does not exist\n",
		    sc->gnv_mc_ifname);
		return (ENOENT);
	}

	if ((ifp->if_flags & IFF_MULTICAST) == 0) {
		if_printf(sc->gnv_ifp, "interface %s does not support multicast\n",
		    sc->gnv_mc_ifname);
		if_rele(ifp);
		return (ENOTSUP);
	}

	sc->gnv_mc_ifp = ifp;
	sc->gnv_mc_ifindex = ifp->if_index;

	return (0);
}

static int
geneve_setup_multicast(struct geneve_softc *sc)
{
	const union sockaddr_union *group;
	int error;

	group = &sc->gnv_dst_addr;
	error = 0;

	if (sc->gnv_mc_ifname[0] != '\0') {
		error = geneve_setup_multicast_interface(sc);
		if (error)
			return (error);
	}

	/*
	 * Initialize an multicast options structure that is sufficiently
	 * populated for use in the respective IP output routine. This
	 * structure is typically stored in the socket, but our sockets
	 * may be shared among multiple interfaces.
	 */
	if (group->sa.sa_family == AF_INET) {
		sc->gnv_im4o = malloc(sizeof(struct ip_moptions), M_GENEVE,
		    M_ZERO | M_WAITOK);
		sc->gnv_im4o->imo_multicast_ifp = sc->gnv_mc_ifp;
		sc->gnv_im4o->imo_multicast_ttl = sc->gnv_ttl;
		sc->gnv_im4o->imo_multicast_vif = -1;
	} else if (group->sa.sa_family == AF_INET6) {
		sc->gnv_im6o = malloc(sizeof(struct ip6_moptions), M_GENEVE,
		    M_ZERO | M_WAITOK);
		sc->gnv_im6o->im6o_multicast_ifp = sc->gnv_mc_ifp;
		sc->gnv_im6o->im6o_multicast_hlim = sc->gnv_ttl;
	}

	return (error);
}

static int
geneve_setup_socket(struct geneve_softc *sc)
{
	struct geneve_socket *gnvso;
	struct ifnet *ifp;
	union sockaddr_union *saddr, *daddr;
	int multicast, error;

	gnvso = NULL;
	ifp = sc->gnv_ifp;
	saddr = &sc->gnv_src_addr;
	daddr = &sc->gnv_dst_addr;
	multicast = geneve_check_multicast_addr(daddr);
	MPASS(multicast != EINVAL);
	sc->gnv_so_mc_index = -1;

	/* Try to create the socket. If that fails, attempt to use an existing one. */
	error = geneve_socket_create(ifp, multicast, saddr, &gnvso);
	if (error) {
		if (multicast != 0)
			gnvso = geneve_socket_mc_lookup(saddr);
		else
			gnvso = geneve_socket_lookup(saddr);

		if (gnvso == NULL) {
			if_printf(ifp, "can't find existing socket\n");
			goto out;
		}
	}

	if (sc->gnv_df == IFLA_GENEVE_DF_SET) {
		error = geneve_socket_set_df(gnvso, true);
		if (error)
			goto out;
	}

	if (multicast != 0) {
		error = geneve_setup_multicast(sc);
		if (error)
			goto out;

		error = geneve_socket_mc_add_group(gnvso, daddr, saddr,
		    sc->gnv_mc_ifindex, &sc->gnv_so_mc_index);
		if (error)
			goto out;
	}

	sc->gnv_sock = gnvso;
	error = geneve_socket_insert_softc(gnvso, sc);
	if (error) {
		sc->gnv_sock = NULL;
		if_printf(ifp, "network identifier %d already exists\n", sc->gnv_vni);
		goto out;
	}

	return (0);

out:
	if (gnvso != NULL) {
		if (sc->gnv_so_mc_index != -1) {
			geneve_socket_mc_release_group(gnvso, sc->gnv_so_mc_index);
			sc->gnv_so_mc_index = -1;
		}
		if (multicast != 0)
			geneve_free_multicast(sc);
		geneve_socket_release(gnvso);
	}

	return (error);
}

static void
geneve_setup_interface_hdrlen(struct geneve_softc *sc)
{
	struct ifnet *ifp;

	GENEVE_LOCK_WASSERT(sc);

	ifp = sc->gnv_ifp;
	ifp->if_hdrlen = ETHER_HDR_LEN + sizeof(struct geneveudphdr);
	if (sc->gnv_proto == GENEVE_PROTO_ETHER)
		ifp->if_hdrlen += ETHER_HDR_LEN;

	if (sc->gnv_dst_addr.sa.sa_family == AF_INET)
		ifp->if_hdrlen += sizeof(struct ip);
	else
		ifp->if_hdrlen += sizeof(struct ip6_hdr);

	if ((sc->gnv_flags & GENEVE_FLAG_USER_MTU) == 0)
		ifp->if_mtu = ETHERMTU - ifp->if_hdrlen;
}

static int
geneve_socket_set_df(struct geneve_socket *gnvso, bool df)
{
	struct sockopt sopt;
	int optval;

	memset(&sopt, 0, sizeof(sopt));
	sopt.sopt_dir = SOPT_SET;

	switch (gnvso->gnvso_laddr.sa.sa_family) {
	case AF_INET:
		sopt.sopt_level = IPPROTO_IP;
		sopt.sopt_name = IP_DONTFRAG;
		break;

	case AF_INET6:
		sopt.sopt_level = IPPROTO_IPV6;
		sopt.sopt_name = IPV6_DONTFRAG;
		break;

	default:
		return (EAFNOSUPPORT);
	}

	optval = df ? 1 : 0;
	sopt.sopt_val = &optval;
	sopt.sopt_valsize = sizeof(optval);

	return (sosetopt(gnvso->gnvso_sock, &sopt));
}

static int
geneve_valid_init_config(struct geneve_softc *sc)
{
	const char *reason;

	if (sc->gnv_vni >= GENEVE_VNI_MAX) {
		if_printf(sc->gnv_ifp, "%u", sc->gnv_vni);
		reason = "invalid virtual network identifier specified";
		goto fail;
	}

	if (geneve_sockaddr_supported(&sc->gnv_src_addr, 1) == 0) {
		reason = "source address type is not supported";
		goto fail;
	}

	if (geneve_sockaddr_supported(&sc->gnv_dst_addr, 0) == 0) {
		reason = "destination address type is not supported";
		goto fail;
	}

	if (geneve_sockaddr_in_any(&sc->gnv_dst_addr) != 0) {
		reason = "no valid destination address specified";
		goto fail;
	}

	if (geneve_check_multicast_addr(&sc->gnv_dst_addr) == 0 &&
	    sc->gnv_mc_ifname[0] != '\0') {
		reason = "can only specify interface with a group address";
		goto fail;
	}

	if (geneve_sockaddr_in_any(&sc->gnv_src_addr) == 0) {
		if (&sc->gnv_src_addr.sa.sa_family ==
		    &sc->gnv_dst_addr.sa.sa_family) {
			reason = "source and destination address must both be either IPv4 or IPv6";
			goto fail;
		}
	}

	if (sc->gnv_src_addr.sin.sin_port == 0) {
		reason = "local port not specified";
		goto fail;
	}

	if (sc->gnv_dst_addr.sin.sin_port == 0) {
		reason = "remote port not specified";
		goto fail;
	}

	return (0);

fail:
	if_printf(sc->gnv_ifp, "cannot initialize interface: %s\n", reason);
	return (EINVAL);
}

static void
geneve_init_complete(struct geneve_softc *sc)
{

	GENEVE_WLOCK(sc);
	sc->gnv_flags |= GENEVE_FLAG_RUNNING;
	sc->gnv_flags &= ~GENEVE_FLAG_INIT;
	wakeup(sc);
	GENEVE_WUNLOCK(sc);
}

static void
geneve_init(void *xsc)
{
	static const uint8_t empty_mac[ETHER_ADDR_LEN];
	struct geneve_softc *sc;
	struct ifnet *ifp;

	sc = xsc;
	sx_xlock(&geneve_sx);
	GENEVE_WLOCK(sc);
	ifp = sc->gnv_ifp;
	if (sc->gnv_flags & GENEVE_FLAG_RUNNING) {
		GENEVE_WUNLOCK(sc);
		sx_xunlock(&geneve_sx);
		return;
	}
	sc->gnv_flags |= GENEVE_FLAG_INIT;
	GENEVE_WUNLOCK(sc);

	if (geneve_valid_init_config(sc) != 0)
		goto out;

	if (geneve_setup_socket(sc) != 0)
		goto out;

	/* Initialize the default forwarding entry. */
	if (sc->gnv_proto == GENEVE_PROTO_ETHER) {
		geneve_ftable_entry_init(sc, &sc->gnv_default_fe, empty_mac,
		    &sc->gnv_dst_addr.sa, GENEVE_FE_FLAG_STATIC);

		GENEVE_WLOCK(sc);
		callout_reset(&sc->gnv_callout, geneve_ftable_prune_period * hz,
		    geneve_timer, sc);
		GENEVE_WUNLOCK(sc);
	}
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	if_link_state_change(ifp, LINK_STATE_UP);

out:
	geneve_init_complete(sc);
	sx_xunlock(&geneve_sx);
}

static void
geneve_release(struct geneve_softc *sc)
{

	/*
	 * The softc may be destroyed as soon as we release our reference,
	 * so we cannot serialize the wakeup with the softc lock. We use a
	 * timeout in our sleeps so a missed wakeup is unfortunate but not fatal.
	 */
	if (GENEVE_RELEASE(sc) != 0)
		wakeup(sc);
}

static void
geneve_teardown_wait(struct geneve_softc *sc)
{

	GENEVE_LOCK_WASSERT(sc);
	while (sc->gnv_flags & GENEVE_FLAG_TEARDOWN)
		rm_sleep(sc, &sc->gnv_lock, 0, "gnvtrn", hz);
}

static void
geneve_teardown_locked(struct geneve_softc *sc)
{
	struct ifnet *ifp;
	struct geneve_socket *gnvso;

	sx_assert(&geneve_sx, SA_XLOCKED);
	GENEVE_LOCK_WASSERT(sc);
	MPASS(sc->gnv_flags & GENEVE_FLAG_TEARDOWN);

	ifp = sc->gnv_ifp;
	ifp->if_flags &= ~IFF_UP;
	sc->gnv_flags &= ~GENEVE_FLAG_RUNNING;

	if (sc->gnv_proto == GENEVE_PROTO_ETHER)
		callout_stop(&sc->gnv_callout);
	gnvso = sc->gnv_sock;
	sc->gnv_sock = NULL;

	GENEVE_WUNLOCK(sc);
	if_link_state_change(ifp, LINK_STATE_DOWN);

	if (gnvso != NULL) {
		geneve_socket_remove_softc(gnvso, sc);

		if (sc->gnv_so_mc_index != -1) {
			geneve_socket_mc_release_group(gnvso, sc->gnv_so_mc_index);
			sc->gnv_so_mc_index = -1;
		}
	}

	GENEVE_WLOCK(sc);
	while (sc->gnv_refcnt != 0)
		rm_sleep(sc, &sc->gnv_lock, 0, "gnvdrn", hz);
	GENEVE_WUNLOCK(sc);

	if (sc->gnv_proto == GENEVE_PROTO_ETHER)
		callout_drain(&sc->gnv_callout);

	geneve_free_multicast(sc);
	if (gnvso != NULL)
		geneve_socket_release(gnvso);

	GENEVE_WLOCK(sc);
	sc->gnv_flags &= ~GENEVE_FLAG_TEARDOWN;
	wakeup(sc);
	GENEVE_WUNLOCK(sc);
}

static void
geneve_teardown(struct geneve_softc *sc)
{

	sx_xlock(&geneve_sx);
	GENEVE_WLOCK(sc);
	if (sc->gnv_flags & GENEVE_FLAG_TEARDOWN) {
		geneve_teardown_wait(sc);
		GENEVE_WUNLOCK(sc);
		sx_xunlock(&geneve_sx);
		return;
	}

	sc->gnv_flags |= GENEVE_FLAG_TEARDOWN;
	geneve_teardown_locked(sc);
	sx_xunlock(&geneve_sx);
}

static void
geneve_timer(void *xsc)
{
	struct geneve_softc *sc;

	sc = xsc;
	GENEVE_LOCK_WASSERT(sc);

	geneve_ftable_expire(sc);
	callout_schedule(&sc->gnv_callout, geneve_ftable_prune_period * hz);
}

static int
geneve_ioctl_ifflags(struct geneve_softc *sc)
{
	struct ifnet *ifp;

	ifp = sc->gnv_ifp;

	if ((ifp->if_flags & IFF_UP) != 0) {
		if ((sc->gnv_flags & GENEVE_FLAG_RUNNING) == 0)
			geneve_init(sc);
	} else {
		if (sc->gnv_flags & GENEVE_FLAG_RUNNING)
			geneve_teardown(sc);
	}

	return (0);
}

static int
geneve_flush_ftable(struct geneve_softc *sc, bool flush)
{

	GENEVE_WLOCK(sc);
	geneve_ftable_flush(sc, flush);
	GENEVE_WUNLOCK(sc);

	return (0);
}

static uint16_t
geneve_get_local_port(struct geneve_softc *sc)
{
	uint16_t port = 0;

	GENEVE_LOCK_ASSERT(sc);

	switch (sc->gnv_src_addr.sa.sa_family) {
	case AF_INET:
		port = ntohs(sc->gnv_src_addr.sin.sin_port);
		break;
	case AF_INET6:
		port = ntohs(sc->gnv_src_addr.sin6.sin6_port);
		break;
	}

	return (port);
}

static uint16_t
geneve_get_remote_port(struct geneve_softc *sc)
{
	uint16_t port = 0;

	GENEVE_LOCK_ASSERT(sc);

	switch (sc->gnv_dst_addr.sa.sa_family) {
	case AF_INET:
		port = ntohs(sc->gnv_dst_addr.sin.sin_port);
		break;
	case AF_INET6:
		port = ntohs(sc->gnv_dst_addr.sin6.sin6_port);
		break;
	}

	return (port);
}

/* Netlink Helpers */
static int
geneve_set_vni_nl(struct geneve_softc *sc, struct nl_pstate *npt, uint32_t vni)
{
	int error;

	error = 0;
	if (vni >= GENEVE_VNI_MAX) {
		error = EINVAL;
		goto ret;
	}

	GENEVE_WLOCK(sc);
	if (geneve_can_change_config(sc))
		sc->gnv_vni = vni;
	else
		error = EBUSY;
	GENEVE_WUNLOCK(sc);

ret:
	if (error == EINVAL)
		nlmsg_report_err_msg(npt, "geneve vni is invalid: %u", vni);

	if (error == EBUSY)
		nlmsg_report_err_msg(npt, "geneve interface is busy.");

	return (error);
}

static int
geneve_set_local_addr_nl(struct geneve_softc *sc, struct nl_pstate *npt,
    struct sockaddr *sa)
{
	union sockaddr_union *unsa = (union sockaddr_union *)sa;
	int error;

	error = geneve_check_sockaddr(unsa, sa->sa_len);
	if (error != 0)
		goto ret;

	error = geneve_check_multicast_addr(unsa);
	if (error != 0)
		goto ret;

#ifdef INET6
	if (unsa->sa.sa_family == AF_INET6) {
		error = sa6_embedscope(&unsa->sin6, V_ip6_use_defzone);
		if (error != 0)
			goto ret;
	}
#endif

	GENEVE_WLOCK(sc);
	if (geneve_can_change_config(sc)) {
		geneve_sockaddr_in_copy(&sc->gnv_src_addr, &unsa->sa);
		geneve_set_hwcaps(sc);
	} else
		error = EBUSY;
	GENEVE_WUNLOCK(sc);

ret:
	if (error == EINVAL)
		nlmsg_report_err_msg(npt, "local address is invalid.");

	if (error == EAFNOSUPPORT)
		nlmsg_report_err_msg(npt, "address family is not supported.");

	if (error == EBUSY)
		nlmsg_report_err_msg(npt, "geneve interface is busy.");

	return (error);
}

static int
geneve_set_remote_addr_nl(struct geneve_softc *sc, struct nl_pstate *npt,
    struct sockaddr *sa)
{
	union sockaddr_union *unsa = (union sockaddr_union *)sa;
	int error;

	error = geneve_check_sockaddr(unsa, sa->sa_len);
	if (error != 0)
		goto ret;

#ifdef INET6
	if (unsa->sa.sa_family == AF_INET6) {
		error = sa6_embedscope(&unsa->sin6, V_ip6_use_defzone);
		if (error != 0)
			goto ret;
	}
#endif

	GENEVE_WLOCK(sc);
	if (geneve_can_change_config(sc)) {
		geneve_sockaddr_in_copy(&sc->gnv_dst_addr, &unsa->sa);
		geneve_setup_interface_hdrlen(sc);
	} else
		error = EBUSY;
	GENEVE_WUNLOCK(sc);

ret:
	if (error == EINVAL)
		nlmsg_report_err_msg(npt, "remote address is invalid.");

	if (error == EAFNOSUPPORT)
		nlmsg_report_err_msg(npt, "address family is not supported.");

	if (error == EBUSY)
		nlmsg_report_err_msg(npt, "geneve interface is busy.");

	return (error);
}

static int
geneve_set_local_port_nl(struct geneve_softc *sc, struct nl_pstate *npt, uint16_t port)
{
	int error;

	error = 0;
	if (port == 0 || port > UINT16_MAX) {
		error = EINVAL;
		goto ret;
	}

	GENEVE_WLOCK(sc);
	if (geneve_can_change_config(sc) == 0) {
		GENEVE_WUNLOCK(sc);
		error = EBUSY;
		goto ret;
	}

	switch (sc->gnv_src_addr.sa.sa_family) {
	case AF_INET:
		sc->gnv_src_addr.sin.sin_port = htons(port);
		break;
	case AF_INET6:
		sc->gnv_src_addr.sin6.sin6_port = htons(port);
		break;
	}
	GENEVE_WUNLOCK(sc);

ret:
	if (error == EINVAL)
		nlmsg_report_err_msg(npt, "local port is invalid: %u", port);

	if (error == EBUSY)
		nlmsg_report_err_msg(npt, "geneve interface is busy.");

	return (error);
}

static int
geneve_set_remote_port_nl(struct geneve_softc *sc, struct nl_pstate *npt, uint16_t port)
{
	int error;

	error = 0;
	if (port == 0 || port > UINT16_MAX) {
		error = EINVAL;
		goto ret;
	}

	GENEVE_WLOCK(sc);
	if (geneve_can_change_config(sc) == 0) {
		GENEVE_WUNLOCK(sc);
		error = EBUSY;
		goto ret;
	}

	switch (sc->gnv_dst_addr.sa.sa_family) {
	case AF_INET:
		sc->gnv_dst_addr.sin.sin_port = htons(port);
		break;
	case AF_INET6:
		sc->gnv_dst_addr.sin6.sin6_port = htons(port);
		break;
	}
	GENEVE_WUNLOCK(sc);

ret:
	if (error == EINVAL)
		nlmsg_report_err_msg(npt, "remote port is invalid: %u", port);

	if (error == EBUSY)
		nlmsg_report_err_msg(npt, "geneve interface is busy.");

	return (error);
}

static int
geneve_set_port_range_nl(struct geneve_softc *sc, struct nl_pstate *npt,
    struct ifla_geneve_port_range port_range)
{
	int error;

	error = 0;
	if (port_range.low <= 0 || port_range.high > UINT16_MAX ||
	    port_range.high < port_range.low) {
		error = EINVAL;
		goto ret;
	}

	GENEVE_WLOCK(sc);
	if (geneve_can_change_config(sc)) {
		sc->gnv_min_port = port_range.low;
		sc->gnv_max_port = port_range.high;
	} else
		error = EBUSY;
	GENEVE_WUNLOCK(sc);

ret:
	if (error == EINVAL)
		nlmsg_report_err_msg(npt, "port range is invalid: %u-%u",
		    port_range.low, port_range.high);

	if (error == EBUSY)
		nlmsg_report_err_msg(npt, "geneve interface is busy.");

	return (error);
}

static int
geneve_set_df_nl(struct geneve_softc *sc, struct nl_pstate *npt,
    enum ifla_geneve_df df)
{
	int error;

	error = 0;
	GENEVE_WLOCK(sc);
	if (geneve_can_change_config(sc))
		sc->gnv_df = df;
	else
		error = EBUSY;
	GENEVE_WUNLOCK(sc);

	if (error == EBUSY)
		nlmsg_report_err_msg(npt, "geneve interface is busy.");

	return (error);
}

static int
geneve_set_ttl_nl(struct geneve_softc *sc, struct nl_pstate *npt __unused,
    uint8_t ttl)
{

	GENEVE_WLOCK(sc);
	sc->gnv_ttl = ttl;
	if (sc->gnv_im4o != NULL)
		sc->gnv_im4o->imo_multicast_ttl = sc->gnv_ttl;
	if (sc->gnv_im6o != NULL)
		sc->gnv_im6o->im6o_multicast_hlim = sc->gnv_ttl;
	GENEVE_WUNLOCK(sc);

	return (0);
}

static int
geneve_set_ttl_inherit_nl(struct geneve_softc *sc,
    struct nl_pstate *npt __unused, bool inherit)
{

	GENEVE_WLOCK(sc);
	if (inherit)
		sc->gnv_flags |= GENEVE_FLAG_TTL_INHERIT;
	else
		sc->gnv_flags &= ~GENEVE_FLAG_TTL_INHERIT;
	GENEVE_WUNLOCK(sc);

	return (0);
}

static int
geneve_set_dscp_inherit_nl(struct geneve_softc *sc,
    struct nl_pstate *npt __unused, bool inherit)
{

	GENEVE_WLOCK(sc);
	if (inherit)
		sc->gnv_flags |= GENEVE_FLAG_DSCP_INHERIT;
	else
		sc->gnv_flags &= ~GENEVE_FLAG_DSCP_INHERIT;
	GENEVE_WUNLOCK(sc);

	return (0);
}

static int
geneve_set_collect_metadata_nl(struct geneve_softc *sc,
    struct nl_pstate *npt __unused, bool external)
{

	GENEVE_WLOCK(sc);
	if (external)
		sc->gnv_flags |= GENEVE_FLAG_COLLECT_METADATA;
	else
		sc->gnv_flags &= ~GENEVE_FLAG_COLLECT_METADATA;
	GENEVE_WUNLOCK(sc);

	return (0);
}

static int
geneve_set_learn_nl(struct geneve_softc *sc, struct nl_pstate *npt,
    bool learn)
{

	GENEVE_WLOCK(sc);
	if (learn)
		sc->gnv_flags |= GENEVE_FLAG_LEARN;
	else
		sc->gnv_flags &= ~GENEVE_FLAG_LEARN;
	GENEVE_WUNLOCK(sc);

	return (0);
}

static int
geneve_set_ftable_max_nl(struct geneve_softc *sc, struct nl_pstate *npt,
    uint32_t max)
{
	int error;

	error = 0;
	GENEVE_WLOCK(sc);
	if (max <= GENEVE_FTABLE_MAX)
		sc->gnv_ftable_max = max;
	else
		error = EINVAL;
	GENEVE_WUNLOCK(sc);

	if (error == EINVAL)
		nlmsg_report_err_msg(npt,
		    "maximum number of entries in the table can not be more than %u",
		    GENEVE_FTABLE_MAX);

	return (error);
}

static int
geneve_set_ftable_timeout_nl(struct geneve_softc *sc, struct nl_pstate *npt,
    uint32_t timeout)
{
	int error;

	error = 0;
	GENEVE_WLOCK(sc);
	if (timeout <= GENEVE_FTABLE_MAX_TIMEOUT)
		sc->gnv_ftable_timeout = timeout;
	else
		error = EINVAL;
	GENEVE_WUNLOCK(sc);

	if (error == EINVAL)
		nlmsg_report_err_msg(npt,
		    "maximum timeout for stale entries in the table can not be more than %u",
		    GENEVE_FTABLE_MAX_TIMEOUT);

	return (error);
}

static int
geneve_set_mc_if_nl(struct geneve_softc *sc, struct nl_pstate *npt,
    char *ifname)
{
	int error;

	error = 0;
	GENEVE_WLOCK(sc);
	if (geneve_can_change_config(sc)) {
		strlcpy(sc->gnv_mc_ifname, ifname, IFNAMSIZ);
		geneve_set_hwcaps(sc);
	} else
		error = EBUSY;
	GENEVE_WUNLOCK(sc);

	if (error == EBUSY)
		nlmsg_report_err_msg(npt, "geneve interface is busy.");

	return (error);
}

static int
geneve_flush_ftable_nl(struct geneve_softc *sc, struct nl_pstate *npt,
    bool flush)
{

	return (geneve_flush_ftable(sc, flush));
}

static void
geneve_get_local_addr_nl(struct geneve_softc *sc, struct nl_writer *nw)
{
	struct sockaddr *sa;

	GENEVE_LOCK_ASSERT(sc);

	sa = &sc->gnv_src_addr.sa;
	if (sa->sa_family == AF_INET) {
		const struct in_addr *in4 = &SATOCONSTSIN(sa)->sin_addr;
		nlattr_add_in_addr(nw, IFLA_GENEVE_LOCAL, in4);
	} else if (sa->sa_family == AF_INET6) {
		const struct in6_addr *in6 = &SATOCONSTSIN6(sa)->sin6_addr;
		nlattr_add_in6_addr(nw, IFLA_GENEVE_LOCAL, in6);
	}
}

static void
geneve_get_remote_addr_nl(struct geneve_softc *sc, struct nl_writer *nw)
{
	struct sockaddr *sa;

	GENEVE_LOCK_ASSERT(sc);

	sa = &sc->gnv_dst_addr.sa;
	if (sa->sa_family == AF_INET) {
		const struct in_addr *in4 = &SATOCONSTSIN(sa)->sin_addr;
		nlattr_add_in_addr(nw, IFLA_GENEVE_REMOTE, in4);
	} else if (sa->sa_family == AF_INET6) {
		const struct in6_addr *in6 = &SATOCONSTSIN6(sa)->sin6_addr;
		nlattr_add_in6_addr(nw, IFLA_GENEVE_REMOTE, in6);
	}
}

static int
geneve_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct rm_priotracker tracker;
	struct geneve_softc *sc;
	struct siocsifcapnv_driver_data *drv_ioctl_data, drv_ioctl_data_d;
	struct ifreq *ifr;
	int max, error;

	CURVNET_ASSERT_SET();

	error = 0;
	sc = ifp->if_softc;
	ifr = (struct ifreq *)data;

	switch (cmd) {
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;

	case SIOCGDRVSPEC:
		break;
	case SIOCSDRVSPEC:
		error = priv_check(curthread, PRIV_NET_GENEVE);
		if (error)
			return (error);
		break;
	}

	switch (cmd) {
	case SIOCSIFFLAGS:
		error = geneve_ioctl_ifflags(sc);
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		if (sc->gnv_proto == GENEVE_PROTO_ETHER)
			error = ifmedia_ioctl(ifp, ifr, &sc->gnv_media, cmd);
		else
			error = EINVAL;
		break;

	case SIOCSIFMTU:
		if (sc->gnv_proto == GENEVE_PROTO_ETHER)
			max = GENEVE_MAX_MTU;
		else
			max = GENEVE_MAX_L3MTU;

		if (ifr->ifr_mtu < ETHERMIN || ifr->ifr_mtu > max)
			error = EINVAL;
		else {
			GENEVE_WLOCK(sc);
			ifp->if_mtu = ifr->ifr_mtu;
			sc->gnv_flags |= GENEVE_FLAG_USER_MTU;
			GENEVE_WUNLOCK(sc);
		}
		break;

	case SIOCGIFCAPNV:
		break;
	case SIOCSIFCAP:
		drv_ioctl_data = &drv_ioctl_data_d;
		drv_ioctl_data->reqcap = ifr->ifr_reqcap;
		drv_ioctl_data->reqcap2 = if_getcapenable2(ifp);
		drv_ioctl_data->nvcap = NULL;
		/* FALLTHROUGH */
	case SIOCSIFCAPNV:
		if (cmd == SIOCSIFCAPNV)
			drv_ioctl_data = (struct siocsifcapnv_driver_data *)data;

		GENEVE_WLOCK(sc);
		error = geneve_set_reqcap(sc, ifp, drv_ioctl_data->reqcap,
		    drv_ioctl_data->reqcap2);
		if (error == 0)
			geneve_set_hwcaps(sc);
		GENEVE_WUNLOCK(sc);
		break;

	case SIOCGTUNFIB:
		GENEVE_RLOCK(sc, &tracker);
		ifr->ifr_fib = sc->gnv_fibnum;
		GENEVE_RUNLOCK(sc, &tracker);
		break;

	case SIOCSTUNFIB:
		if ((error = priv_check(curthread, PRIV_NET_GENEVE)) != 0)
			break;

		if (ifr->ifr_fib >= rt_numfibs)
			error = EINVAL;
		else {
			GENEVE_WLOCK(sc);
			sc->gnv_fibnum = ifr->ifr_fib;
			GENEVE_WUNLOCK(sc);
		}
		break;

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* FALLTHROUGH */
	case SIOCGIFADDR:
		if (sc->gnv_proto == GENEVE_PROTO_ETHER)
			error = ether_ioctl(ifp, cmd, data);
		break;

	default:
		if (sc->gnv_proto == GENEVE_PROTO_ETHER)
			error = ether_ioctl(ifp, cmd, data);
		else
			error = EINVAL;
		break;
	}

	return (error);
}

static uint16_t
geneve_pick_source_port(struct geneve_softc *sc, struct mbuf *m)
{
	int range;
	uint32_t hash;

	range = sc->gnv_max_port - sc->gnv_min_port + 1;

	/* RFC 8926 Section 3.3-2.2.1 */
	if (M_HASHTYPE_ISHASH(m))
		hash = m->m_pkthdr.flowid;
	else
		hash = jenkins_hash(m->m_data, ETHER_HDR_LEN, sc->gnv_port_hash_key);

	return (sc->gnv_min_port + (hash % range));
}

static void
geneve_encap_header(struct geneve_softc *sc, struct mbuf *m, int ipoff,
    uint16_t srcport, uint16_t dstport, uint16_t proto)
{
	struct geneveudphdr *hdr;
	struct udphdr *udph;
	struct genevehdr *gnvh;
	int len;

	len = m->m_pkthdr.len - ipoff;
	MPASS(len >= sizeof(struct geneveudphdr));
	hdr = mtodo(m, ipoff);

	udph = &hdr->geneve_udp;
	udph->uh_sport = srcport;
	udph->uh_dport = dstport;
	udph->uh_ulen = htons(len);
	udph->uh_sum = 0;

	gnvh = &hdr->geneve_hdr;
	gnvh->geneve_ver = 0;
	gnvh->geneve_optlen = 0;
	gnvh->geneve_critical = 0;
	gnvh->geneve_control = 0;
	gnvh->geneve_flags = 0;
	gnvh->geneve_proto = proto;
	gnvh->geneve_vni = htonl(sc->gnv_vni << GENEVE_HDR_VNI_SHIFT);
}

/* Return the CSUM_INNER_* equivalent of CSUM_* caps. */
static uint32_t
csum_flags_to_inner_flags(uint32_t csum_flags_in, const uint32_t encap)
{
	uint32_t csum_flags = encap;
	const uint32_t v4 = CSUM_IP | CSUM_IP_UDP | CSUM_IP_TCP;

	/*
	 * csum_flags can request either v4 or v6 offload but not both.
	 * tcp_output always sets CSUM_TSO (both CSUM_IP_TSO and CSUM_IP6_TSO)
	 * so those bits are no good to detect the IP version.  Other bits are
	 * always set with CSUM_TSO and we use those to figure out the IP
	 * version.
	 */
	if (csum_flags_in & v4) {
		if (csum_flags_in & CSUM_IP)
			csum_flags |= CSUM_INNER_IP;
		if (csum_flags_in & CSUM_IP_UDP)
			csum_flags |= CSUM_INNER_IP_UDP;
		if (csum_flags_in & CSUM_IP_TCP)
			csum_flags |= CSUM_INNER_IP_TCP;
		if (csum_flags_in & CSUM_IP_TSO)
			csum_flags |= CSUM_INNER_IP_TSO;
	} else {
#ifdef INVARIANTS
		const uint32_t v6 = CSUM_IP6_UDP | CSUM_IP6_TCP;
		MPASS((csum_flags_in & v6) != 0);
#endif
		if (csum_flags_in & CSUM_IP6_UDP)
			csum_flags |= CSUM_INNER_IP6_UDP;
		if (csum_flags_in & CSUM_IP6_TCP)
			csum_flags |= CSUM_INNER_IP6_TCP;
		if (csum_flags_in & CSUM_IP6_TSO)
			csum_flags |= CSUM_INNER_IP6_TSO;
	}

	return (csum_flags);
}

static uint16_t
geneve_get_ethertype(struct mbuf *m)
{
	struct ip *ip;
	struct ip6_hdr *ip6;

	/*
	 * We should pullup, but we're only interested in the first byte, so
	 * that'll always be contiguous.
	 */
	ip = mtod(m, struct ip *);
	if (ip->ip_v == IPVERSION)
		return (ETHERTYPE_IP);

	ip6 = mtod(m, struct ip6_hdr *);
	if ((ip6->ip6_vfc & IPV6_VERSION_MASK) == IPV6_VERSION)
		return (ETHERTYPE_IPV6);

	return (0);
}

/* RFC 8926 Section 4.4.2. DSCP, ECN, and TTL */
static int
geneve_inherit_l3_hdr(struct mbuf *m, struct geneve_softc *sc, uint16_t proto,
    uint8_t *tos, uint8_t *ttl, u_short *ip_off)
{
	struct ether_header *eh;
	struct ip *ip_inner, iphdr;
	struct ip6_hdr *ip6_inner, ip6hdr;
	int offset;

	*tos = 0;
	*ttl = sc->gnv_ttl;
	if (sc->gnv_df == IFLA_GENEVE_DF_SET)
		*ip_off = htons(IP_DF);
	else
		*ip_off = 0;

	/* Set offset and address family if proto is ethernet */
	if (proto == GENEVE_PROTO_ETHER) {
		eh = mtod(m, struct ether_header *);
		if (eh->ether_type == htons(ETHERTYPE_IP)) {
			if (m->m_pkthdr.len < ETHER_HDR_LEN + sizeof(struct ip)) {
				m_freem(m);
				return (EINVAL);
			}
			proto = ETHERTYPE_IP;
		} else if (eh->ether_type == htons(ETHERTYPE_IPV6)) {
			if (m->m_pkthdr.len < ETHER_HDR_LEN + sizeof(struct ip6_hdr)) {
				m_freem(m);
				return (EINVAL);
			}
			proto = ETHERTYPE_IPV6;
		} else
			return (0);

		offset = ETHER_HDR_LEN;
	} else
		offset = 0;

	switch (proto) {
	case ETHERTYPE_IP:
		if (__predict_false(m->m_len < offset + sizeof(struct ip))) {
			m_copydata(m, offset, sizeof(struct ip), (caddr_t)&iphdr);
			ip_inner = &iphdr;
		} else
			ip_inner = mtodo(m, offset);

		*tos = ip_inner->ip_tos;
		if (sc->gnv_flags & GENEVE_FLAG_TTL_INHERIT)
			*ttl = ip_inner->ip_ttl;
		if (sc->gnv_df == IFLA_GENEVE_DF_INHERIT)
			*ip_off = ip_inner->ip_off;
		break;

	case ETHERTYPE_IPV6:
		if (__predict_false(m->m_len < offset + sizeof(struct ip6_hdr))) {
			m_copydata(m, offset, sizeof(struct ip6_hdr), (caddr_t)&ip6hdr);
			ip6_inner = &ip6hdr;
		} else
			ip6_inner = mtodo(m, offset);

		*tos = IPV6_TRAFFIC_CLASS(ip6_inner);
		if (sc->gnv_flags & GENEVE_FLAG_TTL_INHERIT)
			*ttl = ip6_inner->ip6_hlim;
		break;
	}

	return (0);
}

#ifdef INET
static int
geneve_encap4(struct geneve_softc *sc, const union sockaddr_union *funsa,
    struct mbuf *m)
{
	struct ifnet *ifp;
	struct ip *ip;
	struct in_addr srcaddr, dstaddr;
	struct route route, *ro;
	struct sockaddr_in *sin;
	int plen, error;
	uint32_t csum_flags;
	uint16_t srcport, dstport, proto;
	u_short ip_off;
	uint8_t tos, ecn, ttl;
	bool mcast;

	NET_EPOCH_ASSERT();

	ifp = sc->gnv_ifp;
	srcaddr = sc->gnv_src_addr.sin.sin_addr;
	srcport = htons(geneve_pick_source_port(sc, m));
	dstaddr = funsa->sin.sin_addr;
	dstport = funsa->sin.sin_port;
	plen = m->m_pkthdr.len;

	if (sc->gnv_proto == GENEVE_PROTO_ETHER)
		proto = sc->gnv_proto;
	else
		proto = geneve_get_ethertype(m);

	error = geneve_inherit_l3_hdr(m, sc, proto, &tos, &ttl, &ip_off);
	if (error) {
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
		return (error);
	}

	M_PREPEND(m, sizeof(struct ip) + sizeof(struct geneveudphdr), M_NOWAIT);
	if (m == NULL) {
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
		return (ENOBUFS);
	}
	ip = mtod(m, struct ip *);

	ecn = (tos & IPTOS_ECN_MASK);
	ip_ecn_ingress(ECN_ALLOWED, &ip->ip_tos, &ecn);
	if (sc->gnv_flags & GENEVE_FLAG_DSCP_INHERIT)
		ip->ip_tos |= (tos & ~IPTOS_ECN_MASK);

	ip->ip_len = htons(m->m_pkthdr.len);
	ip->ip_off = ip_off;
	ip->ip_ttl = ttl;
	ip->ip_p = IPPROTO_UDP;
	ip->ip_sum = 0;
	ip->ip_src = srcaddr;
	ip->ip_dst = dstaddr;

	geneve_encap_header(sc, m, sizeof(struct ip), srcport, dstport, htons(proto));
	mcast = (m->m_flags & (M_MCAST | M_BCAST));
	m->m_flags &= ~(M_MCAST | M_BCAST);

	m->m_pkthdr.csum_flags &= CSUM_FLAGS_TX;
	if (m->m_pkthdr.csum_flags != 0) {
		/*
		 * HW checksum (L3 and/or L4) or TSO has been requested.
		 * Look up the ifnet for the outbound route and verify that the
		 * outbound ifnet can perform the requested operation on the inner frame.
		 */
		memset(&route, 0, sizeof(route));
		ro = &route;
		sin = (struct sockaddr_in *)&ro->ro_dst;
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr = ip->ip_dst;
		ro->ro_nh = fib4_lookup(M_GETFIB(m), ip->ip_dst, 0, NHR_NONE, 0);
		if (ro->ro_nh == NULL) {
			m_freem(m);
			if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
			return (EHOSTUNREACH);
		}

		csum_flags = csum_flags_to_inner_flags(m->m_pkthdr.csum_flags,
		    CSUM_ENCAP_GENEVE);
		if ((csum_flags & ro->ro_nh->nh_ifp->if_hwassist) != csum_flags) {
			if (ppsratecheck(&sc->err_time, &sc->err_pps, 1)) {
				const struct ifnet *nh_ifp = ro->ro_nh->nh_ifp;

				if_printf(ifp, "interface %s is missing hwcaps "
				    "0x%08x, csum_flags 0x%08x -> 0x%08x, "
				    "hwassist 0x%08x\n", nh_ifp->if_xname,
				    csum_flags & ~(uint32_t)nh_ifp->if_hwassist,
				    m->m_pkthdr.csum_flags, csum_flags,
				    (uint32_t)nh_ifp->if_hwassist);
			}
			m_freem(m);
			if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
			return (ENXIO);
		}
		m->m_pkthdr.csum_flags = csum_flags;
		if (csum_flags & (CSUM_INNER_IP | CSUM_INNER_IP_UDP |
		    CSUM_INNER_IP6_UDP | CSUM_INNER_IP_TCP | CSUM_INNER_IP6_TCP)) {
			counter_u64_add(sc->gnv_stats.txcsum, 1);
			if (csum_flags & CSUM_INNER_TSO)
				counter_u64_add(sc->gnv_stats.tso, 1);
		}
	} else
		ro = NULL;

	error = ip_output(m, NULL, ro, 0, sc->gnv_im4o, NULL);
	if (error == 0) {
		if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);
		if_inc_counter(ifp, IFCOUNTER_OBYTES, plen);
		if (mcast)
			if_inc_counter(ifp, IFCOUNTER_OMCASTS, 1);
	} else
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);

	return (error);
}
#endif

#ifdef INET6
static int
geneve_encap6(struct geneve_softc *sc, const union sockaddr_union *funsa,
    struct mbuf *m)
{
	struct ifnet *ifp;
	struct ip6_hdr *ip6;
	struct ip6_pktopts opts;
	struct sockaddr_in6 *sin6;
	struct route_in6 route, *ro;
	const struct in6_addr *srcaddr, *dstaddr;
	int plen, error;
	uint32_t csum_flags;
	uint16_t srcport, dstport, proto;
	u_short ip6_df;
	uint8_t tos, ecn, etos, ttl;
	bool mcast;

	NET_EPOCH_ASSERT();

	ifp = sc->gnv_ifp;
	srcaddr = &sc->gnv_src_addr.sin6.sin6_addr;
	srcport = htons(geneve_pick_source_port(sc, m));
	dstaddr = &funsa->sin6.sin6_addr;
	dstport = funsa->sin6.sin6_port;
	plen = m->m_pkthdr.len;

	if (sc->gnv_proto == GENEVE_PROTO_ETHER)
		proto = sc->gnv_proto;
	else
		proto = geneve_get_ethertype(m);

	error = geneve_inherit_l3_hdr(m, sc, proto, &tos, &ttl, &ip6_df);
	if (error) {
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
			return (error);
	}

	ip6_initpktopts(&opts);
	if (ip6_df)
		opts.ip6po_flags = IP6PO_DONTFRAG;

	M_PREPEND(m, sizeof(struct ip6_hdr) + sizeof(struct geneveudphdr), M_NOWAIT);
	if (m == NULL) {
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
		return (ENOBUFS);
	}

	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_flow = 0;
	ip6->ip6_vfc = IPV6_VERSION;

	ecn = (tos & IPTOS_ECN_MASK);
	ip_ecn_ingress(ECN_ALLOWED, &etos, &ecn);
	ip6->ip6_flow |= htonl((u_int32_t)etos << IPV6_FLOWLABEL_LEN);
	if (sc->gnv_flags & GENEVE_FLAG_DSCP_INHERIT)
		ip6->ip6_flow |= htonl((u_int32_t)tos << IPV6_FLOWLABEL_LEN);

	ip6->ip6_plen = 0;
	ip6->ip6_nxt = IPPROTO_UDP;
	ip6->ip6_hlim = ttl;
	ip6->ip6_src = *srcaddr;
	ip6->ip6_dst = *dstaddr;

	geneve_encap_header(sc, m, sizeof(struct ip6_hdr), srcport, dstport,
	    htons(proto));
	mcast = (m->m_flags & (M_MCAST | M_BCAST));
	m->m_flags &= ~(M_MCAST | M_BCAST);

	ro = NULL;
	m->m_pkthdr.csum_flags &= CSUM_FLAGS_TX;
	if (mcast || m->m_pkthdr.csum_flags != 0) {
		/*
		 * HW checksum (L3 and/or L4) or TSO has been requested.  Look
		 * up the ifnet for the outbound route and verify that the
		 * outbound ifnet can perform the requested operation on the
		 * inner frame.
		 * XXX: There's a rare scenario with ipv6 over multicast
		 * underlay where, when mc_ifname is set, it causes panics
		 * inside a jail. We'll force geneve to select its own outbound
		 * interface to avoid this.
		 */
		memset(&route, 0, sizeof(route));
		ro = &route;
		sin6 = (struct sockaddr_in6 *)&ro->ro_dst;
		sin6->sin6_family = AF_INET6;
		sin6->sin6_len = sizeof(*sin6);
		sin6->sin6_addr = ip6->ip6_dst;
		ro->ro_nh = fib6_lookup(M_GETFIB(m), &ip6->ip6_dst, 0, NHR_NONE, 0);
		if (ro->ro_nh == NULL) {
			m_freem(m);
			if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
			return (EHOSTUNREACH);
		}
	}
	if (m->m_pkthdr.csum_flags != 0) {
		csum_flags = csum_flags_to_inner_flags(m->m_pkthdr.csum_flags,
		    CSUM_ENCAP_GENEVE);
		if ((csum_flags & ro->ro_nh->nh_ifp->if_hwassist) != csum_flags) {
			if (ppsratecheck(&sc->err_time, &sc->err_pps, 1)) {
				const struct ifnet *nh_ifp = ro->ro_nh->nh_ifp;

				if_printf(ifp, "interface %s is missing hwcaps "
				    "0x%08x, csum_flags 0x%08x -> 0x%08x, "
				    "hwassist 0x%08x\n", nh_ifp->if_xname,
				    csum_flags & ~(uint32_t)nh_ifp->if_hwassist,
				    m->m_pkthdr.csum_flags, csum_flags,
				    (uint32_t)nh_ifp->if_hwassist);
			}
			m_freem(m);
			if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
			return (ENXIO);
		}
		m->m_pkthdr.csum_flags = csum_flags;
		if (csum_flags &
		    (CSUM_INNER_IP | CSUM_INNER_IP_UDP | CSUM_INNER_IP6_UDP |
		    CSUM_INNER_IP_TCP | CSUM_INNER_IP6_TCP)) {
			counter_u64_add(sc->gnv_stats.txcsum, 1);
			if (csum_flags & CSUM_INNER_TSO)
				counter_u64_add(sc->gnv_stats.tso, 1);
		}
	} else if (ntohs(dstport) != V_zero_checksum_port) {
		struct udphdr *hdr = mtodo(m, sizeof(struct ip6_hdr));

		hdr->uh_sum = in6_cksum_pseudo(ip6,
		    m->m_pkthdr.len - sizeof(struct ip6_hdr), IPPROTO_UDP, 0);
		m->m_pkthdr.csum_flags = CSUM_UDP_IPV6;
		m->m_pkthdr.csum_data = offsetof(struct udphdr, uh_sum);
	}
	error = ip6_output(m, &opts, ro, 0, sc->gnv_im6o, NULL, NULL);
	if (error == 0) {
		if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);
		if_inc_counter(ifp, IFCOUNTER_OBYTES, plen);
		if (mcast)
			if_inc_counter(ifp, IFCOUNTER_OMCASTS, 1);
	} else
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);

	return (error);
}
#endif

static int
geneve_transmit(struct ifnet *ifp, struct mbuf *m)
{
	struct rm_priotracker tracker;
	union sockaddr_union unsa;
	struct geneve_softc *sc;
	struct gnv_ftable_entry *fe;
	struct ifnet *mcifp;
	struct ether_header *eh;
	uint32_t af;
	int error;

	mcifp = NULL;
	sc = ifp->if_softc;
	GENEVE_RLOCK(sc, &tracker);
	M_SETFIB(m, sc->gnv_fibnum);

	if ((sc->gnv_flags & GENEVE_FLAG_RUNNING) == 0) {
		GENEVE_RUNLOCK(sc, &tracker);
		m_freem(m);
		return (ENETDOWN);
	}
	if (__predict_false(if_tunnel_check_nesting(ifp, m,
	    MTAG_GENEVE_LOOP, 1) != 0)) {
		GENEVE_RUNLOCK(sc, &tracker);
		m_freem(m);
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
		return (ELOOP);
	}

	if (sc->gnv_proto == GENEVE_PROTO_ETHER) {
		fe = NULL;
		eh = mtod(m, struct ether_header *);

		ETHER_BPF_MTAP(ifp, m);
		if ((m->m_flags & (M_BCAST | M_MCAST)) == 0)
			fe = geneve_ftable_entry_lookup(sc, eh->ether_dhost);
		if (fe == NULL)
			fe = &sc->gnv_default_fe;
		geneve_sockaddr_copy(&unsa, &fe->gnvfe_raddr.sa);
	} else
		geneve_sockaddr_copy(&unsa, &sc->gnv_dst_addr.sa);

	af = unsa.sa.sa_family;
	if (geneve_check_multicast_addr(&unsa) != 0)
		mcifp = geneve_multicast_if_ref(sc, af);

	GENEVE_ACQUIRE(sc);
	GENEVE_RUNLOCK(sc, &tracker);

	switch (af) {
#ifdef INET
	case AF_INET:
		error = geneve_encap4(sc, &unsa, m);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		error = geneve_encap6(sc, &unsa, m);
		break;
#endif
	default:
		error = EAFNOSUPPORT;
	}

	geneve_release(sc);
	if (mcifp != NULL)
		if_rele(mcifp);

	return (error);
}

static int
geneve_output(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst,
    struct route *ro)
{
	uint32_t af;
	int error;

#ifdef MAC
	error = mac_ifnet_check_transmit(ifp, m);
	if (error) {
		m_freem(m);
		return (error);
	}
#endif

	/* BPF writes need to be handled specially. */
	if (dst->sa_family == AF_UNSPEC || dst->sa_family == pseudo_AF_HDRCMPLT)
		memmove(&af, dst->sa_data, sizeof(af));
	else
		af = RO_GET_FAMILY(ro, dst);

	BPF_MTAP2(ifp, &af, sizeof(af), m);
	error = (ifp->if_transmit)(ifp, m);
	if (error)
		return (ENOBUFS);
	return (0);
}

static int
geneve_next_option(struct geneve_socket *gnvso, struct genevehdr *gnvh,
	struct mbuf **m0)
{
	int optlen, error;

	error = 0;
	/*
	 * We MUST NOT forward the packet if control (O) bit is set
	 * and currently there is not standard specification for it.
	 * Therefore, we drop it.
	 */
	if (gnvh->geneve_control)
		return (EINVAL);

	optlen = gnvh->geneve_optlen;
	if (optlen == 0)
		return (error);

	/*
	 * XXX: Geneve options processing
	 * We MUST drop the packet if there are options to process
	 * and we are not able to process it.
	 */
	if (gnvh->geneve_critical)
		error = EINVAL;

	return (error);
}

static void
geneve_qflush(struct ifnet *ifp __unused)
{
}

static void
geneve_input_csum(struct mbuf *m, struct ifnet *ifp, counter_u64_t rxcsum)
{
	uint32_t csum_flags;

	if ((((ifp->if_capenable & IFCAP_RXCSUM) != 0 &&
	    (m->m_pkthdr.csum_flags & CSUM_INNER_L3_CALC) != 0) ||
	    ((ifp->if_capenable & IFCAP_RXCSUM_IPV6) != 0 &&
	    (m->m_pkthdr.csum_flags & CSUM_INNER_L3_CALC) == 0))) {
		csum_flags = 0;

		if (m->m_pkthdr.csum_flags & CSUM_INNER_L3_CALC)
			csum_flags |= CSUM_L3_CALC;
		if (m->m_pkthdr.csum_flags & CSUM_INNER_L3_VALID)
			csum_flags |= CSUM_L3_VALID;
		if (m->m_pkthdr.csum_flags & CSUM_INNER_L4_CALC)
			csum_flags |= CSUM_L4_CALC;
		if (m->m_pkthdr.csum_flags & CSUM_INNER_L4_VALID)
			csum_flags |= CSUM_L4_VALID;
		m->m_pkthdr.csum_flags = csum_flags;
		counter_u64_add(rxcsum, 1);
	} else {
		/* clear everything */
		m->m_pkthdr.csum_flags = 0;
		m->m_pkthdr.csum_data = 0;
	}
}

static uint32_t
geneve_map_etype_to_af(uint32_t ethertype)
{

	if (ethertype == ETHERTYPE_IP)
		return (AF_INET);
	if (ethertype == ETHERTYPE_IPV6)
		return (AF_INET6);
	if (ethertype == ETHERTYPE_ARP)
		return (AF_LINK);
	return (0);
}

static bool
geneve_udp_input(struct mbuf *m, int offset, struct inpcb *inpcb,
    const struct sockaddr *srcsa, void *xgnvso)
{
	struct geneve_socket *gnvso;
	struct geneve_pkt_info info;
	struct genevehdr *gnvh, gnvhdr;
	struct geneve_softc *sc;
	struct ip *iphdr;
	struct ip6_hdr *ip6hdr;
	struct ifnet *ifp;
	int32_t plen, af;
	uint32_t vni;
	uint16_t optlen, proto;
	int error;

	M_ASSERTPKTHDR(m);
	plen = m->m_pkthdr.len;
	gnvso = xgnvso;

	if (m->m_pkthdr.len < offset + sizeof(struct geneveudphdr))
		return (false);

	/* Get ECN and TTL values for future processing */
	memset(&info, 0, sizeof(info));
	info.ethertype = geneve_get_ethertype(m);
	if (info.ethertype == ETHERTYPE_IP) {
		iphdr = mtodo(m, offset - sizeof(struct ip));
		info.ecn = (iphdr->ip_tos & IPTOS_ECN_MASK);
		info.ttl = iphdr->ip_ttl;
	} else if (info.ethertype == ETHERTYPE_IPV6) {
		ip6hdr = mtodo(m, offset - sizeof(struct ip6_hdr));
		info.ecn = IPV6_ECN(ip6hdr);
		info.ttl = ip6hdr->ip6_hlim;
	}

	/* Get geneve header */
	offset += sizeof(struct udphdr);
	if (__predict_false(m->m_len < offset + sizeof(struct genevehdr))) {
		m_copydata(m, offset, sizeof(struct genevehdr), (caddr_t)&gnvhdr);
		gnvh = &gnvhdr;
	} else
		gnvh = mtodo(m, offset);

	/*
	 * Drop if there is a reserved bit or unknown version set in the header.
	 * As defined in RFC 8926 3.4
	 */
	if (gnvh->geneve_ver != htons(GENEVE_VERSION) ||
	    gnvh->geneve_vni & ~GENEVE_VNI_MASK)
		return (false);

	/*
	 * The length of the option fields, expressed in 4-byte multiples, not
	 * including the 8-byte fixed tunnel header.
	 */
	optlen = ntohs(gnvh->geneve_optlen) * 4;
	error = geneve_next_option(gnvso, gnvh, &m);
	if (error != 0)
		return (false);

	vni = ntohl(gnvh->geneve_vni) >> GENEVE_HDR_VNI_SHIFT;
	sc = geneve_socket_lookup_softc(gnvso, vni);
	if (sc == NULL)
		return (false);

	if ((sc->gnv_flags & GENEVE_FLAG_RUNNING) == 0)
		goto out;

	proto = ntohs(gnvh->geneve_proto);
	m_adj(m, offset + sizeof(struct genevehdr) + optlen);

	/* if next protocol is ethernet, check its ethertype and learn it */
	if (proto == GENEVE_PROTO_ETHER) {
		offset = ETHER_HDR_LEN;
		error = geneve_input_ether(sc, &m, srcsa, &info);
		if (error != 0)
			goto out;
	} else {
		info.ethertype = proto;
		af = geneve_map_etype_to_af(info.ethertype);
		offset = 0;
	}

	error = geneve_input_inherit(sc, &m, offset, &info);
	if (error != 0)
		goto out;

	ifp = sc->gnv_ifp;
	if (ifp == m->m_pkthdr.rcvif)
		/* XXX Does not catch more complex loops. */
		goto out;

	m_clrprotoflags(m);
	m->m_pkthdr.rcvif = ifp;
	M_SETFIB(m, ifp->if_fib);
	geneve_input_csum(m, ifp, sc->gnv_stats.rxcsum);
	if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);
	if_inc_counter(ifp, IFCOUNTER_IBYTES, plen);
	if (sc->gnv_mc_ifp != NULL)
		if_inc_counter(ifp, IFCOUNTER_IMCASTS, 1);

	MPASS(m != NULL);

	if (proto == GENEVE_PROTO_ETHER)
		(*ifp->if_input)(ifp, m);
	else {
		BPF_MTAP2(ifp, &af, sizeof(af), m);
		netisr_dispatch_src(info.isr, (uintptr_t)xgnvso, m);
	}

	m = NULL;
out:
	geneve_release(sc);
	if (m != NULL) {
		if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
		m_freem(m);
	}

	return (true);
}

static int
geneve_input_ether(struct geneve_softc *sc, struct mbuf **m0,
    const struct sockaddr *sa, struct geneve_pkt_info *info)
{
	struct mbuf *m;
	struct ether_header *eh;

	m = *m0;

	if (sc->gnv_proto != GENEVE_PROTO_ETHER)
		return (EPROTOTYPE);

	if (m->m_pkthdr.len < ETHER_HDR_LEN)
		return (EINVAL);

	if (m->m_len < ETHER_HDR_LEN &&
	    (m = m_pullup(m, ETHER_HDR_LEN)) == NULL) {
		*m0 = NULL;
		return (ENOBUFS);
	}

	eh = mtod(m, struct ether_header *);
	info->ethertype = ntohs(eh->ether_type);
	if (sc->gnv_flags & GENEVE_FLAG_LEARN)
		geneve_ftable_learn(sc, sa, eh->ether_shost);

	*m0 = m;
	return (0);
}

static int
geneve_input_inherit(struct geneve_softc *sc, struct mbuf **m0,
    int offset, struct geneve_pkt_info *info)
{
	struct mbuf *m;
	struct ip *iphdr;
	struct ip6_hdr *ip6hdr;
	uint8_t itos;

	m = *m0;

	switch (info->ethertype) {
	case ETHERTYPE_IP:
		offset += sizeof(struct ip);
		if (m->m_pkthdr.len < offset)
			return (EINVAL);

		if (m->m_len < offset &&
		    (m = m_pullup(m, offset)) == NULL) {
			*m0 = NULL;
			return (ENOBUFS);
		}
		iphdr = mtodo(m, offset - sizeof(struct ip));

		if (ip_ecn_egress(ECN_COMPLETE, &info->ecn, &iphdr->ip_tos) == 0) {
			*m0 = NULL;
			return (ENOBUFS);
		}

		if ((sc->gnv_flags & GENEVE_FLAG_TTL_INHERIT) != 0 && info->ttl > 0)
			iphdr->ip_ttl = info->ttl;

		info->isr = NETISR_IP;
		break;

	case ETHERTYPE_IPV6:
		offset += sizeof(struct ip6_hdr);
		if (m->m_pkthdr.len < offset)
			return (EINVAL);

		if (m->m_len < offset &&
		    (m = m_pullup(m, offset)) == NULL) {
			*m0 = NULL;
			return (ENOBUFS);
		}
		ip6hdr = mtodo(m, offset - sizeof(struct ip6_hdr));

		itos = (ntohl(ip6hdr->ip6_flow) >> IPV6_FLOWLABEL_LEN) & 0xff;
		if (ip_ecn_egress(ECN_COMPLETE, &info->ecn, &itos) == 0) {
			*m0 = NULL;
			return (ENOBUFS);
		}
		ip6hdr->ip6_flow |= htonl((uint32_t)itos << IPV6_FLOWLABEL_LEN);

		if ((sc->gnv_flags & GENEVE_FLAG_TTL_INHERIT) && (info->ttl > 0))
			ip6hdr->ip6_hlim = info->ttl;

		info->isr = NETISR_IPV6;
		break;

	case ETHERTYPE_ARP:
		if (sc->gnv_proto == GENEVE_PROTO_INHERIT)
			return (EINVAL);

		offset += sizeof(struct arphdr);
		if (m->m_pkthdr.len < offset)
			return (EINVAL);

		if (m->m_len < offset &&
		    (m = m_pullup(m, offset)) == NULL) {
			*m0 = NULL;
			return (ENOBUFS);
		}
		info->isr = NETISR_ARP;
		break;

	default:
		if_inc_counter(sc->gnv_ifp, IFCOUNTER_NOPROTO, 1);
		return (EINVAL);
	}

	*m0 = m;
	return (0);
}

static void
geneve_stats_alloc(struct geneve_softc *sc)
{
	struct geneve_statistics *stats = &sc->gnv_stats;

	stats->txcsum = counter_u64_alloc(M_WAITOK);
	stats->tso = counter_u64_alloc(M_WAITOK);
	stats->rxcsum = counter_u64_alloc(M_WAITOK);
}

static void
geneve_stats_free(struct geneve_softc *sc)
{
	struct geneve_statistics *stats = &sc->gnv_stats;

	counter_u64_free(stats->txcsum);
	counter_u64_free(stats->tso);
	counter_u64_free(stats->rxcsum);
}

static void
geneve_set_default_config(struct geneve_softc *sc)
{

	sc->gnv_flags |= GENEVE_FLAG_LEARN;

	sc->gnv_vni = GENEVE_VNI_MAX;
	sc->gnv_ttl = V_ip_defttl;

	sc->gnv_src_addr.sin.sin_port = htons(GENEVE_UDPPORT);
	sc->gnv_dst_addr.sin.sin_port = htons(GENEVE_UDPPORT);

	/*
	 * RFC 8926 Section 3.3, the entire 16-bit range MAY
	 * be used to maximize entropy.
	 */
	sc->gnv_min_port = V_ipport_firstauto;
	sc->gnv_max_port = V_ipport_lastauto;

	sc->gnv_proto = GENEVE_PROTO_ETHER;

	sc->gnv_ftable_max = GENEVE_FTABLE_MAX;
	sc->gnv_ftable_timeout = GENEVE_FTABLE_TIMEOUT;
}

static int
geneve_set_reqcap(struct geneve_softc *sc, struct ifnet *ifp, int reqcap,
    int reqcap2)
{
	int mask = reqcap ^ ifp->if_capenable;

	/* Disable TSO if tx checksums are disabled. */
	if (mask & IFCAP_TXCSUM && !(reqcap & IFCAP_TXCSUM) &&
	    reqcap & IFCAP_TSO4) {
		reqcap &= ~IFCAP_TSO4;
		if_printf(ifp, "tso4 disabled due to -txcsum.\n");
	}
	if (mask & IFCAP_TXCSUM_IPV6 && !(reqcap & IFCAP_TXCSUM_IPV6) &&
	    reqcap & IFCAP_TSO6) {
		reqcap &= ~IFCAP_TSO6;
		if_printf(ifp, "tso6 disabled due to -txcsum6.\n");
	}

	/* Do not enable TSO if tx checksums are disabled. */
	if (mask & IFCAP_TSO4 && reqcap & IFCAP_TSO4 &&
	    !(reqcap & IFCAP_TXCSUM)) {
		if_printf(ifp, "enable txcsum first.\n");
		return (EAGAIN);
	}
	if (mask & IFCAP_TSO6 && reqcap & IFCAP_TSO6 &&
	    !(reqcap & IFCAP_TXCSUM_IPV6)) {
		if_printf(ifp, "enable txcsum6 first.\n");
		return (EAGAIN);
	}

	sc->gnv_reqcap = reqcap;
	sc->gnv_reqcap2 = reqcap2;
	return (0);
}

/*
 * A GENEVE interface inherits the capabilities of the genevedev or the interface
 * hosting the genevelocal address.
 */
static void
geneve_set_hwcaps(struct geneve_softc *sc)
{
	struct epoch_tracker et;
	struct ifnet *p, *ifp;
	struct ifaddr *ifa;
	u_long hwa;
	int cap, ena;
	bool rel;

	/* reset caps */
	ifp = sc->gnv_ifp;
	ifp->if_capabilities &= GENEVE_BASIC_IFCAPS;
	ifp->if_capenable &= GENEVE_BASIC_IFCAPS;
	ifp->if_hwassist = 0;

	NET_EPOCH_ENTER(et);
	CURVNET_SET(ifp->if_vnet);

	p = NULL;
	rel = false;
	if (sc->gnv_mc_ifname[0] != '\0') {
		rel = true;
		p = ifunit_ref(sc->gnv_mc_ifname);
	} else if (geneve_sockaddr_in_any(&sc->gnv_src_addr) == 0) {
		if (sc->gnv_src_addr.sa.sa_family == AF_INET) {
			struct sockaddr_in in4 = sc->gnv_src_addr.sin;

			in4.sin_port = 0;
			ifa = ifa_ifwithaddr((struct sockaddr *)&in4);
			if (ifa != NULL)
				p = ifa->ifa_ifp;
		} else if (sc->gnv_src_addr.sa.sa_family == AF_INET6) {
			struct sockaddr_in6 in6 = sc->gnv_src_addr.sin6;

			in6.sin6_port = 0;
			ifa = ifa_ifwithaddr((struct sockaddr *)&in6);
			if (ifa != NULL)
				p = ifa->ifa_ifp;
		}
	}
	if (p == NULL) {
		CURVNET_RESTORE();
		NET_EPOCH_EXIT(et);
		return;
	}

	cap = ena = hwa = 0;

	/* checksum offload */
	if ((p->if_capabilities2 & IFCAP2_BIT(IFCAP2_GENEVE_HWCSUM)) != 0)
		cap |= p->if_capabilities & (IFCAP_HWCSUM | IFCAP_HWCSUM_IPV6);
	if ((p->if_capenable2 & IFCAP2_BIT(IFCAP2_GENEVE_HWCSUM)) != 0) {
		ena |= sc->gnv_reqcap & p->if_capenable & (IFCAP_HWCSUM | IFCAP_HWCSUM_IPV6);
		if (ena & IFCAP_TXCSUM) {
			if (p->if_hwassist & CSUM_INNER_IP)
				hwa |= CSUM_IP;
			if (p->if_hwassist & CSUM_INNER_IP_UDP)
				hwa |= CSUM_IP_UDP;
			if (p->if_hwassist & CSUM_INNER_IP_TCP)
				hwa |= CSUM_IP_TCP;
		}
		if (ena & IFCAP_TXCSUM_IPV6) {
			if (p->if_hwassist & CSUM_INNER_IP6_UDP)
				hwa |= CSUM_IP6_UDP;
			if (p->if_hwassist & CSUM_INNER_IP6_TCP)
				hwa |= CSUM_IP6_TCP;
		}
	}

	/* hardware TSO */
	if ((p->if_capabilities2 & IFCAP2_BIT(IFCAP2_GENEVE_HWTSO)) != 0) {
		cap |= p->if_capabilities & IFCAP_TSO;
		if (p->if_hw_tsomax > IP_MAXPACKET - ifp->if_hdrlen)
			ifp->if_hw_tsomax = IP_MAXPACKET - ifp->if_hdrlen;
		else
			ifp->if_hw_tsomax = p->if_hw_tsomax;
		ifp->if_hw_tsomaxsegcount = p->if_hw_tsomaxsegcount - 1;
		ifp->if_hw_tsomaxsegsize = p->if_hw_tsomaxsegsize;
	}
	if ((p->if_capenable2 & IFCAP2_BIT(IFCAP2_GENEVE_HWTSO)) != 0) {
		ena |= sc->gnv_reqcap & p->if_capenable & IFCAP_TSO;
		if (ena & IFCAP_TSO) {
			if (p->if_hwassist & CSUM_INNER_IP_TSO)
				hwa |= CSUM_IP_TSO;
			if (p->if_hwassist & CSUM_INNER_IP6_TSO)
				hwa |= CSUM_IP6_TSO;
		}
	}

	ifp->if_capabilities |= cap;
	ifp->if_capenable |= ena;
	ifp->if_hwassist |= hwa;
	if (rel)
		if_rele(p);

	CURVNET_RESTORE();
	NET_EPOCH_EXIT(et);
}

static int
geneve_clone_create_nl(struct if_clone *ifc, char *name, size_t len,
    struct ifc_data_nl *ifd)
{
	struct nl_parsed_link *lattrs = ifd->lattrs;
	struct nl_pstate *npt = ifd->npt;
	struct nl_parsed_geneve attrs = {};
	int error;

	if ((lattrs->ifla_idata == NULL) ||
	    (!nl_has_attr(ifd->bm, IFLA_LINKINFO))) {
		nlmsg_report_err_msg(npt, "geneve protocol is required");
		return (ENOTSUP);
	}

	error = nl_parse_nested(lattrs->ifla_idata, &geneve_create_parser, npt, &attrs);
	if (error != 0)
		return (error);
	if (geneve_check_proto(attrs.ifla_proto)) {
		nlmsg_report_err_msg(npt, "Unsupported ethertype: 0x%04X", attrs.ifla_proto);
		return (ENOTSUP);
	}

	struct geneve_params gnvp = { .ifla_proto = attrs.ifla_proto };
	struct ifc_data ifd_new = {
		.flags = IFC_F_SYSSPACE,
		.unit = ifd->unit,
		.params = &gnvp
	};

	return (geneve_clone_create(ifc, name, len, &ifd_new, &ifd->ifp));
}

static int
geneve_clone_modify_nl(struct ifnet *ifp, struct ifc_data_nl *ifd)
{
	struct geneve_softc *sc = ifp->if_softc;
	struct nl_parsed_link *lattrs = ifd->lattrs;
	struct nl_pstate *npt = ifd->npt;
	struct nl_parsed_geneve params;
	struct nlattr *attrs = lattrs->ifla_idata;
	struct nlattr_bmask bm;
	int error = 0;

	if ((attrs == NULL) ||
	    (nl_has_attr(ifd->bm, IFLA_LINKINFO) == 0)) {
		error = nl_modify_ifp_generic(ifp, lattrs, ifd->bm, npt);
		return (error);
	}

	error = priv_check(curthread, PRIV_NET_GENEVE);
	if (error)
		return (error);

	/* make sure ignored attributes by nl_parse will not cause panics */
	memset(&params, 0, sizeof(params));

	nl_get_attrs_bmask_raw(NLA_DATA(attrs), NLA_DATA_LEN(attrs), &bm);
	error = nl_parse_nested(attrs, &geneve_modify_parser, npt, &params);

	if (error == 0 && nl_has_attr(&bm, IFLA_GENEVE_ID))
		error = geneve_set_vni_nl(sc, npt, params.ifla_vni);

	if (error == 0 && nl_has_attr(&bm, IFLA_GENEVE_LOCAL))
		error = geneve_set_local_addr_nl(sc, npt, params.ifla_local);

	if (error == 0 && nl_has_attr(&bm, IFLA_GENEVE_REMOTE))
		error = geneve_set_remote_addr_nl(sc, npt, params.ifla_remote);

	if (error == 0 && nl_has_attr(&bm, IFLA_GENEVE_LOCAL_PORT))
		error = geneve_set_local_port_nl(sc, npt, params.ifla_local_port);

	if (error == 0 && nl_has_attr(&bm, IFLA_GENEVE_PORT))
		error = geneve_set_remote_port_nl(sc, npt, params.ifla_remote_port);

	if (error == 0 && nl_has_attr(&bm, IFLA_GENEVE_PORT_RANGE))
		error = geneve_set_port_range_nl(sc, npt, params.ifla_port_range);

	if (error == 0 && nl_has_attr(&bm, IFLA_GENEVE_DF))
		error = geneve_set_df_nl(sc, npt, params.ifla_df);

	if (error == 0 && nl_has_attr(&bm, IFLA_GENEVE_TTL))
		error = geneve_set_ttl_nl(sc, npt, params.ifla_ttl);

	if (error == 0 && nl_has_attr(&bm, IFLA_GENEVE_TTL_INHERIT))
		error = geneve_set_ttl_inherit_nl(sc, npt, params.ifla_ttl_inherit);

	if (error == 0 && nl_has_attr(&bm, IFLA_GENEVE_DSCP_INHERIT))
		error = geneve_set_dscp_inherit_nl(sc, npt, params.ifla_dscp_inherit);

	if (error == 0 && nl_has_attr(&bm, IFLA_GENEVE_COLLECT_METADATA))
		error = geneve_set_collect_metadata_nl(sc, npt, params.ifla_external);

	if (error == 0 && nl_has_attr(&bm, IFLA_GENEVE_FTABLE_LEARN))
		error = geneve_set_learn_nl(sc, npt, params.ifla_ftable_learn);

	if (error == 0 && nl_has_attr(&bm, IFLA_GENEVE_FTABLE_FLUSH))
		error = geneve_flush_ftable_nl(sc, npt, params.ifla_ftable_flush);

	if (error == 0 && nl_has_attr(&bm, IFLA_GENEVE_FTABLE_MAX))
		error = geneve_set_ftable_max_nl(sc, npt, params.ifla_ftable_max);

	if (error == 0 && nl_has_attr(&bm, IFLA_GENEVE_FTABLE_TIMEOUT))
		error = geneve_set_ftable_timeout_nl(sc, npt, params.ifla_ftable_timeout);

	if (error == 0 && nl_has_attr(&bm, IFLA_GENEVE_MC_IFNAME))
		error = geneve_set_mc_if_nl(sc, npt, params.ifla_mc_ifname);

	if (error == 0)
		error = nl_modify_ifp_generic(ifp, lattrs, ifd->bm, npt);

	return (error);
}

static void
geneve_clone_dump_nl(struct ifnet *ifp, struct nl_writer *nw)
{
	struct geneve_softc *sc;
	struct rm_priotracker tracker;
	int off, off2;

	nlattr_add_u32(nw, IFLA_LINK, ifp->if_index);
	nlattr_add_string(nw, IFLA_IFNAME, ifp->if_xname);

	off = nlattr_add_nested(nw, IFLA_LINKINFO);
	if (off == 0)
		return;

	nlattr_add_string(nw, IFLA_INFO_KIND, "geneve");
	off2 = nlattr_add_nested(nw, IFLA_INFO_DATA);
	if (off2 == 0) {
		nlattr_set_len(nw, off);
		return;
	}

	sc = ifp->if_softc;
	GENEVE_RLOCK(sc, &tracker);

	nlattr_add_u32(nw, IFLA_GENEVE_ID, sc->gnv_vni);
	nlattr_add_u16(nw, IFLA_GENEVE_PROTOCOL, sc->gnv_proto);
	geneve_get_local_addr_nl(sc, nw);
	geneve_get_remote_addr_nl(sc, nw);
	nlattr_add_u16(nw, IFLA_GENEVE_LOCAL_PORT, geneve_get_local_port(sc));
	nlattr_add_u16(nw, IFLA_GENEVE_PORT, geneve_get_remote_port(sc));

	const struct ifla_geneve_port_range port_range = {
		.low = sc->gnv_min_port,
		.high = sc->gnv_max_port
	};
	nlattr_add(nw, IFLA_GENEVE_PORT_RANGE, sizeof(port_range), &port_range);

	nlattr_add_u8(nw, IFLA_GENEVE_DF, (uint8_t)sc->gnv_df);
	nlattr_add_u8(nw, IFLA_GENEVE_TTL, sc->gnv_ttl);
	nlattr_add_bool(nw, IFLA_GENEVE_TTL_INHERIT,
	    sc->gnv_flags & GENEVE_FLAG_TTL_INHERIT);
	nlattr_add_bool(nw, IFLA_GENEVE_DSCP_INHERIT,
	    sc->gnv_flags & GENEVE_FLAG_DSCP_INHERIT);
	nlattr_add_bool(nw, IFLA_GENEVE_COLLECT_METADATA,
	    sc->gnv_flags & GENEVE_FLAG_COLLECT_METADATA);

	nlattr_add_bool(nw, IFLA_GENEVE_FTABLE_LEARN,
	    sc->gnv_flags & GENEVE_FLAG_LEARN);
	nlattr_add_u32(nw, IFLA_GENEVE_FTABLE_MAX, sc->gnv_ftable_max);
	nlattr_add_u32(nw, IFLA_GENEVE_FTABLE_TIMEOUT, sc->gnv_ftable_timeout);
	nlattr_add_u32(nw, IFLA_GENEVE_FTABLE_COUNT, sc->gnv_ftable_cnt);
	nlattr_add_u32(nw, IFLA_GENEVE_FTABLE_NOSPACE_CNT, sc->gnv_stats.ftable_nospace);
	nlattr_add_u32(nw, IFLA_GENEVE_FTABLE_LOCK_UP_FAIL_CNT,
	    sc->gnv_stats.ftable_lock_upgrade_failed);

	nlattr_add_string(nw, IFLA_GENEVE_MC_IFNAME, sc->gnv_mc_ifname);
	nlattr_add_u32(nw, IFLA_GENEVE_MC_IFINDEX, sc->gnv_mc_ifindex);

	nlattr_add_u64(nw, IFLA_GENEVE_TXCSUM_CNT,
	    counter_u64_fetch(sc->gnv_stats.txcsum));
	nlattr_add_u64(nw, IFLA_GENEVE_TSO_CNT,
	    counter_u64_fetch(sc->gnv_stats.tso));
	nlattr_add_u64(nw, IFLA_GENEVE_RXCSUM_CNT,
	    counter_u64_fetch(sc->gnv_stats.rxcsum));

	nlattr_set_len(nw, off2);
	nlattr_set_len(nw, off);

	GENEVE_RUNLOCK(sc, &tracker);
}

static int
geneve_clone_create(struct if_clone *ifc, char *name, size_t len,
    struct ifc_data *ifd, struct ifnet **ifpp)
{
	struct geneve_softc *sc;
	struct geneve_params gnvp;
	struct ifnet *ifp;
	int error;

	sc = malloc(sizeof(struct geneve_softc), M_GENEVE, M_WAITOK | M_ZERO);
	sc->gnv_fibnum = curthread->td_proc->p_fibnum;
	geneve_set_default_config(sc);

	if (ifd != NULL) {
		error = ifc_copyin(ifd, &gnvp, sizeof(gnvp));
		if (error != 0 ||
		    (error = geneve_check_proto(gnvp.ifla_proto)) != 0) {
			free(sc, M_GENEVE);
			return (error);
		}

		sc->gnv_proto = gnvp.ifla_proto;
	}

	if (sc->gnv_proto == GENEVE_PROTO_ETHER) {
		ifp = if_alloc(IFT_ETHER);
		ifp->if_flags |= IFF_SIMPLEX | IFF_BROADCAST;
		geneve_ftable_init(sc);
		callout_init_rw(&sc->gnv_callout, &sc->gnv_lock, 0);
	} else if (sc->gnv_proto == GENEVE_PROTO_INHERIT) {
		ifp = if_alloc(IFT_TUNNEL);
		ifp->if_flags |= IFF_NOARP;
	} else {
		free(sc, M_GENEVE);
		return (EINVAL);
	}

	geneve_stats_alloc(sc);
	sc->gnv_ifp = ifp;
	rm_init(&sc->gnv_lock, "geneverm");
	sc->gnv_port_hash_key = arc4random();

	ifp->if_softc = sc;
	if_initname(ifp, geneve_name, ifd->unit);
	ifp->if_flags |= IFF_MULTICAST;
	ifp->if_init = geneve_init;
	ifp->if_ioctl = geneve_ioctl;
	ifp->if_transmit = geneve_transmit;
	ifp->if_qflush = geneve_qflush;
	ifp->if_capabilities = GENEVE_BASIC_IFCAPS;
	ifp->if_capenable = GENEVE_BASIC_IFCAPS;
	sc->gnv_reqcap = -1;
	geneve_set_hwcaps(sc);

	if (sc->gnv_proto == GENEVE_PROTO_ETHER) {
		ifmedia_init(&sc->gnv_media, 0, geneve_media_change, geneve_media_status);
		ifmedia_add(&sc->gnv_media, IFM_ETHER | IFM_AUTO, 0, NULL);
		ifmedia_set(&sc->gnv_media, IFM_ETHER | IFM_AUTO);

		ether_gen_addr(ifp, &sc->gnv_hwaddr);
		ether_ifattach(ifp, sc->gnv_hwaddr.octet);

		ifp->if_baudrate = 0;
	} else {
		ifp->if_output = geneve_output;

		if_attach(ifp);
		bpfattach(ifp, DLT_NULL, sizeof(u_int32_t));
	}

	GENEVE_WLOCK(sc);
	geneve_setup_interface_hdrlen(sc);
	GENEVE_WUNLOCK(sc);
	*ifpp = ifp;

	return (0);
}

static int
geneve_clone_destroy(struct if_clone *ifc, struct ifnet *ifp, uint32_t flags)
{
	struct geneve_softc *sc;

	sc = if_getsoftc(ifp);
	geneve_teardown(sc);

	if (sc->gnv_proto == GENEVE_PROTO_ETHER) {
		geneve_ftable_flush(sc, 1);

		ether_ifdetach(ifp);
		if_free(ifp);
		ifmedia_removeall(&sc->gnv_media);

		geneve_ftable_fini(sc);
	} else {
		bpfdetach(ifp);
		if_detach(ifp);
		if_free(ifp);
	}

	rm_destroy(&sc->gnv_lock);
	geneve_stats_free(sc);
	free(sc, M_GENEVE);

	return (0);
}

/* BMV: Taken from if_bridge. */
static uint32_t
geneve_mac_hash(struct geneve_softc *sc, const uint8_t *addr)
{
	uint32_t a = 0x9e3779b9, b = 0x9e3779b9, c = sc->gnv_ftable_hash_key;

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

static int
geneve_media_change(struct ifnet *ifp)
{

	/* Ignore. */
	return (0);
}

static void
geneve_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{

	ifmr->ifm_status = IFM_ACTIVE | IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER | IFM_FDX;
}

static int
geneve_sockaddr_cmp(const union sockaddr_union *unsa,
    const struct sockaddr *sa)
{

	return (memcmp(&unsa->sa, sa, unsa->sa.sa_len));
}

static void
geneve_sockaddr_copy(union sockaddr_union *dst,
    const struct sockaddr *sa)
{

	MPASS(sa->sa_family == AF_INET || sa->sa_family == AF_INET6);
	memset(dst, 0, sizeof(*dst));

	if (sa->sa_family == AF_INET) {
		dst->sin = *SATOCONSTSIN(sa);
		dst->sin.sin_len = sizeof(struct sockaddr_in);
	} else if (sa->sa_family == AF_INET6) {
		dst->sin6 = *SATOCONSTSIN6(sa);
		dst->sin6.sin6_len = sizeof(struct sockaddr_in6);
	}
}

static int
geneve_sockaddr_in_equal(const union sockaddr_union *unsa,
    const struct sockaddr *sa)
{
	int equal;

	if (sa->sa_family == AF_INET) {
		const struct in_addr *in4 = &SATOCONSTSIN(sa)->sin_addr;
		equal = in4->s_addr == unsa->sin.sin_addr.s_addr;
	} else if (sa->sa_family == AF_INET6) {
		const struct in6_addr *in6 = &SATOCONSTSIN6(sa)->sin6_addr;
		equal = IN6_ARE_ADDR_EQUAL(in6, &unsa->sin6.sin6_addr);
	} else
		equal = 0;

	return (equal);
}

static void
geneve_sockaddr_in_copy(union sockaddr_union *dst,
    const struct sockaddr *sa)
{

	MPASS(sa->sa_family == AF_INET || sa->sa_family == AF_INET6);

	if (sa->sa_family == AF_INET) {
		const struct in_addr *in4 = &SATOCONSTSIN(sa)->sin_addr;
		dst->sin.sin_family = AF_INET;
		dst->sin.sin_len = sizeof(struct sockaddr_in);
		dst->sin.sin_addr = *in4;
	} else if (sa->sa_family == AF_INET6) {
		const struct in6_addr *in6 = &SATOCONSTSIN6(sa)->sin6_addr;
		dst->sin6.sin6_family = AF_INET6;
		dst->sin6.sin6_len = sizeof(struct sockaddr_in6);
		dst->sin6.sin6_addr = *in6;
	}
}

static int
geneve_sockaddr_supported(const union sockaddr_union *gnvaddr, int unspec)
{
	const struct sockaddr *sa;
	int supported;

	sa = &gnvaddr->sa;
	supported = 0;

	if (sa->sa_family == AF_UNSPEC && unspec != 0) {
		supported = 1;
	} else if (sa->sa_family == AF_INET) {
		supported = 1;
	} else if (sa->sa_family == AF_INET6) {
		supported = 1;
	}

	return (supported);
}

static int
geneve_sockaddr_in_any(const union sockaddr_union *gnvaddr)
{
	const struct sockaddr *sa;
	int any;

	sa = &gnvaddr->sa;

	if (sa->sa_family == AF_INET) {
		const struct in_addr *in4 = &SATOCONSTSIN(sa)->sin_addr;
		any = in4->s_addr == INADDR_ANY;
	} else if (sa->sa_family == AF_INET6) {
		const struct in6_addr *in6 = &SATOCONSTSIN6(sa)->sin6_addr;
		any = IN6_IS_ADDR_UNSPECIFIED(in6);
	} else
		any = -1;

	return (any);
}

static int
geneve_can_change_config(struct geneve_softc *sc)
{

	GENEVE_LOCK_ASSERT(sc);

	if (sc->gnv_flags & GENEVE_FLAG_RUNNING)
		return (0);
	if (sc->gnv_flags & (GENEVE_FLAG_INIT | GENEVE_FLAG_TEARDOWN))
		return (0);
	if (sc->gnv_flags & GENEVE_FLAG_COLLECT_METADATA)
		return (0);

	return (1);
}

static int
geneve_check_proto(uint16_t proto)
{
	int error;

	switch (proto) {
	case GENEVE_PROTO_ETHER:
	case GENEVE_PROTO_INHERIT:
		error = 0;
		break;

	default:
		error = EAFNOSUPPORT;
		break;
	}

	return (error);
}

static int
geneve_check_multicast_addr(const union sockaddr_union *sa)
{
	int mc;

	if (sa->sa.sa_family == AF_INET) {
		const struct in_addr *in4 = &SATOCONSTSIN(sa)->sin_addr;
		mc = IN_MULTICAST(ntohl(in4->s_addr));
	} else if (sa->sa.sa_family == AF_INET6) {
		const struct in6_addr *in6 = &SATOCONSTSIN6(sa)->sin6_addr;
		mc = IN6_IS_ADDR_MULTICAST(in6);
	} else
		mc = EINVAL;

	return (mc);
}

static int
geneve_check_sockaddr(const union sockaddr_union *sa, const int len)
{
	int error;

	error = 0;
	switch (sa->sa.sa_family) {
	case AF_INET:
	case AF_INET6:
		if (len < sizeof(struct sockaddr))
			error = EINVAL;
		break;

	default:
		error = EAFNOSUPPORT;
	}

	return (error);
}

static int
geneve_prison_remove(void *obj, void *data __unused)
{
#ifdef VIMAGE
	struct prison *pr;

	pr = obj;
	if (prison_owns_vnet(pr)) {
		CURVNET_SET(pr->pr_vnet);
		if (V_geneve_cloner != NULL) {
			ifc_detach_cloner(V_geneve_cloner);
			V_geneve_cloner = NULL;
		}
		CURVNET_RESTORE();
	}
#endif
	return (0);
}

static void
vnet_geneve_load(void)
{
	struct if_clone_addreq_v2 req = {
		.version = 2,
		.flags = IFC_F_AUTOUNIT,
		.match_f = NULL,
		.create_f = geneve_clone_create,
		.destroy_f = geneve_clone_destroy,
		.create_nl_f = geneve_clone_create_nl,
		.modify_nl_f = geneve_clone_modify_nl,
		.dump_nl_f = geneve_clone_dump_nl,
	};
	V_geneve_cloner = ifc_attach_cloner(geneve_name, (struct if_clone_addreq *)&req);
}
VNET_SYSINIT(vnet_geneve_load, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY, vnet_geneve_load, NULL);

static void
vnet_geneve_unload(void)
{

	if (V_geneve_cloner != NULL)
		ifc_detach_cloner(V_geneve_cloner);
}
VNET_SYSUNINIT(vnet_geneve_unload, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY, vnet_geneve_unload, NULL);

static void
geneve_module_init(void)
{
	mtx_init(&geneve_list_mtx, "geneve list", NULL, MTX_DEF);
	osd_method_t methods[PR_MAXMETHOD] = {
		[PR_METHOD_REMOVE] = geneve_prison_remove,
	};

	geneve_osd_jail_slot = osd_jail_register(NULL, methods);
	NL_VERIFY_PARSERS(all_parsers);
}

static void
geneve_module_deinit(void)
{
	struct if_clone *clone;
	VNET_ITERATOR_DECL(vnet_iter);

	VNET_LIST_RLOCK();
	VNET_FOREACH(vnet_iter) {
		clone = VNET_VNET(vnet_iter, geneve_cloner);
		if (clone != NULL) {
			ifc_detach_cloner(clone);
			VNET_VNET(vnet_iter, geneve_cloner) = NULL;
		}
	}
	VNET_LIST_RUNLOCK();
	NET_EPOCH_WAIT();
	MPASS(LIST_EMPTY(&geneve_socket_list));
	mtx_destroy(&geneve_list_mtx);
	if (geneve_osd_jail_slot != 0)
		osd_jail_deregister(geneve_osd_jail_slot);
}

static int
geneve_modevent(module_t mod, int type, void *unused)
{
	int error;

	error = 0;

	switch (type) {
	case MOD_LOAD:
		geneve_module_init();
		break;

	case MOD_UNLOAD:
		geneve_module_deinit();
		break;

	default:
		error = ENOTSUP;
		break;
	}

	return (error);
}

static moduledata_t geneve_mod = {
	"if_geneve",
	geneve_modevent,
	0
};

DECLARE_MODULE(if_geneve, geneve_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(if_geneve, 1);
