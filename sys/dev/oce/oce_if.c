/*-
 * Copyright (C) 2013 Emulex
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Emulex Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Contact Information:
 * freebsd-drivers@emulex.com
 *
 * Emulex
 * 3333 Susan Street
 * Costa Mesa, CA 92626
 */


/* $FreeBSD$ */

#include "opt_inet6.h"
#include "opt_inet.h"

#include "oce_if.h"


/* Driver entry points prototypes */
static int  oce_probe(device_t dev);
static int  oce_attach(device_t dev);
static int  oce_detach(device_t dev);
static int  oce_shutdown(device_t dev);
static int  oce_ioctl(struct ifnet *ifp, u_long command, caddr_t data);
static void oce_init(void *xsc);
static int  oce_multiq_start(struct ifnet *ifp, struct mbuf *m);
static void oce_multiq_flush(struct ifnet *ifp);

/* Driver interrupt routines protypes */
static void oce_intr(void *arg, int pending);
static int  oce_setup_intr(POCE_SOFTC sc);
static int  oce_fast_isr(void *arg);
static int  oce_alloc_intr(POCE_SOFTC sc, int vector,
			  void (*isr) (void *arg, int pending));

/* Media callbacks prototypes */
static void oce_media_status(struct ifnet *ifp, struct ifmediareq *req);
static int  oce_media_change(struct ifnet *ifp);

/* Transmit routines prototypes */
static int  oce_tx(POCE_SOFTC sc, struct mbuf **mpp, int wq_index);
static void oce_tx_restart(POCE_SOFTC sc, struct oce_wq *wq);
static void oce_tx_complete(struct oce_wq *wq, uint32_t wqe_idx,
					uint32_t status);
static int  oce_multiq_transmit(struct ifnet *ifp, struct mbuf *m,
				 struct oce_wq *wq);

/* Receive routines prototypes */
static void oce_discard_rx_comp(struct oce_rq *rq, struct oce_nic_rx_cqe *cqe);
static int  oce_cqe_vtp_valid(POCE_SOFTC sc, struct oce_nic_rx_cqe *cqe);
static int  oce_cqe_portid_valid(POCE_SOFTC sc, struct oce_nic_rx_cqe *cqe);
static void oce_rx(struct oce_rq *rq, uint32_t rqe_idx,
						struct oce_nic_rx_cqe *cqe);

/* Helper function prototypes in this file */
static int  oce_attach_ifp(POCE_SOFTC sc);
static void oce_add_vlan(void *arg, struct ifnet *ifp, uint16_t vtag);
static void oce_del_vlan(void *arg, struct ifnet *ifp, uint16_t vtag);
static int  oce_vid_config(POCE_SOFTC sc);
static void oce_mac_addr_set(POCE_SOFTC sc);
static int  oce_handle_passthrough(struct ifnet *ifp, caddr_t data);
static void oce_local_timer(void *arg);
static void oce_if_deactivate(POCE_SOFTC sc);
static void oce_if_activate(POCE_SOFTC sc);
static void setup_max_queues_want(POCE_SOFTC sc);
static void update_queues_got(POCE_SOFTC sc);
static void process_link_state(POCE_SOFTC sc,
		 struct oce_async_cqe_link_state *acqe);
static int oce_tx_asic_stall_verify(POCE_SOFTC sc, struct mbuf *m);
static void oce_get_config(POCE_SOFTC sc);
static struct mbuf *oce_insert_vlan_tag(POCE_SOFTC sc, struct mbuf *m, boolean_t *complete);

/* IP specific */
#if defined(INET6) || defined(INET)
static int  oce_init_lro(POCE_SOFTC sc);
static void oce_rx_flush_lro(struct oce_rq *rq);
static struct mbuf * oce_tso_setup(POCE_SOFTC sc, struct mbuf **mpp);
#endif

static device_method_t oce_dispatch[] = {
	DEVMETHOD(device_probe, oce_probe),
	DEVMETHOD(device_attach, oce_attach),
	DEVMETHOD(device_detach, oce_detach),
	DEVMETHOD(device_shutdown, oce_shutdown),

	DEVMETHOD_END
};

static driver_t oce_driver = {
	"oce",
	oce_dispatch,
	sizeof(OCE_SOFTC)
};
static devclass_t oce_devclass;


DRIVER_MODULE(oce, pci, oce_driver, oce_devclass, 0, 0);
MODULE_DEPEND(oce, pci, 1, 1, 1);
MODULE_DEPEND(oce, ether, 1, 1, 1);
MODULE_VERSION(oce, 1);


/* global vars */
const char component_revision[32] = {"///" COMPONENT_REVISION "///"};

/* Module capabilites and parameters */
uint32_t oce_max_rsp_handled = OCE_MAX_RSP_HANDLED;
uint32_t oce_enable_rss = OCE_MODCAP_RSS;


TUNABLE_INT("hw.oce.max_rsp_handled", &oce_max_rsp_handled);
TUNABLE_INT("hw.oce.enable_rss", &oce_enable_rss);


/* Supported devices table */
static uint32_t supportedDevices[] =  {
	(PCI_VENDOR_SERVERENGINES << 16) | PCI_PRODUCT_BE2,
	(PCI_VENDOR_SERVERENGINES << 16) | PCI_PRODUCT_BE3,
	(PCI_VENDOR_EMULEX << 16) | PCI_PRODUCT_BE3,
	(PCI_VENDOR_EMULEX << 16) | PCI_PRODUCT_XE201,
	(PCI_VENDOR_EMULEX << 16) | PCI_PRODUCT_XE201_VF,
	(PCI_VENDOR_EMULEX << 16) | PCI_PRODUCT_SH
};




/*****************************************************************************
 *			Driver entry points functions                        *
 *****************************************************************************/

static int
oce_probe(device_t dev)
{
	uint16_t vendor = 0;
	uint16_t device = 0;
	int i = 0;
	char str[256] = {0};
	POCE_SOFTC sc;

	sc = device_get_softc(dev);
	bzero(sc, sizeof(OCE_SOFTC));
	sc->dev = dev;

	vendor = pci_get_vendor(dev);
	device = pci_get_device(dev);

	for (i = 0; i < (sizeof(supportedDevices) / sizeof(uint32_t)); i++) {
		if (vendor == ((supportedDevices[i] >> 16) & 0xffff)) {
			if (device == (supportedDevices[i] & 0xffff)) {
				sprintf(str, "%s:%s", "Emulex CNA NIC function",
					component_revision);
				device_set_desc_copy(dev, str);

				switch (device) {
				case PCI_PRODUCT_BE2:
					sc->flags |= OCE_FLAGS_BE2;
					break;
				case PCI_PRODUCT_BE3:
					sc->flags |= OCE_FLAGS_BE3;
					break;
				case PCI_PRODUCT_XE201:
				case PCI_PRODUCT_XE201_VF:
					sc->flags |= OCE_FLAGS_XE201;
					break;
				case PCI_PRODUCT_SH:
					sc->flags |= OCE_FLAGS_SH;
					break;
				default:
					return ENXIO;
				}
				return BUS_PROBE_DEFAULT;
			}
		}
	}

	return ENXIO;
}


static int
oce_attach(device_t dev)
{
	POCE_SOFTC sc;
	int rc = 0;

	sc = device_get_softc(dev);

	rc = oce_hw_pci_alloc(sc);
	if (rc)
		return rc;

	sc->tx_ring_size = OCE_TX_RING_SIZE;
	sc->rx_ring_size = OCE_RX_RING_SIZE;
	sc->rq_frag_size = OCE_RQ_BUF_SIZE;
	sc->flow_control = OCE_DEFAULT_FLOW_CONTROL;
	sc->promisc	 = OCE_DEFAULT_PROMISCUOUS;

	LOCK_CREATE(&sc->bmbx_lock, "Mailbox_lock");
	LOCK_CREATE(&sc->dev_lock,  "Device_lock");

	/* initialise the hardware */
	rc = oce_hw_init(sc);
	if (rc)
		goto pci_res_free;

	oce_get_config(sc);

	setup_max_queues_want(sc);	

	rc = oce_setup_intr(sc);
	if (rc)
		goto mbox_free;

	rc = oce_queue_init_all(sc);
	if (rc)
		goto intr_free;

	rc = oce_attach_ifp(sc);
	if (rc)
		goto queues_free;

#if defined(INET6) || defined(INET)
	rc = oce_init_lro(sc);
	if (rc)
		goto ifp_free;
#endif

	rc = oce_hw_start(sc);
	if (rc)
		goto lro_free;

	sc->vlan_attach = EVENTHANDLER_REGISTER(vlan_config,
				oce_add_vlan, sc, EVENTHANDLER_PRI_FIRST);
	sc->vlan_detach = EVENTHANDLER_REGISTER(vlan_unconfig,
				oce_del_vlan, sc, EVENTHANDLER_PRI_FIRST);

	rc = oce_stats_init(sc);
	if (rc)
		goto vlan_free;

	oce_add_sysctls(sc);

	callout_init(&sc->timer, CALLOUT_MPSAFE);
	rc = callout_reset(&sc->timer, 2 * hz, oce_local_timer, sc);
	if (rc)
		goto stats_free;

	return 0;

stats_free:
	callout_drain(&sc->timer);
	oce_stats_free(sc);
vlan_free:
	if (sc->vlan_attach)
		EVENTHANDLER_DEREGISTER(vlan_config, sc->vlan_attach);
	if (sc->vlan_detach)
		EVENTHANDLER_DEREGISTER(vlan_unconfig, sc->vlan_detach);
	oce_hw_intr_disable(sc);
lro_free:
#if defined(INET6) || defined(INET)
	oce_free_lro(sc);
ifp_free:
#endif
	ether_ifdetach(sc->ifp);
	if_free(sc->ifp);
queues_free:
	oce_queue_release_all(sc);
intr_free:
	oce_intr_free(sc);
mbox_free:
	oce_dma_free(sc, &sc->bsmbx);
pci_res_free:
	oce_hw_pci_free(sc);
	LOCK_DESTROY(&sc->dev_lock);
	LOCK_DESTROY(&sc->bmbx_lock);
	return rc;

}


static int
oce_detach(device_t dev)
{
	POCE_SOFTC sc = device_get_softc(dev);

	LOCK(&sc->dev_lock);
	oce_if_deactivate(sc);
	UNLOCK(&sc->dev_lock);

	callout_drain(&sc->timer);
	
	if (sc->vlan_attach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_config, sc->vlan_attach);
	if (sc->vlan_detach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_unconfig, sc->vlan_detach);

	ether_ifdetach(sc->ifp);

	if_free(sc->ifp);

	oce_hw_shutdown(sc);

	bus_generic_detach(dev);

	return 0;
}


static int
oce_shutdown(device_t dev)
{
	int rc;
	
	rc = oce_detach(dev);

	return rc;	
}


static int
oce_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct ifreq *ifr = (struct ifreq *)data;
	POCE_SOFTC sc = ifp->if_softc;
	int rc = 0;
	uint32_t u;

	switch (command) {

	case SIOCGIFMEDIA:
		rc = ifmedia_ioctl(ifp, ifr, &sc->media, command);
		break;

	case SIOCSIFMTU:
		if (ifr->ifr_mtu > OCE_MAX_MTU)
			rc = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				sc->ifp->if_drv_flags |= IFF_DRV_RUNNING;	
				oce_init(sc);
			}
			device_printf(sc->dev, "Interface Up\n");	
		} else {
			LOCK(&sc->dev_lock);

			sc->ifp->if_drv_flags &=
			    ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
			oce_if_deactivate(sc);

			UNLOCK(&sc->dev_lock);

			device_printf(sc->dev, "Interface Down\n");
		}

		if ((ifp->if_flags & IFF_PROMISC) && !sc->promisc) {
			sc->promisc = TRUE;
			oce_rxf_set_promiscuous(sc, sc->promisc);
		} else if (!(ifp->if_flags & IFF_PROMISC) && sc->promisc) {
			sc->promisc = FALSE;
			oce_rxf_set_promiscuous(sc, sc->promisc);
		}

		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		rc = oce_hw_update_multicast(sc);
		if (rc)
			device_printf(sc->dev,
				"Update multicast address failed\n");
		break;

	case SIOCSIFCAP:
		u = ifr->ifr_reqcap ^ ifp->if_capenable;

		if (u & IFCAP_TXCSUM) {
			ifp->if_capenable ^= IFCAP_TXCSUM;
			ifp->if_hwassist ^= (CSUM_TCP | CSUM_UDP | CSUM_IP);
			
			if (IFCAP_TSO & ifp->if_capenable &&
			    !(IFCAP_TXCSUM & ifp->if_capenable)) {
				ifp->if_capenable &= ~IFCAP_TSO;
				ifp->if_hwassist &= ~CSUM_TSO;
				if_printf(ifp,
					 "TSO disabled due to -txcsum.\n");
			}
		}

		if (u & IFCAP_RXCSUM)
			ifp->if_capenable ^= IFCAP_RXCSUM;

		if (u & IFCAP_TSO4) {
			ifp->if_capenable ^= IFCAP_TSO4;

			if (IFCAP_TSO & ifp->if_capenable) {
				if (IFCAP_TXCSUM & ifp->if_capenable)
					ifp->if_hwassist |= CSUM_TSO;
				else {
					ifp->if_capenable &= ~IFCAP_TSO;
					ifp->if_hwassist &= ~CSUM_TSO;
					if_printf(ifp,
					    "Enable txcsum first.\n");
					rc = EAGAIN;
				}
			} else
				ifp->if_hwassist &= ~CSUM_TSO;
		}

		if (u & IFCAP_VLAN_HWTAGGING)
			ifp->if_capenable ^= IFCAP_VLAN_HWTAGGING;

		if (u & IFCAP_VLAN_HWFILTER) {
			ifp->if_capenable ^= IFCAP_VLAN_HWFILTER;
			oce_vid_config(sc);
		}
#if defined(INET6) || defined(INET)
		if (u & IFCAP_LRO)
			ifp->if_capenable ^= IFCAP_LRO;
#endif

		break;

	case SIOCGPRIVATE_0:
		rc = oce_handle_passthrough(ifp, data);
		break;
	default:
		rc = ether_ioctl(ifp, command, data);
		break;
	}

	return rc;
}


static void
oce_init(void *arg)
{
	POCE_SOFTC sc = arg;
	
	LOCK(&sc->dev_lock);

	if (sc->ifp->if_flags & IFF_UP) {
		oce_if_deactivate(sc);
		oce_if_activate(sc);
	}
	
	UNLOCK(&sc->dev_lock);

}


static int
oce_multiq_start(struct ifnet *ifp, struct mbuf *m)
{
	POCE_SOFTC sc = ifp->if_softc;
	struct oce_wq *wq = NULL;
	int queue_index = 0;
	int status = 0;

	if (!sc->link_status)
		return ENXIO;

	if ((m->m_flags & M_FLOWID) != 0)
		queue_index = m->m_pkthdr.flowid % sc->nwqs;

	wq = sc->wq[queue_index];

	LOCK(&wq->tx_lock);
	status = oce_multiq_transmit(ifp, m, wq);
	UNLOCK(&wq->tx_lock);

	return status;

}


static void
oce_multiq_flush(struct ifnet *ifp)
{
	POCE_SOFTC sc = ifp->if_softc;
	struct mbuf     *m;
	int i = 0;

	for (i = 0; i < sc->nwqs; i++) {
		while ((m = buf_ring_dequeue_sc(sc->wq[i]->br)) != NULL)
			m_freem(m);
	}
	if_qflush(ifp);
}



/*****************************************************************************
 *                   Driver interrupt routines functions                     *
 *****************************************************************************/

static void
oce_intr(void *arg, int pending)
{

	POCE_INTR_INFO ii = (POCE_INTR_INFO) arg;
	POCE_SOFTC sc = ii->sc;
	struct oce_eq *eq = ii->eq;
	struct oce_eqe *eqe;
	struct oce_cq *cq = NULL;
	int i, num_eqes = 0;


	bus_dmamap_sync(eq->ring->dma.tag, eq->ring->dma.map,
				 BUS_DMASYNC_POSTWRITE);
	do {
		eqe = RING_GET_CONSUMER_ITEM_VA(eq->ring, struct oce_eqe);
		if (eqe->evnt == 0)
			break;
		eqe->evnt = 0;
		bus_dmamap_sync(eq->ring->dma.tag, eq->ring->dma.map,
					BUS_DMASYNC_POSTWRITE);
		RING_GET(eq->ring, 1);
		num_eqes++;

	} while (TRUE);
	
	if (!num_eqes)
		goto eq_arm; /* Spurious */

 	/* Clear EQ entries, but dont arm */
	oce_arm_eq(sc, eq->eq_id, num_eqes, FALSE, FALSE);

	/* Process TX, RX and MCC. But dont arm CQ*/
	for (i = 0; i < eq->cq_valid; i++) {
		cq = eq->cq[i];
		(*cq->cq_handler)(cq->cb_arg);
	}

	/* Arm all cqs connected to this EQ */
	for (i = 0; i < eq->cq_valid; i++) {
		cq = eq->cq[i];
		oce_arm_cq(sc, cq->cq_id, 0, TRUE);
	}

eq_arm:
	oce_arm_eq(sc, eq->eq_id, 0, TRUE, FALSE);

	return;
}


static int
oce_setup_intr(POCE_SOFTC sc)
{
	int rc = 0, use_intx = 0;
	int vector = 0, req_vectors = 0;

	if (is_rss_enabled(sc))
		req_vectors = MAX((sc->nrqs - 1), sc->nwqs);
	else
		req_vectors = 1;

	if (sc->flags & OCE_FLAGS_MSIX_CAPABLE) {
		sc->intr_count = req_vectors;
		rc = pci_alloc_msix(sc->dev, &sc->intr_count);
		if (rc != 0) {
			use_intx = 1;
			pci_release_msi(sc->dev);
		} else
			sc->flags |= OCE_FLAGS_USING_MSIX;
	} else
		use_intx = 1;

	if (use_intx)
		sc->intr_count = 1;

	/* Scale number of queues based on intr we got */
	update_queues_got(sc);

	if (use_intx) {
		device_printf(sc->dev, "Using legacy interrupt\n");
		rc = oce_alloc_intr(sc, vector, oce_intr);
		if (rc)
			goto error;		
	} else {
		for (; vector < sc->intr_count; vector++) {
			rc = oce_alloc_intr(sc, vector, oce_intr);
			if (rc)
				goto error;
		}
	}

	return 0;
error:
	oce_intr_free(sc);
	return rc;
}


static int
oce_fast_isr(void *arg)
{
	POCE_INTR_INFO ii = (POCE_INTR_INFO) arg;
	POCE_SOFTC sc = ii->sc;

	if (ii->eq == NULL)
		return FILTER_STRAY;

	oce_arm_eq(sc, ii->eq->eq_id, 0, FALSE, TRUE);

	taskqueue_enqueue_fast(ii->tq, &ii->task);

 	ii->eq->intr++;	

	return FILTER_HANDLED;
}


static int
oce_alloc_intr(POCE_SOFTC sc, int vector, void (*isr) (void *arg, int pending))
{
	POCE_INTR_INFO ii = &sc->intrs[vector];
	int rc = 0, rr;

	if (vector >= OCE_MAX_EQ)
		return (EINVAL);

	/* Set the resource id for the interrupt.
	 * MSIx is vector + 1 for the resource id,
	 * INTx is 0 for the resource id.
	 */
	if (sc->flags & OCE_FLAGS_USING_MSIX)
		rr = vector + 1;
	else
		rr = 0;
	ii->intr_res = bus_alloc_resource_any(sc->dev,
					      SYS_RES_IRQ,
					      &rr, RF_ACTIVE|RF_SHAREABLE);
	ii->irq_rr = rr;
	if (ii->intr_res == NULL) {
		device_printf(sc->dev,
			  "Could not allocate interrupt\n");
		rc = ENXIO;
		return rc;
	}

	TASK_INIT(&ii->task, 0, isr, ii);
	ii->vector = vector;
	sprintf(ii->task_name, "oce_task[%d]", ii->vector);
	ii->tq = taskqueue_create_fast(ii->task_name,
			M_NOWAIT,
			taskqueue_thread_enqueue,
			&ii->tq);
	taskqueue_start_threads(&ii->tq, 1, PI_NET, "%s taskq",
			device_get_nameunit(sc->dev));

	ii->sc = sc;
	rc = bus_setup_intr(sc->dev,
			ii->intr_res,
			INTR_TYPE_NET,
			oce_fast_isr, NULL, ii, &ii->tag);
	return rc;

}


void
oce_intr_free(POCE_SOFTC sc)
{
	int i = 0;
	
	for (i = 0; i < sc->intr_count; i++) {
		
		if (sc->intrs[i].tag != NULL)
			bus_teardown_intr(sc->dev, sc->intrs[i].intr_res,
						sc->intrs[i].tag);
		if (sc->intrs[i].tq != NULL)
			taskqueue_free(sc->intrs[i].tq);
		
		if (sc->intrs[i].intr_res != NULL)
			bus_release_resource(sc->dev, SYS_RES_IRQ,
						sc->intrs[i].irq_rr,
						sc->intrs[i].intr_res);
		sc->intrs[i].tag = NULL;
		sc->intrs[i].intr_res = NULL;
	}

	if (sc->flags & OCE_FLAGS_USING_MSIX)
		pci_release_msi(sc->dev);

}



/******************************************************************************
*			  Media callbacks functions 			      *
******************************************************************************/

static void
oce_media_status(struct ifnet *ifp, struct ifmediareq *req)
{
	POCE_SOFTC sc = (POCE_SOFTC) ifp->if_softc;


	req->ifm_status = IFM_AVALID;
	req->ifm_active = IFM_ETHER;
	
	if (sc->link_status == 1)
		req->ifm_status |= IFM_ACTIVE;
	else 
		return;
	
	switch (sc->link_speed) {
	case 1: /* 10 Mbps */
		req->ifm_active |= IFM_10_T | IFM_FDX;
		sc->speed = 10;
		break;
	case 2: /* 100 Mbps */
		req->ifm_active |= IFM_100_TX | IFM_FDX;
		sc->speed = 100;
		break;
	case 3: /* 1 Gbps */
		req->ifm_active |= IFM_1000_T | IFM_FDX;
		sc->speed = 1000;
		break;
	case 4: /* 10 Gbps */
		req->ifm_active |= IFM_10G_SR | IFM_FDX;
		sc->speed = 10000;
		break;
	}
	
	return;
}


int
oce_media_change(struct ifnet *ifp)
{
	return 0;
}




/*****************************************************************************
 *			  Transmit routines functions			     *
 *****************************************************************************/

static int
oce_tx(POCE_SOFTC sc, struct mbuf **mpp, int wq_index)
{
	int rc = 0, i, retry_cnt = 0;
	bus_dma_segment_t segs[OCE_MAX_TX_ELEMENTS];
	struct mbuf *m, *m_temp;
	struct oce_wq *wq = sc->wq[wq_index];
	struct oce_packet_desc *pd;
	struct oce_nic_hdr_wqe *nichdr;
	struct oce_nic_frag_wqe *nicfrag;
	int num_wqes;
	uint32_t reg_value;
	boolean_t complete = TRUE;

	m = *mpp;
	if (!m)
		return EINVAL;

	if (!(m->m_flags & M_PKTHDR)) {
		rc = ENXIO;
		goto free_ret;
	}

	if(oce_tx_asic_stall_verify(sc, m)) {
		m = oce_insert_vlan_tag(sc, m, &complete);
		if(!m) {
			device_printf(sc->dev, "Insertion unsuccessful\n");
			return 0;
		}

	}

	if (m->m_pkthdr.csum_flags & CSUM_TSO) {
		/* consolidate packet buffers for TSO/LSO segment offload */
#if defined(INET6) || defined(INET)
		m = oce_tso_setup(sc, mpp);
#else
		m = NULL;
#endif
		if (m == NULL) {
			rc = ENXIO;
			goto free_ret;
		}
	}

	pd = &wq->pckts[wq->pkt_desc_head];
retry:
	rc = bus_dmamap_load_mbuf_sg(wq->tag,
				     pd->map,
				     m, segs, &pd->nsegs, BUS_DMA_NOWAIT);
	if (rc == 0) {
		num_wqes = pd->nsegs + 1;
		if (IS_BE(sc) || IS_SH(sc)) {
			/*Dummy required only for BE3.*/
			if (num_wqes & 1)
				num_wqes++;
		}
		if (num_wqes >= RING_NUM_FREE(wq->ring)) {
			bus_dmamap_unload(wq->tag, pd->map);
			return EBUSY;
		}
		atomic_store_rel_int(&wq->pkt_desc_head,
				     (wq->pkt_desc_head + 1) % \
				      OCE_WQ_PACKET_ARRAY_SIZE);
		bus_dmamap_sync(wq->tag, pd->map, BUS_DMASYNC_PREWRITE);
		pd->mbuf = m;

		nichdr =
		    RING_GET_PRODUCER_ITEM_VA(wq->ring, struct oce_nic_hdr_wqe);
		nichdr->u0.dw[0] = 0;
		nichdr->u0.dw[1] = 0;
		nichdr->u0.dw[2] = 0;
		nichdr->u0.dw[3] = 0;

		nichdr->u0.s.complete = complete;
		nichdr->u0.s.event = 1;
		nichdr->u0.s.crc = 1;
		nichdr->u0.s.forward = 0;
		nichdr->u0.s.ipcs = (m->m_pkthdr.csum_flags & CSUM_IP) ? 1 : 0;
		nichdr->u0.s.udpcs =
			(m->m_pkthdr.csum_flags & CSUM_UDP) ? 1 : 0;
		nichdr->u0.s.tcpcs =
			(m->m_pkthdr.csum_flags & CSUM_TCP) ? 1 : 0;
		nichdr->u0.s.num_wqe = num_wqes;
		nichdr->u0.s.total_length = m->m_pkthdr.len;
		if (m->m_flags & M_VLANTAG) {
			nichdr->u0.s.vlan = 1; /*Vlan present*/
			nichdr->u0.s.vlan_tag = m->m_pkthdr.ether_vtag;
		}
		if (m->m_pkthdr.csum_flags & CSUM_TSO) {
			if (m->m_pkthdr.tso_segsz) {
				nichdr->u0.s.lso = 1;
				nichdr->u0.s.lso_mss  = m->m_pkthdr.tso_segsz;
			}
			if (!IS_BE(sc) || !IS_SH(sc))
				nichdr->u0.s.ipcs = 1;
		}

		RING_PUT(wq->ring, 1);
		atomic_add_int(&wq->ring->num_used, 1);

		for (i = 0; i < pd->nsegs; i++) {
			nicfrag =
			    RING_GET_PRODUCER_ITEM_VA(wq->ring,
						      struct oce_nic_frag_wqe);
			nicfrag->u0.s.rsvd0 = 0;
			nicfrag->u0.s.frag_pa_hi = ADDR_HI(segs[i].ds_addr);
			nicfrag->u0.s.frag_pa_lo = ADDR_LO(segs[i].ds_addr);
			nicfrag->u0.s.frag_len = segs[i].ds_len;
			pd->wqe_idx = wq->ring->pidx;
			RING_PUT(wq->ring, 1);
			atomic_add_int(&wq->ring->num_used, 1);
		}
		if (num_wqes > (pd->nsegs + 1)) {
			nicfrag =
			    RING_GET_PRODUCER_ITEM_VA(wq->ring,
						      struct oce_nic_frag_wqe);
			nicfrag->u0.dw[0] = 0;
			nicfrag->u0.dw[1] = 0;
			nicfrag->u0.dw[2] = 0;
			nicfrag->u0.dw[3] = 0;
			pd->wqe_idx = wq->ring->pidx;
			RING_PUT(wq->ring, 1);
			atomic_add_int(&wq->ring->num_used, 1);
			pd->nsegs++;
		}

		sc->ifp->if_opackets++;
		wq->tx_stats.tx_reqs++;
		wq->tx_stats.tx_wrbs += num_wqes;
		wq->tx_stats.tx_bytes += m->m_pkthdr.len;
		wq->tx_stats.tx_pkts++;

		bus_dmamap_sync(wq->ring->dma.tag, wq->ring->dma.map,
				BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		reg_value = (num_wqes << 16) | wq->wq_id;
		OCE_WRITE_REG32(sc, db, wq->db_offset, reg_value);

	} else if (rc == EFBIG)	{
		if (retry_cnt == 0) {
			m_temp = m_defrag(m, M_NOWAIT);
			if (m_temp == NULL)
				goto free_ret;
			m = m_temp;
			*mpp = m_temp;
			retry_cnt = retry_cnt + 1;
			goto retry;
		} else
			goto free_ret;
	} else if (rc == ENOMEM)
		return rc;
	else
		goto free_ret;
	
	return 0;

free_ret:
	m_freem(*mpp);
	*mpp = NULL;
	return rc;
}


static void
oce_tx_complete(struct oce_wq *wq, uint32_t wqe_idx, uint32_t status)
{
	struct oce_packet_desc *pd;
	POCE_SOFTC sc = (POCE_SOFTC) wq->parent;
	struct mbuf *m;

	pd = &wq->pckts[wq->pkt_desc_tail];
	atomic_store_rel_int(&wq->pkt_desc_tail,
			     (wq->pkt_desc_tail + 1) % OCE_WQ_PACKET_ARRAY_SIZE); 
	atomic_subtract_int(&wq->ring->num_used, pd->nsegs + 1);
	bus_dmamap_sync(wq->tag, pd->map, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(wq->tag, pd->map);

	m = pd->mbuf;
	m_freem(m);
	pd->mbuf = NULL;


	if (sc->ifp->if_drv_flags & IFF_DRV_OACTIVE) {
		if (wq->ring->num_used < (wq->ring->num_items / 2)) {
			sc->ifp->if_drv_flags &= ~(IFF_DRV_OACTIVE);
			oce_tx_restart(sc, wq);	
		}
	}
}


static void
oce_tx_restart(POCE_SOFTC sc, struct oce_wq *wq)
{

	if ((sc->ifp->if_drv_flags & IFF_DRV_RUNNING) != IFF_DRV_RUNNING)
		return;

#if __FreeBSD_version >= 800000
	if (!drbr_empty(sc->ifp, wq->br))
#else
	if (!IFQ_DRV_IS_EMPTY(&sc->ifp->if_snd))
#endif
		taskqueue_enqueue_fast(taskqueue_swi, &wq->txtask);

}


#if defined(INET6) || defined(INET)
static struct mbuf *
oce_tso_setup(POCE_SOFTC sc, struct mbuf **mpp)
{
	struct mbuf *m;
#ifdef INET
	struct ip *ip;
#endif
#ifdef INET6
	struct ip6_hdr *ip6;
#endif
	struct ether_vlan_header *eh;
	struct tcphdr *th;
	uint16_t etype;
	int total_len = 0, ehdrlen = 0;
	
	m = *mpp;

	if (M_WRITABLE(m) == 0) {
		m = m_dup(*mpp, M_NOWAIT);
		if (!m)
			return NULL;
		m_freem(*mpp);
		*mpp = m;
	}

	eh = mtod(m, struct ether_vlan_header *);
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		etype = ntohs(eh->evl_proto);
		ehdrlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
	} else {
		etype = ntohs(eh->evl_encap_proto);
		ehdrlen = ETHER_HDR_LEN;
	}

	switch (etype) {
#ifdef INET
	case ETHERTYPE_IP:
		ip = (struct ip *)(m->m_data + ehdrlen);
		if (ip->ip_p != IPPROTO_TCP)
			return NULL;
		th = (struct tcphdr *)((caddr_t)ip + (ip->ip_hl << 2));

		total_len = ehdrlen + (ip->ip_hl << 2) + (th->th_off << 2);
		break;
#endif
#ifdef INET6
	case ETHERTYPE_IPV6:
		ip6 = (struct ip6_hdr *)(m->m_data + ehdrlen);
		if (ip6->ip6_nxt != IPPROTO_TCP)
			return NULL;
		th = (struct tcphdr *)((caddr_t)ip6 + sizeof(struct ip6_hdr));

		total_len = ehdrlen + sizeof(struct ip6_hdr) + (th->th_off << 2);
		break;
#endif
	default:
		return NULL;
	}
	
	m = m_pullup(m, total_len);
	if (!m)
		return NULL;
	*mpp = m;
	return m;
	
}
#endif /* INET6 || INET */

void
oce_tx_task(void *arg, int npending)
{
	struct oce_wq *wq = arg;
	POCE_SOFTC sc = wq->parent;
	struct ifnet *ifp = sc->ifp;
	int rc = 0;

#if __FreeBSD_version >= 800000
	LOCK(&wq->tx_lock);
	rc = oce_multiq_transmit(ifp, NULL, wq);
	if (rc) {
		device_printf(sc->dev,
				"TX[%d] restart failed\n", wq->queue_index);
	}
	UNLOCK(&wq->tx_lock);
#else
	oce_start(ifp);
#endif

}


void
oce_start(struct ifnet *ifp)
{
	POCE_SOFTC sc = ifp->if_softc;
	struct mbuf *m;
	int rc = 0;
	int def_q = 0; /* Defualt tx queue is 0*/

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
			IFF_DRV_RUNNING)
		return;

	if (!sc->link_status)
		return;
	
	do {
		IF_DEQUEUE(&sc->ifp->if_snd, m);
		if (m == NULL)
			break;

		LOCK(&sc->wq[def_q]->tx_lock);
		rc = oce_tx(sc, &m, def_q);
		UNLOCK(&sc->wq[def_q]->tx_lock);
		if (rc) {
			if (m != NULL) {
				sc->wq[def_q]->tx_stats.tx_stops ++;
				ifp->if_drv_flags |= IFF_DRV_OACTIVE;
				IFQ_DRV_PREPEND(&ifp->if_snd, m);
				m = NULL;
			}
			break;
		}
		if (m != NULL)
			ETHER_BPF_MTAP(ifp, m);

	} while (TRUE);

	return;
}


/* Handle the Completion Queue for transmit */
uint16_t
oce_wq_handler(void *arg)
{
	struct oce_wq *wq = (struct oce_wq *)arg;
	POCE_SOFTC sc = wq->parent;
	struct oce_cq *cq = wq->cq;
	struct oce_nic_tx_cqe *cqe;
	int num_cqes = 0;

	bus_dmamap_sync(cq->ring->dma.tag,
			cq->ring->dma.map, BUS_DMASYNC_POSTWRITE);
	cqe = RING_GET_CONSUMER_ITEM_VA(cq->ring, struct oce_nic_tx_cqe);
	while (cqe->u0.dw[3]) {
		DW_SWAP((uint32_t *) cqe, sizeof(oce_wq_cqe));

		wq->ring->cidx = cqe->u0.s.wqe_index + 1;
		if (wq->ring->cidx >= wq->ring->num_items)
			wq->ring->cidx -= wq->ring->num_items;

		oce_tx_complete(wq, cqe->u0.s.wqe_index, cqe->u0.s.status);
		wq->tx_stats.tx_compl++;
		cqe->u0.dw[3] = 0;
		RING_GET(cq->ring, 1);
		bus_dmamap_sync(cq->ring->dma.tag,
				cq->ring->dma.map, BUS_DMASYNC_POSTWRITE);
		cqe =
		    RING_GET_CONSUMER_ITEM_VA(cq->ring, struct oce_nic_tx_cqe);
		num_cqes++;
	}

	if (num_cqes)
		oce_arm_cq(sc, cq->cq_id, num_cqes, FALSE);

	return 0;
}


static int 
oce_multiq_transmit(struct ifnet *ifp, struct mbuf *m, struct oce_wq *wq)
{
	POCE_SOFTC sc = ifp->if_softc;
	int status = 0, queue_index = 0;
	struct mbuf *next = NULL;
	struct buf_ring *br = NULL;

	br  = wq->br;
	queue_index = wq->queue_index;

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
		IFF_DRV_RUNNING) {
		if (m != NULL)
			status = drbr_enqueue(ifp, br, m);
		return status;
	}

	 if (m != NULL) {
		if ((status = drbr_enqueue(ifp, br, m)) != 0)
			return status;
	} 
	while ((next = drbr_peek(ifp, br)) != NULL) {
		if (oce_tx(sc, &next, queue_index)) {
			if (next == NULL) {
				drbr_advance(ifp, br);
			} else {
				drbr_putback(ifp, br, next);
				wq->tx_stats.tx_stops ++;
				ifp->if_drv_flags |= IFF_DRV_OACTIVE;
				status = drbr_enqueue(ifp, br, next);
			}  
			break;
		}
		drbr_advance(ifp, br);
		ifp->if_obytes += next->m_pkthdr.len;
		if (next->m_flags & M_MCAST)
			ifp->if_omcasts++;
		ETHER_BPF_MTAP(ifp, next);
	}

	return status;
}




/*****************************************************************************
 *			    Receive  routines functions 		     *
 *****************************************************************************/

static void
oce_rx(struct oce_rq *rq, uint32_t rqe_idx, struct oce_nic_rx_cqe *cqe)
{
	uint32_t out;
	struct oce_packet_desc *pd;
	POCE_SOFTC sc = (POCE_SOFTC) rq->parent;
	int i, len, frag_len;
	struct mbuf *m = NULL, *tail = NULL;
	uint16_t vtag;

	len = cqe->u0.s.pkt_size;
	if (!len) {
		/*partial DMA workaround for Lancer*/
		oce_discard_rx_comp(rq, cqe);
		goto exit;
	}

	 /* Get vlan_tag value */
	if(IS_BE(sc) || IS_SH(sc))
		vtag = BSWAP_16(cqe->u0.s.vlan_tag);
	else
		vtag = cqe->u0.s.vlan_tag;


	for (i = 0; i < cqe->u0.s.num_fragments; i++) {

		if (rq->packets_out == rq->packets_in) {
			device_printf(sc->dev,
				  "RQ transmit descriptor missing\n");
		}
		out = rq->packets_out + 1;
		if (out == OCE_RQ_PACKET_ARRAY_SIZE)
			out = 0;
		pd = &rq->pckts[rq->packets_out];
		rq->packets_out = out;

		bus_dmamap_sync(rq->tag, pd->map, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(rq->tag, pd->map);
		rq->pending--;

		frag_len = (len > rq->cfg.frag_size) ? rq->cfg.frag_size : len;
		pd->mbuf->m_len = frag_len;

		if (tail != NULL) {
			/* additional fragments */
			pd->mbuf->m_flags &= ~M_PKTHDR;
			tail->m_next = pd->mbuf;
			tail = pd->mbuf;
		} else {
			/* first fragment, fill out much of the packet header */
			pd->mbuf->m_pkthdr.len = len;
			pd->mbuf->m_pkthdr.csum_flags = 0;
			if (IF_CSUM_ENABLED(sc)) {
				if (cqe->u0.s.l4_cksum_pass) {
					pd->mbuf->m_pkthdr.csum_flags |=
					    (CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
					pd->mbuf->m_pkthdr.csum_data = 0xffff;
				}
				if (cqe->u0.s.ip_cksum_pass) {
					if (!cqe->u0.s.ip_ver) { /* IPV4 */
						pd->mbuf->m_pkthdr.csum_flags |=
						(CSUM_IP_CHECKED|CSUM_IP_VALID);
					}
				}
			}
			m = tail = pd->mbuf;
		}
		pd->mbuf = NULL;
		len -= frag_len;
	}

	if (m) {
		if (!oce_cqe_portid_valid(sc, cqe)) {
			 m_freem(m);
			 goto exit;
		} 

		m->m_pkthdr.rcvif = sc->ifp;
#if __FreeBSD_version >= 800000
		if (rq->queue_index)
			m->m_pkthdr.flowid = (rq->queue_index - 1);
		else
			m->m_pkthdr.flowid = rq->queue_index;
		m->m_flags |= M_FLOWID;
#endif
		/* This deternies if vlan tag is Valid */
		if (oce_cqe_vtp_valid(sc, cqe)) { 
			if (sc->function_mode & FNM_FLEX10_MODE) {
				/* FLEX10. If QnQ is not set, neglect VLAN */
				if (cqe->u0.s.qnq) {
					m->m_pkthdr.ether_vtag = vtag;
					m->m_flags |= M_VLANTAG;
				}
			} else if (sc->pvid != (vtag & VLAN_VID_MASK))  {
				/* In UMC mode generally pvid will be striped by
				   hw. But in some cases we have seen it comes
				   with pvid. So if pvid == vlan, neglect vlan.
				*/
				m->m_pkthdr.ether_vtag = vtag;
				m->m_flags |= M_VLANTAG;
			}
		}

		sc->ifp->if_ipackets++;
#if defined(INET6) || defined(INET)
		/* Try to queue to LRO */
		if (IF_LRO_ENABLED(sc) &&
		    (cqe->u0.s.ip_cksum_pass) &&
		    (cqe->u0.s.l4_cksum_pass) &&
		    (!cqe->u0.s.ip_ver)       &&
		    (rq->lro.lro_cnt != 0)) {

			if (tcp_lro_rx(&rq->lro, m, 0) == 0) {
				rq->lro_pkts_queued ++;		
				goto post_done;
			}
			/* If LRO posting fails then try to post to STACK */
		}
#endif
	
		(*sc->ifp->if_input) (sc->ifp, m);
#if defined(INET6) || defined(INET)
post_done:
#endif
		/* Update rx stats per queue */
		rq->rx_stats.rx_pkts++;
		rq->rx_stats.rx_bytes += cqe->u0.s.pkt_size;
		rq->rx_stats.rx_frags += cqe->u0.s.num_fragments;
		if (cqe->u0.s.pkt_type == OCE_MULTICAST_PACKET)
			rq->rx_stats.rx_mcast_pkts++;
		if (cqe->u0.s.pkt_type == OCE_UNICAST_PACKET)
			rq->rx_stats.rx_ucast_pkts++;
	}
exit:
	return;
}


static void
oce_discard_rx_comp(struct oce_rq *rq, struct oce_nic_rx_cqe *cqe)
{
	uint32_t out, i = 0;
	struct oce_packet_desc *pd;
	POCE_SOFTC sc = (POCE_SOFTC) rq->parent;
	int num_frags = cqe->u0.s.num_fragments;

	for (i = 0; i < num_frags; i++) {
		if (rq->packets_out == rq->packets_in) {
			device_printf(sc->dev,
				"RQ transmit descriptor missing\n");
		}
		out = rq->packets_out + 1;
		if (out == OCE_RQ_PACKET_ARRAY_SIZE)
			out = 0;
		pd = &rq->pckts[rq->packets_out];
		rq->packets_out = out;

		bus_dmamap_sync(rq->tag, pd->map, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(rq->tag, pd->map);
		rq->pending--;
		m_freem(pd->mbuf);
	}

}


static int
oce_cqe_vtp_valid(POCE_SOFTC sc, struct oce_nic_rx_cqe *cqe)
{
	struct oce_nic_rx_cqe_v1 *cqe_v1;
	int vtp = 0;

	if (sc->be3_native) {
		cqe_v1 = (struct oce_nic_rx_cqe_v1 *)cqe;
		vtp =  cqe_v1->u0.s.vlan_tag_present; 
	} else
		vtp = cqe->u0.s.vlan_tag_present;
	
	return vtp;

}


static int
oce_cqe_portid_valid(POCE_SOFTC sc, struct oce_nic_rx_cqe *cqe)
{
	struct oce_nic_rx_cqe_v1 *cqe_v1;
	int port_id = 0;

	if (sc->be3_native && (IS_BE(sc) || IS_SH(sc))) {
		cqe_v1 = (struct oce_nic_rx_cqe_v1 *)cqe;
		port_id =  cqe_v1->u0.s.port;
		if (sc->port_id != port_id)
			return 0;
	} else
		;/* For BE3 legacy and Lancer this is dummy */
	
	return 1;

}

#if defined(INET6) || defined(INET)
static void
oce_rx_flush_lro(struct oce_rq *rq)
{
	struct lro_ctrl	*lro = &rq->lro;
	struct lro_entry *queued;
	POCE_SOFTC sc = (POCE_SOFTC) rq->parent;

	if (!IF_LRO_ENABLED(sc))
		return;

	while ((queued = SLIST_FIRST(&lro->lro_active)) != NULL) {
		SLIST_REMOVE_HEAD(&lro->lro_active, next);
		tcp_lro_flush(lro, queued);
	}
	rq->lro_pkts_queued = 0;
	
	return;
}


static int
oce_init_lro(POCE_SOFTC sc)
{
	struct lro_ctrl *lro = NULL;
	int i = 0, rc = 0;

	for (i = 0; i < sc->nrqs; i++) { 
		lro = &sc->rq[i]->lro;
		rc = tcp_lro_init(lro);
		if (rc != 0) {
			device_printf(sc->dev, "LRO init failed\n");
			return rc;		
		}
		lro->ifp = sc->ifp;
	}

	return rc;		
}


void
oce_free_lro(POCE_SOFTC sc)
{
	struct lro_ctrl *lro = NULL;
	int i = 0;

	for (i = 0; i < sc->nrqs; i++) {
		lro = &sc->rq[i]->lro;
		if (lro)
			tcp_lro_free(lro);
	}
}
#endif

int
oce_alloc_rx_bufs(struct oce_rq *rq, int count)
{
	POCE_SOFTC sc = (POCE_SOFTC) rq->parent;
	int i, in, rc;
	struct oce_packet_desc *pd;
	bus_dma_segment_t segs[6];
	int nsegs, added = 0;
	struct oce_nic_rqe *rqe;
	pd_rxulp_db_t rxdb_reg;

	bzero(&rxdb_reg, sizeof(pd_rxulp_db_t));
	for (i = 0; i < count; i++) {
		in = rq->packets_in + 1;
		if (in == OCE_RQ_PACKET_ARRAY_SIZE)
			in = 0;
		if (in == rq->packets_out)
			break;	/* no more room */

		pd = &rq->pckts[rq->packets_in];
		pd->mbuf = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
		if (pd->mbuf == NULL)
			break;

		pd->mbuf->m_len = pd->mbuf->m_pkthdr.len = MCLBYTES;
		rc = bus_dmamap_load_mbuf_sg(rq->tag,
					     pd->map,
					     pd->mbuf,
					     segs, &nsegs, BUS_DMA_NOWAIT);
		if (rc) {
			m_free(pd->mbuf);
			break;
		}

		if (nsegs != 1) {
			i--;
			continue;
		}

		rq->packets_in = in;
		bus_dmamap_sync(rq->tag, pd->map, BUS_DMASYNC_PREREAD);

		rqe = RING_GET_PRODUCER_ITEM_VA(rq->ring, struct oce_nic_rqe);
		rqe->u0.s.frag_pa_hi = ADDR_HI(segs[0].ds_addr);
		rqe->u0.s.frag_pa_lo = ADDR_LO(segs[0].ds_addr);
		DW_SWAP(u32ptr(rqe), sizeof(struct oce_nic_rqe));
		RING_PUT(rq->ring, 1);
		added++;
		rq->pending++;
	}
	if (added != 0) {
		for (i = added / OCE_MAX_RQ_POSTS; i > 0; i--) {
			rxdb_reg.bits.num_posted = OCE_MAX_RQ_POSTS;
			rxdb_reg.bits.qid = rq->rq_id;
			OCE_WRITE_REG32(sc, db, PD_RXULP_DB, rxdb_reg.dw0);
			added -= OCE_MAX_RQ_POSTS;
		}
		if (added > 0) {
			rxdb_reg.bits.qid = rq->rq_id;
			rxdb_reg.bits.num_posted = added;
			OCE_WRITE_REG32(sc, db, PD_RXULP_DB, rxdb_reg.dw0);
		}
	}
	
	return 0;	
}


/* Handle the Completion Queue for receive */
uint16_t
oce_rq_handler(void *arg)
{
	struct oce_rq *rq = (struct oce_rq *)arg;
	struct oce_cq *cq = rq->cq;
	POCE_SOFTC sc = rq->parent;
	struct oce_nic_rx_cqe *cqe;
	int num_cqes = 0, rq_buffers_used = 0;


	bus_dmamap_sync(cq->ring->dma.tag,
			cq->ring->dma.map, BUS_DMASYNC_POSTWRITE);
	cqe = RING_GET_CONSUMER_ITEM_VA(cq->ring, struct oce_nic_rx_cqe);
	while (cqe->u0.dw[2]) {
		DW_SWAP((uint32_t *) cqe, sizeof(oce_rq_cqe));

		RING_GET(rq->ring, 1);
		if (cqe->u0.s.error == 0) {
			oce_rx(rq, cqe->u0.s.frag_index, cqe);
		} else {
			rq->rx_stats.rxcp_err++;
			sc->ifp->if_ierrors++;
			/* Post L3/L4 errors to stack.*/
			oce_rx(rq, cqe->u0.s.frag_index, cqe);
		}
		rq->rx_stats.rx_compl++;
		cqe->u0.dw[2] = 0;

#if defined(INET6) || defined(INET)
		if (IF_LRO_ENABLED(sc) && rq->lro_pkts_queued >= 16) {
			oce_rx_flush_lro(rq);
		}
#endif

		RING_GET(cq->ring, 1);
		bus_dmamap_sync(cq->ring->dma.tag,
				cq->ring->dma.map, BUS_DMASYNC_POSTWRITE);
		cqe =
		    RING_GET_CONSUMER_ITEM_VA(cq->ring, struct oce_nic_rx_cqe);
		num_cqes++;
		if (num_cqes >= (IS_XE201(sc) ? 8 : oce_max_rsp_handled))
			break;
	}

#if defined(INET6) || defined(INET)
	if (IF_LRO_ENABLED(sc))
		oce_rx_flush_lro(rq);
#endif
	
	if (num_cqes) {
		oce_arm_cq(sc, cq->cq_id, num_cqes, FALSE);
		rq_buffers_used = OCE_RQ_PACKET_ARRAY_SIZE - rq->pending;
		if (rq_buffers_used > 1)
			oce_alloc_rx_bufs(rq, (rq_buffers_used - 1));
	}

	return 0;

}




/*****************************************************************************
 *		   Helper function prototypes in this file 		     *
 *****************************************************************************/

static int 
oce_attach_ifp(POCE_SOFTC sc)
{

	sc->ifp = if_alloc(IFT_ETHER);
	if (!sc->ifp)
		return ENOMEM;

	ifmedia_init(&sc->media, IFM_IMASK, oce_media_change, oce_media_status);
	ifmedia_add(&sc->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->media, IFM_ETHER | IFM_AUTO);

	sc->ifp->if_flags = IFF_BROADCAST | IFF_MULTICAST;
	sc->ifp->if_ioctl = oce_ioctl;
	sc->ifp->if_start = oce_start;
	sc->ifp->if_init = oce_init;
	sc->ifp->if_mtu = ETHERMTU;
	sc->ifp->if_softc = sc;
#if __FreeBSD_version >= 800000
	sc->ifp->if_transmit = oce_multiq_start;
	sc->ifp->if_qflush = oce_multiq_flush;
#endif

	if_initname(sc->ifp,
		    device_get_name(sc->dev), device_get_unit(sc->dev));

	sc->ifp->if_snd.ifq_drv_maxlen = OCE_MAX_TX_DESC - 1;
	IFQ_SET_MAXLEN(&sc->ifp->if_snd, sc->ifp->if_snd.ifq_drv_maxlen);
	IFQ_SET_READY(&sc->ifp->if_snd);

	sc->ifp->if_hwassist = OCE_IF_HWASSIST;
	sc->ifp->if_hwassist |= CSUM_TSO;
	sc->ifp->if_hwassist |= (CSUM_IP | CSUM_TCP | CSUM_UDP);

	sc->ifp->if_capabilities = OCE_IF_CAPABILITIES;
	sc->ifp->if_capabilities |= IFCAP_HWCSUM;
	sc->ifp->if_capabilities |= IFCAP_VLAN_HWFILTER;

#if defined(INET6) || defined(INET)
	sc->ifp->if_capabilities |= IFCAP_TSO;
	sc->ifp->if_capabilities |= IFCAP_LRO;
	sc->ifp->if_capabilities |= IFCAP_VLAN_HWTSO;
#endif
	
	sc->ifp->if_capenable = sc->ifp->if_capabilities;
	if_initbaudrate(sc->ifp, IF_Gbps(10));

	ether_ifattach(sc->ifp, sc->macaddr.mac_addr);
	
	return 0;
}


static void
oce_add_vlan(void *arg, struct ifnet *ifp, uint16_t vtag)
{
	POCE_SOFTC sc = ifp->if_softc;

	if (ifp->if_softc !=  arg)
		return;
	if ((vtag == 0) || (vtag > 4095))
		return;

	sc->vlan_tag[vtag] = 1;
	sc->vlans_added++;
	oce_vid_config(sc);
}


static void
oce_del_vlan(void *arg, struct ifnet *ifp, uint16_t vtag)
{
	POCE_SOFTC sc = ifp->if_softc;

	if (ifp->if_softc !=  arg)
		return;
	if ((vtag == 0) || (vtag > 4095))
		return;

	sc->vlan_tag[vtag] = 0;
	sc->vlans_added--;
	oce_vid_config(sc);
}


/*
 * A max of 64 vlans can be configured in BE. If the user configures
 * more, place the card in vlan promiscuous mode.
 */
static int
oce_vid_config(POCE_SOFTC sc)
{
	struct normal_vlan vtags[MAX_VLANFILTER_SIZE];
	uint16_t ntags = 0, i;
	int status = 0;

	if ((sc->vlans_added <= MAX_VLANFILTER_SIZE) && 
			(sc->ifp->if_capenable & IFCAP_VLAN_HWFILTER)) {
		for (i = 0; i < MAX_VLANS; i++) {
			if (sc->vlan_tag[i]) {
				vtags[ntags].vtag = i;
				ntags++;
			}
		}
		if (ntags)
			status = oce_config_vlan(sc, (uint8_t) sc->if_id,
						vtags, ntags, 1, 0); 
	} else 
		status = oce_config_vlan(sc, (uint8_t) sc->if_id,
					 	NULL, 0, 1, 1);
	return status;
}


static void
oce_mac_addr_set(POCE_SOFTC sc)
{
	uint32_t old_pmac_id = sc->pmac_id;
	int status = 0;

	
	status = bcmp((IF_LLADDR(sc->ifp)), sc->macaddr.mac_addr,
			 sc->macaddr.size_of_struct);
	if (!status)
		return;

	status = oce_mbox_macaddr_add(sc, (uint8_t *)(IF_LLADDR(sc->ifp)),
					sc->if_id, &sc->pmac_id);
	if (!status) {
		status = oce_mbox_macaddr_del(sc, sc->if_id, old_pmac_id);
		bcopy((IF_LLADDR(sc->ifp)), sc->macaddr.mac_addr,
				 sc->macaddr.size_of_struct); 
	}
	if (status)
		device_printf(sc->dev, "Failed update macaddress\n");

}


static int
oce_handle_passthrough(struct ifnet *ifp, caddr_t data)
{
	POCE_SOFTC sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int rc = ENXIO;
	char cookie[32] = {0};
	void *priv_data = (void *)ifr->ifr_data;
	void *ioctl_ptr;
	uint32_t req_size;
	struct mbx_hdr req;
	OCE_DMA_MEM dma_mem;
	struct mbx_common_get_cntl_attr *fw_cmd;

	if (copyin(priv_data, cookie, strlen(IOCTL_COOKIE)))
		return EFAULT;

	if (memcmp(cookie, IOCTL_COOKIE, strlen(IOCTL_COOKIE)))
		return EINVAL;

	ioctl_ptr = (char *)priv_data + strlen(IOCTL_COOKIE);
	if (copyin(ioctl_ptr, &req, sizeof(struct mbx_hdr)))
		return EFAULT;

	req_size = le32toh(req.u0.req.request_length);
	if (req_size > 65536)
		return EINVAL;

	req_size += sizeof(struct mbx_hdr);
	rc = oce_dma_alloc(sc, req_size, &dma_mem, 0);
	if (rc)
		return ENOMEM;

	if (copyin(ioctl_ptr, OCE_DMAPTR(&dma_mem,char), req_size)) {
		rc = EFAULT;
		goto dma_free;
	}

	rc = oce_pass_through_mbox(sc, &dma_mem, req_size);
	if (rc) {
		rc = EIO;
		goto dma_free;
	}

	if (copyout(OCE_DMAPTR(&dma_mem,char), ioctl_ptr, req_size))
		rc =  EFAULT;

	/* 
	   firmware is filling all the attributes for this ioctl except
	   the driver version..so fill it 
	 */
	if(req.u0.rsp.opcode == OPCODE_COMMON_GET_CNTL_ATTRIBUTES) {
		fw_cmd = (struct mbx_common_get_cntl_attr *) ioctl_ptr;
		strncpy(fw_cmd->params.rsp.cntl_attr_info.hba_attr.drv_ver_str,
			COMPONENT_REVISION, strlen(COMPONENT_REVISION));	
	}

dma_free:
	oce_dma_free(sc, &dma_mem);
	return rc;

}

static void
oce_eqd_set_periodic(POCE_SOFTC sc)
{
	struct oce_set_eqd set_eqd[OCE_MAX_EQ];
	struct oce_aic_obj *aic;
	struct oce_eq *eqo;
	uint64_t now = 0, delta;
	int eqd, i, num = 0;
	uint32_t ips = 0;
	int tps;

	for (i = 0 ; i < sc->neqs; i++) {
		eqo = sc->eq[i];
		aic = &sc->aic_obj[i];
		/* When setting the static eq delay from the user space */
		if (!aic->enable) {
			eqd = aic->et_eqd;
			goto modify_eqd;
		}

		now = ticks;

		/* Over flow check */
		if ((now < aic->ticks) || (eqo->intr < aic->intr_prev))
			goto done;

		delta = now - aic->ticks;
		tps = delta/hz;

		/* Interrupt rate based on elapsed ticks */
		if(tps)
			ips = (uint32_t)(eqo->intr - aic->intr_prev) / tps;

		if (ips > INTR_RATE_HWM)
			eqd = aic->cur_eqd + 20;
		else if (ips < INTR_RATE_LWM)
			eqd = aic->cur_eqd / 2;
		else
			goto done;

		if (eqd < 10)
			eqd = 0;

		/* Make sure that the eq delay is in the known range */
		eqd = min(eqd, aic->max_eqd);
		eqd = max(eqd, aic->min_eqd);

modify_eqd:
		if (eqd != aic->cur_eqd) {
			set_eqd[num].delay_multiplier = (eqd * 65)/100;
			set_eqd[num].eq_id = eqo->eq_id;
			aic->cur_eqd = eqd;
			num++;
		}
done:
		aic->intr_prev = eqo->intr;
		aic->ticks = now;
	}

	/* Is there atleast one eq that needs to be modified? */
	if(num)
		oce_mbox_eqd_modify_periodic(sc, set_eqd, num);

}

static void
oce_local_timer(void *arg)
{
	POCE_SOFTC sc = arg;
	int i = 0;
	
	oce_refresh_nic_stats(sc);
	oce_refresh_queue_stats(sc);
	oce_mac_addr_set(sc);
	
	/* TX Watch Dog*/
	for (i = 0; i < sc->nwqs; i++)
		oce_tx_restart(sc, sc->wq[i]);
	
	/* calculate and set the eq delay for optimal interrupt rate */
	if (IS_BE(sc) || IS_SH(sc))
		oce_eqd_set_periodic(sc);

	callout_reset(&sc->timer, hz, oce_local_timer, sc);
}


/* NOTE : This should only be called holding
 *        DEVICE_LOCK.
*/
static void
oce_if_deactivate(POCE_SOFTC sc)
{
	int i, mtime = 0;
	int wait_req = 0;
	struct oce_rq *rq;
	struct oce_wq *wq;
	struct oce_eq *eq;

	sc->ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	/*Wait for max of 400ms for TX completions to be done */
	while (mtime < 400) {
		wait_req = 0;
		for_all_wq_queues(sc, wq, i) {
			if (wq->ring->num_used) {
				wait_req = 1;
				DELAY(1);
				break;
			}
		}
		mtime += 1;
		if (!wait_req)
			break;
	}

	/* Stop intrs and finish any bottom halves pending */
	oce_hw_intr_disable(sc);

	/* Since taskqueue_drain takes a Gaint Lock, We should not acquire
	   any other lock. So unlock device lock and require after
	   completing taskqueue_drain.
	*/
	UNLOCK(&sc->dev_lock);
	for (i = 0; i < sc->intr_count; i++) {
		if (sc->intrs[i].tq != NULL) {
			taskqueue_drain(sc->intrs[i].tq, &sc->intrs[i].task);
		}
	}
	LOCK(&sc->dev_lock);

	/* Delete RX queue in card with flush param */
	oce_stop_rx(sc);

	/* Invalidate any pending cq and eq entries*/	
	for_all_evnt_queues(sc, eq, i)	
		oce_drain_eq(eq);
	for_all_rq_queues(sc, rq, i)
		oce_drain_rq_cq(rq);
	for_all_wq_queues(sc, wq, i)
		oce_drain_wq_cq(wq);

	/* But still we need to get MCC aync events.
	   So enable intrs and also arm first EQ
	*/
	oce_hw_intr_enable(sc);
	oce_arm_eq(sc, sc->eq[0]->eq_id, 0, TRUE, FALSE);

	DELAY(10);
}


static void
oce_if_activate(POCE_SOFTC sc)
{
	struct oce_eq *eq;
	struct oce_rq *rq;
	struct oce_wq *wq;
	int i, rc = 0;

	sc->ifp->if_drv_flags |= IFF_DRV_RUNNING; 
	
	oce_hw_intr_disable(sc);
	
	oce_start_rx(sc);

	for_all_rq_queues(sc, rq, i) {
		rc = oce_start_rq(rq);
		if (rc)
			device_printf(sc->dev, "Unable to start RX\n");
	}

	for_all_wq_queues(sc, wq, i) {
		rc = oce_start_wq(wq);
		if (rc)
			device_printf(sc->dev, "Unable to start TX\n");
	}

	
	for_all_evnt_queues(sc, eq, i)
		oce_arm_eq(sc, eq->eq_id, 0, TRUE, FALSE);

	oce_hw_intr_enable(sc);

}

static void
process_link_state(POCE_SOFTC sc, struct oce_async_cqe_link_state *acqe)
{
	/* Update Link status */
	if ((acqe->u0.s.link_status & ~ASYNC_EVENT_LOGICAL) ==
	     ASYNC_EVENT_LINK_UP) {
		sc->link_status = ASYNC_EVENT_LINK_UP;
		if_link_state_change(sc->ifp, LINK_STATE_UP);
	} else {
		sc->link_status = ASYNC_EVENT_LINK_DOWN;
		if_link_state_change(sc->ifp, LINK_STATE_DOWN);
	}

	/* Update speed */
	sc->link_speed = acqe->u0.s.speed;
	sc->qos_link_speed = (uint32_t) acqe->u0.s.qos_link_speed * 10;

}


/* Handle the Completion Queue for the Mailbox/Async notifications */
uint16_t
oce_mq_handler(void *arg)
{
	struct oce_mq *mq = (struct oce_mq *)arg;
	POCE_SOFTC sc = mq->parent;
	struct oce_cq *cq = mq->cq;
	int num_cqes = 0, evt_type = 0, optype = 0;
	struct oce_mq_cqe *cqe;
	struct oce_async_cqe_link_state *acqe;
	struct oce_async_event_grp5_pvid_state *gcqe;
	struct oce_async_event_qnq *dbgcqe;


	bus_dmamap_sync(cq->ring->dma.tag,
			cq->ring->dma.map, BUS_DMASYNC_POSTWRITE);
	cqe = RING_GET_CONSUMER_ITEM_VA(cq->ring, struct oce_mq_cqe);

	while (cqe->u0.dw[3]) {
		DW_SWAP((uint32_t *) cqe, sizeof(oce_mq_cqe));
		if (cqe->u0.s.async_event) {
			evt_type = cqe->u0.s.event_type;
			optype = cqe->u0.s.async_type;
			if (evt_type  == ASYNC_EVENT_CODE_LINK_STATE) {
				/* Link status evt */
				acqe = (struct oce_async_cqe_link_state *)cqe;
				process_link_state(sc, acqe);
			} else if ((evt_type == ASYNC_EVENT_GRP5) &&
				   (optype == ASYNC_EVENT_PVID_STATE)) {
				/* GRP5 PVID */
				gcqe = 
				(struct oce_async_event_grp5_pvid_state *)cqe;
				if (gcqe->enabled)
					sc->pvid = gcqe->tag & VLAN_VID_MASK;
				else
					sc->pvid = 0;
				
			}
			else if(evt_type == ASYNC_EVENT_CODE_DEBUG &&
				optype == ASYNC_EVENT_DEBUG_QNQ) {
				dbgcqe = 
				(struct oce_async_event_qnq *)cqe;
				if(dbgcqe->valid)
					sc->qnqid = dbgcqe->vlan_tag;
				sc->qnq_debug_event = TRUE;
			}
		}
		cqe->u0.dw[3] = 0;
		RING_GET(cq->ring, 1);
		bus_dmamap_sync(cq->ring->dma.tag,
				cq->ring->dma.map, BUS_DMASYNC_POSTWRITE);
		cqe = RING_GET_CONSUMER_ITEM_VA(cq->ring, struct oce_mq_cqe);
		num_cqes++;
	}

	if (num_cqes)
		oce_arm_cq(sc, cq->cq_id, num_cqes, FALSE);

	return 0;
}


static void
setup_max_queues_want(POCE_SOFTC sc)
{
	/* Check if it is FLEX machine. Is so dont use RSS */	
	if ((sc->function_mode & FNM_FLEX10_MODE) ||
	    (sc->function_mode & FNM_UMC_MODE)    ||
	    (sc->function_mode & FNM_VNIC_MODE)	  ||
	    (!is_rss_enabled(sc))		  ||
	    (sc->flags & OCE_FLAGS_BE2)) {
		sc->nrqs = 1;
		sc->nwqs = 1;
	}
}


static void
update_queues_got(POCE_SOFTC sc)
{
	if (is_rss_enabled(sc)) {
		sc->nrqs = sc->intr_count + 1;
		sc->nwqs = sc->intr_count;
	} else {
		sc->nrqs = 1;
		sc->nwqs = 1;
	}
}

static int 
oce_check_ipv6_ext_hdr(struct mbuf *m)
{
	struct ether_header *eh = mtod(m, struct ether_header *);
	caddr_t m_datatemp = m->m_data;

	if (eh->ether_type == htons(ETHERTYPE_IPV6)) {
		m->m_data += sizeof(struct ether_header);
		struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);

		if((ip6->ip6_nxt != IPPROTO_TCP) && \
				(ip6->ip6_nxt != IPPROTO_UDP)){
			struct ip6_ext *ip6e = NULL;
			m->m_data += sizeof(struct ip6_hdr);

			ip6e = (struct ip6_ext *) mtod(m, struct ip6_ext *);
			if(ip6e->ip6e_len == 0xff) {
				m->m_data = m_datatemp;
				return TRUE;
			}
		} 
		m->m_data = m_datatemp;
	}
	return FALSE;
}

static int 
is_be3_a1(POCE_SOFTC sc)
{
	if((sc->flags & OCE_FLAGS_BE3)  && ((sc->asic_revision & 0xFF) < 2)) {
		return TRUE;
	}
	return FALSE;
}

static struct mbuf *
oce_insert_vlan_tag(POCE_SOFTC sc, struct mbuf *m, boolean_t *complete)
{
	uint16_t vlan_tag = 0;

	if(!M_WRITABLE(m))
		return NULL;

	/* Embed vlan tag in the packet if it is not part of it */
	if(m->m_flags & M_VLANTAG) {
		vlan_tag = EVL_VLANOFTAG(m->m_pkthdr.ether_vtag);
		m->m_flags &= ~M_VLANTAG;
	}

	/* if UMC, ignore vlan tag insertion and instead insert pvid */
	if(sc->pvid) {
		if(!vlan_tag)
			vlan_tag = sc->pvid;
		*complete = FALSE;
	}

	if(vlan_tag) {
		m = ether_vlanencap(m, vlan_tag);
	}

	if(sc->qnqid) {
		m = ether_vlanencap(m, sc->qnqid);
		*complete = FALSE;
	}
	return m;
}

static int 
oce_tx_asic_stall_verify(POCE_SOFTC sc, struct mbuf *m)
{
	if(is_be3_a1(sc) && IS_QNQ_OR_UMC(sc) && \
			oce_check_ipv6_ext_hdr(m)) {
		return TRUE;
	}
	return FALSE;
}

static void
oce_get_config(POCE_SOFTC sc)
{
	int rc = 0;
	uint32_t max_rss = 0;

	if ((IS_BE(sc) || IS_SH(sc)) && (!sc->be3_native))
		max_rss = OCE_LEGACY_MODE_RSS;
	else
		max_rss = OCE_MAX_RSS;

	if (!IS_BE(sc)) {
		rc = oce_get_func_config(sc);
		if (rc) {
			sc->nwqs = OCE_MAX_WQ;
			sc->nrssqs = max_rss;
			sc->nrqs = sc->nrssqs + 1;
		}
	}
	else {
		rc = oce_get_profile_config(sc);
		sc->nrssqs = max_rss;
		sc->nrqs = sc->nrssqs + 1;
		if (rc)
			sc->nwqs = OCE_MAX_WQ;
	}
}
