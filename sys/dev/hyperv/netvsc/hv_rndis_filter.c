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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_var.h>
#include <net/ethernet.h>
#include <sys/types.h>
#include <machine/atomic.h>
#include <sys/sema.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <dev/hyperv/include/hyperv.h>
#include <dev/hyperv/include/vmbus_xact.h>
#include <dev/hyperv/netvsc/hv_net_vsc.h>
#include <dev/hyperv/netvsc/hv_rndis.h>
#include <dev/hyperv/netvsc/hv_rndis_filter.h>
#include <dev/hyperv/netvsc/if_hnreg.h>
#include <dev/hyperv/netvsc/ndis.h>

#define HV_RF_RECVINFO_VLAN	0x1
#define HV_RF_RECVINFO_CSUM	0x2
#define HV_RF_RECVINFO_HASHINF	0x4
#define HV_RF_RECVINFO_HASHVAL	0x8
#define HV_RF_RECVINFO_ALL		\
	(HV_RF_RECVINFO_VLAN |		\
	 HV_RF_RECVINFO_CSUM |		\
	 HV_RF_RECVINFO_HASHINF |	\
	 HV_RF_RECVINFO_HASHVAL)

#define HN_RNDIS_RID_COMPAT_MASK	0xffff
#define HN_RNDIS_RID_COMPAT_MAX		HN_RNDIS_RID_COMPAT_MASK

#define HN_RNDIS_XFER_SIZE		2048

/*
 * Forward declarations
 */
static void hv_rf_receive_indicate_status(struct hn_softc *sc,
    const rndis_msg *response);
static void hv_rf_receive_data(struct hn_rx_ring *rxr,
    const void *data, int dlen);
static int hv_rf_query_device_mac(struct hn_softc *sc, uint8_t *eaddr);
static int hv_rf_query_device_link_status(struct hn_softc *sc,
    uint32_t *link_status);
static int  hv_rf_init_device(struct hn_softc *sc);

static int hn_rndis_query(struct hn_softc *sc, uint32_t oid,
    const void *idata, size_t idlen, void *odata, size_t *odlen0);
static int hn_rndis_set(struct hn_softc *sc, uint32_t oid, const void *data,
    size_t dlen);
static int hn_rndis_conf_offload(struct hn_softc *sc);
static int hn_rndis_get_rsscaps(struct hn_softc *sc, int *rxr_cnt);
static int hn_rndis_conf_rss(struct hn_softc *sc, int nchan);

static __inline uint32_t
hn_rndis_rid(struct hn_softc *sc)
{
	uint32_t rid;

again:
	rid = atomic_fetchadd_int(&sc->hn_rndis_rid, 1);
	if (rid == 0)
		goto again;

	/* Use upper 16 bits for non-compat RNDIS messages. */
	return ((rid & 0xffff) << 16);
}

/*
 * Set the Per-Packet-Info with the specified type
 */
void *
hv_set_rppi_data(rndis_msg *rndis_mesg, uint32_t rppi_size,
	int pkt_type)
{
	rndis_packet *rndis_pkt;
	rndis_per_packet_info *rppi;

	rndis_pkt = &rndis_mesg->msg.packet;
	rndis_pkt->data_offset += rppi_size;

	rppi = (rndis_per_packet_info *)((char *)rndis_pkt +
	    rndis_pkt->per_pkt_info_offset + rndis_pkt->per_pkt_info_length);

	rppi->size = rppi_size;
	rppi->type = pkt_type;
	rppi->per_packet_info_offset = sizeof(rndis_per_packet_info);

	rndis_pkt->per_pkt_info_length += rppi_size;

	return (rppi);
}

/*
 * RNDIS filter receive indicate status
 */
static void 
hv_rf_receive_indicate_status(struct hn_softc *sc, const rndis_msg *response)
{
	const rndis_indicate_status *indicate = &response->msg.indicate_status;
		
	switch(indicate->status) {
	case RNDIS_STATUS_MEDIA_CONNECT:
		netvsc_linkstatus_callback(sc, 1);
		break;
	case RNDIS_STATUS_MEDIA_DISCONNECT:
		netvsc_linkstatus_callback(sc, 0);
		break;
	default:
		/* TODO: */
		if_printf(sc->hn_ifp,
		    "unknown status %d received\n", indicate->status);
		break;
	}
}

static int
hv_rf_find_recvinfo(const rndis_packet *rpkt, struct hn_recvinfo *info)
{
	const struct rndis_pktinfo *pi;
	uint32_t mask = 0, len;

	info->vlan_info = HN_NDIS_VLAN_INFO_INVALID;
	info->csum_info = NULL;
	info->hash_info = NULL;
	info->hash_value = NULL;

	if (rpkt->per_pkt_info_offset == 0)
		return (0);
	if (__predict_false(rpkt->per_pkt_info_offset &
	    (RNDIS_PKTINFO_ALIGN - 1)))
		return (EINVAL);
	if (__predict_false(rpkt->per_pkt_info_offset <
	    RNDIS_PACKET_MSG_OFFSET_MIN))
		return (EINVAL);

	pi = (const struct rndis_pktinfo *)
	    ((const uint8_t *)rpkt + rpkt->per_pkt_info_offset);
	len = rpkt->per_pkt_info_length;

	while (len != 0) {
		const void *data;
		uint32_t dlen;

		if (__predict_false(len < sizeof(*pi)))
			return (EINVAL);
		if (__predict_false(len < pi->rm_size))
			return (EINVAL);
		len -= pi->rm_size;

		if (__predict_false(pi->rm_size & (RNDIS_PKTINFO_ALIGN - 1)))
			return (EINVAL);
		if (__predict_false(pi->rm_size < pi->rm_pktinfooffset))
			return (EINVAL);
		dlen = pi->rm_size - pi->rm_pktinfooffset;
		data = pi->rm_data;

		switch (pi->rm_type) {
		case ieee_8021q_info:
			if (__predict_false(dlen < NDIS_VLAN_INFO_SIZE))
				return (EINVAL);
			info->vlan_info = *((const uint32_t *)data);
			mask |= HV_RF_RECVINFO_VLAN;
			break;

		case tcpip_chksum_info:
			if (__predict_false(dlen <
			    sizeof(rndis_tcp_ip_csum_info)))
				return (EINVAL);
			info->csum_info = data;
			mask |= HV_RF_RECVINFO_CSUM;
			break;

		case nbl_hash_value:
			if (__predict_false(dlen <
			    sizeof(struct rndis_hash_value)))
				return (EINVAL);
			info->hash_value = data;
			mask |= HV_RF_RECVINFO_HASHVAL;
			break;

		case nbl_hash_info:
			if (__predict_false(dlen <
			    sizeof(struct rndis_hash_info)))
				return (EINVAL);
			info->hash_info = data;
			mask |= HV_RF_RECVINFO_HASHINF;
			break;

		default:
			goto next;
		}

		if (mask == HV_RF_RECVINFO_ALL) {
			/* All found; done */
			break;
		}
next:
		pi = (const struct rndis_pktinfo *)
		    ((const uint8_t *)pi + pi->rm_size);
	}
	return (0);
}

/*
 * RNDIS filter receive data
 */
static void
hv_rf_receive_data(struct hn_rx_ring *rxr, const void *data, int dlen)
{
	const rndis_msg *message = data;
	const rndis_packet *rndis_pkt;
	uint32_t data_offset;
	struct hn_recvinfo info;

	rndis_pkt = &message->msg.packet;

	/*
	 * Fixme:  Handle multiple rndis pkt msgs that may be enclosed in this
	 * netvsc packet (ie tot_data_buf_len != message_length)
	 */

	/* Remove rndis header, then pass data packet up the stack */
	data_offset = RNDIS_HEADER_SIZE + rndis_pkt->data_offset;

	dlen -= data_offset;
	if (dlen < rndis_pkt->data_length) {
		if_printf(rxr->hn_ifp,
		    "total length %u is less than data length %u\n",
		    dlen, rndis_pkt->data_length);
		return;
	}

	dlen = rndis_pkt->data_length;
	data = (const uint8_t *)data + data_offset;

	if (hv_rf_find_recvinfo(rndis_pkt, &info)) {
		if_printf(rxr->hn_ifp, "recvinfo parsing failed\n");
		return;
	}
	netvsc_recv(rxr, data, dlen, &info);
}

/*
 * RNDIS filter on receive
 */
int
hv_rf_on_receive(struct hn_softc *sc, struct hn_rx_ring *rxr,
    const void *data, int dlen)
{
	const rndis_msg *rndis_hdr;
	const struct rndis_comp_hdr *comp;

	rndis_hdr = data;
	switch (rndis_hdr->ndis_msg_type) {
	/* data message */
	case REMOTE_NDIS_PACKET_MSG:
		hv_rf_receive_data(rxr, data, dlen);
		break;

	/* completion messages */
	case REMOTE_NDIS_INITIALIZE_CMPLT:
	case REMOTE_NDIS_QUERY_CMPLT:
	case REMOTE_NDIS_SET_CMPLT:
	case REMOTE_NDIS_KEEPALIVE_CMPLT:
		comp = data;
		KASSERT(comp->rm_rid > HN_RNDIS_RID_COMPAT_MAX,
		    ("invalid rid 0x%08x\n", comp->rm_rid));
		vmbus_xact_ctx_wakeup(sc->hn_xact, comp, dlen);
		break;

	/* notification message */
	case REMOTE_NDIS_INDICATE_STATUS_MSG:
		hv_rf_receive_indicate_status(sc, rndis_hdr);
		break;

	case REMOTE_NDIS_RESET_CMPLT:
		/*
		 * Reset completed, no rid.
		 *
		 * NOTE:
		 * RESET is not issued by hn(4), so this message should
		 * _not_ be observed.
		 */
		if_printf(sc->hn_ifp, "RESET CMPLT received\n");
		break;

	default:
		if_printf(sc->hn_ifp, "unknown RNDIS message 0x%x\n",
			rndis_hdr->ndis_msg_type);
		break;
	}
	return (0);
}

/*
 * RNDIS filter query device MAC address
 */
static int
hv_rf_query_device_mac(struct hn_softc *sc, uint8_t *eaddr)
{
	size_t eaddr_len;
	int error;

	eaddr_len = ETHER_ADDR_LEN;
	error = hn_rndis_query(sc, OID_802_3_PERMANENT_ADDRESS, NULL, 0,
	    eaddr, &eaddr_len);
	if (error)
		return (error);
	if (eaddr_len != ETHER_ADDR_LEN) {
		if_printf(sc->hn_ifp, "invalid eaddr len %zu\n", eaddr_len);
		return (EINVAL);
	}
	return (0);
}

/*
 * RNDIS filter query device link status
 */
static int
hv_rf_query_device_link_status(struct hn_softc *sc, uint32_t *link_status)
{
	size_t size;
	int error;

	size = sizeof(*link_status);
	error = hn_rndis_query(sc, OID_GEN_MEDIA_CONNECT_STATUS, NULL, 0,
	    link_status, &size);
	if (error)
		return (error);
	if (size != sizeof(uint32_t)) {
		if_printf(sc->hn_ifp, "invalid link status len %zu\n", size);
		return (EINVAL);
	}
	return (0);
}

static uint8_t netvsc_hash_key[NDIS_HASH_KEYSIZE_TOEPLITZ] = {
	0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
	0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
	0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
	0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
	0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa
};

static const void *
hn_rndis_xact_exec1(struct hn_softc *sc, struct vmbus_xact *xact, size_t reqlen,
    struct hn_send_ctx *sndc, size_t *comp_len)
{
	struct vmbus_gpa gpa[HN_XACT_REQ_PGCNT];
	int gpa_cnt, error;
	bus_addr_t paddr;

	KASSERT(reqlen <= HN_XACT_REQ_SIZE && reqlen > 0,
	    ("invalid request length %zu", reqlen));

	/*
	 * Setup the SG list.
	 */
	paddr = vmbus_xact_req_paddr(xact);
	KASSERT((paddr & PAGE_MASK) == 0,
	    ("vmbus xact request is not page aligned 0x%jx", (uintmax_t)paddr));
	for (gpa_cnt = 0; gpa_cnt < HN_XACT_REQ_PGCNT; ++gpa_cnt) {
		int len = PAGE_SIZE;

		if (reqlen == 0)
			break;
		if (reqlen < len)
			len = reqlen;

		gpa[gpa_cnt].gpa_page = atop(paddr) + gpa_cnt;
		gpa[gpa_cnt].gpa_len = len;
		gpa[gpa_cnt].gpa_ofs = 0;

		reqlen -= len;
	}
	KASSERT(reqlen == 0, ("still have %zu request data left", reqlen));

	/*
	 * Send this RNDIS control message and wait for its completion
	 * message.
	 */
	vmbus_xact_activate(xact);
	error = hv_nv_on_send(sc->hn_prichan, HN_NVS_RNDIS_MTYPE_CTRL, sndc,
	    gpa, gpa_cnt);
	if (error) {
		vmbus_xact_deactivate(xact);
		if_printf(sc->hn_ifp, "RNDIS ctrl send failed: %d\n", error);
		return (NULL);
	}
	return (vmbus_xact_wait(xact, comp_len));
}

static const void *
hn_rndis_xact_execute(struct hn_softc *sc, struct vmbus_xact *xact, uint32_t rid,
    size_t reqlen, size_t *comp_len0, uint32_t comp_type)
{
	const struct rndis_comp_hdr *comp;
	size_t comp_len, min_complen = *comp_len0;

	KASSERT(rid > HN_RNDIS_RID_COMPAT_MAX, ("invalid rid %u\n", rid));
	KASSERT(min_complen >= sizeof(*comp),
	    ("invalid minimum complete len %zu", min_complen));

	/*
	 * Execute the xact setup by the caller.
	 */
	comp = hn_rndis_xact_exec1(sc, xact, reqlen, &hn_send_ctx_none,
	    &comp_len);
	if (comp == NULL)
		return (NULL);

	/*
	 * Check this RNDIS complete message.
	 */
	if (comp_len < min_complen) {
		if (comp_len >= sizeof(*comp)) {
			/* rm_status field is valid */
			if_printf(sc->hn_ifp, "invalid RNDIS comp len %zu, "
			    "status 0x%08x\n", comp_len, comp->rm_status);
		} else {
			if_printf(sc->hn_ifp, "invalid RNDIS comp len %zu\n",
			    comp_len);
		}
		return (NULL);
	}
	if (comp->rm_len < min_complen) {
		if_printf(sc->hn_ifp, "invalid RNDIS comp msglen %u\n",
		    comp->rm_len);
		return (NULL);
	}
	if (comp->rm_type != comp_type) {
		if_printf(sc->hn_ifp, "unexpected RNDIS comp 0x%08x, "
		    "expect 0x%08x\n", comp->rm_type, comp_type);
		return (NULL);
	}
	if (comp->rm_rid != rid) {
		if_printf(sc->hn_ifp, "RNDIS comp rid mismatch %u, "
		    "expect %u\n", comp->rm_rid, rid);
		return (NULL);
	}
	/* All pass! */
	*comp_len0 = comp_len;
	return (comp);
}

static int
hn_rndis_query(struct hn_softc *sc, uint32_t oid,
    const void *idata, size_t idlen, void *odata, size_t *odlen0)
{
	struct rndis_query_req *req;
	const struct rndis_query_comp *comp;
	struct vmbus_xact *xact;
	size_t reqlen, odlen = *odlen0, comp_len;
	int error, ofs;
	uint32_t rid;

	reqlen = sizeof(*req) + idlen;
	xact = vmbus_xact_get(sc->hn_xact, reqlen);
	if (xact == NULL) {
		if_printf(sc->hn_ifp, "no xact for RNDIS query 0x%08x\n", oid);
		return (ENXIO);
	}
	rid = hn_rndis_rid(sc);
	req = vmbus_xact_req_data(xact);
	req->rm_type = REMOTE_NDIS_QUERY_MSG;
	req->rm_len = reqlen;
	req->rm_rid = rid;
	req->rm_oid = oid;
	/*
	 * XXX
	 * This is _not_ RNDIS Spec conforming:
	 * "This MUST be set to 0 when there is no input data
	 *  associated with the OID."
	 *
	 * If this field was set to 0 according to the RNDIS Spec,
	 * Hyper-V would set non-SUCCESS status in the query
	 * completion.
	 */
	req->rm_infobufoffset = RNDIS_QUERY_REQ_INFOBUFOFFSET;

	if (idlen > 0) {
		req->rm_infobuflen = idlen;
		/* Input data immediately follows RNDIS query. */
		memcpy(req + 1, idata, idlen);
	}

	comp_len = sizeof(*comp) + odlen;
	comp = hn_rndis_xact_execute(sc, xact, rid, reqlen, &comp_len,
	    REMOTE_NDIS_QUERY_CMPLT);
	if (comp == NULL) {
		if_printf(sc->hn_ifp, "exec RNDIS query 0x%08x failed\n", oid);
		error = EIO;
		goto done;
	}

	if (comp->rm_status != RNDIS_STATUS_SUCCESS) {
		if_printf(sc->hn_ifp, "RNDIS query 0x%08x failed: "
		    "status 0x%08x\n", oid, comp->rm_status);
		error = EIO;
		goto done;
	}
	if (comp->rm_infobuflen == 0 || comp->rm_infobufoffset == 0) {
		/* No output data! */
		if_printf(sc->hn_ifp, "RNDIS query 0x%08x, no data\n", oid);
		*odlen0 = 0;
		error = 0;
		goto done;
	}

	/*
	 * Check output data length and offset.
	 */
	/* ofs is the offset from the beginning of comp. */
	ofs = RNDIS_QUERY_COMP_INFOBUFABS(comp->rm_infobufoffset);
	if (ofs < sizeof(*comp) || ofs + comp->rm_infobuflen > comp_len) {
		if_printf(sc->hn_ifp, "RNDIS query invalid comp ib off/len, "
		    "%u/%u\n", comp->rm_infobufoffset, comp->rm_infobuflen);
		error = EINVAL;
		goto done;
	}

	/*
	 * Save output data.
	 */
	if (comp->rm_infobuflen < odlen)
		odlen = comp->rm_infobuflen;
	memcpy(odata, ((const uint8_t *)comp) + ofs, odlen);
	*odlen0 = odlen;

	error = 0;
done:
	vmbus_xact_put(xact);
	return (error);
}

static int
hn_rndis_get_rsscaps(struct hn_softc *sc, int *rxr_cnt)
{
	struct ndis_rss_caps in, caps;
	size_t caps_len;
	int error;

	/*
	 * Only NDIS 6.30+ is supported.
	 */
	KASSERT(sc->hn_ndis_ver >= NDIS_VERSION_6_30,
	    ("NDIS 6.30+ is required, NDIS version 0x%08x", sc->hn_ndis_ver));
	*rxr_cnt = 0;

	memset(&in, 0, sizeof(in));
	in.ndis_hdr.ndis_type = NDIS_OBJTYPE_RSS_CAPS;
	in.ndis_hdr.ndis_rev = NDIS_RSS_CAPS_REV_2;
	in.ndis_hdr.ndis_size = NDIS_RSS_CAPS_SIZE;

	caps_len = NDIS_RSS_CAPS_SIZE;
	error = hn_rndis_query(sc, OID_GEN_RECEIVE_SCALE_CAPABILITIES,
	    &in, NDIS_RSS_CAPS_SIZE, &caps, &caps_len);
	if (error)
		return (error);
	if (caps_len < NDIS_RSS_CAPS_SIZE_6_0) {
		if_printf(sc->hn_ifp, "invalid NDIS RSS caps len %zu",
		    caps_len);
		return (EINVAL);
	}

	if (caps.ndis_nrxr == 0) {
		if_printf(sc->hn_ifp, "0 RX rings!?\n");
		return (EINVAL);
	}
	*rxr_cnt = caps.ndis_nrxr;

	if (caps_len == NDIS_RSS_CAPS_SIZE) {
		if (bootverbose) {
			if_printf(sc->hn_ifp, "RSS indirect table size %u\n",
			    caps.ndis_nind);
		}
	}
	return (0);
}

static int
hn_rndis_set(struct hn_softc *sc, uint32_t oid, const void *data, size_t dlen)
{
	struct rndis_set_req *req;
	const struct rndis_set_comp *comp;
	struct vmbus_xact *xact;
	size_t reqlen, comp_len;
	uint32_t rid;
	int error;

	KASSERT(dlen > 0, ("invalid dlen %zu", dlen));

	reqlen = sizeof(*req) + dlen;
	xact = vmbus_xact_get(sc->hn_xact, reqlen);
	if (xact == NULL) {
		if_printf(sc->hn_ifp, "no xact for RNDIS set 0x%08x\n", oid);
		return (ENXIO);
	}
	rid = hn_rndis_rid(sc);
	req = vmbus_xact_req_data(xact);
	req->rm_type = REMOTE_NDIS_SET_MSG;
	req->rm_len = reqlen;
	req->rm_rid = rid;
	req->rm_oid = oid;
	req->rm_infobuflen = dlen;
	req->rm_infobufoffset = RNDIS_SET_REQ_INFOBUFOFFSET;
	/* Data immediately follows RNDIS set. */
	memcpy(req + 1, data, dlen);

	comp_len = sizeof(*comp);
	comp = hn_rndis_xact_execute(sc, xact, rid, reqlen, &comp_len,
	    REMOTE_NDIS_SET_CMPLT);
	if (comp == NULL) {
		if_printf(sc->hn_ifp, "exec RNDIS set 0x%08x failed\n", oid);
		error = EIO;
		goto done;
	}

	if (comp->rm_status != RNDIS_STATUS_SUCCESS) {
		if_printf(sc->hn_ifp, "RNDIS set 0x%08x failed: "
		    "status 0x%08x\n", oid, comp->rm_status);
		error = EIO;
		goto done;
	}
	error = 0;
done:
	vmbus_xact_put(xact);
	return (error);
}

static int
hn_rndis_conf_offload(struct hn_softc *sc)
{
	struct ndis_offload_params params;
	size_t paramsz;
	int error;

	/* NOTE: 0 means "no change" */
	memset(&params, 0, sizeof(params));

	params.ndis_hdr.ndis_type = NDIS_OBJTYPE_DEFAULT;
	if (sc->hn_ndis_ver < NDIS_VERSION_6_30) {
		params.ndis_hdr.ndis_rev = NDIS_OFFLOAD_PARAMS_REV_2;
		paramsz = NDIS_OFFLOAD_PARAMS_SIZE_6_1;
	} else {
		params.ndis_hdr.ndis_rev = NDIS_OFFLOAD_PARAMS_REV_3;
		paramsz = NDIS_OFFLOAD_PARAMS_SIZE;
	}
	params.ndis_hdr.ndis_size = paramsz;

	params.ndis_ip4csum = NDIS_OFFLOAD_PARAM_TXRX;
	params.ndis_tcp4csum = NDIS_OFFLOAD_PARAM_TXRX;
	params.ndis_tcp6csum = NDIS_OFFLOAD_PARAM_TXRX;
	if (sc->hn_ndis_ver >= NDIS_VERSION_6_30) {
		params.ndis_udp4csum = NDIS_OFFLOAD_PARAM_TXRX;
		params.ndis_udp6csum = NDIS_OFFLOAD_PARAM_TXRX;
	}
	params.ndis_lsov2_ip4 = NDIS_OFFLOAD_LSOV2_ON;
	/* XXX ndis_lsov2_ip6 = NDIS_OFFLOAD_LSOV2_ON */

	error = hn_rndis_set(sc, OID_TCP_OFFLOAD_PARAMETERS, &params, paramsz);
	if (error) {
		if_printf(sc->hn_ifp, "offload config failed: %d\n", error);
	} else {
		if (bootverbose)
			if_printf(sc->hn_ifp, "offload config done\n");
	}
	return (error);
}

static int
hn_rndis_conf_rss(struct hn_softc *sc, int nchan)
{
	struct ndis_rssprm_toeplitz *rss = &sc->hn_rss;
	struct ndis_rss_params *prm = &rss->rss_params;
	int i, error;

	/*
	 * Only NDIS 6.30+ is supported.
	 */
	KASSERT(sc->hn_ndis_ver >= NDIS_VERSION_6_30,
	    ("NDIS 6.30+ is required, NDIS version 0x%08x", sc->hn_ndis_ver));

	memset(rss, 0, sizeof(*rss));
	prm->ndis_hdr.ndis_type = NDIS_OBJTYPE_RSS_PARAMS;
	prm->ndis_hdr.ndis_rev = NDIS_RSS_PARAMS_REV_2;
	prm->ndis_hdr.ndis_size = sizeof(*rss);
	prm->ndis_hash = NDIS_HASH_FUNCTION_TOEPLITZ |
	    NDIS_HASH_IPV4 | NDIS_HASH_TCP_IPV4 |
	    NDIS_HASH_IPV6 | NDIS_HASH_TCP_IPV6;
	/* TODO: Take ndis_rss_caps.ndis_nind into account */
	prm->ndis_indsize = sizeof(rss->rss_ind);
	prm->ndis_indoffset =
	    __offsetof(struct ndis_rssprm_toeplitz, rss_ind[0]);
	prm->ndis_keysize = sizeof(rss->rss_key);
	prm->ndis_keyoffset =
	    __offsetof(struct ndis_rssprm_toeplitz, rss_key[0]);

	/* Setup RSS key */
	memcpy(rss->rss_key, netvsc_hash_key, sizeof(rss->rss_key));

	/* Setup RSS indirect table */
	/* TODO: Take ndis_rss_caps.ndis_nind into account */
	for (i = 0; i < NDIS_HASH_INDCNT; ++i)
		rss->rss_ind[i] = i % nchan;

	error = hn_rndis_set(sc, OID_GEN_RECEIVE_SCALE_PARAMETERS,
	    rss, sizeof(*rss));
	if (error) {
		if_printf(sc->hn_ifp, "RSS config failed: %d\n", error);
	} else {
		if (bootverbose)
			if_printf(sc->hn_ifp, "RSS config done\n");
	}
	return (error);
}

static int
hn_rndis_set_rxfilter(struct hn_softc *sc, uint32_t filter)
{
	int error;

	error = hn_rndis_set(sc, OID_GEN_CURRENT_PACKET_FILTER,
	    &filter, sizeof(filter));
	if (error) {
		if_printf(sc->hn_ifp, "set RX filter 0x%08x failed: %d\n",
		    filter, error);
	} else {
		if (bootverbose) {
			if_printf(sc->hn_ifp, "set RX filter 0x%08x done\n",
			    filter);
		}
	}
	return (error);
}

/*
 * RNDIS filter init device
 */
static int
hv_rf_init_device(struct hn_softc *sc)
{
	struct rndis_init_req *req;
	const struct rndis_init_comp *comp;
	struct vmbus_xact *xact;
	size_t comp_len;
	uint32_t rid;
	int error;

	xact = vmbus_xact_get(sc->hn_xact, sizeof(*req));
	if (xact == NULL) {
		if_printf(sc->hn_ifp, "no xact for RNDIS init\n");
		return (ENXIO);
	}
	rid = hn_rndis_rid(sc);
	req = vmbus_xact_req_data(xact);
	req->rm_type = REMOTE_NDIS_INITIALIZE_MSG;
	req->rm_len = sizeof(*req);
	req->rm_rid = rid;
	req->rm_ver_major = RNDIS_VERSION_MAJOR;
	req->rm_ver_minor = RNDIS_VERSION_MINOR;
	req->rm_max_xfersz = HN_RNDIS_XFER_SIZE;

	comp_len = RNDIS_INIT_COMP_SIZE_MIN;
	comp = hn_rndis_xact_execute(sc, xact, rid, sizeof(*req), &comp_len,
	    REMOTE_NDIS_INITIALIZE_CMPLT);
	if (comp == NULL) {
		if_printf(sc->hn_ifp, "exec RNDIS init failed\n");
		error = EIO;
		goto done;
	}

	if (comp->rm_status != RNDIS_STATUS_SUCCESS) {
		if_printf(sc->hn_ifp, "RNDIS init failed: status 0x%08x\n",
		    comp->rm_status);
		error = EIO;
		goto done;
	}
	if (bootverbose) {
		if_printf(sc->hn_ifp, "RNDIS ver %u.%u, pktsz %u, pktcnt %u, "
		    "align %u\n", comp->rm_ver_major, comp->rm_ver_minor,
		    comp->rm_pktmaxsz, comp->rm_pktmaxcnt,
		    1U << comp->rm_align);
	}
	error = 0;
done:
	vmbus_xact_put(xact);
	return (error);
}

/*
 * RNDIS filter halt device
 */
static int
hv_rf_halt_device(struct hn_softc *sc)
{
	struct vmbus_xact *xact;
	struct rndis_halt_req *halt;
	struct hn_send_ctx sndc;
	size_t comp_len;

	xact = vmbus_xact_get(sc->hn_xact, sizeof(*halt));
	if (xact == NULL) {
		if_printf(sc->hn_ifp, "no xact for RNDIS halt\n");
		return (ENXIO);
	}
	halt = vmbus_xact_req_data(xact);
	halt->rm_type = REMOTE_NDIS_HALT_MSG;
	halt->rm_len = sizeof(*halt);
	halt->rm_rid = hn_rndis_rid(sc);

	/* No RNDIS completion; rely on NVS message send completion */
	hn_send_ctx_init_simple(&sndc, hn_nvs_sent_xact, xact);
	hn_rndis_xact_exec1(sc, xact, sizeof(*halt), &sndc, &comp_len);

	vmbus_xact_put(xact);
	if (bootverbose)
		if_printf(sc->hn_ifp, "RNDIS halt done\n");
	return (0);
}

/*
 * RNDIS filter on device add
 */
int
hv_rf_on_device_add(struct hn_softc *sc, void *additl_info,
    int *nchan0, struct hn_rx_ring *rxr)
{
	int ret;
	netvsc_device_info *dev_info = (netvsc_device_info *)additl_info;
	device_t dev = sc->hn_dev;
	struct hn_nvs_subch_req *req;
	const struct hn_nvs_subch_resp *resp;
	size_t resp_len;
	struct vmbus_xact *xact = NULL;
	uint32_t status, nsubch;
	int nchan = *nchan0;
	int rxr_cnt;

	/*
	 * Let the inner driver handle this first to create the netvsc channel
	 * NOTE! Once the channel is created, we may get a receive callback 
	 * (hv_rf_on_receive()) before this call is completed.
	 * Note:  Earlier code used a function pointer here.
	 */
	ret = hv_nv_on_device_add(sc, rxr);
	if (ret != 0)
		return (ret);

	/*
	 * Initialize the rndis device
	 */

	/* Send the rndis initialization message */
	ret = hv_rf_init_device(sc);
	if (ret != 0) {
		/*
		 * TODO: If rndis init failed, we will need to shut down
		 * the channel
		 */
	}

	/* Get the mac address */
	ret = hv_rf_query_device_mac(sc, dev_info->mac_addr);
	if (ret != 0) {
		/* TODO: shut down rndis device and the channel */
	}

	/* Configure NDIS offload settings */
	hn_rndis_conf_offload(sc);

	hv_rf_query_device_link_status(sc, &dev_info->link_state);

	if (sc->hn_ndis_ver < NDIS_VERSION_6_30 || nchan == 1) {
		/*
		 * Either RSS is not supported, or multiple RX/TX rings
		 * are not requested.
		 */
		*nchan0 = 1;
		return (0);
	}

	/*
	 * Get RSS capabilities, e.g. # of RX rings, and # of indirect
	 * table entries.
	 */
	ret = hn_rndis_get_rsscaps(sc, &rxr_cnt);
	if (ret) {
		/* No RSS; this is benign. */
		*nchan0 = 1;
		return (0);
	}
	if (nchan > rxr_cnt)
		nchan = rxr_cnt;
	if_printf(sc->hn_ifp, "RX rings offered %u, requested %d\n",
	    rxr_cnt, nchan);

	if (nchan == 1) {
		device_printf(dev, "only 1 channel is supported, no vRSS\n");
		goto out;
	}
	
	/*
	 * Ask NVS to allocate sub-channels.
	 */
	xact = vmbus_xact_get(sc->hn_xact, sizeof(*req));
	if (xact == NULL) {
		if_printf(sc->hn_ifp, "no xact for nvs subch req\n");
		ret = ENXIO;
		goto out;
	}
	req = vmbus_xact_req_data(xact);
	req->nvs_type = HN_NVS_TYPE_SUBCH_REQ;
	req->nvs_op = HN_NVS_SUBCH_OP_ALLOC;
	req->nvs_nsubch = nchan - 1;

	resp_len = sizeof(*resp);
	resp = hn_nvs_xact_execute(sc, xact, req, sizeof(*req), &resp_len,
	    HN_NVS_TYPE_SUBCH_RESP);
	if (resp == NULL) {
		if_printf(sc->hn_ifp, "exec subch failed\n");
		ret = EIO;
		goto out;
	}

	status = resp->nvs_status;
	nsubch = resp->nvs_nsubch;
	vmbus_xact_put(xact);
	xact = NULL;

	if (status != HN_NVS_STATUS_OK) {
		if_printf(sc->hn_ifp, "subch req failed: %x\n", status);
		ret = EIO;
		goto out;
	}
	if (nsubch > nchan - 1) {
		if_printf(sc->hn_ifp, "%u subchans are allocated, requested %u\n",
		    nsubch, nchan - 1);
		nsubch = nchan - 1;
	}
	nchan = nsubch + 1;

	ret = hn_rndis_conf_rss(sc, nchan);
	if (ret != 0)
		*nchan0 = 1;
	else
		*nchan0 = nchan;
out:
	if (xact != NULL)
		vmbus_xact_put(xact);
	return (ret);
}

/*
 * RNDIS filter on device remove
 */
int
hv_rf_on_device_remove(struct hn_softc *sc)
{
	int ret;

	/* Halt and release the rndis device */
	ret = hv_rf_halt_device(sc);

	/* Pass control to inner driver to remove the device */
	ret |= hv_nv_on_device_remove(sc);

	return (ret);
}

/*
 * RNDIS filter on open
 */
int
hv_rf_on_open(struct hn_softc *sc)
{
	uint32_t filter;

	/* XXX */
	if (hv_promisc_mode != 1) {
		filter = NDIS_PACKET_TYPE_BROADCAST |
		    NDIS_PACKET_TYPE_ALL_MULTICAST |
		    NDIS_PACKET_TYPE_DIRECTED;
	} else {
		filter = NDIS_PACKET_TYPE_PROMISCUOUS;
	}
	return (hn_rndis_set_rxfilter(sc, filter));
}

/*
 * RNDIS filter on close
 */
int 
hv_rf_on_close(struct hn_softc *sc)
{

	return (hn_rndis_set_rxfilter(sc, 0));
}

void
hv_rf_channel_rollup(struct hn_rx_ring *rxr, struct hn_tx_ring *txr)
{

	netvsc_channel_rollup(rxr, txr);
}
