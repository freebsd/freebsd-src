/*-
 * Copyright (c) 2005-2006 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
__FBSDID("$FreeBSD: src/sys/geom/eli/g_eli_privacy.c,v 1.1.8.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/linker.h>
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
#include <sys/vnode.h>

#include <vm/uma.h>

#include <geom/geom.h>
#include <geom/eli/g_eli.h>
#include <geom/eli/pkcs5v2.h>

/*
 * Code paths:
 * BIO_READ:
 *	g_eli_start -> g_io_request -> g_eli_read_done -> g_eli_crypto_run -> g_eli_crypto_read_done -> g_io_deliver
 * BIO_WRITE:
 *	g_eli_start -> g_eli_crypto_run -> g_eli_crypto_write_done -> g_io_request -> g_eli_write_done -> g_io_deliver
 */

MALLOC_DECLARE(M_ELI);

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
 * This is the main function responsible for cryptography (ie. communication
 * with crypto(9) subsystem).
 */
void
g_eli_crypto_run(struct g_eli_worker *wr, struct bio *bp)
{
	struct g_eli_softc *sc;
	struct cryptop *crp;
	struct cryptodesc *crd;
	struct uio *uio;
	struct iovec *iov;
	u_int i, nsec, add, secsize;
	int err, error;
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
	 * do the allocation once instead of allocating memory in pieces (many,
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
		if (g_eli_batch)
			crp->crp_flags |= CRYPTO_F_BATCH;
		crp->crp_desc = crd;

		crd->crd_skip = 0;
		crd->crd_len = secsize;
		crd->crd_flags = CRD_F_IV_EXPLICIT | CRD_F_IV_PRESENT;
		if (bp->bio_cmd == BIO_WRITE)
			crd->crd_flags |= CRD_F_ENCRYPT;
		crd->crd_alg = sc->sc_ealgo;
		crd->crd_key = sc->sc_ekey;
		crd->crd_klen = sc->sc_ekeylen;
		g_eli_crypto_ivgen(sc, bp->bio_offset + add, crd->crd_iv,
		    sizeof(crd->crd_iv));
		crd->crd_next = NULL;

		crp->crp_etype = 0;
		err = crypto_dispatch(crp);
		if (error == 0)
			error = err;
	}
	if (bp->bio_error == 0)
		bp->bio_error = error;
}
