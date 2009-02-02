/*
 * Copyright (c) 2006 Arne Woerner <arne_woerner@yahoo.com>
 * testing + tuning-tricks: veronica@fluffles.net
 * derived from gstripe/gmirror (Pawel Jakub Dawidek <pjd@FreeBSD.org>)
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
__FBSDID("$Id: g_raid5.c,v 1.271.1.274 2008/07/29 13:58:03 aw Exp aw $");

#ifdef KASSERT
#define MYKASSERT(a,b) KASSERT(a,b)
#else
#define MYKASSERT(a,b) do {if (!(a)) { G_RAID5_DEBUG(0,"KASSERT in line %d.",__LINE__); panic b;}} while (0)
#endif
#define ORDER(a,b) do {if (a > b) { int tmp = a; a = b; b = tmp; }} while(0)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bio.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/eventhandler.h>
#include <sys/sched.h>
#include <geom/geom.h>
#include <geom/raid5/g_raid5.h>

/*
 * our sysctl-s
 */
SYSCTL_DECL(_kern_geom);
SYSCTL_NODE(_kern_geom, OID_AUTO, raid5, CTLFLAG_RW, 0, "GEOM_RAID5 stuff");
static u_int g_raid5_cache_size_mem = 64*1024*1024;
TUNABLE_INT("kern.geom.raid5.csm", &g_raid5_cache_size_mem);
SYSCTL_INT(_kern_geom_raid5, OID_AUTO, csm, CTLFLAG_RW, &g_raid5_cache_size_mem,
      0, "cache size ((<disk count-1)*<stripe size> per bucket) in bytes");
static int g_raid5_cache_size = -5;
TUNABLE_INT("kern.geom.raid5.cs", &g_raid5_cache_size);
SYSCTL_INT(_kern_geom_raid5, OID_AUTO, cs, CTLFLAG_RW, &g_raid5_cache_size,0,
      "cache size ((<disk count-1)*<stripe size> per bucket)");
static u_int g_raid5_debug = 0;
TUNABLE_INT("kern.geom.raid5.debug", &g_raid5_debug);
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, debug, CTLFLAG_RW, &g_raid5_debug, 0,
    "Debug level");
static u_int g_raid5_tooc = 5;
TUNABLE_INT("kern.geom.raid5.tooc", &g_raid5_tooc);
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, tooc, CTLFLAG_RW, &g_raid5_tooc, 0,
    "timeout on create (in order to avoid unnecessary rebuilds on reboot)");
static u_int g_raid5_wdt = 5;
TUNABLE_INT("kern.geom.raid5.wdt", &g_raid5_wdt);
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, wdt, CTLFLAG_RW, &g_raid5_wdt, 0,
    "write request delay (in seconds)");
static u_int g_raid5_maxwql = 25;
TUNABLE_INT("kern.geom.raid5.maxwql", &g_raid5_maxwql);
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, maxwql, CTLFLAG_RW, &g_raid5_maxwql, 0,
    "max wait queue length");
static u_int g_raid5_veri_fac = 25;
TUNABLE_INT("kern.geom.raid5.veri_fac", &g_raid5_veri_fac);
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, veri_fac, CTLFLAG_RW, &g_raid5_veri_fac,
    0, "veri brake factor in case of veri_min * X < veri_max");
static u_int g_raid5_veri_nice = 100;
TUNABLE_INT("kern.geom.raid5.veri_nice", &g_raid5_veri_nice);
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO,veri_nice, CTLFLAG_RW,&g_raid5_veri_nice,
    0, "wait this many milli seconds after last user-read (less than 1sec)");
static u_int g_raid5_vsc = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, veri, CTLFLAG_RD, &g_raid5_vsc, 0,
    "verify stripe count");
static u_int g_raid5_vwc = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, veri_w, CTLFLAG_RD, &g_raid5_vwc, 0,
    "verify write count");
static u_int g_raid5_rrc = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, rreq_cnt, CTLFLAG_RD, &g_raid5_rrc, 0,
    "read request count");
static u_int g_raid5_wrc = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, wreq_cnt, CTLFLAG_RD, &g_raid5_wrc, 0,
    "write request count");
static u_int g_raid5_w1rc = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, wreq1_cnt, CTLFLAG_RD, &g_raid5_w1rc, 0,
    "write request count (1-phase)");
static u_int g_raid5_w2rc = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, wreq2_cnt, CTLFLAG_RD, &g_raid5_w2rc, 0,
    "write request count (2-phase)");
static u_int g_raid5_disks_ok = 50;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, dsk_ok, CTLFLAG_RD, &g_raid5_disks_ok,0,
    "repeat EIO'ed request?");
static u_int g_raid5_blked1 = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, blked1, CTLFLAG_RD, &g_raid5_blked1,0,
    "1. kind block count");
static u_int g_raid5_blked2 = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, blked2, CTLFLAG_RD, &g_raid5_blked2,0,
    "2. kind block count");
static u_int g_raid5_wqp = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, wqp, CTLFLAG_RD, &g_raid5_wqp,0,
    "max. write queue length");
static u_int g_raid5_mhm = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, mhm, CTLFLAG_RD, &g_raid5_mhm,0,
    "memory hamster miss");
static u_int g_raid5_mhh = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, mhh, CTLFLAG_RD, &g_raid5_mhh,0,
    "memory hamster hit");

static MALLOC_DEFINE(M_RAID5, "raid5_data", "GEOM_RAID5 Data");

static int g_raid5_destroy(struct g_raid5_softc *sc,
                           boolean_t force, boolean_t noyoyo);
static int g_raid5_destroy_geom(struct gctl_req *req, struct g_class *mp,
                                struct g_geom *gp);

static g_taste_t g_raid5_taste;
static g_ctl_req_t g_raid5_config;
static g_dumpconf_t g_raid5_dumpconf;

static eventhandler_tag g_raid5_post_sync = NULL;

static void g_raid5_init(struct g_class *mp);
static void g_raid5_fini(struct g_class *mp);

struct g_class g_raid5_class = {
	.name = G_RAID5_CLASS_NAME,
	.version = G_VERSION,
	.ctlreq = g_raid5_config,
	.taste = g_raid5_taste,
	.destroy_geom = g_raid5_destroy_geom,
	.init = g_raid5_init,
	.fini = g_raid5_fini
};

/* GCD & LCM */
static __inline u_int
gcd(u_int a, u_int b)
{
	while (b != 0) {
		u_int c = a;
		a = b;
		b = c % b;
	}
	return a;
}
static __inline u_int
g_raid5_lcm(u_int a, u_int b)
{ return ((a * b) / gcd(a, b)); }

/*
 * memory hamster stuff
 * memory hamster stores in the first sizeof(int) bytes of each chunk
 * that is requested * by malloc() the size of that chunk,
 * while the bio-s only see that chunk at offset &[sizeof(int)]...
 */
static __inline int
g_raid5_mh_sz_by_a(caddr_t m)
{ return ((int*)m)[-1]; }
static __inline int
g_raid5_mh_sz_by_i(struct g_raid5_softc *sc, int i)
{ return g_raid5_mh_sz_by_a(sc->mhl[i]); }
static __inline void
g_raid5_mh_sz(caddr_t m, int l)
{ ((int*)m)[-1] = l; }
static __inline void
g_raid5_free_by_a(caddr_t m)
{ free(m - sizeof(int), M_RAID5); }
static __inline void
g_raid5_free_by_i(struct g_raid5_softc *sc, int mi)
{ g_raid5_free_by_a(sc->mhl[mi]); }
static void
g_raid5_mh_all_free(struct g_raid5_softc *sc) {
	for (int i=0; i<sc->mhc; i++)
		g_raid5_free_by_i(sc,i);
	sc->mhc = 0;
}
static caddr_t
g_raid5_malloc(struct g_raid5_softc *sc, int l, int force)
{
	mtx_lock(&sc->mh_mtx);
	int h = l*2;
	int fi = -1;
	int fl = -1;
	int i;
	for (i=0; i<sc->mhc; i++) {
		int ml = g_raid5_mh_sz_by_i(sc,i);
		if (ml < l || ml > h)
			continue;
		if (fl > 0 && ml >= fl)
			continue;
		fl = ml;
		fi = i;
		if (ml == l)
			break;
	}
	caddr_t m;
	if (fi >= 0) {
		m = sc->mhl[fi];
		sc->mhc--;
		if (fi < sc->mhc)
			sc->mhl[fi] = sc->mhl[sc->mhc];
		g_raid5_mhh++;
		mtx_unlock(&sc->mh_mtx);
	} else {
		g_raid5_mhm++;
		mtx_unlock(&sc->mh_mtx);
		m = malloc(l+sizeof(fl), M_RAID5, M_NOWAIT);
		if (m == NULL && force) {
			g_raid5_mh_all_free(sc);
			m = malloc(l+sizeof(fl), M_RAID5, M_WAITOK);
		}
		if (m != NULL) {
			m += sizeof(fl);
			g_raid5_mh_sz(m,l);
		}
	}
	return m;
}
static void
g_raid5_free(struct g_raid5_softc *sc, caddr_t m)
{
	mtx_lock(&sc->mh_mtx);
	MYKASSERT(((int*)m)[-1] > 0, ("this is no mem hamster chunk."));
	if (sc->mhc < sc->mhs) {
		sc->mhl[sc->mhc] = m;
		sc->mhc++;
	} else {
		int l = g_raid5_mh_sz_by_a(m);
		int mi = -1;
		int ml = -1;
		for (int i=0; i<sc->mhc; i++) {
			int nl = g_raid5_mh_sz_by_i(sc,i);
			if (nl >= l)
				continue;
			if (ml > 0 && ml <= nl)
				continue;
			mi = i;
			ml = nl;
		}
		if (mi < 0)
			g_raid5_free_by_a(m);
		else {
			g_raid5_free_by_i(sc,mi);
			sc->mhl[mi] = m;
		}
	}
	mtx_unlock(&sc->mh_mtx);
}
static void
g_raid5_mh_destroy(struct g_raid5_softc *sc)
{
	g_raid5_mh_all_free(sc);
	free(sc->mhl, M_RAID5);
	mtx_destroy(&sc->mh_mtx);
}

/*
 * cache entry manager
 * implements a simple queue (fst; for next bio it (ab)uses bio's bio_queue)
 */
static __inline int
g_raid5_ce_em(struct g_raid5_cache_entry *ce)
{ return ce->fst == NULL; }
static __inline struct g_raid5_cache_entry *
g_raid5_ce_by_i(struct g_raid5_softc *sc, int i)
{ return sc->ce + i; }
static struct g_raid5_cache_entry *
g_raid5_ce_by_sno(struct g_raid5_softc *sc, off_t s)
{
	struct g_raid5_cache_entry *fce = NULL;
	MYKASSERT(s >= 0, ("s must not be negative."));
	s++;
	int i = s % sc->cs;
	for (int j=sc->cs; j>0; j--) {
		struct g_raid5_cache_entry *ce = g_raid5_ce_by_i(sc,i);
		if (ce->sno == s)
			return ce;
		if (fce==NULL && ce->sno == 0)
			fce = ce;
		i++;
		if (i == sc->cs)
			i = 0;
	}
	if (fce == NULL) {
		sc->cfc++;
		return NULL;
	}
	MYKASSERT(fce->fst == NULL, ("ce not free."));
	MYKASSERT(fce->dc == 0, ("%p dc inconsistency %d.",fce,fce->dc));
	MYKASSERT(fce->sno == 0, ("ce not free."));
	fce->sno = s;
	return fce;
}
static __inline struct g_raid5_cache_entry *
g_raid5_ce_by_off(struct g_raid5_softc *sc, off_t o)
{ return g_raid5_ce_by_sno(sc, o/sc->fsl); }
static __inline struct g_raid5_cache_entry *
g_raid5_ce_by_bio(struct g_raid5_softc *sc, struct bio *bp)
{ return g_raid5_ce_by_off(sc, bp->bio_offset); }
#define G_RAID5_C_TRAVERSE(AAA,BBB,CCC) \
	for (int i = AAA->cs-1; i >= 0; i--) \
		G_RAID5_CE_TRAVERSE((CCC=g_raid5_ce_by_i(sc,i)), BBB)
#define G_RAID5_C_TRAVSAFE(AAA,BBB,CCC) \
	for (int i = AAA->cs-1; i >= 0; i--) \
		G_RAID5_CE_TRAVSAFE((CCC=g_raid5_ce_by_i(sc,i)), BBB)
#define G_RAID5_CE_TRAVERSE(AAA, BBB) \
	for (BBB = AAA->fst; BBB != NULL; BBB = g_raid5_q_nx(BBB))
#define G_RAID5_CE_TRAVSAFE(AAA, BBB) \
	for (BBB = AAA->fst, BBB##_nxt = g_raid5_q_nx(BBB); \
	     BBB != NULL; \
	     BBB = BBB##_nxt, BBB##_nxt = g_raid5_q_nx(BBB))
static __inline void
g_raid5_dc_inc(struct g_raid5_softc *sc, struct g_raid5_cache_entry *ce)
{
	MYKASSERT(ce->dc >= 0 && sc->dc >= 0 && sc->wqp >= 0, ("cannot happen."));
	if (ce->dc == 0)
		sc->dc++;
	ce->dc++;
	sc->wqp++;
}
static __inline void
g_raid5_dc_dec(struct g_raid5_softc *sc, struct g_raid5_cache_entry *ce)
{
	MYKASSERT(ce->dc > 0 && sc->dc > 0 && sc->wqp > 0, ("cannot happen."));
	ce->dc--;
	if (ce->dc == 0)
		sc->dc--;
	sc->wqp--;
}

static __inline struct bio *
g_raid5_q_nx(struct bio *bp)
{ return bp==NULL ? NULL : bp->bio_queue.tqe_next; }
static __inline struct bio **
g_raid5_q_pv(struct bio *bp)
{ return bp->bio_queue.tqe_prev; }
static __inline void
g_raid5_q_rm(struct g_raid5_softc *sc,
             struct g_raid5_cache_entry *ce, struct bio *bp, int reserved)
{
	struct bio *nxt = g_raid5_q_nx(bp);
	bp->bio_queue.tqe_next = NULL;
	struct bio **prv = g_raid5_q_pv(bp);
	bp->bio_queue.tqe_prev = NULL;
	if (nxt != NULL)
		nxt->bio_queue.tqe_prev = prv;
	if (prv != NULL)
		(*prv) = nxt;
	if (ce->fst == bp) {
		ce->fst = nxt;
		if (nxt == NULL) {
			if (ce->sd != NULL) {
				g_raid5_free(sc,ce->sd);
				ce->sd = NULL;
			}
			if (ce->sp != NULL) {
				g_raid5_free(sc,ce->sp);
				ce->sp = NULL;
			}
			MYKASSERT(ce->dc == 0, ("dc(%d) must be zero.",ce->dc));
			MYKASSERT(sc->cc > 0, ("cc(%d) must be positive.",sc->cc));
			sc->cc--;
			if (!reserved)
				ce->sno = 0;
		}
	}
}
static __inline void
g_raid5_q_de(struct g_raid5_softc *sc,
             struct g_raid5_cache_entry *ce, struct bio *bp, int reserved)
{
	g_raid5_q_rm(sc,ce,bp,reserved);
	g_destroy_bio(bp);
}
static __inline void
g_raid5_q_in(struct g_raid5_softc *sc,
             struct g_raid5_cache_entry *ce, struct bio *bp, int force)
{
	bp->bio_queue.tqe_prev = NULL;
	bp->bio_queue.tqe_next = ce->fst;
	if (g_raid5_ce_em(ce))
		sc->cc++;
	else
		ce->fst->bio_queue.tqe_prev = &bp->bio_queue.tqe_next;
	ce->fst = bp;
	if (ce->sd == NULL)
		ce->sd = g_raid5_malloc(sc,sc->fsl,force);
	if (ce->sd != NULL)
		bp->bio_data = ce->sd + bp->bio_offset % sc->fsl;
}

static __inline int
g_raid5_bintime_cmp(struct bintime *a, struct bintime *b)
{
	if (a->sec == b->sec) {
		if (a->frac == b->frac)
			return 0;
		else if (a->frac > b->frac)
			return 1;
	} else if (a->sec > b->sec)
		return 1;
	return -1;
}

static __inline int64_t
g_raid5_bintime2micro(struct bintime *a)
{ return (a->sec*1000000) + (((a->frac>>32)*1000000)>>32); }

/*
 * tells if the disk is inserted and not pre-removed
 */
static __inline u_int
g_raid5_disk_good(struct g_raid5_softc *sc, int i)
{ return sc->sc_disks[i] != NULL && sc->preremoved[i] == 0; }

/*
 * gives the number of "good" disks...
 */
static __inline u_int
g_raid5_nvalid(struct g_raid5_softc *sc)
{
/* ARNE: just tsting */ /* this for loop should be not necessary, although it might happen, that some strange locking situation (race condition?) causes trouble*/
	int no = 0;
	for (int i = 0; i < sc->sc_ndisks; i++)
		if (g_raid5_disk_good(sc,i))
			no++; 
	MYKASSERT(no == sc->vdc, ("valid disk count deviates."));
/* ARNE: just for testing ^^^^ */

	return sc->vdc;
}

/*
 * tells if all disks r "good"...
 */
static __inline u_int
g_raid5_allgood(struct g_raid5_softc *sc)
{ return g_raid5_nvalid(sc) == sc->sc_ndisks; }

/*
 * tells if a certain offset is in a COMPLETE area of the device...
 * this area is not necessary the whole device or of size zero, because:
 *   during the REBUILD state it grows to the size of the device
 */
static __inline u_int
g_raid5_data_good(struct g_raid5_softc *sc, int i, off_t end)
{
	if (!g_raid5_disk_good(sc, i))
		return 0;
	if (!g_raid5_allgood(sc))
		return 1;
	if (sc->newest == i && sc->verified >= 0 && end > sc->verified)
		return 0;
	return 1;
}

/*
 * tells if the parity block in the full-stripe, that corresponds
 * to provider offset "end", is in the "COMPLETE" area...
 */
static __inline u_int
g_raid5_parity_good(struct g_raid5_softc *sc, int pno, off_t end)
{
	if (!g_raid5_disk_good(sc, pno))
		return 0;
	if (!g_raid5_allgood(sc))
		return 1;
	if (sc->newest != -1 && sc->newest != pno)
		return 1;
	if (sc->verified >= 0 && end > sc->verified)
		return 0;
	return 1;
}

/*
 * find internal disk number by g_consumer
 * via g_consumer.private, which contains a pointer to ...softc.sc_disks
 */
static __inline int
g_raid5_find_disk(struct g_raid5_softc * sc, struct g_consumer * cp)
{
	struct g_consumer **cpp = cp->private;
	if (cpp == NULL)
		return -1;
	struct g_consumer *rcp = *cpp;
	if (rcp == NULL)
		return -1;
	int dn = cpp - sc->sc_disks;
	MYKASSERT(dn >= 0 && dn < sc->sc_ndisks, ("dn out of range."));
	return dn;
}

/*
 * writes meta data
 */
static int
g_raid5_write_metadata(struct g_consumer **cpp, struct g_raid5_metadata *md,
                       struct bio *ur)
{
	off_t offset;
	int length;
	u_char *sector;
	int error = 0;
	struct g_consumer *cp = *cpp;

	g_topology_assert_not();

	length = cp->provider->sectorsize;
	MYKASSERT(length >= sizeof(*md), ("sector size too low (%d %d).",
	                                  length,(int)sizeof(*md)));
	offset = cp->provider->mediasize - length;

	sector = malloc(length, M_RAID5, M_WAITOK | M_ZERO);
	raid5_metadata_encode(md, sector);

	if (ur != NULL) {
		bzero(ur,sizeof(*ur));
		ur->bio_cmd = BIO_WRITE;
		ur->bio_done = NULL;
		ur->bio_offset = offset;
		ur->bio_length = length;
		ur->bio_data = sector;
		g_io_request(ur, cp);
		error = 0;
	} else {
		error = g_write_data(cp, offset, sector, length);
		free(sector, M_RAID5);
	}

	return error;
}

/*
 * reads meta data
 */
static int
g_raid5_read_metadata(struct g_consumer **cpp, struct g_raid5_metadata *md)
{
	struct g_consumer *cp = *cpp;
	struct g_provider *pp;
	u_char *buf;
	int error;

	g_topology_assert();

	pp = cp->provider;
	if (pp->error != 0)
		return pp->error;
	if (pp->sectorsize == 0)
		return ENXIO;
	MYKASSERT(pp->sectorsize >= sizeof(*md), ("sector size too low (%d %d).",
	                                          pp->sectorsize,(int)sizeof(*md)));

	error = g_access(cp, 1,0,0);
	if (error)
		return error;
	g_topology_unlock();
	buf = g_read_data(cp, pp->mediasize - pp->sectorsize, pp->sectorsize,
	                  &error);
	g_topology_lock();
	if ((*cpp) != NULL)
		g_access(cp, -1,0,0);
	if (buf == NULL)
		return (error);

	/* Decode metadata. */
	raid5_metadata_decode(buf, md);
	g_free(buf);

	return 0;
}

/*
 * updates meta data
 */
static int
g_raid5_update_metadata(struct g_raid5_softc *sc, struct g_consumer ** cpp,
                        int state, int di_no, struct bio *ur)
{
	struct g_raid5_metadata md;
	struct g_consumer *cp = *cpp;

	if (cp == NULL || sc == NULL || sc->sc_provider == NULL)
		return EINVAL;

	g_topology_assert_not();

	bzero(&md,sizeof(md));

	if (state >= 0) {
		if (sc->no_hot && (state & G_RAID5_STATE_HOT))
			state &= ~G_RAID5_STATE_HOT;

		strlcpy(md.md_magic,G_RAID5_MAGIC,sizeof(md.md_magic));
		md.md_version = G_RAID5_VERSION;
		strlcpy(md.md_name,sc->sc_name,sizeof(md.md_name));
		if (sc->hardcoded)
			strlcpy(md.md_provider,cp->provider->name,sizeof(md.md_provider));
		else
			bzero(md.md_provider,sizeof(md.md_provider));
		md.md_id = sc->sc_id;
		md.md_no = di_no;
		md.md_all = sc->sc_ndisks;
		md.md_no_hot = sc->no_hot;
		md.md_provsize = cp->provider->mediasize;
		md.md_stripesize = sc->stripesize;

		if (sc->state&G_RAID5_STATE_SAFEOP)
			state |= G_RAID5_STATE_SAFEOP;
		if (sc->state&G_RAID5_STATE_COWOP)
			state |= G_RAID5_STATE_COWOP;
		if (sc->state&G_RAID5_STATE_VERIFY)
			state |= G_RAID5_STATE_VERIFY;
		md.md_state = state;
		if (state&G_RAID5_STATE_VERIFY) {
			md.md_verified = sc->verified;
			md.md_newest = sc->newest < 0 ? md.md_no : sc->newest;
		} else {
			md.md_verified = -1;
			md.md_newest = -1;
		}
	}

	G_RAID5_DEBUG(1, "%s: %s: update meta data: state%d",
	              sc->sc_name, cp->provider->name, md.md_state);
	return g_raid5_write_metadata(cpp, &md, ur);
}

/*
 * hmm... not so state of the art wakeup procedure...
 * (tries to take into account, that the worker() thread might be already
 *  busy but unable to cope with the new situation before his next sleep)
 */
static __inline void
g_raid5_wakeup(struct g_raid5_softc *sc)
{
	mtx_lock(&sc->sleep_mtx);
	if (!sc->no_sleep)
		sc->no_sleep = 1;
	mtx_unlock(&sc->sleep_mtx);
	wakeup(&sc->worker);
}

/*
 * sleeps until wakeup, if nothing happened since the last wakeup...
 */
static __inline void
g_raid5_sleep(struct g_raid5_softc *sc, int *wt)
{
	mtx_lock(&sc->sleep_mtx);

	if (wt != NULL) {
		if (*wt)
			(*wt) = 0;
		else if (!sc->no_sleep)
			sc->no_sleep = 1;
	}

	if (sc->no_sleep || sc->cfc) {
		sc->no_sleep = 0;
		mtx_unlock(&sc->sleep_mtx);
	} else
		msleep(&sc->worker, &sc->sleep_mtx, PRIBIO | PDROP, "gr5ma", hz);
}

/*
 * detach a consumer...
 */
static __inline void
g_raid5_detach(struct g_consumer *cp)
{
	g_detach(cp);
	g_destroy_consumer(cp);
}
/*
 * detach a consumer...
 * (called by GEOM's event scheduler)
 */
static void
g_raid5_detachE(void *arg, int flags __unused)
{
	g_topology_assert();
	g_raid5_detach( (struct g_consumer*) arg);
}

/*
 * graid5 needs this to serialize configure requests
 * (e. g. before a disk can be removed, we want to be sure, that no
 *  request for that disk is g_io_request()-ed but not done())
 */
static __inline void
g_raid5_no_more_open(struct g_raid5_softc *sc)
{
	mtx_lock(&sc->open_mtx);
	while (sc->no_more_open)
		msleep(&sc->no_more_open, &sc->open_mtx, PRIBIO, "gr5nm", hz);
	sc->no_more_open = 1;
	while (sc->open_count > 0)
		msleep(&sc->open_count, &sc->open_mtx, PRIBIO, "gr5mn", hz);
	MYKASSERT(sc->open_count == 0, ("open count must be zero here."));
	mtx_unlock(&sc->open_mtx);
}
static __inline void
g_raid5_open(struct g_raid5_softc *sc)
{
	mtx_lock(&sc->open_mtx);
	while (sc->no_more_open)
		msleep(&sc->no_more_open, &sc->open_mtx, PRIBIO, "gr5op", hz);
	sc->open_count++;
	mtx_unlock(&sc->open_mtx);
}
static __inline int
g_raid5_try_open(struct g_raid5_softc *sc)
{
	mtx_lock(&sc->open_mtx);
	int can_do = !sc->no_more_open;
	if (can_do)
		sc->open_count++;
	mtx_unlock(&sc->open_mtx);
	return can_do;
}
static __inline void
g_raid5_more_open(struct g_raid5_softc *sc)
{
	MYKASSERT(sc->open_count == 0, ("open count must be zero here."));
	MYKASSERT(sc->no_more_open, ("no more open must be activated."));
	sc->no_more_open = 0;
	wakeup(&sc->no_more_open);
}
static __inline void
g_raid5_unopen(struct g_raid5_softc *sc)
{
	mtx_lock(&sc->open_mtx);
	MYKASSERT(sc->open_count > 0, ("open count must be positive here."));
	sc->open_count--;
	wakeup(&sc->open_count);
	mtx_unlock(&sc->open_mtx);
}

/*
 * remove a disk from graid5
 * can be called by GEOM if a device disappears...
 */
static void
g_raid5_remove_disk(struct g_raid5_softc *sc, struct g_consumer ** cpp,
                    int clear_md, int noyoyo)
{
	struct g_consumer * cp;

	g_topology_assert();

	cp = *cpp;
	if (cp == NULL)
		return;

	g_topology_unlock();
	g_raid5_no_more_open(sc);
	g_topology_lock();

	g_raid5_disks_ok = 50;
	int dn = g_raid5_find_disk(sc, cp);
	MYKASSERT(dn >= 0, ("unknown disk"));
	if (sc->preremoved[dn])
		sc->preremoved[dn] = 0;
	else
		sc->vdc--;
	if (sc->state & G_RAID5_STATE_VERIFY)
		G_RAID5_DEBUG(0, "%s: %s(%d): WARNING: removed while %d is missing.",
		              sc->sc_name, cp->provider->name, dn, sc->newest);

	G_RAID5_DEBUG(0, "%s: %s(%d): disk removed.",
	              sc->sc_name, cp->provider->name, dn);

	*cpp = NULL;

	if (sc->sc_type == G_RAID5_TYPE_AUTOMATIC) {
		g_topology_unlock();
		g_raid5_update_metadata(sc,&cp,clear_md?-1:G_RAID5_STATE_CALM,dn,NULL);
		g_topology_lock();
		if (clear_md)
			sc->state |= G_RAID5_STATE_VERIFY;
	}

	if (cp->acr || cp->acw || cp->ace)
		g_access(cp, -cp->acr, -cp->acw, -cp->ace);

	if (noyoyo)
		g_post_event(g_raid5_detachE, cp, M_WAITOK, NULL);
	else
		g_raid5_detach(cp);

	g_raid5_more_open(sc);
	g_raid5_wakeup(sc);
}

static void
g_raid5_orphan(struct g_consumer *cp)
{
	struct g_raid5_softc *sc;
	struct g_consumer **cpp;
	struct g_geom *gp;

	g_topology_assert();
	gp = cp->geom;
	sc = gp->softc;
	if (sc == NULL)
		return;

	cpp = cp->private;
	if ((*cpp) == NULL)	/* Possible? */
		return;
	g_raid5_remove_disk(sc, cpp, 0, 0);
}

static __inline void
g_raid5_free_bio(struct g_raid5_softc *sc, struct bio *bp)
{
	g_raid5_free(sc,bp->bio_data);
	g_destroy_bio(bp);
}

/*
 * combines 2 bio-s
 * a new bio with a bio, that is already queued
 */
static __inline void
g_raid5_combine(struct g_raid5_softc *sc, struct g_raid5_cache_entry *ce,
                struct bio *bp, struct bio *wbp, off_t woff, int l, off_t mend)
{
	off_t noff = MIN(bp->bio_offset, woff);
	bp->bio_offset = noff;
	bp->bio_length = mend - noff;
	bp->bio_data = ce->sd + noff % sc->fsl;
	bp->bio_t0.sec = wbp->bio_t0.sec;
}

/*
 * combines 2 bio-s
 * both r already queued, before the function is called,
 * and the first one is dequeued, when the function returns,
 *   while the second bio remains in the queue
 */
static void
g_raid5_combine_inner(struct g_raid5_softc *sc, struct g_raid5_cache_entry *ce,
                      struct bio *bp, struct bio *combo, off_t end)
{
	off_t noff = MIN(bp->bio_offset, combo->bio_offset);
	off_t mend = MAX(end, combo->bio_offset + combo->bio_length);
	combo->bio_offset = noff;
	combo->bio_length = mend - noff;
	combo->bio_data = ce->sd + noff % sc->fsl;
	if (!combo->bio_caller1 && bp->bio_caller1) /* inherit block */
		combo->bio_caller1 = bp->bio_caller1;
	g_raid5_q_de(sc,ce,bp,1);
}

/*
 * queued bio-s must have one of these states at each time:
 *   pending:   current data   & write request still necessary
 *   issued:    current data   & write request sent to consumers but not done
 *   requested: undefined data & read request sent to consumers but not done
 *   cached:    current data   & no action necessary or pending
 *   done:      current data   & workerD() chewed it threw and
 *                               now worker() must finalize it
 *   bad (wr):  current data   & workerD() chewed it threw and
 *                               write request failed
 *   bad (rd):  undefined data & workerD() chewed it threw and
 *                               read request failed
 *   current: is pending, issued or cached
 *   started: is issued or requested
 */
static __inline int
g_raid5_is_pending(struct bio *bp)
{ return bp->bio_parent == NULL; }
static __inline int
g_raid5_is_issued(struct g_raid5_softc *sc, struct bio *bp)
{ return bp->bio_parent == (void*) sc; }
static __inline int
g_raid5_is_requested(struct g_raid5_softc *sc, struct bio *bp)
{ return bp->bio_parent == (void*) &sc->cc; }
static __inline int
g_raid5_is_cached(struct g_raid5_softc *sc, struct bio *bp)
{ return bp->bio_parent == (void*) sc->ce; }
static __inline int
g_raid5_is_current(struct g_raid5_softc *sc, struct bio *bp)
{
	return g_raid5_is_cached(sc,bp) ||
	       g_raid5_is_pending(bp) ||
	       g_raid5_is_issued(sc,bp);
}
static __inline int
g_raid5_is_started(struct g_raid5_softc *sc, struct bio *bp)
{ return g_raid5_is_issued(sc,bp) || g_raid5_is_requested(sc,bp); }
static __inline int
g_raid5_is_done(struct g_raid5_softc *sc, struct bio *bp)
{ return g_raid5_is_started(sc,bp) && bp->bio_driver1 == bp; }
static __inline int
g_raid5_is_bad(struct g_raid5_softc *sc, struct bio *bp)
{ return g_raid5_is_started(sc,bp) && bp->bio_caller2 == bp; }

/* cache state codes for ..._dumpconf() */
static __inline char
g_raid5_cache_code(struct g_raid5_softc *sc, struct bio *bp)
{
	if (g_raid5_is_requested(sc,bp))
		return 'r';
	if (g_raid5_is_issued(sc,bp))
		return 'a';
	if (g_raid5_is_cached(sc,bp))
		return 'c';
	return 'p';
}

static __inline void
g_raid5_set_pending(struct bio *bp)
{ bp->bio_parent = NULL; }
static __inline void
g_raid5_set_issued(struct g_raid5_softc *sc, struct bio *bp)
{ bp->bio_parent = (void*) sc; }
static __inline void
g_raid5_set_requested(struct g_raid5_softc *sc, struct bio *bp)
{ bp->bio_parent = (void*) &sc->cc; }
static __inline void
g_raid5_set_cached(struct g_raid5_softc *sc, struct bio *bp)
{ bp->bio_parent = (void*) sc->ce; }
static __inline void
g_raid5_set_bad(struct bio *bp)
{ bp->bio_caller2 = bp; }
static __inline void
g_raid5_set_done(struct bio *bp)
{ bp->bio_driver1 = bp; }

/*
 * Check if bbp lies in the same "full-stripe" (set of corresponding blocks)
 * as a write request, that is already issued (important for serialization of
 * 2-phase write-requests)
 */
static int
g_raid5_stripe_conflict(struct g_raid5_softc *sc,
                        struct g_raid5_cache_entry *ce, struct bio *bbp)
{
	MYKASSERT(g_raid5_is_pending(bbp), ("must not be issued."));
	MYKASSERT(bbp->bio_caller1 == NULL, ("must not be blocked."));
	if (ce == NULL)
		return 1;
	if (bbp->bio_length == 0)
		return 0;

	off_t bbsno = (bbp->bio_offset >> sc->stripebits) / (sc->sc_ndisks - 1);
	int blow = bbp->bio_offset & (sc->stripesize - 1);
	off_t besno = bbp->bio_offset + bbp->bio_length - 1;
	int bhih = besno & (sc->stripesize - 1);
	ORDER(blow,bhih);
	besno = (besno >> sc->stripebits) / (sc->sc_ndisks - 1);

	struct bio *bp;
	G_RAID5_CE_TRAVERSE(ce, bp) {
		if (bp == bbp)
			continue;
		if (bp->bio_length == 0)
			continue;
		if (!g_raid5_is_issued(sc,bp))
			continue;

		off_t bsno = (bp->bio_offset >> sc->stripebits) / (sc->sc_ndisks - 1);
		int low = bp->bio_offset & (sc->stripesize - 1);

		off_t esno = bp->bio_offset + bp->bio_length - 1;
		int hih = esno & (sc->stripesize - 1);
		ORDER(low,hih);
		esno = (esno >> sc->stripebits) / (sc->sc_ndisks - 1);

		if (besno >= bsno && esno >= bbsno && bhih >= low && hih >= blow)
			return 1;
	}

	return 0;
}

/*
 * check if interval a overlaps with b (really; not only adjacent)
 */
static __inline int
g_raid5_overlapf(off_t a1, off_t a2, off_t b1, off_t b2)
{ return a2 > b1 && b2 > a1; }

/*
 * check if interval a overlaps with b (really; not only adjacent)
 */
static __inline int
g_raid5_overlapf_by_bio(struct bio *bp, struct bio *bbp)
{
	off_t end = bp->bio_offset + bp->bio_length;
	off_t bend = bbp->bio_offset + bbp->bio_length;
	return g_raid5_overlapf(bp->bio_offset,end, bbp->bio_offset,bend);
}

/*
 * check if intervals a and b r adjacent
 */
static __inline int
g_raid5_flank(off_t a1, off_t a2, off_t b1, off_t b2)
{ return a2 == b1 || b2 == a1; }

/*
 * check if intervals a and b r adjacent or overlapping
 */
static __inline int
g_raid5_overlap(off_t a1, off_t a2, off_t b1, off_t b2, int *overlapf)
{
	(*overlapf) = g_raid5_overlapf(a1,a2, b1,b2);
	return (*overlapf) || g_raid5_flank(a1,a2, b1,b2);
}

/*
 * check if bbp is still overlapping with another bio,
 *   that is waiting to be ...done()
 */
static __inline int
g_raid5_still_blocked(struct g_raid5_softc *sc,
                      struct g_raid5_cache_entry *ce, struct bio *bbp)
{
	MYKASSERT(g_raid5_is_pending(bbp), ("must not be issued."));
	MYKASSERT(bbp->bio_caller1 != NULL, ("must be blocked."));

	struct bio *bp;
	G_RAID5_CE_TRAVERSE(ce, bp) {
		if (bp == bbp)
			continue;
		if (g_raid5_is_cached(sc,bp))
			continue;
		if (g_raid5_overlapf_by_bio(bp,bbp)) {
			MYKASSERT(g_raid5_is_started(sc,bp),
			          ("combo error found with %p/%d(%p,%p):%jd+%jd %p/%d(%p,%p):%jd+%jd", bp,bp->bio_cmd==BIO_READ,bp->bio_parent,sc,bp->bio_offset,bp->bio_length, bbp,bbp->bio_cmd==BIO_READ,bbp->bio_parent,sc,bbp->bio_offset,bbp->bio_length));
			return 1;
		}
	}

	return 0;
}

/*
 * reset the preremove bits
 */
static void
g_raid5_preremove_reset(struct g_raid5_softc *sc)
{
	for (int i=sc->sc_ndisks-1; i>=0; i--)
		if (sc->preremoved[i]) {
			sc->preremoved[i] = 0;
			sc->vdc++;
		}
}

/*
 * find out if b is inside [a->bio_data .. a->bio_data+a->bio_len[
 */
static __inline int
g_raid5_extra_mem(struct bio *a, caddr_t b)
{
	if (a->bio_data == NULL)
		return 1;
	register_t delta = ((uint8_t*)b) - ((uint8_t*)a->bio_data);
	return delta < 0 || delta >= a->bio_length;
}

/*
 * xor two memory areas of length len
 * len must be an integer multiple of sizeof(u_register_t),
 *   which should be always the case for sectors/stripes
 */
static __inline void
g_raid5_xor(caddr_t a, caddr_t b, int len)
{
	u_register_t *aa = (u_register_t*) a;
	u_register_t *bb = (u_register_t*) b;
	MYKASSERT(len % sizeof(*aa) == 0, ("len has wrong modul."));
	len /= sizeof(*aa);
	while (len > 0) {
		(*aa) ^= (*bb);
		aa++;
		bb++;
		len--;
	}
}

/*
 * compare two memory areas
 */
static __inline int
g_raid5_neq(caddr_t a, caddr_t b, int len)
{
	u_register_t *aa = (u_register_t*) a;
	u_register_t *bb = (u_register_t*) b;
	MYKASSERT(len % sizeof(*aa) == 0, ("len has wrong modul."));
	len /= sizeof(*aa);
	while (len > 0) {
		if ((*aa) != (*bb))
			return 1;
		aa++;
		bb++;
		len--;
	}
	return 0;
}

/*
 * start bio cbp, if corresponding disk is available...
 */
static __inline void
g_raid5_io_req(struct g_raid5_softc *sc, struct bio *cbp)
{
	struct g_consumer *disk = cbp->bio_driver2;
	MYKASSERT(disk != NULL, ("disk must be present"));
	cbp->bio_driver2 = NULL;
	if (g_raid5_find_disk(sc, disk) >= 0) {
		if (cbp->bio_children) {
			cbp->bio_children = 0;
			cbp->bio_inbed = 0;
		}
		g_io_request(cbp, disk);
	} else {
		cbp->bio_error = EIO;
		if (cbp->bio_to == NULL)
			cbp->bio_to = sc->sc_provider;
		g_io_deliver(cbp, cbp->bio_error);
	}
}

/*
 * workerD() thread uses this to mark a internally queued bio as
 * "bad" or "done"
 */
static __inline void
g_raid5_cache_trans(struct g_raid5_softc *sc, struct bio *pbp, struct bio **obp)
{
	if (pbp->bio_caller2 != NULL) { /* no prefetch */
		(*obp) = pbp->bio_caller2;
		pbp->bio_caller2 = NULL;
		MYKASSERT(*obp != pbp, ("bad structure."));
		MYKASSERT((*obp)->bio_cmd == BIO_READ, ("need BIO_READ here."));
		MYKASSERT(pbp->bio_caller1 != NULL, ("wrong memory area."));
		MYKASSERT(!g_raid5_extra_mem(*obp,pbp->bio_caller1), ("bad mem"));
		bcopy(pbp->bio_data, pbp->bio_caller1, pbp->bio_completed);
		pbp->bio_caller1 = NULL;
		(*obp)->bio_completed += pbp->bio_completed;
		(*obp)->bio_inbed++;
		if ((*obp)->bio_inbed < (*obp)->bio_children)
			g_raid5_unopen(sc);
		if (pbp->bio_error && !(*obp)->bio_error)
			(*obp)->bio_error = pbp->bio_error;
	} else
		MYKASSERT(pbp->bio_caller1 == NULL, ("wrong structure."));
	if (pbp->bio_error)
		g_raid5_set_bad(pbp);
	else
		g_raid5_set_done(pbp);
}

/*
 * process request from internal done queue
 * called by workerD() thread
 */
static void
g_raid5_ready(struct g_raid5_softc *sc, struct bio *bp)
{
	MYKASSERT(bp != NULL, ("BIO must not be zero here."));

	struct bio *pbp = bp->bio_parent;
	MYKASSERT(pbp != NULL, ("pbp must not be NULL."));

	struct bio *obp;
	if (pbp->bio_offset < 0 && pbp->bio_parent != NULL)
		obp = pbp->bio_parent;
	else
		obp = pbp;

	/*XXX: inefficient in case of _cmp and _xor (one thread for each request would be better)*/

	pbp->bio_inbed++;
	MYKASSERT(pbp->bio_inbed<=pbp->bio_children, ("more inbed than children."));

	if (bp->bio_error) {
		if (!pbp->bio_error) {
			pbp->bio_error = bp->bio_error;
			if (!obp->bio_error)
				obp->bio_error = pbp->bio_error;
		}
		if (obp->bio_error == EIO || obp->bio_error == ENXIO) {
			struct g_consumer *cp = bp->bio_from;
			int dn = g_raid5_find_disk(sc, cp);
			if (g_raid5_disks_ok < 40) {
				g_raid5_preremove_reset(sc);
				sc->preremoved[dn] = 1;
				sc->vdc--;
				G_RAID5_DEBUG(0,"%s: %s(%d): pre-remove disk due to errors.",
				              sc->sc_name, cp->provider->name, dn);
			}
			if (g_raid5_disks_ok > 0)
				g_raid5_disks_ok--;
			else
				g_error_provider(sc->sc_provider, obp->bio_error);
		}
		G_RAID5_DEBUG(0,"%s: %p: cmd%c off%jd len%jd err:%d/%d c%d",
		              sc->sc_name, obp, obp->bio_cmd==BIO_READ?'R':'W',
		              obp->bio_offset, obp->bio_length,
		              bp->bio_error,obp->bio_error,g_raid5_disks_ok);
	}

	int saved = 0;
	int extra = g_raid5_extra_mem(obp,bp->bio_data);
	if (bp->bio_cmd == BIO_READ) {
		if (obp == pbp) {
			/* best case read */
			MYKASSERT(pbp->bio_cmd == BIO_READ, ("need BIO_READ here."));
			MYKASSERT(g_raid5_is_requested(sc,pbp), ("bad structure"));
			MYKASSERT(!extra, ("wrong mem area."));
			pbp->bio_completed += bp->bio_completed;
			if (pbp->bio_inbed == pbp->bio_children)
				g_raid5_cache_trans(sc, pbp,&obp);
		} else if (obp->bio_cmd == BIO_READ &&
		           pbp->bio_children == sc->sc_ndisks) {
			/* verify read */
			MYKASSERT(pbp->bio_cmd == BIO_WRITE, ("unkown conf."));
			MYKASSERT(bp->bio_offset == -pbp->bio_offset-1, ("offset"));
			if (bp->bio_length == 0) {
				MYKASSERT(obp->bio_driver2 == &sc->worker, ("state"));
				bp->bio_length = pbp->bio_length / 2;
				MYKASSERT(pbp->bio_data+bp->bio_length == bp->bio_data,
				          ("datptr %p+%jd %p",
				           pbp->bio_data,bp->bio_length,bp->bio_data));
			}
			MYKASSERT(bp->bio_length*2 == pbp->bio_length, ("lengths"));
			if (pbp->bio_data+bp->bio_length != bp->bio_data) {
				/* not the stripe in question */
				g_raid5_xor(pbp->bio_data,bp->bio_data,bp->bio_length);
				if (extra)
					saved = 1;
			}
			if (pbp->bio_inbed == pbp->bio_children) {
				g_raid5_vsc++;
				if (pbp->bio_driver1 != NULL) {
					MYKASSERT(!g_raid5_extra_mem(obp,pbp->bio_driver1),("bad addr"));
					bcopy(pbp->bio_data,pbp->bio_driver1,bp->bio_length);
					pbp->bio_driver1 = NULL;
				}
				if (obp->bio_error == 0 && obp->bio_driver2 == &sc->worker) {
					/* corrective write */
				   MYKASSERT(sc->state&G_RAID5_STATE_VERIFY, ("state"));
					MYKASSERT(pbp->bio_caller1 == NULL, ("no counting"));
					g_raid5_vwc++;
					pbp->bio_offset = -pbp->bio_offset-1;
					MYKASSERT(bp->bio_offset == pbp->bio_offset, ("offsets"));
					pbp->bio_length /= 2;
					pbp->bio_children = 0;
					pbp->bio_inbed = 0;
					pbp->bio_caller1 = obp;
					g_raid5_io_req(sc, pbp);
				} else if (obp->bio_error ||
				           !g_raid5_neq(pbp->bio_data, pbp->bio_data+bp->bio_length,
				                        bp->bio_length)) {
					/* read was ok or error */
					pbp->bio_offset = -pbp->bio_offset-1;
					obp->bio_completed += bp->bio_length;
					obp->bio_inbed++;
					MYKASSERT(g_raid5_extra_mem(obp,pbp->bio_data), ("bad addr"));
					g_raid5_free_bio(sc,pbp);
				} else { /* parity mismatch - no correction */
					if (!obp->bio_error)
						obp->bio_error = EIO;
					obp->bio_inbed++;
					MYKASSERT(g_raid5_extra_mem(obp,pbp->bio_data), ("bad addr"));
					int pos;
					for (pos=0; pos<bp->bio_length; pos++)
						if (((char*)pbp->bio_data)[pos] !=
						    ((char*)pbp->bio_data)[pos+bp->bio_length])
							break;
					G_RAID5_DEBUG(0,"%s: %p: parity mismatch: %jd+%jd@%d.",
					              sc->sc_name,obp,bp->bio_offset,bp->bio_length,pos);
					g_raid5_free_bio(sc,pbp);
				}
			}
		} else if (obp->bio_cmd == BIO_WRITE &&
		           pbp->bio_children == sc->sc_ndisks-2 &&
		           g_raid5_extra_mem(obp,pbp->bio_data)) {
			/* preparative read for degraded case write */
			MYKASSERT(extra, ("wrong memory area."));
			MYKASSERT(bp->bio_offset == -pbp->bio_offset-1,
			          ("offsets must correspond"));
			MYKASSERT(bp->bio_length == pbp->bio_length,
			          ("length must correspond"));
			g_raid5_xor(pbp->bio_data,bp->bio_data,bp->bio_length);
			saved = 1;
			if (pbp->bio_inbed == pbp->bio_children) {
				pbp->bio_offset = -pbp->bio_offset-1;
				MYKASSERT(g_raid5_extra_mem(obp,pbp->bio_data), ("bad addr"));
				if (pbp->bio_error) {
					obp->bio_inbed++;
					g_raid5_free_bio(sc,pbp);
				} else
					g_raid5_io_req(sc,pbp);
			}
		} else if ( obp->bio_cmd == BIO_WRITE &&
		            (pbp->bio_children == 2 ||
		             (sc->sc_ndisks == 3 && pbp->bio_children == 1)) ) {
			/* preparative read for best case 2-phase write */
			MYKASSERT(bp->bio_offset == -pbp->bio_offset-1,
					  ("offsets must correspond. %jd / %jd",
			         bp->bio_offset, pbp->bio_offset));
			MYKASSERT(bp->bio_length == pbp->bio_length,
					  ("length must correspond. %jd / %jd",
			         bp->bio_length, pbp->bio_length));
			MYKASSERT(extra, ("wrong memory area %p/%jd+%jd -- %p/%jd+%jd.",
			                  obp->bio_data,obp->bio_offset,obp->bio_length,
			                  bp->bio_data,obp->bio_offset,bp->bio_length));
			struct bio *pab = pbp->bio_caller2;
			g_raid5_xor(pab->bio_data,bp->bio_data,bp->bio_length);
			saved = 1;
			if (pbp->bio_inbed == pbp->bio_children) {
				pbp->bio_offset = -pbp->bio_offset-1;
				pab->bio_offset = -pab->bio_offset-1;
				MYKASSERT(pab->bio_length == pbp->bio_length,
				        ("lengths must correspond"));
				MYKASSERT(pbp->bio_offset == pab->bio_offset,
				        ("offsets must correspond"));
				MYKASSERT(pbp->bio_driver2 != pab->bio_driver2,
				        ("disks must be different"));
				MYKASSERT(g_raid5_extra_mem(obp,pab->bio_data), ("bad addr"));
				MYKASSERT(!g_raid5_extra_mem(obp,pbp->bio_data), ("bad addr"));
				if (pbp->bio_error) {
					obp->bio_inbed += 2;
					g_raid5_free_bio(sc,pab);
					g_destroy_bio(pbp);
				} else {
					g_raid5_io_req(sc, pab);
					g_raid5_io_req(sc, pbp);
				}
			}
		} else {
			/* read degraded stripe */
			MYKASSERT(obp->bio_cmd == BIO_READ, ("need BIO_READ here."));
			MYKASSERT(g_raid5_is_requested(sc,obp), ("bad structure"));
			MYKASSERT(pbp->bio_children == sc->sc_ndisks-1,
			        ("must have %d children here.", sc->sc_ndisks-1));
			MYKASSERT(extra, ("wrong memory area."));
			MYKASSERT(bp->bio_length==pbp->bio_length,("length must correspond."));
			g_raid5_xor(pbp->bio_data,bp->bio_data,bp->bio_length);
			saved = 1;
			if (pbp->bio_inbed == pbp->bio_children) {
				obp->bio_completed += bp->bio_completed;
				obp->bio_inbed++;
				g_destroy_bio(pbp);
				if (obp->bio_inbed == obp->bio_children) {
					pbp = obp;
					g_raid5_cache_trans(sc, pbp,&obp);
				}
			}
		}
	} else {
		MYKASSERT(bp->bio_cmd == BIO_WRITE, ("bio_cmd must be BIO_WRITE here."));
		MYKASSERT(obp == pbp, ("invalid write request configuration"));
		if (extra)
			saved = 1;
		if (bp->bio_caller1 == obp)
			obp->bio_completed += bp->bio_completed;
	}

	if (saved)
		g_raid5_free_bio(sc,bp);
	else
		g_destroy_bio(bp);

	MYKASSERT(obp->bio_inbed <= obp->bio_children, ("inbed > children."));
	if (obp->bio_inbed == obp->bio_children) {
		if (obp->bio_driver2 != NULL) {
			MYKASSERT(obp->bio_driver2 == &sc->worker, ("driver2 is misused."));
			/* corrective verify read requested by worker() */
			obp->bio_driver2 = NULL;
		} else {
			if (g_raid5_disks_ok > 0 &&
			    (obp->bio_error == EIO || obp->bio_error == ENXIO)) {
				obp->bio_error = ENOMEM;
				obp->bio_completed = 0;
				obp->bio_children = 0;
				obp->bio_inbed = 0;
			} else if (obp->bio_error == 0 && g_raid5_disks_ok < 30)
				g_raid5_disks_ok = 50;
			if (g_raid5_is_issued(sc,obp) && obp->bio_cmd == BIO_WRITE) {
				if (obp->bio_error == ENOMEM)
					g_raid5_set_bad(obp); /* retry! */
				else {
					if (obp->bio_error) {
						g_raid5_set_bad(obp); /* abort! */
						G_RAID5_DEBUG(0,"%s: %p: lost data: off%jd len%jd error%d.",
										  sc->sc_name,obp,
						              obp->bio_offset,obp->bio_length,obp->bio_error);
						g_error_provider(sc->sc_provider, obp->bio_error);
							/* cancels all pending write requests */
					} else /* done cleanly */
						g_raid5_set_done(obp);
					wakeup(&sc->wqp);
				}
			} else {
				MYKASSERT(obp->bio_cmd == BIO_READ,
				          ("incompetent for non-BIO_READ %jd/%jd %d %p/%p.",
				           obp->bio_length,obp->bio_offset,
				           sc->sc_ndisks,obp->bio_parent,sc));
				if (obp != pbp)
					g_io_deliver(obp, obp->bio_error);
			}
		}
		g_raid5_wakeup(sc);

		g_raid5_unopen(sc);
	}
}

/*
 * called by GEOM, if bp is done...
 */
static void
g_raid5_done(struct bio *bp)
{
	struct g_raid5_softc *sc = bp->bio_from->geom->softc;
	MYKASSERT(sc != NULL, ("SC must not be zero here."));
	G_RAID5_LOGREQ(bp, "[done err:%d dat:%02x adr:%p]",
	               bp->bio_error,*(unsigned char*)bp->bio_data,bp->bio_data);

	if (sc->workerD == NULL)
		g_raid5_ready(sc, bp);
	else {
		mtx_lock(&sc->dq_mtx);
		bioq_insert_tail(&sc->dq, bp);
		wakeup(&sc->workerD);
		mtx_unlock(&sc->dq_mtx);
	}
}

/*
 * prepare a new bio with certain parameters
 */
static struct bio *
g_raid5_prep(struct g_raid5_softc *sc,
             struct bio_queue_head *queue, struct bio *bp, uint8_t cmd,
             off_t offset, int len, u_char *data, struct g_consumer *disk)
{
	MYKASSERT(disk!=NULL || (offset<0 && cmd==BIO_READ),
	          ("disk shall be not NULL here."));

	struct bio *cbp = g_new_bio();
	if (cbp == NULL)
		return NULL;

	if (data == NULL) {
		cbp->bio_data = g_raid5_malloc(sc,len,0);
		if (cbp->bio_data == NULL) {
			g_destroy_bio(cbp);
			return NULL;
		}
	}

	bp->bio_children++;
	bioq_insert_tail(queue, cbp);
	cbp->bio_parent = bp;
	cbp->bio_cmd = cmd;
	cbp->bio_length = len;
	cbp->bio_offset = offset;
	cbp->bio_attribute = bp->bio_attribute;
	cbp->bio_done = g_raid5_done;
	cbp->bio_driver2 = disk;
	if (data != NULL)
		cbp->bio_data = data;

	return cbp;
}

/*
 * READ ALL parts and build XOR and cmp with zero and {report|save data}
 * this is needed for SAFEOP mode and for REBUILD requests
 */
static int
g_raid5_conv_veri_read(struct bio_queue_head *queue, struct g_raid5_softc *sc,
                       struct bio *obp, off_t loff, int len, int dno, int pno,
                       caddr_t data)
{
	if (!g_raid5_allgood(sc))
		return EIO;

	int qno;
	if (data == NULL)
		qno = sc->newest >= 0 ? sc->newest : pno;
	else
		qno = dno;
	if (!g_raid5_disk_good(sc,qno))
		return EIO;

	struct bio *root = g_raid5_prep(sc,queue,obp,BIO_WRITE,-loff-1,len*2,
	                                NULL,sc->sc_disks[qno]);
	if (root == NULL)
		return ENOMEM;
	bzero(root->bio_data, len);
	root->bio_driver1 = data;
	for (int i=0; i<sc->sc_ndisks; i++) {
		u_char *d = i == qno ? root->bio_data + len : NULL;
		int l = (i == qno && obp->bio_driver2 == &sc->worker) ? 0 : len;
		if (!g_raid5_disk_good(sc,i))
			return EIO;
		struct bio *son = g_raid5_prep(sc,queue,root,BIO_READ,loff,l,
		                               d,sc->sc_disks[i]);
		if (son == NULL)
			return ENOMEM;
	}

	return 0;
}

/*
 * flush the prepared requests for a certain bio in the cache...
 */
static void
g_raid5_req_sq(struct g_raid5_softc *sc, struct bio_queue_head *queue)
{
	struct bio *prv = 0;
	for (;;) {
		struct bio *cbp = bioq_first(queue);
		if (cbp == NULL)
			break;
		bioq_remove(queue, cbp);

		cbp->bio_caller2 = prv;
		prv = cbp;

		if (cbp->bio_offset < 0)
			continue;

		if (cbp->bio_cmd == BIO_WRITE)
			cbp->bio_caller2 = NULL;
		g_raid5_io_req(sc, cbp);
	}
}

/*
 * flush the prepared requests for a certain bio in the cache...
 * e. g. because preparation was interrupted by ENOMEM
 */
static void
g_raid5_des_sq(struct g_raid5_softc *sc,
               struct bio_queue_head *queue, struct bio *bp)
{
	for (;;) {
		struct bio *cbp = bioq_first(queue);
		if (cbp == NULL)
			break;
		bioq_remove(queue, cbp);

		if (cbp->bio_data != NULL &&
		    g_raid5_extra_mem(cbp->bio_parent,cbp->bio_data) &&
		    g_raid5_extra_mem(bp,cbp->bio_data))
			g_raid5_free_bio(sc,cbp);
		else
			g_destroy_bio(cbp);
	}
	bp->bio_completed = 0;
	bp->bio_children = 0;
	bp->bio_inbed = 0;
}

/*
 * order preparation of a 1-phase-write-request... 
 * both modes: DEGRADED/COMPLETE
 */
static int
g_raid5_order_full(struct g_raid5_softc *sc, struct bio_queue_head *queue,
                   struct bio *bp, char *data)
{
	off_t sno = bp->bio_offset >> sc->stripebits;
	off_t loff = (sno / (sc->sc_ndisks-1)) << sc->stripebits;
	int pno = (sno / (sc->sc_ndisks-1)) % sc->sc_ndisks;
	int failed = 0;
	struct bio *pari;
	if (g_raid5_disk_good(sc,pno)) {
		pari = g_raid5_prep(sc,queue,bp,BIO_WRITE,
		                    loff,sc->stripesize,0,sc->sc_disks[pno]);
		if (pari == NULL)
			return ENOMEM;
		pari->bio_caller1 = pari;
		bzero(pari->bio_data, pari->bio_length);
	} else {
		pari = NULL;
		failed++;
	}
	for (int i=0; i<sc->sc_ndisks; i++) {
		if (i == pno)
			continue;
		if (pari != NULL)
			g_raid5_xor(pari->bio_data, data, pari->bio_length);
		if (!g_raid5_disk_good(sc,i)) {
			failed++;
			if (failed > 1)
				return EIO;
			data += sc->stripesize;
			continue;
		}
		struct bio *cbp = g_raid5_prep(sc,queue,bp,BIO_WRITE,
		                               loff,sc->stripesize,data,sc->sc_disks[i]);
		if (cbp == NULL)
			return ENOMEM;
		cbp->bio_caller1 = bp;
		data += sc->stripesize;
	}
	if (failed > 0) {
		if (sc->verified < 0 || sc->verified > loff)
			sc->verified = loff;
	}
	g_raid5_w1rc++;
	return 0;
}

/*
 * order preparation of a 2-phase-write-request or a read-request... 
 * both modes: DEGRADED/COMPLETE
 */
static int
g_raid5_order(struct g_raid5_softc *sc, struct bio_queue_head *queue,
              struct bio *bp, off_t offset, int soff, int len, char *data)
{
	off_t sno = offset >> sc->stripebits;
	off_t loff = ( (sno/(sc->sc_ndisks-1)) << sc->stripebits ) + soff;
	off_t lend = loff + len;
	if (lend > sc->disksize)
		return EIO;

	int dead = 0;
	int dno = sno % (sc->sc_ndisks-1);
	int pno = (sno / (sc->sc_ndisks-1)) % sc->sc_ndisks;
	if (dno >= pno)
		dno++;
	MYKASSERT(dno < sc->sc_ndisks, ("dno: out of range (%d,%d)",pno,dno));


	int datdead = g_raid5_data_good(sc, dno, lend) ? 0 : 1;
	int pardead = g_raid5_parity_good(sc, pno, lend) ? 0 : 1;
	dead = pardead + datdead;
	if (dead != 0) {
		if (dead == 2 || (sc->state&G_RAID5_STATE_SAFEOP))
			return EIO; /* RAID is too badly degraded */
	}

	if (bp->bio_cmd == BIO_READ) {
		if (sc->state&G_RAID5_STATE_SAFEOP)
			return g_raid5_conv_veri_read(queue,sc,bp,loff,len,dno,pno,data);
		if (datdead) {
			struct bio*root= g_raid5_prep(sc,queue,bp,BIO_READ,-loff-1,len,data,0);
			if (root == NULL)
				return ENOMEM;
			for (int i=0; i < sc->sc_ndisks; i++) {
				if (i == dno)
					continue;
				if (!g_raid5_disk_good(sc,i))
					return EIO;
				if (g_raid5_prep(sc,queue,root,BIO_READ,
				                 loff,len,0,sc->sc_disks[i]) == NULL)
					return ENOMEM;
			}
			bzero(data,len);
			return 0;
		}
		if (g_raid5_prep(sc,queue,bp,BIO_READ,
		                 loff,len,data,sc->sc_disks[dno]) == NULL)
			return ENOMEM;
	} else {
		MYKASSERT(bp->bio_cmd == BIO_WRITE, ("bio_cmd WRITE expected"));

		if (dead != 0) {
			if (sc->state&G_RAID5_STATE_COWOP)
				return EPERM;
			if (sc->verified < 0 || sc->verified > loff) {
				sc->verified = loff - soff;
				g_raid5_wakeup(sc);
			}
		}

		if (pardead) { /* parity disk dead-dead */
			struct bio *cbp = g_raid5_prep(sc,queue,bp,BIO_WRITE,
			                               loff,len,data,sc->sc_disks[dno]);
			if (cbp == NULL)
				return ENOMEM;
			cbp->bio_caller1 = bp;
			g_raid5_w1rc++;
			return 0;
		}

		if (sc->sc_ndisks == 2) {
			struct bio *root = g_raid5_prep(sc,queue,bp,BIO_WRITE,
			                                loff,len,data,sc->sc_disks[pno]);
			if (root == NULL)
				return ENOMEM;
			root->bio_caller1 = bp;
			if (!datdead &&
			    g_raid5_prep(sc,queue,bp,BIO_WRITE,
			                 loff,len,data,sc->sc_disks[dno]) == NULL)
				return ENOMEM;
			g_raid5_w1rc++;
			return 0;
		}

		if (!datdead) {
			struct bio * root;
			struct bio * pari;

			pari = g_raid5_prep(sc,queue,bp,BIO_WRITE,
			                    -loff-1,len,0,sc->sc_disks[pno]);
			if (pari == NULL)
				return ENOMEM;
			root = g_raid5_prep(sc,queue,bp,BIO_WRITE,
			                    -loff-1,len,data,sc->sc_disks[dno]);
			if (root == NULL)
				return ENOMEM;
/*ARNE: pass at least data block read request through the cache */
			if (sc->sc_ndisks == 3 &&
			    g_raid5_data_good(sc, 3-dno-pno, lend)) {
				if (g_raid5_prep(sc,queue,root,BIO_READ, 
				                 loff,len,0,sc->sc_disks[3-dno-pno]) == NULL)
					return ENOMEM;
			} else {
				if (g_raid5_prep(sc,queue,root,BIO_READ,
				                 loff,len,0,sc->sc_disks[dno]) == NULL)
					return ENOMEM;
				if (g_raid5_prep(sc,queue,root,BIO_READ,
				                 loff,len,0,sc->sc_disks[pno]) == NULL)
					return ENOMEM;
			}
			bcopy(data, pari->bio_data, len);
			root->bio_caller1 = bp;
			g_raid5_w2rc++;
			return 0;
		}

		MYKASSERT(datdead, ("no valid data disk permissible here"));
		MYKASSERT(!pardead, ("need parity disk."));

		/* read all but dno and pno*/
		int i;
		struct bio *root = g_raid5_prep(sc,queue,bp,BIO_WRITE,
		                                -loff-1,len,0,sc->sc_disks[pno]);
		if (root == NULL)
			return ENOMEM;
		for (i=0; i < sc->sc_ndisks; i++) {
			if (i == dno || i == pno)
				continue;
			if (!g_raid5_data_good(sc, i, lend))
				return EIO;
			if (g_raid5_prep(sc,queue,root,BIO_READ,
			                 loff,len,0,sc->sc_disks[i]) == NULL)
				return ENOMEM;
		}
		bcopy(data, root->bio_data, len);
		root->bio_caller1 = bp;
		g_raid5_w2rc++;
		return 0;
	}
	return 0;
}

/*
 * seperate a bio from the cache into chunks that fit in a single
 * full stripe
 * (I think, this is unnecessary, since each cache entry is shaped, so
 *  that it already fits in a single full-stripe)
 */
static void
g_raid5_dispatch(struct g_raid5_softc *sc, struct bio *bp)
{
	int error = sc->sc_provider->error;
	off_t offset = bp->bio_offset;
	off_t length = bp->bio_length;
	u_char *data = bp->bio_data;

	MYKASSERT(bp->bio_children==0, ("no children."));
	MYKASSERT(bp->bio_inbed==0, ("no inbed."));

	g_topology_assert_not();
	g_raid5_open(sc);

	struct bio_queue_head queue;
	bioq_init(&queue);

	while (length > 0 && !error) {
		int len;
		if ( bp->bio_cmd == BIO_WRITE && bp->bio_length == sc->fsl &&
		     g_raid5_nvalid(sc) >= sc->sc_ndisks - 1 ) {
			error = g_raid5_order_full(sc,&queue,bp,data);
			len = sc->fsl;
		} else {
			int soff = offset & (sc->stripesize-1);
			len = MIN(length, sc->stripesize - soff);
			error = g_raid5_order(sc, &queue, bp, offset, soff, len, data);
		}
		length -= len;
		offset += len;
		data += len;
	}

	if (error) {
		g_raid5_des_sq(sc,&queue,bp);
		if (!bp->bio_error)
			bp->bio_error = error;
		if (bp->bio_cmd == BIO_READ && bp->bio_caller2 != NULL) {
			struct bio *obp = bp->bio_caller2;
			MYKASSERT(!g_raid5_extra_mem(obp,bp->bio_caller1), ("bad structure."));
			bp->bio_caller1 = NULL;
			obp->bio_inbed++;
			if (obp->bio_inbed == obp->bio_children)
				g_io_deliver(obp,obp->bio_error);
		}
		g_raid5_set_bad(bp); /* retry or uncache */
		g_raid5_wakeup(sc);
		g_raid5_unopen(sc);
	} else {
		MYKASSERT(length == 0,
		          ("%s: length is still greater than 0.", sc->sc_name));
		g_raid5_req_sq(sc, &queue);
	}
}

/*
 * order the preparation of consumer-requests for bio bp,
 *   which is in the internal cache...
 */
static void
g_raid5_issue(struct g_raid5_softc *sc, struct bio *bp)
{
	bp->bio_done = NULL;
	bp->bio_children = 0;
	bp->bio_inbed = 0;
	MYKASSERT(bp->bio_caller1 == NULL, ("still dependent."));
	MYKASSERT(bp->bio_caller2 == NULL, ("still error."));
	bp->bio_driver1 = NULL;
	g_raid5_dispatch(sc, bp);
}

/*
 * check if bio bp (which is in the internal cache)
 * must be delayed due to already started bio-s
 * or must be re-shaped, because it would write to 2 blocks in the same
 *   full-stripe
 */
static int
g_raid5_issue_check(struct g_raid5_softc *sc,
                    struct g_raid5_cache_entry *ce, struct bio *bp)
{
	/* already issued? */
	if (!g_raid5_is_pending(bp))
		return 0;
	MYKASSERT(bp->bio_cmd == BIO_WRITE, ("wrong cmd."));

if (sc->term) G_RAID5_DEBUG(0,"line%d",__LINE__);
	/* blocked by already issued? */
	if (bp->bio_caller1 != NULL) {
if (sc->term) G_RAID5_DEBUG(0,"line%d",__LINE__);
		if (g_raid5_still_blocked(sc, ce, bp))
			return 0;
		bp->bio_caller1 = NULL;
	}

if (sc->term) G_RAID5_DEBUG(0,"line%d",__LINE__);
	if (bp->bio_length < sc->fsl && bp->bio_length > sc->stripesize) {
if (sc->term) G_RAID5_DEBUG(0,"line%d",__LINE__);
		int len = sc->stripesize - (bp->bio_offset & (sc->stripesize - 1));

		/* inject tail request */
		struct bio *cbp = g_alloc_bio();
		cbp->bio_cmd = BIO_WRITE;
		cbp->bio_offset = bp->bio_offset + len;
		cbp->bio_length = bp->bio_length - len;
		cbp->bio_t0.sec = bp->bio_t0.sec;
		g_raid5_q_in(sc, ce, cbp, 1);
		g_raid5_dc_inc(sc,ce);

		/* shape original request */
		bp->bio_length = len;
	}

if (sc->term) G_RAID5_DEBUG(0,"line%d",__LINE__);
	/* possible 2-phase stripe conflict? */
	if (g_raid5_stripe_conflict(sc, ce, bp)) {
if (sc->term) G_RAID5_DEBUG(0,"line%d",__LINE__);
		if (bp->bio_driver2 == NULL) {
			g_raid5_blked2++;
			bp->bio_driver2 = bp;
		}
		return 0;
	} else if (bp->bio_driver2 != NULL)
		bp->bio_driver2 = NULL;

if (sc->term) G_RAID5_DEBUG(0,"line%d",__LINE__);
	g_raid5_set_issued(sc,bp);

if (sc->term) G_RAID5_DEBUG(0,"line%d",__LINE__);
	g_raid5_issue(sc,bp);

	sc->wqpi++;
if (sc->term) G_RAID5_DEBUG(0,"line%d",__LINE__);
	return 1;
}

/*
 * check if a certain cache entry contains bio-s that overlap()...
 */
static int
g_raid5_chk_frag(struct g_raid5_softc *sc,
                 struct g_raid5_cache_entry *ce, struct bio *combo)
{
	int found = 0;
	int chged = 0;
	off_t cend = combo->bio_offset + combo->bio_length;
	struct bio *bp, *bp_nxt;
	G_RAID5_CE_TRAVSAFE(ce, bp) {
		int f = found;
		if (f)
			found = 0;
		if (combo == bp) {
			found = 1;
			continue;
		}
		if (!g_raid5_is_cached(sc,bp))
			continue;
		off_t end = bp->bio_offset + bp->bio_length;
		if (end < combo->bio_offset || cend < bp->bio_offset)
			continue;
		if (f)
			chged = 1;
		g_raid5_combine_inner(sc,ce,bp,combo,end);
	}
	return chged;
}

/*
 * insert new bio wbp into the internal cache...
 */
static int
g_raid5_write(struct g_raid5_softc *sc, struct bio *wbp, int o, int l)
{
	MYKASSERT(wbp != NULL, ("no way!"));
	MYKASSERT(wbp->bio_cmd == BIO_WRITE, ("no way!"));
	MYKASSERT(!g_raid5_is_issued(sc,wbp), ("no way!"));

	struct bio *combo = NULL;
	struct bio *issued = NULL;
	off_t woff = wbp->bio_offset + o;
	off_t wend = woff + l;

	struct g_raid5_cache_entry *ce = g_raid5_ce_by_off(sc, woff);
	if (ce == NULL)
		return EAGAIN;
	ce->lu = wbp->bio_t0;

	struct bio *bp, *bp_nxt;
	G_RAID5_CE_TRAVSAFE(ce, bp) {
		if (g_raid5_is_requested(sc,bp))
			return EAGAIN;

		off_t end = bp->bio_offset + bp->bio_length;
		int overlapf;
		int overlap = g_raid5_overlap(bp->bio_offset,end, woff,wend, &overlapf);
		if (!overlap)
			continue;

		/* if bp has been already started,
		     then no combo but look for another bp that matches and mark wbp */
		if (g_raid5_is_issued(sc,bp)) {
			if (overlapf)
				issued = bp;
			continue;
		}

		MYKASSERT(!g_raid5_is_bad(sc,bp), ("bp must not be in state error."));
		if (g_raid5_is_cached(sc,bp)) {
			if (overlapf) {
				if (bp->bio_offset < woff) {
					bp->bio_length = woff - bp->bio_offset;
					if (end > wend) {
						struct bio *cbp = g_alloc_bio();
						cbp->bio_offset = wend;
						cbp->bio_length = end - wend;
						cbp->bio_t0.sec = bp->bio_t0.sec;
						g_raid5_q_in(sc, ce, cbp, 1);
						g_raid5_set_cached(sc, cbp);
					}
				} else if (end <= wend)
					g_raid5_q_de(sc,ce,bp,1);
				else {
					int d = wend - bp->bio_offset;
					bp->bio_offset = wend;
					bp->bio_length = end - wend;
					bp->bio_data += d;
				}
			}
			continue;
		}

		if (combo == NULL) {
			g_raid5_combine(sc,ce, bp, wbp,woff,l, MAX(end, wend));
			combo = bp;
		} else { /* combine bp with combo and remove bp */
			MYKASSERT(g_raid5_is_pending(combo), ("oops something started?"));
			g_raid5_combine_inner(sc,ce,bp,combo,end);
			g_raid5_dc_dec(sc,ce);
		}
	}

	if (combo == NULL) {
		struct bio *cbp = g_alloc_bio();
		cbp->bio_cmd = BIO_WRITE;
		cbp->bio_offset = woff;
		cbp->bio_length = l;
		cbp->bio_t0.sec = wbp->bio_t0.sec;
		g_raid5_q_in(sc, ce, cbp, 1);
		g_raid5_dc_inc(sc,ce);
		g_raid5_set_pending(cbp);
		if (issued) {
			cbp->bio_caller1 = issued;
			g_raid5_blked1++;
		}
	}
	bcopy(wbp->bio_data + o, ce->sd + woff % sc->fsl, l);

	if (combo != NULL && combo->bio_length == sc->fsl)
		g_raid5_issue_check(sc,ce, combo);

	if (sc->wqp > g_raid5_wqp)
		g_raid5_wqp = sc->wqp;

	return 0; /* ARNE: no error can be handled softly -- currently */
}

/*
 * one write request is no longer in state issued (very soon) and not dirty...
 */
static __inline void
g_raid5_wqp_dec(struct g_raid5_softc *sc, struct g_raid5_cache_entry *ce,
                struct bio *bp)
{
	if (g_raid5_is_issued(sc,bp)) {
		g_raid5_dc_dec(sc,ce);
		sc->wqpi--;
	}
}

/*
 * remove bio bp from cache
 * if cache entry is empty now, then update internal counter and wake up
 *   worker() thread (in case it had to delay requests because of ENOMEM)...
 */
static __inline void
g_raid5_uncache(struct g_raid5_softc *sc, struct g_raid5_cache_entry *ce,
                struct bio *bp)
{
	g_raid5_q_de(sc,ce,bp,0);
	if (g_raid5_ce_em(ce)) {
		MYKASSERT(ce->sno == 0, ("sno must be zero here."));
		if (sc->cfc > 0)
			sc->cfc--;
		g_raid5_wakeup(sc);
	}
}

/*
 * check for bio-s in the internal cache that r marked "done" or "bad"...
 * ... or that r too old (uncache or issue)...
 */
static void
g_raid5_undirty(struct g_raid5_softc *sc, struct bintime *now, int rcf)
{
	int wdt = MAX(g_raid5_wdt,2);
	if (wdt > g_raid5_wdt)
		g_raid5_wdt = wdt;
	int edc = 0;
	int pc = 0;
	struct g_raid5_cache_entry *mce = NULL;
	struct bio *bp, *bp_nxt;
	struct g_raid5_cache_entry *ce;
	G_RAID5_C_TRAVSAFE(sc,bp,ce) {
		if (bp_nxt==NULL && ce->dc) {
			if (pc == 0)
				edc++;
			else {
				pc = 0;
				if (mce == NULL || g_raid5_bintime_cmp(&ce->lu,&mce->lu) < 0)
					mce = ce;
			}
		}
		if (g_raid5_is_bad(sc,bp)) {
			MYKASSERT(bp->bio_caller1 == NULL, ("no dependancy possible here."));
			bp->bio_caller2 = NULL;
			if (bp->bio_error == ENOMEM) {
				if (bp->bio_cmd == BIO_WRITE) {
					bp->bio_error = 0;
					g_raid5_set_pending(bp);
					sc->wqpi--;
					g_raid5_issue_check(sc,ce,bp);
					G_RAID5_DEBUG(rcf?0:100,"stale write request in rcf mode");
				} else
					g_raid5_uncache(sc,ce,bp);
			} else {
				G_RAID5_DEBUG(0,"%s: %p: aborted.",sc->sc_name,bp);
				g_error_provider(sc->sc_provider, bp->bio_error);
				g_raid5_wqp_dec(sc,ce,bp);
				g_raid5_uncache(sc,ce,bp);
			}
		} else if (g_raid5_is_done(sc,bp) || g_raid5_is_cached(sc,bp)) {
			if (g_raid5_is_done(sc,bp)) {
				g_raid5_wqp_dec(sc,ce,bp);
				g_raid5_set_cached(sc,bp);
				if (g_raid5_chk_frag(sc,ce,bp))
					bp_nxt = g_raid5_q_nx(bp);
			}
			if ( rcf || sc->term || (!ce->dc && sc->cfc) ||
			     sc->cc * 71 > sc->cs * 64 ||          /* >90% */
			     (now != NULL &&                  /* >80% --> >1sec / >wdt */
			      now->sec - bp->bio_t0.sec > ((sc->cc*5 > sc->cs*4)?1:wdt)) )
				g_raid5_uncache(sc,ce,bp);
		} else if (g_raid5_is_pending(bp)) {
			if (rcf)
				G_RAID5_DEBUG(0,"pending write request in rcf mode (wqp%d wqpi%d)",
				              sc->wqp,sc->wqpi);
			if (sc->term || bp->bio_length == sc->fsl ||
			    now == NULL || now->sec - bp->bio_t0.sec > wdt) {
				if (!g_raid5_issue_check(sc,ce,bp))
					pc++;
			} else
				pc++;
		} else
			MYKASSERT(!rcf,("stale read request in rcf mode"));
	}
	if (mce != NULL) {
		int cqf = (sc->dc - edc)*5 > sc->cs * 4; /* >80% */
		int tio = ((sc->dc - edc)*2 > sc->cs) ? 1 : wdt; /* >50% --> >1 / >wdt*/
		G_RAID5_CE_TRAVERSE(mce,bp) {
			if (!g_raid5_is_pending(bp))
				continue;
			if (now == NULL || sc->term || bp->bio_length == sc->fsl)
				continue;
			if (cqf || sc->wqp - sc->wqpi > g_raid5_maxwql / 4 ||
			    now->sec - bp->bio_t0.sec > tio)
				g_raid5_issue_check(sc,mce,bp);
		}
	}
}

/*
 * update access counters and call ...destroy() if requested...
 */
static int
g_raid5_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_consumer *cp1, *cp2;
	struct g_raid5_softc *sc;
	struct g_geom *gp;
	int error;

	gp = pp->geom;
	sc = gp->softc;

	g_topology_assert();

	if (sc == NULL) {
		/*
		 * It looks like geom is being withered.
		 * In that case we allow only negative requests.
		 */
		MYKASSERT(dr <= 0 && dw <= 0 && de <= 0,
		    ("Positive access request (device=%s).", pp->name));
		if ((pp->acr + dr) == 0 && (pp->acw + dw) == 0 &&
		    (pp->ace + de) == 0) {
			G_RAID5_DEBUG(0, "%s: device definitely destroyed.", gp->name);
		}
		return 0;
	}

	error = ENXIO;
	LIST_FOREACH(cp1, &gp->consumer, consumer) {
		if (g_raid5_find_disk(sc, cp1) >= 0) {
			error = g_access(cp1, dr, dw, de);
			if (error)
				break;
		}
	}
	if (error) {
		/* if we fail here, backout all previous changes. */
		LIST_FOREACH(cp2, &gp->consumer, consumer) {
			if (cp1 == cp2)
				break;
			if (g_raid5_find_disk(sc, cp2) >= 0)
				g_access(cp2, -dr, -dw, -de);
		}
	} else {
		LIST_FOREACH(cp1, &gp->consumer, consumer) {
			if (g_raid5_find_disk(sc, cp1) < 0)
				continue;
			if (cp1->acr < 0) g_access(cp1, -cp1->acr,0,0);
			if (cp1->acw < 0) g_access(cp1, 0,-cp1->acw,0);
			if (cp1->ace < 0) g_access(cp1, 0,0,-cp1->ace);
		}
		if (sc->destroy_on_za &&
		    pp->acr+dr == 0 && pp->acw+dw == 0 && pp->ace+de == 0) {
			sc->destroy_on_za = 0;
			g_raid5_destroy(sc,1,0);
		}
	}

	return error;
}

/*
 * issue all pending write requests and dont return before they r all done...
 */
static void
g_raid5_flush_wq(struct g_raid5_softc *sc)
{
	while (sc->wqp > 0) {
		g_raid5_undirty(sc, NULL,0);
		tsleep(&sc->wqp, PRIBIO, "gr5fl", hz);
	}
	g_raid5_undirty(sc, NULL,0);
}

/*
 * flush pending write requests to the consumers
 * issue a BIO_FLUSH for all consumers
 */
static void
g_raid5_flush(struct g_raid5_softc *sc, struct bio* bp)
{
G_RAID5_DEBUG(0,"g_raid5_flush() begin %d.", __LINE__);
	g_raid5_flush_wq(sc);

	g_raid5_open(sc);

G_RAID5_DEBUG(0,"g_raid5_flush() middle %d.", __LINE__);
	struct bio *cbps[sc->sc_ndisks];
	int i;
	for (i=0; i<sc->sc_ndisks; i++) {
		if (sc->sc_disks[i] == NULL)
			continue;
		cbps[i] = g_clone_bio(bp);
		if (cbps[i] == NULL)
			break;
		cbps[i]->bio_driver2 = sc->sc_disks[i];
		cbps[i]->bio_offset = sc->sc_disks[i]->provider->mediasize;
		cbps[i]->bio_length = 0;
		cbps[i]->bio_done = NULL;
		cbps[i]->bio_attribute = NULL;
		cbps[i]->bio_data = NULL;
		g_raid5_io_req(sc,cbps[i]);
	}
	int err = i < sc->sc_ndisks ? ENOMEM : 0;
	for (int j=0; j<i; j++) {
		if (sc->sc_disks[j] == NULL)
			continue;
		int e = biowait(cbps[j], "gr5bw");
		if (e && !err)
			err = e;
		g_destroy_bio(cbps[j]);
	}
	g_io_deliver(bp, err);
	g_raid5_unopen(sc);
G_RAID5_DEBUG(0,"g_raid5_flush() end %d.", __LINE__);
}

#if __FreeBSD_version < 700000
#ifndef BIO_FLUSH
#define BIO_FLUSH 42
#endif
#endif

/*
 * called by GEOM with a new request...
 */
static void
g_raid5_start(struct bio *bp)
{
	struct g_provider *pp = bp->bio_to;
	struct g_raid5_softc *sc = pp->geom->softc;
	MYKASSERT(sc != NULL, ("%s: provider's error should be set (error %d).",
	                       pp->name, pp->error));
	MYKASSERT(!g_raid5_is_issued(sc,bp), ("no cycles!"));

	g_topology_assert_not();

	G_RAID5_LOGREQ(bp, "[start]");

	if (sc->sc_provider->error) {
		g_io_deliver(bp, ENXIO);
		return;
	}

	switch (bp->bio_cmd) {
	case BIO_WRITE:
		g_raid5_wrc++;
		break;
	case BIO_READ:
		g_raid5_rrc++;
		break;
	case BIO_FLUSH:
		break;
	case BIO_DELETE:
		/* delete? what? */
	case BIO_GETATTR:
		/* To which provider it should be delivered? */
	default:
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	}

	if (bp->bio_length < 0 ||
	    bp->bio_offset < 0 || bp->bio_offset > sc->sc_provider->mediasize) {
		g_io_deliver(bp,EINVAL);
		return;
	}
	if (bp->bio_length + bp->bio_offset > sc->sc_provider->mediasize)
		bp->bio_length = sc->sc_provider->mediasize - bp->bio_offset;
	if (bp->bio_length == 0) {
		g_io_deliver(bp,0);
		return;
	}

	MYKASSERT(bp->bio_children == 0, ("wrong structure"));

	mtx_lock(&sc->sq_mtx);
	if (sc->worker == NULL)
		g_io_deliver(bp, ENXIO);
	else
		bioq_insert_tail(&sc->sq, bp);
	mtx_unlock(&sc->sq_mtx);
	g_raid5_wakeup(sc);
}

/*
 * called by worker() thread in order to update the device state and other
 * meta data...
 */
static void
g_raid5_update_metadatas(struct g_raid5_softc *sc)
{
	int state = sc->state & G_RAID5_STATE_HOT;

	g_topology_assert_not();

	struct bio urs[sc->sc_ndisks];
	for (int no = 0; no < sc->sc_ndisks; no++)
		if (g_raid5_disk_good(sc, no))
			g_raid5_update_metadata(sc, &sc->sc_disks[no], state, no, urs+no);
		else
			urs[no].bio_cmd = BIO_READ;
	for (int no = 0; no < sc->sc_ndisks; no++) {
		if (urs[no].bio_cmd == BIO_READ)
			continue;
		int error = biowait(urs+no, "gr5wr");
		free(urs[no].bio_data, M_RAID5);
		if (error)
			G_RAID5_DEBUG(0,"%s: %s(%d): meta data update failed: error:%d.",
			              sc->sc_name,sc->sc_disks[no]->provider->name,no,error);
	}
}

#if __FreeBSD_version < 602000
static __inline struct bio *
g_duplicate_bio(struct bio *pbp) 
{
	for (;;) {
		struct bio *cbp = g_clone_bio(pbp);
		if (cbp != NULL)
			return cbp;
		tsleep(&pbp, PRIBIO, "gr5db", hz/128);
	}
}
#endif

/*
 * insert a new read request into the internal cache...
 */
static int
g_raid5_read(struct g_raid5_softc *sc, struct bio *rbp, int o, int l)
{
	MYKASSERT(rbp != NULL, ("no way!"));
	MYKASSERT(rbp->bio_cmd == BIO_READ, ("no way!"));

	off_t roff = rbp->bio_offset + o;
	off_t rend = roff + l;
	struct g_raid5_cache_entry *ce = g_raid5_ce_by_off(sc, roff);
	if (ce == NULL)
		return EAGAIN;
	ce->lu = rbp->bio_t0;

	struct bio_queue_head lq;
	bioq_init(&lq);

	struct bio *pbp = g_duplicate_bio(rbp);
	pbp->bio_offset = roff;
	pbp->bio_length = l;
	pbp->bio_caller1 = rbp->bio_data + o;
	pbp->bio_caller2 = ce;
	bioq_insert_tail(&lq, pbp);

	int err = rbp->bio_parent == (void*)sc ? EFAULT : 0;
	int fol = 0;
	struct bio *bp;
	G_RAID5_CE_TRAVERSE(ce, bp) {
		off_t off = bp->bio_offset;
		off_t end = off + bp->bio_length;
		if (!g_raid5_overlapf(off,end, roff,rend)) {
			if (!fol && g_raid5_flank(off,end, roff,rend))
				fol = 1;
			continue;
		}
		if (!fol)
			fol = 1;
		if (!err && !g_raid5_is_current(sc,bp))
			err = EAGAIN;
		bp->bio_t0.sec = rbp->bio_t0.sec;
		struct bio *lbp,*lbp_nxt;
		TAILQ_FOREACH_SAFE(lbp,&lq.queue,bio_queue,lbp_nxt) {
			off_t loff = lbp->bio_offset;
			off_t lend = loff + lbp->bio_length;
			if (!g_raid5_overlapf(off,end, loff,lend))
				continue;
			if (off <= loff) {
				int cl;
				char *src = bp->bio_data + (loff - off);
				if (end < lend) { /* just tail is left */
					int d = lend - end;
					cl = lbp->bio_length - d;
					lbp->bio_offset = end;
					lbp->bio_length = d;
					lbp->bio_caller1 = rbp->bio_data + (end - rbp->bio_offset);
				} else { /* vanished */
					cl = lbp->bio_length;
					bioq_remove(&lq, lbp);
					g_destroy_bio(lbp);
					rbp->bio_children--;
				}
				if (!err) {
					rbp->bio_completed += cl;
					char *tgt = rbp->bio_data + (loff - rbp->bio_offset);
					bcopy(src, tgt, cl);
				}
			} else {
				int cl;
				if (end < lend) { /* split in two */
					cl = bp->bio_length;
					struct bio *pbp = g_duplicate_bio(rbp);
					pbp->bio_offset = end;
					pbp->bio_length = lend - end;
					pbp->bio_caller1 = rbp->bio_data + (end - rbp->bio_offset);
					pbp->bio_caller2 = ce;
					bioq_insert_tail(&lq, pbp);
				} else
					cl = lend - off;
				if (!err) {
					rbp->bio_completed += cl;
					char *tgt = rbp->bio_data + (off - rbp->bio_offset);
					bcopy(bp->bio_data, tgt, cl);
				}
				lbp->bio_length = off - loff; /* head */
			}
		}
	}

	struct bio_queue_head *uq = rbp->bio_driver1;
	TAILQ_CONCAT(&uq->queue, &lq.queue, bio_queue);

	if (err) {
		rbp->bio_completed = 0;
		rbp->bio_children = 0;
		if (err == EFAULT)
			err = 0;
	} else if (fol && rbp->bio_driver2 == NULL)
		rbp->bio_driver2 = rbp;
	return err;
}

/*
 * split a new request so that the parts fit into a single full-stripe
 * and then call ..._read() or ..._write()...
 */
static int
g_raid5_split(struct g_raid5_softc *sc, struct bio *sbp,
              int (*g_raid5_mode)(struct g_raid5_softc *sc,
                                  struct bio *sbp, int o, int l))
{
	for (int o=0; o<sbp->bio_length;) {
		int l = sbp->bio_length - o;
		l = MIN(l, sc->fsl - ((sbp->bio_offset+o) % sc->fsl) );
		int err = g_raid5_mode(sc, sbp,o,l);
		if (err)
			return err;
		o += l;
	}
	return 0;
}

/*
 * take a bio from graid5's internal start queue, preprocess it and
 * call apropriate functions for further processing
 * (final processing is not necessarily ordered)
 */
static int
g_raid5_flush_sq(struct g_raid5_softc *sc, struct bintime *now)
{
	int twarr = 0; /* there was a read request */
	struct bio_queue_head pushback;
	bioq_init(&pushback);
	int no_write = 0;
	for (;;) {
		mtx_lock(&sc->sq_mtx);
		struct bio *sbp = bioq_first(&sc->sq);
		if (sbp == NULL) {
			TAILQ_CONCAT(&sc->sq.queue, &pushback.queue, bio_queue);
			mtx_unlock(&sc->sq_mtx);
			break;
		}
		bioq_remove(&sc->sq, sbp);
		mtx_unlock(&sc->sq_mtx);

		int puba = 0;
		if (sbp->bio_cmd == BIO_WRITE) {
			puba = no_write || sc->wqp > g_raid5_maxwql;
			if (!puba) {
				if (g_raid5_split(sc, sbp, g_raid5_write) == EAGAIN)
					puba = 1;
				else
					bioq_insert_tail(&sc->wdq, sbp);
			}
			if (puba) {
				if (!no_write)
					no_write = 1;
				bioq_insert_tail(&pushback, sbp);
			}
		} else if (sbp->bio_cmd == BIO_FLUSH) {
			if (no_write)
				bioq_insert_tail(&pushback, sbp);
			else
				g_raid5_flush(sc, sbp);
		} else {
			MYKASSERT(sbp->bio_cmd == BIO_READ, ("read expected."));
			MYKASSERT(sbp->bio_children==0,
			          ("no children (cc%d err%d).",
			           sbp->bio_children,sbp->bio_error));
			if (!twarr)
				twarr = 1;
			struct bio_queue_head queue;
			bioq_init(&queue);
			sbp->bio_driver1 = &queue;
			sbp->bio_driver2 = NULL;
			int result = puba ? EAGAIN : g_raid5_split(sc, sbp, g_raid5_read);
			int failed = result == EAGAIN;
			sbp->bio_driver1 = NULL;
			if (sbp->bio_parent == (void*) sc) {
				failed = 1;
				g_destroy_bio(sbp);
			} else if (failed) {
				MYKASSERT(sbp->bio_children==0, ("wrong structure."));
				bioq_insert_tail(&pushback, sbp);
				if (!no_write)
					no_write = 1;
			}
			if (sbp->bio_driver2) {
				MYKASSERT(sbp->bio_driver2==sbp,("bad ptr"));
				sbp->bio_driver2 = NULL;
				if (!failed && 0) { /*ARNE*/ /*read ahead is disabled*/
				struct bio *rah = g_new_bio();
				if (rah != NULL) {
				rah->bio_parent = (void*) sc;
				rah->bio_cmd = BIO_READ;
				rah->bio_offset = sbp->bio_offset + sbp->bio_length;
				rah->bio_length = 2*sc->fsl - rah->bio_offset % sc->stripesize;
				if (rah->bio_offset + rah->bio_length > sc->sc_provider->mediasize)
					g_destroy_bio(rah);
				else {
					rah->bio_t0.sec = sbp->bio_t0.sec;
					mtx_lock(&sc->sq_mtx);
					bioq_insert_tail(&sc->sq, rah);
					mtx_unlock(&sc->sq_mtx);
				}
				}}
			}
			int src;
			for (src=0; ; src++) {
				struct bio *pbp = bioq_first(&queue);
				if (pbp == NULL)
					break;
				bioq_remove(&queue, pbp);
				g_raid5_set_requested(sc,pbp);
				g_raid5_q_in(sc, pbp->bio_caller2, pbp, 1);
				pbp->bio_t0.sec = now->sec;
				if (failed) {
					pbp->bio_caller1 = NULL;
					pbp->bio_caller2 = NULL;
				} else
					pbp->bio_caller2 = sbp;
				g_raid5_dispatch(sc, pbp);
			}
			MYKASSERT(failed || sbp->bio_children==src,("wrong structure."));
			if (!failed && src == 0)
				g_io_deliver(sbp, 0); /* Yahoo! fully from the cache... */
		}
	}

	if (sc->wqp < g_raid5_maxwql) for (;;) {
		struct bio *wbp = bioq_first(&sc->wdq);
		if (wbp == NULL)
			break;
		bioq_remove(&sc->wdq,wbp);
		wbp->bio_completed = wbp->bio_length;
		g_io_deliver(wbp, sc->sc_provider->error);
	}

	g_raid5_undirty(sc, now,0);

	return twarr;
}

/*
 * Here graid5 looks at its internal done queue and tries to finalize
 * requests or -in case of an error- puts them back to the start queue
 * or gives them back to GEOM with ENOMEM or worse (e. g. EIO)...
 */
static void
g_raid5_workerD(void *arg)
{
	struct g_raid5_softc *sc = arg;

	g_topology_assert_not();

#if __FreeBSD_version >= 700000
	thread_lock(curthread);
	sched_prio(curthread, PRIBIO);
	thread_unlock(curthread);
#else
	mtx_lock_spin(&sched_lock);
	sched_prio(curthread, PRIBIO);
	mtx_unlock_spin(&sched_lock);
#endif

	mtx_lock(&sc->dq_mtx);
	while (sc->workerD != NULL) {
		struct bio *dbp = bioq_first(&sc->dq);
		if (dbp == NULL) {
			msleep(&sc->workerD, &sc->dq_mtx, PRIBIO, "gr5do", hz);
			continue;
		}
		bioq_remove(&sc->dq, dbp);

		mtx_unlock(&sc->dq_mtx);
		MYKASSERT(dbp->bio_from->geom->softc == sc, ("done bp's sc no good."));
		g_raid5_ready(sc, dbp);
		mtx_lock(&sc->dq_mtx);
	}
	mtx_unlock(&sc->dq_mtx);

	sc->term = 2;
	wakeup(&sc->term);

	curthread->td_pflags &= ~TDP_GEOM;
	kproc_exit(0);
}

static __inline void
g_raid5_set_stripesize(struct g_raid5_softc *sc, u_int s)
{
	MYKASSERT(powerof2(s), ("stripe size (%d) not power of 2.", s));
	sc->stripesize = s;
	sc->stripebits = bitcount32(s - 1);
	sc->fsl = (sc->sc_ndisks - 1) << sc->stripebits;
}

/*
 * here graid5 tries to figure out its provider size and sector size and
 * issues warnings, if something strange comes out...
 */
static void
g_raid5_check_and_run(struct g_raid5_softc *sc, int forced)
{
	if (!forced && !g_raid5_allgood(sc))
		return;
	if (sc->sc_provider == NULL) {
		sc->sc_provider = g_new_providerf(sc->sc_geom, "raid5/%s", sc->sc_name);
		sc->sc_provider->mediasize = -1ULL;
	}

	off_t min = -1;
	u_int sectorsize = 0;
	u_int no;
	for (no = 0; no < sc->sc_ndisks; no++) {
		struct g_consumer *disk = sc->sc_disks[no];
		if (disk == NULL)
			continue;
		if (sectorsize==0 || min>disk->provider->mediasize) {
			min = disk->provider->mediasize;
			if (sc->sc_type == G_RAID5_TYPE_AUTOMATIC)
				min -= disk->provider->sectorsize;
		}
		if (sectorsize == 0)
			sectorsize = disk->provider->sectorsize;
		else
			sectorsize = g_raid5_lcm(sectorsize,disk->provider->sectorsize);
	}
	min -= min & (sectorsize - 1);
	min -= min & (sc->stripesize - 1);
	sc->disksize = min;
	min *= sc->sc_ndisks - 1;
	if (sc->sc_provider->mediasize != -1ULL && sc->sc_provider->mediasize > min)
		G_RAID5_DEBUG(0,"%s: WARNING: reducing media size (from %jd to %jd).",
		              sc->sc_name, sc->sc_provider->mediasize, min);
	sc->sc_provider->mediasize = min;
	sc->sc_provider->sectorsize = sectorsize;
	if (sc->stripesize < sectorsize) {
		G_RAID5_DEBUG(0,"%s: WARNING: setting stripesize (%u) to sectorsize (%u)",
		              sc->sc_name, sc->stripesize, sectorsize);
		g_raid5_set_stripesize(sc, sectorsize);
	}
	if (sc->sc_provider->error != 0)
		g_error_provider(sc->sc_provider, 0);
}

static __inline int
g_raid5_compute_cs(struct g_raid5_softc *sc)
{
	if (g_raid5_cache_size <= 0) {
		int cs = g_raid5_cache_size_mem / ((sc->sc_ndisks-1) << sc->stripebits);
		if (cs < -g_raid5_cache_size)
			cs = -g_raid5_cache_size;
		if (cs < 5)
			cs = 5;
		return cs;
	}
	if (g_raid5_cache_size < 5)
		return 5;
	return g_raid5_cache_size;
}

static __inline void
g_raid5_cache_init(struct g_raid5_softc *sc)
{
	sc->cso = g_raid5_cache_size;
	sc->csmo = g_raid5_cache_size_mem;

	sc->cs = g_raid5_compute_cs(sc);
	sc->ce = malloc(sizeof(*sc->ce) * sc->cs, M_RAID5, M_WAITOK | M_ZERO);
	sc->cc = 0;
	sc->cfc = 0;

	sc->mhs = sc->cs / 4;
	sc->mhl = malloc(sizeof(*sc->mhl) * sc->mhs, M_RAID5, M_WAITOK);
	sc->mhc = 0;
	mtx_init(&sc->mh_mtx, "graid5:mh", NULL, MTX_DEF);
}

/*
 * here graid5 processes commands that come from "graid5 configure"
 * via g_raid5_ctl_configure().
 * this is called from the worker() thread
 */
static void
g_raid5_worker_conf(struct g_raid5_softc *sc, char **cause)
{
	if (sc->conf_order == 1) {
		sc->verified = 0;
		(*cause) = "verify forced";
	} else if (sc->conf_order == 2) {
		sc->verified = sc->disksize;
		(*cause) = "verify aborted";
	} else if (sc->cso != g_raid5_cache_size ||
	           sc->csmo != g_raid5_cache_size_mem) {
		g_raid5_flush_wq(sc);
		g_raid5_unopen(sc);
		g_raid5_no_more_open(sc);
		g_raid5_more_open(sc);
		g_raid5_open(sc);
		g_raid5_undirty(sc, NULL,1);
		MYKASSERT(sc->dc == 0, ("cache artifact (0.0)."));
		MYKASSERT(sc->wqp == 0, ("cache artifact (0.1)."));
		MYKASSERT(sc->wqpi == 0, ("cache artifact (0.2)."));
		for (int i = sc->cs-1; i >= 0; i--) {
			MYKASSERT(g_raid5_ce_by_i(sc,i)->fst == NULL, ("cache artifact (1)."));
			MYKASSERT(g_raid5_ce_by_i(sc,i)->sd == NULL, ("cache artifact (2)."));
			MYKASSERT(g_raid5_ce_by_i(sc,i)->sp == NULL, ("cache artifact (3)."));
			MYKASSERT(g_raid5_ce_by_i(sc,i)->dc == 0, ("cache artifact (4)."));
		}
		free(sc->ce, M_RAID5);
		g_raid5_mh_destroy(sc);
		g_raid5_cache_init(sc);
		(*cause) = "new cache size";
	} else
		(*cause) = "new configuration";
	sc->conf_order = 0;
}

/*
 * this function implements the worker thread "main"
 * it flushes the internal start queue,
 * updates the device state
 *   (COMPLETE, HOT (write requests pending), CALM, DEGRADED, REBUILD)
 * finalizes the config requests,
 * cares for rebuild requests
 *   (they r injected nearly as if they were normal requests),
 * and
 * handles destroy requests
 *   (tells the workerD() thread to terminate, flushes queues&caches,
 *    free-s data structures)
 */
static void
g_raid5_worker(void *arg)
{
	struct g_raid5_softc *sc = arg;

	int wt = 0;
	off_t curveri = -1;
	off_t last_upd = -1;
	int last_nv = -1;
	struct bintime now = {0,0};
	struct bintime lst_vbp = {0,0};
	struct bintime lst_rrq = {0,0};
	struct bintime lst_umd = {0,0};
	struct bintime veri_min = {0,0};
	struct bintime veri_max = {0,0};
	struct bintime veri_pa = {0,0};
	int veri_sc = 0;
	int veri_pau = 0;

	g_topology_assert_not();

#if __FreeBSD_version >= 700000
	thread_lock(curthread);
	sched_prio(curthread, PRIBIO);
	thread_unlock(curthread);
#else
	mtx_lock_spin(&sched_lock);
	sched_prio(curthread, PRIBIO);
	mtx_unlock_spin(&sched_lock);
#endif

	getbinuptime(&lst_rrq);
	while (sc->sc_provider == NULL && !sc->term) {
		g_raid5_sleep(sc, NULL);
		getbinuptime(&now);
		if (now.sec - lst_rrq.sec > g_raid5_tooc)
			break;
	}
	MYKASSERT(sc->sc_rootmount != NULL, ("why-oh-why???"));
	root_mount_rel(sc->sc_rootmount);
	sc->sc_rootmount = NULL;
	if (!sc->term) {
		g_topology_lock();
		g_raid5_check_and_run(sc, 1);
		g_topology_unlock();
	}
	G_RAID5_DEBUG(0, "%s: activated%s.", sc->sc_name,
	              g_raid5_allgood(sc) ? "" : " (forced)");

	struct bio Vbp;
	struct bio vbp;

	int vbp_active = 0;
	while (!sc->term || vbp_active) {
		g_raid5_sleep(sc, &wt);
		if (curveri < 0)
			getbinuptime(&now);
		else
			binuptime(&now);

		MYKASSERT(sc->sc_provider != NULL, ("provider needed here."));
		if (sc->sc_provider == NULL)
			break;

		g_topology_assert_not();
		if (!vbp_active && !g_raid5_try_open(sc)) {
			wt = 1;
			continue;
		}

		if (g_raid5_flush_sq(sc, &now))
			lst_rrq = now;

		int upd_meta = 0;
		char *cause = NULL;
		if (!vbp_active) {
			if (sc->conf_order) {
				upd_meta = 1;
				g_raid5_worker_conf(sc,&cause);
			}
			if (sc->verified  >= sc->disksize) {
				if (!upd_meta) {
					upd_meta = 1;
					cause = "verify completed";
				}
				sc->verified = -1;
				curveri = -1;
				sc->newest = -1;
				sc->state &= ~G_RAID5_STATE_VERIFY;
				sc->veri_pa = 0;
				if ((sc->state&G_RAID5_STATE_COWOP) && sc->sc_provider->error)
					g_error_provider(sc->sc_provider, 0);
			} else if (sc->verified >= 0) {
				if (sc->verified != curveri) {
					curveri = sc->verified;
					if (sc->newest < 0) {
						for (int i=0; i<sc->sc_ndisks; i++)
							if (!g_raid5_disk_good(sc,i)) {
								sc->newest = i;
								break;
							}
					}
					if (!upd_meta) {
						upd_meta = last_upd < 0 || curveri < last_upd ||
						           now.sec - lst_umd.sec > 200;
						if (upd_meta)
							cause = "store verify progress";
					}
				}
				if (!(sc->state & G_RAID5_STATE_VERIFY)) {
					sc->state |= G_RAID5_STATE_VERIFY;
					if (!upd_meta) {
						upd_meta = 1;
						cause = "store verify flag";
					}
				}
				MYKASSERT((curveri & (sc->stripesize - 1)) == 0,
						  ("invalid value for curveri."));
			} else if (curveri != -1)
				curveri = -1;
		}

		int nv = g_raid5_nvalid(sc);
		if (last_nv != nv) {
			if (!upd_meta) {
				upd_meta = 1;
				cause = "valid disk count";
			}
			last_nv = nv;
		}

		if (sc->wqp) {
			if (!(sc->state & G_RAID5_STATE_HOT)) {
				sc->state |= G_RAID5_STATE_HOT;
				if (!upd_meta && !sc->no_hot) {
					upd_meta = 1;
					cause = "state hot";
				}
			}
		} else {
			if (curveri < 0 && (sc->state & G_RAID5_STATE_HOT) &&
			    now.sec - lst_umd.sec > g_raid5_wdt) {
				sc->state &= ~G_RAID5_STATE_HOT;
				if (!upd_meta && !sc->no_hot) {
					upd_meta = 1;
					cause = "state calm";
				}
			}
		}

		if (upd_meta) {
			if (curveri >= 0) {
				int per = sc->disksize ? curveri * 10000 / sc->disksize : 0;
				if (nv < sc->sc_ndisks)
					G_RAID5_DEBUG(0, "%s: first write at %d.%02d%% (cause: %s).",
					              sc->sc_name, per/100,per%100, cause);
				else {
					int stepper = sc->disksize > 0 && curveri > last_upd ?
					                (curveri - last_upd)*10000 / sc->disksize :
					                0;
					struct g_consumer *np = sc->newest<0 ? NULL: sc->sc_disks[sc->newest];
					char *dna = np==NULL ? "all" : np->provider->name;
					if (stepper > 0) {
						veri_sc = 0;
						struct bintime delta = now;
						bintime_sub(&delta, &lst_vbp);
						int64_t dt = g_raid5_bintime2micro(&delta) / 1000;
						G_RAID5_DEBUG(0, "%s: %s(%d): re-sync in progress: %d.%02d%% "
						                 "p:%d ETA:%jdmin (cause: %s).", sc->sc_name,
						              dna,sc->newest, per/100,per%100,veri_pau,
						              dt*(10000-per)/(stepper*60*1000), cause);
						veri_pau = 0;
					} else
						G_RAID5_DEBUG(1,"%s: %s(%d): re-sync in progress: "
						                "%d.%02d%% (cause: %s).", sc->sc_name,
						                dna, sc->newest, per/100,per%100, cause);
					lst_vbp = now;
				}
			} else
				G_RAID5_DEBUG(last_upd>=0?0:1,
				              "%s: update meta data on %d disks (cause: %s).",
				              sc->sc_name, nv, cause);
			if (!sc->wqp && (sc->state & G_RAID5_STATE_HOT))
				sc->state &= ~G_RAID5_STATE_HOT;
			g_topology_assert_not();
			g_raid5_update_metadatas(sc);
			lst_umd = now;
			last_upd = curveri;
		}

		if (vbp_active || curveri < 0 || nv < sc->sc_ndisks) {
			wt = 1;
			if (vbp_active) {
				if (vbp.bio_driver2 == NULL) {
					vbp_active = 0;
					if (!vbp.bio_error) {
						if (curveri == sc->verified)
							sc->verified += vbp.bio_length;
						wt = 0;
					}
					g_raid5_q_rm(sc,g_raid5_ce_by_bio(sc,&Vbp),&Vbp,0);
					struct bintime delta = now;
					bintime_sub(&delta,&Vbp.bio_t0);
					if (veri_sc == 0) {
						veri_min = delta;
						veri_max = delta;
					} else if (g_raid5_bintime_cmp(&delta, &veri_min) < 0)
						veri_min = delta;
					else if (g_raid5_bintime_cmp(&delta, &veri_max) > 0)
						veri_max = delta;
					delta = veri_max;
					bintime_sub(&delta, &veri_min);
					if (g_raid5_bintime2micro(&veri_max) >
					    g_raid5_bintime2micro(&veri_min)*g_raid5_veri_fac) {
						int dt = (veri_sc&3) + 3;
						veri_pau += dt;
						dt *= 100; /* milli seconds */
						veri_sc = 0;
						veri_pa = now;
						bintime_addx(&veri_pa, dt*18446744073709551ULL);
						sc->veri_pa++;
					} else
						veri_sc++;
				}
			} else
				g_raid5_unopen(sc);
			continue;
		}

		/* wait due to cache shortage? */
		if (sc->cc+1 >= sc->cs) {
			g_raid5_unopen(sc);
			wt = 1;
			continue;
		}

		/* wait due to processing time fluctuation? */
		if (veri_sc == 0 && g_raid5_bintime_cmp(&now,&veri_pa) < 0) {
			g_raid5_unopen(sc);
			wt = 1;
			continue;
		}

		/* wait for concurrent read? */
		struct bintime tmptim = now;
		bintime_sub(&tmptim, &lst_rrq);
		if (tmptim.sec == 0 && g_raid5_veri_nice < 1000 &&
		    tmptim.frac < g_raid5_veri_nice*18446744073709551ULL) {
			g_raid5_unopen(sc);
			wt = 1;
			continue;
		}

		/* wait for cache flush */
		if (sc->cc + 1 >= sc->cs) {
			g_raid5_unopen(sc);
			wt = 1;
			continue;
		}

		bzero(&Vbp,sizeof(Vbp));
		Vbp.bio_offset = curveri * (sc->sc_ndisks - 1);
		Vbp.bio_length = sc->fsl;
		if (g_raid5_stripe_conflict(sc, g_raid5_ce_by_bio(sc,&Vbp), &Vbp)) {
			g_raid5_unopen(sc);
			wt = 1;
			continue;
		}

		bzero(&vbp,sizeof(vbp));
		vbp.bio_cmd = BIO_READ;
		vbp.bio_offset = -1;
		vbp.bio_length = sc->stripesize;
		vbp.bio_driver2 = &sc->worker;

		struct bio_queue_head queue;
		bioq_init(&queue);
		int pno = (curveri >> sc->stripebits) % sc->sc_ndisks;
		int dno = (pno + 1) % sc->sc_ndisks;
		if (g_raid5_conv_veri_read(&queue, sc, &vbp, curveri,
		                           vbp.bio_length,dno,pno,NULL)) {
 			g_raid5_des_sq(sc,&queue, &vbp);
			g_raid5_unopen(sc);
			wt = 1;
		} else {
			Vbp.bio_t0.sec = now.sec;
			g_raid5_set_requested(sc,&Vbp);
			g_raid5_q_in(sc, g_raid5_ce_by_bio(sc,&Vbp), &Vbp, 1);

			g_raid5_req_sq(sc, &queue);

			vbp_active = 1;
		}
	}

	G_RAID5_DEBUG(0,"%s: worker thread exiting.",sc->sc_name);

	/* 1st term stage */
	g_topology_assert_not();
	mtx_lock(&sc->sq_mtx);
	sc->worker = NULL;
	mtx_unlock(&sc->sq_mtx);
	while ((!TAILQ_EMPTY(&sc->sq.queue)) || (!TAILQ_EMPTY(&sc->wdq.queue)))
		g_raid5_flush_sq(sc, &now);
	g_raid5_flush_wq(sc);
	sc->state &= ~G_RAID5_STATE_HOT;

	/* 2nd term stage */
	g_raid5_no_more_open(sc); /* wait for read requests to be readied */
	g_raid5_more_open(sc);
	mtx_lock(&sc->dq_mtx); /* make sure workerD thread sleeps or blocks */
	wakeup(&sc->workerD);
	sc->workerD = NULL;
	mtx_unlock(&sc->dq_mtx);
	g_raid5_undirty(sc, NULL,1);
/*ARNE
	MYKASSERT(sc->cc==0 && sc->dc==0,
	          ("no cache entries allowed here (cc%d dc%d).", sc->cc, sc->dc));
*/
if (sc->cc || sc->dc) { G_RAID5_DEBUG(0,"no cache entries allowed here (cc%d dc%d).",sc->cc,sc->dc); tsleep(&sc,PRIBIO, "gr5db", hz*3); G_RAID5_DEBUG(0,"no cache entries allowed here (cc%d dc%d).",sc->cc,sc->dc); } /*ARNE*/

	/* 3rd term stage */
	while (sc->term < 2) /* workerD thread still active? */
		tsleep(&sc->term, PRIBIO, "gr5ds", hz);
	sc->term = 3;
	wakeup(sc);

	curthread->td_pflags &= ~TDP_GEOM;
	kproc_exit(0);
}

/*
 * Add disk to given device.
 */
static int
g_raid5_add_disk(struct g_raid5_softc *sc, struct g_provider *pp, u_int no)
{
	struct g_consumer *cp, *fcp;
	struct g_geom *gp;
	int error;

	g_topology_assert();
	g_topology_unlock();
	g_raid5_no_more_open(sc);
	g_topology_lock();

	/* Metadata corrupted? */
	if (no >= sc->sc_ndisks) {
		error = EINVAL;
		goto unlock;
	}

	/* Check if disk is not already attached. */
	if (sc->sc_disks[no] != NULL) {
		error = EEXIST;
		goto unlock;
	}
	sc->preremoved[no] = 0;
	sc->vdc++;

	gp = sc->sc_geom;
	fcp = LIST_FIRST(&gp->consumer);
	cp = g_new_consumer(gp);
	error = g_attach(cp, pp);
	if (error != 0) {
		g_destroy_consumer(cp);
		goto unlock;
	}

	if (fcp != NULL && (fcp->acr>0 || fcp->acw>0 || fcp->ace>0))
		error = g_access(cp, fcp->acr, fcp->acw, fcp->ace);
	else
		error = g_access(cp, 1,1,1);
	if (error)
		goto undo;

	if (sc->sc_type == G_RAID5_TYPE_AUTOMATIC) {
		struct g_raid5_metadata md;

		/* Re-read metadata. */
		error = g_raid5_read_metadata(&cp, &md);
		if (error)
			goto undo;

		if (strcmp(md.md_magic, G_RAID5_MAGIC) != 0 ||
		    strcmp(md.md_name, sc->sc_name) != 0 ||
		    md.md_stripesize != sc->stripesize ||
		    md.md_id != sc->sc_id) {
			G_RAID5_DEBUG(0, "%s: %s: metadata changed.", sc->sc_name, pp->name);
			error = ENXIO;
			goto undo;
		}
		sc->hardcoded = md.md_provider[0]!=0;
		sc->no_hot = md.md_no_hot;

		if (md.md_state&G_RAID5_STATE_HOT) {
			if (sc->no_hot)
				G_RAID5_DEBUG(0, "%s: %s(%d): WARNING: HOT although no_hot.",
				              sc->sc_name, pp->name, no);
			sc->state |= G_RAID5_STATE_VERIFY;
			sc->verified = 0;
			if (sc->newest >= 0)
				G_RAID5_DEBUG(0, "%s: %s(%d): newest disk data (HOT): %d/%d.",
				              sc->sc_name,pp->name,no,md.md_newest,sc->newest);
			else {
				G_RAID5_DEBUG(0, "%s: %s(%d): newest disk data (HOT): %d.",
				              sc->sc_name,pp->name,no,md.md_newest);
				if (md.md_newest >= 0)
					sc->newest = md.md_newest;
			}
		} else {
			if (md.md_newest >= 0 && md.md_verified != -1) {
				sc->newest = md.md_newest;
				G_RAID5_DEBUG(0, "%s: %s(%d): newest disk data (CALM): %d.",
				              sc->sc_name,pp->name,no,md.md_newest);
			}
			if (md.md_state&G_RAID5_STATE_VERIFY) {
				sc->state |= G_RAID5_STATE_VERIFY;
				if (sc->verified==-1 || md.md_verified<sc->verified)
					sc->verified = md.md_verified != -1 ? md.md_verified : 0;
			}
		}
		if (!(sc->state&G_RAID5_STATE_VERIFY) && sc->newest >= 0) {
			G_RAID5_DEBUG(0, "%s: %s(%d): WARNING: newest disk but no verify.",
			              sc->sc_name,pp->name,no);
			sc->newest = -1;
		}
		if (md.md_state&G_RAID5_STATE_SAFEOP)
			sc->state |= G_RAID5_STATE_SAFEOP;
		if (md.md_state&G_RAID5_STATE_COWOP)
			sc->state |= G_RAID5_STATE_COWOP;
	} else
		sc->hardcoded = 0;

	cp->private = &sc->sc_disks[no];
	sc->sc_disks[no] = cp;

	G_RAID5_DEBUG(0, "%s: %s(%d): disk attached.", sc->sc_name,pp->name,no);

	g_raid5_check_and_run(sc, 0);

unlock:
	g_raid5_more_open(sc);

	if (!error)
		g_raid5_wakeup(sc);

	return error;

undo:
	g_detach(cp);
	g_destroy_consumer(cp);
	goto unlock;
}

/*
 * creates a device
 */
static struct g_geom *
g_raid5_create(struct g_class *mp, const struct g_raid5_metadata *md,
    u_int type)
{
	struct g_raid5_softc *sc;
	struct g_geom *gp;

	g_topology_assert();
	G_RAID5_DEBUG(1, "%s: creating device.", md->md_name);

	/* Two disks is minimum. */
	if (md->md_all < 2)
		return NULL;

	/* Check for duplicate unit */
	LIST_FOREACH(gp, &mp->geom, geom) {
		sc = gp->softc;
		if (sc != NULL && strcmp(sc->sc_name, md->md_name) == 0) {
			G_RAID5_DEBUG(0, "%s: device already configured.", gp->name);
			return (NULL);
		}
	}
	gp = g_new_geomf(mp, "%s", md->md_name);
	gp->softc = NULL;	/* for a moment */

	sc = malloc(sizeof(*sc), M_RAID5, M_WAITOK | M_ZERO);
	sc->sc_ndisks = md->md_all;
	sc->vdc = 0;
	gp->start = g_raid5_start;
	gp->spoiled = g_raid5_orphan;
	gp->orphan = g_raid5_orphan;
	gp->access = g_raid5_access;
	gp->dumpconf = g_raid5_dumpconf;

	sc->sc_id = md->md_id;
	sc->destroy_on_za = 0;
	sc->state = G_RAID5_STATE_CALM;
	sc->veri_pa = 0;
	sc->verified = -1;
	sc->newest = -1;
	g_raid5_set_stripesize(sc, md->md_stripesize);
	sc->no_more_open = 0;
	sc->open_count = 0;
	mtx_init(&sc->open_mtx, "graid5:open", NULL, MTX_DEF);
	sc->no_sleep = 0;
	mtx_init(&sc->sleep_mtx,"graid5:sleep/main", NULL, MTX_DEF);
	mtx_init(&sc->sq_mtx, "graid5:sq", NULL, MTX_DEF);
	bioq_init(&sc->sq);
	mtx_init(&sc->dq_mtx, "graid5:dq", NULL, MTX_DEF);
	bioq_init(&sc->dq);
	bioq_init(&sc->wdq);
	sc->wqp = 0;
	sc->wqpi = 0;
	sc->sc_disks = malloc(sizeof(*sc->sc_disks) * sc->sc_ndisks,
	                      M_RAID5, M_WAITOK | M_ZERO);
	sc->preremoved = malloc(sc->sc_ndisks, M_RAID5, M_WAITOK | M_ZERO);
	sc->lstdno = 0;
	sc->sc_type = type;

	g_raid5_cache_init(sc);

	sc->term = 0;
	if (kproc_create(g_raid5_worker, sc, &sc->worker, 0, 0,
							 "g_raid5/main %s", md->md_name) != 0) {
		sc->workerD = NULL;
		sc->worker = NULL;
	} else if (kproc_create(g_raid5_workerD, sc, &sc->workerD, 0, 0,
							 "g_raid5/done %s", md->md_name) != 0) {
		sc->workerD = NULL;
		sc->term = 1;
	}

	gp->softc = sc;
	sc->sc_geom = gp;
	sc->sc_provider = NULL;

	G_RAID5_DEBUG(0, "%s: device created (stripesize=%d).",
	              sc->sc_name, sc->stripesize);

	sc->sc_rootmount = root_mount_hold(sc->sc_name);

	return (gp);
}

/*
 * destroys a device
 */
static int
g_raid5_destroy(struct g_raid5_softc *sc, boolean_t force, boolean_t noyoyo)
{
	struct g_provider *pp;
	struct g_geom *gp;
	u_int no;

	g_topology_assert();

	if (sc == NULL)
		return ENXIO;

	pp = sc->sc_provider;
	if (pp != NULL && (pp->acr != 0 || pp->acw != 0 || pp->ace != 0)) {
		if (force) {
			if (sc->destroy_on_za) { /* za = zero access count */
				G_RAID5_DEBUG(0, "%s: device is still open. It "
				              "will be removed on last close.", pp->name);
				return EBUSY;
			}
			else
				G_RAID5_DEBUG(0, "%s: device is still open, so it "
				              "cannot be definitely removed.", pp->name);
		} else {
			G_RAID5_DEBUG(1, "%s: device is still open (r%dw%de%d).",
			              pp->name, pp->acr, pp->acw, pp->ace);
			return EBUSY;
		}
	}

	sc->term = 1;
	g_raid5_wakeup(sc); /* push worker() thread */
	while (sc->term < 3)
		tsleep(sc, PRIBIO, "gr5de", hz);

	for (no = 0; no < sc->sc_ndisks; no++)
		if (sc->sc_disks[no] != NULL)
			g_raid5_remove_disk(sc, &sc->sc_disks[no], 0, noyoyo);

	gp = sc->sc_geom;
	gp->softc = NULL;

	if (sc->sc_provider != NULL) {
		sc->sc_provider->flags |= G_PF_WITHER;
		g_orphan_provider(sc->sc_provider, ENXIO);
		sc->sc_provider = NULL;
	}

	G_RAID5_DEBUG(0, "%s: device removed.", sc->sc_name);

	mtx_destroy(&sc->open_mtx);
	mtx_destroy(&sc->sleep_mtx);
	mtx_destroy(&sc->sq_mtx);
	mtx_destroy(&sc->dq_mtx);
	free(sc->sc_disks, M_RAID5);
	free(sc->preremoved, M_RAID5);
	free(sc->ce, M_RAID5);
	g_raid5_mh_destroy(sc);
	free(sc, M_RAID5);

	pp = LIST_FIRST(&gp->provider);
	if (pp == NULL || (pp->acr == 0 && pp->acw == 0 && pp->ace == 0))
		G_RAID5_DEBUG(0, "%s: device destroyed.", gp->name);

	g_wither_geom(gp, ENXIO);

	return 0;
}

/*
 * eh? who needs this? :-) I think GEOM wants to have this...
 * see struct g_class  g_raid5_class
 */
static int
g_raid5_destroy_geom(struct gctl_req *req __unused,
                     struct g_class *mp __unused, struct g_geom *gp)
{
	struct g_raid5_softc *sc = gp->softc;
	return g_raid5_destroy(sc, 0, 0);
}

/*
 * GEOM calls this, if it finds a new device...
 * graid5 looks at each such device and uses it if certain conditions r met
 * especially it will not care for the size of the device, which makes
 *   device size increase operations easier
 * graid5 considers 2 disks, who share the last sector, to be mis-conf-ed... :)
 */
static struct g_geom *
g_raid5_taste(struct g_class *mp, struct g_provider *pp, int flags __unused)
{
	struct g_raid5_metadata md;
	struct g_consumer *cp;
	struct g_geom *gp;
	int error;

	g_trace(G_T_TOPOLOGY, "%s(%s, %s)", __func__, mp->name, pp->name);
	g_topology_assert();

	G_RAID5_DEBUG(3, "%s: tasting.", pp->name);

	gp = g_new_geomf(mp, "raid5:taste");
	gp->start = g_raid5_start;
	gp->access = g_raid5_access;
	gp->orphan = g_raid5_orphan;
	cp = g_new_consumer(gp);
	g_attach(cp, pp);
	error = g_raid5_read_metadata(&cp, &md);
	g_detach(cp);
	g_destroy_consumer(cp);
	g_destroy_geom(gp);
	if (error) {
		G_RAID5_DEBUG(3, "%s: read meta data failed: error:%d.", pp->name, error);
		return NULL;
	}
	gp = NULL;

	if (strcmp(md.md_magic, G_RAID5_MAGIC) != 0) {
		G_RAID5_DEBUG(3, "%s: wrong magic.", pp->name);
		return NULL;
	}
	if (md.md_version > G_RAID5_VERSION) {
		G_RAID5_DEBUG(0, "%s: geom_raid5.ko module is too old.", pp->name);
		return NULL;
	}
	/*
	 * Backward compatibility:
	 */
	if (md.md_provider[0] != '\0' && strcmp(md.md_provider, pp->name) != 0) {
		G_RAID5_DEBUG(0, "%s: %s: wrong hardcode.", md.md_name,pp->name);
		return NULL;
	}

	/* check for correct size */
	if (md.md_provsize != pp->mediasize)
		G_RAID5_DEBUG(0, "%s: %s: size mismatch (expected %jd, got %jd).",
		              md.md_name,pp->name, md.md_provsize,pp->mediasize);

	/*
	 * Let's check if device already exists.
	 */
	struct g_raid5_softc *sc = NULL;
	int found = 1;
	LIST_FOREACH(gp, &mp->geom, geom) {
		sc = gp->softc;
		if (sc == NULL)
			continue;
		if (sc->sc_type == G_RAID5_TYPE_AUTOMATIC &&
		    strcmp(md.md_name, sc->sc_name) == 0 &&
		    md.md_id == sc->sc_id)
			break;
	}
	if (gp == NULL) {
		found = 0;
		gp = g_raid5_create(mp, &md, G_RAID5_TYPE_AUTOMATIC);
		if (gp == NULL) {
			G_RAID5_DEBUG(0, "%s: cannot create device.", md.md_name);
			return NULL;
		}
		sc = gp->softc;
	}
	G_RAID5_DEBUG(1, "%s: %s: adding disk.", gp->name, pp->name);
	error = g_raid5_add_disk(sc, pp, md.md_no);
	if (error) {
		G_RAID5_DEBUG(0, "%s: %s: cannot add disk (error=%d).",
						  gp->name, pp->name, error);
		if (!found)
			g_raid5_destroy(sc, 1, 0);
		return NULL;
	}

	return gp;
}

/*
 * GEOM needs this... or doesnt it?
 */
static void
g_raid5_ctl_create(struct gctl_req *req, struct g_class *mp)
{
	u_int attached, no;
	struct g_raid5_metadata md;
	struct g_provider *pp;
	struct g_raid5_softc *sc;
	struct g_geom *gp;
	struct sbuf *sb;
	const char *name;
	char param[16];
	int *nargs;

	g_topology_assert();
	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs < 2) {
		gctl_error(req, "Too few arguments.");
		return;
	}

	strlcpy(md.md_magic, G_RAID5_MAGIC, sizeof(md.md_magic));
	md.md_version = G_RAID5_VERSION;
	name = gctl_get_asciiparam(req, "arg0");
	if (name == NULL) {
		gctl_error(req, "No 'arg%u' argument.", 0);
		return;
	}
	strlcpy(md.md_name, name, sizeof(md.md_name));
	md.md_id = arc4random();
	md.md_no = 0;
	md.md_all = *nargs - 1;
	bzero(md.md_provider, sizeof(md.md_provider));
	/* This field is not important here. */
	md.md_provsize = 0;

	/* Check all providers are valid */
	for (no = 1; no < *nargs; no++) {
		snprintf(param, sizeof(param), "arg%u", no);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%u' argument.", no);
			return;
		}
		if (strncmp(name, "/dev/", strlen("/dev/")) == 0)
			name += strlen("/dev/");
		pp = g_provider_by_name(name);
		if (pp == NULL) {
			G_RAID5_DEBUG(1, "%s: disk invalid.", name);
			gctl_error(req, "%s: disk invalid.", name);
			return;
		}
	}

	gp = g_raid5_create(mp, &md, G_RAID5_TYPE_MANUAL);
	if (gp == NULL) {
		gctl_error(req, "Can't configure %s.", md.md_name);
		return;
	}

	sc = gp->softc;
	sb = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
	sbuf_printf(sb, "Can't attach disk(s) to %s:", gp->name);
	for (attached = 0, no = 1; no < *nargs; no++) {
		snprintf(param, sizeof(param), "arg%u", no);
		name = gctl_get_asciiparam(req, param);
		if (strncmp(name, "/dev/", strlen("/dev/")) == 0)
			name += strlen("/dev/");
		pp = g_provider_by_name(name);
		MYKASSERT(pp != NULL, ("Provider %s disappear?!", name));
		if (g_raid5_add_disk(sc, pp, no - 1) != 0) {
			G_RAID5_DEBUG(1, "%s: %s(%d): disk not attached.",
			              gp->name, pp->name, no);
			sbuf_printf(sb, " %s", pp->name);
			continue;
		}
		attached++;
	}
	sbuf_finish(sb);
	if (md.md_all != attached) {
		g_raid5_destroy(gp->softc, 1, 0);
		gctl_error(req, "%s", sbuf_data(sb));
	}
	sbuf_delete(sb);
}

/*
 * find graid5 device by name
 */
static struct g_raid5_softc *
g_raid5_find_device(struct g_class *mp, const char *name)
{
	struct g_raid5_softc *sc;
	struct g_geom *gp;

	LIST_FOREACH(gp, &mp->geom, geom) {
		sc = gp->softc;
		if (sc == NULL)
			continue;
		if (strcmp(sc->sc_name, name) == 0)
			return (sc);
	}
	return (NULL);
}

/*
 * "graid5 destroy"...
 */
static void
g_raid5_ctl_destroy(struct gctl_req *req, struct g_class *mp)
{
	struct g_raid5_softc *sc;
	int *nargs, error;
	const char *name;
	char param[16];
	u_int i;

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
	int *force = gctl_get_paraml(req, "force", sizeof(*force));
	if (force == NULL) {
		gctl_error(req, "No '%s' argument.", "force");
		return;
	}
	int *noyoyo = gctl_get_paraml(req, "noyoyo", sizeof(*noyoyo));
	if (noyoyo == NULL) {
		gctl_error(req, "No '%s' argument.", "noyoyo");
		return;
	}

	for (i = 0; i < (u_int)*nargs; i++) {
		snprintf(param, sizeof(param), "arg%u", i);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%u' argument.", i);
			return;
		}
		sc = g_raid5_find_device(mp, name);
		if (sc == NULL) {
			gctl_error(req, "No such device: %s.", name);
			return;
		}
		error = g_raid5_destroy(sc, *force, *noyoyo);
		if (error != 0) {
			gctl_error(req, "Cannot destroy device %s (error=%d).",
			    sc->sc_name, error);
			return;
		}
	}
}

/*
 * "graid5 remove"...
 */
static void
g_raid5_ctl_remove(struct gctl_req *req, struct g_class *mp)
{
	/*just call remove_disk and clear meta*/
	struct g_raid5_softc *sc;
	int *nargs;
	const char *name;
	u_int i;

	g_topology_assert();

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs != 2) {
		gctl_error(req, "Exactly one device and one provider name needed.");
		return;
	}

	name = gctl_get_asciiparam(req, "arg0");
	if (name == NULL) {
		gctl_error(req, "No <name> argument.");
		return;
	}
	sc = g_raid5_find_device(mp, name);
	if (sc == NULL) {
		gctl_error(req, "provider (device %s) does not exist.", name);
		return;
	}

	for (i = 1; i < (u_int)*nargs; i++) {
		int j;
		struct g_provider *pp;
		char param[16];
		snprintf(param, sizeof(param), "arg%u", i);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%u' argument.", i);
			return;
		}
		pp = g_provider_by_name(name);
		if (pp == NULL) {
			gctl_error(req, "There is no provider '%s'.", name);
			return;
		}
		for (j=0; j<sc->sc_ndisks; j++) {
			struct g_consumer **cp = &sc->sc_disks[j];
			if ((*cp) != NULL && (*cp)->provider == pp) {
				g_raid5_remove_disk(sc, cp, 1, 0);
				break;
			}
		}
		if (j == sc->sc_ndisks)
			gctl_error(req, "Device '%s' has no provider '%s'.",sc->sc_name,name);
		
	}
}

/*
 * "graid5 insert"...
 */
static void
g_raid5_ctl_insert(struct gctl_req *req, struct g_class *mp)
{
	struct g_raid5_softc *sc;
	int *nargs, *hardcode;
	const char *name;
	u_int i;

	g_topology_assert();

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs != 2) {
		gctl_error(req, "Exactly one device and one provider name needed.");
		return;
	}

	hardcode = gctl_get_paraml(req, "hardcode", sizeof(*hardcode));
	if (hardcode == NULL) {
		gctl_error(req, "No <hardcode> argument.");
		return;
	}

	name = gctl_get_asciiparam(req, "arg0");
	if (name == NULL) {
		gctl_error(req, "No <name> argument.");
		return;
	}
	sc = g_raid5_find_device(mp, name);
	if (sc == NULL) {
		gctl_error(req, "No such device: %s.", name);
		return;
	}
	if (*hardcode && !sc->hardcoded)
		sc->hardcoded = 1;

	for (i = 1; i < (u_int)*nargs; i++) {
		int f, j;
		struct g_provider *pp;
		struct g_consumer *cp;
		char param[16];
		snprintf(param, sizeof(param), "arg%u", i);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'prov%u' argument.", i);
			return;
		}
		pp = g_provider_by_name(name);
		if (pp == NULL) {
			gctl_error(req, "There is no provider '%s'.", name);
			return;
		}
		if (sc->stripesize < pp->sectorsize) {
			gctl_error(req, "Provider '%s' has incompatible sector size"
			           " (stripe size is %d).", name, sc->stripesize);
			return;
		}
		for (j=0,f=-1; j < sc->sc_ndisks; j++) {
			struct g_consumer *cp = sc->sc_disks[j];
			if (cp == NULL) {
				if (f < 0)
					f = j;
			} else if (cp->provider == pp) {
				gctl_error(req, "Provider '%s' already attached to '%s'.",
				           name, sc->sc_name);
				return;
			}
		}
		if (f < 0) {
			gctl_error(req, "Device '%s' has enough providers.",sc->sc_name);
			return;
		}
		cp = g_new_consumer(sc->sc_geom);
		if (g_attach(cp, pp) != 0) {
			g_destroy_consumer(cp);
			gctl_error(req, "Cannot attach to provider %s.", name);
			return;
		}
		if (g_access(cp, 1,1,1) != 0) {
			g_detach(cp);
			g_destroy_consumer(cp);
			gctl_error(req, "%s: cannot attach to provider.", name);
			return;
		}
		g_topology_unlock();
		int error = g_raid5_update_metadata(sc, &cp,
							G_RAID5_STATE_HOT|G_RAID5_STATE_VERIFY, f, NULL);
		g_topology_lock();
		g_access(cp, -1,-1,-1);
		G_RAID5_DEBUG(0, "%s: %s: wrote meta data on insert: error:%d.",
		              sc->sc_name, name, error);
		if (pp->acr || pp->acw || pp->ace) {
			G_RAID5_DEBUG(0,"%s: have to correct acc counts: ac(%d,%d,%d).",
			              sc->sc_name,pp->acr,pp->acw,pp->ace);
			g_access(cp,-pp->acr,-pp->acw,-pp->ace);
			cp->acr = cp->acw = cp->ace = 0;
		}
		g_detach(cp);
		g_destroy_consumer(cp);
	}
}

/*
 * "graid5 configure"...
 */
static void
g_raid5_ctl_configure(struct gctl_req *req, struct g_class *mp)
{
	struct g_raid5_softc *sc;
	int *nargs, *hardcode, *rebuild;
	const char *name;

	g_topology_assert();

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs != 1) {
		gctl_error(req, "Wrong device count.");
		return;
	}

	hardcode = gctl_get_paraml(req, "hardcode", sizeof(*hardcode));
	if (hardcode == NULL) {
		gctl_error(req, "No <hardcode> argument.");
		return;
	}

	int *activate = gctl_get_paraml(req, "activate", sizeof(*activate));
	if (activate == NULL) {
		gctl_error(req, "No <activate> argument.");
		return;
	}

	int *no_hot = gctl_get_paraml(req, "nohot", sizeof(*no_hot));
	if (no_hot == NULL) {
		gctl_error(req, "No <nohot> argument.");
		return;
	}

	int *safeop = gctl_get_paraml(req, "safeop", sizeof(*safeop));
	if (safeop == NULL) {
		gctl_error(req, "No <safeop> argument.");
		return;
	}

	int *cowop = gctl_get_paraml(req, "cowop", sizeof(*cowop));
	if (cowop == NULL) {
		gctl_error(req, "No <cowop> argument.");
		return;
	}

	rebuild = gctl_get_paraml(req, "rebuild", sizeof(*rebuild));
	if (rebuild == NULL) {
		gctl_error(req, "No <rebuild> argument.");
		return;
	}

	name = gctl_get_asciiparam(req, "arg0");
	if (name == NULL) {
		gctl_error(req, "No <name> argument.");
		return;
	}
	sc = g_raid5_find_device(mp, name);
	if (sc == NULL) {
		gctl_error(req, "Device %s not found.", name);
		return;
	}

	if (*activate) {
		g_topology_unlock();
		g_raid5_no_more_open(sc);
		g_topology_lock();
		g_raid5_preremove_reset(sc);
		g_raid5_more_open(sc);
		g_error_provider(sc->sc_provider, 0);
	}

	if (*hardcode)
		sc->hardcoded = !sc->hardcoded;

	if (*safeop)
		sc->state ^= G_RAID5_STATE_SAFEOP;

	if (*cowop)
		sc->state ^= G_RAID5_STATE_COWOP;

	if (*no_hot)
		sc->no_hot = sc->no_hot ? 0 : 1;

	if (*rebuild) {
		if (sc->verified < 0)
			sc->conf_order = 1;
		else
			sc->conf_order = 2;
	} else
		sc->conf_order = 3;
}

/*
 * "graid5 ..."...
 */
static void
g_raid5_config(struct gctl_req *req, struct g_class *mp, const char *verb)
{
	uint32_t *version;

	g_topology_assert();

	version = gctl_get_paraml(req, "version", sizeof(*version));
	if (version == NULL) {
		gctl_error(req, "No '%s' argument.", "version");
		return;
	}
	if (*version != G_RAID5_VERSION) {
		gctl_error(req, "Userland and kernel parts are out of sync.");
		return;
	}

	if (strcmp(verb, "create") == 0)
		g_raid5_ctl_create(req, mp);
	else if (strcmp(verb, "destroy") == 0 ||
	         strcmp(verb, "stop") == 0)
		g_raid5_ctl_destroy(req, mp);
	else if (strcmp(verb, "remove") == 0)
		g_raid5_ctl_remove(req, mp);
	else if (strcmp(verb, "insert") == 0)
		g_raid5_ctl_insert(req, mp);
	else if (strcmp(verb, "configure") == 0)
		g_raid5_ctl_configure(req, mp);
	else
		gctl_error(req, "Unknown verb.");
}

/*
 * "graid5 list" / "graid5 status"...
 */
static void
g_raid5_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp,
                 struct g_consumer *cp, struct g_provider *pp)
{
	struct g_raid5_softc *sc;

	g_topology_assert();
	sc = gp->softc;
	if (sc == NULL)
		return;

	if (pp != NULL) {
		/* Nothing here. */
	} else if (cp != NULL) {
		int dn = g_raid5_find_disk(sc, cp);
		sbuf_printf(sb, "%s<Error>%s</Error>\n",
		            indent, dn < 0 || sc->preremoved[dn] ? "Yes" : "No");
		sbuf_printf(sb, "%s<DiskNo>%d</DiskNo>\n", indent, dn);
		if (sc->state & G_RAID5_STATE_VERIFY &&
		    (sc->newest < 0 || dn == sc->newest)) {
			int per = sc->disksize>0?sc->verified * 100 / sc->disksize:-1;
			sbuf_printf(sb, "%s<Synchronized>%jd / %d%% (p:%d)</Synchronized>\n",
			            indent, sc->verified, per, sc->veri_pa);
		}
	} else {
		sbuf_printf(sb, "%s<ID>%u</ID>\n", indent, (u_int)sc->sc_id);
		sbuf_printf(sb, "%s<Newest>%d</Newest>\n", indent, sc->newest);
		sbuf_printf(sb, "%s<Stripesize>%u</Stripesize>\n",
		            indent, (u_int)sc->stripesize);

		sbuf_printf(sb, "%s<Pending>(wqp %d // %d)",
		            indent, sc->wqp, sc->wqpi);
		struct bio *bp;
		struct g_raid5_cache_entry *ce;
		G_RAID5_C_TRAVERSE(sc,bp,ce)
			sbuf_printf(sb, " [%jd..%jd[/%jd%c",
			            bp->bio_offset,bp->bio_offset+bp->bio_length,
			            bp->bio_length,g_raid5_cache_code(sc,bp));
		sbuf_printf(sb, "</Pending>\n");

		sbuf_printf(sb, "%s<Type>", indent);
		switch (sc->sc_type) {
		case G_RAID5_TYPE_AUTOMATIC:
			sbuf_printf(sb, "AUTOMATIC");
			break;
		case G_RAID5_TYPE_MANUAL:
			sbuf_printf(sb, "MANUAL");
			break;
		default:
			sbuf_printf(sb, "UNKNOWN");
			break;
		}
		sbuf_printf(sb, "</Type>\n");

		int nv = g_raid5_nvalid(sc);
		sbuf_printf(sb, "%s<Status>Total:%u, Online:%u/%u",
		    indent, sc->sc_ndisks, nv, sc->vdc);
		sbuf_printf(sb, ", mhc:%d cc:%d/%d+%d</Status>\n",
		            sc->mhc,sc->cc,sc->cs,sc->cfc);

		sbuf_printf(sb, "%s<State>", indent);
		if (sc->sc_provider == NULL || sc->sc_provider->error != 0)
			sbuf_printf(sb, "FAILURE (%d)",
			            sc->sc_provider==NULL? -1 : sc->sc_provider->error);
		else {
			if (nv < sc->sc_ndisks - 1)
				sbuf_printf(sb, "CRITICAL");
			else if (nv < sc->sc_ndisks)
				sbuf_printf(sb, "DEGRADED");
			else if (sc->state & G_RAID5_STATE_VERIFY)
				sbuf_printf(sb, "REBUILDING");
			else
				sbuf_printf(sb, "COMPLETE");
			if (sc->state & G_RAID5_STATE_HOT)
				sbuf_printf(sb, " HOT");
			else
				sbuf_printf(sb, " CALM");
		}

		if (sc->state&G_RAID5_STATE_COWOP)
			sbuf_printf(sb, " (cowop)");
		if (sc->state&G_RAID5_STATE_SAFEOP)
			sbuf_printf(sb, " (safeop)");
		if (sc->hardcoded)
			sbuf_printf(sb, " (hardcoded)");
		if (sc->term)
			sbuf_printf(sb, " (destroyed?)");
		if (sc->no_hot)
			sbuf_printf(sb, " (no hot)");
		if (sc->worker == NULL)
			sbuf_printf(sb, " (NO OPERATION POSSIBLE)");
		sbuf_printf(sb, "</State>\n");
	}
}

/*
 * in case of system shutdown this function gets called, so that we can
 * bring the graid5 devices in a proper state (CALM)
 * since the right order is important we use the "destroy_on_za" feature
 */
static void
g_raid5_shutdown(void *arg, int howto __unused)
{
	struct g_class *mp = arg;
	struct g_geom *gp, *gp2;

	DROP_GIANT();
	g_topology_lock();
	mp->ctlreq = NULL;
	mp->taste = NULL;
	LIST_FOREACH_SAFE(gp, &mp->geom, geom, gp2) {
		struct g_raid5_softc *sc;
		if ((sc = gp->softc) == NULL)
			continue;
		sc->destroy_on_za = 1;
		g_raid5_destroy(sc, 1, 0);
	}
	g_topology_unlock();
	PICKUP_GIANT();
}

/*
 * module init (on load)
 */
static void
g_raid5_init(struct g_class *mp)
{
	g_raid5_post_sync = EVENTHANDLER_REGISTER(shutdown_post_sync,
	    g_raid5_shutdown, mp, SHUTDOWN_PRI_FIRST);
	if (g_raid5_post_sync == NULL)
		G_RAID5_DEBUG(0, "WARNING: cannot register shutdown event.");
	else
		G_RAID5_DEBUG(0,"registered shutdown event handler.");
}

/*
 * module fini (on unload)
 */
static void
g_raid5_fini(struct g_class *mp)
{
	if (g_raid5_post_sync != NULL) {
		G_RAID5_DEBUG(0,"deregister shutdown event handler.");
		EVENTHANDLER_DEREGISTER(shutdown_post_sync, g_raid5_post_sync);
		g_raid5_post_sync = NULL;
	}
}

DECLARE_GEOM_CLASS(g_raid5_class, g_raid5);
