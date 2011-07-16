/*
 * Portions Copyright (C) 2004-2010  Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (C) 1999-2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NETWORK ASSOCIATES DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Portions Copyright (C) 1995-2000 by Network Associates, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NETWORK ASSOCIATES DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Principal Author: Brian Wellington
 * $Id: openssl_link.c,v 1.29 2010-09-15 12:38:36 tbox Exp $
 */
#ifdef OPENSSL

#include <config.h>

#include <isc/entropy.h>
#include <isc/mem.h>
#include <isc/mutex.h>
#include <isc/mutexblock.h>
#include <isc/string.h>
#include <isc/thread.h>
#include <isc/util.h>

#include <dst/result.h>

#include "dst_internal.h"
#include "dst_openssl.h"

#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/conf.h>
#include <openssl/crypto.h>

#if defined(CRYPTO_LOCK_ENGINE) && (OPENSSL_VERSION_NUMBER >= 0x0090707f)
#define USE_ENGINE 1
#endif

#ifdef USE_ENGINE
#include <openssl/engine.h>
#endif

static RAND_METHOD *rm = NULL;

static isc_mutex_t *locks = NULL;
static int nlocks;

#ifdef USE_ENGINE
static ENGINE *e = NULL;
#endif

static int
entropy_get(unsigned char *buf, int num) {
	isc_result_t result;
	if (num < 0)
		return (-1);
	result = dst__entropy_getdata(buf, (unsigned int) num, ISC_FALSE);
	return (result == ISC_R_SUCCESS ? 1 : -1);
}

static int
entropy_status(void) {
	return (dst__entropy_status() > 32);
}

static int
entropy_getpseudo(unsigned char *buf, int num) {
	isc_result_t result;
	if (num < 0)
		return (-1);
	result = dst__entropy_getdata(buf, (unsigned int) num, ISC_TRUE);
	return (result == ISC_R_SUCCESS ? 1 : -1);
}

static void
entropy_add(const void *buf, int num, double entropy) {
	/*
	 * Do nothing.  The only call to this provides no useful data anyway.
	 */
	UNUSED(buf);
	UNUSED(num);
	UNUSED(entropy);
}

static void
lock_callback(int mode, int type, const char *file, int line) {
	UNUSED(file);
	UNUSED(line);
	if ((mode & CRYPTO_LOCK) != 0)
		LOCK(&locks[type]);
	else
		UNLOCK(&locks[type]);
}

static unsigned long
id_callback(void) {
	return ((unsigned long)isc_thread_self());
}

static void *
mem_alloc(size_t size) {
#ifdef OPENSSL_LEAKS
	void *ptr;

	INSIST(dst__memory_pool != NULL);
	ptr = isc_mem_allocate(dst__memory_pool, size);
	return (ptr);
#else
	INSIST(dst__memory_pool != NULL);
	return (isc_mem_allocate(dst__memory_pool, size));
#endif
}

static void
mem_free(void *ptr) {
	INSIST(dst__memory_pool != NULL);
	if (ptr != NULL)
		isc_mem_free(dst__memory_pool, ptr);
}

static void *
mem_realloc(void *ptr, size_t size) {
#ifdef OPENSSL_LEAKS
	void *rptr;

	INSIST(dst__memory_pool != NULL);
	rptr = isc_mem_reallocate(dst__memory_pool, ptr, size);
	return (rptr);
#else
	INSIST(dst__memory_pool != NULL);
	return (isc_mem_reallocate(dst__memory_pool, ptr, size));
#endif
}

isc_result_t
dst__openssl_init(const char *engine) {
	isc_result_t result;
#ifdef USE_ENGINE
	ENGINE *re;
#else

	UNUSED(engine);
#endif

#ifdef  DNS_CRYPTO_LEAKS
	CRYPTO_malloc_debug_init();
	CRYPTO_set_mem_debug_options(V_CRYPTO_MDEBUG_ALL);
	CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);
#endif
	CRYPTO_set_mem_functions(mem_alloc, mem_realloc, mem_free);
	nlocks = CRYPTO_num_locks();
	locks = mem_alloc(sizeof(isc_mutex_t) * nlocks);
	if (locks == NULL)
		return (ISC_R_NOMEMORY);
	result = isc_mutexblock_init(locks, nlocks);
	if (result != ISC_R_SUCCESS)
		goto cleanup_mutexalloc;
	CRYPTO_set_locking_callback(lock_callback);
	CRYPTO_set_id_callback(id_callback);

	rm = mem_alloc(sizeof(RAND_METHOD));
	if (rm == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup_mutexinit;
	}
	rm->seed = NULL;
	rm->bytes = entropy_get;
	rm->cleanup = NULL;
	rm->add = entropy_add;
	rm->pseudorand = entropy_getpseudo;
	rm->status = entropy_status;

#ifdef USE_ENGINE
	OPENSSL_config(NULL);

	if (engine != NULL && *engine == '\0')
		engine = NULL;

	if (engine != NULL) {
		e = ENGINE_by_id(engine);
		if (e == NULL) {
			result = DST_R_NOENGINE;
			goto cleanup_rm;
		}
		/* This will init the engine. */
		if (!ENGINE_set_default(e, ENGINE_METHOD_ALL)) {
			result = DST_R_NOENGINE;
			goto cleanup_rm;
		}
	}

	re = ENGINE_get_default_RAND();
	if (re == NULL) {
		re = ENGINE_new();
		if (re == NULL) {
			result = ISC_R_NOMEMORY;
			goto cleanup_rm;
		}
		ENGINE_set_RAND(re, rm);
		ENGINE_set_default_RAND(re);
		ENGINE_free(re);
	} else
		ENGINE_finish(re);
#else
	RAND_set_rand_method(rm);
#endif /* USE_ENGINE */
	return (ISC_R_SUCCESS);

#ifdef USE_ENGINE
 cleanup_rm:
	if (e != NULL)
		ENGINE_free(e);
	e = NULL;
	mem_free(rm);
	rm = NULL;
#endif
 cleanup_mutexinit:
	CRYPTO_set_locking_callback(NULL);
	DESTROYMUTEXBLOCK(locks, nlocks);
 cleanup_mutexalloc:
	mem_free(locks);
	locks = NULL;
	return (result);
}

void
dst__openssl_destroy() {

	/*
	 * Sequence taken from apps_shutdown() in <apps/apps.h>.
	 */
	if (rm != NULL) {
#if OPENSSL_VERSION_NUMBER >= 0x00907000L
		RAND_cleanup();
#endif
		mem_free(rm);
		rm = NULL;
	}
#if (OPENSSL_VERSION_NUMBER >= 0x00907000L)
	CONF_modules_free();
#endif
	OBJ_cleanup();
	EVP_cleanup();
#if defined(USE_ENGINE)
	if (e != NULL)
		ENGINE_free(e);
	e = NULL;
#if defined(USE_ENGINE) && OPENSSL_VERSION_NUMBER >= 0x00907000L
	ENGINE_cleanup();
#endif
#endif
#if (OPENSSL_VERSION_NUMBER >= 0x00907000L)
	CRYPTO_cleanup_all_ex_data();
#endif
	ERR_clear_error();
	ERR_remove_state(0);
	ERR_free_strings();

#ifdef  DNS_CRYPTO_LEAKS
	CRYPTO_mem_leaks_fp(stderr);
#endif

	if (locks != NULL) {
		CRYPTO_set_locking_callback(NULL);
		DESTROYMUTEXBLOCK(locks, nlocks);
		mem_free(locks);
		locks = NULL;
	}
}

isc_result_t
dst__openssl_toresult(isc_result_t fallback) {
	isc_result_t result = fallback;
	int err = ERR_get_error();

	switch (ERR_GET_REASON(err)) {
	case ERR_R_MALLOC_FAILURE:
		result = ISC_R_NOMEMORY;
		break;
	default:
		break;
	}
	ERR_clear_error();
	return (result);
}

ENGINE *
dst__openssl_getengine(const char *engine) {

	if (engine == NULL)
		return (NULL);
#if defined(USE_ENGINE)
	if (e == NULL)
		return (NULL);
	if (strcmp(engine, ENGINE_get_id(e)) == 0)
		return (e);
#endif
	return (NULL);
}

#else /* OPENSSL */

#include <isc/util.h>

EMPTY_TRANSLATION_UNIT

#endif /* OPENSSL */
/*! \file */
