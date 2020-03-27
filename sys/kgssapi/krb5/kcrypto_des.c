/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2008 Isilon Inc http://www.isilon.com/
 * Authors: Doug Rabson <dfr@rabson.org>
 * Developed with Red Inc: Alfred Perlstein <alfred@freebsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/kobj.h>
#include <sys/malloc.h>
#include <sys/md5.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>
#include <crypto/des/des.h>
#include <opencrypto/cryptodev.h>

#include <kgssapi/gssapi.h>
#include <kgssapi/gssapi_impl.h>

#include "kcrypto.h"

struct des1_state {
	struct mtx	ds_lock;
	crypto_session_t ds_session;
};

static void
des1_init(struct krb5_key_state *ks)
{
	static struct timeval lastwarn;
	struct des1_state *ds;

	ds = malloc(sizeof(struct des1_state), M_GSSAPI, M_WAITOK|M_ZERO);
	mtx_init(&ds->ds_lock, "gss des lock", NULL, MTX_DEF);
	ks->ks_priv = ds;
	if (ratecheck(&lastwarn, &krb5_warn_interval))
		gone_in(13, "DES cipher for Kerberos GSS");
}

static void
des1_destroy(struct krb5_key_state *ks)
{
	struct des1_state *ds = ks->ks_priv;

	if (ds->ds_session)
		crypto_freesession(ds->ds_session);
	mtx_destroy(&ds->ds_lock);
	free(ks->ks_priv, M_GSSAPI);

}

static void
des1_set_key(struct krb5_key_state *ks, const void *in)
{
	struct crypto_session_params csp;
	void *kp = ks->ks_key;
	struct des1_state *ds = ks->ks_priv;

	if (ds->ds_session)
		crypto_freesession(ds->ds_session);

	if (kp != in)
		bcopy(in, kp, ks->ks_class->ec_keylen);

	memset(&csp, 0, sizeof(csp));
	csp.csp_mode = CSP_MODE_CIPHER;
	csp.csp_ivlen = 8;
	csp.csp_cipher_alg = CRYPTO_DES_CBC;
	csp.csp_cipher_klen = 8;
	csp.csp_cipher_key = ks->ks_key;

	crypto_newsession(&ds->ds_session, &csp,
	    CRYPTOCAP_F_HARDWARE | CRYPTOCAP_F_SOFTWARE);
}

static void
des1_random_to_key(struct krb5_key_state *ks, const void *in)
{
	uint8_t *outkey = ks->ks_key;
	const uint8_t *inkey = in;

	/*
	 * Expand 56 bits of random data to 64 bits as follows
	 * (in the example, bit number 1 is the MSB of the 56
	 * bits of random data):
	 *
	 * expanded = 
	 *	 1  2  3  4  5  6  7  p
	 *	 9 10 11 12 13 14 15  p
	 *	17 18 19 20 21 22 23  p
	 *	25 26 27 28 29 30 31  p
	 *	33 34 35 36 37 38 39  p
	 *	41 42 43 44 45 46 47  p
	 *	49 50 51 52 53 54 55  p
	 *	56 48 40 32 24 16  8  p
	 */
	outkey[0] = inkey[0];
	outkey[1] = inkey[1];
	outkey[2] = inkey[2];
	outkey[3] = inkey[3];
	outkey[4] = inkey[4];
	outkey[5] = inkey[5];
	outkey[6] = inkey[6];
	outkey[7] = (((inkey[0] & 1) << 1)
	    | ((inkey[1] & 1) << 2)
	    | ((inkey[2] & 1) << 3)
	    | ((inkey[3] & 1) << 4)
	    | ((inkey[4] & 1) << 5)
	    | ((inkey[5] & 1) << 6)
	    | ((inkey[6] & 1) << 7));
	des_set_odd_parity(outkey);
	if (des_is_weak_key(outkey))
		outkey[7] ^= 0xf0;

	des1_set_key(ks, ks->ks_key);
}

static int
des1_crypto_cb(struct cryptop *crp)
{
	int error;
	struct des1_state *ds = (struct des1_state *) crp->crp_opaque;
	
	if (crypto_ses2caps(ds->ds_session) & CRYPTOCAP_F_SYNC)
		return (0);

	error = crp->crp_etype;
	if (error == EAGAIN)
		error = crypto_dispatch(crp);
	mtx_lock(&ds->ds_lock);
	if (error || (crp->crp_flags & CRYPTO_F_DONE))
		wakeup(crp);
	mtx_unlock(&ds->ds_lock);

	return (0);
}

static void
des1_encrypt_1(const struct krb5_key_state *ks, int buf_type, void *buf,
    size_t skip, size_t len, void *ivec, bool encrypt)
{
	struct des1_state *ds = ks->ks_priv;
	struct cryptop *crp;
	int error;

	crp = crypto_getreq(ds->ds_session, M_WAITOK);

	crp->crp_payload_start = skip;
	crp->crp_payload_length = len;
	crp->crp_op = encrypt ? CRYPTO_OP_ENCRYPT : CRYPTO_OP_DECRYPT;
	crp->crp_flags = CRYPTO_F_CBIFSYNC | CRYPTO_F_IV_SEPARATE;
	if (ivec) {
		memcpy(crp->crp_iv, ivec, 8);
	} else {
		memset(crp->crp_iv, 0, 8);
	}
	crp->crp_buf_type = buf_type;
	crp->crp_buf = buf;
	crp->crp_opaque = ds;
	crp->crp_callback = des1_crypto_cb;

	error = crypto_dispatch(crp);

	if ((crypto_ses2caps(ds->ds_session) & CRYPTOCAP_F_SYNC) == 0) {
		mtx_lock(&ds->ds_lock);
		if (!error && !(crp->crp_flags & CRYPTO_F_DONE))
			error = msleep(crp, &ds->ds_lock, 0, "gssdes", 0);
		mtx_unlock(&ds->ds_lock);
	}

	crypto_freereq(crp);
}

static void
des1_encrypt(const struct krb5_key_state *ks, struct mbuf *inout,
    size_t skip, size_t len, void *ivec, size_t ivlen)
{

	des1_encrypt_1(ks, CRYPTO_BUF_MBUF, inout, skip, len, ivec, true);
}

static void
des1_decrypt(const struct krb5_key_state *ks, struct mbuf *inout,
    size_t skip, size_t len, void *ivec, size_t ivlen)
{

	des1_encrypt_1(ks, CRYPTO_BUF_MBUF, inout, skip, len, ivec, false);
}

static int
MD5Update_int(void *ctx, void *buf, u_int len)
{

	MD5Update(ctx, buf, len);
	return (0);
}

static void
des1_checksum(const struct krb5_key_state *ks, int usage,
    struct mbuf *inout, size_t skip, size_t inlen, size_t outlen)
{
	char hash[16];
	MD5_CTX md5;

	/*
	 * This checksum is specifically for GSS-API. First take the
	 * MD5 checksum of the message, then calculate the CBC mode
	 * checksum of that MD5 checksum using a zero IV.
	 */
	MD5Init(&md5);
	m_apply(inout, skip, inlen, MD5Update_int, &md5);
	MD5Final(hash, &md5);

	des1_encrypt_1(ks, CRYPTO_BUF_CONTIG, hash, 0, 16, NULL, true);
	m_copyback(inout, skip + inlen, outlen, hash + 8);
}

struct krb5_encryption_class krb5_des_encryption_class = {
	"des-cbc-md5",		/* name */
	ETYPE_DES_CBC_CRC,	/* etype */
	0,			/* flags */
	8,			/* blocklen */
	8,			/* msgblocklen */
	8,			/* checksumlen */
	56,			/* keybits */
	8,			/* keylen */
	des1_init,
	des1_destroy,
	des1_set_key,
	des1_random_to_key,
	des1_encrypt,
	des1_decrypt,
	des1_checksum
};
