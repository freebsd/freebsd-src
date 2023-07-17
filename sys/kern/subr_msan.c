/*	$NetBSD: subr_msan.c,v 1.14 2020/09/09 16:29:59 maxv Exp $	*/

/*
 * Copyright (c) 2019-2020 Maxime Villard, m00nbsd.net
 * All rights reserved.
 * Copyright (c) 2021 The FreeBSD Foundation
 *
 * Portions of this software were developed by Mark Johnston under sponsorship
 * from the FreeBSD Foundation.
 *
 * This code is part of the KMSAN subsystem of the NetBSD kernel.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define	SAN_RUNTIME

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#if 0
__KERNEL_RCSID(0, "$NetBSD: subr_msan.c,v 1.14 2020/09/09 16:29:59 maxv Exp $");
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/memdesc.h>
#include <sys/msan.h>
#include <sys/proc.h>
#include <sys/stack.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/msan.h>
#include <machine/stdarg.h>

void kmsan_init_arg(size_t);
void kmsan_init_ret(size_t);

/* -------------------------------------------------------------------------- */

/*
 * Part of the compiler ABI.
 */

typedef struct {
	uint8_t *shad;
	msan_orig_t *orig;
} msan_meta_t;

#define MSAN_PARAM_SIZE		800
#define MSAN_RETVAL_SIZE	800
typedef struct {
	uint8_t param_shadow[MSAN_PARAM_SIZE];
	uint8_t retval_shadow[MSAN_RETVAL_SIZE];
	uint8_t va_arg_shadow[MSAN_PARAM_SIZE];
	uint8_t va_arg_origin[MSAN_PARAM_SIZE];
	uint64_t va_arg_overflow_size;
	msan_orig_t param_origin[MSAN_PARAM_SIZE / sizeof(msan_orig_t)];
	msan_orig_t retval_origin;
} msan_tls_t;

/* -------------------------------------------------------------------------- */

#define MSAN_NCONTEXT	4
#define MSAN_ORIG_MASK	(~0x3)

typedef struct kmsan_td {
	size_t ctx;
	msan_tls_t tls[MSAN_NCONTEXT];
} msan_td_t;

static msan_tls_t dummy_tls;

/*
 * Use separate dummy regions for loads and stores: stores may mark the region
 * as uninitialized, and that can trigger false positives.
 */
static uint8_t msan_dummy_shad[PAGE_SIZE] __aligned(PAGE_SIZE);
static uint8_t msan_dummy_write_shad[PAGE_SIZE] __aligned(PAGE_SIZE);
static uint8_t msan_dummy_orig[PAGE_SIZE] __aligned(PAGE_SIZE);
static msan_td_t msan_thread0;
static bool kmsan_enabled __read_mostly;

static bool kmsan_reporting = false;

/*
 * Avoid clobbering any thread-local state before we panic.
 */
#define	kmsan_panic(f, ...) do {			\
	kmsan_enabled = false;				\
	panic(f, __VA_ARGS__);				\
} while (0)

#define	REPORT(f, ...) do {				\
	if (panic_on_violation) {			\
		kmsan_panic(f, __VA_ARGS__);		\
	} else {					\
		struct stack st;			\
							\
		stack_save(&st);			\
		printf(f "\n", __VA_ARGS__);		\
		stack_print_ddb(&st);			\
	}						\
} while (0)

FEATURE(kmsan, "Kernel memory sanitizer");

static SYSCTL_NODE(_debug, OID_AUTO, kmsan, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "KMSAN options");

static bool panic_on_violation = 1;
SYSCTL_BOOL(_debug_kmsan, OID_AUTO, panic_on_violation, CTLFLAG_RWTUN,
    &panic_on_violation, 0,
    "Panic if an invalid access is detected");

static MALLOC_DEFINE(M_KMSAN, "kmsan", "Kernel memory sanitizer");

/* -------------------------------------------------------------------------- */

static inline const char *
kmsan_orig_name(int type)
{
	switch (type) {
	case KMSAN_TYPE_STACK:
		return ("stack");
	case KMSAN_TYPE_KMEM:
		return ("kmem");
	case KMSAN_TYPE_MALLOC:
		return ("malloc");
	case KMSAN_TYPE_UMA:
		return ("UMA");
	default:
		return ("unknown");
	}
}

static void
kmsan_report_hook(const void *addr, msan_orig_t *orig, size_t size, size_t off,
    const char *hook)
{
	const char *typename;
	char *var, *fn;
	uintptr_t ptr;
	long foff;
	char buf[128];
	int type;

	if (__predict_false(KERNEL_PANICKED() || kdb_active || kmsan_reporting))
		return;

	kmsan_reporting = true;
	__compiler_membar();

	if (*orig == 0) {
		REPORT("MSan: Uninitialized memory in %s, offset %zu",
		    hook, off);
		goto out;
	}

	kmsan_md_orig_decode(*orig, &type, &ptr);
	typename = kmsan_orig_name(type);

	if (linker_ddb_search_symbol_name((caddr_t)ptr, buf,
	    sizeof(buf), &foff) == 0) {
		REPORT("MSan: Uninitialized %s memory in %s, "
		    "offset %zu/%zu, addr %p, from %s+%#lx",
		    typename, hook, off, size, addr, buf, foff);
	} else if (__builtin_memcmp((void *)ptr, "----", 4) == 0) {
		/*
		 * The format of the string is: "----var@function". Parse it to
		 * display a nice warning.
		 */
		var = (char *)ptr + 4;
		strlcpy(buf, var, sizeof(buf));
		var = buf;
		fn = strchr(buf, '@');
		*fn++ = '\0';
		REPORT("MSan: Uninitialized %s memory in %s, offset %zu, "
		    "variable '%s' from %s", typename, hook, off, var, fn);
	} else {
		REPORT("MSan: Uninitialized %s memory in %s, "
		    "offset %zu/%zu, addr %p, PC %p",
		    typename, hook, off, size, addr, (void *)ptr);
	}

out:
	__compiler_membar();
	kmsan_reporting = false;
}

static void
kmsan_report_inline(msan_orig_t orig, unsigned long pc)
{
	const char *typename;
	char *var, *fn;
	uintptr_t ptr;
	char buf[128];
	long foff;
	int type;

	if (__predict_false(KERNEL_PANICKED() || kdb_active || kmsan_reporting))
		return;

	kmsan_reporting = true;
	__compiler_membar();

	if (orig == 0) {
		REPORT("MSan: uninitialized variable in %p", (void *)pc);
		goto out;
	}

	kmsan_md_orig_decode(orig, &type, &ptr);
	typename = kmsan_orig_name(type);

	if (linker_ddb_search_symbol_name((caddr_t)ptr, buf,
	    sizeof(buf), &foff) == 0) {
		REPORT("MSan: Uninitialized %s memory from %s+%#lx",
		    typename, buf, foff);
	} else if (__builtin_memcmp((void *)ptr, "----", 4) == 0) {
		/*
		 * The format of the string is: "----var@function". Parse it to
		 * display a nice warning.
		 */
		var = (char *)ptr + 4;
		strlcpy(buf, var, sizeof(buf));
		var = buf;
		fn = strchr(buf, '@');
		*fn++ = '\0';
		REPORT("MSan: Uninitialized variable '%s' from %s", var, fn);
	} else {
		REPORT("MSan: Uninitialized %s memory, origin %x",
		    typename, orig);
	}

out:
	__compiler_membar();
	kmsan_reporting = false;
}

/* -------------------------------------------------------------------------- */

static inline msan_meta_t
kmsan_meta_get(const void *addr, size_t size, const bool write)
{
	msan_meta_t ret;

	if (__predict_false(!kmsan_enabled)) {
		ret.shad = write ? msan_dummy_write_shad : msan_dummy_shad;
		ret.orig = (msan_orig_t *)msan_dummy_orig;
	} else if (__predict_false(kmsan_md_unsupported((vm_offset_t)addr))) {
		ret.shad = write ? msan_dummy_write_shad : msan_dummy_shad;
		ret.orig = (msan_orig_t *)msan_dummy_orig;
	} else {
		ret.shad = (void *)kmsan_md_addr_to_shad((vm_offset_t)addr);
		ret.orig =
		    (msan_orig_t *)kmsan_md_addr_to_orig((vm_offset_t)addr);
		ret.orig = (msan_orig_t *)((uintptr_t)ret.orig &
		    MSAN_ORIG_MASK);
	}

	return (ret);
}

static inline void
kmsan_origin_fill(const void *addr, msan_orig_t o, size_t size)
{
	msan_orig_t *orig;
	size_t i;

	if (__predict_false(!kmsan_enabled))
		return;
	if (__predict_false(kmsan_md_unsupported((vm_offset_t)addr)))
		return;

	orig = (msan_orig_t *)kmsan_md_addr_to_orig((vm_offset_t)addr);
	size += ((uintptr_t)orig & (sizeof(*orig) - 1));
	orig = (msan_orig_t *)((uintptr_t)orig & MSAN_ORIG_MASK);

	for (i = 0; i < size; i += 4) {
		orig[i / 4] = o;
	}
}

static inline void
kmsan_shadow_fill(uintptr_t addr, uint8_t c, size_t size)
{
	uint8_t *shad;

	if (__predict_false(!kmsan_enabled))
		return;
	if (__predict_false(kmsan_md_unsupported(addr)))
		return;

	shad = (uint8_t *)kmsan_md_addr_to_shad(addr);
	__builtin_memset(shad, c, size);
}

static inline void
kmsan_meta_copy(void *dst, const void *src, size_t size)
{
	uint8_t *orig_src, *orig_dst;
	uint8_t *shad_src, *shad_dst;
	msan_orig_t *_src, *_dst;
	size_t i;

	if (__predict_false(!kmsan_enabled))
		return;
	if (__predict_false(kmsan_md_unsupported((vm_offset_t)dst)))
		return;
	if (__predict_false(kmsan_md_unsupported((vm_offset_t)src))) {
		kmsan_shadow_fill((uintptr_t)dst, KMSAN_STATE_INITED, size);
		return;
	}

	shad_src = (uint8_t *)kmsan_md_addr_to_shad((vm_offset_t)src);
	shad_dst = (uint8_t *)kmsan_md_addr_to_shad((vm_offset_t)dst);
	__builtin_memmove(shad_dst, shad_src, size);

	orig_src = (uint8_t *)kmsan_md_addr_to_orig((vm_offset_t)src);
	orig_dst = (uint8_t *)kmsan_md_addr_to_orig((vm_offset_t)dst);
	for (i = 0; i < size; i++) {
		_src = (msan_orig_t *)((uintptr_t)orig_src & MSAN_ORIG_MASK);
		_dst = (msan_orig_t *)((uintptr_t)orig_dst & MSAN_ORIG_MASK);
		*_dst = *_src;
		orig_src++;
		orig_dst++;
	}
}

static inline void
kmsan_shadow_check(uintptr_t addr, size_t size, const char *hook)
{
	msan_orig_t *orig;
	uint8_t *shad;
	size_t i;

	if (__predict_false(!kmsan_enabled))
		return;
	if (__predict_false(kmsan_md_unsupported(addr)))
		return;

	shad = (uint8_t *)kmsan_md_addr_to_shad(addr);
	for (i = 0; i < size; i++) {
		if (__predict_true(shad[i] == 0))
			continue;
		orig = (msan_orig_t *)kmsan_md_addr_to_orig((vm_offset_t)&shad[i]);
		orig = (msan_orig_t *)((uintptr_t)orig & MSAN_ORIG_MASK);
		kmsan_report_hook((const char *)addr + i, orig, size, i, hook);
		break;
	}
}

void
kmsan_init_arg(size_t n)
{
	msan_td_t *mtd;
	uint8_t *arg;

	if (__predict_false(!kmsan_enabled))
		return;
	if (__predict_false(curthread == NULL))
		return;
	mtd = curthread->td_kmsan;
	arg = mtd->tls[mtd->ctx].param_shadow;
	__builtin_memset(arg, 0, n);
}

void
kmsan_init_ret(size_t n)
{
	msan_td_t *mtd;
	uint8_t *arg;

	if (__predict_false(!kmsan_enabled))
		return;
	if (__predict_false(curthread == NULL))
		return;
	mtd = curthread->td_kmsan;
	arg = mtd->tls[mtd->ctx].retval_shadow;
	__builtin_memset(arg, 0, n);
}

static void
kmsan_check_arg(size_t size, const char *hook)
{
	msan_orig_t *orig;
	msan_td_t *mtd;
	uint8_t *arg;
	size_t ctx, i;

	if (__predict_false(!kmsan_enabled))
		return;
	if (__predict_false(curthread == NULL))
		return;
	mtd = curthread->td_kmsan;
	ctx = mtd->ctx;
	arg = mtd->tls[ctx].param_shadow;

	for (i = 0; i < size; i++) {
		if (__predict_true(arg[i] == 0))
			continue;
		orig = &mtd->tls[ctx].param_origin[i / sizeof(msan_orig_t)];
		kmsan_report_hook((const char *)arg + i, orig, size, i, hook);
		break;
	}
}

void
kmsan_thread_alloc(struct thread *td)
{
	msan_td_t *mtd;

	if (!kmsan_enabled)
		return;

	mtd = td->td_kmsan;
	if (mtd == NULL) {
		/* We might be recycling a thread. */
		kmsan_init_arg(sizeof(size_t) + sizeof(struct malloc_type *) +
		    sizeof(int));
		mtd = malloc(sizeof(*mtd), M_KMSAN, M_WAITOK);
	}
	kmsan_memset(mtd, 0, sizeof(*mtd));
	mtd->ctx = 0;

	if (td->td_kstack != 0)
		kmsan_mark((void *)td->td_kstack, ptoa(td->td_kstack_pages),
		    KMSAN_STATE_UNINIT);

	td->td_kmsan = mtd;
}

void
kmsan_thread_free(struct thread *td)
{
	msan_td_t *mtd;

	if (!kmsan_enabled)
		return;
	if (__predict_false(td == curthread))
		kmsan_panic("%s: freeing KMSAN TLS for curthread", __func__);

	mtd = td->td_kmsan;
	kmsan_init_arg(sizeof(void *) + sizeof(struct malloc_type *));
	free(mtd, M_KMSAN);
	td->td_kmsan = NULL;
}

void kmsan_intr_enter(void);
void kmsan_intr_leave(void);

void
kmsan_intr_enter(void)
{
	msan_td_t *mtd;

	if (__predict_false(!kmsan_enabled))
		return;

	mtd = curthread->td_kmsan;
	mtd->ctx++;
	if (__predict_false(mtd->ctx >= MSAN_NCONTEXT))
		kmsan_panic("%s: mtd->ctx = %zu", __func__, mtd->ctx);
}

void
kmsan_intr_leave(void)
{
	msan_td_t *mtd;

	if (__predict_false(!kmsan_enabled))
		return;

	mtd = curthread->td_kmsan;
	if (__predict_false(mtd->ctx == 0))
		kmsan_panic("%s: mtd->ctx = %zu", __func__, mtd->ctx);
	mtd->ctx--;
}

/* -------------------------------------------------------------------------- */

void
kmsan_shadow_map(vm_offset_t addr, size_t size)
{
	size_t npages, i;
	vm_offset_t va;

	MPASS(addr % PAGE_SIZE == 0);
	MPASS(size % PAGE_SIZE == 0);

	if (!kmsan_enabled)
		return;

	npages = atop(size);

	va = kmsan_md_addr_to_shad(addr);
	for (i = 0; i < npages; i++) {
		pmap_san_enter(va + ptoa(i));
	}

	va = kmsan_md_addr_to_orig(addr);
	for (i = 0; i < npages; i++) {
		pmap_san_enter(va + ptoa(i));
	}
}

void
kmsan_orig(const void *addr, size_t size, int type, uintptr_t pc)
{
	msan_orig_t orig;

	orig = kmsan_md_orig_encode(type, pc);
	kmsan_origin_fill(addr, orig, size);
}

void
kmsan_mark(const void *addr, size_t size, uint8_t c)
{
	kmsan_shadow_fill((uintptr_t)addr, c, size);
}

void
kmsan_mark_bio(const struct bio *bp, uint8_t c)
{
	kmsan_mark(bp->bio_data, bp->bio_length, c);
}

void
kmsan_mark_mbuf(const struct mbuf *m, uint8_t c)
{
	do {
		if ((m->m_flags & M_EXTPG) == 0)
			kmsan_mark(m->m_data, m->m_len, c);
		m = m->m_next;
	} while (m != NULL);
}

void
kmsan_check(const void *p, size_t sz, const char *descr)
{
	kmsan_shadow_check((uintptr_t)p, sz, descr);
}

void
kmsan_check_bio(const struct bio *bp, const char *descr)
{
	kmsan_shadow_check((uintptr_t)bp->bio_data, bp->bio_length, descr);
}

void
kmsan_check_mbuf(const struct mbuf *m, const char *descr)
{
	do {
		kmsan_shadow_check((uintptr_t)mtod(m, void *), m->m_len, descr);
	} while ((m = m->m_next) != NULL);
}

void
kmsan_init(void)
{
	int disabled;

	disabled = 0;
	TUNABLE_INT_FETCH("debug.kmsan.disabled", &disabled);
	if (disabled)
		return;

	/* Initialize the TLS for curthread. */
	msan_thread0.ctx = 0;
	thread0.td_kmsan = &msan_thread0;

	/* Now officially enabled. */
	kmsan_enabled = true;
}

/* -------------------------------------------------------------------------- */

msan_meta_t __msan_metadata_ptr_for_load_n(void *, size_t);
msan_meta_t __msan_metadata_ptr_for_store_n(void *, size_t);

msan_meta_t
__msan_metadata_ptr_for_load_n(void *addr, size_t size)
{
	return (kmsan_meta_get(addr, size, false));
}

msan_meta_t
__msan_metadata_ptr_for_store_n(void *addr, size_t size)
{
	return (kmsan_meta_get(addr, size, true));
}

#define MSAN_META_FUNC(size)						\
	msan_meta_t __msan_metadata_ptr_for_load_##size(void *);	\
	msan_meta_t __msan_metadata_ptr_for_load_##size(void *addr)	\
	{								\
		return (kmsan_meta_get(addr, size, false));		\
	}								\
	msan_meta_t __msan_metadata_ptr_for_store_##size(void *);	\
	msan_meta_t __msan_metadata_ptr_for_store_##size(void *addr)	\
	{								\
		return (kmsan_meta_get(addr, size, true));		\
	}

MSAN_META_FUNC(1)
MSAN_META_FUNC(2)
MSAN_META_FUNC(4)
MSAN_META_FUNC(8)

void __msan_instrument_asm_store(const void *, size_t);
msan_orig_t __msan_chain_origin(msan_orig_t);
void __msan_poison(const void *, size_t);
void __msan_unpoison(const void *, size_t);
void __msan_poison_alloca(const void *, uint64_t, const char *);
void __msan_unpoison_alloca(const void *, uint64_t);
void __msan_warning(msan_orig_t);
msan_tls_t *__msan_get_context_state(void);

void
__msan_instrument_asm_store(const void *addr, size_t size)
{
	kmsan_shadow_fill((uintptr_t)addr, KMSAN_STATE_INITED, size);
}

msan_orig_t
__msan_chain_origin(msan_orig_t origin)
{
	return (origin);
}

void
__msan_poison(const void *addr, size_t size)
{
	kmsan_shadow_fill((uintptr_t)addr, KMSAN_STATE_UNINIT, size);
}

void
__msan_unpoison(const void *addr, size_t size)
{
	kmsan_shadow_fill((uintptr_t)addr, KMSAN_STATE_INITED, size);
}

void
__msan_poison_alloca(const void *addr, uint64_t size, const char *descr)
{
	msan_orig_t orig;

	orig = kmsan_md_orig_encode(KMSAN_TYPE_STACK, (uintptr_t)descr);
	kmsan_origin_fill(addr, orig, size);
	kmsan_shadow_fill((uintptr_t)addr, KMSAN_STATE_UNINIT, size);
}

void
__msan_unpoison_alloca(const void *addr, uint64_t size)
{
	kmsan_shadow_fill((uintptr_t)addr, KMSAN_STATE_INITED, size);
}

void
__msan_warning(msan_orig_t origin)
{
	if (__predict_false(!kmsan_enabled))
		return;
	kmsan_report_inline(origin, KMSAN_RET_ADDR);
}

msan_tls_t *
__msan_get_context_state(void)
{
	msan_td_t *mtd;

	/*
	 * When APs are started, they execute some C code before curthread is
	 * set.  We have to handle that here.
	 */
	if (__predict_false(!kmsan_enabled || curthread == NULL))
		return (&dummy_tls);
	mtd = curthread->td_kmsan;
	return (&mtd->tls[mtd->ctx]);
}

/* -------------------------------------------------------------------------- */

/*
 * Function hooks. Mostly ASM functions which need KMSAN wrappers to handle
 * initialized areas properly.
 */

void *
kmsan_memcpy(void *dst, const void *src, size_t len)
{
	/* No kmsan_check_arg, because inlined. */
	kmsan_init_ret(sizeof(void *));
	if (__predict_true(len != 0)) {
		kmsan_meta_copy(dst, src, len);
	}
	return (__builtin_memcpy(dst, src, len));
}

int
kmsan_memcmp(const void *b1, const void *b2, size_t len)
{
	const uint8_t *_b1 = b1, *_b2 = b2;
	size_t i;

	kmsan_check_arg(sizeof(b1) + sizeof(b2) + sizeof(len),
	    "memcmp():args");
	kmsan_init_ret(sizeof(int));

	for (i = 0; i < len; i++) {
		if (*_b1 != *_b2) {
			kmsan_shadow_check((uintptr_t)b1, i + 1,
			    "memcmp():arg1");
			kmsan_shadow_check((uintptr_t)b2, i + 1,
			    "memcmp():arg2");
			return (*_b1 - *_b2);
		}
		_b1++, _b2++;
	}

	return (0);
}

void *
kmsan_memset(void *dst, int c, size_t len)
{
	/* No kmsan_check_arg, because inlined. */
	kmsan_shadow_fill((uintptr_t)dst, KMSAN_STATE_INITED, len);
	kmsan_init_ret(sizeof(void *));
	return (__builtin_memset(dst, c, len));
}

void *
kmsan_memmove(void *dst, const void *src, size_t len)
{
	/* No kmsan_check_arg, because inlined. */
	if (__predict_true(len != 0)) {
		kmsan_meta_copy(dst, src, len);
	}
	kmsan_init_ret(sizeof(void *));
	return (__builtin_memmove(dst, src, len));
}

__strong_reference(kmsan_memcpy, __msan_memcpy);
__strong_reference(kmsan_memset, __msan_memset);
__strong_reference(kmsan_memmove, __msan_memmove);

char *
kmsan_strcpy(char *dst, const char *src)
{
	const char *_src = src;
	char *_dst = dst;
	size_t len = 0;

	kmsan_check_arg(sizeof(dst) + sizeof(src), "strcpy():args");

	while (1) {
		len++;
		*dst = *src;
		if (*src == '\0')
			break;
		src++, dst++;
	}

	kmsan_shadow_check((uintptr_t)_src, len, "strcpy():arg2");
	kmsan_shadow_fill((uintptr_t)_dst, KMSAN_STATE_INITED, len);
	kmsan_init_ret(sizeof(char *));
	return (_dst);
}

int
kmsan_strcmp(const char *s1, const char *s2)
{
	const char *_s1 = s1, *_s2 = s2;
	size_t len = 0;

	kmsan_check_arg(sizeof(s1) + sizeof(s2), "strcmp():args");
	kmsan_init_ret(sizeof(int));

	while (1) {
		len++;
		if (*s1 != *s2)
			break;
		if (*s1 == '\0') {
			kmsan_shadow_check((uintptr_t)_s1, len, "strcmp():arg1");
			kmsan_shadow_check((uintptr_t)_s2, len, "strcmp():arg2");
			return (0);
		}
		s1++, s2++;
	}

	kmsan_shadow_check((uintptr_t)_s1, len, "strcmp():arg1");
	kmsan_shadow_check((uintptr_t)_s2, len, "strcmp():arg2");

	return (*(const unsigned char *)s1 - *(const unsigned char *)s2);
}

size_t
kmsan_strlen(const char *str)
{
	const char *s;

	kmsan_check_arg(sizeof(str), "strlen():args");

	s = str;
	while (1) {
		if (*s == '\0')
			break;
		s++;
	}

	kmsan_shadow_check((uintptr_t)str, (size_t)(s - str) + 1, "strlen():arg1");
	kmsan_init_ret(sizeof(size_t));
	return (s - str);
}

int	kmsan_copyin(const void *, void *, size_t);
int	kmsan_copyout(const void *, void *, size_t);
int	kmsan_copyinstr(const void *, void *, size_t, size_t *);

int
kmsan_copyin(const void *uaddr, void *kaddr, size_t len)
{
	int ret;

	kmsan_check_arg(sizeof(uaddr) + sizeof(kaddr) + sizeof(len),
	    "copyin():args");
	ret = copyin(uaddr, kaddr, len);
	if (ret == 0)
		kmsan_shadow_fill((uintptr_t)kaddr, KMSAN_STATE_INITED, len);
	kmsan_init_ret(sizeof(int));
	return (ret);
}

int
kmsan_copyout(const void *kaddr, void *uaddr, size_t len)
{
	kmsan_check_arg(sizeof(kaddr) + sizeof(uaddr) + sizeof(len),
	    "copyout():args");
	kmsan_shadow_check((uintptr_t)kaddr, len, "copyout():arg1");
	kmsan_init_ret(sizeof(int));
	return (copyout(kaddr, uaddr, len));
}

int
kmsan_copyinstr(const void *uaddr, void *kaddr, size_t len, size_t *done)
{
	size_t _done;
	int ret;

	kmsan_check_arg(sizeof(uaddr) + sizeof(kaddr) +
	    sizeof(len) + sizeof(done), "copyinstr():args");
	ret = copyinstr(uaddr, kaddr, len, &_done);
	if (ret == 0)
		kmsan_shadow_fill((uintptr_t)kaddr, KMSAN_STATE_INITED, _done);
	if (done != NULL) {
		*done = _done;
		kmsan_shadow_fill((uintptr_t)done, KMSAN_STATE_INITED, sizeof(size_t));
	}
	kmsan_init_ret(sizeof(int));
	return (ret);
}

/* -------------------------------------------------------------------------- */

int
kmsan_fubyte(volatile const void *base)
{
	int ret;

	kmsan_check_arg(sizeof(base), "fubyte(): args");
	ret = fubyte(base);
	kmsan_init_ret(sizeof(int));
	return (ret);
}

int
kmsan_fuword16(volatile const void *base)
{
	int ret;

	kmsan_check_arg(sizeof(base), "fuword16(): args");
	ret = fuword16(base);
	kmsan_init_ret(sizeof(int));
	return (ret);
}

int
kmsan_fueword(volatile const void *base, long *val)
{
	int ret;

	kmsan_check_arg(sizeof(base) + sizeof(val), "fueword(): args");
	ret = fueword(base, val);
	if (ret == 0)
		kmsan_shadow_fill((uintptr_t)val, KMSAN_STATE_INITED,
		    sizeof(*val));
	kmsan_init_ret(sizeof(int));
	return (ret);
}

int
kmsan_fueword32(volatile const void *base, int32_t *val)
{
	int ret;

	kmsan_check_arg(sizeof(base) + sizeof(val), "fueword32(): args");
	ret = fueword32(base, val);
	if (ret == 0)
		kmsan_shadow_fill((uintptr_t)val, KMSAN_STATE_INITED,
		    sizeof(*val));
	kmsan_init_ret(sizeof(int));
	return (ret);
}

int
kmsan_fueword64(volatile const void *base, int64_t *val)
{
	int ret;

	kmsan_check_arg(sizeof(base) + sizeof(val), "fueword64(): args");
	ret = fueword64(base, val);
	if (ret == 0)
		kmsan_shadow_fill((uintptr_t)val, KMSAN_STATE_INITED,
		    sizeof(*val));
	kmsan_init_ret(sizeof(int));
	return (ret);
}

int
kmsan_subyte(volatile void *base, int byte)
{
	int ret;

	kmsan_check_arg(sizeof(base) + sizeof(byte), "subyte():args");
	ret = subyte(base, byte);
	kmsan_init_ret(sizeof(int));
	return (ret);
}

int
kmsan_suword(volatile void *base, long word)
{
	int ret;

	kmsan_check_arg(sizeof(base) + sizeof(word), "suword():args");
	ret = suword(base, word);
	kmsan_init_ret(sizeof(int));
	return (ret);
}

int
kmsan_suword16(volatile void *base, int word)
{
	int ret;

	kmsan_check_arg(sizeof(base) + sizeof(word), "suword16():args");
	ret = suword16(base, word);
	kmsan_init_ret(sizeof(int));
	return (ret);
}

int
kmsan_suword32(volatile void *base, int32_t word)
{
	int ret;

	kmsan_check_arg(sizeof(base) + sizeof(word), "suword32():args");
	ret = suword32(base, word);
	kmsan_init_ret(sizeof(int));
	return (ret);
}

int
kmsan_suword64(volatile void *base, int64_t word)
{
	int ret;

	kmsan_check_arg(sizeof(base) + sizeof(word), "suword64():args");
	ret = suword64(base, word);
	kmsan_init_ret(sizeof(int));
	return (ret);
}

int
kmsan_casueword32(volatile uint32_t *base, uint32_t oldval, uint32_t *oldvalp,
    uint32_t newval)
{
	int ret;

	kmsan_check_arg(sizeof(base) + sizeof(oldval) + sizeof(oldvalp) +
	    sizeof(newval), "casueword32(): args");
	ret = casueword32(base, oldval, oldvalp, newval);
	kmsan_shadow_fill((uintptr_t)oldvalp, KMSAN_STATE_INITED,
	    sizeof(*oldvalp));
	kmsan_init_ret(sizeof(int));
	return (ret);
}

int
kmsan_casueword(volatile u_long *base, u_long oldval, u_long *oldvalp,
    u_long newval)
{
	int ret;

	kmsan_check_arg(sizeof(base) + sizeof(oldval) + sizeof(oldvalp) +
	    sizeof(newval), "casueword32(): args");
	ret = casueword(base, oldval, oldvalp, newval);
	kmsan_shadow_fill((uintptr_t)oldvalp, KMSAN_STATE_INITED,
	    sizeof(*oldvalp));
	kmsan_init_ret(sizeof(int));
	return (ret);
}

/* -------------------------------------------------------------------------- */

#include <machine/atomic.h>
#include <sys/atomic_san.h>

#define _MSAN_ATOMIC_FUNC_ADD(name, type)				\
	void kmsan_atomic_add_##name(volatile type *ptr, type val)	\
	{								\
		kmsan_check_arg(sizeof(ptr) + sizeof(val),		\
		    "atomic_add_" #name "():args");			\
		kmsan_shadow_check((uintptr_t)ptr, sizeof(type),	\
		    "atomic_add_" #name "():ptr");			\
		atomic_add_##name(ptr, val);				\
	}

#define	MSAN_ATOMIC_FUNC_ADD(name, type)				\
	_MSAN_ATOMIC_FUNC_ADD(name, type)				\
	_MSAN_ATOMIC_FUNC_ADD(acq_##name, type)				\
	_MSAN_ATOMIC_FUNC_ADD(rel_##name, type)

#define _MSAN_ATOMIC_FUNC_SUBTRACT(name, type)				\
	void kmsan_atomic_subtract_##name(volatile type *ptr, type val)	\
	{								\
		kmsan_check_arg(sizeof(ptr) + sizeof(val),		\
		    "atomic_subtract_" #name "():args");		\
		kmsan_shadow_check((uintptr_t)ptr, sizeof(type),	\
		    "atomic_subtract_" #name "():ptr");			\
		atomic_subtract_##name(ptr, val);			\
	}

#define	MSAN_ATOMIC_FUNC_SUBTRACT(name, type)				\
	_MSAN_ATOMIC_FUNC_SUBTRACT(name, type)				\
	_MSAN_ATOMIC_FUNC_SUBTRACT(acq_##name, type)			\
	_MSAN_ATOMIC_FUNC_SUBTRACT(rel_##name, type)

#define _MSAN_ATOMIC_FUNC_SET(name, type)				\
	void kmsan_atomic_set_##name(volatile type *ptr, type val)	\
	{								\
		kmsan_check_arg(sizeof(ptr) + sizeof(val),		\
		    "atomic_set_" #name "():args");			\
		kmsan_shadow_check((uintptr_t)ptr, sizeof(type),	\
		    "atomic_set_" #name "():ptr");			\
		atomic_set_##name(ptr, val);				\
	}

#define	MSAN_ATOMIC_FUNC_SET(name, type)				\
	_MSAN_ATOMIC_FUNC_SET(name, type)				\
	_MSAN_ATOMIC_FUNC_SET(acq_##name, type)				\
	_MSAN_ATOMIC_FUNC_SET(rel_##name, type)

#define _MSAN_ATOMIC_FUNC_CLEAR(name, type)				\
	void kmsan_atomic_clear_##name(volatile type *ptr, type val)	\
	{								\
		kmsan_check_arg(sizeof(ptr) + sizeof(val),		\
		    "atomic_clear_" #name "():args");			\
		kmsan_shadow_check((uintptr_t)ptr, sizeof(type),	\
		    "atomic_clear_" #name "():ptr");			\
		atomic_clear_##name(ptr, val);				\
	}

#define	MSAN_ATOMIC_FUNC_CLEAR(name, type)				\
	_MSAN_ATOMIC_FUNC_CLEAR(name, type)				\
	_MSAN_ATOMIC_FUNC_CLEAR(acq_##name, type)			\
	_MSAN_ATOMIC_FUNC_CLEAR(rel_##name, type)

#define	MSAN_ATOMIC_FUNC_FETCHADD(name, type)				\
	type kmsan_atomic_fetchadd_##name(volatile type *ptr, type val)	\
	{								\
		kmsan_check_arg(sizeof(ptr) + sizeof(val),		\
		    "atomic_fetchadd_" #name "():args");		\
		kmsan_shadow_check((uintptr_t)ptr, sizeof(type),	\
		    "atomic_fetchadd_" #name "():ptr");			\
		kmsan_init_ret(sizeof(type));				\
		return (atomic_fetchadd_##name(ptr, val));		\
	}

#define	MSAN_ATOMIC_FUNC_READANDCLEAR(name, type)			\
	type kmsan_atomic_readandclear_##name(volatile type *ptr)	\
	{								\
		kmsan_check_arg(sizeof(ptr),				\
		    "atomic_readandclear_" #name "():args");		\
		kmsan_shadow_check((uintptr_t)ptr, sizeof(type),	\
		    "atomic_readandclear_" #name "():ptr");		\
		kmsan_init_ret(sizeof(type));				\
		return (atomic_readandclear_##name(ptr));		\
	}

#define	MSAN_ATOMIC_FUNC_TESTANDCLEAR(name, type)			\
	int kmsan_atomic_testandclear_##name(volatile type *ptr, u_int v) \
	{								\
		kmsan_check_arg(sizeof(ptr) + sizeof(v),		\
		    "atomic_testandclear_" #name "():args");		\
		kmsan_shadow_check((uintptr_t)ptr, sizeof(type),	\
		    "atomic_testandclear_" #name "():ptr");		\
		kmsan_init_ret(sizeof(int));				\
		return (atomic_testandclear_##name(ptr, v));		\
	}

#define	MSAN_ATOMIC_FUNC_TESTANDSET(name, type)				\
	int kmsan_atomic_testandset_##name(volatile type *ptr, u_int v) \
	{								\
		kmsan_check_arg(sizeof(ptr) + sizeof(v),		\
		    "atomic_testandset_" #name "():args");		\
		kmsan_shadow_check((uintptr_t)ptr, sizeof(type),	\
		    "atomic_testandset_" #name "():ptr");		\
		kmsan_init_ret(sizeof(int));				\
		return (atomic_testandset_##name(ptr, v));		\
	}

#define	MSAN_ATOMIC_FUNC_SWAP(name, type)				\
	type kmsan_atomic_swap_##name(volatile type *ptr, type val)	\
	{								\
		kmsan_check_arg(sizeof(ptr) + sizeof(val),		\
		    "atomic_swap_" #name "():args");			\
		kmsan_shadow_check((uintptr_t)ptr, sizeof(type),	\
		    "atomic_swap_" #name "():ptr");			\
		kmsan_init_ret(sizeof(type));				\
		return (atomic_swap_##name(ptr, val));			\
	}

#define _MSAN_ATOMIC_FUNC_CMPSET(name, type)				\
	int kmsan_atomic_cmpset_##name(volatile type *ptr, type oval,	\
	    type nval)							\
	{								\
		kmsan_check_arg(sizeof(ptr) + sizeof(oval) +		\
		    sizeof(nval), "atomic_cmpset_" #name "():args");	\
		kmsan_shadow_check((uintptr_t)ptr, sizeof(type),	\
		    "atomic_cmpset_" #name "():ptr");			\
		kmsan_init_ret(sizeof(int));				\
		return (atomic_cmpset_##name(ptr, oval, nval));		\
	}

#define	MSAN_ATOMIC_FUNC_CMPSET(name, type)				\
	_MSAN_ATOMIC_FUNC_CMPSET(name, type)				\
	_MSAN_ATOMIC_FUNC_CMPSET(acq_##name, type)			\
	_MSAN_ATOMIC_FUNC_CMPSET(rel_##name, type)

#define _MSAN_ATOMIC_FUNC_FCMPSET(name, type)				\
	int kmsan_atomic_fcmpset_##name(volatile type *ptr, type *oval,	\
	    type nval)							\
	{								\
		kmsan_check_arg(sizeof(ptr) + sizeof(oval) +		\
		    sizeof(nval), "atomic_fcmpset_" #name "():args");	\
		kmsan_shadow_check((uintptr_t)ptr, sizeof(type),	\
		    "atomic_fcmpset_" #name "():ptr");			\
		kmsan_init_ret(sizeof(int));				\
		return (atomic_fcmpset_##name(ptr, oval, nval));	\
	}

#define	MSAN_ATOMIC_FUNC_FCMPSET(name, type)				\
	_MSAN_ATOMIC_FUNC_FCMPSET(name, type)				\
	_MSAN_ATOMIC_FUNC_FCMPSET(acq_##name, type)			\
	_MSAN_ATOMIC_FUNC_FCMPSET(rel_##name, type)

#define MSAN_ATOMIC_FUNC_THREAD_FENCE(name)				\
	void kmsan_atomic_thread_fence_##name(void)			\
	{								\
		atomic_thread_fence_##name();				\
	}

#define	_MSAN_ATOMIC_FUNC_LOAD(name, type)				\
	type kmsan_atomic_load_##name(volatile type *ptr)		\
	{								\
		kmsan_check_arg(sizeof(ptr),				\
		    "atomic_load_" #name "():args");			\
		kmsan_shadow_check((uintptr_t)ptr, sizeof(type),	\
		    "atomic_load_" #name "():ptr");			\
		kmsan_init_ret(sizeof(type));				\
		return (atomic_load_##name(ptr));			\
	}

#define	MSAN_ATOMIC_FUNC_LOAD(name, type)				\
	_MSAN_ATOMIC_FUNC_LOAD(name, type)				\
	_MSAN_ATOMIC_FUNC_LOAD(acq_##name, type)

#define	_MSAN_ATOMIC_FUNC_STORE(name, type)				\
	void kmsan_atomic_store_##name(volatile type *ptr, type val)	\
	{								\
		kmsan_check_arg(sizeof(ptr) + sizeof(val),		\
		    "atomic_store_" #name "():args");			\
		kmsan_shadow_fill((uintptr_t)ptr, KMSAN_STATE_INITED,	\
		    sizeof(type));					\
		atomic_store_##name(ptr, val);				\
	}

#define	MSAN_ATOMIC_FUNC_STORE(name, type)				\
	_MSAN_ATOMIC_FUNC_STORE(name, type)				\
	_MSAN_ATOMIC_FUNC_STORE(rel_##name, type)

MSAN_ATOMIC_FUNC_ADD(8, uint8_t);
MSAN_ATOMIC_FUNC_ADD(16, uint16_t);
MSAN_ATOMIC_FUNC_ADD(32, uint32_t);
MSAN_ATOMIC_FUNC_ADD(64, uint64_t);
MSAN_ATOMIC_FUNC_ADD(int, u_int);
MSAN_ATOMIC_FUNC_ADD(long, u_long);
MSAN_ATOMIC_FUNC_ADD(ptr, uintptr_t);

MSAN_ATOMIC_FUNC_SUBTRACT(8, uint8_t);
MSAN_ATOMIC_FUNC_SUBTRACT(16, uint16_t);
MSAN_ATOMIC_FUNC_SUBTRACT(32, uint32_t);
MSAN_ATOMIC_FUNC_SUBTRACT(64, uint64_t);
MSAN_ATOMIC_FUNC_SUBTRACT(int, u_int);
MSAN_ATOMIC_FUNC_SUBTRACT(long, u_long);
MSAN_ATOMIC_FUNC_SUBTRACT(ptr, uintptr_t);

MSAN_ATOMIC_FUNC_SET(8, uint8_t);
MSAN_ATOMIC_FUNC_SET(16, uint16_t);
MSAN_ATOMIC_FUNC_SET(32, uint32_t);
MSAN_ATOMIC_FUNC_SET(64, uint64_t);
MSAN_ATOMIC_FUNC_SET(int, u_int);
MSAN_ATOMIC_FUNC_SET(long, u_long);
MSAN_ATOMIC_FUNC_SET(ptr, uintptr_t);

MSAN_ATOMIC_FUNC_CLEAR(8, uint8_t);
MSAN_ATOMIC_FUNC_CLEAR(16, uint16_t);
MSAN_ATOMIC_FUNC_CLEAR(32, uint32_t);
MSAN_ATOMIC_FUNC_CLEAR(64, uint64_t);
MSAN_ATOMIC_FUNC_CLEAR(int, u_int);
MSAN_ATOMIC_FUNC_CLEAR(long, u_long);
MSAN_ATOMIC_FUNC_CLEAR(ptr, uintptr_t);

MSAN_ATOMIC_FUNC_FETCHADD(32, uint32_t);
MSAN_ATOMIC_FUNC_FETCHADD(64, uint64_t);
MSAN_ATOMIC_FUNC_FETCHADD(int, u_int);
MSAN_ATOMIC_FUNC_FETCHADD(long, u_long);

MSAN_ATOMIC_FUNC_READANDCLEAR(32, uint32_t);
MSAN_ATOMIC_FUNC_READANDCLEAR(64, uint64_t);
MSAN_ATOMIC_FUNC_READANDCLEAR(int, u_int);
MSAN_ATOMIC_FUNC_READANDCLEAR(long, u_long);
MSAN_ATOMIC_FUNC_READANDCLEAR(ptr, uintptr_t);

MSAN_ATOMIC_FUNC_TESTANDCLEAR(32, uint32_t);
MSAN_ATOMIC_FUNC_TESTANDCLEAR(64, uint64_t);
MSAN_ATOMIC_FUNC_TESTANDCLEAR(int, u_int);
MSAN_ATOMIC_FUNC_TESTANDCLEAR(long, u_long);

MSAN_ATOMIC_FUNC_TESTANDSET(32, uint32_t);
MSAN_ATOMIC_FUNC_TESTANDSET(64, uint64_t);
MSAN_ATOMIC_FUNC_TESTANDSET(int, u_int);
MSAN_ATOMIC_FUNC_TESTANDSET(long, u_long);

MSAN_ATOMIC_FUNC_SWAP(32, uint32_t);
MSAN_ATOMIC_FUNC_SWAP(64, uint64_t);
MSAN_ATOMIC_FUNC_SWAP(int, u_int);
MSAN_ATOMIC_FUNC_SWAP(long, u_long);
MSAN_ATOMIC_FUNC_SWAP(ptr, uintptr_t);

MSAN_ATOMIC_FUNC_CMPSET(8, uint8_t);
MSAN_ATOMIC_FUNC_CMPSET(16, uint16_t);
MSAN_ATOMIC_FUNC_CMPSET(32, uint32_t);
MSAN_ATOMIC_FUNC_CMPSET(64, uint64_t);
MSAN_ATOMIC_FUNC_CMPSET(int, u_int);
MSAN_ATOMIC_FUNC_CMPSET(long, u_long);
MSAN_ATOMIC_FUNC_CMPSET(ptr, uintptr_t);

MSAN_ATOMIC_FUNC_FCMPSET(8, uint8_t);
MSAN_ATOMIC_FUNC_FCMPSET(16, uint16_t);
MSAN_ATOMIC_FUNC_FCMPSET(32, uint32_t);
MSAN_ATOMIC_FUNC_FCMPSET(64, uint64_t);
MSAN_ATOMIC_FUNC_FCMPSET(int, u_int);
MSAN_ATOMIC_FUNC_FCMPSET(long, u_long);
MSAN_ATOMIC_FUNC_FCMPSET(ptr, uintptr_t);

_MSAN_ATOMIC_FUNC_LOAD(bool, bool);
MSAN_ATOMIC_FUNC_LOAD(8, uint8_t);
MSAN_ATOMIC_FUNC_LOAD(16, uint16_t);
MSAN_ATOMIC_FUNC_LOAD(32, uint32_t);
MSAN_ATOMIC_FUNC_LOAD(64, uint64_t);
MSAN_ATOMIC_FUNC_LOAD(char, u_char);
MSAN_ATOMIC_FUNC_LOAD(short, u_short);
MSAN_ATOMIC_FUNC_LOAD(int, u_int);
MSAN_ATOMIC_FUNC_LOAD(long, u_long);
MSAN_ATOMIC_FUNC_LOAD(ptr, uintptr_t);

_MSAN_ATOMIC_FUNC_STORE(bool, bool);
MSAN_ATOMIC_FUNC_STORE(8, uint8_t);
MSAN_ATOMIC_FUNC_STORE(16, uint16_t);
MSAN_ATOMIC_FUNC_STORE(32, uint32_t);
MSAN_ATOMIC_FUNC_STORE(64, uint64_t);
MSAN_ATOMIC_FUNC_STORE(char, u_char);
MSAN_ATOMIC_FUNC_STORE(short, u_short);
MSAN_ATOMIC_FUNC_STORE(int, u_int);
MSAN_ATOMIC_FUNC_STORE(long, u_long);
MSAN_ATOMIC_FUNC_STORE(ptr, uintptr_t);

MSAN_ATOMIC_FUNC_THREAD_FENCE(acq);
MSAN_ATOMIC_FUNC_THREAD_FENCE(rel);
MSAN_ATOMIC_FUNC_THREAD_FENCE(acq_rel);
MSAN_ATOMIC_FUNC_THREAD_FENCE(seq_cst);

void
kmsan_atomic_interrupt_fence(void)
{
	atomic_interrupt_fence();
}

/* -------------------------------------------------------------------------- */

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/bus_san.h>

int
kmsan_bus_space_map(bus_space_tag_t tag, bus_addr_t hnd, bus_size_t size,
    int flags, bus_space_handle_t *handlep)
{
	return (bus_space_map(tag, hnd, size, flags, handlep));
}

void
kmsan_bus_space_unmap(bus_space_tag_t tag, bus_space_handle_t hnd,
    bus_size_t size)
{
	bus_space_unmap(tag, hnd, size);
}

int
kmsan_bus_space_subregion(bus_space_tag_t tag, bus_space_handle_t hnd,
    bus_size_t offset, bus_size_t size, bus_space_handle_t *handlep)
{
	return (bus_space_subregion(tag, hnd, offset, size, handlep));
}

void
kmsan_bus_space_free(bus_space_tag_t tag, bus_space_handle_t hnd,
    bus_size_t size)
{
	bus_space_free(tag, hnd, size);
}

void
kmsan_bus_space_barrier(bus_space_tag_t tag, bus_space_handle_t hnd,
    bus_size_t offset, bus_size_t size, int flags)
{
	bus_space_barrier(tag, hnd, offset, size, flags);
}

/* XXXMJ x86-specific */
#define MSAN_BUS_READ_FUNC(func, width, type)				\
	type kmsan_bus_space_read##func##_##width(bus_space_tag_t tag,	\
	    bus_space_handle_t hnd, bus_size_t offset)			\
	{								\
		type ret;						\
		if ((tag) != X86_BUS_SPACE_IO)				\
			kmsan_shadow_fill((uintptr_t)(hnd + offset),	\
			    KMSAN_STATE_INITED, (width));		\
		ret = bus_space_read##func##_##width(tag, hnd, offset);	\
		kmsan_init_ret(sizeof(type));				\
		return (ret);						\
	}								\

#define MSAN_BUS_READ_PTR_FUNC(func, width, type)			\
	void kmsan_bus_space_read_##func##_##width(bus_space_tag_t tag,	\
	    bus_space_handle_t hnd, bus_size_t size, type *buf,		\
	    bus_size_t count)						\
	{								\
		kmsan_shadow_fill((uintptr_t)buf, KMSAN_STATE_INITED,	\
		    (width) * count);					\
		bus_space_read_##func##_##width(tag, hnd, size, buf, 	\
		    count);						\
	}

MSAN_BUS_READ_FUNC(, 1, uint8_t)
MSAN_BUS_READ_FUNC(_stream, 1, uint8_t)
MSAN_BUS_READ_PTR_FUNC(multi, 1, uint8_t)
MSAN_BUS_READ_PTR_FUNC(multi_stream, 1, uint8_t)
MSAN_BUS_READ_PTR_FUNC(region, 1, uint8_t)
MSAN_BUS_READ_PTR_FUNC(region_stream, 1, uint8_t)

MSAN_BUS_READ_FUNC(, 2, uint16_t)
MSAN_BUS_READ_FUNC(_stream, 2, uint16_t)
MSAN_BUS_READ_PTR_FUNC(multi, 2, uint16_t)
MSAN_BUS_READ_PTR_FUNC(multi_stream, 2, uint16_t)
MSAN_BUS_READ_PTR_FUNC(region, 2, uint16_t)
MSAN_BUS_READ_PTR_FUNC(region_stream, 2, uint16_t)

MSAN_BUS_READ_FUNC(, 4, uint32_t)
MSAN_BUS_READ_FUNC(_stream, 4, uint32_t)
MSAN_BUS_READ_PTR_FUNC(multi, 4, uint32_t)
MSAN_BUS_READ_PTR_FUNC(multi_stream, 4, uint32_t)
MSAN_BUS_READ_PTR_FUNC(region, 4, uint32_t)
MSAN_BUS_READ_PTR_FUNC(region_stream, 4, uint32_t)

MSAN_BUS_READ_FUNC(, 8, uint64_t)

#define	MSAN_BUS_WRITE_FUNC(func, width, type)				\
	void kmsan_bus_space_write##func##_##width(bus_space_tag_t tag,	\
	    bus_space_handle_t hnd, bus_size_t offset, type value)	\
	{								\
		bus_space_write##func##_##width(tag, hnd, offset, value);\
	}								\

#define	MSAN_BUS_WRITE_PTR_FUNC(func, width, type)			\
	void kmsan_bus_space_write_##func##_##width(bus_space_tag_t tag,\
	    bus_space_handle_t hnd, bus_size_t size, const type *buf,	\
	    bus_size_t count)						\
	{								\
		kmsan_shadow_check((uintptr_t)buf, sizeof(type) * count,\
		    "bus_space_write()");				\
		bus_space_write_##func##_##width(tag, hnd, size, buf, 	\
		    count);						\
	}

MSAN_BUS_WRITE_FUNC(, 1, uint8_t)
MSAN_BUS_WRITE_FUNC(_stream, 1, uint8_t)
MSAN_BUS_WRITE_PTR_FUNC(multi, 1, uint8_t)
MSAN_BUS_WRITE_PTR_FUNC(multi_stream, 1, uint8_t)
MSAN_BUS_WRITE_PTR_FUNC(region, 1, uint8_t)
MSAN_BUS_WRITE_PTR_FUNC(region_stream, 1, uint8_t)

MSAN_BUS_WRITE_FUNC(, 2, uint16_t)
MSAN_BUS_WRITE_FUNC(_stream, 2, uint16_t)
MSAN_BUS_WRITE_PTR_FUNC(multi, 2, uint16_t)
MSAN_BUS_WRITE_PTR_FUNC(multi_stream, 2, uint16_t)
MSAN_BUS_WRITE_PTR_FUNC(region, 2, uint16_t)
MSAN_BUS_WRITE_PTR_FUNC(region_stream, 2, uint16_t)

MSAN_BUS_WRITE_FUNC(, 4, uint32_t)
MSAN_BUS_WRITE_FUNC(_stream, 4, uint32_t)
MSAN_BUS_WRITE_PTR_FUNC(multi, 4, uint32_t)
MSAN_BUS_WRITE_PTR_FUNC(multi_stream, 4, uint32_t)
MSAN_BUS_WRITE_PTR_FUNC(region, 4, uint32_t)
MSAN_BUS_WRITE_PTR_FUNC(region_stream, 4, uint32_t)

MSAN_BUS_WRITE_FUNC(, 8, uint64_t)

#define	MSAN_BUS_SET_FUNC(func, width, type)				\
	void kmsan_bus_space_set_##func##_##width(bus_space_tag_t tag,	\
	    bus_space_handle_t hnd, bus_size_t offset, type value,	\
	    bus_size_t count)						\
	{								\
		bus_space_set_##func##_##width(tag, hnd, offset, value,	\
		    count);						\
	}

MSAN_BUS_SET_FUNC(multi, 1, uint8_t)
MSAN_BUS_SET_FUNC(region, 1, uint8_t)
MSAN_BUS_SET_FUNC(multi_stream, 1, uint8_t)
MSAN_BUS_SET_FUNC(region_stream, 1, uint8_t)

MSAN_BUS_SET_FUNC(multi, 2, uint16_t)
MSAN_BUS_SET_FUNC(region, 2, uint16_t)
MSAN_BUS_SET_FUNC(multi_stream, 2, uint16_t)
MSAN_BUS_SET_FUNC(region_stream, 2, uint16_t)

MSAN_BUS_SET_FUNC(multi, 4, uint32_t)
MSAN_BUS_SET_FUNC(region, 4, uint32_t)
MSAN_BUS_SET_FUNC(multi_stream, 4, uint32_t)
MSAN_BUS_SET_FUNC(region_stream, 4, uint32_t)

/* -------------------------------------------------------------------------- */

void
kmsan_bus_dmamap_sync(struct memdesc *desc, bus_dmasync_op_t op)
{
	/*
	 * Some drivers, e.g., nvme, use the same code path for loading device
	 * read and write requests, and will thus specify both flags.  In this
	 * case we should not do any checking since it will generally lead to
	 * false positives.
	 */
	if ((op & (BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE)) ==
	    BUS_DMASYNC_PREWRITE) {
		switch (desc->md_type) {
		case MEMDESC_VADDR:
			kmsan_check(desc->u.md_vaddr, desc->md_len,
			    "dmasync");
			break;
		case MEMDESC_MBUF:
			kmsan_check_mbuf(desc->u.md_mbuf, "dmasync");
			break;
		case 0:
			break;
		default:
			kmsan_panic("%s: unhandled memdesc type %d", __func__,
			    desc->md_type);
		}
	}
	if ((op & BUS_DMASYNC_POSTREAD) != 0) {
		switch (desc->md_type) {
		case MEMDESC_VADDR:
			kmsan_mark(desc->u.md_vaddr, desc->md_len,
			    KMSAN_STATE_INITED);
			break;
		case MEMDESC_MBUF:
			kmsan_mark_mbuf(desc->u.md_mbuf, KMSAN_STATE_INITED);
			break;
		case 0:
			break;
		default:
			kmsan_panic("%s: unhandled memdesc type %d", __func__,
			    desc->md_type);
		}
	}
}
