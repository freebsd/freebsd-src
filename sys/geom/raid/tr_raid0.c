/*-
 * Copyright (c) 2010 Alexander Motin <mav@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
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
#include <sys/bio.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <geom/geom.h>
#include "geom/raid/g_raid.h"
#include "g_raid_tr_if.h"

static MALLOC_DEFINE(M_TR_RAID0, "tr_raid0_data", "GEOM_RAID RAID0 data");

struct g_raid_tr_raid0_object {
	struct g_raid_tr_object	 trso_base;
	int			 trso_starting;
	int			 trso_stopped;
};

static g_raid_tr_taste_t g_raid_tr_taste_raid0;
static g_raid_tr_event_t g_raid_tr_event_raid0;
static g_raid_tr_start_t g_raid_tr_start_raid0;
static g_raid_tr_stop_t g_raid_tr_stop_raid0;
static g_raid_tr_iostart_t g_raid_tr_iostart_raid0;
static g_raid_tr_iodone_t g_raid_tr_iodone_raid0;
static g_raid_tr_kerneldump_t g_raid_tr_kerneldump_raid0;
static g_raid_tr_free_t g_raid_tr_free_raid0;

static kobj_method_t g_raid_tr_raid0_methods[] = {
	KOBJMETHOD(g_raid_tr_taste,	g_raid_tr_taste_raid0),
	KOBJMETHOD(g_raid_tr_event,	g_raid_tr_event_raid0),
	KOBJMETHOD(g_raid_tr_start,	g_raid_tr_start_raid0),
	KOBJMETHOD(g_raid_tr_stop,	g_raid_tr_stop_raid0),
	KOBJMETHOD(g_raid_tr_iostart,	g_raid_tr_iostart_raid0),
	KOBJMETHOD(g_raid_tr_iodone,	g_raid_tr_iodone_raid0),
	KOBJMETHOD(g_raid_tr_kerneldump,	g_raid_tr_kerneldump_raid0),
	KOBJMETHOD(g_raid_tr_free,	g_raid_tr_free_raid0),
	{ 0, 0 }
};

static struct g_raid_tr_class g_raid_tr_raid0_class = {
	"RAID0",
	g_raid_tr_raid0_methods,
	sizeof(struct g_raid_tr_raid0_object),
	.trc_priority = 100
};

static int
g_raid_tr_taste_raid0(struct g_raid_tr_object *tr, struct g_raid_volume *volume)
{
	struct g_raid_tr_raid0_object *trs;

	trs = (struct g_raid_tr_raid0_object *)tr;
	if (tr->tro_volume->v_raid_level != G_RAID_VOLUME_RL_RAID0 ||
	    tr->tro_volume->v_raid_level_qualifier != G_RAID_VOLUME_RLQ_NONE)
		return (G_RAID_TR_TASTE_FAIL);
	trs->trso_starting = 1;
	return (G_RAID_TR_TASTE_SUCCEED);
}

static int
g_raid_tr_update_state_raid0(struct g_raid_volume *vol)
{
	struct g_raid_tr_raid0_object *trs;
	struct g_raid_softc *sc;
	u_int s;
	int n, f;

	sc = vol->v_softc;
	trs = (struct g_raid_tr_raid0_object *)vol->v_tr;
	if (trs->trso_stopped)
		s = G_RAID_VOLUME_S_STOPPED;
	else {
		n = g_raid_nsubdisks(vol, G_RAID_SUBDISK_S_ACTIVE);
		f = g_raid_nsubdisks(vol, G_RAID_SUBDISK_S_FAILED);
		if (n + f == vol->v_disks_count) {
			if (f == 0)
				s = G_RAID_VOLUME_S_OPTIMAL;
			else
				s = G_RAID_VOLUME_S_SUBOPTIMAL;
			trs->trso_starting = 0;
		} else if (trs->trso_starting)
			s = G_RAID_VOLUME_S_STARTING;
		else
			s = G_RAID_VOLUME_S_BROKEN;
	}
	if (s != vol->v_state) {
		g_raid_event_send(vol, G_RAID_VOLUME_S_ALIVE(s) ?
		    G_RAID_VOLUME_E_UP : G_RAID_VOLUME_E_DOWN,
		    G_RAID_EVENT_VOLUME);
		g_raid_change_volume_state(vol, s);
		if (!trs->trso_starting && !trs->trso_stopped)
			g_raid_write_metadata(sc, vol, NULL, NULL);
	}
	return (0);
}

static int
g_raid_tr_event_raid0(struct g_raid_tr_object *tr,
    struct g_raid_subdisk *sd, u_int event)
{
	struct g_raid_tr_raid0_object *trs;
	struct g_raid_softc *sc;
	struct g_raid_volume *vol;
	int state;

	trs = (struct g_raid_tr_raid0_object *)tr;
	vol = tr->tro_volume;
	sc = vol->v_softc;
	if (event == G_RAID_SUBDISK_E_NEW) {
		state = sd->sd_state;
		if (state != G_RAID_SUBDISK_S_FAILED)
			g_raid_change_subdisk_state(sd, G_RAID_SUBDISK_S_ACTIVE);
		if (state != sd->sd_state &&
		    !trs->trso_starting && !trs->trso_stopped)
			g_raid_write_metadata(sc, vol, sd, NULL);
	} else if (event == G_RAID_SUBDISK_E_FAILED) {
//		g_raid_change_subdisk_state(sd, G_RAID_SUBDISK_S_FAILED);
	} else
		g_raid_change_subdisk_state(sd, G_RAID_SUBDISK_S_NONE);
	g_raid_tr_update_state_raid0(vol);
	return (0);
}

static int
g_raid_tr_start_raid0(struct g_raid_tr_object *tr)
{
	struct g_raid_tr_raid0_object *trs;
	struct g_raid_volume *vol;

	trs = (struct g_raid_tr_raid0_object *)tr;
	vol = tr->tro_volume;
	trs->trso_starting = 0;
	g_raid_tr_update_state_raid0(vol);
	return (0);
}

static int
g_raid_tr_stop_raid0(struct g_raid_tr_object *tr)
{
	struct g_raid_tr_raid0_object *trs;
	struct g_raid_volume *vol;

	trs = (struct g_raid_tr_raid0_object *)tr;
	vol = tr->tro_volume;
	trs->trso_starting = 0;
	trs->trso_stopped = 1;
	g_raid_tr_update_state_raid0(vol);
	return (0);
}

static void
g_raid_tr_iostart_raid0(struct g_raid_tr_object *tr, struct bio *bp)
{
	struct g_raid_softc *sc;
	struct g_raid_volume *vol;
	struct g_raid_subdisk *sd;
	struct bio_queue_head queue;
	struct bio *cbp;
	char *addr;
	off_t offset, start, length, nstripe;
	u_int no, strip_size;

	vol = tr->tro_volume;
	if (vol->v_state != G_RAID_VOLUME_S_OPTIMAL) {
		g_raid_iodone(bp, EIO);
		return;
	}
	sc = vol->v_softc;

	addr = bp->bio_data;
	strip_size = vol->v_strip_size;

	/* Stripe number. */
	nstripe = bp->bio_offset / strip_size;
	/* Start position in stripe. */
	start = bp->bio_offset % strip_size;
	/* Disk number. */
	no = nstripe % vol->v_disks_count;
	/* Start position in disk. */
	offset = (nstripe / vol->v_disks_count) * strip_size + start;
	/* Length of data to operate. */
	length = MIN(bp->bio_length, strip_size - start);

	/*
	 * Allocate all bios before sending any request, so we can
	 * return ENOMEM in nice and clean way.
	 */
	bioq_init(&queue);
	cbp = g_clone_bio(bp);
	if (cbp == NULL)
		goto failure;
	/*
	 * Fill in the component buf structure.
	 */
	cbp->bio_offset = offset;
	cbp->bio_data = addr;
	cbp->bio_length = length;
	cbp->bio_caller1 = &vol->v_subdisks[no];
	bioq_insert_tail(&queue, cbp);

	offset -= offset % strip_size;
	addr += length;
	length = bp->bio_length - length;
	for (no++; length > 0;
	    no++, length -= strip_size, addr += strip_size) {
		if (no > vol->v_disks_count - 1) {
			no = 0;
			offset += strip_size;
		}
		cbp = g_clone_bio(bp);
		if (cbp == NULL)
			goto failure;

		/*
		 * Fill in the component buf structure.
		 */
		cbp->bio_offset = offset;
		cbp->bio_data = addr;
		/*
		 * MIN() is in case when
		 * (bp->bio_length % sc->sc_stripesize) != 0.
		 */
		cbp->bio_length = MIN(strip_size, length);

		cbp->bio_caller1 = &vol->v_subdisks[no];
		bioq_insert_tail(&queue, cbp);
	}
	for (cbp = bioq_first(&queue); cbp != NULL;
	    cbp = bioq_first(&queue)) {
		bioq_remove(&queue, cbp);
		sd = cbp->bio_caller1;
		cbp->bio_caller1 = NULL;
		g_raid_subdisk_iostart(sd, cbp);
	}
	return;
failure:
	for (cbp = bioq_first(&queue); cbp != NULL;
	    cbp = bioq_first(&queue)) {
		bioq_remove(&queue, cbp);
		g_destroy_bio(cbp);
	}
	if (bp->bio_error == 0)
		bp->bio_error = ENOMEM;
	g_raid_iodone(bp, bp->bio_error);
}

int
g_raid_tr_kerneldump_raid0(struct g_raid_tr_object *tr,
    void *virtual, vm_offset_t physical, off_t boffset, size_t blength)
{
	struct g_raid_softc *sc;
	struct g_raid_volume *vol;
	char *addr;
	off_t offset, start, length, nstripe;
	u_int no, strip_size;
	int error;

	vol = tr->tro_volume;
	if (vol->v_state != G_RAID_VOLUME_S_OPTIMAL)
		return (ENXIO);
	sc = vol->v_softc;

	addr = virtual;
	strip_size = vol->v_strip_size;
	/* Stripe number. */
	nstripe = boffset / strip_size;
	/* Start position in stripe. */
	start = boffset % strip_size;
	/* Disk number. */
	no = nstripe % vol->v_disks_count;
	/* Start position in disk. */
	offset = (nstripe / vol->v_disks_count) * strip_size + start;
	/* Length of data to operate. */
	length = MIN(blength, strip_size - start);

	error = g_raid_subdisk_kerneldump(&vol->v_subdisks[no],
	    addr, 0, offset, length);
	if (error != 0)
		return (error);

	offset -= offset % strip_size;
	addr += length;
	length = blength - length;
	for (no++; length > 0;
	    no++, length -= strip_size, addr += strip_size) {
		if (no > vol->v_disks_count - 1) {
			no = 0;
			offset += strip_size;
		}
		error = g_raid_subdisk_kerneldump(&vol->v_subdisks[no],
		    addr, 0, offset, MIN(strip_size, length));
		if (error != 0)
			return (error);
	}
	return (0);
}

static void
g_raid_tr_iodone_raid0(struct g_raid_tr_object *tr,
    struct g_raid_subdisk *sd,struct bio *bp)
{
	struct bio *pbp;

	pbp = bp->bio_parent;
	if (pbp->bio_error == 0)
		pbp->bio_error = bp->bio_error;
	g_destroy_bio(bp);
	pbp->bio_inbed++;
	if (pbp->bio_children == pbp->bio_inbed) {
		pbp->bio_completed = pbp->bio_length;
		g_raid_iodone(pbp, bp->bio_error);
	}
}

static int
g_raid_tr_free_raid0(struct g_raid_tr_object *tr)
{

	return (0);
}

G_RAID_TR_DECLARE(g_raid_tr_raid0);
