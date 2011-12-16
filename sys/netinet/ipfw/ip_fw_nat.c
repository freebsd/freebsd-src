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
#include <sys/eventhandler.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/rwlock.h>

#define        IPFW_INTERNAL   /* Access to protected data structures in ip_fw.h. */

#include <netinet/libalias/alias.h>
#include <netinet/libalias/alias_local.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_fw.h>
#include <netinet/ipfw/ip_fw_private.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <machine/in_cksum.h>	/* XXX for in_cksum */

static VNET_DEFINE(eventhandler_tag, ifaddr_event_tag);
#define	V_ifaddr_event_tag	VNET(ifaddr_event_tag)

static void
ifaddr_change(void *arg __unused, struct ifnet *ifp)
{
	struct cfg_nat *ptr;
	struct ifaddr *ifa;
	struct ip_fw_chain *chain;

	chain = &V_layer3_chain;
	IPFW_WLOCK(chain);
	/* Check every nat entry... */
	LIST_FOREACH(ptr, &chain->nat, _next) {
		/* ...using nic 'ifp->if_xname' as dynamic alias address. */
		if (strncmp(ptr->if_name, ifp->if_xname, IF_NAMESIZE) != 0)
			continue;
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
	IPFW_WUNLOCK(chain);
}

/*
 * delete the pointers for nat entry ix, or all of them if ix < 0
 */
static void
flush_nat_ptrs(struct ip_fw_chain *chain, const int ix)
{
	int i;
	ipfw_insn_nat *cmd;

	IPFW_WLOCK_ASSERT(chain);
	for (i = 0; i < chain->n_rules; i++) {
		cmd = (ipfw_insn_nat *)ACTION_PTR(chain->map[i]);
		/* XXX skip log and the like ? */
		if (cmd->o.opcode == O_NAT && cmd->nat != NULL &&
			    (ix < 0 || cmd->nat->id == ix))
			cmd->nat = NULL;
	}
}

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

static void
add_redir_spool_cfg(char *buf, struct cfg_nat *ptr)
{
	struct cfg_redir *r, *ser_r;
	struct cfg_spool *s, *ser_s;
	int cnt, off, i;

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
		/* XXX perhaps return an error instead of panic ? */
		if (r->alink[0] == NULL)
			panic("LibAliasRedirect* returned NULL");
		/* LSNAT handling. */
		for (i = 0; i < r->spool_cnt; i++) {
			ser_s = (struct cfg_spool *)&buf[off];
			s = malloc(SOF_REDIR, M_IPFW, M_WAITOK | M_ZERO);
			memcpy(s, ser_s, SOF_SPOOL);
			LibAliasAddServer(ptr->lib, r->alink[0],
			    s->addr, htons(s->port));
			off += SOF_SPOOL;
			/* Hook spool entry. */
			LIST_INSERT_HEAD(&r->spool_chain, s, _next);
		}
		/* And finally hook this redir entry. */
		LIST_INSERT_HEAD(&ptr->redir_chain, r, _next);
	}
}

static int
ipfw_nat(struct ip_fw_args *args, struct cfg_nat *t, struct mbuf *m)
{
	struct mbuf *mcl;
	struct ip *ip;
	/* XXX - libalias duct tape */
	int ldt, retval, found;
	struct ip_fw_chain *chain;
	char *c;

	ldt = 0;
	retval = 0;
	mcl = m_megapullup(m, m->m_pkthdr.len);
	if (mcl == NULL) {
		args->m = NULL;
		return (IP_FW_DENY);
	}
	ip = mtod(mcl, struct ip *);

	/*
	 * XXX - Libalias checksum offload 'duct tape':
	 *
	 * locally generated packets have only pseudo-header checksum
	 * calculated and libalias will break it[1], so mark them for
	 * later fix.  Moreover there are cases when libalias modifies
	 * tcp packet data[2], mark them for later fix too.
	 *
	 * [1] libalias was never meant to run in kernel, so it does
	 * not have any knowledge about checksum offloading, and
	 * expects a packet with a full internet checksum.
	 * Unfortunately, packets generated locally will have just the
	 * pseudo header calculated, and when libalias tries to adjust
	 * the checksum it will actually compute a wrong value.
	 *
	 * [2] when libalias modifies tcp's data content, full TCP
	 * checksum has to be recomputed: the problem is that
	 * libalias does not have any idea about checksum offloading.
	 * To work around this, we do not do checksumming in LibAlias,
	 * but only mark the packets in th_x2 field. If we receive a
	 * marked packet, we calculate correct checksum for it
	 * aware of offloading.  Why such a terrible hack instead of
	 * recalculating checksum for each packet?
	 * Because the previous checksum was not checked!
	 * Recalculating checksums for EVERY packet will hide ALL
	 * transmission errors. Yes, marked packets still suffer from
	 * this problem. But, sigh, natd(8) has this problem, too.
	 *
	 * TODO: -make libalias mbuf aware (so
	 * it can handle delayed checksum and tso)
	 */

	if (mcl->m_pkthdr.rcvif == NULL &&
	    mcl->m_pkthdr.csum_flags & CSUM_DELAY_DATA)
		ldt = 1;

	c = mtod(mcl, char *);

	/* Check if this is 'global' instance */
	if (t == NULL) {
		if (args->oif == NULL) {
			/* Wrong direction, skip processing */
			args->m = mcl;
			return (IP_FW_NAT);
		}

		found = 0;
		chain = &V_layer3_chain;
		IPFW_RLOCK(chain);
		/* Check every nat entry... */
		LIST_FOREACH(t, &chain->nat, _next) {
			if ((t->mode & PKT_ALIAS_SKIP_GLOBAL) != 0)
				continue;
			retval = LibAliasOutTry(t->lib, c,
			    mcl->m_len + M_TRAILINGSPACE(mcl), 0);
			if (retval == PKT_ALIAS_OK) {
				/* Nat instance recognises state */
				found = 1;
				break;
			}
		}
		IPFW_RUNLOCK(chain);
		if (found != 1) {
			/* No instance found, return ignore */
			args->m = mcl;
			return (IP_FW_NAT);
		}
	} else {
		if (args->oif == NULL)
			retval = LibAliasIn(t->lib, c,
				mcl->m_len + M_TRAILINGSPACE(mcl));
		else
			retval = LibAliasOut(t->lib, c,
				mcl->m_len + M_TRAILINGSPACE(mcl));
	}

	/*
	 * We drop packet when:
	 * 1. libalias returns PKT_ALIAS_ERROR;
	 * 2. For incoming packets:
	 *	a) for unresolved fragments;
	 *	b) libalias returns PKT_ALIAS_IGNORED and
	 *		PKT_ALIAS_DENY_INCOMING flag is set.
	 */
	if (retval == PKT_ALIAS_ERROR ||
	    (args->oif == NULL && (retval == PKT_ALIAS_UNRESOLVED_FRAGMENT ||
	    (retval == PKT_ALIAS_IGNORED &&
	    (t->mode & PKT_ALIAS_DENY_INCOMING) != 0)))) {
		/* XXX - should i add some logging? */
		m_free(mcl);
		args->m = NULL;
		return (IP_FW_DENY);
	}

	if (retval == PKT_ALIAS_RESPOND)
		m->m_flags |= M_SKIP_FIREWALL;
	mcl->m_pkthdr.len = mcl->m_len = ntohs(ip->ip_len);

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
		cksum = in_pseudo(ip->ip_src.s_addr, ip->ip_dst.s_addr,
		    htons(ip->ip_p + ip->ip_len - (ip->ip_hl << 2)));

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
		/* No hw checksum offloading: do it ourselves */
		if ((mcl->m_pkthdr.csum_flags & CSUM_DELAY_DATA) == 0) {
			in_delayed_cksum(mcl);
			mcl->m_pkthdr.csum_flags &= ~CSUM_DELAY_DATA;
		}
		ip->ip_len = htons(ip->ip_len);
	}
	args->m = mcl;
	return (IP_FW_NAT);
}

static struct cfg_nat *
lookup_nat(struct nat_list *l, int nat_id)
{
	struct cfg_nat *res;

	LIST_FOREACH(res, l, _next) {
		if (res->id == nat_id)
			break;
	}
	return res;
}

static int
ipfw_nat_cfg(struct sockopt *sopt)
{
	struct cfg_nat *cfg, *ptr;
	char *buf;
	struct ip_fw_chain *chain = &V_layer3_chain;
	size_t len;
	int gencnt, error = 0;

	len = sopt->sopt_valsize;
	buf = malloc(len, M_TEMP, M_WAITOK | M_ZERO);
	if ((error = sooptcopyin(sopt, buf, len, sizeof(struct cfg_nat))) != 0)
		goto out;

	cfg = (struct cfg_nat *)buf;
	if (cfg->id < 0) {
		error = EINVAL;
		goto out;
	}

	/*
	 * Find/create nat rule.
	 */
	IPFW_WLOCK(chain);
	gencnt = chain->gencnt;
	ptr = lookup_nat(&chain->nat, cfg->id);
	if (ptr == NULL) {
		IPFW_WUNLOCK(chain);
		/* New rule: allocate and init new instance. */
		ptr = malloc(sizeof(struct cfg_nat), M_IPFW, M_WAITOK | M_ZERO);
		ptr->lib = LibAliasInit(NULL);
		LIST_INIT(&ptr->redir_chain);
	} else {
		/* Entry already present: temporarily unhook it. */
		LIST_REMOVE(ptr, _next);
		flush_nat_ptrs(chain, cfg->id);
		IPFW_WUNLOCK(chain);
	}

	/*
	 * Basic nat configuration.
	 */
	ptr->id = cfg->id;
	/*
	 * XXX - what if this rule doesn't nat any ip and just
	 * redirect?
	 * do we set aliasaddress to 0.0.0.0?
	 */
	ptr->ip = cfg->ip;
	ptr->redir_cnt = cfg->redir_cnt;
	ptr->mode = cfg->mode;
	LibAliasSetMode(ptr->lib, cfg->mode, cfg->mode);
	LibAliasSetAddress(ptr->lib, ptr->ip);
	memcpy(ptr->if_name, cfg->if_name, IF_NAMESIZE);

	/*
	 * Redir and LSNAT configuration.
	 */
	/* Delete old cfgs. */
	del_redir_spool_cfg(ptr, &ptr->redir_chain);
	/* Add new entries. */
	add_redir_spool_cfg(&buf[(sizeof(struct cfg_nat))], ptr);

	IPFW_WLOCK(chain);
	/* Extra check to avoid race with another ipfw_nat_cfg() */
	if (gencnt != chain->gencnt &&
	    ((cfg = lookup_nat(&chain->nat, ptr->id)) != NULL))
		LIST_REMOVE(cfg, _next);
	LIST_INSERT_HEAD(&chain->nat, ptr, _next);
	chain->gencnt++;
	IPFW_WUNLOCK(chain);

out:
	free(buf, M_TEMP);
	return (error);
}

static int
ipfw_nat_del(struct sockopt *sopt)
{
	struct cfg_nat *ptr;
	struct ip_fw_chain *chain = &V_layer3_chain;
	int i;

	sooptcopyin(sopt, &i, sizeof i, sizeof i);
	/* XXX validate i */
	IPFW_WLOCK(chain);
	ptr = lookup_nat(&chain->nat, i);
	if (ptr == NULL) {
		IPFW_WUNLOCK(chain);
		return (EINVAL);
	}
	LIST_REMOVE(ptr, _next);
	flush_nat_ptrs(chain, i);
	IPFW_WUNLOCK(chain);
	del_redir_spool_cfg(ptr, &ptr->redir_chain);
	LibAliasUninit(ptr->lib);
	free(ptr, M_IPFW);
	return (0);
}

static int
ipfw_nat_get_cfg(struct sockopt *sopt)
{
	struct ip_fw_chain *chain = &V_layer3_chain;
	struct cfg_nat *n;
	struct cfg_redir *r;
	struct cfg_spool *s;
	char *data;
	int gencnt, nat_cnt, len, error;

	nat_cnt = 0;
	len = sizeof(nat_cnt);

	IPFW_RLOCK(chain);
retry:
	gencnt = chain->gencnt;
	/* Estimate memory amount */
	LIST_FOREACH(n, &chain->nat, _next) {
		nat_cnt++;
		len += sizeof(struct cfg_nat);
		LIST_FOREACH(r, &n->redir_chain, _next) {
			len += sizeof(struct cfg_redir);
			LIST_FOREACH(s, &r->spool_chain, _next)
				len += sizeof(struct cfg_spool);
		}
	}
	IPFW_RUNLOCK(chain);

	data = malloc(len, M_TEMP, M_WAITOK | M_ZERO);
	bcopy(&nat_cnt, data, sizeof(nat_cnt));

	nat_cnt = 0;
	len = sizeof(nat_cnt);

	IPFW_RLOCK(chain);
	if (gencnt != chain->gencnt) {
		free(data, M_TEMP);
		goto retry;
	}
	/* Serialize all the data. */
	LIST_FOREACH(n, &chain->nat, _next) {
		bcopy(n, &data[len], sizeof(struct cfg_nat));
		len += sizeof(struct cfg_nat);
		LIST_FOREACH(r, &n->redir_chain, _next) {
			bcopy(r, &data[len], sizeof(struct cfg_redir));
			len += sizeof(struct cfg_redir);
			LIST_FOREACH(s, &r->spool_chain, _next) {
				bcopy(s, &data[len], sizeof(struct cfg_spool));
				len += sizeof(struct cfg_spool);
			}
		}
	}
	IPFW_RUNLOCK(chain);

	error = sooptcopyout(sopt, data, len);
	free(data, M_TEMP);

	return (error);
}

static int
ipfw_nat_get_log(struct sockopt *sopt)
{
	uint8_t *data;
	struct cfg_nat *ptr;
	int i, size;
	struct ip_fw_chain *chain;

	chain = &V_layer3_chain;

	IPFW_RLOCK(chain);
	/* one pass to count, one to copy the data */
	i = 0;
	LIST_FOREACH(ptr, &chain->nat, _next) {
		if (ptr->lib->logDesc == NULL)
			continue;
		i++;
	}
	size = i * (LIBALIAS_BUF_SIZE + sizeof(int));
	data = malloc(size, M_IPFW, M_NOWAIT | M_ZERO);
	if (data == NULL) {
		IPFW_RUNLOCK(chain);
		return (ENOSPC);
	}
	i = 0;
	LIST_FOREACH(ptr, &chain->nat, _next) {
		if (ptr->lib->logDesc == NULL)
			continue;
		bcopy(&ptr->id, &data[i], sizeof(int));
		i += sizeof(int);
		bcopy(ptr->lib->logDesc, &data[i], LIBALIAS_BUF_SIZE);
		i += LIBALIAS_BUF_SIZE;
	}
	IPFW_RUNLOCK(chain);
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
	V_ifaddr_event_tag = EVENTHANDLER_REGISTER(
	    ifaddr_event, ifaddr_change,
	    NULL, EVENTHANDLER_PRI_ANY);
}

static void
ipfw_nat_destroy(void)
{
	struct cfg_nat *ptr, *ptr_temp;
	struct ip_fw_chain *chain;

	chain = &V_layer3_chain;
	IPFW_WLOCK(chain);
	LIST_FOREACH_SAFE(ptr, &chain->nat, _next, ptr_temp) {
		LIST_REMOVE(ptr, _next);
		del_redir_spool_cfg(ptr, &ptr->redir_chain);
		LibAliasUninit(ptr->lib);
		free(ptr, M_IPFW);
	}
	EVENTHANDLER_DEREGISTER(ifaddr_event, V_ifaddr_event_tag);
	flush_nat_ptrs(chain, -1 /* flush all */);
	/* deregister ipfw_nat */
	ipfw_nat_ptr = NULL;
	lookup_nat_ptr = NULL;
	ipfw_nat_cfg_ptr = NULL;
	ipfw_nat_del_ptr = NULL;
	ipfw_nat_get_cfg_ptr = NULL;
	ipfw_nat_get_log_ptr = NULL;
	IPFW_WUNLOCK(chain);
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
