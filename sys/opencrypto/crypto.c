/*-
 * Copyright (c) 2002-2006 Sam Leffler.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Cryptographic Subsystem.
 *
 * This code is derived from the Openbsd Cryptographic Framework (OCF)
 * that has the copyright shown below.  Very little of the original
 * code remains.
 */

/*-
 * The author of this code is Angelos D. Keromytis (angelos@cis.upenn.edu)
 *
 * This code was written by Angelos D. Keromytis in Athens, Greece, in
 * February 2000. Network Security Technologies Inc. (NSTI) kindly
 * supported the development of this code.
 *
 * Copyright (c) 2000, 2001 Angelos D. Keromytis
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all source code copies of any software which is or includes a copy or
 * modification of this software.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#include "opt_compat.h"
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/counter.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/refcount.h>
#include <sys/sdt.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/uio.h>

#include <ddb/ddb.h>

#include <vm/uma.h>
#include <crypto/intake.h>
#include <opencrypto/cryptodev.h>
#include <opencrypto/xform_auth.h>
#include <opencrypto/xform_enc.h>

#include <sys/kobj.h>
#include <sys/bus.h>
#include "cryptodev_if.h"

#if defined(__i386__) || defined(__amd64__) || defined(__aarch64__)
#include <machine/pcb.h>
#endif

SDT_PROVIDER_DEFINE(opencrypto);

/*
 * Crypto drivers register themselves by allocating a slot in the
 * crypto_drivers table with crypto_get_driverid() and then registering
 * each asym algorithm they support with crypto_kregister().
 */
static	struct mtx crypto_drivers_mtx;		/* lock on driver table */
#define	CRYPTO_DRIVER_LOCK()	mtx_lock(&crypto_drivers_mtx)
#define	CRYPTO_DRIVER_UNLOCK()	mtx_unlock(&crypto_drivers_mtx)
#define	CRYPTO_DRIVER_ASSERT()	mtx_assert(&crypto_drivers_mtx, MA_OWNED)

/*
 * Crypto device/driver capabilities structure.
 *
 * Synchronization:
 * (d) - protected by CRYPTO_DRIVER_LOCK()
 * (q) - protected by CRYPTO_Q_LOCK()
 * Not tagged fields are read-only.
 */
struct cryptocap {
	device_t	cc_dev;
	uint32_t	cc_hid;
	u_int32_t	cc_sessions;		/* (d) # of sessions */
	u_int32_t	cc_koperations;		/* (d) # os asym operations */
	u_int8_t	cc_kalg[CRK_ALGORITHM_MAX + 1];

	int		cc_flags;		/* (d) flags */
#define CRYPTOCAP_F_CLEANUP	0x80000000	/* needs resource cleanup */
	int		cc_qblocked;		/* (q) symmetric q blocked */
	int		cc_kqblocked;		/* (q) asymmetric q blocked */
	size_t		cc_session_size;
	volatile int	cc_refs;
};

static	struct cryptocap **crypto_drivers = NULL;
static	int crypto_drivers_size = 0;

struct crypto_session {
	struct cryptocap *cap;
	void *softc;
	struct crypto_session_params csp;
};

/*
 * There are two queues for crypto requests; one for symmetric (e.g.
 * cipher) operations and one for asymmetric (e.g. MOD)operations.
 * A single mutex is used to lock access to both queues.  We could
 * have one per-queue but having one simplifies handling of block/unblock
 * operations.
 */
static	int crp_sleep = 0;
static	TAILQ_HEAD(cryptop_q ,cryptop) crp_q;		/* request queues */
static	TAILQ_HEAD(,cryptkop) crp_kq;
static	struct mtx crypto_q_mtx;
#define	CRYPTO_Q_LOCK()		mtx_lock(&crypto_q_mtx)
#define	CRYPTO_Q_UNLOCK()	mtx_unlock(&crypto_q_mtx)

SYSCTL_NODE(_kern, OID_AUTO, crypto, CTLFLAG_RW, 0,
    "In-kernel cryptography");

/*
 * Taskqueue used to dispatch the crypto requests
 * that have the CRYPTO_F_ASYNC flag
 */
static struct taskqueue *crypto_tq;

/*
 * Crypto seq numbers are operated on with modular arithmetic
 */
#define	CRYPTO_SEQ_GT(a,b)	((int)((a)-(b)) > 0)

struct crypto_ret_worker {
	struct mtx crypto_ret_mtx;

	TAILQ_HEAD(,cryptop) crp_ordered_ret_q;	/* ordered callback queue for symetric jobs */
	TAILQ_HEAD(,cryptop) crp_ret_q;		/* callback queue for symetric jobs */
	TAILQ_HEAD(,cryptkop) crp_ret_kq;	/* callback queue for asym jobs */

	u_int32_t reorder_ops;		/* total ordered sym jobs received */
	u_int32_t reorder_cur_seq;	/* current sym job dispatched */

	struct proc *cryptoretproc;
};
static struct crypto_ret_worker *crypto_ret_workers = NULL;

#define CRYPTO_RETW(i)		(&crypto_ret_workers[i])
#define CRYPTO_RETW_ID(w)	((w) - crypto_ret_workers)
#define FOREACH_CRYPTO_RETW(w) \
	for (w = crypto_ret_workers; w < crypto_ret_workers + crypto_workers_num; ++w)

#define	CRYPTO_RETW_LOCK(w)	mtx_lock(&w->crypto_ret_mtx)
#define	CRYPTO_RETW_UNLOCK(w)	mtx_unlock(&w->crypto_ret_mtx)
#define	CRYPTO_RETW_EMPTY(w) \
	(TAILQ_EMPTY(&w->crp_ret_q) && TAILQ_EMPTY(&w->crp_ret_kq) && TAILQ_EMPTY(&w->crp_ordered_ret_q))

static int crypto_workers_num = 0;
SYSCTL_INT(_kern_crypto, OID_AUTO, num_workers, CTLFLAG_RDTUN,
	   &crypto_workers_num, 0,
	   "Number of crypto workers used to dispatch crypto jobs");
#ifdef COMPAT_FREEBSD12
SYSCTL_INT(_kern, OID_AUTO, crypto_workers_num, CTLFLAG_RDTUN,
	   &crypto_workers_num, 0,
	   "Number of crypto workers used to dispatch crypto jobs");
#endif

static	uma_zone_t cryptop_zone;
static	uma_zone_t cryptoses_zone;

int	crypto_userasymcrypto = 1;
SYSCTL_INT(_kern_crypto, OID_AUTO, asym_enable, CTLFLAG_RW,
	   &crypto_userasymcrypto, 0,
	   "Enable user-mode access to asymmetric crypto support");
#ifdef COMPAT_FREEBSD12
SYSCTL_INT(_kern, OID_AUTO, userasymcrypto, CTLFLAG_RW,
	   &crypto_userasymcrypto, 0,
	   "Enable/disable user-mode access to asymmetric crypto support");
#endif

int	crypto_devallowsoft = 0;
SYSCTL_INT(_kern_crypto, OID_AUTO, allow_soft, CTLFLAG_RW,
	   &crypto_devallowsoft, 0,
	   "Enable use of software crypto by /dev/crypto");
#ifdef COMPAT_FREEBSD12
SYSCTL_INT(_kern, OID_AUTO, cryptodevallowsoft, CTLFLAG_RW,
	   &crypto_devallowsoft, 0,
	   "Enable/disable use of software crypto by /dev/crypto");
#endif

MALLOC_DEFINE(M_CRYPTO_DATA, "crypto", "crypto session records");

static	void crypto_proc(void);
static	struct proc *cryptoproc;
static	void crypto_ret_proc(struct crypto_ret_worker *ret_worker);
static	void crypto_destroy(void);
static	int crypto_invoke(struct cryptocap *cap, struct cryptop *crp, int hint);
static	int crypto_kinvoke(struct cryptkop *krp);
static	void crypto_task_invoke(void *ctx, int pending);
static void crypto_batch_enqueue(struct cryptop *crp);

static counter_u64_t cryptostats[sizeof(struct cryptostats) / sizeof(uint64_t)];
SYSCTL_COUNTER_U64_ARRAY(_kern_crypto, OID_AUTO, stats, CTLFLAG_RW,
    cryptostats, nitems(cryptostats),
    "Crypto system statistics");

#define	CRYPTOSTAT_INC(stat) do {					\
	counter_u64_add(						\
	    cryptostats[offsetof(struct cryptostats, stat) / sizeof(uint64_t)],\
	    1);								\
} while (0)

static void
cryptostats_init(void *arg __unused)
{
	COUNTER_ARRAY_ALLOC(cryptostats, nitems(cryptostats), M_WAITOK);
}
SYSINIT(cryptostats_init, SI_SUB_COUNTER, SI_ORDER_ANY, cryptostats_init, NULL);

static void
cryptostats_fini(void *arg __unused)
{
	COUNTER_ARRAY_FREE(cryptostats, nitems(cryptostats));
}
SYSUNINIT(cryptostats_fini, SI_SUB_COUNTER, SI_ORDER_ANY, cryptostats_fini,
    NULL);

/* Try to avoid directly exposing the key buffer as a symbol */
static struct keybuf *keybuf;

static struct keybuf empty_keybuf = {
        .kb_nents = 0
};

/* Obtain the key buffer from boot metadata */
static void
keybuf_init(void)
{
	caddr_t kmdp;

	kmdp = preload_search_by_type("elf kernel");

	if (kmdp == NULL)
		kmdp = preload_search_by_type("elf64 kernel");

	keybuf = (struct keybuf *)preload_search_info(kmdp,
	    MODINFO_METADATA | MODINFOMD_KEYBUF);

        if (keybuf == NULL)
                keybuf = &empty_keybuf;
}

/* It'd be nice if we could store these in some kind of secure memory... */
struct keybuf * get_keybuf(void) {

        return (keybuf);
}

static struct cryptocap *
cap_ref(struct cryptocap *cap)
{

	refcount_acquire(&cap->cc_refs);
	return (cap);
}

static void
cap_rele(struct cryptocap *cap)
{

	if (refcount_release(&cap->cc_refs) == 0)
		return;

	KASSERT(cap->cc_sessions == 0,
	    ("freeing crypto driver with active sessions"));
	KASSERT(cap->cc_koperations == 0,
	    ("freeing crypto driver with active key operations"));

	free(cap, M_CRYPTO_DATA);
}

static int
crypto_init(void)
{
	struct crypto_ret_worker *ret_worker;
	int error;

	mtx_init(&crypto_drivers_mtx, "crypto", "crypto driver table",
		MTX_DEF|MTX_QUIET);

	TAILQ_INIT(&crp_q);
	TAILQ_INIT(&crp_kq);
	mtx_init(&crypto_q_mtx, "crypto", "crypto op queues", MTX_DEF);

	cryptop_zone = uma_zcreate("cryptop", sizeof (struct cryptop),
				    0, 0, 0, 0,
				    UMA_ALIGN_PTR, UMA_ZONE_ZINIT);
	cryptoses_zone = uma_zcreate("crypto_session",
	    sizeof(struct crypto_session), NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, UMA_ZONE_ZINIT);

	if (cryptop_zone == NULL || cryptoses_zone == NULL) {
		printf("crypto_init: cannot setup crypto zones\n");
		error = ENOMEM;
		goto bad;
	}

	crypto_drivers_size = CRYPTO_DRIVERS_INITIAL;
	crypto_drivers = malloc(crypto_drivers_size *
	    sizeof(struct cryptocap), M_CRYPTO_DATA, M_NOWAIT | M_ZERO);
	if (crypto_drivers == NULL) {
		printf("crypto_init: cannot setup crypto drivers\n");
		error = ENOMEM;
		goto bad;
	}

	if (crypto_workers_num < 1 || crypto_workers_num > mp_ncpus)
		crypto_workers_num = mp_ncpus;

	crypto_tq = taskqueue_create("crypto", M_WAITOK|M_ZERO,
				taskqueue_thread_enqueue, &crypto_tq);
	if (crypto_tq == NULL) {
		printf("crypto init: cannot setup crypto taskqueue\n");
		error = ENOMEM;
		goto bad;
	}

	taskqueue_start_threads(&crypto_tq, crypto_workers_num, PRI_MIN_KERN,
		"crypto");

	error = kproc_create((void (*)(void *)) crypto_proc, NULL,
		    &cryptoproc, 0, 0, "crypto");
	if (error) {
		printf("crypto_init: cannot start crypto thread; error %d",
			error);
		goto bad;
	}

	crypto_ret_workers = malloc(crypto_workers_num * sizeof(struct crypto_ret_worker),
			M_CRYPTO_DATA, M_NOWAIT|M_ZERO);
	if (crypto_ret_workers == NULL) {
		error = ENOMEM;
		printf("crypto_init: cannot allocate ret workers\n");
		goto bad;
	}


	FOREACH_CRYPTO_RETW(ret_worker) {
		TAILQ_INIT(&ret_worker->crp_ordered_ret_q);
		TAILQ_INIT(&ret_worker->crp_ret_q);
		TAILQ_INIT(&ret_worker->crp_ret_kq);

		ret_worker->reorder_ops = 0;
		ret_worker->reorder_cur_seq = 0;

		mtx_init(&ret_worker->crypto_ret_mtx, "crypto", "crypto return queues", MTX_DEF);

		error = kproc_create((void (*)(void *)) crypto_ret_proc, ret_worker,
				&ret_worker->cryptoretproc, 0, 0, "crypto returns %td", CRYPTO_RETW_ID(ret_worker));
		if (error) {
			printf("crypto_init: cannot start cryptoret thread; error %d",
				error);
			goto bad;
		}
	}

	keybuf_init();

	return 0;
bad:
	crypto_destroy();
	return error;
}

/*
 * Signal a crypto thread to terminate.  We use the driver
 * table lock to synchronize the sleep/wakeups so that we
 * are sure the threads have terminated before we release
 * the data structures they use.  See crypto_finis below
 * for the other half of this song-and-dance.
 */
static void
crypto_terminate(struct proc **pp, void *q)
{
	struct proc *p;

	mtx_assert(&crypto_drivers_mtx, MA_OWNED);
	p = *pp;
	*pp = NULL;
	if (p) {
		wakeup_one(q);
		PROC_LOCK(p);		/* NB: insure we don't miss wakeup */
		CRYPTO_DRIVER_UNLOCK();	/* let crypto_finis progress */
		msleep(p, &p->p_mtx, PWAIT, "crypto_destroy", 0);
		PROC_UNLOCK(p);
		CRYPTO_DRIVER_LOCK();
	}
}

static void
hmac_init_pad(struct auth_hash *axf, const char *key, int klen, void *auth_ctx,
    uint8_t padval)
{
	uint8_t hmac_key[HMAC_MAX_BLOCK_LEN];
	u_int i;

	KASSERT(axf->blocksize <= sizeof(hmac_key),
	    ("Invalid HMAC block size %d", axf->blocksize));

	/*
	 * If the key is larger than the block size, use the digest of
	 * the key as the key instead.
	 */
	memset(hmac_key, 0, sizeof(hmac_key));
	if (klen > axf->blocksize) {
		axf->Init(auth_ctx);
		axf->Update(auth_ctx, key, klen);
		axf->Final(hmac_key, auth_ctx);
		klen = axf->hashsize;
	} else
		memcpy(hmac_key, key, klen);

	for (i = 0; i < axf->blocksize; i++)
		hmac_key[i] ^= padval;

	axf->Init(auth_ctx);
	axf->Update(auth_ctx, hmac_key, axf->blocksize);
	explicit_bzero(hmac_key, sizeof(hmac_key));
}

void
hmac_init_ipad(struct auth_hash *axf, const char *key, int klen,
    void *auth_ctx)
{

	hmac_init_pad(axf, key, klen, auth_ctx, HMAC_IPAD_VAL);
}

void
hmac_init_opad(struct auth_hash *axf, const char *key, int klen,
    void *auth_ctx)
{

	hmac_init_pad(axf, key, klen, auth_ctx, HMAC_OPAD_VAL);
}

static void
crypto_destroy(void)
{
	struct crypto_ret_worker *ret_worker;
	int i;

	/*
	 * Terminate any crypto threads.
	 */
	if (crypto_tq != NULL)
		taskqueue_drain_all(crypto_tq);
	CRYPTO_DRIVER_LOCK();
	crypto_terminate(&cryptoproc, &crp_q);
	FOREACH_CRYPTO_RETW(ret_worker)
		crypto_terminate(&ret_worker->cryptoretproc, &ret_worker->crp_ret_q);
	CRYPTO_DRIVER_UNLOCK();

	/* XXX flush queues??? */

	/*
	 * Reclaim dynamically allocated resources.
	 */
	for (i = 0; i < crypto_drivers_size; i++) {
		if (crypto_drivers[i] != NULL)
			cap_rele(crypto_drivers[i]);
	}
	free(crypto_drivers, M_CRYPTO_DATA);

	if (cryptoses_zone != NULL)
		uma_zdestroy(cryptoses_zone);
	if (cryptop_zone != NULL)
		uma_zdestroy(cryptop_zone);
	mtx_destroy(&crypto_q_mtx);
	FOREACH_CRYPTO_RETW(ret_worker)
		mtx_destroy(&ret_worker->crypto_ret_mtx);
	free(crypto_ret_workers, M_CRYPTO_DATA);
	if (crypto_tq != NULL)
		taskqueue_free(crypto_tq);
	mtx_destroy(&crypto_drivers_mtx);
}

uint32_t
crypto_ses2hid(crypto_session_t crypto_session)
{
	return (crypto_session->cap->cc_hid);
}

uint32_t
crypto_ses2caps(crypto_session_t crypto_session)
{
	return (crypto_session->cap->cc_flags & 0xff000000);
}

void *
crypto_get_driver_session(crypto_session_t crypto_session)
{
	return (crypto_session->softc);
}

const struct crypto_session_params *
crypto_get_params(crypto_session_t crypto_session)
{
	return (&crypto_session->csp);
}

struct auth_hash *
crypto_auth_hash(const struct crypto_session_params *csp)
{

	switch (csp->csp_auth_alg) {
	case CRYPTO_SHA1_HMAC:
		return (&auth_hash_hmac_sha1);
	case CRYPTO_SHA2_224_HMAC:
		return (&auth_hash_hmac_sha2_224);
	case CRYPTO_SHA2_256_HMAC:
		return (&auth_hash_hmac_sha2_256);
	case CRYPTO_SHA2_384_HMAC:
		return (&auth_hash_hmac_sha2_384);
	case CRYPTO_SHA2_512_HMAC:
		return (&auth_hash_hmac_sha2_512);
	case CRYPTO_NULL_HMAC:
		return (&auth_hash_null);
	case CRYPTO_RIPEMD160_HMAC:
		return (&auth_hash_hmac_ripemd_160);
	case CRYPTO_SHA1:
		return (&auth_hash_sha1);
	case CRYPTO_SHA2_224:
		return (&auth_hash_sha2_224);
	case CRYPTO_SHA2_256:
		return (&auth_hash_sha2_256);
	case CRYPTO_SHA2_384:
		return (&auth_hash_sha2_384);
	case CRYPTO_SHA2_512:
		return (&auth_hash_sha2_512);
	case CRYPTO_AES_NIST_GMAC:
		switch (csp->csp_auth_klen) {
		case 128 / 8:
			return (&auth_hash_nist_gmac_aes_128);
		case 192 / 8:
			return (&auth_hash_nist_gmac_aes_192);
		case 256 / 8:
			return (&auth_hash_nist_gmac_aes_256);
		default:
			return (NULL);
		}
	case CRYPTO_BLAKE2B:
		return (&auth_hash_blake2b);
	case CRYPTO_BLAKE2S:
		return (&auth_hash_blake2s);
	case CRYPTO_POLY1305:
		return (&auth_hash_poly1305);
	case CRYPTO_AES_CCM_CBC_MAC:
		switch (csp->csp_auth_klen) {
		case 128 / 8:
			return (&auth_hash_ccm_cbc_mac_128);
		case 192 / 8:
			return (&auth_hash_ccm_cbc_mac_192);
		case 256 / 8:
			return (&auth_hash_ccm_cbc_mac_256);
		default:
			return (NULL);
		}
	default:
		return (NULL);
	}
}

struct enc_xform *
crypto_cipher(const struct crypto_session_params *csp)
{

	switch (csp->csp_cipher_alg) {
	case CRYPTO_RIJNDAEL128_CBC:
		return (&enc_xform_rijndael128);
	case CRYPTO_AES_XTS:
		return (&enc_xform_aes_xts);
	case CRYPTO_AES_ICM:
		return (&enc_xform_aes_icm);
	case CRYPTO_AES_NIST_GCM_16:
		return (&enc_xform_aes_nist_gcm);
	case CRYPTO_CAMELLIA_CBC:
		return (&enc_xform_camellia);
	case CRYPTO_NULL_CBC:
		return (&enc_xform_null);
	case CRYPTO_CHACHA20:
		return (&enc_xform_chacha20);
	case CRYPTO_AES_CCM_16:
		return (&enc_xform_ccm);
	default:
		return (NULL);
	}
}

static struct cryptocap *
crypto_checkdriver(u_int32_t hid)
{

	return (hid >= crypto_drivers_size ? NULL : crypto_drivers[hid]);
}

/*
 * Select a driver for a new session that supports the specified
 * algorithms and, optionally, is constrained according to the flags.
 */
static struct cryptocap *
crypto_select_driver(const struct crypto_session_params *csp, int flags)
{
	struct cryptocap *cap, *best;
	int best_match, error, hid;

	CRYPTO_DRIVER_ASSERT();

	best = NULL;
	for (hid = 0; hid < crypto_drivers_size; hid++) {
		/*
		 * If there is no driver for this slot, or the driver
		 * is not appropriate (hardware or software based on
		 * match), then skip.
		 */
		cap = crypto_drivers[hid];
		if (cap == NULL ||
		    (cap->cc_flags & flags) == 0)
			continue;

		error = CRYPTODEV_PROBESESSION(cap->cc_dev, csp);
		if (error >= 0)
			continue;

		/*
		 * Use the driver with the highest probe value.
		 * Hardware drivers use a higher probe value than
		 * software.  In case of a tie, prefer the driver with
		 * the fewest active sessions.
		 */
		if (best == NULL || error > best_match ||
		    (error == best_match &&
		    cap->cc_sessions < best->cc_sessions)) {
			best = cap;
			best_match = error;
		}
	}
	return best;
}

static enum alg_type {
	ALG_NONE = 0,
	ALG_CIPHER,
	ALG_DIGEST,
	ALG_KEYED_DIGEST,
	ALG_COMPRESSION,
	ALG_AEAD
} alg_types[] = {
	[CRYPTO_SHA1_HMAC] = ALG_KEYED_DIGEST,
	[CRYPTO_RIPEMD160_HMAC] = ALG_KEYED_DIGEST,
	[CRYPTO_AES_CBC] = ALG_CIPHER,
	[CRYPTO_SHA1] = ALG_DIGEST,
	[CRYPTO_NULL_HMAC] = ALG_DIGEST,
	[CRYPTO_NULL_CBC] = ALG_CIPHER,
	[CRYPTO_DEFLATE_COMP] = ALG_COMPRESSION,
	[CRYPTO_SHA2_256_HMAC] = ALG_KEYED_DIGEST,
	[CRYPTO_SHA2_384_HMAC] = ALG_KEYED_DIGEST,
	[CRYPTO_SHA2_512_HMAC] = ALG_KEYED_DIGEST,
	[CRYPTO_CAMELLIA_CBC] = ALG_CIPHER,
	[CRYPTO_AES_XTS] = ALG_CIPHER,
	[CRYPTO_AES_ICM] = ALG_CIPHER,
	[CRYPTO_AES_NIST_GMAC] = ALG_KEYED_DIGEST,
	[CRYPTO_AES_NIST_GCM_16] = ALG_AEAD,
	[CRYPTO_BLAKE2B] = ALG_KEYED_DIGEST,
	[CRYPTO_BLAKE2S] = ALG_KEYED_DIGEST,
	[CRYPTO_CHACHA20] = ALG_CIPHER,
	[CRYPTO_SHA2_224_HMAC] = ALG_KEYED_DIGEST,
	[CRYPTO_RIPEMD160] = ALG_DIGEST,
	[CRYPTO_SHA2_224] = ALG_DIGEST,
	[CRYPTO_SHA2_256] = ALG_DIGEST,
	[CRYPTO_SHA2_384] = ALG_DIGEST,
	[CRYPTO_SHA2_512] = ALG_DIGEST,
	[CRYPTO_POLY1305] = ALG_KEYED_DIGEST,
	[CRYPTO_AES_CCM_CBC_MAC] = ALG_KEYED_DIGEST,
	[CRYPTO_AES_CCM_16] = ALG_AEAD,
};

static enum alg_type
alg_type(int alg)
{

	if (alg < nitems(alg_types))
		return (alg_types[alg]);
	return (ALG_NONE);
}

static bool
alg_is_compression(int alg)
{

	return (alg_type(alg) == ALG_COMPRESSION);
}

static bool
alg_is_cipher(int alg)
{

	return (alg_type(alg) == ALG_CIPHER);
}

static bool
alg_is_digest(int alg)
{

	return (alg_type(alg) == ALG_DIGEST ||
	    alg_type(alg) == ALG_KEYED_DIGEST);
}

static bool
alg_is_keyed_digest(int alg)
{

	return (alg_type(alg) == ALG_KEYED_DIGEST);
}

static bool
alg_is_aead(int alg)
{

	return (alg_type(alg) == ALG_AEAD);
}

/* Various sanity checks on crypto session parameters. */
static bool
check_csp(const struct crypto_session_params *csp)
{
	struct auth_hash *axf;

	/* Mode-independent checks. */
	if ((csp->csp_flags & ~(CSP_F_SEPARATE_OUTPUT | CSP_F_SEPARATE_AAD)) !=
	    0)
		return (false);
	if (csp->csp_ivlen < 0 || csp->csp_cipher_klen < 0 ||
	    csp->csp_auth_klen < 0 || csp->csp_auth_mlen < 0)
		return (false);
	if (csp->csp_auth_key != NULL && csp->csp_auth_klen == 0)
		return (false);
	if (csp->csp_cipher_key != NULL && csp->csp_cipher_klen == 0)
		return (false);

	switch (csp->csp_mode) {
	case CSP_MODE_COMPRESS:
		if (!alg_is_compression(csp->csp_cipher_alg))
			return (false);
		if (csp->csp_flags & CSP_F_SEPARATE_OUTPUT)
			return (false);
		if (csp->csp_flags & CSP_F_SEPARATE_AAD)
			return (false);
		if (csp->csp_cipher_klen != 0 || csp->csp_ivlen != 0 ||
		    csp->csp_auth_alg != 0 || csp->csp_auth_klen != 0 ||
		    csp->csp_auth_mlen != 0)
			return (false);
		break;
	case CSP_MODE_CIPHER:
		if (!alg_is_cipher(csp->csp_cipher_alg))
			return (false);
		if (csp->csp_flags & CSP_F_SEPARATE_AAD)
			return (false);
		if (csp->csp_cipher_alg != CRYPTO_NULL_CBC) {
			if (csp->csp_cipher_klen == 0)
				return (false);
			if (csp->csp_ivlen == 0)
				return (false);
		}
		if (csp->csp_ivlen >= EALG_MAX_BLOCK_LEN)
			return (false);
		if (csp->csp_auth_alg != 0 || csp->csp_auth_klen != 0 ||
		    csp->csp_auth_mlen != 0)
			return (false);
		break;
	case CSP_MODE_DIGEST:
		if (csp->csp_cipher_alg != 0 || csp->csp_cipher_klen != 0)
			return (false);

		if (csp->csp_flags & CSP_F_SEPARATE_AAD)
			return (false);

		/* IV is optional for digests (e.g. GMAC). */
		if (csp->csp_ivlen >= EALG_MAX_BLOCK_LEN)
			return (false);
		if (!alg_is_digest(csp->csp_auth_alg))
			return (false);

		/* Key is optional for BLAKE2 digests. */
		if (csp->csp_auth_alg == CRYPTO_BLAKE2B ||
		    csp->csp_auth_alg == CRYPTO_BLAKE2S)
			;
		else if (alg_is_keyed_digest(csp->csp_auth_alg)) {
			if (csp->csp_auth_klen == 0)
				return (false);
		} else {
			if (csp->csp_auth_klen != 0)
				return (false);
		}
		if (csp->csp_auth_mlen != 0) {
			axf = crypto_auth_hash(csp);
			if (axf == NULL || csp->csp_auth_mlen > axf->hashsize)
				return (false);
		}
		break;
	case CSP_MODE_AEAD:
		if (!alg_is_aead(csp->csp_cipher_alg))
			return (false);
		if (csp->csp_cipher_klen == 0)
			return (false);
		if (csp->csp_ivlen == 0 ||
		    csp->csp_ivlen >= EALG_MAX_BLOCK_LEN)
			return (false);
		if (csp->csp_auth_alg != 0 || csp->csp_auth_klen != 0)
			return (false);

		/*
		 * XXX: Would be nice to have a better way to get this
		 * value.
		 */
		switch (csp->csp_cipher_alg) {
		case CRYPTO_AES_NIST_GCM_16:
		case CRYPTO_AES_CCM_16:
			if (csp->csp_auth_mlen > 16)
				return (false);
			break;
		}
		break;
	case CSP_MODE_ETA:
		if (!alg_is_cipher(csp->csp_cipher_alg))
			return (false);
		if (csp->csp_cipher_alg != CRYPTO_NULL_CBC) {
			if (csp->csp_cipher_klen == 0)
				return (false);
			if (csp->csp_ivlen == 0)
				return (false);
		}
		if (csp->csp_ivlen >= EALG_MAX_BLOCK_LEN)
			return (false);
		if (!alg_is_digest(csp->csp_auth_alg))
			return (false);

		/* Key is optional for BLAKE2 digests. */
		if (csp->csp_auth_alg == CRYPTO_BLAKE2B ||
		    csp->csp_auth_alg == CRYPTO_BLAKE2S)
			;
		else if (alg_is_keyed_digest(csp->csp_auth_alg)) {
			if (csp->csp_auth_klen == 0)
				return (false);
		} else {
			if (csp->csp_auth_klen != 0)
				return (false);
		}
		if (csp->csp_auth_mlen != 0) {
			axf = crypto_auth_hash(csp);
			if (axf == NULL || csp->csp_auth_mlen > axf->hashsize)
				return (false);
		}
		break;
	default:
		return (false);
	}

	return (true);
}

/*
 * Delete a session after it has been detached from its driver.
 */
static void
crypto_deletesession(crypto_session_t cses)
{
	struct cryptocap *cap;

	cap = cses->cap;

	zfree(cses->softc, M_CRYPTO_DATA);
	uma_zfree(cryptoses_zone, cses);

	CRYPTO_DRIVER_LOCK();
	cap->cc_sessions--;
	if (cap->cc_sessions == 0 && cap->cc_flags & CRYPTOCAP_F_CLEANUP)
		wakeup(cap);
	CRYPTO_DRIVER_UNLOCK();
	cap_rele(cap);
}

/*
 * Create a new session.  The crid argument specifies a crypto
 * driver to use or constraints on a driver to select (hardware
 * only, software only, either).  Whatever driver is selected
 * must be capable of the requested crypto algorithms.
 */
int
crypto_newsession(crypto_session_t *cses,
    const struct crypto_session_params *csp, int crid)
{
	crypto_session_t res;
	struct cryptocap *cap;
	int err;

	if (!check_csp(csp))
		return (EINVAL);

	res = NULL;

	CRYPTO_DRIVER_LOCK();
	if ((crid & (CRYPTOCAP_F_HARDWARE | CRYPTOCAP_F_SOFTWARE)) == 0) {
		/*
		 * Use specified driver; verify it is capable.
		 */
		cap = crypto_checkdriver(crid);
		if (cap != NULL && CRYPTODEV_PROBESESSION(cap->cc_dev, csp) > 0)
			cap = NULL;
	} else {
		/*
		 * No requested driver; select based on crid flags.
		 */
		cap = crypto_select_driver(csp, crid);
	}
	if (cap == NULL) {
		CRYPTO_DRIVER_UNLOCK();
		CRYPTDEB("no driver");
		return (EOPNOTSUPP);
	}
	cap_ref(cap);
	cap->cc_sessions++;
	CRYPTO_DRIVER_UNLOCK();

	res = uma_zalloc(cryptoses_zone, M_WAITOK | M_ZERO);
	res->cap = cap;
	res->softc = malloc(cap->cc_session_size, M_CRYPTO_DATA, M_WAITOK |
	    M_ZERO);
	res->csp = *csp;

	/* Call the driver initialization routine. */
	err = CRYPTODEV_NEWSESSION(cap->cc_dev, res, csp);
	if (err != 0) {
		CRYPTDEB("dev newsession failed: %d", err);
		crypto_deletesession(res);
		return (err);
	}

	*cses = res;
	return (0);
}

/*
 * Delete an existing session (or a reserved session on an unregistered
 * driver).
 */
void
crypto_freesession(crypto_session_t cses)
{
	struct cryptocap *cap;

	if (cses == NULL)
		return;

	cap = cses->cap;

	/* Call the driver cleanup routine, if available. */
	CRYPTODEV_FREESESSION(cap->cc_dev, cses);

	crypto_deletesession(cses);
}

/*
 * Return a new driver id.  Registers a driver with the system so that
 * it can be probed by subsequent sessions.
 */
int32_t
crypto_get_driverid(device_t dev, size_t sessionsize, int flags)
{
	struct cryptocap *cap, **newdrv;
	int i;

	if ((flags & (CRYPTOCAP_F_HARDWARE | CRYPTOCAP_F_SOFTWARE)) == 0) {
		device_printf(dev,
		    "no flags specified when registering driver\n");
		return -1;
	}

	cap = malloc(sizeof(*cap), M_CRYPTO_DATA, M_WAITOK | M_ZERO);
	cap->cc_dev = dev;
	cap->cc_session_size = sessionsize;
	cap->cc_flags = flags;
	refcount_init(&cap->cc_refs, 1);

	CRYPTO_DRIVER_LOCK();
	for (;;) {
		for (i = 0; i < crypto_drivers_size; i++) {
			if (crypto_drivers[i] == NULL)
				break;
		}

		if (i < crypto_drivers_size)
			break;

		/* Out of entries, allocate some more. */

		if (2 * crypto_drivers_size <= crypto_drivers_size) {
			CRYPTO_DRIVER_UNLOCK();
			printf("crypto: driver count wraparound!\n");
			cap_rele(cap);
			return (-1);
		}
		CRYPTO_DRIVER_UNLOCK();

		newdrv = malloc(2 * crypto_drivers_size *
		    sizeof(*crypto_drivers), M_CRYPTO_DATA, M_WAITOK | M_ZERO);

		CRYPTO_DRIVER_LOCK();
		memcpy(newdrv, crypto_drivers,
		    crypto_drivers_size * sizeof(*crypto_drivers));

		crypto_drivers_size *= 2;

		free(crypto_drivers, M_CRYPTO_DATA);
		crypto_drivers = newdrv;
	}

	cap->cc_hid = i;
	crypto_drivers[i] = cap;
	CRYPTO_DRIVER_UNLOCK();

	if (bootverbose)
		printf("crypto: assign %s driver id %u, flags 0x%x\n",
		    device_get_nameunit(dev), i, flags);

	return i;
}

/*
 * Lookup a driver by name.  We match against the full device
 * name and unit, and against just the name.  The latter gives
 * us a simple widlcarding by device name.  On success return the
 * driver/hardware identifier; otherwise return -1.
 */
int
crypto_find_driver(const char *match)
{
	struct cryptocap *cap;
	int i, len = strlen(match);

	CRYPTO_DRIVER_LOCK();
	for (i = 0; i < crypto_drivers_size; i++) {
		if (crypto_drivers[i] == NULL)
			continue;
		cap = crypto_drivers[i];
		if (strncmp(match, device_get_nameunit(cap->cc_dev), len) == 0 ||
		    strncmp(match, device_get_name(cap->cc_dev), len) == 0) {
			CRYPTO_DRIVER_UNLOCK();
			return (i);
		}
	}
	CRYPTO_DRIVER_UNLOCK();
	return (-1);
}

/*
 * Return the device_t for the specified driver or NULL
 * if the driver identifier is invalid.
 */
device_t
crypto_find_device_byhid(int hid)
{
	struct cryptocap *cap;
	device_t dev;

	dev = NULL;
	CRYPTO_DRIVER_LOCK();
	cap = crypto_checkdriver(hid);
	if (cap != NULL)
		dev = cap->cc_dev;
	CRYPTO_DRIVER_UNLOCK();
	return (dev);
}

/*
 * Return the device/driver capabilities.
 */
int
crypto_getcaps(int hid)
{
	struct cryptocap *cap;
	int flags;

	flags = 0;
	CRYPTO_DRIVER_LOCK();
	cap = crypto_checkdriver(hid);
	if (cap != NULL)
		flags = cap->cc_flags;
	CRYPTO_DRIVER_UNLOCK();
	return (flags);
}

/*
 * Register support for a key-related algorithm.  This routine
 * is called once for each algorithm supported a driver.
 */
int
crypto_kregister(u_int32_t driverid, int kalg, u_int32_t flags)
{
	struct cryptocap *cap;
	int err;

	CRYPTO_DRIVER_LOCK();

	cap = crypto_checkdriver(driverid);
	if (cap != NULL &&
	    (CRK_ALGORITM_MIN <= kalg && kalg <= CRK_ALGORITHM_MAX)) {
		/*
		 * XXX Do some performance testing to determine placing.
		 * XXX We probably need an auxiliary data structure that
		 * XXX describes relative performances.
		 */

		cap->cc_kalg[kalg] = flags | CRYPTO_ALG_FLAG_SUPPORTED;
		if (bootverbose)
			printf("crypto: %s registers key alg %u flags %u\n"
				, device_get_nameunit(cap->cc_dev)
				, kalg
				, flags
			);
		err = 0;
	} else
		err = EINVAL;

	CRYPTO_DRIVER_UNLOCK();
	return err;
}

/*
 * Unregister all algorithms associated with a crypto driver.
 * If there are pending sessions using it, leave enough information
 * around so that subsequent calls using those sessions will
 * correctly detect the driver has been unregistered and reroute
 * requests.
 */
int
crypto_unregister_all(u_int32_t driverid)
{
	struct cryptocap *cap;

	CRYPTO_DRIVER_LOCK();
	cap = crypto_checkdriver(driverid);
	if (cap == NULL) {
		CRYPTO_DRIVER_UNLOCK();
		return (EINVAL);
	}

	cap->cc_flags |= CRYPTOCAP_F_CLEANUP;
	crypto_drivers[driverid] = NULL;

	/*
	 * XXX: This doesn't do anything to kick sessions that
	 * have no pending operations.
	 */
	while (cap->cc_sessions != 0 || cap->cc_koperations != 0)
		mtx_sleep(cap, &crypto_drivers_mtx, 0, "cryunreg", 0);
	CRYPTO_DRIVER_UNLOCK();
	cap_rele(cap);

	return (0);
}

/*
 * Clear blockage on a driver.  The what parameter indicates whether
 * the driver is now ready for cryptop's and/or cryptokop's.
 */
int
crypto_unblock(u_int32_t driverid, int what)
{
	struct cryptocap *cap;
	int err;

	CRYPTO_Q_LOCK();
	cap = crypto_checkdriver(driverid);
	if (cap != NULL) {
		if (what & CRYPTO_SYMQ)
			cap->cc_qblocked = 0;
		if (what & CRYPTO_ASYMQ)
			cap->cc_kqblocked = 0;
		if (crp_sleep)
			wakeup_one(&crp_q);
		err = 0;
	} else
		err = EINVAL;
	CRYPTO_Q_UNLOCK();

	return err;
}

size_t
crypto_buffer_len(struct crypto_buffer *cb)
{
	switch (cb->cb_type) {
	case CRYPTO_BUF_CONTIG:
		return (cb->cb_buf_len);
	case CRYPTO_BUF_MBUF:
		if (cb->cb_mbuf->m_flags & M_PKTHDR)
			return (cb->cb_mbuf->m_pkthdr.len);
		return (m_length(cb->cb_mbuf, NULL));
	case CRYPTO_BUF_UIO:
		return (cb->cb_uio->uio_resid);
	default:
		return (0);
	}
}

#ifdef INVARIANTS
/* Various sanity checks on crypto requests. */
static void
cb_sanity(struct crypto_buffer *cb, const char *name)
{
	KASSERT(cb->cb_type > CRYPTO_BUF_NONE && cb->cb_type <= CRYPTO_BUF_LAST,
	    ("incoming crp with invalid %s buffer type", name));
	if (cb->cb_type == CRYPTO_BUF_CONTIG)
		KASSERT(cb->cb_buf_len >= 0,
		    ("incoming crp with -ve %s buffer length", name));
}

static void
crp_sanity(struct cryptop *crp)
{
	struct crypto_session_params *csp;
	struct crypto_buffer *out;
	size_t ilen, len, olen;

	KASSERT(crp->crp_session != NULL, ("incoming crp without a session"));
	KASSERT(crp->crp_obuf.cb_type >= CRYPTO_BUF_NONE &&
	    crp->crp_obuf.cb_type <= CRYPTO_BUF_LAST,
	    ("incoming crp with invalid output buffer type"));
	KASSERT(crp->crp_etype == 0, ("incoming crp with error"));
	KASSERT(!(crp->crp_flags & CRYPTO_F_DONE),
	    ("incoming crp already done"));

	csp = &crp->crp_session->csp;
	cb_sanity(&crp->crp_buf, "input");
	ilen = crypto_buffer_len(&crp->crp_buf);
	olen = ilen;
	out = NULL;
	if (csp->csp_flags & CSP_F_SEPARATE_OUTPUT) {
		if (crp->crp_obuf.cb_type != CRYPTO_BUF_NONE) {
			cb_sanity(&crp->crp_obuf, "output");
			out = &crp->crp_obuf;
			olen = crypto_buffer_len(out);
		}
	} else
		KASSERT(crp->crp_obuf.cb_type == CRYPTO_BUF_NONE,
		    ("incoming crp with separate output buffer "
		    "but no session support"));

	switch (csp->csp_mode) {
	case CSP_MODE_COMPRESS:
		KASSERT(crp->crp_op == CRYPTO_OP_COMPRESS ||
		    crp->crp_op == CRYPTO_OP_DECOMPRESS,
		    ("invalid compression op %x", crp->crp_op));
		break;
	case CSP_MODE_CIPHER:
		KASSERT(crp->crp_op == CRYPTO_OP_ENCRYPT ||
		    crp->crp_op == CRYPTO_OP_DECRYPT,
		    ("invalid cipher op %x", crp->crp_op));
		break;
	case CSP_MODE_DIGEST:
		KASSERT(crp->crp_op == CRYPTO_OP_COMPUTE_DIGEST ||
		    crp->crp_op == CRYPTO_OP_VERIFY_DIGEST,
		    ("invalid digest op %x", crp->crp_op));
		break;
	case CSP_MODE_AEAD:
		KASSERT(crp->crp_op ==
		    (CRYPTO_OP_ENCRYPT | CRYPTO_OP_COMPUTE_DIGEST) ||
		    crp->crp_op ==
		    (CRYPTO_OP_DECRYPT | CRYPTO_OP_VERIFY_DIGEST),
		    ("invalid AEAD op %x", crp->crp_op));
		if (csp->csp_cipher_alg == CRYPTO_AES_NIST_GCM_16)
			KASSERT(crp->crp_flags & CRYPTO_F_IV_SEPARATE,
			    ("GCM without a separate IV"));
		if (csp->csp_cipher_alg == CRYPTO_AES_CCM_16)
			KASSERT(crp->crp_flags & CRYPTO_F_IV_SEPARATE,
			    ("CCM without a separate IV"));
		break;
	case CSP_MODE_ETA:
		KASSERT(crp->crp_op ==
		    (CRYPTO_OP_ENCRYPT | CRYPTO_OP_COMPUTE_DIGEST) ||
		    crp->crp_op ==
		    (CRYPTO_OP_DECRYPT | CRYPTO_OP_VERIFY_DIGEST),
		    ("invalid ETA op %x", crp->crp_op));
		break;
	}
	if (csp->csp_mode == CSP_MODE_AEAD || csp->csp_mode == CSP_MODE_ETA) {
		if (crp->crp_aad == NULL) {
			KASSERT(crp->crp_aad_start == 0 ||
			    crp->crp_aad_start < ilen,
			    ("invalid AAD start"));
			KASSERT(crp->crp_aad_length != 0 ||
			    crp->crp_aad_start == 0,
			    ("AAD with zero length and non-zero start"));
			KASSERT(crp->crp_aad_length == 0 ||
			    crp->crp_aad_start + crp->crp_aad_length <= ilen,
			    ("AAD outside input length"));
		} else {
			KASSERT(csp->csp_flags & CSP_F_SEPARATE_AAD,
			    ("session doesn't support separate AAD buffer"));
			KASSERT(crp->crp_aad_start == 0,
			    ("separate AAD buffer with non-zero AAD start"));
			KASSERT(crp->crp_aad_length != 0,
			    ("separate AAD buffer with zero length"));
		}
	} else {
		KASSERT(crp->crp_aad == NULL && crp->crp_aad_start == 0 &&
		    crp->crp_aad_length == 0,
		    ("AAD region in request not supporting AAD"));
	}
	if (csp->csp_ivlen == 0) {
		KASSERT((crp->crp_flags & CRYPTO_F_IV_SEPARATE) == 0,
		    ("IV_SEPARATE set when IV isn't used"));
		KASSERT(crp->crp_iv_start == 0,
		    ("crp_iv_start set when IV isn't used"));
	} else if (crp->crp_flags & CRYPTO_F_IV_SEPARATE) {
		KASSERT(crp->crp_iv_start == 0,
		    ("IV_SEPARATE used with non-zero IV start"));
	} else {
		KASSERT(crp->crp_iv_start < ilen,
		    ("invalid IV start"));
		KASSERT(crp->crp_iv_start + csp->csp_ivlen <= ilen,
		    ("IV outside buffer length"));
	}
	/* XXX: payload_start of 0 should always be < ilen? */
	KASSERT(crp->crp_payload_start == 0 ||
	    crp->crp_payload_start < ilen,
	    ("invalid payload start"));
	KASSERT(crp->crp_payload_start + crp->crp_payload_length <=
	    ilen, ("payload outside input buffer"));
	if (out == NULL) {
		KASSERT(crp->crp_payload_output_start == 0,
		    ("payload output start non-zero without output buffer"));
	} else {
		KASSERT(crp->crp_payload_output_start < olen,
		    ("invalid payload output start"));
		KASSERT(crp->crp_payload_output_start +
		    crp->crp_payload_length <= olen,
		    ("payload outside output buffer"));
	}
	if (csp->csp_mode == CSP_MODE_DIGEST ||
	    csp->csp_mode == CSP_MODE_AEAD || csp->csp_mode == CSP_MODE_ETA) {
		if (crp->crp_op & CRYPTO_OP_VERIFY_DIGEST)
			len = ilen;
		else
			len = olen;
		KASSERT(crp->crp_digest_start == 0 ||
		    crp->crp_digest_start < len,
		    ("invalid digest start"));
		/* XXX: For the mlen == 0 case this check isn't perfect. */
		KASSERT(crp->crp_digest_start + csp->csp_auth_mlen <= len,
		    ("digest outside buffer"));
	} else {
		KASSERT(crp->crp_digest_start == 0,
		    ("non-zero digest start for request without a digest"));
	}
	if (csp->csp_cipher_klen != 0)
		KASSERT(csp->csp_cipher_key != NULL ||
		    crp->crp_cipher_key != NULL,
		    ("cipher request without a key"));
	if (csp->csp_auth_klen != 0)
		KASSERT(csp->csp_auth_key != NULL || crp->crp_auth_key != NULL,
		    ("auth request without a key"));
	KASSERT(crp->crp_callback != NULL, ("incoming crp without callback"));
}
#endif

/*
 * Add a crypto request to a queue, to be processed by the kernel thread.
 */
int
crypto_dispatch(struct cryptop *crp)
{
	struct cryptocap *cap;
	int result;

#ifdef INVARIANTS
	crp_sanity(crp);
#endif

	CRYPTOSTAT_INC(cs_ops);

	crp->crp_retw_id = ((uintptr_t)crp->crp_session) % crypto_workers_num;

	if (CRYPTOP_ASYNC(crp)) {
		if (crp->crp_flags & CRYPTO_F_ASYNC_KEEPORDER) {
			struct crypto_ret_worker *ret_worker;

			ret_worker = CRYPTO_RETW(crp->crp_retw_id);

			CRYPTO_RETW_LOCK(ret_worker);
			crp->crp_seq = ret_worker->reorder_ops++;
			CRYPTO_RETW_UNLOCK(ret_worker);
		}

		TASK_INIT(&crp->crp_task, 0, crypto_task_invoke, crp);
		taskqueue_enqueue(crypto_tq, &crp->crp_task);
		return (0);
	}

	if ((crp->crp_flags & CRYPTO_F_BATCH) == 0) {
		/*
		 * Caller marked the request to be processed
		 * immediately; dispatch it directly to the
		 * driver unless the driver is currently blocked.
		 */
		cap = crp->crp_session->cap;
		if (!cap->cc_qblocked) {
			result = crypto_invoke(cap, crp, 0);
			if (result != ERESTART)
				return (result);
			/*
			 * The driver ran out of resources, put the request on
			 * the queue.
			 */
		}
	}
	crypto_batch_enqueue(crp);
	return 0;
}

void
crypto_batch_enqueue(struct cryptop *crp)
{

	CRYPTO_Q_LOCK();
	TAILQ_INSERT_TAIL(&crp_q, crp, crp_next);
	if (crp_sleep)
		wakeup_one(&crp_q);
	CRYPTO_Q_UNLOCK();
}

/*
 * Add an asymetric crypto request to a queue,
 * to be processed by the kernel thread.
 */
int
crypto_kdispatch(struct cryptkop *krp)
{
	int error;

	CRYPTOSTAT_INC(cs_kops);

	krp->krp_cap = NULL;
	error = crypto_kinvoke(krp);
	if (error == ERESTART) {
		CRYPTO_Q_LOCK();
		TAILQ_INSERT_TAIL(&crp_kq, krp, krp_next);
		if (crp_sleep)
			wakeup_one(&crp_q);
		CRYPTO_Q_UNLOCK();
		error = 0;
	}
	return error;
}

/*
 * Verify a driver is suitable for the specified operation.
 */
static __inline int
kdriver_suitable(const struct cryptocap *cap, const struct cryptkop *krp)
{
	return (cap->cc_kalg[krp->krp_op] & CRYPTO_ALG_FLAG_SUPPORTED) != 0;
}

/*
 * Select a driver for an asym operation.  The driver must
 * support the necessary algorithm.  The caller can constrain
 * which device is selected with the flags parameter.  The
 * algorithm we use here is pretty stupid; just use the first
 * driver that supports the algorithms we need. If there are
 * multiple suitable drivers we choose the driver with the
 * fewest active operations.  We prefer hardware-backed
 * drivers to software ones when either may be used.
 */
static struct cryptocap *
crypto_select_kdriver(const struct cryptkop *krp, int flags)
{
	struct cryptocap *cap, *best;
	int match, hid;

	CRYPTO_DRIVER_ASSERT();

	/*
	 * Look first for hardware crypto devices if permitted.
	 */
	if (flags & CRYPTOCAP_F_HARDWARE)
		match = CRYPTOCAP_F_HARDWARE;
	else
		match = CRYPTOCAP_F_SOFTWARE;
	best = NULL;
again:
	for (hid = 0; hid < crypto_drivers_size; hid++) {
		/*
		 * If there is no driver for this slot, or the driver
		 * is not appropriate (hardware or software based on
		 * match), then skip.
		 */
		cap = crypto_drivers[hid];
		if (cap->cc_dev == NULL ||
		    (cap->cc_flags & match) == 0)
			continue;

		/* verify all the algorithms are supported. */
		if (kdriver_suitable(cap, krp)) {
			if (best == NULL ||
			    cap->cc_koperations < best->cc_koperations)
				best = cap;
		}
	}
	if (best != NULL)
		return best;
	if (match == CRYPTOCAP_F_HARDWARE && (flags & CRYPTOCAP_F_SOFTWARE)) {
		/* sort of an Algol 68-style for loop */
		match = CRYPTOCAP_F_SOFTWARE;
		goto again;
	}
	return best;
}

/*
 * Choose a driver for an asymmetric crypto request.
 */
static struct cryptocap *
crypto_lookup_kdriver(struct cryptkop *krp)
{
	struct cryptocap *cap;
	uint32_t crid;

	/* If this request is requeued, it might already have a driver. */
	cap = krp->krp_cap;
	if (cap != NULL)
		return (cap);

	/* Use krp_crid to choose a driver. */
	crid = krp->krp_crid;
	if ((crid & (CRYPTOCAP_F_HARDWARE | CRYPTOCAP_F_SOFTWARE)) == 0) {
		cap = crypto_checkdriver(crid);
		if (cap != NULL) {
			/*
			 * Driver present, it must support the
			 * necessary algorithm and, if s/w drivers are
			 * excluded, it must be registered as
			 * hardware-backed.
			 */
			if (!kdriver_suitable(cap, krp) ||
			    (!crypto_devallowsoft &&
			    (cap->cc_flags & CRYPTOCAP_F_HARDWARE) == 0))
				cap = NULL;
		}
	} else {
		/*
		 * No requested driver; select based on crid flags.
		 */
		if (!crypto_devallowsoft)	/* NB: disallow s/w drivers */
			crid &= ~CRYPTOCAP_F_SOFTWARE;
		cap = crypto_select_kdriver(krp, crid);
	}

	if (cap != NULL) {
		krp->krp_cap = cap_ref(cap);
		krp->krp_hid = cap->cc_hid;
	}
	return (cap);
}

/*
 * Dispatch an asymmetric crypto request.
 */
static int
crypto_kinvoke(struct cryptkop *krp)
{
	struct cryptocap *cap = NULL;
	int error;

	KASSERT(krp != NULL, ("%s: krp == NULL", __func__));
	KASSERT(krp->krp_callback != NULL,
	    ("%s: krp->crp_callback == NULL", __func__));

	CRYPTO_DRIVER_LOCK();
	cap = crypto_lookup_kdriver(krp);
	if (cap == NULL) {
		CRYPTO_DRIVER_UNLOCK();
		krp->krp_status = ENODEV;
		crypto_kdone(krp);
		return (0);
	}

	/*
	 * If the device is blocked, return ERESTART to requeue it.
	 */
	if (cap->cc_kqblocked) {
		/*
		 * XXX: Previously this set krp_status to ERESTART and
		 * invoked crypto_kdone but the caller would still
		 * requeue it.
		 */
		CRYPTO_DRIVER_UNLOCK();
		return (ERESTART);
	}

	cap->cc_koperations++;
	CRYPTO_DRIVER_UNLOCK();
	error = CRYPTODEV_KPROCESS(cap->cc_dev, krp, 0);
	if (error == ERESTART) {
		CRYPTO_DRIVER_LOCK();
		cap->cc_koperations--;
		CRYPTO_DRIVER_UNLOCK();
		return (error);
	}

	KASSERT(error == 0, ("error %d returned from crypto_kprocess", error));
	return (0);
}

static void
crypto_task_invoke(void *ctx, int pending)
{
	struct cryptocap *cap;
	struct cryptop *crp;
	int result;

	crp = (struct cryptop *)ctx;
	cap = crp->crp_session->cap;
	result = crypto_invoke(cap, crp, 0);
	if (result == ERESTART)
		crypto_batch_enqueue(crp);
}

/*
 * Dispatch a crypto request to the appropriate crypto devices.
 */
static int
crypto_invoke(struct cryptocap *cap, struct cryptop *crp, int hint)
{

	KASSERT(crp != NULL, ("%s: crp == NULL", __func__));
	KASSERT(crp->crp_callback != NULL,
	    ("%s: crp->crp_callback == NULL", __func__));
	KASSERT(crp->crp_session != NULL,
	    ("%s: crp->crp_session == NULL", __func__));

	if (cap->cc_flags & CRYPTOCAP_F_CLEANUP) {
		struct crypto_session_params csp;
		crypto_session_t nses;

		/*
		 * Driver has unregistered; migrate the session and return
		 * an error to the caller so they'll resubmit the op.
		 *
		 * XXX: What if there are more already queued requests for this
		 *      session?
		 *
		 * XXX: Real solution is to make sessions refcounted
		 * and force callers to hold a reference when
		 * assigning to crp_session.  Could maybe change
		 * crypto_getreq to accept a session pointer to make
		 * that work.  Alternatively, we could abandon the
		 * notion of rewriting crp_session in requests forcing
		 * the caller to deal with allocating a new session.
		 * Perhaps provide a method to allow a crp's session to
		 * be swapped that callers could use.
		 */
		csp = crp->crp_session->csp;
		crypto_freesession(crp->crp_session);

		/*
		 * XXX: Key pointers may no longer be valid.  If we
		 * really want to support this we need to define the
		 * KPI such that 'csp' is required to be valid for the
		 * duration of a session by the caller perhaps.
		 *
		 * XXX: If the keys have been changed this will reuse
		 * the old keys.  This probably suggests making
		 * rekeying more explicit and updating the key
		 * pointers in 'csp' when the keys change.
		 */
		if (crypto_newsession(&nses, &csp,
		    CRYPTOCAP_F_HARDWARE | CRYPTOCAP_F_SOFTWARE) == 0)
			crp->crp_session = nses;

		crp->crp_etype = EAGAIN;
		crypto_done(crp);
		return 0;
	} else {
		/*
		 * Invoke the driver to process the request.
		 */
		return CRYPTODEV_PROCESS(cap->cc_dev, crp, hint);
	}
}

void
crypto_freereq(struct cryptop *crp)
{

	if (crp == NULL)
		return;

#ifdef DIAGNOSTIC
	{
		struct cryptop *crp2;
		struct crypto_ret_worker *ret_worker;

		CRYPTO_Q_LOCK();
		TAILQ_FOREACH(crp2, &crp_q, crp_next) {
			KASSERT(crp2 != crp,
			    ("Freeing cryptop from the crypto queue (%p).",
			    crp));
		}
		CRYPTO_Q_UNLOCK();

		FOREACH_CRYPTO_RETW(ret_worker) {
			CRYPTO_RETW_LOCK(ret_worker);
			TAILQ_FOREACH(crp2, &ret_worker->crp_ret_q, crp_next) {
				KASSERT(crp2 != crp,
				    ("Freeing cryptop from the return queue (%p).",
				    crp));
			}
			CRYPTO_RETW_UNLOCK(ret_worker);
		}
	}
#endif

	uma_zfree(cryptop_zone, crp);
}

struct cryptop *
crypto_getreq(crypto_session_t cses, int how)
{
	struct cryptop *crp;

	MPASS(how == M_WAITOK || how == M_NOWAIT);
	crp = uma_zalloc(cryptop_zone, how | M_ZERO);
	crp->crp_session = cses;
	return (crp);
}

/*
 * Invoke the callback on behalf of the driver.
 */
void
crypto_done(struct cryptop *crp)
{
	KASSERT((crp->crp_flags & CRYPTO_F_DONE) == 0,
		("crypto_done: op already done, flags 0x%x", crp->crp_flags));
	crp->crp_flags |= CRYPTO_F_DONE;
	if (crp->crp_etype != 0)
		CRYPTOSTAT_INC(cs_errs);

	/*
	 * CBIMM means unconditionally do the callback immediately;
	 * CBIFSYNC means do the callback immediately only if the
	 * operation was done synchronously.  Both are used to avoid
	 * doing extraneous context switches; the latter is mostly
	 * used with the software crypto driver.
	 */
	if (!CRYPTOP_ASYNC_KEEPORDER(crp) &&
	    ((crp->crp_flags & CRYPTO_F_CBIMM) ||
	    ((crp->crp_flags & CRYPTO_F_CBIFSYNC) &&
	     (crypto_ses2caps(crp->crp_session) & CRYPTOCAP_F_SYNC)))) {
		/*
		 * Do the callback directly.  This is ok when the
		 * callback routine does very little (e.g. the
		 * /dev/crypto callback method just does a wakeup).
		 */
		crp->crp_callback(crp);
	} else {
		struct crypto_ret_worker *ret_worker;
		bool wake;

		ret_worker = CRYPTO_RETW(crp->crp_retw_id);
		wake = false;

		/*
		 * Normal case; queue the callback for the thread.
		 */
		CRYPTO_RETW_LOCK(ret_worker);
		if (CRYPTOP_ASYNC_KEEPORDER(crp)) {
			struct cryptop *tmp;

			TAILQ_FOREACH_REVERSE(tmp, &ret_worker->crp_ordered_ret_q,
					cryptop_q, crp_next) {
				if (CRYPTO_SEQ_GT(crp->crp_seq, tmp->crp_seq)) {
					TAILQ_INSERT_AFTER(&ret_worker->crp_ordered_ret_q,
							tmp, crp, crp_next);
					break;
				}
			}
			if (tmp == NULL) {
				TAILQ_INSERT_HEAD(&ret_worker->crp_ordered_ret_q,
						crp, crp_next);
			}

			if (crp->crp_seq == ret_worker->reorder_cur_seq)
				wake = true;
		}
		else {
			if (CRYPTO_RETW_EMPTY(ret_worker))
				wake = true;

			TAILQ_INSERT_TAIL(&ret_worker->crp_ret_q, crp, crp_next);
		}

		if (wake)
			wakeup_one(&ret_worker->crp_ret_q);	/* shared wait channel */
		CRYPTO_RETW_UNLOCK(ret_worker);
	}
}

/*
 * Invoke the callback on behalf of the driver.
 */
void
crypto_kdone(struct cryptkop *krp)
{
	struct crypto_ret_worker *ret_worker;
	struct cryptocap *cap;

	if (krp->krp_status != 0)
		CRYPTOSTAT_INC(cs_kerrs);
	CRYPTO_DRIVER_LOCK();
	cap = krp->krp_cap;
	KASSERT(cap->cc_koperations > 0, ("cc_koperations == 0"));
	cap->cc_koperations--;
	if (cap->cc_koperations == 0 && cap->cc_flags & CRYPTOCAP_F_CLEANUP)
		wakeup(cap);
	CRYPTO_DRIVER_UNLOCK();
	krp->krp_cap = NULL;
	cap_rele(cap);

	ret_worker = CRYPTO_RETW(0);

	CRYPTO_RETW_LOCK(ret_worker);
	if (CRYPTO_RETW_EMPTY(ret_worker))
		wakeup_one(&ret_worker->crp_ret_q);		/* shared wait channel */
	TAILQ_INSERT_TAIL(&ret_worker->crp_ret_kq, krp, krp_next);
	CRYPTO_RETW_UNLOCK(ret_worker);
}

int
crypto_getfeat(int *featp)
{
	int hid, kalg, feat = 0;

	CRYPTO_DRIVER_LOCK();
	for (hid = 0; hid < crypto_drivers_size; hid++) {
		const struct cryptocap *cap = crypto_drivers[hid];

		if (cap == NULL ||
		    ((cap->cc_flags & CRYPTOCAP_F_SOFTWARE) &&
		    !crypto_devallowsoft)) {
			continue;
		}
		for (kalg = 0; kalg < CRK_ALGORITHM_MAX; kalg++)
			if (cap->cc_kalg[kalg] & CRYPTO_ALG_FLAG_SUPPORTED)
				feat |=  1 << kalg;
	}
	CRYPTO_DRIVER_UNLOCK();
	*featp = feat;
	return (0);
}

/*
 * Terminate a thread at module unload.  The process that
 * initiated this is waiting for us to signal that we're gone;
 * wake it up and exit.  We use the driver table lock to insure
 * we don't do the wakeup before they're waiting.  There is no
 * race here because the waiter sleeps on the proc lock for the
 * thread so it gets notified at the right time because of an
 * extra wakeup that's done in exit1().
 */
static void
crypto_finis(void *chan)
{
	CRYPTO_DRIVER_LOCK();
	wakeup_one(chan);
	CRYPTO_DRIVER_UNLOCK();
	kproc_exit(0);
}

/*
 * Crypto thread, dispatches crypto requests.
 */
static void
crypto_proc(void)
{
	struct cryptop *crp, *submit;
	struct cryptkop *krp;
	struct cryptocap *cap;
	int result, hint;

#if defined(__i386__) || defined(__amd64__) || defined(__aarch64__)
	fpu_kern_thread(FPU_KERN_NORMAL);
#endif

	CRYPTO_Q_LOCK();
	for (;;) {
		/*
		 * Find the first element in the queue that can be
		 * processed and look-ahead to see if multiple ops
		 * are ready for the same driver.
		 */
		submit = NULL;
		hint = 0;
		TAILQ_FOREACH(crp, &crp_q, crp_next) {
			cap = crp->crp_session->cap;
			/*
			 * Driver cannot disappeared when there is an active
			 * session.
			 */
			KASSERT(cap != NULL, ("%s:%u Driver disappeared.",
			    __func__, __LINE__));
			if (cap->cc_flags & CRYPTOCAP_F_CLEANUP) {
				/* Op needs to be migrated, process it. */
				if (submit == NULL)
					submit = crp;
				break;
			}
			if (!cap->cc_qblocked) {
				if (submit != NULL) {
					/*
					 * We stop on finding another op,
					 * regardless whether its for the same
					 * driver or not.  We could keep
					 * searching the queue but it might be
					 * better to just use a per-driver
					 * queue instead.
					 */
					if (submit->crp_session->cap == cap)
						hint = CRYPTO_HINT_MORE;
					break;
				} else {
					submit = crp;
					if ((submit->crp_flags & CRYPTO_F_BATCH) == 0)
						break;
					/* keep scanning for more are q'd */
				}
			}
		}
		if (submit != NULL) {
			TAILQ_REMOVE(&crp_q, submit, crp_next);
			cap = submit->crp_session->cap;
			KASSERT(cap != NULL, ("%s:%u Driver disappeared.",
			    __func__, __LINE__));
			CRYPTO_Q_UNLOCK();
			result = crypto_invoke(cap, submit, hint);
			CRYPTO_Q_LOCK();
			if (result == ERESTART) {
				/*
				 * The driver ran out of resources, mark the
				 * driver ``blocked'' for cryptop's and put
				 * the request back in the queue.  It would
				 * best to put the request back where we got
				 * it but that's hard so for now we put it
				 * at the front.  This should be ok; putting
				 * it at the end does not work.
				 */
				cap->cc_qblocked = 1;
				TAILQ_INSERT_HEAD(&crp_q, submit, crp_next);
				CRYPTOSTAT_INC(cs_blocks);
			}
		}

		/* As above, but for key ops */
		TAILQ_FOREACH(krp, &crp_kq, krp_next) {
			cap = krp->krp_cap;
			if (cap->cc_flags & CRYPTOCAP_F_CLEANUP) {
				/*
				 * Operation needs to be migrated,
				 * clear krp_cap so a new driver is
				 * selected.
				 */
				krp->krp_cap = NULL;
				cap_rele(cap);
				break;
			}
			if (!cap->cc_kqblocked)
				break;
		}
		if (krp != NULL) {
			TAILQ_REMOVE(&crp_kq, krp, krp_next);
			CRYPTO_Q_UNLOCK();
			result = crypto_kinvoke(krp);
			CRYPTO_Q_LOCK();
			if (result == ERESTART) {
				/*
				 * The driver ran out of resources, mark the
				 * driver ``blocked'' for cryptkop's and put
				 * the request back in the queue.  It would
				 * best to put the request back where we got
				 * it but that's hard so for now we put it
				 * at the front.  This should be ok; putting
				 * it at the end does not work.
				 */
				krp->krp_cap->cc_kqblocked = 1;
				TAILQ_INSERT_HEAD(&crp_kq, krp, krp_next);
				CRYPTOSTAT_INC(cs_kblocks);
			}
		}

		if (submit == NULL && krp == NULL) {
			/*
			 * Nothing more to be processed.  Sleep until we're
			 * woken because there are more ops to process.
			 * This happens either by submission or by a driver
			 * becoming unblocked and notifying us through
			 * crypto_unblock.  Note that when we wakeup we
			 * start processing each queue again from the
			 * front. It's not clear that it's important to
			 * preserve this ordering since ops may finish
			 * out of order if dispatched to different devices
			 * and some become blocked while others do not.
			 */
			crp_sleep = 1;
			msleep(&crp_q, &crypto_q_mtx, PWAIT, "crypto_wait", 0);
			crp_sleep = 0;
			if (cryptoproc == NULL)
				break;
			CRYPTOSTAT_INC(cs_intrs);
		}
	}
	CRYPTO_Q_UNLOCK();

	crypto_finis(&crp_q);
}

/*
 * Crypto returns thread, does callbacks for processed crypto requests.
 * Callbacks are done here, rather than in the crypto drivers, because
 * callbacks typically are expensive and would slow interrupt handling.
 */
static void
crypto_ret_proc(struct crypto_ret_worker *ret_worker)
{
	struct cryptop *crpt;
	struct cryptkop *krpt;

	CRYPTO_RETW_LOCK(ret_worker);
	for (;;) {
		/* Harvest return q's for completed ops */
		crpt = TAILQ_FIRST(&ret_worker->crp_ordered_ret_q);
		if (crpt != NULL) {
			if (crpt->crp_seq == ret_worker->reorder_cur_seq) {
				TAILQ_REMOVE(&ret_worker->crp_ordered_ret_q, crpt, crp_next);
				ret_worker->reorder_cur_seq++;
			} else {
				crpt = NULL;
			}
		}

		if (crpt == NULL) {
			crpt = TAILQ_FIRST(&ret_worker->crp_ret_q);
			if (crpt != NULL)
				TAILQ_REMOVE(&ret_worker->crp_ret_q, crpt, crp_next);
		}

		krpt = TAILQ_FIRST(&ret_worker->crp_ret_kq);
		if (krpt != NULL)
			TAILQ_REMOVE(&ret_worker->crp_ret_kq, krpt, krp_next);

		if (crpt != NULL || krpt != NULL) {
			CRYPTO_RETW_UNLOCK(ret_worker);
			/*
			 * Run callbacks unlocked.
			 */
			if (crpt != NULL)
				crpt->crp_callback(crpt);
			if (krpt != NULL)
				krpt->krp_callback(krpt);
			CRYPTO_RETW_LOCK(ret_worker);
		} else {
			/*
			 * Nothing more to be processed.  Sleep until we're
			 * woken because there are more returns to process.
			 */
			msleep(&ret_worker->crp_ret_q, &ret_worker->crypto_ret_mtx, PWAIT,
				"crypto_ret_wait", 0);
			if (ret_worker->cryptoretproc == NULL)
				break;
			CRYPTOSTAT_INC(cs_rets);
		}
	}
	CRYPTO_RETW_UNLOCK(ret_worker);

	crypto_finis(&ret_worker->crp_ret_q);
}

#ifdef DDB
static void
db_show_drivers(void)
{
	int hid;

	db_printf("%12s %4s %4s %8s %2s %2s\n"
		, "Device"
		, "Ses"
		, "Kops"
		, "Flags"
		, "QB"
		, "KB"
	);
	for (hid = 0; hid < crypto_drivers_size; hid++) {
		const struct cryptocap *cap = crypto_drivers[hid];
		if (cap == NULL)
			continue;
		db_printf("%-12s %4u %4u %08x %2u %2u\n"
		    , device_get_nameunit(cap->cc_dev)
		    , cap->cc_sessions
		    , cap->cc_koperations
		    , cap->cc_flags
		    , cap->cc_qblocked
		    , cap->cc_kqblocked
		);
	}
}

DB_SHOW_COMMAND(crypto, db_show_crypto)
{
	struct cryptop *crp;
	struct crypto_ret_worker *ret_worker;

	db_show_drivers();
	db_printf("\n");

	db_printf("%4s %8s %4s %4s %4s %4s %8s %8s\n",
	    "HID", "Caps", "Ilen", "Olen", "Etype", "Flags",
	    "Device", "Callback");
	TAILQ_FOREACH(crp, &crp_q, crp_next) {
		db_printf("%4u %08x %4u %4u %04x %8p %8p\n"
		    , crp->crp_session->cap->cc_hid
		    , (int) crypto_ses2caps(crp->crp_session)
		    , crp->crp_olen
		    , crp->crp_etype
		    , crp->crp_flags
		    , device_get_nameunit(crp->crp_session->cap->cc_dev)
		    , crp->crp_callback
		);
	}
	FOREACH_CRYPTO_RETW(ret_worker) {
		db_printf("\n%8s %4s %4s %4s %8s\n",
		    "ret_worker", "HID", "Etype", "Flags", "Callback");
		if (!TAILQ_EMPTY(&ret_worker->crp_ret_q)) {
			TAILQ_FOREACH(crp, &ret_worker->crp_ret_q, crp_next) {
				db_printf("%8td %4u %4u %04x %8p\n"
				    , CRYPTO_RETW_ID(ret_worker)
				    , crp->crp_session->cap->cc_hid
				    , crp->crp_etype
				    , crp->crp_flags
				    , crp->crp_callback
				);
			}
		}
	}
}

DB_SHOW_COMMAND(kcrypto, db_show_kcrypto)
{
	struct cryptkop *krp;
	struct crypto_ret_worker *ret_worker;

	db_show_drivers();
	db_printf("\n");

	db_printf("%4s %5s %4s %4s %8s %4s %8s\n",
	    "Op", "Status", "#IP", "#OP", "CRID", "HID", "Callback");
	TAILQ_FOREACH(krp, &crp_kq, krp_next) {
		db_printf("%4u %5u %4u %4u %08x %4u %8p\n"
		    , krp->krp_op
		    , krp->krp_status
		    , krp->krp_iparams, krp->krp_oparams
		    , krp->krp_crid, krp->krp_hid
		    , krp->krp_callback
		);
	}

	ret_worker = CRYPTO_RETW(0);
	if (!TAILQ_EMPTY(&ret_worker->crp_ret_q)) {
		db_printf("%4s %5s %8s %4s %8s\n",
		    "Op", "Status", "CRID", "HID", "Callback");
		TAILQ_FOREACH(krp, &ret_worker->crp_ret_kq, krp_next) {
			db_printf("%4u %5u %08x %4u %8p\n"
			    , krp->krp_op
			    , krp->krp_status
			    , krp->krp_crid, krp->krp_hid
			    , krp->krp_callback
			);
		}
	}
}
#endif

int crypto_modevent(module_t mod, int type, void *unused);

/*
 * Initialization code, both for static and dynamic loading.
 * Note this is not invoked with the usual MODULE_DECLARE
 * mechanism but instead is listed as a dependency by the
 * cryptosoft driver.  This guarantees proper ordering of
 * calls on module load/unload.
 */
int
crypto_modevent(module_t mod, int type, void *unused)
{
	int error = EINVAL;

	switch (type) {
	case MOD_LOAD:
		error = crypto_init();
		if (error == 0 && bootverbose)
			printf("crypto: <crypto core>\n");
		break;
	case MOD_UNLOAD:
		/*XXX disallow if active sessions */
		error = 0;
		crypto_destroy();
		return 0;
	}
	return error;
}
MODULE_VERSION(crypto, 1);
MODULE_DEPEND(crypto, zlib, 1, 1, 1);
