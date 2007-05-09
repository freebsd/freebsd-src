/*
 *
 * Copyright (c) 2006
 * NTT (Nippon Telegraph and Telephone Corporation) . All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer as
 *   the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NTT ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL NTT BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>

#include <netinet6/ipsec.h>
#include <netinet6/esp.h>
#include <netinet6/esp_camellia.h>

#include <crypto/camellia/camellia.h>

size_t
esp_camellia_schedlen(algo)
	const struct esp_algorithm *algo;
{

	return sizeof(camellia_ctx);
}

int
esp_camellia_schedule(algo, sav)
	const struct esp_algorithm *algo;
	struct secasvar *sav;
{
	camellia_ctx *ctx;

	ctx = (camellia_ctx *)sav->sched;
	camellia_set_key(ctx,
	    (u_char *)_KEYBUF(sav->key_enc), _KEYLEN(sav->key_enc) * 8);
	return 0;
}

int
esp_camellia_blockdecrypt(algo, sav, s, d)
	const struct esp_algorithm *algo;
	struct secasvar *sav;
	u_int8_t *s;
	u_int8_t *d;
{
	camellia_ctx *ctx;

	ctx = (camellia_ctx *)sav->sched;
	camellia_decrypt(ctx, s, d);
	return 0;
}

int
esp_camellia_blockencrypt(algo, sav, s, d)
	const struct esp_algorithm *algo;
	struct secasvar *sav;
	u_int8_t *s;
	u_int8_t *d;
{
	camellia_ctx *ctx;

	ctx = (camellia_ctx *)sav->sched;
	camellia_encrypt(ctx, s, d);
	return 0;
}
