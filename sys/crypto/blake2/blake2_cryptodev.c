/*-
 * Copyright (c) 2018 Conrad Meyer <cem@FreeBSD.org>
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rwlock.h>
#include <sys/smp.h>

#include <blake2.h>

#include <opencrypto/cryptodev.h>
#include <cryptodev_if.h>

#if defined(__amd64__)
#include <machine/fpu.h>
#elif defined(__i386__)
#include <machine/npx.h>
#endif

struct blake2_session {
	int algo;
	size_t klen;
	size_t mlen;
	uint8_t key[BLAKE2B_KEYBYTES];
	bool used;
	uint32_t id;
	TAILQ_ENTRY(blake2_session) next;
};
CTASSERT((size_t)BLAKE2B_KEYBYTES > (size_t)BLAKE2S_KEYBYTES);

struct blake2_softc {
	bool	dying;
	int32_t cid;
	uint32_t sid;
	TAILQ_HEAD(blake2_sessions_head, blake2_session) sessions;
	struct rwlock lock;
};

static struct mtx_padalign *ctx_mtx;
static struct fpu_kern_ctx **ctx_fpu;

#define ACQUIRE_CTX(i, ctx)					\
	do {							\
		(i) = PCPU_GET(cpuid);				\
		mtx_lock(&ctx_mtx[(i)]);			\
		(ctx) = ctx_fpu[(i)];				\
	} while (0)
#define RELEASE_CTX(i, ctx)					\
	do {							\
		mtx_unlock(&ctx_mtx[(i)]);			\
		(i) = -1;					\
		(ctx) = NULL;					\
	} while (0)

static int blake2_newsession(device_t, uint32_t *sidp, struct cryptoini *cri);
static int blake2_freesession(device_t, uint64_t tid);
static void blake2_freesession_locked(struct blake2_softc *sc,
    struct blake2_session *ses);
static int blake2_cipher_setup(struct blake2_session *ses,
    struct cryptoini *authini);
static int blake2_cipher_process(struct blake2_session *ses,
    struct cryptop *crp);

MALLOC_DEFINE(M_BLAKE2, "blake2_data", "Blake2 Data");

static void
blake2_identify(driver_t *drv, device_t parent)
{

	/* NB: order 10 is so we get attached after h/w devices */
	if (device_find_child(parent, "blaketwo", -1) == NULL &&
	    BUS_ADD_CHILD(parent, 10, "blaketwo", -1) == 0)
		panic("blaketwo: could not attach");
}

static int
blake2_probe(device_t dev)
{
	device_set_desc(dev, "Blake2");
	return (0);
}

static void
blake2_cleanctx(void)
{
	int i;

	/* XXX - no way to return driverid */
	CPU_FOREACH(i) {
		if (ctx_fpu[i] != NULL) {
			mtx_destroy(&ctx_mtx[i]);
			fpu_kern_free_ctx(ctx_fpu[i]);
		}
		ctx_fpu[i] = NULL;
	}
	free(ctx_mtx, M_BLAKE2);
	ctx_mtx = NULL;
	free(ctx_fpu, M_BLAKE2);
	ctx_fpu = NULL;
}

static int
blake2_attach(device_t dev)
{
	struct blake2_softc *sc;
	int i;

	sc = device_get_softc(dev);
	sc->dying = false;
	TAILQ_INIT(&sc->sessions);
	sc->sid = 1;

	sc->cid = crypto_get_driverid(dev, CRYPTOCAP_F_HARDWARE |
	    CRYPTOCAP_F_SYNC);
	if (sc->cid < 0) {
		device_printf(dev, "Could not get crypto driver id.\n");
		return (ENOMEM);
	}

	ctx_mtx = malloc(sizeof(*ctx_mtx) * (mp_maxid + 1), M_BLAKE2,
	    M_WAITOK | M_ZERO);
	ctx_fpu = malloc(sizeof(*ctx_fpu) * (mp_maxid + 1), M_BLAKE2,
	    M_WAITOK | M_ZERO);

	CPU_FOREACH(i) {
		ctx_fpu[i] = fpu_kern_alloc_ctx(0);
		mtx_init(&ctx_mtx[i], "bl2fpumtx", NULL, MTX_DEF | MTX_NEW);
	}

	rw_init(&sc->lock, "blake2_lock");

	crypto_register(sc->cid, CRYPTO_BLAKE2B, 0, 0);
	crypto_register(sc->cid, CRYPTO_BLAKE2S, 0, 0);
	return (0);
}

static int
blake2_detach(device_t dev)
{
	struct blake2_softc *sc;
	struct blake2_session *ses;

	sc = device_get_softc(dev);

	rw_wlock(&sc->lock);
	TAILQ_FOREACH(ses, &sc->sessions, next) {
		if (ses->used) {
			rw_wunlock(&sc->lock);
			device_printf(dev,
			    "Cannot detach, sessions still active.\n");
			return (EBUSY);
		}
	}
	sc->dying = true;
	while ((ses = TAILQ_FIRST(&sc->sessions)) != NULL) {
		TAILQ_REMOVE(&sc->sessions, ses, next);
		free(ses, M_BLAKE2);
	}
	rw_wunlock(&sc->lock);
	crypto_unregister_all(sc->cid);

	rw_destroy(&sc->lock);

	blake2_cleanctx();

	return (0);
}

static int
blake2_newsession(device_t dev, uint32_t *sidp, struct cryptoini *cri)
{
	struct blake2_softc *sc;
	struct blake2_session *ses;
	struct cryptoini *authini;
	int error;

	if (sidp == NULL || cri == NULL) {
		CRYPTDEB("no sidp or cri");
		return (EINVAL);
	}

	sc = device_get_softc(dev);

	ses = NULL;
	authini = NULL;
	for (; cri != NULL; cri = cri->cri_next) {
		switch (cri->cri_alg) {
		case CRYPTO_BLAKE2B:
		case CRYPTO_BLAKE2S:
			if (authini != NULL) {
				CRYPTDEB("authini already set");
				return (EINVAL);
			}
			authini = cri;
			break;
		default:
			CRYPTDEB("unhandled algorithm");
			return (EINVAL);
		}
	}
	if (authini == NULL) {
		CRYPTDEB("no cipher");
		return (EINVAL);
	}

	rw_wlock(&sc->lock);
	if (sc->dying) {
		rw_wunlock(&sc->lock);
		return (EINVAL);
	}
	/*
	 * Free sessions are inserted at the head of the list.  So if the first
	 * session is used, none are free and we must allocate a new one.
	 */
	ses = TAILQ_FIRST(&sc->sessions);
	if (ses == NULL || ses->used) {
		ses = malloc(sizeof(*ses), M_BLAKE2, M_NOWAIT | M_ZERO);
		if (ses == NULL) {
			rw_wunlock(&sc->lock);
			return (ENOMEM);
		}
		ses->id = sc->sid++;
	} else {
		TAILQ_REMOVE(&sc->sessions, ses, next);
	}
	ses->used = true;
	TAILQ_INSERT_TAIL(&sc->sessions, ses, next);
	rw_wunlock(&sc->lock);

	ses->algo = authini->cri_alg;
	error = blake2_cipher_setup(ses, authini);
	if (error != 0) {
		CRYPTDEB("setup failed");
		rw_wlock(&sc->lock);
		blake2_freesession_locked(sc, ses);
		rw_wunlock(&sc->lock);
		return (error);
	}

	*sidp = ses->id;
	return (0);
}

static void
blake2_freesession_locked(struct blake2_softc *sc, struct blake2_session *ses)
{
	uint32_t sid;

	rw_assert(&sc->lock, RA_WLOCKED);

	sid = ses->id;
	TAILQ_REMOVE(&sc->sessions, ses, next);
	explicit_bzero(ses, sizeof(*ses));
	ses->id = sid;
	TAILQ_INSERT_HEAD(&sc->sessions, ses, next);
}

static int
blake2_freesession(device_t dev, uint64_t tid)
{
	struct blake2_softc *sc;
	struct blake2_session *ses;
	uint32_t sid;

	sc = device_get_softc(dev);
	sid = ((uint32_t)tid) & 0xffffffff;
	rw_wlock(&sc->lock);
	TAILQ_FOREACH_REVERSE(ses, &sc->sessions, blake2_sessions_head, next) {
		if (ses->id == sid)
			break;
	}
	if (ses == NULL) {
		rw_wunlock(&sc->lock);
		return (EINVAL);
	}
	blake2_freesession_locked(sc, ses);
	rw_wunlock(&sc->lock);
	return (0);
}

static int
blake2_process(device_t dev, struct cryptop *crp, int hint __unused)
{
	struct blake2_softc *sc;
	struct blake2_session *ses;
	struct cryptodesc *crd, *authcrd;
	int error;

	sc = device_get_softc(dev);
	ses = NULL;
	error = 0;
	authcrd = NULL;

	/* Sanity check. */
	if (crp == NULL)
		return (EINVAL);

	if (crp->crp_callback == NULL || crp->crp_desc == NULL) {
		error = EINVAL;
		goto out;
	}

	for (crd = crp->crp_desc; crd != NULL; crd = crd->crd_next) {
		switch (crd->crd_alg) {
		case CRYPTO_BLAKE2B:
		case CRYPTO_BLAKE2S:
			if (authcrd != NULL) {
				error = EINVAL;
				goto out;
			}
			authcrd = crd;
			break;

		default:
			error = EINVAL;
			goto out;
		}
	}

	rw_rlock(&sc->lock);
	TAILQ_FOREACH_REVERSE(ses, &sc->sessions, blake2_sessions_head, next) {
		if (ses->id == (crp->crp_sid & 0xffffffff))
			break;
	}
	rw_runlock(&sc->lock);
	if (ses == NULL) {
		error = EINVAL;
		goto out;
	}

	error = blake2_cipher_process(ses, crp);
	if (error != 0)
		goto out;

out:
	crp->crp_etype = error;
	crypto_done(crp);
	return (error);
}

static device_method_t blake2_methods[] = {
	DEVMETHOD(device_identify, blake2_identify),
	DEVMETHOD(device_probe, blake2_probe),
	DEVMETHOD(device_attach, blake2_attach),
	DEVMETHOD(device_detach, blake2_detach),

	DEVMETHOD(cryptodev_newsession, blake2_newsession),
	DEVMETHOD(cryptodev_freesession, blake2_freesession),
	DEVMETHOD(cryptodev_process, blake2_process),

	DEVMETHOD_END
};

static driver_t blake2_driver = {
	"blaketwo",
	blake2_methods,
	sizeof(struct blake2_softc),
};
static devclass_t blake2_devclass;

DRIVER_MODULE(blake2, nexus, blake2_driver, blake2_devclass, 0, 0);
MODULE_VERSION(blake2, 1);
MODULE_DEPEND(blake2, crypto, 1, 1, 1);

static int
blake2_cipher_setup(struct blake2_session *ses, struct cryptoini *authini)
{
	int keylen;

	CTASSERT((size_t)BLAKE2S_OUTBYTES <= (size_t)BLAKE2B_OUTBYTES);

	if (authini->cri_mlen < 0)
		return (EINVAL);

	switch (ses->algo) {
	case CRYPTO_BLAKE2S:
		if (authini->cri_mlen != 0 &&
		    authini->cri_mlen > BLAKE2S_OUTBYTES)
			return (EINVAL);
		/* FALLTHROUGH */
	case CRYPTO_BLAKE2B:
		if (authini->cri_mlen != 0 &&
		    authini->cri_mlen > BLAKE2B_OUTBYTES)
			return (EINVAL);

		if (authini->cri_klen % 8 != 0)
			return (EINVAL);
		keylen = authini->cri_klen / 8;
		if (keylen > sizeof(ses->key) ||
		    (ses->algo == CRYPTO_BLAKE2S && keylen > BLAKE2S_KEYBYTES))
			return (EINVAL);
		ses->klen = keylen;
		memcpy(ses->key, authini->cri_key, keylen);
		ses->mlen = authini->cri_mlen;
	}
	return (0);
}

static int
blake2b_applicator(void *state, void *buf, u_int len)
{
	int rc;

	rc = blake2b_update(state, buf, len);
	if (rc != 0)
		return (EINVAL);
	return (0);
}

static int
blake2s_applicator(void *state, void *buf, u_int len)
{
	int rc;

	rc = blake2s_update(state, buf, len);
	if (rc != 0)
		return (EINVAL);
	return (0);
}

static int
blake2_cipher_process(struct blake2_session *ses, struct cryptop *crp)
{
	union {
		blake2b_state sb;
		blake2s_state ss;
	} bctx;
	char res[BLAKE2B_OUTBYTES];
	struct fpu_kern_ctx *ctx;
	int ctxidx;
	bool kt;
	struct cryptodesc *crd;
	int error, rc;
	size_t hashlen;

	crd = crp->crp_desc;
	ctx = NULL;
	ctxidx = 0;
	error = EINVAL;

	kt = is_fpu_kern_thread(0);
	if (!kt) {
		ACQUIRE_CTX(ctxidx, ctx);
		fpu_kern_enter(curthread, ctx,
		    FPU_KERN_NORMAL | FPU_KERN_KTHR);
	}

	if (crd->crd_flags != 0)
		goto out;

	switch (ses->algo) {
	case CRYPTO_BLAKE2B:
		if (ses->mlen != 0)
			hashlen = ses->mlen;
		else
			hashlen = BLAKE2B_OUTBYTES;
		if (ses->klen > 0)
			rc = blake2b_init_key(&bctx.sb, hashlen, ses->key, ses->klen);
		else
			rc = blake2b_init(&bctx.sb, hashlen);
		if (rc != 0)
			goto out;
		error = crypto_apply(crp->crp_flags, crp->crp_buf, crd->crd_skip,
		    crd->crd_len, blake2b_applicator, &bctx.sb);
		if (error != 0)
			goto out;
		rc = blake2b_final(&bctx.sb, res, hashlen);
		if (rc != 0) {
			error = EINVAL;
			goto out;
		}
		break;
	case CRYPTO_BLAKE2S:
		if (ses->mlen != 0)
			hashlen = ses->mlen;
		else
			hashlen = BLAKE2S_OUTBYTES;
		if (ses->klen > 0)
			rc = blake2s_init_key(&bctx.ss, hashlen, ses->key, ses->klen);
		else
			rc = blake2s_init(&bctx.ss, hashlen);
		if (rc != 0)
			goto out;
		error = crypto_apply(crp->crp_flags, crp->crp_buf, crd->crd_skip,
		    crd->crd_len, blake2s_applicator, &bctx.ss);
		if (error != 0)
			goto out;
		rc = blake2s_final(&bctx.ss, res, hashlen);
		if (rc != 0) {
			error = EINVAL;
			goto out;
		}
		break;
	default:
		panic("unreachable");
	}

	crypto_copyback(crp->crp_flags, crp->crp_buf, crd->crd_inject, hashlen,
	    (void *)res);

out:
	if (!kt) {
		fpu_kern_leave(curthread, ctx);
		RELEASE_CTX(ctxidx, ctx);
	}
	return (error);
}
