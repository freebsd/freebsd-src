/*
 * Copyright (c) 2016 Gerard Garcia <nouboh@gmail.com>
 * Copyright (c) 2017 Red Hat, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *   3. The names of the authors may not be used to endorse or promote
 *      products derived from this software without specific prior
 *      written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* \summary: Linux vsock printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"
#include <stddef.h>

#include "netdissect.h"
#include "extract.h"

enum af_vsockmon_transport {
	AF_VSOCK_TRANSPORT_UNKNOWN = 0,
	AF_VSOCK_TRANSPORT_NO_INFO = 1,		/* No transport information */
	AF_VSOCK_TRANSPORT_VIRTIO = 2,		/* Virtio transport header */
};

static const struct tok vsock_transport[] = {
	{AF_VSOCK_TRANSPORT_UNKNOWN, "UNKNOWN"},
	{AF_VSOCK_TRANSPORT_NO_INFO, "NO_INFO"},
	{AF_VSOCK_TRANSPORT_VIRTIO, "VIRTIO"},
	{ 0, NULL }
};

enum af_vsockmon_op {
	AF_VSOCK_OP_UNKNOWN = 0,
	AF_VSOCK_OP_CONNECT = 1,
	AF_VSOCK_OP_DISCONNECT = 2,
	AF_VSOCK_OP_CONTROL = 3,
	AF_VSOCK_OP_PAYLOAD = 4,
};

static const struct tok vsock_op[] = {
	{AF_VSOCK_OP_UNKNOWN, "UNKNOWN"},
	{AF_VSOCK_OP_CONNECT, "CONNECT"},
	{AF_VSOCK_OP_DISCONNECT, "DISCONNECT"},
	{AF_VSOCK_OP_CONTROL, "CONTROL"},
	{AF_VSOCK_OP_PAYLOAD, "PAYLOAD"},
	{ 0, NULL }
};

enum virtio_vsock_type {
	VIRTIO_VSOCK_TYPE_STREAM = 1,
};

static const struct tok virtio_type[] = {
	{VIRTIO_VSOCK_TYPE_STREAM, "STREAM"},
	{ 0, NULL }
};

enum virtio_vsock_op {
	VIRTIO_VSOCK_OP_INVALID = 0,
	VIRTIO_VSOCK_OP_REQUEST = 1,
	VIRTIO_VSOCK_OP_RESPONSE = 2,
	VIRTIO_VSOCK_OP_RST = 3,
	VIRTIO_VSOCK_OP_SHUTDOWN = 4,
	VIRTIO_VSOCK_OP_RW = 5,
	VIRTIO_VSOCK_OP_CREDIT_UPDATE = 6,
	VIRTIO_VSOCK_OP_CREDIT_REQUEST = 7,
};

static const struct tok virtio_op[] = {
	{VIRTIO_VSOCK_OP_INVALID, "INVALID"},
	{VIRTIO_VSOCK_OP_REQUEST, "REQUEST"},
	{VIRTIO_VSOCK_OP_RESPONSE, "RESPONSE"},
	{VIRTIO_VSOCK_OP_RST, "RST"},
	{VIRTIO_VSOCK_OP_SHUTDOWN, "SHUTDOWN"},
	{VIRTIO_VSOCK_OP_RW, "RW"},
	{VIRTIO_VSOCK_OP_CREDIT_UPDATE, "CREDIT UPDATE"},
	{VIRTIO_VSOCK_OP_CREDIT_REQUEST, "CREDIT REQUEST"},
	{ 0, NULL }
};

/* All fields are little-endian */

struct virtio_vsock_hdr {
	nd_uint64_t	src_cid;
	nd_uint64_t	dst_cid;
	nd_uint32_t	src_port;
	nd_uint32_t	dst_port;
	nd_uint32_t	len;
	nd_uint16_t	type;		/* enum virtio_vsock_type */
	nd_uint16_t	op;		/* enum virtio_vsock_op */
	nd_uint32_t	flags;
	nd_uint32_t	buf_alloc;
	nd_uint32_t	fwd_cnt;
};

struct af_vsockmon_hdr {
	nd_uint64_t	src_cid;
	nd_uint64_t	dst_cid;
	nd_uint32_t	src_port;
	nd_uint32_t	dst_port;
	nd_uint16_t	op;		/* enum af_vsockmon_op */
	nd_uint16_t	transport;	/* enum af_vosckmon_transport */
	nd_uint16_t	len;		/* size of transport header */
	nd_uint8_t	reserved[2];
};

static void
vsock_virtio_hdr_print(netdissect_options *ndo, const struct virtio_vsock_hdr *hdr)
{
	uint16_t u16_v;
	uint32_t u32_v;

	u32_v = GET_LE_U_4(hdr->len);
	ND_PRINT("len %u", u32_v);

	u16_v = GET_LE_U_2(hdr->type);
	ND_PRINT(", type %s",
		 tok2str(virtio_type, "Invalid type (%hu)", u16_v));

	u16_v = GET_LE_U_2(hdr->op);
	ND_PRINT(", op %s",
		 tok2str(virtio_op, "Invalid op (%hu)", u16_v));

	u32_v = GET_LE_U_4(hdr->flags);
	ND_PRINT(", flags %x", u32_v);

	u32_v = GET_LE_U_4(hdr->buf_alloc);
	ND_PRINT(", buf_alloc %u", u32_v);

	u32_v = GET_LE_U_4(hdr->fwd_cnt);
	ND_PRINT(", fwd_cnt %u", u32_v);
}

/*
 * This size had better fit in a u_int.
 */
static u_int
vsock_transport_hdr_size(uint16_t transport)
{
	switch (transport) {
		case AF_VSOCK_TRANSPORT_VIRTIO:
			return (u_int)sizeof(struct virtio_vsock_hdr);
		default:
			return 0;
	}
}

/* Returns 0 on success, -1 on truncation */
static int
vsock_transport_hdr_print(netdissect_options *ndo, uint16_t transport,
                          const u_char *p, const u_int caplen)
{
	u_int transport_size = vsock_transport_hdr_size(transport);
	const void *hdr;

	if (caplen < sizeof(struct af_vsockmon_hdr) + transport_size) {
		return -1;
	}

	hdr = p + sizeof(struct af_vsockmon_hdr);
	switch (transport) {
		case AF_VSOCK_TRANSPORT_VIRTIO:
			ND_PRINT(" (");
			vsock_virtio_hdr_print(ndo, hdr);
			ND_PRINT(")");
			break;
		default:
			break;
	}
	return 0;
}

static void
vsock_hdr_print(netdissect_options *ndo, const u_char *p, const u_int caplen)
{
	const struct af_vsockmon_hdr *hdr = (const struct af_vsockmon_hdr *)p;
	uint16_t hdr_transport, hdr_op;
	uint32_t hdr_src_port, hdr_dst_port;
	uint64_t hdr_src_cid, hdr_dst_cid;
	u_int total_hdr_size;
	int ret = 0;

	hdr_transport = GET_LE_U_2(hdr->transport);
	ND_PRINT("%s",
		 tok2str(vsock_transport, "Invalid transport (%u)",
			  hdr_transport));

	/* If verbose level is more than 0 print transport details */
	if (ndo->ndo_vflag) {
		ret = vsock_transport_hdr_print(ndo, hdr_transport, p, caplen);
		if (ret == 0)
			ND_PRINT("\n\t");
	} else
		ND_PRINT(" ");

	hdr_src_cid = GET_LE_U_8(hdr->src_cid);
	hdr_dst_cid = GET_LE_U_8(hdr->dst_cid);
	hdr_src_port = GET_LE_U_4(hdr->src_port);
	hdr_dst_port = GET_LE_U_4(hdr->dst_port);
	hdr_op = GET_LE_U_2(hdr->op);
	ND_PRINT("%" PRIu64 ".%u > %" PRIu64 ".%u %s, length %u",
		 hdr_src_cid, hdr_src_port,
		 hdr_dst_cid, hdr_dst_port,
		 tok2str(vsock_op, " invalid op (%u)", hdr_op),
		 caplen);

	if (ret < 0)
		goto trunc;

	/* If debug level is more than 1 print payload contents */
	/* This size had better fit in a u_int */
	total_hdr_size = (u_int)sizeof(struct af_vsockmon_hdr) +
			 vsock_transport_hdr_size(hdr_transport);
	if (ndo->ndo_vflag > 1 && hdr_op == AF_VSOCK_OP_PAYLOAD) {
		if (caplen > total_hdr_size) {
			const u_char *payload = p + total_hdr_size;

			ND_PRINT("\n");
			print_unknown_data(ndo, payload, "\t",
					   caplen - total_hdr_size);
		} else
			goto trunc;
	}
	return;

trunc:
	nd_print_trunc(ndo);
}

void
vsock_if_print(netdissect_options *ndo, const struct pcap_pkthdr *h,
	       const u_char *cp)
{
	u_int caplen = h->caplen;

	ndo->ndo_protocol = "vsock";

	if (caplen < sizeof(struct af_vsockmon_hdr)) {
		nd_print_trunc(ndo);
		ndo->ndo_ll_hdr_len += caplen;
		return;
	}
	ndo->ndo_ll_hdr_len += sizeof(struct af_vsockmon_hdr);
	vsock_hdr_print(ndo, cp, caplen);
}
