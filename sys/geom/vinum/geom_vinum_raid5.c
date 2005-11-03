/*-
 * Copyright (c) 2004 Lukas Ertl
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/geom/vinum/geom_vinum_raid5.c,v 1.10 2004/11/26 11:59:51 le Exp $");

#include <sys/param.h>
#include <sys/bio.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/systm.h>

#include <geom/geom.h>
#include <geom/vinum/geom_vinum_var.h>
#include <geom/vinum/geom_vinum_raid5.h>
#include <geom/vinum/geom_vinum.h>

int	gv_raid5_offset(struct gv_plex *, off_t, off_t, off_t *, off_t *,
	    int *, int *);

/*
 * Check if the stripe that the work packet wants is already being used by
 * some other work packet.
 */
int
gv_stripe_active(struct gv_plex *p, struct bio *bp)
{
	struct gv_raid5_packet *wp, *owp;
	int overlap;

	wp = bp->bio_driver1;
	if (wp->lockbase == -1)
		return (0);

	overlap = 0;
	TAILQ_FOREACH(owp, &p->packets, list) {
		if (owp == wp)
			break;
		if ((wp->lockbase >= owp->lockbase) &&
		    (wp->lockbase <= owp->lockbase + owp->length)) {
			overlap++;
			break;
		}
		if ((wp->lockbase <= owp->lockbase) &&
		    (wp->lockbase + wp->length >= owp->lockbase)) {
			overlap++;
			break;
		}
	}

	return (overlap);
}

int
gv_check_raid5(struct gv_plex *p, struct gv_raid5_packet *wp, struct bio *bp,
    caddr_t addr, off_t boff, off_t bcount)
{
	struct gv_sd *parity, *s;
	struct gv_bioq *bq;
	struct bio *cbp, *pbp;
	int i, psdno;
	off_t real_len, real_off;

	if (p == NULL || LIST_EMPTY(&p->subdisks))
		return (ENXIO);

	gv_raid5_offset(p, boff, bcount, &real_off, &real_len, NULL, &psdno);

	/* Find the right subdisk. */
	parity = NULL;
	i = 0;
	LIST_FOREACH(s, &p->subdisks, in_plex) {
		if (i == psdno) {
			parity = s;
			break;
		}
		i++;
	}

	/* Parity stripe not found. */
	if (parity == NULL)
		return (ENXIO);

	if (parity->state != GV_SD_UP)
		return (ENXIO);

	wp->length = real_len;
	wp->data = addr;
	wp->lockbase = real_off;

	/* Read all subdisks. */
	LIST_FOREACH(s, &p->subdisks, in_plex) {
		/* Skip the parity subdisk. */
		if (s == parity)
			continue;

		cbp = g_clone_bio(bp);
		if (cbp == NULL)
			return (ENOMEM);
		cbp->bio_cmd = BIO_READ;
		cbp->bio_data = g_malloc(real_len, M_WAITOK);
		cbp->bio_cflags |= GV_BIO_MALLOC;
		cbp->bio_offset = real_off;
		cbp->bio_length = real_len;
		cbp->bio_done = gv_plex_done;
		cbp->bio_caller2 = s->consumer;
		cbp->bio_driver1 = wp;

		GV_ENQUEUE(bp, cbp, pbp);

		bq = g_malloc(sizeof(*bq), M_WAITOK | M_ZERO);
		bq->bp = cbp;
		TAILQ_INSERT_TAIL(&wp->bits, bq, queue);
	}

	/* Read the parity data. */
	cbp = g_clone_bio(bp);
	if (cbp == NULL)
		return (ENOMEM);
	cbp->bio_cmd = BIO_READ;
	cbp->bio_data = g_malloc(real_len, M_WAITOK | M_ZERO);
	cbp->bio_cflags |= GV_BIO_MALLOC;
	cbp->bio_offset = real_off;
	cbp->bio_length = real_len;
	cbp->bio_done = gv_plex_done;
	cbp->bio_caller2 = parity->consumer;
	cbp->bio_driver1 = wp;
	wp->waiting = cbp;

	/*
	 * In case we want to rebuild the parity, create an extra BIO to write
	 * it out.  It also acts as buffer for the XOR operations.
	 */
	cbp = g_clone_bio(bp);
	if (cbp == NULL)
		return (ENOMEM);
	cbp->bio_data = addr;
	cbp->bio_offset = real_off;
	cbp->bio_length = real_len;
	cbp->bio_done = gv_plex_done;
	cbp->bio_caller2 = parity->consumer;
	cbp->bio_driver1 = wp;
	wp->parity = cbp;

	return (0);
}

/* Rebuild a degraded RAID5 plex. */
int
gv_rebuild_raid5(struct gv_plex *p, struct gv_raid5_packet *wp, struct bio *bp,
    caddr_t addr, off_t boff, off_t bcount)
{
	struct gv_sd *broken, *s;
	struct gv_bioq *bq;
	struct bio *cbp, *pbp;
	off_t real_len, real_off;

	if (p == NULL || LIST_EMPTY(&p->subdisks))
		return (ENXIO);

	gv_raid5_offset(p, boff, bcount, &real_off, &real_len, NULL, NULL);

	/* Find the right subdisk. */
	broken = NULL;
	LIST_FOREACH(s, &p->subdisks, in_plex) {
		if (s->state != GV_SD_UP)
			broken = s;
	}

	/* Broken stripe not found. */
	if (broken == NULL)
		return (ENXIO);

	switch (broken->state) {
	case GV_SD_UP:
		return (EINVAL);

	case GV_SD_STALE:
		if (!(bp->bio_cflags & GV_BIO_REBUILD))
			return (ENXIO);

		printf("GEOM_VINUM: sd %s is reviving\n", broken->name);
		gv_set_sd_state(broken, GV_SD_REVIVING, GV_SETSTATE_FORCE);
		break;

	case GV_SD_REVIVING:
		break;

	default:
		/* All other subdisk states mean it's not accessible. */
		return (ENXIO);
	}

	wp->length = real_len;
	wp->data = addr;
	wp->lockbase = real_off;

	KASSERT(wp->length >= 0, ("gv_rebuild_raid5: wp->length < 0"));

	/* Read all subdisks. */
	LIST_FOREACH(s, &p->subdisks, in_plex) {
		/* Skip the broken subdisk. */
		if (s == broken)
			continue;

		cbp = g_clone_bio(bp);
		if (cbp == NULL)
			return (ENOMEM);
		cbp->bio_cmd = BIO_READ;
		cbp->bio_data = g_malloc(real_len, M_WAITOK);
		cbp->bio_cflags |= GV_BIO_MALLOC;
		cbp->bio_offset = real_off;
		cbp->bio_length = real_len;
		cbp->bio_done = gv_plex_done;
		cbp->bio_caller2 = s->consumer;
		cbp->bio_driver1 = wp;

		GV_ENQUEUE(bp, cbp, pbp);

		bq = g_malloc(sizeof(*bq), M_WAITOK | M_ZERO);
		bq->bp = cbp;
		TAILQ_INSERT_TAIL(&wp->bits, bq, queue);
	}

	/* Write the parity data. */
	cbp = g_clone_bio(bp);
	if (cbp == NULL)
		return (ENOMEM);
	cbp->bio_data = g_malloc(real_len, M_WAITOK | M_ZERO);
	cbp->bio_cflags |= GV_BIO_MALLOC;
	cbp->bio_offset = real_off;
	cbp->bio_length = real_len;
	cbp->bio_done = gv_plex_done;
	cbp->bio_caller2 = broken->consumer;
	cbp->bio_driver1 = wp;
	cbp->bio_cflags |= GV_BIO_REBUILD;
	wp->parity = cbp;

	p->synced = boff;

	return (0);
}

/* Build a request group to perform (part of) a RAID5 request. */
int
gv_build_raid5_req(struct gv_plex *p, struct gv_raid5_packet *wp,
    struct bio *bp, caddr_t addr, off_t boff, off_t bcount)
{
	struct g_geom *gp;
	struct gv_sd *broken, *original, *parity, *s;
	struct gv_bioq *bq;
	struct bio *cbp, *pbp;
	int i, psdno, sdno, type;
	off_t real_len, real_off;

	gp = bp->bio_to->geom;

	if (p == NULL || LIST_EMPTY(&p->subdisks))
		return (ENXIO);

	/* We are optimistic and assume that this request will be OK. */
#define	REQ_TYPE_NORMAL		0
#define	REQ_TYPE_DEGRADED	1
#define	REQ_TYPE_NOPARITY	2

	type = REQ_TYPE_NORMAL;
	original = parity = broken = NULL;

	gv_raid5_offset(p, boff, bcount, &real_off, &real_len, &sdno, &psdno);

	/* Find the right subdisks. */
	i = 0;
	LIST_FOREACH(s, &p->subdisks, in_plex) {
		if (i == sdno)
			original = s;
		if (i == psdno)
			parity = s;
		if (s->state != GV_SD_UP)
			broken = s;
		i++;
	}

	if ((original == NULL) || (parity == NULL))
		return (ENXIO);

	/* Our data stripe is missing. */
	if (original->state != GV_SD_UP)
		type = REQ_TYPE_DEGRADED;
	/* Our parity stripe is missing. */
	if (parity->state != GV_SD_UP) {
		/* We cannot take another failure if we're already degraded. */
		if (type != REQ_TYPE_NORMAL)
			return (ENXIO);
		else
			type = REQ_TYPE_NOPARITY;
	}

	wp->length = real_len;
	wp->data = addr;
	wp->lockbase = real_off;

	KASSERT(wp->length >= 0, ("gv_build_raid5_request: wp->length < 0"));

	if ((p->flags & GV_PLEX_SYNCING) && (boff + real_len < p->synced))
		type = REQ_TYPE_NORMAL;

	switch (bp->bio_cmd) {
	case BIO_READ:
		/*
		 * For a degraded read we need to read in all stripes except
		 * the broken one plus the parity stripe and then recalculate
		 * the desired data.
		 */
		if (type == REQ_TYPE_DEGRADED) {
			bzero(wp->data, wp->length);
			LIST_FOREACH(s, &p->subdisks, in_plex) {
				/* Skip the broken subdisk. */
				if (s == broken)
					continue;
				cbp = g_clone_bio(bp);
				if (cbp == NULL)
					return (ENOMEM);
				cbp->bio_data = g_malloc(real_len, M_WAITOK);
				cbp->bio_cflags |= GV_BIO_MALLOC;
				cbp->bio_offset = real_off;
				cbp->bio_length = real_len;
				cbp->bio_done = gv_plex_done;
				cbp->bio_caller2 = s->consumer;
				cbp->bio_driver1 = wp;

				GV_ENQUEUE(bp, cbp, pbp);

				bq = g_malloc(sizeof(*bq), M_WAITOK | M_ZERO);
				bq->bp = cbp;
				TAILQ_INSERT_TAIL(&wp->bits, bq, queue);
			}

		/* A normal read can be fulfilled with the original subdisk. */
		} else {
			cbp = g_clone_bio(bp);
			if (cbp == NULL)
				return (ENOMEM);
			cbp->bio_offset = real_off;
			cbp->bio_length = real_len;
			cbp->bio_data = addr;
			cbp->bio_done = g_std_done;
			cbp->bio_caller2 = original->consumer;

			GV_ENQUEUE(bp, cbp, pbp);
		}
		wp->lockbase = -1;

		break;

	case BIO_WRITE:
		/*
		 * A degraded write means we cannot write to the original data
		 * subdisk.  Thus we need to read in all valid stripes,
		 * recalculate the parity from the original data, and then
		 * write the parity stripe back out.
		 */
		if (type == REQ_TYPE_DEGRADED) {
			/* Read all subdisks. */
			LIST_FOREACH(s, &p->subdisks, in_plex) {
				/* Skip the broken and the parity subdisk. */
				if ((s == broken) || (s == parity))
					continue;

				cbp = g_clone_bio(bp);
				if (cbp == NULL)
					return (ENOMEM);
				cbp->bio_cmd = BIO_READ;
				cbp->bio_data = g_malloc(real_len, M_WAITOK);
				cbp->bio_cflags |= GV_BIO_MALLOC;
				cbp->bio_offset = real_off;
				cbp->bio_length = real_len;
				cbp->bio_done = gv_plex_done;
				cbp->bio_caller2 = s->consumer;
				cbp->bio_driver1 = wp;

				GV_ENQUEUE(bp, cbp, pbp);

				bq = g_malloc(sizeof(*bq), M_WAITOK | M_ZERO);
				bq->bp = cbp;
				TAILQ_INSERT_TAIL(&wp->bits, bq, queue);
			}

			/* Write the parity data. */
			cbp = g_clone_bio(bp);
			if (cbp == NULL)
				return (ENOMEM);
			cbp->bio_data = g_malloc(real_len, M_WAITOK);
			cbp->bio_cflags |= GV_BIO_MALLOC;
			bcopy(addr, cbp->bio_data, real_len);
			cbp->bio_offset = real_off;
			cbp->bio_length = real_len;
			cbp->bio_done = gv_plex_done;
			cbp->bio_caller2 = parity->consumer;
			cbp->bio_driver1 = wp;
			wp->parity = cbp;

		/*
		 * When the parity stripe is missing we just write out the data.
		 */
		} else if (type == REQ_TYPE_NOPARITY) {
			cbp = g_clone_bio(bp);
			if (cbp == NULL)
				return (ENOMEM);
			cbp->bio_offset = real_off;
			cbp->bio_length = real_len;
			cbp->bio_data = addr;
			cbp->bio_done = gv_plex_done;
			cbp->bio_caller2 = original->consumer;
			cbp->bio_driver1 = wp;

			GV_ENQUEUE(bp, cbp, pbp);

			bq = g_malloc(sizeof(*bq), M_WAITOK | M_ZERO);
			bq->bp = cbp;
			TAILQ_INSERT_TAIL(&wp->bits, bq, queue);

		/*
		 * A normal write request goes to the original subdisk, then we
		 * read in all other stripes, recalculate the parity and write
		 * out the parity again.
		 */
		} else {
			/* Read old parity. */
			cbp = g_clone_bio(bp);
			if (cbp == NULL)
				return (ENOMEM);
			cbp->bio_cmd = BIO_READ;
			cbp->bio_data = g_malloc(real_len, M_WAITOK);
			cbp->bio_cflags |= GV_BIO_MALLOC;
			cbp->bio_offset = real_off;
			cbp->bio_length = real_len;
			cbp->bio_done = gv_plex_done;
			cbp->bio_caller2 = parity->consumer;
			cbp->bio_driver1 = wp;

			GV_ENQUEUE(bp, cbp, pbp);

			bq = g_malloc(sizeof(*bq), M_WAITOK | M_ZERO);
			bq->bp = cbp;
			TAILQ_INSERT_TAIL(&wp->bits, bq, queue);

			/* Read old data. */
			cbp = g_clone_bio(bp);
			if (cbp == NULL)
				return (ENOMEM);
			cbp->bio_cmd = BIO_READ;
			cbp->bio_data = g_malloc(real_len, M_WAITOK);
			cbp->bio_cflags |= GV_BIO_MALLOC;
			cbp->bio_offset = real_off;
			cbp->bio_length = real_len;
			cbp->bio_done = gv_plex_done;
			cbp->bio_caller2 = original->consumer;
			cbp->bio_driver1 = wp;

			GV_ENQUEUE(bp, cbp, pbp);

			bq = g_malloc(sizeof(*bq), M_WAITOK | M_ZERO);
			bq->bp = cbp;
			TAILQ_INSERT_TAIL(&wp->bits, bq, queue);

			/* Write new data. */
			cbp = g_clone_bio(bp);
			if (cbp == NULL)
				return (ENOMEM);
			cbp->bio_data = addr;
			cbp->bio_offset = real_off;
			cbp->bio_length = real_len;
			cbp->bio_done = gv_plex_done;
			cbp->bio_caller2 = original->consumer;

			cbp->bio_driver1 = wp;

			/*
			 * We must not write the new data until the old data
			 * was read, so hold this BIO back until we're ready
			 * for it.
			 */
			wp->waiting = cbp;

			/* The final bio for the parity. */
			cbp = g_clone_bio(bp);
			if (cbp == NULL)
				return (ENOMEM);
			cbp->bio_data = g_malloc(real_len, M_WAITOK | M_ZERO);
			cbp->bio_cflags |= GV_BIO_MALLOC;
			cbp->bio_offset = real_off;
			cbp->bio_length = real_len;
			cbp->bio_done = gv_plex_done;
			cbp->bio_caller2 = parity->consumer;
			cbp->bio_driver1 = wp;

			/* Remember that this is the BIO for the parity data. */
			wp->parity = cbp;
		}
		break;

	default:
		return (EINVAL);
	}

	return (0);
}

/* Calculate the offsets in the various subdisks for a RAID5 request. */
int
gv_raid5_offset(struct gv_plex *p, off_t boff, off_t bcount, off_t *real_off,
    off_t *real_len, int *sdno, int *psdno)
{
	int sd, psd;
	off_t len_left, stripeend, stripeoff, stripestart;

	/* The number of the subdisk containing the parity stripe. */
	psd = p->sdcount - 1 - ( boff / (p->stripesize * (p->sdcount - 1))) %
	    p->sdcount;
	KASSERT(psdno >= 0, ("gv_raid5_offset: psdno < 0"));

	/* Offset of the start address from the start of the stripe. */
	stripeoff = boff % (p->stripesize * (p->sdcount - 1));
	KASSERT(stripeoff >= 0, ("gv_raid5_offset: stripeoff < 0"));

	/* The number of the subdisk where the stripe resides. */
	sd = stripeoff / p->stripesize;
	KASSERT(sdno >= 0, ("gv_raid5_offset: sdno < 0"));

	/* At or past parity subdisk. */
	if (sd >= psd)
		sd++;

	/* The offset of the stripe on this subdisk. */
	stripestart = (boff - stripeoff) / (p->sdcount - 1);
	KASSERT(stripestart >= 0, ("gv_raid5_offset: stripestart < 0"));

	stripeoff %= p->stripesize;

	/* The offset of the request on this subdisk. */
	*real_off = stripestart + stripeoff;

	stripeend = stripestart + p->stripesize;
	len_left = stripeend - *real_off;
	KASSERT(len_left >= 0, ("gv_raid5_offset: len_left < 0"));

	*real_len = (bcount <= len_left) ? bcount : len_left;

	if (sdno != NULL)
		*sdno = sd;
	if (psdno != NULL)
		*psdno = psd;

	return (0);
}
