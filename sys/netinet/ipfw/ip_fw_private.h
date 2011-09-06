/*-
 * Copyright (c) 2002-2009 Luigi Rizzo, Universita` di Pisa
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
 *
 * $FreeBSD$
 */

#ifndef _IPFW2_PRIVATE_H
#define _IPFW2_PRIVATE_H

/*
 * Internal constants and data structures used by ipfw components
 * and not meant to be exported outside the kernel.
 */

#ifdef _KERNEL

/*
 * For platforms that do not have SYSCTL support, we wrap the
 * SYSCTL_* into a function (one per file) to collect the values
 * into an array at module initialization. The wrapping macros,
 * SYSBEGIN() and SYSEND, are empty in the default case.
 */
#ifndef SYSBEGIN
#define SYSBEGIN(x)
#endif
#ifndef SYSEND
#define SYSEND
#endif

/* Return values from ipfw_chk() */
enum {
	IP_FW_PASS = 0,
	IP_FW_DENY,
	IP_FW_DIVERT,
	IP_FW_TEE,
	IP_FW_DUMMYNET,
	IP_FW_NETGRAPH,
	IP_FW_NGTEE,
	IP_FW_NAT,
	IP_FW_REASS,
};

/*
 * Structure for collecting parameters to dummynet for ip6_output forwarding
 */
struct _ip6dn_args {
       struct ip6_pktopts *opt_or;
       struct route_in6 ro_or;
       int flags_or;
       struct ip6_moptions *im6o_or;
       struct ifnet *origifp_or;
       struct ifnet *ifp_or;
       struct sockaddr_in6 dst_or;
       u_long mtu_or;
       struct route_in6 ro_pmtu_or;
};


/*
 * Arguments for calling ipfw_chk() and dummynet_io(). We put them
 * all into a structure because this way it is easier and more
 * efficient to pass variables around and extend the interface.
 */
struct ip_fw_args {
	struct mbuf	*m;		/* the mbuf chain		*/
	struct ifnet	*oif;		/* output interface		*/
	struct sockaddr_in *next_hop;	/* forward address		*/
	struct sockaddr_in6 *next_hop6; /* ipv6 forward address		*/

	/*
	 * On return, it points to the matching rule.
	 * On entry, rule.slot > 0 means the info is valid and
	 * contains the starting rule for an ipfw search.
	 * If chain_id == chain->id && slot >0 then jump to that slot.
	 * Otherwise, we locate the first rule >= rulenum:rule_id
	 */
	struct ipfw_rule_ref rule;	/* match/restart info		*/

	struct ether_header *eh;	/* for bridged packets		*/

	struct ipfw_flow_id f_id;	/* grabbed from IP header	*/
	//uint32_t	cookie;		/* a cookie depending on rule action */
	struct inpcb	*inp;

	struct _ip6dn_args	dummypar; /* dummynet->ip6_output */
	struct sockaddr_in hopstore;	/* store here if cannot use a pointer */
};

MALLOC_DECLARE(M_IPFW);

/*
 * Hooks sometime need to know the direction of the packet
 * (divert, dummynet, netgraph, ...)
 * We use a generic definition here, with bit0-1 indicating the
 * direction, bit 2 indicating layer2 or 3, bit 3-4 indicating the
 * specific protocol
 * indicating the protocol (if necessary)
 */
enum {
	DIR_MASK =	0x3,
	DIR_OUT =	0,
	DIR_IN =	1,
	DIR_FWD =	2,
	DIR_DROP =	3,
	PROTO_LAYER2 =	0x4, /* set for layer 2 */
	/* PROTO_DEFAULT = 0, */
	PROTO_IPV4 =	0x08,
	PROTO_IPV6 =	0x10,
	PROTO_IFB =	0x0c, /* layer2 + ifbridge */
   /*	PROTO_OLDBDG =	0x14, unused, old bridge */
};

/* wrapper for freeing a packet, in case we need to do more work */
#ifndef FREE_PKT
#if defined(__linux__) || defined(_WIN32)
#define FREE_PKT(m)	netisr_dispatch(-1, m)
#else
#define FREE_PKT(m)	m_freem(m)
#endif
#endif /* !FREE_PKT */

/*
 * Function definitions.
 */

/* attach (arg = 1) or detach (arg = 0) hooks */
int ipfw_attach_hooks(int);
#ifdef NOTYET
void ipfw_nat_destroy(void);
#endif

/* In ip_fw_log.c */
struct ip;
void ipfw_log_bpf(int);
void ipfw_log(struct ip_fw *f, u_int hlen, struct ip_fw_args *args,
	struct mbuf *m, struct ifnet *oif, u_short offset, uint32_t tablearg,
	struct ip *ip);
VNET_DECLARE(u_int64_t, norule_counter);
#define	V_norule_counter	VNET(norule_counter)
VNET_DECLARE(int, verbose_limit);
#define	V_verbose_limit		VNET(verbose_limit)

/* In ip_fw_dynamic.c */

enum { /* result for matching dynamic rules */
	MATCH_REVERSE = 0,
	MATCH_FORWARD,
	MATCH_NONE,
	MATCH_UNKNOWN,
};

/*
 * The lock for dynamic rules is only used once outside the file,
 * and only to release the result of lookup_dyn_rule().
 * Eventually we may implement it with a callback on the function.
 */
void ipfw_dyn_unlock(void);

struct tcphdr;
struct mbuf *ipfw_send_pkt(struct mbuf *, struct ipfw_flow_id *,
    u_int32_t, u_int32_t, int);
int ipfw_install_state(struct ip_fw *rule, ipfw_insn_limit *cmd,
    struct ip_fw_args *args, uint32_t tablearg);
ipfw_dyn_rule *ipfw_lookup_dyn_rule(struct ipfw_flow_id *pkt,
	int *match_direction, struct tcphdr *tcp);
void ipfw_remove_dyn_children(struct ip_fw *rule);
void ipfw_get_dynamic(char **bp, const char *ep);

void ipfw_dyn_attach(void);	/* uma_zcreate .... */
void ipfw_dyn_detach(void);	/* uma_zdestroy ... */
void ipfw_dyn_init(void);	/* per-vnet initialization */
void ipfw_dyn_uninit(int);	/* per-vnet deinitialization */
int ipfw_dyn_len(void);

/* common variables */
VNET_DECLARE(int, fw_one_pass);
#define	V_fw_one_pass		VNET(fw_one_pass)

VNET_DECLARE(int, fw_verbose);
#define	V_fw_verbose		VNET(fw_verbose)

VNET_DECLARE(struct ip_fw_chain, layer3_chain);
#define	V_layer3_chain		VNET(layer3_chain)

VNET_DECLARE(u_int32_t, set_disable);
#define	V_set_disable		VNET(set_disable)

VNET_DECLARE(int, autoinc_step);
#define V_autoinc_step		VNET(autoinc_step)

struct ip_fw_chain {
	struct ip_fw	*rules;		/* list of rules */
	struct ip_fw	*reap;		/* list of rules to reap */
	struct ip_fw	*default_rule;
	int		n_rules;	/* number of static rules */
	int		static_len;	/* total len of static rules */
	struct ip_fw	**map;		/* array of rule ptrs to ease lookup */
	LIST_HEAD(nat_list, cfg_nat) nat;       /* list of nat entries */
	struct radix_node_head *tables[IPFW_TABLES_MAX];
#if defined( __linux__ ) || defined( _WIN32 )
	spinlock_t rwmtx;
	spinlock_t uh_lock;
#else
	struct rwlock	rwmtx;
	struct rwlock	uh_lock;	/* lock for upper half */
#endif
	uint32_t	id;		/* ruleset id */
	uint32_t	gencnt;		/* generation count */
};

struct sockopt;	/* used by tcp_var.h */

/*
 * The lock is heavily used by ip_fw2.c (the main file) and ip_fw_nat.c
 * so the variable and the macros must be here.
 */

#define	IPFW_LOCK_INIT(_chain) do {			\
	rw_init(&(_chain)->rwmtx, "IPFW static rules");	\
	rw_init(&(_chain)->uh_lock, "IPFW UH lock");	\
	} while (0)

#define	IPFW_LOCK_DESTROY(_chain) do {			\
	rw_destroy(&(_chain)->rwmtx);			\
	rw_destroy(&(_chain)->uh_lock);			\
	} while (0)

#define	IPFW_WLOCK_ASSERT(_chain)	rw_assert(&(_chain)->rwmtx, RA_WLOCKED)

#define IPFW_RLOCK(p) rw_rlock(&(p)->rwmtx)
#define IPFW_RUNLOCK(p) rw_runlock(&(p)->rwmtx)
#define IPFW_WLOCK(p) rw_wlock(&(p)->rwmtx)
#define IPFW_WUNLOCK(p) rw_wunlock(&(p)->rwmtx)

#define IPFW_UH_RLOCK(p) rw_rlock(&(p)->uh_lock)
#define IPFW_UH_RUNLOCK(p) rw_runlock(&(p)->uh_lock)
#define IPFW_UH_WLOCK(p) rw_wlock(&(p)->uh_lock)
#define IPFW_UH_WUNLOCK(p) rw_wunlock(&(p)->uh_lock)

/* In ip_fw_sockopt.c */
int ipfw_find_rule(struct ip_fw_chain *chain, uint32_t key, uint32_t id);
int ipfw_add_rule(struct ip_fw_chain *chain, struct ip_fw *input_rule);
int ipfw_ctl(struct sockopt *sopt);
int ipfw_chk(struct ip_fw_args *args);
void ipfw_reap_rules(struct ip_fw *head);

/* In ip_fw_pfil */
int ipfw_check_hook(void *arg, struct mbuf **m0, struct ifnet *ifp, int dir,
     struct inpcb *inp);

/* In ip_fw_table.c */
struct radix_node;
int ipfw_lookup_table(struct ip_fw_chain *ch, uint16_t tbl, in_addr_t addr,
    uint32_t *val);
int ipfw_init_tables(struct ip_fw_chain *ch);
void ipfw_destroy_tables(struct ip_fw_chain *ch);
int ipfw_flush_table(struct ip_fw_chain *ch, uint16_t tbl);
int ipfw_add_table_entry(struct ip_fw_chain *ch, uint16_t tbl, in_addr_t addr,
    uint8_t mlen, uint32_t value);
int ipfw_dump_table_entry(struct radix_node *rn, void *arg);
int ipfw_del_table_entry(struct ip_fw_chain *ch, uint16_t tbl, in_addr_t addr,
    uint8_t mlen);
int ipfw_count_table(struct ip_fw_chain *ch, uint32_t tbl, uint32_t *cnt);
int ipfw_dump_table(struct ip_fw_chain *ch, ipfw_table *tbl);

/* In ip_fw_nat.c -- XXX to be moved to ip_var.h */

extern struct cfg_nat *(*lookup_nat_ptr)(struct nat_list *, int);

typedef int ipfw_nat_t(struct ip_fw_args *, struct cfg_nat *, struct mbuf *);
typedef int ipfw_nat_cfg_t(struct sockopt *);

extern ipfw_nat_t *ipfw_nat_ptr;
#define IPFW_NAT_LOADED (ipfw_nat_ptr != NULL)

extern ipfw_nat_cfg_t *ipfw_nat_cfg_ptr;
extern ipfw_nat_cfg_t *ipfw_nat_del_ptr;
extern ipfw_nat_cfg_t *ipfw_nat_get_cfg_ptr;
extern ipfw_nat_cfg_t *ipfw_nat_get_log_ptr;

#endif /* _KERNEL */
#endif /* _IPFW2_PRIVATE_H */
