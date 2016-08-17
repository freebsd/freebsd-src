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

struct hv_rf_recvinfo {
	const ndis_8021q_info		*vlan_info;
	const rndis_tcp_ip_csum_info	*csum_info;
	const struct rndis_hash_info	*hash_info;
	const struct rndis_hash_value	*hash_value;
};

#define HV_RF_RECVINFO_VLAN	0x1
#define HV_RF_RECVINFO_CSUM	0x2
#define HV_RF_RECVINFO_HASHINF	0x4
#define HV_RF_RECVINFO_HASHVAL	0x8
#define HV_RF_RECVINFO_ALL		\
	(HV_RF_RECVINFO_VLAN |		\
	 HV_RF_RECVINFO_CSUM |		\
	 HV_RF_RECVINFO_HASHINF |	\
	 HV_RF_RECVINFO_HASHVAL)

/*
 * Forward declarations
 */
static int  hv_rf_send_request(rndis_device *device, rndis_request *request,
			       uint32_t message_type);
static void hv_rf_receive_response(rndis_device *device, rndis_msg *response);
static void hv_rf_receive_indicate_status(rndis_device *device,
					  rndis_msg *response);
static void hv_rf_receive_data(struct hn_rx_ring *rxr, rndis_msg *message,
			       netvsc_packet *pkt);
static int  hv_rf_query_device(rndis_device *device, uint32_t oid,
			       void *result, uint32_t *result_size);
static inline int hv_rf_query_device_mac(rndis_device *device);
static inline int hv_rf_query_device_link_status(rndis_device *device);
static int  hv_rf_set_packet_filter(rndis_device *device, uint32_t new_filter);
static int  hv_rf_init_device(rndis_device *device);
static int  hv_rf_open_device(rndis_device *device);
static int  hv_rf_close_device(rndis_device *device);
int
hv_rf_send_offload_request(struct hn_softc *sc,
    rndis_offload_params *offloads);

static void hn_rndis_sent_halt(struct hn_send_ctx *sndc,
    struct netvsc_dev_ *net_dev, struct vmbus_channel *chan,
    const void *data, int dlen);
static void hn_rndis_sent_cb(struct hn_send_ctx *sndc,
    struct netvsc_dev_ *net_dev, struct vmbus_channel *chan,
    const void *data, int dlen);

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
 * Get the Per-Packet-Info with the specified type
 * return NULL if not found.
 */
void *
hv_get_ppi_data(rndis_packet *rpkt, uint32_t type)
{
	rndis_per_packet_info *ppi;
	int len;

	if (rpkt->per_pkt_info_offset == 0)
		return (NULL);

	ppi = (rndis_per_packet_info *)((unsigned long)rpkt +
	    rpkt->per_pkt_info_offset);
	len = rpkt->per_pkt_info_length;

	while (len > 0) {
		if (ppi->type == type)
			return (void *)((unsigned long)ppi +
			    ppi->per_packet_info_offset);

		len -= ppi->size;
		ppi = (rndis_per_packet_info *)((unsigned long)ppi + ppi->size);
	}

	return (NULL);
}


/*
 * Allow module_param to work and override to switch to promiscuous mode.
 */
static inline rndis_device *
hv_get_rndis_device(void)
{
	rndis_device *device;

	device = malloc(sizeof(rndis_device), M_NETVSC, M_WAITOK | M_ZERO);

	mtx_init(&device->req_lock, "HV-FRL", NULL, MTX_DEF);

	/* Same effect as STAILQ_HEAD_INITIALIZER() static initializer */
	STAILQ_INIT(&device->myrequest_list);

	device->state = RNDIS_DEV_UNINITIALIZED;

	return (device);
}

/*
 *
 */
static inline void
hv_put_rndis_device(rndis_device *device)
{
	mtx_destroy(&device->req_lock);
	free(device, M_NETVSC);
}

/*
 *
 */
static inline rndis_request *
hv_rndis_request(rndis_device *device, uint32_t message_type,
		 uint32_t message_length)
{
	rndis_request *request;
	rndis_msg *rndis_mesg;
	rndis_set_request *set;

	request = malloc(sizeof(rndis_request), M_NETVSC, M_WAITOK | M_ZERO);

	sema_init(&request->wait_sema, 0, "rndis sema");
	
	rndis_mesg = &request->request_msg;
	rndis_mesg->ndis_msg_type = message_type;
	rndis_mesg->msg_len = message_length;

	/*
	 * Set the request id. This field is always after the rndis header
	 * for request/response packet types so we just use the set_request
	 * as a template.
	 */
	set = &rndis_mesg->msg.set_request;
	set->request_id = atomic_fetchadd_int(&device->new_request_id, 1);
	/* Increment to get the new value (call above returns old value) */
	set->request_id += 1;

	/* Add to the request list */
	mtx_lock(&device->req_lock);
	STAILQ_INSERT_TAIL(&device->myrequest_list, request, mylist_entry);
	mtx_unlock(&device->req_lock);

	return (request);
}

/*
 *
 */
static inline void
hv_put_rndis_request(rndis_device *device, rndis_request *request)
{
	mtx_lock(&device->req_lock);
	/* Fixme:  Has O(n) performance */
	/*
	 * XXXKYS: Use Doubly linked lists.
	 */
	STAILQ_REMOVE(&device->myrequest_list, request, rndis_request_,
	    mylist_entry);
	mtx_unlock(&device->req_lock);

	sema_destroy(&request->wait_sema);
	free(request, M_NETVSC);
}

/*
 *
 */
static int
hv_rf_send_request(rndis_device *device, rndis_request *request,
    uint32_t message_type)
{
	netvsc_dev      *net_dev = device->net_dev;
	uint32_t send_buf_section_idx, tot_data_buf_len;
	struct vmbus_gpa gpa[2];
	int gpa_cnt, send_buf_section_size;
	hn_sent_callback_t cb;

	/* Set up the packet to send it */
	tot_data_buf_len = request->request_msg.msg_len;

	gpa_cnt = 1;
	gpa[0].gpa_page = hv_get_phys_addr(&request->request_msg) >> PAGE_SHIFT;
	gpa[0].gpa_len = request->request_msg.msg_len;
	gpa[0].gpa_ofs = (unsigned long)&request->request_msg & (PAGE_SIZE - 1);

	if (gpa[0].gpa_ofs + gpa[0].gpa_len > PAGE_SIZE) {
		gpa_cnt = 2;
		gpa[0].gpa_len = PAGE_SIZE - gpa[0].gpa_ofs;
		gpa[1].gpa_page =
		    hv_get_phys_addr((char*)&request->request_msg +
		    gpa[0].gpa_len) >> PAGE_SHIFT;
		gpa[1].gpa_ofs = 0;
		gpa[1].gpa_len = request->request_msg.msg_len - gpa[0].gpa_len;
	}

	if (message_type != REMOTE_NDIS_HALT_MSG)
		cb = hn_rndis_sent_cb;
	else
		cb = hn_rndis_sent_halt;

	if (tot_data_buf_len < net_dev->send_section_size) {
		send_buf_section_idx = hv_nv_get_next_send_section(net_dev);
		if (send_buf_section_idx != HN_NVS_CHIM_IDX_INVALID) {
			char *dest = ((char *)net_dev->send_buf +
				send_buf_section_idx * net_dev->send_section_size);

			memcpy(dest, &request->request_msg, request->request_msg.msg_len);
			send_buf_section_size = tot_data_buf_len;
			gpa_cnt = 0;
			goto sendit;
		}
		/* Failed to allocate chimney send buffer; move on */
	}
	send_buf_section_idx = HN_NVS_CHIM_IDX_INVALID;
	send_buf_section_size = 0;

sendit:
	hn_send_ctx_init(&request->send_ctx, cb, request,
	    send_buf_section_idx, send_buf_section_size);
	return hv_nv_on_send(device->net_dev->sc->hn_prichan,
	    HN_NVS_RNDIS_MTYPE_CTRL, &request->send_ctx, gpa, gpa_cnt);
}

/*
 * RNDIS filter receive response
 */
static void 
hv_rf_receive_response(rndis_device *device, rndis_msg *response)
{
	rndis_request *request = NULL;
	rndis_request *next_request;
	boolean_t found = FALSE;

	mtx_lock(&device->req_lock);
	request = STAILQ_FIRST(&device->myrequest_list);
	while (request != NULL) {
		/*
		 * All request/response message contains request_id as the
		 * first field
		 */
		if (request->request_msg.msg.init_request.request_id ==
				      response->msg.init_complete.request_id) {
			found = TRUE;
			break;
		}
		next_request = STAILQ_NEXT(request, mylist_entry);
		request = next_request;
	}
	mtx_unlock(&device->req_lock);

	if (found) {
		if (response->msg_len <= sizeof(rndis_msg)) {
			memcpy(&request->response_msg, response,
			    response->msg_len);
		} else {
			if (response->ndis_msg_type == REMOTE_NDIS_RESET_CMPLT) {
				/* Does not have a request id field */
				request->response_msg.msg.reset_complete.status =
				    STATUS_BUFFER_OVERFLOW;
			} else {
				request->response_msg.msg.init_complete.status =
				    STATUS_BUFFER_OVERFLOW;
			}
		}

		sema_post(&request->wait_sema);
	}
}

int
hv_rf_send_offload_request(struct hn_softc *sc,
    rndis_offload_params *offloads)
{
	rndis_request *request;
	rndis_set_request *set;
	rndis_offload_params *offload_req;
	rndis_set_complete *set_complete;	
	rndis_device *rndis_dev;
	device_t dev = sc->hn_dev;
	netvsc_dev *net_dev = sc->net_dev;
	uint32_t vsp_version = net_dev->nvsp_version;
	uint32_t extlen = sizeof(rndis_offload_params);
	int ret;

	if (vsp_version <= NVSP_PROTOCOL_VERSION_4) {
		extlen = VERSION_4_OFFLOAD_SIZE;
		/* On NVSP_PROTOCOL_VERSION_4 and below, we do not support
		 * UDP checksum offload.
		 */
		offloads->udp_ipv4_csum = 0;
		offloads->udp_ipv6_csum = 0;
	}

	rndis_dev = net_dev->extension;

	request = hv_rndis_request(rndis_dev, REMOTE_NDIS_SET_MSG,
	    RNDIS_MESSAGE_SIZE(rndis_set_request) + extlen);
	if (!request)
		return (ENOMEM);

	set = &request->request_msg.msg.set_request;
	set->oid = RNDIS_OID_TCP_OFFLOAD_PARAMETERS;
	set->info_buffer_length = extlen;
	set->info_buffer_offset = sizeof(rndis_set_request);
	set->device_vc_handle = 0;

	offload_req = (rndis_offload_params *)((unsigned long)set +
	    set->info_buffer_offset);
	*offload_req = *offloads;
	offload_req->header.type = RNDIS_OBJECT_TYPE_DEFAULT;
	offload_req->header.revision = RNDIS_OFFLOAD_PARAMETERS_REVISION_3;
	offload_req->header.size = extlen;

	ret = hv_rf_send_request(rndis_dev, request, REMOTE_NDIS_SET_MSG);
	if (ret != 0) {
		device_printf(dev, "hv send offload request failed, ret=%d!\n",
		    ret);
		goto cleanup;
	}

	ret = sema_timedwait(&request->wait_sema, 5 * hz);
	if (ret != 0) {
		device_printf(dev, "hv send offload request timeout\n");
		goto cleanup;
	}

	set_complete = &request->response_msg.msg.set_complete;
	if (set_complete->status == RNDIS_STATUS_SUCCESS) {
		device_printf(dev, "hv send offload request succeeded\n");
		ret = 0;
	} else {
		if (set_complete->status == STATUS_NOT_SUPPORTED) {
			device_printf(dev, "HV Not support offload\n");
			ret = 0;
		} else {
			ret = set_complete->status;
		}
	}

cleanup:
	hv_put_rndis_request(rndis_dev, request);

	return (ret);
}

/*
 * RNDIS filter receive indicate status
 */
static void 
hv_rf_receive_indicate_status(rndis_device *device, rndis_msg *response)
{
	rndis_indicate_status *indicate = &response->msg.indicate_status;
		
	switch(indicate->status) {
	case RNDIS_STATUS_MEDIA_CONNECT:
		netvsc_linkstatus_callback(device->net_dev->sc, 1);
		break;
	case RNDIS_STATUS_MEDIA_DISCONNECT:
		netvsc_linkstatus_callback(device->net_dev->sc, 0);
		break;
	default:
		/* TODO: */
		device_printf(device->net_dev->sc->hn_dev,
		    "unknown status %d received\n", indicate->status);
		break;
	}
}

static int
hv_rf_find_recvinfo(const rndis_packet *rpkt, struct hv_rf_recvinfo *info)
{
	const rndis_per_packet_info *ppi;
	uint32_t mask, len;

	info->vlan_info = NULL;
	info->csum_info = NULL;
	info->hash_info = NULL;
	info->hash_value = NULL;

	if (rpkt->per_pkt_info_offset == 0)
		return 0;

	ppi = (const rndis_per_packet_info *)
	    ((const uint8_t *)rpkt + rpkt->per_pkt_info_offset);
	len = rpkt->per_pkt_info_length;
	mask = 0;

	while (len != 0) {
		const void *ppi_dptr;
		uint32_t ppi_dlen;

		if (__predict_false(ppi->size < ppi->per_packet_info_offset))
			return EINVAL;
		ppi_dlen = ppi->size - ppi->per_packet_info_offset;
		ppi_dptr = (const uint8_t *)ppi + ppi->per_packet_info_offset;

		switch (ppi->type) {
		case ieee_8021q_info:
			if (__predict_false(ppi_dlen < sizeof(ndis_8021q_info)))
				return EINVAL;
			info->vlan_info = ppi_dptr;
			mask |= HV_RF_RECVINFO_VLAN;
			break;

		case tcpip_chksum_info:
			if (__predict_false(ppi_dlen <
			    sizeof(rndis_tcp_ip_csum_info)))
				return EINVAL;
			info->csum_info = ppi_dptr;
			mask |= HV_RF_RECVINFO_CSUM;
			break;

		case nbl_hash_value:
			if (__predict_false(ppi_dlen <
			    sizeof(struct rndis_hash_value)))
				return EINVAL;
			info->hash_value = ppi_dptr;
			mask |= HV_RF_RECVINFO_HASHVAL;
			break;

		case nbl_hash_info:
			if (__predict_false(ppi_dlen <
			    sizeof(struct rndis_hash_info)))
				return EINVAL;
			info->hash_info = ppi_dptr;
			mask |= HV_RF_RECVINFO_HASHINF;
			break;

		default:
			goto skip;
		}

		if (mask == HV_RF_RECVINFO_ALL) {
			/* All found; done */
			break;
		}
skip:
		if (__predict_false(len < ppi->size))
			return EINVAL;
		len -= ppi->size;
		ppi = (const rndis_per_packet_info *)
		    ((const uint8_t *)ppi + ppi->size);
	}
	return 0;
}

/*
 * RNDIS filter receive data
 */
static void
hv_rf_receive_data(struct hn_rx_ring *rxr, rndis_msg *message,
    netvsc_packet *pkt)
{
	rndis_packet *rndis_pkt;
	uint32_t data_offset;
	struct hv_rf_recvinfo info;

	rndis_pkt = &message->msg.packet;

	/*
	 * Fixme:  Handle multiple rndis pkt msgs that may be enclosed in this
	 * netvsc packet (ie tot_data_buf_len != message_length)
	 */

	/* Remove rndis header, then pass data packet up the stack */
	data_offset = RNDIS_HEADER_SIZE + rndis_pkt->data_offset;

	pkt->tot_data_buf_len -= data_offset;
	if (pkt->tot_data_buf_len < rndis_pkt->data_length) {
		pkt->status = nvsp_status_failure;
		if_printf(rxr->hn_ifp,
		    "total length %u is less than data length %u\n",
		    pkt->tot_data_buf_len, rndis_pkt->data_length);
		return;
	}

	pkt->tot_data_buf_len = rndis_pkt->data_length;
	pkt->data = (void *)((unsigned long)pkt->data + data_offset);

	if (hv_rf_find_recvinfo(rndis_pkt, &info)) {
		pkt->status = nvsp_status_failure;
		if_printf(rxr->hn_ifp, "recvinfo parsing failed\n");
		return;
	}

	if (info.vlan_info != NULL)
		pkt->vlan_tci = info.vlan_info->u1.s1.vlan_id;
	else
		pkt->vlan_tci = 0;

	netvsc_recv(rxr, pkt, info.csum_info, info.hash_info, info.hash_value);
}

/*
 * RNDIS filter on receive
 */
int
hv_rf_on_receive(netvsc_dev *net_dev,
    struct hn_rx_ring *rxr, netvsc_packet *pkt)
{
	rndis_device *rndis_dev;
	rndis_msg *rndis_hdr;

	/* Make sure the rndis device state is initialized */
	if (net_dev->extension == NULL) {
		pkt->status = nvsp_status_failure;
		return (ENODEV);
	}

	rndis_dev = (rndis_device *)net_dev->extension;
	if (rndis_dev->state == RNDIS_DEV_UNINITIALIZED) {
		pkt->status = nvsp_status_failure;
		return (EINVAL);
	}

	rndis_hdr = pkt->data;

	switch (rndis_hdr->ndis_msg_type) {

	/* data message */
	case REMOTE_NDIS_PACKET_MSG:
		hv_rf_receive_data(rxr, rndis_hdr, pkt);
		break;
	/* completion messages */
	case REMOTE_NDIS_INITIALIZE_CMPLT:
	case REMOTE_NDIS_QUERY_CMPLT:
	case REMOTE_NDIS_SET_CMPLT:
	case REMOTE_NDIS_RESET_CMPLT:
	case REMOTE_NDIS_KEEPALIVE_CMPLT:
		hv_rf_receive_response(rndis_dev, rndis_hdr);
		break;
	/* notification message */
	case REMOTE_NDIS_INDICATE_STATUS_MSG:
		hv_rf_receive_indicate_status(rndis_dev, rndis_hdr);
		break;
	default:
		printf("hv_rf_on_receive():  Unknown msg_type 0x%x\n",
			rndis_hdr->ndis_msg_type);
		break;
	}

	return (0);
}

/*
 * RNDIS filter query device
 */
static int
hv_rf_query_device(rndis_device *device, uint32_t oid, void *result,
		   uint32_t *result_size)
{
	rndis_request *request;
	uint32_t in_result_size = *result_size;
	rndis_query_request *query;
	rndis_query_complete *query_complete;
	int ret = 0;

	*result_size = 0;
	request = hv_rndis_request(device, REMOTE_NDIS_QUERY_MSG,
	    RNDIS_MESSAGE_SIZE(rndis_query_request));
	if (request == NULL) {
		ret = -1;
		goto cleanup;
	}

	/* Set up the rndis query */
	query = &request->request_msg.msg.query_request;
	query->oid = oid;
	query->info_buffer_offset = sizeof(rndis_query_request); 
	query->info_buffer_length = 0;
	query->device_vc_handle = 0;

	if (oid == RNDIS_OID_GEN_RSS_CAPABILITIES) {
		struct rndis_recv_scale_cap *cap;

		request->request_msg.msg_len += 
			sizeof(struct rndis_recv_scale_cap);
		query->info_buffer_length = sizeof(struct rndis_recv_scale_cap);
		cap = (struct rndis_recv_scale_cap *)((unsigned long)query + 
						query->info_buffer_offset);
		cap->hdr.type = RNDIS_OBJECT_TYPE_RSS_CAPABILITIES;
		cap->hdr.rev = RNDIS_RECEIVE_SCALE_CAPABILITIES_REVISION_2;
		cap->hdr.size = sizeof(struct rndis_recv_scale_cap);
	}

	ret = hv_rf_send_request(device, request, REMOTE_NDIS_QUERY_MSG);
	if (ret != 0) {
		/* Fixme:  printf added */
		printf("RNDISFILTER request failed to Send!\n");
		goto cleanup;
	}

	sema_wait(&request->wait_sema);

	/* Copy the response back */
	query_complete = &request->response_msg.msg.query_complete;
	
	if (query_complete->info_buffer_length > in_result_size) {
		ret = EINVAL;
		goto cleanup;
	}

	memcpy(result, (void *)((unsigned long)query_complete +
	    query_complete->info_buffer_offset),
	    query_complete->info_buffer_length);

	*result_size = query_complete->info_buffer_length;

cleanup:
	if (request != NULL)
		hv_put_rndis_request(device, request);

	return (ret);
}

/*
 * RNDIS filter query device MAC address
 */
static inline int
hv_rf_query_device_mac(rndis_device *device)
{
	uint32_t size = ETHER_ADDR_LEN;

	return (hv_rf_query_device(device,
	    RNDIS_OID_802_3_PERMANENT_ADDRESS, device->hw_mac_addr, &size));
}

/*
 * RNDIS filter query device link status
 */
static inline int
hv_rf_query_device_link_status(rndis_device *device)
{
	uint32_t size = sizeof(uint32_t);

	return (hv_rf_query_device(device,
	    RNDIS_OID_GEN_MEDIA_CONNECT_STATUS, &device->link_status, &size));
}

static uint8_t netvsc_hash_key[HASH_KEYLEN] = {
	0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
	0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
	0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
	0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
	0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa
};

/*
 * RNDIS set vRSS parameters
 */
static int
hv_rf_set_rss_param(rndis_device *device, int num_queue)
{
	rndis_request *request;
	rndis_set_request *set;
	rndis_set_complete *set_complete;
	rndis_recv_scale_param *rssp;
	uint32_t extlen = sizeof(rndis_recv_scale_param) +
	    (4 * ITAB_NUM) + HASH_KEYLEN;
	uint32_t *itab, status;
	uint8_t *keyp;
	int i, ret;


	request = hv_rndis_request(device, REMOTE_NDIS_SET_MSG,
	    RNDIS_MESSAGE_SIZE(rndis_set_request) + extlen);
	if (request == NULL) {
		if (bootverbose)
			printf("Netvsc: No memory to set vRSS parameters.\n");
		ret = -1;
		goto cleanup;
	}

	set = &request->request_msg.msg.set_request;
	set->oid = RNDIS_OID_GEN_RSS_PARAMETERS;
	set->info_buffer_length = extlen;
	set->info_buffer_offset = sizeof(rndis_set_request);
	set->device_vc_handle = 0;

	/* Fill out the rssp parameter structure */
	rssp = (rndis_recv_scale_param *)(set + 1);
	rssp->hdr.type = RNDIS_OBJECT_TYPE_RSS_PARAMETERS;
	rssp->hdr.rev = RNDIS_RECEIVE_SCALE_PARAMETERS_REVISION_2;
	rssp->hdr.size = sizeof(rndis_recv_scale_param);
	rssp->flag = 0;
	rssp->hashinfo = RNDIS_HASH_FUNC_TOEPLITZ | RNDIS_HASH_IPV4 |
	    RNDIS_HASH_TCP_IPV4 | RNDIS_HASH_IPV6 | RNDIS_HASH_TCP_IPV6;
	rssp->indirect_tabsize = 4 * ITAB_NUM;
	rssp->indirect_taboffset = sizeof(rndis_recv_scale_param);
	rssp->hashkey_size = HASH_KEYLEN;
	rssp->hashkey_offset = rssp->indirect_taboffset +
	    rssp->indirect_tabsize;

	/* Set indirection table entries */
	itab = (uint32_t *)(rssp + 1);
	for (i = 0; i < ITAB_NUM; i++)
		itab[i] = i % num_queue;

	/* Set hash key values */
	keyp = (uint8_t *)((unsigned long)rssp + rssp->hashkey_offset);
	for (i = 0; i < HASH_KEYLEN; i++)
		keyp[i] = netvsc_hash_key[i];

	ret = hv_rf_send_request(device, request, REMOTE_NDIS_SET_MSG);
	if (ret != 0) {
		goto cleanup;
	}

	/*
	 * Wait for the response from the host.  Another thread will signal
	 * us when the response has arrived.  In the failure case,
	 * sema_timedwait() returns a non-zero status after waiting 5 seconds.
	 */
	ret = sema_timedwait(&request->wait_sema, 5 * hz);
	if (ret == 0) {
		/* Response received, check status */
		set_complete = &request->response_msg.msg.set_complete;
		status = set_complete->status;
		if (status != RNDIS_STATUS_SUCCESS) {
			/* Bad response status, return error */
			if (bootverbose)
				printf("Netvsc: Failed to set vRSS "
				    "parameters.\n");
			ret = -2;
		} else {
			if (bootverbose)
				printf("Netvsc: Successfully set vRSS "
				    "parameters.\n");
		}
	} else {
		/*
		 * We cannot deallocate the request since we may still
		 * receive a send completion for it.
		 */
		printf("Netvsc: vRSS set timeout, id = %u, ret = %d\n",
		    request->request_msg.msg.init_request.request_id, ret);
		goto exit;
	}

cleanup:
	if (request != NULL) {
		hv_put_rndis_request(device, request);
	}
exit:
	return (ret);
}

/*
 * RNDIS filter set packet filter
 * Sends an rndis request with the new filter, then waits for a response
 * from the host.
 * Returns zero on success, non-zero on failure.
 */
static int
hv_rf_set_packet_filter(rndis_device *device, uint32_t new_filter)
{
	rndis_request *request;
	rndis_set_request *set;
	rndis_set_complete *set_complete;
	uint32_t status;
	int ret;

	request = hv_rndis_request(device, REMOTE_NDIS_SET_MSG,
	    RNDIS_MESSAGE_SIZE(rndis_set_request) + sizeof(uint32_t));
	if (request == NULL) {
		ret = -1;
		goto cleanup;
	}

	/* Set up the rndis set */
	set = &request->request_msg.msg.set_request;
	set->oid = RNDIS_OID_GEN_CURRENT_PACKET_FILTER;
	set->info_buffer_length = sizeof(uint32_t);
	set->info_buffer_offset = sizeof(rndis_set_request); 

	memcpy((void *)((unsigned long)set + sizeof(rndis_set_request)),
	    &new_filter, sizeof(uint32_t));

	ret = hv_rf_send_request(device, request, REMOTE_NDIS_SET_MSG);
	if (ret != 0) {
		goto cleanup;
	}

	/*
	 * Wait for the response from the host.  Another thread will signal
	 * us when the response has arrived.  In the failure case,
	 * sema_timedwait() returns a non-zero status after waiting 5 seconds.
	 */
	ret = sema_timedwait(&request->wait_sema, 5 * hz);
	if (ret == 0) {
		/* Response received, check status */
		set_complete = &request->response_msg.msg.set_complete;
		status = set_complete->status;
		if (status != RNDIS_STATUS_SUCCESS) {
			/* Bad response status, return error */
			ret = -2;
		}
	} else {
		/*
		 * We cannot deallocate the request since we may still
		 * receive a send completion for it.
		 */
		goto exit;
	}

cleanup:
	if (request != NULL) {
		hv_put_rndis_request(device, request);
	}
exit:
	return (ret);
}

/*
 * RNDIS filter init device
 */
static int
hv_rf_init_device(rndis_device *device)
{
	rndis_request *request;
	rndis_initialize_request *init;
	rndis_initialize_complete *init_complete;
	uint32_t status;
	int ret;

	request = hv_rndis_request(device, REMOTE_NDIS_INITIALIZE_MSG,
	    RNDIS_MESSAGE_SIZE(rndis_initialize_request));
	if (!request) {
		ret = -1;
		goto cleanup;
	}

	/* Set up the rndis set */
	init = &request->request_msg.msg.init_request;
	init->major_version = RNDIS_MAJOR_VERSION;
	init->minor_version = RNDIS_MINOR_VERSION;
	/*
	 * Per the RNDIS document, this should be set to the max MTU
	 * plus the header size.  However, 2048 works fine, so leaving
	 * it as is.
	 */
	init->max_xfer_size = 2048;
	
	device->state = RNDIS_DEV_INITIALIZING;

	ret = hv_rf_send_request(device, request, REMOTE_NDIS_INITIALIZE_MSG);
	if (ret != 0) {
		device->state = RNDIS_DEV_UNINITIALIZED;
		goto cleanup;
	}

	sema_wait(&request->wait_sema);

	init_complete = &request->response_msg.msg.init_complete;
	status = init_complete->status;
	if (status == RNDIS_STATUS_SUCCESS) {
		device->state = RNDIS_DEV_INITIALIZED;
		ret = 0;
	} else {
		device->state = RNDIS_DEV_UNINITIALIZED; 
		ret = -1;
	}

cleanup:
	if (request) {
		hv_put_rndis_request(device, request);
	}

	return (ret);
}

#define HALT_COMPLETION_WAIT_COUNT      25

/*
 * RNDIS filter halt device
 */
static int
hv_rf_halt_device(rndis_device *device)
{
	rndis_request *request;
	rndis_halt_request *halt;
	int i, ret;

	/* Attempt to do a rndis device halt */
	request = hv_rndis_request(device, REMOTE_NDIS_HALT_MSG,
	    RNDIS_MESSAGE_SIZE(rndis_halt_request));
	if (request == NULL) {
		return (-1);
	}

	/* initialize "poor man's semaphore" */
	request->halt_complete_flag = 0;

	/* Set up the rndis set */
	halt = &request->request_msg.msg.halt_request;
	halt->request_id = atomic_fetchadd_int(&device->new_request_id, 1);
	/* Increment to get the new value (call above returns old value) */
	halt->request_id += 1;
	
	ret = hv_rf_send_request(device, request, REMOTE_NDIS_HALT_MSG);
	if (ret != 0) {
		return (-1);
	}

	/*
	 * Wait for halt response from halt callback.  We must wait for
	 * the transaction response before freeing the request and other
	 * resources.
	 */
	for (i=HALT_COMPLETION_WAIT_COUNT; i > 0; i--) {
		if (request->halt_complete_flag != 0) {
			break;
		}
		DELAY(400);
	}
	if (i == 0) {
		return (-1);
	}

	device->state = RNDIS_DEV_UNINITIALIZED;

	hv_put_rndis_request(device, request);

	return (0);
}

/*
 * RNDIS filter open device
 */
static int
hv_rf_open_device(rndis_device *device)
{
	int ret;

	if (device->state != RNDIS_DEV_INITIALIZED) {
		return (0);
	}

	if (hv_promisc_mode != 1) {
		ret = hv_rf_set_packet_filter(device, 
		    NDIS_PACKET_TYPE_BROADCAST     |
		    NDIS_PACKET_TYPE_ALL_MULTICAST |
		    NDIS_PACKET_TYPE_DIRECTED);
	} else {
		ret = hv_rf_set_packet_filter(device, 
		    NDIS_PACKET_TYPE_PROMISCUOUS);
	}

	if (ret == 0) {
		device->state = RNDIS_DEV_DATAINITIALIZED;
	}

	return (ret);
}

/*
 * RNDIS filter close device
 */
static int
hv_rf_close_device(rndis_device *device)
{
	int ret;

	if (device->state != RNDIS_DEV_DATAINITIALIZED) {
		return (0);
	}

	ret = hv_rf_set_packet_filter(device, 0);
	if (ret == 0) {
		device->state = RNDIS_DEV_INITIALIZED;
	}

	return (ret);
}

/*
 * RNDIS filter on device add
 */
int
hv_rf_on_device_add(struct hn_softc *sc, void *additl_info,
    int nchan, struct hn_rx_ring *rxr)
{
	struct hn_send_ctx sndc;
	int ret;
	netvsc_dev *net_dev;
	rndis_device *rndis_dev;
	rndis_offload_params offloads;
	struct rndis_recv_scale_cap rsscaps;
	uint32_t rsscaps_size = sizeof(struct rndis_recv_scale_cap);
	netvsc_device_info *dev_info = (netvsc_device_info *)additl_info;
	device_t dev = sc->hn_dev;
	struct hn_nvs_subch_req *req;
	const struct hn_nvs_subch_resp *resp;
	size_t resp_len;
	struct vmbus_xact *xact;
	uint32_t status, nsubch;

	rndis_dev = hv_get_rndis_device();
	if (rndis_dev == NULL) {
		return (ENOMEM);
	}

	/*
	 * Let the inner driver handle this first to create the netvsc channel
	 * NOTE! Once the channel is created, we may get a receive callback 
	 * (hv_rf_on_receive()) before this call is completed.
	 * Note:  Earlier code used a function pointer here.
	 */
	net_dev = hv_nv_on_device_add(sc, additl_info, rxr);
	if (!net_dev) {
		hv_put_rndis_device(rndis_dev);

		return (ENOMEM);
	}

	/*
	 * Initialize the rndis device
	 */

	net_dev->extension = rndis_dev;
	rndis_dev->net_dev = net_dev;

	/* Send the rndis initialization message */
	ret = hv_rf_init_device(rndis_dev);
	if (ret != 0) {
		/*
		 * TODO: If rndis init failed, we will need to shut down
		 * the channel
		 */
	}

	/* Get the mac address */
	ret = hv_rf_query_device_mac(rndis_dev);
	if (ret != 0) {
		/* TODO: shut down rndis device and the channel */
	}

	/* config csum offload and send request to host */
	memset(&offloads, 0, sizeof(offloads));
	offloads.ipv4_csum = RNDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED;
	offloads.tcp_ipv4_csum = RNDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED;
	offloads.udp_ipv4_csum = RNDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED;
	offloads.tcp_ipv6_csum = RNDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED;
	offloads.udp_ipv6_csum = RNDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED;
	offloads.lso_v2_ipv4 = RNDIS_OFFLOAD_PARAMETERS_LSOV2_ENABLED;

	ret = hv_rf_send_offload_request(sc, &offloads);
	if (ret != 0) {
		/* TODO: shut down rndis device and the channel */
		device_printf(dev,
		    "hv_rf_send_offload_request failed, ret=%d\n", ret);
	}
	
	memcpy(dev_info->mac_addr, rndis_dev->hw_mac_addr, ETHER_ADDR_LEN);

	hv_rf_query_device_link_status(rndis_dev);
	
	dev_info->link_state = rndis_dev->link_status;

	net_dev->num_channel = 1;
	if (net_dev->nvsp_version < NVSP_PROTOCOL_VERSION_5 || nchan == 1)
		return (0);

	memset(&rsscaps, 0, rsscaps_size);
	ret = hv_rf_query_device(rndis_dev,
			RNDIS_OID_GEN_RSS_CAPABILITIES,
			&rsscaps, &rsscaps_size);
	if ((ret != 0) || (rsscaps.num_recv_que < 2)) {
		device_printf(dev, "hv_rf_query_device failed or "
			"rsscaps.num_recv_que < 2 \n");
		goto out;
	}
	device_printf(dev, "channel, offered %u, requested %d\n",
	    rsscaps.num_recv_que, nchan);
	if (nchan > rsscaps.num_recv_que)
		nchan = rsscaps.num_recv_que;
	net_dev->num_channel = nchan;

	if (net_dev->num_channel == 1) {
		device_printf(dev, "net_dev->num_channel == 1 under VRSS\n");
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
	req->nvs_nsubch = net_dev->num_channel - 1;

	hn_send_ctx_init_simple(&sndc, hn_nvs_sent_xact, xact);
	vmbus_xact_activate(xact);

	ret = hn_nvs_send(sc->hn_prichan, VMBUS_CHANPKT_FLAG_RC,
	    req, sizeof(*req), &sndc);
	if (ret != 0) {
		if_printf(sc->hn_ifp, "send nvs subch req failed: %d\n", ret);
		vmbus_xact_deactivate(xact);
		vmbus_xact_put(xact);
		goto out;
	}

	resp = vmbus_xact_wait(xact, &resp_len);
	if (resp_len < sizeof(*resp)) {
		if_printf(sc->hn_ifp, "invalid subch resp length %zu\n",
		    resp_len);
		vmbus_xact_put(xact);
		ret = EINVAL;
		goto out;
	}
	if (resp->nvs_type != HN_NVS_TYPE_SUBCH_RESP) {
		if_printf(sc->hn_ifp, "not subch resp, type %u\n",
		    resp->nvs_type);
		vmbus_xact_put(xact);
		ret = EINVAL;
		goto out;
	}

	status = resp->nvs_status;
	nsubch = resp->nvs_nsubch;
	vmbus_xact_put(xact);

	if (status != HN_NVS_STATUS_OK) {
		if_printf(sc->hn_ifp, "subch req failed: %x\n", status);
		ret = EIO;
		goto out;
	}
	if (nsubch > net_dev->num_channel - 1) {
		if_printf(sc->hn_ifp, "%u subchans are allocated, requested %u\n",
		    nsubch, net_dev->num_channel - 1);
		nsubch = net_dev->num_channel - 1;
	}
	net_dev->num_channel = nsubch + 1;

	ret = hv_rf_set_rss_param(rndis_dev, net_dev->num_channel);

out:
	if (ret)
		net_dev->num_channel = 1;

	return (ret);
}

/*
 * RNDIS filter on device remove
 */
int
hv_rf_on_device_remove(struct hn_softc *sc, boolean_t destroy_channel)
{
	netvsc_dev *net_dev = sc->net_dev;
	rndis_device *rndis_dev = (rndis_device *)net_dev->extension;
	int ret;

	/* Halt and release the rndis device */
	ret = hv_rf_halt_device(rndis_dev);

	hv_put_rndis_device(rndis_dev);
	net_dev->extension = NULL;

	/* Pass control to inner driver to remove the device */
	ret |= hv_nv_on_device_remove(sc, destroy_channel);

	return (ret);
}

/*
 * RNDIS filter on open
 */
int
hv_rf_on_open(struct hn_softc *sc)
{
	netvsc_dev *net_dev = sc->net_dev;

	return (hv_rf_open_device((rndis_device *)net_dev->extension));
}

/*
 * RNDIS filter on close
 */
int 
hv_rf_on_close(struct hn_softc *sc)
{
	netvsc_dev *net_dev = sc->net_dev;

	return (hv_rf_close_device((rndis_device *)net_dev->extension));
}

static void
hn_rndis_sent_cb(struct hn_send_ctx *sndc, struct netvsc_dev_ *net_dev,
    struct vmbus_channel *chan __unused, const void *data __unused,
    int dlen __unused)
{
	if (sndc->hn_chim_idx != HN_NVS_CHIM_IDX_INVALID)
		hn_chim_free(net_dev, sndc->hn_chim_idx);
}

static void
hn_rndis_sent_halt(struct hn_send_ctx *sndc, struct netvsc_dev_ *net_dev,
    struct vmbus_channel *chan __unused, const void *data __unused,
    int dlen __unused)
{
	rndis_request *request = sndc->hn_cbarg;

	if (sndc->hn_chim_idx != HN_NVS_CHIM_IDX_INVALID)
		hn_chim_free(net_dev, sndc->hn_chim_idx);

	/*
	 * Notify hv_rf_halt_device() about halt completion.
	 * The halt code must wait for completion before freeing
	 * the transaction resources.
	 */
	request->halt_complete_flag = 1;
}

void
hv_rf_channel_rollup(struct hn_rx_ring *rxr, struct hn_tx_ring *txr)
{

	netvsc_channel_rollup(rxr, txr);
}
