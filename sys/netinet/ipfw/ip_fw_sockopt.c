/*-
 * Copyright (c) 2002 Luigi Rizzo, Universita` di Pisa
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

#define        DEB(x)
#define        DDB(x) x

/*
 * Sockopt support for ipfw. The routines here implement
 * the upper half of the ipfw code.
 */

#if !defined(KLD_MODULE)
#include "opt_ipfw.h"
#include "opt_ipdivert.h"
#include "opt_ipdn.h"
#include "opt_inet.h"
#ifndef INET
#error IPFIREWALL requires INET.
#endif /* INET */
#endif
#include "opt_inet6.h"
#include "opt_ipsec.h"

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
#include <netinet/ip_fw.h>
#include <netinet/ipfw/ip_fw_private.h>
#include <netinet/ip_divert.h>

#include <netgraph/ng_ipfw.h>

#ifdef MAC
#include <security/mac/mac_framework.h>
#endif

MALLOC_DEFINE(M_IPFW, "IpFw/IpAcct", "IpFw/IpAcct chain's");

/*
 * static variables followed by global ones (none in this file)
 */

/*
 * When a rule is added/deleted, clear the next_rule pointers in all rules.
 * These will be reconstructed on the fly as packets are matched.
 */
static void
flush_rule_ptrs(struct ip_fw_chain *chain)
{
	struct ip_fw *rule;

	IPFW_WLOCK_ASSERT(chain);

	chain->id++;

	for (rule = chain->rules; rule; rule = rule->next)
		rule->next_rule = NULL;
}

/*
 * Add a new rule to the list. Copy the rule into a malloc'ed area, then
 * possibly create a rule number and add the rule to the list.
 * Update the rule_number in the input struct so the caller knows it as well.
 */
int
ipfw_add_rule(struct ip_fw_chain *chain, struct ip_fw *input_rule)
{
	struct ip_fw *rule, *f, *prev;
	int l = RULESIZE(input_rule);

	if (chain->rules == NULL && input_rule->rulenum != IPFW_DEFAULT_RULE)
		return (EINVAL);

	rule = malloc(l, M_IPFW, M_NOWAIT | M_ZERO);
	if (rule == NULL)
		return (ENOSPC);

	bcopy(input_rule, rule, l);

	rule->next = NULL;
	rule->next_rule = NULL;

	rule->pcnt = 0;
	rule->bcnt = 0;
	rule->timestamp = 0;

	IPFW_WLOCK(chain);

	if (chain->rules == NULL) {	/* default rule */
		chain->rules = rule;
		rule->id = ++chain->id;
		goto done;
        }

	/*
	 * If rulenum is 0, find highest numbered rule before the
	 * default rule, and add autoinc_step
	 */
	if (V_autoinc_step < 1)
		V_autoinc_step = 1;
	else if (V_autoinc_step > 1000)
		V_autoinc_step = 1000;
	if (rule->rulenum == 0) {
		/*
		 * locate the highest numbered rule before default
		 */
		for (f = chain->rules; f; f = f->next) {
			if (f->rulenum == IPFW_DEFAULT_RULE)
				break;
			rule->rulenum = f->rulenum;
		}
		if (rule->rulenum < IPFW_DEFAULT_RULE - V_autoinc_step)
			rule->rulenum += V_autoinc_step;
		input_rule->rulenum = rule->rulenum;
	}

	/*
	 * Now insert the new rule in the right place in the sorted list.
	 */
	for (prev = NULL, f = chain->rules; f; prev = f, f = f->next) {
		if (f->rulenum > rule->rulenum) { /* found the location */
			if (prev) {
				rule->next = f;
				prev->next = rule;
			} else { /* head insert */
				rule->next = chain->rules;
				chain->rules = rule;
			}
			break;
		}
	}
	flush_rule_ptrs(chain);
	/* chain->id incremented inside flush_rule_ptrs() */
	rule->id = chain->id;
done:
	chain->n_rules++;
	chain->static_len += l;
	IPFW_WUNLOCK(chain);
	return (0);
}

/**
 * Remove a static rule (including derived * dynamic rules)
 * and place it on the ``reap list'' for later reclamation.
 * The caller is in charge of clearing rule pointers to avoid
 * dangling pointers.
 * @return a pointer to the next entry.
 * Arguments are not checked, so they better be correct.
 */
static struct ip_fw *
remove_rule(struct ip_fw_chain *chain, struct ip_fw *rule,
    struct ip_fw *prev)
{
	struct ip_fw *n;
	int l = RULESIZE(rule);

	IPFW_WLOCK_ASSERT(chain);

	n = rule->next;
	ipfw_remove_dyn_children(rule);
	if (prev == NULL)
		chain->rules = n;
	else
		prev->next = n;
	chain->n_rules--;
	chain->static_len -= l;

	rule->next = chain->reap;
	chain->reap = rule;

	return n;
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
		head = head->next;
		free(rule, M_IPFW);
	}
}

/*
 * Remove all rules from a chain (except rules in set RESVD_SET
 * unless kill_default = 1).  The caller is responsible for
 * reclaiming storage for the rules left in chain->reap.
 */
void
ipfw_free_chain(struct ip_fw_chain *chain, int kill_default)
{
	struct ip_fw *prev, *rule;

	IPFW_WLOCK_ASSERT(chain);

	chain->reap = NULL;
	flush_rule_ptrs(chain); /* more efficient to do outside the loop */
	for (prev = NULL, rule = chain->rules; rule ; )
		if (kill_default || rule->set != RESVD_SET)
			rule = remove_rule(chain, rule, prev);
		else {
			prev = rule;
			rule = rule->next;
		}
}

/**
 * Remove all rules with given number, and also do set manipulation.
 * Assumes chain != NULL && *chain != NULL.
 *
 * The argument is an u_int32_t. The low 16 bit are the rule or set number,
 * the next 8 bits are the new set, the top 8 bits are the command:
 *
 *	0	delete rules with given number
 *	1	delete rules with given set number
 *	2	move rules with given number to new set
 *	3	move rules with given set number to new set
 *	4	swap sets with given numbers
 *	5	delete rules with given number and with given set number
 */
static int
del_entry(struct ip_fw_chain *chain, u_int32_t arg)
{
	struct ip_fw *prev = NULL, *rule;
	u_int16_t rulenum;	/* rule or old_set */
	u_int8_t cmd, new_set;

	rulenum = arg & 0xffff;
	cmd = (arg >> 24) & 0xff;
	new_set = (arg >> 16) & 0xff;

	if (cmd > 5 || new_set > RESVD_SET)
		return EINVAL;
	if (cmd == 0 || cmd == 2 || cmd == 5) {
		if (rulenum >= IPFW_DEFAULT_RULE)
			return EINVAL;
	} else {
		if (rulenum > RESVD_SET)	/* old_set */
			return EINVAL;
	}

	IPFW_WLOCK(chain);
	rule = chain->rules;	/* common starting point */
	chain->reap = NULL;	/* prepare for deletions */
	switch (cmd) {
	case 0:	/* delete rules with given number */
		/*
		 * locate first rule to delete
		 */
		for (; rule->rulenum < rulenum; prev = rule, rule = rule->next)
			;
		if (rule->rulenum != rulenum) {
			IPFW_WUNLOCK(chain);
			return EINVAL;
		}

		/*
		 * flush pointers outside the loop, then delete all matching
		 * rules. prev remains the same throughout the cycle.
		 */
		flush_rule_ptrs(chain);
		while (rule->rulenum == rulenum)
			rule = remove_rule(chain, rule, prev);
		break;

	case 1:	/* delete all rules with given set number */
		flush_rule_ptrs(chain);
		while (rule->rulenum < IPFW_DEFAULT_RULE) {
			if (rule->set == rulenum)
				rule = remove_rule(chain, rule, prev);
			else {
				prev = rule;
				rule = rule->next;
			}
		}
		break;

	case 2:	/* move rules with given number to new set */
		for (; rule->rulenum < IPFW_DEFAULT_RULE; rule = rule->next)
			if (rule->rulenum == rulenum)
				rule->set = new_set;
		break;

	case 3: /* move rules with given set number to new set */
		for (; rule->rulenum < IPFW_DEFAULT_RULE; rule = rule->next)
			if (rule->set == rulenum)
				rule->set = new_set;
		break;

	case 4: /* swap two sets */
		for (; rule->rulenum < IPFW_DEFAULT_RULE; rule = rule->next)
			if (rule->set == rulenum)
				rule->set = new_set;
			else if (rule->set == new_set)
				rule->set = rulenum;
		break;

	case 5: /* delete rules with given number and with given set number.
		 * rulenum - given rule number;
		 * new_set - given set number.
		 */
		for (; rule->rulenum < rulenum; prev = rule, rule = rule->next)
			;
		if (rule->rulenum != rulenum) {
			IPFW_WUNLOCK(chain);
			return (EINVAL);
		}
		flush_rule_ptrs(chain);
		while (rule->rulenum == rulenum) {
			if (rule->set == new_set)
				rule = remove_rule(chain, rule, prev);
			else {
				prev = rule;
				rule = rule->next;
			}
		}
	}
	/*
	 * Look for rules to reclaim.  We grab the list before
	 * releasing the lock then reclaim them w/o the lock to
	 * avoid a LOR with dummynet.
	 */
	rule = chain->reap;
	IPFW_WUNLOCK(chain);
	ipfw_reap_rules(rule);
	return 0;
}

/*
 * Clear counters for a specific rule.
 * The enclosing "table" is assumed locked.
 */
static void
clear_counters(struct ip_fw *rule, int log_only)
{
	ipfw_insn_log *l = (ipfw_insn_log *)ACTION_PTR(rule);

	if (log_only == 0) {
		rule->bcnt = rule->pcnt = 0;
		rule->timestamp = 0;
	}
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

	uint16_t rulenum = arg & 0xffff;
	uint8_t set = (arg >> 16) & 0xff;
	uint8_t cmd = (arg >> 24) & 0xff;

	if (cmd > 1)
		return (EINVAL);
	if (cmd == 1 && set > RESVD_SET)
		return (EINVAL);

	IPFW_WLOCK(chain);
	if (rulenum == 0) {
		V_norule_counter = 0;
		for (rule = chain->rules; rule; rule = rule->next) {
			/* Skip rules from another set. */
			if (cmd == 1 && rule->set != set)
				continue;
			clear_counters(rule, log_only);
		}
		msg = log_only ? "All logging counts reset" :
		    "Accounting cleared";
	} else {
		int cleared = 0;
		/*
		 * We can have multiple rules with the same number, so we
		 * need to clear them all.
		 */
		for (rule = chain->rules; rule; rule = rule->next)
			if (rule->rulenum == rulenum) {
				while (rule && rule->rulenum == rulenum) {
					if (cmd == 0 || rule->set == set)
						clear_counters(rule, log_only);
					rule = rule->next;
				}
				cleared = 1;
				break;
			}
		if (!cleared) {	/* we did not find any matching rules */
			IPFW_WUNLOCK(chain);
			return (EINVAL);
		}
		msg = log_only ? "logging count reset" : "cleared";
	}
	IPFW_WUNLOCK(chain);

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
		DEB(printf("ipfw: opcode %d\n", cmd->opcode);)
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
		case O_TCPWIN:
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
			if (cmd->arg1 >= rt_numfibs) {
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
			if (cmd->arg1 >= IPFW_TABLES_MAX) {
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
		case O_TAGGED:
			if (cmdlen < 1 || cmdlen > 31)
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
#ifdef	IPFIREWALL_FORWARD
			if (cmdlen != F_INSN_SIZE(ipfw_insn_sa))
				goto bad_size;
			goto check_action;
#else
			return EINVAL;
#endif

		case O_DIVERT:
		case O_TEE:
			if (ip_divert_ptr == NULL)
				return EINVAL;
			else
				goto check_size;
		case O_NETGRAPH:
		case O_NGTEE:
			if (!NG_IPFW_LOADED)
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
#ifdef INET6
		case O_UNREACH6:
#endif
		case O_SKIPTO:
		case O_REASS:
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
 * Copy the static and dynamic rules to the supplied buffer
 * and return the amount of space actually used.
 */
static size_t
ipfw_getrules(struct ip_fw_chain *chain, void *buf, size_t space)
{
	char *bp = buf;
	char *ep = bp + space;
	struct ip_fw *rule;
	int i;
	time_t	boot_seconds;

        boot_seconds = boottime.tv_sec;
	/* XXX this can take a long time and locking will block packet flow */
	IPFW_RLOCK(chain);
	for (rule = chain->rules; rule ; rule = rule->next) {
		/*
		 * Verify the entry fits in the buffer in case the
		 * rules changed between calculating buffer space and
		 * now.  This would be better done using a generation
		 * number but should suffice for now.
		 */
		i = RULESIZE(rule);
		if (bp + i <= ep) {
			bcopy(rule, bp, i);
			/*
			 * XXX HACK. Store the disable mask in the "next"
			 * pointer in a wild attempt to keep the ABI the same.
			 * Why do we do this on EVERY rule?
			 */
			bcopy(&V_set_disable,
			    &(((struct ip_fw *)bp)->next_rule),
			    sizeof(V_set_disable));
			if (((struct ip_fw *)bp)->timestamp)
				((struct ip_fw *)bp)->timestamp += boot_seconds;
			bp += i;
		}
	}
	IPFW_RUNLOCK(chain);
	ipfw_get_dynamic(&bp, ep); /* protected by the dynamic lock */
	return (bp - (char *)buf);
}


/**
 * {set|get}sockopt parser.
 */
int
ipfw_ctl(struct sockopt *sopt)
{
#define	RULE_MAXSIZE	(256*sizeof(u_int32_t))
	int error;
	size_t size;
	struct ip_fw *buf, *rule;
	struct ip_fw_chain *chain;
	u_int32_t rulenum[2];

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

	switch (sopt->sopt_name) {
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
		size = chain->static_len;	/* size of static rules */
		size += ipfw_dyn_len();

		if (size >= sopt->sopt_valsize)
			break;
		/*
		 * XXX todo: if the user passes a short length just to know
		 * how much room is needed, do not bother filling up the
		 * buffer, just jump to the sooptcopyout.
		 */
		buf = malloc(size, M_TEMP, M_WAITOK);
		error = sooptcopyout(sopt, buf,
				ipfw_getrules(chain, buf, size));
		free(buf, M_TEMP);
		break;

	case IP_FW_FLUSH:
		/*
		 * Normally we cannot release the lock on each iteration.
		 * We could do it here only because we start from the head all
		 * the times so there is no risk of missing some entries.
		 * On the other hand, the risk is that we end up with
		 * a very inconsistent ruleset, so better keep the lock
		 * around the whole cycle.
		 *
		 * XXX this code can be improved by resetting the head of
		 * the list to point to the default rule, and then freeing
		 * the old list without the need for a lock.
		 */

		IPFW_WLOCK(chain);
		ipfw_free_chain(chain, 0 /* keep default rule */);
		rule = chain->reap;
		IPFW_WUNLOCK(chain);
		ipfw_reap_rules(rule);
		break;

	case IP_FW_ADD:
		rule = malloc(RULE_MAXSIZE, M_TEMP, M_WAITOK);
		error = sooptcopyin(sopt, rule, RULE_MAXSIZE,
			sizeof(struct ip_fw) );
		if (error == 0)
			error = check_ipfw_struct(rule, sopt->sopt_valsize);
		if (error == 0) {
			error = ipfw_add_rule(chain, rule);
			size = RULESIZE(rule);
			if (!error && sopt->sopt_dir == SOPT_GET)
				error = sooptcopyout(sopt, rule, size);
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
			V_set_disable =
			    (V_set_disable | rulenum[0]) & ~rulenum[1] &
			    ~(1<<RESVD_SET); /* set RESVD_SET always enabled */
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
			    ent.addr, ent.masklen, ent.value);
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
			    ent.addr, ent.masklen);
		}
		break;

	case IP_FW_TABLE_FLUSH:
		{
			u_int16_t tbl;

			error = sooptcopyin(sopt, &tbl,
			    sizeof(tbl), sizeof(tbl));
			if (error)
				break;
			IPFW_WLOCK(chain);
			error = ipfw_flush_table(chain, tbl);
			IPFW_WUNLOCK(chain);
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
/* end of file */
