/*
 * Copyright (c) 1990, 1991, 1993, 1994, 1995, 1996, 1997
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-vjc.c,v 1.9 2000/10/09 01:53:21 guy Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/time.h>

#include <netinet/in.h>

#include <ctype.h>
#include <netdb.h>
#include <pcap.h>
#include <stdio.h>

#include "interface.h"
#include "addrtoname.h"

#include "slcompress.h"
#include "ppp.h"

int
vjc_print(register const char *bp, register u_int length, u_short proto)
{
	int i;

	switch (bp[0] & 0xf0) {
	case TYPE_IP:
		if (eflag)
			printf("(vjc type=IP) ");
		return PPP_IP;
	case TYPE_UNCOMPRESSED_TCP:
		if (eflag)
			printf("(vjc type=raw TCP) ");
		return PPP_IP;
	case TYPE_COMPRESSED_TCP:
		if (eflag)
			printf("(vjc type=compressed TCP) ");
		for (i = 0; i < 8; i++) {
			if (bp[1] & (0x80 >> i))
				printf("%c", "?CI?SAWU"[i]);
		}
		if (bp[1])
			printf(" ");
		printf("C=0x%02x ", bp[2]);
		printf("sum=0x%04x ", *(u_short *)&bp[3]);
		return -1;
	case TYPE_ERROR:
		if (eflag)
			printf("(vjc type=error) ");
		return -1;
	default:
		if (eflag)
			printf("(vjc type=0x%02x) ", bp[0] & 0xf0);
		return -1;
	}
}
