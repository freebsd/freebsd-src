/*-
 * Copyright (c) 2006 The FreeBSD Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_clone.h>
#include <net/if_types.h>
#include <net/pfil.h>
#include <net/route.h>
#include <net/netisr.h>
#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>
#include "opt_inet6.h"

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif

#include <netipsec/ipsec.h>

#define ENCMTU		(1024+512)

/* XXX this define must have the same value as in OpenBSD */
#define M_CONF		0x0400	/* payload was encrypted (ESP-transport) */
#define M_AUTH		0x0800	/* payload was authenticated (AH or ESP auth) */
#define M_AUTH_AH	0x2000	/* header was authenticated (AH) */

struct enchdr {
	u_int32_t af;
	u_int32_t spi;
	u_int32_t flags;
};

static struct ifnet	*encif;
static struct mtx	enc_mtx;

struct enc_softc {
	struct	ifnet *sc_ifp;
};

static int	enc_ioctl(struct ifnet *, u_long, caddr_t);
static int	enc_output(struct ifnet *ifp, struct mbuf *m,
		    struct sockaddr *dst, struct rtentry *rt);
static int	enc_clone_create(struct if_clone *, int);
static void	enc_clone_destroy(struct ifnet *);

IFC_SIMPLE_DECLARE(enc, 1);

static void
enc_clone_destroy(struct ifnet *ifp)
{
	KASSERT(ifp != encif, ("%s: destroying encif", __func__));

	bpfdetach(ifp);
	if_detach(ifp);
	if_free(ifp);
}

static int
enc_clone_create(struct if_clone *ifc, int unit)
{
	struct ifnet *ifp;
	struct enc_softc *sc;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_ZERO);
	ifp = sc->sc_ifp = if_alloc(IFT_ENC);
	if (ifp == NULL) {
		free(sc, M_DEVBUF);
		return (ENOSPC);
	}

	if_initname(ifp, ifc->ifc_name, unit);
	ifp->if_mtu = ENCMTU;
	ifp->if_ioctl = enc_ioctl;
	ifp->if_output = enc_output;
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
	ifp->if_softc = sc;
	if_attach(ifp);
	bpfattach(ifp, DLT_ENC, sizeof(struct enchdr));

	mtx_lock(&enc_mtx);
	/* grab a pointer to enc0, ignore the rest */
	if (encif == NULL)
		encif = ifp;
	mtx_unlock(&enc_mtx);

	return (0);
}

static int
enc_modevent(module_t mod, int type, void *data)
{
	switch (type) {
	case MOD_LOAD:
		mtx_init(&enc_mtx, "enc mtx", NULL, MTX_DEF);
		if_clone_attach(&enc_cloner);
		break;
	case MOD_UNLOAD:
		printf("enc module unload - not possible for this module\n");
		return (EINVAL);
	default:
		return (EOPNOTSUPP);
	}
	return (0);
}

static moduledata_t enc_mod = {
	"enc",
	enc_modevent,
	0
};

DECLARE_MODULE(enc, enc_mod, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY);

static int
enc_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
    struct rtentry *rt)
{
	m_freem(m);
	return (0);
}

/*
 * Process an ioctl request.
 */
/* ARGSUSED */
static int
enc_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	int error = 0;

	mtx_lock(&enc_mtx);

	switch (cmd) {

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP)
			ifp->if_drv_flags |= IFF_DRV_RUNNING;
		else
			ifp->if_drv_flags &= ~IFF_DRV_RUNNING;

		break;

	default:
		error = EINVAL;
	}

	mtx_unlock(&enc_mtx);
	return (error);
}

int
ipsec_filter(struct mbuf **mp, int dir)
{
	int error, i;
	struct ip *ip;

	KASSERT(encif != NULL, ("%s: encif is null", __func__));

	if ((encif->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return (0);

	/* Skip pfil(9) if no filters are loaded */
	if (inet_pfil_hook.ph_busy_count < 0
#ifdef INET6
	    && inet6_pfil_hook.ph_busy_count < 0
#endif
	    ) {
		return (0);
	}

	i = min((*mp)->m_pkthdr.len, max_protohdr);
	if ((*mp)->m_len < i) {
		*mp = m_pullup(*mp, i);
		if (*mp == NULL) {
			printf("%s: m_pullup failed\n", __func__);
			return (-1);
		}
	}

	error = 0;
	ip = mtod(*mp, struct ip *);
	switch (ip->ip_v) {
		case 4:
			/*
			 * before calling the firewall, swap fields the same as
			 * IP does. here we assume the header is contiguous
			 */
			ip->ip_len = ntohs(ip->ip_len);
			ip->ip_off = ntohs(ip->ip_off);

			error = pfil_run_hooks(&inet_pfil_hook, mp,
			    encif, dir, NULL);

			if (*mp == NULL || error != 0)
				break;

			/* restore byte ordering */
			ip = mtod(*mp, struct ip *);
			ip->ip_len = htons(ip->ip_len);
			ip->ip_off = htons(ip->ip_off);
			break;

#ifdef INET6
		case 6:
			error = pfil_run_hooks(&inet6_pfil_hook, mp,
			    encif, dir, NULL);
			break;
#endif
		default:
			printf("%s: unknown IP version\n", __func__);
	}

	if (*mp == NULL)
		return (error);
	if (error != 0)
		goto bad;

	return (error);

bad:
	m_freem(*mp);
	*mp = NULL;
	return (error);
}

void
ipsec_bpf(struct mbuf *m, struct secasvar *sav, int af)
{
	int flags;
	struct enchdr hdr;

	KASSERT(encif != NULL, ("%s: encif is null", __func__));
	KASSERT(sav != NULL, ("%s: sav is null", __func__));

	if ((encif->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	if (encif->if_bpf) {
		flags = 0;
		if (sav->alg_enc != SADB_EALG_NONE)
			flags |= M_CONF;
		if (sav->alg_auth != SADB_AALG_NONE)
			flags |= M_AUTH;

		/*
		 * We need to prepend the address family as a four byte
		 * field.  Cons up a dummy header to pacify bpf.  This
		 * is safe because bpf will only read from the mbuf
		 * (i.e., it won't try to free it or keep a pointer a
		 * to it).
		 */
		hdr.af = af;
		hdr.spi = sav->spi;
		hdr.flags = flags;

		bpf_mtap2(encif->if_bpf, &hdr, sizeof(hdr), m);
	}
}
