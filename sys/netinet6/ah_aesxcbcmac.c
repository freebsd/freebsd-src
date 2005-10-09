/*	$KAME: ah_aesxcbcmac.c,v 1.6 2003/07/22 02:30:54 itojun Exp $	*/

/*-
 * Copyright (C) 1995, 1996, 1997, 1998 and 2003 WIDE Project.
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/mbuf.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>

#include <netinet6/ipsec.h>
#include <netinet6/ah.h>
#include <netinet6/ah_aesxcbcmac.h>

#include <netkey/key.h>

#include <crypto/rijndael/rijndael.h>

#include <net/net_osdep.h>

#define AES_BLOCKSIZE	16

typedef struct {
	u_int8_t	e[AES_BLOCKSIZE];
	u_int8_t	buf[AES_BLOCKSIZE];
	size_t		buflen;
	u_int32_t	r_k1s[(RIJNDAEL_MAXNR+1)*4];
	u_int32_t	r_k2s[(RIJNDAEL_MAXNR+1)*4];
	u_int32_t	r_k3s[(RIJNDAEL_MAXNR+1)*4];
	int		r_nr; /* key-length-dependent number of rounds */
	u_int8_t	k2[AES_BLOCKSIZE];
	u_int8_t	k3[AES_BLOCKSIZE];
} aesxcbc_ctx;

int
ah_aes_xcbc_mac_init(state, sav)
	struct ah_algorithm_state *state;
	struct secasvar *sav;
{
	u_int8_t k1seed[AES_BLOCKSIZE] = { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 };
	u_int8_t k2seed[AES_BLOCKSIZE] = { 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2 };
	u_int8_t k3seed[AES_BLOCKSIZE] = { 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 };
	u_int32_t r_ks[(RIJNDAEL_MAXNR+1)*4];
	aesxcbc_ctx *ctx;
	u_int8_t k1[AES_BLOCKSIZE];

	if (!state)
		panic("ah_aes_xcbc_mac_init: what?");

	state->sav = sav;
	state->foo = (void *)malloc(sizeof(aesxcbc_ctx), M_TEMP, M_NOWAIT);
	if (!state->foo)
		return ENOBUFS;
	bzero(state->foo, sizeof(aesxcbc_ctx));

	ctx = (aesxcbc_ctx *)state->foo;

	if ((ctx->r_nr = rijndaelKeySetupEnc(r_ks,
	    (char *)_KEYBUF(sav->key_auth), AES_BLOCKSIZE * 8)) == 0)
		return -1;
	rijndaelEncrypt(r_ks, ctx->r_nr, k1seed, k1);
	rijndaelEncrypt(r_ks, ctx->r_nr, k2seed, ctx->k2);
	rijndaelEncrypt(r_ks, ctx->r_nr, k3seed, ctx->k3);
	if (rijndaelKeySetupEnc(ctx->r_k1s, k1, AES_BLOCKSIZE * 8) == 0)
		return -1;
	if (rijndaelKeySetupEnc(ctx->r_k2s, ctx->k2, AES_BLOCKSIZE * 8) == 0)
		return -1;
	if (rijndaelKeySetupEnc(ctx->r_k3s, ctx->k3, AES_BLOCKSIZE * 8) == 0)
		return -1;

	return 0;
}

void
ah_aes_xcbc_mac_loop(state, addr, len)
	struct ah_algorithm_state *state;
	u_int8_t *addr;
	size_t len;
{
	u_int8_t buf[AES_BLOCKSIZE];
	aesxcbc_ctx *ctx;
	u_int8_t *ep;
	int i;

	if (!state || !state->foo)
		panic("ah_aes_xcbc_mac_loop: what?");

	ctx = (aesxcbc_ctx *)state->foo;
	ep = addr + len;

	if (ctx->buflen == sizeof(ctx->buf)) {
		for (i = 0; i < sizeof(ctx->e); i++)
			ctx->buf[i] ^= ctx->e[i];
		rijndaelEncrypt(ctx->r_k1s, ctx->r_nr, ctx->buf, ctx->e);
		ctx->buflen = 0;
	}
	if (ctx->buflen + len < sizeof(ctx->buf)) {
		bcopy(addr, ctx->buf + ctx->buflen, len);
		ctx->buflen += len;
		return;
	}
	if (ctx->buflen && ctx->buflen + len > sizeof(ctx->buf)) {
		bcopy(addr, ctx->buf + ctx->buflen,
		    sizeof(ctx->buf) - ctx->buflen);
		for (i = 0; i < sizeof(ctx->e); i++)
			ctx->buf[i] ^= ctx->e[i];
		rijndaelEncrypt(ctx->r_k1s, ctx->r_nr, ctx->buf, ctx->e);
		addr += sizeof(ctx->buf) - ctx->buflen;
		ctx->buflen = 0;
	}
	/* due to the special processing for M[n], "=" case is not included */
	while (addr + AES_BLOCKSIZE < ep) {
		bcopy(addr, buf, AES_BLOCKSIZE);
		for (i = 0; i < sizeof(buf); i++)
			buf[i] ^= ctx->e[i];
		rijndaelEncrypt(ctx->r_k1s, ctx->r_nr, buf, ctx->e);
		addr += AES_BLOCKSIZE;
	}
	if (addr < ep) {
		bcopy(addr, ctx->buf + ctx->buflen, ep - addr);
		ctx->buflen += ep - addr;
	}
}

void
ah_aes_xcbc_mac_result(state, addr, l)
	struct ah_algorithm_state *state;
	u_int8_t *addr;
	size_t l;
{
	u_char digest[AES_BLOCKSIZE];
	aesxcbc_ctx *ctx;
	int i;

	ctx = (aesxcbc_ctx *)state->foo;

	if (ctx->buflen == sizeof(ctx->buf)) {
		for (i = 0; i < sizeof(ctx->buf); i++) {
			ctx->buf[i] ^= ctx->e[i];
			ctx->buf[i] ^= ctx->k2[i];
		}
		rijndaelEncrypt(ctx->r_k1s, ctx->r_nr, ctx->buf, digest);
	} else {
		for (i = ctx->buflen; i < sizeof(ctx->buf); i++)
			ctx->buf[i] = (i == ctx->buflen) ? 0x80 : 0x00;
		for (i = 0; i < sizeof(ctx->buf); i++) {
			ctx->buf[i] ^= ctx->e[i];
			ctx->buf[i] ^= ctx->k3[i];
		}
		rijndaelEncrypt(ctx->r_k1s, ctx->r_nr, ctx->buf, digest);
	}

	bcopy(digest, addr, sizeof(digest) > l ? l : sizeof(digest));

	free(state->foo, M_TEMP);
}
