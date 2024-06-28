/*-
 * Copyright (c) 2016 Yandex LLC
 * Copyright (c) 2016 Andrey V. Elsukov <ae@FreeBSD.org>
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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_pflog.h>
#include <net/if_var.h>
#include <net/if_clone.h>
#include <net/if_types.h>
#include <net/vnet.h>
#include <net/bpf.h>

#include <netinet/in.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_var.h>
#include <netpfil/ipfw/ip_fw_private.h>

VNET_DEFINE_STATIC(struct ifnet *, log_if);
VNET_DEFINE_STATIC(struct ifnet *, pflog_if);
VNET_DEFINE_STATIC(struct if_clone *, ipfw_cloner);
VNET_DEFINE_STATIC(struct if_clone *, ipfwlog_cloner);
#define	V_ipfw_cloner		VNET(ipfw_cloner)
#define	V_ipfwlog_cloner	VNET(ipfwlog_cloner)
#define	V_log_if		VNET(log_if)
#define	V_pflog_if		VNET(pflog_if)

static const char ipfwname[] = "ipfw";
static const char ipfwlogname[] = "ipfwlog";

static int
ipfw_bpf_ioctl(struct ifnet *ifp, u_long cmd, caddr_t addr)
{

	return (EINVAL);
}

static int
ipfw_bpf_output(struct ifnet *ifp, struct mbuf *m,
	const struct sockaddr *dst, struct route *ro)
{

	if (m != NULL)
		FREE_PKT(m);
	return (0);
}

static void
ipfw_clone_destroy(struct ifnet *ifp)
{

	if (ifp->if_hdrlen == ETHER_HDR_LEN)
		V_log_if = NULL;
	else
		V_pflog_if = NULL;

	NET_EPOCH_WAIT();
	bpfdetach(ifp);
	if_detach(ifp);
	if_free(ifp);
}

static int
ipfw_clone_create(struct if_clone *ifc, int unit, caddr_t params)
{
	struct ifnet *ifp;

	ifp = if_alloc(IFT_PFLOG);
	if_initname(ifp, ipfwname, unit);
	ifp->if_flags = IFF_UP | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_mtu = 65536;
	ifp->if_ioctl = ipfw_bpf_ioctl;
	ifp->if_output = ipfw_bpf_output;
	ifp->if_hdrlen = ETHER_HDR_LEN;
	if_attach(ifp);
	bpfattach(ifp, DLT_EN10MB, ETHER_HDR_LEN);
	if (V_log_if != NULL) {
		bpfdetach(ifp);
		if_detach(ifp);
		if_free(ifp);
		return (EEXIST);
	}
	V_log_if = ifp;
	return (0);
}

static int
ipfwlog_clone_create(struct if_clone *ifc, int unit, caddr_t params)
{
	struct ifnet *ifp;

	ifp = if_alloc(IFT_PFLOG);
	if_initname(ifp, ipfwlogname, unit);
	ifp->if_flags = IFF_UP | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_mtu = 65536;
	ifp->if_ioctl = ipfw_bpf_ioctl;
	ifp->if_output = ipfw_bpf_output;
	ifp->if_hdrlen = PFLOG_HDRLEN;
	if_attach(ifp);
	bpfattach(ifp, DLT_PFLOG, PFLOG_HDRLEN);
	if (V_pflog_if != NULL) {
		bpfdetach(ifp);
		if_detach(ifp);
		if_free(ifp);
		return (EEXIST);
	}
	V_pflog_if = ifp;
	return (0);
}

void
ipfw_bpf_tap(u_char *pkt, u_int pktlen)
{
	struct ifnet *ifp = V_log_if;

	NET_EPOCH_ASSERT();
	if (ifp != NULL)
		BPF_TAP(ifp, pkt, pktlen);
}

void
ipfw_bpf_mtap(struct mbuf *m)
{
	struct ifnet *ifp = V_log_if;

	NET_EPOCH_ASSERT();
	if (ifp != NULL)
		BPF_MTAP(ifp, m);
}

void
ipfw_bpf_mtap2(void *data, u_int dlen, struct mbuf *m)
{
	struct ifnet *logif;

	NET_EPOCH_ASSERT();
	switch (dlen) {
	case (ETHER_HDR_LEN):
		logif = V_log_if;
		break;
	case (PFLOG_HDRLEN):
		logif = V_pflog_if;
		break;
	default:
#ifdef INVARIANTS
		panic("%s: unsupported len %d", __func__, dlen);
#endif
		logif = NULL;
	}

	if (logif != NULL)
		BPF_MTAP2(logif, data, dlen, m);
}

void
ipfw_bpf_init(int first __unused)
{

	V_log_if = NULL;
	V_pflog_if = NULL;
	V_ipfw_cloner = if_clone_simple(ipfwname, ipfw_clone_create,
	    ipfw_clone_destroy, 0);
	V_ipfwlog_cloner = if_clone_simple(ipfwlogname, ipfwlog_clone_create,
	    ipfw_clone_destroy, 0);
}

void
ipfw_bpf_uninit(int last __unused)
{

	if_clone_detach(V_ipfw_cloner);
	if_clone_detach(V_ipfwlog_cloner);
}
