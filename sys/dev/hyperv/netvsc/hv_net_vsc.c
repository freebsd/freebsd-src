/*-
 * Copyright (c) 2009-2012,2016 Microsoft Corp.
 * Copyright (c) 2010-2012 Citrix Inc.
 * Copyright (c) 2012 NetApp Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/**
 * HyperV vmbus network VSC (virtual services client) module
 *
 */


#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/lock.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <machine/bus.h>
#include <machine/atomic.h>

#include <dev/hyperv/include/hyperv.h>
#include <dev/hyperv/include/vmbus_xact.h>
#include <dev/hyperv/netvsc/hv_net_vsc.h>
#include <dev/hyperv/netvsc/hv_rndis.h>
#include <dev/hyperv/netvsc/hv_rndis_filter.h>
#include <dev/hyperv/netvsc/if_hnreg.h>

MALLOC_DEFINE(M_NETVSC, "netvsc", "Hyper-V netvsc driver");

/*
 * Forward declarations
 */
static void hv_nv_on_channel_callback(struct vmbus_channel *chan,
    void *xrxr);
static int  hv_nv_init_send_buffer_with_net_vsp(struct hn_softc *sc);
static int  hv_nv_init_rx_buffer_with_net_vsp(struct hn_softc *);
static int  hv_nv_destroy_send_buffer(netvsc_dev *net_dev);
static int  hv_nv_destroy_rx_buffer(netvsc_dev *net_dev);
static int  hv_nv_connect_to_vsp(struct hn_softc *sc);
static void hv_nv_on_send_completion(netvsc_dev *net_dev,
    struct vmbus_channel *, const struct vmbus_chanpkt_hdr *pkt);
static void hv_nv_on_receive_completion(struct vmbus_channel *chan,
    uint64_t tid);
static void hv_nv_on_receive(netvsc_dev *net_dev,
    struct hn_rx_ring *rxr, struct vmbus_channel *chan,
    const struct vmbus_chanpkt_hdr *pkt);
static void hn_nvs_sent_none(struct hn_send_ctx *sndc,
    struct netvsc_dev_ *net_dev, struct vmbus_channel *chan,
    const void *, int);

static struct hn_send_ctx	hn_send_ctx_none =
    HN_SEND_CTX_INITIALIZER(hn_nvs_sent_none, NULL);

/*
 *
 */
static inline netvsc_dev *
hv_nv_alloc_net_device(struct hn_softc *sc)
{
	netvsc_dev *net_dev;

	net_dev = malloc(sizeof(netvsc_dev), M_NETVSC, M_WAITOK | M_ZERO);

	net_dev->sc = sc;
	net_dev->destroy = FALSE;
	sc->net_dev = net_dev;

	return (net_dev);
}

/*
 * XXX unnecessary; nuke it.
 */
static inline netvsc_dev *
hv_nv_get_outbound_net_device(struct hn_softc *sc)
{
	return sc->net_dev;
}

/*
 * XXX unnecessary; nuke it.
 */
static inline netvsc_dev *
hv_nv_get_inbound_net_device(struct hn_softc *sc)
{
	return sc->net_dev;
}

int
hv_nv_get_next_send_section(netvsc_dev *net_dev)
{
	unsigned long bitsmap_words = net_dev->bitsmap_words;
	unsigned long *bitsmap = net_dev->send_section_bitsmap;
	unsigned long idx;
	int ret = HN_NVS_CHIM_IDX_INVALID;
	int i;

	for (i = 0; i < bitsmap_words; i++) {
		idx = ffsl(~bitsmap[i]);
		if (0 == idx)
			continue;

		idx--;
		KASSERT(i * BITS_PER_LONG + idx < net_dev->send_section_count,
		    ("invalid i %d and idx %lu", i, idx));

		if (atomic_testandset_long(&bitsmap[i], idx))
			continue;

		ret = i * BITS_PER_LONG + idx;
		break;
	}

	return (ret);
}

/*
 * Net VSC initialize receive buffer with net VSP
 * 
 * Net VSP:  Network virtual services client, also known as the
 *     Hyper-V extensible switch and the synthetic data path.
 */
static int 
hv_nv_init_rx_buffer_with_net_vsp(struct hn_softc *sc)
{
	struct vmbus_xact *xact;
	struct hn_nvs_rxbuf_conn *conn;
	const struct hn_nvs_rxbuf_connresp *resp;
	size_t resp_len;
	struct hn_send_ctx sndc;
	netvsc_dev *net_dev;
	uint32_t status;
	int error;

	net_dev = hv_nv_get_outbound_net_device(sc);
	if (!net_dev) {
		return (ENODEV);
	}

	net_dev->rx_buf = hyperv_dmamem_alloc(bus_get_dma_tag(sc->hn_dev),
	    PAGE_SIZE, 0, net_dev->rx_buf_size, &net_dev->rxbuf_dma,
	    BUS_DMA_WAITOK | BUS_DMA_ZERO);
	if (net_dev->rx_buf == NULL) {
		device_printf(sc->hn_dev, "allocate rxbuf failed\n");
		return (ENOMEM);
	}

	/*
	 * Connect the RXBUF GPADL to the primary channel.
	 *
	 * NOTE:
	 * Only primary channel has RXBUF connected to it.  Sub-channels
	 * just share this RXBUF.
	 */
	error = vmbus_chan_gpadl_connect(sc->hn_prichan,
	    net_dev->rxbuf_dma.hv_paddr, net_dev->rx_buf_size,
	    &net_dev->rx_buf_gpadl_handle);
	if (error) {
		if_printf(sc->hn_ifp, "rxbuf gpadl connect failed: %d\n",
		    error);
		goto cleanup;
	}

	/*
	 * Connect RXBUF to NVS.
	 */

	xact = vmbus_xact_get(sc->hn_xact, sizeof(*conn));
	if (xact == NULL) {
		if_printf(sc->hn_ifp, "no xact for nvs rxbuf conn\n");
		error = ENXIO;
		goto cleanup;
	}

	conn = vmbus_xact_req_data(xact);
	conn->nvs_type = HN_NVS_TYPE_RXBUF_CONN;
	conn->nvs_gpadl = net_dev->rx_buf_gpadl_handle;
	conn->nvs_sig = HN_NVS_RXBUF_SIG;

	hn_send_ctx_init_simple(&sndc, hn_nvs_sent_xact, xact);
	vmbus_xact_activate(xact);

	error = hn_nvs_send(sc->hn_prichan, VMBUS_CHANPKT_FLAG_RC,
	    conn, sizeof(*conn), &sndc);
	if (error != 0) {
		if_printf(sc->hn_ifp, "send nvs rxbuf conn failed: %d\n",
		    error);
		vmbus_xact_deactivate(xact);
		vmbus_xact_put(xact);
		goto cleanup;
	}

	resp = vmbus_xact_wait(xact, &resp_len);
	if (resp_len < sizeof(*resp)) {
		if_printf(sc->hn_ifp, "invalid rxbuf conn resp length %zu\n",
		    resp_len);
		vmbus_xact_put(xact);
		error = EINVAL;
		goto cleanup;
	}
	if (resp->nvs_type != HN_NVS_TYPE_RXBUF_CONNRESP) {
		if_printf(sc->hn_ifp, "not rxbuf conn resp, type %u\n",
		    resp->nvs_type);
		vmbus_xact_put(xact);
		error = EINVAL;
		goto cleanup;
	}

	status = resp->nvs_status;
	vmbus_xact_put(xact);

	if (status != HN_NVS_STATUS_OK) {
		if_printf(sc->hn_ifp, "rxbuf conn failed: %x\n", status);
		error = EIO;
		goto cleanup;
	}
	net_dev->rx_section_count = 1;

	return (0);

cleanup:
	hv_nv_destroy_rx_buffer(net_dev);
	return (error);
}

/*
 * Net VSC initialize send buffer with net VSP
 */
static int 
hv_nv_init_send_buffer_with_net_vsp(struct hn_softc *sc)
{
	struct hn_send_ctx sndc;
	struct vmbus_xact *xact;
	struct hn_nvs_chim_conn *chim;
	const struct hn_nvs_chim_connresp *resp;
	size_t resp_len;
	uint32_t status, sectsz;
	netvsc_dev *net_dev;
	int error;

	net_dev = hv_nv_get_outbound_net_device(sc);
	if (!net_dev) {
		return (ENODEV);
	}

	net_dev->send_buf = hyperv_dmamem_alloc(bus_get_dma_tag(sc->hn_dev),
	    PAGE_SIZE, 0, net_dev->send_buf_size, &net_dev->txbuf_dma,
	    BUS_DMA_WAITOK | BUS_DMA_ZERO);
	if (net_dev->send_buf == NULL) {
		device_printf(sc->hn_dev, "allocate chimney txbuf failed\n");
		return (ENOMEM);
	}

	/*
	 * Connect chimney sending buffer GPADL to the primary channel.
	 *
	 * NOTE:
	 * Only primary channel has chimney sending buffer connected to it.
	 * Sub-channels just share this chimney sending buffer.
	 */
	error = vmbus_chan_gpadl_connect(sc->hn_prichan,
  	    net_dev->txbuf_dma.hv_paddr, net_dev->send_buf_size,
	    &net_dev->send_buf_gpadl_handle);
	if (error) {
		if_printf(sc->hn_ifp, "chimney sending buffer gpadl "
		    "connect failed: %d\n", error);
		goto cleanup;
	}

	/*
	 * Connect chimney sending buffer to NVS
	 */

	xact = vmbus_xact_get(sc->hn_xact, sizeof(*chim));
	if (xact == NULL) {
		if_printf(sc->hn_ifp, "no xact for nvs chim conn\n");
		error = ENXIO;
		goto cleanup;
	}

	chim = vmbus_xact_req_data(xact);
	chim->nvs_type = HN_NVS_TYPE_CHIM_CONN;
	chim->nvs_gpadl = net_dev->send_buf_gpadl_handle;
	chim->nvs_sig = HN_NVS_CHIM_SIG;

	hn_send_ctx_init_simple(&sndc, hn_nvs_sent_xact, xact);
	vmbus_xact_activate(xact);

	error = hn_nvs_send(sc->hn_prichan, VMBUS_CHANPKT_FLAG_RC,
  	    chim, sizeof(*chim), &sndc);
	if (error) {
		if_printf(sc->hn_ifp, "send nvs chim conn failed: %d\n",
		    error);
		vmbus_xact_deactivate(xact);
		vmbus_xact_put(xact);
		goto cleanup;
	}

	resp = vmbus_xact_wait(xact, &resp_len);
	if (resp_len < sizeof(*resp)) {
		if_printf(sc->hn_ifp, "invalid chim conn resp length %zu\n",
		    resp_len);
		vmbus_xact_put(xact);
		error = EINVAL;
		goto cleanup;
	}
	if (resp->nvs_type != HN_NVS_TYPE_CHIM_CONNRESP) {
		if_printf(sc->hn_ifp, "not chim conn resp, type %u\n",
		    resp->nvs_type);
		vmbus_xact_put(xact);
		error = EINVAL;
		goto cleanup;
	}

	status = resp->nvs_status;
	sectsz = resp->nvs_sectsz;
	vmbus_xact_put(xact);

	if (status != HN_NVS_STATUS_OK) {
		if_printf(sc->hn_ifp, "chim conn failed: %x\n", status);
		error = EIO;
		goto cleanup;
	}
	if (sectsz == 0) {
		if_printf(sc->hn_ifp, "zero chimney sending buffer "
		    "section size\n");
		return 0;
	}

	net_dev->send_section_size = sectsz;
	net_dev->send_section_count =
	    net_dev->send_buf_size / net_dev->send_section_size;
	net_dev->bitsmap_words = howmany(net_dev->send_section_count,
	    BITS_PER_LONG);
	net_dev->send_section_bitsmap =
	    malloc(net_dev->bitsmap_words * sizeof(long), M_NETVSC,
	    M_WAITOK | M_ZERO);

	if (bootverbose) {
		if_printf(sc->hn_ifp, "chimney sending buffer %u/%u\n",
		    net_dev->send_section_size, net_dev->send_section_count);
	}
	return 0;

cleanup:
	hv_nv_destroy_send_buffer(net_dev);
	return (error);
}

/*
 * Net VSC destroy receive buffer
 */
static int
hv_nv_destroy_rx_buffer(netvsc_dev *net_dev)
{
	int ret = 0;

	if (net_dev->rx_section_count) {
		struct hn_nvs_rxbuf_disconn disconn;

		/*
		 * Disconnect RXBUF from NVS.
		 */
		memset(&disconn, 0, sizeof(disconn));
		disconn.nvs_type = HN_NVS_TYPE_RXBUF_DISCONN;
		disconn.nvs_sig = HN_NVS_RXBUF_SIG;

		/* NOTE: No response. */
		ret = hn_nvs_send(net_dev->sc->hn_prichan,
		    VMBUS_CHANPKT_FLAG_NONE, &disconn, sizeof(disconn),
		    &hn_send_ctx_none);
		if (ret != 0) {
			if_printf(net_dev->sc->hn_ifp,
			    "send rxbuf disconn failed: %d\n", ret);
			return (ret);
		}
		net_dev->rx_section_count = 0;
	}
		
	/* Tear down the gpadl on the vsp end */
	if (net_dev->rx_buf_gpadl_handle) {
		ret = vmbus_chan_gpadl_disconnect(net_dev->sc->hn_prichan,
		    net_dev->rx_buf_gpadl_handle);
		/*
		 * If we failed here, we might as well return and have a leak 
		 * rather than continue and a bugchk
		 */
		if (ret != 0) {
			return (ret);
		}
		net_dev->rx_buf_gpadl_handle = 0;
	}

	if (net_dev->rx_buf) {
		/* Free up the receive buffer */
		hyperv_dmamem_free(&net_dev->rxbuf_dma, net_dev->rx_buf);
		net_dev->rx_buf = NULL;
	}

	return (ret);
}

/*
 * Net VSC destroy send buffer
 */
static int
hv_nv_destroy_send_buffer(netvsc_dev *net_dev)
{
	int ret = 0;

	if (net_dev->send_section_size) {
		struct hn_nvs_chim_disconn disconn;

		/*
		 * Disconnect chimney sending buffer from NVS.
		 */
		memset(&disconn, 0, sizeof(disconn));
		disconn.nvs_type = HN_NVS_TYPE_CHIM_DISCONN;
		disconn.nvs_sig = HN_NVS_CHIM_SIG;

		/* NOTE: No response. */
		ret = hn_nvs_send(net_dev->sc->hn_prichan,
		    VMBUS_CHANPKT_FLAG_NONE, &disconn, sizeof(disconn),
		    &hn_send_ctx_none);
		if (ret != 0) {
			if_printf(net_dev->sc->hn_ifp,
			    "send chim disconn failed: %d\n", ret);
			return (ret);
		}
	}
		
	/* Tear down the gpadl on the vsp end */
	if (net_dev->send_buf_gpadl_handle) {
		ret = vmbus_chan_gpadl_disconnect(net_dev->sc->hn_prichan,
		    net_dev->send_buf_gpadl_handle);

		/*
		 * If we failed here, we might as well return and have a leak 
		 * rather than continue and a bugchk
		 */
		if (ret != 0) {
			return (ret);
		}
		net_dev->send_buf_gpadl_handle = 0;
	}

	if (net_dev->send_buf) {
		/* Free up the receive buffer */
		hyperv_dmamem_free(&net_dev->txbuf_dma, net_dev->send_buf);
		net_dev->send_buf = NULL;
	}

	if (net_dev->send_section_bitsmap) {
		free(net_dev->send_section_bitsmap, M_NETVSC);
	}

	return (ret);
}

static int
hv_nv_negotiate_nvsp_protocol(struct hn_softc *sc, netvsc_dev *net_dev,
    uint32_t nvs_ver)
{
	struct hn_send_ctx sndc;
	struct vmbus_xact *xact;
	struct hn_nvs_init *init;
	const struct hn_nvs_init_resp *resp;
	size_t resp_len;
	uint32_t status;
	int error;

	xact = vmbus_xact_get(sc->hn_xact, sizeof(*init));
	if (xact == NULL) {
		if_printf(sc->hn_ifp, "no xact for nvs init\n");
		return (ENXIO);
	}

	init = vmbus_xact_req_data(xact);
	init->nvs_type = HN_NVS_TYPE_INIT;
	init->nvs_ver_min = nvs_ver;
	init->nvs_ver_max = nvs_ver;

	vmbus_xact_activate(xact);
	hn_send_ctx_init_simple(&sndc, hn_nvs_sent_xact, xact);

	error = hn_nvs_send(sc->hn_prichan, VMBUS_CHANPKT_FLAG_RC,
	    init, sizeof(*init), &sndc);
	if (error) {
		if_printf(sc->hn_ifp, "send nvs init failed: %d\n", error);
		vmbus_xact_deactivate(xact);
		vmbus_xact_put(xact);
		return (error);
	}

	resp = vmbus_xact_wait(xact, &resp_len);
	if (resp_len < sizeof(*resp)) {
		if_printf(sc->hn_ifp, "invalid init resp length %zu\n",
		    resp_len);
		vmbus_xact_put(xact);
		return (EINVAL);
	}
	if (resp->nvs_type != HN_NVS_TYPE_INIT_RESP) {
		if_printf(sc->hn_ifp, "not init resp, type %u\n",
		    resp->nvs_type);
		vmbus_xact_put(xact);
		return (EINVAL);
	}

	status = resp->nvs_status;
	vmbus_xact_put(xact);

	if (status != HN_NVS_STATUS_OK) {
		if_printf(sc->hn_ifp, "nvs init failed for ver 0x%x\n",
		    nvs_ver);
		return (EINVAL);
	}
	return (0);
}

/*
 * Send NDIS version 2 config packet containing MTU.
 *
 * Not valid for NDIS version 1.
 */
static int
hv_nv_send_ndis_config(struct hn_softc *sc, uint32_t mtu)
{
	struct hn_nvs_ndis_conf conf;
	int error;

	memset(&conf, 0, sizeof(conf));
	conf.nvs_type = HN_NVS_TYPE_NDIS_CONF;
	conf.nvs_mtu = mtu;
	conf.nvs_caps = HN_NVS_NDIS_CONF_VLAN;

	/* NOTE: No response. */
	error = hn_nvs_send(sc->hn_prichan, VMBUS_CHANPKT_FLAG_NONE,
	    &conf, sizeof(conf), &hn_send_ctx_none);
	if (error)
		if_printf(sc->hn_ifp, "send nvs ndis conf failed: %d\n", error);
	return (error);
}

/*
 * Net VSC connect to VSP
 */
static int
hv_nv_connect_to_vsp(struct hn_softc *sc)
{
	netvsc_dev *net_dev;
	uint32_t protocol_list[] = { NVSP_PROTOCOL_VERSION_1,
	    NVSP_PROTOCOL_VERSION_2,
	    NVSP_PROTOCOL_VERSION_4,
	    NVSP_PROTOCOL_VERSION_5 };
	int i;
	int protocol_number = nitems(protocol_list);
	int ret = 0;
	device_t dev = sc->hn_dev;
	struct ifnet *ifp = sc->hn_ifp;
	struct hn_nvs_ndis_init ndis;

	net_dev = hv_nv_get_outbound_net_device(sc);

	/*
	 * Negotiate the NVSP version.  Try the latest NVSP first.
	 */
	for (i = protocol_number - 1; i >= 0; i--) {
		if (hv_nv_negotiate_nvsp_protocol(sc, net_dev,
		    protocol_list[i]) == 0) {
			net_dev->nvsp_version = protocol_list[i];
			if (bootverbose)
				device_printf(dev, "Netvsc: got version 0x%x\n",
				    net_dev->nvsp_version);
			break;
		}
	}

	if (i < 0) {
		if (bootverbose)
			device_printf(dev, "failed to negotiate a valid "
			    "protocol.\n");
		return (EPROTO);
	}

	/*
	 * Set the MTU if supported by this NVSP protocol version
	 * This needs to be right after the NVSP init message per Haiyang
	 */
	if (net_dev->nvsp_version >= NVSP_PROTOCOL_VERSION_2)
		ret = hv_nv_send_ndis_config(sc, ifp->if_mtu);

	/*
	 * Initialize NDIS.
	 */

	memset(&ndis, 0, sizeof(ndis));
	ndis.nvs_type = HN_NVS_TYPE_NDIS_INIT;
	ndis.nvs_ndis_major = NDIS_VERSION_MAJOR_6;
	if (net_dev->nvsp_version <= NVSP_PROTOCOL_VERSION_4)
		ndis.nvs_ndis_minor = NDIS_VERSION_MINOR_1;
	else
		ndis.nvs_ndis_minor = NDIS_VERSION_MINOR_30;

	/* NOTE: No response. */
	ret = hn_nvs_send(sc->hn_prichan, VMBUS_CHANPKT_FLAG_NONE,
	    &ndis, sizeof(ndis), &hn_send_ctx_none);
	if (ret != 0) {
		if_printf(sc->hn_ifp, "send nvs ndis init failed: %d\n", ret);
		goto cleanup;
	}

	/* Post the big receive buffer to NetVSP */
	if (net_dev->nvsp_version <= NVSP_PROTOCOL_VERSION_2)
		net_dev->rx_buf_size = NETVSC_RECEIVE_BUFFER_SIZE_LEGACY;
	else
		net_dev->rx_buf_size = NETVSC_RECEIVE_BUFFER_SIZE;
	net_dev->send_buf_size = NETVSC_SEND_BUFFER_SIZE;

	ret = hv_nv_init_rx_buffer_with_net_vsp(sc);
	if (ret == 0)
		ret = hv_nv_init_send_buffer_with_net_vsp(sc);

cleanup:
	return (ret);
}

/*
 * Net VSC disconnect from VSP
 */
static void
hv_nv_disconnect_from_vsp(netvsc_dev *net_dev)
{
	hv_nv_destroy_rx_buffer(net_dev);
	hv_nv_destroy_send_buffer(net_dev);
}

void
hv_nv_subchan_attach(struct vmbus_channel *chan, struct hn_rx_ring *rxr)
{
	KASSERT(rxr->hn_rx_idx == vmbus_chan_subidx(chan),
	    ("chan%u subidx %u, rxr%d mismatch",
	     vmbus_chan_id(chan), vmbus_chan_subidx(chan), rxr->hn_rx_idx));
	vmbus_chan_open(chan, NETVSC_DEVICE_RING_BUFFER_SIZE,
	    NETVSC_DEVICE_RING_BUFFER_SIZE, NULL, 0,
	    hv_nv_on_channel_callback, rxr);
}

/*
 * Net VSC on device add
 * 
 * Callback when the device belonging to this driver is added
 */
netvsc_dev *
hv_nv_on_device_add(struct hn_softc *sc, void *additional_info,
    struct hn_rx_ring *rxr)
{
	struct vmbus_channel *chan = sc->hn_prichan;
	netvsc_dev *net_dev;
	int ret = 0;

	net_dev = hv_nv_alloc_net_device(sc);
	if (net_dev == NULL)
		return NULL;

	/* Initialize the NetVSC channel extension */

	/*
	 * Open the channel
	 */
	KASSERT(rxr->hn_rx_idx == vmbus_chan_subidx(chan),
	    ("chan%u subidx %u, rxr%d mismatch",
	     vmbus_chan_id(chan), vmbus_chan_subidx(chan), rxr->hn_rx_idx));
	ret = vmbus_chan_open(chan,
	    NETVSC_DEVICE_RING_BUFFER_SIZE, NETVSC_DEVICE_RING_BUFFER_SIZE,
	    NULL, 0, hv_nv_on_channel_callback, rxr);
	if (ret != 0)
		goto cleanup;

	/*
	 * Connect with the NetVsp
	 */
	ret = hv_nv_connect_to_vsp(sc);
	if (ret != 0)
		goto close;

	return (net_dev);

close:
	/* Now, we can close the channel safely */
	vmbus_chan_close(chan);

cleanup:
	/*
	 * Free the packet buffers on the netvsc device packet queue.
	 * Release other resources.
	 */
	free(net_dev, M_NETVSC);

	return (NULL);
}

/*
 * Net VSC on device remove
 */
int
hv_nv_on_device_remove(struct hn_softc *sc, boolean_t destroy_channel)
{
	netvsc_dev *net_dev = sc->net_dev;;
	
	/* Stop outbound traffic ie sends and receives completions */
	net_dev->destroy = TRUE;

	hv_nv_disconnect_from_vsp(net_dev);

	/* At this point, no one should be accessing net_dev except in here */

	/* Now, we can close the channel safely */

	vmbus_chan_close(sc->hn_prichan);

	free(net_dev, M_NETVSC);

	return (0);
}

void
hn_nvs_sent_xact(struct hn_send_ctx *sndc,
    struct netvsc_dev_ *net_dev __unused, struct vmbus_channel *chan __unused,
    const void *data, int dlen)
{

	vmbus_xact_wakeup(sndc->hn_cbarg, data, dlen);
}

static void
hn_nvs_sent_none(struct hn_send_ctx *sndc __unused,
    struct netvsc_dev_ *net_dev __unused, struct vmbus_channel *chan __unused,
    const void *data __unused, int dlen __unused)
{
	/* EMPTY */
}

void
hn_chim_free(struct netvsc_dev_ *net_dev, uint32_t chim_idx)
{
	u_long mask;
	uint32_t idx;

	idx = chim_idx / BITS_PER_LONG;
	KASSERT(idx < net_dev->bitsmap_words,
	    ("invalid chimney index 0x%x", chim_idx));

	mask = 1UL << (chim_idx % BITS_PER_LONG);
	KASSERT(net_dev->send_section_bitsmap[idx] & mask,
	    ("index bitmap 0x%lx, chimney index %u, "
	     "bitmap idx %d, bitmask 0x%lx",
	     net_dev->send_section_bitsmap[idx], chim_idx, idx, mask));

	atomic_clear_long(&net_dev->send_section_bitsmap[idx], mask);
}

/*
 * Net VSC on send completion
 */
static void
hv_nv_on_send_completion(netvsc_dev *net_dev, struct vmbus_channel *chan,
    const struct vmbus_chanpkt_hdr *pkt)
{
	struct hn_send_ctx *sndc;

	sndc = (struct hn_send_ctx *)(uintptr_t)pkt->cph_xactid;
	sndc->hn_cb(sndc, net_dev, chan, VMBUS_CHANPKT_CONST_DATA(pkt),
	    VMBUS_CHANPKT_DATALEN(pkt));
	/*
	 * NOTE:
	 * 'sndc' CAN NOT be accessed anymore, since it can be freed by
	 * its callback.
	 */
}

/*
 * Net VSC on send
 * Sends a packet on the specified Hyper-V device.
 * Returns 0 on success, non-zero on failure.
 */
int
hv_nv_on_send(struct vmbus_channel *chan, uint32_t rndis_mtype,
    struct hn_send_ctx *sndc, struct vmbus_gpa *gpa, int gpa_cnt)
{
	struct hn_nvs_rndis rndis;
	int ret;

	rndis.nvs_type = HN_NVS_TYPE_RNDIS;
	rndis.nvs_rndis_mtype = rndis_mtype;
	rndis.nvs_chim_idx = sndc->hn_chim_idx;
	rndis.nvs_chim_sz = sndc->hn_chim_sz;

	if (gpa_cnt) {
		ret = hn_nvs_send_sglist(chan, gpa, gpa_cnt,
		    &rndis, sizeof(rndis), sndc);
	} else {
		ret = hn_nvs_send(chan, VMBUS_CHANPKT_FLAG_RC,
		    &rndis, sizeof(rndis), sndc);
	}

	return (ret);
}

/*
 * Net VSC on receive
 *
 * In the FreeBSD Hyper-V virtual world, this function deals exclusively
 * with virtual addresses.
 */
static void
hv_nv_on_receive(netvsc_dev *net_dev, struct hn_rx_ring *rxr,
    struct vmbus_channel *chan, const struct vmbus_chanpkt_hdr *pkthdr)
{
	const struct vmbus_chanpkt_rxbuf *pkt;
	const struct hn_nvs_hdr *nvs_hdr;
	int count = 0;
	int i = 0;

	/* Make sure that this is a RNDIS message. */
	nvs_hdr = VMBUS_CHANPKT_CONST_DATA(pkthdr);
	if (__predict_false(nvs_hdr->nvs_type != HN_NVS_TYPE_RNDIS)) {
		if_printf(rxr->hn_ifp, "nvs type %u, not RNDIS\n",
		    nvs_hdr->nvs_type);
		return;
	}
	
	pkt = (const struct vmbus_chanpkt_rxbuf *)pkthdr;

	if (pkt->cp_rxbuf_id != NETVSC_RECEIVE_BUFFER_ID) {
		if_printf(rxr->hn_ifp, "rxbuf_id %d is invalid!\n",
		    pkt->cp_rxbuf_id);
		return;
	}

	count = pkt->cp_rxbuf_cnt;

	/* Each range represents 1 RNDIS pkt that contains 1 Ethernet frame */
	for (i = 0; i < count; i++) {
		hv_rf_on_receive(net_dev, rxr,
		    (const uint8_t *)net_dev->rx_buf + pkt->cp_rxbuf[i].rb_ofs,
		    pkt->cp_rxbuf[i].rb_len);
	}
	
	/*
	 * Moved completion call back here so that all received 
	 * messages (not just data messages) will trigger a response
	 * message back to the host.
	 */
	hv_nv_on_receive_completion(chan, pkt->cp_hdr.cph_xactid);
}

/*
 * Net VSC on receive completion
 *
 * Send a receive completion packet to RNDIS device (ie NetVsp)
 */
static void
hv_nv_on_receive_completion(struct vmbus_channel *chan, uint64_t tid)
{
	struct hn_nvs_rndis_ack ack;
	int retries = 0;
	int ret = 0;
	
	ack.nvs_type = HN_NVS_TYPE_RNDIS_ACK;
	ack.nvs_status = HN_NVS_STATUS_OK;

retry_send_cmplt:
	/* Send the completion */
	ret = vmbus_chan_send(chan, VMBUS_CHANPKT_TYPE_COMP,
	    VMBUS_CHANPKT_FLAG_NONE, &ack, sizeof(ack), tid);
	if (ret == 0) {
		/* success */
		/* no-op */
	} else if (ret == EAGAIN) {
		/* no more room... wait a bit and attempt to retry 3 times */
		retries++;

		if (retries < 4) {
			DELAY(100);
			goto retry_send_cmplt;
		}
	}
}

static void
hn_proc_notify(struct hn_softc *sc, const struct vmbus_chanpkt_hdr *pkt)
{
	const struct hn_nvs_hdr *hdr;

	hdr = VMBUS_CHANPKT_CONST_DATA(pkt);
	if (hdr->nvs_type == HN_NVS_TYPE_TXTBL_NOTE) {
		/* Useless; ignore */
		return;
	}
	if_printf(sc->hn_ifp, "got notify, nvs type %u\n", hdr->nvs_type);
}

/*
 * Net VSC on channel callback
 */
static void
hv_nv_on_channel_callback(struct vmbus_channel *chan, void *xrxr)
{
	struct hn_rx_ring *rxr = xrxr;
	struct hn_softc *sc = rxr->hn_ifp->if_softc;
	netvsc_dev *net_dev;
	void *buffer;
	int bufferlen = NETVSC_PACKET_SIZE;

	net_dev = hv_nv_get_inbound_net_device(sc);
	if (net_dev == NULL)
		return;

	buffer = rxr->hn_rdbuf;
	do {
		struct vmbus_chanpkt_hdr *pkt = buffer;
		uint32_t bytes_rxed;
		int ret;

		bytes_rxed = bufferlen;
		ret = vmbus_chan_recv_pkt(chan, pkt, &bytes_rxed);
		if (ret == 0) {
			if (bytes_rxed > 0) {
				switch (pkt->cph_type) {
				case VMBUS_CHANPKT_TYPE_COMP:
					hv_nv_on_send_completion(net_dev, chan,
					    pkt);
					break;
				case VMBUS_CHANPKT_TYPE_RXBUF:
					hv_nv_on_receive(net_dev, rxr, chan, pkt);
					break;
				case VMBUS_CHANPKT_TYPE_INBAND:
					hn_proc_notify(sc, pkt);
					break;
				default:
					if_printf(rxr->hn_ifp,
					    "unknown chan pkt %u\n",
					    pkt->cph_type);
					break;
				}
			}
		} else if (ret == ENOBUFS) {
			/* Handle large packet */
			if (bufferlen > NETVSC_PACKET_SIZE) {
				free(buffer, M_NETVSC);
				buffer = NULL;
			}

			/* alloc new buffer */
			buffer = malloc(bytes_rxed, M_NETVSC, M_NOWAIT);
			if (buffer == NULL) {
				if_printf(rxr->hn_ifp,
				    "hv_cb malloc buffer failed, len=%u\n",
				    bytes_rxed);
				bufferlen = 0;
				break;
			}
			bufferlen = bytes_rxed;
		} else {
			/* No more packets */
			break;
		}
	} while (1);

	if (bufferlen > NETVSC_PACKET_SIZE)
		free(buffer, M_NETVSC);

	hv_rf_channel_rollup(rxr, rxr->hn_txr);
}
