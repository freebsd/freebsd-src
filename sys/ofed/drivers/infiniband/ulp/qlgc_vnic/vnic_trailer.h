/*
 * Copyright (c) 2006 QLogic, Inc.  All rights reserved.
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
 */

#ifndef VNIC_TRAILER_H_INCLUDED
#define VNIC_TRAILER_H_INCLUDED

/* pkt_flags values */
enum {
	PF_CHASH_VALID		= 0x01,
	PF_IPSEC_VALID		= 0x02,
	PF_TCP_SEGMENT		= 0x04,
	PF_KICK			= 0x08,
	PF_VLAN_INSERT		= 0x10,
	PF_PVID_OVERRIDDEN 	= 0x20,
	PF_FCS_INCLUDED 	= 0x40,
	PF_FORCE_ROUTE		= 0x80
};

/* tx_chksum_flags values */
enum {
	TX_CHKSUM_FLAGS_CHECKSUM_V4	= 0x01,
	TX_CHKSUM_FLAGS_CHECKSUM_V6	= 0x02,
	TX_CHKSUM_FLAGS_TCP_CHECKSUM	= 0x04,
	TX_CHKSUM_FLAGS_UDP_CHECKSUM	= 0x08,
	TX_CHKSUM_FLAGS_IP_CHECKSUM	= 0x10
};

/* rx_chksum_flags values */
enum {
	RX_CHKSUM_FLAGS_TCP_CHECKSUM_FAILED	= 0x01,
	RX_CHKSUM_FLAGS_UDP_CHECKSUM_FAILED	= 0x02,
	RX_CHKSUM_FLAGS_IP_CHECKSUM_FAILED	= 0x04,
	RX_CHKSUM_FLAGS_TCP_CHECKSUM_SUCCEEDED	= 0x08,
	RX_CHKSUM_FLAGS_UDP_CHECKSUM_SUCCEEDED	= 0x10,
	RX_CHKSUM_FLAGS_IP_CHECKSUM_SUCCEEDED	= 0x20,
	RX_CHKSUM_FLAGS_LOOPBACK		= 0x40,
	RX_CHKSUM_FLAGS_RESERVED		= 0x80
};

/* connection_hash_and_valid values */
enum {
	CHV_VALID	= 0x80,
	CHV_HASH_MASH	= 0x7f
};

struct viport_trailer {
	s8	data_alignment_offset;
	u8	rndis_header_length;	/* reserved for use by edp */
	__be16	data_length;
	u8	pkt_flags;
	u8	tx_chksum_flags;
	u8	rx_chksum_flags;
	u8	ip_sec_flags;
	u32	tcp_seq_no;
	u32	ip_sec_offload_handle;
	u32	ip_sec_next_offload_handle;
	u8	dest_mac_addr[6];
	__be16	vlan;
	u16	time_stamp;
	u8	origin;
	u8	connection_hash_and_valid;
};

#define VIPORT_TRAILER_ALIGNMENT	32

#define BUFFER_SIZE(len)					\
	(sizeof(struct viport_trailer) +			\
	 ALIGN((len), VIPORT_TRAILER_ALIGNMENT))

#define MAX_PAYLOAD(len)					\
	ALIGN_DOWN((len) - sizeof(struct viport_trailer),	\
		   VIPORT_TRAILER_ALIGNMENT)

#endif	/* VNIC_TRAILER_H_INCLUDED */
