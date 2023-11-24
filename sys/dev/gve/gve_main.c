/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Google LLC
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "gve.h"
#include "gve_adminq.h"

#define GVE_DRIVER_VERSION "GVE-FBSD-1.0.1\n"
#define GVE_VERSION_MAJOR 1
#define GVE_VERSION_MINOR 0
#define GVE_VERSION_SUB 1

#define GVE_DEFAULT_RX_COPYBREAK 256

/* Devices supported by this driver. */
static struct gve_dev {
        uint16_t vendor_id;
        uint16_t device_id;
        const char *name;
} gve_devs[] = {
	{ PCI_VENDOR_ID_GOOGLE, PCI_DEV_ID_GVNIC, "gVNIC" }
};

struct sx gve_global_lock;

static int
gve_verify_driver_compatibility(struct gve_priv *priv)
{
	int err;
	struct gve_driver_info *driver_info;
	struct gve_dma_handle driver_info_mem;

	err = gve_dma_alloc_coherent(priv, sizeof(struct gve_driver_info),
	    PAGE_SIZE, &driver_info_mem);

	if (err != 0)
		return (ENOMEM);

	driver_info = driver_info_mem.cpu_addr;

	*driver_info = (struct gve_driver_info) {
		.os_type = 3, /* Freebsd */
		.driver_major = GVE_VERSION_MAJOR,
		.driver_minor = GVE_VERSION_MINOR,
		.driver_sub = GVE_VERSION_SUB,
		.os_version_major = htobe32(FBSD_VERSION_MAJOR),
		.os_version_minor = htobe32(FBSD_VERSION_MINOR),
		.os_version_sub = htobe32(FBSD_VERSION_PATCH),
		.driver_capability_flags = {
			htobe64(GVE_DRIVER_CAPABILITY_FLAGS1),
			htobe64(GVE_DRIVER_CAPABILITY_FLAGS2),
			htobe64(GVE_DRIVER_CAPABILITY_FLAGS3),
			htobe64(GVE_DRIVER_CAPABILITY_FLAGS4),
		},
	};

	snprintf(driver_info->os_version_str1, sizeof(driver_info->os_version_str1),
	    "FreeBSD %u", __FreeBSD_version);

	bus_dmamap_sync(driver_info_mem.tag, driver_info_mem.map,
	    BUS_DMASYNC_PREREAD);

	err = gve_adminq_verify_driver_compatibility(priv,
	    sizeof(struct gve_driver_info), driver_info_mem.bus_addr);

	/* It's ok if the device doesn't support this */
	if (err == EOPNOTSUPP)
		err = 0;

	gve_dma_free_coherent(&driver_info_mem);

	return (err);
}

static int
gve_up(struct gve_priv *priv)
{
	if_t ifp = priv->ifp;
	int err;

	GVE_IFACE_LOCK_ASSERT(priv->gve_iface_lock);

	if (device_is_attached(priv->dev) == 0) {
		device_printf(priv->dev, "Cannot bring the iface up when detached\n");
		return (ENXIO);
	}

	if (gve_get_state_flag(priv, GVE_STATE_FLAG_QUEUES_UP))
		return (0);

	if_clearhwassist(ifp);
	if (if_getcapenable(ifp) & IFCAP_TXCSUM)
		if_sethwassistbits(ifp, CSUM_TCP | CSUM_UDP, 0);
	if (if_getcapenable(ifp) & IFCAP_TXCSUM_IPV6)
		if_sethwassistbits(ifp, CSUM_IP6_TCP | CSUM_IP6_UDP, 0);
	if (if_getcapenable(ifp) & IFCAP_TSO4)
		if_sethwassistbits(ifp, CSUM_IP_TSO, 0);
	if (if_getcapenable(ifp) & IFCAP_TSO6)
		if_sethwassistbits(ifp, CSUM_IP6_TSO, 0);

	err = gve_register_qpls(priv);
	if (err != 0)
		goto reset;

	err = gve_create_rx_rings(priv);
	if (err != 0)
		goto reset;

	err = gve_create_tx_rings(priv);
	if (err != 0)
		goto reset;

	if_setdrvflagbits(ifp, IFF_DRV_RUNNING, IFF_DRV_OACTIVE);

	if (!gve_get_state_flag(priv, GVE_STATE_FLAG_LINK_UP)) {
		if_link_state_change(ifp, LINK_STATE_UP);
		gve_set_state_flag(priv, GVE_STATE_FLAG_LINK_UP);
	}

	gve_unmask_all_queue_irqs(priv);
	gve_set_state_flag(priv, GVE_STATE_FLAG_QUEUES_UP);
	priv->interface_up_cnt++;
	return (0);

reset:
	gve_schedule_reset(priv);
	return (err);
}

static void
gve_down(struct gve_priv *priv)
{
	GVE_IFACE_LOCK_ASSERT(priv->gve_iface_lock);

	if (!gve_get_state_flag(priv, GVE_STATE_FLAG_QUEUES_UP))
		return;

	if (gve_get_state_flag(priv, GVE_STATE_FLAG_LINK_UP)) {
		if_link_state_change(priv->ifp, LINK_STATE_DOWN);
		gve_clear_state_flag(priv, GVE_STATE_FLAG_LINK_UP);
	}

	if_setdrvflagbits(priv->ifp, IFF_DRV_OACTIVE, IFF_DRV_RUNNING);

	if (gve_destroy_rx_rings(priv) != 0)
		goto reset;

	if (gve_destroy_tx_rings(priv) != 0)
		goto reset;

	if (gve_unregister_qpls(priv) != 0)
		goto reset;

	gve_mask_all_queue_irqs(priv);
	gve_clear_state_flag(priv, GVE_STATE_FLAG_QUEUES_UP);
	priv->interface_down_cnt++;
	return;

reset:
	gve_schedule_reset(priv);
}

static int
gve_set_mtu(if_t ifp, uint32_t new_mtu)
{
	struct gve_priv *priv = if_getsoftc(ifp);
	int err;

	if ((new_mtu > priv->max_mtu) || (new_mtu < ETHERMIN)) {
		device_printf(priv->dev, "Invalid new MTU setting. new mtu: %d max mtu: %d min mtu: %d\n",
		    new_mtu, priv->max_mtu, ETHERMIN);
		return (EINVAL);
	}

	err = gve_adminq_set_mtu(priv, new_mtu);
	if (err == 0) {
		if (bootverbose)
			device_printf(priv->dev, "MTU set to %d\n", new_mtu);
		if_setmtu(ifp, new_mtu);
	} else {
		device_printf(priv->dev, "Failed to set MTU to %d\n", new_mtu);
	}

	return (err);
}

static void
gve_init(void *arg)
{
	struct gve_priv *priv = (struct gve_priv *)arg;

	if (!gve_get_state_flag(priv, GVE_STATE_FLAG_QUEUES_UP)) {
		GVE_IFACE_LOCK_LOCK(priv->gve_iface_lock);
		gve_up(priv);
		GVE_IFACE_LOCK_UNLOCK(priv->gve_iface_lock);
	}
}

static int
gve_ioctl(if_t ifp, u_long command, caddr_t data)
{
	struct gve_priv *priv;
	struct ifreq *ifr;
	int rc = 0;

	priv = if_getsoftc(ifp);
	ifr = (struct ifreq *)data;

	switch (command) {
	case SIOCSIFMTU:
		if (if_getmtu(ifp) == ifr->ifr_mtu)
			break;
		GVE_IFACE_LOCK_LOCK(priv->gve_iface_lock);
		gve_down(priv);
		gve_set_mtu(ifp, ifr->ifr_mtu);
		rc = gve_up(priv);
		GVE_IFACE_LOCK_UNLOCK(priv->gve_iface_lock);
		break;

	case SIOCSIFFLAGS:
		if ((if_getflags(ifp) & IFF_UP) != 0) {
			if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0) {
				GVE_IFACE_LOCK_LOCK(priv->gve_iface_lock);
				rc = gve_up(priv);
				GVE_IFACE_LOCK_UNLOCK(priv->gve_iface_lock);
			}
		} else {
			if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) != 0) {
				GVE_IFACE_LOCK_LOCK(priv->gve_iface_lock);
				gve_down(priv);
				GVE_IFACE_LOCK_UNLOCK(priv->gve_iface_lock);
			}
		}
		break;

	case SIOCSIFCAP:
		if (ifr->ifr_reqcap == if_getcapenable(ifp))
			break;
		GVE_IFACE_LOCK_LOCK(priv->gve_iface_lock);
		gve_down(priv);
		if_setcapenable(ifp, ifr->ifr_reqcap);
		rc = gve_up(priv);
		GVE_IFACE_LOCK_UNLOCK(priv->gve_iface_lock);
		break;

	case SIOCSIFMEDIA:
		/* FALLTHROUGH */
	case SIOCGIFMEDIA:
		rc = ifmedia_ioctl(ifp, ifr, &priv->media, command);
		break;

	default:
		rc = ether_ioctl(ifp, command, data);
		break;
	}

	return (rc);
}

static int
gve_media_change(if_t ifp)
{
	struct gve_priv *priv = if_getsoftc(ifp);

	device_printf(priv->dev, "Media change not supported\n");
	return (0);
}

static void
gve_media_status(if_t ifp, struct ifmediareq *ifmr)
{
	struct gve_priv *priv = if_getsoftc(ifp);

	GVE_IFACE_LOCK_LOCK(priv->gve_iface_lock);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (gve_get_state_flag(priv, GVE_STATE_FLAG_LINK_UP)) {
		ifmr->ifm_status |= IFM_ACTIVE;
		ifmr->ifm_active |= IFM_AUTO;
	} else {
		ifmr->ifm_active |= IFM_NONE;
	}

	GVE_IFACE_LOCK_UNLOCK(priv->gve_iface_lock);
}

static uint64_t
gve_get_counter(if_t ifp, ift_counter cnt)
{
	struct gve_priv *priv;
	uint64_t rpackets = 0;
	uint64_t tpackets = 0;
	uint64_t rbytes = 0;
	uint64_t tbytes = 0;
	uint64_t rx_dropped_pkt = 0;
	uint64_t tx_dropped_pkt = 0;

	priv = if_getsoftc(ifp);

	gve_accum_stats(priv, &rpackets, &rbytes, &rx_dropped_pkt, &tpackets,
	    &tbytes, &tx_dropped_pkt);

	switch (cnt) {
	case IFCOUNTER_IPACKETS:
		return (rpackets);

	case IFCOUNTER_OPACKETS:
		return (tpackets);

	case IFCOUNTER_IBYTES:
		return (rbytes);

	case IFCOUNTER_OBYTES:
		return (tbytes);

	case IFCOUNTER_IQDROPS:
		return (rx_dropped_pkt);

	case IFCOUNTER_OQDROPS:
		return (tx_dropped_pkt);

	default:
		return (if_get_counter_default(ifp, cnt));
	}
}

static int
gve_setup_ifnet(device_t dev, struct gve_priv *priv)
{
	int caps = 0;
	if_t ifp;

	ifp = priv->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(priv->dev, "Failed to allocate ifnet struct\n");
		return (ENXIO);
	}

	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	if_setsoftc(ifp, priv);
	if_setdev(ifp, dev);
	if_setinitfn(ifp, gve_init);
	if_setioctlfn(ifp, gve_ioctl);
	if_settransmitfn(ifp, gve_xmit_ifp);
	if_setqflushfn(ifp, gve_qflush);

#if __FreeBSD_version >= 1400086
	if_setflags(ifp, IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST);
#else
	if_setflags(ifp, IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST | IFF_KNOWSEPOCH);
#endif

	ifmedia_init(&priv->media, IFM_IMASK, gve_media_change, gve_media_status);
	if_setgetcounterfn(ifp, gve_get_counter);

	caps = IFCAP_RXCSUM |
	       IFCAP_TXCSUM |
	       IFCAP_TXCSUM_IPV6 |
	       IFCAP_TSO |
	       IFCAP_LRO;

	if ((priv->supported_features & GVE_SUP_JUMBO_FRAMES_MASK) != 0)
		caps |= IFCAP_JUMBO_MTU;

	if_setcapabilities(ifp, caps);
	if_setcapenable(ifp, caps);

	if (bootverbose)
		device_printf(priv->dev, "Setting initial MTU to %d\n", priv->max_mtu);
	if_setmtu(ifp, priv->max_mtu);

	ether_ifattach(ifp, priv->mac);

	ifmedia_add(&priv->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&priv->media, IFM_ETHER | IFM_AUTO);

	return (0);
}

static int
gve_alloc_counter_array(struct gve_priv *priv)
{
	int err;

	err = gve_dma_alloc_coherent(priv, sizeof(uint32_t) * priv->num_event_counters,
	    PAGE_SIZE, &priv->counter_array_mem);
	if (err != 0)
		return (err);

	priv->counters = priv->counter_array_mem.cpu_addr;
	return (0);
}

static void
gve_free_counter_array(struct gve_priv *priv)
{
	if (priv->counters != NULL)
		gve_dma_free_coherent(&priv->counter_array_mem);
	priv->counter_array_mem = (struct gve_dma_handle){};
}

static int
gve_alloc_irq_db_array(struct gve_priv *priv)
{
	int err;

	err = gve_dma_alloc_coherent(priv,
	    sizeof(struct gve_irq_db) * (priv->num_queues), PAGE_SIZE,
	    &priv->irqs_db_mem);
	if (err != 0)
		return (err);

	priv->irq_db_indices = priv->irqs_db_mem.cpu_addr;
	return (0);
}

static void
gve_free_irq_db_array(struct gve_priv *priv)
{
	if (priv->irq_db_indices != NULL)
		gve_dma_free_coherent(&priv->irqs_db_mem);
	priv->irqs_db_mem = (struct gve_dma_handle){};
}

static void
gve_free_rings(struct gve_priv *priv)
{
	gve_free_irqs(priv);
	gve_free_tx_rings(priv);
	gve_free_rx_rings(priv);
	gve_free_qpls(priv);
}

static int
gve_alloc_rings(struct gve_priv *priv)
{
	int err;

	err = gve_alloc_qpls(priv);
	if (err != 0)
		goto abort;

	err = gve_alloc_rx_rings(priv);
	if (err != 0)
		goto abort;

	err = gve_alloc_tx_rings(priv);
	if (err != 0)
		goto abort;

	err = gve_alloc_irqs(priv);
	if (err != 0)
		goto abort;

	return (0);

abort:
	gve_free_rings(priv);
	return (err);
}

static void
gve_deconfigure_resources(struct gve_priv *priv)
{
	int err;

	if (gve_get_state_flag(priv, GVE_STATE_FLAG_RESOURCES_OK)) {
		err = gve_adminq_deconfigure_device_resources(priv);
		if (err != 0) {
			device_printf(priv->dev, "Failed to deconfigure device resources: err=%d\n",
			    err);
			return;
		}
		if (bootverbose)
			device_printf(priv->dev, "Deconfigured device resources\n");
		gve_clear_state_flag(priv, GVE_STATE_FLAG_RESOURCES_OK);
	}

	gve_free_irq_db_array(priv);
	gve_free_counter_array(priv);
}

static int
gve_configure_resources(struct gve_priv *priv)
{
	int err;

	if (gve_get_state_flag(priv, GVE_STATE_FLAG_RESOURCES_OK))
		return (0);

	err = gve_alloc_counter_array(priv);
	if (err != 0)
		return (err);

	err = gve_alloc_irq_db_array(priv);
	if (err != 0)
		goto abort;

	err = gve_adminq_configure_device_resources(priv);
	if (err != 0) {
		device_printf(priv->dev, "Failed to configure device resources: err=%d\n",
			      err);
		err = (ENXIO);
		goto abort;
	}

	gve_set_state_flag(priv, GVE_STATE_FLAG_RESOURCES_OK);
	if (bootverbose)
		device_printf(priv->dev, "Configured device resources\n");
	return (0);

abort:
	gve_deconfigure_resources(priv);
	return (err);
}

static void
gve_set_queue_cnts(struct gve_priv *priv)
{
	priv->tx_cfg.max_queues = gve_reg_bar_read_4(priv, MAX_TX_QUEUES);
	priv->rx_cfg.max_queues = gve_reg_bar_read_4(priv, MAX_RX_QUEUES);
	priv->tx_cfg.num_queues = priv->tx_cfg.max_queues;
	priv->rx_cfg.num_queues = priv->rx_cfg.max_queues;

	if (priv->default_num_queues > 0) {
		priv->tx_cfg.num_queues = MIN(priv->default_num_queues,
		    priv->tx_cfg.num_queues);
		priv->rx_cfg.num_queues = MIN(priv->default_num_queues,
		    priv->rx_cfg.num_queues);
	}

	priv->num_queues = priv->tx_cfg.num_queues + priv->rx_cfg.num_queues;
	priv->mgmt_msix_idx = priv->num_queues;
}

static int
gve_alloc_adminq_and_describe_device(struct gve_priv *priv)
{
	int err;

	if ((err = gve_adminq_alloc(priv)) != 0)
		return (err);

	if ((err = gve_verify_driver_compatibility(priv)) != 0) {
		device_printf(priv->dev,
		    "Failed to verify driver compatibility: err=%d\n", err);
		goto abort;
	}

	if ((err = gve_adminq_describe_device(priv)) != 0)
		goto abort;

	gve_set_queue_cnts(priv);

	priv->num_registered_pages = 0;
	return (0);

abort:
	gve_release_adminq(priv);
	return (err);
}

void
gve_schedule_reset(struct gve_priv *priv)
{
	if (gve_get_state_flag(priv, GVE_STATE_FLAG_IN_RESET))
		return;

	device_printf(priv->dev, "Scheduling reset task!\n");
	gve_set_state_flag(priv, GVE_STATE_FLAG_DO_RESET);
	taskqueue_enqueue(priv->service_tq, &priv->service_task);
}

static void
gve_destroy(struct gve_priv *priv)
{
	gve_down(priv);
	gve_deconfigure_resources(priv);
	gve_release_adminq(priv);
}

static void
gve_restore(struct gve_priv *priv)
{
	int err;

	err = gve_adminq_alloc(priv);
	if (err != 0)
		goto abort;

	err = gve_configure_resources(priv);
	if (err != 0)
		goto abort;

	err = gve_up(priv);
	if (err != 0)
		goto abort;

	return;

abort:
	device_printf(priv->dev, "Restore failed!\n");
	return;
}

static void
gve_handle_reset(struct gve_priv *priv)
{
	if (!gve_get_state_flag(priv, GVE_STATE_FLAG_DO_RESET))
		return;

	gve_clear_state_flag(priv, GVE_STATE_FLAG_DO_RESET);
	gve_set_state_flag(priv, GVE_STATE_FLAG_IN_RESET);

	GVE_IFACE_LOCK_LOCK(priv->gve_iface_lock);

	if_setdrvflagbits(priv->ifp, IFF_DRV_OACTIVE, IFF_DRV_RUNNING);
	if_link_state_change(priv->ifp, LINK_STATE_DOWN);
	gve_clear_state_flag(priv, GVE_STATE_FLAG_LINK_UP);

	/*
	 * Releasing the adminq causes the NIC to destroy all resources
	 * registered with it, so by clearing the flags beneath we cause
	 * the subsequent gve_down call below to not attempt to tell the
	 * NIC to destroy these resources again.
	 *
	 * The call to gve_down is needed in the first place to refresh
	 * the state and the DMA-able memory within each driver ring.
	 */
	gve_release_adminq(priv);
	gve_clear_state_flag(priv, GVE_STATE_FLAG_RESOURCES_OK);
	gve_clear_state_flag(priv, GVE_STATE_FLAG_QPLREG_OK);
	gve_clear_state_flag(priv, GVE_STATE_FLAG_RX_RINGS_OK);
	gve_clear_state_flag(priv, GVE_STATE_FLAG_TX_RINGS_OK);

	gve_down(priv);
	gve_restore(priv);

	GVE_IFACE_LOCK_UNLOCK(priv->gve_iface_lock);

	priv->reset_cnt++;
	gve_clear_state_flag(priv, GVE_STATE_FLAG_IN_RESET);
}

static void
gve_handle_link_status(struct gve_priv *priv)
{
	uint32_t status = gve_reg_bar_read_4(priv, DEVICE_STATUS);
	bool link_up = status & GVE_DEVICE_STATUS_LINK_STATUS;

	if (link_up == gve_get_state_flag(priv, GVE_STATE_FLAG_LINK_UP))
		return;

	if (link_up) {
		if (bootverbose)
			device_printf(priv->dev, "Device link is up.\n");
		if_link_state_change(priv->ifp, LINK_STATE_UP);
		gve_set_state_flag(priv, GVE_STATE_FLAG_LINK_UP);
	} else {
		device_printf(priv->dev, "Device link is down.\n");
		if_link_state_change(priv->ifp, LINK_STATE_DOWN);
		gve_clear_state_flag(priv, GVE_STATE_FLAG_LINK_UP);
	}
}

static void
gve_service_task(void *arg, int pending)
{
	struct gve_priv *priv = (struct gve_priv *)arg;
	uint32_t status = gve_reg_bar_read_4(priv, DEVICE_STATUS);

	if (((GVE_DEVICE_STATUS_RESET_MASK & status) != 0) &&
	    !gve_get_state_flag(priv, GVE_STATE_FLAG_IN_RESET)) {
		device_printf(priv->dev, "Device requested reset\n");
		gve_set_state_flag(priv, GVE_STATE_FLAG_DO_RESET);
	}

	gve_handle_reset(priv);
	gve_handle_link_status(priv);
}

static int
gve_probe(device_t dev)
{
	uint16_t deviceid, vendorid;
	int i;

	vendorid = pci_get_vendor(dev);
	deviceid = pci_get_device(dev);

	for (i = 0; i < nitems(gve_devs); i++) {
		if (vendorid == gve_devs[i].vendor_id &&
		    deviceid == gve_devs[i].device_id) {
			device_set_desc(dev, gve_devs[i].name);
			return (BUS_PROBE_DEFAULT);
		}
	}
	return (ENXIO);
}

static void
gve_free_sys_res_mem(struct gve_priv *priv)
{
	if (priv->msix_table != NULL)
		bus_release_resource(priv->dev, SYS_RES_MEMORY,
		    rman_get_rid(priv->msix_table), priv->msix_table);

	if (priv->db_bar != NULL)
		bus_release_resource(priv->dev, SYS_RES_MEMORY,
		    rman_get_rid(priv->db_bar), priv->db_bar);

	if (priv->reg_bar != NULL)
		bus_release_resource(priv->dev, SYS_RES_MEMORY,
		    rman_get_rid(priv->reg_bar), priv->reg_bar);
}

static int
gve_attach(device_t dev)
{
	struct gve_priv *priv;
	int rid;
	int err;

	priv = device_get_softc(dev);
	priv->dev = dev;
	GVE_IFACE_LOCK_INIT(priv->gve_iface_lock);

	pci_enable_busmaster(dev);

	rid = PCIR_BAR(GVE_REGISTER_BAR);
	priv->reg_bar = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);
	if (priv->reg_bar == NULL) {
		device_printf(dev, "Failed to allocate BAR0\n");
		err = ENXIO;
		goto abort;
	}

	rid = PCIR_BAR(GVE_DOORBELL_BAR);
	priv->db_bar = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);
	if (priv->db_bar == NULL) {
		device_printf(dev, "Failed to allocate BAR2\n");
		err = ENXIO;
		goto abort;
	}

	rid = pci_msix_table_bar(priv->dev);
	priv->msix_table = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);
	if (priv->msix_table == NULL) {
		device_printf(dev, "Failed to allocate msix table\n");
		err = ENXIO;
		goto abort;
	}

	err = gve_alloc_adminq_and_describe_device(priv);
	if (err != 0)
		goto abort;

	err = gve_configure_resources(priv);
	if (err != 0)
		goto abort;

	err = gve_alloc_rings(priv);
	if (err != 0)
		goto abort;

	err = gve_setup_ifnet(dev, priv);
	if (err != 0)
		goto abort;

	priv->rx_copybreak = GVE_DEFAULT_RX_COPYBREAK;

	bus_write_multi_1(priv->reg_bar, DRIVER_VERSION, GVE_DRIVER_VERSION,
	    sizeof(GVE_DRIVER_VERSION) - 1);

	TASK_INIT(&priv->service_task, 0, gve_service_task, priv);
	priv->service_tq = taskqueue_create("gve service", M_WAITOK | M_ZERO,
	    taskqueue_thread_enqueue, &priv->service_tq);
	taskqueue_start_threads(&priv->service_tq, 1, PI_NET, "%s service tq",
	    device_get_nameunit(priv->dev));

        gve_setup_sysctl(priv);

	if (bootverbose)
		device_printf(priv->dev, "Successfully attached %s", GVE_DRIVER_VERSION);
	return (0);

abort:
	gve_free_rings(priv);
	gve_deconfigure_resources(priv);
	gve_release_adminq(priv);
	gve_free_sys_res_mem(priv);
	GVE_IFACE_LOCK_DESTROY(priv->gve_iface_lock);
	return (err);
}

static int
gve_detach(device_t dev)
{
	struct gve_priv *priv = device_get_softc(dev);
	if_t ifp = priv->ifp;

	ether_ifdetach(ifp);

	GVE_IFACE_LOCK_LOCK(priv->gve_iface_lock);
	gve_destroy(priv);
	GVE_IFACE_LOCK_UNLOCK(priv->gve_iface_lock);

	gve_free_rings(priv);
	gve_free_sys_res_mem(priv);
	GVE_IFACE_LOCK_DESTROY(priv->gve_iface_lock);

	while (taskqueue_cancel(priv->service_tq, &priv->service_task, NULL))
		taskqueue_drain(priv->service_tq, &priv->service_task);
	taskqueue_free(priv->service_tq);

	if_free(ifp);
	return (bus_generic_detach(dev));
}

static device_method_t gve_methods[] = {
	DEVMETHOD(device_probe, gve_probe),
	DEVMETHOD(device_attach, gve_attach),
	DEVMETHOD(device_detach, gve_detach),
	DEVMETHOD_END
};

static driver_t gve_driver = {
	"gve",
	gve_methods,
	sizeof(struct gve_priv)
};

#if __FreeBSD_version < 1301503
static devclass_t gve_devclass;

DRIVER_MODULE(gve, pci, gve_driver, gve_devclass, 0, 0);
#else
DRIVER_MODULE(gve, pci, gve_driver, 0, 0);
#endif
MODULE_PNP_INFO("U16:vendor;U16:device;D:#", pci, gve, gve_devs,
    nitems(gve_devs));
