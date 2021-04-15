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
 *
 * $FreeBSD$
 */

#ifndef _PFCTL_IOCTL_H_
#define _PFCTL_IOCTL_H_

#include <netpfil/pf/pf.h>

struct pfctl_anchor;

struct pfctl_pool {
	struct pf_palist	 list;
	struct pf_pooladdr	*cur;
	struct pf_poolhashkey	 key;
	struct pf_addr		 counter;
	struct pf_mape_portset	 mape;
	int			 tblidx;
	u_int16_t		 proxy_port[2];
	u_int8_t		 opts;
};

struct pfctl_rule {
	struct pf_rule_addr	 src;
	struct pf_rule_addr	 dst;
	union pf_rule_ptr	 skip[PF_SKIP_COUNT];
	char			 label[PF_RULE_LABEL_SIZE];
	char			 ifname[IFNAMSIZ];
	char			 qname[PF_QNAME_SIZE];
	char			 pqname[PF_QNAME_SIZE];
	char			 tagname[PF_TAG_NAME_SIZE];
	char			 match_tagname[PF_TAG_NAME_SIZE];

	char			 overload_tblname[PF_TABLE_NAME_SIZE];

	TAILQ_ENTRY(pfctl_rule)	 entries;
	struct pfctl_pool	 rpool;

	u_int64_t		 evaluations;
	u_int64_t		 packets[2];
	u_int64_t		 bytes[2];

	struct pfi_kif		*kif;
	struct pfctl_anchor	*anchor;
	struct pfr_ktable	*overload_tbl;

	pf_osfp_t		 os_fingerprint;

	int			 rtableid;
	u_int32_t		 timeout[PFTM_MAX];
	u_int32_t		 max_states;
	u_int32_t		 max_src_nodes;
	u_int32_t		 max_src_states;
	u_int32_t		 max_src_conn;
	struct {
		u_int32_t		limit;
		u_int32_t		seconds;
	}			 max_src_conn_rate;
	u_int32_t		 qid;
	u_int32_t		 pqid;
	u_int32_t		 nr;
	u_int32_t		 prob;
	uid_t			 cuid;
	pid_t			 cpid;

	uint64_t		 states_cur;
	uint64_t		 states_tot;
	uint64_t		 src_nodes;

	u_int16_t		 return_icmp;
	u_int16_t		 return_icmp6;
	u_int16_t		 max_mss;
	u_int16_t		 tag;
	u_int16_t		 match_tag;
	u_int16_t		 scrub_flags;

	struct pf_rule_uid	 uid;
	struct pf_rule_gid	 gid;

	u_int32_t		 rule_flag;
	u_int8_t		 action;
	u_int8_t		 direction;
	u_int8_t		 log;
	u_int8_t		 logif;
	u_int8_t		 quick;
	u_int8_t		 ifnot;
	u_int8_t		 match_tag_not;
	u_int8_t		 natpass;

	u_int8_t		 keep_state;
	sa_family_t		 af;
	u_int8_t		 proto;
	u_int8_t		 type;
	u_int8_t		 code;
	u_int8_t		 flags;
	u_int8_t		 flagset;
	u_int8_t		 min_ttl;
	u_int8_t		 allow_opts;
	u_int8_t		 rt;
	u_int8_t		 return_ttl;
	u_int8_t		 tos;
	u_int8_t		 set_tos;
	u_int8_t		 anchor_relative;
	u_int8_t		 anchor_wildcard;

	u_int8_t		 flush;
	u_int8_t		 prio;
	u_int8_t		 set_prio[2];

	struct {
		struct pf_addr		addr;
		u_int16_t		port;
	}			divert;
};

TAILQ_HEAD(pfctl_rulequeue, pfctl_rule);

struct pfctl_ruleset {
	struct {
		struct pfctl_rulequeue	 queues[2];
		struct {
			struct pfctl_rulequeue	*ptr;
			struct pfctl_rule	**ptr_array;
			u_int32_t		 rcount;
			u_int32_t		 ticket;
			int			 open;
		}			 active, inactive;
	}			 rules[PF_RULESET_MAX];
	struct pfctl_anchor	*anchor;
	u_int32_t		 tticket;
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

int	pfctl_get_rule(int dev, u_int32_t nr, u_int32_t ticket,
	    const char *anchor, u_int32_t ruleset, struct pfctl_rule *rule,
	    char *anchor_call);
int	pfctl_get_clear_rule(int dev, u_int32_t nr, u_int32_t ticket,
	    const char *anchor, u_int32_t ruleset, struct pfctl_rule *rule,
	    char *anchor_call, bool clear);
int	pfctl_add_rule(int dev, const struct pfctl_rule *r,
	    const char *anchor, const char *anchor_call, u_int32_t ticket,
	    u_int32_t pool_ticket);
int	pfctl_set_keepcounters(int dev, bool keep);

#endif
