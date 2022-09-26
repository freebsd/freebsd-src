/*	$NetBSD: subr_asan.c,v 1.26 2020/09/10 14:10:46 maxv Exp $	*/

/*
 * Copyright (c) 2018-2020 Maxime Villard, m00nbsd.net
 * All rights reserved.
 *
 * This code is part of the KASAN subsystem of the NetBSD kernel.
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
__KERNEL_RCSID(0, "$NetBSD: subr_asan.c,v 1.26 2020/09/10 14:10:46 maxv Exp $");
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/asan.h>
#include <sys/kernel.h>
#include <sys/stack.h>
#include <sys/sysctl.h>

#include <machine/asan.h>
#include <machine/bus.h>

/* ASAN constants. Part of the compiler ABI. */
#define KASAN_SHADOW_MASK		(KASAN_SHADOW_SCALE - 1)
#define KASAN_ALLOCA_SCALE_SIZE		32

/* ASAN ABI version. */
#if defined(__clang__) && (__clang_major__ - 0 >= 6)
#define ASAN_ABI_VERSION	8
#elif __GNUC_PREREQ__(7, 1) && !defined(__clang__)
#define ASAN_ABI_VERSION	8
#elif __GNUC_PREREQ__(6, 1) && !defined(__clang__)
#define ASAN_ABI_VERSION	6
#else
#error "Unsupported compiler version"
#endif

#define __RET_ADDR	(unsigned long)__builtin_return_address(0)

/* Global variable descriptor. Part of the compiler ABI.  */
struct __asan_global_source_location {
	const char *filename;
	int line_no;
	int column_no;
};

struct __asan_global {
	const void *beg;		/* address of the global variable */
	size_t size;			/* size of the global variable */
	size_t size_with_redzone;	/* size with the redzone */
	const void *name;		/* name of the variable */
	const void *module_name;	/* name of the module where the var is declared */
	unsigned long has_dynamic_init;	/* the var has dyn initializer (c++) */
	struct __asan_global_source_location *location;
#if ASAN_ABI_VERSION >= 7
	uintptr_t odr_indicator;	/* the address of the ODR indicator symbol */
#endif
};

FEATURE(kasan, "Kernel address sanitizer");

static SYSCTL_NODE(_debug, OID_AUTO, kasan, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "KASAN options");

static int panic_on_violation = 1;
SYSCTL_INT(_debug_kasan, OID_AUTO, panic_on_violation, CTLFLAG_RDTUN,
    &panic_on_violation, 0,
    "Panic if an invalid access is detected");

static bool kasan_enabled __read_mostly = false;

/* -------------------------------------------------------------------------- */

void
kasan_shadow_map(vm_offset_t addr, size_t size)
{
	size_t sz, npages, i;
	vm_offset_t sva, eva;

	KASSERT(addr % KASAN_SHADOW_SCALE == 0,
	    ("%s: invalid address %#lx", __func__, addr));

	sz = roundup(size, KASAN_SHADOW_SCALE) / KASAN_SHADOW_SCALE;

	sva = kasan_md_addr_to_shad(addr);
	eva = kasan_md_addr_to_shad(addr) + sz;

	sva = rounddown(sva, PAGE_SIZE);
	eva = roundup(eva, PAGE_SIZE);

	npages = (eva - sva) / PAGE_SIZE;

	KASSERT(sva >= KASAN_MIN_ADDRESS && eva < KASAN_MAX_ADDRESS,
	    ("%s: invalid address range %#lx-%#lx", __func__, sva, eva));

	for (i = 0; i < npages; i++)
		pmap_san_enter(sva + ptoa(i));
}

void
kasan_init(void)
{
	int disabled;

	disabled = 0;
	TUNABLE_INT_FETCH("debug.kasan.disabled", &disabled);
	if (disabled)
		return;

	/* MD initialization. */
	kasan_md_init();

	/* Now officially enabled. */
	kasan_enabled = true;
}

void
kasan_init_early(vm_offset_t stack, size_t size)
{
	kasan_md_init_early(stack, size);
}

static inline const char *
kasan_code_name(uint8_t code)
{
	switch (code) {
	case KASAN_GENERIC_REDZONE:
		return "GenericRedZone";
	case KASAN_MALLOC_REDZONE:
		return "MallocRedZone";
	case KASAN_KMEM_REDZONE:
		return "KmemRedZone";
	case KASAN_UMA_FREED:
		return "UMAUseAfterFree";
	case KASAN_KSTACK_FREED:
		return "KernelStack";
	case KASAN_EXEC_ARGS_FREED:
		return "ExecKVA";
	case 1 ... 7:
		return "RedZonePartial";
	case KASAN_STACK_LEFT:
		return "StackLeft";
	case KASAN_STACK_MID:
		return "StackMiddle";
	case KASAN_STACK_RIGHT:
		return "StackRight";
	case KASAN_USE_AFTER_RET:
		return "UseAfterRet";
	case KASAN_USE_AFTER_SCOPE:
		return "UseAfterScope";
	default:
		return "Unknown";
	}
}

#define	REPORT(f, ...) do {				\
	if (panic_on_violation) {			\
		kasan_enabled = false;			\
		panic(f, __VA_ARGS__);			\
	} else {					\
		struct stack st;			\
							\
		stack_save(&st);			\
		printf(f "\n", __VA_ARGS__);		\
		stack_print_ddb(&st);			\
	}						\
} while (0)

static void
kasan_report(unsigned long addr, size_t size, bool write, unsigned long pc,
    uint8_t code)
{
	REPORT("ASan: Invalid access, %zu-byte %s at %#lx, %s(%x)",
	    size, (write ? "write" : "read"), addr, kasan_code_name(code),
	    code);
}

static __always_inline void
kasan_shadow_1byte_markvalid(unsigned long addr)
{
	int8_t *byte = (int8_t *)kasan_md_addr_to_shad(addr);
	int8_t last = (addr & KASAN_SHADOW_MASK) + 1;

	*byte = last;
}

static __always_inline void
kasan_shadow_Nbyte_markvalid(const void *addr, size_t size)
{
	size_t i;

	for (i = 0; i < size; i++) {
		kasan_shadow_1byte_markvalid((unsigned long)addr + i);
	}
}

static __always_inline void
kasan_shadow_Nbyte_fill(const void *addr, size_t size, uint8_t code)
{
	void *shad;

	if (__predict_false(size == 0))
		return;
	if (__predict_false(kasan_md_unsupported((vm_offset_t)addr)))
		return;

	KASSERT((vm_offset_t)addr % KASAN_SHADOW_SCALE == 0,
	    ("%s: invalid address %p", __func__, addr));
	KASSERT(size % KASAN_SHADOW_SCALE == 0,
	    ("%s: invalid size %zu", __func__, size));

	shad = (void *)kasan_md_addr_to_shad((uintptr_t)addr);
	size = size >> KASAN_SHADOW_SCALE_SHIFT;

	__builtin_memset(shad, code, size);
}

/*
 * In an area of size 'sz_with_redz', mark the 'size' first bytes as valid,
 * and the rest as invalid. There are generally two use cases:
 *
 *  o kasan_mark(addr, origsize, size, code), with origsize < size. This marks
 *    the redzone at the end of the buffer as invalid. If the entire is to be
 *    marked invalid, origsize will be 0.
 *
 *  o kasan_mark(addr, size, size, 0). This marks the entire buffer as valid.
 */
void
kasan_mark(const void *addr, size_t size, size_t redzsize, uint8_t code)
{
	size_t i, n, redz;
	int8_t *shad;

	if ((vm_offset_t)addr >= DMAP_MIN_ADDRESS &&
	    (vm_offset_t)addr < DMAP_MAX_ADDRESS)
		return;

	KASSERT((vm_offset_t)addr >= VM_MIN_KERNEL_ADDRESS &&
	    (vm_offset_t)addr < VM_MAX_KERNEL_ADDRESS,
	    ("%s: invalid address %p", __func__, addr));
	KASSERT((vm_offset_t)addr % KASAN_SHADOW_SCALE == 0,
	    ("%s: invalid address %p", __func__, addr));
	redz = redzsize - roundup(size, KASAN_SHADOW_SCALE);
	KASSERT(redz % KASAN_SHADOW_SCALE == 0,
	    ("%s: invalid size %zu", __func__, redz));
	shad = (int8_t *)kasan_md_addr_to_shad((uintptr_t)addr);

	/* Chunks of 8 bytes, valid. */
	n = size / KASAN_SHADOW_SCALE;
	for (i = 0; i < n; i++) {
		*shad++ = 0;
	}

	/* Possibly one chunk, mid. */
	if ((size & KASAN_SHADOW_MASK) != 0) {
		*shad++ = (size & KASAN_SHADOW_MASK);
	}

	/* Chunks of 8 bytes, invalid. */
	n = redz / KASAN_SHADOW_SCALE;
	for (i = 0; i < n; i++) {
		*shad++ = code;
	}
}

/* -------------------------------------------------------------------------- */

#define ADDR_CROSSES_SCALE_BOUNDARY(addr, size) 		\
	(addr >> KASAN_SHADOW_SCALE_SHIFT) !=			\
	    ((addr + size - 1) >> KASAN_SHADOW_SCALE_SHIFT)

static __always_inline bool
kasan_shadow_1byte_isvalid(unsigned long addr, uint8_t *code)
{
	int8_t *byte = (int8_t *)kasan_md_addr_to_shad(addr);
	int8_t last = (addr & KASAN_SHADOW_MASK) + 1;

	if (__predict_true(*byte == 0 || last <= *byte)) {
		return (true);
	}
	*code = *byte;
	return (false);
}

static __always_inline bool
kasan_shadow_2byte_isvalid(unsigned long addr, uint8_t *code)
{
	int8_t *byte, last;

	if (ADDR_CROSSES_SCALE_BOUNDARY(addr, 2)) {
		return (kasan_shadow_1byte_isvalid(addr, code) &&
		    kasan_shadow_1byte_isvalid(addr+1, code));
	}

	byte = (int8_t *)kasan_md_addr_to_shad(addr);
	last = ((addr + 1) & KASAN_SHADOW_MASK) + 1;

	if (__predict_true(*byte == 0 || last <= *byte)) {
		return (true);
	}
	*code = *byte;
	return (false);
}

static __always_inline bool
kasan_shadow_4byte_isvalid(unsigned long addr, uint8_t *code)
{
	int8_t *byte, last;

	if (ADDR_CROSSES_SCALE_BOUNDARY(addr, 4)) {
		return (kasan_shadow_2byte_isvalid(addr, code) &&
		    kasan_shadow_2byte_isvalid(addr+2, code));
	}

	byte = (int8_t *)kasan_md_addr_to_shad(addr);
	last = ((addr + 3) & KASAN_SHADOW_MASK) + 1;

	if (__predict_true(*byte == 0 || last <= *byte)) {
		return (true);
	}
	*code = *byte;
	return (false);
}

static __always_inline bool
kasan_shadow_8byte_isvalid(unsigned long addr, uint8_t *code)
{
	int8_t *byte, last;

	if (ADDR_CROSSES_SCALE_BOUNDARY(addr, 8)) {
		return (kasan_shadow_4byte_isvalid(addr, code) &&
		    kasan_shadow_4byte_isvalid(addr+4, code));
	}

	byte = (int8_t *)kasan_md_addr_to_shad(addr);
	last = ((addr + 7) & KASAN_SHADOW_MASK) + 1;

	if (__predict_true(*byte == 0 || last <= *byte)) {
		return (true);
	}
	*code = *byte;
	return (false);
}

static __always_inline bool
kasan_shadow_Nbyte_isvalid(unsigned long addr, size_t size, uint8_t *code)
{
	size_t i;

	for (i = 0; i < size; i++) {
		if (!kasan_shadow_1byte_isvalid(addr+i, code))
			return (false);
	}

	return (true);
}

static __always_inline void
kasan_shadow_check(unsigned long addr, size_t size, bool write,
    unsigned long retaddr)
{
	uint8_t code;
	bool valid;

	if (__predict_false(!kasan_enabled))
		return;
	if (__predict_false(size == 0))
		return;
	if (__predict_false(kasan_md_unsupported(addr)))
		return;
	if (KERNEL_PANICKED())
		return;

	if (__builtin_constant_p(size)) {
		switch (size) {
		case 1:
			valid = kasan_shadow_1byte_isvalid(addr, &code);
			break;
		case 2:
			valid = kasan_shadow_2byte_isvalid(addr, &code);
			break;
		case 4:
			valid = kasan_shadow_4byte_isvalid(addr, &code);
			break;
		case 8:
			valid = kasan_shadow_8byte_isvalid(addr, &code);
			break;
		default:
			valid = kasan_shadow_Nbyte_isvalid(addr, size, &code);
			break;
		}
	} else {
		valid = kasan_shadow_Nbyte_isvalid(addr, size, &code);
	}

	if (__predict_false(!valid)) {
		kasan_report(addr, size, write, retaddr, code);
	}
}

/* -------------------------------------------------------------------------- */

void *
kasan_memcpy(void *dst, const void *src, size_t len)
{
	kasan_shadow_check((unsigned long)src, len, false, __RET_ADDR);
	kasan_shadow_check((unsigned long)dst, len, true, __RET_ADDR);
	return (__builtin_memcpy(dst, src, len));
}

int
kasan_memcmp(const void *b1, const void *b2, size_t len)
{
	kasan_shadow_check((unsigned long)b1, len, false, __RET_ADDR);
	kasan_shadow_check((unsigned long)b2, len, false, __RET_ADDR);
	return (__builtin_memcmp(b1, b2, len));
}

void *
kasan_memset(void *b, int c, size_t len)
{
	kasan_shadow_check((unsigned long)b, len, true, __RET_ADDR);
	return (__builtin_memset(b, c, len));
}

void *
kasan_memmove(void *dst, const void *src, size_t len)
{
	kasan_shadow_check((unsigned long)src, len, false, __RET_ADDR);
	kasan_shadow_check((unsigned long)dst, len, true, __RET_ADDR);
	return (__builtin_memmove(dst, src, len));
}

size_t
kasan_strlen(const char *str)
{
	const char *s;

	s = str;
	while (1) {
		kasan_shadow_check((unsigned long)s, 1, false, __RET_ADDR);
		if (*s == '\0')
			break;
		s++;
	}

	return (s - str);
}

char *
kasan_strcpy(char *dst, const char *src)
{
	char *save = dst;

	while (1) {
		kasan_shadow_check((unsigned long)src, 1, false, __RET_ADDR);
		kasan_shadow_check((unsigned long)dst, 1, true, __RET_ADDR);
		*dst = *src;
		if (*src == '\0')
			break;
		src++, dst++;
	}

	return save;
}

int
kasan_strcmp(const char *s1, const char *s2)
{
	while (1) {
		kasan_shadow_check((unsigned long)s1, 1, false, __RET_ADDR);
		kasan_shadow_check((unsigned long)s2, 1, false, __RET_ADDR);
		if (*s1 != *s2)
			break;
		if (*s1 == '\0')
			return 0;
		s1++, s2++;
	}

	return (*(const unsigned char *)s1 - *(const unsigned char *)s2);
}

int
kasan_copyin(const void *uaddr, void *kaddr, size_t len)
{
	kasan_shadow_check((unsigned long)kaddr, len, true, __RET_ADDR);
	return (copyin(uaddr, kaddr, len));
}

int
kasan_copyinstr(const void *uaddr, void *kaddr, size_t len, size_t *done)
{
	kasan_shadow_check((unsigned long)kaddr, len, true, __RET_ADDR);
	return (copyinstr(uaddr, kaddr, len, done));
}

int
kasan_copyout(const void *kaddr, void *uaddr, size_t len)
{
	kasan_shadow_check((unsigned long)kaddr, len, false, __RET_ADDR);
	return (copyout(kaddr, uaddr, len));
}

/* -------------------------------------------------------------------------- */

int
kasan_fubyte(volatile const void *base)
{
	return (fubyte(base));
}

int
kasan_fuword16(volatile const void *base)
{
	return (fuword16(base));
}

int
kasan_fueword(volatile const void *base, long *val)
{
	kasan_shadow_check((unsigned long)val, sizeof(*val), true, __RET_ADDR);
	return (fueword(base, val));
}

int
kasan_fueword32(volatile const void *base, int32_t *val)
{
	kasan_shadow_check((unsigned long)val, sizeof(*val), true, __RET_ADDR);
	return (fueword32(base, val));
}

int
kasan_fueword64(volatile const void *base, int64_t *val)
{
	kasan_shadow_check((unsigned long)val, sizeof(*val), true, __RET_ADDR);
	return (fueword64(base, val));
}

int
kasan_subyte(volatile void *base, int byte)
{
	return (subyte(base, byte));
}

int
kasan_suword(volatile void *base, long word)
{
	return (suword(base, word));
}

int
kasan_suword16(volatile void *base, int word)
{
	return (suword16(base, word));
}

int
kasan_suword32(volatile void *base, int32_t word)
{
	return (suword32(base, word));
}

int
kasan_suword64(volatile void *base, int64_t word)
{
	return (suword64(base, word));
}

int
kasan_casueword32(volatile uint32_t *base, uint32_t oldval, uint32_t *oldvalp,
    uint32_t newval)
{
	kasan_shadow_check((unsigned long)oldvalp, sizeof(*oldvalp), true,
	    __RET_ADDR);
	return (casueword32(base, oldval, oldvalp, newval));
}

int
kasan_casueword(volatile u_long *base, u_long oldval, u_long *oldvalp,
    u_long newval)
{
	kasan_shadow_check((unsigned long)oldvalp, sizeof(*oldvalp), true,
	    __RET_ADDR);
	return (casueword(base, oldval, oldvalp, newval));
}

/* -------------------------------------------------------------------------- */

#include <machine/atomic.h>
#include <sys/atomic_san.h>

#define _ASAN_ATOMIC_FUNC_ADD(name, type)				\
	void kasan_atomic_add_##name(volatile type *ptr, type val)	\
	{								\
		kasan_shadow_check((uintptr_t)ptr, sizeof(type), true,	\
		    __RET_ADDR);					\
		atomic_add_##name(ptr, val);				\
	}

#define	ASAN_ATOMIC_FUNC_ADD(name, type)				\
	_ASAN_ATOMIC_FUNC_ADD(name, type)				\
	_ASAN_ATOMIC_FUNC_ADD(acq_##name, type)				\
	_ASAN_ATOMIC_FUNC_ADD(rel_##name, type)

#define _ASAN_ATOMIC_FUNC_SUBTRACT(name, type)				\
	void kasan_atomic_subtract_##name(volatile type *ptr, type val)	\
	{								\
		kasan_shadow_check((uintptr_t)ptr, sizeof(type), true,	\
		    __RET_ADDR);					\
		atomic_subtract_##name(ptr, val);			\
	}

#define	ASAN_ATOMIC_FUNC_SUBTRACT(name, type)				\
	_ASAN_ATOMIC_FUNC_SUBTRACT(name, type)				\
	_ASAN_ATOMIC_FUNC_SUBTRACT(acq_##name, type)			\
	_ASAN_ATOMIC_FUNC_SUBTRACT(rel_##name, type)

#define _ASAN_ATOMIC_FUNC_SET(name, type)				\
	void kasan_atomic_set_##name(volatile type *ptr, type val)	\
	{								\
		kasan_shadow_check((uintptr_t)ptr, sizeof(type), true,	\
		    __RET_ADDR);					\
		atomic_set_##name(ptr, val);				\
	}

#define	ASAN_ATOMIC_FUNC_SET(name, type)				\
	_ASAN_ATOMIC_FUNC_SET(name, type)				\
	_ASAN_ATOMIC_FUNC_SET(acq_##name, type)				\
	_ASAN_ATOMIC_FUNC_SET(rel_##name, type)

#define _ASAN_ATOMIC_FUNC_CLEAR(name, type)				\
	void kasan_atomic_clear_##name(volatile type *ptr, type val)	\
	{								\
		kasan_shadow_check((uintptr_t)ptr, sizeof(type), true,	\
		    __RET_ADDR);					\
		atomic_clear_##name(ptr, val);				\
	}

#define	ASAN_ATOMIC_FUNC_CLEAR(name, type)				\
	_ASAN_ATOMIC_FUNC_CLEAR(name, type)				\
	_ASAN_ATOMIC_FUNC_CLEAR(acq_##name, type)			\
	_ASAN_ATOMIC_FUNC_CLEAR(rel_##name, type)

#define	ASAN_ATOMIC_FUNC_FETCHADD(name, type)				\
	type kasan_atomic_fetchadd_##name(volatile type *ptr, type val)	\
	{								\
		kasan_shadow_check((uintptr_t)ptr, sizeof(type), true,	\
		    __RET_ADDR);					\
		return (atomic_fetchadd_##name(ptr, val));		\
	}

#define	ASAN_ATOMIC_FUNC_READANDCLEAR(name, type)			\
	type kasan_atomic_readandclear_##name(volatile type *ptr)	\
	{								\
		kasan_shadow_check((uintptr_t)ptr, sizeof(type), true,	\
		    __RET_ADDR);					\
		return (atomic_readandclear_##name(ptr));		\
	}

#define	ASAN_ATOMIC_FUNC_TESTANDCLEAR(name, type)			\
	int kasan_atomic_testandclear_##name(volatile type *ptr, u_int v) \
	{								\
		kasan_shadow_check((uintptr_t)ptr, sizeof(type), true,	\
		    __RET_ADDR);					\
		return (atomic_testandclear_##name(ptr, v));		\
	}

#define	ASAN_ATOMIC_FUNC_TESTANDSET(name, type)				\
	int kasan_atomic_testandset_##name(volatile type *ptr, u_int v) \
	{								\
		kasan_shadow_check((uintptr_t)ptr, sizeof(type), true,	\
		    __RET_ADDR);					\
		return (atomic_testandset_##name(ptr, v));		\
	}

#define	ASAN_ATOMIC_FUNC_SWAP(name, type)				\
	type kasan_atomic_swap_##name(volatile type *ptr, type val)	\
	{								\
		kasan_shadow_check((uintptr_t)ptr, sizeof(type), true,	\
		    __RET_ADDR);					\
		return (atomic_swap_##name(ptr, val));			\
	}

#define _ASAN_ATOMIC_FUNC_CMPSET(name, type)				\
	int kasan_atomic_cmpset_##name(volatile type *ptr, type oval,	\
	    type nval)							\
	{								\
		kasan_shadow_check((uintptr_t)ptr, sizeof(type), true,	\
		    __RET_ADDR);					\
		return (atomic_cmpset_##name(ptr, oval, nval));		\
	}

#define	ASAN_ATOMIC_FUNC_CMPSET(name, type)				\
	_ASAN_ATOMIC_FUNC_CMPSET(name, type)				\
	_ASAN_ATOMIC_FUNC_CMPSET(acq_##name, type)			\
	_ASAN_ATOMIC_FUNC_CMPSET(rel_##name, type)

#define _ASAN_ATOMIC_FUNC_FCMPSET(name, type)				\
	int kasan_atomic_fcmpset_##name(volatile type *ptr, type *oval,	\
	    type nval)							\
	{								\
		kasan_shadow_check((uintptr_t)ptr, sizeof(type), true,	\
		    __RET_ADDR);					\
		return (atomic_fcmpset_##name(ptr, oval, nval));	\
	}

#define	ASAN_ATOMIC_FUNC_FCMPSET(name, type)				\
	_ASAN_ATOMIC_FUNC_FCMPSET(name, type)				\
	_ASAN_ATOMIC_FUNC_FCMPSET(acq_##name, type)			\
	_ASAN_ATOMIC_FUNC_FCMPSET(rel_##name, type)

#define ASAN_ATOMIC_FUNC_THREAD_FENCE(name)				\
	void kasan_atomic_thread_fence_##name(void)			\
	{								\
		atomic_thread_fence_##name();				\
	}

#define	_ASAN_ATOMIC_FUNC_LOAD(name, type)				\
	type kasan_atomic_load_##name(volatile type *ptr)		\
	{								\
		kasan_shadow_check((uintptr_t)ptr, sizeof(type), true,	\
		    __RET_ADDR);					\
		return (atomic_load_##name(ptr));			\
	}

#define	ASAN_ATOMIC_FUNC_LOAD(name, type)				\
	_ASAN_ATOMIC_FUNC_LOAD(name, type)				\
	_ASAN_ATOMIC_FUNC_LOAD(acq_##name, type)

#define	_ASAN_ATOMIC_FUNC_STORE(name, type)				\
	void kasan_atomic_store_##name(volatile type *ptr, type val)	\
	{								\
		kasan_shadow_check((uintptr_t)ptr, sizeof(type), true,	\
		    __RET_ADDR);					\
		atomic_store_##name(ptr, val);				\
	}

#define	ASAN_ATOMIC_FUNC_STORE(name, type)				\
	_ASAN_ATOMIC_FUNC_STORE(name, type)				\
	_ASAN_ATOMIC_FUNC_STORE(rel_##name, type)

ASAN_ATOMIC_FUNC_ADD(8, uint8_t);
ASAN_ATOMIC_FUNC_ADD(16, uint16_t);
ASAN_ATOMIC_FUNC_ADD(32, uint32_t);
ASAN_ATOMIC_FUNC_ADD(64, uint64_t);
ASAN_ATOMIC_FUNC_ADD(int, u_int);
ASAN_ATOMIC_FUNC_ADD(long, u_long);
ASAN_ATOMIC_FUNC_ADD(ptr, uintptr_t);

ASAN_ATOMIC_FUNC_SUBTRACT(8, uint8_t);
ASAN_ATOMIC_FUNC_SUBTRACT(16, uint16_t);
ASAN_ATOMIC_FUNC_SUBTRACT(32, uint32_t);
ASAN_ATOMIC_FUNC_SUBTRACT(64, uint64_t);
ASAN_ATOMIC_FUNC_SUBTRACT(int, u_int);
ASAN_ATOMIC_FUNC_SUBTRACT(long, u_long);
ASAN_ATOMIC_FUNC_SUBTRACT(ptr, uintptr_t);

ASAN_ATOMIC_FUNC_SET(8, uint8_t);
ASAN_ATOMIC_FUNC_SET(16, uint16_t);
ASAN_ATOMIC_FUNC_SET(32, uint32_t);
ASAN_ATOMIC_FUNC_SET(64, uint64_t);
ASAN_ATOMIC_FUNC_SET(int, u_int);
ASAN_ATOMIC_FUNC_SET(long, u_long);
ASAN_ATOMIC_FUNC_SET(ptr, uintptr_t);

ASAN_ATOMIC_FUNC_CLEAR(8, uint8_t);
ASAN_ATOMIC_FUNC_CLEAR(16, uint16_t);
ASAN_ATOMIC_FUNC_CLEAR(32, uint32_t);
ASAN_ATOMIC_FUNC_CLEAR(64, uint64_t);
ASAN_ATOMIC_FUNC_CLEAR(int, u_int);
ASAN_ATOMIC_FUNC_CLEAR(long, u_long);
ASAN_ATOMIC_FUNC_CLEAR(ptr, uintptr_t);

ASAN_ATOMIC_FUNC_FETCHADD(32, uint32_t);
ASAN_ATOMIC_FUNC_FETCHADD(64, uint64_t);
ASAN_ATOMIC_FUNC_FETCHADD(int, u_int);
ASAN_ATOMIC_FUNC_FETCHADD(long, u_long);

ASAN_ATOMIC_FUNC_READANDCLEAR(32, uint32_t);
ASAN_ATOMIC_FUNC_READANDCLEAR(64, uint64_t);
ASAN_ATOMIC_FUNC_READANDCLEAR(int, u_int);
ASAN_ATOMIC_FUNC_READANDCLEAR(long, u_long);
ASAN_ATOMIC_FUNC_READANDCLEAR(ptr, uintptr_t);

ASAN_ATOMIC_FUNC_TESTANDCLEAR(32, uint32_t);
ASAN_ATOMIC_FUNC_TESTANDCLEAR(64, uint64_t);
ASAN_ATOMIC_FUNC_TESTANDCLEAR(int, u_int);
ASAN_ATOMIC_FUNC_TESTANDCLEAR(long, u_long);

ASAN_ATOMIC_FUNC_TESTANDSET(32, uint32_t);
ASAN_ATOMIC_FUNC_TESTANDSET(64, uint64_t);
ASAN_ATOMIC_FUNC_TESTANDSET(int, u_int);
ASAN_ATOMIC_FUNC_TESTANDSET(long, u_long);

ASAN_ATOMIC_FUNC_SWAP(32, uint32_t);
ASAN_ATOMIC_FUNC_SWAP(64, uint64_t);
ASAN_ATOMIC_FUNC_SWAP(int, u_int);
ASAN_ATOMIC_FUNC_SWAP(long, u_long);
ASAN_ATOMIC_FUNC_SWAP(ptr, uintptr_t);

ASAN_ATOMIC_FUNC_CMPSET(8, uint8_t);
ASAN_ATOMIC_FUNC_CMPSET(16, uint16_t);
ASAN_ATOMIC_FUNC_CMPSET(32, uint32_t);
ASAN_ATOMIC_FUNC_CMPSET(64, uint64_t);
ASAN_ATOMIC_FUNC_CMPSET(int, u_int);
ASAN_ATOMIC_FUNC_CMPSET(long, u_long);
ASAN_ATOMIC_FUNC_CMPSET(ptr, uintptr_t);

ASAN_ATOMIC_FUNC_FCMPSET(8, uint8_t);
ASAN_ATOMIC_FUNC_FCMPSET(16, uint16_t);
ASAN_ATOMIC_FUNC_FCMPSET(32, uint32_t);
ASAN_ATOMIC_FUNC_FCMPSET(64, uint64_t);
ASAN_ATOMIC_FUNC_FCMPSET(int, u_int);
ASAN_ATOMIC_FUNC_FCMPSET(long, u_long);
ASAN_ATOMIC_FUNC_FCMPSET(ptr, uintptr_t);

ASAN_ATOMIC_FUNC_LOAD(8, uint8_t);
ASAN_ATOMIC_FUNC_LOAD(16, uint16_t);
ASAN_ATOMIC_FUNC_LOAD(32, uint32_t);
ASAN_ATOMIC_FUNC_LOAD(64, uint64_t);
ASAN_ATOMIC_FUNC_LOAD(char, u_char);
ASAN_ATOMIC_FUNC_LOAD(short, u_short);
ASAN_ATOMIC_FUNC_LOAD(int, u_int);
ASAN_ATOMIC_FUNC_LOAD(long, u_long);
ASAN_ATOMIC_FUNC_LOAD(ptr, uintptr_t);

ASAN_ATOMIC_FUNC_STORE(8, uint8_t);
ASAN_ATOMIC_FUNC_STORE(16, uint16_t);
ASAN_ATOMIC_FUNC_STORE(32, uint32_t);
ASAN_ATOMIC_FUNC_STORE(64, uint64_t);
ASAN_ATOMIC_FUNC_STORE(char, u_char);
ASAN_ATOMIC_FUNC_STORE(short, u_short);
ASAN_ATOMIC_FUNC_STORE(int, u_int);
ASAN_ATOMIC_FUNC_STORE(long, u_long);
ASAN_ATOMIC_FUNC_STORE(ptr, uintptr_t);

ASAN_ATOMIC_FUNC_THREAD_FENCE(acq);
ASAN_ATOMIC_FUNC_THREAD_FENCE(rel);
ASAN_ATOMIC_FUNC_THREAD_FENCE(acq_rel);
ASAN_ATOMIC_FUNC_THREAD_FENCE(seq_cst);

void
kasan_atomic_interrupt_fence(void)
{
}

/* -------------------------------------------------------------------------- */

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/bus_san.h>

int
kasan_bus_space_map(bus_space_tag_t tag, bus_addr_t hnd, bus_size_t size,
    int flags, bus_space_handle_t *handlep)
{
	return (bus_space_map(tag, hnd, size, flags, handlep));
}

void
kasan_bus_space_unmap(bus_space_tag_t tag, bus_space_handle_t hnd,
    bus_size_t size)
{
	bus_space_unmap(tag, hnd, size);
}

int
kasan_bus_space_subregion(bus_space_tag_t tag, bus_space_handle_t hnd,
    bus_size_t offset, bus_size_t size, bus_space_handle_t *handlep)
{
	return (bus_space_subregion(tag, hnd, offset, size, handlep));
}

void
kasan_bus_space_free(bus_space_tag_t tag, bus_space_handle_t hnd,
    bus_size_t size)
{
	bus_space_free(tag, hnd, size);
}

void
kasan_bus_space_barrier(bus_space_tag_t tag, bus_space_handle_t hnd,
    bus_size_t offset, bus_size_t size, int flags)
{
	bus_space_barrier(tag, hnd, offset, size, flags);
}

#define ASAN_BUS_READ_FUNC(func, width, type)				\
	type kasan_bus_space_read##func##_##width(bus_space_tag_t tag,	\
	    bus_space_handle_t hnd, bus_size_t offset)			\
	{								\
		return (bus_space_read##func##_##width(tag, hnd,	\
		    offset));						\
	}								\

#define ASAN_BUS_READ_PTR_FUNC(func, width, type)			\
	void kasan_bus_space_read_##func##_##width(bus_space_tag_t tag,	\
	    bus_space_handle_t hnd, bus_size_t size, type *buf,		\
	    bus_size_t count)						\
	{								\
		kasan_shadow_check((uintptr_t)buf, sizeof(type) * count,\
		    false, __RET_ADDR);					\
		bus_space_read_##func##_##width(tag, hnd, size, buf, 	\
		    count);						\
	}

ASAN_BUS_READ_FUNC(, 1, uint8_t)
ASAN_BUS_READ_FUNC(_stream, 1, uint8_t)
ASAN_BUS_READ_PTR_FUNC(multi, 1, uint8_t)
ASAN_BUS_READ_PTR_FUNC(multi_stream, 1, uint8_t)
ASAN_BUS_READ_PTR_FUNC(region, 1, uint8_t)
ASAN_BUS_READ_PTR_FUNC(region_stream, 1, uint8_t)

ASAN_BUS_READ_FUNC(, 2, uint16_t)
ASAN_BUS_READ_FUNC(_stream, 2, uint16_t)
ASAN_BUS_READ_PTR_FUNC(multi, 2, uint16_t)
ASAN_BUS_READ_PTR_FUNC(multi_stream, 2, uint16_t)
ASAN_BUS_READ_PTR_FUNC(region, 2, uint16_t)
ASAN_BUS_READ_PTR_FUNC(region_stream, 2, uint16_t)

ASAN_BUS_READ_FUNC(, 4, uint32_t)
ASAN_BUS_READ_FUNC(_stream, 4, uint32_t)
ASAN_BUS_READ_PTR_FUNC(multi, 4, uint32_t)
ASAN_BUS_READ_PTR_FUNC(multi_stream, 4, uint32_t)
ASAN_BUS_READ_PTR_FUNC(region, 4, uint32_t)
ASAN_BUS_READ_PTR_FUNC(region_stream, 4, uint32_t)

ASAN_BUS_READ_FUNC(, 8, uint64_t)

#define	ASAN_BUS_WRITE_FUNC(func, width, type)				\
	void kasan_bus_space_write##func##_##width(bus_space_tag_t tag,	\
	    bus_space_handle_t hnd, bus_size_t offset, type value)	\
	{								\
		bus_space_write##func##_##width(tag, hnd, offset, value);\
	}								\

#define	ASAN_BUS_WRITE_PTR_FUNC(func, width, type)			\
	void kasan_bus_space_write_##func##_##width(bus_space_tag_t tag,\
	    bus_space_handle_t hnd, bus_size_t size, const type *buf,	\
	    bus_size_t count)						\
	{								\
		kasan_shadow_check((uintptr_t)buf, sizeof(type) * count,\
		    true, __RET_ADDR);					\
		bus_space_write_##func##_##width(tag, hnd, size, buf, 	\
		    count);						\
	}

ASAN_BUS_WRITE_FUNC(, 1, uint8_t)
ASAN_BUS_WRITE_FUNC(_stream, 1, uint8_t)
ASAN_BUS_WRITE_PTR_FUNC(multi, 1, uint8_t)
ASAN_BUS_WRITE_PTR_FUNC(multi_stream, 1, uint8_t)
ASAN_BUS_WRITE_PTR_FUNC(region, 1, uint8_t)
ASAN_BUS_WRITE_PTR_FUNC(region_stream, 1, uint8_t)

ASAN_BUS_WRITE_FUNC(, 2, uint16_t)
ASAN_BUS_WRITE_FUNC(_stream, 2, uint16_t)
ASAN_BUS_WRITE_PTR_FUNC(multi, 2, uint16_t)
ASAN_BUS_WRITE_PTR_FUNC(multi_stream, 2, uint16_t)
ASAN_BUS_WRITE_PTR_FUNC(region, 2, uint16_t)
ASAN_BUS_WRITE_PTR_FUNC(region_stream, 2, uint16_t)

ASAN_BUS_WRITE_FUNC(, 4, uint32_t)
ASAN_BUS_WRITE_FUNC(_stream, 4, uint32_t)
ASAN_BUS_WRITE_PTR_FUNC(multi, 4, uint32_t)
ASAN_BUS_WRITE_PTR_FUNC(multi_stream, 4, uint32_t)
ASAN_BUS_WRITE_PTR_FUNC(region, 4, uint32_t)
ASAN_BUS_WRITE_PTR_FUNC(region_stream, 4, uint32_t)

ASAN_BUS_WRITE_FUNC(, 8, uint64_t)

#define	ASAN_BUS_SET_FUNC(func, width, type)				\
	void kasan_bus_space_set_##func##_##width(bus_space_tag_t tag,	\
	    bus_space_handle_t hnd, bus_size_t offset, type value,	\
	    bus_size_t count)						\
	{								\
		bus_space_set_##func##_##width(tag, hnd, offset, value,	\
		    count);						\
	}

ASAN_BUS_SET_FUNC(multi, 1, uint8_t)
ASAN_BUS_SET_FUNC(region, 1, uint8_t)
ASAN_BUS_SET_FUNC(multi_stream, 1, uint8_t)
ASAN_BUS_SET_FUNC(region_stream, 1, uint8_t)

ASAN_BUS_SET_FUNC(multi, 2, uint16_t)
ASAN_BUS_SET_FUNC(region, 2, uint16_t)
ASAN_BUS_SET_FUNC(multi_stream, 2, uint16_t)
ASAN_BUS_SET_FUNC(region_stream, 2, uint16_t)

ASAN_BUS_SET_FUNC(multi, 4, uint32_t)
ASAN_BUS_SET_FUNC(region, 4, uint32_t)
ASAN_BUS_SET_FUNC(multi_stream, 4, uint32_t)
ASAN_BUS_SET_FUNC(region_stream, 4, uint32_t)

#define	ASAN_BUS_PEEK_FUNC(width, type)					\
	int kasan_bus_space_peek_##width(bus_space_tag_t tag,		\
	    bus_space_handle_t hnd, bus_size_t offset, type *valuep)	\
	{								\
		return (bus_space_peek_##width(tag, hnd, offset,	\
		    valuep));						\
	}

ASAN_BUS_PEEK_FUNC(1, uint8_t)
ASAN_BUS_PEEK_FUNC(2, uint16_t)
ASAN_BUS_PEEK_FUNC(4, uint32_t)
ASAN_BUS_PEEK_FUNC(8, uint64_t)

#define	ASAN_BUS_POKE_FUNC(width, type)					\
	int kasan_bus_space_poke_##width(bus_space_tag_t tag,		\
	    bus_space_handle_t hnd, bus_size_t offset, type value)	\
	{								\
		return (bus_space_poke_##width(tag, hnd, offset,	\
		    value));						\
	}

ASAN_BUS_POKE_FUNC(1, uint8_t)
ASAN_BUS_POKE_FUNC(2, uint16_t)
ASAN_BUS_POKE_FUNC(4, uint32_t)
ASAN_BUS_POKE_FUNC(8, uint64_t)

/* -------------------------------------------------------------------------- */

void __asan_register_globals(struct __asan_global *, size_t);
void __asan_unregister_globals(struct __asan_global *, size_t);

void
__asan_register_globals(struct __asan_global *globals, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++) {
		kasan_mark(globals[i].beg, globals[i].size,
		    globals[i].size_with_redzone, KASAN_GENERIC_REDZONE);
	}
}

void
__asan_unregister_globals(struct __asan_global *globals, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++) {
		kasan_mark(globals[i].beg, globals[i].size_with_redzone,
		    globals[i].size_with_redzone, 0);
	}
}

#define ASAN_LOAD_STORE(size)					\
	void __asan_load##size(unsigned long);			\
	void __asan_load##size(unsigned long addr)		\
	{							\
		kasan_shadow_check(addr, size, false, __RET_ADDR);\
	} 							\
	void __asan_load##size##_noabort(unsigned long);	\
	void __asan_load##size##_noabort(unsigned long addr)	\
	{							\
		kasan_shadow_check(addr, size, false, __RET_ADDR);\
	}							\
	void __asan_store##size(unsigned long);			\
	void __asan_store##size(unsigned long addr)		\
	{							\
		kasan_shadow_check(addr, size, true, __RET_ADDR);\
	}							\
	void __asan_store##size##_noabort(unsigned long);	\
	void __asan_store##size##_noabort(unsigned long addr)	\
	{							\
		kasan_shadow_check(addr, size, true, __RET_ADDR);\
	}

ASAN_LOAD_STORE(1);
ASAN_LOAD_STORE(2);
ASAN_LOAD_STORE(4);
ASAN_LOAD_STORE(8);
ASAN_LOAD_STORE(16);

void __asan_loadN(unsigned long, size_t);
void __asan_loadN_noabort(unsigned long, size_t);
void __asan_storeN(unsigned long, size_t);
void __asan_storeN_noabort(unsigned long, size_t);
void __asan_handle_no_return(void);

void
__asan_loadN(unsigned long addr, size_t size)
{
	kasan_shadow_check(addr, size, false, __RET_ADDR);
}

void
__asan_loadN_noabort(unsigned long addr, size_t size)
{
	kasan_shadow_check(addr, size, false, __RET_ADDR);
}

void
__asan_storeN(unsigned long addr, size_t size)
{
	kasan_shadow_check(addr, size, true, __RET_ADDR);
}

void
__asan_storeN_noabort(unsigned long addr, size_t size)
{
	kasan_shadow_check(addr, size, true, __RET_ADDR);
}

void
__asan_handle_no_return(void)
{
	/* nothing */
}

#define ASAN_SET_SHADOW(byte) \
	void __asan_set_shadow_##byte(void *, size_t);			\
	void __asan_set_shadow_##byte(void *addr, size_t size)		\
	{								\
		__builtin_memset((void *)addr, 0x##byte, size);		\
	}

ASAN_SET_SHADOW(00);
ASAN_SET_SHADOW(f1);
ASAN_SET_SHADOW(f2);
ASAN_SET_SHADOW(f3);
ASAN_SET_SHADOW(f5);
ASAN_SET_SHADOW(f8);

void __asan_poison_stack_memory(const void *, size_t);
void __asan_unpoison_stack_memory(const void *, size_t);

void
__asan_poison_stack_memory(const void *addr, size_t size)
{
	size = roundup(size, KASAN_SHADOW_SCALE);
	kasan_shadow_Nbyte_fill(addr, size, KASAN_USE_AFTER_SCOPE);
}

void
__asan_unpoison_stack_memory(const void *addr, size_t size)
{
	kasan_shadow_Nbyte_markvalid(addr, size);
}

void __asan_alloca_poison(const void *, size_t);
void __asan_allocas_unpoison(const void *, const void *);

void
__asan_alloca_poison(const void *addr, size_t size)
{
	const void *l, *r;

	KASSERT((vm_offset_t)addr % KASAN_ALLOCA_SCALE_SIZE == 0,
	    ("%s: invalid address %p", __func__, addr));

	l = (const uint8_t *)addr - KASAN_ALLOCA_SCALE_SIZE;
	r = (const uint8_t *)addr + roundup(size, KASAN_ALLOCA_SCALE_SIZE);

	kasan_shadow_Nbyte_fill(l, KASAN_ALLOCA_SCALE_SIZE, KASAN_STACK_LEFT);
	kasan_mark(addr, size, roundup(size, KASAN_ALLOCA_SCALE_SIZE),
	    KASAN_STACK_MID);
	kasan_shadow_Nbyte_fill(r, KASAN_ALLOCA_SCALE_SIZE, KASAN_STACK_RIGHT);
}

void
__asan_allocas_unpoison(const void *stkbegin, const void *stkend)
{
	size_t size;

	if (__predict_false(!stkbegin))
		return;
	if (__predict_false((uintptr_t)stkbegin > (uintptr_t)stkend))
		return;
	size = (uintptr_t)stkend - (uintptr_t)stkbegin;

	kasan_shadow_Nbyte_fill(stkbegin, size, 0);
}

void __asan_poison_memory_region(const void *addr, size_t size);
void __asan_unpoison_memory_region(const void *addr, size_t size);

void
__asan_poison_memory_region(const void *addr, size_t size)
{
}

void
__asan_unpoison_memory_region(const void *addr, size_t size)
{
}
