/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2005-2019 Pawel Jakub Dawidek <pawel@dawidek.net>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cons.h>
#include <sys/kenv.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bio.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/eventhandler.h>
#include <sys/kthread.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/uio.h>
#include <sys/vnode.h>

#include <machine/vmparam.h>

#include <vm/uma.h>
#include <vm/vm.h>
#include <vm/swap_pager.h>

#include <geom/geom.h>
#include <geom/geom_dbg.h>
#include <geom/eli/g_eli.h>
#include <geom/eli/pkcs5v2.h>

#include <crypto/intake.h>

FEATURE(geom_eli, "GEOM crypto module");

MALLOC_DEFINE(M_ELI, "eli_data", "GEOM_ELI Data");

SYSCTL_DECL(_kern_geom);
SYSCTL_NODE(_kern_geom, OID_AUTO, eli, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "GEOM_ELI stuff");
static int g_eli_version = G_ELI_VERSION;
SYSCTL_INT(_kern_geom_eli, OID_AUTO, version, CTLFLAG_RD, &g_eli_version, 0,
    "GELI version");
int g_eli_debug = 0;
SYSCTL_INT(_kern_geom_eli, OID_AUTO, debug, CTLFLAG_RWTUN, &g_eli_debug, 0,
    "Debug level");
static u_int g_eli_tries = 3;
SYSCTL_UINT(_kern_geom_eli, OID_AUTO, tries, CTLFLAG_RWTUN, &g_eli_tries, 0,
    "Number of tries for entering the passphrase");
static u_int g_eli_visible_passphrase = GETS_NOECHO;
SYSCTL_UINT(_kern_geom_eli, OID_AUTO, visible_passphrase, CTLFLAG_RWTUN,
    &g_eli_visible_passphrase, 0,
    "Visibility of passphrase prompt (0 = invisible, 1 = visible, 2 = asterisk)");
u_int g_eli_overwrites = G_ELI_OVERWRITES;
SYSCTL_UINT(_kern_geom_eli, OID_AUTO, overwrites, CTLFLAG_RWTUN, &g_eli_overwrites,
    0, "Number of times on-disk keys should be overwritten when destroying them");
static u_int g_eli_threads = 0;
SYSCTL_UINT(_kern_geom_eli, OID_AUTO, threads, CTLFLAG_RWTUN, &g_eli_threads, 0,
    "Number of threads doing crypto work");
u_int g_eli_batch = 0;
SYSCTL_UINT(_kern_geom_eli, OID_AUTO, batch, CTLFLAG_RWTUN, &g_eli_batch, 0,
    "Use crypto operations batching");
static u_int g_eli_minbufs = 16;
static int sysctl_g_eli_minbufs(SYSCTL_HANDLER_ARGS);
SYSCTL_PROC(_kern_geom_eli, OID_AUTO, minbufs, CTLTYPE_UINT | CTLFLAG_RW |
    CTLFLAG_MPSAFE, NULL, 0, sysctl_g_eli_minbufs, "IU",
    "Number of GELI bufs reserved for swap transactions");
static bool g_eli_blocking_malloc = false;
SYSCTL_BOOL(_kern_geom_eli, OID_AUTO, blocking_malloc, CTLFLAG_RWTUN,
    &g_eli_blocking_malloc, 0, "Use blocking malloc calls for GELI buffers");
static bool g_eli_unmapped_io = true;
SYSCTL_BOOL(_kern_geom_eli, OID_AUTO, unmapped_io, CTLFLAG_RDTUN,
    &g_eli_unmapped_io, 0, "Enable support for unmapped I/O");
static int g_eli_alloc_sz;
SYSCTL_UINT(_kern_geom_eli, OID_AUTO, use_uma_bytes, CTLFLAG_RD,
    &g_eli_alloc_sz, 0, "Use uma(9) for allocations of this size or smaller.");

static struct sx g_eli_umalock;	/* Controls changes to UMA zone. */
SX_SYSINIT(g_eli_umalock, &g_eli_umalock, "GELI UMA");
static uma_zone_t g_eli_uma = NULL;
static volatile int g_eli_umaoutstanding;
static volatile int g_eli_devs;

/*
 * Control the number of reserved entries in the GELI zone.
 * If the GELI zone has already been allocated, update the zone. Otherwise,
 * simply update the variable for use the next time the zone is created.
 */
static int
sysctl_g_eli_minbufs(SYSCTL_HANDLER_ARGS)
{
	int error;
	u_int new;

	new = g_eli_minbufs;
	error = sysctl_handle_int(oidp, &new, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	sx_xlock(&g_eli_umalock);
	if (g_eli_uma != NULL) {
		if (new != g_eli_minbufs)
			uma_zone_reserve(g_eli_uma, new);
		if (new > g_eli_minbufs)
			uma_prealloc(g_eli_uma, new - g_eli_minbufs);
	}
	if (new != g_eli_minbufs)
		g_eli_minbufs = new;
	sx_xunlock(&g_eli_umalock);
	return (0);
}

/*
 * Passphrase cached during boot, in order to be more user-friendly if
 * there are multiple providers using the same passphrase.
 */
static char cached_passphrase[256];
static u_int g_eli_boot_passcache = 1;
TUNABLE_INT("kern.geom.eli.boot_passcache", &g_eli_boot_passcache);
SYSCTL_UINT(_kern_geom_eli, OID_AUTO, boot_passcache, CTLFLAG_RD,
    &g_eli_boot_passcache, 0,
    "Passphrases are cached during boot process for possible reuse");
static void
fetch_loader_passphrase(void * dummy)
{
	char * env_passphrase;

	KASSERT(dynamic_kenv, ("need dynamic kenv"));

	if ((env_passphrase = kern_getenv("kern.geom.eli.passphrase")) != NULL) {
		/* Extract passphrase from the environment. */
		strlcpy(cached_passphrase, env_passphrase,
		    sizeof(cached_passphrase));
		freeenv(env_passphrase);

		/* Wipe the passphrase from the environment. */
		kern_unsetenv("kern.geom.eli.passphrase");
	}
}
SYSINIT(geli_fetch_loader_passphrase, SI_SUB_KMEM + 1, SI_ORDER_ANY,
    fetch_loader_passphrase, NULL);

static void
zero_boot_passcache(void)
{

	explicit_bzero(cached_passphrase, sizeof(cached_passphrase));
}

static void
zero_geli_intake_keys(void)
{
	struct keybuf *keybuf;
	int i;

	if ((keybuf = get_keybuf()) != NULL) {
		/* Scan the key buffer, clear all GELI keys. */
		for (i = 0; i < keybuf->kb_nents; i++) {
			 if (keybuf->kb_ents[i].ke_type == KEYBUF_TYPE_GELI) {
				explicit_bzero(keybuf->kb_ents[i].ke_data,
				    sizeof(keybuf->kb_ents[i].ke_data));
				keybuf->kb_ents[i].ke_type = KEYBUF_TYPE_NONE;
			}
		}
	}
}

static void
zero_intake_passcache(void *dummy __unused)
{
	zero_boot_passcache();
	zero_geli_intake_keys();
}
EVENTHANDLER_DEFINE(mountroot, zero_intake_passcache, NULL, 0);

static eventhandler_tag g_eli_pre_sync = NULL;

static int g_eli_read_metadata_offset(struct g_class *mp, struct g_provider *pp,
    off_t offset, struct g_eli_metadata *md);

static int g_eli_destroy_geom(struct gctl_req *req, struct g_class *mp,
    struct g_geom *gp);
static void g_eli_init(struct g_class *mp);
static void g_eli_fini(struct g_class *mp);

static g_taste_t g_eli_taste;
static g_dumpconf_t g_eli_dumpconf;

struct g_class g_eli_class = {
	.name = G_ELI_CLASS_NAME,
	.version = G_VERSION,
	.ctlreq = g_eli_config,
	.taste = g_eli_taste,
	.destroy_geom = g_eli_destroy_geom,
	.init = g_eli_init,
	.fini = g_eli_fini
};

/*
 * Code paths:
 * BIO_READ:
 *	g_eli_start -> g_eli_crypto_read -> g_io_request -> g_eli_read_done -> g_eli_crypto_run -> g_eli_crypto_read_done -> g_io_deliver
 * BIO_WRITE:
 *	g_eli_start -> g_eli_crypto_run -> g_eli_crypto_write_done -> g_io_request -> g_eli_write_done -> g_io_deliver
 */

/*
 * EAGAIN from crypto(9) means, that we were probably balanced to another crypto
 * accelerator or something like this.
 * The function updates the SID and rerun the operation.
 */
int
g_eli_crypto_rerun(struct cryptop *crp)
{
	struct g_eli_softc *sc;
	struct g_eli_worker *wr;
	struct bio *bp;
	int error;

	bp = (struct bio *)crp->crp_opaque;
	sc = bp->bio_to->geom->softc;
	LIST_FOREACH(wr, &sc->sc_workers, w_next) {
		if (wr->w_number == G_ELI_WORKER(bp->bio_pflags))
			break;
	}
	KASSERT(wr != NULL, ("Invalid worker (%u).",
	    G_ELI_WORKER(bp->bio_pflags)));
	G_ELI_DEBUG(1, "Rerunning crypto %s request (sid: %p -> %p).",
	    bp->bio_cmd == BIO_READ ? "READ" : "WRITE", wr->w_sid,
	    crp->crp_session);
	wr->w_sid = crp->crp_session;
	crp->crp_etype = 0;
	error = crypto_dispatch(crp);
	if (error == 0)
		return (0);
	G_ELI_DEBUG(1, "%s: crypto_dispatch() returned %d.", __func__, error);
	crp->crp_etype = error;
	return (error);
}

static void
g_eli_getattr_done(struct bio *bp)
{
	if (bp->bio_error == 0 &&
	    !strcmp(bp->bio_attribute, "GEOM::physpath")) {
		strlcat(bp->bio_data, "/eli", bp->bio_length);
	}
	g_std_done(bp);
}

/*
 * The function is called afer reading encrypted data from the provider.
 *
 * g_eli_start -> g_eli_crypto_read -> g_io_request -> G_ELI_READ_DONE -> g_eli_crypto_run -> g_eli_crypto_read_done -> g_io_deliver
 */
void
g_eli_read_done(struct bio *bp)
{
	struct g_eli_softc *sc;
	struct bio *pbp;

	G_ELI_LOGREQ(2, bp, "Request done.");
	pbp = bp->bio_parent;
	if (pbp->bio_error == 0 && bp->bio_error != 0)
		pbp->bio_error = bp->bio_error;
	g_destroy_bio(bp);
	/*
	 * Do we have all sectors already?
	 */
	pbp->bio_inbed++;
	if (pbp->bio_inbed < pbp->bio_children)
		return;
	sc = pbp->bio_to->geom->softc;
	if (pbp->bio_error != 0) {
		G_ELI_LOGREQ(0, pbp, "%s() failed (error=%d)", __func__,
		    pbp->bio_error);
		pbp->bio_completed = 0;
		g_eli_free_data(pbp);
		g_io_deliver(pbp, pbp->bio_error);
		if (sc != NULL)
			atomic_subtract_int(&sc->sc_inflight, 1);
		return;
	}
	mtx_lock(&sc->sc_queue_mtx);
	bioq_insert_tail(&sc->sc_queue, pbp);
	mtx_unlock(&sc->sc_queue_mtx);
	wakeup(sc);
}

/*
 * The function is called after we encrypt and write data.
 *
 * g_eli_start -> g_eli_crypto_run -> g_eli_crypto_write_done -> g_io_request -> G_ELI_WRITE_DONE -> g_io_deliver
 */
void
g_eli_write_done(struct bio *bp)
{
	struct g_eli_softc *sc;
	struct bio *pbp;

	G_ELI_LOGREQ(2, bp, "Request done.");
	pbp = bp->bio_parent;
	if (pbp->bio_error == 0 && bp->bio_error != 0)
		pbp->bio_error = bp->bio_error;
	g_destroy_bio(bp);
	/*
	 * Do we have all sectors already?
	 */
	pbp->bio_inbed++;
	if (pbp->bio_inbed < pbp->bio_children)
		return;
	sc = pbp->bio_to->geom->softc;
	g_eli_free_data(pbp);
	if (pbp->bio_error != 0) {
		G_ELI_LOGREQ(0, pbp, "%s() failed (error=%d)", __func__,
		    pbp->bio_error);
		pbp->bio_completed = 0;
	} else
		pbp->bio_completed = pbp->bio_length;

	/*
	 * Write is finished, send it up.
	 */
	g_io_deliver(pbp, pbp->bio_error);
	if (sc != NULL)
		atomic_subtract_int(&sc->sc_inflight, 1);
}

/*
 * This function should never be called, but GEOM made as it set ->orphan()
 * method for every geom.
 */
static void
g_eli_orphan_spoil_assert(struct g_consumer *cp)
{

	panic("Function %s() called for %s.", __func__, cp->geom->name);
}

static void
g_eli_orphan(struct g_consumer *cp)
{
	struct g_eli_softc *sc;

	g_topology_assert();
	sc = cp->geom->softc;
	if (sc == NULL)
		return;
	g_eli_destroy(sc, TRUE);
}

static void
g_eli_resize(struct g_consumer *cp)
{
	struct g_eli_softc *sc;
	struct g_provider *epp, *pp;
	off_t oldsize;

	g_topology_assert();
	sc = cp->geom->softc;
	if (sc == NULL)
		return;

	if ((sc->sc_flags & G_ELI_FLAG_AUTORESIZE) == 0) {
		G_ELI_DEBUG(0, "Autoresize is turned off, old size: %jd.",
		    (intmax_t)sc->sc_provsize);
		return;
	}

	pp = cp->provider;

	if ((sc->sc_flags & G_ELI_FLAG_ONETIME) == 0) {
		struct g_eli_metadata md;
		u_char *sector;
		int error;

		sector = NULL;

		error = g_eli_read_metadata_offset(cp->geom->class, pp,
		    sc->sc_provsize - pp->sectorsize, &md);
		if (error != 0) {
			G_ELI_DEBUG(0, "Cannot read metadata from %s (error=%d).",
			    pp->name, error);
			goto iofail;
		}

		md.md_provsize = pp->mediasize;

		sector = malloc(pp->sectorsize, M_ELI, M_WAITOK | M_ZERO);
		eli_metadata_encode(&md, sector);
		error = g_write_data(cp, pp->mediasize - pp->sectorsize, sector,
		    pp->sectorsize);
		if (error != 0) {
			G_ELI_DEBUG(0, "Cannot store metadata on %s (error=%d).",
			    pp->name, error);
			goto iofail;
		}
		explicit_bzero(sector, pp->sectorsize);
		error = g_write_data(cp, sc->sc_provsize - pp->sectorsize,
		    sector, pp->sectorsize);
		if (error != 0) {
			G_ELI_DEBUG(0, "Cannot clear old metadata from %s (error=%d).",
			    pp->name, error);
			goto iofail;
		}
iofail:
		explicit_bzero(&md, sizeof(md));
		zfree(sector, M_ELI);
	}

	oldsize = sc->sc_mediasize;
	sc->sc_mediasize = eli_mediasize(sc, pp->mediasize, pp->sectorsize);
	g_eli_key_resize(sc);
	sc->sc_provsize = pp->mediasize;

	epp = LIST_FIRST(&sc->sc_geom->provider);
	g_resize_provider(epp, sc->sc_mediasize);
	G_ELI_DEBUG(0, "Device %s size changed from %jd to %jd.", epp->name,
	    (intmax_t)oldsize, (intmax_t)sc->sc_mediasize);
}

/*
 * BIO_READ:
 *	G_ELI_START -> g_eli_crypto_read -> g_io_request -> g_eli_read_done -> g_eli_crypto_run -> g_eli_crypto_read_done -> g_io_deliver
 * BIO_WRITE:
 *	G_ELI_START -> g_eli_crypto_run -> g_eli_crypto_write_done -> g_io_request -> g_eli_write_done -> g_io_deliver
 */
static void
g_eli_start(struct bio *bp)
{
	struct g_eli_softc *sc;
	struct g_consumer *cp;
	struct bio *cbp;

	sc = bp->bio_to->geom->softc;
	KASSERT(sc != NULL,
	    ("Provider's error should be set (error=%d)(device=%s).",
	    bp->bio_to->error, bp->bio_to->name));
	G_ELI_LOGREQ(2, bp, "Request received.");

	switch (bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
	case BIO_GETATTR:
	case BIO_FLUSH:
	case BIO_ZONE:
	case BIO_SPEEDUP:
		break;
	case BIO_DELETE:
		/*
		 * If the user hasn't set the NODELETE flag, we just pass
		 * it down the stack and let the layers beneath us do (or
		 * not) whatever they do with it.  If they have, we
		 * reject it.  A possible extension would be an
		 * additional flag to take it as a hint to shred the data
		 * with [multiple?] overwrites.
		 */
		if (!(sc->sc_flags & G_ELI_FLAG_NODELETE))
			break;
	default:
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	}
	cbp = g_clone_bio(bp);
	if (cbp == NULL) {
		g_io_deliver(bp, ENOMEM);
		return;
	}
	bp->bio_driver1 = cbp;
	bp->bio_pflags = 0;
	G_ELI_SET_NEW_BIO(bp->bio_pflags);
	switch (bp->bio_cmd) {
	case BIO_READ:
		if (!(sc->sc_flags & G_ELI_FLAG_AUTH)) {
			g_eli_crypto_read(sc, bp, 0);
			break;
		}
		/* FALLTHROUGH */
	case BIO_WRITE:
		mtx_lock(&sc->sc_queue_mtx);
		bioq_insert_tail(&sc->sc_queue, bp);
		mtx_unlock(&sc->sc_queue_mtx);
		wakeup(sc);
		break;
	case BIO_GETATTR:
	case BIO_FLUSH:
	case BIO_DELETE:
	case BIO_SPEEDUP:
	case BIO_ZONE:
		if (bp->bio_cmd == BIO_GETATTR)
			cbp->bio_done = g_eli_getattr_done;
		else
			cbp->bio_done = g_std_done;
		cp = LIST_FIRST(&sc->sc_geom->consumer);
		cbp->bio_to = cp->provider;
		G_ELI_LOGREQ(2, cbp, "Sending request.");
		g_io_request(cbp, cp);
		break;
	}
}

static int
g_eli_newsession(struct g_eli_worker *wr)
{
	struct g_eli_softc *sc;
	struct crypto_session_params csp;
	uint32_t caps;
	int error, new_crypto;
	void *key;

	sc = wr->w_softc;

	memset(&csp, 0, sizeof(csp));
	csp.csp_mode = CSP_MODE_CIPHER;
	csp.csp_cipher_alg = sc->sc_ealgo;
	csp.csp_ivlen = g_eli_ivlen(sc->sc_ealgo);
	csp.csp_cipher_klen = sc->sc_ekeylen / 8;
	if (sc->sc_ealgo == CRYPTO_AES_XTS)
		csp.csp_cipher_klen <<= 1;
	if ((sc->sc_flags & G_ELI_FLAG_FIRST_KEY) != 0) {
		key = g_eli_key_hold(sc, 0,
		    LIST_FIRST(&sc->sc_geom->consumer)->provider->sectorsize);
		csp.csp_cipher_key = key;
	} else {
		key = NULL;
		csp.csp_cipher_key = sc->sc_ekey;
	}
	if (sc->sc_flags & G_ELI_FLAG_AUTH) {
		csp.csp_mode = CSP_MODE_ETA;
		csp.csp_auth_alg = sc->sc_aalgo;
		csp.csp_auth_klen = G_ELI_AUTH_SECKEYLEN;
	}

	switch (sc->sc_crypto) {
	case G_ELI_CRYPTO_SW_ACCEL:
	case G_ELI_CRYPTO_SW:
		error = crypto_newsession(&wr->w_sid, &csp,
		    CRYPTOCAP_F_SOFTWARE);
		break;
	case G_ELI_CRYPTO_HW:
		error = crypto_newsession(&wr->w_sid, &csp,
		    CRYPTOCAP_F_HARDWARE);
		break;
	case G_ELI_CRYPTO_UNKNOWN:
		error = crypto_newsession(&wr->w_sid, &csp,
		    CRYPTOCAP_F_HARDWARE | CRYPTOCAP_F_SOFTWARE);
		if (error == 0) {
			caps = crypto_ses2caps(wr->w_sid);
			if (caps & CRYPTOCAP_F_HARDWARE)
				new_crypto = G_ELI_CRYPTO_HW;
			else if (caps & CRYPTOCAP_F_ACCEL_SOFTWARE)
				new_crypto = G_ELI_CRYPTO_SW_ACCEL;
			else
				new_crypto = G_ELI_CRYPTO_SW;
			mtx_lock(&sc->sc_queue_mtx);
			if (sc->sc_crypto == G_ELI_CRYPTO_UNKNOWN)
				sc->sc_crypto = new_crypto;
			mtx_unlock(&sc->sc_queue_mtx);
		}
		break;
	default:
		panic("%s: invalid condition", __func__);
	}

	if ((sc->sc_flags & G_ELI_FLAG_FIRST_KEY) != 0) {
		if (error)
			g_eli_key_drop(sc, key);
		else
			wr->w_first_key = key;
	}

	return (error);
}

static void
g_eli_freesession(struct g_eli_worker *wr)
{
	struct g_eli_softc *sc;

	crypto_freesession(wr->w_sid);
	if (wr->w_first_key != NULL) {
		sc = wr->w_softc;
		g_eli_key_drop(sc, wr->w_first_key);
		wr->w_first_key = NULL;
	}
}

static void
g_eli_cancel(struct g_eli_softc *sc)
{
	struct bio *bp;

	mtx_assert(&sc->sc_queue_mtx, MA_OWNED);

	while ((bp = bioq_takefirst(&sc->sc_queue)) != NULL) {
		KASSERT(G_ELI_IS_NEW_BIO(bp->bio_pflags),
		    ("Not new bio when canceling (bp=%p).", bp));
		g_io_deliver(bp, ENXIO);
	}
}

static struct bio *
g_eli_takefirst(struct g_eli_softc *sc)
{
	struct bio *bp;

	mtx_assert(&sc->sc_queue_mtx, MA_OWNED);

	if (!(sc->sc_flags & G_ELI_FLAG_SUSPEND))
		return (bioq_takefirst(&sc->sc_queue));
	/*
	 * Device suspended, so we skip new I/O requests.
	 */
	TAILQ_FOREACH(bp, &sc->sc_queue.queue, bio_queue) {
		if (!G_ELI_IS_NEW_BIO(bp->bio_pflags))
			break;
	}
	if (bp != NULL)
		bioq_remove(&sc->sc_queue, bp);
	return (bp);
}

/*
 * This is the main function for kernel worker thread when we don't have
 * hardware acceleration and we have to do cryptography in software.
 * Dedicated thread is needed, so we don't slow down g_up/g_down GEOM
 * threads with crypto work.
 */
static void
g_eli_worker(void *arg)
{
	struct g_eli_softc *sc;
	struct g_eli_worker *wr;
	struct bio *bp;
	int error __diagused;

	wr = arg;
	sc = wr->w_softc;
#ifdef EARLY_AP_STARTUP
	MPASS(!sc->sc_cpubind || smp_started);
#elif defined(SMP)
	/* Before sched_bind() to a CPU, wait for all CPUs to go on-line. */
	if (sc->sc_cpubind) {
		while (!smp_started)
			tsleep(wr, 0, "geli:smp", hz / 4);
	}
#endif
	thread_lock(curthread);
	sched_prio(curthread, PUSER);
	if (sc->sc_cpubind)
		sched_bind(curthread, wr->w_number % mp_ncpus);
	thread_unlock(curthread);

	G_ELI_DEBUG(1, "Thread %s started.", curthread->td_proc->p_comm);

	for (;;) {
		mtx_lock(&sc->sc_queue_mtx);
again:
		bp = g_eli_takefirst(sc);
		if (bp == NULL) {
			if (sc->sc_flags & G_ELI_FLAG_DESTROY) {
				g_eli_cancel(sc);
				LIST_REMOVE(wr, w_next);
				g_eli_freesession(wr);
				free(wr, M_ELI);
				G_ELI_DEBUG(1, "Thread %s exiting.",
				    curthread->td_proc->p_comm);
				wakeup(&sc->sc_workers);
				mtx_unlock(&sc->sc_queue_mtx);
				kproc_exit(0);
			}
			while (sc->sc_flags & G_ELI_FLAG_SUSPEND) {
				if (sc->sc_inflight > 0) {
					G_ELI_DEBUG(0, "inflight=%d",
					    sc->sc_inflight);
					/*
					 * We still have inflight BIOs, so
					 * sleep and retry.
					 */
					msleep(sc, &sc->sc_queue_mtx, PRIBIO,
					    "geli:inf", hz / 5);
					goto again;
				}
				/*
				 * Suspend requested, mark the worker as
				 * suspended and go to sleep.
				 */
				if (wr->w_active) {
					g_eli_freesession(wr);
					wr->w_active = FALSE;
				}
				wakeup(&sc->sc_workers);
				msleep(sc, &sc->sc_queue_mtx, PRIBIO,
				    "geli:suspend", 0);
				if (!wr->w_active &&
				    !(sc->sc_flags & G_ELI_FLAG_SUSPEND)) {
					error = g_eli_newsession(wr);
					KASSERT(error == 0,
					    ("g_eli_newsession() failed on resume (error=%d)",
					    error));
					wr->w_active = TRUE;
				}
				goto again;
			}
			msleep(sc, &sc->sc_queue_mtx, PDROP, "geli:w", 0);
			continue;
		}
		if (G_ELI_IS_NEW_BIO(bp->bio_pflags))
			atomic_add_int(&sc->sc_inflight, 1);
		mtx_unlock(&sc->sc_queue_mtx);
		if (G_ELI_IS_NEW_BIO(bp->bio_pflags)) {
			G_ELI_SETWORKER(bp->bio_pflags, 0);
			if (sc->sc_flags & G_ELI_FLAG_AUTH) {
				if (bp->bio_cmd == BIO_READ)
					g_eli_auth_read(sc, bp);
				else
					g_eli_auth_run(wr, bp);
			} else {
				if (bp->bio_cmd == BIO_READ)
					g_eli_crypto_read(sc, bp, 1);
				else
					g_eli_crypto_run(wr, bp);
			}
		} else {
			if (sc->sc_flags & G_ELI_FLAG_AUTH)
				g_eli_auth_run(wr, bp);
			else
				g_eli_crypto_run(wr, bp);
		}
	}
}

static int
g_eli_read_metadata_offset(struct g_class *mp, struct g_provider *pp,
    off_t offset, struct g_eli_metadata *md)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	u_char *buf = NULL;
	int error;

	g_topology_assert();

	gp = g_new_geomf(mp, "eli:taste");
	gp->start = g_eli_start;
	gp->access = g_std_access;
	/*
	 * g_eli_read_metadata() is always called from the event thread.
	 * Our geom is created and destroyed in the same event, so there
	 * could be no orphan nor spoil event in the meantime.
	 */
	gp->orphan = g_eli_orphan_spoil_assert;
	gp->spoiled = g_eli_orphan_spoil_assert;
	cp = g_new_consumer(gp);
	cp->flags |= G_CF_DIRECT_SEND | G_CF_DIRECT_RECEIVE;
	error = g_attach(cp, pp);
	if (error != 0)
		goto end;
	error = g_access(cp, 1, 0, 0);
	if (error != 0)
		goto end;
	g_topology_unlock();
	buf = g_read_data(cp, offset, pp->sectorsize, &error);
	g_topology_lock();
	if (buf == NULL)
		goto end;
	error = eli_metadata_decode(buf, md);
	if (error != 0)
		goto end;
	/* Metadata was read and decoded successfully. */
end:
	g_free(buf);
	if (cp->provider != NULL) {
		if (cp->acr == 1)
			g_access(cp, -1, 0, 0);
		g_detach(cp);
	}
	g_destroy_consumer(cp);
	g_destroy_geom(gp);
	return (error);
}

int
g_eli_read_metadata(struct g_class *mp, struct g_provider *pp,
    struct g_eli_metadata *md)
{

	return (g_eli_read_metadata_offset(mp, pp,
	    pp->mediasize - pp->sectorsize, md));
}

/*
 * The function is called when we had last close on provider and user requested
 * to close it when this situation occur.
 */
static void
g_eli_last_close(void *arg, int flags __unused)
{
	struct g_geom *gp;
	char gpname[64];
	int error __diagused;

	g_topology_assert();
	gp = arg;
	strlcpy(gpname, gp->name, sizeof(gpname));
	error = g_eli_destroy(gp->softc, TRUE);
	KASSERT(error == 0, ("Cannot detach %s on last close (error=%d).",
	    gpname, error));
	G_ELI_DEBUG(0, "Detached %s on last close.", gpname);
}

int
g_eli_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_eli_softc *sc;
	struct g_geom *gp;

	gp = pp->geom;
	sc = gp->softc;

	if (dw > 0) {
		if (sc->sc_flags & G_ELI_FLAG_RO) {
			/* Deny write attempts. */
			return (EROFS);
		}
		/* Someone is opening us for write, we need to remember that. */
		sc->sc_flags |= G_ELI_FLAG_WOPEN;
		return (0);
	}
	/* Is this the last close? */
	if (pp->acr + dr > 0 || pp->acw + dw > 0 || pp->ace + de > 0)
		return (0);

	/*
	 * Automatically detach on last close if requested.
	 */
	if ((sc->sc_flags & G_ELI_FLAG_RW_DETACH) ||
	    (sc->sc_flags & G_ELI_FLAG_WOPEN)) {
		g_post_event(g_eli_last_close, gp, M_WAITOK, NULL);
	}
	return (0);
}

static int
g_eli_cpu_is_disabled(int cpu)
{
#ifdef SMP
	return (CPU_ISSET(cpu, &hlt_cpus_mask));
#else
	return (0);
#endif
}

static void
g_eli_init_uma(void)
{

	atomic_add_int(&g_eli_devs, 1);
	sx_xlock(&g_eli_umalock);
	if (g_eli_uma == NULL) {
		/*
		 * Calculate the maximum-sized swap buffer we are
		 * likely to see.
		 */
		g_eli_alloc_sz = roundup2((PAGE_SIZE + sizeof(int) +
		    G_ELI_AUTH_SECKEYLEN) * nsw_cluster_max +
		    sizeof(uintptr_t), PAGE_SIZE);

		g_eli_uma = uma_zcreate("GELI buffers", g_eli_alloc_sz,
		    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);

		/* Reserve and pre-allocate pages, as appropriate. */
		uma_zone_reserve(g_eli_uma, g_eli_minbufs);
		uma_prealloc(g_eli_uma, g_eli_minbufs);
	}
	sx_xunlock(&g_eli_umalock);
}

/*
 * Try to destroy the UMA pool. This will do nothing if there are existing
 * GELI devices or existing UMA allocations.
 */
static void
g_eli_destroy_uma(void)
{
	uma_zone_t oldzone;

	sx_xlock(&g_eli_umalock);
	/* Ensure we really should be destroying this. */
	if (atomic_load_int(&g_eli_devs) == 0 &&
	    atomic_load_int(&g_eli_umaoutstanding) == 0) {
		oldzone = g_eli_uma;
		g_eli_uma = NULL;
	} else
		oldzone = NULL;
	sx_xunlock(&g_eli_umalock);

	if (oldzone != NULL)
		uma_zdestroy(oldzone);
}

static void
g_eli_fini_uma(void)
{

	/*
	 * If this is the last outstanding GELI device, try to
	 * destroy the UMA pool.
	 */
	if (atomic_fetchadd_int(&g_eli_devs, -1) == 1)
		g_eli_destroy_uma();
}

/*
 * Allocate a data buffer. If the size fits within our swap-sized buffers,
 * try to allocate a swap-sized buffer from the UMA pool. Otherwise, fall
 * back to using malloc.
 *
 * Swap-related requests are special: they can only use the UMA pool, they
 * use M_USE_RESERVE to let them dip farther into system resources, and
 * they always use M_NOWAIT to prevent swap operations from deadlocking.
 */
bool
g_eli_alloc_data(struct bio *bp, int sz)
{

	KASSERT(sz <= g_eli_alloc_sz || (bp->bio_flags & BIO_SWAP) == 0,
	    ("BIO_SWAP request for %d bytes exceeds the precalculated buffer"
	    " size (%d)", sz, g_eli_alloc_sz));
	if (sz <= g_eli_alloc_sz) {
		bp->bio_driver2 = uma_zalloc(g_eli_uma, M_NOWAIT |
		    ((bp->bio_flags & BIO_SWAP) != 0 ? M_USE_RESERVE : 0));
		if (bp->bio_driver2 != NULL) {
			bp->bio_pflags |= G_ELI_UMA_ALLOC;
			atomic_add_int(&g_eli_umaoutstanding, 1);
		}
		if (bp->bio_driver2 != NULL || (bp->bio_flags & BIO_SWAP) != 0)
			return (bp->bio_driver2 != NULL);
	}
	bp->bio_pflags &= ~(G_ELI_UMA_ALLOC);
	bp->bio_driver2 = malloc(sz, M_ELI, g_eli_blocking_malloc ? M_WAITOK :
	    M_NOWAIT);
	return (bp->bio_driver2 != NULL);
}

/*
 * Free a buffer from bp->bio_driver2 which was allocated with
 * g_eli_alloc_data(). This function makes sure that the memory is freed
 * to the correct place.
 *
 * Additionally, if this function frees the last outstanding UMA request
 * and there are no open GELI devices, this will destroy the UMA pool.
 */
void
g_eli_free_data(struct bio *bp)
{

	/*
	 * Mimic the free(9) behavior of allowing a NULL pointer to be
	 * freed.
	 */
	if (bp->bio_driver2 == NULL)
		return;

	if ((bp->bio_pflags & G_ELI_UMA_ALLOC) != 0) {
		uma_zfree(g_eli_uma, bp->bio_driver2);
		if (atomic_fetchadd_int(&g_eli_umaoutstanding, -1) == 1 &&
		    atomic_load_int(&g_eli_devs) == 0)
			g_eli_destroy_uma();
	} else
		free(bp->bio_driver2, M_ELI);
	bp->bio_driver2 = NULL;
}

struct g_geom *
g_eli_create(struct gctl_req *req, struct g_class *mp, struct g_provider *bpp,
    const struct g_eli_metadata *md, const u_char *mkey, int nkey)
{
	struct g_eli_softc *sc;
	struct g_eli_worker *wr;
	struct g_geom *gp;
	struct g_provider *pp;
	struct g_consumer *cp;
	struct g_geom_alias *gap;
	u_int i, threads;
	int dcw, error;

	G_ELI_DEBUG(1, "Creating device %s%s.", bpp->name, G_ELI_SUFFIX);
	KASSERT(eli_metadata_crypto_supported(md),
	    ("%s: unsupported crypto for %s", __func__, bpp->name));

	gp = g_new_geomf(mp, "%s%s", bpp->name, G_ELI_SUFFIX);
	sc = malloc(sizeof(*sc), M_ELI, M_WAITOK | M_ZERO);
	gp->start = g_eli_start;
	/*
	 * Spoiling can happen even though we have the provider open
	 * exclusively, e.g. through media change events.
	 */
	gp->spoiled = g_eli_orphan;
	gp->orphan = g_eli_orphan;
	gp->resize = g_eli_resize;
	gp->dumpconf = g_eli_dumpconf;
	/*
	 * If detach-on-last-close feature is not enabled and we don't operate
	 * on read-only provider, we can simply use g_std_access().
	 */
	if (md->md_flags & (G_ELI_FLAG_WO_DETACH | G_ELI_FLAG_RO))
		gp->access = g_eli_access;
	else
		gp->access = g_std_access;

	eli_metadata_softc(sc, md, bpp->sectorsize, bpp->mediasize);
	sc->sc_nkey = nkey;

	gp->softc = sc;
	sc->sc_geom = gp;

	bioq_init(&sc->sc_queue);
	mtx_init(&sc->sc_queue_mtx, "geli:queue", NULL, MTX_DEF);
	mtx_init(&sc->sc_ekeys_lock, "geli:ekeys", NULL, MTX_DEF);
	g_eli_init_uma();

	pp = NULL;
	cp = g_new_consumer(gp);
	cp->flags |= G_CF_DIRECT_SEND | G_CF_DIRECT_RECEIVE;

	error = g_attach(cp, bpp);
	if (error != 0) {
		if (req != NULL) {
			gctl_error(req, "Cannot attach to %s (error=%d).",
			    bpp->name, error);
		} else {
			G_ELI_DEBUG(1, "Cannot attach to %s (error=%d).",
			    bpp->name, error);
		}
		goto failed;
	}
	/*
	 * Keep provider open all the time, so we can run critical tasks,
	 * like Master Keys deletion, without wondering if we can open
	 * provider or not.
	 * We don't open provider for writing only when user requested read-only
	 * access.
	 */
	dcw = (sc->sc_flags & G_ELI_FLAG_RO) ? 0 : 1;
	error = g_access(cp, 1, dcw, 1);
	if (error != 0) {
		if (req != NULL) {
			gctl_error(req, "Cannot access %s (error=%d).",
			    bpp->name, error);
		} else {
			G_ELI_DEBUG(1, "Cannot access %s (error=%d).",
			    bpp->name, error);
		}
		goto failed;
	}

	/*
	 * Remember the keys in our softc structure.
	 */
	g_eli_mkey_propagate(sc, mkey);

	LIST_INIT(&sc->sc_workers);

	threads = g_eli_threads;
	if (threads == 0)
		threads = mp_ncpus;
	sc->sc_cpubind = (mp_ncpus > 1 && threads == mp_ncpus);
	for (i = 0; i < threads; i++) {
		if (g_eli_cpu_is_disabled(i)) {
			G_ELI_DEBUG(1, "%s: CPU %u disabled, skipping.",
			    bpp->name, i);
			continue;
		}
		wr = malloc(sizeof(*wr), M_ELI, M_WAITOK | M_ZERO);
		wr->w_softc = sc;
		wr->w_number = i;
		wr->w_active = TRUE;

		error = g_eli_newsession(wr);
		if (error != 0) {
			free(wr, M_ELI);
			if (req != NULL) {
				gctl_error(req, "Cannot set up crypto session "
				    "for %s (error=%d).", bpp->name, error);
			} else {
				G_ELI_DEBUG(1, "Cannot set up crypto session "
				    "for %s (error=%d).", bpp->name, error);
			}
			goto failed;
		}

		error = kproc_create(g_eli_worker, wr, &wr->w_proc, 0, 0,
		    "g_eli[%u] %s", i, bpp->name);
		if (error != 0) {
			g_eli_freesession(wr);
			free(wr, M_ELI);
			if (req != NULL) {
				gctl_error(req, "Cannot create kernel thread "
				    "for %s (error=%d).", bpp->name, error);
			} else {
				G_ELI_DEBUG(1, "Cannot create kernel thread "
				    "for %s (error=%d).", bpp->name, error);
			}
			goto failed;
		}
		LIST_INSERT_HEAD(&sc->sc_workers, wr, w_next);
	}

	/*
	 * Create decrypted provider.
	 */
	pp = g_new_providerf(gp, "%s%s", bpp->name, G_ELI_SUFFIX);
	pp->flags |= G_PF_DIRECT_SEND | G_PF_DIRECT_RECEIVE;
	if (g_eli_unmapped_io && CRYPTO_HAS_VMPAGE) {
		/*
		 * On DMAP architectures we can use unmapped I/O.  But don't
		 * use it with data integrity verification.  That code hasn't
		 * been written yet.
		 */
		 if ((sc->sc_flags & G_ELI_FLAG_AUTH) == 0)
			pp->flags |= G_PF_ACCEPT_UNMAPPED;
	}
	pp->mediasize = sc->sc_mediasize;
	pp->sectorsize = sc->sc_sectorsize;
	LIST_FOREACH(gap, &bpp->aliases, ga_next)
		g_provider_add_alias(pp, "%s%s", gap->ga_alias, G_ELI_SUFFIX);

	g_error_provider(pp, 0);

	G_ELI_DEBUG(0, "Device %s created.", pp->name);
	G_ELI_DEBUG(0, "Encryption: %s %u", g_eli_algo2str(sc->sc_ealgo),
	    sc->sc_ekeylen);
	if (sc->sc_flags & G_ELI_FLAG_AUTH)
		G_ELI_DEBUG(0, " Integrity: %s", g_eli_algo2str(sc->sc_aalgo));
	G_ELI_DEBUG(0, "    Crypto: %s",
	    sc->sc_crypto == G_ELI_CRYPTO_SW_ACCEL ? "accelerated software" :
	    sc->sc_crypto == G_ELI_CRYPTO_SW ? "software" : "hardware");
	return (gp);

failed:
	mtx_lock(&sc->sc_queue_mtx);
	sc->sc_flags |= G_ELI_FLAG_DESTROY;
	wakeup(sc);
	/*
	 * Wait for kernel threads self destruction.
	 */
	while (!LIST_EMPTY(&sc->sc_workers)) {
		msleep(&sc->sc_workers, &sc->sc_queue_mtx, PRIBIO,
		    "geli:destroy", 0);
	}
	mtx_destroy(&sc->sc_queue_mtx);
	if (cp->provider != NULL) {
		if (cp->acr == 1)
			g_access(cp, -1, -dcw, -1);
		g_detach(cp);
	}
	g_destroy_consumer(cp);
	g_destroy_geom(gp);
	g_eli_key_destroy(sc);
	g_eli_fini_uma();
	zfree(sc, M_ELI);
	return (NULL);
}

int
g_eli_destroy(struct g_eli_softc *sc, boolean_t force)
{
	struct g_geom *gp;
	struct g_provider *pp;

	g_topology_assert();

	if (sc == NULL)
		return (ENXIO);

	gp = sc->sc_geom;
	pp = LIST_FIRST(&gp->provider);
	if (pp != NULL && (pp->acr != 0 || pp->acw != 0 || pp->ace != 0)) {
		if (force) {
			G_ELI_DEBUG(1, "Device %s is still open, so it "
			    "cannot be definitely removed.", pp->name);
			sc->sc_flags |= G_ELI_FLAG_RW_DETACH;
			gp->access = g_eli_access;
			g_wither_provider(pp, ENXIO);
			return (EBUSY);
		} else {
			G_ELI_DEBUG(1,
			    "Device %s is still open (r%dw%de%d).", pp->name,
			    pp->acr, pp->acw, pp->ace);
			return (EBUSY);
		}
	}

	mtx_lock(&sc->sc_queue_mtx);
	sc->sc_flags |= G_ELI_FLAG_DESTROY;
	wakeup(sc);
	while (!LIST_EMPTY(&sc->sc_workers)) {
		msleep(&sc->sc_workers, &sc->sc_queue_mtx, PRIBIO,
		    "geli:destroy", 0);
	}
	mtx_destroy(&sc->sc_queue_mtx);
	gp->softc = NULL;
	g_eli_key_destroy(sc);
	g_eli_fini_uma();
	zfree(sc, M_ELI);

	G_ELI_DEBUG(0, "Device %s destroyed.", gp->name);
	g_wither_geom_close(gp, ENXIO);

	return (0);
}

static int
g_eli_destroy_geom(struct gctl_req *req __unused,
    struct g_class *mp __unused, struct g_geom *gp)
{
	struct g_eli_softc *sc;

	sc = gp->softc;
	return (g_eli_destroy(sc, FALSE));
}

static int
g_eli_keyfiles_load(struct hmac_ctx *ctx, const char *provider)
{
	u_char *keyfile, *data;
	char *file, name[64];
	size_t size;
	int i;

	for (i = 0; ; i++) {
		snprintf(name, sizeof(name), "%s:geli_keyfile%d", provider, i);
		keyfile = preload_search_by_type(name);
		if (keyfile == NULL && i == 0) {
			/*
			 * If there is only one keyfile, allow simpler name.
			 */
			snprintf(name, sizeof(name), "%s:geli_keyfile", provider);
			keyfile = preload_search_by_type(name);
		}
		if (keyfile == NULL)
			return (i);	/* Return number of loaded keyfiles. */
		data = preload_fetch_addr(keyfile);
		if (data == NULL) {
			G_ELI_DEBUG(0, "Cannot find key file data for %s.",
			    name);
			return (0);
		}
		size = preload_fetch_size(keyfile);
		if (size == 0) {
			G_ELI_DEBUG(0, "Cannot find key file size for %s.",
			    name);
			return (0);
		}
		file = preload_search_info(keyfile, MODINFO_NAME);
		if (file == NULL) {
			G_ELI_DEBUG(0, "Cannot find key file name for %s.",
			    name);
			return (0);
		}
		G_ELI_DEBUG(1, "Loaded keyfile %s for %s (type: %s).", file,
		    provider, name);
		g_eli_crypto_hmac_update(ctx, data, size);
	}
}

static void
g_eli_keyfiles_clear(const char *provider)
{
	u_char *keyfile, *data;
	char name[64];
	size_t size;
	int i;

	for (i = 0; ; i++) {
		snprintf(name, sizeof(name), "%s:geli_keyfile%d", provider, i);
		keyfile = preload_search_by_type(name);
		if (keyfile == NULL)
			return;
		data = preload_fetch_addr(keyfile);
		size = preload_fetch_size(keyfile);
		if (data != NULL && size != 0)
			explicit_bzero(data, size);
	}
}

/*
 * Tasting is only made on boot.
 * We detect providers which should be attached before root is mounted.
 */
static struct g_geom *
g_eli_taste(struct g_class *mp, struct g_provider *pp, int flags __unused)
{
	struct g_eli_metadata md;
	struct g_geom *gp;
	struct hmac_ctx ctx;
	char passphrase[256];
	u_char key[G_ELI_USERKEYLEN], mkey[G_ELI_DATAIVKEYLEN];
	u_int i, nkey, nkeyfiles, tries, showpass;
	int error;
	struct keybuf *keybuf;

	g_trace(G_T_TOPOLOGY, "%s(%s, %s)", __func__, mp->name, pp->name);
	g_topology_assert();

	if (root_mounted() || g_eli_tries == 0)
		return (NULL);

	G_ELI_DEBUG(3, "Tasting %s.", pp->name);

	error = g_eli_read_metadata(mp, pp, &md);
	if (error != 0)
		return (NULL);
	gp = NULL;

	if (strcmp(md.md_magic, G_ELI_MAGIC) != 0)
		return (NULL);
	if (md.md_version > G_ELI_VERSION) {
		printf("geom_eli.ko module is too old to handle %s.\n",
		    pp->name);
		return (NULL);
	}
	if (md.md_provsize != pp->mediasize)
		return (NULL);
	/* Should we attach it on boot? */
	if (!(md.md_flags & G_ELI_FLAG_BOOT) &&
	    !(md.md_flags & G_ELI_FLAG_GELIBOOT))
		return (NULL);
	if (md.md_keys == 0x00) {
		G_ELI_DEBUG(0, "No valid keys on %s.", pp->name);
		return (NULL);
	}
	if (!eli_metadata_crypto_supported(&md)) {
		G_ELI_DEBUG(0, "%s uses invalid or unsupported algorithms\n",
		    pp->name);
		return (NULL);
	}
	if (md.md_iterations == -1) {
		/* If there is no passphrase, we try only once. */
		tries = 1;
	} else {
		/* Ask for the passphrase no more than g_eli_tries times. */
		tries = g_eli_tries;
	}

	if ((keybuf = get_keybuf()) != NULL) {
		/* Scan the key buffer, try all GELI keys. */
		for (i = 0; i < keybuf->kb_nents; i++) {
			 if (keybuf->kb_ents[i].ke_type == KEYBUF_TYPE_GELI) {
				 memcpy(key, keybuf->kb_ents[i].ke_data,
				     sizeof(key));

				if (g_eli_mkey_decrypt_any(&md, key,
				    mkey, &nkey) == 0 ) {
					explicit_bzero(key, sizeof(key));
					goto have_key;
				}
			}
		}
	}

	for (i = 0; i <= tries; i++) {
		g_eli_crypto_hmac_init(&ctx, NULL, 0);

		/*
		 * Load all key files.
		 */
		nkeyfiles = g_eli_keyfiles_load(&ctx, pp->name);

		if (nkeyfiles == 0 && md.md_iterations == -1) {
			/*
			 * No key files and no passphrase, something is
			 * definitely wrong here.
			 * geli(8) doesn't allow for such situation, so assume
			 * that there was really no passphrase and in that case
			 * key files are no properly defined in loader.conf.
			 */
			G_ELI_DEBUG(0,
			    "Found no key files in loader.conf for %s.",
			    pp->name);
			return (NULL);
		}

		/* Ask for the passphrase if defined. */
		if (md.md_iterations >= 0) {
			/* Try first with cached passphrase. */
			if (i == 0) {
				if (!g_eli_boot_passcache)
					continue;
				memcpy(passphrase, cached_passphrase,
				    sizeof(passphrase));
			} else {
				printf("Enter passphrase for %s: ", pp->name);
				showpass = g_eli_visible_passphrase;
				if ((md.md_flags & G_ELI_FLAG_GELIDISPLAYPASS) != 0)
					showpass = GETS_ECHOPASS;
				cngets(passphrase, sizeof(passphrase),
				    showpass);
				memcpy(cached_passphrase, passphrase,
				    sizeof(passphrase));
			}
		}

		/*
		 * Prepare Derived-Key from the user passphrase.
		 */
		if (md.md_iterations == 0) {
			g_eli_crypto_hmac_update(&ctx, md.md_salt,
			    sizeof(md.md_salt));
			g_eli_crypto_hmac_update(&ctx, passphrase,
			    strlen(passphrase));
			explicit_bzero(passphrase, sizeof(passphrase));
		} else if (md.md_iterations > 0) {
			u_char dkey[G_ELI_USERKEYLEN];

			pkcs5v2_genkey(dkey, sizeof(dkey), md.md_salt,
			    sizeof(md.md_salt), passphrase, md.md_iterations);
			explicit_bzero(passphrase, sizeof(passphrase));
			g_eli_crypto_hmac_update(&ctx, dkey, sizeof(dkey));
			explicit_bzero(dkey, sizeof(dkey));
		}

		g_eli_crypto_hmac_final(&ctx, key, 0);

		/*
		 * Decrypt Master-Key.
		 */
		error = g_eli_mkey_decrypt_any(&md, key, mkey, &nkey);
		explicit_bzero(key, sizeof(key));
		if (error == -1) {
			if (i == tries) {
				G_ELI_DEBUG(0,
				    "Wrong key for %s. No tries left.",
				    pp->name);
				g_eli_keyfiles_clear(pp->name);
				return (NULL);
			}
			if (i > 0) {
				G_ELI_DEBUG(0,
				    "Wrong key for %s. Tries left: %u.",
				    pp->name, tries - i);
			}
			/* Try again. */
			continue;
		} else if (error > 0) {
			G_ELI_DEBUG(0,
			    "Cannot decrypt Master Key for %s (error=%d).",
			    pp->name, error);
			g_eli_keyfiles_clear(pp->name);
			return (NULL);
		}
		g_eli_keyfiles_clear(pp->name);
		G_ELI_DEBUG(1, "Using Master Key %u for %s.", nkey, pp->name);
		break;
	}
have_key:

	/*
	 * We have correct key, let's attach provider.
	 */
	gp = g_eli_create(NULL, mp, pp, &md, mkey, nkey);
	explicit_bzero(mkey, sizeof(mkey));
	explicit_bzero(&md, sizeof(md));
	if (gp == NULL) {
		G_ELI_DEBUG(0, "Cannot create device %s%s.", pp->name,
		    G_ELI_SUFFIX);
		return (NULL);
	}
	return (gp);
}

static void
g_eli_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp,
    struct g_consumer *cp, struct g_provider *pp)
{
	struct g_eli_softc *sc;

	g_topology_assert();
	sc = gp->softc;
	if (sc == NULL)
		return;
	if (pp != NULL || cp != NULL)
		return;	/* Nothing here. */

	sbuf_printf(sb, "%s<KeysTotal>%ju</KeysTotal>\n", indent,
	    (uintmax_t)sc->sc_ekeys_total);
	sbuf_printf(sb, "%s<KeysAllocated>%ju</KeysAllocated>\n", indent,
	    (uintmax_t)sc->sc_ekeys_allocated);
	sbuf_printf(sb, "%s<Flags>", indent);
	if (sc->sc_flags == 0)
		sbuf_cat(sb, "NONE");
	else {
		int first = 1;

#define ADD_FLAG(flag, name)	do {					\
	if (sc->sc_flags & (flag)) {					\
		if (!first)						\
			sbuf_cat(sb, ", ");				\
		else							\
			first = 0;					\
		sbuf_cat(sb, name);					\
	}								\
} while (0)
		ADD_FLAG(G_ELI_FLAG_SUSPEND, "SUSPEND");
		ADD_FLAG(G_ELI_FLAG_SINGLE_KEY, "SINGLE-KEY");
		ADD_FLAG(G_ELI_FLAG_NATIVE_BYTE_ORDER, "NATIVE-BYTE-ORDER");
		ADD_FLAG(G_ELI_FLAG_ONETIME, "ONETIME");
		ADD_FLAG(G_ELI_FLAG_BOOT, "BOOT");
		ADD_FLAG(G_ELI_FLAG_WO_DETACH, "W-DETACH");
		ADD_FLAG(G_ELI_FLAG_RW_DETACH, "RW-DETACH");
		ADD_FLAG(G_ELI_FLAG_AUTH, "AUTH");
		ADD_FLAG(G_ELI_FLAG_WOPEN, "W-OPEN");
		ADD_FLAG(G_ELI_FLAG_DESTROY, "DESTROY");
		ADD_FLAG(G_ELI_FLAG_RO, "READ-ONLY");
		ADD_FLAG(G_ELI_FLAG_NODELETE, "NODELETE");
		ADD_FLAG(G_ELI_FLAG_GELIBOOT, "GELIBOOT");
		ADD_FLAG(G_ELI_FLAG_GELIDISPLAYPASS, "GELIDISPLAYPASS");
		ADD_FLAG(G_ELI_FLAG_AUTORESIZE, "AUTORESIZE");
#undef  ADD_FLAG
	}
	sbuf_cat(sb, "</Flags>\n");

	if (!(sc->sc_flags & G_ELI_FLAG_ONETIME)) {
		sbuf_printf(sb, "%s<UsedKey>%u</UsedKey>\n", indent,
		    sc->sc_nkey);
	}
	sbuf_printf(sb, "%s<Version>%u</Version>\n", indent, sc->sc_version);
	sbuf_printf(sb, "%s<Crypto>", indent);
	switch (sc->sc_crypto) {
	case G_ELI_CRYPTO_HW:
		sbuf_cat(sb, "hardware");
		break;
	case G_ELI_CRYPTO_SW:
		sbuf_cat(sb, "software");
		break;
	case G_ELI_CRYPTO_SW_ACCEL:
		sbuf_cat(sb, "accelerated software");
		break;
	default:
		sbuf_cat(sb, "UNKNOWN");
		break;
	}
	sbuf_cat(sb, "</Crypto>\n");
	if (sc->sc_flags & G_ELI_FLAG_AUTH) {
		sbuf_printf(sb,
		    "%s<AuthenticationAlgorithm>%s</AuthenticationAlgorithm>\n",
		    indent, g_eli_algo2str(sc->sc_aalgo));
	}
	sbuf_printf(sb, "%s<KeyLength>%u</KeyLength>\n", indent,
	    sc->sc_ekeylen);
	sbuf_printf(sb, "%s<EncryptionAlgorithm>%s</EncryptionAlgorithm>\n",
	    indent, g_eli_algo2str(sc->sc_ealgo));
	sbuf_printf(sb, "%s<State>%s</State>\n", indent,
	    (sc->sc_flags & G_ELI_FLAG_SUSPEND) ? "SUSPENDED" : "ACTIVE");
}

static void
g_eli_shutdown_pre_sync(void *arg, int howto)
{
	struct g_class *mp;
	struct g_geom *gp, *gp2;
	struct g_provider *pp;
	struct g_eli_softc *sc;

	mp = arg;
	g_topology_lock();
	LIST_FOREACH_SAFE(gp, &mp->geom, geom, gp2) {
		sc = gp->softc;
		if (sc == NULL)
			continue;
		pp = LIST_FIRST(&gp->provider);
		KASSERT(pp != NULL, ("No provider? gp=%p (%s)", gp, gp->name));
		if (pp->acr != 0 || pp->acw != 0 || pp->ace != 0 ||
		    SCHEDULER_STOPPED())
		{
			sc->sc_flags |= G_ELI_FLAG_RW_DETACH;
			gp->access = g_eli_access;
		} else {
			(void) g_eli_destroy(sc, TRUE);
		}
	}
	g_topology_unlock();
}

static void
g_eli_init(struct g_class *mp)
{

	g_eli_pre_sync = EVENTHANDLER_REGISTER(shutdown_pre_sync,
	    g_eli_shutdown_pre_sync, mp, SHUTDOWN_PRI_FIRST);
	if (g_eli_pre_sync == NULL)
		G_ELI_DEBUG(0, "Warning! Cannot register shutdown event.");
}

static void
g_eli_fini(struct g_class *mp)
{

	if (g_eli_pre_sync != NULL)
		EVENTHANDLER_DEREGISTER(shutdown_pre_sync, g_eli_pre_sync);
}

DECLARE_GEOM_CLASS(g_eli_class, g_eli);
MODULE_DEPEND(g_eli, crypto, 1, 1, 1);
MODULE_VERSION(geom_eli, 0);
