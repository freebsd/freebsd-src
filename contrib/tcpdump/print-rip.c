/*
 * Copyright (c) 1989, 1990, 1991, 1993, 1994, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* \summary: Routing Information Protocol (RIP) printer */

/* specification: RFC 1058, RFC 2453, RFC 4822 */

#include <config.h>

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"

#include "af.h"


/*
 * RFC 1058 and RFC 2453 header of packet.
 *
 *  0                   1                   2                   3 3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Command (1)   | Version (1)   |           unused              |
 * +---------------+---------------+-------------------------------+
 */
struct rip {
	nd_uint8_t rip_cmd;		/* request/response */
	nd_uint8_t rip_vers;		/* protocol version # */
	nd_byte    unused[2];		/* unused */
};

#define	RIPCMD_REQUEST		1	/* want info */
#define	RIPCMD_RESPONSE		2	/* responding to request */
#define	RIPCMD_TRACEON		3	/* turn tracing on */
#define	RIPCMD_TRACEOFF		4	/* turn it off */
/* 5 is reserved */
#define RIPCMD_TRIGREQ		6
#define RIPCMD_TRIGRESP		7
#define RIPCMD_TRIGACK		8
#define RIPCMD_UPDREQ		9
#define RIPCMD_UPDRESP		10
#define RIPCMD_UPDACK		11

static const struct tok rip_cmd_values[] = {
    { RIPCMD_REQUEST,	        "Request" },
    { RIPCMD_RESPONSE,	        "Response" },
    { RIPCMD_TRACEON,	        "Trace on" },
    { RIPCMD_TRACEOFF,	        "Trace off" },
    { RIPCMD_TRIGREQ,	        "Triggered Request" },
    { RIPCMD_TRIGRESP,	        "Triggered Response" },
    { RIPCMD_TRIGACK,	        "Triggered Acknowledgement" },
    { RIPCMD_UPDREQ,	        "Update Request" },
    { RIPCMD_UPDRESP,	        "Update Response" },
    { RIPCMD_UPDACK,	        "Update Acknowledge" },
    { 0, NULL}
};

#define RIP_AUTHLEN  16
#define RIP_ROUTELEN 20

/*
 * First 4 bytes of all RIPv1/RIPv2 entries.
 */
struct rip_entry_header {
	nd_uint16_t rip_family;
	nd_uint16_t rip_tag;
};

/*
 * RFC 1058 entry.
 *
 *  0                   1                   2                   3 3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Address Family Identifier (2) |       must be zero (2)        |
 * +-------------------------------+-------------------------------+
 * |                         IP Address (4)                        |
 * +---------------------------------------------------------------+
 * |                        must be zero (4)                       |
 * +---------------------------------------------------------------+
 * |                        must be zero (4)                       |
 * +---------------------------------------------------------------+
 * |                         Metric (4)                            |
 * +---------------------------------------------------------------+
 */
struct rip_netinfo_v1 {
	nd_uint16_t rip_family;
	nd_byte     rip_mbz1[2];
	nd_ipv4     rip_dest;
	nd_byte     rip_mbz2[4];
	nd_byte     rip_mbz3[4];
	nd_uint32_t rip_metric;		/* cost of route */
};


/*
 * RFC 2453 route entry
 *
 *  0                   1                   2                   3 3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Address Family Identifier (2) |        Route Tag (2)          |
 * +-------------------------------+-------------------------------+
 * |                         IP Address (4)                        |
 * +---------------------------------------------------------------+
 * |                         Subnet Mask (4)                       |
 * +---------------------------------------------------------------+
 * |                         Next Hop (4)                          |
 * +---------------------------------------------------------------+
 * |                         Metric (4)                            |
 * +---------------------------------------------------------------+
 *
 */

struct rip_netinfo_v2 {
	nd_uint16_t rip_family;
	nd_uint16_t rip_tag;
	nd_ipv4     rip_dest;
	nd_uint32_t rip_dest_mask;
	nd_ipv4     rip_router;
	nd_uint32_t rip_metric;		/* cost of route */
};

/*
 * RFC 2453 authentication entry
 *
 *  0                   1                   2                   3 3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |            0xFFFF             |    Authentication Type (2)    |
 * +-------------------------------+-------------------------------+
 * -                      Authentication (16)                      -
 * +---------------------------------------------------------------+
 */

struct rip_auth_v2 {
	nd_uint16_t rip_family;
	nd_uint16_t rip_tag;
	nd_byte     rip_auth[16];
};

/*
 * RFC 4822 Cryptographic Authentication entry.
 *
 *  0                   1                   2                   3 3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |     RIPv2 Packet Length       |   Key ID      | Auth Data Len |
 * +---------------+---------------+---------------+---------------+
 * |               Sequence Number (non-decreasing)                |
 * +---------------+---------------+---------------+---------------+
 * |                      reserved must be zero                    |
 * +---------------+---------------+---------------+---------------+
 * |                      reserved must be zero                    |
 * +---------------+---------------+---------------+---------------+
 */
struct rip_auth_crypto_v2 {
	nd_uint16_t rip_packet_len;
	nd_uint8_t  rip_key_id;
	nd_uint8_t  rip_auth_data_len;
	nd_uint32_t rip_seq_num;
	nd_byte     rip_mbz1[4];
	nd_byte     rip_mbz2[4];
};

static unsigned
rip_entry_print_v1(netdissect_options *ndo, const u_char *p,
		   unsigned remaining)
{
	const struct rip_entry_header *eh = (const struct rip_entry_header *)p;
	u_short family;
	const struct rip_netinfo_v1 *ni = (const struct rip_netinfo_v1 *)p;

	/* RFC 1058 */
	if (remaining < RIP_ROUTELEN)
		return (0);
	ND_TCHECK_SIZE(ni);
	family = GET_BE_U_2(ni->rip_family);
	if (family != BSD_AFNUM_INET && family != 0) {
		ND_PRINT("\n\t AFI %s, ", tok2str(bsd_af_values, "Unknown (%u)", family));
		print_unknown_data(ndo, p + sizeof(*eh), "\n\t  ", RIP_ROUTELEN - sizeof(*eh));
		return (RIP_ROUTELEN);
	}
	if (GET_BE_U_2(ni->rip_mbz1) ||
	    GET_BE_U_4(ni->rip_mbz2) ||
	    GET_BE_U_4(ni->rip_mbz3)) {
		/* MBZ fields not zero */
		print_unknown_data(ndo, p, "\n\t  ", RIP_ROUTELEN);
		return (RIP_ROUTELEN);
	}
	if (family == 0) {
		ND_PRINT("\n\t  AFI 0, %s, metric: %u",
			 GET_IPADDR_STRING(ni->rip_dest),
			 GET_BE_U_4(ni->rip_metric));
		return (RIP_ROUTELEN);
	} /* BSD_AFNUM_INET */
	ND_PRINT("\n\t  %s, metric: %u",
		 GET_IPADDR_STRING(ni->rip_dest),
		 GET_BE_U_4(ni->rip_metric));
	return (RIP_ROUTELEN);
trunc:
	return 0;
}

static unsigned
rip_entry_print_v2(netdissect_options *ndo, const u_char *p,
		   unsigned remaining)
{
	const struct rip_entry_header *eh = (const struct rip_entry_header *)p;
	u_short family;
	const struct rip_netinfo_v2 *ni;

	if (remaining < sizeof(*eh))
		return (0);
	ND_TCHECK_SIZE(eh);
	family = GET_BE_U_2(eh->rip_family);
	if (family == 0xFFFF) { /* variable-sized authentication structures */
		uint16_t auth_type = GET_BE_U_2(eh->rip_tag);

		p += sizeof(*eh);
		remaining -= sizeof(*eh);
		if (auth_type == 2) {
			ND_PRINT("\n\t  Simple Text Authentication data: ");
			nd_printjnp(ndo, p, RIP_AUTHLEN);
		} else if (auth_type == 3) {
			const struct rip_auth_crypto_v2 *ch;

			ch = (const struct rip_auth_crypto_v2 *)p;
			ND_TCHECK_SIZE(ch);
			if (remaining < sizeof(*ch))
				return (0);
			ND_PRINT("\n\t  Auth header:");
			ND_PRINT(" Packet Len %u,",
				 GET_BE_U_2(ch->rip_packet_len));
			ND_PRINT(" Key-ID %u,", GET_U_1(ch->rip_key_id));
			ND_PRINT(" Auth Data Len %u,",
				 GET_U_1(ch->rip_auth_data_len));
			ND_PRINT(" SeqNo %u,", GET_BE_U_4(ch->rip_seq_num));
			ND_PRINT(" MBZ %u,", GET_BE_U_4(ch->rip_mbz1));
			ND_PRINT(" MBZ %u", GET_BE_U_4(ch->rip_mbz2));
		} else if (auth_type == 1) {
			ND_PRINT("\n\t  Auth trailer:");
			print_unknown_data(ndo, p, "\n\t  ", remaining);
			return (sizeof(*eh) + remaining); /* AT spans till the packet end */
		} else {
			ND_PRINT("\n\t  Unknown (%u) Authentication data:",
				 auth_type);
			print_unknown_data(ndo, p, "\n\t  ", remaining);
			return (sizeof(*eh) + remaining); /* we don't know how long this is, so we go to the packet end */
		}
	} else if (family != BSD_AFNUM_INET && family != 0) {
		ND_PRINT("\n\t  AFI %s", tok2str(bsd_af_values, "Unknown (%u)", family));
		print_unknown_data(ndo, p + sizeof(*eh), "\n\t  ", RIP_ROUTELEN - sizeof(*eh));
	} else { /* BSD_AFNUM_INET or AFI 0 */
		ni = (const struct rip_netinfo_v2 *)p;
		ND_TCHECK_SIZE(ni);
		if (remaining < sizeof(*ni))
			return (0);
		ND_PRINT("\n\t  AFI %s, %15s/%-2d, tag 0x%04x, metric: %u, next-hop: ",
			 tok2str(bsd_af_values, "%u", family),
			 GET_IPADDR_STRING(ni->rip_dest),
			 mask2plen(GET_BE_U_4(ni->rip_dest_mask)),
			 GET_BE_U_2(ni->rip_tag),
			 GET_BE_U_4(ni->rip_metric));
		if (GET_BE_U_4(ni->rip_router))
			ND_PRINT("%s", GET_IPADDR_STRING(ni->rip_router));
		else
			ND_PRINT("self");
	}
	return (RIP_ROUTELEN);
trunc:
	return 0;
}

void
rip_print(netdissect_options *ndo,
	  const u_char *dat, u_int length)
{
	const struct rip *rp;
	uint8_t vers, cmd;
	const u_char *p;
	u_int len, routecount;
	unsigned entry_size;

	ndo->ndo_protocol = "rip";
	if (ndo->ndo_snapend < dat) {
		nd_print_trunc(ndo);
		return;
	}
	len = ND_BYTES_AVAILABLE_AFTER(dat);
	if (len > length)
		len = length;
	if (len < sizeof(*rp)) {
		nd_print_trunc(ndo);
		return;
	}
	len -= sizeof(*rp);

	rp = (const struct rip *)dat;

	ND_TCHECK_SIZE(rp);
	vers = GET_U_1(rp->rip_vers);
	ND_PRINT("%sRIPv%u",
		 (ndo->ndo_vflag >= 1) ? "\n\t" : "",
		 vers);

	/* dump version and lets see if we know the commands name*/
	cmd = GET_U_1(rp->rip_cmd);
	ND_PRINT(", %s, length: %u",
		tok2str(rip_cmd_values, "unknown command (%u)", cmd),
		length);

	if (ndo->ndo_vflag < 1)
		return;

	switch (cmd) {

	case RIPCMD_REQUEST:
	case RIPCMD_RESPONSE:
		switch (vers) {

		case 1:
			routecount = length / RIP_ROUTELEN;
			ND_PRINT(", routes: %u", routecount);
			p = (const u_char *)(rp + 1);
			while (len != 0) {
				entry_size = rip_entry_print_v1(ndo, p, len);
				if (entry_size == 0) {
					/* Error */
					nd_print_trunc(ndo);
					break;
				}
				if (len < entry_size) {
					ND_PRINT(" [remaining entries length %u < %u]",
						 len, entry_size);
					nd_print_invalid(ndo);
					break;
				}
				p += entry_size;
				len -= entry_size;
			}
			break;

		case 2:
			routecount = length / RIP_ROUTELEN;
			ND_PRINT(", routes: %u or less", routecount);
			p = (const u_char *)(rp + 1);
			while (len != 0) {
				entry_size = rip_entry_print_v2(ndo, p, len);
				if (entry_size == 0) {
					/* Error */
					nd_print_trunc(ndo);
					break;
				}
				if (len < entry_size) {
					ND_PRINT(" [remaining entries length %u < %u]",
						 len, entry_size);
					nd_print_invalid(ndo);
					break;
				}
				p += entry_size;
				len -= entry_size;
			}
			break;

		default:
			ND_PRINT(", unknown version");
			break;
		}
		break;

	case RIPCMD_TRACEON:
	case RIPCMD_TRACEOFF:
	case RIPCMD_TRIGREQ:
	case RIPCMD_TRIGRESP:
	case RIPCMD_TRIGACK:
	case RIPCMD_UPDREQ:
	case RIPCMD_UPDRESP:
	case RIPCMD_UPDACK:
		break;

	default:
		if (ndo->ndo_vflag <= 1) {
			if (!print_unknown_data(ndo, (const uint8_t *)rp, "\n\t", length))
				return;
		}
		break;
	}
	/* do we want to see an additionally hexdump ? */
	if (ndo->ndo_vflag> 1) {
		if (!print_unknown_data(ndo, (const uint8_t *)rp, "\n\t", length))
			return;
	}
trunc:
	return;
}
