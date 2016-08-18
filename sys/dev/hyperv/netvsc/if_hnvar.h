/*-
 * Copyright (c) 2016 Microsoft Corp.
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

#ifndef _IF_HNVAR_H_
#define _IF_HNVAR_H_

#include <sys/param.h>

#include <dev/hyperv/include/vmbus.h>
#include <dev/hyperv/netvsc/if_hnreg.h>

struct netvsc_dev_;

struct vmbus_channel;
struct hn_send_ctx;

typedef void		(*hn_sent_callback_t)
			(struct hn_send_ctx *, struct netvsc_dev_ *,
			 struct vmbus_channel *, const void *, int);

struct hn_send_ctx {
	hn_sent_callback_t	hn_cb;
	void			*hn_cbarg;
	uint32_t		hn_chim_idx;
	int			hn_chim_sz;
};

struct rndis_hash_info;
struct rndix_hash_value;
struct ndis_8021q_info_;
struct rndis_tcp_ip_csum_info_;

struct hn_recvinfo {
	const struct ndis_8021q_info_	*vlan_info;
	const struct rndis_tcp_ip_csum_info_ *csum_info;
	const struct rndis_hash_info	*hash_info;
	const struct rndis_hash_value	*hash_value;
};

#define HN_SEND_CTX_INITIALIZER(cb, cbarg)		\
{							\
	.hn_cb		= cb,				\
	.hn_cbarg	= cbarg,			\
	.hn_chim_idx	= HN_NVS_CHIM_IDX_INVALID,	\
	.hn_chim_sz	= 0				\
}

static __inline void
hn_send_ctx_init(struct hn_send_ctx *sndc, hn_sent_callback_t cb,
    void *cbarg, uint32_t chim_idx, int chim_sz)
{

	sndc->hn_cb = cb;
	sndc->hn_cbarg = cbarg;
	sndc->hn_chim_idx = chim_idx;
	sndc->hn_chim_sz = chim_sz;
}

static __inline void
hn_send_ctx_init_simple(struct hn_send_ctx *sndc, hn_sent_callback_t cb,
    void *cbarg)
{

	hn_send_ctx_init(sndc, cb, cbarg, HN_NVS_CHIM_IDX_INVALID, 0);
}

static __inline int
hn_nvs_send(struct vmbus_channel *chan, uint16_t flags,
    void *nvs_msg, int nvs_msglen, struct hn_send_ctx *sndc)
{

	return (vmbus_chan_send(chan, VMBUS_CHANPKT_TYPE_INBAND, flags,
	    nvs_msg, nvs_msglen, (uint64_t)(uintptr_t)sndc));
}

static __inline int
hn_nvs_send_sglist(struct vmbus_channel *chan, struct vmbus_gpa sg[], int sglen,
    void *nvs_msg, int nvs_msglen, struct hn_send_ctx *sndc)
{

	return (vmbus_chan_send_sglist(chan, sg, sglen, nvs_msg, nvs_msglen,
	    (uint64_t)(uintptr_t)sndc));
}

void		hn_nvs_sent_xact(struct hn_send_ctx *sndc,
		    struct netvsc_dev_ *net_dev, struct vmbus_channel *chan,
		    const void *data, int dlen);
void		hn_chim_free(struct netvsc_dev_ *net_dev, uint32_t chim_idx);

#endif	/* !_IF_HNVAR_H_ */
