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
    "@(#) $Header: /tcpdump/master/tcpdump/print-esp.c,v 1.20 2002/01/21 11:39:59 mcr Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#ifdef HAVE_LIBCRYPTO
#include <openssl/des.h>
#include <openssl/blowfish.h>
#ifdef HAVE_RC5_H
#include <openssl/rc5.h>
#endif
#ifdef HAVE_CAST_H
#include <openssl/cast.h>
#endif
#endif

#include <stdio.h>
#include <unistd.h>

#include "ip.h"
#include "esp.h"
#ifdef INET6
#include "ip6.h"
#endif

#define AVOID_CHURN 1
#include "interface.h"
#include "addrtoname.h"

static struct esp_algorithm *espsecret_xform=NULL;  /* cache of decoded alg. */
static char                 *espsecret_key=NULL;


enum cipher { NONE,
	      DESCBC,
	      BLOWFISH,
	      RC5,
	      CAST128,
	      DES3CBC};



struct esp_algorithm {
	char        *name;
	enum  cipher algo;
	int          ivlen;
	int          authlen;
	int          replaysize;
};

struct esp_algorithm esp_xforms[]={
	{"none",                  NONE,    0,  0, 0},
	{"des-cbc",               DESCBC,  8,  0, 0},
	{"des-cbc-hmac96",        DESCBC,  8, 12, 4},
	{"blowfish-cbc",          BLOWFISH,8,  0, 0},
	{"blowfish-cbc-hmac96",   BLOWFISH,8, 12, 4},
	{"rc5-cbc",               RC5,     8,  0, 0},
	{"rc5-cbc-hmac96",        RC5,     8, 12, 4},
	{"cast128-cbc",           CAST128, 8,  0, 0},
	{"cast128-cbc-hmac96",    CAST128, 8, 12, 4},
	{"3des-cbc-hmac96",       DES3CBC, 8, 12, 4},
};

static int hexdigit(char hex)
{
	if(hex >= '0' && hex <= '9') {
		return (hex - '0');
	} else if(hex >= 'A' && hex <= 'F') {
		return (hex - 'A' + 10);
	} else if(hex >= 'a' && hex <= 'f') {
		return (hex - 'a' + 10);
	} else {
		printf("invalid hex digit %c in espsecret\n", hex);
		return 0;
	}
}

static int hex2byte(char *hexstring)
{
	int byte;

	byte = (hexdigit(hexstring[0]) << 4) +
		hexdigit(hexstring[1]);
	return byte;
}


void esp_print_decodesecret()
{
	char *colon;
	int   len, i;
	struct esp_algorithm *xf;

	if(espsecret == NULL) {
		/* set to NONE transform */
		espsecret_xform = esp_xforms;
		return;
	}

	if(espsecret_key != NULL) {
		return;
	}

	colon = strchr(espsecret, ':');
	if(colon == NULL) {
		printf("failed to decode espsecret: %s\n",
		       espsecret);
		/* set to NONE transform */
		espsecret_xform = esp_xforms;
	}

	len   = colon - espsecret;
	xf = esp_xforms;
	while(xf->name && strncasecmp(espsecret, xf->name, len)!=0) {
		xf++;
	}
	if(xf->name == NULL) {
		printf("failed to find cipher algo %s\n",
		       espsecret);
		espsecret_xform = esp_xforms;
		return;
	}
	espsecret_xform = xf;

	colon++;
	if(colon[0]=='0' && colon[1]=='x') {
		/* decode some hex! */
		colon+=2;
		len = strlen(colon) / 2;
		espsecret_key = (char *)malloc(len);
		if(espsecret_key == NULL) {
		  fprintf(stderr, "%s: ran out of memory (%d) to allocate secret key\n",
			  program_name, len);
		  exit(2);
		}
		i = 0;
		while(colon[0] != '\0' && colon[1]!='\0') {
			espsecret_key[i]=hex2byte(colon);
			colon+=2;
			i++;
		}
	} else {
		espsecret_key = colon;
	}
}

int
esp_print(register const u_char *bp, register const u_char *bp2,
	  int *nhdr, int *padlen)
{
	register const struct esp *esp;
	register const u_char *ep;
	u_int32_t spi;
	struct ip *ip = NULL;
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;
#endif
	int advance;
	int len;
	char *secret;
	int ivlen = 0;
	u_char *ivoff;
	u_char *p;
	
	esp = (struct esp *)bp;
	spi = (u_int32_t)ntohl(esp->esp_spi);
	secret = NULL;

#if 0
	/* keep secret out of a register */
	p = (u_char *)&secret;
#endif

	/* 'ep' points to the end of available data. */
	ep = snapend;

	if ((u_char *)(esp + 1) >= ep - sizeof(struct esp)) {
		fputs("[|ESP]", stdout);
		goto fail;
	}
	printf("ESP(spi=0x%08x", spi);
	printf(",seq=0x%x", (u_int32_t)ntohl(*(u_int32_t *)(esp + 1)));
	printf(")");

	/* if we don't have decryption key, we can't decrypt this packet. */
	if (!espsecret)
		goto fail;

	if(!espsecret_xform) {
		esp_print_decodesecret();
	}
	if(espsecret_xform->algo == NONE) {
		goto fail;
	}

	ip = (struct ip *)bp2;
	switch (IP_V(ip)) {
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
		/* nexthdr & padding are in the last fragment */
		if (ntohs(ip->ip_off) & IP_MF)
			goto fail;
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

	ivoff = (u_char *)(esp + 1) + espsecret_xform->replaysize;
	ivlen = espsecret_xform->ivlen;
	secret = espsecret_key;

	switch (espsecret_xform->algo) {
	case DESCBC:
#ifdef HAVE_LIBCRYPTO
	    {
		u_char iv[8];
		des_key_schedule schedule;

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
#endif /*HAVE_LIBCRYPTO*/

	case BLOWFISH:
#ifdef HAVE_LIBCRYPTO
	    {
		BF_KEY schedule;

		BF_set_key(&schedule, strlen(secret), secret);

		p = ivoff + ivlen;
		BF_cbc_encrypt(p, p, (long)(ep - p), &schedule, ivoff,
			BF_DECRYPT);
		advance = ivoff - (u_char *)esp + ivlen;
		break;
	    }
#else
		goto fail;
#endif /*HAVE_LIBCRYPTO*/

	case RC5:
#if defined(HAVE_LIBCRYPTO) && defined(HAVE_RC5_H)
	    {
		RC5_32_KEY schedule;

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
#endif /*HAVE_LIBCRYPTO*/

	case CAST128:
#if defined(HAVE_LIBCRYPTO) && defined(HAVE_CAST_H) && !defined(HAVE_BUGGY_CAST128)
	    {
		CAST_KEY schedule;

		CAST_set_key(&schedule, strlen(secret), secret);

		p = ivoff + ivlen;
		CAST_cbc_encrypt(p, p, (long)(ep - p), &schedule, ivoff,
			CAST_DECRYPT);
		advance = ivoff - (u_char *)esp + ivlen;
		break;
	    }
#else
		goto fail;
#endif /*HAVE_LIBCRYPTO*/

	case DES3CBC:
#if defined(HAVE_LIBCRYPTO)
	    {
		des_key_schedule s1, s2, s3;

		des_check_key = 1;
		des_set_odd_parity((void *)secret);
		des_set_odd_parity((void *)secret+8);
		des_set_odd_parity((void *)secret+16);
		if(des_set_key((void *)secret, s1) != 0) {
		  printf("failed to schedule key 1\n");
		}
		if(des_set_key((void *)(secret + 8), s2)!=0) {
		  printf("failed to schedule key 2\n");
		}
		if(des_set_key((void *)(secret + 16), s3)!=0) {
		  printf("failed to schedule key 3\n");
		}

		p = ivoff + ivlen;
		des_ede3_cbc_encrypt((void *)p, (void *)p,
				     (long)(ep - p),
				     s1, s2, s3,
				     (void *)ivoff, DES_DECRYPT);
		advance = ivoff - (u_char *)esp + ivlen;
		break;
	    }
#else
		goto fail;
#endif /*HAVE_LIBCRYPTO*/

	case NONE:
	default:
		advance = sizeof(struct esp) + espsecret_xform->replaysize;
		break;
	}

	ep = ep - espsecret_xform->authlen;
	/* sanity check for pad length */
	if (ep - bp < *(ep - 2))
		goto fail;

	if (padlen)
		*padlen = *(ep - 2) + 2;

	if (nhdr)
		*nhdr = *(ep - 1);

	printf(": ");
	return advance;

fail:
	if (nhdr)
		*nhdr = -1;
	return 65536;
}
