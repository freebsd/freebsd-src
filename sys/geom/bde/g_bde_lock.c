/*-
 * Copyright (c) 2002 Poul-Henning Kamp
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Poul-Henning Kamp
 * and NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 *
 * $FreeBSD$
 *
 * This souce file contains routines which operates on the lock sectors, both
 * for the kernel and the userland program gbde(1).
 *
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stdint.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/md5.h>

#ifdef _KERNEL
#include <sys/malloc.h>
#include <sys/systm.h>
#else
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#define g_free(foo)	free(foo)
#endif

#include <geom/geom.h>
#include <geom/bde/g_bde.h>

#include <crypto/rijndael/rijndael.h>

/*
 * Encode/Decode the lock structure in byte-sequence format.
 *
 * Security objectives:  none.
 *
 * C-structure packing and byte-endianess depends on architecture, compiler
 * and compiler options.  We therefore explicitly encode and decode struct
 * g_bde_key using an invariant byte-sequence format.
 *
 */

void
g_bde_encode_lock(struct g_bde_key *gl, u_char *ptr)
{

	bcopy(gl->hash, ptr + 0, sizeof gl->hash);
	g_enc_le8(ptr +  16, gl->sector0);
	g_enc_le8(ptr +  24, gl->sectorN);
	g_enc_le8(ptr +  32, gl->keyoffset);
	g_enc_le4(ptr +  40, gl->sectorsize);
	g_enc_le4(ptr +  44, gl->flags);
	g_enc_le8(ptr +  48, gl->lsector[0]);
	g_enc_le8(ptr +  56, gl->lsector[1]);
	g_enc_le8(ptr +  64, gl->lsector[2]);
	g_enc_le8(ptr +  72, gl->lsector[3]);
	bcopy(gl->spare, ptr +  80, sizeof gl->spare);
	bcopy(gl->salt, ptr + 112, sizeof gl->salt);
	bcopy(gl->mkey, ptr + 128, sizeof gl->mkey);
}

void
g_bde_decode_lock(struct g_bde_key *gl, u_char *ptr)
{
	bcopy(ptr + 0, gl->hash, sizeof gl->hash);
	gl->sector0    = g_dec_le8(ptr +  16);
	gl->sectorN    = g_dec_le8(ptr +  24);
	gl->keyoffset  = g_dec_le8(ptr +  32);
	gl->sectorsize = g_dec_le4(ptr +  40);
	gl->flags      = g_dec_le4(ptr +  44);
	gl->lsector[0] = g_dec_le8(ptr +  48);
	gl->lsector[1] = g_dec_le8(ptr +  56);
	gl->lsector[2] = g_dec_le8(ptr +  64);
	gl->lsector[3] = g_dec_le8(ptr +  72);
	bcopy(ptr +  80, gl->spare, sizeof gl->spare);
	bcopy(ptr + 112, gl->salt, sizeof gl->salt);
	bcopy(ptr + 128, gl->mkey, sizeof gl->mkey);
}

/*
 * Generate key-material used for protecting lock sectors.
 *
 * Security objectives: from the pass-phrase provide by the user, produce a 
 * reproducible stream of bits/bytes which resemeble pseudo-random bits.
 *
 * This is the stream-cipher algorithm called ARC4.  See for instance the
 * description in "Applied Cryptography" by Bruce Scneier.
 */

u_char
g_bde_arc4(struct g_bde_softc *sc)
{
	u_char c;

	sc->arc4_j += sc->arc4_sbox[++sc->arc4_i];
	c = sc->arc4_sbox[sc->arc4_i];
	sc->arc4_sbox[sc->arc4_i] = sc->arc4_sbox[sc->arc4_j];
	sc->arc4_sbox[sc->arc4_j] = c;
	c = sc->arc4_sbox[sc->arc4_i] + sc->arc4_sbox[sc->arc4_j];
	c = sc->arc4_sbox[c];
	return (c);
}

void
g_bde_arc4_seq(struct g_bde_softc *sc, void *ptr, u_int len)
{
	u_char *p;
	
	p = ptr;
	while (len--)
		*p++ = g_bde_arc4(sc);
}

void
g_bde_arc4_seed(struct g_bde_softc *sc, const void *ptr, u_int len)
{
	u_char k[256], c;
	const u_char *p;
	u_int i;

	p = ptr;
	sc->arc4_i = 0;
	bzero(k, sizeof k);
	while(len--)
		k[sc->arc4_i++] ^= *p++;

	sc->arc4_j = 0;
	for (i = 0; i < 256; i++) 
		sc->arc4_sbox[i] = i;
	for (i = 0; i < 256; i++) {
		sc->arc4_j += sc->arc4_sbox[i] + k[i];
		c = sc->arc4_sbox[i];
		sc->arc4_sbox[i] = sc->arc4_sbox[sc->arc4_j];
		sc->arc4_sbox[sc->arc4_j] = c;
	}
	sc->arc4_i = 0;
	sc->arc4_j = 0;
}

/*
 * Encrypt/Decrypt the metadata address with key-material.
 */

int
g_bde_keyloc_encrypt(struct g_bde_softc *sc, void *input, void *output)
{
	u_char *p;
	u_char buf[16], buf1[16];
	u_int i;
	keyInstance ki;
	cipherInstance ci;

	bcopy(input, output, 16);
	return 0;
	rijndael_cipherInit(&ci, MODE_CBC, NULL);
	p = input;
	g_bde_arc4_seq(sc, buf, sizeof buf);
	for (i = 0; i < sizeof buf; i++)
		buf1[i] = p[i] ^ buf[i];
	g_bde_arc4_seq(sc, buf, sizeof buf);
	rijndael_makeKey(&ki, DIR_ENCRYPT, G_BDE_KKEYBITS, buf);
	rijndael_blockEncrypt(&ci, &ki, buf1, 16 * 8, output);
	bzero(&ci, sizeof ci);
	bzero(&ki, sizeof ki);
	return (0);
}

int
g_bde_keyloc_decrypt(struct g_bde_softc *sc, void *input, void *output)
{
	u_char *p;
	u_char buf1[16], buf2[16];
	u_int i;
	keyInstance ki;
	cipherInstance ci;

	bcopy(input, output, 16);
	return 0;
	rijndael_cipherInit(&ci, MODE_CBC, NULL);
	g_bde_arc4_seq(sc, buf1, sizeof buf1);
	g_bde_arc4_seq(sc, buf2, sizeof buf2);
	rijndael_makeKey(&ki, DIR_DECRYPT, G_BDE_KKEYBITS, buf2);
	rijndael_blockDecrypt(&ci, &ki, input, 16 * 8, output);
	p = output;
	for (i = 0; i < sizeof buf1; i++)
		p[i] ^= buf1[i];
	bzero(&ci, sizeof ci);
	bzero(&ki, sizeof ki);
	return (0);
}

/*
 * Encode/Decode lock sectors, do the real work.
 */

static int
g_bde_decrypt_lockx(struct g_bde_softc *sc, u_char *sbox, u_char *meta, off_t mediasize, u_int sectorsize, u_int *nkey)
{
	u_char *buf, k1buf[16], k2buf[G_BDE_LOCKSIZE], k3buf[16], *q;
	struct g_bde_key *gl;
	uint64_t off[2];
	int error, m, i;
	MD5_CTX c;
	keyInstance ki;
	cipherInstance ci;

	rijndael_cipherInit(&ci, MODE_CBC, NULL);
	bcopy(sbox, sc->arc4_sbox, 256);
	sc->arc4_i = 0;
	sc->arc4_j = 0;
	gl = &sc->key;
	error = g_bde_keyloc_decrypt(sc, meta, off);
	if (error)
		return(error);

	if (off[0] + G_BDE_LOCKSIZE > (uint64_t)mediasize) {
		bzero(off, sizeof off);
		return (EINVAL);
	}
	off[1] = 0;
	m = 1;
	if (off[0] % sectorsize > sectorsize - G_BDE_LOCKSIZE)
		m++;
	buf = g_read_data(sc->consumer,
		off[0] - (off[0] % sectorsize),
		m * sectorsize, &error);
	if (buf == NULL) {
		off[0] = 0;
		return(error);
	}

	q = buf + off[0] % sectorsize;

	off[1] = 0;
	for (i = 0; i < G_BDE_LOCKSIZE; i++)
		off[1] += q[i];

	if (off[1] == 0) {
		off[0] = 0;
		g_free(buf);
		return (ESRCH);
	}

	g_bde_arc4_seq(sc, k1buf, sizeof k1buf);
	g_bde_arc4_seq(sc, k2buf, sizeof k2buf);
	g_bde_arc4_seq(sc, k3buf, sizeof k3buf);

	MD5Init(&c);
	MD5Update(&c, "0000", 4);	/* XXX: for future versioning */
	MD5Update(&c, k1buf, 16);
	MD5Final(k1buf, &c);

	rijndael_makeKey(&ki, DIR_DECRYPT, 128, k3buf);
	bzero(k3buf, sizeof k3buf);
	rijndael_blockDecrypt(&ci, &ki, q, G_BDE_LOCKSIZE * 8, q);

	for (i = 0; i < G_BDE_LOCKSIZE; i++)
		q[i] ^= k2buf[i];
	bzero(k2buf, sizeof k2buf);

	if (bcmp(q, k1buf, sizeof k1buf)) {
		bzero(k1buf, sizeof k1buf);
		bzero(buf, sectorsize * m);
		g_free(buf);
		off[0] = 0;
		return (ENOTDIR);
	}
	bzero(k1buf, sizeof k1buf);

	g_bde_decode_lock(gl, q);
	bzero(buf, sectorsize * m);
	g_free(buf);

	off[1] = 0;
	for (i = 0; i < (int)sizeof(gl->mkey); i++)
		off[1] += gl->mkey[i];

	if (off[1] == 0) {
		off[0] = 0;
		return (ENOENT);
	}
	for (i = 0; i < G_BDE_MAXKEYS; i++)
		if (nkey != NULL && off[0] == gl->lsector[i])
			*nkey = i;

	return (0);
}

/*
 * Encode/Decode lock sectors.
 */

int
g_bde_decrypt_lock(struct g_bde_softc *sc, u_char *sbox, u_char *meta, off_t mediasize, u_int sectorsize, u_int *nkey)
{
	u_char *buf, buf1[16];
	int error, e, i;

	bzero(buf1, sizeof buf1);
	if (bcmp(buf1, meta, sizeof buf1))
		return (g_bde_decrypt_lockx(sc, sbox, meta, mediasize,
		    sectorsize, nkey));

	buf = g_read_data(sc->consumer, 0, sectorsize, &error);
	if (buf == NULL)
		return(error);
	error = 0;
	for (i = 0; i < G_BDE_MAXKEYS; i++) {
		e = g_bde_decrypt_lockx(sc, sbox, buf + i * 16, mediasize,
		    sectorsize, nkey);
		if (e == 0 || e == ENOENT) {
			error = e;
			break;
		}
		if (e == ESRCH)
			error = ENOTDIR;
		else if (e != 0)
			error = e;
	}
	g_free(buf);
	return (error);
}
