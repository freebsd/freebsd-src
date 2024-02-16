/*-
 * SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
 *
 * Copyright (c) 2015 - 2023 Intel Corporation
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenFabrics.org BSD license below:
 *
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *
 *    - Redistributions of source code must retain the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer.
 *
 *    - Redistributions in binary form must reproduce the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer in the documentation and/or other materials
 *	provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "irdma_main.h"

static void irdma_cm_post_event(struct irdma_cm_event *event);
static void irdma_disconnect_worker(struct work_struct *work);

/**
 * irdma_free_sqbuf - put back puda buffer if refcount is 0
 * @vsi: The VSI structure of the device
 * @bufp: puda buffer to free
 */
void
irdma_free_sqbuf(struct irdma_sc_vsi *vsi, void *bufp)
{
	struct irdma_puda_buf *buf = bufp;
	struct irdma_puda_rsrc *ilq = vsi->ilq;

	if (atomic_dec_and_test(&buf->refcount))
		irdma_puda_ret_bufpool(ilq, buf);
}

/**
 * irdma_record_ird_ord - Record IRD/ORD passed in
 * @cm_node: connection's node
 * @conn_ird: connection IRD
 * @conn_ord: connection ORD
 */
static void
irdma_record_ird_ord(struct irdma_cm_node *cm_node, u32 conn_ird,
		     u32 conn_ord)
{
	if (conn_ird > cm_node->dev->hw_attrs.max_hw_ird)
		conn_ird = cm_node->dev->hw_attrs.max_hw_ird;

	if (conn_ord > cm_node->dev->hw_attrs.max_hw_ord)
		conn_ord = cm_node->dev->hw_attrs.max_hw_ord;
	else if (!conn_ord && cm_node->send_rdma0_op == SEND_RDMA_READ_ZERO)
		conn_ord = 1;
	cm_node->ird_size = conn_ird;
	cm_node->ord_size = conn_ord;
}

/**
 * irdma_copy_ip_ntohl - copy IP address from  network to host
 * @dst: IP address in host order
 * @src: IP address in network order (big endian)
 */
void
irdma_copy_ip_ntohl(u32 *dst, __be32 *src)
{
	*dst++ = ntohl(*src++);
	*dst++ = ntohl(*src++);
	*dst++ = ntohl(*src++);
	*dst = ntohl(*src);
}

/**
 * irdma_copy_ip_htonl - copy IP address from host to network order
 * @dst: IP address in network order (big endian)
 * @src: IP address in host order
 */
void
irdma_copy_ip_htonl(__be32 *dst, u32 *src)
{
	*dst++ = htonl(*src++);
	*dst++ = htonl(*src++);
	*dst++ = htonl(*src++);
	*dst = htonl(*src);
}

/**
 * irdma_get_addr_info
 * @cm_node: contains ip/tcp info
 * @cm_info: to get a copy of the cm_node ip/tcp info
 */
static void
irdma_get_addr_info(struct irdma_cm_node *cm_node,
		    struct irdma_cm_info *cm_info)
{
	memset(cm_info, 0, sizeof(*cm_info));
	cm_info->ipv4 = cm_node->ipv4;
	cm_info->vlan_id = cm_node->vlan_id;
	memcpy(cm_info->loc_addr, cm_node->loc_addr, sizeof(cm_info->loc_addr));
	memcpy(cm_info->rem_addr, cm_node->rem_addr, sizeof(cm_info->rem_addr));
	cm_info->loc_port = cm_node->loc_port;
	cm_info->rem_port = cm_node->rem_port;
}

/**
 * irdma_fill_sockaddr4 - fill in addr info for IPv4 connection
 * @cm_node: connection's node
 * @event: upper layer's cm event
 */
static inline void
irdma_fill_sockaddr4(struct irdma_cm_node *cm_node,
		     struct iw_cm_event *event)
{
	struct sockaddr_in *laddr = (struct sockaddr_in *)&event->local_addr;
	struct sockaddr_in *raddr = (struct sockaddr_in *)&event->remote_addr;

	laddr->sin_family = AF_INET;
	raddr->sin_family = AF_INET;

	laddr->sin_port = htons(cm_node->loc_port);
	raddr->sin_port = htons(cm_node->rem_port);

	laddr->sin_addr.s_addr = htonl(cm_node->loc_addr[0]);
	raddr->sin_addr.s_addr = htonl(cm_node->rem_addr[0]);
}

/**
 * irdma_fill_sockaddr6 - fill in addr info for IPv6 connection
 * @cm_node: connection's node
 * @event: upper layer's cm event
 */
static inline void
irdma_fill_sockaddr6(struct irdma_cm_node *cm_node,
		     struct iw_cm_event *event)
{
	struct sockaddr_in6 *laddr6 = (struct sockaddr_in6 *)&event->local_addr;
	struct sockaddr_in6 *raddr6 = (struct sockaddr_in6 *)&event->remote_addr;

	laddr6->sin6_family = AF_INET6;
	raddr6->sin6_family = AF_INET6;

	laddr6->sin6_port = htons(cm_node->loc_port);
	raddr6->sin6_port = htons(cm_node->rem_port);

	irdma_copy_ip_htonl(laddr6->sin6_addr.__u6_addr.__u6_addr32,
			    cm_node->loc_addr);
	irdma_copy_ip_htonl(raddr6->sin6_addr.__u6_addr.__u6_addr32,
			    cm_node->rem_addr);
}

/**
 * irdma_get_cmevent_info - for cm event upcall
 * @cm_node: connection's node
 * @cm_id: upper layers cm struct for the event
 * @event: upper layer's cm event
 */
static inline void
irdma_get_cmevent_info(struct irdma_cm_node *cm_node,
		       struct iw_cm_id *cm_id,
		       struct iw_cm_event *event)
{
	memcpy(&event->local_addr, &cm_id->m_local_addr,
	       sizeof(event->local_addr));
	memcpy(&event->remote_addr, &cm_id->m_remote_addr,
	       sizeof(event->remote_addr));
	if (cm_node) {
		event->private_data = cm_node->pdata_buf;
		event->private_data_len = (u8)cm_node->pdata.size;
		event->ird = cm_node->ird_size;
		event->ord = cm_node->ord_size;
	}
}

/**
 * irdma_send_cm_event - upcall cm's event handler
 * @cm_node: connection's node
 * @cm_id: upper layer's cm info struct
 * @type: Event type to indicate
 * @status: status for the event type
 */
static int
irdma_send_cm_event(struct irdma_cm_node *cm_node,
		    struct iw_cm_id *cm_id,
		    enum iw_cm_event_type type, int status)
{
	struct iw_cm_event event = {0};

	event.event = type;
	event.status = status;

	irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
		    "cm_node %p cm_id=%p state=%d accel=%d event_type=%d status=%d\n",
		    cm_node, cm_id, cm_node->accelerated, cm_node->state, type,
		    status);

	switch (type) {
	case IW_CM_EVENT_CONNECT_REQUEST:
		if (cm_node->ipv4)
			irdma_fill_sockaddr4(cm_node, &event);
		else
			irdma_fill_sockaddr6(cm_node, &event);
		event.provider_data = cm_node;
		event.private_data = cm_node->pdata_buf;
		event.private_data_len = (u8)cm_node->pdata.size;
		event.ird = cm_node->ird_size;
		break;
	case IW_CM_EVENT_CONNECT_REPLY:
		irdma_get_cmevent_info(cm_node, cm_id, &event);
		break;
	case IW_CM_EVENT_ESTABLISHED:
		event.ird = cm_node->ird_size;
		event.ord = cm_node->ord_size;
		break;
	case IW_CM_EVENT_DISCONNECT:
	case IW_CM_EVENT_CLOSE:
		/* Wait if we are in RTS but havent issued the iwcm event upcall */
		if (!cm_node->accelerated)
			wait_for_completion(&cm_node->establish_comp);
		break;
	default:
		return -EINVAL;
	}

	return cm_id->event_handler(cm_id, &event);
}

/**
 * irdma_timer_list_prep - add connection nodes to a list to perform timer tasks
 * @cm_core: cm's core
 * @timer_list: a timer list to which cm_node will be selected
 */
static void
irdma_timer_list_prep(struct irdma_cm_core *cm_core,
		      struct list_head *timer_list)
{
	struct irdma_cm_node *cm_node;
	int bkt;

	HASH_FOR_EACH_RCU(cm_core->cm_hash_tbl, bkt, cm_node, list) {
		if ((cm_node->close_entry || cm_node->send_entry) &&
		    atomic_inc_not_zero(&cm_node->refcnt))
			list_add(&cm_node->timer_entry, timer_list);
	}
}

/**
 * irdma_create_event - create cm event
 * @cm_node: connection's node
 * @type: Event type to generate
 */
static struct irdma_cm_event *
irdma_create_event(struct irdma_cm_node *cm_node,
		   enum irdma_cm_event_type type)
{
	struct irdma_cm_event *event;

	if (!cm_node->cm_id)
		return NULL;

	event = kzalloc(sizeof(*event), GFP_ATOMIC);

	if (!event)
		return NULL;

	event->type = type;
	event->cm_node = cm_node;
	memcpy(event->cm_info.rem_addr, cm_node->rem_addr,
	       sizeof(event->cm_info.rem_addr));
	memcpy(event->cm_info.loc_addr, cm_node->loc_addr,
	       sizeof(event->cm_info.loc_addr));
	event->cm_info.rem_port = cm_node->rem_port;
	event->cm_info.loc_port = cm_node->loc_port;
	event->cm_info.cm_id = cm_node->cm_id;
	irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
		    "node=%p event=%p type=%u dst=%x src=%x\n", cm_node, event,
		    type, event->cm_info.loc_addr[0],
		    event->cm_info.rem_addr[0]);
	irdma_cm_post_event(event);

	return event;
}

/**
 * irdma_free_retrans_entry - free send entry
 * @cm_node: connection's node
 */
static void
irdma_free_retrans_entry(struct irdma_cm_node *cm_node)
{
	struct irdma_device *iwdev = cm_node->iwdev;
	struct irdma_timer_entry *send_entry;

	send_entry = cm_node->send_entry;
	if (!send_entry)
		return;

	cm_node->send_entry = NULL;
	irdma_free_sqbuf(&iwdev->vsi, send_entry->sqbuf);
	kfree(send_entry);
	atomic_dec(&cm_node->refcnt);
}

/**
 * irdma_cleanup_retrans_entry - free send entry with lock
 * @cm_node: connection's node
 */
static void
irdma_cleanup_retrans_entry(struct irdma_cm_node *cm_node)
{
	unsigned long flags;

	spin_lock_irqsave(&cm_node->retrans_list_lock, flags);
	irdma_free_retrans_entry(cm_node);
	spin_unlock_irqrestore(&cm_node->retrans_list_lock, flags);
}

/**
 * irdma_form_ah_cm_frame - get a free packet and build frame with address handle
 * @cm_node: connection's node ionfo to use in frame
 * @options: pointer to options info
 * @hdr: pointer mpa header
 * @pdata: pointer to private data
 * @flags:  indicates FIN or ACK
 */
static struct irdma_puda_buf *
irdma_form_ah_cm_frame(struct irdma_cm_node *cm_node,
		       struct irdma_kmem_info *options,
		       struct irdma_kmem_info *hdr,
		       struct irdma_mpa_priv_info *pdata,
		       u8 flags)
{
	struct irdma_puda_buf *sqbuf;
	struct irdma_sc_vsi *vsi = &cm_node->iwdev->vsi;
	u8 *buf;
	struct tcphdr *tcph;
	u16 pktsize;
	u32 opts_len = 0;
	u32 pd_len = 0;
	u32 hdr_len = 0;

	if (!cm_node->ah || !cm_node->ah->ah_info.ah_valid) {
		irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM, "AH invalid\n");
		return NULL;
	}

	sqbuf = irdma_puda_get_bufpool(vsi->ilq);
	if (!sqbuf) {
		irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM, "SQ buf NULL\n");
		return NULL;
	}

	sqbuf->ah_id = cm_node->ah->ah_info.ah_idx;
	buf = sqbuf->mem.va;
	if (options)
		opts_len = (u32)options->size;

	if (hdr)
		hdr_len = hdr->size;

	if (pdata)
		pd_len = pdata->size;

	pktsize = sizeof(*tcph) + opts_len + hdr_len + pd_len;

	memset(buf, 0, pktsize);

	sqbuf->totallen = pktsize;
	sqbuf->tcphlen = sizeof(*tcph) + opts_len;
	sqbuf->scratch = cm_node;

	tcph = (struct tcphdr *)buf;
	buf += sizeof(*tcph);

	tcph->th_sport = htons(cm_node->loc_port);
	tcph->th_dport = htons(cm_node->rem_port);
	tcph->th_seq = htonl(cm_node->tcp_cntxt.loc_seq_num);

	if (flags & SET_ACK) {
		cm_node->tcp_cntxt.loc_ack_num = cm_node->tcp_cntxt.rcv_nxt;
		tcph->th_ack = htonl(cm_node->tcp_cntxt.loc_ack_num);
		tcph->th_flags |= TH_ACK;
	} else {
		tcph->th_ack = 0;
	}

	if (flags & SET_SYN) {
		cm_node->tcp_cntxt.loc_seq_num++;
		tcph->th_flags |= TH_SYN;
	} else {
		cm_node->tcp_cntxt.loc_seq_num += hdr_len + pd_len;
	}

	if (flags & SET_FIN) {
		cm_node->tcp_cntxt.loc_seq_num++;
		tcph->th_flags |= TH_FIN;
	}

	if (flags & SET_RST)
		tcph->th_flags |= TH_RST;

	tcph->th_off = (u16)((sizeof(*tcph) + opts_len + 3) >> 2);
	sqbuf->tcphlen = tcph->th_off << 2;
	tcph->th_win = htons(cm_node->tcp_cntxt.rcv_wnd);
	tcph->th_urp = 0;

	if (opts_len) {
		memcpy(buf, options->addr, opts_len);
		buf += opts_len;
	}

	if (hdr_len) {
		memcpy(buf, hdr->addr, hdr_len);
		buf += hdr_len;
	}

	if (pdata && pdata->addr)
		memcpy(buf, pdata->addr, pdata->size);

	atomic_set(&sqbuf->refcount, 1);

	irdma_debug_buf(vsi->dev, IRDMA_DEBUG_ILQ, "TRANSMIT ILQ BUFFER",
			sqbuf->mem.va, sqbuf->totallen);

	return sqbuf;
}

/**
 * irdma_form_uda_cm_frame - get a free packet and build frame full tcpip packet
 * @cm_node: connection's node ionfo to use in frame
 * @options: pointer to options info
 * @hdr: pointer mpa header
 * @pdata: pointer to private data
 * @flags:  indicates FIN or ACK
 */
static struct irdma_puda_buf *
irdma_form_uda_cm_frame(struct irdma_cm_node *cm_node,
			struct irdma_kmem_info *options,
			struct irdma_kmem_info *hdr,
			struct irdma_mpa_priv_info *pdata,
			u8 flags)
{
	struct irdma_puda_buf *sqbuf;
	struct irdma_sc_vsi *vsi = &cm_node->iwdev->vsi;
	u8 *buf;

	struct tcphdr *tcph;
	struct ip *iph;
	struct ip6_hdr *ip6h;
	struct ether_header *ethh;
	u16 pktsize;
	u16 eth_hlen = ETH_HLEN;
	u32 opts_len = 0;
	u32 pd_len = 0;
	u32 hdr_len = 0;

	u16 vtag;

	sqbuf = irdma_puda_get_bufpool(vsi->ilq);
	if (!sqbuf)
		return NULL;

	buf = sqbuf->mem.va;

	if (options)
		opts_len = (u32)options->size;

	if (hdr)
		hdr_len = hdr->size;

	if (pdata)
		pd_len = pdata->size;

	if (cm_node->vlan_id < VLAN_N_VID)
		eth_hlen += 4;

	if (cm_node->ipv4)
		pktsize = sizeof(*iph) + sizeof(*tcph);
	else
		pktsize = sizeof(*ip6h) + sizeof(*tcph);
	pktsize += opts_len + hdr_len + pd_len;

	memset(buf, 0, eth_hlen + pktsize);

	sqbuf->totallen = pktsize + eth_hlen;
	sqbuf->maclen = eth_hlen;
	sqbuf->tcphlen = sizeof(*tcph) + opts_len;
	sqbuf->scratch = cm_node;

	ethh = (struct ether_header *)buf;
	buf += eth_hlen;

	if (cm_node->do_lpb)
		sqbuf->do_lpb = true;

	if (cm_node->ipv4) {
		sqbuf->ipv4 = true;

		iph = (struct ip *)buf;
		buf += sizeof(*iph);
		tcph = (struct tcphdr *)buf;
		buf += sizeof(*tcph);

		ether_addr_copy(ethh->ether_dhost, cm_node->rem_mac);
		ether_addr_copy(ethh->ether_shost, cm_node->loc_mac);
		if (cm_node->vlan_id < VLAN_N_VID) {
			((struct ether_vlan_header *)ethh)->evl_proto =
			    htons(ETH_P_8021Q);
			vtag = (cm_node->user_pri << VLAN_PRIO_SHIFT) |
			    cm_node->vlan_id;
			((struct ether_vlan_header *)ethh)->evl_tag = htons(vtag);

			((struct ether_vlan_header *)ethh)->evl_encap_proto =
			    htons(ETH_P_IP);
		} else {
			ethh->ether_type = htons(ETH_P_IP);
		}

		iph->ip_v = IPVERSION;
		iph->ip_hl = 5;	/* 5 * 4Byte words, IP headr len */
		iph->ip_tos = cm_node->tos;
		iph->ip_len = htons(pktsize);
		iph->ip_id = htons(++cm_node->tcp_cntxt.loc_id);

		iph->ip_off = htons(0x4000);
		iph->ip_ttl = 0x40;
		iph->ip_p = IPPROTO_TCP;
		iph->ip_src.s_addr = htonl(cm_node->loc_addr[0]);
		iph->ip_dst.s_addr = htonl(cm_node->rem_addr[0]);
	} else {
		sqbuf->ipv4 = false;
		ip6h = (struct ip6_hdr *)buf;
		buf += sizeof(*ip6h);
		tcph = (struct tcphdr *)buf;
		buf += sizeof(*tcph);

		ether_addr_copy(ethh->ether_dhost, cm_node->rem_mac);
		ether_addr_copy(ethh->ether_shost, cm_node->loc_mac);
		if (cm_node->vlan_id < VLAN_N_VID) {
			((struct ether_vlan_header *)ethh)->evl_proto =
			    htons(ETH_P_8021Q);
			vtag = (cm_node->user_pri << VLAN_PRIO_SHIFT) |
			    cm_node->vlan_id;
			((struct ether_vlan_header *)ethh)->evl_tag = htons(vtag);
			((struct ether_vlan_header *)ethh)->evl_encap_proto =
			    htons(ETH_P_IPV6);
		} else {
			ethh->ether_type = htons(ETH_P_IPV6);
		}
		ip6h->ip6_vfc = 6 << 4;
		ip6h->ip6_vfc |= cm_node->tos >> 4;
		ip6h->ip6_flow = cm_node->tos << 20;
		ip6h->ip6_plen = htons(pktsize - sizeof(*ip6h));
		ip6h->ip6_nxt = 6;
		ip6h->ip6_hops = 128;
		irdma_copy_ip_htonl(ip6h->ip6_src.__u6_addr.__u6_addr32,
				    cm_node->loc_addr);
		irdma_copy_ip_htonl(ip6h->ip6_dst.__u6_addr.__u6_addr32,
				    cm_node->rem_addr);
	}

	tcph->th_sport = htons(cm_node->loc_port);
	tcph->th_dport = htons(cm_node->rem_port);
	tcph->th_seq = htonl(cm_node->tcp_cntxt.loc_seq_num);

	if (flags & SET_ACK) {
		cm_node->tcp_cntxt.loc_ack_num = cm_node->tcp_cntxt.rcv_nxt;
		tcph->th_ack = htonl(cm_node->tcp_cntxt.loc_ack_num);
		tcph->th_flags |= TH_ACK;
	} else {
		tcph->th_ack = 0;
	}

	if (flags & SET_SYN) {
		cm_node->tcp_cntxt.loc_seq_num++;
		tcph->th_flags |= TH_SYN;
	} else {
		cm_node->tcp_cntxt.loc_seq_num += hdr_len + pd_len;
	}

	if (flags & SET_FIN) {
		cm_node->tcp_cntxt.loc_seq_num++;
		tcph->th_flags |= TH_FIN;
	}

	if (flags & SET_RST)
		tcph->th_flags |= TH_RST;

	tcph->th_off = (u16)((sizeof(*tcph) + opts_len + 3) >> 2);
	sqbuf->tcphlen = tcph->th_off << 2;
	tcph->th_win = htons(cm_node->tcp_cntxt.rcv_wnd);
	tcph->th_urp = 0;

	if (opts_len) {
		memcpy(buf, options->addr, opts_len);
		buf += opts_len;
	}

	if (hdr_len) {
		memcpy(buf, hdr->addr, hdr_len);
		buf += hdr_len;
	}

	if (pdata && pdata->addr)
		memcpy(buf, pdata->addr, pdata->size);

	atomic_set(&sqbuf->refcount, 1);

	irdma_debug_buf(vsi->dev, IRDMA_DEBUG_ILQ, "TRANSMIT ILQ BUFFER",
			sqbuf->mem.va, sqbuf->totallen);

	return sqbuf;
}

/**
 * irdma_send_reset - Send RST packet
 * @cm_node: connection's node
 */
int
irdma_send_reset(struct irdma_cm_node *cm_node)
{
	struct irdma_puda_buf *sqbuf;
	int flags = SET_RST | SET_ACK;

	sqbuf = cm_node->cm_core->form_cm_frame(cm_node, NULL, NULL, NULL,
						flags);
	if (!sqbuf)
		return -ENOMEM;

	irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
		    "caller: %pS cm_node %p cm_id=%p accel=%d state=%d rem_port=0x%04x, loc_port=0x%04x rem_addr=%x loc_addr=%x\n",
		    __builtin_return_address(0), cm_node, cm_node->cm_id,
		    cm_node->accelerated, cm_node->state, cm_node->rem_port,
		    cm_node->loc_port, cm_node->rem_addr[0],
		    cm_node->loc_addr[0]);

	return irdma_schedule_cm_timer(cm_node, sqbuf, IRDMA_TIMER_TYPE_SEND, 0,
				       1);
}

/**
 * irdma_active_open_err - send event for active side cm error
 * @cm_node: connection's node
 * @reset: Flag to send reset or not
 */
static void
irdma_active_open_err(struct irdma_cm_node *cm_node, bool reset)
{
	irdma_cleanup_retrans_entry(cm_node);
	cm_node->cm_core->stats_connect_errs++;
	if (reset) {
		irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "cm_node=%p state=%d\n", cm_node, cm_node->state);
		atomic_inc(&cm_node->refcnt);
		irdma_send_reset(cm_node);
	}

	cm_node->state = IRDMA_CM_STATE_CLOSED;
	irdma_create_event(cm_node, IRDMA_CM_EVENT_ABORTED);
}

/**
 * irdma_passive_open_err - handle passive side cm error
 * @cm_node: connection's node
 * @reset: send reset or just free cm_node
 */
static void
irdma_passive_open_err(struct irdma_cm_node *cm_node, bool reset)
{
	irdma_cleanup_retrans_entry(cm_node);
	cm_node->cm_core->stats_passive_errs++;
	cm_node->state = IRDMA_CM_STATE_CLOSED;
	irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
		    "cm_node=%p state=%d\n", cm_node, cm_node->state);
	if (reset)
		irdma_send_reset(cm_node);
	else
		irdma_rem_ref_cm_node(cm_node);
}

/**
 * irdma_event_connect_error - to create connect error event
 * @event: cm information for connect event
 */
static void
irdma_event_connect_error(struct irdma_cm_event *event)
{
	struct irdma_qp *iwqp;
	struct iw_cm_id *cm_id;

	cm_id = event->cm_node->cm_id;
	if (!cm_id)
		return;

	iwqp = cm_id->provider_data;

	if (!iwqp || !iwqp->iwdev)
		return;

	iwqp->cm_id = NULL;
	cm_id->provider_data = NULL;
	irdma_send_cm_event(event->cm_node, cm_id, IW_CM_EVENT_CONNECT_REPLY,
			    -ECONNRESET);
	irdma_rem_ref_cm_node(event->cm_node);
}

/**
 * irdma_process_options - process options from TCP header
 * @cm_node: connection's node
 * @optionsloc: point to start of options
 * @optionsize: size of all options
 * @syn_pkt: flag if syn packet
 */
static int
irdma_process_options(struct irdma_cm_node *cm_node, u8 *optionsloc,
		      u32 optionsize, u32 syn_pkt)
{
	u32 tmp;
	u32 offset = 0;
	union all_known_options *all_options;
	char got_mss_option = 0;

	while (offset < optionsize) {
		all_options = (union all_known_options *)(optionsloc + offset);
		switch (all_options->base.optionnum) {
		case OPTION_NUM_EOL:
			offset = optionsize;
			break;
		case OPTION_NUM_NONE:
			offset += 1;
			continue;
		case OPTION_NUM_MSS:
			irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
				    "MSS Length: %d Offset: %d Size: %d\n",
				    all_options->mss.len, offset, optionsize);
			got_mss_option = 1;
			if (all_options->mss.len != 4)
				return -EINVAL;
			tmp = ntohs(all_options->mss.mss);
			if ((cm_node->ipv4 &&
			     (tmp + IRDMA_MTU_TO_MSS_IPV4) < IRDMA_MIN_MTU_IPV4) ||
			    (!cm_node->ipv4 &&
			     (tmp + IRDMA_MTU_TO_MSS_IPV6) < IRDMA_MIN_MTU_IPV6))
				return -EINVAL;
			if (tmp < cm_node->tcp_cntxt.mss)
				cm_node->tcp_cntxt.mss = tmp;
			break;
		case OPTION_NUM_WINDOW_SCALE:
			cm_node->tcp_cntxt.snd_wscale =
			    all_options->windowscale.shiftcount;
			break;
		default:
			irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
				    "Unsupported TCP Option: %x\n",
				    all_options->base.optionnum);
			break;
		}
		offset += all_options->base.len;
	}
	if (!got_mss_option && syn_pkt)
		cm_node->tcp_cntxt.mss = IRDMA_CM_DEFAULT_MSS;

	return 0;
}

/**
 * irdma_handle_tcp_options - setup TCP context info after parsing TCP options
 * @cm_node: connection's node
 * @tcph: pointer tcp header
 * @optionsize: size of options rcvd
 * @passive: active or passive flag
 */
static int
irdma_handle_tcp_options(struct irdma_cm_node *cm_node,
			 struct tcphdr *tcph, int optionsize,
			 int passive)
{
	u8 *optionsloc = (u8 *)&tcph[1];
	int ret;

	if (optionsize) {
		ret = irdma_process_options(cm_node, optionsloc, optionsize,
					    (u32)tcph->th_flags & TH_SYN);
		if (ret) {
			irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
				    "Node %p, Sending Reset\n", cm_node);
			if (passive)
				irdma_passive_open_err(cm_node, true);
			else
				irdma_active_open_err(cm_node, true);
			return ret;
		}
	}

	cm_node->tcp_cntxt.snd_wnd = ntohs(tcph->th_win)
	    << cm_node->tcp_cntxt.snd_wscale;

	if (cm_node->tcp_cntxt.snd_wnd > cm_node->tcp_cntxt.max_snd_wnd)
		cm_node->tcp_cntxt.max_snd_wnd = cm_node->tcp_cntxt.snd_wnd;

	return 0;
}

/**
 * irdma_build_mpa_v1 - build a MPA V1 frame
 * @cm_node: connection's node
 * @start_addr: address where to build frame
 * @mpa_key: to do read0 or write0
 */
static void
irdma_build_mpa_v1(struct irdma_cm_node *cm_node, void *start_addr,
		   u8 mpa_key)
{
	struct ietf_mpa_v1 *mpa_frame = start_addr;

	switch (mpa_key) {
	case MPA_KEY_REQUEST:
		memcpy(mpa_frame->key, IEFT_MPA_KEY_REQ, IETF_MPA_KEY_SIZE);
		break;
	case MPA_KEY_REPLY:
		memcpy(mpa_frame->key, IEFT_MPA_KEY_REP, IETF_MPA_KEY_SIZE);
		break;
	default:
		break;
	}
	mpa_frame->flags = IETF_MPA_FLAGS_CRC;
	mpa_frame->rev = cm_node->mpa_frame_rev;
	mpa_frame->priv_data_len = htons(cm_node->pdata.size);
}

/**
 * irdma_build_mpa_v2 - build a MPA V2 frame
 * @cm_node: connection's node
 * @start_addr: buffer start address
 * @mpa_key: to do read0 or write0
 */
static void
irdma_build_mpa_v2(struct irdma_cm_node *cm_node, void *start_addr,
		   u8 mpa_key)
{
	struct ietf_mpa_v2 *mpa_frame = start_addr;
	struct ietf_rtr_msg *rtr_msg = &mpa_frame->rtr_msg;
	u16 ctrl_ird, ctrl_ord;

	/* initialize the upper 5 bytes of the frame */
	irdma_build_mpa_v1(cm_node, start_addr, mpa_key);
	mpa_frame->flags |= IETF_MPA_V2_FLAG;
	if (cm_node->iwdev->iw_ooo) {
		mpa_frame->flags |= IETF_MPA_FLAGS_MARKERS;
		cm_node->rcv_mark_en = true;
	}
	mpa_frame->priv_data_len = cpu_to_be16(be16_to_cpu(mpa_frame->priv_data_len) +
					       IETF_RTR_MSG_SIZE);

	/* initialize RTR msg */
	if (cm_node->mpav2_ird_ord == IETF_NO_IRD_ORD) {
		ctrl_ird = IETF_NO_IRD_ORD;
		ctrl_ord = IETF_NO_IRD_ORD;
	} else {
		ctrl_ird = (cm_node->ird_size > IETF_NO_IRD_ORD) ?
		    IETF_NO_IRD_ORD :
		    cm_node->ird_size;
		ctrl_ord = (cm_node->ord_size > IETF_NO_IRD_ORD) ?
		    IETF_NO_IRD_ORD :
		    cm_node->ord_size;
	}
	ctrl_ird |= IETF_PEER_TO_PEER;

	switch (mpa_key) {
	case MPA_KEY_REQUEST:
		ctrl_ord |= IETF_RDMA0_WRITE;
		ctrl_ord |= IETF_RDMA0_READ;
		break;
	case MPA_KEY_REPLY:
		switch (cm_node->send_rdma0_op) {
		case SEND_RDMA_WRITE_ZERO:
			ctrl_ord |= IETF_RDMA0_WRITE;
			break;
		case SEND_RDMA_READ_ZERO:
			ctrl_ord |= IETF_RDMA0_READ;
			break;
		}
		break;
	default:
		break;
	}
	rtr_msg->ctrl_ird = htons(ctrl_ird);
	rtr_msg->ctrl_ord = htons(ctrl_ord);
}

/**
 * irdma_cm_build_mpa_frame - build mpa frame for mpa version 1 or version 2
 * @cm_node: connection's node
 * @mpa: mpa: data buffer
 * @mpa_key: to do read0 or write0
 */
static int
irdma_cm_build_mpa_frame(struct irdma_cm_node *cm_node,
			 struct irdma_kmem_info *mpa, u8 mpa_key)
{
	int hdr_len = 0;

	switch (cm_node->mpa_frame_rev) {
	case IETF_MPA_V1:
		hdr_len = sizeof(struct ietf_mpa_v1);
		irdma_build_mpa_v1(cm_node, mpa->addr, mpa_key);
		break;
	case IETF_MPA_V2:
		hdr_len = sizeof(struct ietf_mpa_v2);
		irdma_build_mpa_v2(cm_node, mpa->addr, mpa_key);
		break;
	default:
		break;
	}

	return hdr_len;
}

/**
 * irdma_send_mpa_request - active node send mpa request to passive node
 * @cm_node: connection's node
 */
static int
irdma_send_mpa_request(struct irdma_cm_node *cm_node)
{
	struct irdma_puda_buf *sqbuf;

	cm_node->mpa_hdr.addr = &cm_node->mpa_v2_frame;
	cm_node->mpa_hdr.size = irdma_cm_build_mpa_frame(cm_node,
							 &cm_node->mpa_hdr,
							 MPA_KEY_REQUEST);
	if (!cm_node->mpa_hdr.size) {
		irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "mpa size = %d\n", cm_node->mpa_hdr.size);
		return -EINVAL;
	}

	sqbuf = cm_node->cm_core->form_cm_frame(cm_node, NULL,
						&cm_node->mpa_hdr,
						&cm_node->pdata, SET_ACK);
	if (!sqbuf)
		return -ENOMEM;

	return irdma_schedule_cm_timer(cm_node, sqbuf, IRDMA_TIMER_TYPE_SEND, 1,
				       0);
}

/**
 * irdma_send_mpa_reject -
 * @cm_node: connection's node
 * @pdata: reject data for connection
 * @plen: length of reject data
 */
static int
irdma_send_mpa_reject(struct irdma_cm_node *cm_node,
		      const void *pdata, u8 plen)
{
	struct irdma_puda_buf *sqbuf;
	struct irdma_mpa_priv_info priv_info;

	cm_node->mpa_hdr.addr = &cm_node->mpa_v2_frame;
	cm_node->mpa_hdr.size = irdma_cm_build_mpa_frame(cm_node,
							 &cm_node->mpa_hdr,
							 MPA_KEY_REPLY);

	cm_node->mpa_v2_frame.flags |= IETF_MPA_FLAGS_REJECT;
	priv_info.addr = pdata;
	priv_info.size = plen;

	sqbuf = cm_node->cm_core->form_cm_frame(cm_node, NULL,
						&cm_node->mpa_hdr, &priv_info,
						SET_ACK | SET_FIN);
	if (!sqbuf)
		return -ENOMEM;

	cm_node->state = IRDMA_CM_STATE_FIN_WAIT1;

	return irdma_schedule_cm_timer(cm_node, sqbuf, IRDMA_TIMER_TYPE_SEND, 1,
				       0);
}

/**
 * irdma_negotiate_mpa_v2_ird_ord - negotiate MPAv2 IRD/ORD
 * @cm_node: connection's node
 * @buf: Data pointer
 */
static int
irdma_negotiate_mpa_v2_ird_ord(struct irdma_cm_node *cm_node,
			       u8 *buf)
{
	struct ietf_mpa_v2 *mpa_v2_frame;
	struct ietf_rtr_msg *rtr_msg;
	u16 ird_size;
	u16 ord_size;
	u16 ctrl_ord;
	u16 ctrl_ird;

	mpa_v2_frame = (struct ietf_mpa_v2 *)buf;
	rtr_msg = &mpa_v2_frame->rtr_msg;

	/* parse rtr message */
	ctrl_ord = ntohs(rtr_msg->ctrl_ord);
	ctrl_ird = ntohs(rtr_msg->ctrl_ird);
	ird_size = ctrl_ird & IETF_NO_IRD_ORD;
	ord_size = ctrl_ord & IETF_NO_IRD_ORD;

	if (!(ctrl_ird & IETF_PEER_TO_PEER))
		return -EOPNOTSUPP;

	if (ird_size == IETF_NO_IRD_ORD || ord_size == IETF_NO_IRD_ORD) {
		cm_node->mpav2_ird_ord = IETF_NO_IRD_ORD;
		goto negotiate_done;
	}

	if (cm_node->state != IRDMA_CM_STATE_MPAREQ_SENT) {
		/* responder */
		if (!ord_size && (ctrl_ord & IETF_RDMA0_READ))
			cm_node->ird_size = 1;
		if (cm_node->ord_size > ird_size)
			cm_node->ord_size = ird_size;
	} else {
		/* initiator */
		if (!ird_size && (ctrl_ord & IETF_RDMA0_READ))
			/* Remote peer doesn't support RDMA0_READ */
			return -EOPNOTSUPP;

		if (cm_node->ord_size > ird_size)
			cm_node->ord_size = ird_size;

		if (cm_node->ird_size < ord_size)
			/* no resources available */
			return -EINVAL;
	}

negotiate_done:
	if (ctrl_ord & IETF_RDMA0_READ)
		cm_node->send_rdma0_op = SEND_RDMA_READ_ZERO;
	else if (ctrl_ord & IETF_RDMA0_WRITE)
		cm_node->send_rdma0_op = SEND_RDMA_WRITE_ZERO;
	else
		/* Not supported RDMA0 operation */
		return -EOPNOTSUPP;

	irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
		    "MPAV2 Negotiated ORD: %d, IRD: %d\n", cm_node->ord_size,
		    cm_node->ird_size);
	return 0;
}

/**
 * irdma_parse_mpa - process an IETF MPA frame
 * @cm_node: connection's node
 * @buf: Data pointer
 * @type: to return accept or reject
 * @len: Len of mpa buffer
 */
static int
irdma_parse_mpa(struct irdma_cm_node *cm_node, u8 *buf, u32 *type,
		u32 len)
{
	struct ietf_mpa_v1 *mpa_frame;
	int mpa_hdr_len, priv_data_len, ret;

	*type = IRDMA_MPA_REQUEST_ACCEPT;

	if (len < sizeof(*mpa_frame)) {
		irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "ietf buffer small (%x)\n", len);
		return -EINVAL;
	}

	mpa_frame = (struct ietf_mpa_v1 *)buf;
	mpa_hdr_len = sizeof(*mpa_frame);
	priv_data_len = ntohs(mpa_frame->priv_data_len);

	if (priv_data_len > IETF_MAX_PRIV_DATA_LEN) {
		irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "private_data too big %d\n", priv_data_len);
		return -EOVERFLOW;
	}

	if (mpa_frame->rev != IETF_MPA_V1 && mpa_frame->rev != IETF_MPA_V2) {
		irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "unsupported mpa rev = %d\n", mpa_frame->rev);
		return -EINVAL;
	}

	if (mpa_frame->rev > cm_node->mpa_frame_rev) {
		irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "rev %d\n", mpa_frame->rev);
		return -EINVAL;
	}

	cm_node->mpa_frame_rev = mpa_frame->rev;
	if (cm_node->state != IRDMA_CM_STATE_MPAREQ_SENT) {
		if (memcmp(mpa_frame->key, IEFT_MPA_KEY_REQ,
			   IETF_MPA_KEY_SIZE)) {
			irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
				    "Unexpected MPA Key received\n");
			return -EINVAL;
		}
	} else {
		if (memcmp(mpa_frame->key, IEFT_MPA_KEY_REP,
			   IETF_MPA_KEY_SIZE)) {
			irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
				    "Unexpected MPA Key received\n");
			return -EINVAL;
		}
	}

	if (priv_data_len + mpa_hdr_len > len) {
		irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "ietf buffer len(%x + %x != %x)\n", priv_data_len,
			    mpa_hdr_len, len);
		return -EOVERFLOW;
	}

	if (len > IRDMA_MAX_CM_BUF) {
		irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "ietf buffer large len = %d\n", len);
		return -EOVERFLOW;
	}

	switch (mpa_frame->rev) {
	case IETF_MPA_V2:
		mpa_hdr_len += IETF_RTR_MSG_SIZE;
		ret = irdma_negotiate_mpa_v2_ird_ord(cm_node, buf);
		if (ret)
			return ret;
		break;
	case IETF_MPA_V1:
	default:
		break;
	}

	memcpy(cm_node->pdata_buf, buf + mpa_hdr_len, priv_data_len);
	cm_node->pdata.size = priv_data_len;

	if (mpa_frame->flags & IETF_MPA_FLAGS_REJECT)
		*type = IRDMA_MPA_REQUEST_REJECT;

	if (mpa_frame->flags & IETF_MPA_FLAGS_MARKERS)
		cm_node->snd_mark_en = true;

	return 0;
}

/**
 * irdma_schedule_cm_timer
 * @cm_node: connection's node
 * @sqbuf: buffer to send
 * @type: if it is send or close
 * @send_retrans: if rexmits to be done
 * @close_when_complete: is cm_node to be removed
 *
 * note - cm_node needs to be protected before calling this. Encase in:
 *		irdma_rem_ref_cm_node(cm_core, cm_node);
 *		irdma_schedule_cm_timer(...)
 *		atomic_inc(&cm_node->refcnt);
 */
int
irdma_schedule_cm_timer(struct irdma_cm_node *cm_node,
			struct irdma_puda_buf *sqbuf,
			enum irdma_timer_type type, int send_retrans,
			int close_when_complete)
{
	struct irdma_sc_vsi *vsi = &cm_node->iwdev->vsi;
	struct irdma_cm_core *cm_core = cm_node->cm_core;
	struct irdma_timer_entry *new_send;
	u32 was_timer_set;
	unsigned long flags;

	new_send = kzalloc(sizeof(*new_send), GFP_ATOMIC);
	if (!new_send) {
		if (type != IRDMA_TIMER_TYPE_CLOSE)
			irdma_free_sqbuf(vsi, sqbuf);
		return -ENOMEM;
	}

	new_send->retrycount = IRDMA_DEFAULT_RETRYS;
	new_send->retranscount = IRDMA_DEFAULT_RETRANS;
	new_send->sqbuf = sqbuf;
	new_send->timetosend = jiffies;
	new_send->type = type;
	new_send->send_retrans = send_retrans;
	new_send->close_when_complete = close_when_complete;

	if (type == IRDMA_TIMER_TYPE_CLOSE) {
		new_send->timetosend += (HZ / 10);
		if (cm_node->close_entry) {
			kfree(new_send);
			irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
				    "already close entry\n");
			return -EINVAL;
		}

		cm_node->close_entry = new_send;
	} else {		/* type == IRDMA_TIMER_TYPE_SEND */
		spin_lock_irqsave(&cm_node->retrans_list_lock, flags);
		cm_node->send_entry = new_send;
		atomic_inc(&cm_node->refcnt);
		spin_unlock_irqrestore(&cm_node->retrans_list_lock, flags);
		new_send->timetosend = jiffies + IRDMA_RETRY_TIMEOUT;

		atomic_inc(&sqbuf->refcount);
		irdma_puda_send_buf(vsi->ilq, sqbuf);
		if (!send_retrans) {
			irdma_cleanup_retrans_entry(cm_node);
			if (close_when_complete)
				irdma_rem_ref_cm_node(cm_node);
			return 0;
		}
	}

	spin_lock_irqsave(&cm_core->ht_lock, flags);
	was_timer_set = timer_pending(&cm_core->tcp_timer);

	if (!was_timer_set) {
		cm_core->tcp_timer.expires = new_send->timetosend;
		add_timer(&cm_core->tcp_timer);
	}
	spin_unlock_irqrestore(&cm_core->ht_lock, flags);

	return 0;
}

/**
 * irdma_retrans_expired - Could not rexmit the packet
 * @cm_node: connection's node
 */
static void
irdma_retrans_expired(struct irdma_cm_node *cm_node)
{
	enum irdma_cm_node_state state = cm_node->state;

	cm_node->state = IRDMA_CM_STATE_CLOSED;
	switch (state) {
	case IRDMA_CM_STATE_SYN_RCVD:
	case IRDMA_CM_STATE_CLOSING:
		irdma_rem_ref_cm_node(cm_node);
		break;
	case IRDMA_CM_STATE_FIN_WAIT1:
	case IRDMA_CM_STATE_LAST_ACK:
		irdma_send_reset(cm_node);
		break;
	default:
		atomic_inc(&cm_node->refcnt);
		irdma_send_reset(cm_node);
		irdma_create_event(cm_node, IRDMA_CM_EVENT_ABORTED);
		break;
	}
}

/**
 * irdma_handle_close_entry - for handling retry/timeouts
 * @cm_node: connection's node
 * @rem_node: flag for remove cm_node
 */
static void
irdma_handle_close_entry(struct irdma_cm_node *cm_node,
			 u32 rem_node)
{
	struct irdma_timer_entry *close_entry = cm_node->close_entry;
	struct irdma_qp *iwqp;
	unsigned long flags;

	if (!close_entry)
		return;
	iwqp = (struct irdma_qp *)close_entry->sqbuf;
	if (iwqp) {
		spin_lock_irqsave(&iwqp->lock, flags);
		if (iwqp->cm_id) {
			iwqp->hw_tcp_state = IRDMA_TCP_STATE_CLOSED;
			iwqp->hw_iwarp_state = IRDMA_QP_STATE_ERROR;
			iwqp->last_aeq = IRDMA_AE_RESET_SENT;
			iwqp->ibqp_state = IB_QPS_ERR;
			spin_unlock_irqrestore(&iwqp->lock, flags);
			irdma_cm_disconn(iwqp);
		} else {
			spin_unlock_irqrestore(&iwqp->lock, flags);
		}
	} else if (rem_node) {
		/* TIME_WAIT state */
		irdma_rem_ref_cm_node(cm_node);
	}

	kfree(close_entry);
	cm_node->close_entry = NULL;
}

/**
 * irdma_cm_timer_tick - system's timer expired callback
 * @t: Pointer to timer_list
 */
static void
irdma_cm_timer_tick(struct timer_list *t)
{
	unsigned long nexttimeout = jiffies + IRDMA_LONG_TIME;
	struct irdma_cm_node *cm_node;
	struct irdma_timer_entry *send_entry, *close_entry;
	struct list_head *list_core_temp;
	struct list_head *list_node;
	struct irdma_cm_core *cm_core = from_timer(cm_core, t, tcp_timer);
	struct irdma_sc_vsi *vsi;
	u32 settimer = 0;
	unsigned long timetosend;
	unsigned long flags;
	struct list_head timer_list;

	INIT_LIST_HEAD(&timer_list);

	rcu_read_lock();
	irdma_timer_list_prep(cm_core, &timer_list);
	rcu_read_unlock();

	list_for_each_safe(list_node, list_core_temp, &timer_list) {
		cm_node = container_of(list_node, struct irdma_cm_node,
				       timer_entry);
		close_entry = cm_node->close_entry;

		if (close_entry) {
			if (time_after(close_entry->timetosend, jiffies)) {
				if (nexttimeout > close_entry->timetosend ||
				    !settimer) {
					nexttimeout = close_entry->timetosend;
					settimer = 1;
				}
			} else {
				irdma_handle_close_entry(cm_node, 1);
			}
		}

		spin_lock_irqsave(&cm_node->retrans_list_lock, flags);

		send_entry = cm_node->send_entry;
		if (!send_entry)
			goto done;
		if (time_after(send_entry->timetosend, jiffies)) {
			if (cm_node->state != IRDMA_CM_STATE_OFFLOADED) {
				if (nexttimeout > send_entry->timetosend ||
				    !settimer) {
					nexttimeout = send_entry->timetosend;
					settimer = 1;
				}
			} else {
				irdma_free_retrans_entry(cm_node);
			}
			goto done;
		}

		if (cm_node->state == IRDMA_CM_STATE_OFFLOADED ||
		    cm_node->state == IRDMA_CM_STATE_CLOSED) {
			irdma_free_retrans_entry(cm_node);
			goto done;
		}

		if (!send_entry->retranscount || !send_entry->retrycount) {
			irdma_free_retrans_entry(cm_node);

			spin_unlock_irqrestore(&cm_node->retrans_list_lock,
					       flags);
			irdma_retrans_expired(cm_node);
			cm_node->state = IRDMA_CM_STATE_CLOSED;
			spin_lock_irqsave(&cm_node->retrans_list_lock, flags);
			goto done;
		}
		spin_unlock_irqrestore(&cm_node->retrans_list_lock, flags);

		vsi = &cm_node->iwdev->vsi;
		if (!cm_node->ack_rcvd) {
			atomic_inc(&send_entry->sqbuf->refcount);
			irdma_puda_send_buf(vsi->ilq, send_entry->sqbuf);
			cm_node->cm_core->stats_pkt_retrans++;
		}

		spin_lock_irqsave(&cm_node->retrans_list_lock, flags);
		if (send_entry->send_retrans) {
			send_entry->retranscount--;
			timetosend = (IRDMA_RETRY_TIMEOUT <<
				      (IRDMA_DEFAULT_RETRANS -
				       send_entry->retranscount));

			send_entry->timetosend = jiffies +
			    min(timetosend, IRDMA_MAX_TIMEOUT);
			if (nexttimeout > send_entry->timetosend || !settimer) {
				nexttimeout = send_entry->timetosend;
				settimer = 1;
			}
		} else {
			int close_when_complete;

			close_when_complete = send_entry->close_when_complete;
			irdma_free_retrans_entry(cm_node);
			if (close_when_complete)
				irdma_rem_ref_cm_node(cm_node);
		}
done:
		spin_unlock_irqrestore(&cm_node->retrans_list_lock, flags);
		irdma_rem_ref_cm_node(cm_node);
	}

	if (settimer) {
		spin_lock_irqsave(&cm_core->ht_lock, flags);
		if (!timer_pending(&cm_core->tcp_timer)) {
			cm_core->tcp_timer.expires = nexttimeout;
			add_timer(&cm_core->tcp_timer);
		}
		spin_unlock_irqrestore(&cm_core->ht_lock, flags);
	}
}

/**
 * irdma_send_syn - send SYN packet
 * @cm_node: connection's node
 * @sendack: flag to set ACK bit or not
 */
int
irdma_send_syn(struct irdma_cm_node *cm_node, u32 sendack)
{
	struct irdma_puda_buf *sqbuf;
	int flags = SET_SYN;
	char optionsbuf[sizeof(struct option_mss) +
			sizeof(struct option_windowscale) +
			sizeof(struct option_base) + TCP_OPTIONS_PADDING];
	struct irdma_kmem_info opts;
	int optionssize = 0;

	/* Sending MSS option */
	union all_known_options *options;

	opts.addr = optionsbuf;
	if (!cm_node)
		return -EINVAL;

	options = (union all_known_options *)&optionsbuf[optionssize];
	options->mss.optionnum = OPTION_NUM_MSS;
	options->mss.len = sizeof(struct option_mss);
	options->mss.mss = htons(cm_node->tcp_cntxt.mss);
	optionssize += sizeof(struct option_mss);

	options = (union all_known_options *)&optionsbuf[optionssize];
	options->windowscale.optionnum = OPTION_NUM_WINDOW_SCALE;
	options->windowscale.len = sizeof(struct option_windowscale);
	options->windowscale.shiftcount = cm_node->tcp_cntxt.rcv_wscale;
	optionssize += sizeof(struct option_windowscale);
	options = (union all_known_options *)&optionsbuf[optionssize];
	options->eol = OPTION_NUM_EOL;
	optionssize += 1;

	if (sendack)
		flags |= SET_ACK;

	opts.size = optionssize;

	sqbuf = cm_node->cm_core->form_cm_frame(cm_node, &opts, NULL, NULL,
						flags);
	if (!sqbuf)
		return -ENOMEM;

	return irdma_schedule_cm_timer(cm_node, sqbuf, IRDMA_TIMER_TYPE_SEND, 1,
				       0);
}

/**
 * irdma_send_ack - Send ACK packet
 * @cm_node: connection's node
 */
void
irdma_send_ack(struct irdma_cm_node *cm_node)
{
	struct irdma_puda_buf *sqbuf;
	struct irdma_sc_vsi *vsi = &cm_node->iwdev->vsi;

	sqbuf = cm_node->cm_core->form_cm_frame(cm_node, NULL, NULL, NULL,
						SET_ACK);
	if (sqbuf)
		irdma_puda_send_buf(vsi->ilq, sqbuf);
}

/**
 * irdma_send_fin - Send FIN pkt
 * @cm_node: connection's node
 */
static int
irdma_send_fin(struct irdma_cm_node *cm_node)
{
	struct irdma_puda_buf *sqbuf;

	sqbuf = cm_node->cm_core->form_cm_frame(cm_node, NULL, NULL, NULL,
						SET_ACK | SET_FIN);
	if (!sqbuf)
		return -ENOMEM;

	return irdma_schedule_cm_timer(cm_node, sqbuf, IRDMA_TIMER_TYPE_SEND, 1,
				       0);
}

/**
 * irdma_find_listener - find a cm node listening on this addr-port pair
 * @cm_core: cm's core
 * @dst_addr: listener ip addr
 * @ipv4: flag indicating IPv4 when true
 * @dst_port: listener tcp port num
 * @vlan_id: virtual LAN ID
 * @listener_state: state to match with listen node's
 */
static struct irdma_cm_listener *
irdma_find_listener(struct irdma_cm_core *cm_core, u32 *dst_addr, bool ipv4, u16 dst_port,
		    u16 vlan_id, enum irdma_cm_listener_state listener_state)
{
	struct irdma_cm_listener *listen_node;
	static const u32 ip_zero[4] = {0, 0, 0, 0};
	u32 listen_addr[4];
	u16 listen_port;
	unsigned long flags;

	/* walk list and find cm_node associated with this session ID */
	spin_lock_irqsave(&cm_core->listen_list_lock, flags);
	list_for_each_entry(listen_node, &cm_core->listen_list, list) {
		memcpy(listen_addr, listen_node->loc_addr, sizeof(listen_addr));
		listen_port = listen_node->loc_port;
		if (listen_node->ipv4 != ipv4 || listen_port != dst_port ||
		    !(listener_state & listen_node->listener_state))
			continue;
		/* compare node pair, return node handle if a match */
		if (!memcmp(listen_addr, ip_zero, sizeof(listen_addr)) ||
		    (!memcmp(listen_addr, dst_addr, sizeof(listen_addr)) &&
		     vlan_id == listen_node->vlan_id)) {
			atomic_inc(&listen_node->refcnt);
			spin_unlock_irqrestore(&cm_core->listen_list_lock,
					       flags);
			return listen_node;
		}
	}
	spin_unlock_irqrestore(&cm_core->listen_list_lock, flags);

	return NULL;
}

/**
 * irdma_del_multiple_qhash - Remove qhash and child listens
 * @iwdev: iWarp device
 * @cm_info: CM info for parent listen node
 * @cm_parent_listen_node: The parent listen node
 */
static int
irdma_del_multiple_qhash(struct irdma_device *iwdev,
			 struct irdma_cm_info *cm_info,
			 struct irdma_cm_listener *cm_parent_listen_node)
{
	struct irdma_cm_listener *child_listen_node;
	struct list_head *pos, *tpos;
	unsigned long flags;
	int ret = -EINVAL;

	spin_lock_irqsave(&iwdev->cm_core.listen_list_lock, flags);
	list_for_each_safe(pos, tpos,
			   &cm_parent_listen_node->child_listen_list) {
		child_listen_node = list_entry(pos, struct irdma_cm_listener,
					       child_listen_list);
		if (child_listen_node->ipv4)
			irdma_debug(&iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
				    "removing child listen for IP=%x, port=%d, vlan=%d\n",
				    child_listen_node->loc_addr[0],
				    child_listen_node->loc_port,
				    child_listen_node->vlan_id);
		else
			irdma_debug(&iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
				    "removing child listen for IP=%x:%x:%x:%x, port=%d, vlan=%d\n",
				    IRDMA_PRINT_IP6(child_listen_node->loc_addr),
				    child_listen_node->loc_port,
				    child_listen_node->vlan_id);
		list_del(pos);
		memcpy(cm_info->loc_addr, child_listen_node->loc_addr,
		       sizeof(cm_info->loc_addr));
		cm_info->vlan_id = child_listen_node->vlan_id;
		if (child_listen_node->qhash_set) {
			ret = irdma_manage_qhash(iwdev, cm_info,
						 IRDMA_QHASH_TYPE_TCP_SYN,
						 IRDMA_QHASH_MANAGE_TYPE_DELETE,
						 NULL, false);
			child_listen_node->qhash_set = false;
		} else {
			ret = 0;
		}
		irdma_debug(&iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "Child listen node freed = %p\n",
			    child_listen_node);
		kfree(child_listen_node);
		cm_parent_listen_node->cm_core->stats_listen_nodes_destroyed++;
	}
	spin_unlock_irqrestore(&iwdev->cm_core.listen_list_lock, flags);

	return ret;
}

static u8 irdma_iw_get_vlan_prio(u32 *loc_addr, u8 prio, bool ipv4)
{
	return prio;
}

/**
 * irdma_get_vlan_mac_ipv6 - Get the vlan and mac for an IPv6
 * address
 * @addr: local IPv6 address
 * @vlan_id: vlan id for the given IPv6 address
 * @mac: mac address for the given IPv6 address
 *
 * Returns the net_device of the IPv6 address and also sets the
 * vlan id and mac for that address.
 */
void
irdma_get_vlan_mac_ipv6(struct iw_cm_id *cm_id, u32 *addr, u16 *vlan_id, u8 *mac)
{
	if_t ip_dev = NULL;
	struct in6_addr laddr6;
	struct vnet *vnet = &init_net;
	struct ifaddr *ifa;
	u16 scope_id = 0;

	irdma_copy_ip_htonl(laddr6.__u6_addr.__u6_addr32, addr);
	if (vlan_id)
		*vlan_id = 0xFFFF;	/* Match rdma_vlan_dev_vlan_id() */
	if (mac)
		eth_zero_addr(mac);

	if (IN6_IS_SCOPE_LINKLOCAL(&laddr6) ||
	    IN6_IS_ADDR_MC_INTFACELOCAL(&laddr6))
		scope_id = ntohs(laddr6.__u6_addr.__u6_addr16[1]);

#ifdef VIMAGE
	vnet = irdma_cmid_to_vnet(cm_id);
#endif
	ip_dev = ip6_ifp_find(vnet, laddr6, scope_id);
	if (ip_dev) {
		if (vlan_id)
			*vlan_id = rdma_vlan_dev_vlan_id(ip_dev);
		ifa = if_getifaddr(ip_dev);
		if (ifa && ifa->ifa_addr && mac)
			ether_addr_copy(mac, if_getlladdr(ip_dev));
	}
}

/**
 * irdma_get_vlan_ipv4 - Returns the vlan_id for IPv4 address
 * @addr: local IPv4 address
 */
u16
irdma_get_vlan_ipv4(struct iw_cm_id *cm_id, u32 *addr)
{
	if_t netdev;
	struct vnet *vnet = &init_net;
	u16 vlan_id = 0xFFFF;

#ifdef VIMAGE
	vnet = irdma_cmid_to_vnet(cm_id);
#endif
	netdev = ip_ifp_find(vnet, htonl(addr[0]));
	if (netdev) {
		vlan_id = rdma_vlan_dev_vlan_id(netdev);
		dev_put(netdev);
	}

	return vlan_id;
}

static int
irdma_manage_qhash_wait(struct irdma_pci_f *rf, struct irdma_cm_info *cm_info)
{
	struct irdma_cqp_request *cqp_request = cm_info->cqp_request;
	int cnt = rf->sc_dev.hw_attrs.max_cqp_compl_wait_time_ms * CQP_TIMEOUT_THRESHOLD;
	u32 ret_val;

	if (!cqp_request)
		return -ENOMEM;
	do {
		irdma_cqp_ce_handler(rf, &rf->ccq.sc_cq);
		mdelay(1);
	} while (!READ_ONCE(cqp_request->request_done) && --cnt);

	ret_val = cqp_request->compl_info.op_ret_val;
	irdma_put_cqp_request(&rf->cqp, cqp_request);
	if (cnt) {
		if (!ret_val)
			return 0;
		return -EINVAL;
	}

	return -ETIMEDOUT;
}

/**
 * irdma_add_mqh_ifa_cb - Adds multiple qhashes for IPv4/IPv6
 * @arg: Calback argument structure from irdma_add_mqh
 * @ifa: Current address to compute against
 * @count: Current cumulative output of all callbacks in this iteration
 *
 * Adds a qhash and a child listen node for a single IPv4/IPv6 address
 * on the adapter and adds the associated qhash filter
 */
static u_int
irdma_add_mqh_ifa_cb(void *arg, struct ifaddr *ifa, u_int count){
	struct irdma_add_mqh_cbs *cbs = arg;
	struct irdma_cm_listener *child_listen_node;
	struct irdma_cm_info *cm_info = cbs->cm_info;
	struct irdma_device *iwdev = cbs->iwdev;
	struct irdma_cm_listener *cm_parent_listen_node = cbs->cm_listen_node;
	if_t ip_dev = ifa->ifa_ifp;
	unsigned long flags;
	int ret;

	if (count)
		return 0;

	child_listen_node = kzalloc(sizeof(*child_listen_node), GFP_ATOMIC);
	if (!child_listen_node) {
		irdma_debug(&iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "listener memory allocation\n");
		return -ENOMEM;
	}

	memcpy(child_listen_node, cm_parent_listen_node,
	       sizeof(*child_listen_node));
	cm_info->vlan_id = rdma_vlan_dev_vlan_id(ip_dev);
	child_listen_node->vlan_id = cm_info->vlan_id;
	if (cm_info->ipv4) {
		irdma_debug(&iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "Allocating child CM Listener forIP=%x, vlan_id=%d, MAC=%x:%x:%x:%x:%x:%x\n",
			    ((struct sockaddr_in *)&ifa->ifa_addr)->sin_addr.s_addr,
			    rdma_vlan_dev_vlan_id(ip_dev),
			    if_getlladdr(ip_dev)[0], if_getlladdr(ip_dev)[1],
			    if_getlladdr(ip_dev)[2], if_getlladdr(ip_dev)[3],
			    if_getlladdr(ip_dev)[4], if_getlladdr(ip_dev)[5]);
		child_listen_node->loc_addr[0] =
		    ntohl(((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr);
	} else {
		irdma_debug(&iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "IP=%x:%x:%x:%x, vlan_id=%d, MAC=%x:%x:%x:%x:%x:%x\n",
			    IRDMA_PRINT_IP6(&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr),
			    rdma_vlan_dev_vlan_id(ip_dev),
			    if_getlladdr(ip_dev)[0], if_getlladdr(ip_dev)[1],
			    if_getlladdr(ip_dev)[2], if_getlladdr(ip_dev)[3],
			    if_getlladdr(ip_dev)[4], if_getlladdr(ip_dev)[5]);
		irdma_copy_ip_ntohl(child_listen_node->loc_addr,
				    ((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr.__u6_addr.__u6_addr32);
	}
	memcpy(cm_info->loc_addr, child_listen_node->loc_addr,
	       sizeof(cm_info->loc_addr));
	if (!iwdev->vsi.dscp_mode)
		cm_info->user_pri =
		    irdma_iw_get_vlan_prio(child_listen_node->loc_addr,
					   cm_info->user_pri,
					   cm_info->ipv4);
	ret = irdma_manage_qhash(iwdev, cm_info,
				 IRDMA_QHASH_TYPE_TCP_SYN,
				 IRDMA_QHASH_MANAGE_TYPE_ADD,
				 NULL, false);
	if (ret) {
		kfree(child_listen_node);
		return ret;
	}
	/* wait for qhash finish */
	ret = irdma_manage_qhash_wait(iwdev->rf, cm_info);
	if (ret) {
		kfree(child_listen_node);
		return ret;
	}

	child_listen_node->qhash_set = true;
	spin_lock_irqsave(&iwdev->cm_core.listen_list_lock, flags);
	list_add(&child_listen_node->child_listen_list,
		 &cm_parent_listen_node->child_listen_list);
	spin_unlock_irqrestore(&iwdev->cm_core.listen_list_lock, flags);
	cm_parent_listen_node->cm_core->stats_listen_nodes_created++;

	return 0;
}

/**
 * irdma_add_mqh - Adds multiple qhashes
 * @iwdev: iWarp device
 * @cm_info: CM info for parent listen node
 * @cm_listen_node: The parent listen node
 */
static int
irdma_add_mqh(struct irdma_device *iwdev,
	      struct irdma_cm_info *cm_info,
	      struct irdma_cm_listener *cm_listen_node)
{
	struct epoch_tracker et;
	struct irdma_add_mqh_cbs cbs;
	struct if_iter iter;
	if_t ifp;
	int err = -ENOENT;

	cbs.iwdev = iwdev;
	cbs.cm_info = cm_info;
	cbs.cm_listen_node = cm_listen_node;

	VNET_ITERATOR_DECL(vnet_iter);

	VNET_LIST_RLOCK();
	NET_EPOCH_ENTER(et);
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET_QUIET(vnet_iter);
		for (ifp = if_iter_start(&iter); ifp != NULL; ifp = if_iter_next(&iter)) {
			if (!(if_getflags(ifp) & IFF_UP))
				continue;

			if (((rdma_vlan_dev_vlan_id(ifp) >= VLAN_N_VID) ||
			     (rdma_vlan_dev_real_dev(ifp) != iwdev->netdev)) &&
			    ifp != iwdev->netdev)
				continue;

			if_addr_rlock(ifp);
			if (cm_info->ipv4)
				err = if_foreach_addr_type(ifp, AF_INET, irdma_add_mqh_ifa_cb, &cbs);
			else
				err = if_foreach_addr_type(ifp, AF_INET6, irdma_add_mqh_ifa_cb, &cbs);
			if_addr_runlock(ifp);
		}
		if_iter_finish(&iter);
		CURVNET_RESTORE();
	}
	NET_EPOCH_EXIT(et);
	VNET_LIST_RUNLOCK();

	return err;
}

/**
 * irdma_reset_list_prep - add connection nodes slated for reset to list
 * @cm_core: cm's core
 * @listener: pointer to listener node
 * @reset_list: a list to which cm_node will be selected
 */
static void
irdma_reset_list_prep(struct irdma_cm_core *cm_core,
		      struct irdma_cm_listener *listener,
		      struct list_head *reset_list)
{
	struct irdma_cm_node *cm_node;
	int bkt;

	HASH_FOR_EACH_RCU(cm_core->cm_hash_tbl, bkt, cm_node, list) {
		if (cm_node->listener == listener &&
		    !cm_node->accelerated &&
		    atomic_inc_not_zero(&cm_node->refcnt))
			list_add(&cm_node->reset_entry, reset_list);
	}
}

/**
 * irdma_dec_refcnt_listen - delete listener and associated cm nodes
 * @cm_core: cm's core
 * @listener: pointer to listener node
 * @free_hanging_nodes: to free associated cm_nodes
 * @apbvt_del: flag to delete the apbvt
 */
static int
irdma_dec_refcnt_listen(struct irdma_cm_core *cm_core,
			struct irdma_cm_listener *listener,
			int free_hanging_nodes, bool apbvt_del)
{
	struct list_head *list_pos;
	struct list_head *list_temp;
	struct irdma_cm_node *cm_node;
	struct list_head reset_list;
	struct irdma_cm_info nfo;
	enum irdma_cm_node_state old_state;
	unsigned long flags;
	int err;

	/* free non-accelerated child nodes for this listener */
	INIT_LIST_HEAD(&reset_list);
	if (free_hanging_nodes) {
		rcu_read_lock();
		irdma_reset_list_prep(cm_core, listener, &reset_list);
		rcu_read_unlock();
	}

	list_for_each_safe(list_pos, list_temp, &reset_list) {
		cm_node = container_of(list_pos, struct irdma_cm_node,
				       reset_entry);
		if (cm_node->state >= IRDMA_CM_STATE_FIN_WAIT1) {
			irdma_rem_ref_cm_node(cm_node);
			continue;
		}

		irdma_cleanup_retrans_entry(cm_node);
		err = irdma_send_reset(cm_node);
		if (err) {
			cm_node->state = IRDMA_CM_STATE_CLOSED;
			irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
				    "send reset failed\n");
		} else {
			old_state = cm_node->state;
			cm_node->state = IRDMA_CM_STATE_LISTENER_DESTROYED;
			if (old_state != IRDMA_CM_STATE_MPAREQ_RCVD)
				irdma_rem_ref_cm_node(cm_node);
		}
	}

	if (atomic_dec_and_test(&listener->refcnt)) {
		spin_lock_irqsave(&cm_core->listen_list_lock, flags);
		list_del(&listener->list);
		spin_unlock_irqrestore(&cm_core->listen_list_lock, flags);

		if (apbvt_del)
			irdma_del_apbvt(listener->iwdev,
					listener->apbvt_entry);
		memcpy(nfo.loc_addr, listener->loc_addr, sizeof(nfo.loc_addr));
		nfo.loc_port = listener->loc_port;
		nfo.ipv4 = listener->ipv4;
		nfo.vlan_id = listener->vlan_id;
		nfo.user_pri = listener->user_pri;
		nfo.qh_qpid = listener->iwdev->vsi.ilq->qp_id;

		if (!list_empty(&listener->child_listen_list)) {
			irdma_del_multiple_qhash(listener->iwdev, &nfo,
						 listener);
		} else {
			if (listener->qhash_set)
				irdma_manage_qhash(listener->iwdev,
						   &nfo,
						   IRDMA_QHASH_TYPE_TCP_SYN,
						   IRDMA_QHASH_MANAGE_TYPE_DELETE,
						   NULL, false);
		}

		cm_core->stats_listen_destroyed++;
		cm_core->stats_listen_nodes_destroyed++;
		irdma_debug(&listener->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "loc_port=0x%04x loc_addr=%x cm_listen_node=%p cm_id=%p qhash_set=%d vlan_id=%d apbvt_del=%d\n",
			    listener->loc_port, listener->loc_addr[0], listener,
			    listener->cm_id, listener->qhash_set,
			    listener->vlan_id, apbvt_del);
		kfree(listener);
		listener = NULL;
		return 0;
	}

	return -EINVAL;
}

/**
 * irdma_cm_del_listen - delete a listener
 * @cm_core: cm's core
 * @listener: passive connection's listener
 * @apbvt_del: flag to delete apbvt
 */
static int
irdma_cm_del_listen(struct irdma_cm_core *cm_core,
		    struct irdma_cm_listener *listener,
		    bool apbvt_del)
{
	listener->listener_state = IRDMA_CM_LISTENER_PASSIVE_STATE;
	listener->cm_id = NULL;

	return irdma_dec_refcnt_listen(cm_core, listener, 1, apbvt_del);
}

/**
 * irdma_find_node - find a cm node that matches the reference cm node
 * @cm_core: cm's core
 * @rem_port: remote tcp port num
 * @rem_addr: remote ip addr
 * @loc_port: local tcp port num
 * @loc_addr: local ip addr
 * @vlan_id: local VLAN ID
 */
struct irdma_cm_node *
irdma_find_node(struct irdma_cm_core *cm_core,
		u16 rem_port, u32 *rem_addr, u16 loc_port,
		u32 *loc_addr, u16 vlan_id)
{
	struct irdma_cm_node *cm_node;
	u32 key = (rem_port << 16) | loc_port;

	rcu_read_lock();
	HASH_FOR_EACH_POSSIBLE_RCU(cm_core->cm_hash_tbl, cm_node, list, key) {
		if (cm_node->vlan_id == vlan_id &&
		    cm_node->loc_port == loc_port && cm_node->rem_port == rem_port &&
		    !memcmp(cm_node->loc_addr, loc_addr, sizeof(cm_node->loc_addr)) &&
		    !memcmp(cm_node->rem_addr, rem_addr, sizeof(cm_node->rem_addr))) {
			if (!atomic_inc_not_zero(&cm_node->refcnt))
				goto exit;
			rcu_read_unlock();
			return cm_node;
		}
	}

exit:
	rcu_read_unlock();

	/* no owner node */
	return NULL;
}

/**
 * irdma_add_hte_node - add a cm node to the hash table
 * @cm_core: cm's core
 * @cm_node: connection's node
 */
static void
irdma_add_hte_node(struct irdma_cm_core *cm_core,
		   struct irdma_cm_node *cm_node)
{
	unsigned long flags;
	u32 key = (cm_node->rem_port << 16) | cm_node->loc_port;

	spin_lock_irqsave(&cm_core->ht_lock, flags);
	HASH_ADD_RCU(cm_core->cm_hash_tbl, &cm_node->list, key);
	spin_unlock_irqrestore(&cm_core->ht_lock, flags);
}

/**
 * irdma_ipv4_is_lpb - check if loopback
 * @loc_addr: local addr to compare
 * @rem_addr: remote address
 */
bool
irdma_ipv4_is_lpb(u32 loc_addr, u32 rem_addr)
{
	return ipv4_is_loopback(htonl(rem_addr)) || (loc_addr == rem_addr);
}

/**
 * irdma_ipv6_is_lpb - check if loopback
 * @loc_addr: local addr to compare
 * @rem_addr: remote address
 */
bool
irdma_ipv6_is_lpb(u32 *loc_addr, u32 *rem_addr)
{
	struct in6_addr raddr6;

	irdma_copy_ip_htonl(raddr6.__u6_addr.__u6_addr32, rem_addr);

	return !memcmp(loc_addr, rem_addr, 16) || ipv6_addr_loopback(&raddr6);
}

/**
 * irdma_cm_create_ah - create a cm address handle
 * @cm_node: The connection manager node to create AH for
 * @wait: Provides option to wait for ah creation or not
 */
static int
irdma_cm_create_ah(struct irdma_cm_node *cm_node, bool wait)
{
	struct irdma_ah_info ah_info = {0};
	struct irdma_device *iwdev = cm_node->iwdev;
#ifdef VIMAGE
	struct vnet *vnet = irdma_cmid_to_vnet(cm_node->cm_id);
#endif

	ether_addr_copy(ah_info.mac_addr, if_getlladdr(iwdev->netdev));

	ah_info.hop_ttl = 0x40;
	ah_info.tc_tos = cm_node->tos;
	ah_info.vsi = &iwdev->vsi;

	if (cm_node->ipv4) {
		ah_info.ipv4_valid = true;
		ah_info.dest_ip_addr[0] = cm_node->rem_addr[0];
		ah_info.src_ip_addr[0] = cm_node->loc_addr[0];
		CURVNET_SET_QUIET(vnet);
		ah_info.do_lpbk = irdma_ipv4_is_lpb(ah_info.src_ip_addr[0],
						    ah_info.dest_ip_addr[0]);
		CURVNET_RESTORE();
	} else {
		memcpy(ah_info.dest_ip_addr, cm_node->rem_addr,
		       sizeof(ah_info.dest_ip_addr));
		memcpy(ah_info.src_ip_addr, cm_node->loc_addr,
		       sizeof(ah_info.src_ip_addr));
		ah_info.do_lpbk = irdma_ipv6_is_lpb(ah_info.src_ip_addr,
						    ah_info.dest_ip_addr);
	}

	ah_info.vlan_tag = cm_node->vlan_id;
	if (cm_node->vlan_id < VLAN_N_VID) {
		ah_info.insert_vlan_tag = 1;
		ah_info.vlan_tag |= cm_node->user_pri << VLAN_PRIO_SHIFT;
	}

	ah_info.dst_arpindex =
	    irdma_arp_table(iwdev->rf, ah_info.dest_ip_addr,
			    NULL, IRDMA_ARP_RESOLVE);

	if (irdma_puda_create_ah(&iwdev->rf->sc_dev, &ah_info, wait,
				 IRDMA_PUDA_RSRC_TYPE_ILQ, cm_node,
				 &cm_node->ah))
		return -ENOMEM;

	return 0;
}

/**
 * irdma_cm_free_ah - free a cm address handle
 * @cm_node: The connection manager node to create AH for
 */
static void
irdma_cm_free_ah(struct irdma_cm_node *cm_node)
{
	struct irdma_device *iwdev = cm_node->iwdev;

	irdma_puda_free_ah(&iwdev->rf->sc_dev, cm_node->ah);
	cm_node->ah = NULL;
}

/**
 * irdma_make_cm_node - create a new instance of a cm node
 * @cm_core: cm's core
 * @iwdev: iwarp device structure
 * @cm_info: quad info for connection
 * @listener: passive connection's listener
 */
static struct irdma_cm_node *
irdma_make_cm_node(struct irdma_cm_core *cm_core, struct irdma_device *iwdev,
		   struct irdma_cm_info *cm_info,
		   struct irdma_cm_listener *listener)
{
	struct irdma_cm_node *cm_node;
	int arpindex;
	if_t netdev = iwdev->netdev;

	/* create an hte and cm_node for this instance */
	cm_node = kzalloc(sizeof(*cm_node), GFP_ATOMIC);
	if (!cm_node)
		return NULL;

	/* set our node specific transport info */
	cm_node->ipv4 = cm_info->ipv4;
	cm_node->vlan_id = cm_info->vlan_id;
	if (cm_node->vlan_id >= VLAN_N_VID && iwdev->dcb_vlan_mode)
		cm_node->vlan_id = 0;
	cm_node->tos = cm_info->tos;
	cm_node->user_pri = cm_info->user_pri;
	if (listener) {
		if (listener->tos != cm_info->tos)
			irdma_dev_warn(&iwdev->ibdev,
				       "application TOS[%d] and remote client TOS[%d] mismatch\n",
				       listener->tos, cm_info->tos);
		if (iwdev->vsi.dscp_mode) {
			cm_node->user_pri = listener->user_pri;
		} else {
			cm_node->tos = max(listener->tos, cm_info->tos);
			cm_node->user_pri = rt_tos2priority(cm_node->tos);
			cm_node->user_pri =
			    irdma_iw_get_vlan_prio(cm_info->loc_addr,
						   cm_node->user_pri,
						   cm_info->ipv4);
		}
		irdma_debug(&iwdev->rf->sc_dev, IRDMA_DEBUG_DCB,
			    "listener: TOS:[%d] UP:[%d]\n", cm_node->tos,
			    cm_node->user_pri);
	}
	memcpy(cm_node->loc_addr, cm_info->loc_addr, sizeof(cm_node->loc_addr));
	memcpy(cm_node->rem_addr, cm_info->rem_addr, sizeof(cm_node->rem_addr));
	cm_node->loc_port = cm_info->loc_port;
	cm_node->rem_port = cm_info->rem_port;

	cm_node->mpa_frame_rev = IRDMA_CM_DEFAULT_MPA_VER;
	cm_node->send_rdma0_op = SEND_RDMA_READ_ZERO;
	cm_node->iwdev = iwdev;
	cm_node->dev = &iwdev->rf->sc_dev;

	cm_node->ird_size = cm_node->dev->hw_attrs.max_hw_ird;
	cm_node->ord_size = cm_node->dev->hw_attrs.max_hw_ord;

	cm_node->listener = listener;
	cm_node->cm_id = cm_info->cm_id;
	ether_addr_copy(cm_node->loc_mac, if_getlladdr(netdev));
	spin_lock_init(&cm_node->retrans_list_lock);
	cm_node->ack_rcvd = false;

	init_completion(&cm_node->establish_comp);
	atomic_set(&cm_node->refcnt, 1);
	/* associate our parent CM core */
	cm_node->cm_core = cm_core;
	cm_node->tcp_cntxt.loc_id = IRDMA_CM_DEFAULT_LOCAL_ID;
	cm_node->tcp_cntxt.rcv_wscale = iwdev->rcv_wscale;
	cm_node->tcp_cntxt.rcv_wnd = iwdev->rcv_wnd >> cm_node->tcp_cntxt.rcv_wscale;
	kc_set_loc_seq_num_mss(cm_node);

	arpindex = irdma_resolve_neigh_lpb_chk(iwdev, cm_node, cm_info);
	if (arpindex < 0)
		goto err;

	ether_addr_copy(cm_node->rem_mac, iwdev->rf->arp_table[arpindex].mac_addr);
	irdma_add_hte_node(cm_core, cm_node);
	cm_core->stats_nodes_created++;
	return cm_node;

err:
	kfree(cm_node);

	return NULL;
}

static void
irdma_destroy_connection(struct irdma_cm_node *cm_node)
{
	struct irdma_cm_core *cm_core = cm_node->cm_core;
	struct irdma_qp *iwqp;
	struct irdma_cm_info nfo;

	/* if the node is destroyed before connection was accelerated */
	if (!cm_node->accelerated && cm_node->accept_pend) {
		irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "node destroyed before established\n");
		atomic_dec(&cm_node->listener->pend_accepts_cnt);
	}
	if (cm_node->close_entry)
		irdma_handle_close_entry(cm_node, 0);
	if (cm_node->listener) {
		irdma_dec_refcnt_listen(cm_core, cm_node->listener, 0, true);
	} else {
		if (cm_node->apbvt_set) {
			irdma_del_apbvt(cm_node->iwdev, cm_node->apbvt_entry);
			cm_node->apbvt_set = 0;
		}
		irdma_get_addr_info(cm_node, &nfo);
		if (cm_node->qhash_set) {
			nfo.qh_qpid = cm_node->iwdev->vsi.ilq->qp_id;
			irdma_manage_qhash(cm_node->iwdev, &nfo,
					   IRDMA_QHASH_TYPE_TCP_ESTABLISHED,
					   IRDMA_QHASH_MANAGE_TYPE_DELETE, NULL,
					   false);
			cm_node->qhash_set = 0;
		}
	}

	iwqp = cm_node->iwqp;
	if (iwqp) {
		cm_node->cm_id->rem_ref(cm_node->cm_id);
		cm_node->cm_id = NULL;
		iwqp->cm_id = NULL;
		irdma_qp_rem_ref(&iwqp->ibqp);
		cm_node->iwqp = NULL;
	} else if (cm_node->qhash_set) {
		irdma_get_addr_info(cm_node, &nfo);
		nfo.qh_qpid = cm_node->iwdev->vsi.ilq->qp_id;
		irdma_manage_qhash(cm_node->iwdev, &nfo,
				   IRDMA_QHASH_TYPE_TCP_ESTABLISHED,
				   IRDMA_QHASH_MANAGE_TYPE_DELETE, NULL, false);
		cm_node->qhash_set = 0;
	}

	cm_core->cm_free_ah(cm_node);
}

/**
 * irdma_rem_ref_cm_node - destroy an instance of a cm node
 * @cm_node: connection's node
 */
void
irdma_rem_ref_cm_node(struct irdma_cm_node *cm_node)
{
	struct irdma_cm_core *cm_core = cm_node->cm_core;
	unsigned long flags;

	spin_lock_irqsave(&cm_core->ht_lock, flags);

	if (!atomic_dec_and_test(&cm_node->refcnt)) {
		spin_unlock_irqrestore(&cm_core->ht_lock, flags);
		return;
	}
	if (cm_node->iwqp) {
		cm_node->iwqp->cm_node = NULL;
		cm_node->iwqp->cm_id = NULL;
	}
	HASH_DEL_RCU(cm_core->cm_hash_tbl, &cm_node->list);
	cm_node->cm_core->stats_nodes_destroyed++;

	spin_unlock_irqrestore(&cm_core->ht_lock, flags);

	irdma_destroy_connection(cm_node);

	kfree_rcu(cm_node, rcu_head);
}

/**
 * irdma_handle_fin_pkt - FIN packet received
 * @cm_node: connection's node
 */
static void
irdma_handle_fin_pkt(struct irdma_cm_node *cm_node)
{
	switch (cm_node->state) {
	case IRDMA_CM_STATE_SYN_RCVD:
	case IRDMA_CM_STATE_SYN_SENT:
	case IRDMA_CM_STATE_ESTABLISHED:
	case IRDMA_CM_STATE_MPAREJ_RCVD:
		cm_node->tcp_cntxt.rcv_nxt++;
		irdma_cleanup_retrans_entry(cm_node);
		cm_node->state = IRDMA_CM_STATE_LAST_ACK;
		irdma_send_fin(cm_node);
		break;
	case IRDMA_CM_STATE_MPAREQ_SENT:
		irdma_create_event(cm_node, IRDMA_CM_EVENT_ABORTED);
		cm_node->tcp_cntxt.rcv_nxt++;
		irdma_cleanup_retrans_entry(cm_node);
		cm_node->state = IRDMA_CM_STATE_CLOSED;
		atomic_inc(&cm_node->refcnt);
		irdma_send_reset(cm_node);
		break;
	case IRDMA_CM_STATE_FIN_WAIT1:
		cm_node->tcp_cntxt.rcv_nxt++;
		irdma_cleanup_retrans_entry(cm_node);
		cm_node->state = IRDMA_CM_STATE_CLOSING;
		irdma_send_ack(cm_node);
		/*
		 * Wait for ACK as this is simultaneous close. After we receive ACK, do not send anything. Just rm the
		 * node.
		 */
		break;
	case IRDMA_CM_STATE_FIN_WAIT2:
		cm_node->tcp_cntxt.rcv_nxt++;
		irdma_cleanup_retrans_entry(cm_node);
		cm_node->state = IRDMA_CM_STATE_TIME_WAIT;
		irdma_send_ack(cm_node);
		irdma_schedule_cm_timer(cm_node, NULL, IRDMA_TIMER_TYPE_CLOSE,
					1, 0);
		break;
	case IRDMA_CM_STATE_TIME_WAIT:
		cm_node->tcp_cntxt.rcv_nxt++;
		irdma_cleanup_retrans_entry(cm_node);
		cm_node->state = IRDMA_CM_STATE_CLOSED;
		irdma_rem_ref_cm_node(cm_node);
		break;
	case IRDMA_CM_STATE_OFFLOADED:
	default:
		irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "bad state node state = %d\n", cm_node->state);
		break;
	}
}

/**
 * irdma_handle_rst_pkt - process received RST packet
 * @cm_node: connection's node
 * @rbuf: receive buffer
 */
static void
irdma_handle_rst_pkt(struct irdma_cm_node *cm_node,
		     struct irdma_puda_buf *rbuf)
{
	irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
		    "caller: %pS cm_node=%p state=%d rem_port=0x%04x loc_port=0x%04x rem_addr=%x loc_addr=%x\n",
		    __builtin_return_address(0), cm_node, cm_node->state,
		    cm_node->rem_port, cm_node->loc_port, cm_node->rem_addr[0],
		    cm_node->loc_addr[0]);

	irdma_cleanup_retrans_entry(cm_node);
	switch (cm_node->state) {
	case IRDMA_CM_STATE_SYN_SENT:
	case IRDMA_CM_STATE_MPAREQ_SENT:
		switch (cm_node->mpa_frame_rev) {
		case IETF_MPA_V2:
			/* Drop down to MPA_V1 */
			cm_node->mpa_frame_rev = IETF_MPA_V1;
			/* send a syn and goto syn sent state */
			cm_node->state = IRDMA_CM_STATE_SYN_SENT;
			if (irdma_send_syn(cm_node, 0))
				irdma_active_open_err(cm_node, false);
			break;
		case IETF_MPA_V1:
		default:
			irdma_active_open_err(cm_node, false);
			break;
		}
		break;
	case IRDMA_CM_STATE_MPAREQ_RCVD:
		atomic_inc(&cm_node->passive_state);
		break;
	case IRDMA_CM_STATE_ESTABLISHED:
	case IRDMA_CM_STATE_SYN_RCVD:
	case IRDMA_CM_STATE_LISTENING:
		irdma_passive_open_err(cm_node, false);
		break;
	case IRDMA_CM_STATE_OFFLOADED:
		irdma_active_open_err(cm_node, false);
		break;
	case IRDMA_CM_STATE_CLOSED:
		break;
	case IRDMA_CM_STATE_FIN_WAIT2:
	case IRDMA_CM_STATE_FIN_WAIT1:
	case IRDMA_CM_STATE_LAST_ACK:
	case IRDMA_CM_STATE_TIME_WAIT:
		cm_node->state = IRDMA_CM_STATE_CLOSED;
		irdma_rem_ref_cm_node(cm_node);
		break;
	default:
		break;
	}
}

/**
 * irdma_handle_rcv_mpa - Process a recv'd mpa buffer
 * @cm_node: connection's node
 * @rbuf: receive buffer
 */
static void
irdma_handle_rcv_mpa(struct irdma_cm_node *cm_node,
		     struct irdma_puda_buf *rbuf)
{
	int err;
	int datasize = rbuf->datalen;
	u8 *dataloc = rbuf->data;

	enum irdma_cm_event_type type = IRDMA_CM_EVENT_UNKNOWN;
	u32 res_type;

	err = irdma_parse_mpa(cm_node, dataloc, &res_type, datasize);
	if (err) {
		if (cm_node->state == IRDMA_CM_STATE_MPAREQ_SENT)
			irdma_active_open_err(cm_node, true);
		else
			irdma_passive_open_err(cm_node, true);
		return;
	}

	switch (cm_node->state) {
	case IRDMA_CM_STATE_ESTABLISHED:
		if (res_type == IRDMA_MPA_REQUEST_REJECT)
			irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
				    "state for reject\n");
		cm_node->state = IRDMA_CM_STATE_MPAREQ_RCVD;
		type = IRDMA_CM_EVENT_MPA_REQ;
		irdma_send_ack(cm_node);	/* ACK received MPA request */
		atomic_set(&cm_node->passive_state,
			   IRDMA_PASSIVE_STATE_INDICATED);
		break;
	case IRDMA_CM_STATE_MPAREQ_SENT:
		irdma_cleanup_retrans_entry(cm_node);
		if (res_type == IRDMA_MPA_REQUEST_REJECT) {
			type = IRDMA_CM_EVENT_MPA_REJECT;
			cm_node->state = IRDMA_CM_STATE_MPAREJ_RCVD;
		} else {
			type = IRDMA_CM_EVENT_CONNECTED;
			cm_node->state = IRDMA_CM_STATE_OFFLOADED;
		}
		irdma_send_ack(cm_node);
		break;
	default:
		irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "wrong cm_node state=%d\n", cm_node->state);
		break;
	}
	irdma_create_event(cm_node, type);
}

/**
 * irdma_check_syn - Check for error on received syn ack
 * @cm_node: connection's node
 * @tcph: pointer tcp header
 */
static int
irdma_check_syn(struct irdma_cm_node *cm_node, struct tcphdr *tcph)
{
	if (ntohl(tcph->th_ack) != cm_node->tcp_cntxt.loc_seq_num) {
		irdma_active_open_err(cm_node, true);
		return 1;
	}

	return 0;
}

/**
 * irdma_check_seq - check seq numbers if OK
 * @cm_node: connection's node
 * @tcph: pointer tcp header
 */
static int
irdma_check_seq(struct irdma_cm_node *cm_node, struct tcphdr *tcph)
{
	u32 seq;
	u32 ack_seq;
	u32 loc_seq_num = cm_node->tcp_cntxt.loc_seq_num;
	u32 rcv_nxt = cm_node->tcp_cntxt.rcv_nxt;
	u32 rcv_wnd;
	int err = 0;

	seq = ntohl(tcph->th_seq);
	ack_seq = ntohl(tcph->th_ack);
	rcv_wnd = cm_node->tcp_cntxt.rcv_wnd;
	if (ack_seq != loc_seq_num ||
	    !between(seq, rcv_nxt, (rcv_nxt + rcv_wnd)))
		err = -1;
	if (err)
		irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "seq number err\n");

	return err;
}

void
irdma_add_conn_est_qh(struct irdma_cm_node *cm_node)
{
	struct irdma_cm_info nfo;

	irdma_get_addr_info(cm_node, &nfo);
	nfo.qh_qpid = cm_node->iwdev->vsi.ilq->qp_id;
	irdma_manage_qhash(cm_node->iwdev, &nfo,
			   IRDMA_QHASH_TYPE_TCP_ESTABLISHED,
			   IRDMA_QHASH_MANAGE_TYPE_ADD,
			   cm_node, false);
	cm_node->qhash_set = true;
}

/**
 * irdma_handle_syn_pkt - is for Passive node
 * @cm_node: connection's node
 * @rbuf: receive buffer
 */
static void
irdma_handle_syn_pkt(struct irdma_cm_node *cm_node,
		     struct irdma_puda_buf *rbuf)
{
	struct tcphdr *tcph = (struct tcphdr *)rbuf->tcph;
	int err;
	u32 inc_sequence;
	int optionsize;

	optionsize = (tcph->th_off << 2) - sizeof(*tcph);
	inc_sequence = ntohl(tcph->th_seq);

	switch (cm_node->state) {
	case IRDMA_CM_STATE_SYN_SENT:
	case IRDMA_CM_STATE_MPAREQ_SENT:
		/* Rcvd syn on active open connection */
		irdma_active_open_err(cm_node, 1);
		break;
	case IRDMA_CM_STATE_LISTENING:
		/* Passive OPEN */
		if (atomic_read(&cm_node->listener->pend_accepts_cnt) >
		    cm_node->listener->backlog) {
			cm_node->cm_core->stats_backlog_drops++;
			irdma_passive_open_err(cm_node, false);
			break;
		}
		err = irdma_handle_tcp_options(cm_node, tcph, optionsize, 1);
		if (err) {
			irdma_passive_open_err(cm_node, false);
			/* drop pkt */
			break;
		}
		err = cm_node->cm_core->cm_create_ah(cm_node, false);
		if (err) {
			irdma_passive_open_err(cm_node, false);
			/* drop pkt */
			break;
		}
		cm_node->tcp_cntxt.rcv_nxt = inc_sequence + 1;
		cm_node->accept_pend = 1;
		atomic_inc(&cm_node->listener->pend_accepts_cnt);

		cm_node->state = IRDMA_CM_STATE_SYN_RCVD;
		break;
	case IRDMA_CM_STATE_CLOSED:
		irdma_cleanup_retrans_entry(cm_node);
		atomic_inc(&cm_node->refcnt);
		irdma_send_reset(cm_node);
		break;
	case IRDMA_CM_STATE_OFFLOADED:
	case IRDMA_CM_STATE_ESTABLISHED:
	case IRDMA_CM_STATE_FIN_WAIT1:
	case IRDMA_CM_STATE_FIN_WAIT2:
	case IRDMA_CM_STATE_MPAREQ_RCVD:
	case IRDMA_CM_STATE_LAST_ACK:
	case IRDMA_CM_STATE_CLOSING:
	case IRDMA_CM_STATE_UNKNOWN:
	default:
		break;
	}
}

/**
 * irdma_handle_synack_pkt - Process SYN+ACK packet (active side)
 * @cm_node: connection's node
 * @rbuf: receive buffer
 */
static void
irdma_handle_synack_pkt(struct irdma_cm_node *cm_node,
			struct irdma_puda_buf *rbuf)
{
	struct tcphdr *tcph = (struct tcphdr *)rbuf->tcph;
	int err;
	u32 inc_sequence;
	int optionsize;

	optionsize = (tcph->th_off << 2) - sizeof(*tcph);
	inc_sequence = ntohl(tcph->th_seq);
	switch (cm_node->state) {
	case IRDMA_CM_STATE_SYN_SENT:
		irdma_cleanup_retrans_entry(cm_node);
		/* active open */
		if (irdma_check_syn(cm_node, tcph)) {
			irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
				    "check syn fail\n");
			return;
		}
		cm_node->tcp_cntxt.rem_ack_num = ntohl(tcph->th_ack);
		/* setup options */
		err = irdma_handle_tcp_options(cm_node, tcph, optionsize, 0);
		if (err) {
			irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
				    "cm_node=%p tcp_options failed\n", cm_node);
			break;
		}
		irdma_cleanup_retrans_entry(cm_node);
		cm_node->tcp_cntxt.rcv_nxt = inc_sequence + 1;
		irdma_send_ack(cm_node);	/* ACK  for the syn_ack */
		err = irdma_send_mpa_request(cm_node);
		if (err) {
			irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
				    "cm_node=%p irdma_send_mpa_request failed\n",
				    cm_node);
			break;
		}
		cm_node->state = IRDMA_CM_STATE_MPAREQ_SENT;
		break;
	case IRDMA_CM_STATE_MPAREQ_RCVD:
		irdma_passive_open_err(cm_node, true);
		break;
	case IRDMA_CM_STATE_LISTENING:
		cm_node->tcp_cntxt.loc_seq_num = ntohl(tcph->th_ack);
		irdma_cleanup_retrans_entry(cm_node);
		cm_node->state = IRDMA_CM_STATE_CLOSED;
		irdma_send_reset(cm_node);
		break;
	case IRDMA_CM_STATE_CLOSED:
		cm_node->tcp_cntxt.loc_seq_num = ntohl(tcph->th_ack);
		irdma_cleanup_retrans_entry(cm_node);
		atomic_inc(&cm_node->refcnt);
		irdma_send_reset(cm_node);
		break;
	case IRDMA_CM_STATE_ESTABLISHED:
	case IRDMA_CM_STATE_FIN_WAIT1:
	case IRDMA_CM_STATE_FIN_WAIT2:
	case IRDMA_CM_STATE_LAST_ACK:
	case IRDMA_CM_STATE_OFFLOADED:
	case IRDMA_CM_STATE_CLOSING:
	case IRDMA_CM_STATE_UNKNOWN:
	case IRDMA_CM_STATE_MPAREQ_SENT:
	default:
		break;
	}
}

/**
 * irdma_handle_ack_pkt - process packet with ACK
 * @cm_node: connection's node
 * @rbuf: receive buffer
 */
static int
irdma_handle_ack_pkt(struct irdma_cm_node *cm_node,
		     struct irdma_puda_buf *rbuf)
{
	struct tcphdr *tcph = (struct tcphdr *)rbuf->tcph;
	u32 inc_sequence;
	int ret;
	int optionsize;
	u32 datasize = rbuf->datalen;

	optionsize = (tcph->th_off << 2) - sizeof(*tcph);

	if (irdma_check_seq(cm_node, tcph))
		return -EINVAL;

	inc_sequence = ntohl(tcph->th_seq);
	switch (cm_node->state) {
	case IRDMA_CM_STATE_SYN_RCVD:
		irdma_cleanup_retrans_entry(cm_node);
		ret = irdma_handle_tcp_options(cm_node, tcph, optionsize, 1);
		if (ret)
			return ret;
		cm_node->tcp_cntxt.rem_ack_num = ntohl(tcph->th_ack);
		cm_node->state = IRDMA_CM_STATE_ESTABLISHED;
		if (datasize) {
			cm_node->tcp_cntxt.rcv_nxt = inc_sequence + datasize;
			irdma_handle_rcv_mpa(cm_node, rbuf);
		}
		break;
	case IRDMA_CM_STATE_ESTABLISHED:
		irdma_cleanup_retrans_entry(cm_node);
		if (datasize) {
			cm_node->tcp_cntxt.rcv_nxt = inc_sequence + datasize;
			irdma_handle_rcv_mpa(cm_node, rbuf);
		}
		break;
	case IRDMA_CM_STATE_MPAREQ_SENT:
		cm_node->tcp_cntxt.rem_ack_num = ntohl(tcph->th_ack);
		if (datasize) {
			cm_node->tcp_cntxt.rcv_nxt = inc_sequence + datasize;
			cm_node->ack_rcvd = false;
			irdma_handle_rcv_mpa(cm_node, rbuf);
		} else {
			cm_node->ack_rcvd = true;
		}
		break;
	case IRDMA_CM_STATE_LISTENING:
		irdma_cleanup_retrans_entry(cm_node);
		cm_node->state = IRDMA_CM_STATE_CLOSED;
		irdma_send_reset(cm_node);
		break;
	case IRDMA_CM_STATE_CLOSED:
		irdma_cleanup_retrans_entry(cm_node);
		atomic_inc(&cm_node->refcnt);
		irdma_send_reset(cm_node);
		break;
	case IRDMA_CM_STATE_LAST_ACK:
	case IRDMA_CM_STATE_CLOSING:
		irdma_cleanup_retrans_entry(cm_node);
		cm_node->state = IRDMA_CM_STATE_CLOSED;
		irdma_rem_ref_cm_node(cm_node);
		break;
	case IRDMA_CM_STATE_FIN_WAIT1:
		irdma_cleanup_retrans_entry(cm_node);
		cm_node->state = IRDMA_CM_STATE_FIN_WAIT2;
		break;
	case IRDMA_CM_STATE_SYN_SENT:
	case IRDMA_CM_STATE_FIN_WAIT2:
	case IRDMA_CM_STATE_OFFLOADED:
	case IRDMA_CM_STATE_MPAREQ_RCVD:
	case IRDMA_CM_STATE_UNKNOWN:
	default:
		irdma_cleanup_retrans_entry(cm_node);
		break;
	}

	return 0;
}

/**
 * irdma_process_pkt - process cm packet
 * @cm_node: connection's node
 * @rbuf: receive buffer
 */
static void
irdma_process_pkt(struct irdma_cm_node *cm_node,
		  struct irdma_puda_buf *rbuf)
{
	enum irdma_tcpip_pkt_type pkt_type = IRDMA_PKT_TYPE_UNKNOWN;
	struct tcphdr *tcph = (struct tcphdr *)rbuf->tcph;
	u32 fin_set = 0;
	int err;

	if (tcph->th_flags & TH_RST) {
		pkt_type = IRDMA_PKT_TYPE_RST;
	} else if (tcph->th_flags & TH_SYN) {
		pkt_type = IRDMA_PKT_TYPE_SYN;
		if (tcph->th_flags & TH_ACK)
			pkt_type = IRDMA_PKT_TYPE_SYNACK;
	} else if (tcph->th_flags & TH_ACK) {
		pkt_type = IRDMA_PKT_TYPE_ACK;
	}
	if (tcph->th_flags & TH_FIN)
		fin_set = 1;

	switch (pkt_type) {
	case IRDMA_PKT_TYPE_SYN:
		irdma_handle_syn_pkt(cm_node, rbuf);
		break;
	case IRDMA_PKT_TYPE_SYNACK:
		irdma_handle_synack_pkt(cm_node, rbuf);
		break;
	case IRDMA_PKT_TYPE_ACK:
		err = irdma_handle_ack_pkt(cm_node, rbuf);
		if (fin_set && !err)
			irdma_handle_fin_pkt(cm_node);
		break;
	case IRDMA_PKT_TYPE_RST:
		irdma_handle_rst_pkt(cm_node, rbuf);
		break;
	default:
		if (fin_set &&
		    (!irdma_check_seq(cm_node, (struct tcphdr *)rbuf->tcph)))
			irdma_handle_fin_pkt(cm_node);
		break;
	}
}

/**
 * irdma_make_listen_node - create a listen node with params
 * @cm_core: cm's core
 * @iwdev: iwarp device structure
 * @cm_info: quad info for connection
 */
static struct irdma_cm_listener *
irdma_make_listen_node(struct irdma_cm_core *cm_core,
		       struct irdma_device *iwdev,
		       struct irdma_cm_info *cm_info)
{
	struct irdma_cm_listener *listener;
	unsigned long flags;

	/* cannot have multiple matching listeners */
	listener = irdma_find_listener(cm_core, cm_info->loc_addr, cm_info->ipv4,
				       cm_info->loc_port, cm_info->vlan_id,
				       IRDMA_CM_LISTENER_EITHER_STATE);
	if (listener &&
	    listener->listener_state == IRDMA_CM_LISTENER_ACTIVE_STATE) {
		atomic_dec(&listener->refcnt);
		return NULL;
	}

	if (!listener) {
		/*
		 * create a CM listen node 1/2 node to compare incoming traffic to
		 */
		listener = kzalloc(sizeof(*listener), GFP_KERNEL);
		if (!listener)
			return NULL;
		cm_core->stats_listen_nodes_created++;
		memcpy(listener->loc_addr, cm_info->loc_addr,
		       sizeof(listener->loc_addr));
		listener->loc_port = cm_info->loc_port;

		INIT_LIST_HEAD(&listener->child_listen_list);

		atomic_set(&listener->refcnt, 1);
	} else {
		listener->reused_node = 1;
	}

	listener->cm_id = cm_info->cm_id;
	listener->ipv4 = cm_info->ipv4;
	listener->vlan_id = cm_info->vlan_id;
	atomic_set(&listener->pend_accepts_cnt, 0);
	listener->cm_core = cm_core;
	listener->iwdev = iwdev;

	listener->backlog = cm_info->backlog;
	listener->listener_state = IRDMA_CM_LISTENER_ACTIVE_STATE;

	if (!listener->reused_node) {
		spin_lock_irqsave(&cm_core->listen_list_lock, flags);
		list_add(&listener->list, &cm_core->listen_list);
		spin_unlock_irqrestore(&cm_core->listen_list_lock, flags);
	}

	return listener;
}

/**
 * irdma_create_cm_node - make a connection node with params
 * @cm_core: cm's core
 * @iwdev: iwarp device structure
 * @conn_param: connection parameters
 * @cm_info: quad info for connection
 * @caller_cm_node: pointer to cm_node structure to return
 */
static int
irdma_create_cm_node(struct irdma_cm_core *cm_core,
		     struct irdma_device *iwdev,
		     struct iw_cm_conn_param *conn_param,
		     struct irdma_cm_info *cm_info,
		     struct irdma_cm_node **caller_cm_node)
{
	struct irdma_cm_node *cm_node;
	u16 private_data_len = conn_param->private_data_len;
	const void *private_data = conn_param->private_data;

	/* create a CM connection node */
	cm_node = irdma_make_cm_node(cm_core, iwdev, cm_info, NULL);
	if (!cm_node)
		return -ENOMEM;

	/* set our node side to client (active) side */
	cm_node->tcp_cntxt.client = 1;
	cm_node->tcp_cntxt.rcv_wscale = IRDMA_CM_DEFAULT_RCV_WND_SCALE;

	irdma_record_ird_ord(cm_node, conn_param->ird, conn_param->ord);

	cm_node->pdata.size = private_data_len;
	cm_node->pdata.addr = cm_node->pdata_buf;

	memcpy(cm_node->pdata_buf, private_data, private_data_len);
	*caller_cm_node = cm_node;

	return 0;
}

/**
 * irdma_cm_reject - reject and teardown a connection
 * @cm_node: connection's node
 * @pdata: ptr to private data for reject
 * @plen: size of private data
 */
static int
irdma_cm_reject(struct irdma_cm_node *cm_node, const void *pdata,
		u8 plen)
{
	int ret;
	int passive_state;

	if (cm_node->tcp_cntxt.client)
		return 0;

	irdma_cleanup_retrans_entry(cm_node);

	passive_state = atomic_add_return(1, &cm_node->passive_state);
	if (passive_state == IRDMA_SEND_RESET_EVENT) {
		cm_node->state = IRDMA_CM_STATE_CLOSED;
		irdma_rem_ref_cm_node(cm_node);
		return 0;
	}

	if (cm_node->state == IRDMA_CM_STATE_LISTENER_DESTROYED) {
		irdma_rem_ref_cm_node(cm_node);
		return 0;
	}

	ret = irdma_send_mpa_reject(cm_node, pdata, plen);
	if (!ret)
		return 0;

	cm_node->state = IRDMA_CM_STATE_CLOSED;
	if (irdma_send_reset(cm_node))
		irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "send reset failed\n");

	return ret;
}

/**
 * irdma_cm_close - close of cm connection
 * @cm_node: connection's node
 */
static int
irdma_cm_close(struct irdma_cm_node *cm_node)
{
	switch (cm_node->state) {
	case IRDMA_CM_STATE_SYN_RCVD:
	case IRDMA_CM_STATE_SYN_SENT:
	case IRDMA_CM_STATE_ONE_SIDE_ESTABLISHED:
	case IRDMA_CM_STATE_ESTABLISHED:
	case IRDMA_CM_STATE_ACCEPTING:
	case IRDMA_CM_STATE_MPAREQ_SENT:
	case IRDMA_CM_STATE_MPAREQ_RCVD:
		irdma_cleanup_retrans_entry(cm_node);
		irdma_send_reset(cm_node);
		break;
	case IRDMA_CM_STATE_CLOSE_WAIT:
		cm_node->state = IRDMA_CM_STATE_LAST_ACK;
		irdma_send_fin(cm_node);
		break;
	case IRDMA_CM_STATE_FIN_WAIT1:
	case IRDMA_CM_STATE_FIN_WAIT2:
	case IRDMA_CM_STATE_LAST_ACK:
	case IRDMA_CM_STATE_TIME_WAIT:
	case IRDMA_CM_STATE_CLOSING:
		return -EINVAL;
	case IRDMA_CM_STATE_LISTENING:
		irdma_cleanup_retrans_entry(cm_node);
		irdma_send_reset(cm_node);
		break;
	case IRDMA_CM_STATE_MPAREJ_RCVD:
	case IRDMA_CM_STATE_UNKNOWN:
	case IRDMA_CM_STATE_INITED:
	case IRDMA_CM_STATE_CLOSED:
	case IRDMA_CM_STATE_LISTENER_DESTROYED:
		irdma_rem_ref_cm_node(cm_node);
		break;
	case IRDMA_CM_STATE_OFFLOADED:
		if (cm_node->send_entry)
			irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
				    "CM send_entry in OFFLOADED state\n");
		irdma_rem_ref_cm_node(cm_node);
		break;
	}

	return 0;
}

/**
 * irdma_receive_ilq - recv an ETHERNET packet, and process it
 * through CM
 * @vsi: VSI structure of dev
 * @rbuf: receive buffer
 */
void
irdma_receive_ilq(struct irdma_sc_vsi *vsi, struct irdma_puda_buf *rbuf)
{
	struct irdma_cm_node *cm_node;
	struct irdma_cm_listener *listener;
	struct ip *iph;
	struct ip6_hdr *ip6h;
	struct tcphdr *tcph;
	struct irdma_cm_info cm_info = {0};
	struct irdma_device *iwdev = vsi->back_vsi;
	struct irdma_cm_core *cm_core = &iwdev->cm_core;
	struct ether_vlan_header *ethh;
	u16 vtag;

	/* if vlan, then maclen = 18 else 14 */
	iph = (struct ip *)rbuf->iph;
	irdma_debug_buf(vsi->dev, IRDMA_DEBUG_ILQ, "RECEIVE ILQ BUFFER",
			rbuf->mem.va, rbuf->totallen);
	if (iwdev->rf->sc_dev.hw_attrs.uk_attrs.hw_rev >= IRDMA_GEN_2) {
		if (rbuf->vlan_valid) {
			vtag = rbuf->vlan_id;
			cm_info.user_pri = (vtag & EVL_PRI_MASK) >>
			    VLAN_PRIO_SHIFT;
			cm_info.vlan_id = vtag & EVL_VLID_MASK;
		} else {
			cm_info.vlan_id = 0xFFFF;
		}
	} else {
		ethh = rbuf->mem.va;

		if (ethh->evl_proto == htons(ETH_P_8021Q)) {
			vtag = ntohs(ethh->evl_tag);
			cm_info.user_pri = (vtag & EVL_PRI_MASK) >>
			    VLAN_PRIO_SHIFT;
			cm_info.vlan_id = vtag & EVL_VLID_MASK;
			irdma_debug(&cm_core->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
				    "vlan_id=%d\n", cm_info.vlan_id);
		} else {
			cm_info.vlan_id = 0xFFFF;
		}
	}
	tcph = (struct tcphdr *)rbuf->tcph;

	if (rbuf->ipv4) {
		cm_info.loc_addr[0] = ntohl(iph->ip_dst.s_addr);
		cm_info.rem_addr[0] = ntohl(iph->ip_src.s_addr);
		cm_info.ipv4 = true;
		cm_info.tos = iph->ip_tos;
	} else {
		ip6h = (struct ip6_hdr *)rbuf->iph;
		irdma_copy_ip_ntohl(cm_info.loc_addr,
				    ip6h->ip6_dst.__u6_addr.__u6_addr32);
		irdma_copy_ip_ntohl(cm_info.rem_addr,
				    ip6h->ip6_src.__u6_addr.__u6_addr32);
		cm_info.ipv4 = false;
		cm_info.tos = (ip6h->ip6_vfc << 4) | ip6h->ip6_flow;
	}
	cm_info.loc_port = ntohs(tcph->th_dport);
	cm_info.rem_port = ntohs(tcph->th_sport);
	cm_node = irdma_find_node(cm_core, cm_info.rem_port, cm_info.rem_addr,
				  cm_info.loc_port, cm_info.loc_addr, cm_info.vlan_id);

	if (!cm_node) {
		/*
		 * Only type of packet accepted are for the PASSIVE open (syn only)
		 */
		if (!(tcph->th_flags & TH_SYN) || tcph->th_flags & TH_ACK)
			return;

		listener = irdma_find_listener(cm_core,
					       cm_info.loc_addr,
					       cm_info.ipv4,
					       cm_info.loc_port,
					       cm_info.vlan_id,
					       IRDMA_CM_LISTENER_ACTIVE_STATE);
		if (!listener) {
			cm_info.cm_id = NULL;
			irdma_debug(&cm_core->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
				    "no listener found\n");
			return;
		}

		cm_info.cm_id = listener->cm_id;
		cm_node = irdma_make_cm_node(cm_core, iwdev, &cm_info,
					     listener);
		if (!cm_node) {
			irdma_debug(&cm_core->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
				    "allocate node failed\n");
			atomic_dec(&listener->refcnt);
			return;
		}

		if (!(tcph->th_flags & (TH_RST | TH_FIN))) {
			cm_node->state = IRDMA_CM_STATE_LISTENING;
		} else {
			irdma_rem_ref_cm_node(cm_node);
			return;
		}

		atomic_inc(&cm_node->refcnt);
	} else if (cm_node->state == IRDMA_CM_STATE_OFFLOADED) {
		irdma_rem_ref_cm_node(cm_node);
		return;
	}

	irdma_process_pkt(cm_node, rbuf);
	irdma_rem_ref_cm_node(cm_node);
}

static int
irdma_add_qh(struct irdma_cm_node *cm_node, bool active)
{
	if (!active)
		irdma_add_conn_est_qh(cm_node);
	return 0;
}

static void
irdma_cm_free_ah_nop(struct irdma_cm_node *cm_node)
{
}

/**
 * irdma_setup_cm_core - setup top level instance of a cm core
 * @iwdev: iwarp device structure
 * @rdma_ver: HW version
 */
int
irdma_setup_cm_core(struct irdma_device *iwdev, u8 rdma_ver)
{
	struct irdma_cm_core *cm_core = &iwdev->cm_core;

	cm_core->iwdev = iwdev;
	cm_core->dev = &iwdev->rf->sc_dev;

	/* Handles CM event work items send to Iwarp core */
	cm_core->event_wq = alloc_ordered_workqueue("iwarp-event-wq", 0);
	if (!cm_core->event_wq)
		return -ENOMEM;

	INIT_LIST_HEAD(&cm_core->listen_list);

	timer_setup(&cm_core->tcp_timer, irdma_cm_timer_tick, 0);

	spin_lock_init(&cm_core->ht_lock);
	spin_lock_init(&cm_core->listen_list_lock);
	spin_lock_init(&cm_core->apbvt_lock);
	switch (rdma_ver) {
	case IRDMA_GEN_1:
		cm_core->form_cm_frame = irdma_form_uda_cm_frame;
		cm_core->cm_create_ah = irdma_add_qh;
		cm_core->cm_free_ah = irdma_cm_free_ah_nop;
		break;
	case IRDMA_GEN_2:
	default:
		cm_core->form_cm_frame = irdma_form_ah_cm_frame;
		cm_core->cm_create_ah = irdma_cm_create_ah;
		cm_core->cm_free_ah = irdma_cm_free_ah;
	}

	return 0;
}

/**
 * irdma_cleanup_cm_core - deallocate a top level instance of a
 * cm core
 * @cm_core: cm's core
 */
void
irdma_cleanup_cm_core(struct irdma_cm_core *cm_core)
{
	if (!cm_core)
		return;

	del_timer_sync(&cm_core->tcp_timer);

	destroy_workqueue(cm_core->event_wq);
	cm_core->dev->ws_reset(&cm_core->iwdev->vsi);
}

/**
 * irdma_init_tcp_ctx - setup qp context
 * @cm_node: connection's node
 * @tcp_info: offload info for tcp
 * @iwqp: associate qp for the connection
 */
static void
irdma_init_tcp_ctx(struct irdma_cm_node *cm_node,
		   struct irdma_tcp_offload_info *tcp_info,
		   struct irdma_qp *iwqp)
{
	tcp_info->ipv4 = cm_node->ipv4;
	tcp_info->drop_ooo_seg = !iwqp->iwdev->iw_ooo;
	tcp_info->wscale = true;
	tcp_info->ignore_tcp_opt = true;
	tcp_info->ignore_tcp_uns_opt = true;
	tcp_info->no_nagle = false;

	tcp_info->ttl = IRDMA_DEFAULT_TTL;
	tcp_info->rtt_var = IRDMA_DEFAULT_RTT_VAR;
	tcp_info->ss_thresh = IRDMA_DEFAULT_SS_THRESH;
	tcp_info->rexmit_thresh = IRDMA_DEFAULT_REXMIT_THRESH;

	tcp_info->tcp_state = IRDMA_TCP_STATE_ESTABLISHED;
	tcp_info->snd_wscale = cm_node->tcp_cntxt.snd_wscale;
	tcp_info->rcv_wscale = cm_node->tcp_cntxt.rcv_wscale;

	tcp_info->snd_nxt = cm_node->tcp_cntxt.loc_seq_num;
	tcp_info->snd_wnd = cm_node->tcp_cntxt.snd_wnd;
	tcp_info->rcv_nxt = cm_node->tcp_cntxt.rcv_nxt;
	tcp_info->snd_max = cm_node->tcp_cntxt.loc_seq_num;

	tcp_info->snd_una = cm_node->tcp_cntxt.loc_seq_num;
	tcp_info->cwnd = 2 * cm_node->tcp_cntxt.mss;
	tcp_info->snd_wl1 = cm_node->tcp_cntxt.rcv_nxt;
	tcp_info->snd_wl2 = cm_node->tcp_cntxt.loc_seq_num;
	tcp_info->max_snd_window = cm_node->tcp_cntxt.max_snd_wnd;
	tcp_info->rcv_wnd = cm_node->tcp_cntxt.rcv_wnd
	    << cm_node->tcp_cntxt.rcv_wscale;

	tcp_info->flow_label = 0;
	tcp_info->snd_mss = (u32)cm_node->tcp_cntxt.mss;
	tcp_info->tos = cm_node->tos;
	if (cm_node->vlan_id < VLAN_N_VID) {
		tcp_info->insert_vlan_tag = true;
		tcp_info->vlan_tag = cm_node->vlan_id;
		tcp_info->vlan_tag |= cm_node->user_pri << VLAN_PRIO_SHIFT;
	}
	tcp_info->src_port = cm_node->loc_port;
	tcp_info->dst_port = cm_node->rem_port;
	tcp_info->arp_idx = (u16)irdma_arp_table(iwqp->iwdev->rf,
						  cm_node->rem_addr, NULL,
						  IRDMA_ARP_RESOLVE);
	if (cm_node->ipv4) {
		tcp_info->dest_ip_addr[3] = cm_node->rem_addr[0];
		tcp_info->local_ipaddr[3] = cm_node->loc_addr[0];
	} else {
		memcpy(tcp_info->dest_ip_addr, cm_node->rem_addr,
		       sizeof(tcp_info->dest_ip_addr));
		memcpy(tcp_info->local_ipaddr, cm_node->loc_addr,
		       sizeof(tcp_info->local_ipaddr));
	}
}

/**
 * irdma_cm_init_tsa_conn - setup qp for RTS
 * @iwqp: associate qp for the connection
 * @cm_node: connection's node
 */
static void
irdma_cm_init_tsa_conn(struct irdma_qp *iwqp,
		       struct irdma_cm_node *cm_node)
{
	struct irdma_iwarp_offload_info *iwarp_info;
	struct irdma_qp_host_ctx_info *ctx_info;

	iwarp_info = &iwqp->iwarp_info;
	ctx_info = &iwqp->ctx_info;

	ctx_info->tcp_info = &iwqp->tcp_info;
	ctx_info->send_cq_num = iwqp->iwscq->sc_cq.cq_uk.cq_id;
	ctx_info->rcv_cq_num = iwqp->iwrcq->sc_cq.cq_uk.cq_id;

	iwarp_info->ord_size = cm_node->ord_size;
	iwarp_info->ird_size = cm_node->ird_size;
	iwarp_info->rd_en = true;
	iwarp_info->rdmap_ver = 1;
	iwarp_info->ddp_ver = 1;
	iwarp_info->pd_id = iwqp->iwpd->sc_pd.pd_id;

	ctx_info->tcp_info_valid = true;
	ctx_info->iwarp_info_valid = true;
	ctx_info->user_pri = cm_node->user_pri;

	irdma_init_tcp_ctx(cm_node, &iwqp->tcp_info, iwqp);
	if (cm_node->snd_mark_en) {
		iwarp_info->snd_mark_en = true;
		iwarp_info->snd_mark_offset = (iwqp->tcp_info.snd_nxt & SNDMARKER_SEQNMASK) +
		    cm_node->lsmm_size;
	}

	cm_node->state = IRDMA_CM_STATE_OFFLOADED;
	iwqp->tcp_info.tcp_state = IRDMA_TCP_STATE_ESTABLISHED;
	iwqp->tcp_info.src_mac_addr_idx = iwqp->iwdev->mac_ip_table_idx;

	if (cm_node->rcv_mark_en) {
		iwarp_info->rcv_mark_en = true;
		iwarp_info->align_hdrs = true;
	}

	irdma_sc_qp_setctx(&iwqp->sc_qp, iwqp->host_ctx.va, ctx_info);

	/* once tcp_info is set, no need to do it again */
	ctx_info->tcp_info_valid = false;
	ctx_info->iwarp_info_valid = false;
}

/**
 * irdma_cm_disconn - when a connection is being closed
 * @iwqp: associated qp for the connection
 */
void
irdma_cm_disconn(struct irdma_qp *iwqp)
{
	struct irdma_device *iwdev = iwqp->iwdev;
	struct disconn_work *work;
	unsigned long flags;

	work = kzalloc(sizeof(*work), GFP_ATOMIC);
	if (!work)
		return;

	spin_lock_irqsave(&iwdev->rf->qptable_lock, flags);
	if (!iwdev->rf->qp_table[iwqp->ibqp.qp_num]) {
		spin_unlock_irqrestore(&iwdev->rf->qptable_lock, flags);
		irdma_debug(&iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "qp_id %d is already freed\n", iwqp->ibqp.qp_num);
		kfree(work);
		return;
	}
	irdma_qp_add_ref(&iwqp->ibqp);
	spin_unlock_irqrestore(&iwdev->rf->qptable_lock, flags);

	work->iwqp = iwqp;
	INIT_WORK(&work->work, irdma_disconnect_worker);
	queue_work(iwdev->cleanup_wq, &work->work);
}

/**
 * irdma_qp_disconnect - free qp and close cm
 * @iwqp: associate qp for the connection
 */
static void
irdma_qp_disconnect(struct irdma_qp *iwqp)
{
	struct irdma_device *iwdev = iwqp->iwdev;

	iwqp->active_conn = 0;
	/* close the CM node down if it is still active */
	irdma_debug(&iwdev->rf->sc_dev, IRDMA_DEBUG_CM, "Call close API\n");
	irdma_cm_close(iwqp->cm_node);
}

/**
 * irdma_cm_disconn_true - called by worker thread to disconnect qp
 * @iwqp: associate qp for the connection
 */
static void
irdma_cm_disconn_true(struct irdma_qp *iwqp)
{
	struct iw_cm_id *cm_id;
	struct irdma_device *iwdev;
	struct irdma_sc_qp *qp = &iwqp->sc_qp;
	u16 last_ae;
	u8 original_hw_tcp_state;
	u8 original_ibqp_state;
	int disconn_status = 0;
	int issue_disconn = 0;
	int issue_close = 0;
	int issue_flush = 0;
	unsigned long flags;
	int err;

	iwdev = iwqp->iwdev;
	spin_lock_irqsave(&iwqp->lock, flags);
	if (rdma_protocol_roce(&iwdev->ibdev, 1)) {
		struct ib_qp_attr attr;

		if (iwqp->flush_issued || iwqp->sc_qp.qp_uk.destroy_pending) {
			spin_unlock_irqrestore(&iwqp->lock, flags);
			return;
		}

		spin_unlock_irqrestore(&iwqp->lock, flags);

		attr.qp_state = IB_QPS_ERR;
		irdma_modify_qp_roce(&iwqp->ibqp, &attr, IB_QP_STATE, NULL);
		irdma_ib_qp_event(iwqp, qp->event_type);
		return;
	}

	cm_id = iwqp->cm_id;
	original_hw_tcp_state = iwqp->hw_tcp_state;
	original_ibqp_state = iwqp->ibqp_state;
	last_ae = iwqp->last_aeq;

	if (qp->term_flags) {
		issue_disconn = 1;
		issue_close = 1;
		iwqp->cm_id = NULL;
		irdma_terminate_del_timer(qp);
		if (!iwqp->flush_issued) {
			iwqp->flush_issued = 1;
			issue_flush = 1;
		}
	} else if ((original_hw_tcp_state == IRDMA_TCP_STATE_CLOSE_WAIT) ||
		   ((original_ibqp_state == IB_QPS_RTS) &&
		    (last_ae == IRDMA_AE_LLP_CONNECTION_RESET))) {
		issue_disconn = 1;
		if (last_ae == IRDMA_AE_LLP_CONNECTION_RESET)
			disconn_status = -ECONNRESET;
	}

	if (original_hw_tcp_state == IRDMA_TCP_STATE_CLOSED ||
	    original_hw_tcp_state == IRDMA_TCP_STATE_TIME_WAIT ||
	    last_ae == IRDMA_AE_RDMAP_ROE_BAD_LLP_CLOSE ||
	    last_ae == IRDMA_AE_BAD_CLOSE ||
	    last_ae == IRDMA_AE_LLP_CONNECTION_RESET || iwdev->rf->reset || !cm_id) {
		issue_close = 1;
		iwqp->cm_id = NULL;
		qp->term_flags = 0;
		if (!iwqp->flush_issued) {
			iwqp->flush_issued = 1;
			issue_flush = 1;
		}
	}

	spin_unlock_irqrestore(&iwqp->lock, flags);
	if (issue_flush && !iwqp->sc_qp.qp_uk.destroy_pending) {
		irdma_flush_wqes(iwqp, IRDMA_FLUSH_SQ | IRDMA_FLUSH_RQ |
				 IRDMA_FLUSH_WAIT);

		if (qp->term_flags)
			irdma_ib_qp_event(iwqp, qp->event_type);
	}

	if (!cm_id || !cm_id->event_handler)
		return;

	spin_lock_irqsave(&iwdev->cm_core.ht_lock, flags);
	if (!iwqp->cm_node) {
		spin_unlock_irqrestore(&iwdev->cm_core.ht_lock, flags);
		return;
	}
	atomic_inc(&iwqp->cm_node->refcnt);

	spin_unlock_irqrestore(&iwdev->cm_core.ht_lock, flags);

	if (issue_disconn) {
		err = irdma_send_cm_event(iwqp->cm_node, cm_id,
					  IW_CM_EVENT_DISCONNECT,
					  disconn_status);
		if (err)
			irdma_debug(&iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
				    "disconnect event failed: - cm_id = %p\n",
				    cm_id);
	}
	if (issue_close) {
		cm_id->provider_data = iwqp;
		err = irdma_send_cm_event(iwqp->cm_node, cm_id,
					  IW_CM_EVENT_CLOSE, 0);
		if (err)
			irdma_debug(&iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
				    "close event failed: - cm_id = %p\n",
				    cm_id);
		irdma_qp_disconnect(iwqp);
	}
	irdma_rem_ref_cm_node(iwqp->cm_node);
}

/**
 * irdma_disconnect_worker - worker for connection close
 * @work: points or disconn structure
 */
static void
irdma_disconnect_worker(struct work_struct *work)
{
	struct disconn_work *dwork = container_of(work, struct disconn_work, work);
	struct irdma_qp *iwqp = dwork->iwqp;

	kfree(dwork);
	irdma_cm_disconn_true(iwqp);
	irdma_qp_rem_ref(&iwqp->ibqp);
}

/**
 * irdma_free_lsmm_rsrc - free lsmm memory and deregister
 * @iwqp: associate qp for the connection
 */
void
irdma_free_lsmm_rsrc(struct irdma_qp *iwqp)
{
	struct irdma_device *iwdev;

	iwdev = iwqp->iwdev;

	if (iwqp->ietf_mem.va) {
		if (iwqp->lsmm_mr)
			iwdev->ibdev.dereg_mr(iwqp->lsmm_mr);

		irdma_free_dma_mem(iwdev->rf->sc_dev.hw,
				   &iwqp->ietf_mem);
		iwqp->ietf_mem.va = NULL;
	}
}

/**
 * irdma_accept - registered call for connection to be accepted
 * @cm_id: cm information for passive connection
 * @conn_param: accpet parameters
 */
int
irdma_accept(struct iw_cm_id *cm_id, struct iw_cm_conn_param *conn_param)
{
	struct ib_qp *ibqp;
	struct irdma_qp *iwqp;
	struct irdma_device *iwdev;
	struct irdma_sc_dev *dev;
	struct irdma_cm_node *cm_node;
	struct ib_qp_attr attr = {0};
	int passive_state;
	struct ib_mr *ibmr;
	struct irdma_pd *iwpd;
	u16 buf_len = 0;
	struct irdma_kmem_info accept;
	u64 tagged_offset;
	int wait_ret;
	int ret = 0;

	ibqp = irdma_get_qp(cm_id->device, conn_param->qpn);
	if (!ibqp)
		return -EINVAL;

	iwqp = to_iwqp(ibqp);
	iwdev = iwqp->iwdev;
	dev = &iwdev->rf->sc_dev;
	cm_node = cm_id->provider_data;

	if (((struct sockaddr_in *)&cm_id->local_addr)->sin_family == AF_INET) {
		cm_node->ipv4 = true;
		cm_node->vlan_id = irdma_get_vlan_ipv4(cm_id, cm_node->loc_addr);
	} else {
		cm_node->ipv4 = false;
		irdma_get_vlan_mac_ipv6(cm_id, cm_node->loc_addr, &cm_node->vlan_id,
					NULL);
	}
	irdma_debug(&iwdev->rf->sc_dev, IRDMA_DEBUG_CM, "Accept vlan_id=%d\n",
		    cm_node->vlan_id);

	if (cm_node->state == IRDMA_CM_STATE_LISTENER_DESTROYED) {
		ret = -EINVAL;
		goto error;
	}

	passive_state = atomic_add_return(1, &cm_node->passive_state);
	if (passive_state == IRDMA_SEND_RESET_EVENT) {
		ret = -ECONNRESET;
		goto error;
	}

	buf_len = conn_param->private_data_len + IRDMA_MAX_IETF_SIZE;
	iwqp->ietf_mem.size = buf_len;
	iwqp->ietf_mem.va = irdma_allocate_dma_mem(dev->hw, &iwqp->ietf_mem,
						   iwqp->ietf_mem.size, 1);
	if (!iwqp->ietf_mem.va) {
		ret = -ENOMEM;
		goto error;
	}

	cm_node->pdata.size = conn_param->private_data_len;
	accept.addr = iwqp->ietf_mem.va;
	accept.size = irdma_cm_build_mpa_frame(cm_node, &accept, MPA_KEY_REPLY);
	memcpy((u8 *)accept.addr + accept.size, conn_param->private_data,
	       conn_param->private_data_len);

	if (cm_node->dev->ws_add(iwqp->sc_qp.vsi, cm_node->user_pri)) {
		ret = -ENOMEM;
		goto error;
	}
	iwqp->sc_qp.user_pri = cm_node->user_pri;
	irdma_qp_add_qos(&iwqp->sc_qp);
	if (cm_node->dev->hw_attrs.uk_attrs.hw_rev == IRDMA_GEN_2)
		iwdev->rf->check_fc(&iwdev->vsi, &iwqp->sc_qp);
	/* setup our first outgoing iWarp send WQE (the IETF frame response) */
	iwpd = iwqp->iwpd;
	tagged_offset = (uintptr_t)iwqp->ietf_mem.va;
	ibmr = irdma_reg_phys_mr(&iwpd->ibpd, iwqp->ietf_mem.pa, buf_len,
				 IB_ACCESS_LOCAL_WRITE, &tagged_offset);
	if (IS_ERR(ibmr)) {
		ret = -ENOMEM;
		goto error;
	}

	ibmr->pd = &iwpd->ibpd;
	ibmr->device = iwpd->ibpd.device;
	iwqp->lsmm_mr = ibmr;
	if (iwqp->page)
		iwqp->sc_qp.qp_uk.sq_base = kmap_local_page(iwqp->page);

	cm_node->lsmm_size = accept.size + conn_param->private_data_len;
	irdma_sc_send_lsmm(&iwqp->sc_qp, iwqp->ietf_mem.va, cm_node->lsmm_size,
			   ibmr->lkey);

	if (iwqp->page)
		kunmap_local(iwqp->sc_qp.qp_uk.sq_base);

	iwqp->cm_id = cm_id;
	cm_node->cm_id = cm_id;

	cm_id->provider_data = iwqp;
	iwqp->active_conn = 0;
	iwqp->cm_node = cm_node;
	cm_node->iwqp = iwqp;
	irdma_cm_init_tsa_conn(iwqp, cm_node);
	irdma_qp_add_ref(&iwqp->ibqp);
	cm_id->add_ref(cm_id);

	attr.qp_state = IB_QPS_RTS;
	cm_node->qhash_set = false;
	cm_node->cm_core->cm_free_ah(cm_node);

	irdma_modify_qp(&iwqp->ibqp, &attr, IB_QP_STATE, NULL);
	if (dev->hw_attrs.uk_attrs.feature_flags & IRDMA_FEATURE_RTS_AE) {
		wait_ret = wait_event_interruptible_timeout(iwqp->waitq,
							    iwqp->rts_ae_rcvd,
							    IRDMA_MAX_TIMEOUT);
		if (!wait_ret) {
			irdma_debug(&iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
				    "Slow Connection: cm_node=%p, loc_port=%d, rem_port=%d, cm_id=%p\n",
				    cm_node, cm_node->loc_port,
				    cm_node->rem_port, cm_node->cm_id);
			ret = -ECONNRESET;
			goto error;
		}
	}

	irdma_send_cm_event(cm_node, cm_id, IW_CM_EVENT_ESTABLISHED, 0);
	cm_node->accelerated = true;
	complete(&cm_node->establish_comp);

	if (cm_node->accept_pend) {
		atomic_dec(&cm_node->listener->pend_accepts_cnt);
		cm_node->accept_pend = 0;
	}

	irdma_debug(&iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
		    "rem_port=0x%04x, loc_port=0x%04x rem_addr=%x loc_addr=%x cm_node=%p cm_id=%p qp_id=%d\n\n",
		    cm_node->rem_port, cm_node->loc_port, cm_node->rem_addr[0],
		    cm_node->loc_addr[0], cm_node, cm_id, ibqp->qp_num);
	cm_node->cm_core->stats_accepts++;

	return 0;
error:
	irdma_free_lsmm_rsrc(iwqp);
	irdma_rem_ref_cm_node(cm_node);

	return ret;
}

/**
 * irdma_reject - registered call for connection to be rejected
 * @cm_id: cm information for passive connection
 * @pdata: private data to be sent
 * @pdata_len: private data length
 */
int
irdma_reject(struct iw_cm_id *cm_id, const void *pdata, u8 pdata_len)
{
	struct irdma_device *iwdev;
	struct irdma_cm_node *cm_node;

	cm_node = cm_id->provider_data;
	cm_node->pdata.size = pdata_len;

	iwdev = to_iwdev(cm_id->device);
	if (!iwdev)
		return -EINVAL;

	cm_node->cm_core->stats_rejects++;

	if (pdata_len + sizeof(struct ietf_mpa_v2) > IRDMA_MAX_CM_BUF)
		return -EINVAL;

	return irdma_cm_reject(cm_node, pdata, pdata_len);
}

/**
 * irdma_connect - registered call for connection to be established
 * @cm_id: cm information for passive connection
 * @conn_param: Information about the connection
 */
int
irdma_connect(struct iw_cm_id *cm_id, struct iw_cm_conn_param *conn_param)
{
	struct ib_qp *ibqp;
	struct irdma_qp *iwqp;
	struct irdma_device *iwdev;
	struct irdma_cm_node *cm_node;
	struct irdma_cm_info cm_info;
	struct sockaddr_in *laddr;
	struct sockaddr_in *raddr;
	struct sockaddr_in6 *laddr6;
	struct sockaddr_in6 *raddr6;
	int ret = 0;

	ibqp = irdma_get_qp(cm_id->device, conn_param->qpn);
	if (!ibqp)
		return -EINVAL;
	iwqp = to_iwqp(ibqp);
	if (!iwqp)
		return -EINVAL;
	iwdev = iwqp->iwdev;
	if (!iwdev)
		return -EINVAL;

	laddr = (struct sockaddr_in *)&cm_id->m_local_addr;
	raddr = (struct sockaddr_in *)&cm_id->m_remote_addr;
	laddr6 = (struct sockaddr_in6 *)&cm_id->m_local_addr;
	raddr6 = (struct sockaddr_in6 *)&cm_id->m_remote_addr;

	if (!(laddr->sin_port) || !(raddr->sin_port))
		return -EINVAL;

	iwqp->active_conn = 1;
	iwqp->cm_id = NULL;
	cm_id->provider_data = iwqp;

	/* set up the connection params for the node */
	if (cm_id->remote_addr.ss_family == AF_INET) {
		if (iwdev->vsi.mtu < IRDMA_MIN_MTU_IPV4)
			return -EINVAL;

		cm_info.ipv4 = true;
		memset(cm_info.loc_addr, 0, sizeof(cm_info.loc_addr));
		memset(cm_info.rem_addr, 0, sizeof(cm_info.rem_addr));
		cm_info.loc_addr[0] = ntohl(laddr->sin_addr.s_addr);
		cm_info.rem_addr[0] = ntohl(raddr->sin_addr.s_addr);
		cm_info.loc_port = ntohs(laddr->sin_port);
		cm_info.rem_port = ntohs(raddr->sin_port);
		cm_info.vlan_id = irdma_get_vlan_ipv4(cm_id, cm_info.loc_addr);
	} else {
		if (iwdev->vsi.mtu < IRDMA_MIN_MTU_IPV6)
			return -EINVAL;

		cm_info.ipv4 = false;
		irdma_copy_ip_ntohl(cm_info.loc_addr,
				    laddr6->sin6_addr.__u6_addr.__u6_addr32);
		irdma_copy_ip_ntohl(cm_info.rem_addr,
				    raddr6->sin6_addr.__u6_addr.__u6_addr32);
		cm_info.loc_port = ntohs(laddr6->sin6_port);
		cm_info.rem_port = ntohs(raddr6->sin6_port);
		irdma_get_vlan_mac_ipv6(cm_id, cm_info.loc_addr, &cm_info.vlan_id, NULL);
	}
	cm_info.cm_id = cm_id;
	cm_info.qh_qpid = iwdev->vsi.ilq->qp_id;
	cm_info.tos = cm_id->tos;
	if (iwdev->vsi.dscp_mode) {
		cm_info.user_pri =
		    iwqp->sc_qp.vsi->dscp_map[irdma_tos2dscp(cm_info.tos)];
	} else {
		cm_info.user_pri = rt_tos2priority(cm_id->tos);
		cm_info.user_pri =
		    irdma_iw_get_vlan_prio(cm_info.loc_addr,
					   cm_info.user_pri,
					   cm_info.ipv4);
	}

	if (iwqp->sc_qp.dev->ws_add(iwqp->sc_qp.vsi, cm_info.user_pri))
		return -ENOMEM;
	iwqp->sc_qp.user_pri = cm_info.user_pri;
	irdma_qp_add_qos(&iwqp->sc_qp);
	if (iwdev->rf->sc_dev.hw_attrs.uk_attrs.hw_rev == IRDMA_GEN_2)
		iwdev->rf->check_fc(&iwdev->vsi, &iwqp->sc_qp);
	irdma_debug(&iwdev->rf->sc_dev, IRDMA_DEBUG_DCB, "TOS:[%d] UP:[%d]\n",
		    cm_id->tos, cm_info.user_pri);

	ret = irdma_create_cm_node(&iwdev->cm_core, iwdev, conn_param, &cm_info,
				   &cm_node);
	if (ret)
		return ret;
	ret = cm_node->cm_core->cm_create_ah(cm_node, true);
	if (ret)
		goto err;
	if (irdma_manage_qhash(iwdev, &cm_info,
			       IRDMA_QHASH_TYPE_TCP_ESTABLISHED,
			       IRDMA_QHASH_MANAGE_TYPE_ADD, NULL, true)) {
		ret = -EINVAL;
		goto err;
	}
	cm_node->qhash_set = true;

	cm_node->apbvt_entry = irdma_add_apbvt(iwdev, cm_info.loc_port);
	if (!cm_node->apbvt_entry) {
		ret = -EINVAL;
		goto err;
	}

	cm_node->apbvt_set = true;
	iwqp->cm_node = cm_node;
	cm_node->iwqp = iwqp;
	iwqp->cm_id = cm_id;
	irdma_qp_add_ref(&iwqp->ibqp);
	cm_id->add_ref(cm_id);

	if (cm_node->state != IRDMA_CM_STATE_OFFLOADED) {
		cm_node->state = IRDMA_CM_STATE_SYN_SENT;
		ret = irdma_send_syn(cm_node, 0);
		if (ret)
			goto err;
	}

	irdma_debug(&iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
		    "rem_port=0x%04x, loc_port=0x%04x rem_addr=%x loc_addr=%x cm_node=%p cm_id=%p qp_id = %d\n\n",
		    cm_node->rem_port, cm_node->loc_port, cm_node->rem_addr[0],
		    cm_node->loc_addr[0], cm_node, cm_id, ibqp->qp_num);

	return 0;

err:
	if (cm_info.ipv4)
		irdma_debug(&iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "connect() FAILED: dest addr=%x",
			    cm_info.rem_addr[0]);
	else
		irdma_debug(&iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "connect() FAILED: dest addr=%x:%x:%x:%x",
			    IRDMA_PRINT_IP6(cm_info.rem_addr));
	irdma_rem_ref_cm_node(cm_node);
	iwdev->cm_core.stats_connect_errs++;

	return ret;
}

/**
 * irdma_create_listen - registered call creating listener
 * @cm_id: cm information for passive connection
 * @backlog: to max accept pending count
 */
int
irdma_create_listen(struct iw_cm_id *cm_id, int backlog)
{
	struct irdma_device *iwdev;
	struct irdma_cm_listener *cm_listen_node;
	struct irdma_cm_info cm_info = {0};
	struct sockaddr_in *laddr;
	struct sockaddr_in6 *laddr6;
	bool wildcard = false;
	int err;

	iwdev = to_iwdev(cm_id->device);
	if (!iwdev)
		return -EINVAL;

	laddr = (struct sockaddr_in *)&cm_id->m_local_addr;
	laddr6 = (struct sockaddr_in6 *)&cm_id->m_local_addr;
	cm_info.qh_qpid = iwdev->vsi.ilq->qp_id;

	if (laddr->sin_family == AF_INET) {
		if (iwdev->vsi.mtu < IRDMA_MIN_MTU_IPV4)
			return -EINVAL;

		cm_info.ipv4 = true;
		cm_info.loc_addr[0] = ntohl(laddr->sin_addr.s_addr);
		cm_info.loc_port = ntohs(laddr->sin_port);

		if (laddr->sin_addr.s_addr != htonl(INADDR_ANY)) {
			cm_info.vlan_id = irdma_get_vlan_ipv4(cm_id, cm_info.loc_addr);
		} else {
			cm_info.vlan_id = 0xFFFF;
			wildcard = true;
		}
	} else {
		if (iwdev->vsi.mtu < IRDMA_MIN_MTU_IPV6)
			return -EINVAL;

		cm_info.ipv4 = false;
		irdma_copy_ip_ntohl(cm_info.loc_addr,
				    laddr6->sin6_addr.__u6_addr.__u6_addr32);
		cm_info.loc_port = ntohs(laddr6->sin6_port);
		if (!IN6_IS_ADDR_UNSPECIFIED(&laddr6->sin6_addr)) {
			irdma_get_vlan_mac_ipv6(cm_id, cm_info.loc_addr,
						&cm_info.vlan_id, NULL);
		} else {
			cm_info.vlan_id = 0xFFFF;
			wildcard = true;
		}
	}

	if (cm_info.vlan_id >= VLAN_N_VID && iwdev->dcb_vlan_mode)
		cm_info.vlan_id = 0;
	cm_info.backlog = backlog;
	cm_info.cm_id = cm_id;

	cm_listen_node = irdma_make_listen_node(&iwdev->cm_core, iwdev,
						&cm_info);
	if (!cm_listen_node) {
		irdma_debug(&iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "cm_listen_node == NULL\n");
		return -ENOMEM;
	}

	cm_id->provider_data = cm_listen_node;

	cm_listen_node->tos = cm_id->tos;
	if (iwdev->vsi.dscp_mode)
		cm_listen_node->user_pri =
		    iwdev->vsi.dscp_map[irdma_tos2dscp(cm_id->tos)];
	else
		cm_listen_node->user_pri = rt_tos2priority(cm_id->tos);
	cm_info.user_pri = cm_listen_node->user_pri;
	if (!cm_listen_node->reused_node) {
		if (wildcard) {
			err = irdma_add_mqh(iwdev, &cm_info, cm_listen_node);
			if (err)
				goto error;
		} else {
			if (!iwdev->vsi.dscp_mode)
				cm_info.user_pri = cm_listen_node->user_pri =
				    irdma_iw_get_vlan_prio(cm_info.loc_addr,
							   cm_info.user_pri,
							   cm_info.ipv4);
			err = irdma_manage_qhash(iwdev, &cm_info,
						 IRDMA_QHASH_TYPE_TCP_SYN,
						 IRDMA_QHASH_MANAGE_TYPE_ADD,
						 NULL, true);
			if (err)
				goto error;

			cm_listen_node->qhash_set = true;
		}

		cm_listen_node->apbvt_entry = irdma_add_apbvt(iwdev,
							      cm_info.loc_port);
		if (!cm_listen_node->apbvt_entry)
			goto error;
	}
	cm_id->add_ref(cm_id);
	cm_listen_node->cm_core->stats_listen_created++;
	irdma_debug(&iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
		    "loc_port=0x%04x loc_addr=%x cm_listen_node=%p cm_id=%p qhash_set=%d vlan_id=%d\n",
		    cm_listen_node->loc_port, cm_listen_node->loc_addr[0],
		    cm_listen_node, cm_listen_node->cm_id,
		    cm_listen_node->qhash_set, cm_listen_node->vlan_id);

	return 0;

error:

	irdma_cm_del_listen(&iwdev->cm_core, cm_listen_node, false);

	return -EINVAL;
}

/**
 * irdma_destroy_listen - registered call to destroy listener
 * @cm_id: cm information for passive connection
 */
int
irdma_destroy_listen(struct iw_cm_id *cm_id)
{
	struct irdma_device *iwdev;

	iwdev = to_iwdev(cm_id->device);
	if (cm_id->provider_data)
		irdma_cm_del_listen(&iwdev->cm_core, cm_id->provider_data,
				    true);
	else
		irdma_debug(&iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "cm_id->provider_data was NULL\n");

	cm_id->rem_ref(cm_id);

	return 0;
}

/**
 * irdma_iw_teardown_list_prep - add conn nodes slated for tear
 * down to list
 * @cm_core: cm's core
 * @teardown_list: a list to which cm_node will be selected
 * @ipaddr: pointer to ip address
 * @nfo: pointer to cm_info structure instance
 * @disconnect_all: flag indicating disconnect all QPs
 */
static void
irdma_iw_teardown_list_prep(struct irdma_cm_core *cm_core,
			    struct list_head *teardown_list,
			    u32 *ipaddr,
			    struct irdma_cm_info *nfo,
			    bool disconnect_all)
{
	struct irdma_cm_node *cm_node;
	int bkt;

	HASH_FOR_EACH_RCU(cm_core->cm_hash_tbl, bkt, cm_node, list) {
		if ((disconnect_all ||
		     (nfo->vlan_id == cm_node->vlan_id &&
		      !memcmp(cm_node->loc_addr, ipaddr, nfo->ipv4 ? 4 : 16))) &&
		    atomic_inc_not_zero(&cm_node->refcnt))
			list_add(&cm_node->teardown_entry, teardown_list);
	}
}

static inline bool
irdma_ip_vlan_match(u32 *ip1, u16 vlan_id1,
		    bool check_vlan, u32 *ip2,
		    u16 vlan_id2, bool ipv4)
{
	return (!check_vlan || vlan_id1 == vlan_id2) &&
	    !memcmp(ip1, ip2, ipv4 ? 4 : 16);
}

/**
 * irdma_roce_teardown_list_prep - add conn nodes slated for
 * tear down to list
 * @iwdev: RDMA device
 * @teardown_list: a list to which cm_node will be selected
 * @ipaddr: pointer to ip address
 * @nfo: pointer to cm_info structure instance
 * @disconnect_all: flag indicating disconnect all QPs
 */
static void
irdma_roce_teardown_list_prep(struct irdma_device *iwdev,
			      struct list_head *teardown_list,
			      u32 *ipaddr,
			      struct irdma_cm_info *nfo,
			      bool disconnect_all)
{
	struct irdma_sc_vsi *vsi = &iwdev->vsi;
	struct irdma_sc_qp *sc_qp;
	struct list_head *list_node;
	struct irdma_qp *qp;
	unsigned long flags;
	int i;

	for (i = 0; i < IRDMA_MAX_USER_PRIORITY; i++) {
		mutex_lock(&vsi->qos[i].qos_mutex);
		list_for_each(list_node, &vsi->qos[i].qplist) {
			u32 qp_ip[4];

			sc_qp = container_of(list_node, struct irdma_sc_qp,
					     list);
			if (sc_qp->qp_uk.qp_type != IRDMA_QP_TYPE_ROCE_RC)
				continue;

			qp = sc_qp->qp_uk.back_qp;
			if (!disconnect_all) {
				if (nfo->ipv4)
					qp_ip[0] = qp->udp_info.local_ipaddr[3];
				else
					memcpy(qp_ip,
					       &qp->udp_info.local_ipaddr[0],
					       sizeof(qp_ip));
			}

			if (disconnect_all ||
			    irdma_ip_vlan_match(qp_ip,
						qp->udp_info.vlan_tag & EVL_VLID_MASK,
						qp->udp_info.insert_vlan_tag,
						ipaddr, nfo->vlan_id, nfo->ipv4)) {
				spin_lock_irqsave(&iwdev->rf->qptable_lock, flags);
				if (iwdev->rf->qp_table[sc_qp->qp_uk.qp_id]) {
					irdma_qp_add_ref(&qp->ibqp);
					list_add(&qp->teardown_entry, teardown_list);
				}
				spin_unlock_irqrestore(&iwdev->rf->qptable_lock, flags);
			}
		}
		mutex_unlock(&vsi->qos[i].qos_mutex);
	}
}

/**
 * irdma_cm_event_connected - handle connected active node
 * @event: the info for cm_node of connection
 */
static void
irdma_cm_event_connected(struct irdma_cm_event *event)
{
	struct irdma_qp *iwqp;
	struct irdma_device *iwdev;
	struct irdma_cm_node *cm_node;
	struct irdma_sc_dev *dev;
	struct ib_qp_attr attr = {0};
	struct iw_cm_id *cm_id;
	int status;
	bool read0;
	int wait_ret = 0;

	cm_node = event->cm_node;
	cm_id = cm_node->cm_id;
	iwqp = cm_id->provider_data;
	iwdev = iwqp->iwdev;
	dev = &iwdev->rf->sc_dev;
	if (iwqp->sc_qp.qp_uk.destroy_pending) {
		status = -ETIMEDOUT;
		goto error;
	}

	irdma_cm_init_tsa_conn(iwqp, cm_node);
	read0 = (cm_node->send_rdma0_op == SEND_RDMA_READ_ZERO);
	if (iwqp->page)
		iwqp->sc_qp.qp_uk.sq_base = kmap_local_page(iwqp->page);
	irdma_sc_send_rtt(&iwqp->sc_qp, read0);
	if (iwqp->page)
		kunmap_local(iwqp->sc_qp.qp_uk.sq_base);

	attr.qp_state = IB_QPS_RTS;
	cm_node->qhash_set = false;
	irdma_modify_qp(&iwqp->ibqp, &attr, IB_QP_STATE, NULL);
	if (dev->hw_attrs.uk_attrs.feature_flags & IRDMA_FEATURE_RTS_AE) {
		wait_ret = wait_event_interruptible_timeout(iwqp->waitq,
							    iwqp->rts_ae_rcvd,
							    IRDMA_MAX_TIMEOUT);
		if (!wait_ret)
			irdma_debug(&iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
				    "Slow Connection: cm_node=%p, loc_port=%d, rem_port=%d, cm_id=%p\n",
				    cm_node, cm_node->loc_port,
				    cm_node->rem_port, cm_node->cm_id);
	}

	irdma_send_cm_event(cm_node, cm_id, IW_CM_EVENT_CONNECT_REPLY, 0);
	cm_node->accelerated = true;
	complete(&cm_node->establish_comp);
	cm_node->cm_core->cm_free_ah(cm_node);
	return;

error:
	iwqp->cm_id = NULL;
	cm_id->provider_data = NULL;
	irdma_send_cm_event(event->cm_node, cm_id, IW_CM_EVENT_CONNECT_REPLY,
			    status);
	irdma_rem_ref_cm_node(event->cm_node);
}

/**
 * irdma_cm_event_reset - handle reset
 * @event: the info for cm_node of connection
 */
static void
irdma_cm_event_reset(struct irdma_cm_event *event)
{
	struct irdma_cm_node *cm_node = event->cm_node;
	struct iw_cm_id *cm_id = cm_node->cm_id;
	struct irdma_qp *iwqp;

	if (!cm_id)
		return;

	iwqp = cm_id->provider_data;
	if (!iwqp)
		return;

	irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
		    "reset event %p - cm_id = %p\n", event->cm_node, cm_id);
	iwqp->cm_id = NULL;

	irdma_send_cm_event(cm_node, cm_node->cm_id, IW_CM_EVENT_DISCONNECT,
			    -ECONNRESET);
	irdma_send_cm_event(cm_node, cm_node->cm_id, IW_CM_EVENT_CLOSE, 0);
}

/**
 * irdma_cm_event_handler - send event to cm upper layer
 * @work: pointer of cm event info.
 */
static void
irdma_cm_event_handler(struct work_struct *work)
{
	struct irdma_cm_event *event = container_of(work, struct irdma_cm_event, event_work);
	struct irdma_cm_node *cm_node;

	if (!event || !event->cm_node || !event->cm_node->cm_core)
		return;

	cm_node = event->cm_node;

	switch (event->type) {
	case IRDMA_CM_EVENT_MPA_REQ:
		irdma_send_cm_event(cm_node, cm_node->cm_id,
				    IW_CM_EVENT_CONNECT_REQUEST, 0);
		break;
	case IRDMA_CM_EVENT_RESET:
		irdma_cm_event_reset(event);
		break;
	case IRDMA_CM_EVENT_CONNECTED:
		if (!event->cm_node->cm_id ||
		    event->cm_node->state != IRDMA_CM_STATE_OFFLOADED)
			break;
		irdma_cm_event_connected(event);
		break;
	case IRDMA_CM_EVENT_MPA_REJECT:
		if (!event->cm_node->cm_id ||
		    cm_node->state == IRDMA_CM_STATE_OFFLOADED)
			break;
		irdma_send_cm_event(cm_node, cm_node->cm_id,
				    IW_CM_EVENT_CONNECT_REPLY, -ECONNREFUSED);
		break;
	case IRDMA_CM_EVENT_ABORTED:
		if (!event->cm_node->cm_id ||
		    event->cm_node->state == IRDMA_CM_STATE_OFFLOADED)
			break;
		irdma_event_connect_error(event);
		break;
	default:
		irdma_debug(&cm_node->iwdev->rf->sc_dev, IRDMA_DEBUG_CM,
			    "bad event type = %d\n", event->type);
		break;
	}

	irdma_rem_ref_cm_node(event->cm_node);
	kfree(event);
}

/**
 * irdma_cm_post_event - queue event request for worker thread
 * @event: cm node's info for up event call
 */
static void
irdma_cm_post_event(struct irdma_cm_event *event)
{
	atomic_inc(&event->cm_node->refcnt);
	INIT_WORK(&event->event_work, irdma_cm_event_handler);
	queue_work(event->cm_node->cm_core->event_wq, &event->event_work);
}

/**
 * irdma_cm_teardown_connections - teardown QPs
 * @iwdev: device pointer
 * @ipaddr: Pointer to IPv4 or IPv6 address
 * @nfo: Connection info
 * @disconnect_all: flag indicating disconnect all QPs
 *
 * teardown QPs where source or destination addr matches ip addr
 */
static void __unused
irdma_cm_teardown_connections(struct irdma_device *iwdev,
			      u32 *ipaddr,
			      struct irdma_cm_info *nfo,
			      bool disconnect_all)
{
	struct irdma_cm_core *cm_core = &iwdev->cm_core;
	struct list_head *list_core_temp;
	struct list_head *list_node;
	struct irdma_cm_node *cm_node;
	struct list_head teardown_list;
	struct ib_qp_attr attr;
	struct irdma_qp *qp;

	INIT_LIST_HEAD(&teardown_list);

	rcu_read_lock();
	irdma_iw_teardown_list_prep(cm_core, &teardown_list, ipaddr, nfo, disconnect_all);
	rcu_read_unlock();

	attr.qp_state = IB_QPS_ERR;
	list_for_each_safe(list_node, list_core_temp, &teardown_list) {
		cm_node = container_of(list_node, struct irdma_cm_node,
				       teardown_entry);
		irdma_modify_qp(&cm_node->iwqp->ibqp, &attr, IB_QP_STATE, NULL);
		if (iwdev->rf->reset)
			irdma_cm_disconn(cm_node->iwqp);
		irdma_rem_ref_cm_node(cm_node);
	}

	if (!rdma_protocol_roce(&iwdev->ibdev, 1))
		return;

	INIT_LIST_HEAD(&teardown_list);
	irdma_roce_teardown_list_prep(iwdev, &teardown_list, ipaddr, nfo, disconnect_all);

	list_for_each_safe(list_node, list_core_temp, &teardown_list) {
		qp = container_of(list_node, struct irdma_qp, teardown_entry);
		irdma_modify_qp_roce(&qp->ibqp, &attr, IB_QP_STATE, NULL);
		irdma_ib_qp_event(qp, IRDMA_QP_EVENT_CATASTROPHIC);
		irdma_qp_rem_ref(&qp->ibqp);
	}
}
