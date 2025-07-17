/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Adrian Chadd <adrian@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/gpio.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/smp.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_vlan_var.h>
#include <net/if_media.h>
#include <net/ethernet.h>
#include <net/if_types.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/atomic.h>

#include <dev/gpio/gpiobusvar.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/qcom_ess_edma/qcom_ess_edma_var.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_reg.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_hw.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_desc.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_rx.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_tx.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_debug.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_gmac.h>

static int
qcom_ess_edma_gmac_mediachange(if_t ifp)
{
	struct qcom_ess_edma_gmac *gmac = if_getsoftc(ifp);
	struct qcom_ess_edma_softc *sc = gmac->sc;
	struct ifmedia *ifm = &gmac->ifm;
	struct ifmedia_entry *ife = ifm->ifm_cur;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
	return (EINVAL);

	if (IFM_SUBTYPE(ife->ifm_media) == IFM_AUTO) {
		device_printf(sc->sc_dev,
		    "AUTO is not supported this MAC");
		return (EINVAL);
	}

	/*
	 * Ignore everything
	 */
	return (0);
}

static void
qcom_ess_edma_gmac_mediastatus(if_t ifp, struct ifmediareq *ifmr)
{

	ifmr->ifm_status = IFM_AVALID | IFM_ACTIVE;
	ifmr->ifm_active = IFM_ETHER | IFM_1000_T | IFM_FDX;
}

static int
qcom_ess_edma_gmac_ioctl(if_t ifp, u_long command, caddr_t data)
{
	struct qcom_ess_edma_gmac *gmac = if_getsoftc(ifp);
	struct qcom_ess_edma_softc *sc = gmac->sc;
	struct ifreq *ifr = (struct ifreq *) data;
	int error, mask;

	switch (command) {
	case SIOCSIFFLAGS:
		if ((if_getflags(ifp) & IFF_UP) != 0) {
			/* up */
			QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_STATE,
			    "%s: gmac%d: IFF_UP\n",
			    __func__,
			    gmac->id);
			if_setdrvflagbits(ifp, IFF_DRV_RUNNING,
			    IFF_DRV_OACTIVE);
			if_link_state_change(ifp, LINK_STATE_UP);

		} else if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) != 0) {
			/* down */
			if_setdrvflagbits(ifp, 0, IFF_DRV_RUNNING);
			QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_STATE,
			    "%s: gmac%d: IF down\n",
			    __func__,
			    gmac->id);
			if_link_state_change(ifp, LINK_STATE_DOWN);
		}
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &gmac->ifm, command);
		break;
	case SIOCSIFCAP:
		mask = ifr->ifr_reqcap ^ if_getcapenable(ifp);
		error = 0;

		if ((mask & IFCAP_RXCSUM) != 0 &&
		    (if_getcapabilities(ifp) & IFCAP_RXCSUM) != 0)
			if_togglecapenable(ifp, IFCAP_RXCSUM);

		if ((mask & IFCAP_VLAN_HWTAGGING) != 0 &&
		    (if_getcapabilities(ifp) & IFCAP_VLAN_HWTAGGING) != 0)
			if_togglecapenable(ifp, IFCAP_VLAN_HWTAGGING);

		VLAN_CAPABILITIES(ifp);
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return (error);
}

static void
qcom_ess_edma_gmac_init(void *arg)
{
	struct qcom_ess_edma_gmac *gmac = arg;
	struct qcom_ess_edma_softc *sc = gmac->sc;

	QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_STATE,
	    "%s: gmac%d: called\n",
	    __func__,
	    gmac->id);

	if_setdrvflagbits(gmac->ifp, IFF_DRV_RUNNING, IFF_DRV_OACTIVE);
	if_link_state_change(gmac->ifp, LINK_STATE_UP);
}

static int
qcom_ess_edma_gmac_transmit(if_t ifp, struct mbuf *m)
{
	struct qcom_ess_edma_gmac *gmac = if_getsoftc(ifp);
	struct qcom_ess_edma_softc *sc = gmac->sc;
	struct qcom_ess_edma_tx_state *txs;
	int ret;
	int q;

	/* Make sure our CPU doesn't change whilst we're running */
	sched_pin();

	/*
	 * Map flowid / curcpu to a given transmit queue.
	 *
	 * Since we're running on a platform with either two
	 * or four CPUs, we want to distribute the load to a set
	 * of TX queues that won't clash with any other CPU TX queue
	 * use.
	 */
	if (M_HASHTYPE_GET(m) != M_HASHTYPE_NONE) {
		/* Map flowid to a queue */
		q = m->m_pkthdr.flowid % sc->sc_config.num_tx_queue_per_cpu;

		/* Now, map the queue to a set of unique queues per CPU */
		q = q << (mp_ncpus * curcpu);

		/* And ensure we're not overflowing */
		q = q % QCOM_ESS_EDMA_NUM_TX_RINGS;
	} else {
		/*
		 * Use the first TXQ in each CPU group, so we don't
		 * hit lock contention with traffic that has flowids.
		 */
		q = (mp_ncpus * curcpu) % QCOM_ESS_EDMA_NUM_TX_RINGS;
	}

	/* Attempt to enqueue in the buf_ring. */
	/*
	 * XXX TODO: maybe move this into *tx.c so gmac.c doesn't
	 * need to reach into the tx_state stuff?
	 */
	txs = &sc->sc_tx_state[q];

	/* XXX TODO: add an mbuf tag instead? for the transmit gmac/ifp ? */
	m->m_pkthdr.rcvif = ifp;

	ret = buf_ring_enqueue(txs->br, m);

	if (ret == 0) {
		if (atomic_cmpset_int(&txs->enqueue_is_running, 0, 1) == 1) {
			taskqueue_enqueue(txs->completion_tq, &txs->xmit_task);
		}
	}

	sched_unpin();

	/* Don't consume mbuf; if_transmit caller will if needed */
	return (ret);
}

static void
qcom_ess_edma_gmac_qflush(if_t ifp)
{
	struct qcom_ess_edma_gmac *gmac = if_getsoftc(ifp);
	struct qcom_ess_edma_softc *sc = gmac->sc;

	/* XXX TODO */
	device_printf(sc->sc_dev, "%s: gmac%d: called\n",
	    __func__,
	    gmac->id);

	/*
	 * Flushing the ifnet would, sigh, require walking each buf_ring
	 * and then removing /only/ the entries matching that ifnet.
	 * Which is a complete pain to do right now.
	 */
}

int
qcom_ess_edma_gmac_parse(struct qcom_ess_edma_softc *sc, int gmac_id)
{
	struct qcom_ess_edma_gmac *gmac;
	char gmac_name[10];
	uint32_t vlan_tag[2];
	phandle_t p;
	int len;

	sprintf(gmac_name, "gmac%d", gmac_id);

	gmac = &sc->sc_gmac[gmac_id];

	/* Find our sub-device */
	p = ofw_bus_find_child(ofw_bus_get_node(sc->sc_dev), gmac_name);
	if (p <= 0) {
		device_printf(sc->sc_dev,
		    "%s: couldn't find %s\n", __func__,
		    gmac_name);
		return (ENOENT);
	}

	/* local-mac-address */
	len = OF_getprop(p, "local-mac-address", (void *) &gmac->eaddr,
	    sizeof(struct ether_addr));
	if (len != sizeof(struct ether_addr)) {
		device_printf(sc->sc_dev,
		    "gmac%d: Couldn't parse local-mac-address\n",
		    gmac_id);
		memset(&gmac->eaddr, 0, sizeof(gmac->eaddr));
	}

	/* vlan-tag - <id portmask> tuple */
	len = OF_getproplen(p, "vlan_tag");
	if (len != sizeof(vlan_tag)) {
		device_printf(sc->sc_dev,
		    "gmac%d: no vlan_tag field or invalid size/values\n",
		    gmac_id);
		return (EINVAL);
	}
	len = OF_getencprop(p, "vlan_tag", (void *) &vlan_tag,
	    sizeof(vlan_tag));
	if (len != sizeof(vlan_tag)) {
		device_printf(sc->sc_dev,
		    "gmac%d: couldn't parse vlan_tag field\n", gmac_id);
		return (EINVAL);
	}

	/*
	 * Setup the given gmac entry.
	 */
	gmac->sc = sc;
	gmac->id = gmac_id;
	gmac->enabled = true;
	gmac->vlan_id = vlan_tag[0];
	gmac->port_mask = vlan_tag[1];

	device_printf(sc->sc_dev,
	    "gmac%d: MAC=%6D, vlan id=%d, port_mask=0x%04x\n",
	    gmac_id,
	    &gmac->eaddr, ":",
	    gmac->vlan_id,
	    gmac->port_mask);

	return (0);
}

int
qcom_ess_edma_gmac_create_ifnet(struct qcom_ess_edma_softc *sc, int gmac_id)
{
	struct qcom_ess_edma_gmac *gmac;
	char gmac_name[10];

	sprintf(gmac_name, "gmac%d", gmac_id);

	gmac = &sc->sc_gmac[gmac_id];

	/* Skip non-setup gmacs */
	if (gmac->enabled == false)
		return (0);

	gmac->ifp = if_alloc(IFT_ETHER);
	if (gmac->ifp == NULL) {
		device_printf(sc->sc_dev, "gmac%d: couldn't allocate ifnet\n",
		    gmac_id);
		return (ENOSPC);
	}

	if_setsoftc(gmac->ifp, gmac);

	if_initname(gmac->ifp, "gmac", gmac_id);

	if (ETHER_IS_ZERO(gmac->eaddr.octet)) {
		device_printf(sc->sc_dev, "gmac%d: generating random MAC\n",
		    gmac_id);
		ether_gen_addr(gmac->ifp, (void *) &gmac->eaddr.octet);
	}

	if_setflags(gmac->ifp, IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST);

	if_setioctlfn(gmac->ifp, qcom_ess_edma_gmac_ioctl);
	if_setinitfn(gmac->ifp, qcom_ess_edma_gmac_init);
	if_settransmitfn(gmac->ifp, qcom_ess_edma_gmac_transmit);
	if_setqflushfn(gmac->ifp, qcom_ess_edma_gmac_qflush);

	if_setcapabilitiesbit(gmac->ifp, IFCAP_VLAN_MTU |
	    IFCAP_VLAN_HWTAGGING, 0);

	if_setcapabilitiesbit(gmac->ifp, IFCAP_RXCSUM, 0);

	/* CSUM_TCP | CSUM_UDP for TX checksum offload */
	if_clearhwassist(gmac->ifp);

	/* Configure a hard-coded media */
	ifmedia_init(&gmac->ifm, 0, qcom_ess_edma_gmac_mediachange,
	    qcom_ess_edma_gmac_mediastatus);
	ifmedia_add(&gmac->ifm, IFM_ETHER | IFM_1000_T | IFM_FDX, 0, NULL);
	ifmedia_set(&gmac->ifm, IFM_ETHER | IFM_1000_T | IFM_FDX);

	ether_ifattach(gmac->ifp, (char *) &gmac->eaddr);

	if_setcapenable(gmac->ifp, if_getcapabilities(gmac->ifp));

	return (0);
}

/*
 * Setup the port mapping for the given GMAC.
 *
 * This populates sc->sc_gmac_port_map[] to point the given port
 * entry to this gmac index.  The receive path code can then use
 * this to figure out which gmac ifp to push a receive frame into.
 */
int
qcom_ess_edma_gmac_setup_port_mapping(struct qcom_ess_edma_softc *sc,
    int gmac_id)
{
	struct qcom_ess_edma_gmac *gmac;
	int i;

	gmac = &sc->sc_gmac[gmac_id];

	/* Skip non-setup gmacs */
	if (gmac->enabled == false)
		return (0);

	for (i = 0; i < QCOM_ESS_EDMA_MAX_NUM_PORTS; i++) {
		if ((gmac->port_mask & (1U << i)) == 0)
			continue;
		if (sc->sc_gmac_port_map[i] != -1) {
			device_printf(sc->sc_dev,
			    "DUPLICATE GMAC port map (port %d)\n",
			    i);
			return (ENXIO);
		}

		sc->sc_gmac_port_map[i] = gmac_id;

		if (bootverbose)
			device_printf(sc->sc_dev,
			    "ESS port %d maps to gmac%d\n",
			    i, gmac_id);
	}

	return (0);
}

/*
 * Receive frames to into the network stack.
 *
 * This takes a list of mbufs in the mbufq and receives them
 * up into the appopriate ifnet context.  It takes care of
 * the network epoch as well.
 *
 * This must be called with no locks held.
 */
int
qcom_ess_edma_gmac_receive_frames(struct qcom_ess_edma_softc *sc,
    int rx_queue, struct mbufq *mq)
{
	struct qcom_ess_edma_desc_ring *ring;
	struct epoch_tracker et;
	struct mbuf *m;
	if_t ifp;

	ring = &sc->sc_rx_ring[rx_queue];

	NET_EPOCH_ENTER(et);
	while ((m = mbufq_dequeue(mq)) != NULL) {
		if (m->m_pkthdr.rcvif == NULL) {
			ring->stats.num_rx_no_gmac++;
			m_freem(m);
		} else {
			ring->stats.num_rx_ok++;
			ifp = m->m_pkthdr.rcvif;
			if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);
			if_input(ifp, m);
		}
	}
	NET_EPOCH_EXIT(et);
	return (0);
}
