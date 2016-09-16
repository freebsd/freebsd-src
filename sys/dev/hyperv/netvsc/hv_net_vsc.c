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
#include <dev/hyperv/netvsc/hv_rndis_filter.h>
#include <dev/hyperv/netvsc/if_hnreg.h>
#include <dev/hyperv/netvsc/if_hnvar.h>

MALLOC_DEFINE(M_NETVSC, "netvsc", "Hyper-V netvsc driver");

/*
 * Forward declarations
 */
static int  hn_nvs_conn_chim(struct hn_softc *sc);
static int  hn_nvs_conn_rxbuf(struct hn_softc *);
static int  hn_nvs_disconn_chim(struct hn_softc *sc);
static int  hn_nvs_disconn_rxbuf(struct hn_softc *sc);
static void hn_nvs_sent_none(struct hn_send_ctx *sndc,
    struct hn_softc *, struct vmbus_channel *chan,
    const void *, int);

struct hn_send_ctx	hn_send_ctx_none =
    HN_SEND_CTX_INITIALIZER(hn_nvs_sent_none, NULL);

static const uint32_t		hn_nvs_version[] = {
	HN_NVS_VERSION_5,
	HN_NVS_VERSION_4,
	HN_NVS_VERSION_2,
	HN_NVS_VERSION_1
};

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

static const void *
hn_nvs_xact_execute(struct hn_softc *sc, struct vmbus_xact *xact,
    void *req, int reqlen, size_t *resplen0, uint32_t type)
{
	struct hn_send_ctx sndc;
	size_t resplen, min_resplen = *resplen0;
	const struct hn_nvs_hdr *hdr;
	int error;

	KASSERT(min_resplen >= sizeof(*hdr),
	    ("invalid minimum response len %zu", min_resplen));

	/*
	 * Execute the xact setup by the caller.
	 */
	hn_send_ctx_init_simple(&sndc, hn_nvs_sent_xact, xact);

	vmbus_xact_activate(xact);
	error = hn_nvs_send(sc->hn_prichan, VMBUS_CHANPKT_FLAG_RC,
	    req, reqlen, &sndc);
	if (error) {
		vmbus_xact_deactivate(xact);
		return (NULL);
	}
	hdr = vmbus_xact_wait(xact, &resplen);

	/*
	 * Check this NVS response message.
	 */
	if (resplen < min_resplen) {
		if_printf(sc->hn_ifp, "invalid NVS resp len %zu\n", resplen);
		return (NULL);
	}
	if (hdr->nvs_type != type) {
		if_printf(sc->hn_ifp, "unexpected NVS resp 0x%08x, "
		    "expect 0x%08x\n", hdr->nvs_type, type);
		return (NULL);
	}
	/* All pass! */
	*resplen0 = resplen;
	return (hdr);
}

static __inline int
hn_nvs_req_send(struct hn_softc *sc, void *req, int reqlen)
{

	return (hn_nvs_send(sc->hn_prichan, VMBUS_CHANPKT_FLAG_NONE,
	    req, reqlen, &hn_send_ctx_none));
}

static int 
hn_nvs_conn_rxbuf(struct hn_softc *sc)
{
	struct vmbus_xact *xact = NULL;
	struct hn_nvs_rxbuf_conn *conn;
	const struct hn_nvs_rxbuf_connresp *resp;
	size_t resp_len;
	uint32_t status;
	int error, rxbuf_size;

	/*
	 * Limit RXBUF size for old NVS.
	 */
	if (sc->hn_nvs_ver <= HN_NVS_VERSION_2)
		rxbuf_size = NETVSC_RECEIVE_BUFFER_SIZE_LEGACY;
	else
		rxbuf_size = NETVSC_RECEIVE_BUFFER_SIZE;

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
		if_printf(sc->hn_ifp, "rxbuf gpadl conn failed: %d\n",
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

	resp_len = sizeof(*resp);
	resp = hn_nvs_xact_execute(sc, xact, conn, sizeof(*conn), &resp_len,
	    HN_NVS_TYPE_RXBUF_CONNRESP);
	if (resp == NULL) {
		if_printf(sc->hn_ifp, "exec nvs rxbuf conn failed\n");
		error = EIO;
		goto cleanup;
	}

	status = resp->nvs_status;
	vmbus_xact_put(xact);
	xact = NULL;

	if (status != HN_NVS_STATUS_OK) {
		if_printf(sc->hn_ifp, "nvs rxbuf conn failed: %x\n", status);
		error = EIO;
		goto cleanup;
	}
	sc->hn_flags |= HN_FLAG_RXBUF_CONNECTED;

	return (0);

cleanup:
	if (xact != NULL)
		vmbus_xact_put(xact);
	hn_nvs_disconn_rxbuf(sc);
	return (error);
}

static int 
hn_nvs_conn_chim(struct hn_softc *sc)
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
		if_printf(sc->hn_ifp, "chim gpadl conn failed: %d\n", error);
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

	resp_len = sizeof(*resp);
	resp = hn_nvs_xact_execute(sc, xact, chim, sizeof(*chim), &resp_len,
	    HN_NVS_TYPE_CHIM_CONNRESP);
	if (resp == NULL) {
		if_printf(sc->hn_ifp, "exec nvs chim conn failed\n");
		error = EIO;
		goto cleanup;
	}

	status = resp->nvs_status;
	sectsz = resp->nvs_sectsz;
	vmbus_xact_put(xact);
	xact = NULL;

	if (status != HN_NVS_STATUS_OK) {
		if_printf(sc->hn_ifp, "nvs chim conn failed: %x\n", status);
		error = EIO;
		goto cleanup;
	}
	if (sectsz == 0) {
		if_printf(sc->hn_ifp, "zero chimney sending buffer "
		    "section size\n");
		return (0);
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
	return (0);

cleanup:
	if (xact != NULL)
		vmbus_xact_put(xact);
	hn_nvs_disconn_chim(sc);
	return (error);
}

static int
hn_nvs_disconn_rxbuf(struct hn_softc *sc)
{
	int error;

	if (sc->hn_flags & HN_FLAG_RXBUF_CONNECTED) {
		struct hn_nvs_rxbuf_disconn disconn;

		/*
		 * Disconnect RXBUF from NVS.
		 */
		memset(&disconn, 0, sizeof(disconn));
		disconn.nvs_type = HN_NVS_TYPE_RXBUF_DISCONN;
		disconn.nvs_sig = HN_NVS_RXBUF_SIG;

		/* NOTE: No response. */
		error = hn_nvs_req_send(sc, &disconn, sizeof(disconn));
		if (error) {
			if_printf(sc->hn_ifp,
			    "send nvs rxbuf disconn failed: %d\n", error);
			return (error);
		}
		sc->hn_flags &= ~HN_FLAG_RXBUF_CONNECTED;
	}

	if (sc->hn_rxbuf_gpadl != 0) {
		/*
		 * Disconnect RXBUF from primary channel.
		 */
		error = vmbus_chan_gpadl_disconnect(sc->hn_prichan,
		    sc->hn_rxbuf_gpadl);
		if (error) {
			if_printf(sc->hn_ifp,
			    "rxbuf gpadl disconn failed: %d\n", error);
			return (error);
		}
		sc->hn_rxbuf_gpadl = 0;
	}
	return (0);
}

static int
hn_nvs_disconn_chim(struct hn_softc *sc)
{
	int error;

	if (sc->hn_flags & HN_FLAG_CHIM_CONNECTED) {
		struct hn_nvs_chim_disconn disconn;

		/*
		 * Disconnect chimney sending buffer from NVS.
		 */
		memset(&disconn, 0, sizeof(disconn));
		disconn.nvs_type = HN_NVS_TYPE_CHIM_DISCONN;
		disconn.nvs_sig = HN_NVS_CHIM_SIG;

		/* NOTE: No response. */
		error = hn_nvs_req_send(sc, &disconn, sizeof(disconn));
		if (error) {
			if_printf(sc->hn_ifp,
			    "send nvs chim disconn failed: %d\n", error);
			return (error);
		}
		sc->hn_flags &= ~HN_FLAG_CHIM_CONNECTED;
	}

	if (sc->hn_chim_gpadl != 0) {
		/*
		 * Disconnect chimney sending buffer from primary channel.
		 */
		error = vmbus_chan_gpadl_disconnect(sc->hn_prichan,
		    sc->hn_chim_gpadl);
		if (error) {
			if_printf(sc->hn_ifp,
			    "chim gpadl disconn failed: %d\n", error);
			return (error);
		}
		sc->hn_chim_gpadl = 0;
	}

	if (sc->hn_chim_bmap != NULL) {
		free(sc->hn_chim_bmap, M_NETVSC);
		sc->hn_chim_bmap = NULL;
	}
	return (0);
}

static int
hn_nvs_doinit(struct hn_softc *sc, uint32_t nvs_ver)
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

	resp_len = sizeof(*resp);
	resp = hn_nvs_xact_execute(sc, xact, init, sizeof(*init), &resp_len,
	    HN_NVS_TYPE_INIT_RESP);
	if (resp == NULL) {
		if_printf(sc->hn_ifp, "exec init failed\n");
		vmbus_xact_put(xact);
		return (EIO);
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
 * Configure MTU and enable VLAN.
 */
static int
hn_nvs_conf_ndis(struct hn_softc *sc, int mtu)
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

static int
hn_nvs_init_ndis(struct hn_softc *sc)
{
	struct hn_nvs_ndis_init ndis;
	int error;

	memset(&ndis, 0, sizeof(ndis));
	ndis.nvs_type = HN_NVS_TYPE_NDIS_INIT;
	ndis.nvs_ndis_major = HN_NDIS_VERSION_MAJOR(sc->hn_ndis_ver);
	ndis.nvs_ndis_minor = HN_NDIS_VERSION_MINOR(sc->hn_ndis_ver);

	/* NOTE: No response. */
	error = hn_nvs_req_send(sc, &ndis, sizeof(ndis));
	if (error)
		if_printf(sc->hn_ifp, "send nvs ndis init failed: %d\n", error);
	return (error);
}

static int
hn_nvs_init(struct hn_softc *sc)
{
	int i;

	for (i = 0; i < nitems(hn_nvs_version); ++i) {
		int error;

		error = hn_nvs_doinit(sc, hn_nvs_version[i]);
		if (!error) {
			sc->hn_nvs_ver = hn_nvs_version[i];

			/* Set NDIS version according to NVS version. */
			sc->hn_ndis_ver = HN_NDIS_VERSION_6_30;
			if (sc->hn_nvs_ver <= HN_NVS_VERSION_4)
				sc->hn_ndis_ver = HN_NDIS_VERSION_6_1;

			if (bootverbose) {
				if_printf(sc->hn_ifp, "NVS version 0x%x, "
				    "NDIS version %u.%u\n", sc->hn_nvs_ver,
				    HN_NDIS_VERSION_MAJOR(sc->hn_ndis_ver),
				    HN_NDIS_VERSION_MINOR(sc->hn_ndis_ver));
			}
			return (0);
		}
	}
	if_printf(sc->hn_ifp, "no NVS available\n");
	return (ENXIO);
}

int
hn_nvs_attach(struct hn_softc *sc, int mtu)
{
	int error;

	/*
	 * Initialize NVS.
	 */
	error = hn_nvs_init(sc);
	if (error)
		return (error);

	if (sc->hn_nvs_ver >= HN_NVS_VERSION_2) {
		/*
		 * Configure NDIS before initializing it.
		 */
		error = hn_nvs_conf_ndis(sc, mtu);
		if (error)
			return (error);
	}

	/*
	 * Initialize NDIS.
	 */
	error = hn_nvs_init_ndis(sc);
	if (error)
		return (error);

	/*
	 * Connect RXBUF.
	 */
	error = hn_nvs_conn_rxbuf(sc);
	if (error)
		return (error);

	/*
	 * Connect chimney sending buffer.
	 */
	error = hn_nvs_conn_chim(sc);
	if (error)
		return (error);
	return (0);
}

/*
 * Net VSC disconnect from VSP
 */
static void
hv_nv_disconnect_from_vsp(struct hn_softc *sc)
{
	hn_nvs_disconn_rxbuf(sc);
	hn_nvs_disconn_chim(sc);
}

/*
 * Net VSC on device remove
 */
int
hv_nv_on_device_remove(struct hn_softc *sc)
{
	
	hv_nv_disconnect_from_vsp(sc);
	return (0);
}

void
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

int
hn_nvs_alloc_subchans(struct hn_softc *sc, int *nsubch0)
{
	struct vmbus_xact *xact;
	struct hn_nvs_subch_req *req;
	const struct hn_nvs_subch_resp *resp;
	int error, nsubch_req;
	uint32_t nsubch;
	size_t resp_len;

	nsubch_req = *nsubch0;
	KASSERT(nsubch_req > 0, ("invalid # of sub-channels %d", nsubch_req));

	xact = vmbus_xact_get(sc->hn_xact, sizeof(*req));
	if (xact == NULL) {
		if_printf(sc->hn_ifp, "no xact for nvs subch alloc\n");
		return (ENXIO);
	}
	req = vmbus_xact_req_data(xact);
	req->nvs_type = HN_NVS_TYPE_SUBCH_REQ;
	req->nvs_op = HN_NVS_SUBCH_OP_ALLOC;
	req->nvs_nsubch = nsubch_req;

	resp_len = sizeof(*resp);
	resp = hn_nvs_xact_execute(sc, xact, req, sizeof(*req), &resp_len,
	    HN_NVS_TYPE_SUBCH_RESP);
	if (resp == NULL) {
		if_printf(sc->hn_ifp, "exec nvs subch alloc failed\n");
		error = EIO;
		goto done;
	}
	if (resp->nvs_status != HN_NVS_STATUS_OK) {
		if_printf(sc->hn_ifp, "nvs subch alloc failed: %x\n",
		    resp->nvs_status);
		error = EIO;
		goto done;
	}

	nsubch = resp->nvs_nsubch;
	if (nsubch > nsubch_req) {
		if_printf(sc->hn_ifp, "%u subchans are allocated, "
		    "requested %d\n", nsubch, nsubch_req);
		nsubch = nsubch_req;
	}
	*nsubch0 = nsubch;
	error = 0;
done:
	vmbus_xact_put(xact);
	return (error);
}
