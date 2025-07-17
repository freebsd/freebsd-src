/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Yandex LLC
 * Copyright (c) 2025 Andrey V. Elsukov <ae@FreeBSD.org>
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
/*
 * Example of compatibility layer for ipfw's rule management routines.
 */

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipfw.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/rmlock.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/fnv_hash.h>
#include <net/if.h>
#include <net/pfil.h>
#include <net/route.h>
#include <net/vnet.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <netinet/in.h>
#include <netinet/ip_var.h> /* hooks */
#include <netinet/ip_fw.h>

#include <netpfil/ipfw/ip_fw_private.h>
#include <netpfil/ipfw/ip_fw_table.h>

#ifdef MAC
#include <security/mac/mac_framework.h>
#endif

/*
 * These structures were used by IP_FW3 socket option with version 0.
 */
typedef struct _ipfw_dyn_rule_v0 {
	ipfw_dyn_rule	*next;		/* linked list of rules.	*/
	struct ip_fw *rule;		/* pointer to rule		*/
	/* 'rule' is used to pass up the rule number (from the parent)	*/

	ipfw_dyn_rule *parent;		/* pointer to parent rule	*/
	u_int64_t	pcnt;		/* packet match counter		*/
	u_int64_t	bcnt;		/* byte match counter		*/
	struct ipfw_flow_id id;		/* (masked) flow id		*/
	u_int32_t	expire;		/* expire time			*/
	u_int32_t	bucket;		/* which bucket in hash table	*/
	u_int32_t	state;		/* state of this rule (typically a
					 * combination of TCP flags)
					 */
	u_int32_t	ack_fwd;	/* most recent ACKs in forward	*/
	u_int32_t	ack_rev;	/* and reverse directions (used	*/
					/* to generate keepalives)	*/
	u_int16_t	dyn_type;	/* rule type			*/
	u_int16_t	count;		/* refcount			*/
	u_int16_t	kidx;		/* index of named object */
} __packed __aligned(8) ipfw_dyn_rule_v0;

typedef struct _ipfw_obj_dyntlv_v0 {
	ipfw_obj_tlv	head;
	ipfw_dyn_rule_v0 state;
} ipfw_obj_dyntlv_v0;

typedef struct _ipfw_obj_ntlv_v0 {
	ipfw_obj_tlv	head;		/* TLV header			*/
	uint16_t	idx;		/* Name index			*/
	uint8_t		set;		/* set, if applicable		*/
	uint8_t		type;		/* object type, if applicable	*/
	uint32_t	spare;		/* unused			*/
	char		name[64];	/* Null-terminated name		*/
} ipfw_obj_ntlv_v0;

typedef struct _ipfw_range_tlv_v0 {
	ipfw_obj_tlv	head;		/* TLV header			*/
	uint32_t	flags;		/* Range flags			*/
	uint16_t	start_rule;	/* Range start			*/
	uint16_t	end_rule;	/* Range end			*/
	uint32_t	set;		/* Range set to match		 */
	uint32_t	new_set;	/* New set to move/swap to	*/
} ipfw_range_tlv_v0;

typedef struct _ipfw_range_header_v0 {
	ip_fw3_opheader	opheader;	/* IP_FW3 opcode		*/
	ipfw_range_tlv_v0 range;
} ipfw_range_header_v0;

typedef struct	_ipfw_insn_limit_v0 {
	ipfw_insn o;
	uint8_t _pad;
	uint8_t limit_mask;
	uint16_t conn_limit;
} ipfw_insn_limit_v0;

typedef struct	_ipfw_obj_tentry_v0 {
	ipfw_obj_tlv	head;		/* TLV header			*/
	uint8_t		subtype;	/* subtype (IPv4,IPv6)		*/
	uint8_t		masklen;	/* mask length			*/
	uint8_t		result;		/* request result		*/
	uint8_t		spare0;
	uint16_t	idx;		/* Table name index		*/
	uint16_t	spare1;
	union {
		/* Longest field needs to be aligned by 8-byte boundary	*/
		struct in_addr		addr;	/* IPv4 address	*/
		uint32_t		key;	/* uid/gid/port	*/
		struct in6_addr		addr6;	/* IPv6 address	*/
		char	iface[IF_NAMESIZE];	/* interface name */
		struct tflow_entry	flow;
	} k;
	union {
		ipfw_table_value	value;	/* value data */
		uint32_t		kidx;	/* value kernel index */
	} v;
} ipfw_obj_tentry_v0;

static sopt_handler_f dump_config_v0, add_rules_v0, del_rules_v0,
    clear_rules_v0, move_rules_v0, manage_sets_v0, dump_soptcodes_v0,
    dump_srvobjects_v0;

static struct ipfw_sopt_handler scodes[] = {
    { IP_FW_XGET,		IP_FW3_OPVER_0, HDIR_GET, dump_config_v0 },
    { IP_FW_XADD,		IP_FW3_OPVER_0, HDIR_BOTH, add_rules_v0 },
    { IP_FW_XDEL,		IP_FW3_OPVER_0, HDIR_BOTH, del_rules_v0 },
    { IP_FW_XZERO,		IP_FW3_OPVER_0, HDIR_SET, clear_rules_v0 },
    { IP_FW_XRESETLOG,		IP_FW3_OPVER_0, HDIR_SET, clear_rules_v0 },
    { IP_FW_XMOVE,		IP_FW3_OPVER_0, HDIR_SET, move_rules_v0 },
    { IP_FW_SET_SWAP,		IP_FW3_OPVER_0, HDIR_SET, manage_sets_v0 },
    { IP_FW_SET_MOVE,		IP_FW3_OPVER_0, HDIR_SET, manage_sets_v0 },
    { IP_FW_SET_ENABLE,		IP_FW3_OPVER_0, HDIR_SET, manage_sets_v0 },
    { IP_FW_DUMP_SOPTCODES,	IP_FW3_OPVER_0, HDIR_GET, dump_soptcodes_v0 },
    { IP_FW_DUMP_SRVOBJECTS,	IP_FW3_OPVER_0, HDIR_GET, dump_srvobjects_v0 },
};

static int
dump_config_v0(struct ip_fw_chain *chain, ip_fw3_opheader *op3,
    struct sockopt_data *sd)
{
	return (EOPNOTSUPP);
}

/*
 * Calculate the size adjust needed to store opcodes converted from v0
 * to v1.
 */
static int
adjust_size_v0(ipfw_insn *cmd)
{
	int cmdlen, adjust;

	cmdlen = F_LEN(cmd);
	switch (cmd->opcode) {
	case O_CHECK_STATE:
	case O_KEEP_STATE:
	case O_PROBE_STATE:
	case O_EXTERNAL_ACTION:
	case O_EXTERNAL_INSTANCE:
		adjust = F_INSN_SIZE(ipfw_insn_kidx) - cmdlen;
		break;
	case O_LIMIT:
		adjust = F_INSN_SIZE(ipfw_insn_limit) - cmdlen;
		break;
	case O_IP_SRC_LOOKUP:
	case O_IP_DST_LOOKUP:
	case O_IP_FLOW_LOOKUP:
	case O_MAC_SRC_LOOKUP:
	case O_MAC_DST_LOOKUP:
		if (cmdlen == F_INSN_SIZE(ipfw_insn))
			adjust = F_INSN_SIZE(ipfw_insn_kidx) - cmdlen;
		else
			adjust = F_INSN_SIZE(ipfw_insn_table) - cmdlen;
		break;
	case O_SKIPTO:
	case O_CALLRETURN:
		adjust = F_INSN_SIZE(ipfw_insn_u32) - cmdlen;
		break;
	default:
		adjust = 0;
	}
	return (adjust);
}

static int
parse_rules_v0(struct ip_fw_chain *chain, ip_fw3_opheader *op3,
    struct sockopt_data *sd, ipfw_obj_ctlv **prtlv,
    struct rule_check_info **pci)
{
	ipfw_obj_ctlv *ctlv, *rtlv, *tstate;
	ipfw_obj_ntlv_v0 *ntlv;
	struct rule_check_info *ci, *cbuf;
	struct ip_fw_rule *r;
	size_t count, clen, read, rsize;
	uint32_t rulenum;
	int idx, error;

	op3 = (ip_fw3_opheader *)ipfw_get_sopt_space(sd, sd->valsize);
	ctlv = (ipfw_obj_ctlv *)(op3 + 1);
	read = sizeof(ip_fw3_opheader);
	if (read + sizeof(*ctlv) > sd->valsize)
		return (EINVAL);

	rtlv = NULL;
	tstate = NULL;
	cbuf = NULL;
	/* Table names or other named objects. */
	if (ctlv->head.type == IPFW_TLV_TBLNAME_LIST) {
		/* Check size and alignment. */
		clen = ctlv->head.length;
		if (read + clen > sd->valsize || clen < sizeof(*ctlv) ||
		    (clen % sizeof(uint64_t)) != 0)
			return (EINVAL);
		/* Check for validness. */
		count = (ctlv->head.length - sizeof(*ctlv)) / sizeof(*ntlv);
		if (ctlv->count != count || ctlv->objsize != sizeof(*ntlv))
			return (EINVAL);
		/*
		 * Check each TLV.
		 * Ensure TLVs are sorted ascending and
		 * there are no duplicates.
		 */
		idx = -1;
		ntlv = (ipfw_obj_ntlv_v0 *)(ctlv + 1);
		while (count > 0) {
			if (ntlv->head.length != sizeof(ipfw_obj_ntlv_v0))
				return (EINVAL);

			error = ipfw_check_object_name_generic(ntlv->name);
			if (error != 0)
				return (error);

			if (ntlv->idx <= idx)
				return (EINVAL);

			idx = ntlv->idx;
			count--;
			ntlv++;
		}

		tstate = ctlv;
		read += ctlv->head.length;
		ctlv = (ipfw_obj_ctlv *)((caddr_t)ctlv + ctlv->head.length);

		if (read + sizeof(*ctlv) > sd->valsize)
			return (EINVAL);
	}

	/* List of rules. */
	if (ctlv->head.type == IPFW_TLV_RULE_LIST) {
		clen = ctlv->head.length;
		if (read + clen > sd->valsize || clen < sizeof(*ctlv) ||
		    (clen % sizeof(uint64_t)) != 0)
			return (EINVAL);

		clen -= sizeof(*ctlv);
		if (ctlv->count == 0 ||
		    ctlv->count > clen / sizeof(struct ip_fw_rule))
			return (EINVAL);

		/* Allocate state for each rule */
		cbuf = malloc(ctlv->count * sizeof(struct rule_check_info),
		    M_TEMP, M_WAITOK | M_ZERO);

		/*
		 * Check each rule for validness.
		 * Ensure numbered rules are sorted ascending
		 * and properly aligned
		 */
		rulenum = 0;
		count = 0;
		error = 0;
		ci = cbuf;
		r = (struct ip_fw_rule *)(ctlv + 1);
		while (clen > 0) {
			rsize = RULEUSIZE1(r);
			if (rsize > clen || count > ctlv->count) {
				error = EINVAL;
				break;
			}
			ci->ctlv = tstate;
			ci->version = IP_FW3_OPVER_0;
			error = ipfw_check_rule(r, rsize, ci);
			if (error != 0)
				break;

			/* Check sorting */
			if (r->rulenum != 0 && r->rulenum < rulenum) {
				printf("ipfw: wrong order: rulenum %u"
				    " vs %u\n", r->rulenum, rulenum);
				error = EINVAL;
				break;
			}
			rulenum = r->rulenum;
			ci->urule = (caddr_t)r;
			clen -= rsize;
			r = (struct ip_fw_rule *)((caddr_t)r + rsize);
			count++;
			ci++;
		}

		if (ctlv->count != count || error != 0) {
			free(cbuf, M_TEMP);
			return (EINVAL);
		}

		rtlv = ctlv;
		read += ctlv->head.length;
		ctlv = (ipfw_obj_ctlv *)((caddr_t)ctlv + ctlv->head.length);
	}

	if (read != sd->valsize || rtlv == NULL) {
		free(cbuf, M_TEMP);
		return (EINVAL);
	}

	*prtlv = rtlv;
	*pci = cbuf;
	return (0);
}

static void
convert_v0_to_v1(struct rule_check_info *ci, int rule_len)
{
	struct ip_fw_rule *urule;
	struct ip_fw *krule;
	ipfw_insn *src, *dst;
	int l, cmdlen, newlen;

	urule = (struct ip_fw_rule *)ci->urule;
	krule = ci->krule;
	for (l = urule->cmd_len, src = urule->cmd, dst = krule->cmd;
	    l > 0 && rule_len > 0;
	    l -= cmdlen, src += cmdlen,
	    rule_len -= newlen, dst += newlen) {
		cmdlen = F_LEN(src);
		switch (src->opcode) {
		case O_CHECK_STATE:
		case O_KEEP_STATE:
		case O_PROBE_STATE:
		case O_EXTERNAL_ACTION:
		case O_EXTERNAL_INSTANCE:
			newlen = F_INSN_SIZE(ipfw_insn_kidx);
			insntod(dst, kidx)->kidx = src->arg1;
			break;
		case O_LIMIT:
			newlen = F_INSN_SIZE(ipfw_insn_limit);
			insntod(dst, limit)->kidx = src->arg1;
			insntod(dst, limit)->limit_mask =
			    insntoc(src, limit)->limit_mask;
			insntod(dst, limit)->conn_limit =
			    insntoc(src, limit)->conn_limit;
			break;
		case O_IP_DST_LOOKUP:
			if (cmdlen == F_INSN_SIZE(ipfw_insn) + 2) {
				/* lookup type stored in d[1] */
				dst->arg1 = insntoc(src, table)->value;
			}
		case O_IP_SRC_LOOKUP:
		case O_IP_FLOW_LOOKUP:
		case O_MAC_SRC_LOOKUP:
		case O_MAC_DST_LOOKUP:
			if (cmdlen == F_INSN_SIZE(ipfw_insn)) {
				newlen = F_INSN_SIZE(ipfw_insn_kidx);
				insntod(dst, kidx)->kidx = src->arg1;
			} else {
				newlen = F_INSN_SIZE(ipfw_insn_table);
				insntod(dst, table)->kidx = src->arg1;
				insntod(dst, table)->value =
				    insntoc(src, u32)->d[0];
			}
			break;
		case O_CALLRETURN:
		case O_SKIPTO:
			newlen = F_INSN_SIZE(ipfw_insn_u32);
			insntod(dst, u32)->d[0] = src->arg1;
			break;
		default:
			newlen = cmdlen;
			memcpy(dst, src, sizeof(uint32_t) * newlen);
			continue;
		}
		dst->opcode = src->opcode;
		dst->len = (src->len & (F_NOT | F_OR)) | newlen;
	}
}

/*
 * Copy rule @urule from v0 userland format to kernel @krule.
 */
static void
import_rule_v0(struct ip_fw_chain *chain, struct rule_check_info *ci)
{
	struct ip_fw_rule *urule;
	struct ip_fw *krule;
	ipfw_insn *cmd;
	int l, cmdlen, adjust, aadjust;

	urule = (struct ip_fw_rule *)ci->urule;
	l = urule->cmd_len;
	cmd = urule->cmd;
	adjust = aadjust = 0;

	/* Scan all opcodes and determine the needed size */
	while (l > 0) {
		adjust += adjust_size_v0(cmd);
		if (ACTION_PTR(urule) < cmd)
			aadjust = adjust;
		cmdlen = F_LEN(cmd);
		l -= cmdlen;
		cmd += cmdlen;
	}

	cmdlen = urule->cmd_len + adjust;
	krule = ci->krule = ipfw_alloc_rule(chain, /* RULEKSIZE1(cmdlen) */
	    roundup2(sizeof(struct ip_fw) + cmdlen * 4 - 4, 8));

	krule->act_ofs = urule->act_ofs + aadjust;
	krule->cmd_len = urule->cmd_len + adjust;

	if (adjust != 0)
		printf("%s: converted rule %u: cmd_len %u -> %u, "
		    "act_ofs %u -> %u\n", __func__, urule->rulenum,
		    urule->cmd_len, krule->cmd_len, urule->act_ofs,
		    krule->act_ofs);

	krule->rulenum = urule->rulenum;
	krule->set = urule->set;
	krule->flags = urule->flags;

	/* Save rulenum offset */
	ci->urule_numoff = offsetof(struct ip_fw_rule, rulenum);
	convert_v0_to_v1(ci, cmdlen);
}

static int
add_rules_v0(struct ip_fw_chain *chain, ip_fw3_opheader *op3,
    struct sockopt_data *sd)
{
	ipfw_obj_ctlv *rtlv;
	struct rule_check_info *ci, *nci;
	int i, ret;

	/*
	 * Check rules buffer for validness.
	 */
	ret = parse_rules_v0(chain, op3, sd, &rtlv, &nci);
	if (ret != 0)
		return (ret);
	/*
	 * Allocate storage for the kernel representation of rules.
	 */
	for (i = 0, ci = nci; i < rtlv->count; i++, ci++)
		import_rule_v0(chain, ci);
	/*
	 * Try to add new rules to the chain.
	 */
	if ((ret = ipfw_commit_rules(chain, nci, rtlv->count)) != 0) {
		for (i = 0, ci = nci; i < rtlv->count; i++, ci++)
			ipfw_free_rule(ci->krule);
	}
	/* Cleanup after ipfw_parse_rules() */
	free(nci, M_TEMP);
	return (ret);
}

static int
check_range_tlv_v0(const ipfw_range_tlv_v0 *rt, ipfw_range_tlv *crt)
{
	if (rt->head.length != sizeof(*rt))
		return (1);
	if (rt->start_rule > rt->end_rule)
		return (1);
	if (rt->set >= IPFW_MAX_SETS || rt->new_set >= IPFW_MAX_SETS)
		return (1);
	if ((rt->flags & IPFW_RCFLAG_USER) != rt->flags)
		return (1);

	crt->head = rt->head;
	crt->head.length = sizeof(*crt);
	crt->flags = rt->flags;
	crt->start_rule = rt->start_rule;
	crt->end_rule = rt->end_rule;
	crt->set = rt->set;
	crt->new_set = rt->new_set;
	return (0);
}

static int
del_rules_v0(struct ip_fw_chain *chain, ip_fw3_opheader *op3,
    struct sockopt_data *sd)
{
	ipfw_range_tlv rv;
	ipfw_range_header_v0 *rh;
	int error, ndel;

	if (sd->valsize != sizeof(*rh))
		return (EINVAL);

	rh = (ipfw_range_header_v0 *)ipfw_get_sopt_space(sd, sd->valsize);
	if (check_range_tlv_v0(&rh->range, &rv) != 0)
		return (EINVAL);

	ndel = 0;
	if ((error = delete_range(chain, &rv, &ndel)) != 0)
		return (error);

	/* Save number of rules deleted */
	rh->range.new_set = ndel;
	return (0);
}

static int
clear_rules_v0(struct ip_fw_chain *chain, ip_fw3_opheader *op3,
    struct sockopt_data *sd)
{
	return (EOPNOTSUPP);
}

static int
move_rules_v0(struct ip_fw_chain *chain, ip_fw3_opheader *op3,
    struct sockopt_data *sd)
{
	return (EOPNOTSUPP);
}

static int
manage_sets_v0(struct ip_fw_chain *chain, ip_fw3_opheader *op3,
    struct sockopt_data *sd)
{
	return (EOPNOTSUPP);
}

static int
dump_soptcodes_v0(struct ip_fw_chain *chain, ip_fw3_opheader *op3,
    struct sockopt_data *sd)
{
	return (EOPNOTSUPP);
}

static int
dump_srvobjects_v0(struct ip_fw_chain *chain, ip_fw3_opheader *op3,
    struct sockopt_data *sd)
{
	return (EOPNOTSUPP);
}

static enum ipfw_opcheck_result
check_opcode_compat(ipfw_insn **pcmd, int *plen, struct rule_check_info *ci)
{
	ipfw_insn *cmd;
	size_t cmdlen;

	if (ci->version != IP_FW3_OPVER_0)
		return (FAILED);

	cmd = *pcmd;
	cmdlen = F_LEN(cmd);
	switch (cmd->opcode) {
	case O_PROBE_STATE:
	case O_KEEP_STATE:
		if (cmdlen != F_INSN_SIZE(ipfw_insn))
			return (BAD_SIZE);
		ci->object_opcodes++;
		break;
	case O_LIMIT:
		if (cmdlen != F_INSN_SIZE(ipfw_insn_limit_v0))
			return (BAD_SIZE);
		ci->object_opcodes++;
		break;
	case O_IP_SRC_LOOKUP:
		if (cmdlen > F_INSN_SIZE(ipfw_insn_u32))
			return (BAD_SIZE);
		/* FALLTHROUGH */
	case O_IP_DST_LOOKUP:
		if (cmdlen != F_INSN_SIZE(ipfw_insn) &&
		    cmdlen != F_INSN_SIZE(ipfw_insn_u32) + 1 &&
		    cmdlen != F_INSN_SIZE(ipfw_insn_u32))
			return (BAD_SIZE);
		if (cmd->arg1 >= V_fw_tables_max) {
			printf("ipfw: invalid table number %u\n",
			    cmd->arg1);
			return (FAILED);
		}
		ci->object_opcodes++;
		break;
	case O_IP_FLOW_LOOKUP:
		if (cmdlen != F_INSN_SIZE(ipfw_insn) &&
		    cmdlen != F_INSN_SIZE(ipfw_insn_u32))
			return (BAD_SIZE);
		if (cmd->arg1 >= V_fw_tables_max) {
			printf("ipfw: invalid table number %u\n",
			    cmd->arg1);
			return (FAILED);
		}
		ci->object_opcodes++;
		break;
	case O_CHECK_STATE:
		ci->object_opcodes++;
		/* FALLTHROUGH */
	case O_SKIPTO:
	case O_CALLRETURN:
		if (cmdlen != F_INSN_SIZE(ipfw_insn))
			return (BAD_SIZE);
		return (CHECK_ACTION);

	case O_EXTERNAL_ACTION:
		if (cmd->arg1 == 0 ||
		    cmdlen != F_INSN_SIZE(ipfw_insn)) {
			printf("ipfw: invalid external "
			    "action opcode\n");
			return (FAILED);
		}
		ci->object_opcodes++;
		/*
		 * Do we have O_EXTERNAL_INSTANCE or O_EXTERNAL_DATA
		 * opcode?
		 */
		if (*plen != cmdlen) {
			*plen -= cmdlen;
			*pcmd = cmd += cmdlen;
			cmdlen = F_LEN(cmd);
			if (cmd->opcode == O_EXTERNAL_DATA)
				return (CHECK_ACTION);
			if (cmd->opcode != O_EXTERNAL_INSTANCE) {
				printf("ipfw: invalid opcode "
				    "next to external action %u\n",
				    cmd->opcode);
				return (FAILED);
			}
			if (cmd->arg1 == 0 ||
			    cmdlen != F_INSN_SIZE(ipfw_insn)) {
				printf("ipfw: invalid external "
				    "action instance opcode\n");
				return (FAILED);
			}
			ci->object_opcodes++;
		}
		return (CHECK_ACTION);

	default:
		return (ipfw_check_opcode(pcmd, plen, ci));
	}
	return (SUCCESS);
}

static int
ipfw_compat_modevent(module_t mod, int type, void *unused)
{
	switch (type) {
	case MOD_LOAD:
		IPFW_ADD_SOPT_HANDLER(1, scodes);
		ipfw_register_compat(check_opcode_compat);
		break;
	case MOD_UNLOAD:
		ipfw_unregister_compat();
		IPFW_DEL_SOPT_HANDLER(1, scodes);
		break;
	default:
		return (EOPNOTSUPP);
	}
	return (0);
}

static moduledata_t ipfw_compat_mod = {
	"ipfw_compat",
	ipfw_compat_modevent,
	0
};

/* Define startup order. */
#define	IPFW_COMPAT_SI_SUB_FIREWALL	SI_SUB_PROTO_FIREWALL
#define	IPFW_COMPAT_MODEVENT_ORDER	(SI_ORDER_ANY - 128) /* after ipfw */
#define	IPFW_COMPAT_MODULE_ORDER	(IPFW_COMPAT_MODEVENT_ORDER + 1)

DECLARE_MODULE(ipfw_compat, ipfw_compat_mod, IPFW_COMPAT_SI_SUB_FIREWALL,
    IPFW_COMPAT_MODULE_ORDER);
MODULE_DEPEND(ipfw_compat, ipfw, 3, 3, 3);
MODULE_VERSION(ipfw_compat, 1);
