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
    "@(#) $Header: /tcpdump/master/tcpdump/print-esp.c,v 1.5 1999/12/15 08:10:18 fenner Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <net/route.h>
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>

#ifdef CRYPTO
#include <des.h>
#include <blowfish.h>
#ifdef HAVE_RC5_H
#include <rc5.h>
#endif
#ifdef HAVE_CAST_H
#include <cast.h>
#endif
#endif

#include <stdio.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif

/* there's no standard definition so we are on our own */
struct esp {
	u_int32_t	esp_spi;	/* ESP */
	/*variable size, 32bit bound*/	/* Initialization Vector */
	/*variable size*/		/* Payload data */
	/*variable size*/		/* padding */
	/*8bit*/			/* pad size */
	/*8bit*/			/* next header */
	/*8bit*/			/* next header */
	/*variable size, 32bit bound*/	/* Authentication data (new IPsec) */
};

struct newesp {
	u_int32_t	esp_spi;	/* ESP */
	u_int32_t	esp_seq;	/* Sequence number */
	/*variable size*/		/* (IV and) Payload data */
	/*variable size*/		/* padding */
	/*8bit*/			/* pad size */
	/*8bit*/			/* next header */
	/*8bit*/			/* next header */
	/*variable size, 32bit bound*/	/* Authentication data */
};

#include "interface.h"
#include "addrtoname.h"

int
esp_print(register const u_char *bp, register const u_char *bp2, int *nhdr)
{
	register const struct esp *esp;
	register const u_char *ep;
	u_int32_t spi;
	enum { NONE, DESCBC, BLOWFISH, RC5, CAST128, DES3CBC } algo = NONE;
	struct ip *ip = NULL;
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;
#endif
	int advance;
	int len;
	char *secret = NULL;
	int ivlen = 0;
	u_char *ivoff;

	esp = (struct esp *)bp;
	spi = (u_int32_t)ntohl(esp->esp_spi);

	/* 'ep' points to the end of avaible data. */
	ep = snapend;

	if ((u_char *)(esp + 1) >= ep - sizeof(struct esp)) {
		fputs("[|ESP]", stdout);
		goto fail;
	}
	printf("ESP(spi=%u", spi);
	printf(",seq=0x%x", (u_int32_t)ntohl(*(u_int32_t *)(esp + 1)));
	printf(")");

	/* if we don't have decryption key, we can't decrypt this packet. */
	if (!espsecret)
		goto fail;

	if (strncmp(espsecret, "des-cbc:", 8) == 0
	 && strlen(espsecret + 8) == 8) {
		algo = DESCBC;
		ivlen = 8;
		secret = espsecret + 8;
	} else if (strncmp(espsecret, "blowfish-cbc:", 13) == 0) {
		algo = BLOWFISH;
		ivlen = 8;
		secret = espsecret + 13;
	} else if (strncmp(espsecret, "rc5-cbc:", 8) == 0) {
		algo = RC5;
		ivlen = 8;
		secret = espsecret + 8;
	} else if (strncmp(espsecret, "cast128-cbc:", 12) == 0) {
		algo = CAST128;
		ivlen = 8;
		secret = espsecret + 12;
	} else if (strncmp(espsecret, "3des-cbc:", 9) == 0
		&& strlen(espsecret + 9) == 24) {
		algo = DES3CBC;
		ivlen = 8;
		secret = espsecret + 9;
	} else if (strncmp(espsecret, "none:", 5) == 0) {
		algo = NONE;
		ivlen = 0;
		secret = espsecret + 5;
	} else if (strlen(espsecret) == 8) {
		algo = DESCBC;
		ivlen = 8;
		secret = espsecret;
	} else {
		algo = NONE;
		ivlen = 0;
		secret = espsecret;
	}

	ip = (struct ip *)bp2;
	switch (ip->ip_v) {
#ifdef INET6
	case 6:
		ip6 = (struct ip6_hdr *)bp2;
		ip = NULL;
		/* we do not attempt to decrypt jumbograms */
		if (!ntohs(ip6->ip6_plen))
			goto fail;
		/* if we can't get nexthdr, we do not need to decrypt it */
		len = sizeof(struct ip6_hdr) + ntohs(ip6->ip6_plen);
		break;
#endif /*INET6*/
	case 4:
#ifdef INET6
		ip6 = NULL;
#endif
		len = ntohs(ip->ip_len);
		break;
	default:
		goto fail;
	}

	/* if we can't get nexthdr, we do not need to decrypt it */
	if (ep - bp2 < len)
		goto fail;

	if (Rflag)
		ivoff = (u_char *)(esp + 1) + sizeof(u_int32_t);
	else
		ivoff = (u_char *)(esp + 1);

	switch (algo) {
	case DESCBC:
#ifdef CRYPTO
	    {
		u_char iv[8];
		des_key_schedule schedule;
		u_char *p;

		switch (ivlen) {
		case 4:
			memcpy(iv, ivoff, 4);
			memcpy(&iv[4], ivoff, 4);
			p = &iv[4];
			*p++ ^= 0xff;
			*p++ ^= 0xff;
			*p++ ^= 0xff;
			*p++ ^= 0xff;
			break;
		case 8:
			memcpy(iv, ivoff, 8);
			break;
		default:
			goto fail;
		}

		des_check_key = 0;
		des_set_key((void *)secret, schedule);

		p = ivoff + ivlen;
		des_cbc_encrypt((void *)p, (void *)p,
			(long)(ep - p), schedule, (void *)iv,
			DES_DECRYPT);
		advance = ivoff - (u_char *)esp + ivlen;
		break;
	    }
#else
		goto fail;
#endif /*CRYPTO*/

	case BLOWFISH:
#ifdef CRYPTO
	    {
		BF_KEY schedule;
		u_char *p;

		BF_set_key(&schedule, strlen(secret), secret);

		p = ivoff + ivlen;
		BF_cbc_encrypt(p, p, (long)(ep - p), &schedule, ivoff,
			BF_DECRYPT);
		advance = ivoff - (u_char *)esp + ivlen;
		break;
	    }
#else
		goto fail;
#endif /*CRYPTO*/

	case RC5:
#if defined(CRYPTO) && defined(HAVE_RC5_H)
	    {
		RC5_32_KEY schedule;
		u_char *p;

		RC5_32_set_key(&schedule, strlen(secret), secret,
			RC5_16_ROUNDS);

		p = ivoff + ivlen;
		RC5_32_cbc_encrypt(p, p, (long)(ep - p), &schedule, ivoff,
			RC5_DECRYPT);
		advance = ivoff - (u_char *)esp + ivlen;
		break;
	    }
#else
		goto fail;
#endif /*CRYPTO*/

	case CAST128:
#if defined(CRYPTO) && defined(HAVE_CAST_H) && !defined(HAVE_BUGGY_CAST128)
	    {
		CAST_KEY schedule;
		u_char *p;

		CAST_set_key(&schedule, strlen(secret), secret);

		p = ivoff + ivlen;
		CAST_cbc_encrypt(p, p, (long)(ep - p), &schedule, ivoff,
			CAST_DECRYPT);
		advance = ivoff - (u_char *)esp + ivlen;
		break;
	    }
#else
		goto fail;
#endif /*CRYPTO*/

	case DES3CBC:
#if defined(CRYPTO)
	    {
		des_key_schedule s1, s2, s3;
		u_char *p;

		des_check_key = 0;
		des_set_key((void *)secret, s1);
		des_set_key((void *)(secret + 8), s2);
		des_set_key((void *)(secret + 16), s3);

		p = ivoff + ivlen;
		des_ede3_cbc_encrypt((void *)p, (void *)p,
			(long)(ep - p), s1, s2, s3, (void *)ivoff, DES_DECRYPT);
		advance = ivoff - (u_char *)esp + ivlen;
		break;
	    }
#else
		goto fail;
#endif /*CRYPTO*/

	case NONE:
	default:
		if (Rflag)
			advance = sizeof(struct esp) + sizeof(u_int32_t);
		else
			advance = sizeof(struct esp);
		break;
	}

	/* sanity check for pad length */
	if (ep - bp < *(ep - 2))
		goto fail;

	if (nhdr)
		*nhdr = *(ep - 1);

	printf(": ");
	return advance;

fail:
	if (nhdr)
		*nhdr = -1;
	return 65536;
}
