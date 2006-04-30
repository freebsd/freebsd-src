/*-
 * Copyright (c) 2005 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * Copyright (c) 2004 Mark R V Murray
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

/*	$OpenBSD: via.c,v 1.3 2004/06/15 23:36:55 deraadt Exp $	*/
/*-
 * Copyright (c) 2003 Jason Wright
 * Copyright (c) 2003, 2004 Theo de Raadt
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
#include <sys/mbuf.h>
#include <sys/uio.h>
#if defined(__i386__) && !defined(PC98)
#include <machine/cpufunc.h>
#include <machine/cputypes.h>
#endif

#include <opencrypto/cryptodev.h>
#include <crypto/rijndael/rijndael.h>


#define	PADLOCK_ROUND_COUNT_AES128	10
#define	PADLOCK_ROUND_COUNT_AES192	12
#define	PADLOCK_ROUND_COUNT_AES256	14

#define	PADLOCK_ALGORITHM_TYPE_AES	0

#define	PADLOCK_KEY_GENERATION_HW	0
#define	PADLOCK_KEY_GENERATION_SW	1

#define	PADLOCK_DIRECTION_ENCRYPT	0
#define	PADLOCK_DIRECTION_DECRYPT	1

#define	PADLOCK_KEY_SIZE_128	0
#define	PADLOCK_KEY_SIZE_192	1
#define	PADLOCK_KEY_SIZE_256	2

union padlock_cw {
	uint64_t raw;
	struct {
		u_int round_count : 4;
		u_int algorithm_type : 3;
		u_int key_generation : 1;
		u_int intermediate : 1;
		u_int direction : 1;
		u_int key_size : 2;
		u_int filler0 : 20;
		u_int filler1 : 32;
		u_int filler2 : 32;
		u_int filler3 : 32;
	} __field;
};
#define	cw_round_count		__field.round_count
#define	cw_algorithm_type	__field.algorithm_type
#define	cw_key_generation	__field.key_generation
#define	cw_intermediate		__field.intermediate
#define	cw_direction		__field.direction
#define	cw_key_size		__field.key_size
#define	cw_filler0		__field.filler0
#define	cw_filler1		__field.filler1
#define	cw_filler2		__field.filler2
#define	cw_filler3		__field.filler3

struct padlock_session {
	union padlock_cw ses_cw __aligned(16);
	uint32_t	ses_ekey[4 * (RIJNDAEL_MAXNR + 1) + 4] __aligned(16);	/* 128 bit aligned */
	uint32_t	ses_dkey[4 * (RIJNDAEL_MAXNR + 1) + 4] __aligned(16);	/* 128 bit aligned */
	uint8_t		ses_iv[16] __aligned(16);			/* 128 bit aligned */
	int		ses_used;
	uint32_t	ses_id;
	TAILQ_ENTRY(padlock_session) ses_next;
};

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

static __inline void
padlock_cbc(void *in, void *out, size_t count, void *key, union padlock_cw *cw,
    void *iv)
{
#ifdef __GNUCLIKE_ASM
	/* The .byte line is really VIA C3 "xcrypt-cbc" instruction */
	__asm __volatile(
		"pushf				\n\t"
		"popf				\n\t"
		"rep				\n\t"
		".byte	0x0f, 0xa7, 0xd0"
			: "+a" (iv), "+c" (count), "+D" (out), "+S" (in)
			: "b" (key), "d" (cw)
			: "cc", "memory"
		);
#endif
}

static int
padlock_init(void)
{
	struct padlock_softc *sc;
#if defined(__i386__) && !defined(PC98)
	u_int regs[4];
	int has_ace = 0;

	if (cpu_class < CPUCLASS_586)
		return (EINVAL);
	do_cpuid(1, regs);
	if ((regs[0] & 0xf) >= 3) {
		do_cpuid(0xc0000000, regs);
		if (regs[0] == 0xc0000001) {
			do_cpuid(0xc0000001, regs);
			if ((regs[3] & 0xc0) == 0xc0)
				has_ace = 1;
		}
	}
	if (!has_ace) {
		printf("PADLOCK: No ACE support.\n");
		return (EINVAL);
	}
#else
	return (EINVAL);
#endif

	padlock_sc = sc = malloc(sizeof(*padlock_sc), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (padlock_sc == NULL) {
		printf("PADLOCK: Could not allocate memory.\n");
		return (ENOMEM);
	}
	TAILQ_INIT(&sc->sc_sessions);
	sc->sc_sid = 1;

	sc->sc_cid = crypto_get_driverid(0);
	if (sc->sc_cid < 0) {
		printf("PADLOCK: Could not get crypto driver id.\n");
		free(padlock_sc, M_DEVBUF);
		padlock_sc = NULL;
		return (ENOMEM);
	}       

	mtx_init(&sc->sc_sessions_mtx, "padlock_mtx", NULL, MTX_DEF);
	crypto_register(sc->sc_cid, CRYPTO_AES_CBC, 0, 0, padlock_newsession,
	    padlock_freesession, padlock_process, NULL);
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
		printf("PADLOCK: Cannot destroy, %u sessions active.\n",
		    active);
		return (EBUSY);
	}
	padlock_sc = NULL;
	for (ses = TAILQ_FIRST(&sc->sc_sessions); ses != NULL;
	    ses = TAILQ_FIRST(&sc->sc_sessions)) {
		TAILQ_REMOVE(&sc->sc_sessions, ses, ses_next);
		free(ses, M_DEVBUF);
	}
	mtx_destroy(&sc->sc_sessions_mtx);
	crypto_unregister_all(sc->sc_cid);
	free(sc, M_DEVBUF);
	return (0);
}

static int
padlock_newsession(void *arg __unused, uint32_t *sidp, struct cryptoini *cri)
{
	struct padlock_softc *sc = padlock_sc;
	struct padlock_session *ses = NULL;
	union padlock_cw *cw;
	int i;

	if (sc == NULL || sidp == NULL || cri == NULL ||
	    cri->cri_next != NULL || cri->cri_alg != CRYPTO_AES_CBC) {
		return (EINVAL);
	}
	if (cri->cri_klen != 128 && cri->cri_klen != 192 &&
	    cri->cri_klen != 256) {
		return (EINVAL);
	}

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
		ses = malloc(sizeof(*ses), M_DEVBUF, M_NOWAIT | M_ZERO);
		if (ses == NULL)
			return (ENOMEM);
		ses->ses_used = 1;
		mtx_lock(&sc->sc_sessions_mtx);
		ses->ses_id = sc->sc_sid++;
		TAILQ_INSERT_TAIL(&sc->sc_sessions, ses, ses_next);
		mtx_unlock(&sc->sc_sessions_mtx);
	}

	cw = &ses->ses_cw;
	bzero(cw, sizeof(*cw));
	cw->cw_algorithm_type = PADLOCK_ALGORITHM_TYPE_AES;
	cw->cw_key_generation = PADLOCK_KEY_GENERATION_SW;
	cw->cw_intermediate = 0;
	switch (cri->cri_klen) {
	case 128:
		cw->cw_round_count = PADLOCK_ROUND_COUNT_AES128;
		cw->cw_key_size = PADLOCK_KEY_SIZE_128;
#ifdef HW_KEY_GENERATION
		/* This doesn't buy us much, that's why it is commented out. */
		cw->cw_key_generation = PADLOCK_KEY_GENERATION_HW;
#endif
		break;
	case 192:
		cw->cw_round_count = PADLOCK_ROUND_COUNT_AES192;
		cw->cw_key_size = PADLOCK_KEY_SIZE_192;
		break;
	case 256:
		cw->cw_round_count = PADLOCK_ROUND_COUNT_AES256;
		cw->cw_key_size = PADLOCK_KEY_SIZE_256;
		break;
	}

	arc4rand(ses->ses_iv, sizeof(ses->ses_iv), 0);

	if (cw->cw_key_generation == PADLOCK_KEY_GENERATION_SW) {
		/* Build expanded keys for both directions */
		rijndaelKeySetupEnc(ses->ses_ekey, cri->cri_key, cri->cri_klen);
		rijndaelKeySetupDec(ses->ses_dkey, cri->cri_key, cri->cri_klen);
		for (i = 0; i < 4 * (RIJNDAEL_MAXNR + 1); i++) {
			ses->ses_ekey[i] = ntohl(ses->ses_ekey[i]);
			ses->ses_dkey[i] = ntohl(ses->ses_dkey[i]);
		}
	} else {
		bcopy(cri->cri_key, ses->ses_ekey, cri->cri_klen);
		bcopy(cri->cri_key, ses->ses_dkey, cri->cri_klen);
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
	bzero(ses, sizeof(ses));
	ses->ses_used = 0;
	TAILQ_INSERT_TAIL(&sc->sc_sessions, ses, ses_next);
	mtx_unlock(&sc->sc_sessions_mtx);
	return (0);
}

static int
padlock_process(void *arg __unused, struct cryptop *crp, int hint __unused)
{
	struct padlock_softc *sc = padlock_sc;
	struct padlock_session *ses;
	union padlock_cw *cw;
	struct cryptodesc *crd = NULL;
	uint32_t *key;
	u_char *buf, *abuf;
	int err = 0;

	buf = NULL;
	if (crp == NULL || crp->crp_callback == NULL) {
		err = EINVAL;
		goto out;
	}
	crd = crp->crp_desc;
	if (crd == NULL || crd->crd_next != NULL ||
	    crd->crd_alg != CRYPTO_AES_CBC || 
	    (crd->crd_len % 16) != 0) {
		err = EINVAL;
		goto out;
	}

	mtx_lock(&sc->sc_sessions_mtx);
	TAILQ_FOREACH(ses, &sc->sc_sessions, ses_next) {
		if (ses->ses_id == (crp->crp_sid & 0xffffffff))
			break;
	}
	mtx_unlock(&sc->sc_sessions_mtx);
	if (ses == NULL) {
		err = EINVAL;
		goto out;
	}

	buf = malloc(crd->crd_len + 16, M_DEVBUF, M_NOWAIT);
	if (buf == NULL) {
		err = ENOMEM;
		goto out;
	}
	abuf = buf + 16 - ((uintptr_t)buf % 16);

	cw = &ses->ses_cw;
	cw->cw_filler0 = 0;
	cw->cw_filler1 = 0;
	cw->cw_filler2 = 0;
	cw->cw_filler3 = 0;
	if ((crd->crd_flags & CRD_F_ENCRYPT) != 0) {
		cw->cw_direction = PADLOCK_DIRECTION_ENCRYPT;
		key = ses->ses_ekey;
		if ((crd->crd_flags & CRD_F_IV_EXPLICIT) != 0)
			bcopy(crd->crd_iv, ses->ses_iv, 16);

		if ((crd->crd_flags & CRD_F_IV_PRESENT) == 0) {
			if ((crp->crp_flags & CRYPTO_F_IMBUF) != 0) {
				m_copyback((struct mbuf *)crp->crp_buf,
				    crd->crd_inject, 16, ses->ses_iv);
			} else if ((crp->crp_flags & CRYPTO_F_IOV) != 0) {
				cuio_copyback((struct uio *)crp->crp_buf,
				    crd->crd_inject, 16, ses->ses_iv);
			} else {
				bcopy(ses->ses_iv,
				    crp->crp_buf + crd->crd_inject, 16);
			}
		}
	} else {
		cw->cw_direction = PADLOCK_DIRECTION_DECRYPT;
		key = ses->ses_dkey;
		if ((crd->crd_flags & CRD_F_IV_EXPLICIT) != 0)
			bcopy(crd->crd_iv, ses->ses_iv, 16);
		else {
			if ((crp->crp_flags & CRYPTO_F_IMBUF) != 0) {
				m_copydata((struct mbuf *)crp->crp_buf,
				    crd->crd_inject, 16, ses->ses_iv);
			} else if ((crp->crp_flags & CRYPTO_F_IOV) != 0) {
				cuio_copydata((struct uio *)crp->crp_buf,
				    crd->crd_inject, 16, ses->ses_iv);
			} else {
				bcopy(crp->crp_buf + crd->crd_inject,
				    ses->ses_iv, 16);
			}
		}
	}

	if ((crp->crp_flags & CRYPTO_F_IMBUF) != 0) {
		m_copydata((struct mbuf *)crp->crp_buf, crd->crd_skip,
		    crd->crd_len, abuf);
	} else if ((crp->crp_flags & CRYPTO_F_IOV) != 0) {
		cuio_copydata((struct uio *)crp->crp_buf, crd->crd_skip,
		    crd->crd_len, abuf);
	} else {
		bcopy(crp->crp_buf + crd->crd_skip, abuf, crd->crd_len);
	}

	padlock_cbc(abuf, abuf, crd->crd_len / 16, key, cw, ses->ses_iv);

	if ((crp->crp_flags & CRYPTO_F_IMBUF) != 0) {
		m_copyback((struct mbuf *)crp->crp_buf, crd->crd_skip,
		    crd->crd_len, abuf);
	} else if ((crp->crp_flags & CRYPTO_F_IOV) != 0) {
		cuio_copyback((struct uio *)crp->crp_buf, crd->crd_skip,
		    crd->crd_len, abuf);
	} else {
		bcopy(abuf, crp->crp_buf + crd->crd_skip, crd->crd_len);
	}

	/* copy out last block for use as next session IV */
	if ((crd->crd_flags & CRD_F_ENCRYPT) != 0) {
		if ((crp->crp_flags & CRYPTO_F_IMBUF) != 0) {
			m_copydata((struct mbuf *)crp->crp_buf,
			    crd->crd_skip + crd->crd_len - 16, 16, ses->ses_iv);
		} else if ((crp->crp_flags & CRYPTO_F_IOV) != 0) {
			cuio_copydata((struct uio *)crp->crp_buf,
			    crd->crd_skip + crd->crd_len - 16, 16, ses->ses_iv);
		} else {
			bcopy(crp->crp_buf + crd->crd_skip + crd->crd_len - 16,
			    ses->ses_iv, 16);
		}
	}

out:
	if (buf != NULL) {
		bzero(buf, crd->crd_len + 16);
		free(buf, M_DEVBUF);
	}
	crp->crp_etype = err;
	crypto_done(crp);
	return (err);
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
