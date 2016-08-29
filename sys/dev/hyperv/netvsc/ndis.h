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

#ifndef _NET_NDIS_H_
#define _NET_NDIS_H_

#define NDIS_MEDIA_STATE_CONNECTED	0
#define NDIS_MEDIA_STATE_DISCONNECTED	1

#define OID_TCP_OFFLOAD_PARAMETERS	0xFC01020C

#define NDIS_OBJTYPE_DEFAULT		0x80

/* common_set */
#define NDIS_OFFLOAD_SET_NOCHG		0
#define NDIS_OFFLOAD_SET_ON		1
#define NDIS_OFFLOAD_SET_OFF		2

/* a.k.a GRE MAC */
#define NDIS_ENCAP_TYPE_NVGRE		0x00000001

struct ndis_object_hdr {
	uint8_t			ndis_type;		/* NDIS_OBJTYPE_ */
	uint8_t			ndis_rev;		/* type specific */
	uint16_t		ndis_size;		/* incl. this hdr */
};

/* OID_TCP_OFFLOAD_PARAMETERS */
struct ndis_offload_params {
	struct ndis_object_hdr	ndis_hdr;
	uint8_t			ndis_ip4csum;		/* param_set */
	uint8_t			ndis_tcp4csum;		/* param_set */
	uint8_t			ndis_udp4csum;		/* param_set */
	uint8_t			ndis_tcp6csum;		/* param_set */
	uint8_t			ndis_udp6csum;		/* param_set */
	uint8_t			ndis_lsov1;		/* lsov1_set */
	uint8_t			ndis_ipsecv1;		/* ipsecv1_set */
	uint8_t			ndis_lsov2_ip4;		/* lsov2_set */
	uint8_t			ndis_lsov2_ip6;		/* lsov2_set */
	uint8_t			ndis_tcp4conn;		/* PARAM_NOCHG */
	uint8_t			ndis_tcp6conn;		/* PARAM_NOCHG */
	uint32_t		ndis_flags;		/* 0 */
	/* NDIS >= 6.1 */
	uint8_t			ndis_ipsecv2;		/* ipsecv2_set */
	uint8_t			ndis_ipsecv2_ip4;	/* ipsecv2_set */
	/* NDIS >= 6.30 */
	uint8_t			ndis_rsc_ip4;		/* rsc_set */
	uint8_t			ndis_rsc_ip6;		/* rsc_set */
	uint8_t			ndis_encap;		/* common_set */
	uint8_t			ndis_encap_types;	/* NDIS_ENCAP_TYPE_ */
};

#define NDIS_OFFLOAD_PARAMS_SIZE	sizeof(struct ndis_offload_params)
#define NDIS_OFFLOAD_PARAMS_SIZE_6_1	\
	__offsetof(struct ndis_offload_params, ndis_rsc_ip4)

#define NDIS_OFFLOAD_PARAMS_REV_2	2	/* NDIS 6.1 */
#define NDIS_OFFLOAD_PARAMS_REV_3	3	/* NDIS 6.30 */

/* param_set */
#define NDIS_OFFLOAD_PARAM_NOCHG	0	/* common to all sets */
#define NDIS_OFFLOAD_PARAM_OFF		1
#define NDIS_OFFLOAD_PARAM_TX		2
#define NDIS_OFFLOAD_PARAM_RX		3
#define NDIS_OFFLOAD_PARAM_TXRX		4

/* lsov1_set */
/* NDIS_OFFLOAD_PARAM_NOCHG */
#define NDIS_OFFLOAD_LSOV1_OFF		1
#define NDIS_OFFLOAD_LSOV1_ON		2

/* ipsecv1_set */
/* NDIS_OFFLOAD_PARAM_NOCHG */
#define NDIS_OFFLOAD_IPSECV1_OFF	1
#define NDIS_OFFLOAD_IPSECV1_AH		2
#define NDIS_OFFLOAD_IPSECV1_ESP	3
#define NDIS_OFFLOAD_IPSECV1_AH_ESP	4

/* lsov2_set */
/* NDIS_OFFLOAD_PARAM_NOCHG */
#define NDIS_OFFLOAD_LSOV2_OFF		1
#define NDIS_OFFLOAD_LSOV2_ON		2

/* ipsecv2_set */
/* NDIS_OFFLOAD_PARAM_NOCHG */
#define NDIS_OFFLOAD_IPSECV2_OFF	1
#define NDIS_OFFLOAD_IPSECV2_AH		2
#define NDIS_OFFLOAD_IPSECV2_ESP	3
#define NDIS_OFFLOAD_IPSECV2_AH_ESP	4

/* rsc_set */
/* NDIS_OFFLOAD_PARAM_NOCHG */
#define NDIS_OFFLOAD_RSC_OFF		1
#define NDIS_OFFLOAD_RSC_ON		2

#endif	/* !_NET_NDIS_H_ */
