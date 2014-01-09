/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998
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
 *
 * Name to id translation routines used by the scanner.
 * These functions are not time critical.
 *
 * $FreeBSD$
 */

#include <sys/types.h>

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/libpcap/nametoaddr.c,v 1.83 2008-02-06 10:21:30 guy Exp $ (LBL)";
#endif

/* XXX: bogus defintion */
struct tok {
	int unused;
};

#include "ethertype.h"

struct eproto {
	const char *s;
	u_short p;
};

/* Static data base of ether protocol types. */
struct eproto eproto_db[] = {
#if 0
	/* The FreeBSD elf linker generates a request to copy this array
	 * (including its size) when you link with -lpcap.  In order to
	 * not bump the major version number of this libpcap.so, we need
	 * to ensure that the array stays the same size.  Since PUP is
	 * likely never seen in real life any more, it's the first to
	 * be sacrificed (in favor of ip6).
	 */
	{ "pup", ETHERTYPE_PUP },
#endif
	{ "xns", ETHERTYPE_NS },
	{ "ip", ETHERTYPE_IP },
#ifdef INET6
	{ "ip6", ETHERTYPE_IPV6 },
#endif
	{ "arp", ETHERTYPE_ARP },
	{ "rarp", ETHERTYPE_REVARP },
	{ "sprite", ETHERTYPE_SPRITE },
	{ "mopdl", ETHERTYPE_MOPDL },
	{ "moprc", ETHERTYPE_MOPRC },
	{ "decnet", ETHERTYPE_DN },
	{ "lat", ETHERTYPE_LAT },
	{ "sca", ETHERTYPE_SCA },
	{ "lanbridge", ETHERTYPE_LANBRIDGE },
	{ "vexp", ETHERTYPE_VEXP },
	{ "vprod", ETHERTYPE_VPROD },
	{ "atalk", ETHERTYPE_ATALK },
	{ "atalkarp", ETHERTYPE_AARP },
	{ "loopback", ETHERTYPE_LOOPBACK },
	{ "decdts", ETHERTYPE_DECDTS },
	{ "decdns", ETHERTYPE_DECDNS },
	{ (char *)0, 0 }
};
