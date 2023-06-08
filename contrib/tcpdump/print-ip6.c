/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994
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

/* \summary: IPv6 printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include <string.h>

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"

#include "ip6.h"
#include "ipproto.h"

/*
 * If routing headers are presend and valid, set dst to the final destination.
 * Otherwise, set it to the IPv6 destination.
 *
 * This is used for UDP and TCP pseudo-header in the checksum
 * calculation.
 */
static void
ip6_finddst(netdissect_options *ndo, nd_ipv6 *dst,
            const struct ip6_hdr *ip6)
{
	const u_char *cp;
	u_int advance;
	u_int nh;
	const void *dst_addr;
	const struct ip6_rthdr *dp;
	const struct ip6_rthdr0 *dp0;
	const struct ip6_srh *srh;
	const u_char *p;
	int i, len;

	cp = (const u_char *)ip6;
	advance = sizeof(struct ip6_hdr);
	nh = GET_U_1(ip6->ip6_nxt);
	dst_addr = (const void *)ip6->ip6_dst;

	while (cp < ndo->ndo_snapend) {
		cp += advance;

		switch (nh) {

		case IPPROTO_HOPOPTS:
		case IPPROTO_DSTOPTS:
		case IPPROTO_MOBILITY_OLD:
		case IPPROTO_MOBILITY:
			/*
			 * These have a header length byte, following
			 * the next header byte, giving the length of
			 * the header, in units of 8 octets, excluding
			 * the first 8 octets.
			 */
			advance = (GET_U_1(cp + 1) + 1) << 3;
			nh = GET_U_1(cp);
			break;

		case IPPROTO_FRAGMENT:
			/*
			 * The byte following the next header byte is
			 * marked as reserved, and the header is always
			 * the same size.
			 */
			advance = sizeof(struct ip6_frag);
			nh = GET_U_1(cp);
			break;

		case IPPROTO_ROUTING:
			/*
			 * OK, we found it.
			 */
			dp = (const struct ip6_rthdr *)cp;
			ND_TCHECK_SIZE(dp);
			len = GET_U_1(dp->ip6r_len);
			switch (GET_U_1(dp->ip6r_type)) {

			case IPV6_RTHDR_TYPE_0:
			case IPV6_RTHDR_TYPE_2:		/* Mobile IPv6 ID-20 */
				dp0 = (const struct ip6_rthdr0 *)dp;
				if (len % 2 == 1)
					goto trunc;
				len >>= 1;
				p = (const u_char *) dp0->ip6r0_addr;
				for (i = 0; i < len; i++) {
					ND_TCHECK_16(p);
					dst_addr = (const void *)p;
					p += 16;
				}
				break;
			case IPV6_RTHDR_TYPE_4:
				/* IPv6 Segment Routing Header (SRH) */
				srh = (const struct ip6_srh *)dp;
				if (len % 2 == 1)
					goto trunc;
				p = (const u_char *) srh->srh_segments;
				/*
				 * The list of segments are encoded in the reverse order.
				 * Accordingly, the final DA is encoded in srh_segments[0]
				 */
				ND_TCHECK_16(p);
				dst_addr = (const void *)p;
				break;

			default:
				break;
			}

			/*
			 * Only one routing header to a customer.
			 */
			goto done;

		case IPPROTO_AH:
		case IPPROTO_ESP:
		case IPPROTO_IPCOMP:
		default:
			/*
			 * AH and ESP are, in the RFCs that describe them,
			 * described as being "viewed as an end-to-end
			 * payload" "in the IPv6 context, so that they
			 * "should appear after hop-by-hop, routing, and
			 * fragmentation extension headers".  We assume
			 * that's the case, and stop as soon as we see
			 * one.  (We can't handle an ESP header in
			 * the general case anyway, as its length depends
			 * on the encryption algorithm.)
			 *
			 * IPComp is also "viewed as an end-to-end
			 * payload" "in the IPv6 context".
			 *
			 * All other protocols are assumed to be the final
			 * protocol.
			 */
			goto done;
		}
	}

done:
trunc:
	GET_CPY_BYTES(dst, dst_addr, sizeof(nd_ipv6));
}

/*
 * Compute a V6-style checksum by building a pseudoheader.
 */
uint16_t
nextproto6_cksum(netdissect_options *ndo,
                 const struct ip6_hdr *ip6, const uint8_t *data,
		 u_int len, u_int covlen, uint8_t next_proto)
{
        struct {
                nd_ipv6 ph_src;
                nd_ipv6 ph_dst;
                uint32_t       ph_len;
                uint8_t        ph_zero[3];
                uint8_t        ph_nxt;
        } ph;
        struct cksum_vec vec[2];
        u_int nh;

        /* pseudo-header */
        memset(&ph, 0, sizeof(ph));
        GET_CPY_BYTES(&ph.ph_src, ip6->ip6_src, sizeof(nd_ipv6));
        nh = GET_U_1(ip6->ip6_nxt);
        switch (nh) {

        case IPPROTO_HOPOPTS:
        case IPPROTO_DSTOPTS:
        case IPPROTO_MOBILITY_OLD:
        case IPPROTO_MOBILITY:
        case IPPROTO_FRAGMENT:
        case IPPROTO_ROUTING:
                /*
                 * The next header is either a routing header or a header
                 * after which there might be a routing header, so scan
                 * for a routing header.
                 */
                ip6_finddst(ndo, &ph.ph_dst, ip6);
                break;

        default:
                GET_CPY_BYTES(&ph.ph_dst, ip6->ip6_dst, sizeof(nd_ipv6));
                break;
        }
        ph.ph_len = htonl(len);
        ph.ph_nxt = next_proto;

        vec[0].ptr = (const uint8_t *)(void *)&ph;
        vec[0].len = sizeof(ph);
        vec[1].ptr = data;
        vec[1].len = covlen;

        return in_cksum(vec, 2);
}

/*
 * print an IP6 datagram.
 */
void
ip6_print(netdissect_options *ndo, const u_char *bp, u_int length)
{
	const struct ip6_hdr *ip6;
	int advance;
	u_int len;
	u_int total_advance;
	const u_char *cp;
	uint32_t payload_len;
	uint8_t ph, nh;
	int fragmented = 0;
	u_int flow;
	int found_extension_header;
	int found_jumbo;
	int found_hbh;

	ndo->ndo_protocol = "ip6";
	ip6 = (const struct ip6_hdr *)bp;

	ND_TCHECK_SIZE(ip6);
	if (length < sizeof (struct ip6_hdr)) {
		ND_PRINT("truncated-ip6 %u", length);
		return;
	}

	if (!ndo->ndo_eflag)
	    ND_PRINT("IP6 ");

	if (IP6_VERSION(ip6) != 6) {
	  ND_PRINT("version error: %u != 6", IP6_VERSION(ip6));
	  return;
	}

	payload_len = GET_BE_U_2(ip6->ip6_plen);
	/*
	 * RFC 1883 says:
	 *
	 * The Payload Length field in the IPv6 header must be set to zero
	 * in every packet that carries the Jumbo Payload option.  If a
	 * packet is received with a valid Jumbo Payload option present and
	 * a non-zero IPv6 Payload Length field, an ICMP Parameter Problem
	 * message, Code 0, should be sent to the packet's source, pointing
	 * to the Option Type field of the Jumbo Payload option.
	 *
	 * Later versions of the IPv6 spec don't discuss the Jumbo Payload
	 * option.
	 *
	 * If the payload length is 0, we temporarily just set the total
	 * length to the remaining data in the packet (which, for Ethernet,
	 * could include frame padding, but if it's a Jumbo Payload frame,
	 * it shouldn't even be sendable over Ethernet, so we don't worry
	 * about that), so we can process the extension headers in order
	 * to *find* a Jumbo Payload hop-by-hop option and, when we've
	 * processed all the extension headers, check whether we found
	 * a Jumbo Payload option, and fail if we haven't.
	 */
	if (payload_len != 0) {
		len = payload_len + sizeof(struct ip6_hdr);
		if (length < len)
			ND_PRINT("truncated-ip6 - %u bytes missing!",
				len - length);
	} else
		len = length + sizeof(struct ip6_hdr);

	ph = 255;
	nh = GET_U_1(ip6->ip6_nxt);
	if (ndo->ndo_vflag) {
	    flow = GET_BE_U_4(ip6->ip6_flow);
	    ND_PRINT("(");
	    /* RFC 2460 */
	    if (flow & 0x0ff00000)
	        ND_PRINT("class 0x%02x, ", (flow & 0x0ff00000) >> 20);
	    if (flow & 0x000fffff)
	        ND_PRINT("flowlabel 0x%05x, ", flow & 0x000fffff);

	    ND_PRINT("hlim %u, next-header %s (%u) payload length: %u) ",
	                 GET_U_1(ip6->ip6_hlim),
	                 tok2str(ipproto_values,"unknown",nh),
	                 nh,
	                 payload_len);
	}

	/*
	 * Cut off the snapshot length to the end of the IP payload.
	 */
	if (!nd_push_snaplen(ndo, bp, len)) {
		(*ndo->ndo_error)(ndo, S_ERR_ND_MEM_ALLOC,
			"%s: can't push snaplen on buffer stack", __func__);
	}

	cp = (const u_char *)ip6;
	advance = sizeof(struct ip6_hdr);
	total_advance = 0;
	/* Process extension headers */
	found_extension_header = 0;
	found_jumbo = 0;
	found_hbh = 0;
	while (cp < ndo->ndo_snapend && advance > 0) {
		if (len < (u_int)advance)
			goto trunc;
		cp += advance;
		len -= advance;
		total_advance += advance;

		if (cp == (const u_char *)(ip6 + 1) &&
		    nh != IPPROTO_TCP && nh != IPPROTO_UDP &&
		    nh != IPPROTO_DCCP && nh != IPPROTO_SCTP) {
			ND_PRINT("%s > %s: ", GET_IP6ADDR_STRING(ip6->ip6_src),
				     GET_IP6ADDR_STRING(ip6->ip6_dst));
		}

		switch (nh) {

		case IPPROTO_HOPOPTS:
			/*
			 * The Hop-by-Hop Options header, when present,
			 * must immediately follow the IPv6 header (RFC 8200)
			 */
			if (found_hbh == 1) {
				ND_PRINT("[The Hop-by-Hop Options header was already found]");
				nd_print_invalid(ndo);
				return;
			}
			if (ph != 255) {
				ND_PRINT("[The Hop-by-Hop Options header don't follow the IPv6 header]");
				nd_print_invalid(ndo);
				return;
			}
			advance = hbhopt_process(ndo, cp, &found_jumbo, &payload_len);
			if (payload_len == 0 && found_jumbo == 0) {
				ND_PRINT("[No valid Jumbo Payload Hop-by-Hop option found]");
				nd_print_invalid(ndo);
				return;
			}
			if (advance < 0) {
				nd_pop_packet_info(ndo);
				return;
			}
			found_extension_header = 1;
			found_hbh = 1;
			nh = GET_U_1(cp);
			break;

		case IPPROTO_DSTOPTS:
			advance = dstopt_process(ndo, cp);
			if (advance < 0) {
				nd_pop_packet_info(ndo);
				return;
			}
			found_extension_header = 1;
			nh = GET_U_1(cp);
			break;

		case IPPROTO_FRAGMENT:
			advance = frag6_print(ndo, cp, (const u_char *)ip6);
			if (advance < 0 || ndo->ndo_snapend <= cp + advance) {
				nd_pop_packet_info(ndo);
				return;
			}
			found_extension_header = 1;
			nh = GET_U_1(cp);
			fragmented = 1;
			break;

		case IPPROTO_MOBILITY_OLD:
		case IPPROTO_MOBILITY:
			/*
			 * XXX - we don't use "advance"; RFC 3775 says that
			 * the next header field in a mobility header
			 * should be IPPROTO_NONE, but speaks of
			 * the possibility of a future extension in
			 * which payload can be piggybacked atop a
			 * mobility header.
			 */
			advance = mobility_print(ndo, cp, (const u_char *)ip6);
			if (advance < 0) {
				nd_pop_packet_info(ndo);
				return;
			}
			found_extension_header = 1;
			nh = GET_U_1(cp);
			nd_pop_packet_info(ndo);
			return;

		case IPPROTO_ROUTING:
			ND_TCHECK_1(cp);
			advance = rt6_print(ndo, cp, (const u_char *)ip6);
			if (advance < 0) {
				nd_pop_packet_info(ndo);
				return;
			}
			found_extension_header = 1;
			nh = GET_U_1(cp);
			break;

		default:
			/*
			 * Not an extension header; hand off to the
			 * IP protocol demuxer.
			 */
			if (found_jumbo) {
				/*
				 * We saw a Jumbo Payload option.
				 * Set the length to the payload length
				 * plus the IPv6 header length, and
				 * change the snapshot length accordingly.
				 *
				 * But make sure it's not shorter than
				 * the total number of bytes we've
				 * processed so far.
				 */
				len = payload_len + sizeof(struct ip6_hdr);
				if (len < total_advance)
					goto trunc;
				if (length < len)
					ND_PRINT("truncated-ip6 - %u bytes missing!",
						len - length);
				nd_change_snaplen(ndo, bp, len);

				/*
				 * Now subtract the length of the IPv6
				 * header plus extension headers to get
				 * the payload length.
				 */
				len -= total_advance;
			} else {
				/*
				 * We didn't see a Jumbo Payload option;
				 * was the payload length zero?
				 */
				if (payload_len == 0) {
					/*
					 * Yes.  If we found an extension
					 * header, treat that as a truncated
					 * packet header, as there was
					 * no payload to contain an
					 * extension header.
					 */
					if (found_extension_header)
						goto trunc;

					/*
					 * OK, we didn't see any extension
					 * header, but that means we have
					 * no payload, so set the length
					 * to the IPv6 header length,
					 * and change the snapshot length
					 * accordingly.
					 */
					len = sizeof(struct ip6_hdr);
					nd_change_snaplen(ndo, bp, len);

					/*
					 * Now subtract the length of
					 * the IPv6 header plus extension
					 * headers (there weren't any, so
					 * that's just the IPv6 header
					 * length) to get the payload length.
					 */
					len -= total_advance;
				}
			}
			ip_demux_print(ndo, cp, len, 6, fragmented,
				       GET_U_1(ip6->ip6_hlim), nh, bp);
			nd_pop_packet_info(ndo);
			return;
		}
		ph = nh;

		/* ndo_protocol reassignment after xxx_print() calls */
		ndo->ndo_protocol = "ip6";
	}

	nd_pop_packet_info(ndo);
	return;
trunc:
	nd_print_trunc(ndo);
}
