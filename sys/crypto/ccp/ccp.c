/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2017 Chelsio Communications, Inc.
 * Copyright (c) 2017 Conrad Meyer <cem@FreeBSD.org>
 * All rights reserved.
 * Largely borrowed from ccr(4), Written by: John Baldwin <jhb@FreeBSD.org>
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
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/random.h>
#include <sys/sglist.h>
#include <sys/sysctl.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <dev/pci/pcivar.h>

#include <dev/random/randomdev.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform.h>

#include "cryptodev_if.h"

#include "ccp.h"
#include "ccp_hardware.h"

MALLOC_DEFINE(M_CCP, "ccp", "AMD CCP crypto");

/*
 * Need a global softc available for garbage random_source API, which lacks any
 * context pointer.  It's also handy for debugging.
 */
struct ccp_softc *g_ccp_softc;

bool g_debug_print = false;
SYSCTL_BOOL(_hw_ccp, OID_AUTO, debug, CTLFLAG_RWTUN, &g_debug_print, 0,
    "Set to enable debugging log messages");

static struct pciid {
	uint32_t devid;
	const char *desc;
} ccp_ids[] = {
	{ 0x14561022, "AMD CCP-5a" },
	{ 0x14681022, "AMD CCP-5b" },
	{ 0x15df1022, "AMD CCP-5a" },
};

static struct random_source random_ccp = {
	.rs_ident = "AMD CCP TRNG",
	.rs_source = RANDOM_PURE_CCP,
	.rs_read = random_ccp_read,
};

/*
 * ccp_populate_sglist() generates a scatter/gather list that covers the entire
 * crypto operation buffer.
 */
static int
ccp_populate_sglist(struct sglist *sg, struct crypto_buffer *cb)
{
	int error;

	sglist_reset(sg);
	switch (cb->cb_type) {
	case CRYPTO_BUF_MBUF:
		error = sglist_append_mbuf(sg, cb->cb_mbuf);
		break;
	case CRYPTO_BUF_SINGLE_MBUF:
		error = sglist_append_single_mbuf(sg, cb->cb_mbuf);
		break;
	case CRYPTO_BUF_UIO:
		error = sglist_append_uio(sg, cb->cb_uio);
		break;
	case CRYPTO_BUF_CONTIG:
		error = sglist_append(sg, cb->cb_buf, cb->cb_buf_len);
		break;
	case CRYPTO_BUF_VMPAGE:
		error = sglist_append_vmpages(sg, cb->cb_vm_page,
		    cb->cb_vm_page_offset, cb->cb_vm_page_len);
		break;
	default:
		error = EINVAL;
	}
	return (error);
}

static int
ccp_probe(device_t dev)
{
	struct pciid *ip;
	uint32_t id;

	id = pci_get_devid(dev);
	for (ip = ccp_ids; ip < &ccp_ids[nitems(ccp_ids)]; ip++) {
		if (id == ip->devid) {
			device_set_desc(dev, ip->desc);
			return (0);
		}
	}
	return (ENXIO);
}

static void
ccp_initialize_queues(struct ccp_softc *sc)
{
	struct ccp_queue *qp;
	size_t i;

	for (i = 0; i < nitems(sc->queues); i++) {
		qp = &sc->queues[i];

		qp->cq_softc = sc;
		qp->cq_qindex = i;
		mtx_init(&qp->cq_lock, "ccp queue", NULL, MTX_DEF);
		/* XXX - arbitrarily chosen sizes */
		qp->cq_sg_crp = sglist_alloc(32, M_WAITOK);
		/* Two more SGEs than sg_crp to accommodate ipad. */
		qp->cq_sg_ulptx = sglist_alloc(34, M_WAITOK);
		qp->cq_sg_dst = sglist_alloc(2, M_WAITOK);
	}
}

static void
ccp_free_queues(struct ccp_softc *sc)
{
	struct ccp_queue *qp;
	size_t i;

	for (i = 0; i < nitems(sc->queues); i++) {
		qp = &sc->queues[i];

		mtx_destroy(&qp->cq_lock);
		sglist_free(qp->cq_sg_crp);
		sglist_free(qp->cq_sg_ulptx);
		sglist_free(qp->cq_sg_dst);
	}
}

static int
ccp_attach(device_t dev)
{
	struct ccp_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;

	sc->cid = crypto_get_driverid(dev, sizeof(struct ccp_session),
	    CRYPTOCAP_F_HARDWARE);
	if (sc->cid < 0) {
		device_printf(dev, "could not get crypto driver id\n");
		return (ENXIO);
	}

	error = ccp_hw_attach(dev);
	if (error != 0)
		return (error);

	mtx_init(&sc->lock, "ccp", NULL, MTX_DEF);

	ccp_initialize_queues(sc);

	if (g_ccp_softc == NULL) {
		g_ccp_softc = sc;
		if ((sc->hw_features & VERSION_CAP_TRNG) != 0)
			random_source_register(&random_ccp);
	}

	return (0);
}

static int
ccp_detach(device_t dev)
{
	struct ccp_softc *sc;

	sc = device_get_softc(dev);

	mtx_lock(&sc->lock);
	sc->detaching = true;
	mtx_unlock(&sc->lock);

	crypto_unregister_all(sc->cid);
	if (g_ccp_softc == sc && (sc->hw_features & VERSION_CAP_TRNG) != 0)
		random_source_deregister(&random_ccp);

	ccp_hw_detach(dev);
	ccp_free_queues(sc);

	if (g_ccp_softc == sc)
		g_ccp_softc = NULL;

	mtx_destroy(&sc->lock);
	return (0);
}

static void
ccp_init_hmac_digest(struct ccp_session *s, const char *key, int klen)
{
	union authctx auth_ctx;
	const struct auth_hash *axf;
	u_int i;

	/*
	 * If the key is larger than the block size, use the digest of
	 * the key as the key instead.
	 */
	axf = s->hmac.auth_hash;
	if (klen > axf->blocksize) {
		axf->Init(&auth_ctx);
		axf->Update(&auth_ctx, key, klen);
		axf->Final(s->hmac.ipad, &auth_ctx);
		explicit_bzero(&auth_ctx, sizeof(auth_ctx));
		klen = axf->hashsize;
	} else
		memcpy(s->hmac.ipad, key, klen);

	memset(s->hmac.ipad + klen, 0, axf->blocksize - klen);
	memcpy(s->hmac.opad, s->hmac.ipad, axf->blocksize);

	for (i = 0; i < axf->blocksize; i++) {
		s->hmac.ipad[i] ^= HMAC_IPAD_VAL;
		s->hmac.opad[i] ^= HMAC_OPAD_VAL;
	}
}

static bool
ccp_aes_check_keylen(int alg, int klen)
{

	switch (klen * 8) {
	case 128:
	case 192:
		if (alg == CRYPTO_AES_XTS)
			return (false);
		break;
	case 256:
		break;
	case 512:
		if (alg != CRYPTO_AES_XTS)
			return (false);
		break;
	default:
		return (false);
	}
	return (true);
}

static void
ccp_aes_setkey(struct ccp_session *s, int alg, const void *key, int klen)
{
	unsigned kbits;

	if (alg == CRYPTO_AES_XTS)
		kbits = (klen / 2) * 8;
	else
		kbits = klen * 8;

	switch (kbits) {
	case 128:
		s->blkcipher.cipher_type = CCP_AES_TYPE_128;
		break;
	case 192:
		s->blkcipher.cipher_type = CCP_AES_TYPE_192;
		break;
	case 256:
		s->blkcipher.cipher_type = CCP_AES_TYPE_256;
		break;
	default:
		panic("should not get here");
	}

	s->blkcipher.key_len = klen;
	memcpy(s->blkcipher.enckey, key, s->blkcipher.key_len);
}

static bool
ccp_auth_supported(struct ccp_softc *sc,
    const struct crypto_session_params *csp)
{
	
	if ((sc->hw_features & VERSION_CAP_SHA) == 0)
		return (false);
	switch (csp->csp_auth_alg) {
	case CRYPTO_SHA1_HMAC:
	case CRYPTO_SHA2_256_HMAC:
	case CRYPTO_SHA2_384_HMAC:
	case CRYPTO_SHA2_512_HMAC:
		if (csp->csp_auth_key == NULL)
			return (false);
		break;
	default:
		return (false);
	}
	return (true);
}

static bool
ccp_cipher_supported(struct ccp_softc *sc,
    const struct crypto_session_params *csp)
{

	if ((sc->hw_features & VERSION_CAP_AES) == 0)
		return (false);
	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_CBC:
		if (csp->csp_ivlen != AES_BLOCK_LEN)
			return (false);
		break;
	case CRYPTO_AES_ICM:
		if (csp->csp_ivlen != AES_BLOCK_LEN)
			return (false);
		break;
	case CRYPTO_AES_XTS:
		if (csp->csp_ivlen != AES_XTS_IV_LEN)
			return (false);
		break;
	default:
		return (false);
	}
	return (ccp_aes_check_keylen(csp->csp_cipher_alg,
	    csp->csp_cipher_klen));
}

static int
ccp_probesession(device_t dev, const struct crypto_session_params *csp)
{
	struct ccp_softc *sc;

	if (csp->csp_flags != 0)
		return (EINVAL);
	sc = device_get_softc(dev);
	switch (csp->csp_mode) {
	case CSP_MODE_DIGEST:
		if (!ccp_auth_supported(sc, csp))
			return (EINVAL);
		break;
	case CSP_MODE_CIPHER:
		if (!ccp_cipher_supported(sc, csp))
			return (EINVAL);
		break;
	case CSP_MODE_AEAD:
		switch (csp->csp_cipher_alg) {
		case CRYPTO_AES_NIST_GCM_16:
			if ((sc->hw_features & VERSION_CAP_AES) == 0)
				return (EINVAL);
			break;
		default:
			return (EINVAL);
		}
		break;
	case CSP_MODE_ETA:
		if (!ccp_auth_supported(sc, csp) ||
		    !ccp_cipher_supported(sc, csp))
			return (EINVAL);
		break;
	default:
		return (EINVAL);
	}

	return (CRYPTODEV_PROBE_HARDWARE);
}

static int
ccp_newsession(device_t dev, crypto_session_t cses,
    const struct crypto_session_params *csp)
{
	struct ccp_softc *sc;
	struct ccp_session *s;
	const struct auth_hash *auth_hash;
	enum ccp_aes_mode cipher_mode;
	unsigned auth_mode;
	unsigned q;

	/* XXX reconcile auth_mode with use by ccp_sha */
	switch (csp->csp_auth_alg) {
	case CRYPTO_SHA1_HMAC:
		auth_hash = &auth_hash_hmac_sha1;
		auth_mode = SHA1;
		break;
	case CRYPTO_SHA2_256_HMAC:
		auth_hash = &auth_hash_hmac_sha2_256;
		auth_mode = SHA2_256;
		break;
	case CRYPTO_SHA2_384_HMAC:
		auth_hash = &auth_hash_hmac_sha2_384;
		auth_mode = SHA2_384;
		break;
	case CRYPTO_SHA2_512_HMAC:
		auth_hash = &auth_hash_hmac_sha2_512;
		auth_mode = SHA2_512;
		break;
	default:
		auth_hash = NULL;
		auth_mode = 0;
		break;
	}

	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_CBC:
		cipher_mode = CCP_AES_MODE_CBC;
		break;
	case CRYPTO_AES_ICM:
		cipher_mode = CCP_AES_MODE_CTR;
		break;
	case CRYPTO_AES_NIST_GCM_16:
		cipher_mode = CCP_AES_MODE_GCTR;
		break;
	case CRYPTO_AES_XTS:
		cipher_mode = CCP_AES_MODE_XTS;
		break;
	default:
		cipher_mode = CCP_AES_MODE_ECB;
		break;
	}

	sc = device_get_softc(dev);
	mtx_lock(&sc->lock);
	if (sc->detaching) {
		mtx_unlock(&sc->lock);
		return (ENXIO);
	}

	s = crypto_get_driver_session(cses);

	/* Just grab the first usable queue for now. */
	for (q = 0; q < nitems(sc->queues); q++)
		if ((sc->valid_queues & (1 << q)) != 0)
			break;
	if (q == nitems(sc->queues)) {
		mtx_unlock(&sc->lock);
		return (ENXIO);
	}
	s->queue = q;

	switch (csp->csp_mode) {
	case CSP_MODE_AEAD:
		s->mode = GCM;
		break;
	case CSP_MODE_ETA:
		s->mode = AUTHENC;
		break;
	case CSP_MODE_DIGEST:
		s->mode = HMAC;
		break;
	case CSP_MODE_CIPHER:
		s->mode = BLKCIPHER;
		break;
	}

	if (s->mode == GCM) {
		if (csp->csp_auth_mlen == 0)
			s->gmac.hash_len = AES_GMAC_HASH_LEN;
		else
			s->gmac.hash_len = csp->csp_auth_mlen;
	} else if (auth_hash != NULL) {
		s->hmac.auth_hash = auth_hash;
		s->hmac.auth_mode = auth_mode;
		if (csp->csp_auth_mlen == 0)
			s->hmac.hash_len = auth_hash->hashsize;
		else
			s->hmac.hash_len = csp->csp_auth_mlen;
		ccp_init_hmac_digest(s, csp->csp_auth_key, csp->csp_auth_klen);
	}
	if (cipher_mode != CCP_AES_MODE_ECB) {
		s->blkcipher.cipher_mode = cipher_mode;
		if (csp->csp_cipher_key != NULL)
			ccp_aes_setkey(s, csp->csp_cipher_alg,
			    csp->csp_cipher_key, csp->csp_cipher_klen);
	}

	s->active = true;
	mtx_unlock(&sc->lock);

	return (0);
}

static void
ccp_freesession(device_t dev, crypto_session_t cses)
{
	struct ccp_session *s;

	s = crypto_get_driver_session(cses);

	if (s->pending != 0)
		device_printf(dev,
		    "session %p freed with %d pending requests\n", s,
		    s->pending);
	s->active = false;
}

static int
ccp_process(device_t dev, struct cryptop *crp, int hint)
{
	const struct crypto_session_params *csp;
	struct ccp_softc *sc;
	struct ccp_queue *qp;
	struct ccp_session *s;
	int error;
	bool qpheld;

	qpheld = false;
	qp = NULL;

	csp = crypto_get_params(crp->crp_session);
	s = crypto_get_driver_session(crp->crp_session);
	sc = device_get_softc(dev);
	mtx_lock(&sc->lock);
	qp = &sc->queues[s->queue];
	mtx_unlock(&sc->lock);
	error = ccp_queue_acquire_reserve(qp, 1 /* placeholder */, M_NOWAIT);
	if (error != 0)
		goto out;
	qpheld = true;

	error = ccp_populate_sglist(qp->cq_sg_crp, &crp->crp_buf);
	if (error != 0)
		goto out;

	if (crp->crp_auth_key != NULL) {
		KASSERT(s->hmac.auth_hash != NULL, ("auth key without HMAC"));
		ccp_init_hmac_digest(s, crp->crp_auth_key, csp->csp_auth_klen);
	}
	if (crp->crp_cipher_key != NULL)
		ccp_aes_setkey(s, csp->csp_cipher_alg, crp->crp_cipher_key,
		    csp->csp_cipher_klen);

	switch (s->mode) {
	case HMAC:
		if (s->pending != 0) {
			error = EAGAIN;
			break;
		}
		error = ccp_hmac(qp, s, crp);
		break;
	case BLKCIPHER:
		if (s->pending != 0) {
			error = EAGAIN;
			break;
		}
		error = ccp_blkcipher(qp, s, crp);
		break;
	case AUTHENC:
		if (s->pending != 0) {
			error = EAGAIN;
			break;
		}
		error = ccp_authenc(qp, s, crp);
		break;
	case GCM:
		if (s->pending != 0) {
			error = EAGAIN;
			break;
		}
		error = ccp_gcm(qp, s, crp);
		break;
	}

	if (error == 0)
		s->pending++;

out:
	if (qpheld) {
		if (error != 0) {
			/*
			 * Squash EAGAIN so callers don't uselessly and
			 * expensively retry if the ring was full.
			 */
			if (error == EAGAIN)
				error = ENOMEM;
			ccp_queue_abort(qp);
		} else
			ccp_queue_release(qp);
	}

	if (error != 0) {
		DPRINTF(dev, "%s: early error:%d\n", __func__, error);
		crp->crp_etype = error;
		crypto_done(crp);
	}
	return (0);
}

static device_method_t ccp_methods[] = {
	DEVMETHOD(device_probe,		ccp_probe),
	DEVMETHOD(device_attach,	ccp_attach),
	DEVMETHOD(device_detach,	ccp_detach),

	DEVMETHOD(cryptodev_probesession, ccp_probesession),
	DEVMETHOD(cryptodev_newsession,	ccp_newsession),
	DEVMETHOD(cryptodev_freesession, ccp_freesession),
	DEVMETHOD(cryptodev_process,	ccp_process),

	DEVMETHOD_END
};

static driver_t ccp_driver = {
	"ccp",
	ccp_methods,
	sizeof(struct ccp_softc)
};

DRIVER_MODULE(ccp, pci, ccp_driver, NULL, NULL);
MODULE_VERSION(ccp, 1);
MODULE_DEPEND(ccp, crypto, 1, 1, 1);
MODULE_DEPEND(ccp, random_device, 1, 1, 1);
#if 0	/* There are enough known issues that we shouldn't load automatically */
MODULE_PNP_INFO("W32:vendor/device", pci, ccp, ccp_ids,
    nitems(ccp_ids));
#endif

static int
ccp_queue_reserve_space(struct ccp_queue *qp, unsigned n, int mflags)
{
	struct ccp_softc *sc;

	mtx_assert(&qp->cq_lock, MA_OWNED);
	sc = qp->cq_softc;

	if (n < 1 || n >= (1 << sc->ring_size_order))
		return (EINVAL);

	while (true) {
		if (ccp_queue_get_ring_space(qp) >= n)
			return (0);
		if ((mflags & M_WAITOK) == 0)
			return (EAGAIN);
		qp->cq_waiting = true;
		msleep(&qp->cq_tail, &qp->cq_lock, 0, "ccpqfull", 0);
	}
}

int
ccp_queue_acquire_reserve(struct ccp_queue *qp, unsigned n, int mflags)
{
	int error;

	mtx_lock(&qp->cq_lock);
	qp->cq_acq_tail = qp->cq_tail;
	error = ccp_queue_reserve_space(qp, n, mflags);
	if (error != 0)
		mtx_unlock(&qp->cq_lock);
	return (error);
}

void
ccp_queue_release(struct ccp_queue *qp)
{

	mtx_assert(&qp->cq_lock, MA_OWNED);
	if (qp->cq_tail != qp->cq_acq_tail) {
		wmb();
		ccp_queue_write_tail(qp);
	}
	mtx_unlock(&qp->cq_lock);
}

void
ccp_queue_abort(struct ccp_queue *qp)
{
	unsigned i;

	mtx_assert(&qp->cq_lock, MA_OWNED);

	/* Wipe out any descriptors associated with this aborted txn. */
	for (i = qp->cq_acq_tail; i != qp->cq_tail;
	    i = (i + 1) % (1 << qp->cq_softc->ring_size_order)) {
		memset(&qp->desc_ring[i], 0, sizeof(qp->desc_ring[i]));
	}
	qp->cq_tail = qp->cq_acq_tail;

	mtx_unlock(&qp->cq_lock);
}

#ifdef DDB
#define	_db_show_lock(lo)	LOCK_CLASS(lo)->lc_ddb_show(lo)
#define	db_show_lock(lk)	_db_show_lock(&(lk)->lock_object)
static void
db_show_ccp_sc(struct ccp_softc *sc)
{

	db_printf("ccp softc at %p\n", sc);
	db_printf(" cid: %d\n", (int)sc->cid);

	db_printf(" lock: ");
	db_show_lock(&sc->lock);

	db_printf(" detaching: %d\n", (int)sc->detaching);
	db_printf(" ring_size_order: %u\n", sc->ring_size_order);

	db_printf(" hw_version: %d\n", (int)sc->hw_version);
	db_printf(" hw_features: %b\n", (int)sc->hw_features,
	    "\20\24ELFC\23TRNG\22Zip_Compress\16Zip_Decompress\13ECC\12RSA"
	    "\11SHA\0103DES\07AES");

	db_printf(" hw status:\n");
	db_ccp_show_hw(sc);
}

static void
db_show_ccp_qp(struct ccp_queue *qp)
{

	db_printf(" lock: ");
	db_show_lock(&qp->cq_lock);

	db_printf(" cq_qindex: %u\n", qp->cq_qindex);
	db_printf(" cq_softc: %p\n", qp->cq_softc);

	db_printf(" head: %u\n", qp->cq_head);
	db_printf(" tail: %u\n", qp->cq_tail);
	db_printf(" acq_tail: %u\n", qp->cq_acq_tail);
	db_printf(" desc_ring: %p\n", qp->desc_ring);
	db_printf(" completions_ring: %p\n", qp->completions_ring);
	db_printf(" descriptors (phys): 0x%jx\n",
	    (uintmax_t)qp->desc_ring_bus_addr);

	db_printf(" hw status:\n");
	db_ccp_show_queue_hw(qp);
}

DB_SHOW_COMMAND(ccp, db_show_ccp)
{
	struct ccp_softc *sc;
	unsigned unit, qindex;

	if (!have_addr)
		goto usage;

	unit = (unsigned)addr;

	sc = devclass_get_softc(devclass_find("ccp"), unit);
	if (sc == NULL) {
		db_printf("No such device ccp%u\n", unit);
		goto usage;
	}

	if (count == -1) {
		db_show_ccp_sc(sc);
		return;
	}

	qindex = (unsigned)count;
	if (qindex >= nitems(sc->queues)) {
		db_printf("No such queue %u\n", qindex);
		goto usage;
	}
	db_show_ccp_qp(&sc->queues[qindex]);
	return;

usage:
	db_printf("usage: show ccp <unit>[,<qindex>]\n");
	return;
}
#endif /* DDB */
