/* $OpenBSD: deattack.c,v 1.32 2015/01/20 23:14:00 deraadt Exp $ */
/*
 * Cryptographic attack detector for ssh - source code
 *
 * Copyright (c) 1998 CORE SDI S.A., Buenos Aires, Argentina.
 *
 * All rights reserved. Redistribution and use in source and binary
 * forms, with or without modification, are permitted provided that
 * this copyright notice is retained.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES ARE DISCLAIMED. IN NO EVENT SHALL CORE SDI S.A. BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY OR
 * CONSEQUENTIAL DAMAGES RESULTING FROM THE USE OR MISUSE OF THIS
 * SOFTWARE.
 *
 * Ariel Futoransky <futo@core-sdi.com>
 * <http://www.core-sdi.com>
 */

#include "includes.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "deattack.h"
#include "crc32.h"
#include "sshbuf.h"
#include "misc.h"

/*
 * CRC attack detection has a worst-case behaviour that is O(N^3) over
 * the number of identical blocks in a packet. This behaviour can be 
 * exploited to create a limited denial of service attack. 
 * 
 * However, because we are dealing with encrypted data, identical
 * blocks should only occur every 2^35 maximally-sized packets or so. 
 * Consequently, we can detect this DoS by looking for identical blocks
 * in a packet.
 *
 * The parameter below determines how many identical blocks we will
 * accept in a single packet, trading off between attack detection and
 * likelihood of terminating a legitimate connection. A value of 32 
 * corresponds to an average of 2^40 messages before an attack is
 * misdetected
 */
#define MAX_IDENTICAL	32

/* SSH Constants */
#define SSH_MAXBLOCKS	(32 * 1024)
#define SSH_BLOCKSIZE	(8)

/* Hashing constants */
#define HASH_MINSIZE	(8 * 1024)
#define HASH_ENTRYSIZE	(2)
#define HASH_FACTOR(x)	((x)*3/2)
#define HASH_UNUSEDCHAR	(0xff)
#define HASH_UNUSED	(0xffff)
#define HASH_IV		(0xfffe)

#define HASH_MINBLOCKS	(7*SSH_BLOCKSIZE)


/* Hash function (Input keys are cipher results) */
#define HASH(x)		PEEK_U32(x)

#define CMP(a, b)	(memcmp(a, b, SSH_BLOCKSIZE))

static void
crc_update(u_int32_t *a, u_int32_t b)
{
	b ^= *a;
	*a = ssh_crc32((u_char *)&b, sizeof(b));
}

/* detect if a block is used in a particular pattern */
static int
check_crc(const u_char *S, const u_char *buf, u_int32_t len)
{
	u_int32_t crc;
	const u_char *c;

	crc = 0;
	for (c = buf; c < buf + len; c += SSH_BLOCKSIZE) {
		if (!CMP(S, c)) {
			crc_update(&crc, 1);
			crc_update(&crc, 0);
		} else {
			crc_update(&crc, 0);
			crc_update(&crc, 0);
		}
	}
	return crc == 0;
}

void
deattack_init(struct deattack_ctx *dctx)
{
	bzero(dctx, sizeof(*dctx));
	dctx->n = HASH_MINSIZE / HASH_ENTRYSIZE;
}

/* Detect a crc32 compensation attack on a packet */
int
detect_attack(struct deattack_ctx *dctx, const u_char *buf, u_int32_t len)
{
	u_int32_t i, j, l, same;
	u_int16_t *tmp;
	const u_char *c, *d;

	if (len > (SSH_MAXBLOCKS * SSH_BLOCKSIZE) ||
	    len % SSH_BLOCKSIZE != 0)
		return DEATTACK_ERROR;
	for (l = dctx->n; l < HASH_FACTOR(len / SSH_BLOCKSIZE); l = l << 2)
		;

	if (dctx->h == NULL) {
		if ((dctx->h = calloc(l, HASH_ENTRYSIZE)) == NULL)
			return DEATTACK_ERROR;
		dctx->n = l;
	} else {
		if (l > dctx->n) {
			if ((tmp = reallocarray(dctx->h, l, HASH_ENTRYSIZE))
			    == NULL) {
				free(dctx->h);
				dctx->h = NULL;
				return DEATTACK_ERROR;
			}
			dctx->h = tmp;
			dctx->n = l;
		}
	}

	if (len <= HASH_MINBLOCKS) {
		for (c = buf; c < buf + len; c += SSH_BLOCKSIZE) {
			for (d = buf; d < c; d += SSH_BLOCKSIZE) {
				if (!CMP(c, d)) {
					if ((check_crc(c, buf, len)))
						return DEATTACK_DETECTED;
					else
						break;
				}
			}
		}
		return DEATTACK_OK;
	}
	memset(dctx->h, HASH_UNUSEDCHAR, dctx->n * HASH_ENTRYSIZE);

	for (c = buf, same = j = 0; c < (buf + len); c += SSH_BLOCKSIZE, j++) {
		for (i = HASH(c) & (dctx->n - 1); dctx->h[i] != HASH_UNUSED;
		    i = (i + 1) & (dctx->n - 1)) {
			if (!CMP(c, buf + dctx->h[i] * SSH_BLOCKSIZE)) {
				if (++same > MAX_IDENTICAL)
					return DEATTACK_DOS_DETECTED;
				if (check_crc(c, buf, len))
					return DEATTACK_DETECTED;
				else
					break;
			}
		}
		dctx->h[i] = j;
	}
	return DEATTACK_OK;
}
