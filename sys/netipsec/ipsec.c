/*	$FreeBSD$	*/
/*	$KAME: ipsec.c,v 1.103 2001/05/24 07:14:18 sakane Exp $	*/

/*-
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * IPsec controller part.
 */

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/priv.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/hhook.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/if_enc.h>
#include <net/if_var.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <netinet/ip6.h>
#ifdef INET6
#include <netinet6/ip6_var.h>
#endif
#include <netinet/in_pcb.h>
#ifdef INET6
#include <netinet/icmp6.h>
#endif

#include <sys/types.h>
#include <netipsec/ipsec.h>
#ifdef INET6
#include <netipsec/ipsec6.h>
#endif
#include <netipsec/ah_var.h>
#include <netipsec/esp_var.h>
#include <netipsec/ipcomp.h>		/*XXX*/
#include <netipsec/ipcomp_var.h>

#include <netipsec/key.h>
#include <netipsec/keydb.h>
#include <netipsec/key_debug.h>

#include <netipsec/xform.h>

#include <machine/in_cksum.h>

#include <opencrypto/cryptodev.h>

#ifdef IPSEC_DEBUG
VNET_DEFINE(int, ipsec_debug) = 1;
#else
VNET_DEFINE(int, ipsec_debug) = 0;
#endif

/* NB: name changed so netstat doesn't use it. */
VNET_PCPUSTAT_DEFINE(struct ipsecstat, ipsec4stat);
VNET_PCPUSTAT_SYSINIT(ipsec4stat);

#ifdef VIMAGE
VNET_PCPUSTAT_SYSUNINIT(ipsec4stat);
#endif /* VIMAGE */

VNET_DEFINE(int, ip4_ah_offsetmask) = 0;	/* maybe IP_DF? */
/* DF bit on encap. 0: clear 1: set 2: copy */
VNET_DEFINE(int, ip4_ipsec_dfbit) = 0;
VNET_DEFINE(int, ip4_esp_trans_deflev) = IPSEC_LEVEL_USE;
VNET_DEFINE(int, ip4_esp_net_deflev) = IPSEC_LEVEL_USE;
VNET_DEFINE(int, ip4_ah_trans_deflev) = IPSEC_LEVEL_USE;
VNET_DEFINE(int, ip4_ah_net_deflev) = IPSEC_LEVEL_USE;
/* ECN ignore(-1)/forbidden(0)/allowed(1) */
VNET_DEFINE(int, ip4_ipsec_ecn) = 0;
VNET_DEFINE(int, ip4_esp_randpad) = -1;

static VNET_DEFINE(struct secpolicy, def_policy);
#define	V_def_policy	VNET(def_policy)
/*
 * Crypto support requirements:
 *
 *  1	require hardware support
 * -1	require software support
 *  0	take anything
 */
VNET_DEFINE(int, crypto_support) = CRYPTOCAP_F_HARDWARE | CRYPTOCAP_F_SOFTWARE;

FEATURE(ipsec, "Internet Protocol Security (IPsec)");
#ifdef IPSEC_NAT_T
FEATURE(ipsec_natt, "UDP Encapsulation of IPsec ESP Packets ('NAT-T')");
#endif

SYSCTL_DECL(_net_inet_ipsec);

/* net.inet.ipsec */
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_DEF_POLICY, def_policy,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(def_policy).policy, 0,
	"IPsec default policy.");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_DEF_ESP_TRANSLEV, esp_trans_deflev,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip4_esp_trans_deflev), 0,
	"Default ESP transport mode level");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_DEF_ESP_NETLEV, esp_net_deflev,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip4_esp_net_deflev), 0,
	"Default ESP tunnel mode level.");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_DEF_AH_TRANSLEV, ah_trans_deflev,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip4_ah_trans_deflev), 0,
	"AH transfer mode default level.");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_DEF_AH_NETLEV, ah_net_deflev,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip4_ah_net_deflev), 0,
	"AH tunnel mode default level.");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_AH_CLEARTOS, ah_cleartos,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ah_cleartos), 0,
	"If set clear type-of-service field when doing AH computation.");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_AH_OFFSETMASK, ah_offsetmask,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip4_ah_offsetmask), 0,
	"If not set clear offset field mask when doing AH computation.");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_DFBIT, dfbit,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip4_ipsec_dfbit), 0,
	"Do not fragment bit on encap.");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_ECN, ecn,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip4_ipsec_ecn), 0,
	"Explicit Congestion Notification handling.");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_DEBUG, debug,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ipsec_debug), 0,
	"Enable IPsec debugging output when set.");
SYSCTL_INT(_net_inet_ipsec, OID_AUTO, crypto_support,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(crypto_support), 0,
	"Crypto driver selection.");
SYSCTL_VNET_PCPUSTAT(_net_inet_ipsec, OID_AUTO, ipsecstats, struct ipsecstat,
    ipsec4stat, "IPsec IPv4 statistics.");

#ifdef REGRESSION
/*
 * When set to 1, IPsec will send packets with the same sequence number.
 * This allows to verify if the other side has proper replay attacks detection.
 */
VNET_DEFINE(int, ipsec_replay) = 0;
SYSCTL_INT(_net_inet_ipsec, OID_AUTO, test_replay,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ipsec_replay), 0,
	"Emulate replay attack");
/*
 * When set 1, IPsec will send packets with corrupted HMAC.
 * This allows to verify if the other side properly detects modified packets.
 */
VNET_DEFINE(int, ipsec_integrity) = 0;
SYSCTL_INT(_net_inet_ipsec, OID_AUTO, test_integrity,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ipsec_integrity), 0,
	"Emulate man-in-the-middle attack");
#endif

#ifdef INET6 
VNET_PCPUSTAT_DEFINE(struct ipsecstat, ipsec6stat);
VNET_PCPUSTAT_SYSINIT(ipsec6stat);

#ifdef VIMAGE
VNET_PCPUSTAT_SYSUNINIT(ipsec6stat);
#endif /* VIMAGE */

VNET_DEFINE(int, ip6_esp_trans_deflev) = IPSEC_LEVEL_USE;
VNET_DEFINE(int, ip6_esp_net_deflev) = IPSEC_LEVEL_USE;
VNET_DEFINE(int, ip6_ah_trans_deflev) = IPSEC_LEVEL_USE;
VNET_DEFINE(int, ip6_ah_net_deflev) = IPSEC_LEVEL_USE;
VNET_DEFINE(int, ip6_ipsec_ecn) = 0;	/* ECN ignore(-1)/forbidden(0)/allowed(1) */

SYSCTL_DECL(_net_inet6_ipsec6);

/* net.inet6.ipsec6 */
SYSCTL_INT(_net_inet6_ipsec6, IPSECCTL_DEF_POLICY, def_policy,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(def_policy).policy, 0,
	"IPsec default policy.");
SYSCTL_INT(_net_inet6_ipsec6, IPSECCTL_DEF_ESP_TRANSLEV, esp_trans_deflev,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_esp_trans_deflev), 0,
	"Default ESP transport mode level.");
SYSCTL_INT(_net_inet6_ipsec6, IPSECCTL_DEF_ESP_NETLEV, esp_net_deflev,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_esp_net_deflev), 0,
	"Default ESP tunnel mode level.");
SYSCTL_INT(_net_inet6_ipsec6, IPSECCTL_DEF_AH_TRANSLEV, ah_trans_deflev,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_ah_trans_deflev), 0,
	"AH transfer mode default level.");
SYSCTL_INT(_net_inet6_ipsec6, IPSECCTL_DEF_AH_NETLEV, ah_net_deflev,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_ah_net_deflev), 0,
	"AH tunnel mode default level.");
SYSCTL_INT(_net_inet6_ipsec6, IPSECCTL_ECN, ecn,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_ipsec_ecn), 0,
	"Explicit Congestion Notification handling.");
SYSCTL_INT(_net_inet6_ipsec6, IPSECCTL_DEBUG, debug,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ipsec_debug), 0,
	"Enable IPsec debugging output when set.");
SYSCTL_VNET_PCPUSTAT(_net_inet6_ipsec6, IPSECCTL_STATS, ipsecstats,
    struct ipsecstat, ipsec6stat, "IPsec IPv6 statistics.");
#endif /* INET6 */

static int ipsec_in_reject(struct secpolicy *, struct mbuf *);
static int ipsec_setspidx_inpcb(struct mbuf *, struct inpcb *);
static int ipsec_setspidx(struct mbuf *, struct secpolicyindex *, int);
static void ipsec4_get_ulp(struct mbuf *m, struct secpolicyindex *, int);
static int ipsec4_setspidx_ipaddr(struct mbuf *, struct secpolicyindex *);
#ifdef INET6
static void ipsec6_get_ulp(struct mbuf *m, struct secpolicyindex *, int);
static int ipsec6_setspidx_ipaddr(struct mbuf *, struct secpolicyindex *);
#endif
static void ipsec_delpcbpolicy(struct inpcbpolicy *);
static struct secpolicy *ipsec_deepcopy_policy(struct secpolicy *src);
static void vshiftl(unsigned char *, int, int);

MALLOC_DEFINE(M_IPSEC_INPCB, "inpcbpolicy", "inpcb-resident ipsec policy");

/*
 * Return a held reference to the default SP.
 */
static struct secpolicy *
key_allocsp_default(const char* where, int tag)
{
	struct secpolicy *sp;

	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP key_allocsp_default from %s:%u\n", where, tag));

	sp = &V_def_policy;
	if (sp->policy != IPSEC_POLICY_DISCARD &&
	    sp->policy != IPSEC_POLICY_NONE) {
		ipseclog((LOG_INFO, "fixed system default policy: %d->%d\n",
		    sp->policy, IPSEC_POLICY_NONE));
		sp->policy = IPSEC_POLICY_NONE;
	}
	key_addref(sp);

	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP key_allocsp_default returns SP:%p (%u)\n",
			sp, sp->refcnt));
	return (sp);
}
#define	KEY_ALLOCSP_DEFAULT() \
	key_allocsp_default(__FILE__, __LINE__)

/*
 * For OUTBOUND packet having a socket. Searching SPD for packet,
 * and return a pointer to SP.
 * OUT:	NULL:	no apropreate SP found, the following value is set to error.
 *		0	: bypass
 *		EACCES	: discard packet.
 *		ENOENT	: ipsec_acquire() in progress, maybe.
 *		others	: error occured.
 *	others:	a pointer to SP
 *
 * NOTE: IPv6 mapped adddress concern is implemented here.
 */
struct secpolicy *
ipsec_getpolicy(struct tdb_ident *tdbi, u_int dir)
{
	struct secpolicy *sp;

	IPSEC_ASSERT(tdbi != NULL, ("null tdbi"));
	IPSEC_ASSERT(dir == IPSEC_DIR_INBOUND || dir == IPSEC_DIR_OUTBOUND,
		("invalid direction %u", dir));

	sp = KEY_ALLOCSP2(tdbi->spi, &tdbi->dst, tdbi->proto, dir);
	if (sp == NULL)			/*XXX????*/
		sp = KEY_ALLOCSP_DEFAULT();
	IPSEC_ASSERT(sp != NULL, ("null SP"));
	return (sp);
}

/*
 * For OUTBOUND packet having a socket. Searching SPD for packet,
 * and return a pointer to SP.
 * OUT:	NULL:	no apropreate SP found, the following value is set to error.
 *		0	: bypass
 *		EACCES	: discard packet.
 *		ENOENT	: ipsec_acquire() in progress, maybe.
 *		others	: error occured.
 *	others:	a pointer to SP
 *
 * NOTE: IPv6 mapped adddress concern is implemented here.
 */
static struct secpolicy *
ipsec_getpolicybysock(struct mbuf *m, u_int dir, struct inpcb *inp, int *error)
{
	struct inpcbpolicy *pcbsp;
	struct secpolicy *currsp = NULL;	/* Policy on socket. */
	struct secpolicy *sp;

	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(inp != NULL, ("null inpcb"));
	IPSEC_ASSERT(error != NULL, ("null error"));
	IPSEC_ASSERT(dir == IPSEC_DIR_INBOUND || dir == IPSEC_DIR_OUTBOUND,
		("invalid direction %u", dir));

	if (!key_havesp(dir)) {
		/* No SP found, use system default. */
		sp = KEY_ALLOCSP_DEFAULT();
		return (sp);
	}

	/* Set spidx in pcb. */
	*error = ipsec_setspidx_inpcb(m, inp);
	if (*error)
		return (NULL);

	pcbsp = inp->inp_sp;
	IPSEC_ASSERT(pcbsp != NULL, ("null pcbsp"));
	switch (dir) {
	case IPSEC_DIR_INBOUND:
		currsp = pcbsp->sp_in;
		break;
	case IPSEC_DIR_OUTBOUND:
		currsp = pcbsp->sp_out;
		break;
	}
	IPSEC_ASSERT(currsp != NULL, ("null currsp"));

	if (pcbsp->priv) {			/* When privilieged socket. */
		switch (currsp->policy) {
		case IPSEC_POLICY_BYPASS:
		case IPSEC_POLICY_IPSEC:
			key_addref(currsp);
			sp = currsp;
			break;

		case IPSEC_POLICY_ENTRUST:
			/* Look for a policy in SPD. */
			sp = KEY_ALLOCSP(&currsp->spidx, dir);
			if (sp == NULL)		/* No SP found. */
				sp = KEY_ALLOCSP_DEFAULT();
			break;

		default:
			ipseclog((LOG_ERR, "%s: Invalid policy for PCB %d\n",
				__func__, currsp->policy));
			*error = EINVAL;
			return (NULL);
		}
	} else {				/* Unpriv, SPD has policy. */
		sp = KEY_ALLOCSP(&currsp->spidx, dir);
		if (sp == NULL) {		/* No SP found. */
			switch (currsp->policy) {
			case IPSEC_POLICY_BYPASS:
				ipseclog((LOG_ERR, "%s: Illegal policy for "
					"non-priviliged defined %d\n",
					__func__, currsp->policy));
				*error = EINVAL;
				return (NULL);

			case IPSEC_POLICY_ENTRUST:
				sp = KEY_ALLOCSP_DEFAULT();
				break;

			case IPSEC_POLICY_IPSEC:
				key_addref(currsp);
				sp = currsp;
				break;

			default:
				ipseclog((LOG_ERR, "%s: Invalid policy for "
					"PCB %d\n", __func__, currsp->policy));
				*error = EINVAL;
				return (NULL);
			}
		}
	}
	IPSEC_ASSERT(sp != NULL,
		("null SP (priv %u policy %u", pcbsp->priv, currsp->policy));
	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP %s (priv %u policy %u) allocate SP:%p (refcnt %u)\n",
			__func__, pcbsp->priv, currsp->policy, sp, sp->refcnt));
	return (sp);
}

/*
 * For FORWADING packet or OUTBOUND without a socket. Searching SPD for packet,
 * and return a pointer to SP.
 * OUT:	positive: a pointer to the entry for security policy leaf matched.
 *	NULL:	no apropreate SP found, the following value is set to error.
 *		0	: bypass
 *		EACCES	: discard packet.
 *		ENOENT	: ipsec_acquire() in progress, maybe.
 *		others	: error occured.
 */
struct secpolicy *
ipsec_getpolicybyaddr(struct mbuf *m, u_int dir, int *error)
{
	struct secpolicyindex spidx;
	struct secpolicy *sp;

	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(error != NULL, ("null error"));
	IPSEC_ASSERT(dir == IPSEC_DIR_INBOUND || dir == IPSEC_DIR_OUTBOUND,
		("invalid direction %u", dir));

	sp = NULL;
	*error = 0;
	if (key_havesp(dir)) {
		/* Make an index to look for a policy. */
		*error = ipsec_setspidx(m, &spidx, 0);
		if (*error != 0) {
			DPRINTF(("%s: setpidx failed, dir %u\n",
				__func__, dir));
			return (NULL);
		}
		spidx.dir = dir;
		sp = KEY_ALLOCSP(&spidx, dir);
	}
	if (sp == NULL)			/* No SP found, use system default. */
		sp = KEY_ALLOCSP_DEFAULT();
	IPSEC_ASSERT(sp != NULL, ("null SP"));
	return (sp);
}

struct secpolicy *
ipsec4_checkpolicy(struct mbuf *m, u_int dir, int *error, struct inpcb *inp)
{
	struct secpolicy *sp;

	*error = 0;
	if (inp == NULL)
		sp = ipsec_getpolicybyaddr(m, dir, error);
	else
		sp = ipsec_getpolicybysock(m, dir, inp, error);
	if (sp == NULL) {
		IPSEC_ASSERT(*error != 0, ("getpolicy failed w/o error"));
		IPSECSTAT_INC(ips_out_inval);
		return (NULL);
	}
	IPSEC_ASSERT(*error == 0, ("sp w/ error set to %u", *error));
	switch (sp->policy) {
	case IPSEC_POLICY_ENTRUST:
	default:
		printf("%s: invalid policy %u\n", __func__, sp->policy);
		/* FALLTHROUGH */
	case IPSEC_POLICY_DISCARD:
		IPSECSTAT_INC(ips_out_polvio);
		*error = -EINVAL;	/* Packet is discarded by caller. */
		break;
	case IPSEC_POLICY_BYPASS:
	case IPSEC_POLICY_NONE:
		KEY_FREESP(&sp);
		sp = NULL;		/* NB: force NULL result. */
		break;
	case IPSEC_POLICY_IPSEC:
		if (sp->req == NULL)	/* Acquire a SA. */
			*error = key_spdacquire(sp);
		break;
	}
	if (*error != 0) {
		KEY_FREESP(&sp);
		sp = NULL;
	}
	return (sp);
}

static int
ipsec_setspidx_inpcb(struct mbuf *m, struct inpcb *inp)
{
	int error;

	IPSEC_ASSERT(inp != NULL, ("null inp"));
	IPSEC_ASSERT(inp->inp_sp != NULL, ("null inp_sp"));
	IPSEC_ASSERT(inp->inp_sp->sp_out != NULL && inp->inp_sp->sp_in != NULL,
		("null sp_in || sp_out"));

	error = ipsec_setspidx(m, &inp->inp_sp->sp_in->spidx, 1);
	if (error == 0) {
		inp->inp_sp->sp_in->spidx.dir = IPSEC_DIR_INBOUND;
		inp->inp_sp->sp_out->spidx = inp->inp_sp->sp_in->spidx;
		inp->inp_sp->sp_out->spidx.dir = IPSEC_DIR_OUTBOUND;
	} else {
		bzero(&inp->inp_sp->sp_in->spidx,
			sizeof (inp->inp_sp->sp_in->spidx));
		bzero(&inp->inp_sp->sp_out->spidx,
			sizeof (inp->inp_sp->sp_in->spidx));
	}
	return (error);
}

/*
 * Configure security policy index (src/dst/proto/sport/dport)
 * by looking at the content of mbuf.
 * The caller is responsible for error recovery (like clearing up spidx).
 */
static int
ipsec_setspidx(struct mbuf *m, struct secpolicyindex *spidx, int needport)
{
	struct ip *ip = NULL;
	struct ip ipbuf;
	u_int v;
	struct mbuf *n;
	int len;
	int error;

	IPSEC_ASSERT(m != NULL, ("null mbuf"));

	/*
	 * Validate m->m_pkthdr.len.  We see incorrect length if we
	 * mistakenly call this function with inconsistent mbuf chain
	 * (like 4.4BSD tcp/udp processing).  XXX Should we panic here?
	 */
	len = 0;
	for (n = m; n; n = n->m_next)
		len += n->m_len;
	if (m->m_pkthdr.len != len) {
		KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
			printf("%s: pkthdr len(%d) mismatch (%d), ignored.\n",
				__func__, len, m->m_pkthdr.len));
		return (EINVAL);
	}

	if (m->m_pkthdr.len < sizeof(struct ip)) {
		KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
			printf("%s: pkthdr len(%d) too small (v4), ignored.\n",
			    __func__, m->m_pkthdr.len));
		return (EINVAL);
	}

	if (m->m_len >= sizeof(*ip))
		ip = mtod(m, struct ip *);
	else {
		m_copydata(m, 0, sizeof(ipbuf), (caddr_t)&ipbuf);
		ip = &ipbuf;
	}
	v = ip->ip_v;
	switch (v) {
	case 4:
		error = ipsec4_setspidx_ipaddr(m, spidx);
		if (error)
			return (error);
		ipsec4_get_ulp(m, spidx, needport);
		return (0);
#ifdef INET6
	case 6:
		if (m->m_pkthdr.len < sizeof(struct ip6_hdr)) {
			KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
				printf("%s: pkthdr len(%d) too small (v6), "
				"ignored\n", __func__, m->m_pkthdr.len));
			return (EINVAL);
		}
		error = ipsec6_setspidx_ipaddr(m, spidx);
		if (error)
			return (error);
		ipsec6_get_ulp(m, spidx, needport);
		return (0);
#endif
	default:
		KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
			printf("%s: " "unknown IP version %u, ignored.\n",
				__func__, v));
		return (EINVAL);
	}
}

static void
ipsec4_get_ulp(struct mbuf *m, struct secpolicyindex *spidx, int needport)
{
	u_int8_t nxt;
	int off;

	/* Sanity check. */
	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(m->m_pkthdr.len >= sizeof(struct ip),("packet too short"));

	if (m->m_len >= sizeof (struct ip)) {
		struct ip *ip = mtod(m, struct ip *);
		if (ip->ip_off & htons(IP_MF | IP_OFFMASK))
			goto done;
		off = ip->ip_hl << 2;
		nxt = ip->ip_p;
	} else {
		struct ip ih;

		m_copydata(m, 0, sizeof (struct ip), (caddr_t) &ih);
		if (ih.ip_off & htons(IP_MF | IP_OFFMASK))
			goto done;
		off = ih.ip_hl << 2;
		nxt = ih.ip_p;
	}

	while (off < m->m_pkthdr.len) {
		struct ip6_ext ip6e;
		struct tcphdr th;
		struct udphdr uh;

		switch (nxt) {
		case IPPROTO_TCP:
			spidx->ul_proto = nxt;
			if (!needport)
				goto done_proto;
			if (off + sizeof(struct tcphdr) > m->m_pkthdr.len)
				goto done;
			m_copydata(m, off, sizeof (th), (caddr_t) &th);
			spidx->src.sin.sin_port = th.th_sport;
			spidx->dst.sin.sin_port = th.th_dport;
			return;
		case IPPROTO_UDP:
			spidx->ul_proto = nxt;
			if (!needport)
				goto done_proto;
			if (off + sizeof(struct udphdr) > m->m_pkthdr.len)
				goto done;
			m_copydata(m, off, sizeof (uh), (caddr_t) &uh);
			spidx->src.sin.sin_port = uh.uh_sport;
			spidx->dst.sin.sin_port = uh.uh_dport;
			return;
		case IPPROTO_AH:
			if (off + sizeof(ip6e) > m->m_pkthdr.len)
				goto done;
			/* XXX Sigh, this works but is totally bogus. */
			m_copydata(m, off, sizeof(ip6e), (caddr_t) &ip6e);
			off += (ip6e.ip6e_len + 2) << 2;
			nxt = ip6e.ip6e_nxt;
			break;
		case IPPROTO_ICMP:
		default:
			/* XXX Intermediate headers??? */
			spidx->ul_proto = nxt;
			goto done_proto;
		}
	}
done:
	spidx->ul_proto = IPSEC_ULPROTO_ANY;
done_proto:
	spidx->src.sin.sin_port = IPSEC_PORT_ANY;
	spidx->dst.sin.sin_port = IPSEC_PORT_ANY;
}

/* Assumes that m is sane. */
static int
ipsec4_setspidx_ipaddr(struct mbuf *m, struct secpolicyindex *spidx)
{
	static const struct sockaddr_in template = {
		sizeof (struct sockaddr_in),
		AF_INET,
		0, { 0 }, { 0, 0, 0, 0, 0, 0, 0, 0 }
	};

	spidx->src.sin = template;
	spidx->dst.sin = template;

	if (m->m_len < sizeof (struct ip)) {
		m_copydata(m, offsetof(struct ip, ip_src),
			   sizeof (struct  in_addr),
			   (caddr_t) &spidx->src.sin.sin_addr);
		m_copydata(m, offsetof(struct ip, ip_dst),
			   sizeof (struct  in_addr),
			   (caddr_t) &spidx->dst.sin.sin_addr);
	} else {
		struct ip *ip = mtod(m, struct ip *);
		spidx->src.sin.sin_addr = ip->ip_src;
		spidx->dst.sin.sin_addr = ip->ip_dst;
	}

	spidx->prefs = sizeof(struct in_addr) << 3;
	spidx->prefd = sizeof(struct in_addr) << 3;

	return (0);
}

#ifdef INET6
static void
ipsec6_get_ulp(struct mbuf *m, struct secpolicyindex *spidx, int needport)
{
	int off, nxt;
	struct tcphdr th;
	struct udphdr uh;
	struct icmp6_hdr ih;

	/* Sanity check. */
	if (m == NULL)
		panic("%s: NULL pointer was passed.\n", __func__);

	KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
		printf("%s:\n", __func__); kdebug_mbuf(m));

	/* Set default. */
	spidx->ul_proto = IPSEC_ULPROTO_ANY;
	((struct sockaddr_in6 *)&spidx->src)->sin6_port = IPSEC_PORT_ANY;
	((struct sockaddr_in6 *)&spidx->dst)->sin6_port = IPSEC_PORT_ANY;

	nxt = -1;
	off = ip6_lasthdr(m, 0, IPPROTO_IPV6, &nxt);
	if (off < 0 || m->m_pkthdr.len < off)
		return;

	switch (nxt) {
	case IPPROTO_TCP:
		spidx->ul_proto = nxt;
		if (!needport)
			break;
		if (off + sizeof(struct tcphdr) > m->m_pkthdr.len)
			break;
		m_copydata(m, off, sizeof(th), (caddr_t)&th);
		((struct sockaddr_in6 *)&spidx->src)->sin6_port = th.th_sport;
		((struct sockaddr_in6 *)&spidx->dst)->sin6_port = th.th_dport;
		break;
	case IPPROTO_UDP:
		spidx->ul_proto = nxt;
		if (!needport)
			break;
		if (off + sizeof(struct udphdr) > m->m_pkthdr.len)
			break;
		m_copydata(m, off, sizeof(uh), (caddr_t)&uh);
		((struct sockaddr_in6 *)&spidx->src)->sin6_port = uh.uh_sport;
		((struct sockaddr_in6 *)&spidx->dst)->sin6_port = uh.uh_dport;
		break;
	case IPPROTO_ICMPV6:
		spidx->ul_proto = nxt;
		if (off + sizeof(struct icmp6_hdr) > m->m_pkthdr.len)
			break;
		m_copydata(m, off, sizeof(ih), (caddr_t)&ih);
		((struct sockaddr_in6 *)&spidx->src)->sin6_port =
		    htons((uint16_t)ih.icmp6_type);
		((struct sockaddr_in6 *)&spidx->dst)->sin6_port =
		    htons((uint16_t)ih.icmp6_code);
		break;
	default:
		/* XXX Intermediate headers??? */
		spidx->ul_proto = nxt;
		break;
	}
}

/* Assumes that m is sane. */
static int
ipsec6_setspidx_ipaddr(struct mbuf *m, struct secpolicyindex *spidx)
{
	struct ip6_hdr *ip6 = NULL;
	struct ip6_hdr ip6buf;
	struct sockaddr_in6 *sin6;

	if (m->m_len >= sizeof(*ip6))
		ip6 = mtod(m, struct ip6_hdr *);
	else {
		m_copydata(m, 0, sizeof(ip6buf), (caddr_t)&ip6buf);
		ip6 = &ip6buf;
	}

	sin6 = (struct sockaddr_in6 *)&spidx->src;
	bzero(sin6, sizeof(*sin6));
	sin6->sin6_family = AF_INET6;
	sin6->sin6_len = sizeof(struct sockaddr_in6);
	bcopy(&ip6->ip6_src, &sin6->sin6_addr, sizeof(ip6->ip6_src));
	if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_src)) {
		sin6->sin6_addr.s6_addr16[1] = 0;
		sin6->sin6_scope_id = ntohs(ip6->ip6_src.s6_addr16[1]);
	}
	spidx->prefs = sizeof(struct in6_addr) << 3;

	sin6 = (struct sockaddr_in6 *)&spidx->dst;
	bzero(sin6, sizeof(*sin6));
	sin6->sin6_family = AF_INET6;
	sin6->sin6_len = sizeof(struct sockaddr_in6);
	bcopy(&ip6->ip6_dst, &sin6->sin6_addr, sizeof(ip6->ip6_dst));
	if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_dst)) {
		sin6->sin6_addr.s6_addr16[1] = 0;
		sin6->sin6_scope_id = ntohs(ip6->ip6_dst.s6_addr16[1]);
	}
	spidx->prefd = sizeof(struct in6_addr) << 3;

	return (0);
}
#endif

int
ipsec_run_hhooks(struct ipsec_ctx_data *ctx, int type)
{
	int idx;

	switch (ctx->af) {
#ifdef INET
	case AF_INET:
		idx = HHOOK_IPSEC_INET;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		idx = HHOOK_IPSEC_INET6;
		break;
#endif
	default:
		return (EPFNOSUPPORT);
	}
	if (type == HHOOK_TYPE_IPSEC_IN)
		HHOOKS_RUN_IF(V_ipsec_hhh_in[idx], ctx, NULL);
	else
		HHOOKS_RUN_IF(V_ipsec_hhh_out[idx], ctx, NULL);
	if (*ctx->mp == NULL)
		return (EACCES);
	return (0);
}

static void
ipsec_delpcbpolicy(struct inpcbpolicy *p)
{

	free(p, M_IPSEC_INPCB);
}

/* Initialize policy in PCB. */
int
ipsec_init_policy(struct socket *so, struct inpcbpolicy **pcb_sp)
{
	struct inpcbpolicy *new;

	/* Sanity check. */
	if (so == NULL || pcb_sp == NULL)
		panic("%s: NULL pointer was passed.\n", __func__);

	new = (struct inpcbpolicy *) malloc(sizeof(struct inpcbpolicy),
					    M_IPSEC_INPCB, M_NOWAIT|M_ZERO);
	if (new == NULL) {
		ipseclog((LOG_DEBUG, "%s: No more memory.\n", __func__));
		return (ENOBUFS);
	}

	new->priv = IPSEC_IS_PRIVILEGED_SO(so);

	if ((new->sp_in = KEY_NEWSP()) == NULL) {
		ipsec_delpcbpolicy(new);
		return (ENOBUFS);
	}
	new->sp_in->policy = IPSEC_POLICY_ENTRUST;
	if ((new->sp_out = KEY_NEWSP()) == NULL) {
		KEY_FREESP(&new->sp_in);
		ipsec_delpcbpolicy(new);
		return (ENOBUFS);
	}
	new->sp_out->policy = IPSEC_POLICY_ENTRUST;
	*pcb_sp = new;

	return (0);
}

/* Copy old IPsec policy into new. */
int
ipsec_copy_policy(struct inpcbpolicy *old, struct inpcbpolicy *new)
{
	struct secpolicy *sp;

	sp = ipsec_deepcopy_policy(old->sp_in);
	if (sp) {
		KEY_FREESP(&new->sp_in);
		new->sp_in = sp;
	} else
		return (ENOBUFS);

	sp = ipsec_deepcopy_policy(old->sp_out);
	if (sp) {
		KEY_FREESP(&new->sp_out);
		new->sp_out = sp;
	} else
		return (ENOBUFS);

	new->priv = old->priv;

	return (0);
}

struct ipsecrequest *
ipsec_newisr(void)
{
	struct ipsecrequest *p;

	p = malloc(sizeof(struct ipsecrequest), M_IPSEC_SR, M_NOWAIT|M_ZERO);
	if (p != NULL)
		IPSECREQUEST_LOCK_INIT(p);
	return (p);
}

void
ipsec_delisr(struct ipsecrequest *p)
{

	IPSECREQUEST_LOCK_DESTROY(p);
	free(p, M_IPSEC_SR);
}

/* Deep-copy a policy in PCB. */
static struct secpolicy *
ipsec_deepcopy_policy(struct secpolicy *src)
{
	struct ipsecrequest *newchain = NULL;
	struct ipsecrequest *p;
	struct ipsecrequest **q;
	struct ipsecrequest *r;
	struct secpolicy *dst;

	if (src == NULL)
		return (NULL);
	dst = KEY_NEWSP();
	if (dst == NULL)
		return (NULL);

	/*
	 * Deep-copy IPsec request chain.  This is required since struct
	 * ipsecrequest is not reference counted.
	 */
	q = &newchain;
	for (p = src->req; p; p = p->next) {
		*q = ipsec_newisr();
		if (*q == NULL)
			goto fail;
		(*q)->saidx.proto = p->saidx.proto;
		(*q)->saidx.mode = p->saidx.mode;
		(*q)->level = p->level;
		(*q)->saidx.reqid = p->saidx.reqid;

		bcopy(&p->saidx.src, &(*q)->saidx.src, sizeof((*q)->saidx.src));
		bcopy(&p->saidx.dst, &(*q)->saidx.dst, sizeof((*q)->saidx.dst));

		(*q)->sp = dst;

		q = &((*q)->next);
	}

	dst->req = newchain;
	dst->policy = src->policy;
	/* Do not touch the refcnt fields. */

	return (dst);

fail:
	for (p = newchain; p; p = r) {
		r = p->next;
		ipsec_delisr(p);
		p = NULL;
	}
	return (NULL);
}

/* Set policy and IPsec request if present. */
static int
ipsec_set_policy_internal(struct secpolicy **pcb_sp, int optname,
    caddr_t request, size_t len, struct ucred *cred)
{
	struct sadb_x_policy *xpl;
	struct secpolicy *newsp = NULL;
	int error;

	/* Sanity check. */
	if (pcb_sp == NULL || *pcb_sp == NULL || request == NULL)
		return (EINVAL);
	if (len < sizeof(*xpl))
		return (EINVAL);
	xpl = (struct sadb_x_policy *)request;

	KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
		printf("%s: passed policy\n", __func__);
		kdebug_sadb_x_policy((struct sadb_ext *)xpl));

	/* Check policy type. */
	/* ipsec_set_policy_internal() accepts IPSEC, ENTRUST and BYPASS. */
	if (xpl->sadb_x_policy_type == IPSEC_POLICY_DISCARD
	 || xpl->sadb_x_policy_type == IPSEC_POLICY_NONE)
		return (EINVAL);

	/* Check privileged socket. */
	if (cred != NULL && xpl->sadb_x_policy_type == IPSEC_POLICY_BYPASS) {
		error = priv_check_cred(cred, PRIV_NETINET_IPSEC, 0);
		if (error)
			return (EACCES);
	}

	/* Allocating new SP entry. */
	if ((newsp = key_msg2sp(xpl, len, &error)) == NULL)
		return (error);

	/* Clear old SP and set new SP. */
	KEY_FREESP(pcb_sp);
	*pcb_sp = newsp;
	KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
		printf("%s: new policy\n", __func__);
		kdebug_secpolicy(newsp));

	return (0);
}

int
ipsec_set_policy(struct inpcb *inp, int optname, caddr_t request,
    size_t len, struct ucred *cred)
{
	struct sadb_x_policy *xpl;
	struct secpolicy **pcb_sp;

	/* Sanity check. */
	if (inp == NULL || request == NULL)
		return (EINVAL);
	if (len < sizeof(*xpl))
		return (EINVAL);
	xpl = (struct sadb_x_policy *)request;

	/* Select direction. */
	switch (xpl->sadb_x_policy_dir) {
	case IPSEC_DIR_INBOUND:
		pcb_sp = &inp->inp_sp->sp_in;
		break;
	case IPSEC_DIR_OUTBOUND:
		pcb_sp = &inp->inp_sp->sp_out;
		break;
	default:
		ipseclog((LOG_ERR, "%s: invalid direction=%u\n", __func__,
			xpl->sadb_x_policy_dir));
		return (EINVAL);
	}

	return (ipsec_set_policy_internal(pcb_sp, optname, request, len, cred));
}

int
ipsec_get_policy(struct inpcb *inp, caddr_t request, size_t len,
    struct mbuf **mp)
{
	struct sadb_x_policy *xpl;
	struct secpolicy *pcb_sp;

	/* Sanity check. */
	if (inp == NULL || request == NULL || mp == NULL)
		return (EINVAL);
	IPSEC_ASSERT(inp->inp_sp != NULL, ("null inp_sp"));
	if (len < sizeof(*xpl))
		return (EINVAL);
	xpl = (struct sadb_x_policy *)request;

	/* Select direction. */
	switch (xpl->sadb_x_policy_dir) {
	case IPSEC_DIR_INBOUND:
		pcb_sp = inp->inp_sp->sp_in;
		break;
	case IPSEC_DIR_OUTBOUND:
		pcb_sp = inp->inp_sp->sp_out;
		break;
	default:
		ipseclog((LOG_ERR, "%s: invalid direction=%u\n", __func__,
			xpl->sadb_x_policy_dir));
		return (EINVAL);
	}

	/* Sanity check. Should be an IPSEC_ASSERT. */
	if (pcb_sp == NULL)
		return (EINVAL);

	*mp = key_sp2msg(pcb_sp);
	if (!*mp) {
		ipseclog((LOG_DEBUG, "%s: No more memory.\n", __func__));
		return (ENOBUFS);
	}

	(*mp)->m_type = MT_DATA;
	KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
		printf("%s:\n", __func__); kdebug_mbuf(*mp));

	return (0);
}

/* Delete policy in PCB. */
int
ipsec_delete_pcbpolicy(struct inpcb *inp)
{
	IPSEC_ASSERT(inp != NULL, ("null inp"));

	if (inp->inp_sp == NULL)
		return (0);

	if (inp->inp_sp->sp_in != NULL)
		KEY_FREESP(&inp->inp_sp->sp_in);

	if (inp->inp_sp->sp_out != NULL)
		KEY_FREESP(&inp->inp_sp->sp_out);

	ipsec_delpcbpolicy(inp->inp_sp);
	inp->inp_sp = NULL;

	return (0);
}

/*
 * Return current level.
 * Either IPSEC_LEVEL_USE or IPSEC_LEVEL_REQUIRE are always returned.
 */
u_int
ipsec_get_reqlevel(struct ipsecrequest *isr)
{
	u_int level = 0;
	u_int esp_trans_deflev, esp_net_deflev;
	u_int ah_trans_deflev, ah_net_deflev;

	IPSEC_ASSERT(isr != NULL && isr->sp != NULL, ("null argument"));
	IPSEC_ASSERT(isr->sp->spidx.src.sa.sa_family == isr->sp->spidx.dst.sa.sa_family,
		("af family mismatch, src %u, dst %u",
		 isr->sp->spidx.src.sa.sa_family,
		 isr->sp->spidx.dst.sa.sa_family));

/* XXX Note that we have ipseclog() expanded here - code sync issue. */
#define IPSEC_CHECK_DEFAULT(lev) \
	(((lev) != IPSEC_LEVEL_USE && (lev) != IPSEC_LEVEL_REQUIRE	      \
			&& (lev) != IPSEC_LEVEL_UNIQUE)			      \
		? (V_ipsec_debug						      \
			? log(LOG_INFO, "fixed system default level " #lev ":%d->%d\n",\
				(lev), IPSEC_LEVEL_REQUIRE)		      \
			: 0),						      \
			(lev) = IPSEC_LEVEL_REQUIRE,			      \
			(lev)						      \
		: (lev))

	/* Set default level. */
	switch (((struct sockaddr *)&isr->sp->spidx.src)->sa_family) {
#ifdef INET
	case AF_INET:
		esp_trans_deflev = IPSEC_CHECK_DEFAULT(V_ip4_esp_trans_deflev);
		esp_net_deflev = IPSEC_CHECK_DEFAULT(V_ip4_esp_net_deflev);
		ah_trans_deflev = IPSEC_CHECK_DEFAULT(V_ip4_ah_trans_deflev);
		ah_net_deflev = IPSEC_CHECK_DEFAULT(V_ip4_ah_net_deflev);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		esp_trans_deflev = IPSEC_CHECK_DEFAULT(V_ip6_esp_trans_deflev);
		esp_net_deflev = IPSEC_CHECK_DEFAULT(V_ip6_esp_net_deflev);
		ah_trans_deflev = IPSEC_CHECK_DEFAULT(V_ip6_ah_trans_deflev);
		ah_net_deflev = IPSEC_CHECK_DEFAULT(V_ip6_ah_net_deflev);
		break;
#endif /* INET6 */
	default:
		panic("%s: unknown af %u",
			__func__, isr->sp->spidx.src.sa.sa_family);
	}

#undef IPSEC_CHECK_DEFAULT

	/* Set level. */
	switch (isr->level) {
	case IPSEC_LEVEL_DEFAULT:
		switch (isr->saidx.proto) {
		case IPPROTO_ESP:
			if (isr->saidx.mode == IPSEC_MODE_TUNNEL)
				level = esp_net_deflev;
			else
				level = esp_trans_deflev;
			break;
		case IPPROTO_AH:
			if (isr->saidx.mode == IPSEC_MODE_TUNNEL)
				level = ah_net_deflev;
			else
				level = ah_trans_deflev;
			break;
		case IPPROTO_IPCOMP:
			/*
			 * We don't really care, as IPcomp document says that
			 * we shouldn't compress small packets.
			 */
			level = IPSEC_LEVEL_USE;
			break;
		default:
			panic("%s: Illegal protocol defined %u\n", __func__,
				isr->saidx.proto);
		}
		break;

	case IPSEC_LEVEL_USE:
	case IPSEC_LEVEL_REQUIRE:
		level = isr->level;
		break;
	case IPSEC_LEVEL_UNIQUE:
		level = IPSEC_LEVEL_REQUIRE;
		break;

	default:
		panic("%s: Illegal IPsec level %u\n", __func__, isr->level);
	}

	return (level);
}

/*
 * Check security policy requirements against the actual
 * packet contents.  Return one if the packet should be
 * reject as "invalid"; otherwiser return zero to have the
 * packet treated as "valid".
 *
 * OUT:
 *	0: valid
 *	1: invalid
 */
static int
ipsec_in_reject(struct secpolicy *sp, struct mbuf *m)
{
	struct ipsecrequest *isr;
	int need_auth;

	KEYDEBUG(KEYDEBUG_IPSEC_DATA,
		printf("%s: using SP\n", __func__); kdebug_secpolicy(sp));

	/* Check policy. */
	switch (sp->policy) {
	case IPSEC_POLICY_DISCARD:
		return (1);
	case IPSEC_POLICY_BYPASS:
	case IPSEC_POLICY_NONE:
		return (0);
	}

	IPSEC_ASSERT(sp->policy == IPSEC_POLICY_IPSEC,
		("invalid policy %u", sp->policy));

	/* XXX Should compare policy against IPsec header history. */

	need_auth = 0;
	for (isr = sp->req; isr != NULL; isr = isr->next) {
		if (ipsec_get_reqlevel(isr) != IPSEC_LEVEL_REQUIRE)
			continue;
		switch (isr->saidx.proto) {
		case IPPROTO_ESP:
			if ((m->m_flags & M_DECRYPTED) == 0) {
				KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
				    printf("%s: ESP m_flags:%x\n", __func__,
					    m->m_flags));
				return (1);
			}

			if (!need_auth &&
			    isr->sav != NULL &&
			    isr->sav->tdb_authalgxform != NULL &&
			    (m->m_flags & M_AUTHIPDGM) == 0) {
				KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
				    printf("%s: ESP/AH m_flags:%x\n", __func__,
					    m->m_flags));
				return (1);
			}
			break;
		case IPPROTO_AH:
			need_auth = 1;
			if ((m->m_flags & M_AUTHIPHDR) == 0) {
				KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
				    printf("%s: AH m_flags:%x\n", __func__,
					    m->m_flags));
				return (1);
			}
			break;
		case IPPROTO_IPCOMP:
			/*
			 * We don't really care, as IPcomp document
			 * says that we shouldn't compress small
			 * packets.  IPComp policy should always be
			 * treated as being in "use" level.
			 */
			break;
		}
	}
	return (0);		/* Valid. */
}

/*
 * Non zero return value means security policy DISCARD or policy violation.
 */
static int
ipsec46_in_reject(struct mbuf *m, struct inpcb *inp)
{
	struct secpolicy *sp;
	int error;
	int result;

	if (!key_havesp(IPSEC_DIR_INBOUND))
		return 0;

	IPSEC_ASSERT(m != NULL, ("null mbuf"));

	/* Get SP for this packet. */
	if (inp == NULL)
		sp = ipsec_getpolicybyaddr(m, IPSEC_DIR_INBOUND, &error);
	else
		sp = ipsec_getpolicybysock(m, IPSEC_DIR_INBOUND, inp, &error);

	if (sp != NULL) {
		result = ipsec_in_reject(sp, m);
		KEY_FREESP(&sp);
	} else {
		result = 1;	/* treat errors as policy violation */
	}
	return (result);
}

/*
 * Check AH/ESP integrity.
 * This function is called from tcp_input(), udp_input(),
 * and {ah,esp}4_input for tunnel mode.
 */
int
ipsec4_in_reject(struct mbuf *m, struct inpcb *inp)
{
	int result;

	result = ipsec46_in_reject(m, inp);
	if (result)
		IPSECSTAT_INC(ips_in_polvio);

	return (result);
}

#ifdef INET6
/*
 * Check AH/ESP integrity.
 * This function is called from tcp6_input(), udp6_input(),
 * and {ah,esp}6_input for tunnel mode.
 */
int
ipsec6_in_reject(struct mbuf *m, struct inpcb *inp)
{
	int result;

	result = ipsec46_in_reject(m, inp);
	if (result)
		IPSEC6STAT_INC(ips_in_polvio);

	return (result);
}
#endif

/*
 * Compute the byte size to be occupied by IPsec header.
 * In case it is tunnelled, it includes the size of outer IP header.
 * NOTE: SP passed is freed in this function.
 */
static size_t
ipsec_hdrsiz_internal(struct secpolicy *sp)
{
	struct ipsecrequest *isr;
	size_t size;

	KEYDEBUG(KEYDEBUG_IPSEC_DATA,
		printf("%s: using SP\n", __func__); kdebug_secpolicy(sp));

	switch (sp->policy) {
	case IPSEC_POLICY_DISCARD:
	case IPSEC_POLICY_BYPASS:
	case IPSEC_POLICY_NONE:
		return (0);
	}

	IPSEC_ASSERT(sp->policy == IPSEC_POLICY_IPSEC,
		("invalid policy %u", sp->policy));

	size = 0;
	for (isr = sp->req; isr != NULL; isr = isr->next) {
		size_t clen = 0;

		switch (isr->saidx.proto) {
		case IPPROTO_ESP:
			clen = esp_hdrsiz(isr->sav);
			break;
		case IPPROTO_AH:
			clen = ah_hdrsiz(isr->sav);
			break;
		case IPPROTO_IPCOMP:
			clen = sizeof(struct ipcomp);
			break;
		}

		if (isr->saidx.mode == IPSEC_MODE_TUNNEL) {
			switch (isr->saidx.dst.sa.sa_family) {
			case AF_INET:
				clen += sizeof(struct ip);
				break;
#ifdef INET6
			case AF_INET6:
				clen += sizeof(struct ip6_hdr);
				break;
#endif
			default:
				ipseclog((LOG_ERR, "%s: unknown AF %d in "
				    "IPsec tunnel SA\n", __func__,
				    ((struct sockaddr *)&isr->saidx.dst)->sa_family));
				break;
			}
		}
		size += clen;
	}

	return (size);
}

/* 
 * This function is called from ipsec_hdrsiz_tcp(), ip_ipsec_mtu(),
 * disabled ip6_ipsec_mtu() and ip6_forward().
 */
size_t
ipsec_hdrsiz(struct mbuf *m, u_int dir, struct inpcb *inp)
{
	struct secpolicy *sp;
	int error;
	size_t size;

	if (!key_havesp(dir))
		return 0;

	IPSEC_ASSERT(m != NULL, ("null mbuf"));

	/* Get SP for this packet. */
	if (inp == NULL)
		sp = ipsec_getpolicybyaddr(m, dir, &error);
	else
		sp = ipsec_getpolicybysock(m, dir, inp, &error);

	if (sp != NULL) {
		size = ipsec_hdrsiz_internal(sp);
		KEYDEBUG(KEYDEBUG_IPSEC_DATA,
			printf("%s: size:%lu.\n", __func__,
				(unsigned long)size));

		KEY_FREESP(&sp);
	} else {
		size = 0;	/* XXX Should be panic?
				 * -> No, we are called w/o knowing if
				 *    IPsec processing is needed. */
	}
	return (size);
}

/*
 * Check the variable replay window.
 * ipsec_chkreplay() performs replay check before ICV verification.
 * ipsec_updatereplay() updates replay bitmap.  This must be called after
 * ICV verification (it also performs replay check, which is usually done
 * beforehand).
 * 0 (zero) is returned if packet disallowed, 1 if packet permitted.
 *
 * Based on RFC 2401.
 */
int
ipsec_chkreplay(u_int32_t seq, struct secasvar *sav)
{
	const struct secreplay *replay;
	u_int32_t diff;
	int fr;
	u_int32_t wsizeb;	/* Constant: bits of window size. */
	int frlast;		/* Constant: last frame. */

	IPSEC_ASSERT(sav != NULL, ("Null SA"));
	IPSEC_ASSERT(sav->replay != NULL, ("Null replay state"));

	replay = sav->replay;

	if (replay->wsize == 0)
		return (1);	/* No need to check replay. */

	/* Constant. */
	frlast = replay->wsize - 1;
	wsizeb = replay->wsize << 3;

	/* Sequence number of 0 is invalid. */
	if (seq == 0)
		return (0);

	/* First time is always okay. */
	if (replay->count == 0)
		return (1);

	if (seq > replay->lastseq) {
		/* Larger sequences are okay. */
		return (1);
	} else {
		/* seq is equal or less than lastseq. */
		diff = replay->lastseq - seq;

		/* Over range to check, i.e. too old or wrapped. */
		if (diff >= wsizeb)
			return (0);

		fr = frlast - diff / 8;

		/* This packet already seen? */
		if ((replay->bitmap)[fr] & (1 << (diff % 8)))
			return (0);

		/* Out of order but good. */
		return (1);
	}
}

/*
 * Check replay counter whether to update or not.
 * OUT:	0:	OK
 *	1:	NG
 */
int
ipsec_updatereplay(u_int32_t seq, struct secasvar *sav)
{
	char buf[128];
	struct secreplay *replay;
	u_int32_t diff;
	int fr;
	u_int32_t wsizeb;	/* Constant: bits of window size. */
	int frlast;		/* Constant: last frame. */

	IPSEC_ASSERT(sav != NULL, ("Null SA"));
	IPSEC_ASSERT(sav->replay != NULL, ("Null replay state"));

	replay = sav->replay;

	if (replay->wsize == 0)
		goto ok;	/* No need to check replay. */

	/* Constant. */
	frlast = replay->wsize - 1;
	wsizeb = replay->wsize << 3;

	/* Sequence number of 0 is invalid. */
	if (seq == 0)
		return (1);

	/* First time. */
	if (replay->count == 0) {
		replay->lastseq = seq;
		bzero(replay->bitmap, replay->wsize);
		(replay->bitmap)[frlast] = 1;
		goto ok;
	}

	if (seq > replay->lastseq) {
		/* seq is larger than lastseq. */
		diff = seq - replay->lastseq;

		/* New larger sequence number. */
		if (diff < wsizeb) {
			/* In window. */
			/* Set bit for this packet. */
			vshiftl(replay->bitmap, diff, replay->wsize);
			(replay->bitmap)[frlast] |= 1;
		} else {
			/* This packet has a "way larger". */
			bzero(replay->bitmap, replay->wsize);
			(replay->bitmap)[frlast] = 1;
		}
		replay->lastseq = seq;

		/* Larger is good. */
	} else {
		/* seq is equal or less than lastseq. */
		diff = replay->lastseq - seq;

		/* Over range to check, i.e. too old or wrapped. */
		if (diff >= wsizeb)
			return (1);

		fr = frlast - diff / 8;

		/* This packet already seen? */
		if ((replay->bitmap)[fr] & (1 << (diff % 8)))
			return (1);

		/* Mark as seen. */
		(replay->bitmap)[fr] |= (1 << (diff % 8));

		/* Out of order but good. */
	}

ok:
	if (replay->count == ~0) {

		/* Set overflow flag. */
		replay->overflow++;

		/* Don't increment, no more packets accepted. */
		if ((sav->flags & SADB_X_EXT_CYCSEQ) == 0)
			return (1);

		ipseclog((LOG_WARNING, "%s: replay counter made %d cycle. %s\n",
		    __func__, replay->overflow,
		    ipsec_logsastr(sav, buf, sizeof(buf))));
	}

	replay->count++;

	return (0);
}

/*
 * Shift variable length buffer to left.
 * IN:	bitmap: pointer to the buffer
 * 	nbit:	the number of to shift.
 *	wsize:	buffer size (bytes).
 */
static void
vshiftl(unsigned char *bitmap, int nbit, int wsize)
{
	int s, j, i;
	unsigned char over;

	for (j = 0; j < nbit; j += 8) {
		s = (nbit - j < 8) ? (nbit - j): 8;
		bitmap[0] <<= s;
		for (i = 1; i < wsize; i++) {
			over = (bitmap[i] >> (8 - s));
			bitmap[i] <<= s;
			bitmap[i-1] |= over;
		}
	}
}

/* Return a printable string for the address. */
char*
ipsec_address(union sockaddr_union* sa, char *buf, socklen_t size)
{

	switch (sa->sa.sa_family) {
#ifdef INET
	case AF_INET:
		return (inet_ntop(AF_INET, &sa->sin.sin_addr, buf, size));
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		return (inet_ntop(AF_INET6, &sa->sin6.sin6_addr, buf, size));
#endif /* INET6 */
	default:
		return ("(unknown address family)");
	}
}

char *
ipsec_logsastr(struct secasvar *sav, char *buf, size_t size)
{
	char sbuf[INET6_ADDRSTRLEN], dbuf[INET6_ADDRSTRLEN];

	IPSEC_ASSERT(sav->sah->saidx.src.sa.sa_family ==
	    sav->sah->saidx.dst.sa.sa_family, ("address family mismatch"));

	snprintf(buf, size, "SA(SPI=%08lx src=%s dst=%s)",
	    (u_long)ntohl(sav->spi),
	    ipsec_address(&sav->sah->saidx.src, sbuf, sizeof(sbuf)),
	    ipsec_address(&sav->sah->saidx.dst, dbuf, sizeof(dbuf)));
	return (buf);
}

void
ipsec_dumpmbuf(struct mbuf *m)
{
	int totlen;
	int i;
	u_char *p;

	totlen = 0;
	printf("---\n");
	while (m) {
		p = mtod(m, u_char *);
		for (i = 0; i < m->m_len; i++) {
			printf("%02x ", p[i]);
			totlen++;
			if (totlen % 16 == 0)
				printf("\n");
		}
		m = m->m_next;
	}
	if (totlen % 16 != 0)
		printf("\n");
	printf("---\n");
}

static void
def_policy_init(const void *unused __unused)
{

	bzero(&V_def_policy, sizeof(struct secpolicy));
	V_def_policy.policy = IPSEC_POLICY_NONE;
	V_def_policy.refcnt = 1;
}
VNET_SYSINIT(def_policy_init, SI_SUB_PROTO_DOMAININIT, SI_ORDER_ANY,
    def_policy_init, NULL);


/* XXX This stuff doesn't belong here... */

static	struct xformsw* xforms = NULL;

/*
 * Register a transform; typically at system startup.
 */
void
xform_register(struct xformsw* xsp)
{

	xsp->xf_next = xforms;
	xforms = xsp;
}

/*
 * Initialize transform support in an sav.
 */
int
xform_init(struct secasvar *sav, int xftype)
{
	struct xformsw *xsp;

	if (sav->tdb_xform != NULL)	/* Previously initialized. */
		return (0);
	for (xsp = xforms; xsp; xsp = xsp->xf_next)
		if (xsp->xf_type == xftype)
			return ((*xsp->xf_init)(sav, xsp));
	return (EINVAL);
}
