/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Rubicon Communications, LLC (Netgate)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _PFCTL_IOCTL_H_
#define _PFCTL_IOCTL_H_

#include <netpfil/pf/pf.h>

struct pfctl_anchor;
struct pfctl_eth_anchor;

struct pfctl_status_counter {
	uint64_t	 id;
	uint64_t	 counter;
	char		*name;

	TAILQ_ENTRY(pfctl_status_counter) entry;
};
TAILQ_HEAD(pfctl_status_counters, pfctl_status_counter);

struct pfctl_status {
	bool		running;
	uint32_t	since;
	uint32_t	debug;
	uint32_t	hostid;
	uint64_t	states;
	uint64_t	src_nodes;
	char		ifname[IFNAMSIZ];
	uint8_t		pf_chksum[PF_MD5_DIGEST_LENGTH];
	bool		syncookies_active;
	uint32_t	reass;

	struct pfctl_status_counters	 counters;
	struct pfctl_status_counters	 lcounters;
	struct pfctl_status_counters	 fcounters;
	struct pfctl_status_counters	 scounters;
	uint64_t	pcounters[2][2][2];
	uint64_t	bcounters[2][2];
};

struct pfctl_eth_rulesets_info {
	uint32_t	nr;
};

struct pfctl_eth_rules_info {
	uint32_t	nr;
	uint32_t	ticket;
};

struct pfctl_eth_addr {
	uint8_t	addr[ETHER_ADDR_LEN];
	uint8_t	mask[ETHER_ADDR_LEN];
	bool	neg;
	bool	isset;
};

struct pfctl_eth_rule {
	uint32_t		 nr;

	char			label[PF_RULE_MAX_LABEL_COUNT][PF_RULE_LABEL_SIZE];
	uint32_t		ridentifier;

	bool			 quick;

	/* Filter */
	char			 ifname[IFNAMSIZ];
	uint8_t			 ifnot;
	uint8_t			 direction;
	uint16_t		 proto;
	struct pfctl_eth_addr	 src, dst;
	struct pf_rule_addr	 ipsrc, ipdst;
	char			 match_tagname[PF_TAG_NAME_SIZE];
	uint16_t		 match_tag;
	bool			 match_tag_not;

	/* Stats */
	uint64_t		 evaluations;
	uint64_t		 packets[2];
	uint64_t		 bytes[2];
	time_t			 last_active_timestamp;

	/* Action */
	char			 qname[PF_QNAME_SIZE];
	char			 tagname[PF_TAG_NAME_SIZE];
	uint16_t		 dnpipe;
	uint32_t		 dnflags;
	char			 bridge_to[IFNAMSIZ];
	uint8_t			 action;

	struct pfctl_eth_anchor	*anchor;
	uint8_t			 anchor_relative;
	uint8_t			 anchor_wildcard;

	TAILQ_ENTRY(pfctl_eth_rule)	 entries;
};
TAILQ_HEAD(pfctl_eth_rules, pfctl_eth_rule);

struct pfctl_eth_ruleset_info {
	uint32_t	nr;
	char		name[PF_ANCHOR_NAME_SIZE];
	char		path[MAXPATHLEN];
};

struct pfctl_eth_ruleset {
	struct pfctl_eth_rules	 rules;
	struct pfctl_eth_anchor	*anchor;
};

struct pfctl_eth_anchor {
	struct pfctl_eth_anchor		*parent;
	char				 name[PF_ANCHOR_NAME_SIZE];
	char				 path[MAXPATHLEN];
	struct pfctl_eth_ruleset	 ruleset;
	int				 refcnt;	/* anchor rules */
	int				 match;	/* XXX: used for pfctl black magic */
};

struct pfctl_pool {
	struct pf_palist	 list;
	struct pf_pooladdr	*cur;
	struct pf_poolhashkey	 key;
	struct pf_addr		 counter;
	struct pf_mape_portset	 mape;
	int			 tblidx;
	uint16_t		 proxy_port[2];
	uint8_t			 opts;
};

struct pfctl_rules_info {
	uint32_t	nr;
	uint32_t	ticket;
};

struct pfctl_rule {
	struct pf_rule_addr	 src;
	struct pf_rule_addr	 dst;
	union pf_rule_ptr	 skip[PF_SKIP_COUNT];
	char			 label[PF_RULE_MAX_LABEL_COUNT][PF_RULE_LABEL_SIZE];
	uint32_t		 ridentifier;
	char			 ifname[IFNAMSIZ];
	char			 qname[PF_QNAME_SIZE];
	char			 pqname[PF_QNAME_SIZE];
	char			 tagname[PF_TAG_NAME_SIZE];
	char			 match_tagname[PF_TAG_NAME_SIZE];

	char			 overload_tblname[PF_TABLE_NAME_SIZE];

	TAILQ_ENTRY(pfctl_rule)	 entries;
	struct pfctl_pool	 nat;
	union {
		/* Alias old and new names. */
		struct pfctl_pool	 rpool;
		struct pfctl_pool	 rdr;
	};
	struct pfctl_pool	 route;

	uint64_t		 evaluations;
	uint64_t		 packets[2];
	uint64_t		 bytes[2];
	time_t			 last_active_timestamp;

	struct pfi_kif		*kif;
	struct pfctl_anchor	*anchor;
	struct pfr_ktable	*overload_tbl;

	pf_osfp_t		 os_fingerprint;

	int			 rtableid;
	uint32_t		 timeout[PFTM_MAX];
	uint32_t		 max_states;
	uint32_t		 max_src_nodes;
	uint32_t		 max_src_states;
	uint32_t		 max_src_conn;
	struct {
		uint32_t		limit;
		uint32_t		seconds;
	}			 max_src_conn_rate;
	uint32_t		 qid;
	uint32_t		 pqid;
	uint16_t		 dnpipe;
	uint16_t		 dnrpipe;
	uint32_t		 free_flags;
	uint32_t		 nr;
	uint32_t		 prob;
	uid_t			 cuid;
	pid_t			 cpid;

	uint64_t		 states_cur;
	uint64_t		 states_tot;
	uint64_t		 src_nodes;
	uint64_t		 src_nodes_type[PF_SN_MAX];

	uint16_t		 return_icmp;
	uint16_t		 return_icmp6;
	uint16_t		 max_mss;
	uint16_t		 tag;
	uint16_t		 match_tag;
	uint16_t		 scrub_flags;

	struct pf_rule_uid	 uid;
	struct pf_rule_gid	 gid;
	char			 rcv_ifname[IFNAMSIZ];
	bool			 rcvifnot;

	uint32_t		 rule_flag;
	uint8_t			 action;
	uint8_t			 direction;
	uint8_t			 log;
	uint8_t			 logif;
	uint8_t			 quick;
	uint8_t			 ifnot;
	uint8_t			 match_tag_not;
	uint8_t			 natpass;

	uint8_t			 keep_state;
	sa_family_t		 af;
	uint8_t			 proto;
	uint8_t			 type;
	uint8_t			 code;
	uint8_t			 flags;
	uint8_t			 flagset;
	uint8_t			 min_ttl;
	uint8_t			 allow_opts;
	uint8_t			 rt;
	uint8_t			 return_ttl;
	uint8_t			 tos;
	uint8_t			 set_tos;
	uint8_t			 anchor_relative;
	uint8_t			 anchor_wildcard;

	uint8_t			 flush;
	uint8_t			 prio;
	uint8_t			 set_prio[2];
	sa_family_t		 naf;

	struct {
		struct pf_addr		addr;
		uint16_t		port;
	}			divert;
};

TAILQ_HEAD(pfctl_rulequeue, pfctl_rule);

struct pfctl_ruleset {
	struct {
		struct pfctl_rulequeue	 queues[2];
		struct {
			struct pfctl_rulequeue	*ptr;
			struct pfctl_rule	**ptr_array;
			uint32_t		 rcount;
			uint32_t		 ticket;
			int			 open;
		}			 active, inactive;
	}			 rules[PF_RULESET_MAX];
	struct pfctl_anchor	*anchor;
	uint32_t		 tticket;
	int			 tables;
	int			 topen;
};

RB_HEAD(pfctl_anchor_global, pfctl_anchor);
RB_HEAD(pfctl_anchor_node, pfctl_anchor);
struct pfctl_anchor {
	RB_ENTRY(pfctl_anchor)	 entry_global;
	RB_ENTRY(pfctl_anchor)	 entry_node;
	struct pfctl_anchor	*parent;
	struct pfctl_anchor_node children;
	char			 name[PF_ANCHOR_NAME_SIZE];
	char			 path[MAXPATHLEN];
	struct pfctl_ruleset	 ruleset;
	int			 refcnt;	/* anchor rules */
	int			 match;	/* XXX: used for pfctl black magic */
};
RB_PROTOTYPE(pfctl_anchor_global, pfctl_anchor, entry_global,
    pf_anchor_compare);
RB_PROTOTYPE(pfctl_anchor_node, pfctl_anchor, entry_node,
    pf_anchor_compare);

struct pfctl_state_cmp {
	uint64_t	id;
	uint32_t	creatorid;
	uint8_t		direction;
};

struct pfctl_kill {
	struct pfctl_state_cmp	cmp;
	sa_family_t		af;
	int			proto;
	struct pf_rule_addr	src;
	struct pf_rule_addr	dst;
	struct pf_rule_addr	rt_addr;
	char			ifname[IFNAMSIZ];
	char			label[PF_RULE_LABEL_SIZE];
	bool			kill_match;
	bool			nat;
};

struct pfctl_state_peer {
	uint32_t			 seqlo;
	uint32_t			 seqhi;
	uint32_t			 seqdiff;
	uint8_t				 state;
	uint8_t				 wscale;
};

struct pfctl_state_key {
	struct pf_addr	 addr[2];
	uint16_t	 port[2];
	sa_family_t	 af;
	uint8_t	 	 proto;
};

struct pfctl_state {
	TAILQ_ENTRY(pfctl_state)	entry;

	uint64_t		 id;
	uint32_t		 creatorid;
	uint8_t		 	 direction;

	struct pfctl_state_peer	 src;
	struct pfctl_state_peer	 dst;

	uint32_t		 rule;
	uint32_t		 anchor;
	uint32_t		 nat_rule;
	struct pf_addr		 rt_addr;
	struct pfctl_state_key	 key[2];	/* addresses stack and wire  */
	char			 ifname[IFNAMSIZ];
	char			 orig_ifname[IFNAMSIZ];
	uint64_t		 packets[2];
	uint64_t		 bytes[2];
	uint32_t		 creation;
	uint32_t		 expire;
	uint32_t		 pfsync_time;
	uint16_t		 state_flags;
	uint32_t		 sync_flags;
	uint16_t		 qid;
	uint16_t		 pqid;
	uint16_t		 dnpipe;
	uint16_t		 dnrpipe;
	uint8_t			 log;
	int32_t			 rtableid;
	uint8_t			 min_ttl;
	uint8_t			 set_tos;
	uint16_t		 max_mss;
	uint8_t			 set_prio[2];
	uint8_t			 rt;
	char			 rt_ifname[IFNAMSIZ];
	uint8_t			 src_node_flags;
};

TAILQ_HEAD(pfctl_statelist, pfctl_state);
struct pfctl_states {
	struct pfctl_statelist	states;
};

enum pfctl_syncookies_mode {
	PFCTL_SYNCOOKIES_NEVER,
	PFCTL_SYNCOOKIES_ALWAYS,
	PFCTL_SYNCOOKIES_ADAPTIVE
};
extern const char* PFCTL_SYNCOOKIES_MODE_NAMES[];

struct pfctl_syncookies {
	enum pfctl_syncookies_mode	mode;
	uint8_t				highwater;	/* Percent */
	uint8_t				lowwater;	/* Percent */
	uint32_t			halfopen_states;
};

struct pfctl_threshold {
	uint32_t		limit;
	uint32_t		seconds;
	uint32_t		count;
	uint32_t		last;
};

struct pfctl_src_node {
	struct pf_addr		addr;
	struct pf_addr		raddr;
	int			rule;
	uint64_t		bytes[2];
	uint64_t		packets[2];
	uint32_t		states;
	uint32_t		conn;
	sa_family_t		af;
	sa_family_t		naf;
	uint8_t			ruletype;
	uint64_t		creation;
	uint64_t		expire;
	struct pfctl_threshold	conn_rate;
	pf_sn_types_t		type;
};

#define	PF_DEVICE	"/dev/pf"

struct pfctl_handle;
struct pfctl_handle	*pfctl_open(const char *pf_device);
void	pfctl_close(struct pfctl_handle *);
int	pfctl_fd(struct pfctl_handle *);

int	pfctl_startstop(struct pfctl_handle *h, int start);
struct pfctl_status* pfctl_get_status_h(struct pfctl_handle *h);
struct pfctl_status* pfctl_get_status(int dev);
int	pfctl_clear_status(struct pfctl_handle *h);
uint64_t pfctl_status_counter(struct pfctl_status *status, int id);
uint64_t pfctl_status_lcounter(struct pfctl_status *status, int id);
uint64_t pfctl_status_fcounter(struct pfctl_status *status, int id);
uint64_t pfctl_status_scounter(struct pfctl_status *status, int id);
void	pfctl_free_status(struct pfctl_status *status);

int	pfctl_get_eth_rulesets_info(int dev,
	    struct pfctl_eth_rulesets_info *ri, const char *path);
int	pfctl_get_eth_ruleset(int dev, const char *path, int nr,
	    struct pfctl_eth_ruleset_info *ri);
int	pfctl_get_eth_rules_info(int dev, struct pfctl_eth_rules_info *rules,
	    const char *path);
int	pfctl_get_eth_rule(int dev, uint32_t nr, uint32_t ticket,
	    const char *path, struct pfctl_eth_rule *rule, bool clear,
	    char *anchor_call);
int	pfctl_add_eth_rule(int dev, const struct pfctl_eth_rule *r,
	    const char *anchor, const char *anchor_call, uint32_t ticket);
int	pfctl_get_rules_info_h(struct pfctl_handle *h,
	    struct pfctl_rules_info *rules, uint32_t ruleset,
	    const char *path);
int	pfctl_get_rules_info(int dev, struct pfctl_rules_info *rules,
	    uint32_t ruleset, const char *path);
int	pfctl_get_rule(int dev, uint32_t nr, uint32_t ticket,
	    const char *anchor, uint32_t ruleset, struct pfctl_rule *rule,
	    char *anchor_call);
int	pfctl_get_rule_h(struct pfctl_handle *h, uint32_t nr, uint32_t ticket,
	    const char *anchor, uint32_t ruleset, struct pfctl_rule *rule,
	    char *anchor_call);
int	pfctl_get_clear_rule(int dev, uint32_t nr, uint32_t ticket,
	    const char *anchor, uint32_t ruleset, struct pfctl_rule *rule,
	    char *anchor_call, bool clear);
int	pfctl_get_clear_rule_h(struct pfctl_handle *h, uint32_t nr, uint32_t ticket,
	    const char *anchor, uint32_t ruleset, struct pfctl_rule *rule,
	    char *anchor_call, bool clear);
int	pfctl_add_rule(int dev, const struct pfctl_rule *r,
	    const char *anchor, const char *anchor_call, uint32_t ticket,
	    uint32_t pool_ticket);
int	pfctl_add_rule_h(struct pfctl_handle *h, const struct pfctl_rule *r,
	    const char *anchor, const char *anchor_call, uint32_t ticket,
	    uint32_t pool_ticket);
int	pfctl_set_keepcounters(int dev, bool keep);
int	pfctl_get_creatorids(struct pfctl_handle *h, uint32_t *creators, size_t *len);

struct pfctl_state_filter {
	char			ifname[IFNAMSIZ];
	uint16_t		proto;
	sa_family_t		af;
	struct pf_addr		addr;
	struct pf_addr		mask;
};
typedef int (*pfctl_get_state_fn)(struct pfctl_state *, void *);
int pfctl_get_states_iter(pfctl_get_state_fn f, void *arg);
int pfctl_get_filtered_states_iter(struct pfctl_state_filter *filter, pfctl_get_state_fn f, void *arg);
int	pfctl_get_states(int dev, struct pfctl_states *states);
void	pfctl_free_states(struct pfctl_states *states);
int	pfctl_clear_states(int dev, const struct pfctl_kill *kill,
	    unsigned int *killed);
int	pfctl_kill_states(int dev, const struct pfctl_kill *kill,
	    unsigned int *killed);
int	pfctl_clear_states_h(struct pfctl_handle *h, const struct pfctl_kill *kill,
	    unsigned int *killed);
int	pfctl_kill_states_h(struct pfctl_handle *h, const struct pfctl_kill *kill,
	    unsigned int *killed);
int	pfctl_clear_rules(int dev, const char *anchorname);
int	pfctl_clear_nat(int dev, const char *anchorname);
int	pfctl_clear_eth_rules(int dev, const char *anchorname);
int	pfctl_set_syncookies(int dev, const struct pfctl_syncookies *s);
int	pfctl_get_syncookies(int dev, struct pfctl_syncookies *s);
int	pfctl_table_add_addrs(int dev, struct pfr_table *tbl, struct pfr_addr
	    *addr, int size, int *nadd, int flags);
int	pfctl_table_del_addrs(int dev, struct pfr_table *tbl, struct pfr_addr
	    *addr, int size, int *ndel, int flags);
int     pfctl_table_set_addrs(int dev, struct pfr_table *tbl, struct pfr_addr
	    *addr, int size, int *size2, int *nadd, int *ndel, int *nchange,
	    int flags);
int	pfctl_table_get_addrs(int dev, struct pfr_table *tbl, struct pfr_addr
	    *addr, int *size, int flags);
int	pfctl_set_statusif(struct pfctl_handle *h, const char *ifname);

struct pfctl_natlook_key {
	sa_family_t af;
	uint8_t direction;
	uint8_t proto;
	struct pf_addr saddr;
	struct pf_addr daddr;
	uint16_t sport;
	uint16_t dport;
};
struct pfctl_natlook {
	struct pf_addr saddr;
	struct pf_addr daddr;
	uint16_t sport;
	uint16_t dport;
};
int	pfctl_natlook(struct pfctl_handle *h,
	    const struct pfctl_natlook_key *k, struct pfctl_natlook *r);
int	pfctl_set_debug(struct pfctl_handle *h, uint32_t level);
int	pfctl_set_timeout(struct pfctl_handle *h, uint32_t timeout, uint32_t seconds);
int	pfctl_get_timeout(struct pfctl_handle *h, uint32_t timeout, uint32_t *seconds);
int	pfctl_set_limit(struct pfctl_handle *h, const int index, const uint limit);
int	pfctl_get_limit(struct pfctl_handle *h, const int index, uint *limit);
int	pfctl_begin_addrs(struct pfctl_handle *h, uint32_t *ticket);
int	pfctl_add_addr(struct pfctl_handle *h, const struct pfioc_pooladdr *pa, int which);
int	pfctl_get_addrs(struct pfctl_handle *h, uint32_t ticket, uint32_t r_num,
	    uint8_t r_action, const char *anchor, uint32_t *nr, int which);
int	pfctl_get_addr(struct pfctl_handle *h, uint32_t ticket, uint32_t r_num,
	    uint8_t r_action, const char *anchor, uint32_t nr, struct pfioc_pooladdr *pa,
	    int which);
int	pfctl_get_rulesets(struct pfctl_handle *h, const char *path, uint32_t *nr);
int	pfctl_get_ruleset(struct pfctl_handle *h, const char *path, uint32_t nr, struct pfioc_ruleset *rs);
typedef int (*pfctl_get_srcnode_fn)(struct pfctl_src_node*, void *);
int	pfctl_get_srcnodes(struct pfctl_handle *h, pfctl_get_srcnode_fn fn, void *arg);

int	pfctl_clear_tables(struct pfctl_handle *h, struct pfr_table *filter,
	    int *ndel, int flags);
int	pfctl_add_table(struct pfctl_handle *h, struct pfr_table *table,
	    int *nadd, int flags);
int	pfctl_del_table(struct pfctl_handle *h, struct pfr_table *table,
	    int *ndel, int flags);

#endif
