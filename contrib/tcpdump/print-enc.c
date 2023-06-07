/*	$OpenBSD: print-enc.c,v 1.7 2002/02/19 19:39:40 millert Exp $	*/

/*
 * Copyright (c) 1990, 1991, 1993, 1994, 1995, 1996
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

/* \summary: OpenBSD IPsec encapsulation BPF layer printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "extract.h"
#include "af.h"

/* From $OpenBSD: if_enc.h,v 1.8 2001/06/25 05:14:00 angelos Exp $ */
/*
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr) and
 * Niels Provos (provos@physnet.uni-hamburg.de).
 *
 * This code was written by John Ioannidis for BSD/OS in Athens, Greece,
 * in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis.
 *
 * Additional transforms and features in 1997 and 1998 by Angelos D. Keromytis
 * and Niels Provos.
 *
 * Copyright (C) 1995, 1996, 1997, 1998 by John Ioannidis, Angelos D. Keromytis
 * and Niels Provos.
 * Copyright (c) 2001, Angelos D. Keromytis.
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 * You may use this code under the GNU public license if you so wish. Please
 * contribute changes back to the authors under this freer than GPL license
 * so that we may further the use of strong encryption without limitations to
 * all.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#define ENC_HDRLEN	12

/* From $OpenBSD: mbuf.h,v 1.56 2002/01/25 15:50:23 art Exp $	*/
#define M_CONF		0x0400  /* packet was encrypted (ESP-transport) */
#define M_AUTH		0x0800  /* packet was authenticated (AH) */

struct enchdr {
	nd_uint32_t af;
	nd_uint32_t spi;
	nd_uint32_t flags;
};

#define ENC_PRINT_TYPE(wh, xf, name) \
	if ((wh) & (xf)) { \
		ND_PRINT("%s%s", name, (wh) == (xf) ? "): " : ","); \
		(wh) &= ~(xf); \
	}

/*
 * Byte-swap a 32-bit number.
 * ("htonl()" or "ntohl()" won't work - we want to byte-swap even on
 * big-endian platforms.)
 */
#define	SWAPLONG(y) \
((((y)&0xff)<<24) | (((y)&0xff00)<<8) | (((y)&0xff0000)>>8) | (((y)>>24)&0xff))

void
enc_if_print(netdissect_options *ndo,
             const struct pcap_pkthdr *h, const u_char *p)
{
	u_int length = h->len;
	u_int af, flags;
	const struct enchdr *hdr;

	ndo->ndo_protocol = "enc";
	ND_TCHECK_LEN(p, ENC_HDRLEN);
	ndo->ndo_ll_hdr_len += ENC_HDRLEN;

	hdr = (const struct enchdr *)p;
	/*
	 * The address family and flags fields are in the byte order
	 * of the host that originally captured the traffic.
	 *
	 * To determine that, look at the address family.  It's 32-bit,
	 * it is not likely ever to be > 65535 (I doubt there will
	 * ever be > 65535 address families and, so far, AF_ values have
	 * not been allocated very sparsely) so it should not have the
	 * upper 16 bits set, and it is not likely ever to be AF_UNSPEC,
	 * i.e. it's not likely ever to be 0, so if it's byte-swapped,
	 * it should have at least one of the upper 16 bits set.
	 *
	 * So if any of the upper 16 bits are set, we assume it, and
	 * the flags field, are byte-swapped.
	 *
	 * The SPI field is always in network byte order, i.e. big-
	 * endian.
	 */
	UNALIGNED_MEMCPY(&af, &hdr->af, sizeof (af));
	UNALIGNED_MEMCPY(&flags, &hdr->flags, sizeof (flags));
	if ((af & 0xFFFF0000) != 0) {
		af = SWAPLONG(af);
		flags = SWAPLONG(flags);
	}

	if (flags == 0)
		ND_PRINT("(unprotected): ");
	else
		ND_PRINT("(");
	ENC_PRINT_TYPE(flags, M_AUTH, "authentic");
	ENC_PRINT_TYPE(flags, M_CONF, "confidential");
	/* ENC_PRINT_TYPE(flags, M_TUNNEL, "tunnel"); */
	ND_PRINT("SPI 0x%08x: ", GET_BE_U_4(hdr->spi));

	length -= ENC_HDRLEN;
	p += ENC_HDRLEN;

	switch (af) {
	case BSD_AFNUM_INET:
		ip_print(ndo, p, length);
		break;
	case BSD_AFNUM_INET6_BSD:
	case BSD_AFNUM_INET6_FREEBSD:
	case BSD_AFNUM_INET6_DARWIN:
		ip6_print(ndo, p, length);
		break;
	}
}
