/*-
 * Copyright (c) 2008 Paolo Pisati
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/condvar.h>
#include <sys/eventhandler.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/jail.h>
#include <sys/module.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/ucred.h>

#define        IPFW_INTERNAL   /* Access to protected data structures in ip_fw.h. */

#include <netinet/libalias/alias.h>
#include <netinet/libalias/alias_local.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_fw.h>
#include <netinet/ipfw/ip_fw_private.h>
#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <machine/in_cksum.h>	/* XXX for in_cksum */

static VNET_DEFINE(eventhandler_tag, ifaddr_event_tag);
#define	V_ifaddr_event_tag	VNET(ifaddr_event_tag)

static void 
ifaddr_change(void *arg __unused, struct ifnet *ifp)
{
	struct cfg_nat *ptr;
	struct ifaddr *ifa;

	IPFW_WLOCK(&V_layer3_chain);			
	/* Check every nat entry... */
	LIST_FOREACH(ptr, &V_layer3_chain.nat, _next) {
		/* ...using nic 'ifp->if_xname' as dynamic alias address. */
		if (strncmp(ptr->if_name, ifp->if_xname, IF_NAMESIZE) == 0) {
			if_addr_rlock(ifp);
			TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
				if (ifa->ifa_addr == NULL)
					continue;
				if (ifa->ifa_addr->sa_family != AF_INET)
					continue;
				ptr->ip = ((struct sockaddr_in *) 
				    (ifa->ifa_addr))->sin_addr;
				LibAliasSetAddress(ptr->lib, ptr->ip);
			}
			if_addr_runlock(ifp);
		}
	}
	IPFW_WUNLOCK(&V_layer3_chain);	
}

static void
flush_nat_ptrs(const int i)
{
	struct ip_fw *rule;

	IPFW_WLOCK_ASSERT(&V_layer3_chain);
	for (rule = V_layer3_chain.rules; rule; rule = rule->next) {
		ipfw_insn_nat *cmd = (ipfw_insn_nat *)ACTION_PTR(rule);
		if (cmd->o.opcode != O_NAT)
			continue;
		if (cmd->nat != NULL && cmd->nat->id == i)
			cmd->nat = NULL;
	}
}

#define HOOK_NAT(b, p) do {				\
		IPFW_WLOCK_ASSERT(&V_layer3_chain);	\
		LIST_INSERT_HEAD(b, p, _next);		\
	} while (0)

#define UNHOOK_NAT(p) do {				\
		IPFW_WLOCK_ASSERT(&V_layer3_chain);	\
		LIST_REMOVE(p, _next);			\
	} while (0)

#define HOOK_REDIR(b, p) do {			\
		LIST_INSERT_HEAD(b, p, _next);	\
	} while (0)

#define HOOK_SPOOL(b, p) do {			\
		LIST_INSERT_HEAD(b, p, _next);	\
	} while (0)

static void
del_redir_spool_cfg(struct cfg_nat *n, struct redir_chain *head)
{
	struct cfg_redir *r, *tmp_r;
	struct cfg_spool *s, *tmp_s;
	int i, num;

	LIST_FOREACH_SAFE(r, head, _next, tmp_r) {
		num = 1; /* Number of alias_link to delete. */
		switch (r->mode) {
		case REDIR_PORT:
			num = r->pport_cnt;
			/* FALLTHROUGH */
		case REDIR_ADDR:
		case REDIR_PROTO:
			/* Delete all libalias redirect entry. */
			for (i = 0; i < num; i++)
				LibAliasRedirectDelete(n->lib, r->alink[i]);
			/* Del spool cfg if any. */
			LIST_FOREACH_SAFE(s, &r->spool_chain, _next, tmp_s) {
				LIST_REMOVE(s, _next);
				free(s, M_IPFW);
			}
			free(r->alink, M_IPFW);
			LIST_REMOVE(r, _next);
			free(r, M_IPFW);
			break;
		default:
			printf("unknown redirect mode: %u\n", r->mode);				
			/* XXX - panic?!?!? */
			break; 
		}
	}
}

static int
add_redir_spool_cfg(char *buf, struct cfg_nat *ptr)
{
	struct cfg_redir *r, *ser_r;
	struct cfg_spool *s, *ser_s;
	int cnt, off, i;
	char *panic_err;

	for (cnt = 0, off = 0; cnt < ptr->redir_cnt; cnt++) {
		ser_r = (struct cfg_redir *)&buf[off];
		r = malloc(SOF_REDIR, M_IPFW, M_WAITOK | M_ZERO);
		memcpy(r, ser_r, SOF_REDIR);
		LIST_INIT(&r->spool_chain);
		off += SOF_REDIR;
		r->alink = malloc(sizeof(struct alias_link *) * r->pport_cnt,
		    M_IPFW, M_WAITOK | M_ZERO);
		switch (r->mode) {
		case REDIR_ADDR:
			r->alink[0] = LibAliasRedirectAddr(ptr->lib, r->laddr,
			    r->paddr);
			break;
		case REDIR_PORT:
			for (i = 0 ; i < r->pport_cnt; i++) {
				/* If remotePort is all ports, set it to 0. */
				u_short remotePortCopy = r->rport + i;
				if (r->rport_cnt == 1 && r->rport == 0)
					remotePortCopy = 0;
				r->alink[i] = LibAliasRedirectPort(ptr->lib,
				    r->laddr, htons(r->lport + i), r->raddr,
				    htons(remotePortCopy), r->paddr, 
				    htons(r->pport + i), r->proto);
				if (r->alink[i] == NULL) {
					r->alink[0] = NULL;
					break;
				}
			}
			break;
		case REDIR_PROTO:
			r->alink[0] = LibAliasRedirectProto(ptr->lib ,r->laddr,
			    r->raddr, r->paddr, r->proto);
			break;
		default:
			printf("unknown redirect mode: %u\n", r->mode);
			break; 
		}
		if (r->alink[0] == NULL) {
			panic_err = "LibAliasRedirect* returned NULL";
			goto bad;
		} else /* LSNAT handling. */
			for (i = 0; i < r->spool_cnt; i++) {
				ser_s = (struct cfg_spool *)&buf[off];
				s = malloc(SOF_REDIR, M_IPFW, 
				    M_WAITOK | M_ZERO);
				memcpy(s, ser_s, SOF_SPOOL);
				LibAliasAddServer(ptr->lib, r->alink[0], 
				    s->addr, htons(s->port));
				off += SOF_SPOOL;
				/* Hook spool entry. */
				HOOK_SPOOL(&r->spool_chain, s);
			}
		/* And finally hook this redir entry. */
		HOOK_REDIR(&ptr->redir_chain, r);
	}
	return (1);
bad:
	/* something really bad happened: panic! */
	panic("%s\n", panic_err);
}

static int
ipfw_nat(struct ip_fw_args *args, struct cfg_nat *t, struct mbuf *m)
{
	struct mbuf *mcl;
	struct ip *ip;
	/* XXX - libalias duct tape */
	int ldt, retval;
	char *c;

	ldt = 0;
	retval = 0;
	if ((mcl = m_megapullup(m, m->m_pkthdr.len)) ==
	    NULL)
		goto badnat;
	ip = mtod(mcl, struct ip *);
	if (args->eh == NULL) {
		ip->ip_len = htons(ip->ip_len);
		ip->ip_off = htons(ip->ip_off);
	}

	/* 
	 * XXX - Libalias checksum offload 'duct tape':
	 * 
	 * locally generated packets have only
	 * pseudo-header checksum calculated
	 * and libalias will screw it[1], so
	 * mark them for later fix.  Moreover
	 * there are cases when libalias
	 * modify tcp packet data[2], mark it
	 * for later fix too.
	 *
	 * [1] libalias was never meant to run
	 * in kernel, so it doesn't have any
	 * knowledge about checksum
	 * offloading, and it expects a packet
	 * with a full internet
	 * checksum. Unfortunately, packets
	 * generated locally will have just the
	 * pseudo header calculated, and when
	 * libalias tries to adjust the
	 * checksum it will actually screw it.
	 *
	 * [2] when libalias modify tcp's data
	 * content, full TCP checksum has to
	 * be recomputed: the problem is that
	 * libalias doesn't have any idea
	 * about checksum offloading To
	 * workaround this, we do not do
	 * checksumming in LibAlias, but only
	 * mark the packets in th_x2 field. If
	 * we receive a marked packet, we
	 * calculate correct checksum for it
	 * aware of offloading.  Why such a
	 * terrible hack instead of
	 * recalculating checksum for each
	 * packet?  Because the previous
	 * checksum was not checked!
	 * Recalculating checksums for EVERY
	 * packet will hide ALL transmission
	 * errors. Yes, marked packets still
	 * suffer from this problem. But,
	 * sigh, natd(8) has this problem,
	 * too.
	 *
	 * TODO: -make libalias mbuf aware (so
	 * it can handle delayed checksum and tso)
	 */

	if (mcl->m_pkthdr.rcvif == NULL && 
	    mcl->m_pkthdr.csum_flags & 
	    CSUM_DELAY_DATA)
		ldt = 1;

	c = mtod(mcl, char *);
	if (args->oif == NULL)
		retval = LibAliasIn(t->lib, c, 
			mcl->m_len + M_TRAILINGSPACE(mcl));
	else
		retval = LibAliasOut(t->lib, c, 
			mcl->m_len + M_TRAILINGSPACE(mcl));
	if (retval == PKT_ALIAS_RESPOND) {
	  m->m_flags |= M_SKIP_FIREWALL;
	  retval = PKT_ALIAS_OK;
	}
	if (retval != PKT_ALIAS_OK &&
	    retval != PKT_ALIAS_FOUND_HEADER_FRAGMENT) {
		/* XXX - should i add some logging? */
		m_free(mcl);
	badnat:
		args->m = NULL;
		return (IP_FW_DENY);
	}
	mcl->m_pkthdr.len = mcl->m_len = 
	    ntohs(ip->ip_len);

	/* 
	 * XXX - libalias checksum offload 
	 * 'duct tape' (see above) 
	 */

	if ((ip->ip_off & htons(IP_OFFMASK)) == 0 && 
	    ip->ip_p == IPPROTO_TCP) {
		struct tcphdr 	*th; 

		th = (struct tcphdr *)(ip + 1);
		if (th->th_x2) 
			ldt = 1;
	}

	if (ldt) {
		struct tcphdr 	*th;
		struct udphdr 	*uh;
		u_short cksum;

		ip->ip_len = ntohs(ip->ip_len);
		cksum = in_pseudo(
		    ip->ip_src.s_addr,
		    ip->ip_dst.s_addr, 
		    htons(ip->ip_p + ip->ip_len - (ip->ip_hl << 2))
		);
					
		switch (ip->ip_p) {
		case IPPROTO_TCP:
			th = (struct tcphdr *)(ip + 1);
			/* 
			 * Maybe it was set in 
			 * libalias... 
			 */
			th->th_x2 = 0;
			th->th_sum = cksum;
			mcl->m_pkthdr.csum_data = 
			    offsetof(struct tcphdr, th_sum);
			break;
		case IPPROTO_UDP:
			uh = (struct udphdr *)(ip + 1);
			uh->uh_sum = cksum;
			mcl->m_pkthdr.csum_data = 
			    offsetof(struct udphdr, uh_sum);
			break;						
		}
		/* 
		 * No hw checksum offloading: do it 
		 * by ourself. 
		 */
		if ((mcl->m_pkthdr.csum_flags & 
		     CSUM_DELAY_DATA) == 0) {
			in_delayed_cksum(mcl);
			mcl->m_pkthdr.csum_flags &= 
			    ~CSUM_DELAY_DATA;
		}
		ip->ip_len = htons(ip->ip_len);
	}

	if (args->eh == NULL) {
		ip->ip_len = ntohs(ip->ip_len);
		ip->ip_off = ntohs(ip->ip_off);
	}

	args->m = mcl;
	return (IP_FW_NAT);
}

#define LOOKUP_NAT(head, i, p) do {			\
		LIST_FOREACH((p), head, _next) {	\
			if ((p)->id == (i)) {		\
				break;			\
			}				\
		}					\
	} while (0)

static struct cfg_nat *
lookup_nat(struct nat_list *l, int nat_id)
{
	struct cfg_nat *res;

	LOOKUP_NAT(l, nat_id, res);
	return res;
}

static int 
ipfw_nat_cfg(struct sockopt *sopt)
{
	struct cfg_nat *ptr, *ser_n;
	char *buf;

	buf = malloc(NAT_BUF_LEN, M_IPFW, M_WAITOK | M_ZERO);
	sooptcopyin(sopt, buf, NAT_BUF_LEN, 
	    sizeof(struct cfg_nat));
	ser_n = (struct cfg_nat *)buf;

	/* 
	 * Find/create nat rule.
	 */
	IPFW_WLOCK(&V_layer3_chain);
	LOOKUP_NAT(&V_layer3_chain.nat, ser_n->id, ptr);
	if (ptr == NULL) {
		/* New rule: allocate and init new instance. */
		ptr = malloc(sizeof(struct cfg_nat), 
		    M_IPFW, M_NOWAIT | M_ZERO);
		if (ptr == NULL) {
			IPFW_WUNLOCK(&V_layer3_chain);				
			free(buf, M_IPFW);
			return (ENOSPC);
		}
		ptr->lib = LibAliasInit(NULL);
		if (ptr->lib == NULL) {
			IPFW_WUNLOCK(&V_layer3_chain);
			free(ptr, M_IPFW);
			free(buf, M_IPFW);
			return (EINVAL);
		}
		LIST_INIT(&ptr->redir_chain);
	} else {
		/* Entry already present: temporarly unhook it. */
		UNHOOK_NAT(ptr);
		flush_nat_ptrs(ser_n->id);
	}
	IPFW_WUNLOCK(&V_layer3_chain);

	/* 
	 * Basic nat configuration.
	 */
	ptr->id = ser_n->id;
	/* 
	 * XXX - what if this rule doesn't nat any ip and just 
	 * redirect? 
	 * do we set aliasaddress to 0.0.0.0?
	 */
	ptr->ip = ser_n->ip;
	ptr->redir_cnt = ser_n->redir_cnt;
	ptr->mode = ser_n->mode;
	LibAliasSetMode(ptr->lib, ser_n->mode, ser_n->mode);
	LibAliasSetAddress(ptr->lib, ptr->ip);
	memcpy(ptr->if_name, ser_n->if_name, IF_NAMESIZE);

	/* 
	 * Redir and LSNAT configuration.
	 */
	/* Delete old cfgs. */
	del_redir_spool_cfg(ptr, &ptr->redir_chain);
	/* Add new entries. */
	add_redir_spool_cfg(&buf[(sizeof(struct cfg_nat))], ptr);
	free(buf, M_IPFW);
	IPFW_WLOCK(&V_layer3_chain);
	HOOK_NAT(&V_layer3_chain.nat, ptr);
	IPFW_WUNLOCK(&V_layer3_chain);
	return (0);
}

static int
ipfw_nat_del(struct sockopt *sopt)
{
	struct cfg_nat *ptr;
	int i;
		
	sooptcopyin(sopt, &i, sizeof i, sizeof i);
	IPFW_WLOCK(&V_layer3_chain);
	LOOKUP_NAT(&V_layer3_chain.nat, i, ptr);
	if (ptr == NULL) {
		IPFW_WUNLOCK(&V_layer3_chain);
		return (EINVAL);
	}
	UNHOOK_NAT(ptr);
	flush_nat_ptrs(i);
	IPFW_WUNLOCK(&V_layer3_chain);
	del_redir_spool_cfg(ptr, &ptr->redir_chain);
	LibAliasUninit(ptr->lib);
	free(ptr, M_IPFW);
	return (0);
}

static int
ipfw_nat_get_cfg(struct sockopt *sopt)
{	
	uint8_t *data;
	struct cfg_nat *n;
	struct cfg_redir *r;
	struct cfg_spool *s;
	int nat_cnt, off;
		
	nat_cnt = 0;
	off = sizeof(nat_cnt);

	data = malloc(NAT_BUF_LEN, M_IPFW, M_WAITOK | M_ZERO);
	IPFW_RLOCK(&V_layer3_chain);
	/* Serialize all the data. */
	LIST_FOREACH(n, &V_layer3_chain.nat, _next) {
		nat_cnt++;
		if (off + SOF_NAT < NAT_BUF_LEN) {
			bcopy(n, &data[off], SOF_NAT);
			off += SOF_NAT;
			LIST_FOREACH(r, &n->redir_chain, _next) {
				if (off + SOF_REDIR < NAT_BUF_LEN) {
					bcopy(r, &data[off], 
					    SOF_REDIR);
					off += SOF_REDIR;
					LIST_FOREACH(s, &r->spool_chain, 
					    _next) {
						if (off + SOF_SPOOL < 
						    NAT_BUF_LEN) {
							bcopy(s, &data[off],
							    SOF_SPOOL);
							off += SOF_SPOOL;
						} else
							goto nospace;
					}
				} else
					goto nospace;
			}
		} else
			goto nospace;
	}
	bcopy(&nat_cnt, data, sizeof(nat_cnt));
	IPFW_RUNLOCK(&V_layer3_chain);
	sooptcopyout(sopt, data, NAT_BUF_LEN);
	free(data, M_IPFW);
	return (0);
nospace:
	IPFW_RUNLOCK(&V_layer3_chain);
	printf("serialized data buffer not big enough:"
	    "please increase NAT_BUF_LEN\n");
	free(data, M_IPFW);
	return (ENOSPC);
}

static int
ipfw_nat_get_log(struct sockopt *sopt)
{
	uint8_t *data;
	struct cfg_nat *ptr;
	int i, size, cnt, sof;

	data = NULL;
	sof = LIBALIAS_BUF_SIZE;
	cnt = 0;

	IPFW_RLOCK(&V_layer3_chain);
	size = i = 0;
	LIST_FOREACH(ptr, &V_layer3_chain.nat, _next) {
		if (ptr->lib->logDesc == NULL) 
			continue;
		cnt++;
		size = cnt * (sof + sizeof(int));
		data = realloc(data, size, M_IPFW, M_NOWAIT | M_ZERO);
		if (data == NULL) {
			IPFW_RUNLOCK(&V_layer3_chain);
			return (ENOSPC);
		}
		bcopy(&ptr->id, &data[i], sizeof(int));
		i += sizeof(int);
		bcopy(ptr->lib->logDesc, &data[i], sof);
		i += sof;
	}
	IPFW_RUNLOCK(&V_layer3_chain);
	sooptcopyout(sopt, data, size);
	free(data, M_IPFW);
	return(0);
}

static void
ipfw_nat_init(void)
{

	IPFW_WLOCK(&V_layer3_chain);
	/* init ipfw hooks */
	ipfw_nat_ptr = ipfw_nat;
	lookup_nat_ptr = lookup_nat;
	ipfw_nat_cfg_ptr = ipfw_nat_cfg;
	ipfw_nat_del_ptr = ipfw_nat_del;
	ipfw_nat_get_cfg_ptr = ipfw_nat_get_cfg;
	ipfw_nat_get_log_ptr = ipfw_nat_get_log;
	IPFW_WUNLOCK(&V_layer3_chain);
	V_ifaddr_event_tag = EVENTHANDLER_REGISTER(ifaddr_event, ifaddr_change, 
	    NULL, EVENTHANDLER_PRI_ANY);
}

static void
ipfw_nat_destroy(void)
{
	struct ip_fw *rule;
	struct cfg_nat *ptr, *ptr_temp;
	
	IPFW_WLOCK(&V_layer3_chain);
	LIST_FOREACH_SAFE(ptr, &V_layer3_chain.nat, _next, ptr_temp) {
		LIST_REMOVE(ptr, _next);
		del_redir_spool_cfg(ptr, &ptr->redir_chain);
		LibAliasUninit(ptr->lib);
		free(ptr, M_IPFW);
	}
	EVENTHANDLER_DEREGISTER(ifaddr_event, V_ifaddr_event_tag);
	/* flush all nat ptrs */
	for (rule = V_layer3_chain.rules; rule; rule = rule->next) {
		ipfw_insn_nat *cmd = (ipfw_insn_nat *)ACTION_PTR(rule);
		if (cmd->o.opcode == O_NAT)
			cmd->nat = NULL;
	}
	/* deregister ipfw_nat */
	ipfw_nat_ptr = NULL;
	lookup_nat_ptr = NULL;
	ipfw_nat_cfg_ptr = NULL;
	ipfw_nat_del_ptr = NULL;
	ipfw_nat_get_cfg_ptr = NULL;
	ipfw_nat_get_log_ptr = NULL;
	IPFW_WUNLOCK(&V_layer3_chain);
}

static int
ipfw_nat_modevent(module_t mod, int type, void *unused)
{
	int err = 0;

	switch (type) {
	case MOD_LOAD:
		ipfw_nat_init();
		break;

	case MOD_UNLOAD:
		ipfw_nat_destroy();
		break;

	default:
		return EOPNOTSUPP;
		break;
	}
	return err;
}

static moduledata_t ipfw_nat_mod = {
	"ipfw_nat",
	ipfw_nat_modevent,
	0
};

DECLARE_MODULE(ipfw_nat, ipfw_nat_mod, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY);
MODULE_DEPEND(ipfw_nat, libalias, 1, 1, 1);
MODULE_DEPEND(ipfw_nat, ipfw, 2, 2, 2);
MODULE_VERSION(ipfw_nat, 1);
/* end of file */
