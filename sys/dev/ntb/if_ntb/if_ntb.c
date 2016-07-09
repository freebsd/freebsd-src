/*-
 * Copyright (c) 2016 Alexander Motin <mav@FreeBSD.org>
 * Copyright (C) 2013 Intel Corporation
 * Copyright (C) 2015 EMC Corporation
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

/*
 * The Non-Transparent Bridge (NTB) is a device that allows you to connect
 * two or more systems using a PCI-e links, providing remote memory access.
 *
 * This module contains a driver for simulated Ethernet device, using
 * underlying NTB Transport device.
 *
 * NOTE: Much of the code in this module is shared with Linux. Any patches may
 * be picked up and redistributed in Linux with a dual GPL/BSD license.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/limits.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/bpf.h>
#include <net/ethernet.h>

#include <machine/bus.h>

#include "../ntb_transport.h"

#define KTR_NTB KTR_SPARE3

struct ntb_net_ctx {
	device_t		*dev;
	struct ifnet		*ifp;
	struct ntb_transport_qp *qp;
	u_char			eaddr[ETHER_ADDR_LEN];
	struct mtx		tx_lock;
	struct callout		queue_full;
};

static int ntb_net_probe(device_t dev);
static int ntb_net_attach(device_t dev);
static int ntb_net_detach(device_t dev);
static void ntb_net_init(void *arg);
static int ntb_ioctl(struct ifnet *ifp, u_long command, caddr_t data);
static void ntb_start(struct ifnet *ifp);
static void ntb_net_tx_handler(struct ntb_transport_qp *qp, void *qp_data,
    void *data, int len);
static void ntb_net_rx_handler(struct ntb_transport_qp *qp, void *qp_data,
    void *data, int len);
static void ntb_net_event_handler(void *data, enum ntb_link_event status);
static void ntb_qp_full(void *arg);
static void create_random_local_eui48(u_char *eaddr);

static int
ntb_net_probe(device_t dev)
{

	device_set_desc(dev, "NTB Network Interface");
	return (0);
}

static int
ntb_net_attach(device_t dev)
{
	struct ntb_net_ctx *sc = device_get_softc(dev);
	struct ifnet *ifp;
	struct ntb_queue_handlers handlers = { ntb_net_rx_handler,
	    ntb_net_tx_handler, ntb_net_event_handler };

	ifp = sc->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		printf("ntb: Cannot allocate ifnet structure\n");
		return (ENOMEM);
	}
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));

	mtx_init(&sc->tx_lock, "ntb tx", NULL, MTX_DEF);
	callout_init(&sc->queue_full, 1);

	sc->qp = ntb_transport_create_queue(ifp, device_get_parent(dev),
	    &handlers);
	ifp->if_init = ntb_net_init;
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = ntb_ioctl;
	ifp->if_start = ntb_start;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);
	create_random_local_eui48(sc->eaddr);
	ether_ifattach(ifp, sc->eaddr);
	ifp->if_capabilities = IFCAP_HWCSUM | IFCAP_JUMBO_MTU;
	ifp->if_capenable = ifp->if_capabilities;
	ifp->if_mtu = ntb_transport_max_size(sc->qp) - ETHER_HDR_LEN -
	    ETHER_CRC_LEN;

	ntb_transport_link_up(sc->qp);
	return (0);
}

static int
ntb_net_detach(device_t dev)
{
	struct ntb_net_ctx *sc = device_get_softc(dev);

	if (sc->qp != NULL) {
		ntb_transport_link_down(sc->qp);
		ntb_transport_free_queue(sc->qp);
	}

	if (sc->ifp != NULL) {
		ether_ifdetach(sc->ifp);
		if_free(sc->ifp);
		sc->ifp = NULL;
	}
	mtx_destroy(&sc->tx_lock);

	return (0);
}

/* Network device interface */

static void
ntb_net_init(void *arg)
{
	struct ntb_net_ctx *sc = arg;
	struct ifnet *ifp = sc->ifp;

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	ifp->if_flags |= IFF_UP;
	if_link_state_change(ifp, LINK_STATE_UP);
}

static int
ntb_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct ntb_net_ctx *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int error = 0;

	switch (command) {
	case SIOCSIFMTU:
	    {
		if (ifr->ifr_mtu > ntb_transport_max_size(sc->qp) -
		    ETHER_HDR_LEN - ETHER_CRC_LEN) {
			error = EINVAL;
			break;
		}

		ifp->if_mtu = ifr->ifr_mtu;
		break;
	    }
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return (error);
}


static void
ntb_start(struct ifnet *ifp)
{
	struct mbuf *m_head;
	struct ntb_net_ctx *sc = ifp->if_softc;
	int rc;

	mtx_lock(&sc->tx_lock);
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	CTR0(KTR_NTB, "TX: ntb_start");
	while (!IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		CTR1(KTR_NTB, "TX: start mbuf %p", m_head);
		rc = ntb_transport_tx_enqueue(sc->qp, m_head, m_head,
			     m_length(m_head, NULL));
		if (rc != 0) {
			CTR1(KTR_NTB,
			    "TX: could not tx mbuf %p. Returning to snd q",
			    m_head);
			if (rc == EAGAIN) {
				ifp->if_drv_flags |= IFF_DRV_OACTIVE;
				IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
				callout_reset(&sc->queue_full, hz / 1000,
				    ntb_qp_full, ifp);
			}
			break;
		}
	}
	mtx_unlock(&sc->tx_lock);
}

/* Network Device Callbacks */
static void
ntb_net_tx_handler(struct ntb_transport_qp *qp, void *qp_data, void *data,
    int len)
{

	m_freem(data);
	CTR1(KTR_NTB, "TX: tx_handler freeing mbuf %p", data);
}

static void
ntb_net_rx_handler(struct ntb_transport_qp *qp, void *qp_data, void *data,
    int len)
{
	struct mbuf *m = data;
	struct ifnet *ifp = qp_data;

	CTR0(KTR_NTB, "RX: rx handler");
	m->m_pkthdr.csum_flags = CSUM_IP_CHECKED | CSUM_IP_VALID;
	(*ifp->if_input)(ifp, m);
}

static void
ntb_net_event_handler(void *data, enum ntb_link_event status)
{
	struct ifnet *ifp;

	ifp = data;
	(void)ifp;

	/* XXX The Linux driver munges with the carrier status here. */

	switch (status) {
	case NTB_LINK_DOWN:
		break;
	case NTB_LINK_UP:
		break;
	default:
		panic("Bogus ntb_link_event %u\n", status);
	}
}

static void
ntb_qp_full(void *arg)
{

	CTR0(KTR_NTB, "TX: qp_full callout");
	ntb_start(arg);
}

/* Helper functions */
/* TODO: This too should really be part of the kernel */
#define EUI48_MULTICAST			1 << 0
#define EUI48_LOCALLY_ADMINISTERED	1 << 1
static void
create_random_local_eui48(u_char *eaddr)
{
	static uint8_t counter = 0;
	uint32_t seed = ticks;

	eaddr[0] = EUI48_LOCALLY_ADMINISTERED;
	memcpy(&eaddr[1], &seed, sizeof(uint32_t));
	eaddr[5] = counter++;
}

static device_method_t ntb_net_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,     ntb_net_probe),
	DEVMETHOD(device_attach,    ntb_net_attach),
	DEVMETHOD(device_detach,    ntb_net_detach),
	DEVMETHOD_END
};

devclass_t ntb_net_devclass;
static DEFINE_CLASS_0(ntb, ntb_net_driver, ntb_net_methods,
    sizeof(struct ntb_net_ctx));
DRIVER_MODULE(if_ntb, ntb_transport, ntb_net_driver, ntb_net_devclass,
    NULL, NULL);
MODULE_DEPEND(if_ntb, ntb_transport, 1, 1, 1);
MODULE_VERSION(if_ntb, 1);
