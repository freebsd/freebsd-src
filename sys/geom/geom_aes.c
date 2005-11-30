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
 */

/*
 * This method provides AES encryption with a compiled in key (default
 * all zeroes).
 *
 * XXX: This could probably save a lot of code by pretending to be a slicer.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/bio.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/libkern.h>
#include <sys/endian.h>
#include <sys/md5.h>
#include <sys/errno.h>
#include <geom/geom.h>

#include <crypto/rijndael/rijndael-api-fst.h>

#define AES_CLASS_NAME "AES"

#define MASTER_KEY_LENGTH	(1024/8)

static const u_char *aes_magic = "<<FreeBSD-GEOM-AES>>";
static const u_char *aes_magic_random = "<<FreeBSD-GEOM-AES-RANDOM>>";
static const u_char *aes_magic_test = "<<FreeBSD-GEOM-AES-TEST>>";


struct g_aes_softc {
	enum {
		KEY_ZERO,
		KEY_RANDOM,
		KEY_TEST
	} keying;
	u_int	sectorsize;
	off_t	mediasize;
	cipherInstance ci;
	u_char master_key[MASTER_KEY_LENGTH];
};

/*
 * Generate a sectorkey from the masterkey and the offset position.
 *
 * For KEY_ZERO we just return a key of all zeros.
 *
 * We feed the sector byte offset, 16 bytes of the master-key and
 * the sector byte offset once more to MD5.
 * The sector byte offset is converted to little-endian format first
 * to support multi-architecture operation.
 * We use 16 bytes from the master-key starting at the logical sector
 * number modulus he length of the master-key.  If need be we wrap
 * around to the start of the master-key.
 */

static void
g_aes_makekey(struct g_aes_softc *sc, off_t off, keyInstance *ki, int dir)
{
	MD5_CTX cx;
	u_int64_t u64;
	u_int u, u1;
	u_char *p, buf[16];

	if (sc->keying == KEY_ZERO) {
		rijndael_makeKey(ki, dir, 128, sc->master_key);
		return;
	}
	MD5Init(&cx);
	u64 = htole64(off);
	MD5Update(&cx, (u_char *)&u64, sizeof(u64));
	u = off / sc->sectorsize;
	u %= sizeof sc->master_key;
	p = sc->master_key + u;
	if (u + 16 <= sizeof(sc->master_key)) {
		MD5Update(&cx, p, 16);
	} else {
		u1 = sizeof sc->master_key - u;
		MD5Update(&cx, p, u1);
		MD5Update(&cx, sc->master_key, 16 - u1);
		u1 = 0;				/* destroy evidence */
	}
	u = 0;					/* destroy evidence */
	MD5Update(&cx, (u_char *)&u64, sizeof(u64));
	u64 = 0;				/* destroy evidence */
	MD5Final(buf, &cx);
	bzero(&cx, sizeof cx);			/* destroy evidence */
	rijndael_makeKey(ki, dir, 128, buf);
	bzero(buf, sizeof buf);			/* destroy evidence */

}

static void
g_aes_read_done(struct bio *bp)
{
	struct g_geom *gp;
	struct g_aes_softc *sc;
	u_char *p, *b, *e, *sb;
	keyInstance dkey;
	off_t o;

	gp = bp->bio_from->geom;
	sc = gp->softc;
	sb = g_malloc(sc->sectorsize, M_WAITOK);
	b = bp->bio_data;
	e = bp->bio_data;
	e += bp->bio_length;
	o = bp->bio_offset - sc->sectorsize;
	for (p = b; p < e; p += sc->sectorsize) {
		g_aes_makekey(sc, o, &dkey, DIR_DECRYPT);
		rijndael_blockDecrypt(&sc->ci, &dkey, p, sc->sectorsize * 8, sb);
		bcopy(sb, p, sc->sectorsize);
		o += sc->sectorsize;
	}
	bzero(&dkey, sizeof dkey);		/* destroy evidence */
	bzero(sb, sc->sectorsize);		/* destroy evidence */
	g_free(sb);
	g_std_done(bp);
}

static void
g_aes_write_done(struct bio *bp)
{

	bzero(bp->bio_data, bp->bio_length);	/* destroy evidence */
	g_free(bp->bio_data);
	g_std_done(bp);
}

static void
g_aes_start(struct bio *bp)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	struct g_aes_softc *sc;
	struct bio *bp2;
	u_char *p1, *p2, *b, *e;
	keyInstance ekey;
	off_t o;

	gp = bp->bio_to->geom;
	cp = LIST_FIRST(&gp->consumer);
	sc = gp->softc;
	switch (bp->bio_cmd) {
	case BIO_READ:
		bp2 = g_clone_bio(bp);
		if (bp2 == NULL) {
			g_io_deliver(bp, ENOMEM);
			return;
		}
		bp2->bio_done = g_aes_read_done;
		bp2->bio_offset += sc->sectorsize;
		g_io_request(bp2, cp);
		break;
	case BIO_WRITE:
		bp2 = g_clone_bio(bp);
		if (bp2 == NULL) {
			g_io_deliver(bp, ENOMEM);
			return;
		}
		bp2->bio_done = g_aes_write_done;
		bp2->bio_offset += sc->sectorsize;
		bp2->bio_data = g_malloc(bp->bio_length, M_WAITOK);
		b = bp->bio_data;
		e = bp->bio_data;
		e += bp->bio_length;
		p2 = bp2->bio_data;
		o = bp->bio_offset;
		for (p1 = b; p1 < e; p1 += sc->sectorsize) {
			g_aes_makekey(sc, o, &ekey, DIR_ENCRYPT);
			rijndael_blockEncrypt(&sc->ci, &ekey,
			    p1, sc->sectorsize * 8, p2);
			p2 += sc->sectorsize;
			o += sc->sectorsize;
		}
		bzero(&ekey, sizeof ekey);	/* destroy evidence */
		g_io_request(bp2, cp);
		break;
	case BIO_GETATTR:
		bp2 = g_clone_bio(bp);
		if (bp2 == NULL) {
			g_io_deliver(bp, ENOMEM);
			return;
		}
		bp2->bio_done = g_std_done;
		bp2->bio_offset += sc->sectorsize;
		g_io_request(bp2, cp);
		break;
	default:
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	}
	return;
}

static void
g_aes_orphan(struct g_consumer *cp)
{
	struct g_geom *gp;
	struct g_aes_softc *sc;

	g_trace(G_T_TOPOLOGY, "g_aes_orphan(%p/%s)", cp, cp->provider->name);
	g_topology_assert();
	KASSERT(cp->provider->error != 0,
		("g_aes_orphan with error == 0"));

	gp = cp->geom;
	sc = gp->softc;
	g_wither_geom(gp, cp->provider->error);
	bzero(sc, sizeof(struct g_aes_softc));	/* destroy evidence */
	g_free(sc);
	return;
}

static int
g_aes_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_geom *gp;
	struct g_consumer *cp;

	gp = pp->geom;
	cp = LIST_FIRST(&gp->consumer);
	/* On first open, grab an extra "exclusive" bit */
	if (cp->acr == 0 && cp->acw == 0 && cp->ace == 0)
		de++;
	/* ... and let go of it on last close */
	if ((cp->acr + dr) == 0 && (cp->acw + dw) == 0 && (cp->ace + de) == 1)
		de--;
	return (g_access(cp, dr, dw, de));
}

static struct g_geom *
g_aes_taste(struct g_class *mp, struct g_provider *pp, int flags __unused)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	struct g_aes_softc *sc;
	int error;
	u_int sectorsize;
	off_t mediasize;
	u_char *buf;

	g_trace(G_T_TOPOLOGY, "aes_taste(%s,%s)", mp->name, pp->name);
	g_topology_assert();
	gp = g_new_geomf(mp, "%s.aes", pp->name);
	cp = g_new_consumer(gp);
	g_attach(cp, pp);
	error = g_access(cp, 1, 0, 0);
	if (error) {
		g_detach(cp);
		g_destroy_consumer(cp);
		g_destroy_geom(gp);
		return (NULL);
	}
	buf = NULL;
	g_topology_unlock();
	do {
		if (gp->rank != 2)
			break;
		sectorsize = cp->provider->sectorsize;
		mediasize = cp->provider->mediasize;
		buf = g_read_data(cp, 0, sectorsize, &error);
		if (buf == NULL) {
			break;
		}
		sc = g_malloc(sizeof(struct g_aes_softc), M_WAITOK | M_ZERO);
		if (!memcmp(buf, aes_magic, strlen(aes_magic))) {
			sc->keying = KEY_ZERO;
		} else if (!memcmp(buf, aes_magic_random, 
		    strlen(aes_magic_random))) {
			sc->keying = KEY_RANDOM;
		} else if (!memcmp(buf, aes_magic_test, 
		    strlen(aes_magic_test))) {
			sc->keying = KEY_TEST;
		} else {
			g_free(sc);
			break;
		}
		g_free(buf);
		gp->softc = sc;
		sc->sectorsize = sectorsize;
		sc->mediasize = mediasize - sectorsize;
		rijndael_cipherInit(&sc->ci, MODE_CBC, NULL);
		if (sc->keying == KEY_TEST) {
			int i;
			u_char *p;

			p = sc->master_key;
			for (i = 0; i < (int)sizeof sc->master_key; i ++) 
				*p++ = i;
		}
		if (sc->keying == KEY_RANDOM) {
			int i;
			u_int32_t u;
			u_char *p;

			p = sc->master_key;
			for (i = 0; i < (int)sizeof sc->master_key; i += sizeof u) {
				u = arc4random();
				*p++ = u;
				*p++ = u >> 8;
				*p++ = u >> 16;
				*p++ = u >> 24;
			}
		}
		g_topology_lock();
		pp = g_new_providerf(gp, gp->name);
		pp->mediasize = mediasize - sectorsize;
		pp->sectorsize = sectorsize;
		g_error_provider(pp, 0);
		g_topology_unlock();
	} while(0);
	g_topology_lock();
	if (buf)
		g_free(buf);
	g_access(cp, -1, 0, 0);
	if (gp->softc != NULL) 
		return (gp);
	g_detach(cp);
	g_destroy_consumer(cp);
	g_destroy_geom(gp);
	return (NULL);
}

static struct g_class g_aes_class	= {
	.name = AES_CLASS_NAME,
	.version = G_VERSION,
	.taste = g_aes_taste,
	.start = g_aes_start,
	.orphan = g_aes_orphan,
	.spoiled = g_std_spoiled,
	.access = g_aes_access,
};

DECLARE_GEOM_CLASS(g_aes_class, g_aes);
