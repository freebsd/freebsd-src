/*-
 * Copyright (c) 2005 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bio.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/kthread.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/uio.h>

#include <vm/uma.h>

#include <geom/geom.h>
#include <geom/eli/g_eli.h>
#include <geom/eli/pkcs5v2.h>


MALLOC_DEFINE(M_ELI, "eli data", "GEOM_ELI Data");

SYSCTL_DECL(_kern_geom);
SYSCTL_NODE(_kern_geom, OID_AUTO, eli, CTLFLAG_RW, 0, "GEOM_ELI stuff");
u_int g_eli_debug = 0;
TUNABLE_INT("kern.geom.eli.debug", &g_eli_debug);
SYSCTL_UINT(_kern_geom_eli, OID_AUTO, debug, CTLFLAG_RW, &g_eli_debug, 0,
    "Debug level");
static u_int g_eli_tries = 3;
TUNABLE_INT("kern.geom.eli.tries", &g_eli_tries);
SYSCTL_UINT(_kern_geom_eli, OID_AUTO, tries, CTLFLAG_RW, &g_eli_tries, 0,
    "Number of tries when asking for passphrase");
static u_int g_eli_visible_passphrase = 0;
TUNABLE_INT("kern.geom.eli.visible_passphrase", &g_eli_visible_passphrase);
SYSCTL_UINT(_kern_geom_eli, OID_AUTO, visible_passphrase, CTLFLAG_RW,
    &g_eli_visible_passphrase, 0,
    "Turn on echo when entering passphrase (debug purposes only!!)");
u_int g_eli_overwrites = 5;
SYSCTL_UINT(_kern_geom_eli, OID_AUTO, overwrites, CTLFLAG_RW, &g_eli_overwrites,
    0, "Number of overwrites on-disk keys when destroying");
static u_int g_eli_threads = 0;
TUNABLE_INT("kern.geom.eli.threads", &g_eli_threads);
SYSCTL_UINT(_kern_geom_eli, OID_AUTO, threads, CTLFLAG_RW, &g_eli_threads, 0,
    "Number of threads doing crypto work");

static int g_eli_do_taste = 0;

static int g_eli_destroy_geom(struct gctl_req *req, struct g_class *mp,
    struct g_geom *gp);
static void g_eli_crypto_run(struct g_eli_worker *wr, struct bio *bp);

static g_taste_t g_eli_taste;
static g_dumpconf_t g_eli_dumpconf;

struct g_class g_eli_class = {
	.name = G_ELI_CLASS_NAME,
	.version = G_VERSION,
	.ctlreq = g_eli_config,
	.taste = g_eli_taste,
	.destroy_geom = g_eli_destroy_geom
};


/*
 * Code paths:
 * BIO_READ:
 *	g_eli_start -> g_io_request -> g_eli_read_done -> g_eli_crypto_run -> g_eli_crypto_read_done -> g_io_deliver
 * BIO_WRITE:
 *	g_eli_start -> g_eli_crypto_run -> g_eli_crypto_write_done -> g_io_request -> g_eli_write_done -> g_io_deliver
 */


/*
 * EAGAIN from crypto(9) means, that we were probably balanced to another crypto
 * accelerator or something like this.
 * The function updates the SID and rerun the operation.
 */
static int
g_eli_crypto_rerun(struct cryptop *crp)
{
	struct g_eli_softc *sc;
	struct g_eli_worker *wr;
	struct bio *bp;
	int error;

	bp = (struct bio *)crp->crp_opaque;
	sc = bp->bio_to->geom->softc;
	LIST_FOREACH(wr, &sc->sc_workers, w_next) {
		if (wr->w_number == bp->bio_pflags)
			break;
	}
	KASSERT(wr != NULL, ("Invalid worker (%u).", bp->bio_pflags));
	G_ELI_DEBUG(1, "Reruning crypto %s request (sid: %ju -> %ju).",
	    bp->bio_cmd == BIO_READ ? "READ" : "WRITE", (uintmax_t)wr->w_sid,
	    (uintmax_t)crp->crp_sid);
	wr->w_sid = crp->crp_sid;
	crp->crp_etype = 0;
	error = crypto_dispatch(crp);
	if (error == 0)
		return (0);
	G_ELI_DEBUG(1, "%s: crypto_dispatch() returned %d.", __func__, error);
	crp->crp_etype = error;
	return (error);
}

/*
 * The function is called afer reading encrypted data from the provider.
 *
 * g_eli_start -> g_io_request -> G_ELI_READ_DONE -> g_eli_crypto_run -> g_eli_crypto_read_done -> g_io_deliver
 */
static void
g_eli_read_done(struct bio *bp)
{
	struct g_eli_softc *sc;
	struct bio *pbp;

	G_ELI_LOGREQ(2, bp, "Request done.");
	pbp = bp->bio_parent;
	if (pbp->bio_error == 0)
		pbp->bio_error = bp->bio_error;
	g_destroy_bio(bp);
	if (pbp->bio_error != 0) {
		G_ELI_LOGREQ(0, pbp, "%s() failed", __func__);
		pbp->bio_completed = 0;
		g_io_deliver(pbp, pbp->bio_error);
		return;
	}
	sc = pbp->bio_to->geom->softc;
	mtx_lock(&sc->sc_queue_mtx);
	bioq_insert_tail(&sc->sc_queue, pbp);
	mtx_unlock(&sc->sc_queue_mtx);
	wakeup(sc);
}

/*
 * The function is called after we read and decrypt data.
 *
 * g_eli_start -> g_io_request -> g_eli_read_done -> g_eli_crypto_run -> G_ELI_CRYPTO_READ_DONE -> g_io_deliver
 */
static int
g_eli_crypto_read_done(struct cryptop *crp)
{
	struct bio *bp;

	if (crp->crp_etype == EAGAIN) {
		if (g_eli_crypto_rerun(crp) == 0)
			return (0);
	}
	bp = (struct bio *)crp->crp_opaque;
	bp->bio_inbed++;
	if (crp->crp_etype == 0) {
		G_ELI_DEBUG(3, "Crypto READ request done (%d/%d).",
		    bp->bio_inbed, bp->bio_children);
		bp->bio_completed += crp->crp_olen;
	} else {
		G_ELI_DEBUG(1, "Crypto READ request failed (%d/%d) error=%d.",
		    bp->bio_inbed, bp->bio_children, crp->crp_etype);
		if (bp->bio_error == 0)
			bp->bio_error = crp->crp_etype;
	}
	/*
	 * Do we have all sectors already?
	 */
	if (bp->bio_inbed < bp->bio_children)
		return (0);
	free(bp->bio_driver2, M_ELI);
	bp->bio_driver2 = NULL;
	if (bp->bio_error != 0) {
		G_ELI_LOGREQ(0, bp, "Crypto READ request failed (error=%d).",
		    bp->bio_error);
		bp->bio_completed = 0;
	}
	/*
	 * Read is finished, send it up.
	 */
	g_io_deliver(bp, bp->bio_error);
	return (0);
}

/*
 * The function is called after we encrypt and write data.
 *
 * g_eli_start -> g_eli_crypto_run -> g_eli_crypto_write_done -> g_io_request -> G_ELI_WRITE_DONE -> g_io_deliver
 */
static void
g_eli_write_done(struct bio *bp)
{
	struct bio *pbp;

	G_ELI_LOGREQ(2, bp, "Request done.");
	pbp = bp->bio_parent;
	if (pbp->bio_error == 0)
		pbp->bio_error = bp->bio_error;
	free(pbp->bio_driver2, M_ELI);
	pbp->bio_driver2 = NULL;
	if (pbp->bio_error == 0)
		pbp->bio_completed = pbp->bio_length;
	else {
		G_ELI_LOGREQ(0, pbp, "Crypto READ request failed (error=%d).",
		    pbp->bio_error);
		pbp->bio_completed = 0;
	}
	g_destroy_bio(bp);
	/*
	 * Write is finished, send it up.
	 */
	g_io_deliver(pbp, pbp->bio_error);
}

/*
 * The function is called after data encryption.
 *
 * g_eli_start -> g_eli_crypto_run -> G_ELI_CRYPTO_WRITE_DONE -> g_io_request -> g_eli_write_done -> g_io_deliver
 */
static int
g_eli_crypto_write_done(struct cryptop *crp)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	struct bio *bp, *cbp;

	if (crp->crp_etype == EAGAIN) {
		if (g_eli_crypto_rerun(crp) == 0)
			return (0);
	}
	bp = (struct bio *)crp->crp_opaque;
	bp->bio_inbed++;
	if (crp->crp_etype == 0) {
		G_ELI_DEBUG(3, "Crypto WRITE request done (%d/%d).",
		    bp->bio_inbed, bp->bio_children);
	} else {
		G_ELI_DEBUG(1, "Crypto WRITE request failed (%d/%d) error=%d.",
		    bp->bio_inbed, bp->bio_children, crp->crp_etype);
		if (bp->bio_error == 0)
			bp->bio_error = crp->crp_etype;
	}
	/*
	 * All sectors are already encrypted?
	 */
	if (bp->bio_inbed < bp->bio_children)
		return (0);
	bp->bio_inbed = 0;
	bp->bio_children = 1;
	cbp = bp->bio_driver1;
	bp->bio_driver1 = NULL;
	if (bp->bio_error != 0) {
		G_ELI_LOGREQ(0, bp, "Crypto WRITE request failed (error=%d).",
		    bp->bio_error);
		free(bp->bio_driver2, M_ELI);
		bp->bio_driver2 = NULL;
		g_destroy_bio(cbp);
		g_io_deliver(bp, bp->bio_error);
		return (0);
	}
	cbp->bio_data = bp->bio_driver2;
	cbp->bio_done = g_eli_write_done;
	gp = bp->bio_to->geom;
	cp = LIST_FIRST(&gp->consumer);
	cbp->bio_to = cp->provider;
	G_ELI_LOGREQ(2, cbp, "Sending request.");
	/*
	 * Send encrypted data to the provider.
	 */
	g_io_request(cbp, cp);
	return (0);
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
	g_eli_destroy(sc, 1);
}

/*
 * BIO_READ : G_ELI_START -> g_io_request -> g_eli_read_done -> g_eli_crypto_run -> g_eli_crypto_read_done -> g_io_deliver
 * BIO_WRITE: G_ELI_START -> g_eli_crypto_run -> g_eli_crypto_write_done -> g_io_request -> g_eli_write_done -> g_io_deliver
 */
static void     
g_eli_start(struct bio *bp)
{       
	struct g_eli_softc *sc;
	struct bio *cbp;

	sc = bp->bio_to->geom->softc;
	KASSERT(sc != NULL,
	    ("Provider's error should be set (error=%d)(device=%s).",
	    bp->bio_to->error, bp->bio_to->name));
	G_ELI_LOGREQ(2, bp, "Request received.");

	switch (bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
		break;
	case BIO_DELETE:
		/*
		 * We could eventually support BIO_DELETE request.
		 * It could be done by overwritting requested sector with
		 * random data g_eli_overwrites number of times.
		 */
	case BIO_GETATTR:
	default:	
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	}
	cbp = g_clone_bio(bp);
	if (cbp == NULL) {
		g_io_deliver(bp, ENOMEM);
		return;
	}
	if (bp->bio_cmd == BIO_READ) {
		struct g_consumer *cp;

		cbp->bio_done = g_eli_read_done;
		cp = LIST_FIRST(&sc->sc_geom->consumer);
		cbp->bio_to = cp->provider;
		G_ELI_LOGREQ(2, bp, "Sending request.");
		/*
		 * Read encrypted data from provider.
		 */
		g_io_request(cbp, cp);
	} else /* if (bp->bio_cmd == BIO_WRITE) */ {
		bp->bio_driver1 = cbp;
		mtx_lock(&sc->sc_queue_mtx);
		bioq_insert_tail(&sc->sc_queue, bp);
		mtx_unlock(&sc->sc_queue_mtx);
		wakeup(sc);
	}
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

	wr = arg;
	sc = wr->w_softc;
	mtx_lock_spin(&sched_lock);
	sched_prio(curthread, PRIBIO);
	if (sc->sc_crypto == G_ELI_CRYPTO_SW && g_eli_threads == 0)
		sched_bind(curthread, wr->w_number);
	mtx_unlock_spin(&sched_lock);
 
	G_ELI_DEBUG(1, "Thread %s started.", curthread->td_proc->p_comm);

	for (;;) {
		mtx_lock(&sc->sc_queue_mtx);
		bp = bioq_takefirst(&sc->sc_queue);
		if (bp == NULL) {
			if ((sc->sc_flags & G_ELI_FLAG_DESTROY) != 0) {
				LIST_REMOVE(wr, w_next);
				crypto_freesession(wr->w_sid);
				free(wr, M_ELI);
				G_ELI_DEBUG(1, "Thread %s exiting.",
				    curthread->td_proc->p_comm);
				wakeup(&sc->sc_workers);
				mtx_unlock(&sc->sc_queue_mtx);
				kthread_exit(0);
			}
			msleep(sc, &sc->sc_queue_mtx, PRIBIO | PDROP,
			    "geli:w", 0);
			continue;
		}
		mtx_unlock(&sc->sc_queue_mtx);
		g_eli_crypto_run(wr, bp);
	}
}

/*
 * Here we generate IV. It is unique for every sector.
 */
static void
g_eli_crypto_ivgen(struct g_eli_softc *sc, off_t offset, u_char *iv,
    size_t size)
{
	u_char hash[SHA256_DIGEST_LENGTH];
	SHA256_CTX ctx;

	/* Copy precalculated SHA256 context for IV-Key. */
	bcopy(&sc->sc_ivctx, &ctx, sizeof(ctx));
	SHA256_Update(&ctx, (uint8_t *)&offset, sizeof(offset));
	SHA256_Final(hash, &ctx);
	bcopy(hash, iv, size);
}

/*
 * This is the main function responsible for cryptography (ie. communication
 * with crypto(9) subsystem).
 */
static void
g_eli_crypto_run(struct g_eli_worker *wr, struct bio *bp)
{
	struct g_eli_softc *sc;
	struct cryptop *crp;
	struct cryptodesc *crd;
	struct uio *uio;
	struct iovec *iov;
	u_int i, nsec, add, secsize;
	int err, error, flags;
	size_t size;
	u_char *p, *data;

	G_ELI_LOGREQ(3, bp, "%s", __func__);

	bp->bio_pflags = wr->w_number;
	sc = wr->w_softc;
	secsize = LIST_FIRST(&sc->sc_geom->provider)->sectorsize;
	nsec = bp->bio_length / secsize;

	/*
	 * Calculate how much memory do we need.
	 * We need separate crypto operation for every single sector.
	 * It is much faster to calculate total amount of needed memory here and
	 * do the allocation once insteaf of allocate memory in pieces (many,
	 * many pieces).
	 */
	size = sizeof(*crp) * nsec;
	size += sizeof(*crd) * nsec;
	size += sizeof(*uio) * nsec;
	size += sizeof(*iov) * nsec;
	/*
	 * If we write the data we cannot destroy current bio_data content,
	 * so we need to allocate more memory for encrypted data.
	 */
	if (bp->bio_cmd == BIO_WRITE)
		size += bp->bio_length;
	p = malloc(size, M_ELI, M_WAITOK);

	bp->bio_inbed = 0;
	bp->bio_children = nsec;
	bp->bio_driver2 = p;

	if (bp->bio_cmd == BIO_READ)
		data = bp->bio_data;
	else {
		data = p;
		p += bp->bio_length;
		bcopy(bp->bio_data, data, bp->bio_length);
	}

	error = 0;
	for (i = 0, add = 0; i < nsec; i++, add += secsize) {
		crp = (struct cryptop *)p;	p += sizeof(*crp);
		crd = (struct cryptodesc *)p;	p += sizeof(*crd);
		uio = (struct uio *)p;		p += sizeof(*uio);
		iov = (struct iovec *)p;	p += sizeof(*iov);

		iov->iov_len = secsize;
		iov->iov_base = data;
		data += secsize;

		uio->uio_iov = iov;
		uio->uio_iovcnt = 1;
		uio->uio_segflg = UIO_SYSSPACE;
		uio->uio_resid = secsize;

		crp->crp_sid = wr->w_sid;
		crp->crp_ilen = secsize;
		crp->crp_olen = secsize;
		crp->crp_opaque = (void *)bp;
		crp->crp_buf = (void *)uio;
		if (bp->bio_cmd == BIO_WRITE)
			crp->crp_callback = g_eli_crypto_write_done;
		else /* if (bp->bio_cmd == BIO_READ) */
			crp->crp_callback = g_eli_crypto_read_done;
		crp->crp_flags = CRYPTO_F_IOV | CRYPTO_F_CBIFSYNC | CRYPTO_F_REL;
		crp->crp_desc = crd;

		crd->crd_skip = 0;
		crd->crd_len = secsize;
		crd->crd_flags = flags;
		crd->crd_flags =
		    CRD_F_IV_EXPLICIT | CRD_F_IV_PRESENT | CRD_F_KEY_EXPLICIT;
		if (bp->bio_cmd == BIO_WRITE)
			crd->crd_flags |= CRD_F_ENCRYPT;
		crd->crd_alg = sc->sc_algo;
		crd->crd_key = sc->sc_datakey;
		crd->crd_klen = sc->sc_keylen;
		g_eli_crypto_ivgen(sc, bp->bio_offset + add, crd->crd_iv,
		    sizeof(crd->crd_iv));
		crd->crd_next = NULL;

		err = crypto_dispatch(crp);
		if (error == 0)
			error = err;
	}
	if (bp->bio_error == 0)
		bp->bio_error = error;
}

int
g_eli_read_metadata(struct g_class *mp, struct g_provider *pp,
    struct g_eli_metadata *md)
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
	error = g_attach(cp, pp);
	if (error != 0)
		goto end;
	error = g_access(cp, 1, 0, 0);
	if (error != 0)
		goto end;
	g_topology_unlock();
	buf = g_read_data(cp, pp->mediasize - pp->sectorsize, pp->sectorsize,
	    &error);
	g_topology_lock();
	if (error != 0)
		goto end;
	eli_metadata_decode(buf, md);
end:
	if (buf != NULL)
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

/*
 * The function is called when we had last close on provider and user requested
 * to close it when this situation occur.
 */
static void
g_eli_last_close(struct g_eli_softc *sc)
{
	struct g_geom *gp;
	struct g_provider *pp;
	char ppname[64];
	int error;

	g_topology_assert();
	gp = sc->sc_geom;
	pp = LIST_FIRST(&gp->provider);
	strlcpy(ppname, pp->name, sizeof(ppname));
	error = g_eli_destroy(sc, 1);
	KASSERT(error == 0, ("Cannot detach %s on last close (error=%d).",
	    ppname, error));
	G_ELI_DEBUG(0, "Detached %s on last close.", ppname);
}

int
g_eli_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_eli_softc *sc;
	struct g_geom *gp;

	gp = pp->geom;
	sc = gp->softc;

	if (dw > 0) {
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
		g_eli_last_close(sc);
	}
	return (0);
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
	struct cryptoini cri;
	u_int i, threads;
	int error;

	G_ELI_DEBUG(1, "Creating device %s%s.", bpp->name, G_ELI_SUFFIX);

	gp = g_new_geomf(mp, "%s%s", bpp->name, G_ELI_SUFFIX);
	gp->softc = NULL;	/* for a moment */

	sc = malloc(sizeof(*sc), M_ELI, M_WAITOK | M_ZERO);
	gp->start = g_eli_start;
	/*
	 * Spoiling cannot happen actually, because we keep provider open for
	 * writing all the time.
	 */
	gp->spoiled = g_eli_orphan_spoil_assert;
	gp->orphan = g_eli_orphan;
	/*
	 * If detach-on-last-close feature is not enabled, we can simply use
	 * g_std_access().
	 */
	if (md->md_flags & G_ELI_FLAG_WO_DETACH)
		gp->access = g_eli_access;
	else
		gp->access = g_std_access;
	gp->dumpconf = g_eli_dumpconf;

	sc->sc_crypto = G_ELI_CRYPTO_SW;
	sc->sc_flags = md->md_flags;
	sc->sc_algo = md->md_algo;
	sc->sc_nkey = nkey;
	/*
	 * Remember the keys in our softc structure.
	 */
	bcopy(mkey, sc->sc_ivkey, sizeof(sc->sc_ivkey));
	mkey += sizeof(sc->sc_ivkey);
	bcopy(mkey, sc->sc_datakey, sizeof(sc->sc_datakey));
	sc->sc_keylen = md->md_keylen;

	/*
	 * Precalculate SHA256 for IV generation.
	 * This is expensive operation and we can do it only once now or for
	 * every access to sector, so now will be much better.
	 */
	SHA256_Init(&sc->sc_ivctx);
	SHA256_Update(&sc->sc_ivctx, sc->sc_ivkey, sizeof(sc->sc_ivkey));

	gp->softc = sc;
	sc->sc_geom = gp;

	bioq_init(&sc->sc_queue);
	mtx_init(&sc->sc_queue_mtx, "geli:queue", NULL, MTX_DEF);

	pp = NULL;
	cp = g_new_consumer(gp);
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
	 */
	error = g_access(cp, 1, 1, 1);
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

	LIST_INIT(&sc->sc_workers);

	bzero(&cri, sizeof(cri));
	cri.cri_alg = sc->sc_algo;
	cri.cri_klen = sc->sc_keylen;
	cri.cri_key = sc->sc_datakey;

	threads = g_eli_threads;
	if (threads == 0)
		threads = mp_ncpus;
	else if (threads > mp_ncpus) {
		/* There is really no need for too many worker threads. */
		threads = mp_ncpus;
		G_ELI_DEBUG(0, "Reducing number of threads to %u.", threads);
	}
	for (i = 0; i < threads; i++) {
		wr = malloc(sizeof(*wr), M_ELI, M_WAITOK | M_ZERO);
		wr->w_softc = sc;
		wr->w_number = i;

		/*
		 * If this is the first pass, try to get hardware support.
		 * Use software cryptography, if we cannot get it.
		 */
		if (i == 0) {
			error = crypto_newsession(&wr->w_sid, &cri, 1);
			if (error == 0)
				sc->sc_crypto = G_ELI_CRYPTO_HW;
		}
		if (sc->sc_crypto == G_ELI_CRYPTO_SW)
			error = crypto_newsession(&wr->w_sid, &cri, 0);
		if (error != 0) {
			free(wr, M_ELI);
			if (req != NULL) {
				gctl_error(req, "Cannot setup crypto session "
				    "for %s (error=%d).", bpp->name, error);
			} else {
				G_ELI_DEBUG(1, "Cannot setup crypto session "
				    "for %s (error=%d).", bpp->name, error);
			}
			goto failed;
		}

		error = kthread_create(g_eli_worker, wr, &wr->w_proc, 0, 0,
		    "g_eli[%u] %s", i, bpp->name);
		if (error != 0) {
			crypto_freesession(wr->w_sid);
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
		/* If we have hardware support, one thread is enough. */
		if (sc->sc_crypto == G_ELI_CRYPTO_HW)
			break;
	}

	/*
	 * Create decrypted provider.
	 */
	pp = g_new_providerf(gp, "%s%s", bpp->name, G_ELI_SUFFIX);
	pp->sectorsize = md->md_sectorsize;
	pp->mediasize = bpp->mediasize;
	if ((sc->sc_flags & G_ELI_FLAG_ONETIME) == 0)
		pp->mediasize -= bpp->sectorsize;
	pp->mediasize -= (pp->mediasize % pp->sectorsize);
	g_error_provider(pp, 0);

	G_ELI_DEBUG(0, "Device %s created.", pp->name);
	G_ELI_DEBUG(0, "    Cipher: %s", g_eli_algo2str(sc->sc_algo));
	G_ELI_DEBUG(0, "Key length: %u", sc->sc_keylen);
	G_ELI_DEBUG(0, "    Crypto: %s",
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
			g_access(cp, -1, -1, -1);
		g_detach(cp);
	}
	g_destroy_consumer(cp);
	if (pp != NULL)
		g_destroy_provider(pp);
	g_destroy_geom(gp);
	bzero(sc, sizeof(*sc));
	free(sc, M_ELI);
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
			    "can't be definitely removed.", pp->name);
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
	bzero(sc, sizeof(*sc));
	free(sc, M_ELI);

	if (pp == NULL || (pp->acr == 0 && pp->acw == 0 && pp->ace == 0))
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
	return (g_eli_destroy(sc, 0));
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
	u_int nkey, i;
	int error;

	g_trace(G_T_TOPOLOGY, "%s(%s, %s)", __func__, mp->name, pp->name);
	g_topology_assert();

	if (!g_eli_do_taste || g_eli_tries == 0)
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
	if ((md.md_flags & G_ELI_FLAG_BOOT) == 0)
		return (NULL);
	if (md.md_keys == 0x00) {
		G_ELI_DEBUG(0, "No valid keys on %s.", pp->name);
		return (NULL);
	}

	/*
	 * Ask for the passphrase no more than g_eli_tries times.
	 */
	for (i = 0; i < g_eli_tries; i++) {
		printf("Enter passphrase for %s: ", pp->name);
		gets(passphrase, sizeof(passphrase), g_eli_visible_passphrase);
		KASSERT(md.md_iterations >= 0, ("md_iterations = %d for %s",
		    (int)md.md_iterations, pp->name));
		/*
		 * Prepare Derived-Key from the user passphrase.
		 */
		g_eli_crypto_hmac_init(&ctx, NULL, 0);
		if (md.md_iterations == 0) {
			g_eli_crypto_hmac_update(&ctx, md.md_salt,
			    sizeof(md.md_salt));
			g_eli_crypto_hmac_update(&ctx, passphrase,
			    strlen(passphrase));
		} else {
			u_char dkey[G_ELI_USERKEYLEN];

			pkcs5v2_genkey(dkey, sizeof(dkey), md.md_salt,
			    sizeof(md.md_salt), passphrase, md.md_iterations);
			g_eli_crypto_hmac_update(&ctx, dkey, sizeof(dkey));
			bzero(dkey, sizeof(dkey));
		}
		g_eli_crypto_hmac_final(&ctx, key, 0);

		/*
		 * Decrypt Master-Key.
		 */
		error = g_eli_mkey_decrypt(&md, key, mkey, &nkey);
		bzero(key, sizeof(key));
		if (error == -1) {
			if (i == g_eli_tries - 1) {
				i++;
				break;
			}
			G_ELI_DEBUG(0, "Wrong key for %s. Tries left: %u.",
			    pp->name, g_eli_tries - i - 1);
			/* Try again. */
			continue;
		} else if (error > 0) {
			G_ELI_DEBUG(0, "Cannot decrypt Master Key for %s (error=%d).",
			    pp->name, error);
			return (NULL);
		}
		G_ELI_DEBUG(1, "Using Master Key %u for %s.", nkey, pp->name);
		break;
	}
	if (i == g_eli_tries) {
		G_ELI_DEBUG(0, "Wrong key for %s. No tries left.", pp->name);
		return (NULL);
	}

	/*
	 * We have correct key, let's attach provider.
	 */
	gp = g_eli_create(NULL, mp, pp, &md, mkey, nkey);
	bzero(mkey, sizeof(mkey));
	bzero(&md, sizeof(md));
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
	sbuf_printf(sb, "%s<Flags>", indent);
	if (sc->sc_flags == 0)
		sbuf_printf(sb, "NONE");
	else {
		int first = 1;

#define ADD_FLAG(flag, name)	do {					\
	if ((sc->sc_flags & (flag)) != 0) {				\
		if (!first)						\
			sbuf_printf(sb, ", ");				\
		else							\
			first = 0;					\
		sbuf_printf(sb, name);					\
	}								\
} while (0)
		ADD_FLAG(G_ELI_FLAG_ONETIME, "ONETIME");
		ADD_FLAG(G_ELI_FLAG_BOOT, "BOOT");
		ADD_FLAG(G_ELI_FLAG_WO_DETACH, "W-DETACH");
		ADD_FLAG(G_ELI_FLAG_RW_DETACH, "RW-DETACH");
		ADD_FLAG(G_ELI_FLAG_WOPEN, "W-OPEN");
		ADD_FLAG(G_ELI_FLAG_DESTROY, "DESTROY");
#undef  ADD_FLAG
	}
	sbuf_printf(sb, "</Flags>\n");

	if ((sc->sc_flags & G_ELI_FLAG_ONETIME) == 0) {
		sbuf_printf(sb, "%s<UsedKey>%u</UsedKey>\n", indent,
		    sc->sc_nkey);
	}
	sbuf_printf(sb, "%s<Crypto>", indent);
	switch (sc->sc_crypto) {
	case G_ELI_CRYPTO_HW:
		sbuf_printf(sb, "hardware");
		break;
	case G_ELI_CRYPTO_SW:
		sbuf_printf(sb, "software");
		break;
	default:
		sbuf_printf(sb, "UNKNOWN");
		break;
	}
	sbuf_printf(sb, "</Crypto>\n");
	sbuf_printf(sb, "%s<KeyLength>%u</KeyLength>\n", indent, sc->sc_keylen);
	sbuf_printf(sb, "%s<Cipher>%s</Cipher>\n", indent,
	    g_eli_algo2str(sc->sc_algo));
}

static void
g_eli_on_boot_start(void *dummy __unused)
{

	/* This prevents from tasting when module is loaded after boot. */
	if (cold) {
		G_ELI_DEBUG(1, "Start tasting.");
		g_eli_do_taste = 1;
	} else {
		G_ELI_DEBUG(1, "Tasting not started.");
	}
}
SYSINIT(geli_boot_start, SI_SUB_TUNABLES, SI_ORDER_ANY, g_eli_on_boot_start, NULL)

static void
g_eli_on_boot_end(void *dummy __unused)
{

	if (g_eli_do_taste) {
		G_ELI_DEBUG(1, "Tasting no more.");
		g_eli_do_taste = 0;
	}
}
SYSINIT(geli_boot_end, SI_SUB_RUN_SCHEDULER, SI_ORDER_ANY, g_eli_on_boot_end, NULL)

DECLARE_GEOM_CLASS(g_eli_class, g_eli);
MODULE_DEPEND(geom_eli, crypto, 1, 1, 1);
