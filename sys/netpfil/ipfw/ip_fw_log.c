/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
 */

#include <sys/cdefs.h>
/*
 * Logging support for ipfw
 */

#include "opt_ipfw.h"
#include "opt_inet.h"
#ifndef INET
#error IPFIREWALL requires INET.
#endif /* INET */
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <net/ethernet.h> /* for ETHERTYPE_IP */
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_var.h>
#include <net/if_private.h>
#include <net/vnet.h>
#include <net/route.h>
#include <net/route/route_var.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <netinet/ip_fw.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>

#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#ifdef INET6
#include <netinet6/in6_var.h>	/* ip6_sprintf() */
#endif

#include <netpfil/ipfw/ip_fw_private.h>

#ifdef MAC
#include <security/mac/mac_framework.h>
#endif

/*
 * L3HDR maps an ipv4 pointer into a layer3 header pointer of type T
 * Other macros just cast void * into the appropriate type
 */
#define	L3HDR(T, ip)	((T *)((u_int32_t *)(ip) + (ip)->ip_hl))
#define	TCP(p)		((struct tcphdr *)(p))
#define	SCTP(p)		((struct sctphdr *)(p))
#define	UDP(p)		((struct udphdr *)(p))
#define	ICMP(p)		((struct icmphdr *)(p))
#define	ICMP6(p)	((struct icmp6_hdr *)(p))

#ifdef __APPLE__
#undef snprintf
#define snprintf	sprintf
#define SNPARGS(buf, len) buf + len
#define SNP(buf) buf
#else	/* !__APPLE__ */
#define SNPARGS(buf, len) buf + len, sizeof(buf) > len ? sizeof(buf) - len : 0
#define SNP(buf) buf, sizeof(buf)
#endif /* !__APPLE__ */

#define	TARG(k, f)	IP_FW_ARG_TABLEARG(chain, k, f)

/*
 * XXX this function alone takes about 2Kbytes of code!
 */
static void
ipfw_log_syslog(struct ip_fw_chain *chain, struct ip_fw *f, u_int hlen,
    struct ip_fw_args *args, u_short offset, uint32_t tablearg, struct ip *ip)
{
	char *action;
	int limit_reached = 0;
	char action2[92], proto[128], fragment[32], mark_str[24];

	fragment[0] = '\0';
	proto[0] = '\0';

	if (f == NULL) {	/* bogus pkt */
		if (V_verbose_limit != 0 && V_norule_counter >= V_verbose_limit)
			return;
		V_norule_counter++;
		if (V_norule_counter == V_verbose_limit)
			limit_reached = V_verbose_limit;
		action = "Refuse";
	} else {	/* O_LOG is the first action, find the real one */
		ipfw_insn *cmd = ACTION_PTR(f);
		ipfw_insn_log *l = (ipfw_insn_log *)cmd;

		if (l->max_log != 0 && l->log_left == 0)
			return;
		l->log_left--;
		if (l->log_left == 0)
			limit_reached = l->max_log;
		cmd += F_LEN(cmd);	/* point to first action */
		if (cmd->opcode == O_ALTQ) {
			ipfw_insn_altq *altq = (ipfw_insn_altq *)cmd;

			snprintf(SNPARGS(action2, 0), "Altq %d",
				altq->qid);
			cmd += F_LEN(cmd);
		}
		if (cmd->opcode == O_PROB || cmd->opcode == O_TAG)
			cmd += F_LEN(cmd);

		action = action2;
		switch (cmd->opcode) {
		case O_DENY:
			action = "Deny";
			break;

		case O_REJECT:
			if (cmd->arg1==ICMP_REJECT_RST)
				action = "Reset";
			else if (cmd->arg1==ICMP_REJECT_ABORT)
				action = "Abort";
			else if (cmd->arg1==ICMP_UNREACH_HOST)
				action = "Reject";
			else
				snprintf(SNPARGS(action2, 0), "Unreach %d",
					cmd->arg1);
			break;

		case O_UNREACH6:
			if (cmd->arg1==ICMP6_UNREACH_RST)
				action = "Reset";
			else if (cmd->arg1==ICMP6_UNREACH_ABORT)
				action = "Abort";
			else
				snprintf(SNPARGS(action2, 0), "Unreach %d",
					cmd->arg1);
			break;

		case O_ACCEPT:
			action = "Accept";
			break;
		case O_COUNT:
			action = "Count";
			break;
		case O_DIVERT:
			snprintf(SNPARGS(action2, 0), "Divert %d",
				TARG(cmd->arg1, divert));
			break;
		case O_TEE:
			snprintf(SNPARGS(action2, 0), "Tee %d",
				TARG(cmd->arg1, divert));
			break;
		case O_SETDSCP:
			snprintf(SNPARGS(action2, 0), "SetDscp %d",
				TARG(cmd->arg1, dscp) & 0x3F);
			break;
		case O_SETFIB:
			snprintf(SNPARGS(action2, 0), "SetFib %d",
				TARG(cmd->arg1, fib) & 0x7FFF);
			break;
		case O_SKIPTO:
			snprintf(SNPARGS(action2, 0), "SkipTo %d",
			    TARG(insntod(cmd, u32)->d[0], skipto));
			break;
		case O_PIPE:
			snprintf(SNPARGS(action2, 0), "Pipe %d",
				TARG(cmd->arg1, pipe));
			break;
		case O_QUEUE:
			snprintf(SNPARGS(action2, 0), "Queue %d",
				TARG(cmd->arg1, pipe));
			break;
		case O_FORWARD_IP:
			if (IS_IP4_FLOW_ID(&args->f_id)) {
				char buf[INET_ADDRSTRLEN];
				const struct sockaddr_in *sin = &insntod(cmd, sa)->sa;
				int len;

				/* handle fwd tablearg */
				if (sin->sin_addr.s_addr == INADDR_ANY) {
					struct in_addr tmp;

					tmp.s_addr = htonl(
					    TARG_VAL(chain, tablearg, nh4));
					inet_ntoa_r(tmp, buf);
				} else
					inet_ntoa_r(sin->sin_addr, buf);
				len = snprintf(SNPARGS(action2, 0),
				    "Forward to %s", buf);
				if (sin->sin_port != 0)
					snprintf(SNPARGS(action2, len), ":%d",
					    sin->sin_port);
			}
			/* FALLTHROUGH */
#ifdef INET6
		case O_FORWARD_IP6:
			if (IS_IP6_FLOW_ID(&args->f_id)) {
				char buf[INET6_ADDRSTRLEN];
				struct sockaddr_in6 tmp;
				const struct sockaddr_in *sin = &insntod(cmd, sa)->sa;
				struct sockaddr_in6 *sin6 = &insntod(cmd, sa6)->sa;
				int len;

				if (cmd->opcode == O_FORWARD_IP &&
				    sin->sin_addr.s_addr == INADDR_ANY) {
					sin6 = &tmp;
					sin6->sin6_addr =
					    TARG_VAL(chain, tablearg, nh6);
					sin6->sin6_scope_id =
					    TARG_VAL(chain, tablearg, zoneid);
					sin6->sin6_port = sin->sin_port;
				}

				ip6_sprintf(buf, &sin6->sin6_addr);
				if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr) &&
				    sin6->sin6_scope_id != 0)
					len = snprintf(SNPARGS(action2, 0),
					    "Forward to [%s%%%u]",
					    buf, sin6->sin6_scope_id);
				else
					len = snprintf(SNPARGS(action2, 0),
					    "Forward to [%s]", buf);
				if (sin6->sin6_port != 0)
					snprintf(SNPARGS(action2, len), ":%u",
					    sin6->sin6_port);
			}
#endif
			break;
		case O_NETGRAPH:
			snprintf(SNPARGS(action2, 0), "Netgraph %d",
				cmd->arg1);
			break;
		case O_NGTEE:
			snprintf(SNPARGS(action2, 0), "Ngtee %d",
				cmd->arg1);
			break;
		case O_NAT:
			action = "Nat";
 			break;
		case O_REASS:
			action = "Reass";
			break;
		case O_CALLRETURN:
			if (cmd->len & F_NOT)
				snprintf(SNPARGS(action2, 0), "Return %s",
				    cmd->arg1 == RETURN_NEXT_RULENUM ?
				    "next-rulenum": "next-rule");
			else
				snprintf(SNPARGS(action2, 0), "Call %d",
				    TARG(insntod(cmd, u32)->d[0], skipto));
			break;
		case O_SETMARK:
			if (cmd->arg1 == IP_FW_TARG)
				snprintf(SNPARGS(action2, 0), "SetMark %#010x",
				    TARG(cmd->arg1, mark));
			else
				snprintf(SNPARGS(action2, 0), "SetMark %#010x",
				    insntoc(cmd, u32)->d[0]);
			break;
		case O_EXTERNAL_ACTION:
			snprintf(SNPARGS(action2, 0), "Eaction %s",
			    ((struct named_object *)SRV_OBJECT(chain,
			    insntod(cmd, kidx)->kidx))->name);
			break;
		default:
			action = "UNKNOWN";
			break;
		}
	}

	if (hlen == 0) {	/* non-ip */
		snprintf(SNPARGS(proto, 0), "MAC");

	} else {
		int len;
#ifdef INET6
		char src[INET6_ADDRSTRLEN + 2], dst[INET6_ADDRSTRLEN + 2];
#else
		char src[INET_ADDRSTRLEN], dst[INET_ADDRSTRLEN];
#endif
		struct icmphdr *icmp;
		struct tcphdr *tcp;
		struct udphdr *udp;
#ifdef INET6
		struct ip6_hdr *ip6 = NULL;
		struct icmp6_hdr *icmp6;
		u_short ip6f_mf;
#endif
		src[0] = '\0';
		dst[0] = '\0';
#ifdef INET6
		ip6f_mf = offset & IP6F_MORE_FRAG;
		offset &= IP6F_OFF_MASK;

		if (IS_IP6_FLOW_ID(&(args->f_id))) {
			char ip6buf[INET6_ADDRSTRLEN];
			snprintf(src, sizeof(src), "[%s]",
			    ip6_sprintf(ip6buf, &args->f_id.src_ip6));
			snprintf(dst, sizeof(dst), "[%s]",
			    ip6_sprintf(ip6buf, &args->f_id.dst_ip6));

			ip6 = (struct ip6_hdr *)ip;
			tcp = (struct tcphdr *)(((char *)ip) + hlen);
			udp = (struct udphdr *)(((char *)ip) + hlen);
		} else
#endif
		{
			tcp = L3HDR(struct tcphdr, ip);
			udp = L3HDR(struct udphdr, ip);

			inet_ntop(AF_INET, &ip->ip_src, src, sizeof(src));
			inet_ntop(AF_INET, &ip->ip_dst, dst, sizeof(dst));
		}

		switch (args->f_id.proto) {
		case IPPROTO_TCP:
			len = snprintf(SNPARGS(proto, 0), "TCP %s", src);
			if (offset == 0)
				snprintf(SNPARGS(proto, len), ":%d %s:%d",
				    ntohs(tcp->th_sport),
				    dst,
				    ntohs(tcp->th_dport));
			else
				snprintf(SNPARGS(proto, len), " %s", dst);
			break;

		case IPPROTO_UDP:
		case IPPROTO_UDPLITE:
			len = snprintf(SNPARGS(proto, 0), "UDP%s%s",
			    args->f_id.proto == IPPROTO_UDP ? " ": "Lite ",
			    src);
			if (offset == 0)
				snprintf(SNPARGS(proto, len), ":%d %s:%d",
				    ntohs(udp->uh_sport),
				    dst,
				    ntohs(udp->uh_dport));
			else
				snprintf(SNPARGS(proto, len), " %s", dst);
			break;

		case IPPROTO_ICMP:
			icmp = L3HDR(struct icmphdr, ip);
			if (offset == 0)
				len = snprintf(SNPARGS(proto, 0),
				    "ICMP:%u.%u ",
				    icmp->icmp_type, icmp->icmp_code);
			else
				len = snprintf(SNPARGS(proto, 0), "ICMP ");
			len += snprintf(SNPARGS(proto, len), "%s", src);
			snprintf(SNPARGS(proto, len), " %s", dst);
			break;
#ifdef INET6
		case IPPROTO_ICMPV6:
			icmp6 = (struct icmp6_hdr *)(((char *)ip) + hlen);
			if (offset == 0)
				len = snprintf(SNPARGS(proto, 0),
				    "ICMPv6:%u.%u ",
				    icmp6->icmp6_type, icmp6->icmp6_code);
			else
				len = snprintf(SNPARGS(proto, 0), "ICMPv6 ");
			len += snprintf(SNPARGS(proto, len), "%s", src);
			snprintf(SNPARGS(proto, len), " %s", dst);
			break;
#endif
		default:
			len = snprintf(SNPARGS(proto, 0), "P:%d %s",
			    args->f_id.proto, src);
			snprintf(SNPARGS(proto, len), " %s", dst);
			break;
		}

#ifdef INET6
		if (IS_IP6_FLOW_ID(&(args->f_id))) {
			if (offset || ip6f_mf)
				snprintf(SNPARGS(fragment, 0),
				    " (frag %08x:%d@%d%s)",
				    args->f_id.extra,
				    ntohs(ip6->ip6_plen) - hlen,
				    ntohs(offset) << 3, ip6f_mf ? "+" : "");
		} else
#endif
		{
			int ipoff, iplen;
			ipoff = ntohs(ip->ip_off);
			iplen = ntohs(ip->ip_len);
			if (ipoff & (IP_MF | IP_OFFMASK))
				snprintf(SNPARGS(fragment, 0),
				    " (frag %d:%d@%d%s)",
				    ntohs(ip->ip_id), iplen - (ip->ip_hl << 2),
				    offset << 3,
				    (ipoff & IP_MF) ? "+" : "");
		}
	}

	/* [fw]mark */
	if (args->rule.pkt_mark)
		snprintf(SNPARGS(mark_str, 0), " mark:%#x",
		    args->rule.pkt_mark);
	else
		mark_str[0] = '\0';

#ifdef __FreeBSD__
	log(LOG_SECURITY | LOG_INFO, "ipfw: %d %s %s%s %s via %s%s\n",
	    f ? f->rulenum : -1, action, proto, mark_str,
	    args->flags & IPFW_ARGS_OUT ? "out" : "in", args->ifp->if_xname,
	    fragment);
#else
	log(LOG_SECURITY | LOG_INFO, "ipfw: %d %s %s%s [no if info]%s\n",
	    f ? f->rulenum : -1, action, proto, mark_str, fragment);
#endif
	if (limit_reached)
		log(LOG_SECURITY | LOG_NOTICE,
		    "ipfw: limit %d reached on entry %d\n",
		    limit_reached, f ? f->rulenum : -1);
}

static void
ipfw_rtsocklog_fill_l3(struct ip_fw_args *args,
    char **buf, struct sockaddr **src, struct sockaddr **dst)
{
	struct sockaddr_in *v4src, *v4dst;
#ifdef INET6
	struct sockaddr_in6 *v6src, *v6dst;

	if (IS_IP6_FLOW_ID(&(args->f_id))) {
		v6src = (struct sockaddr_in6 *)*buf;
		*buf += sizeof(*v6src);
		v6dst = (struct sockaddr_in6 *)*buf;
		*buf += sizeof(*v6dst);
		v6src->sin6_len = v6dst->sin6_len = sizeof(*v6src);
		v6src->sin6_family = v6dst->sin6_family = AF_INET6;
		v6src->sin6_addr = args->f_id.src_ip6;
		v6dst->sin6_addr = args->f_id.dst_ip6;

		*src = (struct sockaddr *)v6src;
		*dst = (struct sockaddr *)v6dst;
	} else
#endif
	{
		v4src = (struct sockaddr_in *)*buf;
		*buf += sizeof(*v4src);
		v4dst = (struct sockaddr_in *)*buf;
		*buf += sizeof(*v4dst);
		v4src->sin_len = v4dst->sin_len = sizeof(*v4src);
		v4src->sin_family = v4dst->sin_family = AF_INET;
		v4src->sin_addr.s_addr = htonl(args->f_id.src_ip);
		v4dst->sin_addr.s_addr = htonl(args->f_id.dst_ip);

		*src = (struct sockaddr *)v4src;
		*dst = (struct sockaddr *)v4dst;
	}
}

static struct sockaddr *
ipfw_rtsocklog_handle_tablearg(struct ip_fw_chain *chain,
    struct ip_fw_args *args, ipfw_insn *cmd, uint32_t tablearg,
    uint32_t *targ_value, char **buf)
{
	/* handle tablearg now */
	switch (cmd->opcode) {
	case O_DIVERT:
	case O_TEE:
		*targ_value = TARG(cmd->arg1, divert);
		break;
	case O_NETGRAPH:
	case O_NGTEE:
		*targ_value = TARG(cmd->arg1, netgraph);
		break;
	case O_SETDSCP:
		*targ_value = (TARG(cmd->arg1, dscp) & 0x3F);
		break;
	case O_SETFIB:
		*targ_value = (TARG(cmd->arg1, fib) & 0x7FFF);
		break;
	case O_SKIPTO:
	case O_CALLRETURN:
		if (cmd->opcode == O_CALLRETURN && (cmd->len & F_NOT))
			break;
		*targ_value = TARG(insntod(cmd, u32)->d[0], skipto);
		break;
	case O_PIPE:
	case O_QUEUE:
		*targ_value = TARG(cmd->arg1, pipe);
		break;
	case O_SETMARK:
		if (cmd->arg1 == IP_FW_TARG)
			*targ_value = TARG_VAL(chain, tablearg, mark);
		break;
	case O_FORWARD_IP:
		if (IS_IP4_FLOW_ID(&args->f_id)) {
			struct sockaddr_in *nh = (struct sockaddr_in *)*buf;

			*buf += sizeof(*nh);
			memcpy(nh, &insntod(cmd, sa)->sa, sizeof(*nh));
			if (nh->sin_addr.s_addr == INADDR_ANY)
				nh->sin_addr.s_addr = htonl(
				    TARG_VAL(chain, tablearg, nh4));
			return ((struct sockaddr *)nh);
		}
		/* FALLTHROUGH */
#ifdef INET6
	case O_FORWARD_IP6:
		if (IS_IP6_FLOW_ID(&args->f_id)) {
			const struct sockaddr_in *sin = &insntod(cmd, sa)->sa;
			struct sockaddr_in6 *nh = (struct sockaddr_in6 *)*buf;

			*buf += sizeof(*nh);
			if (cmd->opcode == O_FORWARD_IP &&
			    sin->sin_addr.s_addr == INADDR_ANY) {
				nh->sin6_family = AF_INET6;
				nh->sin6_len = sizeof(*nh);
				nh->sin6_addr = TARG_VAL(chain, tablearg, nh6);
				nh->sin6_port = sin->sin_port;
				nh->sin6_scope_id =
				    TARG_VAL(chain, tablearg, zoneid);
			} else
				memcpy(nh, &insntod(cmd, sa6)->sa, sizeof(*nh));
			return ((struct sockaddr *)nh);
		}
#endif
	default:
		break;
	}

	return (NULL);
}

#define	MAX_COMMENT_LEN	80

static size_t
ipfw_copy_rule_comment(struct ip_fw *f, char *dst)
{
	ipfw_insn *cmd;
	size_t rcomment_len = 0;
	int l, cmdlen;

	for (l = f->cmd_len, cmd = f->cmd; l > 0; l -= cmdlen, cmd += cmdlen) {
		cmdlen = F_LEN(cmd);
		if (cmd->opcode != O_NOP) {
			continue;
		} else if (cmd->len == 1) {
			return (0);
		}
		break;
	}
	if (l <= 0) {
		return (0);
	}
	rcomment_len = strnlen((char *)(cmd + 1), MAX_COMMENT_LEN - 1) + 1;
	strlcpy(dst, (char *)(cmd + 1), rcomment_len);
	return (rcomment_len);
}

/*
 * Logs a packet matched by a rule as a route(4) socket message.
 *
 * While ipfw0 pseudo interface provides a way to observe full packet body,
 * no metadata (rule number, action, mark, etc) is available.
 * pflog(4) is not an option either as it's header is hardcoded and does not
 * provide sufficient space for ipfw meta information.
 *
 * To be able to get a machine-readable event with all meta information needed
 * for user-space daemons we construct a route(4) message and pack as much meta
 * information as we can into it.
 *
 * RTAX_DST(0): (struct sockaddr_dl) carrying ipfwlog_rtsock_hdr_v2 in sdl_data
 *		with general rule information (rule number, set, action, mark,
 *		cmd, comment) and source/destination MAC addresses in case we're
 *		logging in layer2 pass.
 *
 * RTAX_GATEWAY(1): (struct sockaddr) IP source address
 *
 * RTAX_NETMASK(2): (struct sockaddr) IP destination address
 *
 * RTAX_GENMASK(3): (struct sockaddr) IP address and port used in fwd action
 *
 * One SHOULD set an explicit logamount for any rule using rtsock as flooding
 * route socket with such events could lead to various system-wide side effects.
 * RTF_PROTO1 flag in (struct rt_addrinfo).rti_flags is set in all messages
 * once half of logamount limit is crossed. This could be used by the software
 * processing these logs to issue `ipfw resetlog` command to keep the event
 * flow.
 *
 * TODO: convert ipfwlog_rtsock_hdr_v2 data into TLV to ease expansion.
*/

static void
ipfw_log_rtsock(struct ip_fw_chain *chain, struct ip_fw *f, u_int hlen,
    struct ip_fw_args *args, u_short offset, uint32_t tablearg, void *_eh)
{
	struct sockaddr_dl *sdl_ipfwcmd;
	struct ether_header *eh = _eh;
	struct rt_addrinfo *info;
	uint32_t *targ_value;
	ipfwlog_rtsock_hdr_v2 *hdr;
	ipfw_insn *cmd;
	ipfw_insn_log *l;
	char *buf, *orig_buf;
	/* at least 4 x sizeof(struct sockaddr_dl) + rule comment (80) */
	size_t buflen = 512;

	/* Should we log? O_LOG is the first one */
	cmd = ACTION_PTR(f);
	l = (ipfw_insn_log *)cmd;

	if (l->max_log != 0 && l->log_left == 0)
		return;

	if (hlen == 0) /* non-ip */
		return;

	l->log_left--;
	if (V_fw_verbose != 0 && l->log_left == 0) {
		log(LOG_SECURITY | LOG_NOTICE,
		    "ipfw: limit %d reached on entry %d\n",
		    l->max_log, f ? f->rulenum : -1);
	}

	buf = orig_buf = malloc(buflen, M_TEMP, M_NOWAIT | M_ZERO);
	if (buf == NULL)
		return;

	info = (struct rt_addrinfo *)buf;
	buf += sizeof (*info);

	cmd = ipfw_get_action(f);
	sdl_ipfwcmd = (struct sockaddr_dl *)buf;
	sdl_ipfwcmd->sdl_family = AF_IPFWLOG;
	sdl_ipfwcmd->sdl_index = f->set;
	sdl_ipfwcmd->sdl_type = 2; /* version */
	sdl_ipfwcmd->sdl_alen = sizeof(*hdr);
	hdr = (ipfwlog_rtsock_hdr_v2 *)(sdl_ipfwcmd->sdl_data);
	/* fill rule comment in if any */
	sdl_ipfwcmd->sdl_nlen = ipfw_copy_rule_comment(f, hdr->comment);
	targ_value = &hdr->tablearg;
	hdr->rulenum = f->rulenum;
	hdr->mark = args->rule.pkt_mark;
	hdr->cmd = *cmd;

	sdl_ipfwcmd->sdl_len = sizeof(*sdl_ipfwcmd);
	if (sizeof(*hdr) + sdl_ipfwcmd->sdl_nlen > sizeof(sdl_ipfwcmd->sdl_data)) {
		sdl_ipfwcmd->sdl_len += sizeof(*hdr) + sdl_ipfwcmd->sdl_nlen  -
		    sizeof(sdl_ipfwcmd->sdl_data);
	}
	buf += sdl_ipfwcmd->sdl_len;

	/* fill L2 in if present */
	if (args->flags & IPFW_ARGS_ETHER && eh != NULL) {
		sdl_ipfwcmd->sdl_slen = sizeof(eh->ether_shost);
		memcpy(hdr->ether_shost, eh->ether_shost,
		    sdl_ipfwcmd->sdl_slen);
		memcpy(hdr->ether_dhost, eh->ether_dhost,
		    sdl_ipfwcmd->sdl_slen);
	}

	info->rti_info[RTAX_DST] = (struct sockaddr *)sdl_ipfwcmd;

	/* Warn if we're about to stop sending messages */
	if (l->max_log != 0 && l->log_left < (l->max_log >> 1)) {
		info->rti_flags |= RTF_PROTO1;
	}

	/* handle tablearg */
	info->rti_info[RTAX_GENMASK] = ipfw_rtsocklog_handle_tablearg(
	    chain, args, cmd, tablearg, targ_value, &buf);

	/* L3 */
	ipfw_rtsocklog_fill_l3(args, &buf,
	    &info->rti_info[RTAX_GATEWAY],
	    &info->rti_info[RTAX_NETMASK]);

	KASSERT(buf <= (orig_buf + buflen),
	    ("ipfw: buffer for logdst rtsock is not big enough"));

	info->rti_ifp = args->ifp;
	rtsock_routemsg_info(RTM_IPFWLOG, info, RT_ALL_FIBS);

	free(orig_buf, M_TEMP);
}

/*
 * We enter here when we have a rule with O_LOG.
 */
void
ipfw_log(struct ip_fw_chain *chain, struct ip_fw *f, u_int hlen,
    struct ip_fw_args *args, u_short offset, uint32_t tablearg,
    struct ip *ip, void *eh)
{
	ipfw_insn *cmd;

	/* Fallback to default logging if we're missing rule pointer */
	if (f == NULL ||
	    /* O_LOG is the first action */
	    ((cmd = ACTION_PTR(f)) && cmd->arg1 == IPFW_LOG_DEFAULT)) {
		if (V_fw_verbose == 0) {
			ipfw_bpf_tap(args, ip,
			    f != NULL ? f->rulenum : IPFW_DEFAULT_RULE);
			return;
		}
		ipfw_log_syslog(chain, f, hlen, args, offset, tablearg, ip);
		return;
	}

	if (cmd->arg1 & IPFW_LOG_SYSLOG)
		ipfw_log_syslog(chain, f, hlen, args, offset, tablearg, ip);

	if (cmd->arg1 & IPFW_LOG_RTSOCK)
		ipfw_log_rtsock(chain, f, hlen, args, offset, tablearg, eh);

	if (cmd->arg1 & IPFW_LOG_IPFW0)
		ipfw_bpf_tap(args, ip, f->rulenum);
}
/* end of file */
