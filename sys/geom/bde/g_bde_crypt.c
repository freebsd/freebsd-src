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
 * This source file contains the functions responsible for the crypto, keying
 * and mapping operations on the I/O requests.
 *
 */

#include <sys/param.h>
#include <sys/stdint.h>
#include <sys/bio.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/libkern.h>
#include <sys/md5.h>

#include <geom/geom.h>
#include <geom/bde/g_bde.h>

#include <crypto/rijndael/rijndael.h>

/*
 * These four functions wrap the raw Rijndael functions and make sure we
 * explode if something fails which shouldn't.
 */

static void
AES_init(cipherInstance *ci)
{
	int error;

	error = rijndael_cipherInit(ci, MODE_CBC, NULL);
	KASSERT(error > 0, ("rijndael_cipherInit %d", error));
}

static void
AES_makekey(keyInstance *ki, int dir, u_int len, void *key)
{
	int error;

	error = rijndael_makeKey(ki, dir, len, key);
	KASSERT(error > 0, ("rijndael_makeKey %d", error));
}

static void
AES_encrypt(cipherInstance *ci, keyInstance *ki, void *in, void *out, u_int len)
{
	int error;

	error = rijndael_blockEncrypt(ci, ki, in, len * 8, out);
	KASSERT(error > 0, ("rijndael_blockEncrypt %d", error));
}

static void
AES_decrypt(cipherInstance *ci, keyInstance *ki, void *in, void *out, u_int len)
{
	int error;

	error = rijndael_blockDecrypt(ci, ki, in, len * 8, out);
	KASSERT(error > 0, ("rijndael_blockDecrypt %d", error));
}

/*
 * Derive kkey from mkey + sector offset.
 *
 * Security objective: Derive a potentially very large number of distinct skeys
 * from the comparatively small key material in our mkey, in such a way that
 * if one, more or even many of the kkeys are compromised, this does not
 * significantly help an attack on other kkeys and in particular does not
 * weaken or compromised the mkey.
 *
 * We do this by cherry-picking characters out of the mkey, feeding these to
 * MD5 with the sector offset in the middle and using the MD5 hash as kkey.
 *
 * The MD5 only acts as a "diode" against brute-force reversal, it offsers no
 * protection if the input to MD5 is predictable or insufficiently uncorrelated
 * from sector to sector.
 *
 * The amount of entropy in a sector number is very low, and the amount of
 * entropy between two sector numbers is even lower, (only slightly higher than
 * one bit), so we rely heavily on the mkey to make the cherry picking non-
 * linear and irreversible.
 *
 * This strong dependency on the mkey is very desirable, but the low amount
 * of entropy from the sector number means that the algorithm is vulnerable
 * to mkeys which has a lumpy histogram of byte values or little entropy.
 *
 * If you read this comment in order to find a weak spot or the best way to
 * attack GBDE, you have probably come to the right place.  Good luck.
 */

static void
g_bde_kkey(struct g_bde_softc *sc, keyInstance *ki, int dir, off_t sector)
{
	u_int u, v, w, t;
	MD5_CTX ct;
	u_char buf[16], c;

	MD5Init(&ct);
	w = sector /= sc->sectorsize;
	v = w % 211;	/* A prime slightly smaller than G_BDE_MKEYLEN */
	u = w % 19;	/* A small prime */
	for (t = 0; t < G_BDE_SKEYLEN; t++) {
		u %= G_BDE_MKEYLEN;
		v %= G_BDE_MKEYLEN;
		c = sc->key.key[u] ^ sc->key.key[v];
		MD5Update(&ct, &c, 1);
		v += c + t;
		u += sc->key.key[c];
		if (w & 1)
			v += 13;	/* A small prime */
		else
			u += 131;	/* A prime roughly G_BDE_MKEYLEN / 2 */
		if (t == G_BDE_SKEYLEN / 2)
			MD5Update(&ct, (void *)&sector, sizeof sector);
	}
	w = v = u - 0;
	MD5Update(&ct, (void *)&sector, sizeof sector);
	MD5Final(buf, &ct);
	bzero(&ct, sizeof ct);
	AES_makekey(ki, dir, G_BDE_KKEYBITS, buf);
	bzero(buf, sizeof buf);
}

/*
 * Encryption work for read operation.
 *
 * Security objective: Find the kkey, find the skey, decrypt the sector data.
 */

void
g_bde_crypt_read(struct g_bde_work *wp)
{
	struct g_bde_softc *sc;
	u_char *d;
	u_int n;
	off_t o;
	u_char skey[G_BDE_SKEYLEN];
	keyInstance ki;
	cipherInstance ci;
	

	AES_init(&ci);
	sc = wp->softc;
	o = 0;
	for (n = 0; o < wp->length; n++, o += sc->sectorsize) {
		d = (u_char *)wp->ksp->data + wp->ko + n * G_BDE_SKEYLEN;
		g_bde_kkey(sc, &ki, DIR_DECRYPT, wp->offset + o);
		AES_decrypt(&ci, &ki, d, skey, sizeof skey);
		d = (u_char *)wp->data + o;
		AES_makekey(&ki, DIR_DECRYPT, G_BDE_SKEYBITS, skey);
		AES_decrypt(&ci, &ki, d, d, sc->sectorsize);
	}
	bzero(skey, sizeof skey);
	bzero(&ci, sizeof ci);
	bzero(&ki, sizeof ci);
}

/*
 * Encryption work for write operation.
 *
 * Security objective: Create random skey, encrypt sector data,
 * encrypt skey with the kkey.
 */

void
g_bde_crypt_write(struct g_bde_work *wp)
{
	u_char *s, *d;
	struct g_bde_softc *sc;
	u_int n;
	off_t o;
	u_char skey[G_BDE_SKEYLEN];
	keyInstance ki;
	cipherInstance ci;

	sc = wp->softc;
	AES_init(&ci);
	o = 0;
	for (n = 0; o < wp->length; n++, o += sc->sectorsize) {

		s = (u_char *)wp->data + o;
		d = (u_char *)wp->sp->data + o;
		arc4rand(&skey, sizeof skey, 0);
		AES_makekey(&ki, DIR_ENCRYPT, G_BDE_SKEYBITS, skey);
		AES_encrypt(&ci, &ki, s, d, sc->sectorsize);

		d = (u_char *)wp->ksp->data + wp->ko + n * G_BDE_SKEYLEN;
		g_bde_kkey(sc, &ki, DIR_ENCRYPT, wp->offset + o);
		AES_encrypt(&ci, &ki, skey, d, sizeof skey);
		bzero(skey, sizeof skey);
	}
	bzero(skey, sizeof skey);
	bzero(&ci, sizeof ci);
	bzero(&ki, sizeof ci);
}

/*
 * Encryption work for delete operation.
 *
 * Security objective: Write random data to the sectors.
 *
 * XXX: At a hit in performance we would trash the encrypted skey as well.
 * XXX: This would add frustration to the cleaning lady attack by making
 * XXX: deletes look like writes.
 */

void
g_bde_crypt_delete(struct g_bde_work *wp)
{
	struct g_bde_softc *sc;
	u_char *d;
	off_t o;

	sc = wp->softc;
	d = wp->sp->data;
	/*
	 * Do not unroll this loop!
	 * Our zone may be significantly wider than the amount of random
	 * bytes arc4rand likes to give in one reseeding, whereas our
	 * sectorsize is far more likely to be in the same range.
	 */
	for (o = 0; o < wp->length; o += sc->sectorsize) {
		arc4rand(d, sc->sectorsize, 0);
		d += sc->sectorsize;
	}
	/*
	 * Having written a long random sequence to disk here, we want to
	 * force a reseed, to avoid weakening the next time we use random
	 * data for something important.
	 */
	arc4rand(&o, sizeof o, 1);
}

/*
 * Calculate the total payload size of the encrypted device.
 *
 * Security objectives: none.
 *
 * This function needs to agree with g_bde_map_sector() about things.
 */

uint64_t
g_bde_max_sector(struct g_bde_key *kp)
{
	uint64_t maxsect;

	maxsect = kp->media_width;
	maxsect /= kp->zone_width;
	maxsect *= kp->zone_cont;
	return (maxsect);
}

/*
 * Convert an unencrypted side offset to offsets on the encrypted side.
 *
 * Security objective:  Make it harder to identify what sectors contain what
 * on a "cold" disk image.
 *
 * We do this by adding the "keyoffset" from the lock to the physical sector
 * number modulus the available number of sectors, since all physical sectors
 * presumably look the same cold, this should be enough.
 *
 * Shuffling things further is an option, but the incremental frustration is
 * not currently deemed worth the run-time performance hit resulting from the
 * increased number of disk arm movements it would incur.
 *
 * This function offers nothing but a trivial diversion for an attacker able
 * to do "the cleaning lady attack" in its current static mapping form.
 */

void
g_bde_map_sector(struct g_bde_key *kp,
	uint64_t isector,
	uint64_t *osector,
	uint64_t *ksector,
	u_int *koffset)
{

	u_int	zone, zoff, zidx, u;
	uint64_t os;

	/* find which zone and the offset and index in it */
	zone = isector / kp->zone_cont;
	zoff = isector % kp->zone_cont;
	zidx = zoff / kp->sectorsize;

	/* Find physical sector address */
	os = zone * kp->zone_width + zoff;
	os += kp->keyoffset;
	os %= kp->media_width - (G_BDE_MAXKEYS * kp->sectorsize);
	os += kp->sector0;

	/* Compensate for lock sectors */
	for (u = 0; u < G_BDE_MAXKEYS; u++)
		if (os >= kp->lsector[u])
			os += kp->sectorsize;

	*osector = os;

	/* The key sector is the last in this zone. */
	os = (1 + zone) * kp->zone_width - kp->sectorsize;
	os += kp->keyoffset;
	os %= kp->media_width - (G_BDE_MAXKEYS * kp->sectorsize);
	os += kp->sector0; 

	for (u = 0; u < G_BDE_MAXKEYS; u++)
		if (os >= kp->lsector[u])
			os += kp->sectorsize;
	*ksector = os;

	*koffset = zidx * G_BDE_SKEYLEN;

#if 0
	printf("off %jd %jd %jd %u\n",
	    (intmax_t)isector,
	    (intmax_t)*osector,
	    (intmax_t)*ksector,
	    *koffset);
#endif
}
