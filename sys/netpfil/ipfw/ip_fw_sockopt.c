/*-
 * Copyright (c) 2002-2009 Luigi Rizzo, Universita` di Pisa
 *
 * Supported by: Valeria Paoli
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Sockopt support for ipfw. The routines here implement
 * the upper half of the ipfw code.
 */

#include "opt_ipfw.h"
#include "opt_inet.h"
#ifndef INET
#error IPFIREWALL requires INET.
#endif /* INET */
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>	/* struct m_tag used by nested headers */
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <net/if.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/ip_var.h> /* hooks */
#include <netinet/ip_fw.h>

#include <netpfil/ipfw/ip_fw_private.h>

#ifdef MAC
#include <security/mac/mac_framework.h>
#endif

MALLOC_DEFINE(M_IPFW, "IpFw/IpAcct", "IpFw/IpAcct chain's");

/*
 * static variables followed by global ones (none in this file)
 */

/*
 * Find the smallest rule >= key, id.
 * We could use bsearch but it is so simple that we code it directly
 */
int
ipfw_find_rule(struct ip_fw_chain *chain, uint32_t key, uint32_t id)
{
	int i, lo, hi;
	struct ip_fw *r;

  	for (lo = 0, hi = chain->n_rules - 1; lo < hi;) {
		i = (lo + hi) / 2;
		r = chain->map[i];
		if (r->rulenum < key)
			lo = i + 1;	/* continue from the next one */
		else if (r->rulenum > key)
			hi = i;		/* this might be good */
		else if (r->id < id)
			lo = i + 1;	/* continue from the next one */
		else /* r->id >= id */
			hi = i;		/* this might be good */
	};
	return hi;
}

/*
 * allocate a new map, returns the chain locked. extra is the number
 * of entries to add or delete.
 */
static struct ip_fw **
get_map(struct ip_fw_chain *chain, int extra, int locked)
{

	for (;;) {
		struct ip_fw **map;
		int i;

		i = chain->n_rules + extra;
		map = malloc(i * sizeof(struct ip_fw *), M_IPFW,
			locked ? M_NOWAIT : M_WAITOK);
		if (map == NULL) {
			printf("%s: cannot allocate map\n", __FUNCTION__);
			return NULL;
		}
		if (!locked)
			IPFW_UH_WLOCK(chain);
		if (i >= chain->n_rules + extra) /* good */
			return map;
		/* otherwise we lost the race, free and retry */
		if (!locked)
			IPFW_UH_WUNLOCK(chain);
		free(map, M_IPFW);
	}
}

/*
 * swap the maps. It is supposed to be called with IPFW_UH_WLOCK
 */
static struct ip_fw **
swap_map(struct ip_fw_chain *chain, struct ip_fw **new_map, int new_len)
{
	struct ip_fw **old_map;

	IPFW_WLOCK(chain);
	chain->id++;
	chain->n_rules = new_len;
	old_map = chain->map;
	chain->map = new_map;
	IPFW_WUNLOCK(chain);
	return old_map;
}

/*
 * Add a new rule to the list. Copy the rule into a malloc'ed area, then
 * possibly create a rule number and add the rule to the list.
 * Update the rule_number in the input struct so the caller knows it as well.
 * XXX DO NOT USE FOR THE DEFAULT RULE.
 * Must be called without IPFW_UH held
 */
int
ipfw_add_rule(struct ip_fw_chain *chain, struct ip_fw *input_rule)
{
	struct ip_fw *rule;
	int i, l, insert_before;
	struct ip_fw **map;	/* the new array of pointers */

	if (chain->rules == NULL || input_rule->rulenum > IPFW_DEFAULT_RULE-1)
		return (EINVAL);

	l = RULESIZE(input_rule);
	rule = malloc(l, M_IPFW, M_WAITOK | M_ZERO);
	/* get_map returns with IPFW_UH_WLOCK if successful */
	map = get_map(chain, 1, 0 /* not locked */);
	if (map == NULL) {
		free(rule, M_IPFW);
		return ENOSPC;
	}

	bcopy(input_rule, rule, l);
	/* clear fields not settable from userland */
	rule->x_next = NULL;
	rule->next_rule = NULL;
	IPFW_ZERO_RULE_COUNTER(rule);

	if (V_autoinc_step < 1)
		V_autoinc_step = 1;
	else if (V_autoinc_step > 1000)
		V_autoinc_step = 1000;
	/* find the insertion point, we will insert before */
	insert_before = rule->rulenum ? rule->rulenum + 1 : IPFW_DEFAULT_RULE;
	i = ipfw_find_rule(chain, insert_before, 0);
	/* duplicate first part */
	if (i > 0)
		bcopy(chain->map, map, i * sizeof(struct ip_fw *));
	map[i] = rule;
	/* duplicate remaining part, we always have the default rule */
	bcopy(chain->map + i, map + i + 1,
		sizeof(struct ip_fw *) *(chain->n_rules - i));
	if (rule->rulenum == 0) {
		/* write back the number */
		rule->rulenum = i > 0 ? map[i-1]->rulenum : 0;
		if (rule->rulenum < IPFW_DEFAULT_RULE - V_autoinc_step)
			rule->rulenum += V_autoinc_step;
		input_rule->rulenum = rule->rulenum;
	}

	rule->id = chain->id + 1;
	map = swap_map(chain, map, chain->n_rules + 1);
	chain->static_len += l;
	IPFW_UH_WUNLOCK(chain);
	if (map)
		free(map, M_IPFW);
	return (0);
}

/*
 * Reclaim storage associated with a list of rules.  This is
 * typically the list created using remove_rule.
 * A NULL pointer on input is handled correctly.
 */
void
ipfw_reap_rules(struct ip_fw *head)
{
	struct ip_fw *rule;

	while ((rule = head) != NULL) {
		head = head->x_next;
		free(rule, M_IPFW);
	}
}

/*
 * Used by del_entry() to check if a rule should be kept.
 * Returns 1 if the rule must be kept, 0 otherwise.
 *
 * Called with cmd = {0,1,5}.
 * cmd == 0 matches on rule numbers, excludes rules in RESVD_SET if n == 0 ;
 * cmd == 1 matches on set numbers only, rule numbers are ignored;
 * cmd == 5 matches on rule and set numbers.
 *
 * n == 0 is a wildcard for rule numbers, there is no wildcard for sets.
 *
 * Rules to keep are
 *	(default || reserved || !match_set || !match_number)
 * where
 *   default ::= (rule->rulenum == IPFW_DEFAULT_RULE)
 *	// the default rule is always protected
 *
 *   reserved ::= (cmd == 0 && n == 0 && rule->set == RESVD_SET)
 *	// RESVD_SET is protected only if cmd == 0 and n == 0 ("ipfw flush")
 *
 *   match_set ::= (cmd == 0 || rule->set == set)
 *	// set number is ignored for cmd == 0
 *
 *   match_number ::= (cmd == 1 || n == 0 || n == rule->rulenum)
 *	// number is ignored for cmd == 1 or n == 0
 *
 */
static int
keep_rule(struct ip_fw *rule, uint8_t cmd, uint8_t set, uint32_t n)
{
	return
		 (rule->rulenum == IPFW_DEFAULT_RULE)		||
		 (cmd == 0 && n == 0 && rule->set == RESVD_SET)	||
		!(cmd == 0 || rule->set == set)			||
		!(cmd == 1 || n == 0 || n == rule->rulenum);
}

/**
 * Remove all rules with given number, or do set manipulation.
 * Assumes chain != NULL && *chain != NULL.
 *
 * The argument is an uint32_t. The low 16 bit are the rule or set number;
 * the next 8 bits are the new set; the top 8 bits indicate the command:
 *
 *	0	delete rules numbered "rulenum"
 *	1	delete rules in set "rulenum"
 *	2	move rules "rulenum" to set "new_set"
 *	3	move rules from set "rulenum" to set "new_set"
 *	4	swap sets "rulenum" and "new_set"
 *	5	delete rules "rulenum" and set "new_set"
 */
static int
del_entry(struct ip_fw_chain *chain, uint32_t arg)
{
	struct ip_fw *rule;
	uint32_t num;	/* rule number or old_set */
	uint8_t cmd, new_set;
	int start, end, i, ofs, n;
	struct ip_fw **map = NULL;
	int error = 0;

	num = arg & 0xffff;
	cmd = (arg >> 24) & 0xff;
	new_set = (arg >> 16) & 0xff;

	if (cmd > 5 || new_set > RESVD_SET)
		return EINVAL;
	if (cmd == 0 || cmd == 2 || cmd == 5) {
		if (num >= IPFW_DEFAULT_RULE)
			return EINVAL;
	} else {
		if (num > RESVD_SET)	/* old_set */
			return EINVAL;
	}

	IPFW_UH_WLOCK(chain);	/* arbitrate writers */
	chain->reap = NULL;	/* prepare for deletions */

	switch (cmd) {
	case 0:	/* delete rules "num" (num == 0 matches all) */
	case 1:	/* delete all rules in set N */
	case 5: /* delete rules with number N and set "new_set". */

		/*
		 * Locate first rule to delete (start), the rule after
		 * the last one to delete (end), and count how many
		 * rules to delete (n). Always use keep_rule() to
		 * determine which rules to keep.
		 */
		n = 0;
		if (cmd == 1) {
			/* look for a specific set including RESVD_SET.
			 * Must scan the entire range, ignore num.
			 */
			new_set = num;
			for (start = -1, end = i = 0; i < chain->n_rules; i++) {
				if (keep_rule(chain->map[i], cmd, new_set, 0))
					continue;
				if (start < 0)
					start = i;
				end = i;
				n++;
			}
			end++;	/* first non-matching */
		} else {
			/* Optimized search on rule numbers */
			start = ipfw_find_rule(chain, num, 0);
			for (end = start; end < chain->n_rules; end++) {
				rule = chain->map[end];
				if (num > 0 && rule->rulenum != num)
					break;
				if (!keep_rule(rule, cmd, new_set, num))
					n++;
			}
		}

		if (n == 0) {
			/* A flush request (arg == 0 or cmd == 1) on empty
			 * ruleset returns with no error. On the contrary,
			 * if there is no match on a specific request,
			 * we return EINVAL.
			 */
			if (arg != 0 && cmd != 1)
				error = EINVAL;
			break;
		}

		/* We have something to delete. Allocate the new map */
		map = get_map(chain, -n, 1 /* locked */);
		if (map == NULL) {
			error = EINVAL;
			break;
		}

		/* 1. bcopy the initial part of the map */
		if (start > 0)
			bcopy(chain->map, map, start * sizeof(struct ip_fw *));
		/* 2. copy active rules between start and end */
		for (i = ofs = start; i < end; i++) {
			rule = chain->map[i];
			if (keep_rule(rule, cmd, new_set, num))
				map[ofs++] = rule;
		}
		/* 3. copy the final part of the map */
		bcopy(chain->map + end, map + ofs,
			(chain->n_rules - end) * sizeof(struct ip_fw *));
		/* 4. swap the maps (under BH_LOCK) */
		map = swap_map(chain, map, chain->n_rules - n);
		/* 5. now remove the rules deleted from the old map */
		if (cmd == 1)
			ipfw_expire_dyn_rules(chain, NULL, new_set);
		for (i = start; i < end; i++) {
			rule = map[i];
			if (keep_rule(rule, cmd, new_set, num))
				continue;
			chain->static_len -= RULESIZE(rule);
			if (cmd != 1)
				ipfw_expire_dyn_rules(chain, rule, RESVD_SET);
			rule->x_next = chain->reap;
			chain->reap = rule;
		}
		break;

	/*
	 * In the next 3 cases the loop stops at (n_rules - 1)
	 * because the default rule is never eligible..
	 */

	case 2:	/* move rules with given RULE number to new set */
		for (i = 0; i < chain->n_rules - 1; i++) {
			rule = chain->map[i];
			if (rule->rulenum == num)
				rule->set = new_set;
		}
		break;

	case 3: /* move rules with given SET number to new set */
		for (i = 0; i < chain->n_rules - 1; i++) {
			rule = chain->map[i];
			if (rule->set == num)
				rule->set = new_set;
		}
		break;

	case 4: /* swap two sets */
		for (i = 0; i < chain->n_rules - 1; i++) {
			rule = chain->map[i];
			if (rule->set == num)
				rule->set = new_set;
			else if (rule->set == new_set)
				rule->set = num;
		}
		break;
	}

	rule = chain->reap;
	chain->reap = NULL;
	IPFW_UH_WUNLOCK(chain);
	ipfw_reap_rules(rule);
	if (map)
		free(map, M_IPFW);
	return error;
}

/*
 * Clear counters for a specific rule.
 * Normally run under IPFW_UH_RLOCK, but these are idempotent ops
 * so we only care that rules do not disappear.
 */
static void
clear_counters(struct ip_fw *rule, int log_only)
{
	ipfw_insn_log *l = (ipfw_insn_log *)ACTION_PTR(rule);

	if (log_only == 0)
		IPFW_ZERO_RULE_COUNTER(rule);
	if (l->o.opcode == O_LOG)
		l->log_left = l->max_log;
}

/**
 * Reset some or all counters on firewall rules.
 * The argument `arg' is an u_int32_t. The low 16 bit are the rule number,
 * the next 8 bits are the set number, the top 8 bits are the command:
 *	0	work with rules from all set's;
 *	1	work with rules only from specified set.
 * Specified rule number is zero if we want to clear all entries.
 * log_only is 1 if we only want to reset logs, zero otherwise.
 */
static int
zero_entry(struct ip_fw_chain *chain, u_int32_t arg, int log_only)
{
	struct ip_fw *rule;
	char *msg;
	int i;

	uint16_t rulenum = arg & 0xffff;
	uint8_t set = (arg >> 16) & 0xff;
	uint8_t cmd = (arg >> 24) & 0xff;

	if (cmd > 1)
		return (EINVAL);
	if (cmd == 1 && set > RESVD_SET)
		return (EINVAL);

	IPFW_UH_RLOCK(chain);
	if (rulenum == 0) {
		V_norule_counter = 0;
		for (i = 0; i < chain->n_rules; i++) {
			rule = chain->map[i];
			/* Skip rules not in our set. */
			if (cmd == 1 && rule->set != set)
				continue;
			clear_counters(rule, log_only);
		}
		msg = log_only ? "All logging counts reset" :
		    "Accounting cleared";
	} else {
		int cleared = 0;
		for (i = 0; i < chain->n_rules; i++) {
			rule = chain->map[i];
			if (rule->rulenum == rulenum) {
				if (cmd == 0 || rule->set == set)
					clear_counters(rule, log_only);
				cleared = 1;
			}
			if (rule->rulenum > rulenum)
				break;
		}
		if (!cleared) {	/* we did not find any matching rules */
			IPFW_UH_RUNLOCK(chain);
			return (EINVAL);
		}
		msg = log_only ? "logging count reset" : "cleared";
	}
	IPFW_UH_RUNLOCK(chain);

	if (V_fw_verbose) {
		int lev = LOG_SECURITY | LOG_NOTICE;

		if (rulenum)
			log(lev, "ipfw: Entry %d %s.\n", rulenum, msg);
		else
			log(lev, "ipfw: %s.\n", msg);
	}
	return (0);
}

/*
 * Check validity of the structure before insert.
 * Rules are simple, so this mostly need to check rule sizes.
 */
static int
check_ipfw_struct(struct ip_fw *rule, int size)
{
	int l, cmdlen = 0;
	int have_action=0;
	ipfw_insn *cmd;

	if (size < sizeof(*rule)) {
		printf("ipfw: rule too short\n");
		return (EINVAL);
	}
	/* first, check for valid size */
	l = RULESIZE(rule);
	if (l != size) {
		printf("ipfw: size mismatch (have %d want %d)\n", size, l);
		return (EINVAL);
	}
	if (rule->act_ofs >= rule->cmd_len) {
		printf("ipfw: bogus action offset (%u > %u)\n",
		    rule->act_ofs, rule->cmd_len - 1);
		return (EINVAL);
	}
	/*
	 * Now go for the individual checks. Very simple ones, basically only
	 * instruction sizes.
	 */
	for (l = rule->cmd_len, cmd = rule->cmd ;
			l > 0 ; l -= cmdlen, cmd += cmdlen) {
		cmdlen = F_LEN(cmd);
		if (cmdlen > l) {
			printf("ipfw: opcode %d size truncated\n",
			    cmd->opcode);
			return EINVAL;
		}
		switch (cmd->opcode) {
		case O_PROBE_STATE:
		case O_KEEP_STATE:
		case O_PROTO:
		case O_IP_SRC_ME:
		case O_IP_DST_ME:
		case O_LAYER2:
		case O_IN:
		case O_FRAG:
		case O_DIVERTED:
		case O_IPOPT:
		case O_IPTOS:
		case O_IPPRECEDENCE:
		case O_IPVER:
		case O_SOCKARG:
		case O_TCPFLAGS:
		case O_TCPOPTS:
		case O_ESTAB:
		case O_VERREVPATH:
		case O_VERSRCREACH:
		case O_ANTISPOOF:
		case O_IPSEC:
#ifdef INET6
		case O_IP6_SRC_ME:
		case O_IP6_DST_ME:
		case O_EXT_HDR:
		case O_IP6:
#endif
		case O_IP4:
		case O_TAG:
			if (cmdlen != F_INSN_SIZE(ipfw_insn))
				goto bad_size;
			break;

		case O_FIB:
			if (cmdlen != F_INSN_SIZE(ipfw_insn))
				goto bad_size;
			if (cmd->arg1 >= rt_numfibs) {
				printf("ipfw: invalid fib number %d\n",
					cmd->arg1);
				return EINVAL;
			}
			break;

		case O_SETFIB:
			if (cmdlen != F_INSN_SIZE(ipfw_insn))
				goto bad_size;
			if ((cmd->arg1 != IP_FW_TABLEARG) &&
			    (cmd->arg1 >= rt_numfibs)) {
				printf("ipfw: invalid fib number %d\n",
					cmd->arg1);
				return EINVAL;
			}
			goto check_action;

		case O_UID:
		case O_GID:
		case O_JAIL:
		case O_IP_SRC:
		case O_IP_DST:
		case O_TCPSEQ:
		case O_TCPACK:
		case O_PROB:
		case O_ICMPTYPE:
			if (cmdlen != F_INSN_SIZE(ipfw_insn_u32))
				goto bad_size;
			break;

		case O_LIMIT:
			if (cmdlen != F_INSN_SIZE(ipfw_insn_limit))
				goto bad_size;
			break;

		case O_LOG:
			if (cmdlen != F_INSN_SIZE(ipfw_insn_log))
				goto bad_size;

			((ipfw_insn_log *)cmd)->log_left =
			    ((ipfw_insn_log *)cmd)->max_log;

			break;

		case O_IP_SRC_MASK:
		case O_IP_DST_MASK:
			/* only odd command lengths */
			if ( !(cmdlen & 1) || cmdlen > 31)
				goto bad_size;
			break;

		case O_IP_SRC_SET:
		case O_IP_DST_SET:
			if (cmd->arg1 == 0 || cmd->arg1 > 256) {
				printf("ipfw: invalid set size %d\n",
					cmd->arg1);
				return EINVAL;
			}
			if (cmdlen != F_INSN_SIZE(ipfw_insn_u32) +
			    (cmd->arg1+31)/32 )
				goto bad_size;
			break;

		case O_IP_SRC_LOOKUP:
		case O_IP_DST_LOOKUP:
			if (cmd->arg1 >= V_fw_tables_max) {
				printf("ipfw: invalid table number %d\n",
				    cmd->arg1);
				return (EINVAL);
			}
			if (cmdlen != F_INSN_SIZE(ipfw_insn) &&
			    cmdlen != F_INSN_SIZE(ipfw_insn_u32) + 1 &&
			    cmdlen != F_INSN_SIZE(ipfw_insn_u32))
				goto bad_size;
			break;
		case O_MACADDR2:
			if (cmdlen != F_INSN_SIZE(ipfw_insn_mac))
				goto bad_size;
			break;

		case O_NOP:
		case O_IPID:
		case O_IPTTL:
		case O_IPLEN:
		case O_TCPDATALEN:
		case O_TCPWIN:
		case O_TAGGED:
			if (cmdlen < 1 || cmdlen > 31)
				goto bad_size;
			break;

		case O_DSCP:
			if (cmdlen != F_INSN_SIZE(ipfw_insn_u32) + 1)
				goto bad_size;
			break;

		case O_MAC_TYPE:
		case O_IP_SRCPORT:
		case O_IP_DSTPORT: /* XXX artificial limit, 30 port pairs */
			if (cmdlen < 2 || cmdlen > 31)
				goto bad_size;
			break;

		case O_RECV:
		case O_XMIT:
		case O_VIA:
			if (cmdlen != F_INSN_SIZE(ipfw_insn_if))
				goto bad_size;
			break;

		case O_ALTQ:
			if (cmdlen != F_INSN_SIZE(ipfw_insn_altq))
				goto bad_size;
			break;

		case O_PIPE:
		case O_QUEUE:
			if (cmdlen != F_INSN_SIZE(ipfw_insn))
				goto bad_size;
			goto check_action;

		case O_FORWARD_IP:
			if (cmdlen != F_INSN_SIZE(ipfw_insn_sa))
				goto bad_size;
			goto check_action;
#ifdef INET6
		case O_FORWARD_IP6:
			if (cmdlen != F_INSN_SIZE(ipfw_insn_sa6))
				goto bad_size;
			goto check_action;
#endif /* INET6 */

		case O_DIVERT:
		case O_TEE:
			if (ip_divert_ptr == NULL)
				return EINVAL;
			else
				goto check_size;
		case O_NETGRAPH:
		case O_NGTEE:
			if (ng_ipfw_input_p == NULL)
				return EINVAL;
			else
				goto check_size;
		case O_NAT:
			if (!IPFW_NAT_LOADED)
				return EINVAL;
			if (cmdlen != F_INSN_SIZE(ipfw_insn_nat))
 				goto bad_size;		
 			goto check_action;
		case O_FORWARD_MAC: /* XXX not implemented yet */
		case O_CHECK_STATE:
		case O_COUNT:
		case O_ACCEPT:
		case O_DENY:
		case O_REJECT:
		case O_SETDSCP:
#ifdef INET6
		case O_UNREACH6:
#endif
		case O_SKIPTO:
		case O_REASS:
		case O_CALLRETURN:
check_size:
			if (cmdlen != F_INSN_SIZE(ipfw_insn))
				goto bad_size;
check_action:
			if (have_action) {
				printf("ipfw: opcode %d, multiple actions"
					" not allowed\n",
					cmd->opcode);
				return EINVAL;
			}
			have_action = 1;
			if (l != cmdlen) {
				printf("ipfw: opcode %d, action must be"
					" last opcode\n",
					cmd->opcode);
				return EINVAL;
			}
			break;
#ifdef INET6
		case O_IP6_SRC:
		case O_IP6_DST:
			if (cmdlen != F_INSN_SIZE(struct in6_addr) +
			    F_INSN_SIZE(ipfw_insn))
				goto bad_size;
			break;

		case O_FLOW6ID:
			if (cmdlen != F_INSN_SIZE(ipfw_insn_u32) +
			    ((ipfw_insn_u32 *)cmd)->o.arg1)
				goto bad_size;
			break;

		case O_IP6_SRC_MASK:
		case O_IP6_DST_MASK:
			if ( !(cmdlen & 1) || cmdlen > 127)
				goto bad_size;
			break;
		case O_ICMP6TYPE:
			if( cmdlen != F_INSN_SIZE( ipfw_insn_icmp6 ) )
				goto bad_size;
			break;
#endif

		default:
			switch (cmd->opcode) {
#ifndef INET6
			case O_IP6_SRC_ME:
			case O_IP6_DST_ME:
			case O_EXT_HDR:
			case O_IP6:
			case O_UNREACH6:
			case O_IP6_SRC:
			case O_IP6_DST:
			case O_FLOW6ID:
			case O_IP6_SRC_MASK:
			case O_IP6_DST_MASK:
			case O_ICMP6TYPE:
				printf("ipfw: no IPv6 support in kernel\n");
				return EPROTONOSUPPORT;
#endif
			default:
				printf("ipfw: opcode %d, unknown opcode\n",
					cmd->opcode);
				return EINVAL;
			}
		}
	}
	if (have_action == 0) {
		printf("ipfw: missing action\n");
		return EINVAL;
	}
	return 0;

bad_size:
	printf("ipfw: opcode %d size %d wrong\n",
		cmd->opcode, cmdlen);
	return EINVAL;
}


/*
 * Translation of requests for compatibility with FreeBSD 7.2/8.
 * a static variable tells us if we have an old client from userland,
 * and if necessary we translate requests and responses between the
 * two formats.
 */
static int is7 = 0;

struct ip_fw7 {
	struct ip_fw7	*next;		/* linked list of rules     */
	struct ip_fw7	*next_rule;	/* ptr to next [skipto] rule    */
	/* 'next_rule' is used to pass up 'set_disable' status      */

	uint16_t	act_ofs;	/* offset of action in 32-bit units */
	uint16_t	cmd_len;	/* # of 32-bit words in cmd */
	uint16_t	rulenum;	/* rule number          */
	uint8_t		set;		/* rule set (0..31)     */
	// #define RESVD_SET   31  /* set for default and persistent rules */
	uint8_t		_pad;		/* padding          */
	// uint32_t        id;             /* rule id, only in v.8 */
	/* These fields are present in all rules.           */
	uint64_t	pcnt;		/* Packet counter       */
	uint64_t	bcnt;		/* Byte counter         */
	uint32_t	timestamp;	/* tv_sec of last match     */

	ipfw_insn	cmd[1];		/* storage for commands     */
};

	int convert_rule_to_7(struct ip_fw *rule);
int convert_rule_to_8(struct ip_fw *rule);

#ifndef RULESIZE7
#define RULESIZE7(rule)  (sizeof(struct ip_fw7) + \
	((struct ip_fw7 *)(rule))->cmd_len * 4 - 4)
#endif


/*
 * Copy the static and dynamic rules to the supplied buffer
 * and return the amount of space actually used.
 * Must be run under IPFW_UH_RLOCK
 */
static size_t
ipfw_getrules(struct ip_fw_chain *chain, void *buf, size_t space)
{
	char *bp = buf;
	char *ep = bp + space;
	struct ip_fw *rule, *dst;
	int l, i;
	time_t	boot_seconds;

        boot_seconds = boottime.tv_sec;
	for (i = 0; i < chain->n_rules; i++) {
		rule = chain->map[i];

		if (is7) {
		    /* Convert rule to FreeBSd 7.2 format */
		    l = RULESIZE7(rule);
		    if (bp + l + sizeof(uint32_t) <= ep) {
			int error;
			bcopy(rule, bp, l + sizeof(uint32_t));
			error = convert_rule_to_7((struct ip_fw *) bp);
			if (error)
				return 0; /*XXX correct? */
			/*
			 * XXX HACK. Store the disable mask in the "next"
			 * pointer in a wild attempt to keep the ABI the same.
			 * Why do we do this on EVERY rule?
			 */
			bcopy(&V_set_disable,
				&(((struct ip_fw7 *)bp)->next_rule),
				sizeof(V_set_disable));
			if (((struct ip_fw7 *)bp)->timestamp)
			    ((struct ip_fw7 *)bp)->timestamp += boot_seconds;
			bp += l;
		    }
		    continue; /* go to next rule */
		}

		/* normal mode, don't touch rules */
		l = RULESIZE(rule);
		if (bp + l > ep) { /* should not happen */
			printf("overflow dumping static rules\n");
			break;
		}
		dst = (struct ip_fw *)bp;
		bcopy(rule, dst, l);
		/*
		 * XXX HACK. Store the disable mask in the "next"
		 * pointer in a wild attempt to keep the ABI the same.
		 * Why do we do this on EVERY rule?
		 */
		bcopy(&V_set_disable, &dst->next_rule, sizeof(V_set_disable));
		if (dst->timestamp)
			dst->timestamp += boot_seconds;
		bp += l;
	}
	ipfw_get_dynamic(chain, &bp, ep); /* protected by the dynamic lock */
	return (bp - (char *)buf);
}


#define IP_FW3_OPLENGTH(x)	((x)->sopt_valsize - sizeof(ip_fw3_opheader))
/**
 * {set|get}sockopt parser.
 */
int
ipfw_ctl(struct sockopt *sopt)
{
#define	RULE_MAXSIZE	(256*sizeof(u_int32_t))
	int error;
	size_t size, len, valsize;
	struct ip_fw *buf, *rule;
	struct ip_fw_chain *chain;
	u_int32_t rulenum[2];
	uint32_t opt;
	char xbuf[128];
	ip_fw3_opheader *op3 = NULL;

	error = priv_check(sopt->sopt_td, PRIV_NETINET_IPFW);
	if (error)
		return (error);

	/*
	 * Disallow modifications in really-really secure mode, but still allow
	 * the logging counters to be reset.
	 */
	if (sopt->sopt_name == IP_FW_ADD ||
	    (sopt->sopt_dir == SOPT_SET && sopt->sopt_name != IP_FW_RESETLOG)) {
		error = securelevel_ge(sopt->sopt_td->td_ucred, 3);
		if (error)
			return (error);
	}

	chain = &V_layer3_chain;
	error = 0;

	/* Save original valsize before it is altered via sooptcopyin() */
	valsize = sopt->sopt_valsize;
	if ((opt = sopt->sopt_name) == IP_FW3) {
		/* 
		 * Copy not less than sizeof(ip_fw3_opheader).
		 * We hope any IP_FW3 command will fit into 128-byte buffer.
		 */
		if ((error = sooptcopyin(sopt, xbuf, sizeof(xbuf),
			sizeof(ip_fw3_opheader))) != 0)
			return (error);
		op3 = (ip_fw3_opheader *)xbuf;
		opt = op3->opcode;
	}

	switch (opt) {
	case IP_FW_GET:
		/*
		 * pass up a copy of the current rules. Static rules
		 * come first (the last of which has number IPFW_DEFAULT_RULE),
		 * followed by a possibly empty list of dynamic rule.
		 * The last dynamic rule has NULL in the "next" field.
		 *
		 * Note that the calculated size is used to bound the
		 * amount of data returned to the user.  The rule set may
		 * change between calculating the size and returning the
		 * data in which case we'll just return what fits.
		 */
		for (;;) {
			int len = 0, want;

			size = chain->static_len;
			size += ipfw_dyn_len();
			if (size >= sopt->sopt_valsize)
				break;
			buf = malloc(size, M_TEMP, M_WAITOK);
			IPFW_UH_RLOCK(chain);
			/* check again how much space we need */
			want = chain->static_len + ipfw_dyn_len();
			if (size >= want)
				len = ipfw_getrules(chain, buf, size);
			IPFW_UH_RUNLOCK(chain);
			if (size >= want)
				error = sooptcopyout(sopt, buf, len);
			free(buf, M_TEMP);
			if (size >= want)
				break;
		}
		break;

	case IP_FW_FLUSH:
		/* locking is done within del_entry() */
		error = del_entry(chain, 0); /* special case, rule=0, cmd=0 means all */
		break;

	case IP_FW_ADD:
		rule = malloc(RULE_MAXSIZE, M_TEMP, M_WAITOK);
		error = sooptcopyin(sopt, rule, RULE_MAXSIZE,
			sizeof(struct ip_fw7) );

		/*
		 * If the size of commands equals RULESIZE7 then we assume
		 * a FreeBSD7.2 binary is talking to us (set is7=1).
		 * is7 is persistent so the next 'ipfw list' command
		 * will use this format.
		 * NOTE: If wrong version is guessed (this can happen if
		 *       the first ipfw command is 'ipfw [pipe] list')
		 *       the ipfw binary may crash or loop infinitly...
		 */
		if (sopt->sopt_valsize == RULESIZE7(rule)) {
		    is7 = 1;
		    error = convert_rule_to_8(rule);
		    if (error)
			return error;
		    if (error == 0)
			error = check_ipfw_struct(rule, RULESIZE(rule));
		} else {
		    is7 = 0;
		if (error == 0)
			error = check_ipfw_struct(rule, sopt->sopt_valsize);
		}
		if (error == 0) {
			/* locking is done within ipfw_add_rule() */
			error = ipfw_add_rule(chain, rule);
			size = RULESIZE(rule);
			if (!error && sopt->sopt_dir == SOPT_GET) {
				if (is7) {
					error = convert_rule_to_7(rule);
					size = RULESIZE7(rule);
					if (error)
						return error;
				}
				error = sooptcopyout(sopt, rule, size);
		}
		}
		free(rule, M_TEMP);
		break;

	case IP_FW_DEL:
		/*
		 * IP_FW_DEL is used for deleting single rules or sets,
		 * and (ab)used to atomically manipulate sets. Argument size
		 * is used to distinguish between the two:
		 *    sizeof(u_int32_t)
		 *	delete single rule or set of rules,
		 *	or reassign rules (or sets) to a different set.
		 *    2*sizeof(u_int32_t)
		 *	atomic disable/enable sets.
		 *	first u_int32_t contains sets to be disabled,
		 *	second u_int32_t contains sets to be enabled.
		 */
		error = sooptcopyin(sopt, rulenum,
			2*sizeof(u_int32_t), sizeof(u_int32_t));
		if (error)
			break;
		size = sopt->sopt_valsize;
		if (size == sizeof(u_int32_t) && rulenum[0] != 0) {
			/* delete or reassign, locking done in del_entry() */
			error = del_entry(chain, rulenum[0]);
		} else if (size == 2*sizeof(u_int32_t)) { /* set enable/disable */
			IPFW_UH_WLOCK(chain);
			V_set_disable =
			    (V_set_disable | rulenum[0]) & ~rulenum[1] &
			    ~(1<<RESVD_SET); /* set RESVD_SET always enabled */
			IPFW_UH_WUNLOCK(chain);
		} else
			error = EINVAL;
		break;

	case IP_FW_ZERO:
	case IP_FW_RESETLOG: /* argument is an u_int_32, the rule number */
		rulenum[0] = 0;
		if (sopt->sopt_val != 0) {
		    error = sooptcopyin(sopt, rulenum,
			    sizeof(u_int32_t), sizeof(u_int32_t));
		    if (error)
			break;
		}
		error = zero_entry(chain, rulenum[0],
			sopt->sopt_name == IP_FW_RESETLOG);
		break;

	/*--- TABLE manipulations are protected by the IPFW_LOCK ---*/
	case IP_FW_TABLE_ADD:
		{
			ipfw_table_entry ent;

			error = sooptcopyin(sopt, &ent,
			    sizeof(ent), sizeof(ent));
			if (error)
				break;
			error = ipfw_add_table_entry(chain, ent.tbl,
			    &ent.addr, sizeof(ent.addr), ent.masklen, 
			    IPFW_TABLE_CIDR, ent.value);
		}
		break;

	case IP_FW_TABLE_DEL:
		{
			ipfw_table_entry ent;

			error = sooptcopyin(sopt, &ent,
			    sizeof(ent), sizeof(ent));
			if (error)
				break;
			error = ipfw_del_table_entry(chain, ent.tbl,
			    &ent.addr, sizeof(ent.addr), ent.masklen, IPFW_TABLE_CIDR);
		}
		break;

	case IP_FW_TABLE_XADD: /* IP_FW3 */
	case IP_FW_TABLE_XDEL: /* IP_FW3 */
		{
			ipfw_table_xentry *xent = (ipfw_table_xentry *)(op3 + 1);

			/* Check minimum header size */
			if (IP_FW3_OPLENGTH(sopt) < offsetof(ipfw_table_xentry, k)) {
				error = EINVAL;
				break;
			}

			/* Check if len field is valid */
			if (xent->len > sizeof(ipfw_table_xentry)) {
				error = EINVAL;
				break;
			}
			
			len = xent->len - offsetof(ipfw_table_xentry, k);

			error = (opt == IP_FW_TABLE_XADD) ?
				ipfw_add_table_entry(chain, xent->tbl, &xent->k, 
					len, xent->masklen, xent->type, xent->value) :
				ipfw_del_table_entry(chain, xent->tbl, &xent->k,
					len, xent->masklen, xent->type);
		}
		break;

	case IP_FW_TABLE_FLUSH:
		{
			u_int16_t tbl;

			error = sooptcopyin(sopt, &tbl,
			    sizeof(tbl), sizeof(tbl));
			if (error)
				break;
			error = ipfw_flush_table(chain, tbl);
		}
		break;

	case IP_FW_TABLE_GETSIZE:
		{
			u_int32_t tbl, cnt;

			if ((error = sooptcopyin(sopt, &tbl, sizeof(tbl),
			    sizeof(tbl))))
				break;
			IPFW_RLOCK(chain);
			error = ipfw_count_table(chain, tbl, &cnt);
			IPFW_RUNLOCK(chain);
			if (error)
				break;
			error = sooptcopyout(sopt, &cnt, sizeof(cnt));
		}
		break;

	case IP_FW_TABLE_LIST:
		{
			ipfw_table *tbl;

			if (sopt->sopt_valsize < sizeof(*tbl)) {
				error = EINVAL;
				break;
			}
			size = sopt->sopt_valsize;
			tbl = malloc(size, M_TEMP, M_WAITOK);
			error = sooptcopyin(sopt, tbl, size, sizeof(*tbl));
			if (error) {
				free(tbl, M_TEMP);
				break;
			}
			tbl->size = (size - sizeof(*tbl)) /
			    sizeof(ipfw_table_entry);
			IPFW_RLOCK(chain);
			error = ipfw_dump_table(chain, tbl);
			IPFW_RUNLOCK(chain);
			if (error) {
				free(tbl, M_TEMP);
				break;
			}
			error = sooptcopyout(sopt, tbl, size);
			free(tbl, M_TEMP);
		}
		break;

	case IP_FW_TABLE_XGETSIZE: /* IP_FW3 */
		{
			uint32_t *tbl;

			if (IP_FW3_OPLENGTH(sopt) < sizeof(uint32_t)) {
				error = EINVAL;
				break;
			}

			tbl = (uint32_t *)(op3 + 1);

			IPFW_RLOCK(chain);
			error = ipfw_count_xtable(chain, *tbl, tbl);
			IPFW_RUNLOCK(chain);
			if (error)
				break;
			error = sooptcopyout(sopt, op3, sopt->sopt_valsize);
		}
		break;

	case IP_FW_TABLE_XLIST: /* IP_FW3 */
		{
			ipfw_xtable *tbl;

			if ((size = valsize) < sizeof(ipfw_xtable)) {
				error = EINVAL;
				break;
			}

			tbl = malloc(size, M_TEMP, M_ZERO | M_WAITOK);
			memcpy(tbl, op3, sizeof(ipfw_xtable));

			/* Get maximum number of entries we can store */
			tbl->size = (size - sizeof(ipfw_xtable)) /
			    sizeof(ipfw_table_xentry);
			IPFW_RLOCK(chain);
			error = ipfw_dump_xtable(chain, tbl);
			IPFW_RUNLOCK(chain);
			if (error) {
				free(tbl, M_TEMP);
				break;
			}

			/* Revert size field back to bytes */
			tbl->size = tbl->size * sizeof(ipfw_table_xentry) +
				sizeof(ipfw_table);
			/* 
			 * Since we call sooptcopyin() with small buffer, sopt_valsize is
			 * decreased to reflect supplied buffer size. Set it back to original value
			 */
			sopt->sopt_valsize = valsize;
			error = sooptcopyout(sopt, tbl, size);
			free(tbl, M_TEMP);
		}
		break;

	/*--- NAT operations are protected by the IPFW_LOCK ---*/
	case IP_FW_NAT_CFG:
		if (IPFW_NAT_LOADED)
			error = ipfw_nat_cfg_ptr(sopt);
		else {
			printf("IP_FW_NAT_CFG: %s\n",
			    "ipfw_nat not present, please load it");
			error = EINVAL;
		}
		break;

	case IP_FW_NAT_DEL:
		if (IPFW_NAT_LOADED)
			error = ipfw_nat_del_ptr(sopt);
		else {
			printf("IP_FW_NAT_DEL: %s\n",
			    "ipfw_nat not present, please load it");
			error = EINVAL;
		}
		break;

	case IP_FW_NAT_GET_CONFIG:
		if (IPFW_NAT_LOADED)
			error = ipfw_nat_get_cfg_ptr(sopt);
		else {
			printf("IP_FW_NAT_GET_CFG: %s\n",
			    "ipfw_nat not present, please load it");
			error = EINVAL;
		}
		break;

	case IP_FW_NAT_GET_LOG:
		if (IPFW_NAT_LOADED)
			error = ipfw_nat_get_log_ptr(sopt);
		else {
			printf("IP_FW_NAT_GET_LOG: %s\n",
			    "ipfw_nat not present, please load it");
			error = EINVAL;
		}
		break;

	default:
		printf("ipfw: ipfw_ctl invalid option %d\n", sopt->sopt_name);
		error = EINVAL;
	}

	return (error);
#undef RULE_MAXSIZE
}


#define	RULE_MAXSIZE	(256*sizeof(u_int32_t))

/* Functions to convert rules 7.2 <==> 8.0 */
int
convert_rule_to_7(struct ip_fw *rule)
{
	/* Used to modify original rule */
	struct ip_fw7 *rule7 = (struct ip_fw7 *)rule;
	/* copy of original rule, version 8 */
	struct ip_fw *tmp;

	/* Used to copy commands */
	ipfw_insn *ccmd, *dst;
	int ll = 0, ccmdlen = 0;

	tmp = malloc(RULE_MAXSIZE, M_TEMP, M_NOWAIT | M_ZERO);
	if (tmp == NULL) {
		return 1; //XXX error
	}
	bcopy(rule, tmp, RULE_MAXSIZE);

	/* Copy fields */
	rule7->_pad = tmp->_pad;
	rule7->set = tmp->set;
	rule7->rulenum = tmp->rulenum;
	rule7->cmd_len = tmp->cmd_len;
	rule7->act_ofs = tmp->act_ofs;
	rule7->next_rule = (struct ip_fw7 *)tmp->next_rule;
	rule7->next = (struct ip_fw7 *)tmp->x_next;
	rule7->cmd_len = tmp->cmd_len;
	rule7->pcnt = tmp->pcnt;
	rule7->bcnt = tmp->bcnt;
	rule7->timestamp = tmp->timestamp;

	/* Copy commands */
	for (ll = tmp->cmd_len, ccmd = tmp->cmd, dst = rule7->cmd ;
			ll > 0 ; ll -= ccmdlen, ccmd += ccmdlen, dst += ccmdlen) {
		ccmdlen = F_LEN(ccmd);

		bcopy(ccmd, dst, F_LEN(ccmd)*sizeof(uint32_t));

		if (dst->opcode > O_NAT)
			/* O_REASS doesn't exists in 7.2 version, so
			 * decrement opcode if it is after O_REASS
			 */
			dst->opcode--;

		if (ccmdlen > ll) {
			printf("ipfw: opcode %d size truncated\n",
				ccmd->opcode);
			return EINVAL;
		}
	}
	free(tmp, M_TEMP);

	return 0;
}

int
convert_rule_to_8(struct ip_fw *rule)
{
	/* Used to modify original rule */
	struct ip_fw7 *rule7 = (struct ip_fw7 *) rule;

	/* Used to copy commands */
	ipfw_insn *ccmd, *dst;
	int ll = 0, ccmdlen = 0;

	/* Copy of original rule */
	struct ip_fw7 *tmp = malloc(RULE_MAXSIZE, M_TEMP, M_NOWAIT | M_ZERO);
	if (tmp == NULL) {
		return 1; //XXX error
	}

	bcopy(rule7, tmp, RULE_MAXSIZE);

	for (ll = tmp->cmd_len, ccmd = tmp->cmd, dst = rule->cmd ;
			ll > 0 ; ll -= ccmdlen, ccmd += ccmdlen, dst += ccmdlen) {
		ccmdlen = F_LEN(ccmd);
		
		bcopy(ccmd, dst, F_LEN(ccmd)*sizeof(uint32_t));

		if (dst->opcode > O_NAT)
			/* O_REASS doesn't exists in 7.2 version, so
			 * increment opcode if it is after O_REASS
			 */
			dst->opcode++;

		if (ccmdlen > ll) {
			printf("ipfw: opcode %d size truncated\n",
			    ccmd->opcode);
			return EINVAL;
		}
	}

	rule->_pad = tmp->_pad;
	rule->set = tmp->set;
	rule->rulenum = tmp->rulenum;
	rule->cmd_len = tmp->cmd_len;
	rule->act_ofs = tmp->act_ofs;
	rule->next_rule = (struct ip_fw *)tmp->next_rule;
	rule->x_next = (struct ip_fw *)tmp->next;
	rule->cmd_len = tmp->cmd_len;
	rule->id = 0; /* XXX see if is ok = 0 */
	rule->pcnt = tmp->pcnt;
	rule->bcnt = tmp->bcnt;
	rule->timestamp = tmp->timestamp;

	free (tmp, M_TEMP);
	return 0;
}

/* end of file */
