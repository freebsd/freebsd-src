/*
 * Copyright (c) 2013, Petar Alilovic,
 * Faculty of Electrical Engineering and Computing, University of Zagreb
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *	 this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *	 notice, this list of conditions and the following disclaimer in the
 *	 documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* \summary: DLT_NFLOG printer */

#include <config.h>

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "extract.h"

#ifdef DLT_NFLOG

/*
 * Structure of an NFLOG header and TLV parts, as described at
 * https://www.tcpdump.org/linktypes/LINKTYPE_NFLOG.html
 *
 * The NFLOG header is big-endian.
 *
 * The TLV length and type are in host byte order.  The value is either
 * big-endian or is an array of bytes in some externally-specified byte
 * order (text string, link-layer address, link-layer header, packet
 * data, etc.).
 */
typedef struct nflog_hdr {
	nd_uint8_t	nflog_family;		/* address family */
	nd_uint8_t	nflog_version;		/* version */
	nd_uint16_t	nflog_rid;		/* resource ID */
} nflog_hdr_t;

#define NFLOG_HDR_LEN sizeof(nflog_hdr_t)

typedef struct nflog_tlv {
	nd_uint16_t	tlv_length;		/* tlv length */
	nd_uint16_t	tlv_type;		/* tlv type */
	/* value follows this */
} nflog_tlv_t;

#define NFLOG_TLV_LEN sizeof(nflog_tlv_t)

typedef struct nflog_packet_hdr {
	nd_uint16_t	hw_protocol;	/* hw protocol */
	nd_uint8_t	hook;		/* netfilter hook */
	nd_byte		pad[1];		/* padding to 32 bits */
} nflog_packet_hdr_t;

typedef struct nflog_hwaddr {
	nd_uint16_t	hw_addrlen;	/* address length */
	nd_byte		pad[2];		/* padding to 32-bit boundary */
	nd_byte		hw_addr[8];	/* address, up to 8 bytes */
} nflog_hwaddr_t;

typedef struct nflog_timestamp {
	nd_uint64_t	sec;
	nd_uint64_t	usec;
} nflog_timestamp_t;

/*
 * TLV types.
 */
#define NFULA_PACKET_HDR		1	/* nflog_packet_hdr_t */
#define NFULA_MARK			2	/* packet mark from skbuff */
#define NFULA_TIMESTAMP			3	/* nflog_timestamp_t for skbuff's time stamp */
#define NFULA_IFINDEX_INDEV		4	/* ifindex of device on which packet received (possibly bridge group) */
#define NFULA_IFINDEX_OUTDEV		5	/* ifindex of device on which packet transmitted (possibly bridge group) */
#define NFULA_IFINDEX_PHYSINDEV		6	/* ifindex of physical device on which packet received (not bridge group) */
#define NFULA_IFINDEX_PHYSOUTDEV	7	/* ifindex of physical device on which packet transmitted (not bridge group) */
#define NFULA_HWADDR			8	/* nflog_hwaddr_t for hardware address */
#define NFULA_PAYLOAD			9	/* packet payload */
#define NFULA_PREFIX			10	/* text string - null-terminated, count includes NUL */
#define NFULA_UID			11	/* UID owning socket on which packet was sent/received */
#define NFULA_SEQ			12	/* sequence number of packets on this NFLOG socket */
#define NFULA_SEQ_GLOBAL		13	/* sequence number of packets on all NFLOG sockets */
#define NFULA_GID			14	/* GID owning socket on which packet was sent/received */
#define NFULA_HWTYPE			15	/* ARPHRD_ type of skbuff's device */
#define NFULA_HWHEADER			16	/* skbuff's MAC-layer header */
#define NFULA_HWLEN			17	/* length of skbuff's MAC-layer header */

/*
 * Define two constants specifically for the two AF code points from the
 * LINKTYPE_NFLOG specification above and use these constants instead of
 * AF_INET and AF_INET6.  This is the only way to dissect the "wire" encoding
 * correctly because some BSD systems define AF_INET6 differently from Linux
 * (see af.h) and Haiku defines both AF_INET and AF_INET6 differently from
 * Linux.
 */
#define NFLOG_AF_INET   2
#define NFLOG_AF_INET6 10
static const struct tok nflog_values[] = {
	{ NFLOG_AF_INET,	"IPv4" },
	{ NFLOG_AF_INET6,	"IPv6" },
	{ 0,			NULL }
};

static void
nflog_hdr_print(netdissect_options *ndo, const nflog_hdr_t *hdr, u_int length)
{
	ND_PRINT("version %u, resource ID %u",
	    GET_U_1(hdr->nflog_version), GET_BE_U_2(hdr->nflog_rid));

	if (!ndo->ndo_qflag) {
		ND_PRINT(", family %s (%u)",
			 tok2str(nflog_values, "Unknown",
				 GET_U_1(hdr->nflog_family)),
			 GET_U_1(hdr->nflog_family));
		} else {
		ND_PRINT(", %s",
			 tok2str(nflog_values,
				 "Unknown NFLOG (0x%02x)",
			 GET_U_1(hdr->nflog_family)));
		}

	ND_PRINT(", length %u: ", length);
}

void
nflog_if_print(netdissect_options *ndo,
	       const struct pcap_pkthdr *h, const u_char *p)
{
	const nflog_hdr_t *hdr = (const nflog_hdr_t *)p;
	uint16_t size;
	uint16_t h_size = NFLOG_HDR_LEN;
	u_int caplen = h->caplen;
	u_int length = h->len;

	ndo->ndo_protocol = "nflog";
	if (caplen < NFLOG_HDR_LEN) {
		nd_print_trunc(ndo);
		ndo->ndo_ll_hdr_len += caplen;
		return;
	}
	ndo->ndo_ll_hdr_len += NFLOG_HDR_LEN;

	ND_TCHECK_SIZE(hdr);
	if (GET_U_1(hdr->nflog_version) != 0) {
		ND_PRINT("version %u (unknown)", GET_U_1(hdr->nflog_version));
		return;
	}

	if (ndo->ndo_eflag)
		nflog_hdr_print(ndo, hdr, length);

	p += NFLOG_HDR_LEN;
	length -= NFLOG_HDR_LEN;
	caplen -= NFLOG_HDR_LEN;

	while (length > 0) {
		const nflog_tlv_t *tlv;

		/* We have some data.  Do we have enough for the TLV header? */
		if (caplen < NFLOG_TLV_LEN)
			goto trunc;	/* No. */

		tlv = (const nflog_tlv_t *) p;
		ND_TCHECK_SIZE(tlv);
		size = GET_HE_U_2(tlv->tlv_length);
		if (size % 4 != 0)
			size += 4 - size % 4;

		/* Is the TLV's length less than the minimum? */
		if (size < NFLOG_TLV_LEN)
			goto trunc;	/* Yes. Give up now. */

		/* Do we have enough data for the full TLV? */
		if (caplen < size)
			goto trunc;	/* No. */

		if (GET_HE_U_2(tlv->tlv_type) == NFULA_PAYLOAD) {
			/*
			 * This TLV's data is the packet payload.
			 * Skip past the TLV header, and break out
			 * of the loop so we print the packet data.
			 */
			p += NFLOG_TLV_LEN;
			h_size += NFLOG_TLV_LEN;
			length -= NFLOG_TLV_LEN;
			caplen -= NFLOG_TLV_LEN;
			break;
		}

		p += size;
		h_size += size;
		length -= size;
		caplen -= size;
	}

	switch (GET_U_1(hdr->nflog_family)) {

	case NFLOG_AF_INET:
		ip_print(ndo, p, length);
		break;

	case NFLOG_AF_INET6:
		ip6_print(ndo, p, length);
		break;

	default:
		if (!ndo->ndo_eflag)
			nflog_hdr_print(ndo, hdr,
					length + NFLOG_HDR_LEN);

		if (!ndo->ndo_suppress_default_print)
			ND_DEFAULTPRINT(p, caplen);
		break;
	}

	ndo->ndo_ll_hdr_len += h_size - NFLOG_HDR_LEN;
	return;
trunc:
	nd_print_trunc(ndo);
	ndo->ndo_ll_hdr_len += h_size - NFLOG_HDR_LEN;
}

#endif /* DLT_NFLOG */
