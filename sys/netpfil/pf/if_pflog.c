/*-
 * SPDX-License-Identifier: ISC
 *
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr) and
 * Niels Provos (provos@physnet.uni-hamburg.de).
 *
 * This code was written by John Ioannidis for BSD/OS in Athens, Greece,
 * in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis.
 *
 * Additional transforms and features in 1997 and 1998 by Angelos D. Keromytis
 * and Niels Provos.
 *
 * Copyright (C) 1995, 1996, 1997, 1998 by John Ioannidis, Angelos D. Keromytis
 * and Niels Provos.
 * Copyright (c) 2001, Angelos D. Keromytis, Niels Provos.
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 * You may use this code under the GNU public license if you so wish. Please
 * contribute changes back to the authors under this freer than GPL license
 * so that we may further the use of strong encryption without limitations to
 * all.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 *
 *	$OpenBSD: if_pflog.c,v 1.26 2007/10/18 21:58:18 mpf Exp $
 */

#include <sys/cdefs.h>
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_bpf.h"
#include "opt_pf.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_pflog.h>
#include <net/if_private.h>
#include <net/if_types.h>
#include <net/vnet.h>
#include <net/pfvar.h>

#if defined(INET) || defined(INET6)
#include <netinet/in.h>
#endif
#ifdef	INET
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#ifdef INET6
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#endif /* INET6 */

#ifdef INET
#include <machine/in_cksum.h>
#endif /* INET */

#define PFLOGMTU	(32768 + MHLEN + MLEN)

#ifdef PFLOGDEBUG
#define DPRINTF(x)    do { if (pflogdebug) printf x ; } while (0)
#else
#define DPRINTF(x)
#endif

static void	pflogattach(void);
static int	pflog_create(int);
static void	pflog_destroy(int);
static int	sysctl_pflog_if_count(SYSCTL_HANDLER_ARGS);
static bool	bpf_pflog_chkdir(void *, const struct mbuf *, int);

static const char pflogname[] = "pflog";

static const struct bif_methods bpf_pflog_methods = {
	.bif_chkdir = bpf_pflog_chkdir,
};

struct pflog_dev {
	struct bpf_if	*pflog_bpf;
	char		 pflog_name[IFNAMSIZ];
};
VNET_DEFINE_STATIC(uint8_t, npflogifs) = 8;
#define	V_npflogifs		VNET(npflogifs)
VNET_DEFINE(struct pflog_dev, pflogifs[256]);	/* for fast access */
#define	V_pflogifs		VNET(pflogifs)

SYSCTL_NODE(_net, OID_AUTO, pflog, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "PFLOG");
SYSCTL_PROC(_net_pflog, OID_AUTO, if_count,
    CTLTYPE_U8 | CTLFLAG_RW | CTLFLAG_VNET,
    0, 0, sysctl_pflog_if_count, "CU",
    "Number of pflog(4) interfaces");
static struct sx pflog_dev_lock;
SX_SYSINIT(pflog_dev_lock, &pflog_dev_lock, "pflog(4) device lock");

static void
pflogattach(void)
{
	int ret __diagused;

	sx_xlock(&pflog_dev_lock);
	for (int i = 0; i < V_npflogifs; i++) {
		ret = pflog_create(i);
		MPASS(ret == 0);
	}
	sx_xunlock(&pflog_dev_lock);
}

static int
pflog_create(int unit)
{
	sx_assert(&pflog_dev_lock, SX_XLOCKED);

	if (unit < 0)
		return (EINVAL);
	if (unit > PFLOG_MAX_DEVS)
		return (EINVAL);

	if (V_pflogifs[unit].pflog_bpf != NULL)
		return (EEXIST);

	snprintf(V_pflogifs[unit].pflog_name, IFNAMSIZ, "pflog%d", unit);

	V_pflogifs[unit].pflog_bpf = bpf_attach(V_pflogifs[unit].pflog_name,
	    DLT_PFLOG, PFLOG_HDRLEN, &bpf_pflog_methods, NULL);

	return (0);
}

static void
pflog_destroy(int unit)
{
	sx_assert(&pflog_dev_lock, SX_XLOCKED);

	bpf_detach(V_pflogifs[unit].pflog_bpf);
	V_pflogifs[unit].pflog_bpf = NULL;
}

static int
sysctl_pflog_if_count(SYSCTL_HANDLER_ARGS)
{
	uint8_t n = V_npflogifs;
	int error;

	error = sysctl_handle_8(oidp, &n, 0, req);

	if (n != V_npflogifs) {
		sx_xlock(&pflog_dev_lock);
		if (n > V_npflogifs) {
			for (int i = V_npflogifs; i < n; i++) {
				pflog_create(i);
			}
		} else {
			for (int i = V_npflogifs - 1; i >= n; i--) {
				pflog_destroy(i);
			}
		}
		V_npflogifs = n;
		sx_xunlock(&pflog_dev_lock);
	}

	return (error);
}

static bool
bpf_pflog_chkdir(void *arg __unused, const struct mbuf *m, int dir)
{
	return ((dir == BPF_D_IN && m_rcvif(m) == NULL) ||
	    (dir == BPF_D_OUT && m_rcvif(m) != NULL));
}

static int
pflog_packet(uint8_t action, u_int8_t reason,
    struct pf_krule *rm, struct pf_krule *am,
    struct pf_kruleset *ruleset, struct pf_pdesc *pd, int lookupsafe,
    struct pf_krule *trigger)
{
	struct bpf_if *ifn;
	struct pfloghdr hdr;

	NET_EPOCH_ASSERT();

	if (rm == NULL || pd == NULL)
		return (1);
	if (trigger == NULL)
		trigger = rm;

	if (trigger->logif > V_npflogifs)
		return (0);

	ifn = V_pflogifs[trigger->logif].pflog_bpf;
	if (ifn == NULL)
		return (0);

	bzero(&hdr, sizeof(hdr));
	hdr.length = PFLOG_REAL_HDRLEN;
	hdr.af = pd->af;
	hdr.action = action;
	hdr.reason = reason;
	memcpy(hdr.ifname, pd->kif->pfik_name, sizeof(hdr.ifname));

	if (am == NULL) {
		hdr.rulenr = htonl(rm->nr);
		hdr.subrulenr = -1;
	} else {
		hdr.rulenr = htonl(am->nr);
		hdr.subrulenr = htonl(rm->nr);
		if (ruleset != NULL && ruleset->anchor != NULL)
			strlcpy(hdr.ruleset, ruleset->anchor->name,
			    sizeof(hdr.ruleset));
	}
	hdr.ridentifier = htonl(rm->ridentifier);
	/*
	 * XXXGL: we avoid pf_socket_lookup() when we are holding
	 * state lock, since this leads to unsafe LOR.
	 * These conditions are very very rare, however.
	 */
	if (trigger->log & PF_LOG_USER && !pd->lookup.done && lookupsafe)
		pd->lookup.done = pf_socket_lookup(pd);
	if (trigger->log & PF_LOG_USER && pd->lookup.done > 0)
		hdr.uid = pd->lookup.uid;
	else
		hdr.uid = -1;
	hdr.pid = NO_PID;
	hdr.rule_uid = rm->cuid;
	hdr.rule_pid = rm->cpid;
	hdr.dir = pd->dir;

#ifdef INET
	if (pd->af == AF_INET && pd->dir == PF_OUT) {
		struct ip *ip;

		ip = mtod(pd->m, struct ip *);
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(pd->m, ip->ip_hl << 2);
	}
#endif /* INET */

	bpf_mtap2(ifn, &hdr, PFLOG_HDRLEN, pd->m);

	return (0);
}

static void
vnet_pflog_init(const void *unused __unused)
{

	pflogattach();
}
VNET_SYSINIT(vnet_pflog_init, SI_SUB_PROTO_FIREWALL, SI_ORDER_ANY,
    vnet_pflog_init, NULL);

static void
vnet_pflog_uninit(const void *unused __unused)
{
	sx_xlock(&pflog_dev_lock);
	for (int i = 0; i < V_npflogifs; i++)
		pflog_destroy(i);
	sx_xunlock(&pflog_dev_lock);
}

/*
 * Detach after pf is gone; otherwise we might touch pflog memory
 * from within pf after freeing pflog.
 */
VNET_SYSUNINIT(vnet_pflog_uninit, SI_SUB_PROTO_FIREWALL, SI_ORDER_SECOND,
    vnet_pflog_uninit, NULL);

static int
pflog_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		PF_RULES_WLOCK();
		pflog_packet_ptr = pflog_packet;
		PF_RULES_WUNLOCK();
		break;
	case MOD_UNLOAD:
		PF_RULES_WLOCK();
		pflog_packet_ptr = NULL;
		PF_RULES_WUNLOCK();
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return error;
}

static moduledata_t pflog_mod = { pflogname, pflog_modevent, 0 };

#define PFLOG_MODVER 1

/* Do not run before pf is initialized as we depend on its locks. */
DECLARE_MODULE(pflog, pflog_mod, SI_SUB_PROTO_FIREWALL, SI_ORDER_ANY);
MODULE_VERSION(pflog, PFLOG_MODVER);
MODULE_DEPEND(pflog, pf, PF_MODVER, PF_MODVER, PF_MODVER);
