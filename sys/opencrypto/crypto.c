/*-
 * Copyright (c) 2002-2006 Sam Leffler.  All rights reserved.
 * Copyright (c) 2021 The FreeBSD Foundation
 *
 * Portions of this software were developed by Ararat River
 * Consulting, LLC under sponsorship of the FreeBSD Foundation.
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

#include <machine/vmparam.h>
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
 * crypto_drivers table with crypto_get_driverid().
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
	uint32_t	cc_sessions;		/* (d) # of sessions */

	int		cc_flags;		/* (d) flags */
#define CRYPTOCAP_F_CLEANUP	0x80000000	/* needs resource cleanup */
	int		cc_qblocked;		/* (q) symmetric q blocked */
	size_t		cc_session_size;
	volatile int	cc_refs;
};

static	struct cryptocap **crypto_drivers = NULL;
static	int crypto_drivers_size = 0;

struct crypto_session {
	struct cryptocap *cap;
	struct crypto_session_params csp;
	uint64_t id;
	/* Driver softc follows. */
};

static	int crp_sleep = 0;
static	TAILQ_HEAD(cryptop_q ,cryptop) crp_q;		/* request queues */
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

	uint32_t reorder_ops;		/* total ordered sym jobs received */
	uint32_t reorder_cur_seq;	/* current sym job dispatched */

	struct thread *td;
};
static struct crypto_ret_worker *crypto_ret_workers = NULL;

#define CRYPTO_RETW(i)		(&crypto_ret_workers[i])
#define CRYPTO_RETW_ID(w)	((w) - crypto_ret_workers)
#define FOREACH_CRYPTO_RETW(w) \
	for (w = crypto_ret_workers; w < crypto_ret_workers + crypto_workers_num; ++w)

#define	CRYPTO_RETW_LOCK(w)	mtx_lock(&w->crypto_ret_mtx)
#define	CRYPTO_RETW_UNLOCK(w)	mtx_unlock(&w->crypto_ret_mtx)

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

int	crypto_devallowsoft = 0;
SYSCTL_INT(_kern_crypto, OID_AUTO, allow_soft, CTLFLAG_RWTUN,
	   &crypto_devallowsoft, 0,
	   "Enable use of software crypto by /dev/crypto");
#ifdef COMPAT_FREEBSD12
SYSCTL_INT(_kern, OID_AUTO, cryptodevallowsoft, CTLFLAG_RWTUN,
	   &crypto_devallowsoft, 0,
	   "Enable/disable use of software crypto by /dev/crypto");
#endif

MALLOC_DEFINE(M_CRYPTO_DATA, "crypto", "crypto session records");

static	void crypto_dispatch_thread(void *arg);
static	struct thread *cryptotd;
static	void crypto_ret_thread(void *arg);
static	void crypto_destroy(void);
static	int crypto_invoke(struct cryptocap *cap, struct cryptop *crp, int hint);
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
struct keybuf *
get_keybuf(void)
{

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

	free(cap, M_CRYPTO_DATA);
}

static int
crypto_init(void)
{
	struct crypto_ret_worker *ret_worker;
	struct proc *p;
	int error;

	mtx_init(&crypto_drivers_mtx, "crypto driver table", NULL, MTX_DEF);

	TAILQ_INIT(&crp_q);
	mtx_init(&crypto_q_mtx, "crypto op queues", NULL, MTX_DEF);

	cryptop_zone = uma_zcreate("cryptop",
	    sizeof(struct cryptop), NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, UMA_ZONE_ZINIT);

	crypto_drivers_size = CRYPTO_DRIVERS_INITIAL;
	crypto_drivers = malloc(crypto_drivers_size *
	    sizeof(struct cryptocap), M_CRYPTO_DATA, M_WAITOK | M_ZERO);

	if (crypto_workers_num < 1 || crypto_workers_num > mp_ncpus)
		crypto_workers_num = mp_ncpus;

	crypto_tq = taskqueue_create("crypto", M_WAITOK | M_ZERO,
	    taskqueue_thread_enqueue, &crypto_tq);

	taskqueue_start_threads(&crypto_tq, crypto_workers_num, PRI_MIN_KERN,
	    "crypto");

	p = NULL;
	error = kproc_kthread_add(crypto_dispatch_thread, NULL, &p, &cryptotd,
	    0, 0, "crypto", "crypto");
	if (error) {
		printf("crypto_init: cannot start crypto thread; error %d",
			error);
		goto bad;
	}

	crypto_ret_workers = mallocarray(crypto_workers_num,
	    sizeof(struct crypto_ret_worker), M_CRYPTO_DATA, M_WAITOK | M_ZERO);

	FOREACH_CRYPTO_RETW(ret_worker) {
		TAILQ_INIT(&ret_worker->crp_ordered_ret_q);
		TAILQ_INIT(&ret_worker->crp_ret_q);

		ret_worker->reorder_ops = 0;
		ret_worker->reorder_cur_seq = 0;

		mtx_init(&ret_worker->crypto_ret_mtx, "crypto return queues",
		    NULL, MTX_DEF);

		error = kthread_add(crypto_ret_thread, ret_worker, p,
		    &ret_worker->td, 0, 0, "crypto returns %td",
		    CRYPTO_RETW_ID(ret_worker));
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
crypto_terminate(struct thread **tdp, void *q)
{
	struct thread *td;

	mtx_assert(&crypto_drivers_mtx, MA_OWNED);
	td = *tdp;
	*tdp = NULL;
	if (td != NULL) {
		wakeup_one(q);
		mtx_sleep(td, &crypto_drivers_mtx, PWAIT, "crypto_destroy", 0);
	}
}

static void
hmac_init_pad(const struct auth_hash *axf, const char *key, int klen,
    void *auth_ctx, uint8_t padval)
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
hmac_init_ipad(const struct auth_hash *axf, const char *key, int klen,
    void *auth_ctx)
{

	hmac_init_pad(axf, key, klen, auth_ctx, HMAC_IPAD_VAL);
}

void
hmac_init_opad(const struct auth_hash *axf, const char *key, int klen,
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
	crypto_terminate(&cryptotd, &crp_q);
	FOREACH_CRYPTO_RETW(ret_worker)
		crypto_terminate(&ret_worker->td, &ret_worker->crp_ret_q);
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
	return (crypto_session + 1);
}

const struct crypto_session_params *
crypto_get_params(crypto_session_t crypto_session)
{
	return (&crypto_session->csp);
}

const struct auth_hash *
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

const struct enc_xform *
crypto_cipher(const struct crypto_session_params *csp)
{

	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_CBC:
		return (&enc_xform_aes_cbc);
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
	case CRYPTO_CHACHA20_POLY1305:
		return (&enc_xform_chacha20_poly1305);
	default:
		return (NULL);
	}
}

static struct cryptocap *
crypto_checkdriver(uint32_t hid)
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
	[CRYPTO_CHACHA20_POLY1305] = ALG_AEAD,
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

static bool
ccm_tag_length_valid(int len)
{
	/* RFC 3610 */
	switch (len) {
	case 4:
	case 6:
	case 8:
	case 10:
	case 12:
	case 14:
	case 16:
		return (true);
	default:
		return (false);
	}
}

#define SUPPORTED_SES (CSP_F_SEPARATE_OUTPUT | CSP_F_SEPARATE_AAD | CSP_F_ESN)

/* Various sanity checks on crypto session parameters. */
static bool
check_csp(const struct crypto_session_params *csp)
{
	const struct auth_hash *axf;

	/* Mode-independent checks. */
	if ((csp->csp_flags & ~(SUPPORTED_SES)) != 0)
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
		switch (csp->csp_auth_alg) {
		case CRYPTO_AES_CCM_CBC_MAC:
			if (csp->csp_ivlen < 7 || csp->csp_ivlen > 13)
				return (false);
			break;
		case CRYPTO_AES_NIST_GMAC:
			if (csp->csp_ivlen != AES_GCM_IV_LEN)
				return (false);
			break;
		default:
			if (csp->csp_ivlen != 0)
				return (false);
			break;
		}

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

			if (csp->csp_auth_alg == CRYPTO_AES_CCM_CBC_MAC &&
			    !ccm_tag_length_valid(csp->csp_auth_mlen))
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

		switch (csp->csp_cipher_alg) {
		case CRYPTO_AES_CCM_16:
			if (csp->csp_auth_mlen != 0 &&
			    !ccm_tag_length_valid(csp->csp_auth_mlen))
				return (false);

			if (csp->csp_ivlen < 7 || csp->csp_ivlen > 13)
				return (false);
			break;
		case CRYPTO_AES_NIST_GCM_16:
			if (csp->csp_auth_mlen > AES_GMAC_HASH_LEN)
				return (false);

			if (csp->csp_ivlen != AES_GCM_IV_LEN)
				return (false);
			break;
		case CRYPTO_CHACHA20_POLY1305:
			if (csp->csp_ivlen != 8 && csp->csp_ivlen != 12)
				return (false);
			if (csp->csp_auth_mlen > POLY1305_HASH_LEN)
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

	zfree(cses, M_CRYPTO_DATA);

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
	static uint64_t sessid = 0;
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

	/* Allocate a single block for the generic session and driver softc. */
	res = malloc(sizeof(*res) + cap->cc_session_size, M_CRYPTO_DATA,
	    M_WAITOK | M_ZERO);
	res->cap = cap;
	res->csp = *csp;
	res->id = atomic_fetchadd_64(&sessid, 1);

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
 * Unregister all algorithms associated with a crypto driver.
 * If there are pending sessions using it, leave enough information
 * around so that subsequent calls using those sessions will
 * correctly detect the driver has been unregistered and reroute
 * requests.
 */
int
crypto_unregister_all(uint32_t driverid)
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
	while (cap->cc_sessions != 0)
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
crypto_unblock(uint32_t driverid, int what)
{
	struct cryptocap *cap;
	int err;

	CRYPTO_Q_LOCK();
	cap = crypto_checkdriver(driverid);
	if (cap != NULL) {
		if (what & CRYPTO_SYMQ)
			cap->cc_qblocked = 0;
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
	case CRYPTO_BUF_SINGLE_MBUF:
		return (cb->cb_mbuf->m_len);
	case CRYPTO_BUF_VMPAGE:
		return (cb->cb_vm_page_len);
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
	switch (cb->cb_type) {
	case CRYPTO_BUF_CONTIG:
		KASSERT(cb->cb_buf_len >= 0,
		    ("incoming crp with -ve %s buffer length", name));
		break;
	case CRYPTO_BUF_VMPAGE:
		KASSERT(CRYPTO_HAS_VMPAGE,
		    ("incoming crp uses dmap on supported arch"));
		KASSERT(cb->cb_vm_page_len >= 0,
		    ("incoming crp with -ve %s buffer length", name));
		KASSERT(cb->cb_vm_page_offset >= 0,
		    ("incoming crp with -ve %s buffer offset", name));
		KASSERT(cb->cb_vm_page_offset < PAGE_SIZE,
		    ("incoming crp with %s buffer offset greater than page size"
		     , name));
		break;
	default:
		break;
	}
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
		KASSERT(crp->crp_flags & CRYPTO_F_IV_SEPARATE,
		    ("AEAD without a separate IV"));
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
		KASSERT(crp->crp_payload_output_start == 0 ||
		    crp->crp_payload_output_start < olen,
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

static int
crypto_dispatch_one(struct cryptop *crp, int hint)
{
	struct cryptocap *cap;
	int result;

#ifdef INVARIANTS
	crp_sanity(crp);
#endif
	CRYPTOSTAT_INC(cs_ops);

	crp->crp_retw_id = crp->crp_session->id % crypto_workers_num;

	/*
	 * Caller marked the request to be processed immediately; dispatch it
	 * directly to the driver unless the driver is currently blocked, in
	 * which case it is queued for deferred dispatch.
	 */
	cap = crp->crp_session->cap;
	if (!atomic_load_int(&cap->cc_qblocked)) {
		result = crypto_invoke(cap, crp, hint);
		if (result != ERESTART)
			return (result);

		/*
		 * The driver ran out of resources, put the request on the
		 * queue.
		 */
	}
	crypto_batch_enqueue(crp);
	return (0);
}

int
crypto_dispatch(struct cryptop *crp)
{
	return (crypto_dispatch_one(crp, 0));
}

int
crypto_dispatch_async(struct cryptop *crp, int flags)
{
	struct crypto_ret_worker *ret_worker;

	if (!CRYPTO_SESS_SYNC(crp->crp_session)) {
		/*
		 * The driver issues completions asynchonously, don't bother
		 * deferring dispatch to a worker thread.
		 */
		return (crypto_dispatch(crp));
	}

#ifdef INVARIANTS
	crp_sanity(crp);
#endif
	CRYPTOSTAT_INC(cs_ops);

	crp->crp_retw_id = crp->crp_session->id % crypto_workers_num;
	if ((flags & CRYPTO_ASYNC_ORDERED) != 0) {
		crp->crp_flags |= CRYPTO_F_ASYNC_ORDERED;
		ret_worker = CRYPTO_RETW(crp->crp_retw_id);
		CRYPTO_RETW_LOCK(ret_worker);
		crp->crp_seq = ret_worker->reorder_ops++;
		CRYPTO_RETW_UNLOCK(ret_worker);
	}
	TASK_INIT(&crp->crp_task, 0, crypto_task_invoke, crp);
	taskqueue_enqueue(crypto_tq, &crp->crp_task);
	return (0);
}

void
crypto_dispatch_batch(struct cryptopq *crpq, int flags)
{
	struct cryptop *crp;
	int hint;

	while ((crp = TAILQ_FIRST(crpq)) != NULL) {
		hint = TAILQ_NEXT(crp, crp_next) != NULL ? CRYPTO_HINT_MORE : 0;
		TAILQ_REMOVE(crpq, crp, crp_next);
		if (crypto_dispatch_one(crp, hint) != 0)
			crypto_batch_enqueue(crp);
	}
}

static void
crypto_batch_enqueue(struct cryptop *crp)
{

	CRYPTO_Q_LOCK();
	TAILQ_INSERT_TAIL(&crp_q, crp, crp_next);
	if (crp_sleep)
		wakeup_one(&crp_q);
	CRYPTO_Q_UNLOCK();
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
crypto_destroyreq(struct cryptop *crp)
{
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
}

void
crypto_freereq(struct cryptop *crp)
{
	if (crp == NULL)
		return;

	crypto_destroyreq(crp);
	uma_zfree(cryptop_zone, crp);
}

static void
_crypto_initreq(struct cryptop *crp, crypto_session_t cses)
{
	crp->crp_session = cses;
}

void
crypto_initreq(struct cryptop *crp, crypto_session_t cses)
{
	memset(crp, 0, sizeof(*crp));
	_crypto_initreq(crp, cses);
}

struct cryptop *
crypto_getreq(crypto_session_t cses, int how)
{
	struct cryptop *crp;

	MPASS(how == M_WAITOK || how == M_NOWAIT);
	crp = uma_zalloc(cryptop_zone, how | M_ZERO);
	if (crp != NULL)
		_crypto_initreq(crp, cses);
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
	if ((crp->crp_flags & CRYPTO_F_ASYNC_ORDERED) == 0 &&
	    ((crp->crp_flags & CRYPTO_F_CBIMM) != 0 ||
	    ((crp->crp_flags & CRYPTO_F_CBIFSYNC) != 0 &&
	    CRYPTO_SESS_SYNC(crp->crp_session)))) {
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

		/*
		 * Normal case; queue the callback for the thread.
		 */
		CRYPTO_RETW_LOCK(ret_worker);
		if ((crp->crp_flags & CRYPTO_F_ASYNC_ORDERED) != 0) {
			struct cryptop *tmp;

			TAILQ_FOREACH_REVERSE(tmp,
			    &ret_worker->crp_ordered_ret_q, cryptop_q,
			    crp_next) {
				if (CRYPTO_SEQ_GT(crp->crp_seq, tmp->crp_seq)) {
					TAILQ_INSERT_AFTER(
					    &ret_worker->crp_ordered_ret_q, tmp,
					    crp, crp_next);
					break;
				}
			}
			if (tmp == NULL) {
				TAILQ_INSERT_HEAD(
				    &ret_worker->crp_ordered_ret_q, crp,
				    crp_next);
			}

			wake = crp->crp_seq == ret_worker->reorder_cur_seq;
		} else {
			wake = TAILQ_EMPTY(&ret_worker->crp_ret_q);
			TAILQ_INSERT_TAIL(&ret_worker->crp_ret_q, crp,
			    crp_next);
		}

		if (wake)
			wakeup_one(&ret_worker->crp_ret_q);	/* shared wait channel */
		CRYPTO_RETW_UNLOCK(ret_worker);
	}
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
	kthread_exit();
}

/*
 * Crypto thread, dispatches crypto requests.
 */
static void
crypto_dispatch_thread(void *arg __unused)
{
	struct cryptop *crp, *submit;
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
				} else {
					submit = crp;
				}
				break;
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
		} else {
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
			if (cryptotd == NULL)
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
crypto_ret_thread(void *arg)
{
	struct crypto_ret_worker *ret_worker = arg;
	struct cryptop *crpt;

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

		if (crpt != NULL) {
			CRYPTO_RETW_UNLOCK(ret_worker);
			/*
			 * Run callbacks unlocked.
			 */
			if (crpt != NULL)
				crpt->crp_callback(crpt);
			CRYPTO_RETW_LOCK(ret_worker);
		} else {
			/*
			 * Nothing more to be processed.  Sleep until we're
			 * woken because there are more returns to process.
			 */
			msleep(&ret_worker->crp_ret_q, &ret_worker->crypto_ret_mtx, PWAIT,
				"crypto_ret_wait", 0);
			if (ret_worker->td == NULL)
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

	db_printf("%12s %4s %8s %2s\n"
		, "Device"
		, "Ses"
		, "Flags"
		, "QB"
	);
	for (hid = 0; hid < crypto_drivers_size; hid++) {
		const struct cryptocap *cap = crypto_drivers[hid];
		if (cap == NULL)
			continue;
		db_printf("%-12s %4u %08x %2u\n"
		    , device_get_nameunit(cap->cc_dev)
		    , cap->cc_sessions
		    , cap->cc_flags
		    , cap->cc_qblocked
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
