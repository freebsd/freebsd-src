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

#define SD_READ_THRESHOLD 10 /* errors to cause a rebuild */
#define SD_REBUILD_SLAB	(1 << 20) /* One transation in a rebuild */
#define SD_REBUILD_FAIR_IO 20 /* use 1/x of the available I/O */
#define SD_REBUILD_CLUSTER_BUSY 4
#define SD_REBUILD_CLUSTER_IDLE 10
#define SD_REBUILD_META_UPDATE 500 /* update meta data every 5 GB or so */

/*
 * We don't want to hammer the disk with I/O requests when doing a rebuild or
 * a resync.  So, we send these events to ourselves when we go idle (or every
 * Nth normal I/O to 'clock' the process along.  The number and speed that we
 * send these will determine the bandwidth we consume of the disk drive and
 * how long these operations will take.
 */
#define G_RAID_SUBDISK_E_TR_REBUILD_SOME (G_RAID_SUBDISK_E_FIRST_TR_PRIVATE + 0)
#define G_RAID_SUBDISK_E_TR_RESYNC_SOME (G_RAID_SUBDISK_E_FIRST_TR_PRIVATE + 1)

static MALLOC_DEFINE(M_TR_raid1, "tr_raid1_data", "GEOM_RAID raid1 data");

#define TR_RAID1_NONE 0
#define TR_RAID1_REBUILD 1
#define TR_RAID1_RESYNC 2

#define TR_RAID1_F_DOING_SOME	0x1

struct g_raid_tr_raid1_object {
	struct g_raid_tr_object	 trso_base;
	int			 trso_starting;
	int			 trso_stopped;
	int			 trso_type;
	int			 trso_recover_slabs; /* might need to be more */
	int			 trso_fair_io;
	int			 trso_meta_update;
	int			 trso_flags;
	struct g_raid_subdisk	*trso_failed_sd;/* like per volume */
	void			*trso_buffer;	/* Buffer space */
	struct bio		 trso_bio;
};

static g_raid_tr_taste_t g_raid_tr_taste_raid1;
static g_raid_tr_event_t g_raid_tr_event_raid1;
static g_raid_tr_start_t g_raid_tr_start_raid1;
static g_raid_tr_stop_t g_raid_tr_stop_raid1;
static g_raid_tr_iostart_t g_raid_tr_iostart_raid1;
static g_raid_tr_iodone_t g_raid_tr_iodone_raid1;
static g_raid_tr_kerneldump_t g_raid_tr_kerneldump_raid1;
static g_raid_tr_locked_t g_raid_tr_locked_raid1;
static g_raid_tr_free_t g_raid_tr_free_raid1;

static kobj_method_t g_raid_tr_raid1_methods[] = {
	KOBJMETHOD(g_raid_tr_taste,	g_raid_tr_taste_raid1),
	KOBJMETHOD(g_raid_tr_event,	g_raid_tr_event_raid1),
	KOBJMETHOD(g_raid_tr_start,	g_raid_tr_start_raid1),
	KOBJMETHOD(g_raid_tr_stop,	g_raid_tr_stop_raid1),
	KOBJMETHOD(g_raid_tr_iostart,	g_raid_tr_iostart_raid1),
	KOBJMETHOD(g_raid_tr_iodone,	g_raid_tr_iodone_raid1),
	KOBJMETHOD(g_raid_tr_kerneldump,	g_raid_tr_kerneldump_raid1),
	KOBJMETHOD(g_raid_tr_locked,	g_raid_tr_locked_raid1),
	KOBJMETHOD(g_raid_tr_free,	g_raid_tr_free_raid1),
	{ 0, 0 }
};

static struct g_raid_tr_class g_raid_tr_raid1_class = {
	"RAID1",
	g_raid_tr_raid1_methods,
	sizeof(struct g_raid_tr_raid1_object),
	.trc_priority = 100
};

static struct g_raid_subdisk *g_raid_tr_raid1_find_good_drive(
	struct g_raid_volume *vol);
static void g_raid_tr_raid1_maybe_rebuild(struct g_raid_tr_object *tr,
    struct g_raid_volume *vol);

static int
g_raid_tr_taste_raid1(struct g_raid_tr_object *tr, struct g_raid_volume *volume)
{
	struct g_raid_tr_raid1_object *trs;

	trs = (struct g_raid_tr_raid1_object *)tr;
	if (tr->tro_volume->v_raid_level != G_RAID_VOLUME_RL_RAID1 ||
	    tr->tro_volume->v_raid_level_qualifier != G_RAID_VOLUME_RLQ_NONE)
		return (G_RAID_TR_TASTE_FAIL);
	trs->trso_starting = 1;
	return (G_RAID_TR_TASTE_SUCCEED);
}

static int
g_raid_tr_update_state_raid1(struct g_raid_volume *vol)
{
	struct g_raid_tr_raid1_object *trs;
	u_int s;
	int n;

	trs = (struct g_raid_tr_raid1_object *)vol->v_tr;
	if (trs->trso_stopped)
		s = G_RAID_VOLUME_S_STOPPED;
	else {
		n = g_raid_nsubdisks(vol, G_RAID_SUBDISK_S_ACTIVE);
		if (n == vol->v_disks_count) {
			s = G_RAID_VOLUME_S_OPTIMAL;
			trs->trso_starting = 0;
		} else {
			if (trs->trso_starting)
				s = G_RAID_VOLUME_S_STARTING;
			else if (n > 0)
				s = G_RAID_VOLUME_S_DEGRADED;
			else
				s = G_RAID_VOLUME_S_BROKEN;
		}
	}
	g_raid_tr_raid1_maybe_rebuild(vol->v_tr, vol);
	if (s != vol->v_state) {
		g_raid_event_send(vol, G_RAID_VOLUME_S_ALIVE(s) ?
		    G_RAID_VOLUME_E_UP : G_RAID_VOLUME_E_DOWN,
		    G_RAID_EVENT_VOLUME);
		g_raid_change_volume_state(vol, s);
	}
	return (0);
}

static void
g_raid_tr_raid1_rebuild_some(struct g_raid_tr_object *tr,
    struct g_raid_subdisk *sd)
{
	struct g_raid_tr_raid1_object *trs;
	struct bio *bp, *bp2;

	trs = (struct g_raid_tr_raid1_object *)tr;
	if (trs->trso_flags & TR_RAID1_F_DOING_SOME)
		return;
	trs->trso_flags |= TR_RAID1_F_DOING_SOME;
	trs->trso_recover_slabs = SD_REBUILD_CLUSTER_IDLE;
	trs->trso_fair_io = SD_REBUILD_FAIR_IO;
	bp = &trs->trso_bio;
	memset(bp, 0, sizeof(*bp));
	bp->bio_offset = sd->sd_rebuild_pos;
	bp->bio_length = MIN(SD_REBUILD_SLAB,
	    sd->sd_volume->v_mediasize - sd->sd_rebuild_pos);
	bp->bio_data = trs->trso_buffer;
	bp->bio_cmd = BIO_READ;
	bp2 = g_clone_bio(bp);
	bp2->bio_cflags = G_RAID_BIO_FLAG_SYNC;
	bp2->bio_caller1 = g_raid_tr_raid1_find_good_drive(sd->sd_volume);
	g_raid_lock_range(sd->sd_volume,	/* Lock callback starts I/O */
	    bp2->bio_offset, bp2->bio_length, bp2);
}

static void
g_raid_tr_raid1_resync_some(struct g_raid_tr_object *tr,
    struct g_raid_subdisk *sd)
{
	panic("We don't implement resync yet");
}

static void
g_raid_tr_raid1_idle_rebuild(struct g_raid_volume *vol, void *argp)
{
	struct g_raid_tr_raid1_object *trs;

	trs = (struct g_raid_tr_raid1_object *)argp;
	if (trs->trso_failed_sd == NULL) {
		printf("I hit the case that's obsolete, right?\n");
		return;
	}
	g_raid_event_send(trs->trso_failed_sd, G_RAID_SUBDISK_E_TR_REBUILD_SOME,
	    G_RAID_EVENT_SUBDISK);
}

static void
g_raid_tr_raid1_rebuild_finish(struct g_raid_tr_object *tr, struct g_raid_volume *vol)
{
	struct g_raid_tr_raid1_object *trs;
	struct g_raid_subdisk *sd;

	trs = (struct g_raid_tr_raid1_object *)tr;
	sd = trs->trso_failed_sd;
	sd->sd_rebuild_pos = 0;
	g_raid_change_subdisk_state(sd, G_RAID_SUBDISK_S_ACTIVE);
	g_raid_tr_update_state_raid1(vol);
	g_raid_write_metadata(vol->v_softc, vol, sd, sd->sd_disk);
	free(trs->trso_buffer, M_TR_raid1);
	trs->trso_flags &= ~TR_RAID1_F_DOING_SOME;
	trs->trso_type = TR_RAID1_NONE;
	trs->trso_recover_slabs = 0;
	trs->trso_failed_sd = NULL;
	trs->trso_buffer = NULL;
	vol->v_timeout = 0;
}

static void
g_raid_tr_raid1_rebuild_abort(struct g_raid_tr_object *tr,
    struct g_raid_volume *vol)
{
	struct g_raid_tr_raid1_object *trs;
	struct g_raid_subdisk *sd;

	trs = (struct g_raid_tr_raid1_object *)tr;
	sd = trs->trso_failed_sd;
//	sd->sd_rebuild_pos = 0; /* We may need this here... */
	g_raid_unlock_range(tr->tro_volume,sd->sd_rebuild_pos, SD_REBUILD_SLAB);
	g_raid_write_metadata(vol->v_softc, vol, sd, sd->sd_disk);
	free(trs->trso_buffer, M_TR_raid1);
	trs->trso_flags &= ~TR_RAID1_F_DOING_SOME;
	trs->trso_type = TR_RAID1_NONE;
	trs->trso_recover_slabs = 0;
	trs->trso_failed_sd = NULL;
	trs->trso_buffer = NULL;
	vol->v_timeout = 0;
}

static struct g_raid_subdisk *
g_raid_tr_raid1_find_good_drive(struct g_raid_volume *vol)
{
	int i;

	for (i = 0; i < vol->v_disks_count; i++)
		if (vol->v_subdisks[i].sd_state == G_RAID_SUBDISK_S_ACTIVE)
			return (&vol->v_subdisks[i]);
	return (NULL);
}

static struct g_raid_subdisk *
g_raid_tr_raid1_find_failed_drive(struct g_raid_volume *vol)
{
	int i;

	for (i = 0; i < vol->v_disks_count; i++)
		if (vol->v_subdisks[i].sd_state == G_RAID_SUBDISK_S_REBUILD ||
		    vol->v_subdisks[i].sd_state == G_RAID_SUBDISK_S_RESYNC)
			return (&vol->v_subdisks[i]);
	return (NULL);
}

static void
g_raid_tr_raid1_rebuild_start(struct g_raid_tr_object *tr, struct g_raid_volume *vol)
{
  /* XXX ---- XXX Should this be based on trs state or vol state? XXX --- XXX */
	struct g_raid_tr_raid1_object *trs;
	struct g_raid_subdisk *sd;

	trs = (struct g_raid_tr_raid1_object *)tr;
	if (trs->trso_failed_sd) {
		G_RAID_DEBUG(1, "Already rebuild in start rebuild. pos %jd\n",
		    (intmax_t)trs->trso_failed_sd->sd_rebuild_pos);
		return;
	}
	sd = g_raid_tr_raid1_find_good_drive(vol);
	trs->trso_failed_sd = g_raid_tr_raid1_find_failed_drive(vol);
	if (sd == NULL || trs->trso_failed_sd == NULL) {
		G_RAID_DEBUG(1, "No failed disk to rebuild.  night night.");
		return;
	}
	G_RAID_DEBUG(2, "Kicking off a rebuild at %jd...",
	    trs->trso_failed_sd->sd_rebuild_pos);
	trs->trso_type = TR_RAID1_REBUILD;
/*
 * I'm not sure when to reset this and when to use what's there already.
 * XXX
 */
//	trs->trso_failed_sd->sd_rebuild_pos = 0;
	trs->trso_buffer = malloc(SD_REBUILD_SLAB, M_TR_raid1, M_WAITOK);
	trs->trso_meta_update = SD_REBUILD_META_UPDATE;
	vol->v_to_arg = trs;
	vol->v_timeout = g_raid_tr_raid1_idle_rebuild;
	g_raid_tr_raid1_rebuild_some(tr, trs->trso_failed_sd);
}


static void
g_raid_tr_raid1_maybe_rebuild(struct g_raid_tr_object *tr, struct g_raid_volume *vol)
{
	struct g_raid_tr_raid1_object *trs;
	int na, nr;
	
	/*
	 * If we're stopped, don't do anything.  If we don't have at least
	 * one good disk and one bad disk, we don't do anything.  And if there's
	 * a 'good disk' stored in the trs, then we're in progress and we punt.
	 * If we make it past all these checks, we need to rebuild.
	 */
	trs = (struct g_raid_tr_raid1_object *)tr;
	if (trs->trso_stopped)
		return;
	na = g_raid_nsubdisks(vol, G_RAID_SUBDISK_S_ACTIVE);
	nr = g_raid_nsubdisks(vol, G_RAID_SUBDISK_S_REBUILD);
	switch(trs->trso_type) {
	case TR_RAID1_NONE:
		if (na == 0 || nr == 0)
			return;
		if (trs->trso_type != TR_RAID1_NONE)
			return;
		g_raid_tr_raid1_rebuild_start(tr, vol);
		break;
	case TR_RAID1_REBUILD:
		/*
		 * We're rebuilding, maybe we need to stop...
		 */
		break;
	case TR_RAID1_RESYNC:
		break;
	}
}

static int
g_raid_tr_event_raid1(struct g_raid_tr_object *tr,
    struct g_raid_subdisk *sd, u_int event)
{
	struct g_raid_tr_raid1_object *trs;
	struct g_raid_volume *vol;

	trs = (struct g_raid_tr_raid1_object *)tr;
	vol = tr->tro_volume;
	switch (event) {
	case G_RAID_SUBDISK_E_NEW:
		printf("Current disk state is %d\n", sd->sd_state);
		if (sd->sd_state == G_RAID_SUBDISK_S_NEW)
			g_raid_change_subdisk_state(sd, G_RAID_SUBDISK_S_REBUILD);
		break;
	case G_RAID_SUBDISK_E_FAILED:
		if (trs->trso_type == TR_RAID1_REBUILD)
			g_raid_tr_raid1_rebuild_abort(tr, vol);
//		g_raid_change_subdisk_state(sd, G_RAID_SUBDISK_S_FAILED);
		break;
	case G_RAID_SUBDISK_E_DISCONNECTED:
		if (trs->trso_type == TR_RAID1_REBUILD)
			g_raid_tr_raid1_rebuild_abort(tr, vol);
		g_raid_change_subdisk_state(sd, G_RAID_SUBDISK_S_NONE);
		break;
	case G_RAID_SUBDISK_E_TR_REBUILD_SOME:
		g_raid_tr_raid1_rebuild_some(tr, sd);
		break;
	case G_RAID_SUBDISK_E_TR_RESYNC_SOME:
		g_raid_tr_raid1_resync_some(tr, sd);
		break;
	}
	g_raid_tr_update_state_raid1(vol);
	return (0);
}

static int
g_raid_tr_start_raid1(struct g_raid_tr_object *tr)
{
	struct g_raid_tr_raid1_object *trs;
	struct g_raid_volume *vol;

	trs = (struct g_raid_tr_raid1_object *)tr;
	vol = tr->tro_volume;
	trs->trso_starting = 0;
	g_raid_tr_update_state_raid1(vol);
	return (0);
}

static int
g_raid_tr_stop_raid1(struct g_raid_tr_object *tr)
{
	struct g_raid_tr_raid1_object *trs;
	struct g_raid_volume *vol;

	trs = (struct g_raid_tr_raid1_object *)tr;
	vol = tr->tro_volume;
	trs->trso_starting = 0;
	trs->trso_stopped = 1;
	g_raid_tr_update_state_raid1(vol);
	return (0);
}

/*
 * Select the disk to do the reads to.  For now, we just pick the first one in
 * the list that's active always.  This ensures we favor one disk on boot, and
 * have more deterministic recovery from the weird edge cases of power
 * failure.  In the future, we can imagine policies that go for the least
 * loaded disk to improve performance, or we need to limit reads to a disk
 * during some kind of error recovery with that disk.
 */
static struct g_raid_subdisk *
g_raid_tr_raid1_select_read_disk(struct g_raid_volume *vol)
{
	int i;

	for (i = 0; i < vol->v_disks_count; i++)
		if (vol->v_subdisks[i].sd_state == G_RAID_SUBDISK_S_ACTIVE)
			return (&vol->v_subdisks[i]);
	return (NULL);
}

static void
g_raid_tr_iostart_raid1_read(struct g_raid_tr_object *tr, struct bio *bp)
{
	struct g_raid_subdisk *sd;
	struct bio *cbp;

	sd = g_raid_tr_raid1_select_read_disk(tr->tro_volume);
	KASSERT(sd != NULL, ("No active disks in volume %s.", tr->tro_volume->v_name));

	cbp = g_clone_bio(bp);
	if (cbp == NULL) {
		g_raid_iodone(bp, ENOMEM);
		return;
	}

	g_raid_subdisk_iostart(sd, cbp);
}

static void
g_raid_tr_iostart_raid1_write(struct g_raid_tr_object *tr, struct bio *bp)
{
	struct g_raid_softc *sc;
	struct g_raid_volume *vol;
	struct g_raid_subdisk *sd;
	struct bio_queue_head queue;
	struct bio *cbp;
	int i;

	vol = tr->tro_volume;
	sc = vol->v_softc;

	/*
	 * Allocate all bios before sending any request, so we can return
	 * ENOMEM in nice and clean way.
	 */
	bioq_init(&queue);
	for (i = 0; i < vol->v_disks_count; i++) {
		sd = &vol->v_subdisks[i];
		switch (sd->sd_state) {
		case G_RAID_SUBDISK_S_ACTIVE:
			break;
		case G_RAID_SUBDISK_S_REBUILD:
			/*
			 * When rebuilding, only part of this subdisk is
			 * writable, the rest will be written as part of the
			 * that process.
			 */
			if (bp->bio_offset >= sd->sd_rebuild_pos)
				continue;
			break;
		case G_RAID_SUBDISK_S_RESYNC:
			/*
			 * Resyncing still writes on the theory that the
			 * resync'd disk is very close and writing it will
			 * keep it that way better if we keep up while
			 * resyncing.
			 */
		default:
			continue;
		}
		cbp = g_clone_bio(bp);
		if (cbp == NULL) {
			for (cbp = bioq_first(&queue); cbp != NULL;
			    cbp = bioq_first(&queue)) {
				bioq_remove(&queue, cbp);
				g_destroy_bio(cbp);
			}
			if (bp->bio_error == 0)
				bp->bio_error = ENOMEM;
			g_raid_iodone(bp, bp->bio_error);
			return;
		}
		cbp->bio_caller1 = sd;
		bioq_insert_tail(&queue, cbp);
	}
	for (cbp = bioq_first(&queue); cbp != NULL;
	    cbp = bioq_first(&queue)) {
		bioq_remove(&queue, cbp);
		sd = cbp->bio_caller1;
		cbp->bio_caller1 = NULL;
		g_raid_subdisk_iostart(sd, cbp);
	}

}

static void
g_raid_tr_iostart_raid1(struct g_raid_tr_object *tr, struct bio *bp)
{
	struct g_raid_volume *vol;
	struct g_raid_tr_raid1_object *trs;

	vol = tr->tro_volume;
	trs = (struct g_raid_tr_raid1_object *)tr;
	if (vol->v_state != G_RAID_VOLUME_S_OPTIMAL &&
	    vol->v_state != G_RAID_VOLUME_S_SUBOPTIMAL &&
	    vol->v_state != G_RAID_VOLUME_S_DEGRADED) {
		g_raid_iodone(bp, EIO);
		return;
	}
	/*
	 * If we're rebuilding, squeeze in rebuild activity every so often,
	 * even when the disk is busy.  Be sure to only count real I/O
	 * to the disk.  All 'SPECIAL' I/O is traffic generated to the disk
	 * by this module.
	 */
	if (trs->trso_failed_sd != NULL &&
	    !(bp->bio_cflags & G_RAID_BIO_FLAG_SPECIAL)) {
		if (--trs->trso_fair_io <= 0) {
			g_raid_event_send(trs->trso_failed_sd,
			    G_RAID_SUBDISK_E_TR_REBUILD_SOME,
			    G_RAID_EVENT_SUBDISK);
		}
	}
	switch (bp->bio_cmd) {
	case BIO_READ:
		g_raid_tr_iostart_raid1_read(tr, bp);
		break;
	case BIO_WRITE:
		g_raid_tr_iostart_raid1_write(tr, bp);
		break;
	case BIO_DELETE:
		g_raid_iodone(bp, EIO);
		break;
	default:
		KASSERT(1 == 0, ("Invalid command here: %u (volume=%s)",
		    bp->bio_cmd, vol->v_name));
		break;
	}
}

static void
g_raid_tr_iodone_raid1(struct g_raid_tr_object *tr,
    struct g_raid_subdisk *sd, struct bio *bp)
{
	struct bio *cbp;
	struct g_raid_subdisk *nsd;
	struct g_raid_volume *vol;
	struct bio *pbp;
	struct g_raid_tr_raid1_object *trs;
	int i;

	trs = (struct g_raid_tr_raid1_object *)tr;
	pbp = bp->bio_parent;
	if (bp->bio_cflags & G_RAID_BIO_FLAG_SYNC) {
		/*
		 * This operation is part of a rebuild or resync
		 * operation.  See what work just got done, then
		 * schedule the next bit of work, if any.
		 * Rebuild/resync is done a little bit at a time.
		 * Either when a timeout happens, or after we get a
		 * bunch of I/Os to the disk (to make sure an active
		 * system will complete in a sane amount of time).
		 *
		 * We are setup to do differing amounts of work for
		 * each of these cases.  so long as the slabs is
		 * smallish (less than 50 or so, I'd guess, but that's
		 * just a WAG), we shouldn't have any bio starvation
		 * issues.  For active disks, we do 5MB of data, for
		 * inactive ones, we do 50MB.
		 */
		if (trs->trso_type == TR_RAID1_REBUILD) {
			vol = tr->tro_volume;
			pbp->bio_inbed++;
			if (bp->bio_cmd == BIO_READ) {
				/*
				 * The read operation finished, queue the
				 * write and get out.
				 */
				G_RAID_LOGREQ(4, bp, "rebuild read done. %d",
				    bp->bio_error);
				if (bp->bio_error != 0) {
					g_raid_tr_raid1_rebuild_abort(tr, vol);
					return;
				}
				cbp = g_clone_bio(pbp);
				cbp->bio_cmd = BIO_WRITE;
				cbp->bio_cflags = G_RAID_BIO_FLAG_SYNC;
				cbp->bio_offset = bp->bio_offset;
				cbp->bio_length = bp->bio_length;
				G_RAID_LOGREQ(4, bp, "Queueing reguild write.");
				g_raid_subdisk_iostart(trs->trso_failed_sd, cbp);
			} else {
				/*
				 * The write operation just finished.  Do
				 * another.  We keep cloning the master bio
				 * since it has the right buffers allocated to
				 * it.  We'll free it when slabs get to 0.
				 * We'll also tie up SD_REBUILD_CLUSTER * 2 +
				 * 1 bios from the pool.  Since
				 * SD_REBUILD_CLUSTER should be small, that
				 * shouldn't be a problem.
				 */
				G_RAID_LOGREQ(4, bp,
				    "rebuild write done. Error %d", bp->bio_error);
				if (bp->bio_error != 0) {
					g_raid_tr_raid1_rebuild_abort(tr, vol);
					return;
				}
/* XXX A lot of the following is needed when we kick of the work -- refactor */
				nsd = trs->trso_failed_sd;
				g_raid_unlock_range(sd->sd_volume,
				    bp->bio_offset, bp->bio_length);
				nsd->sd_rebuild_pos += pbp->bio_length;
				if (nsd->sd_rebuild_pos >= vol->v_mediasize) {
					g_raid_tr_raid1_rebuild_finish(tr, vol);
					return;
				}
				if (--trs->trso_recover_slabs <= 0) {
					if (--trs->trso_meta_update <= 0) {
						g_raid_write_metadata(vol->v_softc,
						    vol, nsd, nsd->sd_disk);
						trs->trso_meta_update = SD_REBUILD_META_UPDATE;
					}
					trs->trso_flags &= ~TR_RAID1_F_DOING_SOME;
					return;
				}
				pbp->bio_offset = nsd->sd_rebuild_pos;
				cbp = g_clone_bio(pbp);
				cbp->bio_cmd = BIO_READ;
				cbp->bio_cflags = G_RAID_BIO_FLAG_SYNC;
				cbp->bio_offset = nsd->sd_rebuild_pos;
				cbp->bio_length = MIN(SD_REBUILD_SLAB,
				    vol->v_mediasize - nsd->sd_rebuild_pos);
				cbp->bio_caller1 =
				    g_raid_tr_raid1_find_good_drive(vol);
				G_RAID_LOGREQ(4, bp,
				    "Rebuild read at %jd.", cbp->bio_offset);
				g_raid_lock_range(sd->sd_volume,	/* Lock callback starts I/O */
				    cbp->bio_offset, cbp->bio_length, cbp);
			}
		} else if (trs->trso_type == TR_RAID1_RESYNC) {
			/*
			 * read good sd, read bad sd in parallel.
			 * when both done, compare the buffers.  write
			 * good to the failed if different.  do the
			 * next bit of work.
			 */
			panic("Somehow, we think we're doing a resync");
		}
		return;
	}
	if (bp->bio_error != 0 && bp->bio_cmd == BIO_READ &&
	    pbp->bio_children == 1 && bp->bio_cflags == 0) {
		/*
		 * Read failed on first drive.  Retry the read error on
		 * another disk drive, if available, before erroring out the
		 * read.
		 */
		vol = tr->tro_volume;
		sd->sd_read_errs++;
		G_RAID_LOGREQ(3, bp,
		    "Read failure, attempting recovery. %d total read errs",
		    sd->sd_read_errs);

		/*
		 * If there are too many read errors, we move to degraded.
		 */
		if (sd->sd_read_errs > SD_READ_THRESHOLD) {
			g_raid_fail_disk(sd->sd_softc, sd, sd->sd_disk);
		}

		/*
		 * Find the other disk, and try to do the I/O to it.
		 */
		for (nsd = NULL, i = 0; i < vol->v_disks_count; i++) {
			nsd = &vol->v_subdisks[i];
			if (sd == nsd)
				continue;
			if (nsd->sd_state != G_RAID_SUBDISK_S_ACTIVE)
				continue;
			cbp = g_clone_bio(pbp);
			if (cbp == NULL)
				break;
			G_RAID_LOGREQ(2, cbp, "Retrying read");
			g_raid_subdisk_iostart(nsd, cbp);
			pbp->bio_inbed++;
			return;
		}
		/*
		 * We can't retry.  Return the original error by falling
		 * through.  This will happen when there's only one good disk.
		 * We don't need to fail the raid, since its actual state is
		 * based on the state of the subdisks.
		 */
		G_RAID_LOGREQ(2, bp, "Couldn't retry read, failing it");
	}
	pbp->bio_inbed++;
	if (pbp->bio_cmd == BIO_READ && pbp->bio_children == 2 &&
	    bp->bio_cflags == 0) {
		/*
		 * If it was a read, and bio_children is 2, then we just
		 * recovered the data from the second drive.  We should try to
		 * write that data to the first drive if sector remapping is
		 * enabled.  A write should put the data in a new place on the
		 * disk, remapping the bad sector.  Do we need to do that by
		 * queueing a request to the main worker thread?  It doesn't
		 * affect the return code of this current read, and can be
		 * done at our liesure.  However, to make the code simpler, it
		 * is done syncrhonously.
		 */
		G_RAID_LOGREQ(3, bp, "Recovered data from other drive");
		cbp = g_clone_bio(pbp);
		if (cbp != NULL) {
			nsd = bp->bio_caller1;
			cbp->bio_cmd = BIO_WRITE;
			cbp->bio_cflags = G_RAID_BIO_FLAG_REMAP;
			cbp->bio_caller1 = nsd;
			G_RAID_LOGREQ(3, bp,
			    "Attempting bad sector remap on failing drive.");
			g_raid_lock_range(sd->sd_volume,	/* Lock callback starts I/O */
			    cbp->bio_offset, cbp->bio_length, cbp);
		}
	}
	if (bp->bio_cflags & G_RAID_BIO_FLAG_REMAP) {
		/*
		 * We're doing a remap write, mark the range as unlocked now
		 * and fail the disk if the write failed.  If the write
		 * failed, the parent's bio isn't failed since the recovered
		 * read for that actually succeeded.
		 */
		G_RAID_LOGREQ(2, bp, "REMAP done %d.", bp->bio_error);
		g_raid_unlock_range(sd->sd_volume, bp->bio_offset,
		    bp->bio_length);
		if (bp->bio_error) {
			G_RAID_LOGREQ(3, bp, "Error on remap: mark subdisk bad.");
			g_raid_fail_disk(sd->sd_softc, sd, sd->sd_disk);
			bp->bio_error = 0;
		}
	}
	if (pbp->bio_children == pbp->bio_inbed) {
		pbp->bio_completed = pbp->bio_length;
		g_raid_iodone(pbp, bp->bio_error);
	}
}

int
g_raid_tr_kerneldump_raid1(struct g_raid_tr_object *tr,
    void *virtual, vm_offset_t physical, off_t offset, size_t length)
{
	struct g_raid_volume *vol;
	struct g_raid_subdisk *sd;
	int error, i, ok;

	vol = tr->tro_volume;
	error = 0;
	ok = 0;
	for (i = 0; i < vol->v_disks_count; i++) {
		sd = &vol->v_subdisks[i];
		switch (sd->sd_state) {
		case G_RAID_SUBDISK_S_ACTIVE:
			break;
//		case G_RAID_DISK_STATE_SYNCHRONIZING:
//			if (bp->bio_offset >= sync->ds_offset)
//				continue;
//			break;
		default:
			continue;
		}
		error = g_raid_subdisk_kerneldump(sd,
		    virtual, physical, offset, length);
		if (error == 0)
			ok++;
	}
	return (ok > 0 ? 0 : error);
}

static int
g_raid_tr_locked_raid1(struct g_raid_tr_object *tr, void *argp)
{
	struct bio *bp;
	struct g_raid_subdisk *sd;

	bp = (struct bio *)argp;
	sd = (struct g_raid_subdisk *)bp->bio_caller1;
	g_raid_subdisk_iostart(sd, bp);

	return (0);
}

static int
g_raid_tr_free_raid1(struct g_raid_tr_object *tr)
{
	struct g_raid_tr_raid1_object *trs;

	trs = (struct g_raid_tr_raid1_object *)tr;

	return (0);
}

G_RAID_TR_DECLARE(g_raid_tr_raid1);
