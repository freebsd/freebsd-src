/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/crypto/hmac_md5.c,v 1.1 1999/12/22 19:13:05 shin Exp $
 */

/*
 * Based on sample code appeared on RFC2104.
 */

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <crypto/md5.h>

#include <crypto/hmac_md5.h>

void
hmac_md5(src0, srclen, key0, keylen, digest)
	caddr_t src0;
	size_t srclen;
	caddr_t key0;
	size_t keylen;
	caddr_t digest;
{
	u_int8_t *src;
	u_int8_t *key;
	u_int8_t tk[16];
	u_int8_t ipad[65];
	u_int8_t opad[65];
	size_t i;

	src = (u_int8_t *)src0;
	key = (u_int8_t *)key0;

	/*
	 * compress the key into 16bytes, if key is too long.
	 */
	if (64 < keylen) {
		md5_init();
		md5_loop(key, keylen);
		md5_pad();
		md5_result(&tk[0]);
		key = &tk[0];
		keylen = 16;
	}

	/*
	 *
	 */
	bzero(&ipad[0], sizeof ipad);
	bzero(&opad[0], sizeof opad);
	bcopy(key, &ipad[0], keylen);
	bcopy(key, &opad[0], keylen);

	for (i = 0; i < 64; i++) {
		ipad[i] ^= 0x36;
		opad[i] ^= 0x5c;
	}

	md5_init();
	md5_loop(&ipad[0], 64);
	md5_loop(src, srclen);
	md5_pad();
	md5_result((u_int8_t *)digest);

	md5_init();
	md5_loop(&opad[0], 64);
	md5_loop((u_int8_t *)digest, 16);
	md5_pad();
	md5_result((u_int8_t *)digest);
}
