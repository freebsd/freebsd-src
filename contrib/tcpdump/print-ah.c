/*	$NetBSD: print-ah.c,v 1.4 1996/05/20 00:41:16 fvdl Exp $	*/

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

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-ah.c,v 1.14 2000/12/12 09:58:40 itojun Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <stdio.h>

#include "ah.h"

#include "interface.h"
#include "addrtoname.h"

int
ah_print(register const u_char *bp, register const u_char *bp2)
{
	register const struct ah *ah;
	register const u_char *ep;
	int sumlen;
	u_int32_t spi;

	ah = (struct ah *)bp;
	ep = snapend;		/* 'ep' points to the end of available data. */

	if ((u_char *)(ah + 1) >= ep - sizeof(struct ah))
		goto trunc;

	sumlen = ah->ah_len << 2;
	spi = (u_int32_t)ntohl(ah->ah_spi);

	printf("AH(spi=0x%08x", spi);
	if (vflag)
		printf(",sumlen=%d", sumlen);
	printf(",seq=0x%x", (u_int32_t)ntohl(*(u_int32_t *)(ah + 1)));
	if (bp + sizeof(struct ah) + sumlen > ep)
		fputs("[truncated]", stdout);
	fputs("): ", stdout);
	
	return sizeof(struct ah) + sumlen;
 trunc:
	fputs("[|AH]", stdout);
	return 65535;
}
