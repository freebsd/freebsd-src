/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Margo Seltzer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)hash_func.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>

#include <db.h>
#include "hash.h"
#include "page.h"
#include "extern.h"

static int hash1 __P((u_char *, int));
static int hash2 __P((u_char *, int));
static int hash3 __P((u_char *, int));
static int hash4 __P((u_char *, int));

/* Global default hash function */
int (*__default_hash) __P((u_char *, int)) = hash4;

/******************************* HASH FUNCTIONS **************************/
/*
 * Assume that we've already split the bucket to which this key hashes,
 * calculate that bucket, and check that in fact we did already split it.
 *
 * This came from ejb's hsearch.
 */

#define PRIME1		37
#define PRIME2		1048583

static int
hash1(key, len)
	register u_char *key;
	register int len;
{
	register int h;

	h = 0;
	/* Convert string to integer */
	while (len--)
		h = h * PRIME1 ^ (*key++ - ' ');
	h %= PRIME2;
	return (h);
}

/*
 * Phong's linear congruential hash
 */
#define dcharhash(h, c)	((h) = 0x63c63cd9*(h) + 0x9c39c33d + (c))

static int
hash2(key, len)
	register u_char *key;
	int len;
{
	register u_char *e, c;
	register int h;

	e = key + len;
	for (h = 0; key != e;) {
		c = *key++;
		if (!c && key > e)
			break;
		dcharhash(h, c);
	}
	return (h);
}

/*
 * This is INCREDIBLY ugly, but fast.  We break the string up into 8 byte
 * units.  On the first time through the loop we get the "leftover bytes"
 * (strlen % 8).  On every other iteration, we perform 8 HASHC's so we handle
 * all 8 bytes.  Essentially, this saves us 7 cmp & branch instructions.  If
 * this routine is heavily used enough, it's worth the ugly coding.
 *
 * OZ's original sdbm hash
 */
static int
hash3(key, len)
	register u_char *key;
	register int len;
{
	register int n, loop;

#define HASHC   n = *key++ + 65599 * n

	n = 0;
	if (len > 0) {
		loop = (len + 8 - 1) >> 3;

		switch (len & (8 - 1)) {
		case 0:
			do {	/* All fall throughs */
				HASHC;
		case 7:
				HASHC;
		case 6:
				HASHC;
		case 5:
				HASHC;
		case 4:
				HASHC;
		case 3:
				HASHC;
		case 2:
				HASHC;
		case 1:
				HASHC;
			} while (--loop);
		}

	}
	return (n);
}

/* Hash function from Chris Torek. */
static int
hash4(key, len)
	register u_char *key;
	register int len;
{
	register int h, loop;

#define HASH4a   h = (h << 5) - h + *key++;
#define HASH4b   h = (h << 5) + h + *key++;
#define HASH4 HASH4b

	h = 0;
	if (len > 0) {
		loop = (len + 8 - 1) >> 3;

		switch (len & (8 - 1)) {
		case 0:
			do {	/* All fall throughs */
				HASH4;
		case 7:
				HASH4;
		case 6:
				HASH4;
		case 5:
				HASH4;
		case 4:
				HASH4;
		case 3:
				HASH4;
		case 2:
				HASH4;
		case 1:
				HASH4;
			} while (--loop);
		}

	}
	return (h);
}
