/*
 * Copyright (c) 2007 Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/if_vlan.h>
#include <net/ip.h>
#include <linux/etherdevice.h>

#include "mlx4_en.h"


static struct mlx4_en_ipfrag *find_session(struct mlx4_en_rx_ring *ring,
					   struct iphdr *iph)
{
	struct mlx4_en_ipfrag *session;
	int i;

	for (i = 0; i < MLX4_EN_NUM_IPFRAG_SESSIONS; i++) {
		session = &ring->ipfrag[i];
		if (session->fragments == NULL)
			continue;
		if (session->daddr == iph->daddr &&
		    session->saddr == iph->saddr &&
		    session->id == iph->id &&
		    session->protocol == iph->protocol) {
			return session;
		}
	}
	return NULL;
}

static struct mlx4_en_ipfrag *start_session(struct mlx4_en_rx_ring *ring,
					    struct iphdr *iph)
{
	struct mlx4_en_ipfrag *session;
	int index = -1;
	int i;

	for (i = 0; i < MLX4_EN_NUM_IPFRAG_SESSIONS; i++) {
		if (ring->ipfrag[i].fragments == NULL) {
			index = i;
			break;
		}
	}
	if (index < 0)
		return NULL;

	session = &ring->ipfrag[index];

	return session;
}


static void flush_session(struct mlx4_en_priv *priv,
			  struct mlx4_en_ipfrag *session,
			  u16 more)
{
	struct sk_buff *skb = session->fragments;
	struct iphdr *iph = ip_hdr(skb);
	struct net_device *dev = skb->dev;

	/* Update IP length and checksum */
	iph->tot_len = htons(session->total_len);
	iph->frag_off = htons(more | (session->offset >> 3));
	iph->check = 0;
	iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);

	if (session->vlan)
		vlan_hwaccel_receive_skb(skb, priv->vlgrp,
					 be16_to_cpu(session->sl_vid));
	else
		netif_receive_skb(skb);
	dev->last_rx = jiffies;
	session->fragments = NULL;
	session->last = NULL;
}


static inline void frag_append(struct mlx4_en_priv *priv,
			       struct mlx4_en_ipfrag *session,
			       struct sk_buff *skb,
			       unsigned int data_len)
{
	struct sk_buff *parent = session->fragments;

	/* Update skb bookkeeping */
	parent->len += data_len;
	parent->data_len += data_len;
	session->total_len += data_len;

	skb_pull(skb, skb->len - data_len);
	parent->truesize += skb->truesize;

	if (session->last)
		session->last->next = skb;
	else
		skb_shinfo(parent)->frag_list = skb;

	session->last = skb;
}

int mlx4_en_rx_frags(struct mlx4_en_priv *priv, struct mlx4_en_rx_ring *ring,
		     struct sk_buff *skb, struct mlx4_cqe *cqe)
{
	struct mlx4_en_ipfrag *session;
	struct iphdr *iph;
	u16 ip_len;
	u16 ip_hlen;
	int data_len;
	u16 offset;

	skb_reset_network_header(skb);
	skb_reset_transport_header(skb);
	iph = ip_hdr(skb);
	ip_len = ntohs(iph->tot_len);
	ip_hlen = iph->ihl * 4;
	data_len = ip_len - ip_hlen;
	offset = ntohs(iph->frag_off);
	offset &= IP_OFFSET;
	offset <<= 3;

	session = find_session(ring, iph);
	if (unlikely(ip_fast_csum((u8 *)iph, iph->ihl))) {
		if (session)
			flush_session(priv, session, IP_MF);
		return -EINVAL;
	}
	if (session) {
		if (unlikely(session->offset + session->total_len !=
			     offset + ip_hlen)) {
			flush_session(priv, session, IP_MF);
			goto new_session;
		}
		/* Packets smaller then 60 bytes are padded to that size
		 * Need to fix len field of the skb to fit the actual data size
		 * Since ethernet header already removed, the IP total length
		 * is exactly the data size (the skb is linear)
		 */
		skb->len = ip_len;

		frag_append(priv, session, skb, data_len);
	} else {
new_session:
		session = start_session(ring, iph);
		if (unlikely(!session))
			return -ENOSPC;

		session->fragments = skb;
		session->daddr = iph->daddr;
		session->saddr = iph->saddr;
		session->id = iph->id;
		session->protocol = iph->protocol;
		session->total_len = ip_len;
		session->offset = offset;
		session->vlan = (priv->vlgrp &&
				 (be32_to_cpu(cqe->vlan_my_qpn) &
				  MLX4_CQE_VLAN_PRESENT_MASK)) ? 1 : 0;
		session->sl_vid = cqe->sl_vid;
	}
	if (!(ntohs(iph->frag_off) & IP_MF))
		flush_session(priv, session, 0);
	else if (session->fragments->len + priv->dev->mtu > 65536)
		flush_session(priv, session, IP_MF);

	return 0;
}


void mlx4_en_flush_frags(struct mlx4_en_priv *priv,
			 struct mlx4_en_rx_ring *ring)
{
	struct mlx4_en_ipfrag *session;
	int i;

	for (i = 0; i < MLX4_EN_NUM_IPFRAG_SESSIONS; i++) {
		session = &ring->ipfrag[i];
		if (session->fragments)
			flush_session(priv, session, IP_MF);
	}
}
