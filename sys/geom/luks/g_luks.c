/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Nicolas Provost <dev@npsoft.fr>
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

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/bio.h>
#include <sys/uuid.h>
#include <sys/uio.h>
#include <sys/sbuf.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/priority.h>
#include <sys/sched.h>
#include <geom/geom_ctl.h>
#include <geom/luks/g_luks.h>

static int g_luks_dbglvl = 0;

MALLOC_DEFINE(M_LUKS, "luks data", "GEOM_LUKS data");

#define G_LUKS_CLASS_NAME "LUKS"

FEATURE(geom_luks, "GEOM LUKS Disk Encryption");

int g_luks_debug = 0;

static struct g_luks_softc*
g_luks_create_softc(struct g_provider *pp, g_luks_state state)
{
	struct g_luks_softc *sc;

	sc = g_luks_malloc(sizeof(struct g_luks_softc), M_WAITOK | M_ZERO);
	if (sc == NULL)
		return (NULL);
	sc->state = state;
	if (pp != NULL) {
		sc->sectorsize = pp->sectorsize;
		sc->mediasize = pp->mediasize;
	}
	return (sc);
}

static void
g_luks_free_softc(struct g_luks_softc **sc)
{
	if ((*sc)->mk)
		g_luks_mfree(&(*sc)->mk, (*sc)->meta.mk_len);
	if ((*sc)->pbkpass)
		g_luks_mfree(&(*sc)->pbkpass, (*sc)->pbkpass_len);
	g_luks_mfree((uint8_t**) sc, sizeof(struct g_luks_softc));
}

static int
g_luks_process_sectors(struct g_luks_cipher_ctx *ctx,
			uint8_t* data, size_t data_len,
			uint64_t nsector, size_t sector_len)
{
	size_t len;
	int error = 0;

	G_LUKS_DEBUG(8, "process ctx=%p len=%zu sector=%lu",
			ctx, data_len, nsector);
	for (len = data_len ; error == 0 && len > 0; nsector++) {
		g_luks_cipher_setup_iv(ctx, nsector);
		error = g_luks_cipher_do(ctx, data, sector_len);
		len -= sector_len;
		data += sector_len;
	}
	return (error);
}

static int
g_luks_update_unmapped(struct bio *bp, uint8_t *buf,
			off_t offset, size_t len)
{
	struct uio uio;
	struct iovec iov[1];

	if (bp->bio_ma == NULL)
		return (EOPNOTSUPP);
	iov[0].iov_base = buf;
	iov[0].iov_len = len;
	uio.uio_iov = iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = offset;
	uio.uio_resid = len;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_WRITE;
	return (uiomove_fromphys(bp->bio_ma, bp->bio_ma_offset + offset,
				bp->bio_length, &uio));
}

static int
g_luks_get_unmapped(struct bio *bp, uint8_t **buf)
{
	struct uio uio;
	struct iovec iov[1];
	int error;

	if (bp->bio_ma == NULL)
		return (EOPNOTSUPP);
	*buf = g_luks_malloc(bp->bio_length, M_WAITOK);
	if (*buf == NULL)
		return (ENOMEM);
	iov[0].iov_base = *buf;
	iov[0].iov_len = bp->bio_length;
	uio.uio_iov = iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = 0;
	uio.uio_resid = bp->bio_length;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	error = uiomove_fromphys(bp->bio_ma, bp->bio_ma_offset,
					bp->bio_length, &uio);
	if (error != 0)
		g_luks_mfree(buf, bp->bio_length);

	return (error);
}

/************
 * IO FUNCS *
 ************/
static void
g_luks_done(struct bio *bp)
{
	struct g_luks_softc *sc;

	G_LUKS_DEBUG(8, "done flags=%x off=%li",
			bp->bio_flags, bp->bio_offset);
	if (bp->bio_pflags & G_LUKS_STATE_ALLOCATED) {
		sc = bp->bio_to->geom->softc;
		g_luks_mfree((uint8_t**)&bp->bio_data, bp->bio_length);
	}
	if (bp->bio_error == 0) {
		if (bp->bio_cmd == BIO_GETATTR) {
			if (strcmp("GEOM::physpath", bp->bio_attribute) == 0)
				strlcat(bp->bio_data, "/luks", bp->bio_length);
		}
	}
	g_std_done(bp);
}

static void
g_luks_io_pass(struct bio *bp, struct g_luks_softc *sc)
{
	struct bio *bp2;

	bp2 = g_clone_bio(bp);
	if (bp2 == NULL)
		g_io_deliver(bp, ENOMEM);
	else {
		bp2->bio_done = g_luks_done;
		bp2->bio_offset += sc->meta.payload_offset;
		g_io_request(bp2, sc->consumer);
	}
}

static void
g_luks_io_enqueue(struct bio *bp, struct g_luks_softc *sc)
{
	if (sc->state & G_LUKS_STATE_STOP) {
		g_io_deliver(bp, EIO);
		return;
	}
	mtx_lock(&sc->queue_mtx);
	bioq_insert_tail(&sc->queue, bp);
	mtx_unlock(&sc->queue_mtx);
	wakeup(sc);
}

static void
g_luks_io_getattr(struct bio *bp, struct g_luks_softc *sc)
{
	int error = 0;
	const char* name = bp->bio_attribute;

	G_LUKS_DEBUG(8, "getattr %s len=%li", name, bp->bio_length);
	if (strcmp("GEOM::ident", name) == 0) {
		if (bp->bio_length > G_LUKS_UUID_LEN) {
			bzero(bp->bio_data, bp->bio_length);
			memcpy(bp->bio_data, sc->meta.uuid, G_LUKS_UUID_LEN);
			bp->bio_completed = bp->bio_length;
		}
		g_io_deliver(bp, 0);
	}
	else if (strcmp("PART::isleaf", name) == 0)
		g_handleattr_int(bp, name, 1);
	else if (strcmp("GEOM::fwsectors", name) == 0)
		error = EOPNOTSUPP;
	else if (strcmp("GEOM::fwheads", name) == 0)
		error = EOPNOTSUPP;
	else
		error = EOPNOTSUPP;

	if (error != 0)
		g_io_deliver(bp, error);
	/* GEOM::candelete 4 */
	/* GEOM::physpath 1024 -> in g_luks_done */
}

static void
g_luks_io_not_supp(struct bio *bp, struct g_luks_softc *sc)
{
	g_io_deliver(bp, EOPNOTSUPP);
}

typedef void (*g_luks_io_func_t)(struct bio *, struct g_luks_softc *);

#define G_LUKS_IO_FUNCS 10

static const g_luks_io_func_t g_luks_io_funcs[G_LUKS_IO_FUNCS+1] = {
	g_luks_io_not_supp,
	g_luks_io_enqueue, /* BIO_READ */
	g_luks_io_enqueue, /* BIO_WRITE */
	g_luks_io_pass, /* BIO_DELETE: TODO shred data ? */
	g_luks_io_getattr, /* BIO_GETATTR */
	g_luks_io_pass, /* BIO_FLUSH */
	g_luks_io_not_supp, /* BIO_CMD0 */
	g_luks_io_not_supp, /* BIO_CMD1 */
	g_luks_io_not_supp, /* BIO_CMD2 */
	g_luks_io_pass, /* BIO_ZONE */
	g_luks_io_pass /* BIO_SPEEDUP */
};

static void
g_luks_start(struct bio *bp)
{
	struct g_luks_softc *sc;

	sc = bp->bio_to->geom->softc;
	if (sc == NULL) {
		g_io_deliver(bp, ENXIO);
		return;
	}

	/* LUKS CTL */
	if ((sc->state & G_LUKS_STATE_RUN) == 0) {
		g_luks_io_pass(bp, sc);
		return;
	}

	/* check worker */
	if (sc->worker->p_state == PRS_ZOMBIE) {
		if ((sc->state & G_LUKS_STATE_STOP) == 0) {
			sc->state |= G_LUKS_STATE_STOP;
			G_LUKS_DEBUG(0, "%s LUKS worker died",
					sc->geom->name);
		}
	}
	if (sc->state & G_LUKS_STATE_STOP) {
		g_io_deliver(bp, EIO);
		return;
	}

	G_LUKS_DEBUG(8, "cmd=%i data=%p flags=%x off=%li len=%li from=%s to=%s",
			bp->bio_cmd, bp->bio_data, bp->bio_flags,
			bp->bio_offset, bp->bio_length,
			bp->bio_from->geom->name,
			bp->bio_to->name);

	if (bp->bio_cmd > G_LUKS_IO_FUNCS)
		g_io_deliver(bp, EOPNOTSUPP);
	else
		g_luks_io_funcs[bp->bio_cmd](bp, sc);
}

/*****************
 * WORKER THREAD *
 *****************/
static void
g_luks_worker_read(struct bio *bp, struct g_luks_softc* sc)
{
	int error;
	off_t dev_off;
	uint8_t* buf;
	uint64_t offset;

	dev_off = sc->meta.payload_offset + bp->bio_offset;
	buf = g_read_data(sc->consumer, dev_off, bp->bio_length, &error);
	if (error == 0) {
		offset = bp->bio_offset / sc->meta.sector_len;
		error = g_luks_process_sectors(&sc->rctx, buf, bp->bio_length,
						offset, sc->meta.sector_len);
		if (error == 0) {
			if ((bp->bio_flags & BIO_UNMAPPED) != 0) {
				error = g_luks_update_unmapped(bp, buf, 0,
								bp->bio_length);
			}
			else
				memcpy(bp->bio_data, buf, bp->bio_length);
			bp->bio_completed = bp->bio_length;
		}
		zfree(buf, M_GEOM);
	}
	g_io_deliver(bp, error);
}

static void
g_luks_worker_write(struct bio *bp, struct g_luks_softc* sc)
{
	int error = 0;
	uint64_t offset;
	struct bio *bp2;
	uint8_t *buf;

	if (sc->state & G_LUKS_STATE_RO) {
		g_io_deliver(bp, EACCES);
		return;
	}

	offset = bp->bio_offset / sc->meta.sector_len;
	if ((bp->bio_flags & BIO_UNMAPPED) != 0)
		error = g_luks_get_unmapped(bp, &buf);
	else {
		buf = g_luks_malloc(bp->bio_length, M_WAITOK);
		if (buf == NULL)
			error = ENOMEM;
		else
			bcopy(bp->bio_data, buf, bp->bio_length);
	}
	if (error == 0) {
		error = g_luks_process_sectors(&sc->wctx, buf,
						bp->bio_length, offset,
						sc->meta.sector_len);
	}
	if (error == 0) {
		bp2 = g_clone_bio(bp);
		if (bp2 == NULL)
			error = ENOMEM;
		else {
			bp2->bio_data = buf;
			bp2->bio_offset += sc->meta.payload_offset;
			bp2->bio_done = g_luks_done;
			bp2->bio_pflags = G_LUKS_STATE_ALLOCATED;
			bp2->bio_flags &= ~BIO_UNMAPPED;
			g_io_request(bp2, sc->consumer);
			return;
		}
	}
	g_io_deliver(bp, error);
}

static void
g_luks_worker(void *arg)
{
	struct g_luks_softc *sc = arg;
	struct bio *bp;

	thread_lock(curthread);
	sched_prio(curthread, PRIBIO);
	thread_unlock(curthread);
	G_LUKS_DEBUG(1, "Starting LUKS worker for %s.", sc->geom->name);

	for( ; (sc->state & G_LUKS_STATE_STOP) == 0; ) {
		mtx_lock(&sc->queue_mtx);
		bp = bioq_takefirst(&sc->queue);
		mtx_unlock(&sc->queue_mtx);
		if (bp != NULL) {
			/* process this request, either a READ or a WRITE */
			if (bp->bio_cmd == BIO_READ)
				g_luks_worker_read(bp, sc);
			else
				g_luks_worker_write(bp, sc);
		}
	}
	G_LUKS_DEBUG(1, "Stopping LUKS worker for %s.", sc->geom->name);
	kproc_exit(0);
}

static int
g_luks_prepare(struct g_luks_softc *sc, int readonly, int onewr)
{
	bioq_init(&sc->queue);
	mtx_init(&sc->queue_mtx, "gluks:queue", NULL, MTX_DEF);
	if (onewr)
		sc->state |= G_LUKS_STATE_ONEWR;
	if (readonly)
		sc->state |= G_LUKS_STATE_RO;
	sc->state |= G_LUKS_STATE_RUN;

	g_luks_cipher_init(&sc->rctx, G_LUKS_COP_DECRYPT,
				sc->meta.cipher, sc->meta.mode,
				sc->mk, sc->meta.mk_len);

	if (readonly == 0)
		g_luks_cipher_init(&sc->wctx, G_LUKS_COP_ENCRYPT,
					sc->meta.cipher, sc->meta.mode,
					sc->mk, sc->meta.mk_len);

	/* master key no more needed */
	g_luks_mfree(&sc->mk, sc->meta.mk_len);

	return (kproc_create(g_luks_worker, sc, &sc->worker, 0, 0,
				"%s", sc->geom->name));
}

/************
 * OP FUNCS *
 ************/
static inline size_t g_luks_roundss(size_t len, size_t sectorsize)
{
	size_t sslen = len / sectorsize;

	if (len % sectorsize)
		sslen++;

	return (sslen * sectorsize);
}

static void
g_luks_destroy(struct g_luks_softc **sc)
{
	struct g_provider *pp;
	struct g_geom *gp;
	struct bio *bp;

	g_topology_assert();
	(*sc)->state |= G_LUKS_STATE_STOP;
	mtx_lock(&(*sc)->queue_mtx);
	for (bp = bioq_first(&(*sc)->queue); bp != NULL;
		bp = bioq_first(&(*sc)->queue)) {
		bioq_remove(&(*sc)->queue, bp);
		g_io_deliver(bp, EIO);
	}
	mtx_unlock(&(*sc)->queue_mtx);
	wakeup(*sc);
	mtx_lock(&(*sc)->queue_mtx);
	mtx_destroy(&(*sc)->queue_mtx);
	gp = (*sc)->geom;
	pp = LIST_FIRST(&gp->provider);
	if (pp != NULL)
		g_wither_provider(pp, ENXIO);

	gp->flags |= G_GEOM_WITHER;
	g_luks_free_softc(sc);
}

static int
g_luks_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_luks_softc *sc;

	g_topology_assert();
	sc = pp->geom->softc;

	if (sc == NULL) {
		if (pp->acr + dr == 0 && pp->acw + dw == 0 && pp->ace + de == 0)
			return (0);
		else
			return (EPERM);
	}
	else if ((sc->state & G_LUKS_STATE_RUN) != 0 &&
		(sc->state & G_LUKS_STATE_ONEWR) != 0) {
		/* Allow one writer and give it exclusive access. Allow
		 * multiple readers (possibly one with exclusive access).
		 */
		if (dw > 0) {
			if (sc->writers > 0)
				return (EPERM);
			else {
				sc->writers++;
				if (de != 1)
					de = 1;
			}
		}
		else if (dw < 0) {
			if (dw < -sc->writers || sc->writers == 0)
				return (EPERM);
			else {
				sc->writers += dw;
				if (sc->writers == 0)
					de = -1;
			}
		}
		if (de > 0 && pp->ace > 0)
			return (EPERM);
	}

	return (g_access(sc->consumer, dr, dw, de));
}

static void
g_luks_orphan(struct g_consumer *cp)
{
	g_trace(G_T_TOPOLOGY, "g_luks_orphan(%p/%s)", cp, cp->provider->name);
	g_topology_assert();
	g_luks_destroy((struct g_luks_softc**)&cp->geom->softc);
}

static const char* g_luks_ciphers[] = {
	"aes",
	NULL
};

static const char* g_luks_modes[] = {
	"ecb", "cbc-plain", "cbc-essiv:sha256", "xts-plain64",
	NULL
};

static const char *
g_luks_v1_error(int n)
{
	switch(n) {
	case -1:
		return "unable to read header";
	case -2:
		return "bad LUKS magic";
	case -3:
		return "unsupported LUKS version";		
	case -4:
		return "unsupported cipher algorithm";		
	case -5:
		return "unsupported encryption scheme";		
	case -6:
		return "unsupported digest algorithm";		
	case -7:
		return "invalid payload offset";
	case -8:
		return "bad slot content";		
	case -9:
		return "bad payload offset";		
	default:
		return "?";		
	}
}

static g_luks_mode
g_luks_find_mode(const char* s, size_t max_len)
{
	int i;

	for (i = 0; g_luks_modes[i]; i++) {
		if (strncmp(g_luks_modes[i], s, max_len) == 0)
			return (i+1);
	}
	return (0);
}

static int
g_luks_read_phdr(struct g_consumer *cp, struct g_luks_softc *sc)
{
	u_char *buf;
	size_t len;
	uint32_t off;
	int error = 0;
	int i;
	u_char *p;
	struct g_luks_metadata *meta = &sc->meta;

	len = g_luks_roundss(G_LUKS_PHDR_LEN, sc->sectorsize);
	g_topology_unlock();
	buf = g_read_data(cp, 0, len, &error);
	g_topology_lock();
	if (error != 0) {
		if (buf)
			g_free(buf);
		return -1;
	}

	/* parse the LUKS header */
	p = buf;
	if (memcmp(p, G_LUKS_MAGIC, 6) != 0)
		error = -2;
	else
		p += 6;
	if (error == 0) {
		if (*p++ == 0 && *p++ == 1) {
			meta->version = 1;
			meta->sector_len = G_LUKS_SECTOR_LEN;
		}
		else
			error = -3;
	}
	if (error == 0) {
		for (i = 0; g_luks_ciphers[i]; i++) {
			if (strncmp(g_luks_ciphers[i], p, 32) == 0) {
				meta->cipher = i + 1;
				break;
			}
		}
		if (meta->cipher == G_LUKS_CIPHER_UNKNOWN)
			error = -4;
		else
			p += 32;
	}
	if (error == 0) {
		meta->mode = g_luks_find_mode(p, 32);
		if (meta->mode == G_LUKS_MODE_UNKNOWN)
			error = -5;
		else
			p += 32;
	}
	if (error == 0) {
		meta->hash = g_luks_digest_from_str(p, 32);
		if (meta->hash == G_LUKS_HASH_UNKNOWN)
			error = -6;
		else {
			meta->hash_len = g_luks_digest_output_len(meta->hash);
			p += 32;
		}
	}
	if (error == 0) {
		meta->s_payload = be32dec(p);
		p += 4;
		meta->payload_offset = meta->s_payload * meta->sector_len;
		if (meta->payload_offset >= sc->mediasize)
			error = -7;
	}
	if (error == 0) {
		/* mk_len is 64 for xts-plain64 but here k1 == k2 */
		meta->mk_len = be32dec(p);
		p += 4;
		memcpy(meta->mk_digest, p, G_LUKS_DIGEST_LEN);
		p += G_LUKS_DIGEST_LEN;
		meta->mk_digest_len = G_LUKS_DIGEST_LEN;
		memcpy(meta->mk_digest_salt, p, G_LUKS_SALT_LEN);
		p += G_LUKS_SALT_LEN;
		meta->mk_digest_salt_len = G_LUKS_SALT_LEN;
		meta->mk_digest_iter = be32dec(p);
		p += 4;
		memcpy(meta->uuid, p, G_LUKS_UUID_LEN);
		p += G_LUKS_UUID_LEN;
		for (i = 0; i < G_LUKS_MAX_SLOTS && error == 0; i++) {
			meta->slot[i].active = be32dec(p);
			p += 4;
			if (meta->slot[i].active == G_LUKS_SLOT_ENABLED)
				meta->slot[i].active = 1;
			else if (meta->slot[i].active == G_LUKS_SLOT_DISABLED) {
				meta->slot[i].active = 0;
				p += 4 + G_LUKS_SALT_LEN + 4 + 4;
				continue;
			}
			else {
				error = -8;
				break;
			}
			meta->slot[i].iter = be32dec(p);
			p += 4;
			memcpy(meta->slot[i].salt, p, G_LUKS_SALT_LEN);
			p += G_LUKS_SALT_LEN;
			meta->slot[i].salt_len = G_LUKS_SALT_LEN, 
			off = meta->slot[i].s_offset = be32dec(p);
			p += 4;
			meta->slot[i].stripes = be32dec(p);
			p += 4;
			if (off >= meta->s_payload ||
				(off +
				(g_luks_roundss(meta->slot[i].stripes,
				meta->sector_len) / meta->sector_len)) >
					meta->s_payload) {
				error = -9;
				break;
			}
		}
	}
	zfree(buf, M_GEOM);

	return (error);
}

static inline int hex_char(uint8_t c)
{
	if (c >= '0' && c <= '9')
		return (c-'0');
	else if (c >= 'a' && c <= 'f')
		return (c-'a'+10);
	else if (c >= 'A' && c <= 'F')
		return (c-'A'+10);
	else
		return -1;
}

static int
g_luks_hexpass(const char *pass, size_t pass_len, uint8_t **hpass)
{
	size_t real_len;
	size_t i;
	int error = 0;
	int c1, c2;
	uint8_t c;

	if (pass_len % 2 != 0)
		return (EINVAL);

	real_len = pass_len / 2;
	*hpass = g_luks_malloc(real_len, M_WAITOK);
	for (i = 0; error == 0 && i < pass_len; i += 2) {
		c1 = hex_char(pass[i]);
		c2 = hex_char(pass[i+1]);
		if (c1 == -1 || c2 == -1) 
			error = EINVAL;
		else {
			c = (uint8_t)((c1 << 4) + c2);
			(*hpass)[i/2] = c;
		}
	}
	if (error != 0 && *hpass != NULL)
		g_luks_mfree(hpass, real_len);

	return (error);
}

static int
g_luks_compute_pbkpass(struct g_luks_softc *sc, int nkey,
			const char* passphrase, size_t pass_len, int hexpass)
{
	struct g_luks_metadata *meta = &sc->meta;
	uint8_t *hpass = NULL;
	size_t real_len;
	int error = 0;
	int alg = meta->hash;
	int iter = meta->slot[nkey].iter;
	const uint8_t* p;
	
	if (iter < G_LUKS_MIN_ITER)
		iter = G_LUKS_MIN_ITER;

	if (hexpass) {
		error = g_luks_hexpass(passphrase, pass_len, &hpass);
		if (error == 0) {
			real_len = pass_len / 2;
			p = hpass;
		}
	}
	else {
		p = passphrase;
		real_len = pass_len;
	}

	if (error == 0) {
		sc->pbkpass_len = meta->mk_len;
		sc->pbkpass = g_luks_malloc(sc->pbkpass_len, M_WAITOK);
		if (sc->pbkpass == NULL)
			error = ENOMEM;
		else
			error = g_luks_pbkdf2(p, real_len,
						meta->slot[nkey].salt,
						meta->slot[nkey].salt_len,
						sc->pbkpass, sc->pbkpass_len,
						iter, alg);
	}

	if (hpass != NULL)
		g_luks_mfree(&hpass, real_len);

	return (error);
}

static int
g_luks_decrypt_key_material(struct g_luks_softc *sc, uint8_t* kmat, size_t len)
{
	int error = 0;
	struct g_luks_cipher_ctx ctx;
	struct g_luks_metadata *meta = &sc->meta;
	int scnt;

	if ((len % sc->meta.sector_len) != 0)
		return (EINVAL);

	for (scnt = 0; error == 0 && len > 0;
		scnt++, len -= sc->meta.sector_len) {
		error = g_luks_cipher_init(&ctx, G_LUKS_COP_DECRYPT,
						meta->cipher, meta->mode,
						sc->pbkpass, sc->pbkpass_len);
		if (error == 0)
			error = g_luks_cipher_setup_iv(&ctx, scnt);
		if (error == 0)
			error = g_luks_cipher_do(&ctx, kmat,
						sc->meta.sector_len);

		kmat += sc->meta.sector_len;
	}
	g_luks_cipher_clear(&ctx);
	return (error);
}

/* Test if the device can be unlocked using slot #nkey and given passphrase. */
static int
g_luks_open_slot(struct g_consumer *cp, struct g_luks_softc *sc,
		int nkey, const char *passphrase, size_t pass_len, int hexpass)
{
	struct g_luks_metadata *meta = &sc->meta;
	uint8_t *kmat;
	size_t real_len;
	size_t strp_len;
	int error = 0;

	/* Compute PBKDF2 for passphrase and slot. */
	error = g_luks_compute_pbkpass(sc, nkey, passphrase, pass_len, hexpass);
	if (error != 0)
		return (error);

	/* Decrypt key material using the PBKDF2 of the passphrase. */
	strp_len = meta->slot[nkey].stripes * meta->mk_len;
	real_len = g_luks_roundss(strp_len, sc->sectorsize);
	g_topology_unlock();
	kmat = g_read_data(cp, meta->slot[nkey].s_offset * meta->sector_len,
				real_len, &error);
	g_topology_lock();
	if (kmat == NULL || error != 0) {
		if (kmat != NULL)
			zfree(kmat, M_GEOM);
		if (error == 0)
			error = EIO;
		g_luks_mfree(&sc->pbkpass, sc->pbkpass_len);
		return (error);
	}
	error = g_luks_decrypt_key_material(sc, kmat, strp_len);
	g_luks_mfree(&sc->pbkpass, sc->pbkpass_len);
	if (error) {
		zfree(kmat, M_GEOM);
		return (error);
	}
	
	/* Unsplit the candidate key material. */
	sc->mk = g_luks_malloc(meta->mk_len, M_WAITOK);
	error = g_luks_af_merge(meta->hash, meta->slot[nkey].stripes,
				kmat, strp_len, sc->mk, meta->mk_len);
	zfree(kmat, M_GEOM);
	if (error) {
		g_luks_mfree(&sc->mk, meta->mk_len);
		return (error);
	}
	
	/* Check that our master key candidate has a good PBKDF2. */
	sc->pbkpass = g_luks_malloc(meta->mk_digest_len, M_WAITOK);
	error = g_luks_pbkdf2(sc->mk, meta->mk_len,
				meta->mk_digest_salt, meta->mk_digest_salt_len, 
				sc->pbkpass, meta->mk_digest_len,
				meta->mk_digest_iter,
				meta->hash);
	if (error == 0) {
		error = memcmp(sc->pbkpass,
				meta->mk_digest, meta->mk_digest_len);
	}
	g_luks_mfree(&sc->pbkpass, meta->mk_digest_len);

	if (error != 0)
		g_luks_mfree(&sc->mk, meta->mk_len);

	return (error);
}

static int
g_luks_probe(struct g_provider *pp, struct g_class *mp,
		struct g_luks_softc **sc, g_luks_state state,
		struct g_consumer **csm, struct g_geom **geom)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	int error;
	int excl;

	*sc = g_luks_create_softc(pp, state);
	if (*sc == NULL)
		return (ENOMEM);
	gp = g_new_geomf(mp, "%s.luksprobe", pp->name);
	gp->softc = *sc;
	(*sc)->geom = gp;
	cp = g_new_consumer(gp);
	(*sc)->consumer = cp;
	error = g_attach(cp, pp);
	if (error == 0) {
		excl = (state & G_LUKS_STATE_OPEN) ? 1 : 0;
		error = g_access(cp, 1, 0, excl);
		if (error != 0)
			g_detach(cp);
	}
	if (error != 0) {
		g_destroy_consumer(cp);
		g_destroy_geom(gp);
		g_luks_free_softc(sc);
		return (error);
	}

	error = g_luks_read_phdr(cp, *sc);
	if (error != 0)
		g_luks_free_softc(sc);
	if (error == 0 && csm != NULL)
		*csm = cp;
	else {
		g_access(cp, -1, 0, -excl);
		g_detach(cp);
		g_destroy_consumer(cp);
	}
	if (error == 0 && geom != NULL)
		*geom = gp;
	else
		g_destroy_geom(gp);
	return (error);
}

static void
g_luks_wbe(uint8_t **p, uint32_t n, size_t len)
{
	size_t i;
	uint8_t *q = *p;

	for (i = len; i > 0; i--) {
		q[i-1] = (uint8_t)(n & 0xff);
		n >>= 8;
	}
	*p = q + len;
}

/* when formatting for version 1, we choose to fix some parameters:
 * - first key slot used
 * - digest algorithm: sha256 (thus a 32 or 64 bytes master key)
 * - iterations: randomly chosen between 0x10000 and 0x4ffff
 */
static int
g_luks_format_v1(struct g_class *mp, struct g_provider *pp,
		const uint8_t* pass, size_t pass_len,
		g_luks_cipher cipher, g_luks_mode mode)
{
	const int stripes = G_LUKS_STRIPES;
	const int hash = G_LUKS_HASH_SHA256;
	struct g_luks_softc *sc = NULL;
	struct g_luks_cipher_ctx cr;
	struct g_geom *gp;
	struct g_consumer *cp;
	size_t phdr_len;
	size_t i;
	size_t len;
	uint8_t* phdr;
	uint8_t* p;
	uint8_t* psl;
	size_t mk_len;
	size_t strp_len;
	size_t keym_len;
	uint8_t* pbk;
	uint8_t* mk;
	int iter, siter, slot_iter;
	struct uuid uuid = { 0 };
	int error;

	if (mode == G_LUKS_MODE_XTS_PLAIN64)
		mk_len = 64;
	else
		mk_len = 32;
	mk = g_luks_malloc(mk_len, M_WAITOK);
	if (mk == NULL)
		return (ENOMEM);
	pbk = g_luks_malloc(mk_len, M_WAITOK);
	if (pbk == NULL) {
		g_luks_mfree(&mk, mk_len);
		return (ENOMEM);
	}

	strp_len = mk_len * stripes;
	keym_len = g_luks_roundss(strp_len, G_LUKS_SLOT_ALIGN);
	phdr_len = g_luks_roundss(G_LUKS_SLOT_ALIGN +
					(G_LUKS_MAX_SLOTS * keym_len),
					G_LUKS_SECTOR_LEN);
	phdr = g_luks_malloc(phdr_len, M_WAITOK);
	if (phdr == NULL) {
		g_luks_mfree(&pbk, mk_len);
		g_luks_mfree(&mk, mk_len);
		return (ENOMEM);
	}
	for (i = 0, p = phdr; i < phdr_len; i += 4)
		g_luks_wbe(&p, arc4random(), 4);
	p = phdr;

	memcpy(p, G_LUKS_MAGIC, 6);
	p += 6;
	g_luks_wbe(&p, 1, 2); /* version */
	memset(p, 0, 32);
	snprintf(p, 32, "aes");
	p += 32;
	memset(p, 0, 32);
	snprintf(p, 32, "%s", g_luks_modes[mode-1]);
	p += 32;
	memset(p, 0, 32);
	snprintf(p, 32, "sha256");
	p += 32;
	g_luks_wbe(&p, phdr_len / G_LUKS_SECTOR_LEN, 4); /* payload sector */
	g_luks_wbe(&p, mk_len, 4); /* key len */
	p += 20; /* mk digest */
	p += 32; /* mk digest salt */
	iter = (arc4random() & 0xffff);
	iter += ((arc4random() & 0x3) + 1) << 16;
	g_luks_wbe(&p, iter, 4); /* iterations */

	/* uuid (40 bytes) */
	memset(p, 0, G_LUKS_UUID_LEN);
	kern_uuidgen(&uuid, 1); 
	snprintf_uuid(p, G_LUKS_UUID_LEN, &uuid);
	p += G_LUKS_UUID_LEN;
	psl = p;
	
	/* slots */
	for (i = 0; i < G_LUKS_MAX_SLOTS; i++) {
		g_luks_wbe(&p, (i == 0) ? G_LUKS_SLOT_ENABLED :
				G_LUKS_SLOT_DISABLED, 4);
		siter = (arc4random() & 0xffff);
		siter += ((arc4random() & 0x3) + 1) << 16;
		if (i == 0)
			slot_iter = siter;
		g_luks_wbe(&p, siter, 4); /* iterations */
		p += G_LUKS_SALT_LEN; /* salt */
		/* key material sector */
		g_luks_wbe(&p, ((i*keym_len) / G_LUKS_SECTOR_LEN) +
				G_LUKS_SLOT_ALIGN / G_LUKS_SECTOR_LEN, 4);
		g_luks_wbe(&p, stripes, 4);
	}

	/* compute PBKDF2 of passphrase */
	error = g_luks_pbkdf2(pass, pass_len, psl + 8, G_LUKS_SALT_LEN,
				pbk, mk_len, slot_iter, hash);

	if (error == 0) {
		/* generate master key */
		for (i = 0; i < mk_len; i++)
			mk[i] = arc4random() & 0xff; 

		/* compute master key digest */
		error = g_luks_pbkdf2(mk, mk_len,
				phdr + G_LUKS_SALT_OFFSET, G_LUKS_SALT_LEN,
				phdr + G_LUKS_DIGEST_OFFSET, G_LUKS_DIGEST_LEN,
				iter, hash);
	}

	/* split and encrypt master key for first slot */
	if (error == 0) {
		error = g_luks_af_split(hash, stripes, mk, mk_len,
					phdr + G_LUKS_SLOT_ALIGN, strp_len);
	}
	g_luks_mfree(&mk, mk_len);

	if (error == 0) {
		p = phdr + G_LUKS_SLOT_ALIGN;
		error = g_luks_cipher_init(&cr, G_LUKS_COP_ENCRYPT,
						cipher, mode, pbk, mk_len);
		if (error == 0)
			error = g_luks_process_sectors(&cr,
						phdr + G_LUKS_SLOT_ALIGN,
						strp_len, 0,
						G_LUKS_SECTOR_LEN);
		g_luks_cipher_clear(&cr);
	}
	g_luks_mfree(&pbk, mk_len);

	/* write to provider */
	if (error == 0) {
		sc = g_luks_create_softc(pp, G_LUKS_STATE_FORMAT);
		if (sc == NULL)
			error = ENOMEM;
	}
	if (error == 0) {
		gp = g_new_geomf(mp, "%s.luksformat", pp->name);
		gp->softc = sc;
		sc->geom = gp;
		cp = g_new_consumer(gp);
		sc->consumer = cp;
		error = g_attach(cp, pp);
		if (error == 0) {
			error = g_access(cp, 1, 1, 1);
			if (error == 0) {
				for (len = 0, p = phdr;
					error == 0 && len < phdr_len;
					len += G_LUKS_SECTOR_LEN) {
					g_topology_unlock();
					error = g_write_data(cp, len, p,
							G_LUKS_SECTOR_LEN);
					g_topology_lock();
					p += G_LUKS_SECTOR_LEN;
				}
				g_access(cp, -1, -1, -1);
			}
		}
		g_detach(cp);
		g_destroy_consumer(cp);
		g_destroy_geom(gp);
	}

	if (sc != NULL)
		g_luks_free_softc(&sc);
	g_luks_mfree(&phdr, phdr_len);
	return (error);
}

/************
 * GEOM CTL *
 ************/
static void
g_luks_ctl_open(struct gctl_req *req, struct g_class *mp,
			struct g_provider *pp)
{
	struct g_geom *gp = NULL;
	struct g_geom_alias *gap;
	struct g_provider *prv;
	struct g_consumer *cp;
	struct g_luks_softc *sc;
	const char *pass;
	size_t pass_len;
	char *name;
	int error, len, i, hexpass, readonly, onewr, *arg;

	g_trace(G_T_TOPOLOGY, "g_luks open (%s)", pp->name);
	g_topology_assert();

	/* exit now if a LUKS provider already exists */
	len = strlen(pp->name)+6;
	name = g_luks_malloc(len, M_WAITOK | M_ZERO);
	snprintf(name, len, "%s.luks", pp->name);
	prv = g_provider_by_name(name);
	g_luks_mfree((uint8_t**)&name, len);
	if (prv != NULL) {
		gctl_error(req, "a LUKS provider is already active");
		return;
	}

	/* get configuration */
	pass = gctl_get_asciiparam(req, "arg1");
	if (pass == NULL) {
		gctl_error(req, "missing LUKS passphrase");
		return;
	}
	pass_len = strlen(pass);
	arg = gctl_get_paraml(req, "hex", sizeof(int));
	if (arg != NULL && *arg != 0)
		hexpass = 1;
	else
		hexpass = 0;
	arg = gctl_get_paraml(req, "readonly", sizeof(int));
	if (arg != NULL && *arg != 0)
		readonly = 1;
	else
		readonly = 0;
	arg = gctl_get_paraml(req, "onewriter", sizeof(int));
	if (arg != NULL && *arg != 0)
		onewr = 1;
	else
		onewr = 0;

	/* probe for LUKS header */
	error = g_luks_probe(pp, mp, &sc, G_LUKS_STATE_OPEN, &cp, &gp);
	if (error > 0) {
		gctl_error(req, "could not attach device");
		return;
	}
	else if (error < 0) {
		gctl_error(req, "%s isn't a LUKS device (%s)", pp->name,
				g_luks_v1_error(error));
		return;
	}

	/* try to open the encrypted device */
	for (i = 0; i < G_LUKS_MAX_SLOTS; i++) {
		if (sc->meta.slot[i].active != 1)
			continue;
		error = g_luks_open_slot(cp, sc, i, pass, pass_len, hexpass);
		if (error == 0 || error == EINVAL)
			break;
	}
	g_access(cp, -1, 0, -1);
	g_detach(cp);
	g_destroy_consumer(cp);
	g_destroy_geom(gp);
	if (error == EINVAL)
		gctl_error(req, "invalid passphrase content");
	else if (i >= G_LUKS_MAX_SLOTS) {
		error = EINVAL;
		gctl_error(req, "bad LUKS passphrase");
	}
	if (error != 0) {
		g_luks_free_softc(&sc);
		return;
	}

	/* create LUKS provider for our open request */
	gp = g_new_geomf(mp, "%s.luks", pp->name);
	gp->softc = sc;
	sc->geom = gp;
	cp = g_new_consumer(gp);
	sc->consumer = cp;
	error = g_attach(cp, pp);
	if (error == 0) {
		if (readonly)
			error = g_access(cp, 1, 0, 0);
		else
			error = g_access(cp, 1, 1, 1);
		if (error != 0) 
			g_detach(cp);
	}
	if (error == 0)
		error = g_luks_prepare(sc, readonly, onewr);
	if (error != 0) {
		gctl_error(req, "could not create LUKS instance");
		g_destroy_consumer(cp);
		g_destroy_geom(gp);
		g_luks_free_softc(&sc);
		return;
	}

	prv = g_new_providerf(gp, "%s", gp->name);
	prv->flags |= G_PF_ACCEPT_UNMAPPED;
	prv->mediasize = sc->mediasize - sc->meta.payload_offset;
	prv->sectorsize = sc->meta.sector_len;
	LIST_FOREACH(gap, &pp->aliases, ga_next)
		g_provider_add_alias(prv, "%s.luks", gap->ga_alias);	
	g_error_provider(prv, 0);
	G_LUKS_DEBUG(0, "Device %s created%s.", gp->name,
		(readonly ? " in read-only mode" : ""));
}

static void
g_luks_ctl_info(struct gctl_req *req, struct g_class *mp,
			struct g_provider *pp)
{
	struct sbuf *sb;
	struct g_luks_softc *sc;
	int error, i, quiet;
	int *arg;

	g_trace(G_T_TOPOLOGY, "g_luks info (%s)", pp->name);
	g_topology_assert();

	arg = gctl_get_paraml(req, "quiet", sizeof(int));
	quiet = (arg != NULL && *arg != 0) ? 1 : 0;
	error = g_luks_probe(pp, mp, &sc, G_LUKS_STATE_INFO, NULL, NULL);
	G_LUKS_DEBUG(0, "probe <%s> error=%i", pp->name, error);
	if (error > 0) {
		if (quiet == 0)
			printf("LUKS: could not attach device %s\n", pp->name);
		req->nerror = error;
		return;
	}
	else if (error < 0) {
		if (quiet == 0)
			printf("%s isn't a supported LUKS version 1 device "
				"(%s)\n", pp->name, g_luks_v1_error(error));
		req->nerror = EOPNOTSUPP;
		return;
	}
	else if (quiet) {
		g_luks_free_softc(&sc);
		return;
	}

	sb = sbuf_new_auto();
	sbuf_printf(sb, "Found LUKS version 1 header on %s\n", pp->name);
	sbuf_printf(sb, "Scheme: aes-%s\n", g_luks_modes[sc->meta.mode-1]);
	sbuf_printf(sb, "Key length: %u\n", sc->meta.mk_len);
	sbuf_printf(sb, "Active slot(s):");
	for (i = 0; i < G_LUKS_MAX_SLOTS; i++) {
		if (sc->meta.slot[i].active)
			sbuf_printf(sb, " %i", i+1);
	}
	sbuf_printf(sb, "\nUUID: ");
	for (i = 0; i < G_LUKS_UUID_LEN; i++)
		sbuf_printf(sb, "%c", sc->meta.uuid[i]);
	sbuf_printf(sb, "\n");
	sbuf_printf(sb, "Sector size: %zu\n", sc->meta.sector_len);
	sbuf_printf(sb, "Unencrypted media length: %li sectors\n",
			pp->mediasize / sc->meta.sector_len -
			sc->meta.s_payload);
	sbuf_finish(sb);
	printf("%s", sbuf_data(sb));
	sbuf_delete(sb);
	g_luks_free_softc(&sc);
}

static int
g_luks_ctl_close(struct gctl_req *req, struct g_class *mp,
			struct g_geom* gp)
{
	g_topology_assert();
	if (gp->softc)
		g_luks_destroy((struct g_luks_softc**)&gp->softc);
	g_wither_geom_close(gp, ENXIO);
	G_LUKS_DEBUG(0, "Device %s destroyed.", gp->name);
	return (0);
}

static void
g_luks_ctl_format(struct gctl_req *req, struct g_class *mp,
			struct g_provider *pp)
{
	const char *pass;
	const char *scheme;
	size_t pass_len;
	int error, *arg;
	uint8_t* hpass = NULL;
	const uint8_t *p;
	g_luks_cipher cipher;
	g_luks_mode mode;

	g_trace(G_T_TOPOLOGY, "g_luks format (%s)", pp->name);
	g_topology_assert();

	/* get configuration */
	arg = gctl_get_paraml(req, "yes", sizeof(int));
	if (arg == NULL || *arg != 1) {
		gctl_error(req, "you must confirm that all data "
				"on %s will be destroyed", pp->name);
		return;
	}
	scheme = gctl_get_asciiparam(req, "arg1");
	if (scheme == NULL) {
		gctl_error(req, "missing encryption scheme");
		return;
	}
	if (strncmp(scheme, "aes-", 4) != 0) {
		gctl_error(req, "unsupported encryption algorithm");
		return;
	}
	cipher = G_LUKS_CIPHER_AES;
	mode = g_luks_find_mode(scheme + 4, strlen(scheme) - 4);
	if (mode == G_LUKS_MODE_UNKNOWN) {
		gctl_error(req, "unsupported encryption scheme");
		return;
	}
	pass = gctl_get_asciiparam(req, "arg2");
	if (pass == NULL) {
		gctl_error(req, "missing LUKS passphrase");
		return;
	}
	pass_len = strlen(pass);
	arg = gctl_get_paraml(req, "hex", sizeof(int));
	if (arg != NULL && *arg != 0) {
		if (g_luks_hexpass(pass, pass_len, &hpass) != 0) {
			gctl_error(req, "bad hexadecimal passphrase");
			return;
		}
		p = hpass;
		pass_len /= 2;
	}
	else
		p = pass;

	error = g_luks_format_v1(mp, pp, p, pass_len, cipher, mode);
	if (error != 0) {
		gctl_error(req, "Failed to LUKS-format device %s (%i)",
				pp->name, error);
	}
	if (hpass != NULL)
		g_luks_mfree(&hpass, pass_len);
}

static void
g_luks_ctlreq(struct gctl_req *req, struct g_class *mp, char const *verb)
{
	struct g_geom *gp;
	struct g_provider *pp;

	if (strcmp(verb, "open") == 0) {
		pp = gctl_get_provider(req, "arg0");
		if (pp != NULL)
			g_luks_ctl_open(req, mp, pp);
		else
			gctl_error(req, "unknown device");
 	}	
	else if (strcmp(verb, "info") == 0) {
		pp = gctl_get_provider(req, "arg0");
		if (pp != NULL)
			g_luks_ctl_info(req, mp, pp);
		else
			gctl_error(req, "unknown device");
 	}	
	else if (strcmp(verb, "format") == 0) {
		pp = gctl_get_provider(req, "arg0");
		if (pp != NULL)
			g_luks_ctl_format(req, mp, pp);
		else
			gctl_error(req, "unknown device");
 	}	
	else if (strcmp(verb, "close") == 0) {
		gp = gctl_get_geom(req, mp, "arg0");
		if (gp != NULL)
			g_luks_ctl_close(req, mp, gp);
		else
			gctl_error(req, "unknown provider");
	}
	else 
		gctl_error(req, "unknown verb");
}

static struct g_class g_luks_class = {
	.name = G_LUKS_CLASS_NAME,
	.version = G_VERSION,
	.ctlreq = g_luks_ctlreq,
	.start = g_luks_start,
	.orphan = g_luks_orphan,
	.access = g_luks_access,
	.spoiled = g_std_spoiled,
};

DECLARE_GEOM_CLASS(g_luks_class, g_luks);
MODULE_VERSION(geom_luks, 1);
