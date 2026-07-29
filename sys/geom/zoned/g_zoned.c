/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 voidanix <voidanix@FreeBSD.org>
 */

/*
 * GEOM_ZONED presents a plain (non-zoned) provider to the rest of the system
 * as a host-managed zoned block device. The medium is divided into equal
 * sequential-write-required zones. This GEOM class tracks a write pointer per
 * zone and answers BIO_ZONE management commands similarly to a real ZBC/ZAC
 * drive would.
 *
 * Persistence:
 *   - The provider's last sector holds a metadata block, written by the
 *     userland "create" command. The kernel tastes it on every provider
 *     arrival and re-creates the zoned device automatically.
 *   - The sectors just before it hold the per-zone state (condition + write
 *     pointer). That table is read at taste time and rewritten lazily.
 *     Zone-state changes are committed to disk on BIO_FLUSH (and the table is
 *     also flushed when zone-management commands run), mirroring a drive whose
 *     zone state is volatile until a cache flush. Changes since the last
 *     flush may be rolled back by an unclean shutdown.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/disk_zone.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>

#include <geom/geom.h>
#include <geom/geom_dbg.h>
#include <geom/zoned/g_zoned.h>

FEATURE(geom_zoned, "GEOM Zoned Storage Medium emulation");

static MALLOC_DEFINE(M_ZONED, "zoned_data", "GEOM_ZONED Data");

SYSCTL_DECL(_kern_geom);
static SYSCTL_NODE(_kern_geom, OID_AUTO, zoned, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "GEOM_ZONED stuff");
static u_int g_zoned_debug = 0;
SYSCTL_UINT(_kern_geom_zoned, OID_AUTO, debug, CTLFLAG_RW, &g_zoned_debug, 0,
    "Debug level");

static g_access_t g_zoned_access;
static g_ctl_req_t g_zoned_config;
static g_ctl_destroy_geom_t g_zoned_destroy_geom;
static g_dumpconf_t g_zoned_dumpconf;
static g_orphan_t g_zoned_orphan;
static g_provgone_t g_zoned_providergone;
static g_resize_t g_zoned_resize;
static g_start_t g_zoned_start;
static g_taste_t g_zoned_taste;

static struct g_class g_zoned_class = {
	.name = G_ZONED_CLASS_NAME,
	.version = G_VERSION,
	.ctlreq = g_zoned_config,
	.destroy_geom = g_zoned_destroy_geom,
	.taste = g_zoned_taste,
	.access = g_zoned_access,
	.dumpconf = g_zoned_dumpconf,
	.orphan = g_zoned_orphan,
	.providergone = g_zoned_providergone,
	.resize = g_zoned_resize,
	.spoiled = g_zoned_orphan,
	.start = g_zoned_start,
};

/*
 * Index of the zone that contains the given LBA. Caller must have range-checked
 * the LBA against sc_maxlba.
 */
static __inline uint32_t
g_zoned_zoneno(struct g_zoned_softc *sc, uint64_t lba)
{
	return ((uint32_t)(lba / sc->sc_zonesecs));
}

static bool
g_zoned_is_conv(const struct g_zoned_softc *sc, uint32_t zno)
{
	const struct g_zoned_convrange *cr;
	uint32_t i;

	for (i = 0; i < sc->sc_nconv; i++) {
		cr = &sc->sc_conv[i];
		if (zno >= cr->cr_first && zno - cr->cr_first < cr->cr_count)
			return (true);
	}
	return (false);
}

/*
 * Extend the dirty zone range to cover zone zno, so the next BIO_FLUSH
 * writes its table entry out. Must be called with sc_lock held.
 */
static void
g_zoned_mark_dirty(struct g_zoned_softc *sc, uint32_t zno)
{

	mtx_assert(&sc->sc_lock, MA_OWNED);

	if (!sc->sc_dirty) {
		sc->sc_dirty = true;
		sc->sc_dirtylo = sc->sc_dirtyhi = zno;
	} else {
		if (zno < sc->sc_dirtylo)
			sc->sc_dirtylo = zno;
		if (zno > sc->sc_dirtyhi)
			sc->sc_dirtyhi = zno;
	}
}

static bool
g_zoned_cond_is_open(uint8_t cond)
{
	return (cond == DISK_ZONE_COND_IMPLICIT_OPEN ||
	    cond == DISK_ZONE_COND_EXPLICIT_OPEN);
}

/*
 * Put a zone into a new condition, maintaining the count of open
 * zones. Must be called with sc_lock held.
 */
static void
g_zoned_set_cond(struct g_zoned_softc *sc, struct disk_zone_rep_entry *z,
    uint8_t cond)
{
	bool is_open, want_open;

	mtx_assert(&sc->sc_lock, MA_OWNED);

	is_open = g_zoned_cond_is_open(z->zone_condition);
	want_open = g_zoned_cond_is_open(cond);
	if (!is_open && want_open)
		sc->sc_nopen++;
	else if (is_open && !want_open)
		sc->sc_nopen--;
	z->zone_condition = cond;
}

/*
 * Make room to open one more zone under the open-zones limit by implicitly
 * closing the lowest-numbered implicitly-open zone, similarly to a real drive.
 */
static bool
g_zoned_open_room(struct g_zoned_softc *sc)
{
	struct disk_zone_rep_entry *z;
	uint32_t i;

	mtx_assert(&sc->sc_lock, MA_OWNED);

	if (sc->sc_maxopen == 0 || sc->sc_nopen < sc->sc_maxopen)
		return (true);
	for (i = 0; i < sc->sc_nzones; i++) {
		z = &sc->sc_zones[i];
		if (z->zone_condition == DISK_ZONE_COND_IMPLICIT_OPEN) {
			g_zoned_set_cond(sc, z, DISK_ZONE_COND_CLOSED);
			g_zoned_mark_dirty(sc, i);
			return (true);
		}
	}
	return (false);
}

/*
 * Put every zone into its pure state, with the type coming from the
 * metadata-defined conventional ranges.
 */
static void
g_zoned_reset_zones(struct g_zoned_softc *sc)
{
	struct disk_zone_rep_entry *z;
	uint32_t i;

	sc->sc_nopen = 0;
	for (i = 0; i < sc->sc_nzones; i++) {
		z = &sc->sc_zones[i];
		z->zone_start_lba = (uint64_t)i * sc->sc_zonesecs;
		z->zone_length = sc->sc_zonesecs;
		z->zone_flags = 0;
		if (g_zoned_is_conv(sc, i)) {
			z->zone_type = DISK_ZONE_TYPE_CONVENTIONAL;
			z->zone_condition = DISK_ZONE_COND_NOT_WP;
			/* Conventional zones have no write pointer. */
			z->write_pointer_lba = G_ZONED_WP_NONE_LBA;
		} else {
			z->zone_type = DISK_ZONE_TYPE_SEQ_REQUIRED;
			z->zone_condition = DISK_ZONE_COND_EMPTY;
			z->write_pointer_lba = z->zone_start_lba;
		}
	}
}

/*
 * Mark the whole table (header included) dirty so that the first BIO_FLUSH
 * writes it out.
 */
static void
g_zoned_dirty_table(struct g_zoned_softc *sc)
{

	sc->sc_dirty = true;
	sc->sc_hdrdirty = true;
	sc->sc_dirtylo = 0;
	sc->sc_dirtyhi = sc->sc_nzones - 1;
}

/*
 * Read the existing persistent zone table from the provider into the live zone
 * array, otherwise initialise an empty one if no valid table is present.
 */
static void
g_zoned_load_table(struct g_zoned_softc *sc, struct g_consumer *cp)
{
	struct g_zoned_table_hdr th;
	struct disk_zone_rep_entry *z;
	u_char *buf;
	off_t off, resid, chunk;
	uint32_t i, n, zno;
	int error;
	bool valid;

	g_topology_assert();

	error = g_access(cp, 1, 0, 0);
	if (error != 0) {
		G_ZONED_DEBUG(0,
		    "Cannot open %s to read zone table (error=%d);"
		    " assuming empty.",
		    cp->provider->name, error);
		g_zoned_reset_zones(sc);
		g_zoned_dirty_table(sc);
		return;
	}
	g_topology_unlock();

	valid = false;
	buf = g_read_data(cp, sc->sc_taboff, sc->sc_secsize, &error);
	if (buf != NULL) {
		zoned_table_hdr_decode(buf, &th);
		valid = memcmp(th.th_magic, G_ZONED_TABLE_MAGIC,
		    sizeof(G_ZONED_TABLE_MAGIC)) == 0 &&
		    th.th_version == G_ZONED_VERSION &&
		    th.th_nzones == sc->sc_nzones;
		g_free(buf);
	}

	zno = 0;
	off = sc->sc_taboff + sc->sc_secsize;
	resid = (off_t)(sc->sc_tabsecs - 1) * sc->sc_secsize;
	while (valid && resid > 0) {
		chunk = MIN(resid, (off_t)maxphys);
		chunk -= chunk % sc->sc_secsize;
		if (chunk == 0)
			chunk = sc->sc_secsize;
		buf = g_read_data(cp, off, chunk, &error);
		if (buf == NULL) {
			valid = false;
			break;
		}
		n = MIN((uint32_t)(chunk / G_ZONED_ENTRY_SIZE),
		    sc->sc_nzones - zno);
		for (i = 0; i < n; i++)
			g_zoned_entry_decode(buf + i * G_ZONED_ENTRY_SIZE,
			    &sc->sc_zones[zno + i],
			    (uint64_t)(zno + i) * sc->sc_zonesecs);
		g_free(buf);
		zno += n;
		off += chunk;
		resid -= chunk;
	}

	g_topology_lock();
	g_access(cp, -1, 0, 0);

	if (!valid) {
		G_ZONED_DEBUG(1, "No valid zone table on %s; initialising.",
		    cp->provider->name);
		g_zoned_reset_zones(sc);
		g_zoned_dirty_table(sc);
		return;
	}

	for (i = 0; i < sc->sc_nzones; i++) {
		z = &sc->sc_zones[i];
		z->zone_start_lba = (uint64_t)i * sc->sc_zonesecs;
		z->zone_length = sc->sc_zonesecs;
		/* The metadata dictates zone types, not the table. */
		if (g_zoned_is_conv(sc, i)) {
			z->zone_type = DISK_ZONE_TYPE_CONVENTIONAL;
			z->zone_condition = DISK_ZONE_COND_NOT_WP;
			z->write_pointer_lba = G_ZONED_WP_NONE_LBA;
			continue;
		}
		z->zone_type = DISK_ZONE_TYPE_SEQ_REQUIRED;
		/* Guard against a corrupt write pointer. */
		if (z->write_pointer_lba < z->zone_start_lba ||
		    z->write_pointer_lba >
		    z->zone_start_lba + z->zone_length) {
			z->write_pointer_lba = z->zone_start_lba;
			z->zone_condition = DISK_ZONE_COND_EMPTY;
		}
	}
	sc->sc_nopen = 0;
	for (i = 0; i < sc->sc_nzones; i++)
		if (g_zoned_cond_is_open(sc->sc_zones[i].zone_condition))
			sc->sc_nopen++;
	G_ZONED_DEBUG(1, "Restored zone table from %s.", cp->provider->name);
}

/*
 * "Flush the dirty table sectors, then forward the cache flush" thing. We
 * allocate one of those per BIO_FLUSH that finds dirty state to commit.
 */
struct g_zoned_flush {
	struct bio *fl_orig;	  /* Original BIO_FLUSH. */
	struct g_consumer *fl_cp; /* Where to send the I/O. */
	u_char *fl_buf;		  /* Snapshot of dirty sectors. */
	off_t fl_off;		  /* Disk offset of first sector. */
	off_t fl_total;		  /* Bytes to write. */
	off_t fl_done;		  /* Bytes written so far. */
	u_int fl_secsize;
};

static void g_zoned_flush_step(struct g_zoned_flush *fc);

static void
g_zoned_flush_final(struct bio *bp)
{
	struct g_zoned_flush *fc = bp->bio_caller1;
	struct bio *orig = fc->fl_orig;
	int error = bp->bio_error;

	g_destroy_bio(bp);
	g_free(fc->fl_buf);
	g_free(fc);
	g_io_deliver(orig, error);
}

static void
g_zoned_flush_write_done(struct bio *bp)
{
	struct g_zoned_flush *fc = bp->bio_caller1;
	int error = bp->bio_error;

	g_destroy_bio(bp);
	if (error != 0) {
		G_ZONED_DEBUG(0, "Zone table write failed (error=%d).", error);
		g_free(fc->fl_buf);
		g_io_deliver(fc->fl_orig, error);
		g_free(fc);
		return;
	}
	g_zoned_flush_step(fc);
}

static void
g_zoned_flush_step(struct g_zoned_flush *fc)
{
	struct bio *cbp;
	off_t chunk;

	if (fc->fl_done < fc->fl_total) {
		chunk = fc->fl_total - fc->fl_done;
		if (chunk > (off_t)maxphys) {
			chunk = maxphys;
			chunk -= chunk % fc->fl_secsize;
		}
		cbp = g_alloc_bio();
		cbp->bio_cmd = BIO_WRITE;
		cbp->bio_offset = fc->fl_off + fc->fl_done;
		cbp->bio_data = fc->fl_buf + fc->fl_done;
		cbp->bio_length = chunk;
		cbp->bio_done = g_zoned_flush_write_done;
		cbp->bio_caller1 = fc;
		fc->fl_done += chunk;
		g_io_request(cbp, fc->fl_cp);
		return;
	}
	/* Table is on its way down; now forward the real cache flush. */
	cbp = g_alloc_bio();
	cbp->bio_cmd = BIO_FLUSH;
	cbp->bio_done = g_zoned_flush_final;
	cbp->bio_caller1 = fc;
	g_io_request(cbp, fc->fl_cp);
}

/*
 * Snapshot the dirty part of the table into a freshly allocated buffer of
 * whole sectors, encoded in the on-disk format and clear the dirty state.
 */
static u_char *
g_zoned_encode_dirty(struct g_zoned_softc *sc, off_t *offp, off_t *totalp)
{
	struct g_zoned_table_hdr th;
	u_char *buf;
	off_t total;
	uint32_t first, i, limit, seclo, sechi, zpersec;

	mtx_assert(&sc->sc_lock, MA_OWNED);

	if (!sc->sc_dirty)
		return (NULL);
	/* Table sector 0 holds the header; entries start at sector 1. */
	zpersec = sc->sc_secsize / G_ZONED_ENTRY_SIZE;
	seclo = sc->sc_hdrdirty ? 0 : 1 + sc->sc_dirtylo / zpersec;
	sechi = 1 + sc->sc_dirtyhi / zpersec;
	total = (off_t)(sechi - seclo + 1) * sc->sc_secsize;
	buf = g_malloc(total, M_NOWAIT | M_ZERO);
	if (buf == NULL)
		return (NULL);
	if (seclo == 0) {
		memset(&th, 0, sizeof(th));
		memcpy(th.th_magic, G_ZONED_TABLE_MAGIC,
		    sizeof(G_ZONED_TABLE_MAGIC));
		th.th_version = G_ZONED_VERSION;
		th.th_nzones = sc->sc_nzones;
		zoned_table_hdr_encode(&th, buf);
	}
	/* Encode every entry falling into the sectors being written. */
	first = (seclo == 0) ? 0 : (seclo - 1) * zpersec;
	limit = MIN(sc->sc_nzones, sechi * zpersec);
	for (i = first; i < limit; i++)
		g_zoned_entry_encode(&sc->sc_zones[i], buf +
		    (seclo == 0 ? sc->sc_secsize : 0) +
		    (off_t)(i - first) * G_ZONED_ENTRY_SIZE);
	sc->sc_dirty = false;
	sc->sc_hdrdirty = false;
	*offp = sc->sc_taboff + (off_t)seclo * sc->sc_secsize;
	*totalp = total;
	return (buf);
}

/*
 * Attempt commiting the dirty zone-table state for BIO_FLUSH. Returns true
 * if it took ownership of bp i.e. an async chain is running, false if the
 * caller should forward the flush normally.
 */
static bool
g_zoned_flush_begin(struct g_zoned_softc *sc, struct g_geom *gp,
    struct bio *bp)
{
	struct g_zoned_flush *fc;
	struct g_consumer *cp;
	u_char *buf;
	off_t off, total;

	cp = LIST_FIRST(&gp->consumer);
	/* Persistence is not possible without write access; just forward. */
	if (cp->acw == 0)
		return (false);

	fc = g_malloc(sizeof(*fc), M_NOWAIT);
	if (fc == NULL)
		return (false);

	mtx_lock(&sc->sc_lock);
	buf = g_zoned_encode_dirty(sc, &off, &total);
	mtx_unlock(&sc->sc_lock);
	if (buf == NULL) {
		g_free(fc);
		return (false);
	}

	fc->fl_orig = bp;
	fc->fl_cp = cp;
	fc->fl_buf = buf;
	fc->fl_off = off;
	fc->fl_total = total;
	fc->fl_done = 0;
	fc->fl_secsize = sc->sc_secsize;
	g_zoned_flush_step(fc);
	return (true);
}

/*
 * Best-effort, synchronous commit of the dirty zone-table state, for the
 * destroy path where no further BIO_FLUSH will arrive. A failure means the
 * on-disk table stays as of the last flush.
 */
static void
g_zoned_flush_sync(struct g_zoned_softc *sc, struct g_consumer *cp)
{
	u_char *buf;
	off_t chunk, done, off, total;
	int error;

	g_topology_assert();

	if (cp == NULL || cp->provider == NULL)
		return;
	if (g_access(cp, 0, 1, 0) != 0)
		return;
	mtx_lock(&sc->sc_lock);
	buf = g_zoned_encode_dirty(sc, &off, &total);
	mtx_unlock(&sc->sc_lock);
	if (buf == NULL) {
		g_access(cp, 0, -1, 0);
		return;
	}
	g_topology_unlock();
	error = 0;
	for (done = 0; done < total; done += chunk) {
		chunk = MIN(total - done, (off_t)maxphys);
		chunk -= chunk % sc->sc_secsize;
		error = g_write_data(cp, off + done, buf + done, chunk);
		if (error != 0)
			break;
	}
	if (error == 0)
		error = g_io_flush(cp);
	if (error != 0)
		G_ZONED_DEBUG(0,
		    "Final zone table write on %s failed (error=%d).",
		    cp->provider->name, error);
	g_free(buf);
	g_topology_lock();
	g_access(cp, 0, -1, 0);
}

/*
 * Test a zone against a REPORT ZONES reporting option (DISK_ZONE_REP_*).
 */
static bool
g_zoned_rep_match(const struct disk_zone_rep_entry *z, uint8_t rep_option)
{
	switch (rep_option) {
	case DISK_ZONE_REP_ALL:
		return (true);
	case DISK_ZONE_REP_EMPTY:
		return (z->zone_condition == DISK_ZONE_COND_EMPTY);
	case DISK_ZONE_REP_IMP_OPEN:
		return (z->zone_condition == DISK_ZONE_COND_IMPLICIT_OPEN);
	case DISK_ZONE_REP_EXP_OPEN:
		return (z->zone_condition == DISK_ZONE_COND_EXPLICIT_OPEN);
	case DISK_ZONE_REP_CLOSED:
		return (z->zone_condition == DISK_ZONE_COND_CLOSED);
	case DISK_ZONE_REP_FULL:
		return (z->zone_condition == DISK_ZONE_COND_FULL);
	case DISK_ZONE_REP_READONLY:
		return (z->zone_condition == DISK_ZONE_COND_READONLY);
	case DISK_ZONE_REP_OFFLINE:
		return (z->zone_condition == DISK_ZONE_COND_OFFLINE);
	case DISK_ZONE_REP_RWP:
		return ((z->zone_flags & DISK_ZONE_FLAG_RESET) != 0);
	case DISK_ZONE_REP_NON_SEQ:
		return ((z->zone_flags & DISK_ZONE_FLAG_NON_SEQ) != 0);
	case DISK_ZONE_REP_NON_WP:
		return (z->zone_condition == DISK_ZONE_COND_NOT_WP);
	default:
		return (false);
	}
}

static bool
g_zoned_rep_option_valid(uint8_t rep_option)
{
	return (rep_option <= DISK_ZONE_REP_OFFLINE ||
	    rep_option == DISK_ZONE_REP_RWP ||
	    rep_option == DISK_ZONE_REP_NON_SEQ ||
	    rep_option == DISK_ZONE_REP_NON_WP);
}

/*
 * Best-effort BIO_DELETE of just-reset zones, so that backing stores can
 * reclaim the space, similarly to real drives de-allocating a reset zone.
 */
struct g_zoned_punch {
	struct bio	*pu_orig;	/* Original BIO_ZONE. */
	u_int		 pu_pending;	/* Outstanding deletes + 1. */
};

static void
g_zoned_punch_rele(struct g_zoned_punch *pc)
{

	if (atomic_fetchadd_int(&pc->pu_pending, -1) == 1) {
		g_io_deliver(pc->pu_orig, 0);
		g_free(pc);
	}
}

static void
g_zoned_punch_done(struct bio *bp)
{
	struct g_zoned_punch *pc = bp->bio_caller1;

	g_destroy_bio(bp);
	g_zoned_punch_rele(pc);
}

static void
g_zoned_punch_zones(struct bio *bp, struct g_zoned_softc *sc, uint32_t first,
    uint32_t limit)
{
	struct g_zoned_punch *pc;
	struct g_consumer *cp;
	struct bio *dbp;
	uint32_t i;

	cp = LIST_FIRST(&bp->bio_to->geom->consumer);
	pc = (cp->acw > 0) ? g_malloc(sizeof(*pc), M_NOWAIT) : NULL;
	if (pc == NULL) {
		g_io_deliver(bp, 0);
		return;
	}
	pc->pu_orig = bp;
	pc->pu_pending = 1;
	for (i = first; i < limit; i++) {
		if (sc->sc_zones[i].zone_type == DISK_ZONE_TYPE_CONVENTIONAL)
			continue;
		dbp = g_new_bio();
		if (dbp == NULL)
			continue;
		dbp->bio_cmd = BIO_DELETE;
		dbp->bio_offset = (off_t)sc->sc_zones[i].zone_start_lba *
		    sc->sc_secsize;
		dbp->bio_length = sc->sc_zonesize;
		dbp->bio_data = NULL;
		dbp->bio_done = g_zoned_punch_done;
		dbp->bio_caller1 = pc;
		atomic_add_int(&pc->pu_pending, 1);
		g_io_request(dbp, cp);
	}
	g_zoned_punch_rele(pc);
}

/*
 * Emulate BIO_ZONE management commands.
 */
static void
g_zoned_zonecmd(struct bio *bp, struct g_zoned_softc *sc)
{
	struct disk_zone_args *args = &bp->bio_zone;
	uint32_t i, first, limit;

	switch (args->zone_cmd) {
	case DISK_ZONE_GET_PARAMS: {
		struct disk_zone_disk_params *p =
		    &args->zone_params.disk_params;

		p->zone_mode = DISK_ZONE_MODE_HOST_MANAGED;
		p->flags = DISK_ZONE_RZ_SUP | DISK_ZONE_OPEN_SUP |
		    DISK_ZONE_CLOSE_SUP | DISK_ZONE_FINISH_SUP |
		    DISK_ZONE_RWP_SUP | DISK_ZONE_MAX_SEQ_SET;
		if (!sc->sc_rdrestrict)
			p->flags |= DISK_ZONE_DISK_URSWRZ;
		/* Optimal zone counts are host-aware concepts; leave unset. */
		p->optimal_seq_zones = 0;
		p->optimal_nonseq_zones = 0;
		p->max_seq_zones = (sc->sc_maxopen != 0) ? sc->sc_maxopen :
		    G_ZONED_SEQ_UNLIMITED;
		g_io_deliver(bp, 0);
		return;
	}
	case DISK_ZONE_REPORT_ZONES: {
		struct disk_zone_report *rep = &args->zone_params.report;
		uint32_t filled, zno;

		if (!g_zoned_rep_option_valid(rep->rep_options)) {
			g_io_deliver(bp, EINVAL);
			return;
		}

		mtx_lock(&sc->sc_lock);
		sc->sc_zonecmds++;
		rep->header.same = (sc->sc_convzones == 0 ||
		    sc->sc_convzones == sc->sc_nzones) ?
		    DISK_ZONE_SAME_ALL_SAME : DISK_ZONE_SAME_TYPES_DIFFERENT;
		rep->header.maximum_lba = sc->sc_maxlba - 1;

		if (rep->starting_id >= sc->sc_maxlba)
			zno = sc->sc_nzones;
		else
			zno = g_zoned_zoneno(sc, rep->starting_id);

		rep->entries_available = 0;
		filled = 0;
		for (; zno < sc->sc_nzones; zno++) {
			if (!g_zoned_rep_match(&sc->sc_zones[zno],
			    rep->rep_options))
				continue;
			rep->entries_available++;
			if (filled < rep->entries_allocated &&
			    rep->entries != NULL)
				rep->entries[filled++] = sc->sc_zones[zno];
		}
		rep->entries_filled = filled;
		mtx_unlock(&sc->sc_lock);
		g_io_deliver(bp, 0);
		return;
	}
	case DISK_ZONE_OPEN:
	case DISK_ZONE_CLOSE:
	case DISK_ZONE_FINISH:
	case DISK_ZONE_RWP: {
		struct disk_zone_rwp *rwp = &args->zone_params.rwp;
		bool all = (rwp->flags & DISK_ZONE_RWP_FLAG_ALL) != 0;

		if (all) {
			first = 0;
			limit = sc->sc_nzones;
		} else {
			if (rwp->id >= sc->sc_maxlba) {
				g_io_deliver(bp, EINVAL);
				return;
			}
			first = g_zoned_zoneno(sc, rwp->id);
			limit = first + 1;
		}

		mtx_lock(&sc->sc_lock);
		sc->sc_zonecmds++;

		/*
		 * "Open all" must fit within the open-zone limit.
		 */
		if (all && args->zone_cmd == DISK_ZONE_OPEN &&
		    sc->sc_maxopen != 0) {
			uint32_t nclosed = 0;

			for (i = 0; i < sc->sc_nzones; i++)
				if (sc->sc_zones[i].zone_condition ==
				    DISK_ZONE_COND_CLOSED)
					nclosed++;
			if (sc->sc_nopen + nclosed > sc->sc_maxopen) {
				mtx_unlock(&sc->sc_lock);
				g_io_deliver(bp, ENOSPC);
				return;
			}
		}

		for (i = first; i < limit; i++) {
			struct disk_zone_rep_entry *z = &sc->sc_zones[i];

			/*
			 * Conventional zones have no write pointer to manage;
			 * "all zones" operations skip them, while explicitly
			 * targeting one is the caller's error. The same goes
			 * for zones taken readonly or offline.
			 */
			if (z->zone_type == DISK_ZONE_TYPE_CONVENTIONAL ||
			    z->zone_condition == DISK_ZONE_COND_READONLY ||
			    z->zone_condition == DISK_ZONE_COND_OFFLINE) {
				if (all)
					continue;
				mtx_unlock(&sc->sc_lock);
				g_io_deliver(bp, EINVAL);
				return;
			}
			switch (args->zone_cmd) {
			/*
			 * Many operations here are no-op, per ZBC-r06.
			 */
			case DISK_ZONE_OPEN:
				if (all) {
					if (z->zone_condition !=
					    DISK_ZONE_COND_CLOSED)
						continue;
				} else if (z->zone_condition ==
				    DISK_ZONE_COND_FULL ||
				    z->zone_condition ==
				    DISK_ZONE_COND_EXPLICIT_OPEN)
					continue;
				if (!g_zoned_cond_is_open(z->zone_condition) &&
				    !g_zoned_open_room(sc)) {
					mtx_unlock(&sc->sc_lock);
					g_io_deliver(bp, ENOSPC);
					return;
				}
				g_zoned_set_cond(sc, z,
				    DISK_ZONE_COND_EXPLICIT_OPEN);
				break;
			case DISK_ZONE_CLOSE:
				if (!g_zoned_cond_is_open(z->zone_condition))
					continue;
				if (z->write_pointer_lba == z->zone_start_lba)
					g_zoned_set_cond(sc, z,
					    DISK_ZONE_COND_EMPTY);
				else
					g_zoned_set_cond(sc, z,
					    DISK_ZONE_COND_CLOSED);
				break;
			case DISK_ZONE_FINISH:
				if (all &&
				    !g_zoned_cond_is_open(z->zone_condition) &&
				    z->zone_condition != DISK_ZONE_COND_CLOSED)
					continue;
				if (z->zone_condition == DISK_ZONE_COND_FULL)
					continue;
				z->write_pointer_lba = z->zone_start_lba +
				    z->zone_length;
				g_zoned_set_cond(sc, z, DISK_ZONE_COND_FULL);
				break;
			case DISK_ZONE_RWP:
				if (z->zone_condition == DISK_ZONE_COND_EMPTY)
					continue;
				z->write_pointer_lba = z->zone_start_lba;
				z->zone_flags &= ~DISK_ZONE_FLAG_RESET;
				g_zoned_set_cond(sc, z, DISK_ZONE_COND_EMPTY);
				break;
			}
			g_zoned_mark_dirty(sc, i);
		}
		mtx_unlock(&sc->sc_lock);
		if (args->zone_cmd == DISK_ZONE_RWP) {
			g_zoned_punch_zones(bp, sc, first, limit);
			return;
		}
		g_io_deliver(bp, 0);
		return;
	}
	default:
		G_ZONED_LOGREQ(bp, "Unsupported zone command %u.",
		    args->zone_cmd);
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	}
}

/*
 * Validate a write against the zone model, advancing the write pointer.
 */
static int
g_zoned_write_check(struct g_zoned_softc *sc, struct bio *bp)
{
	struct disk_zone_rep_entry *z;
	uint64_t lba, end;
	uint32_t i, last, zno;

	mtx_assert(&sc->sc_lock, MA_OWNED);

	lba = bp->bio_offset / sc->sc_secsize;
	end = (bp->bio_offset + bp->bio_length) / sc->sc_secsize;
	if (end > sc->sc_maxlba)
		return (EIO);

	zno = g_zoned_zoneno(sc, lba);
	z = &sc->sc_zones[zno];

	if (z->zone_type == DISK_ZONE_TYPE_CONVENTIONAL) {
		/*
		 * Conventional zones take writes anywhere and requests may
		 * span zone boundaries into other conventional zones.
		 */
		last = (end > lba) ? g_zoned_zoneno(sc, end - 1) : zno;
		for (i = zno; i <= last; i++) {
			if (i > zno && sc->sc_zones[i].zone_type !=
			    DISK_ZONE_TYPE_CONVENTIONAL) {
				G_ZONED_LOGREQ(bp, "Write crosses from a "
				    "conventional into a sequential zone.");
				return (EIO);
			}
			if (sc->sc_zones[i].zone_condition ==
			    DISK_ZONE_COND_READONLY ||
			    sc->sc_zones[i].zone_condition ==
			    DISK_ZONE_COND_OFFLINE) {
				G_ZONED_LOGREQ(bp,
				    "Write to a readonly/offline zone.");
				return (EIO);
			}
		}
	} else {
		if (z->zone_condition == DISK_ZONE_COND_READONLY ||
		    z->zone_condition == DISK_ZONE_COND_OFFLINE) {
			G_ZONED_LOGREQ(bp,
			    "Write to a readonly/offline zone.");
			return (EIO);
		}
		if (end > z->zone_start_lba + z->zone_length) {
			G_ZONED_LOGREQ(bp, "Write crosses a zone boundary.");
			return (EIO);
		}

		/*
		 * Sequential-write-required zones accept writes at the
		 * WP only.
		 */
		if (z->zone_condition == DISK_ZONE_COND_FULL ||
		    lba != z->write_pointer_lba) {
			G_ZONED_LOGREQ(bp,
			    "Out-of-order write to zone %u (lba %ju, wp %ju).",
			    zno, (uintmax_t)lba,
			    (uintmax_t)z->write_pointer_lba);
			return (EIO);
		}

		/*
		 * Implicitly opening one more zone(s) must respect the
		 * open-zone limit; a write that fills the zone outright
		 * never leaves it open.
		 */
		if (!g_zoned_cond_is_open(z->zone_condition) &&
		    end < z->zone_start_lba + z->zone_length &&
		    !g_zoned_open_room(sc)) {
			G_ZONED_LOGREQ(bp,
			    "Cannot implicitly open zone %u:"
			    " open-zone limit reached.", zno);
			return (ENOSPC);
		}

		z->write_pointer_lba = end;
		if (z->write_pointer_lba >= z->zone_start_lba + z->zone_length)
			g_zoned_set_cond(sc, z, DISK_ZONE_COND_FULL);
		else if (!g_zoned_cond_is_open(z->zone_condition))
			g_zoned_set_cond(sc, z, DISK_ZONE_COND_IMPLICIT_OPEN);
		g_zoned_mark_dirty(sc, zno);
	}

	sc->sc_writes++;
	sc->sc_wrotebytes += bp->bio_length;
	return (0);
}

/*
 * Validate a read against the zone model for devices created without
 * unrestricted-read (URSWRZ) support. Reads may only span conventional zones,
 * and reads in a sequential zone must end at or below the write pointer.
 */
static int
g_zoned_read_check(struct g_zoned_softc *sc, struct bio *bp)
{
	struct disk_zone_rep_entry *z;
	uint64_t lba, end;
	uint32_t i, last, zno;

	mtx_assert(&sc->sc_lock, MA_OWNED);

	lba = bp->bio_offset / sc->sc_secsize;
	end = (bp->bio_offset + bp->bio_length) / sc->sc_secsize;
	if (end > sc->sc_maxlba)
		return (EIO);

	zno = g_zoned_zoneno(sc, lba);
	z = &sc->sc_zones[zno];

	if (z->zone_type == DISK_ZONE_TYPE_CONVENTIONAL) {
		last = (end > lba) ? g_zoned_zoneno(sc, end - 1) : zno;
		for (i = zno + 1; i <= last; i++) {
			if (sc->sc_zones[i].zone_type !=
			    DISK_ZONE_TYPE_CONVENTIONAL) {
				G_ZONED_LOGREQ(bp, "Read crosses from a "
				    "conventional into a sequential zone.");
				return (EIO);
			}
		}
	} else {
		if (end > z->zone_start_lba + z->zone_length) {
			G_ZONED_LOGREQ(bp, "Read crosses a zone boundary.");
			return (EIO);
		}
		if (end > z->write_pointer_lba) {
			G_ZONED_LOGREQ(bp,
			    "Read above the write pointer of zone %u"
			    " (lba %ju, wp %ju).", zno, (uintmax_t)lba,
			    (uintmax_t)z->write_pointer_lba);
			return (EIO);
		}
	}
	return (0);
}

/*
 * A failed backing write leaves the data missing, so retract the optimistic
 * write-pointer advance, as long as no later write has moved the pointer
 * further.
 */
static void
g_zoned_write_done(struct bio *cbp)
{
	struct g_zoned_softc *sc;
	struct disk_zone_rep_entry *z;
	struct bio *pbp;
	uint64_t lba, end;
	uint32_t zno;

	pbp = cbp->bio_parent;
	sc = pbp->bio_to->geom->softc;
	if (cbp->bio_error != 0 && sc != NULL) {
		lba = cbp->bio_offset / sc->sc_secsize;
		end = (cbp->bio_offset + cbp->bio_length) / sc->sc_secsize;
		mtx_lock(&sc->sc_lock);
		zno = g_zoned_zoneno(sc, lba);
		z = &sc->sc_zones[zno];
		if (z->zone_type != DISK_ZONE_TYPE_CONVENTIONAL &&
		    z->write_pointer_lba == end) {
			z->write_pointer_lba = lba;
			g_zoned_set_cond(sc, z,
			    (lba == z->zone_start_lba) ?
				DISK_ZONE_COND_EMPTY :
				DISK_ZONE_COND_IMPLICIT_OPEN);
			g_zoned_mark_dirty(sc, zno);
		}
		mtx_unlock(&sc->sc_lock);
	}
	g_std_done(cbp);
}

static void
g_zoned_start(struct bio *bp)
{
	struct g_zoned_softc *sc;
	struct g_geom *gp;
	struct bio *cbp;
	int error;

	gp = bp->bio_to->geom;
	sc = gp->softc;
	G_ZONED_LOGREQ(bp, "Request received.");

	switch (bp->bio_cmd) {
	case BIO_ZONE:
		g_zoned_zonecmd(bp, sc);
		return;
	case BIO_WRITE:
		mtx_lock(&sc->sc_lock);
		error = g_zoned_write_check(sc, bp);
		mtx_unlock(&sc->sc_lock);
		if (error != 0) {
			g_io_deliver(bp, error);
			return;
		}
		break;
	case BIO_READ:
		mtx_lock(&sc->sc_lock);
		if (sc->sc_rdrestrict) {
			error = g_zoned_read_check(sc, bp);
			if (error != 0) {
				mtx_unlock(&sc->sc_lock);
				g_io_deliver(bp, error);
				return;
			}
		}
		sc->sc_reads++;
		sc->sc_readbytes += bp->bio_length;
		mtx_unlock(&sc->sc_lock);
		break;
	case BIO_FLUSH:
		if (g_zoned_flush_begin(sc, gp, bp))
			return;
		break;
	case BIO_GETATTR:
		/* BIO_DELETE is refused below, don't advertise it. */
		if (g_handleattr_int(bp, "GEOM::candelete", 0))
			return;
		break;
	case BIO_DELETE:
		/*
		 * Zoned device have no unmap; sequential zones are reclaimed by
		 * resetting the write pointer instead.
		 */
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	default:
		break;
	}

	cbp = g_clone_bio(bp);
	if (cbp == NULL) {
		g_io_deliver(bp, ENOMEM);
		return;
	}
	cbp->bio_done = (bp->bio_cmd == BIO_WRITE) ? g_zoned_write_done :
						     g_std_done;
	G_ZONED_LOGREQ(cbp, "Sending request.");
	g_io_request(cbp, LIST_FIRST(&gp->consumer));
}

static int
g_zoned_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_geom *gp;
	struct g_consumer *cp;

	gp = pp->geom;
	cp = LIST_FIRST(&gp->consumer);
	return (g_access(cp, dr, dw, de));
}

static struct g_geom *
g_zoned_create(struct g_class *mp, const struct g_zoned_metadata *md,
    struct g_provider *pp)
{
	struct g_zoned_softc *sc;
	struct g_geom *gp;
	struct g_provider *newpp;
	struct g_consumer *cp;
	char name[64];
	uint64_t nzones, zonesecs;
	uint32_t i;
	int error;

	g_topology_assert();

	nzones = md->md_nzones;
	zonesecs = md->md_zonesize / pp->sectorsize;
	if (nzones == 0 || zonesecs == 0 || zonesecs > G_ZONED_MAXZONESECS ||
	    md->md_nconv > G_ZONED_MAXCONV ||
	    (md->md_flags & ~G_ZONED_MD_FLAGSMASK) != 0) {
		G_ZONED_DEBUG(0, "Bogus metadata on %s.", pp->name);
		return (NULL);
	}
	for (i = 0; i < md->md_nconv; i++) {
		if (md->md_conv[i].cr_count == 0 ||
		    md->md_conv[i].cr_first >= nzones ||
		    md->md_conv[i].cr_count >
		    nzones - md->md_conv[i].cr_first) {
			G_ZONED_DEBUG(0,
			    "Bogus conventional zone range on %s.", pp->name);
			return (NULL);
		}
	}

	snprintf(name, sizeof(name), "%s%s", pp->name, G_ZONED_SUFFIX);
	LIST_FOREACH(gp, &mp->geom, geom) {
		if (strcmp(gp->name, name) == 0) {
			G_ZONED_DEBUG(0, "Device %s already exists.", name);
			return (NULL);
		}
	}

	gp = g_new_geom(mp, name);
	sc = g_malloc(sizeof(*sc), M_WAITOK | M_ZERO);

	sc->sc_id = md->md_id;
	sc->sc_zonesize = md->md_zonesize;
	sc->sc_secsize = pp->sectorsize;
	sc->sc_zonesecs = zonesecs;
	sc->sc_nzones = (uint32_t)nzones;
	sc->sc_maxlba = nzones * zonesecs;
	sc->sc_nconv = md->md_nconv;
	sc->sc_maxopen = md->md_maxopen;
	sc->sc_rdrestrict = (md->md_flags & G_ZONED_MD_RESTRICTED_READS) != 0;
	bcopy(md->md_conv, sc->sc_conv, sizeof(sc->sc_conv));
	/* Count per zone so that overlapping ranges are not double-counted. */
	for (i = 0; i < nzones; i++)
		if (g_zoned_is_conv(sc, i))
			sc->sc_convzones++;
	sc->sc_zones = malloc(nzones * sizeof(*sc->sc_zones), M_ZONED,
	    M_WAITOK | M_ZERO);
	sc->sc_tabsecs = 1 +
	    howmany(nzones * G_ZONED_ENTRY_SIZE, pp->sectorsize);
	sc->sc_taboff = (pp->mediasize - pp->sectorsize) -
	    (off_t)sc->sc_tabsecs * pp->sectorsize;
	g_zoned_reset_zones(sc);
	mtx_init(&sc->sc_lock, "gzoned lock", NULL, MTX_DEF);
	gp->softc = sc;

	newpp = g_new_providerf(gp, "%s", gp->name);
	newpp->flags |= G_PF_DIRECT_SEND | G_PF_DIRECT_RECEIVE;
	newpp->mediasize = (off_t)nzones * md->md_zonesize;
	newpp->sectorsize = pp->sectorsize;
	newpp->stripesize = pp->stripesize;
	newpp->stripeoffset = pp->stripeoffset;

	cp = g_new_consumer(gp);
	cp->flags |= G_CF_DIRECT_SEND | G_CF_DIRECT_RECEIVE;
	error = g_attach(cp, pp);
	if (error != 0) {
		G_ZONED_DEBUG(0, "Cannot attach to provider %s.", pp->name);
		goto fail;
	}

	/* Restore the persistent zone state or initialise an empty table. */
	g_zoned_load_table(sc, cp);

	newpp->flags |= pp->flags & G_PF_ACCEPT_UNMAPPED;
	g_error_provider(newpp, 0);
	G_ZONED_DEBUG(0, "Device %s created (%u zones of %jd bytes).",
	    gp->name, sc->sc_nzones, (intmax_t)sc->sc_zonesize);
	return (gp);
fail:
	if (cp->provider != NULL)
		g_detach(cp);
	g_destroy_consumer(cp);
	g_destroy_provider(newpp);
	mtx_destroy(&sc->sc_lock);
	free(sc->sc_zones, M_ZONED);
	g_free(sc);
	g_destroy_geom(gp);
	return (NULL);
}

static int
g_zoned_read_metadata(struct g_consumer *cp, struct g_zoned_metadata *md)
{
	struct g_provider *pp;
	u_char *buf;
	int error;

	g_topology_assert();

	error = g_access(cp, 1, 0, 0);
	if (error != 0)
		return (error);
	pp = cp->provider;
	g_topology_unlock();
	buf = g_read_data(cp, pp->mediasize - pp->sectorsize, pp->sectorsize,
	    &error);
	g_topology_lock();
	g_access(cp, -1, 0, 0);
	if (buf == NULL)
		return (error);
	zoned_metadata_decode(buf, md);
	g_free(buf);
	return (0);
}

static struct g_geom *
g_zoned_taste(struct g_class *mp, struct g_provider *pp, int flags __unused)
{
	struct g_zoned_metadata md;
	struct g_zoned_softc *sc;
	struct g_consumer *cp;
	struct g_geom *gp;
	int error;

	g_topology_assert();

	/* Skip providers that are already open for writing. */
	if (pp->acw > 0)
		return (NULL);

	G_ZONED_DEBUG(3, "Tasting %s.", pp->name);

	gp = g_new_geom(mp, "zoned:taste");
	cp = g_new_consumer(gp);
	cp->flags |= G_CF_DIRECT_SEND | G_CF_DIRECT_RECEIVE;
	error = g_attach(cp, pp);
	if (error == 0) {
		error = g_zoned_read_metadata(cp, &md);
		g_detach(cp);
	}
	g_destroy_consumer(cp);
	g_destroy_geom(gp);
	if (error != 0)
		return (NULL);

	if (strcmp(md.md_magic, G_ZONED_MAGIC) != 0)
		return (NULL);
	if (md.md_version > G_ZONED_VERSION) {
		printf("geom_zoned.ko module is too old to handle %s.\n",
		    pp->name);
		return (NULL);
	}
	if (md.md_provsize != (uint64_t)pp->mediasize)
		return (NULL);
	if (md.md_sectorsize != pp->sectorsize)
		return (NULL);
	if (md.md_nzones == 0 || md.md_zonesize == 0)
		return (NULL);

	/* Already running? */
	LIST_FOREACH(gp, &mp->geom, geom) {
		sc = gp->softc;
		if (sc != NULL && sc->sc_id == md.md_id)
			return (NULL);
	}

	gp = g_zoned_create(mp, &md, pp);
	if (gp == NULL)
		G_ZONED_DEBUG(0, "Cannot create zoned device on %s.",
		    pp->name);
	return (gp);
}

static int
g_zoned_destroy(struct g_geom *gp, boolean_t force)
{
	struct g_zoned_softc *sc;
	struct g_provider *pp;

	g_topology_assert();
	sc = gp->softc;
	if (sc == NULL)
		return (ENXIO);
	pp = LIST_FIRST(&gp->provider);
	if (pp != NULL && (pp->acr != 0 || pp->acw != 0 || pp->ace != 0)) {
		if (force) {
			G_ZONED_DEBUG(0,
			    "Device %s is still open, so it "
			    "can't be definitely removed.",
			    pp->name);
		} else {
			G_ZONED_DEBUG(1, "Device %s is still open (r%dw%de%d).",
			    pp->name, pp->acr, pp->acw, pp->ace);
			return (EBUSY);
		}
	} else {
		G_ZONED_DEBUG(0, "Device %s removed.", gp->name);
	}

	/* Commit unflushed zone state to survive the stop. */
	g_zoned_flush_sync(sc, LIST_FIRST(&gp->consumer));

	g_wither_geom(gp, ENXIO);
	return (0);
}

static int
g_zoned_destroy_geom(struct gctl_req *req __unused,
    struct g_class *mp __unused, struct g_geom *gp)
{

	return (g_zoned_destroy(gp, 0));
}

static void
g_zoned_orphan(struct g_consumer *cp)
{
	g_topology_assert();
	g_zoned_destroy(cp->geom, 1);
}

/*
 * The metadata and zone table live at the tail of the backing provider, which
 * has just moved: neither the on-disk state nor the zone layout can stay
 * consistent, so tear the device down. Unflushed zone state is discarded rather
 * than written to what is no longer the table's location.
 */
static void
g_zoned_resize(struct g_consumer *cp)
{
	struct g_zoned_softc *sc;

	g_topology_assert();

	sc = cp->geom->softc;
	if (sc == NULL)
		return;
	G_ZONED_DEBUG(0, "Provider %s resized; destroying %s.",
	    cp->provider->name, cp->geom->name);
	mtx_lock(&sc->sc_lock);
	sc->sc_dirty = false;
	sc->sc_hdrdirty = false;
	mtx_unlock(&sc->sc_lock);
	g_zoned_destroy(cp->geom, 1);
}

static void
g_zoned_providergone(struct g_provider *pp)
{
	struct g_geom *gp = pp->geom;
	struct g_zoned_softc *sc = gp->softc;

	gp->softc = NULL;
	free(sc->sc_zones, M_ZONED);
	mtx_destroy(&sc->sc_lock);
	g_free(sc);
}

static struct g_geom *
g_zoned_find_geom(struct g_class *mp, const char *name)
{
	struct g_geom *gp;

	if (strncmp(name, _PATH_DEV, strlen(_PATH_DEV)) == 0)
		name += strlen(_PATH_DEV);

	LIST_FOREACH(gp, &mp->geom, geom) {
		if (gp->softc != NULL && strcmp(gp->name, name) == 0)
			return (gp);
	}
	return (NULL);
}

static void
g_zoned_ctl_destroy(struct gctl_req *req, struct g_class *mp)
{
	struct g_geom *gp;
	const char *name;
	char param[16];
	int *force, *nargs, error, i;

	g_topology_assert();

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs <= 0) {
		gctl_error(req, "Missing device(s).");
		return;
	}
	force = gctl_get_paraml(req, "force", sizeof(*force));
	if (force == NULL) {
		gctl_error(req, "No '%s' argument.", "force");
		return;
	}

	for (i = 0; i < *nargs; i++) {
		snprintf(param, sizeof(param), "arg%d", i);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%d' argument.", i);
			return;
		}
		gp = g_zoned_find_geom(mp, name);
		if (gp == NULL) {
			gctl_error(req, "No such device: %s.", name);
			return;
		}
		error = g_zoned_destroy(gp, *force);
		if (error != 0) {
			gctl_error(req, "Cannot destroy device %s (error=%d).",
			    gp->name, error);
			return;
		}
	}
}

/*
 * Fault injection: force a zone readonly or offline and flag it as needing a
 * write-pointer reset, or clear the fault again. The state persists through the
 * zone table.
 */
static void
g_zoned_ctl_fault(struct gctl_req *req, struct g_class *mp)
{
	struct g_zoned_softc *sc;
	struct g_geom *gp;
	struct disk_zone_rep_entry *z;
	const char *name, *state;
	intmax_t *zone;
	uint32_t zno;

	g_topology_assert();

	name = gctl_get_asciiparam(req, "arg0");
	if (name == NULL) {
		gctl_error(req, "Missing device.");
		return;
	}
	zone = gctl_get_paraml(req, "zone", sizeof(*zone));
	if (zone == NULL) {
		gctl_error(req, "No '%s' argument.", "zone");
		return;
	}
	state = gctl_get_asciiparam(req, "state");
	if (state == NULL) {
		gctl_error(req, "No '%s' argument.", "state");
		return;
	}
	gp = g_zoned_find_geom(mp, name);
	if (gp == NULL) {
		gctl_error(req, "No such device: %s.", name);
		return;
	}
	sc = gp->softc;
	if (*zone < 0 || (uintmax_t)*zone >= sc->sc_nzones) {
		gctl_error(req, "Zone %jd out of range.", *zone);
		return;
	}
	zno = (uint32_t)*zone;

	mtx_lock(&sc->sc_lock);
	z = &sc->sc_zones[zno];
	if (strcmp(state, "ro") == 0 || strcmp(state, "readonly") == 0)
		g_zoned_set_cond(sc, z, DISK_ZONE_COND_READONLY);
	else if (strcmp(state, "offline") == 0)
		g_zoned_set_cond(sc, z, DISK_ZONE_COND_OFFLINE);
	else if (strcmp(state, "reset") == 0)
		z->zone_flags |= DISK_ZONE_FLAG_RESET;
	else if (strcmp(state, "clear") == 0) {
		/* Reobtain the normal condition from the write pointer. */
		z->zone_flags &= ~DISK_ZONE_FLAG_RESET;
		if (z->zone_type == DISK_ZONE_TYPE_CONVENTIONAL)
			g_zoned_set_cond(sc, z, DISK_ZONE_COND_NOT_WP);
		else if (z->write_pointer_lba == z->zone_start_lba)
			g_zoned_set_cond(sc, z, DISK_ZONE_COND_EMPTY);
		else if (z->write_pointer_lba ==
		    z->zone_start_lba + z->zone_length)
			g_zoned_set_cond(sc, z, DISK_ZONE_COND_FULL);
		else
			g_zoned_set_cond(sc, z, DISK_ZONE_COND_CLOSED);
	} else {
		mtx_unlock(&sc->sc_lock);
		gctl_error(req, "Invalid state '%s'.", state);
		return;
	}
	g_zoned_mark_dirty(sc, zno);
	mtx_unlock(&sc->sc_lock);
}

static void
g_zoned_config(struct gctl_req *req, struct g_class *mp, const char *verb)
{
	uint32_t *version;

	g_topology_assert();

	version = gctl_get_paraml(req, "version", sizeof(*version));
	if (version == NULL) {
		gctl_error(req, "No '%s' argument.", "version");
		return;
	}
	if (*version != G_ZONED_VERSION) {
		gctl_error(req, "Userland and kernel parts are out of sync.");
		return;
	}

	if (strcmp(verb, "destroy") == 0 || strcmp(verb, "stop") == 0) {
		g_zoned_ctl_destroy(req, mp);
		return;
	}
	if (strcmp(verb, "fault") == 0) {
		g_zoned_ctl_fault(req, mp);
		return;
	}
	gctl_error(req, "Unknown verb.");
}

static void
g_zoned_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp,
    struct g_consumer *cp, struct g_provider *pp)
{
	struct g_zoned_softc *sc;

	if (pp != NULL || cp != NULL)
		return;
	sc = gp->softc;
	sbuf_printf(sb, "%s<ZoneSize>%jd</ZoneSize>\n", indent,
	    (intmax_t)sc->sc_zonesize);
	sbuf_printf(sb, "%s<Zones>%u</Zones>\n", indent, sc->sc_nzones);
	sbuf_printf(sb, "%s<ConventionalZones>%u</ConventionalZones>\n",
	    indent, sc->sc_convzones);
	sbuf_printf(sb, "%s<Mode>Host Managed</Mode>\n", indent);
	sbuf_printf(sb, "%s<UnrestrictedReads>%s</UnrestrictedReads>\n", indent,
	    sc->sc_rdrestrict ? "No" : "Yes");
	sbuf_printf(sb, "%s<MaxOpenZones>%u</MaxOpenZones>\n", indent,
	    sc->sc_maxopen);
	sbuf_printf(sb, "%s<OpenZones>%u</OpenZones>\n", indent, sc->sc_nopen);
	sbuf_printf(sb, "%s<Reads>%ju</Reads>\n", indent, sc->sc_reads);
	sbuf_printf(sb, "%s<Writes>%ju</Writes>\n", indent, sc->sc_writes);
	sbuf_printf(sb, "%s<ReadBytes>%ju</ReadBytes>\n", indent,
	    sc->sc_readbytes);
	sbuf_printf(sb, "%s<WroteBytes>%ju</WroteBytes>\n", indent,
	    sc->sc_wrotebytes);
	sbuf_printf(sb, "%s<ZoneCommands>%ju</ZoneCommands>\n", indent,
	    sc->sc_zonecmds);
}

DECLARE_GEOM_CLASS(g_zoned_class, g_zoned);
MODULE_VERSION(geom_zoned, 0);
