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
RCSID("$OpenBSD: deattack.c,v 1.18 2002/03/04 17:27:39 stevesk Exp $");

#include "deattack.h"
#include "log.h"
#include "crc32.h"
#include "getput.h"
#include "xmalloc.h"
#include "deattack.h"

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
#define HASH(x)		GET_32BIT(x)

#define CMP(a, b)	(memcmp(a, b, SSH_BLOCKSIZE))

static void
crc_update(u_int32_t *a, u_int32_t b)
{
	b ^= *a;
	*a = ssh_crc32((u_char *) &b, sizeof(b));
}

/* detect if a block is used in a particular pattern */
static int
check_crc(u_char *S, u_char *buf, u_int32_t len,
	  u_char *IV)
{
	u_int32_t crc;
	u_char *c;

	crc = 0;
	if (IV && !CMP(S, IV)) {
		crc_update(&crc, 1);
		crc_update(&crc, 0);
	}
	for (c = buf; c < buf + len; c += SSH_BLOCKSIZE) {
		if (!CMP(S, c)) {
			crc_update(&crc, 1);
			crc_update(&crc, 0);
		} else {
			crc_update(&crc, 0);
			crc_update(&crc, 0);
		}
	}
	return (crc == 0);
}


/* Detect a crc32 compensation attack on a packet */
int
detect_attack(u_char *buf, u_int32_t len, u_char *IV)
{
	static u_int16_t *h = (u_int16_t *) NULL;
	static u_int32_t n = HASH_MINSIZE / HASH_ENTRYSIZE;
	u_int32_t i, j;
	u_int32_t l;
	u_char *c;
	u_char *d;

	if (len > (SSH_MAXBLOCKS * SSH_BLOCKSIZE) ||
	    len % SSH_BLOCKSIZE != 0) {
		fatal("detect_attack: bad length %d", len);
	}
	for (l = n; l < HASH_FACTOR(len / SSH_BLOCKSIZE); l = l << 2)
		;

	if (h == NULL) {
		debug("Installing crc compensation attack detector.");
		n = l;
		h = (u_int16_t *) xmalloc(n * HASH_ENTRYSIZE);
	} else {
		if (l > n) {
			n = l;
			h = (u_int16_t *) xrealloc(h, n * HASH_ENTRYSIZE);
		}
	}

	if (len <= HASH_MINBLOCKS) {
		for (c = buf; c < buf + len; c += SSH_BLOCKSIZE) {
			if (IV && (!CMP(c, IV))) {
				if ((check_crc(c, buf, len, IV)))
					return (DEATTACK_DETECTED);
				else
					break;
			}
			for (d = buf; d < c; d += SSH_BLOCKSIZE) {
				if (!CMP(c, d)) {
					if ((check_crc(c, buf, len, IV)))
						return (DEATTACK_DETECTED);
					else
						break;
				}
			}
		}
		return (DEATTACK_OK);
	}
	memset(h, HASH_UNUSEDCHAR, n * HASH_ENTRYSIZE);

	if (IV)
		h[HASH(IV) & (n - 1)] = HASH_IV;

	for (c = buf, j = 0; c < (buf + len); c += SSH_BLOCKSIZE, j++) {
		for (i = HASH(c) & (n - 1); h[i] != HASH_UNUSED;
		    i = (i + 1) & (n - 1)) {
			if (h[i] == HASH_IV) {
				if (!CMP(c, IV)) {
					if (check_crc(c, buf, len, IV))
						return (DEATTACK_DETECTED);
					else
						break;
				}
			} else if (!CMP(c, buf + h[i] * SSH_BLOCKSIZE)) {
				if (check_crc(c, buf, len, IV))
					return (DEATTACK_DETECTED);
				else
					break;
			}
		}
		h[i] = j;
	}
	return (DEATTACK_OK);
}
