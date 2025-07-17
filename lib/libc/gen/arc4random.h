/*	$OpenBSD: arc4random.h,v 1.4 2015/01/15 06:57:18 deraadt Exp $	*/

/*
 * Copyright (c) 1996, David Mazieres <dm@uun.org>
 * Copyright (c) 2008, Damien Miller <djm@openbsd.org>
 * Copyright (c) 2013, Markus Friedl <markus@openbsd.org>
 * Copyright (c) 2014, Theo de Raadt <deraadt@openbsd.org>
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

/*
 * Stub functions for portability.
 */
#include <sys/elf.h>
#include <sys/endian.h>
#include <sys/mman.h>
#if ARC4RANDOM_FXRNG != 0
#include <sys/time.h>	/* for sys/vdso.h only. */
#include <sys/vdso.h>
#include <machine/atomic.h>
#endif

#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>

#if ARC4RANDOM_FXRNG != 0
/*
 * The kernel root seed version is a 64-bit counter, but we truncate it to a
 * 32-bit value in userspace for the convenience of 32-bit platforms.  32-bit
 * rollover is not possible with the current reseed interval (1 hour at limit)
 * without dynamic addition of new random devices (which also force a reseed in
 * the FXRNG design).  We don't have any dynamic device mechanism at this
 * time, and anyway something else is very wrong if billions of new devices are
 * being added.
 *
 * As is, it takes roughly 456,000 years of runtime to overflow the 32-bit
 * version.
 */
#define	fxrng_load_acq_generation(x)	atomic_load_acq_32(x)
static struct vdso_fxrng_generation_1 *vdso_fxrngp;
#endif

static pthread_mutex_t	arc4random_mtx = PTHREAD_MUTEX_INITIALIZER;
#define	_ARC4_LOCK()						\
	do {							\
		if (__isthreaded)				\
			_pthread_mutex_lock(&arc4random_mtx);	\
	} while (0)

#define	_ARC4_UNLOCK()						\
	do {							\
		if (__isthreaded)				\
			_pthread_mutex_unlock(&arc4random_mtx);	\
	} while (0)

static inline void
_getentropy_fail(void)
{
	raise(SIGKILL);
}

static inline void
_rs_initialize_fxrng(void)
{
#if ARC4RANDOM_FXRNG != 0
	struct vdso_fxrng_generation_1 *fxrngp;
	int error;

	error = _elf_aux_info(AT_FXRNG, &fxrngp, sizeof(fxrngp));
	if (error != 0) {
		/*
		 * New userspace on an old or !RANDOM_FENESTRASX kernel; or an
		 * arch that does not have a VDSO page.
		 */
		return;
	}

	/* Old userspace on newer kernel. */
	if (fxrngp->fx_vdso_version != VDSO_FXRNG_VER_1)
		return;

	vdso_fxrngp = fxrngp;
#endif
}

static inline int
_rs_allocate(struct _rs **rsp, struct _rsx **rsxp)
{
	struct {
		struct _rs rs;
		struct _rsx rsx;
	} *p;

	if ((p = mmap(NULL, sizeof(*p), PROT_READ|PROT_WRITE,
	    MAP_ANON|MAP_PRIVATE, -1, 0)) == MAP_FAILED)
		return (-1);
	/* Allow bootstrapping arc4random.c on Linux/macOS */
#ifdef INHERIT_ZERO
	if (minherit(p, sizeof(*p), INHERIT_ZERO) == -1) {
		munmap(p, sizeof(*p));
		return (-1);
	}
#endif

	_rs_initialize_fxrng();

	*rsp = &p->rs;
	*rsxp = &p->rsx;
	return (0);
}

/*
 * This isn't only detecting fork.  We're also using the existing callback from
 * _rs_stir_if_needed() to force arc4random(3) to reseed if the fenestrasX root
 * seed version has changed.  (That is, the root random(4) has reseeded from
 * pooled entropy.)
 */
static inline void
_rs_forkdetect(void)
{
	/* Detect fork (minherit(2) INHERIT_ZERO). */
	if (__predict_false(rs == NULL || rsx == NULL))
		return;
#if ARC4RANDOM_FXRNG != 0
	/* If present, detect kernel FenestrasX seed version change. */
	if (vdso_fxrngp == NULL)
		return;
	if (__predict_true(rsx->rs_seed_generation ==
	    fxrng_load_acq_generation(&vdso_fxrngp->fx_generation32)))
		return;
#endif
	/* Invalidate rs_buf to force "stir" (reseed). */
	memset(rs, 0, sizeof(*rs));
}
