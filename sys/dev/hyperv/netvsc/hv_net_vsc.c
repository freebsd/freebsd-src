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
#include <sys/limits.h>
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
static int  hv_nv_init_rx_buffer_with_net_vsp(struct hn_softc *, int);
static int  hv_nv_destroy_send_buffer(struct hn_softc *sc);
static int  hv_nv_destroy_rx_buffer(struct hn_softc *sc);
static int  hv_nv_connect_to_vsp(struct hn_softc *sc);
static void hv_nv_on_send_completion(struct hn_softc *sc,
    struct vmbus_channel *, const struct vmbus_chanpkt_hdr *pkt);
static void hv_nv_on_receive_completion(struct vmbus_channel *chan,
    uint64_t tid);
static void hv_nv_on_receive(struct hn_softc *sc,
    struct hn_rx_ring *rxr, struct vmbus_channel *chan,
    const struct vmbus_chanpkt_hdr *pkt);
static void hn_nvs_sent_none(struct hn_send_ctx *sndc,
    struct hn_softc *, struct vmbus_channel *chan,
    const void *, int);
static void hn_nvs_sent_xact(struct hn_send_ctx *, struct hn_softc *sc,
    struct vmbus_channel *, const void *, int);

struct hn_send_ctx	hn_send_ctx_none =
    HN_SEND_CTX_INITIALIZER(hn_nvs_sent_none, NULL);

uint32_t
hn_chim_alloc(struct hn_softc *sc)
{
	int i, bmap_cnt = sc->hn_chim_bmap_cnt;
	u_long *bmap = sc->hn_chim_bmap;
	uint32_t ret = HN_NVS_CHIM_IDX_INVALID;

	for (i = 0; i < bmap_cnt; ++i) {
		int idx;

		idx = ffsl(~bmap[i]);
		if (idx == 0)
			continue;

		--idx; /* ffsl is 1-based */
		KASSERT(i * LONG_BIT + idx < sc->hn_chim_cnt,
		    ("invalid i %d and idx %d", i, idx));

		if (atomic_testandset_long(&bmap[i], idx))
			continue;

		ret = i * LONG_BIT + idx;
		break;
	}
	return (ret);
}

const void *
hn_nvs_xact_execute(struct hn_softc *sc, struct vmbus_xact *xact,
    void *req, int reqlen, size_t *resp_len)
{
	struct hn_send_ctx sndc;
	int error;

	hn_send_ctx_init_simple(&sndc, hn_nvs_sent_xact, xact);
	vmbus_xact_activate(xact);

	error = hn_nvs_send(sc->hn_prichan, VMBUS_CHANPKT_FLAG_RC,
	    req, reqlen, &sndc);
	if (error) {
		vmbus_xact_deactivate(xact);
		return NULL;
	}
	return (vmbus_xact_wait(xact, resp_len));
}

static __inline int
hn_nvs_req_send(struct hn_softc *sc, void *req, int reqlen)
{

	return (hn_nvs_send(sc->hn_prichan, VMBUS_CHANPKT_FLAG_NONE,
	    req, reqlen, &hn_send_ctx_none));
}

/*
 * Net VSC initialize receive buffer with net VSP
 * 
 * Net VSP:  Network virtual services client, also known as the
 *     Hyper-V extensible switch and the synthetic data path.
 */
static int 
hv_nv_init_rx_buffer_with_net_vsp(struct hn_softc *sc, int rxbuf_size)
{
	struct vmbus_xact *xact = NULL;
	struct hn_nvs_rxbuf_conn *conn;
	const struct hn_nvs_rxbuf_connresp *resp;
	size_t resp_len;
	uint32_t status;
	int error;

	KASSERT(rxbuf_size <= NETVSC_RECEIVE_BUFFER_SIZE,
	    ("invalid rxbuf size %d", rxbuf_size));

	/*
	 * Connect the RXBUF GPADL to the primary channel.
	 *
	 * NOTE:
	 * Only primary channel has RXBUF connected to it.  Sub-channels
	 * just share this RXBUF.
	 */
	error = vmbus_chan_gpadl_connect(sc->hn_prichan,
	    sc->hn_rxbuf_dma.hv_paddr, rxbuf_size, &sc->hn_rxbuf_gpadl);
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
	conn->nvs_gpadl = sc->hn_rxbuf_gpadl;
	conn->nvs_sig = HN_NVS_RXBUF_SIG;

	resp = hn_nvs_xact_execute(sc, xact, conn, sizeof(*conn), &resp_len);
	if (resp == NULL) {
		if_printf(sc->hn_ifp, "exec rxbuf conn failed\n");
		error = EIO;
		goto cleanup;
	}
	if (resp_len < sizeof(*resp)) {
		if_printf(sc->hn_ifp, "invalid rxbuf conn resp length %zu\n",
		    resp_len);
		error = EINVAL;
		goto cleanup;
	}
	if (resp->nvs_type != HN_NVS_TYPE_RXBUF_CONNRESP) {
		if_printf(sc->hn_ifp, "not rxbuf conn resp, type %u\n",
		    resp->nvs_type);
		error = EINVAL;
		goto cleanup;
	}

	status = resp->nvs_status;
	vmbus_xact_put(xact);
	xact = NULL;

	if (status != HN_NVS_STATUS_OK) {
		if_printf(sc->hn_ifp, "rxbuf conn failed: %x\n", status);
		error = EIO;
		goto cleanup;
	}
	sc->hn_flags |= HN_FLAG_RXBUF_CONNECTED;

	return (0);

cleanup:
	if (xact != NULL)
		vmbus_xact_put(xact);
	hv_nv_destroy_rx_buffer(sc);
	return (error);
}

/*
 * Net VSC initialize send buffer with net VSP
 */
static int 
hv_nv_init_send_buffer_with_net_vsp(struct hn_softc *sc)
{
	struct vmbus_xact *xact = NULL;
	struct hn_nvs_chim_conn *chim;
	const struct hn_nvs_chim_connresp *resp;
	size_t resp_len;
	uint32_t status, sectsz;
	int error;

	/*
	 * Connect chimney sending buffer GPADL to the primary channel.
	 *
	 * NOTE:
	 * Only primary channel has chimney sending buffer connected to it.
	 * Sub-channels just share this chimney sending buffer.
	 */
	error = vmbus_chan_gpadl_connect(sc->hn_prichan,
  	    sc->hn_chim_dma.hv_paddr, NETVSC_SEND_BUFFER_SIZE,
	    &sc->hn_chim_gpadl);
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
	chim->nvs_gpadl = sc->hn_chim_gpadl;
	chim->nvs_sig = HN_NVS_CHIM_SIG;

	resp = hn_nvs_xact_execute(sc, xact, chim, sizeof(*chim), &resp_len);
	if (resp == NULL) {
		if_printf(sc->hn_ifp, "exec chim conn failed\n");
		error = EIO;
		goto cleanup;
	}
	if (resp_len < sizeof(*resp)) {
		if_printf(sc->hn_ifp, "invalid chim conn resp length %zu\n",
		    resp_len);
		error = EINVAL;
		goto cleanup;
	}
	if (resp->nvs_type != HN_NVS_TYPE_CHIM_CONNRESP) {
		if_printf(sc->hn_ifp, "not chim conn resp, type %u\n",
		    resp->nvs_type);
		error = EINVAL;
		goto cleanup;
	}

	status = resp->nvs_status;
	sectsz = resp->nvs_sectsz;
	vmbus_xact_put(xact);
	xact = NULL;

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

	sc->hn_chim_szmax = sectsz;
	sc->hn_chim_cnt = NETVSC_SEND_BUFFER_SIZE / sc->hn_chim_szmax;
	if (NETVSC_SEND_BUFFER_SIZE % sc->hn_chim_szmax != 0) {
		if_printf(sc->hn_ifp, "chimney sending sections are "
		    "not properly aligned\n");
	}
	if (sc->hn_chim_cnt % LONG_BIT != 0) {
		if_printf(sc->hn_ifp, "discard %d chimney sending sections\n",
		    sc->hn_chim_cnt % LONG_BIT);
	}

	sc->hn_chim_bmap_cnt = sc->hn_chim_cnt / LONG_BIT;
	sc->hn_chim_bmap = malloc(sc->hn_chim_bmap_cnt * sizeof(u_long),
	    M_NETVSC, M_WAITOK | M_ZERO);

	/* Done! */
	sc->hn_flags |= HN_FLAG_CHIM_CONNECTED;
	if (bootverbose) {
		if_printf(sc->hn_ifp, "chimney sending buffer %d/%d\n",
		    sc->hn_chim_szmax, sc->hn_chim_cnt);
	}
	return 0;

cleanup:
	if (xact != NULL)
		vmbus_xact_put(xact);
	hv_nv_destroy_send_buffer(sc);
	return (error);
}

/*
 * Net VSC destroy receive buffer
 */
static int
hv_nv_destroy_rx_buffer(struct hn_softc *sc)
{
	int ret = 0;

	if (sc->hn_flags & HN_FLAG_RXBUF_CONNECTED) {
		struct hn_nvs_rxbuf_disconn disconn;

		/*
		 * Disconnect RXBUF from NVS.
		 */
		memset(&disconn, 0, sizeof(disconn));
		disconn.nvs_type = HN_NVS_TYPE_RXBUF_DISCONN;
		disconn.nvs_sig = HN_NVS_RXBUF_SIG;

		/* NOTE: No response. */
		ret = hn_nvs_req_send(sc, &disconn, sizeof(disconn));
		if (ret != 0) {
			if_printf(sc->hn_ifp,
			    "send rxbuf disconn failed: %d\n", ret);
			return (ret);
		}
		sc->hn_flags &= ~HN_FLAG_RXBUF_CONNECTED;
	}
		
	if (sc->hn_rxbuf_gpadl != 0) {
		/*
		 * Disconnect RXBUF from primary channel.
		 */
		ret = vmbus_chan_gpadl_disconnect(sc->hn_prichan,
		    sc->hn_rxbuf_gpadl);
		if (ret != 0) {
			if_printf(sc->hn_ifp,
			    "rxbuf disconn failed: %d\n", ret);
			return (ret);
		}
		sc->hn_rxbuf_gpadl = 0;
	}
	return (ret);
}

/*
 * Net VSC destroy send buffer
 */
static int
hv_nv_destroy_send_buffer(struct hn_softc *sc)
{
	int ret = 0;

	if (sc->hn_flags & HN_FLAG_CHIM_CONNECTED) {
		struct hn_nvs_chim_disconn disconn;

		/*
		 * Disconnect chimney sending buffer from NVS.
		 */
		memset(&disconn, 0, sizeof(disconn));
		disconn.nvs_type = HN_NVS_TYPE_CHIM_DISCONN;
		disconn.nvs_sig = HN_NVS_CHIM_SIG;

		/* NOTE: No response. */
		ret = hn_nvs_req_send(sc, &disconn, sizeof(disconn));
		if (ret != 0) {
			if_printf(sc->hn_ifp,
			    "send chim disconn failed: %d\n", ret);
			return (ret);
		}
		sc->hn_flags &= ~HN_FLAG_CHIM_CONNECTED;
	}
		
	if (sc->hn_chim_gpadl != 0) {
		/*
		 * Disconnect chimney sending buffer from primary channel.
		 */
		ret = vmbus_chan_gpadl_disconnect(sc->hn_prichan,
		    sc->hn_chim_gpadl);
		if (ret != 0) {
			if_printf(sc->hn_ifp,
			    "chim disconn failed: %d\n", ret);
			return (ret);
		}
		sc->hn_chim_gpadl = 0;
	}

	if (sc->hn_chim_bmap != NULL) {
		free(sc->hn_chim_bmap, M_NETVSC);
		sc->hn_chim_bmap = NULL;
	}

	return (ret);
}

static int
hv_nv_negotiate_nvsp_protocol(struct hn_softc *sc, uint32_t nvs_ver)
{
	struct vmbus_xact *xact;
	struct hn_nvs_init *init;
	const struct hn_nvs_init_resp *resp;
	size_t resp_len;
	uint32_t status;

	xact = vmbus_xact_get(sc->hn_xact, sizeof(*init));
	if (xact == NULL) {
		if_printf(sc->hn_ifp, "no xact for nvs init\n");
		return (ENXIO);
	}
	init = vmbus_xact_req_data(xact);
	init->nvs_type = HN_NVS_TYPE_INIT;
	init->nvs_ver_min = nvs_ver;
	init->nvs_ver_max = nvs_ver;

	resp = hn_nvs_xact_execute(sc, xact, init, sizeof(*init), &resp_len);
	if (resp == NULL) {
		if_printf(sc->hn_ifp, "exec init failed\n");
		vmbus_xact_put(xact);
		return (EIO);
	}
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
	error = hn_nvs_req_send(sc, &conf, sizeof(conf));
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
	int rxbuf_size;

	/*
	 * Negotiate the NVSP version.  Try the latest NVSP first.
	 */
	for (i = protocol_number - 1; i >= 0; i--) {
		if (hv_nv_negotiate_nvsp_protocol(sc, protocol_list[i]) == 0) {
			sc->hn_nvs_ver = protocol_list[i];
			sc->hn_ndis_ver = NDIS_VERSION_6_30;
			if (sc->hn_nvs_ver <= NVSP_PROTOCOL_VERSION_4)
				sc->hn_ndis_ver = NDIS_VERSION_6_1;
			if (bootverbose) {
				if_printf(sc->hn_ifp, "NVS version 0x%x, "
				    "NDIS version %u.%u\n",
				    sc->hn_nvs_ver,
				    NDIS_VERSION_MAJOR(sc->hn_ndis_ver),
				    NDIS_VERSION_MINOR(sc->hn_ndis_ver));
			}
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
	if (sc->hn_nvs_ver >= NVSP_PROTOCOL_VERSION_2)
		ret = hv_nv_send_ndis_config(sc, ifp->if_mtu);

	/*
	 * Initialize NDIS.
	 */

	memset(&ndis, 0, sizeof(ndis));
	ndis.nvs_type = HN_NVS_TYPE_NDIS_INIT;
	ndis.nvs_ndis_major = NDIS_VERSION_MAJOR(sc->hn_ndis_ver);
	ndis.nvs_ndis_minor = NDIS_VERSION_MINOR(sc->hn_ndis_ver);

	/* NOTE: No response. */
	ret = hn_nvs_req_send(sc, &ndis, sizeof(ndis));
	if (ret != 0) {
		if_printf(sc->hn_ifp, "send nvs ndis init failed: %d\n", ret);
		goto cleanup;
	}

	/* Post the big receive buffer to NetVSP */
	if (sc->hn_nvs_ver <= NVSP_PROTOCOL_VERSION_2)
		rxbuf_size = NETVSC_RECEIVE_BUFFER_SIZE_LEGACY;
	else
		rxbuf_size = NETVSC_RECEIVE_BUFFER_SIZE;

	ret = hv_nv_init_rx_buffer_with_net_vsp(sc, rxbuf_size);
	if (ret == 0)
		ret = hv_nv_init_send_buffer_with_net_vsp(sc);

cleanup:
	return (ret);
}

/*
 * Net VSC disconnect from VSP
 */
static void
hv_nv_disconnect_from_vsp(struct hn_softc *sc)
{
	hv_nv_destroy_rx_buffer(sc);
	hv_nv_destroy_send_buffer(sc);
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
int
hv_nv_on_device_add(struct hn_softc *sc, struct hn_rx_ring *rxr)
{
	struct vmbus_channel *chan = sc->hn_prichan;
	int ret = 0;

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

	return (0);

close:
	/* Now, we can close the channel safely */
	vmbus_chan_close(chan);
cleanup:
	return (ret);
}

/*
 * Net VSC on device remove
 */
int
hv_nv_on_device_remove(struct hn_softc *sc, boolean_t destroy_channel)
{
	
	hv_nv_disconnect_from_vsp(sc);

	/* Now, we can close the channel safely */

	vmbus_chan_close(sc->hn_prichan);

	return (0);
}

static void
hn_nvs_sent_xact(struct hn_send_ctx *sndc,
    struct hn_softc *sc __unused, struct vmbus_channel *chan __unused,
    const void *data, int dlen)
{

	vmbus_xact_wakeup(sndc->hn_cbarg, data, dlen);
}

static void
hn_nvs_sent_none(struct hn_send_ctx *sndc __unused,
    struct hn_softc *sc __unused, struct vmbus_channel *chan __unused,
    const void *data __unused, int dlen __unused)
{
	/* EMPTY */
}

void
hn_chim_free(struct hn_softc *sc, uint32_t chim_idx)
{
	u_long mask;
	uint32_t idx;

	idx = chim_idx / LONG_BIT;
	KASSERT(idx < sc->hn_chim_bmap_cnt,
	    ("invalid chimney index 0x%x", chim_idx));

	mask = 1UL << (chim_idx % LONG_BIT);
	KASSERT(sc->hn_chim_bmap[idx] & mask,
	    ("index bitmap 0x%lx, chimney index %u, "
	     "bitmap idx %d, bitmask 0x%lx",
	     sc->hn_chim_bmap[idx], chim_idx, idx, mask));

	atomic_clear_long(&sc->hn_chim_bmap[idx], mask);
}

/*
 * Net VSC on send completion
 */
static void
hv_nv_on_send_completion(struct hn_softc *sc, struct vmbus_channel *chan,
    const struct vmbus_chanpkt_hdr *pkt)
{
	struct hn_send_ctx *sndc;

	sndc = (struct hn_send_ctx *)(uintptr_t)pkt->cph_xactid;
	sndc->hn_cb(sndc, sc, chan, VMBUS_CHANPKT_CONST_DATA(pkt),
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
hv_nv_on_receive(struct hn_softc *sc, struct hn_rx_ring *rxr,
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
		hv_rf_on_receive(sc, rxr,
		    rxr->hn_rxbuf + pkt->cp_rxbuf[i].rb_ofs,
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
	void *buffer;
	int bufferlen = NETVSC_PACKET_SIZE;

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
					hv_nv_on_send_completion(sc, chan, pkt);
					break;
				case VMBUS_CHANPKT_TYPE_RXBUF:
					hv_nv_on_receive(sc, rxr, chan, pkt);
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
