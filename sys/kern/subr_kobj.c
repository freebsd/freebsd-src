/*-
 * Copyright (c) 2000,2003 Doug Rabson
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#ifndef TEST
#include <sys/systm.h>
#endif

#ifdef TEST
#include "usertest.h"
#endif

static MALLOC_DEFINE(M_KOBJ, "kobj", "Kernel object structures");

#ifdef KOBJ_STATS

u_int kobj_lookup_hits;
u_int kobj_lookup_misses;

SYSCTL_UINT(_kern, OID_AUTO, kobj_hits, CTLFLAG_RD,
	   &kobj_lookup_hits, 0, "");
SYSCTL_UINT(_kern, OID_AUTO, kobj_misses, CTLFLAG_RD,
	   &kobj_lookup_misses, 0, "");

#endif

static struct mtx kobj_mtx;
static int kobj_next_id = 1;

SYSCTL_UINT(_kern, OID_AUTO, kobj_methodcount, CTLFLAG_RD,
	   &kobj_next_id, 0, "");

static void
kobj_init_mutex(void *arg)
{

	mtx_init(&kobj_mtx, "kobj", NULL, MTX_DEF);
}

SYSINIT(kobj, SI_SUB_LOCK, SI_ORDER_ANY, kobj_init_mutex, NULL);

/*
 * This method structure is used to initialise new caches. Since the
 * desc pointer is NULL, it is guaranteed never to match any read
 * descriptors.
 */
static struct kobj_method null_method = {
	0, 0,
};

int
kobj_error_method(void)
{

	return ENXIO;
}

static void
kobj_register_method(struct kobjop_desc *desc)
{

	mtx_assert(&kobj_mtx, MA_OWNED);
	if (desc->id == 0) {
		desc->id = kobj_next_id++;
	}
}

static void
kobj_unregister_method(struct kobjop_desc *desc)
{
}

static void
kobj_class_compile_common(kobj_class_t cls, kobj_ops_t ops)
{
	kobj_method_t *m;
	int i;

	mtx_assert(&kobj_mtx, MA_OWNED);

	/*
	 * Don't do anything if we are already compiled.
	 */
	if (cls->ops)
		return;

	/*
	 * First register any methods which need it.
	 */
	for (i = 0, m = cls->methods; m->desc; i++, m++)
		kobj_register_method(m->desc);

	/*
	 * Then initialise the ops table.
	 */
	for (i = 0; i < KOBJ_CACHE_SIZE; i++)
		ops->cache[i] = &null_method;
	ops->cls = cls;
	cls->ops = ops;
}

void
kobj_class_compile(kobj_class_t cls)
{
	kobj_ops_t ops;

	mtx_assert(&kobj_mtx, MA_NOTOWNED);

	/*
	 * Allocate space for the compiled ops table.
	 */
	ops = malloc(sizeof(struct kobj_ops), M_KOBJ, M_NOWAIT);
	if (!ops)
		panic("kobj_compile_methods: out of memory");

	mtx_lock(&kobj_mtx);
	
	/*
	 * We may have lost a race for kobj_class_compile here - check
	 * to make sure someone else hasn't already compiled this
	 * class.
	 */
	if (cls->ops) {
		mtx_unlock(&kobj_mtx);
		free(ops, M_KOBJ);
		return;
	}

	kobj_class_compile_common(cls, ops);
	mtx_unlock(&kobj_mtx);
}

void
kobj_class_compile_static(kobj_class_t cls, kobj_ops_t ops)
{

	mtx_assert(&kobj_mtx, MA_NOTOWNED);

	/*
	 * Increment refs to make sure that the ops table is not freed.
	 */
	mtx_lock(&kobj_mtx);
	cls->refs++;
	kobj_class_compile_common(cls, ops);
	mtx_unlock(&kobj_mtx);
}

static kobj_method_t*
kobj_lookup_method_class(kobj_class_t cls, kobjop_desc_t desc)
{
	kobj_method_t *methods = cls->methods;
	kobj_method_t *ce;

	for (ce = methods; ce && ce->desc; ce++) {
		if (ce->desc == desc) {
			return ce;
		}
	}

	return 0;
}

static kobj_method_t*
kobj_lookup_method_mi(kobj_class_t cls,
		      kobjop_desc_t desc)
{
	kobj_method_t *ce;
	kobj_class_t *basep;

	ce = kobj_lookup_method_class(cls, desc);
	if (ce)
		return ce;

	basep = cls->baseclasses;
	if (basep) {
		for (; *basep; basep++) {
			ce = kobj_lookup_method_mi(*basep, desc);
			if (ce)
				return ce;
		}
	}

	return 0;
}

kobj_method_t*
kobj_lookup_method(kobj_class_t cls,
		   kobj_method_t **cep,
		   kobjop_desc_t desc)
{
	kobj_method_t *ce;

#ifdef KOBJ_STATS
	/*
	 * Correct for the 'hit' assumption in KOBJOPLOOKUP and record
	 * a 'miss'.
	 */
	kobj_lookup_hits--;
	kobj_lookup_misses--;
#endif

	ce = kobj_lookup_method_mi(cls, desc);
	if (!ce)
		ce = desc->deflt;
	*cep = ce;
	return ce;
}

void
kobj_class_free(kobj_class_t cls)
{
	int i;
	kobj_method_t *m;
	void* ops = 0;

	mtx_assert(&kobj_mtx, MA_NOTOWNED);
	mtx_lock(&kobj_mtx);

	/*
	 * Protect against a race between kobj_create and
	 * kobj_delete.
	 */
	if (cls->refs == 0) {
		/*
		 * Unregister any methods which are no longer used.
		 */
		for (i = 0, m = cls->methods; m->desc; i++, m++)
			kobj_unregister_method(m->desc);

		/*
		 * Free memory and clean up.
		 */
		ops = cls->ops;
		cls->ops = 0;
	}
	
	mtx_unlock(&kobj_mtx);

	if (ops)
		free(ops, M_KOBJ);
}

kobj_t
kobj_create(kobj_class_t cls,
	    struct malloc_type *mtype,
	    int mflags)
{
	kobj_t obj;

	/*
	 * Allocate and initialise the new object.
	 */
	obj = malloc(cls->size, mtype, mflags | M_ZERO);
	if (!obj)
		return 0;
	kobj_init(obj, cls);

	return obj;
}

void
kobj_init(kobj_t obj, kobj_class_t cls)
{
	mtx_assert(&kobj_mtx, MA_NOTOWNED);
  retry:
	mtx_lock(&kobj_mtx);

	/*
	 * Consider compiling the class' method table.
	 */
	if (!cls->ops) {
		/*
		 * kobj_class_compile doesn't want the lock held
		 * because of the call to malloc - we drop the lock
		 * and re-try.
		 */
		mtx_unlock(&kobj_mtx);
		kobj_class_compile(cls);
		goto retry;
	}

	obj->ops = cls->ops;
	cls->refs++;

	mtx_unlock(&kobj_mtx);
}

void
kobj_delete(kobj_t obj, struct malloc_type *mtype)
{
	kobj_class_t cls = obj->ops->cls;
	int refs;

	/*
	 * Consider freeing the compiled method table for the class
	 * after its last instance is deleted. As an optimisation, we
	 * should defer this for a short while to avoid thrashing.
	 */
	mtx_assert(&kobj_mtx, MA_NOTOWNED);
	mtx_lock(&kobj_mtx);
	cls->refs--;
	refs = cls->refs;
	mtx_unlock(&kobj_mtx);

	if (!refs)
		kobj_class_free(cls);

	obj->ops = 0;
	if (mtype)
		free(obj, mtype);
}
