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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/libkern.h>
#if defined(__i386__) && !defined(PC98)
#include <machine/cpufunc.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>
#endif

#include <opencrypto/cryptodev.h>

#include <crypto/via/padlock.h>

/*
 * Technical documentation about the PadLock engine can be found here:
 *
 * http://www.via.com.tw/en/downloads/whitepapers/initiatives/padlock/programming_guide.pdf
 */

struct padlock_softc {
	int32_t		sc_cid;
	uint32_t	sc_sid;
	TAILQ_HEAD(, padlock_session) sc_sessions;
	struct mtx	sc_sessions_mtx;
};

static struct padlock_softc *padlock_sc;

static int padlock_newsession(void *arg __unused, uint32_t *sidp,
    struct cryptoini *cri);
static int padlock_freesession(void *arg __unused, uint64_t tid);
static int padlock_process(void *arg __unused, struct cryptop *crp,
    int hint __unused);

MALLOC_DEFINE(M_PADLOCK, "padlock_data", "PadLock Data");

static int
padlock_init(void)
{
	struct padlock_softc *sc;
	char capp[256];

#if defined(__i386__) && !defined(PC98)
	/* If there is no AES support, we has nothing to do here. */
	if (!(via_feature_xcrypt & VIA_HAS_AES)) {
		printf("PadLock: No ACE support.\n");
		return (EINVAL);
	}
	strlcpy(capp, "AES-CBC", sizeof(capp));
#if 0
	strlcat(capp, ",AES-EBC", sizeof(capp));
	strlcat(capp, ",AES-CFB", sizeof(capp));
	strlcat(capp, ",AES-OFB", sizeof(capp));
#endif
	if (via_feature_xcrypt & VIA_HAS_SHA) {
		strlcat(capp, ",SHA1", sizeof(capp));
		strlcat(capp, ",SHA256", sizeof(capp));
	}
#if 0
	if (via_feature_xcrypt & VIA_HAS_AESCTR)
		strlcat(capp, ",AES-CTR", sizeof(capp));
	if (via_feature_xcrypt & VIA_HAS_MM)
		strlcat(capp, ",RSA", sizeof(capp));
#endif
	printf("PadLock: HW support loaded for %s.\n", capp);
#else
	return (EINVAL);
#endif

	padlock_sc = sc = malloc(sizeof(*padlock_sc), M_PADLOCK,
	    M_WAITOK | M_ZERO);
	TAILQ_INIT(&sc->sc_sessions);
	sc->sc_sid = 1;

	sc->sc_cid = crypto_get_driverid(0);
	if (sc->sc_cid < 0) {
		printf("PadLock: Could not get crypto driver id.\n");
		free(padlock_sc, M_PADLOCK);
		padlock_sc = NULL;
		return (ENOMEM);
	}

	mtx_init(&sc->sc_sessions_mtx, "padlock_mtx", NULL, MTX_DEF);
	crypto_register(sc->sc_cid, CRYPTO_AES_CBC, 0, 0, padlock_newsession,
	    padlock_freesession, padlock_process, NULL);
	crypto_register(sc->sc_cid, CRYPTO_MD5_HMAC, 0, 0, padlock_newsession,
	    padlock_freesession, padlock_process, NULL);
	crypto_register(sc->sc_cid, CRYPTO_SHA1_HMAC, 0, 0, padlock_newsession,
	    padlock_freesession, padlock_process, NULL);
	crypto_register(sc->sc_cid, CRYPTO_RIPEMD160_HMAC, 0, 0,
	    padlock_newsession, padlock_freesession, padlock_process, NULL);
	crypto_register(sc->sc_cid, CRYPTO_SHA2_256_HMAC, 0, 0,
	    padlock_newsession, padlock_freesession, padlock_process, NULL);
	crypto_register(sc->sc_cid, CRYPTO_SHA2_384_HMAC, 0, 0,
	    padlock_newsession, padlock_freesession, padlock_process, NULL);
	crypto_register(sc->sc_cid, CRYPTO_SHA2_512_HMAC, 0, 0,
	    padlock_newsession, padlock_freesession, padlock_process, NULL);
	return (0);
}

static int
padlock_destroy(void)
{
	struct padlock_softc *sc = padlock_sc;
	struct padlock_session *ses;
	u_int active = 0;

	if (sc == NULL)
		return (0);
	mtx_lock(&sc->sc_sessions_mtx);
	TAILQ_FOREACH(ses, &sc->sc_sessions, ses_next) {
		if (ses->ses_used)
			active++;
	}
	if (active > 0) {
		mtx_unlock(&sc->sc_sessions_mtx);
		printf("PadLock: Cannot destroy, %u sessions active.\n",
		    active);
		return (EBUSY);
	}
	padlock_sc = NULL;
	for (ses = TAILQ_FIRST(&sc->sc_sessions); ses != NULL;
	    ses = TAILQ_FIRST(&sc->sc_sessions)) {
		TAILQ_REMOVE(&sc->sc_sessions, ses, ses_next);
		free(ses, M_PADLOCK);
	}
	mtx_destroy(&sc->sc_sessions_mtx);
	crypto_unregister_all(sc->sc_cid);
	free(sc, M_PADLOCK);
	return (0);
}

static int
padlock_newsession(void *arg __unused, uint32_t *sidp, struct cryptoini *cri)
{
	struct padlock_softc *sc = padlock_sc;
	struct padlock_session *ses = NULL;
	struct cryptoini *encini, *macini;
	int error;

	if (sc == NULL || sidp == NULL || cri == NULL)
		return (EINVAL);

	encini = macini = NULL;
	for (; cri != NULL; cri = cri->cri_next) {
		switch (cri->cri_alg) {
		case CRYPTO_NULL_HMAC:
		case CRYPTO_MD5_HMAC:
		case CRYPTO_SHA1_HMAC:
		case CRYPTO_RIPEMD160_HMAC:
		case CRYPTO_SHA2_256_HMAC:
		case CRYPTO_SHA2_384_HMAC:
		case CRYPTO_SHA2_512_HMAC:
			if (macini != NULL)
				return (EINVAL);
			macini = cri;
			break;
		case CRYPTO_AES_CBC:
			if (encini != NULL)
				return (EINVAL);
			encini = cri;
			break;
		default:
			return (EINVAL);
		}
	}

	/*
	 * We only support HMAC algorithms to be able to work with
	 * fast_ipsec(4), so if we are asked only for authentication without
	 * encryption, don't pretend we can accellerate it.
	 */
	if (encini == NULL)
		return (EINVAL);

	/*
	 * Let's look for a free session structure.
	 */
	mtx_lock(&sc->sc_sessions_mtx);
	/*
	 * Free sessions goes first, so if first session is used, we need to
	 * allocate one.
	 */
	ses = TAILQ_FIRST(&sc->sc_sessions);
	if (ses == NULL || ses->ses_used)
		ses = NULL;
	else {
		TAILQ_REMOVE(&sc->sc_sessions, ses, ses_next);
		ses->ses_used = 1;
		TAILQ_INSERT_TAIL(&sc->sc_sessions, ses, ses_next);
	}
	mtx_unlock(&sc->sc_sessions_mtx);
	if (ses == NULL) {
		ses = malloc(sizeof(*ses), M_PADLOCK, M_NOWAIT | M_ZERO);
		if (ses == NULL)
			return (ENOMEM);
		ses->ses_used = 1;
		mtx_lock(&sc->sc_sessions_mtx);
		ses->ses_id = sc->sc_sid++;
		TAILQ_INSERT_TAIL(&sc->sc_sessions, ses, ses_next);
		mtx_unlock(&sc->sc_sessions_mtx);
	}

	error = padlock_cipher_setup(ses, encini);
	if (error != 0) {
		padlock_freesession(NULL, ses->ses_id);
		return (error);
	}

	if (macini != NULL) {
		error = padlock_hash_setup(ses, macini);
		if (error != 0) {
			padlock_freesession(NULL, ses->ses_id);
			return (error);
		}
	}

	*sidp = ses->ses_id;
	return (0);
}

static int
padlock_freesession(void *arg __unused, uint64_t tid)
{
	struct padlock_softc *sc = padlock_sc;
	struct padlock_session *ses;
	uint32_t sid = ((uint32_t)tid) & 0xffffffff;

	if (sc == NULL)
		return (EINVAL);
	mtx_lock(&sc->sc_sessions_mtx);
	TAILQ_FOREACH(ses, &sc->sc_sessions, ses_next) {
		if (ses->ses_id == sid)
			break;
	}
	if (ses == NULL) {
		mtx_unlock(&sc->sc_sessions_mtx);
		return (EINVAL);
	}
	TAILQ_REMOVE(&sc->sc_sessions, ses, ses_next);
	padlock_hash_free(ses);
	bzero(ses, sizeof(*ses));
	ses->ses_used = 0;
	TAILQ_INSERT_TAIL(&sc->sc_sessions, ses, ses_next);
	mtx_unlock(&sc->sc_sessions_mtx);
	return (0);
}

static int
padlock_process(void *arg __unused, struct cryptop *crp, int hint __unused)
{
	struct padlock_softc *sc = padlock_sc;
	struct padlock_session *ses = NULL;
	struct cryptodesc *crd, *enccrd, *maccrd;
	int error = 0;

	enccrd = maccrd = NULL;

	if (crp == NULL || crp->crp_callback == NULL || crp->crp_desc == NULL) {
		error = EINVAL;
		goto out;
	}

	for (crd = crp->crp_desc; crd != NULL; crd = crd->crd_next) {
		switch (crd->crd_alg) {
		case CRYPTO_NULL_HMAC:
		case CRYPTO_MD5_HMAC:
		case CRYPTO_SHA1_HMAC:
		case CRYPTO_RIPEMD160_HMAC:
		case CRYPTO_SHA2_256_HMAC:
		case CRYPTO_SHA2_384_HMAC:
		case CRYPTO_SHA2_512_HMAC:
			if (maccrd != NULL) {
				error = EINVAL;
				goto out;
			}
			maccrd = crd;
			break;
		case CRYPTO_AES_CBC:
			if (enccrd != NULL) {
				error = EINVAL;
				goto out;
			}
			enccrd = crd;
			break;
		default:
			return (EINVAL);
		}
	}
	if (enccrd == NULL || (enccrd->crd_len % AES_BLOCK_LEN) != 0) {
		error = EINVAL;
		goto out;
	}

	mtx_lock(&sc->sc_sessions_mtx);
	TAILQ_FOREACH(ses, &sc->sc_sessions, ses_next) {
		if (ses->ses_id == (crp->crp_sid & 0xffffffff))
			break;
	}
	mtx_unlock(&sc->sc_sessions_mtx);
	if (ses == NULL) {
		error = EINVAL;
		goto out;
	}

	/* Perform data authentication if requested before encryption. */
	if (maccrd != NULL && maccrd->crd_next == enccrd) {
		error = padlock_hash_process(ses, maccrd, crp);
		if (error != 0)
			goto out;
	}

	error = padlock_cipher_process(ses, enccrd, crp);
	if (error != 0)
		goto out;

	/* Perform data authentication if requested after encryption. */
	if (maccrd != NULL && enccrd->crd_next == maccrd) {
		error = padlock_hash_process(ses, maccrd, crp);
		if (error != 0)
			goto out;
	}

out:
#if 0
	/*
	 * This code is not necessary, because contexts will be freed on next
	 * padlock_setup_mackey() call or at padlock_freesession() call.
	 */
	if (ses != NULL && maccrd != NULL &&
	    (maccrd->crd_flags & CRD_F_KEY_EXPLICIT) != 0) {
		padlock_free_ctx(ses->ses_axf, ses->ses_ictx);
		padlock_free_ctx(ses->ses_axf, ses->ses_octx);
	}
#endif
	crp->crp_etype = error;
	crypto_done(crp);
	return (error);
}

static int
padlock_modevent(module_t mod, int type, void *unused __unused)
{
	int error;

	error = EOPNOTSUPP;
	switch (type) {
	case MOD_LOAD:
		error = padlock_init();
		break;
	case MOD_UNLOAD:
		error = padlock_destroy();
		break;
	}
	return (error);
}

static moduledata_t padlock_mod = {
	"padlock",
	padlock_modevent,
	0
};
DECLARE_MODULE(padlock, padlock_mod, SI_SUB_DRIVERS, SI_ORDER_ANY);
MODULE_VERSION(padlock, 1);
MODULE_DEPEND(padlock, crypto, 1, 1, 1);
