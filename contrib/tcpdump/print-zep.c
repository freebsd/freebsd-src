/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997
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

/* \summary: ZigBee Encapsulation Protocol (ZEP) printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"

#include "extract.h"

/* From wireshark packet-zep.c:
 *
 ***********************************************************************
 *
 * ZEP Packets must be received in the following format:
 *
 * |UDP Header|  ZEP Header |IEEE 802.15.4 Packet|
 * | 8 bytes  | 16/32 bytes |    <= 127 bytes    |
 *
 ***********************************************************************
 *
 * ZEP v1 Header will have the following format:
 * |Preamble|Version|Channel ID|Device ID|CRC/LQI Mode|LQI Val|Reserved|Length|
 * |2 bytes |1 byte |  1 byte  | 2 bytes |   1 byte   |1 byte |7 bytes |1 byte|
 *
 * ZEP v2 Header will have the following format (if type=1/Data):
 * |Prmbl|Ver  |Type |ChnlID|DevID|C/L Mode|LQI|NTP TS|Seq#|Res |Len|
 * | 2   | 1   | 1   | 1    | 2   | 1      | 1 | 8    | 4  | 10 | 1 |
 *
 * ZEP v2 Header will have the following format (if type=2/Ack):
 * |Preamble|Version| Type |Sequence#|
 * |2 bytes |1 byte |1 byte| 4 bytes |
 *------------------------------------------------------------
 */

#define     JAN_1970        2208988800U

/* Print timestamp */
static void zep_print_ts(netdissect_options *ndo, const u_char *p)
{
	int32_t i;
	uint32_t uf;
	uint32_t f;
	float ff;

	i = GET_BE_U_4(p);
	uf = GET_BE_U_4(p + 4);
	ff = (float) uf;
	if (ff < 0.0)           /* some compilers are buggy */
		ff += FMAXINT;
	ff = (float) (ff / FMAXINT); /* shift radix point by 32 bits */
	f = (uint32_t) (ff * 1000000000.0);  /* treat fraction as parts per
						billion */
	ND_PRINT("%u.%09d", i, f);

	/*
	 * print the time in human-readable format.
	 */
	if (i) {
		time_t seconds = i - JAN_1970;
		char time_buf[128];

		ND_PRINT(" (%s)",
		    nd_format_time(time_buf, sizeof (time_buf), "%Y/%m/%d %H:%M:%S",
		      localtime(&seconds)));
	}
}

/*
 * Main function to print packets.
 */

void
zep_print(netdissect_options *ndo,
	  const u_char *bp, u_int len)
{
	uint8_t version, inner_len;
	uint32_t seq_no;

	ndo->ndo_protocol = "zep";

	nd_print_protocol_caps(ndo);

	/* Preamble Code (must be "EX") */
	if (GET_U_1(bp) != 'E' || GET_U_1(bp + 1) != 'X') {
		ND_PRINT(" [Preamble Code: ");
		fn_print_char(ndo, GET_U_1(bp));
		fn_print_char(ndo, GET_U_1(bp + 1));
		ND_PRINT("]");
		nd_print_invalid(ndo);
		return;
	}

	version = GET_U_1(bp + 2);
	ND_PRINT("v%u ", version);

	if (version == 1) {
		/* ZEP v1 packet. */
		ND_LCHECK_U(len, 16);
		ND_PRINT("Channel ID %u, Device ID 0x%04x, ",
			 GET_U_1(bp + 3), GET_BE_U_2(bp + 4));
		if (GET_U_1(bp + 6))
			ND_PRINT("CRC, ");
		else
			ND_PRINT("LQI %u, ", GET_U_1(bp + 7));
		inner_len = GET_U_1(bp + 15);
		ND_PRINT("inner len = %u", inner_len);

		bp += 16;
		len -= 16;
	} else {
		/* ZEP v2 packet. */
		if (GET_U_1(bp + 3) == 2) {
			/* ZEP v2 ack. */
			ND_LCHECK_U(len, 8);
			seq_no = GET_BE_U_4(bp + 4);
			ND_PRINT("ACK, seq# = %u", seq_no);
			inner_len = 0;
			bp += 8;
			len -= 8;
		} else {
			/* ZEP v2 data, or some other. */
			ND_LCHECK_U(len, 32);
			ND_PRINT("Type %u, Channel ID %u, Device ID 0x%04x, ",
				 GET_U_1(bp + 3), GET_U_1(bp + 4),
				 GET_BE_U_2(bp + 5));
			if (GET_U_1(bp + 7))
				ND_PRINT("CRC, ");
			else
				ND_PRINT("LQI %u, ", GET_U_1(bp + 8));

			zep_print_ts(ndo, bp + 9);
			seq_no = GET_BE_U_4(bp + 17);
			inner_len = GET_U_1(bp + 31);
			ND_PRINT(", seq# = %u, inner len = %u",
				 seq_no, inner_len);
			bp += 32;
			len -= 32;
		}
	}

	if (inner_len != 0) {
		/* Call 802.15.4 dissector. */
		ND_PRINT("\n\t");
		if (ieee802_15_4_print(ndo, bp, inner_len)) {
			ND_TCHECK_LEN(bp, len);
			bp += len;
			len = 0;
		}
	}

	if (!ndo->ndo_suppress_default_print)
		ND_DEFAULTPRINT(bp, len);
	return;
invalid:
	nd_print_invalid(ndo);
}
