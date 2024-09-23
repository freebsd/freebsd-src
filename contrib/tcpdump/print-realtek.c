/*
 * Copyright (c) 2007 - Andrey "nording" Chernyak <andrew@nording.ru>
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
 *
 * Format and print Realtek Remote Control Protocol (RRCP), Realtek
 * Loop Detection Protocol (RLDP), and Realtek Echo Protocol (REP) packets,
 * as well as tag formats used by some Realtek switch chips to supply
 * tag information to a host CPU for a switch.
 */

/* \summary: printer for various Realtek protocols */

#include <config.h>

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"

#define RTL_PROTOCOL_OFFSET	0	/* Protocol and possibly other data - 1 byte */

#define RTL_PROTOCOL_RRCP	0x01	/* RRCP */
#define RTL_PROTOCOL_REP	0x02	/* REP */
#define RTL_PROTOCOL_RLDP	0x03	/* RLDP */
#define RTL_PROTOCOL_RLDP2	0x23	/* also RLDP */
#define RTL_PROTOCOL_XXX_DSA	0x04	/* DSA protocol for some chip(s) */

/*
 * Values for the upper 4 bits of the protocol field, for
 * protocols where the lower 4 bits contain protocol data.
 *
 * See section 8.10 "CPU Tag Function" of
 *
 *    http://realtek.info/pdf/rtl8306sd%28m%29_datasheet_1.1.pdf
 *
 * for the RTL8306 DSA protocol tag format.
 */
#define RTL_PROTOCOL_8306_DSA		0x90	/* RTL8306 DSA protocol */
#define RTL_PROTOCOL_8366RB_DSA		0xA0	/* RTL8366RB DSA protocol */

#define RRCP_OPCODE_ISREPLY_OFFSET	1	/* opcode and isreply flag - 1 byte */

#define RRCP_OPCODE_MASK		0x7F	/* 0x00 = hello, 0x01 = get, 0x02 = set */
#define RRCP_ISREPLY			0x80	/* 0 = request to switch, 0x80 = reply from switch */

#define RRCP_OPCODE_HELLO		0x00
#define RRCP_OPCODE_GET_CONFIGURATION	0x01
#define RRCP_OPCODE_SET_CONFIGURATION	0x02

#define RRCP_AUTHKEY_OFFSET		2	/* authorization key - 2 bytes, 0x2379 by default */

/* most packets */
#define RRCP_REG_ADDR_OFFSET		4	/* register address - 2 bytes */
#define RRCP_REG_DATA_OFFSET		6	/* register data - 4 bytes */
#define RRCP_COOKIE1_OFFSET		10	/* 4 bytes */
#define RRCP_COOKIE2_OFFSET		14	/* 4 bytes */

/* hello reply packets */
#define RRCP_DOWNLINK_PORT_OFFSET	4	/* 1 byte */
#define RRCP_UPLINK_PORT_OFFSET		5	/* 1 byte */
#define RRCP_UPLINK_MAC_OFFSET		6	/* 6 byte MAC address */
#define RRCP_CHIP_ID_OFFSET		12	/* 2 bytes */
#define RRCP_VENDOR_ID_OFFSET		14	/* 4 bytes */

static const struct tok opcode_values[] = {
	{ RRCP_OPCODE_HELLO,             "hello" },
	{ RRCP_OPCODE_GET_CONFIGURATION, "get" },
	{ RRCP_OPCODE_SET_CONFIGURATION, "set" },
	{ 0, NULL }
};

/*
 * Print RRCP packets.
 *
 * See, for example, section 8.20 "Realtek Remote Control Protocol" of
 *
 *    http://realtek.info/pdf/rtl8324.pdf
 *
 * and section 7.22 "Realtek Remote Control Protocol" of
 *
 *    http://realtek.info/pdf/rtl8326.pdf
 *
 * and this page on the OpenRRCP Wiki:
 *
 *    http://openrrcp.org.ru/wiki/rrcp_protocol
 *
 * for information on RRCP.
 */
static void
rrcp_print(netdissect_options *ndo,
	  const u_char *cp)
{
	uint8_t rrcp_opcode;

	ndo->ndo_protocol = "rrcp";
	rrcp_opcode = GET_U_1((cp + RRCP_OPCODE_ISREPLY_OFFSET)) & RRCP_OPCODE_MASK;
	ND_PRINT("RRCP %s: %s",
		((GET_U_1(cp + RRCP_OPCODE_ISREPLY_OFFSET)) & RRCP_ISREPLY) ? "reply" : "query",
		tok2str(opcode_values,"unknown opcode (0x%02x)",rrcp_opcode));
	if (rrcp_opcode==RRCP_OPCODE_GET_CONFIGURATION ||
	    rrcp_opcode==RRCP_OPCODE_SET_CONFIGURATION){
	    ND_PRINT(" addr=0x%04x, data=0x%08x",
		     GET_LE_U_2(cp + RRCP_REG_ADDR_OFFSET),
		     GET_LE_U_4(cp + RRCP_REG_DATA_OFFSET));
	}
	ND_PRINT(", auth=0x%04x",
	         GET_BE_U_2(cp + RRCP_AUTHKEY_OFFSET));
	if (rrcp_opcode==RRCP_OPCODE_HELLO &&
	     ((GET_U_1(cp + RRCP_OPCODE_ISREPLY_OFFSET)) & RRCP_ISREPLY)){
	    ND_PRINT(" downlink_port=%u, uplink_port=%u, uplink_mac=%s, vendor_id=%08x ,chip_id=%04x ",
		     GET_U_1(cp + RRCP_DOWNLINK_PORT_OFFSET),
		     GET_U_1(cp + RRCP_UPLINK_PORT_OFFSET),
		     GET_ETHERADDR_STRING(cp + RRCP_UPLINK_MAC_OFFSET),
		     GET_BE_U_4(cp + RRCP_VENDOR_ID_OFFSET),
		     GET_BE_U_2(cp + RRCP_CHIP_ID_OFFSET));
	}else if (rrcp_opcode==RRCP_OPCODE_GET_CONFIGURATION ||
	          rrcp_opcode==RRCP_OPCODE_SET_CONFIGURATION){
	    ND_PRINT(", cookie=0x%08x%08x ",
		    GET_BE_U_4(cp + RRCP_COOKIE2_OFFSET),
		    GET_BE_U_4(cp + RRCP_COOKIE1_OFFSET));
	}
}

/*
 * Print Realtek packets.
 *
 * See, for example, section 8.22 "Realtek Echo Protocol" of
 *
 *    http://realtek.info/pdf/rtl8324.pdf
 *
 * and section 7.24 "Realtek Echo Protocol" of
 *
 *    http://realtek.info/pdf/rtl8326.pdf
 *
 * for information on REP.
 *
 * See section 8.21 "Network Loop Connection Fault Detection" of
 *
 *    http://realtek.info/pdf/rtl8324.pdf
 *
 * and section 7.23 "Network Loop Connection Fault Detection" of
 *
 *    http://realtek.info/pdf/rtl8326.pdf
 *
 * for information on RLDP.
 *
 * See also section 7.3.8 "Loop Detection" of
 *
 *    http://www.ibselectronics.com/ibsstore/datasheet/RTL8306E-CG.pdf
 *
 * (revision 1.1 of the RTL8306E-CG datasheet), which describes a loop
 * detection protocol for which the payload has a 16-bit (presumably
 * big-endian) field containing the value 0x0300, followed by what is
 * presumably a 16-bit big-endian field the upper 12 bits of which are 0
 * and the lower 4 bits of which are a TTL value, followed by zeroes to
 * pad the packet out to the minimum Ethernet packet size.
 *
 * See also section 7.3.13 "Loop Detection" of
 *
 *    http://realtek.info/pdf/rtl8305sb.pdf
 *
 * (revision 1.3 of the RTL8305SB datasheet), which describes a similar
 * loop detection protocol that lacks the TTL field - all the bytes
 * after 0x0300 are zero.
 *
 * See also section 7.3.7 "Loop Detection" of
 *
 *    https://datasheet.lcsc.com/lcsc/1810221720_Realtek-Semicon-RTL8305NB-CG_C52146.pdf
 *
 * (revision 1.0 of the RTL8305NB-CT datasheet), which describes a loop
 * detection protocol similar to the one from the RTL8306E-CG datasheet,
 * except that the first value is 0x2300, not 0x0300.
 *
 * And, on top of all that, I've seen packets where the first octet of
 * the packet is 0x23, and that's followed by 6 unknown octets (a MAC
 * address of some sort?  It differs from packet to packet in a capture),
 * followed by the MAC address that appears in the source address in the
 * Ethernet header (possibly the originator, in case the packet is forwarded,
 * in which case the forwarded packets won't have the source address from
 * the Ethernet header there), followed by unknown stuff (0x0d followed by
 * zeroes for all such packets in one capture, 0x01 followed by zeroes for
 * all such packets in another capture, 0x07 followed by 0x20's for all
 * such packets in yet another capture).  The OpenRRCP issue at
 * https://github.com/illarionov/OpenRRCP/issues/3 shows a capture
 * similar to the last of those, but with 0x02 instead of 0x07.  Or is that
 * just crap in the buffer in which the chip constructed the packet, left
 * over from something else?
 */
void
rtl_print(netdissect_options *ndo,
	  const u_char *cp,
	  u_int length _U_,
	  const struct lladdr_info *src,
	  const struct lladdr_info *dst)
{
	uint8_t rtl_proto;

	ndo->ndo_protocol = "rtl";

	if (src != NULL && dst != NULL) {
		ND_PRINT("%s > %s, ",
			(src->addr_string)(ndo, src->addr),
			(dst->addr_string)(ndo, dst->addr));
	}

	rtl_proto = GET_U_1(cp + RTL_PROTOCOL_OFFSET);

	if (rtl_proto == RTL_PROTOCOL_RRCP)
		rrcp_print(ndo, cp);
	else if (rtl_proto == RTL_PROTOCOL_REP) {
		/*
		 * REP packets have no payload.
		 */
		ND_PRINT("REP");
	} else if (rtl_proto == RTL_PROTOCOL_RLDP ||
	           rtl_proto == RTL_PROTOCOL_RLDP2) {
		/*
		 * RLDP packets have no payload.
		 * (XXX - except when they do?  See above.)
		 */
		ND_PRINT("RLDP");
	} else if (rtl_proto == RTL_PROTOCOL_XXX_DSA)
		ND_PRINT("Realtek 8-byte DSA tag");
	else if ((rtl_proto & 0xF0) == RTL_PROTOCOL_8306_DSA)
		ND_PRINT("Realtek RTL8306 4-byte DSA tag");
	else if ((rtl_proto & 0xF0) == RTL_PROTOCOL_8366RB_DSA)
		ND_PRINT("Realtek RTL8366RB 4-byte DSA tag");
	else
		ND_PRINT("Realtek unknown type 0x%02x", rtl_proto);
}
