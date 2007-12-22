/*	$Id: acss.h,v 1.2 2004/02/06 04:22:43 dtucker Exp $ */
/*
 * Copyright (c) 2004 The OpenBSD project
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _ACSS_H_
#define _ACSS_H_

/* 40bit key */
#define ACSS_KEYSIZE		5

/* modes of acss */
#define ACSS_AUTHENTICATE	0
#define ACSS_SESSIONKEY		1
#define ACSS_TITLEKEY		2
#define ACSS_DATA		3

typedef struct acss_key_st {
	unsigned int	lfsr17;		/* current state of lfsrs */
	unsigned int	lfsr25;
	unsigned int	lfsrsum;
	unsigned char	seed[ACSS_KEYSIZE];
	unsigned char	data[ACSS_KEYSIZE];
	unsigned char	subkey[ACSS_KEYSIZE];
	int		encrypt;	/* XXX make these bit flags? */
	int		mode;
	int		seeded;
	int		subkey_avilable;
} ACSS_KEY;

void acss_setkey(ACSS_KEY *, const unsigned char *, int, int);
void acss_setsubkey(ACSS_KEY *, const unsigned char *);
int acss(ACSS_KEY *, unsigned long, const unsigned char *, unsigned char *);

#endif /* ifndef _ACSS_H_ */
