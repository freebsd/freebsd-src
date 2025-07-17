/*-
 * Copyright (c) 2014 The FreeBSD Foundation
 *
 * This software was developed by John-Mark Gurney under
 * the sponsorship of the FreeBSD Foundation and
 * Rubicon Communications, LLC (Netgate).
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <opencrypto/gfmult.h>
#include <opencrypto/gmac.h>

void
AES_GMAC_Init(void *ctx)
{
	struct aes_gmac_ctx *agc;

	agc = ctx;
	bzero(agc, sizeof *agc);
}

void
AES_GMAC_Setkey(void *ctx, const uint8_t *key, u_int klen)
{
	struct aes_gmac_ctx *agc;
	const uint8_t zeros[GMAC_BLOCK_LEN] = {};
	struct gf128 h;
	uint8_t hbuf[GMAC_BLOCK_LEN];

	agc = ctx;
	agc->rounds = rijndaelKeySetupEnc(agc->keysched, key, klen * 8);

	rijndaelEncrypt(agc->keysched, agc->rounds, zeros, hbuf);

	h = gf128_read(hbuf);
	gf128_genmultable4(h, &agc->ghashtbl);

	explicit_bzero(&h, sizeof h);
	explicit_bzero(hbuf, sizeof hbuf);
}

void
AES_GMAC_Reinit(void *ctx, const uint8_t *iv, u_int ivlen)
{
	struct aes_gmac_ctx *agc;

	agc = ctx;
	KASSERT(ivlen <= sizeof agc->counter, ("passed ivlen too large!"));
	memset(agc->counter, 0, sizeof(agc->counter));
	bcopy(iv, agc->counter, ivlen);
	agc->counter[GMAC_BLOCK_LEN - 1] = 1;

	memset(&agc->hash, 0, sizeof(agc->hash));
}

int
AES_GMAC_Update(void *ctx, const void *vdata, u_int len)
{
	struct aes_gmac_ctx *agc;
	const uint8_t *data;
	struct gf128 v;
	uint8_t buf[GMAC_BLOCK_LEN] = {};
	int i;

	agc = ctx;
	data = vdata;
	v = agc->hash;

	while (len > 0) {
		if (len >= 4*GMAC_BLOCK_LEN) {
			i = 4*GMAC_BLOCK_LEN;
			v = gf128_mul4b(v, data, &agc->ghashtbl);
		} else if (len >= GMAC_BLOCK_LEN) {
			i = GMAC_BLOCK_LEN;
			v = gf128_add(v, gf128_read(data));
			v = gf128_mul(v, &agc->ghashtbl.tbls[0]);
		} else {
			i = len;
			bcopy(data, buf, i);
			v = gf128_add(v, gf128_read(&buf[0]));
			v = gf128_mul(v, &agc->ghashtbl.tbls[0]);
			explicit_bzero(buf, sizeof buf);
		}
		len -= i;
		data += i;
	}

	agc->hash = v;
	explicit_bzero(&v, sizeof v);

	return (0);
}

void
AES_GMAC_Final(uint8_t *digest, void *ctx)
{
	struct aes_gmac_ctx *agc;
	uint8_t enccntr[GMAC_BLOCK_LEN];
	struct gf128 a;

	agc = ctx;

	rijndaelEncrypt(agc->keysched, agc->rounds, agc->counter, enccntr);
	a = gf128_add(agc->hash, gf128_read(enccntr));
	gf128_write(a, digest);

	explicit_bzero(enccntr, sizeof enccntr);
}
