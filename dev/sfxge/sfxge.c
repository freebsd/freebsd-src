/*-
 * Copyright (c) 2010-2011 Solarflare Communications, Inc.
 * All rights reserved.
 *
 * This software was developed in part by Philip Paeps under contract for
 * Solarflare Communications, Inc.
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
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/taskqueue.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include "common/efx.h"

#include "sfxge.h"
#include "sfxge_rx.h"

#define	SFXGE_CAP (IFCAP_VLAN_MTU | \
		   IFCAP_HWCSUM | IFCAP_VLAN_HWCSUM | IFCAP_TSO |	\
		   IFCAP_JUMBO_MTU | IFCAP_LRO |			\
		   IFCAP_VLAN_HWTSO | IFCAP_LINKSTATE)
#define	SFXGE_CAP_ENABLE SFXGE_CAP
#define	SFXGE_CAP_FIXED (IFCAP_VLAN_MTU | IFCAP_HWCSUM | IFCAP_VLAN_HWCSUM | \
			 IFCAP_JUMBO_MTU | IFCAP_LINKSTATE)

MALLOC_DEFINE(M_SFXGE, "sfxge", "Solarflare 10GigE driver");


SYSCTL_NODE(_hw, OID_AUTO, sfxge, CTLFLAG_RD, 0,
	    "SFXGE driver parameters");

#define	SFXGE_PARAM_RX_RING	SFXGE_PARAM(rx_ring)
static int sfxge_rx_ring_entries = SFXGE_NDESCS;
TUNABLE_INT(SFXGE_PARAM_RX_RING, &sfxge_rx_ring_entries);
SYSCTL_INT(_hw_sfxge, OID_AUTO, rx_ring, CTLFLAG_RDTUN,
	   &sfxge_rx_ring_entries, 0,
	   "Maximum number of descriptors in a receive ring");

#define	SFXGE_PARAM_TX_RING	SFXGE_PARAM(tx_ring)
static int sfxge_tx_ring_entries = SFXGE_NDESCS;
TUNABLE_INT(SFXGE_PARAM_TX_RING, &sfxge_tx_ring_entries);
SYSCTL_INT(_hw_sfxge, OID_AUTO, tx_ring, CTLFLAG_RDTUN,
	   &sfxge_tx_ring_entries, 0,
	   "Maximum number of descriptors in a transmit ring");


static void
sfxge_reset(void *arg, int npending);

static int
sfxge_start(struct sfxge_softc *sc)
{
	int rc;

	sx_assert(&sc->softc_lock, LA_XLOCKED);

	if (sc->init_state == SFXGE_STARTED)
		return (0);

	if (sc->init_state != SFXGE_REGISTERED) {
		rc = EINVAL;
		goto fail;
	}

	if ((rc = efx_nic_init(sc->enp)) != 0)
		goto fail;

	/* Start processing interrupts. */
	if ((rc = sfxge_intr_start(sc)) != 0)
		goto fail2;

	/* Start processing events. */
	if ((rc = sfxge_ev_start(sc)) != 0)
		goto fail3;

	/* Start the receiver side. */
	if ((rc = sfxge_rx_start(sc)) != 0)
		goto fail4;

	/* Start the transmitter side. */
	if ((rc = sfxge_tx_start(sc)) != 0)
		goto fail5;

	/* Fire up the port. */
	if ((rc = sfxge_port_start(sc)) != 0)
		goto fail6;

	sc->init_state = SFXGE_STARTED;

	/* Tell the stack we're running. */
	sc->ifnet->if_drv_flags |= IFF_DRV_RUNNING;
	sc->ifnet->if_drv_flags &= ~IFF_DRV_OACTIVE;

	return (0);

fail6:
	sfxge_tx_stop(sc);

fail5:
	sfxge_rx_stop(sc);

fail4:
	sfxge_ev_stop(sc);

fail3:
	sfxge_intr_stop(sc);

fail2:
	efx_nic_fini(sc->enp);

fail:
	device_printf(sc->dev, "sfxge_start: %d\n", rc);

	return (rc);
}

static void
sfxge_if_init(void *arg)
{
	struct sfxge_softc *sc;

	sc = (struct sfxge_softc *)arg;

	sx_xlock(&sc->softc_lock);
	(void)sfxge_start(sc);
	sx_xunlock(&sc->softc_lock);
}

static void
sfxge_stop(struct sfxge_softc *sc)
{
	sx_assert(&sc->softc_lock, LA_XLOCKED);

	if (sc->init_state != SFXGE_STARTED)
		return;

	sc->init_state = SFXGE_REGISTERED;

	/* Stop the port. */
	sfxge_port_stop(sc);

	/* Stop the transmitter. */
	sfxge_tx_stop(sc);

	/* Stop the receiver. */
	sfxge_rx_stop(sc);

	/* Stop processing events. */
	sfxge_ev_stop(sc);

	/* Stop processing interrupts. */
	sfxge_intr_stop(sc);

	efx_nic_fini(sc->enp);

	sc->ifnet->if_drv_flags &= ~IFF_DRV_RUNNING;
}

static int
sfxge_if_ioctl(struct ifnet *ifp, unsigned long command, caddr_t data)
{
	struct sfxge_softc *sc;
	struct ifreq *ifr;
	int error;

	ifr = (struct ifreq *)data;
	sc = ifp->if_softc;
	error = 0;

	switch (command) {
	case SIOCSIFFLAGS:
		sx_xlock(&sc->softc_lock);
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				if ((ifp->if_flags ^ sc->if_flags) &
				    (IFF_PROMISC | IFF_ALLMULTI)) {
					sfxge_mac_filter_set(sc);
				}
			} else
				sfxge_start(sc);
		} else
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				sfxge_stop(sc);
		sc->if_flags = ifp->if_flags;
		sx_xunlock(&sc->softc_lock);
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu == ifp->if_mtu) {
			/* Nothing to do */
			error = 0;
		} else if (ifr->ifr_mtu > SFXGE_MAX_MTU) {
			error = EINVAL;
		} else if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
			ifp->if_mtu = ifr->ifr_mtu;
			error = 0;
		} else {
			/* Restart required */
			sx_xlock(&sc->softc_lock);
			sfxge_stop(sc);
			ifp->if_mtu = ifr->ifr_mtu;
			error = sfxge_start(sc);
			sx_xunlock(&sc->softc_lock);
			if (error != 0) {
				ifp->if_flags &= ~IFF_UP;
				ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
				if_down(ifp);
			}
		}
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			sfxge_mac_filter_set(sc);
		break;
	case SIOCSIFCAP:
		sx_xlock(&sc->softc_lock);

		/*
		 * The networking core already rejects attempts to
		 * enable capabilities we don't have.  We still have
		 * to reject attempts to disable capabilities that we
		 * can't (yet) disable.
		 */
		if (~ifr->ifr_reqcap & SFXGE_CAP_FIXED) {
			error = EINVAL;
			sx_xunlock(&sc->softc_lock);
			break;
		}

		ifp->if_capenable = ifr->ifr_reqcap;
		if (ifp->if_capenable & IFCAP_TXCSUM)
			ifp->if_hwassist |= (CSUM_IP | CSUM_TCP | CSUM_UDP);
		else
			ifp->if_hwassist &= ~(CSUM_IP | CSUM_TCP | CSUM_UDP);
		if (ifp->if_capenable & IFCAP_TSO)
			ifp->if_hwassist |= CSUM_TSO;
		else
			ifp->if_hwassist &= ~CSUM_TSO;

		sx_xunlock(&sc->softc_lock);
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->media, command);
		break;
	default:
		error = ether_ioctl(ifp, command, data);
	}

	return (error);
}

static void
sfxge_ifnet_fini(struct ifnet *ifp)
{
	struct sfxge_softc *sc = ifp->if_softc;

	sx_xlock(&sc->softc_lock);
	sfxge_stop(sc);
	sx_xunlock(&sc->softc_lock);

	ifmedia_removeall(&sc->media);
	ether_ifdetach(ifp);
	if_free(ifp);
}

static int
sfxge_ifnet_init(struct ifnet *ifp, struct sfxge_softc *sc)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sc->enp);
	device_t dev;
	int rc;

	dev = sc->dev;
	sc->ifnet = ifp;

	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_init = sfxge_if_init;
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = sfxge_if_ioctl;

	ifp->if_capabilities = SFXGE_CAP;
	ifp->if_capenable = SFXGE_CAP_ENABLE;
	ifp->if_hwassist = CSUM_TCP | CSUM_UDP | CSUM_IP | CSUM_TSO;

	ether_ifattach(ifp, encp->enc_mac_addr);

#ifdef SFXGE_HAVE_MQ
	ifp->if_transmit = sfxge_if_transmit;
	ifp->if_qflush = sfxge_if_qflush;
#else
	ifp->if_start = sfxge_if_start;
	IFQ_SET_MAXLEN(&ifp->if_snd, sc->txq_entries - 1);
	ifp->if_snd.ifq_drv_maxlen = sc->txq_entries - 1;
	IFQ_SET_READY(&ifp->if_snd);

	mtx_init(&sc->tx_lock, "txq", NULL, MTX_DEF);
#endif

	if ((rc = sfxge_port_ifmedia_init(sc)) != 0)
		goto fail;

	return (0);

fail:
	ether_ifdetach(sc->ifnet);
	return (rc);
}

void
sfxge_sram_buf_tbl_alloc(struct sfxge_softc *sc, size_t n, uint32_t *idp)
{
	KASSERT(sc->buffer_table_next + n <=
		efx_nic_cfg_get(sc->enp)->enc_buftbl_limit,
		("buffer table full"));

	*idp = sc->buffer_table_next;
	sc->buffer_table_next += n;
}

static int
sfxge_bar_init(struct sfxge_softc *sc)
{
	efsys_bar_t *esbp = &sc->bar;

	esbp->esb_rid = PCIR_BAR(EFX_MEM_BAR);
	if ((esbp->esb_res = bus_alloc_resource_any(sc->dev, SYS_RES_MEMORY,
	    &esbp->esb_rid, RF_ACTIVE)) == NULL) {
		device_printf(sc->dev, "Cannot allocate BAR region %d\n",
		    EFX_MEM_BAR);
		return (ENXIO);
	}
	esbp->esb_tag = rman_get_bustag(esbp->esb_res);
	esbp->esb_handle = rman_get_bushandle(esbp->esb_res);
	mtx_init(&esbp->esb_lock, "sfxge_efsys_bar", NULL, MTX_DEF);

	return (0);
}

static void
sfxge_bar_fini(struct sfxge_softc *sc)
{
	efsys_bar_t *esbp = &sc->bar;

	bus_release_resource(sc->dev, SYS_RES_MEMORY, esbp->esb_rid,
	    esbp->esb_res);
	mtx_destroy(&esbp->esb_lock);
}

static int
sfxge_create(struct sfxge_softc *sc)
{
	device_t dev;
	efx_nic_t *enp;
	int error;

	dev = sc->dev;

	sx_init(&sc->softc_lock, "sfxge_softc");

	sc->stats_node = SYSCTL_ADD_NODE(
		device_get_sysctl_ctx(dev),
		SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
		OID_AUTO, "stats", CTLFLAG_RD, NULL, "Statistics");
	if (sc->stats_node == NULL) {
		error = ENOMEM;
		goto fail;
	}

	TASK_INIT(&sc->task_reset, 0, sfxge_reset, sc);

	(void) pci_enable_busmaster(dev);

	/* Initialize DMA mappings. */
	if ((error = sfxge_dma_init(sc)) != 0)
		goto fail;

	/* Map the device registers. */
	if ((error = sfxge_bar_init(sc)) != 0)
		goto fail;

	error = efx_family(pci_get_vendor(dev), pci_get_device(dev),
	    &sc->family);
	KASSERT(error == 0, ("Family should be filtered by sfxge_probe()"));

	/* Create the common code nic object. */
	mtx_init(&sc->enp_lock, "sfxge_nic", NULL, MTX_DEF);
	if ((error = efx_nic_create(sc->family, (efsys_identifier_t *)sc,
	    &sc->bar, &sc->enp_lock, &enp)) != 0)
		goto fail3;
	sc->enp = enp;

	if (!ISP2(sfxge_rx_ring_entries) ||
	    !(sfxge_rx_ring_entries & EFX_RXQ_NDESCS_MASK)) {
		log(LOG_ERR, "%s=%d must be power of 2 from %u to %u",
		    SFXGE_PARAM_RX_RING, sfxge_rx_ring_entries,
		    EFX_RXQ_MINNDESCS, EFX_RXQ_MAXNDESCS);
		error = EINVAL;
		goto fail_rx_ring_entries;
	}
	sc->rxq_entries = sfxge_rx_ring_entries;

	if (!ISP2(sfxge_tx_ring_entries) ||
	    !(sfxge_tx_ring_entries & EFX_TXQ_NDESCS_MASK)) {
		log(LOG_ERR, "%s=%d must be power of 2 from %u to %u",
		    SFXGE_PARAM_TX_RING, sfxge_tx_ring_entries,
		    EFX_TXQ_MINNDESCS, EFX_TXQ_MAXNDESCS);
		error = EINVAL;
		goto fail_tx_ring_entries;
	}
	sc->txq_entries = sfxge_tx_ring_entries;

	/* Initialize MCDI to talk to the microcontroller. */
	if ((error = sfxge_mcdi_init(sc)) != 0)
		goto fail4;

	/* Probe the NIC and build the configuration data area. */
	if ((error = efx_nic_probe(enp)) != 0)
		goto fail5;

	/* Initialize the NVRAM. */
	if ((error = efx_nvram_init(enp)) != 0)
		goto fail6;

	/* Initialize the VPD. */
	if ((error = efx_vpd_init(enp)) != 0)
		goto fail7;

	/* Reset the NIC. */
	if ((error = efx_nic_reset(enp)) != 0)
		goto fail8;

	/* Initialize buffer table allocation. */
	sc->buffer_table_next = 0;

	/* Set up interrupts. */
	if ((error = sfxge_intr_init(sc)) != 0)
		goto fail8;

	/* Initialize event processing state. */
	if ((error = sfxge_ev_init(sc)) != 0)
		goto fail11;

	/* Initialize receive state. */
	if ((error = sfxge_rx_init(sc)) != 0)
		goto fail12;

	/* Initialize transmit state. */
	if ((error = sfxge_tx_init(sc)) != 0)
		goto fail13;

	/* Initialize port state. */
	if ((error = sfxge_port_init(sc)) != 0)
		goto fail14;

	sc->init_state = SFXGE_INITIALIZED;

	return (0);

fail14:
	sfxge_tx_fini(sc);

fail13:
	sfxge_rx_fini(sc);

fail12:
	sfxge_ev_fini(sc);

fail11:
	sfxge_intr_fini(sc);

fail8:
	efx_vpd_fini(enp);

fail7:
	efx_nvram_fini(enp);

fail6:
	efx_nic_unprobe(enp);

fail5:
	sfxge_mcdi_fini(sc);

fail4:
fail_tx_ring_entries:
fail_rx_ring_entries:
	sc->enp = NULL;
	efx_nic_destroy(enp);
	mtx_destroy(&sc->enp_lock);

fail3:
	sfxge_bar_fini(sc);
	(void) pci_disable_busmaster(sc->dev);

fail:
	sc->dev = NULL;
	sx_destroy(&sc->softc_lock);
	return (error);
}

static void
sfxge_destroy(struct sfxge_softc *sc)
{
	efx_nic_t *enp;

	/* Clean up port state. */
	sfxge_port_fini(sc);

	/* Clean up transmit state. */
	sfxge_tx_fini(sc);

	/* Clean up receive state. */
	sfxge_rx_fini(sc);

	/* Clean up event processing state. */
	sfxge_ev_fini(sc);

	/* Clean up interrupts. */
	sfxge_intr_fini(sc);

	/* Tear down common code subsystems. */
	efx_nic_reset(sc->enp);
	efx_vpd_fini(sc->enp);
	efx_nvram_fini(sc->enp);
	efx_nic_unprobe(sc->enp);

	/* Tear down MCDI. */
	sfxge_mcdi_fini(sc);

	/* Destroy common code context. */
	enp = sc->enp;
	sc->enp = NULL;
	efx_nic_destroy(enp);

	/* Free DMA memory. */
	sfxge_dma_fini(sc);

	/* Free mapped BARs. */
	sfxge_bar_fini(sc);

	(void) pci_disable_busmaster(sc->dev);

	taskqueue_drain(taskqueue_thread, &sc->task_reset);

	/* Destroy the softc lock. */
	sx_destroy(&sc->softc_lock);
}

static int
sfxge_vpd_handler(SYSCTL_HANDLER_ARGS)
{
	struct sfxge_softc *sc = arg1;
	efx_vpd_value_t value;
	int rc;

	value.evv_tag = arg2 >> 16;
	value.evv_keyword = arg2 & 0xffff;
	if ((rc = efx_vpd_get(sc->enp, sc->vpd_data, sc->vpd_size, &value))
	    != 0)
		return (rc);

	return (SYSCTL_OUT(req, value.evv_value, value.evv_length));
}

static void
sfxge_vpd_try_add(struct sfxge_softc *sc, struct sysctl_oid_list *list,
		  efx_vpd_tag_t tag, const char *keyword)
{
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(sc->dev);
	efx_vpd_value_t value;

	/* Check whether VPD tag/keyword is present */
	value.evv_tag = tag;
	value.evv_keyword = EFX_VPD_KEYWORD(keyword[0], keyword[1]);
	if (efx_vpd_get(sc->enp, sc->vpd_data, sc->vpd_size, &value) != 0)
		return;

	SYSCTL_ADD_PROC(
		ctx, list, OID_AUTO, keyword, CTLTYPE_STRING|CTLFLAG_RD,
		sc, tag << 16 | EFX_VPD_KEYWORD(keyword[0], keyword[1]),
		sfxge_vpd_handler, "A", "");
}

static int
sfxge_vpd_init(struct sfxge_softc *sc)
{
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(sc->dev);
	struct sysctl_oid *vpd_node;
	struct sysctl_oid_list *vpd_list;
	char keyword[3];
	efx_vpd_value_t value;
	int rc;

	if ((rc = efx_vpd_size(sc->enp, &sc->vpd_size)) != 0)
		goto fail;
	sc->vpd_data = malloc(sc->vpd_size, M_SFXGE, M_WAITOK);
	if ((rc = efx_vpd_read(sc->enp, sc->vpd_data, sc->vpd_size)) != 0)
		goto fail2;

	/* Copy ID (product name) into device description, and log it. */
	value.evv_tag = EFX_VPD_ID;
	if (efx_vpd_get(sc->enp, sc->vpd_data, sc->vpd_size, &value) == 0) {
		value.evv_value[value.evv_length] = 0;
		device_set_desc_copy(sc->dev, value.evv_value);
		device_printf(sc->dev, "%s\n", value.evv_value);
	}

	vpd_node = SYSCTL_ADD_NODE(
		ctx, SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev)),
		OID_AUTO, "vpd", CTLFLAG_RD, NULL, "Vital Product Data");
	vpd_list = SYSCTL_CHILDREN(vpd_node);

	/* Add sysctls for all expected and any vendor-defined keywords. */
	sfxge_vpd_try_add(sc, vpd_list, EFX_VPD_RO, "PN");
	sfxge_vpd_try_add(sc, vpd_list, EFX_VPD_RO, "EC");
	sfxge_vpd_try_add(sc, vpd_list, EFX_VPD_RO, "SN");
	keyword[0] = 'V';
	keyword[2] = 0;
	for (keyword[1] = '0'; keyword[1] <= '9'; keyword[1]++)
		sfxge_vpd_try_add(sc, vpd_list, EFX_VPD_RO, keyword);
	for (keyword[1] = 'A'; keyword[1] <= 'Z'; keyword[1]++)
		sfxge_vpd_try_add(sc, vpd_list, EFX_VPD_RO, keyword);

	return (0);

fail2:
	free(sc->vpd_data, M_SFXGE);
fail:
	return (rc);
}

static void
sfxge_vpd_fini(struct sfxge_softc *sc)
{
	free(sc->vpd_data, M_SFXGE);
}

static void
sfxge_reset(void *arg, int npending)
{
	struct sfxge_softc *sc;
	int rc;

	(void)npending;

	sc = (struct sfxge_softc *)arg;

	sx_xlock(&sc->softc_lock);

	if (sc->init_state != SFXGE_STARTED)
		goto done;

	sfxge_stop(sc);
	efx_nic_reset(sc->enp);
	if ((rc = sfxge_start(sc)) != 0)
		device_printf(sc->dev,
			      "reset failed (%d); interface is now stopped\n",
			      rc);

done:
	sx_xunlock(&sc->softc_lock);
}

void
sfxge_schedule_reset(struct sfxge_softc *sc)
{
	taskqueue_enqueue(taskqueue_thread, &sc->task_reset);
}

static int
sfxge_attach(device_t dev)
{
	struct sfxge_softc *sc;
	struct ifnet *ifp;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;

	/* Allocate ifnet. */
	ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "Couldn't allocate ifnet\n");
		error = ENOMEM;
		goto fail;
	}
	sc->ifnet = ifp;

	/* Initialize hardware. */
	if ((error = sfxge_create(sc)) != 0)
		goto fail2;

	/* Create the ifnet for the port. */
	if ((error = sfxge_ifnet_init(ifp, sc)) != 0)
		goto fail3;

	if ((error = sfxge_vpd_init(sc)) != 0)
		goto fail4;

	sc->init_state = SFXGE_REGISTERED;

	return (0);

fail4:
	sfxge_ifnet_fini(ifp);
fail3:
	sfxge_destroy(sc);

fail2:
	if_free(sc->ifnet);

fail:
	return (error);
}

static int
sfxge_detach(device_t dev)
{
	struct sfxge_softc *sc;

	sc = device_get_softc(dev);

	sfxge_vpd_fini(sc);

	/* Destroy the ifnet. */
	sfxge_ifnet_fini(sc->ifnet);

	/* Tear down hardware. */
	sfxge_destroy(sc);

	return (0);
}

static int
sfxge_probe(device_t dev)
{
	uint16_t pci_vendor_id;
	uint16_t pci_device_id;
	efx_family_t family;
	int rc;

	pci_vendor_id = pci_get_vendor(dev);
	pci_device_id = pci_get_device(dev);

	rc = efx_family(pci_vendor_id, pci_device_id, &family);
	if (rc != 0)
		return (ENXIO);

	KASSERT(family == EFX_FAMILY_SIENA, ("impossible controller family"));
	device_set_desc(dev, "Solarflare SFC9000 family");
	return (0);
}

static device_method_t sfxge_methods[] = {
	DEVMETHOD(device_probe,		sfxge_probe),
	DEVMETHOD(device_attach,	sfxge_attach),
	DEVMETHOD(device_detach,	sfxge_detach),

	DEVMETHOD_END
};

static devclass_t sfxge_devclass;

static driver_t sfxge_driver = {
	"sfxge",
	sfxge_methods,
	sizeof(struct sfxge_softc)
};

DRIVER_MODULE(sfxge, pci, sfxge_driver, sfxge_devclass, 0, 0);
