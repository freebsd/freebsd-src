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

#define MTAG_IPFW	1148380143	/* IPFW-tagged cookie */

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

/* flags for divert mtag */
#define	IP_FW_DIVERT_LOOPBACK_FLAG	0x00080000
#define	IP_FW_DIVERT_OUTPUT_FLAG	0x00100000

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
	struct ip_fw	*rule;		/* matching rule		*/
	uint32_t	rule_id;	/* matching rule id */
	uint32_t	chain_id;	/* ruleset id */
	struct ether_header *eh;	/* for bridged packets		*/

	struct ipfw_flow_id f_id;	/* grabbed from IP header	*/
	uint32_t	cookie;		/* a cookie depending on rule action */
	struct inpcb	*inp;

	struct _ip6dn_args	dummypar; /* dummynet->ip6_output */
	struct sockaddr_in hopstore;	/* store here if cannot use a pointer */
};

MALLOC_DECLARE(M_IPFW);

/*
 * Function definitions.
 */

/* Firewall hooks */

int ipfw_check_in(void *, struct mbuf **, struct ifnet *, int, struct inpcb *inp);
int ipfw_check_out(void *, struct mbuf **, struct ifnet *, int, struct inpcb *inp);

int ipfw_chk(struct ip_fw_args *);

int ipfw_hook(void);
int ipfw6_hook(void);
int ipfw_unhook(void);
int ipfw6_unhook(void);
#ifdef NOTYET
void ipfw_nat_destroy(void);
#endif

/* In ip_fw_log.c */
struct ip;
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
struct mbuf *send_pkt(struct mbuf *, struct ipfw_flow_id *,
    u_int32_t, u_int32_t, int);
int install_state(struct ip_fw *rule, ipfw_insn_limit *cmd,
    struct ip_fw_args *args, uint32_t tablearg);
ipfw_dyn_rule * lookup_dyn_rule_locked(struct ipfw_flow_id *pkt, int *match_direction,
    struct tcphdr *tcp);
ipfw_dyn_rule * lookup_dyn_rule(struct ipfw_flow_id *pkt, int *match_direction,
    struct tcphdr *tcp);
void remove_dyn_children(struct ip_fw *rule);
void ipfw_get_dynamic(char **bp, const char *ep);

void ipfw_dyn_attach(void);	/* uma_zcreate .... */
void ipfw_dyn_detach(void);	/* uma_zdestroy ... */
void ipfw_dyn_init(void);	/* per-vnet initialization */
void ipfw_dyn_uninit(int);	/* per-vnet deinitialization */
int ipfw_dyn_len(void);

/* common variables */
VNET_DECLARE(int, fw_one_pass);
VNET_DECLARE(int, fw_enable);
VNET_DECLARE(int, fw_verbose);
VNET_DECLARE(struct ip_fw_chain, layer3_chain);

#define	V_fw_one_pass		VNET(fw_one_pass)
#define	V_fw_enable		VNET(fw_enable)
#define	V_fw_verbose		VNET(fw_enable)
#define	V_layer3_chain		VNET(layer3_chain)

#ifdef INET6
VNET_DECLARE(int, fw6_enable);
#define	V_fw6_enable		VNET(fw6_enable)
#endif

struct ip_fw_chain {
	struct ip_fw	*rules;		/* list of rules */
	struct ip_fw	*reap;		/* list of rules to reap */
	LIST_HEAD(nat_list, cfg_nat) nat;       /* list of nat entries */
	struct radix_node_head *tables[IPFW_TABLES_MAX];
	struct rwlock	rwmtx;
	uint32_t	id;		/* ruleset id */
};

struct sockopt;	/* used by tcp_var.h */

/*
 * The lock is heavily used by ip_fw2.c (the main file) and ip_fw_nat.c
 * so the variable and the macros must be here.
 */

#define	IPFW_LOCK_INIT(_chain) \
	rw_init(&(_chain)->rwmtx, "IPFW static rules")
#define	IPFW_LOCK_DESTROY(_chain)	rw_destroy(&(_chain)->rwmtx)
#define	IPFW_WLOCK_ASSERT(_chain)	rw_assert(&(_chain)->rwmtx, RA_WLOCKED)

#define IPFW_RLOCK(p) rw_rlock(&(p)->rwmtx)
#define IPFW_RUNLOCK(p) rw_runlock(&(p)->rwmtx)
#define IPFW_WLOCK(p) rw_wlock(&(p)->rwmtx)
#define IPFW_WUNLOCK(p) rw_wunlock(&(p)->rwmtx)

/* In ip_fw_nat.c */
extern struct cfg_nat *(*lookup_nat_ptr)(struct nat_list *, int);

typedef int ipfw_nat_t(struct ip_fw_args *, struct cfg_nat *, struct mbuf *);
typedef int ipfw_nat_cfg_t(struct sockopt *);

#endif /* _KERNEL */
#endif /* _IPFW2_PRIVATE_H */
