/*	$OpenBSD: if_pflow.c,v 1.100 2023/11/09 08:53:20 mvs Exp $	*/

/*
 * Copyright (c) 2023 Rubicon Communications, LLC (Netgate)
 * Copyright (c) 2011 Florian Obser <florian@narrans.de>
 * Copyright (c) 2011 Sebastian Benoit <benoit-lists@fb12.de>
 * Copyright (c) 2008 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2008 Joerg Goltermann <jg@osn.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/endian.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/priv.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/bpf.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/tcp.h>

#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/in_pcb.h>

#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_generic.h>
#include <netlink/netlink_message_writer.h>

#include <net/pfvar.h>
#include <net/pflow.h>
#include "net/if_var.h"

#define PFLOW_MINMTU	\
    (sizeof(struct pflow_header) + sizeof(struct pflow_flow))

#ifdef PFLOWDEBUG
#define DPRINTF(x)	do { printf x ; } while (0)
#else
#define DPRINTF(x)
#endif

static void	pflow_output_process(void *);
static int	pflow_create(int);
static int	pflow_destroy(int, bool);
static int	pflow_calc_mtu(struct pflow_softc *, int, int);
static void	pflow_setmtu(struct pflow_softc *, int);
static int	pflowvalidsockaddr(const struct sockaddr *, int);

static struct mbuf	*pflow_get_mbuf(struct pflow_softc *, u_int16_t);
static void	pflow_flush(struct pflow_softc *);
static int	pflow_sendout_v5(struct pflow_softc *);
static int	pflow_sendout_ipfix(struct pflow_softc *, sa_family_t);
static int	pflow_sendout_ipfix_tmpl(struct pflow_softc *);
static int	pflow_sendout_mbuf(struct pflow_softc *, struct mbuf *);
static void	pflow_timeout(void *);
static void	pflow_timeout6(void *);
static void	pflow_timeout_tmpl(void *);
static void	copy_flow_data(struct pflow_flow *, struct pflow_flow *,
	const struct pf_kstate *, struct pf_state_key *, int, int);
static void	copy_flow_ipfix_4_data(struct pflow_ipfix_flow4 *,
	struct pflow_ipfix_flow4 *, const struct pf_kstate *, struct pf_state_key *,
	struct pflow_softc *, int, int);
static void	copy_flow_ipfix_6_data(struct pflow_ipfix_flow6 *,
	struct pflow_ipfix_flow6 *, const struct pf_kstate *, struct pf_state_key *,
	struct pflow_softc *, int, int);
static int	pflow_pack_flow(const struct pf_kstate *, struct pf_state_key *,
	struct pflow_softc *);
static int	pflow_pack_flow_ipfix(const struct pf_kstate *, struct pf_state_key *,
	struct pflow_softc *);
static void	export_pflow(const struct pf_kstate *);
static int	export_pflow_if(const struct pf_kstate*, struct pf_state_key *,
	struct pflow_softc *);
static int	copy_flow_to_m(struct pflow_flow *flow, struct pflow_softc *sc);
static int	copy_flow_ipfix_4_to_m(struct pflow_ipfix_flow4 *flow,
	struct pflow_softc *sc);
static int	copy_flow_ipfix_6_to_m(struct pflow_ipfix_flow6 *flow,
	struct pflow_softc *sc);

static const char pflowname[] = "pflow";

/**
 * Locking concept
 *
 * The list of pflow devices (V_pflowif_list) is managed through epoch.
 * It is safe to read the list without locking (while in NET_EPOCH).
 * There may only be one simultaneous modifier, hence we need V_pflow_list_mtx
 * on every add/delete.
 *
 * Each pflow interface protects its own data with the sc_lock mutex.
 *
 * We do not require any pf locks, and in fact expect to be called without
 * hashrow locks held.
 **/

VNET_DEFINE(struct unrhdr *,	pflow_unr);
#define	V_pflow_unr	VNET(pflow_unr)
VNET_DEFINE(CK_LIST_HEAD(, pflow_softc), pflowif_list);
#define	V_pflowif_list	VNET(pflowif_list)
VNET_DEFINE(struct mtx, pflowif_list_mtx);
#define	V_pflowif_list_mtx	VNET(pflowif_list_mtx)
VNET_DEFINE(struct pflowstats,	 pflowstat);
#define	V_pflowstats	VNET(pflowstat)

#define	PFLOW_LOCK(_sc)		mtx_lock(&(_sc)->sc_lock)
#define	PFLOW_UNLOCK(_sc)	mtx_unlock(&(_sc)->sc_lock)
#define	PFLOW_ASSERT(_sc)	mtx_assert(&(_sc)->sc_lock, MA_OWNED)

SYSCTL_NODE(_net, OID_AUTO, pflow, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "PFLOW");
SYSCTL_STRUCT(_net_pflow, OID_AUTO, stats, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(pflowstat), pflowstats,
    "PFLOW statistics (struct pflowstats, net/if_pflow.h)");

static void
vnet_pflowattach(void)
{
	CK_LIST_INIT(&V_pflowif_list);
	mtx_init(&V_pflowif_list_mtx, "pflow interface list mtx", NULL, MTX_DEF);

	V_pflow_unr = new_unrhdr(0, INT_MAX, &V_pflowif_list_mtx);
}
VNET_SYSINIT(vnet_pflowattach, SI_SUB_PROTO_FIREWALL, SI_ORDER_ANY,
    vnet_pflowattach, NULL);

static void
vnet_pflowdetach(void)
{
	struct pflow_softc	*sc;

	CK_LIST_FOREACH(sc, &V_pflowif_list, sc_next) {
		pflow_destroy(sc->sc_id, false);
	}

	MPASS(CK_LIST_EMPTY(&V_pflowif_list));
	delete_unrhdr(V_pflow_unr);
	mtx_destroy(&V_pflowif_list_mtx);
}
VNET_SYSUNINIT(vnet_pflowdetach, SI_SUB_PROTO_FIREWALL, SI_ORDER_FOURTH,
    vnet_pflowdetach, NULL);

static void
vnet_pflow_finalise(void)
{
	/*
	 * Ensure we've freed all interfaces, and do not have pending
	 * epoch cleanup calls.
	 */
	NET_EPOCH_DRAIN_CALLBACKS();
}
VNET_SYSUNINIT(vnet_pflow_finalise, SI_SUB_PROTO_FIREWALL, SI_ORDER_THIRD,
    vnet_pflow_finalise, NULL);

static void
pflow_output_process(void *arg)
{
	struct mbufq ml;
	struct pflow_softc *sc = arg;
	struct mbuf *m;

	mbufq_init(&ml, 0);

	PFLOW_LOCK(sc);
	mbufq_concat(&ml, &sc->sc_outputqueue);
	PFLOW_UNLOCK(sc);

	CURVNET_SET(sc->sc_vnet);
	while ((m = mbufq_dequeue(&ml)) != NULL) {
		pflow_sendout_mbuf(sc, m);
	}
	CURVNET_RESTORE();
}

static int
pflow_create(int unit)
{
	struct pflow_softc	*pflowif;
	int			 error;

	pflowif = malloc(sizeof(*pflowif), M_DEVBUF, M_WAITOK|M_ZERO);
	mtx_init(&pflowif->sc_lock, "pflowlk", NULL, MTX_DEF);
	pflowif->sc_version = PFLOW_PROTO_DEFAULT;

	/* ipfix template init */
	bzero(&pflowif->sc_tmpl_ipfix,sizeof(pflowif->sc_tmpl_ipfix));
	pflowif->sc_tmpl_ipfix.set_header.set_id =
	    htons(PFLOW_IPFIX_TMPL_SET_ID);
	pflowif->sc_tmpl_ipfix.set_header.set_length =
	    htons(sizeof(struct pflow_ipfix_tmpl));

	/* ipfix IPv4 template */
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.h.tmpl_id =
	    htons(PFLOW_IPFIX_TMPL_IPV4_ID);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.h.field_count
	    = htons(PFLOW_IPFIX_TMPL_IPV4_FIELD_COUNT);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.src_ip.field_id =
	    htons(PFIX_IE_sourceIPv4Address);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.src_ip.len = htons(4);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.dest_ip.field_id =
	    htons(PFIX_IE_destinationIPv4Address);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.dest_ip.len = htons(4);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.if_index_in.field_id =
	    htons(PFIX_IE_ingressInterface);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.if_index_in.len = htons(4);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.if_index_out.field_id =
	    htons(PFIX_IE_egressInterface);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.if_index_out.len = htons(4);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.packets.field_id =
	    htons(PFIX_IE_packetDeltaCount);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.packets.len = htons(8);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.octets.field_id =
	    htons(PFIX_IE_octetDeltaCount);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.octets.len = htons(8);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.start.field_id =
	    htons(PFIX_IE_flowStartMilliseconds);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.start.len = htons(8);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.finish.field_id =
	    htons(PFIX_IE_flowEndMilliseconds);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.finish.len = htons(8);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.src_port.field_id =
	    htons(PFIX_IE_sourceTransportPort);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.src_port.len = htons(2);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.dest_port.field_id =
	    htons(PFIX_IE_destinationTransportPort);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.dest_port.len = htons(2);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.tos.field_id =
	    htons(PFIX_IE_ipClassOfService);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.tos.len = htons(1);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.protocol.field_id =
	    htons(PFIX_IE_protocolIdentifier);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.protocol.len = htons(1);

	/* ipfix IPv6 template */
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.h.tmpl_id =
	    htons(PFLOW_IPFIX_TMPL_IPV6_ID);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.h.field_count =
	    htons(PFLOW_IPFIX_TMPL_IPV6_FIELD_COUNT);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.src_ip.field_id =
	    htons(PFIX_IE_sourceIPv6Address);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.src_ip.len = htons(16);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.dest_ip.field_id =
	    htons(PFIX_IE_destinationIPv6Address);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.dest_ip.len = htons(16);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.if_index_in.field_id =
	    htons(PFIX_IE_ingressInterface);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.if_index_in.len = htons(4);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.if_index_out.field_id =
	    htons(PFIX_IE_egressInterface);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.if_index_out.len = htons(4);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.packets.field_id =
	    htons(PFIX_IE_packetDeltaCount);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.packets.len = htons(8);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.octets.field_id =
	    htons(PFIX_IE_octetDeltaCount);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.octets.len = htons(8);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.start.field_id =
	    htons(PFIX_IE_flowStartMilliseconds);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.start.len = htons(8);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.finish.field_id =
	    htons(PFIX_IE_flowEndMilliseconds);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.finish.len = htons(8);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.src_port.field_id =
	    htons(PFIX_IE_sourceTransportPort);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.src_port.len = htons(2);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.dest_port.field_id =
	    htons(PFIX_IE_destinationTransportPort);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.dest_port.len = htons(2);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.tos.field_id =
	    htons(PFIX_IE_ipClassOfService);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.tos.len = htons(1);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.protocol.field_id =
	    htons(PFIX_IE_protocolIdentifier);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.protocol.len = htons(1);

	pflowif->sc_id = unit;
	pflowif->sc_vnet = curvnet;

	mbufq_init(&pflowif->sc_outputqueue, 8192);
	pflow_setmtu(pflowif, ETHERMTU);

	callout_init_mtx(&pflowif->sc_tmo, &pflowif->sc_lock, 0);
	callout_init_mtx(&pflowif->sc_tmo6, &pflowif->sc_lock, 0);
	callout_init_mtx(&pflowif->sc_tmo_tmpl, &pflowif->sc_lock, 0);

	error = swi_add(&pflowif->sc_swi_ie, pflowname, pflow_output_process,
	    pflowif, SWI_NET, INTR_MPSAFE, &pflowif->sc_swi_cookie);
	if (error) {
		free(pflowif, M_DEVBUF);
		return (error);
	}

	/* Insert into list of pflows */
	mtx_lock(&V_pflowif_list_mtx);
	CK_LIST_INSERT_HEAD(&V_pflowif_list, pflowif, sc_next);
	mtx_unlock(&V_pflowif_list_mtx);

	V_pflow_export_state_ptr = export_pflow;

	return (0);
}

static void
pflow_free_cb(struct epoch_context *ctx)
{
	struct pflow_softc *sc;

	sc = __containerof(ctx, struct pflow_softc, sc_epoch_ctx);

	free(sc, M_DEVBUF);
}

static int
pflow_destroy(int unit, bool drain)
{
	struct pflow_softc	*sc;
	int			 error __diagused;

	mtx_lock(&V_pflowif_list_mtx);
	CK_LIST_FOREACH(sc, &V_pflowif_list, sc_next) {
		if (sc->sc_id == unit)
			break;
	}
	if (sc == NULL) {
		mtx_unlock(&V_pflowif_list_mtx);
		return (ENOENT);
	}
	CK_LIST_REMOVE(sc, sc_next);
	if (CK_LIST_EMPTY(&V_pflowif_list))
		V_pflow_export_state_ptr = NULL;
	mtx_unlock(&V_pflowif_list_mtx);

	sc->sc_dying = 1;

	if (drain) {
		/* Let's be sure no one is using this interface any more. */
		NET_EPOCH_DRAIN_CALLBACKS();
	}

	error = swi_remove(sc->sc_swi_cookie);
	MPASS(error == 0);
	error = intr_event_destroy(sc->sc_swi_ie);
	MPASS(error == 0);

	callout_drain(&sc->sc_tmo);
	callout_drain(&sc->sc_tmo6);
	callout_drain(&sc->sc_tmo_tmpl);

	m_freem(sc->sc_mbuf);
	m_freem(sc->sc_mbuf6);

	PFLOW_LOCK(sc);
	mbufq_drain(&sc->sc_outputqueue);
	if (sc->so != NULL) {
		soclose(sc->so);
		sc->so = NULL;
	}
	if (sc->sc_flowdst != NULL)
		free(sc->sc_flowdst, M_DEVBUF);
	if (sc->sc_flowsrc != NULL)
		free(sc->sc_flowsrc, M_DEVBUF);
	PFLOW_UNLOCK(sc);

	mtx_destroy(&sc->sc_lock);

	free_unr(V_pflow_unr, unit);

	NET_EPOCH_CALL(pflow_free_cb, &sc->sc_epoch_ctx);

	return (0);
}

static int
pflowvalidsockaddr(const struct sockaddr *sa, int ignore_port)
{
	const struct sockaddr_in6	*sin6;
	const struct sockaddr_in	*sin;

	if (sa == NULL)
		return (0);
	switch(sa->sa_family) {
	case AF_INET:
		sin = (const struct sockaddr_in *)sa;
		return (sin->sin_addr.s_addr != INADDR_ANY &&
		    (ignore_port || sin->sin_port != 0));
	case AF_INET6:
		sin6 = (const struct sockaddr_in6 *)sa;
		return (!IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr) &&
		    (ignore_port || sin6->sin6_port != 0));
	default:
		return (0);
	}
}

static int
pflow_calc_mtu(struct pflow_softc *sc, int mtu, int hdrsz)
{

	sc->sc_maxcount4 = (mtu - hdrsz -
	    sizeof(struct udpiphdr)) / sizeof(struct pflow_ipfix_flow4);
	sc->sc_maxcount6 = (mtu - hdrsz -
	    sizeof(struct udpiphdr)) / sizeof(struct pflow_ipfix_flow6);
	if (sc->sc_maxcount4 > PFLOW_MAXFLOWS)
		sc->sc_maxcount4 = PFLOW_MAXFLOWS;
	if (sc->sc_maxcount6 > PFLOW_MAXFLOWS)
		sc->sc_maxcount6 = PFLOW_MAXFLOWS;
	return (hdrsz + sizeof(struct udpiphdr) +
	    MIN(sc->sc_maxcount4 * sizeof(struct pflow_ipfix_flow4),
	    sc->sc_maxcount6 * sizeof(struct pflow_ipfix_flow6)));
}

static void
pflow_setmtu(struct pflow_softc *sc, int mtu_req)
{
	int	mtu;

	mtu = mtu_req;

	switch (sc->sc_version) {
	case PFLOW_PROTO_5:
		sc->sc_maxcount = (mtu - sizeof(struct pflow_header) -
		    sizeof(struct udpiphdr)) / sizeof(struct pflow_flow);
		if (sc->sc_maxcount > PFLOW_MAXFLOWS)
		    sc->sc_maxcount = PFLOW_MAXFLOWS;
		break;
	case PFLOW_PROTO_10:
		pflow_calc_mtu(sc, mtu, sizeof(struct pflow_v10_header));
		break;
	default: /* NOTREACHED */
		break;
	}
}

static struct mbuf *
pflow_get_mbuf(struct pflow_softc *sc, u_int16_t set_id)
{
	struct pflow_set_header	 set_hdr;
	struct pflow_header	 h;
	struct mbuf		*m;

	MGETHDR(m, M_NOWAIT, MT_DATA);
	if (m == NULL) {
		V_pflowstats.pflow_onomem++;
		return (NULL);
	}

	MCLGET(m, M_NOWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_free(m);
		V_pflowstats.pflow_onomem++;
		return (NULL);
	}

	m->m_len = m->m_pkthdr.len = 0;

	if (sc == NULL)		/* get only a new empty mbuf */
		return (m);

	switch (sc->sc_version) {
	case PFLOW_PROTO_5:
		/* populate pflow_header */
		h.reserved1 = 0;
		h.reserved2 = 0;
		h.count = 0;
		h.version = htons(PFLOW_PROTO_5);
		h.flow_sequence = htonl(sc->sc_gcounter);
		h.engine_type = PFLOW_ENGINE_TYPE;
		h.engine_id = PFLOW_ENGINE_ID;
		m_copyback(m, 0, PFLOW_HDRLEN, (caddr_t)&h);

		sc->sc_count = 0;
		callout_reset(&sc->sc_tmo, PFLOW_TIMEOUT * hz,
		    pflow_timeout, sc);
		break;
	case PFLOW_PROTO_10:
		/* populate pflow_set_header */
		set_hdr.set_length = 0;
		set_hdr.set_id = htons(set_id);
		m_copyback(m, 0, PFLOW_SET_HDRLEN, (caddr_t)&set_hdr);
		break;
	default: /* NOTREACHED */
		break;
	}

	return (m);
}

static void
copy_flow_data(struct pflow_flow *flow1, struct pflow_flow *flow2,
    const struct pf_kstate *st, struct pf_state_key *sk, int src, int dst)
{
	flow1->src_ip = flow2->dest_ip = sk->addr[src].v4.s_addr;
	flow1->src_port = flow2->dest_port = sk->port[src];
	flow1->dest_ip = flow2->src_ip = sk->addr[dst].v4.s_addr;
	flow1->dest_port = flow2->src_port = sk->port[dst];

	flow1->dest_as = flow2->src_as =
	    flow1->src_as = flow2->dest_as = 0;
	flow1->if_index_in = htons(st->if_index_in);
	flow1->if_index_out = htons(st->if_index_out);
	flow2->if_index_in = htons(st->if_index_out);
	flow2->if_index_out = htons(st->if_index_in);
	flow1->dest_mask = flow2->src_mask =
	    flow1->src_mask = flow2->dest_mask = 0;

	flow1->flow_packets = htonl(st->packets[0]);
	flow2->flow_packets = htonl(st->packets[1]);
	flow1->flow_octets = htonl(st->bytes[0]);
	flow2->flow_octets = htonl(st->bytes[1]);

	/*
	 * Pretend the flow was created or expired when the machine came up
	 * when creation is in the future of the last time a package was seen
	 * or was created / expired before this machine came up due to pfsync.
	 */
	flow1->flow_start = flow2->flow_start = st->creation < 0 ||
	    st->creation > st->expire ? htonl(0) : htonl(st->creation * 1000);
	flow1->flow_finish = flow2->flow_finish = st->expire < 0 ? htonl(0) :
	    htonl(st->expire * 1000);
	flow1->tcp_flags = flow2->tcp_flags = 0;
	flow1->protocol = flow2->protocol = sk->proto;
	flow1->tos = flow2->tos = st->rule.ptr->tos;
}

static void
copy_flow_ipfix_4_data(struct pflow_ipfix_flow4 *flow1,
    struct pflow_ipfix_flow4 *flow2, const struct pf_kstate *st,
    struct pf_state_key *sk, struct pflow_softc *sc, int src, int dst)
{
	flow1->src_ip = flow2->dest_ip = sk->addr[src].v4.s_addr;
	flow1->src_port = flow2->dest_port = sk->port[src];
	flow1->dest_ip = flow2->src_ip = sk->addr[dst].v4.s_addr;
	flow1->dest_port = flow2->src_port = sk->port[dst];

	flow1->if_index_in = htonl(st->if_index_in);
	flow1->if_index_out = htonl(st->if_index_out);
	flow2->if_index_in = htonl(st->if_index_out);
	flow2->if_index_out = htonl(st->if_index_in);

	flow1->flow_packets = htobe64(st->packets[0]);
	flow2->flow_packets = htobe64(st->packets[1]);
	flow1->flow_octets = htobe64(st->bytes[0]);
	flow2->flow_octets = htobe64(st->bytes[1]);

	/*
	 * Pretend the flow was created when the machine came up when creation
	 * is in the future of the last time a package was seen due to pfsync.
	 */
	if (st->creation > st->expire)
		flow1->flow_start = flow2->flow_start = htobe64((time_second -
		    time_uptime)*1000);
	else
		flow1->flow_start = flow2->flow_start = htobe64((time_second -
		    (time_uptime - st->creation))*1000);
	flow1->flow_finish = flow2->flow_finish = htobe64((time_second -
	    (time_uptime - st->expire))*1000);

	flow1->protocol = flow2->protocol = sk->proto;
	flow1->tos = flow2->tos = st->rule.ptr->tos;
}

static void
copy_flow_ipfix_6_data(struct pflow_ipfix_flow6 *flow1,
    struct pflow_ipfix_flow6 *flow2, const struct pf_kstate *st,
    struct pf_state_key *sk, struct pflow_softc *sc, int src, int dst)
{
	bcopy(&sk->addr[src].v6, &flow1->src_ip, sizeof(flow1->src_ip));
	bcopy(&sk->addr[src].v6, &flow2->dest_ip, sizeof(flow2->dest_ip));
	flow1->src_port = flow2->dest_port = sk->port[src];
	bcopy(&sk->addr[dst].v6, &flow1->dest_ip, sizeof(flow1->dest_ip));
	bcopy(&sk->addr[dst].v6, &flow2->src_ip, sizeof(flow2->src_ip));
	flow1->dest_port = flow2->src_port = sk->port[dst];

	flow1->if_index_in = htonl(st->if_index_in);
	flow1->if_index_out = htonl(st->if_index_out);
	flow2->if_index_in = htonl(st->if_index_out);
	flow2->if_index_out = htonl(st->if_index_in);

	flow1->flow_packets = htobe64(st->packets[0]);
	flow2->flow_packets = htobe64(st->packets[1]);
	flow1->flow_octets = htobe64(st->bytes[0]);
	flow2->flow_octets = htobe64(st->bytes[1]);

	/*
	 * Pretend the flow was created when the machine came up when creation
	 * is in the future of the last time a package was seen due to pfsync.
	 */
	if (st->creation > st->expire)
		flow1->flow_start = flow2->flow_start = htobe64((time_second -
		    time_uptime)*1000);
	else
		flow1->flow_start = flow2->flow_start = htobe64((time_second -
		    (time_uptime - st->creation))*1000);
	flow1->flow_finish = flow2->flow_finish = htobe64((time_second -
	    (time_uptime - st->expire))*1000);

	flow1->protocol = flow2->protocol = sk->proto;
	flow1->tos = flow2->tos = st->rule.ptr->tos;
}

static void
export_pflow(const struct pf_kstate *st)
{
	struct pflow_softc	*sc = NULL;
	struct pf_state_key	*sk;

	NET_EPOCH_ASSERT();

	sk = st->key[st->direction == PF_IN ? PF_SK_WIRE : PF_SK_STACK];

	CK_LIST_FOREACH(sc, &V_pflowif_list, sc_next) {
		PFLOW_LOCK(sc);
		switch (sc->sc_version) {
		case PFLOW_PROTO_5:
			if (sk->af == AF_INET)
				export_pflow_if(st, sk, sc);
			break;
		case PFLOW_PROTO_10:
			if (sk->af == AF_INET || sk->af == AF_INET6)
				export_pflow_if(st, sk, sc);
			break;
		default: /* NOTREACHED */
			break;
		}
		PFLOW_UNLOCK(sc);
	}
}

static int
export_pflow_if(const struct pf_kstate *st, struct pf_state_key *sk,
    struct pflow_softc *sc)
{
	struct pf_kstate	 pfs_copy;
	u_int64_t		 bytes[2];
	int			 ret = 0;

	if (sc->sc_version == PFLOW_PROTO_10)
		return (pflow_pack_flow_ipfix(st, sk, sc));

	/* PFLOW_PROTO_5 */
	if ((st->bytes[0] < (u_int64_t)PFLOW_MAXBYTES)
	    && (st->bytes[1] < (u_int64_t)PFLOW_MAXBYTES))
		return (pflow_pack_flow(st, sk, sc));

	/* flow > PFLOW_MAXBYTES need special handling */
	bcopy(st, &pfs_copy, sizeof(pfs_copy));
	bytes[0] = pfs_copy.bytes[0];
	bytes[1] = pfs_copy.bytes[1];

	while (bytes[0] > PFLOW_MAXBYTES) {
		pfs_copy.bytes[0] = PFLOW_MAXBYTES;
		pfs_copy.bytes[1] = 0;

		if ((ret = pflow_pack_flow(&pfs_copy, sk, sc)) != 0)
			return (ret);
		if ((bytes[0] - PFLOW_MAXBYTES) > 0)
			bytes[0] -= PFLOW_MAXBYTES;
	}

	while (bytes[1] > (u_int64_t)PFLOW_MAXBYTES) {
		pfs_copy.bytes[1] = PFLOW_MAXBYTES;
		pfs_copy.bytes[0] = 0;

		if ((ret = pflow_pack_flow(&pfs_copy, sk, sc)) != 0)
			return (ret);
		if ((bytes[1] - PFLOW_MAXBYTES) > 0)
			bytes[1] -= PFLOW_MAXBYTES;
	}

	pfs_copy.bytes[0] = bytes[0];
	pfs_copy.bytes[1] = bytes[1];

	return (pflow_pack_flow(&pfs_copy, sk, sc));
}

static int
copy_flow_to_m(struct pflow_flow *flow, struct pflow_softc *sc)
{
	int		ret = 0;

	PFLOW_ASSERT(sc);

	if (sc->sc_mbuf == NULL) {
		if ((sc->sc_mbuf = pflow_get_mbuf(sc, 0)) == NULL)
			return (ENOBUFS);
	}
	m_copyback(sc->sc_mbuf, PFLOW_HDRLEN +
	    (sc->sc_count * sizeof(struct pflow_flow)),
	    sizeof(struct pflow_flow), (caddr_t)flow);

	if (V_pflowstats.pflow_flows == sc->sc_gcounter)
		V_pflowstats.pflow_flows++;
	sc->sc_gcounter++;
	sc->sc_count++;

	if (sc->sc_count >= sc->sc_maxcount)
		ret = pflow_sendout_v5(sc);

	return(ret);
}

static int
copy_flow_ipfix_4_to_m(struct pflow_ipfix_flow4 *flow, struct pflow_softc *sc)
{
	int		ret = 0;

	PFLOW_ASSERT(sc);

	if (sc->sc_mbuf == NULL) {
		if ((sc->sc_mbuf =
		    pflow_get_mbuf(sc, PFLOW_IPFIX_TMPL_IPV4_ID)) == NULL) {
			return (ENOBUFS);
		}
		sc->sc_count4 = 0;
		callout_reset(&sc->sc_tmo, PFLOW_TIMEOUT * hz,
		    pflow_timeout, sc);
	}
	m_copyback(sc->sc_mbuf, PFLOW_SET_HDRLEN +
	    (sc->sc_count4 * sizeof(struct pflow_ipfix_flow4)),
	    sizeof(struct pflow_ipfix_flow4), (caddr_t)flow);

	if (V_pflowstats.pflow_flows == sc->sc_gcounter)
		V_pflowstats.pflow_flows++;
	sc->sc_gcounter++;
	sc->sc_count4++;

	if (sc->sc_count4 >= sc->sc_maxcount4)
		ret = pflow_sendout_ipfix(sc, AF_INET);
	return(ret);
}

static int
copy_flow_ipfix_6_to_m(struct pflow_ipfix_flow6 *flow, struct pflow_softc *sc)
{
	int		ret = 0;

	PFLOW_ASSERT(sc);

	if (sc->sc_mbuf6 == NULL) {
		if ((sc->sc_mbuf6 =
		    pflow_get_mbuf(sc, PFLOW_IPFIX_TMPL_IPV6_ID)) == NULL) {
			return (ENOBUFS);
		}
		sc->sc_count6 = 0;
		callout_reset(&sc->sc_tmo6, PFLOW_TIMEOUT * hz,
		    pflow_timeout6, sc);
	}
	m_copyback(sc->sc_mbuf6, PFLOW_SET_HDRLEN +
	    (sc->sc_count6 * sizeof(struct pflow_ipfix_flow6)),
	    sizeof(struct pflow_ipfix_flow6), (caddr_t)flow);

	if (V_pflowstats.pflow_flows == sc->sc_gcounter)
		V_pflowstats.pflow_flows++;
	sc->sc_gcounter++;
	sc->sc_count6++;

	if (sc->sc_count6 >= sc->sc_maxcount6)
		ret = pflow_sendout_ipfix(sc, AF_INET6);

	return(ret);
}

static int
pflow_pack_flow(const struct pf_kstate *st, struct pf_state_key *sk,
    struct pflow_softc *sc)
{
	struct pflow_flow	 flow1;
	struct pflow_flow	 flow2;
	int			 ret = 0;

	bzero(&flow1, sizeof(flow1));
	bzero(&flow2, sizeof(flow2));

	if (st->direction == PF_OUT)
		copy_flow_data(&flow1, &flow2, st, sk, 1, 0);
	else
		copy_flow_data(&flow1, &flow2, st, sk, 0, 1);

	if (st->bytes[0] != 0) /* first flow from state */
		ret = copy_flow_to_m(&flow1, sc);

	if (st->bytes[1] != 0) /* second flow from state */
		ret = copy_flow_to_m(&flow2, sc);

	return (ret);
}

static int
pflow_pack_flow_ipfix(const struct pf_kstate *st, struct pf_state_key *sk,
    struct pflow_softc *sc)
{
	struct pflow_ipfix_flow4	 flow4_1, flow4_2;
	struct pflow_ipfix_flow6	 flow6_1, flow6_2;
	int				 ret = 0;
	if (sk->af == AF_INET) {
		bzero(&flow4_1, sizeof(flow4_1));
		bzero(&flow4_2, sizeof(flow4_2));

		if (st->direction == PF_OUT)
			copy_flow_ipfix_4_data(&flow4_1, &flow4_2, st, sk, sc,
			    1, 0);
		else
			copy_flow_ipfix_4_data(&flow4_1, &flow4_2, st, sk, sc,
			    0, 1);

		if (st->bytes[0] != 0) /* first flow from state */
			ret = copy_flow_ipfix_4_to_m(&flow4_1, sc);

		if (st->bytes[1] != 0) /* second flow from state */
			ret = copy_flow_ipfix_4_to_m(&flow4_2, sc);
	} else if (sk->af == AF_INET6) {
		bzero(&flow6_1, sizeof(flow6_1));
		bzero(&flow6_2, sizeof(flow6_2));

		if (st->direction == PF_OUT)
			copy_flow_ipfix_6_data(&flow6_1, &flow6_2, st, sk, sc,
			    1, 0);
		else
			copy_flow_ipfix_6_data(&flow6_1, &flow6_2, st, sk, sc,
			    0, 1);

		if (st->bytes[0] != 0) /* first flow from state */
			ret = copy_flow_ipfix_6_to_m(&flow6_1, sc);

		if (st->bytes[1] != 0) /* second flow from state */
			ret = copy_flow_ipfix_6_to_m(&flow6_2, sc);
	}
	return (ret);
}

static void
pflow_timeout(void *v)
{
	struct pflow_softc	*sc = v;

	PFLOW_ASSERT(sc);
	CURVNET_SET(sc->sc_vnet);

	switch (sc->sc_version) {
	case PFLOW_PROTO_5:
		pflow_sendout_v5(sc);
		break;
	case PFLOW_PROTO_10:
		pflow_sendout_ipfix(sc, AF_INET);
		break;
	default: /* NOTREACHED */
		panic("Unsupported version %d", sc->sc_version);
		break;
	}

	CURVNET_RESTORE();
}

static void
pflow_timeout6(void *v)
{
	struct pflow_softc	*sc = v;

	PFLOW_ASSERT(sc);

	if (sc->sc_version != PFLOW_PROTO_10)
		return;

	CURVNET_SET(sc->sc_vnet);
	pflow_sendout_ipfix(sc, AF_INET6);
	CURVNET_RESTORE();
}

static void
pflow_timeout_tmpl(void *v)
{
	struct pflow_softc	*sc = v;

	PFLOW_ASSERT(sc);

	if (sc->sc_version != PFLOW_PROTO_10)
		return;

	CURVNET_SET(sc->sc_vnet);
	pflow_sendout_ipfix_tmpl(sc);
	CURVNET_RESTORE();
}

static void
pflow_flush(struct pflow_softc *sc)
{
	PFLOW_ASSERT(sc);

	switch (sc->sc_version) {
	case PFLOW_PROTO_5:
		pflow_sendout_v5(sc);
		break;
	case PFLOW_PROTO_10:
		pflow_sendout_ipfix(sc, AF_INET);
		pflow_sendout_ipfix(sc, AF_INET6);
		break;
	default: /* NOTREACHED */
		break;
	}
}

static int
pflow_sendout_v5(struct pflow_softc *sc)
{
	struct mbuf		*m = sc->sc_mbuf;
	struct pflow_header	*h;
	struct timespec		tv;

	PFLOW_ASSERT(sc);

	if (m == NULL)
		return (0);

	sc->sc_mbuf = NULL;

	V_pflowstats.pflow_packets++;
	h = mtod(m, struct pflow_header *);
	h->count = htons(sc->sc_count);

	/* populate pflow_header */
	h->uptime_ms = htonl(time_uptime * 1000);

	getnanotime(&tv);
	h->time_sec = htonl(tv.tv_sec);			/* XXX 2038 */
	h->time_nanosec = htonl(tv.tv_nsec);
	if (mbufq_enqueue(&sc->sc_outputqueue, m) == 0)
		swi_sched(sc->sc_swi_cookie, 0);

	return (0);
}

static int
pflow_sendout_ipfix(struct pflow_softc *sc, sa_family_t af)
{
	struct mbuf			*m;
	struct pflow_v10_header		*h10;
	struct pflow_set_header		*set_hdr;
	u_int32_t			 count;
	int				 set_length;

	PFLOW_ASSERT(sc);

	switch (af) {
	case AF_INET:
		m = sc->sc_mbuf;
		callout_stop(&sc->sc_tmo);
		if (m == NULL)
			return (0);
		sc->sc_mbuf = NULL;
		count = sc->sc_count4;
		set_length = sizeof(struct pflow_set_header)
		    + sc->sc_count4 * sizeof(struct pflow_ipfix_flow4);
		break;
	case AF_INET6:
		m = sc->sc_mbuf6;
		callout_stop(&sc->sc_tmo6);
		if (m == NULL)
			return (0);
		sc->sc_mbuf6 = NULL;
		count = sc->sc_count6;
		set_length = sizeof(struct pflow_set_header)
		    + sc->sc_count6 * sizeof(struct pflow_ipfix_flow6);
		break;
	default:
		panic("Unsupported AF %d", af);
	}

	V_pflowstats.pflow_packets++;
	set_hdr = mtod(m, struct pflow_set_header *);
	set_hdr->set_length = htons(set_length);

	/* populate pflow_header */
	M_PREPEND(m, sizeof(struct pflow_v10_header), M_NOWAIT);
	if (m == NULL) {
		V_pflowstats.pflow_onomem++;
		return (ENOBUFS);
	}
	h10 = mtod(m, struct pflow_v10_header *);
	h10->version = htons(PFLOW_PROTO_10);
	h10->length = htons(PFLOW_IPFIX_HDRLEN + set_length);
	h10->time_sec = htonl(time_second);		/* XXX 2038 */
	h10->flow_sequence = htonl(sc->sc_sequence);
	sc->sc_sequence += count;
	h10->observation_dom = htonl(PFLOW_ENGINE_TYPE);
	if (mbufq_enqueue(&sc->sc_outputqueue, m) == 0)
		swi_sched(sc->sc_swi_cookie, 0);

	return (0);
}

static int
pflow_sendout_ipfix_tmpl(struct pflow_softc *sc)
{
	struct mbuf			*m;
	struct pflow_v10_header		*h10;

	PFLOW_ASSERT(sc);

	m = pflow_get_mbuf(sc, 0);
	if (m == NULL)
		return (0);
	m_copyback(m, 0, sizeof(struct pflow_ipfix_tmpl),
	    (caddr_t)&sc->sc_tmpl_ipfix);

	V_pflowstats.pflow_packets++;

	/* populate pflow_header */
	M_PREPEND(m, sizeof(struct pflow_v10_header), M_NOWAIT);
	if (m == NULL) {
		V_pflowstats.pflow_onomem++;
		return (ENOBUFS);
	}
	h10 = mtod(m, struct pflow_v10_header *);
	h10->version = htons(PFLOW_PROTO_10);
	h10->length = htons(PFLOW_IPFIX_HDRLEN + sizeof(struct
	    pflow_ipfix_tmpl));
	h10->time_sec = htonl(time_second);		/* XXX 2038 */
	h10->flow_sequence = htonl(sc->sc_sequence);
	h10->observation_dom = htonl(PFLOW_ENGINE_TYPE);

	callout_reset(&sc->sc_tmo_tmpl, PFLOW_TMPL_TIMEOUT * hz,
	    pflow_timeout_tmpl, sc);
	if (mbufq_enqueue(&sc->sc_outputqueue, m) == 0)
		swi_sched(sc->sc_swi_cookie, 0);

	return (0);
}

static int
pflow_sendout_mbuf(struct pflow_softc *sc, struct mbuf *m)
{
	if (sc->so == NULL) {
		m_freem(m);
		return (EINVAL);
	}
	return (sosend(sc->so, sc->sc_flowdst, NULL, m, NULL, 0, curthread));
}

static int
pflow_nl_list(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct epoch_tracker	 et;
	struct pflow_softc	*sc = NULL;
	struct nl_writer	 *nw = npt->nw;
	int			 error = 0;

	hdr->nlmsg_flags |= NLM_F_MULTI;

	NET_EPOCH_ENTER(et);
	CK_LIST_FOREACH(sc, &V_pflowif_list, sc_next) {
		if (!nlmsg_reply(nw, hdr, sizeof(struct genlmsghdr))) {
			error = ENOMEM;
			goto out;
		}

		struct genlmsghdr *ghdr_new = nlmsg_reserve_object(nw, struct genlmsghdr);
		ghdr_new->cmd = PFLOWNL_CMD_LIST;
		ghdr_new->version = 0;
		ghdr_new->reserved = 0;

		nlattr_add_u32(nw, PFLOWNL_L_ID, sc->sc_id);

		if (! nlmsg_end(nw)) {
			error = ENOMEM;
			goto out;
		}
	}

out:
	NET_EPOCH_EXIT(et);

	if (error != 0)
		nlmsg_abort(nw);

	return (error);
}

static int
pflow_nl_create(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct nl_writer	 *nw = npt->nw;
	int			 error = 0;
	int			 unit;

	if (! nlmsg_reply(nw, hdr, sizeof(struct genlmsghdr))) {
		return (ENOMEM);
	}

	struct genlmsghdr *ghdr_new = nlmsg_reserve_object(nw, struct genlmsghdr);
	ghdr_new->cmd = PFLOWNL_CMD_CREATE;
	ghdr_new->version = 0;
	ghdr_new->reserved = 0;

	unit = alloc_unr(V_pflow_unr);

	error = pflow_create(unit);
	if (error != 0) {
		free_unr(V_pflow_unr, unit);
		nlmsg_abort(nw);
		return (error);
	}

	nlattr_add_s32(nw, PFLOWNL_CREATE_ID, unit);

	if (! nlmsg_end(nw)) {
		pflow_destroy(unit, true);
		return (ENOMEM);
	}

	return (0);
}

struct pflow_parsed_del {
	int id;
};
#define	_IN(_field)	offsetof(struct genlmsghdr, _field)
#define	_OUT(_field)	offsetof(struct pflow_parsed_del, _field)
static const struct nlattr_parser nla_p_del[] = {
	{ .type = PFLOWNL_DEL_ID, .off = _OUT(id), .cb = nlattr_get_uint32 },
};
static const struct nlfield_parser nlf_p_del[] = {};
#undef _IN
#undef _OUT
NL_DECLARE_PARSER(del_parser, struct genlmsghdr, nlf_p_del, nla_p_del);

static int
pflow_nl_del(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct pflow_parsed_del d = {};
	int error;

	error = nl_parse_nlmsg(hdr, &del_parser, npt, &d);
	if (error != 0)
		return (error);

	error = pflow_destroy(d.id, true);

	return (error);
}

struct pflow_parsed_get {
	int id;
};
#define	_IN(_field)	offsetof(struct genlmsghdr, _field)
#define	_OUT(_field)	offsetof(struct pflow_parsed_get, _field)
static const struct nlattr_parser nla_p_get[] = {
	{ .type = PFLOWNL_GET_ID, .off = _OUT(id), .cb = nlattr_get_uint32 },
};
static const struct nlfield_parser nlf_p_get[] = {};
#undef _IN
#undef _OUT
NL_DECLARE_PARSER(get_parser, struct genlmsghdr, nlf_p_get, nla_p_get);

static bool
nlattr_add_sockaddr(struct nl_writer *nw, int attr, const struct sockaddr *s)
{
	int off = nlattr_add_nested(nw, attr);
	if (off == 0)
		return (false);

	nlattr_add_u8(nw, PFLOWNL_ADDR_FAMILY, s->sa_family);

	switch (s->sa_family) {
	case AF_INET: {
		const struct sockaddr_in *in = (const struct sockaddr_in *)s;
		nlattr_add_u16(nw, PFLOWNL_ADDR_PORT, in->sin_port);
		nlattr_add_in_addr(nw, PFLOWNL_ADDR_IP, &in->sin_addr);
		break;
	}
	case AF_INET6: {
		const struct sockaddr_in6 *in6 = (const struct sockaddr_in6 *)s;
		nlattr_add_u16(nw, PFLOWNL_ADDR_PORT, in6->sin6_port);
		nlattr_add_in6_addr(nw, PFLOWNL_ADDR_IP6, &in6->sin6_addr);
		break;
	}
	default:
		panic("Unknown address family %d", s->sa_family);
	}

	nlattr_set_len(nw, off);
	return (true);
}

static int
pflow_nl_get(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct epoch_tracker et;
	struct pflow_parsed_get g = {};
	struct pflow_softc *sc = NULL;
	struct nl_writer *nw = npt->nw;
	struct genlmsghdr *ghdr_new;
	int error;

	error = nl_parse_nlmsg(hdr, &get_parser, npt, &g);
	if (error != 0)
		return (error);

	NET_EPOCH_ENTER(et);
	CK_LIST_FOREACH(sc, &V_pflowif_list, sc_next) {
		if (sc->sc_id == g.id)
			break;
	}
	if (sc == NULL) {
		error = ENOENT;
		goto out;
	}

	if (! nlmsg_reply(nw, hdr, sizeof(struct genlmsghdr))) {
		nlmsg_abort(nw);
		error = ENOMEM;
		goto out;
	}

	ghdr_new = nlmsg_reserve_object(nw, struct genlmsghdr);
	if (ghdr_new == NULL) {
		nlmsg_abort(nw);
		error = ENOMEM;
		goto out;
	}

	ghdr_new->cmd = PFLOWNL_CMD_GET;
	ghdr_new->version = 0;
	ghdr_new->reserved = 0;

	nlattr_add_u32(nw, PFLOWNL_GET_ID, sc->sc_id);
	nlattr_add_u16(nw, PFLOWNL_GET_VERSION, sc->sc_version);
	if (sc->sc_flowsrc)
		nlattr_add_sockaddr(nw, PFLOWNL_GET_SRC, sc->sc_flowsrc);
	if (sc->sc_flowdst)
		nlattr_add_sockaddr(nw, PFLOWNL_GET_DST, sc->sc_flowdst);

	if (! nlmsg_end(nw)) {
		nlmsg_abort(nw);
		error = ENOMEM;
	}

out:
	NET_EPOCH_EXIT(et);

	return (error);
}

struct pflow_sockaddr {
	union {
		struct sockaddr_in in;
		struct sockaddr_in6 in6;
		struct sockaddr_storage storage;
	};
};
static bool
pflow_postparse_sockaddr(void *parsed_args, struct nl_pstate *npt __unused)
{
	struct pflow_sockaddr *s = (struct pflow_sockaddr *)parsed_args;

	if (s->storage.ss_family == AF_INET)
		s->storage.ss_len = sizeof(struct sockaddr_in);
	else if (s->storage.ss_family == AF_INET6)
		s->storage.ss_len = sizeof(struct sockaddr_in6);
	else
		return (false);

	return (true);
}

#define	_OUT(_field)	offsetof(struct pflow_sockaddr, _field)
static struct nlattr_parser nla_p_sockaddr[] = {
	{ .type = PFLOWNL_ADDR_FAMILY, .off = _OUT(in.sin_family), .cb = nlattr_get_uint8 },
	{ .type = PFLOWNL_ADDR_PORT, .off = _OUT(in.sin_port), .cb = nlattr_get_uint16 },
	{ .type = PFLOWNL_ADDR_IP, .off = _OUT(in.sin_addr), .cb = nlattr_get_in_addr },
	{ .type = PFLOWNL_ADDR_IP6, .off = _OUT(in6.sin6_addr), .cb = nlattr_get_in6_addr },
};
NL_DECLARE_ATTR_PARSER_EXT(addr_parser, nla_p_sockaddr, pflow_postparse_sockaddr);
#undef _OUT

struct pflow_parsed_set {
	int id;
	uint16_t version;
	struct sockaddr_storage src;
	struct sockaddr_storage dst;
};
#define	_IN(_field)	offsetof(struct genlmsghdr, _field)
#define	_OUT(_field)	offsetof(struct pflow_parsed_set, _field)
static const struct nlattr_parser nla_p_set[] = {
	{ .type = PFLOWNL_SET_ID, .off = _OUT(id), .cb = nlattr_get_uint32 },
	{ .type = PFLOWNL_SET_VERSION, .off = _OUT(version), .cb = nlattr_get_uint16 },
	{ .type = PFLOWNL_SET_SRC, .off = _OUT(src), .arg = &addr_parser, .cb = nlattr_get_nested },
	{ .type = PFLOWNL_SET_DST, .off = _OUT(dst), .arg = &addr_parser, .cb = nlattr_get_nested },
};
static const struct nlfield_parser nlf_p_set[] = {};
#undef _IN
#undef _OUT
NL_DECLARE_PARSER(set_parser, struct genlmsghdr, nlf_p_set, nla_p_set);

static int
pflow_set(struct pflow_softc *sc, const struct pflow_parsed_set *pflowr, struct ucred *cred)
{
	struct thread		*td;
	struct socket		*so;
	int			 error = 0;

	td = curthread;

	PFLOW_ASSERT(sc);

	if (pflowr->version != 0) {
		switch(pflowr->version) {
		case PFLOW_PROTO_5:
		case PFLOW_PROTO_10:
			break;
		default:
			return(EINVAL);
		}
	}

	pflow_flush(sc);

	if (pflowr->dst.ss_len != 0) {
		if (sc->sc_flowdst != NULL &&
		    sc->sc_flowdst->sa_family != pflowr->dst.ss_family) {
			free(sc->sc_flowdst, M_DEVBUF);
			sc->sc_flowdst = NULL;
			if (sc->so != NULL) {
				soclose(sc->so);
				sc->so = NULL;
			}
		}

		switch (pflowr->dst.ss_family) {
		case AF_INET:
			if (sc->sc_flowdst == NULL) {
				if ((sc->sc_flowdst = malloc(
				    sizeof(struct sockaddr_in),
				    M_DEVBUF,  M_NOWAIT)) == NULL)
					return (ENOMEM);
			}
			memcpy(sc->sc_flowdst, &pflowr->dst,
			    sizeof(struct sockaddr_in));
			sc->sc_flowdst->sa_len = sizeof(struct
			    sockaddr_in);
			break;
		case AF_INET6:
			if (sc->sc_flowdst == NULL) {
				if ((sc->sc_flowdst = malloc(
				    sizeof(struct sockaddr_in6),
				    M_DEVBUF, M_NOWAIT)) == NULL)
					return (ENOMEM);
			}
			memcpy(sc->sc_flowdst, &pflowr->dst,
			    sizeof(struct sockaddr_in6));
			sc->sc_flowdst->sa_len = sizeof(struct
			    sockaddr_in6);
			break;
		default:
			break;
		}
	}

	if (pflowr->src.ss_len != 0) {
		if (sc->sc_flowsrc != NULL)
			free(sc->sc_flowsrc, M_DEVBUF);
		sc->sc_flowsrc = NULL;
		if (sc->so != NULL) {
			soclose(sc->so);
			sc->so = NULL;
		}
		switch(pflowr->src.ss_family) {
		case AF_INET:
			if ((sc->sc_flowsrc = malloc(
			    sizeof(struct sockaddr_in),
			    M_DEVBUF, M_NOWAIT)) == NULL)
				return (ENOMEM);
			memcpy(sc->sc_flowsrc, &pflowr->src,
			    sizeof(struct sockaddr_in));
			sc->sc_flowsrc->sa_len = sizeof(struct
			    sockaddr_in);
			break;
		case AF_INET6:
			if ((sc->sc_flowsrc = malloc(
			    sizeof(struct sockaddr_in6),
			    M_DEVBUF, M_NOWAIT)) == NULL)
				return (ENOMEM);
			memcpy(sc->sc_flowsrc, &pflowr->src,
			    sizeof(struct sockaddr_in6));
			sc->sc_flowsrc->sa_len = sizeof(struct
			    sockaddr_in6);
			break;
		default:
			break;
		}
	}

	if (sc->so == NULL) {
		if (pflowvalidsockaddr(sc->sc_flowdst, 0)) {
			error = socreate(sc->sc_flowdst->sa_family,
			    &so, SOCK_DGRAM, IPPROTO_UDP, cred, td);
			if (error)
				return (error);
			if (pflowvalidsockaddr(sc->sc_flowsrc, 1)) {
				error = sobind(so, sc->sc_flowsrc, td);
				if (error) {
					soclose(so);
					return (error);
				}
			}
			sc->so = so;
		}
	} else if (!pflowvalidsockaddr(sc->sc_flowdst, 0)) {
		soclose(sc->so);
		sc->so = NULL;
	}

	/* error check is above */
	if (pflowr->version != 0)
		sc->sc_version = pflowr->version;

	pflow_setmtu(sc, ETHERMTU);

	switch (sc->sc_version) {
	case PFLOW_PROTO_5:
		callout_stop(&sc->sc_tmo6);
		callout_stop(&sc->sc_tmo_tmpl);
		break;
	case PFLOW_PROTO_10:
		callout_reset(&sc->sc_tmo_tmpl, PFLOW_TMPL_TIMEOUT * hz,
		    pflow_timeout_tmpl, sc);
		break;
	default: /* NOTREACHED */
		break;
	}

	return (0);
}

static int
pflow_nl_set(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct epoch_tracker et;
	struct pflow_parsed_set s = {};
	struct pflow_softc *sc = NULL;
	int error;

	error = nl_parse_nlmsg(hdr, &set_parser, npt, &s);
	if (error != 0)
		return (error);

	NET_EPOCH_ENTER(et);
	CK_LIST_FOREACH(sc, &V_pflowif_list, sc_next) {
		if (sc->sc_id == s.id)
			break;
	}
	if (sc == NULL) {
		error = ENOENT;
		goto out;
	}

	PFLOW_LOCK(sc);
	error = pflow_set(sc, &s, nlp_get_cred(npt->nlp));
	PFLOW_UNLOCK(sc);

out:
	NET_EPOCH_EXIT(et);
	return (error);
}

static const struct genl_cmd pflow_cmds[] = {
	{
		.cmd_num = PFLOWNL_CMD_LIST,
		.cmd_name = "LIST",
		.cmd_cb = pflow_nl_list,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFLOWNL_CMD_CREATE,
		.cmd_name = "CREATE",
		.cmd_cb = pflow_nl_create,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFLOWNL_CMD_DEL,
		.cmd_name = "DEL",
		.cmd_cb = pflow_nl_del,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFLOWNL_CMD_GET,
		.cmd_name = "GET",
		.cmd_cb = pflow_nl_get,
		.cmd_flags = GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
	{
		.cmd_num = PFLOWNL_CMD_SET,
		.cmd_name = "SET",
		.cmd_cb = pflow_nl_set,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_NETINET_PF,
	},
};

static const struct nlhdr_parser *all_parsers[] = {
	&del_parser,
	&get_parser,
	&set_parser,
};

static int
pflow_init(void)
{
	bool ret;
	int family_id __diagused;

	NL_VERIFY_PARSERS(all_parsers);

	family_id = genl_register_family(PFLOWNL_FAMILY_NAME, 0, 2, PFLOWNL_CMD_MAX);
	MPASS(family_id != 0);
	ret = genl_register_cmds(PFLOWNL_FAMILY_NAME, pflow_cmds, NL_ARRAY_LEN(pflow_cmds));

	return (ret ? 0 : ENODEV);
}

static void
pflow_uninit(void)
{
	genl_unregister_family(PFLOWNL_FAMILY_NAME);
}

static int
pflow_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		error = pflow_init();
		break;
	case MOD_UNLOAD:
		pflow_uninit();
		break;
	default:
		error = EINVAL;
		break;
	}

	return (error);
}

static moduledata_t pflow_mod = {
	pflowname,
	pflow_modevent,
	0
};

DECLARE_MODULE(pflow, pflow_mod, SI_SUB_PROTO_FIREWALL, SI_ORDER_ANY);
MODULE_VERSION(pflow, 1);
MODULE_DEPEND(pflow, pf, PF_MODVER, PF_MODVER, PF_MODVER);
