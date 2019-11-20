/*	$NetBSD: subr_csan.c,v 1.5 2019/11/15 08:11:37 maxv Exp $	*/

/*
 * Copyright (c) 2019 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Maxime Villard.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_csan.c,v 1.5 2019/11/15 08:11:37 maxv Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/csan.h>
#include <sys/cpu.h>

#ifdef KCSAN_PANIC
#define REPORT panic
#else
#define REPORT printf
#endif

typedef struct {
	uintptr_t addr;
	uint32_t size;
	bool write:1;
	bool atomic:1;
	uintptr_t pc;
} csan_cell_t;

typedef struct {
	bool inited;
	uint32_t cnt;
	csan_cell_t cell;
} csan_cpu_t;

static csan_cpu_t kcsan_cpus[MAXCPUS];
static bool kcsan_enabled __read_mostly;

#define __RET_ADDR	(uintptr_t)__builtin_return_address(0)

#define KCSAN_NACCESSES	1024
#define KCSAN_DELAY	10	/* 10 microseconds */

/* -------------------------------------------------------------------------- */

/* The MD code. */
#include <machine/csan.h>

/* -------------------------------------------------------------------------- */

void
kcsan_init(void)
{
	kcsan_enabled = true;
}

void
kcsan_cpu_init(struct cpu_info *ci)
{
	kcsan_cpus[cpu_index(ci)].inited = true;
}

/* -------------------------------------------------------------------------- */

static inline void
kcsan_report(csan_cell_t *new, cpuid_t newcpu, csan_cell_t *old, cpuid_t oldcpu)
{
	const char *newsym, *oldsym;

	if (ksyms_getname(NULL, &newsym, (vaddr_t)new->pc, KSYMS_PROC) != 0) {
		newsym = "Unknown";
	}
	if (ksyms_getname(NULL, &oldsym, (vaddr_t)old->pc, KSYMS_PROC) != 0) {
		oldsym = "Unknown";
	}
	REPORT("CSan: Racy Access "
	    "[Cpu%lu %s%s Addr=%p Size=%u PC=%p<%s>] "
	    "[Cpu%lu %s%s Addr=%p Size=%u PC=%p<%s>]\n",
	    newcpu,
	    (new->atomic ? "Atomic " : ""), (new->write ? "Write" : "Read"),
	    (void *)new->addr, new->size, (void *)new->pc, newsym,
	    oldcpu,
	    (old->atomic ? "Atomic " : ""), (old->write ? "Write" : "Read"),
	    (void *)old->addr, old->size, (void *)old->pc, oldsym);
	kcsan_md_unwind();
}

static inline bool
kcsan_access_is_atomic(csan_cell_t *new, csan_cell_t *old)
{
	if (new->write && !new->atomic)
		return false;
	if (old->write && !old->atomic)
		return false;
	return true;
}

static inline void
kcsan_access(uintptr_t addr, size_t size, bool write, bool atomic, uintptr_t pc)
{
	csan_cell_t old, new;
	csan_cpu_t *cpu;
	uint64_t intr;
	size_t i;

	if (__predict_false(!kcsan_enabled))
		return;
	if (__predict_false(kcsan_md_unsupported((vaddr_t)addr)))
		return;

	new.addr = addr;
	new.size = size;
	new.write = write;
	new.atomic = atomic;
	new.pc = pc;

	for (i = 0; i < ncpu; i++) {
		__builtin_memcpy(&old, &kcsan_cpus[i].cell, sizeof(old));

		if (old.addr + old.size <= new.addr)
			continue;
		if (new.addr + new.size <= old.addr)
			continue;
		if (__predict_true(!old.write && !new.write))
			continue;
		if (__predict_true(kcsan_access_is_atomic(&new, &old)))
			continue;

		kcsan_report(&new, cpu_number(), &old, i);
		break;
	}

	if (__predict_false(!kcsan_md_is_avail()))
		return;

	kcsan_md_disable_intrs(&intr);

	cpu = &kcsan_cpus[cpu_number()];
	if (__predict_false(!cpu->inited))
		goto out;
	cpu->cnt = (cpu->cnt + 1) % KCSAN_NACCESSES;
	if (__predict_true(cpu->cnt != 0))
		goto out;

	__builtin_memcpy(&cpu->cell, &new, sizeof(new));
	kcsan_md_delay(KCSAN_DELAY);
	__builtin_memset(&cpu->cell, 0, sizeof(new));

out:
	kcsan_md_enable_intrs(&intr);
}

#define CSAN_READ(size)							\
	void __tsan_read##size(uintptr_t);				\
	void __tsan_read##size(uintptr_t addr)				\
	{								\
		kcsan_access(addr, size, false, false, __RET_ADDR);	\
	}

CSAN_READ(1)
CSAN_READ(2)
CSAN_READ(4)
CSAN_READ(8)
CSAN_READ(16)

#define CSAN_WRITE(size)						\
	void __tsan_write##size(uintptr_t);				\
	void __tsan_write##size(uintptr_t addr)				\
	{								\
		kcsan_access(addr, size, true, false, __RET_ADDR);	\
	}

CSAN_WRITE(1)
CSAN_WRITE(2)
CSAN_WRITE(4)
CSAN_WRITE(8)
CSAN_WRITE(16)

void __tsan_read_range(uintptr_t, size_t);
void __tsan_write_range(uintptr_t, size_t);

void
__tsan_read_range(uintptr_t addr, size_t size)
{
	kcsan_access(addr, size, false, false, __RET_ADDR);
}

void
__tsan_write_range(uintptr_t addr, size_t size)
{
	kcsan_access(addr, size, true, false, __RET_ADDR);
}

void __tsan_init(void);
void __tsan_func_entry(void *);
void __tsan_func_exit(void);

void
__tsan_init(void)
{
}

void
__tsan_func_entry(void *call_pc)
{
}

void
__tsan_func_exit(void)
{
}

/* -------------------------------------------------------------------------- */

void *
kcsan_memcpy(void *dst, const void *src, size_t len)
{
	kcsan_access((uintptr_t)src, len, false, false, __RET_ADDR);
	kcsan_access((uintptr_t)dst, len, true, false, __RET_ADDR);
	return __builtin_memcpy(dst, src, len);
}

int
kcsan_memcmp(const void *b1, const void *b2, size_t len)
{
	kcsan_access((uintptr_t)b1, len, false, false, __RET_ADDR);
	kcsan_access((uintptr_t)b2, len, false, false, __RET_ADDR);
	return __builtin_memcmp(b1, b2, len);
}

void *
kcsan_memset(void *b, int c, size_t len)
{
	kcsan_access((uintptr_t)b, len, true, false, __RET_ADDR);
	return __builtin_memset(b, c, len);
}

void *
kcsan_memmove(void *dst, const void *src, size_t len)
{
	kcsan_access((uintptr_t)src, len, false, false, __RET_ADDR);
	kcsan_access((uintptr_t)dst, len, true, false, __RET_ADDR);
	return __builtin_memmove(dst, src, len);
}

char *
kcsan_strcpy(char *dst, const char *src)
{
	char *save = dst;

	while (1) {
		kcsan_access((uintptr_t)src, 1, false, false, __RET_ADDR);
		kcsan_access((uintptr_t)dst, 1, true, false, __RET_ADDR);
		*dst = *src;
		if (*src == '\0')
			break;
		src++, dst++;
	}

	return save;
}

int
kcsan_strcmp(const char *s1, const char *s2)
{
	while (1) {
		kcsan_access((uintptr_t)s1, 1, false, false, __RET_ADDR);
		kcsan_access((uintptr_t)s2, 1, false, false, __RET_ADDR);
		if (*s1 != *s2)
			break;
		if (*s1 == '\0')
			return 0;
		s1++, s2++;
	}

	return (*(const unsigned char *)s1 - *(const unsigned char *)s2);
}

size_t
kcsan_strlen(const char *str)
{
	const char *s;

	s = str;
	while (1) {
		kcsan_access((uintptr_t)s, 1, false, false, __RET_ADDR);
		if (*s == '\0')
			break;
		s++;
	}

	return (s - str);
}

#undef kcopy
#undef copystr
#undef copyinstr
#undef copyoutstr
#undef copyin
#undef copyout

int	kcsan_kcopy(const void *, void *, size_t);
int	kcsan_copystr(const void *, void *, size_t, size_t *);
int	kcsan_copyinstr(const void *, void *, size_t, size_t *);
int	kcsan_copyoutstr(const void *, void *, size_t, size_t *);
int	kcsan_copyin(const void *, void *, size_t);
int	kcsan_copyout(const void *, void *, size_t);
int	kcopy(const void *, void *, size_t);
int	copystr(const void *, void *, size_t, size_t *);
int	copyinstr(const void *, void *, size_t, size_t *);
int	copyoutstr(const void *, void *, size_t, size_t *);
int	copyin(const void *, void *, size_t);
int	copyout(const void *, void *, size_t);

int
kcsan_kcopy(const void *src, void *dst, size_t len)
{
	kcsan_access((uintptr_t)src, len, false, false, __RET_ADDR);
	kcsan_access((uintptr_t)dst, len, true, false, __RET_ADDR);
	return kcopy(src, dst, len);
}

int
kcsan_copystr(const void *kfaddr, void *kdaddr, size_t len, size_t *done)
{
	kcsan_access((uintptr_t)kdaddr, len, true, false, __RET_ADDR);
	return copystr(kfaddr, kdaddr, len, done);
}

int
kcsan_copyin(const void *uaddr, void *kaddr, size_t len)
{
	kcsan_access((uintptr_t)kaddr, len, true, false, __RET_ADDR);
	return copyin(uaddr, kaddr, len);
}

int
kcsan_copyout(const void *kaddr, void *uaddr, size_t len)
{
	kcsan_access((uintptr_t)kaddr, len, false, false, __RET_ADDR);
	return copyout(kaddr, uaddr, len);
}

int
kcsan_copyinstr(const void *uaddr, void *kaddr, size_t len, size_t *done)
{
	kcsan_access((uintptr_t)kaddr, len, true, false, __RET_ADDR);
	return copyinstr(uaddr, kaddr, len, done);
}

int
kcsan_copyoutstr(const void *kaddr, void *uaddr, size_t len, size_t *done)
{
	kcsan_access((uintptr_t)kaddr, len, false, false, __RET_ADDR);
	return copyoutstr(kaddr, uaddr, len, done);
}

/* -------------------------------------------------------------------------- */

#undef atomic_add_32
#undef atomic_add_int
#undef atomic_add_long
#undef atomic_add_ptr
#undef atomic_add_64
#undef atomic_add_32_nv
#undef atomic_add_int_nv
#undef atomic_add_long_nv
#undef atomic_add_ptr_nv
#undef atomic_add_64_nv
#undef atomic_and_32
#undef atomic_and_uint
#undef atomic_and_ulong
#undef atomic_and_64
#undef atomic_and_32_nv
#undef atomic_and_uint_nv
#undef atomic_and_ulong_nv
#undef atomic_and_64_nv
#undef atomic_or_32
#undef atomic_or_uint
#undef atomic_or_ulong
#undef atomic_or_64
#undef atomic_or_32_nv
#undef atomic_or_uint_nv
#undef atomic_or_ulong_nv
#undef atomic_or_64_nv
#undef atomic_cas_32
#undef atomic_cas_uint
#undef atomic_cas_ulong
#undef atomic_cas_ptr
#undef atomic_cas_64
#undef atomic_cas_32_ni
#undef atomic_cas_uint_ni
#undef atomic_cas_ulong_ni
#undef atomic_cas_ptr_ni
#undef atomic_cas_64_ni
#undef atomic_swap_32
#undef atomic_swap_uint
#undef atomic_swap_ulong
#undef atomic_swap_ptr
#undef atomic_swap_64
#undef atomic_dec_32
#undef atomic_dec_uint
#undef atomic_dec_ulong
#undef atomic_dec_ptr
#undef atomic_dec_64
#undef atomic_dec_32_nv
#undef atomic_dec_uint_nv
#undef atomic_dec_ulong_nv
#undef atomic_dec_ptr_nv
#undef atomic_dec_64_nv
#undef atomic_inc_32
#undef atomic_inc_uint
#undef atomic_inc_ulong
#undef atomic_inc_ptr
#undef atomic_inc_64
#undef atomic_inc_32_nv
#undef atomic_inc_uint_nv
#undef atomic_inc_ulong_nv
#undef atomic_inc_ptr_nv
#undef atomic_inc_64_nv

#define CSAN_ATOMIC_FUNC_ADD(name, tret, targ1, targ2) \
	void atomic_add_##name(volatile targ1 *, targ2); \
	void kcsan_atomic_add_##name(volatile targ1 *, targ2); \
	void kcsan_atomic_add_##name(volatile targ1 *ptr, targ2 val) \
	{ \
		kcsan_access((uintptr_t)ptr, sizeof(tret), true, true, \
		    __RET_ADDR); \
		atomic_add_##name(ptr, val); \
	} \
	tret atomic_add_##name##_nv(volatile targ1 *, targ2); \
	tret kcsan_atomic_add_##name##_nv(volatile targ1 *, targ2); \
	tret kcsan_atomic_add_##name##_nv(volatile targ1 *ptr, targ2 val) \
	{ \
		kcsan_access((uintptr_t)ptr, sizeof(tret), true, true, \
		    __RET_ADDR); \
		return atomic_add_##name##_nv(ptr, val); \
	}

#define CSAN_ATOMIC_FUNC_AND(name, tret, targ1, targ2) \
	void atomic_and_##name(volatile targ1 *, targ2); \
	void kcsan_atomic_and_##name(volatile targ1 *, targ2); \
	void kcsan_atomic_and_##name(volatile targ1 *ptr, targ2 val) \
	{ \
		kcsan_access((uintptr_t)ptr, sizeof(tret), true, true, \
		    __RET_ADDR); \
		atomic_and_##name(ptr, val); \
	} \
	tret atomic_and_##name##_nv(volatile targ1 *, targ2); \
	tret kcsan_atomic_and_##name##_nv(volatile targ1 *, targ2); \
	tret kcsan_atomic_and_##name##_nv(volatile targ1 *ptr, targ2 val) \
	{ \
		kcsan_access((uintptr_t)ptr, sizeof(tret), true, true, \
		    __RET_ADDR); \
		return atomic_and_##name##_nv(ptr, val); \
	}

#define CSAN_ATOMIC_FUNC_OR(name, tret, targ1, targ2) \
	void atomic_or_##name(volatile targ1 *, targ2); \
	void kcsan_atomic_or_##name(volatile targ1 *, targ2); \
	void kcsan_atomic_or_##name(volatile targ1 *ptr, targ2 val) \
	{ \
		kcsan_access((uintptr_t)ptr, sizeof(tret), true, true, \
		    __RET_ADDR); \
		atomic_or_##name(ptr, val); \
	} \
	tret atomic_or_##name##_nv(volatile targ1 *, targ2); \
	tret kcsan_atomic_or_##name##_nv(volatile targ1 *, targ2); \
	tret kcsan_atomic_or_##name##_nv(volatile targ1 *ptr, targ2 val) \
	{ \
		kcsan_access((uintptr_t)ptr, sizeof(tret), true, true, \
		    __RET_ADDR); \
		return atomic_or_##name##_nv(ptr, val); \
	}

#define CSAN_ATOMIC_FUNC_CAS(name, tret, targ1, targ2) \
	tret atomic_cas_##name(volatile targ1 *, targ2, targ2); \
	tret kcsan_atomic_cas_##name(volatile targ1 *, targ2, targ2); \
	tret kcsan_atomic_cas_##name(volatile targ1 *ptr, targ2 exp, targ2 new) \
	{ \
		kcsan_access((uintptr_t)ptr, sizeof(tret), true, true, \
		    __RET_ADDR); \
		return atomic_cas_##name(ptr, exp, new); \
	} \
	tret atomic_cas_##name##_ni(volatile targ1 *, targ2, targ2); \
	tret kcsan_atomic_cas_##name##_ni(volatile targ1 *, targ2, targ2); \
	tret kcsan_atomic_cas_##name##_ni(volatile targ1 *ptr, targ2 exp, targ2 new) \
	{ \
		kcsan_access((uintptr_t)ptr, sizeof(tret), true, true, \
		    __RET_ADDR); \
		return atomic_cas_##name##_ni(ptr, exp, new); \
	}

#define CSAN_ATOMIC_FUNC_SWAP(name, tret, targ1, targ2) \
	tret atomic_swap_##name(volatile targ1 *, targ2); \
	tret kcsan_atomic_swap_##name(volatile targ1 *, targ2); \
	tret kcsan_atomic_swap_##name(volatile targ1 *ptr, targ2 val) \
	{ \
		kcsan_access((uintptr_t)ptr, sizeof(tret), true, true, \
		    __RET_ADDR); \
		return atomic_swap_##name(ptr, val); \
	}

#define CSAN_ATOMIC_FUNC_DEC(name, tret, targ1) \
	void atomic_dec_##name(volatile targ1 *); \
	void kcsan_atomic_dec_##name(volatile targ1 *); \
	void kcsan_atomic_dec_##name(volatile targ1 *ptr) \
	{ \
		kcsan_access((uintptr_t)ptr, sizeof(tret), true, true, \
		    __RET_ADDR); \
		atomic_dec_##name(ptr); \
	} \
	tret atomic_dec_##name##_nv(volatile targ1 *); \
	tret kcsan_atomic_dec_##name##_nv(volatile targ1 *); \
	tret kcsan_atomic_dec_##name##_nv(volatile targ1 *ptr) \
	{ \
		kcsan_access((uintptr_t)ptr, sizeof(tret), true, true, \
		    __RET_ADDR); \
		return atomic_dec_##name##_nv(ptr); \
	}

#define CSAN_ATOMIC_FUNC_INC(name, tret, targ1) \
	void atomic_inc_##name(volatile targ1 *); \
	void kcsan_atomic_inc_##name(volatile targ1 *); \
	void kcsan_atomic_inc_##name(volatile targ1 *ptr) \
	{ \
		kcsan_access((uintptr_t)ptr, sizeof(tret), true, true, \
		    __RET_ADDR); \
		atomic_inc_##name(ptr); \
	} \
	tret atomic_inc_##name##_nv(volatile targ1 *); \
	tret kcsan_atomic_inc_##name##_nv(volatile targ1 *); \
	tret kcsan_atomic_inc_##name##_nv(volatile targ1 *ptr) \
	{ \
		kcsan_access((uintptr_t)ptr, sizeof(tret), true, true, \
		    __RET_ADDR); \
		return atomic_inc_##name##_nv(ptr); \
	}

CSAN_ATOMIC_FUNC_ADD(32, uint32_t, uint32_t, int32_t);
CSAN_ATOMIC_FUNC_ADD(64, uint64_t, uint64_t, int64_t);
CSAN_ATOMIC_FUNC_ADD(int, unsigned int, unsigned int, int);
CSAN_ATOMIC_FUNC_ADD(long, unsigned long, unsigned long, long);
CSAN_ATOMIC_FUNC_ADD(ptr, void *, void, ssize_t);

CSAN_ATOMIC_FUNC_AND(32, uint32_t, uint32_t, uint32_t);
CSAN_ATOMIC_FUNC_AND(64, uint64_t, uint64_t, uint64_t);
CSAN_ATOMIC_FUNC_AND(uint, unsigned int, unsigned int, unsigned int);
CSAN_ATOMIC_FUNC_AND(ulong, unsigned long, unsigned long, unsigned long);

CSAN_ATOMIC_FUNC_OR(32, uint32_t, uint32_t, uint32_t);
CSAN_ATOMIC_FUNC_OR(64, uint64_t, uint64_t, uint64_t);
CSAN_ATOMIC_FUNC_OR(uint, unsigned int, unsigned int, unsigned int);
CSAN_ATOMIC_FUNC_OR(ulong, unsigned long, unsigned long, unsigned long);

CSAN_ATOMIC_FUNC_CAS(32, uint32_t, uint32_t, uint32_t);
CSAN_ATOMIC_FUNC_CAS(64, uint64_t, uint64_t, uint64_t);
CSAN_ATOMIC_FUNC_CAS(uint, unsigned int, unsigned int, unsigned int);
CSAN_ATOMIC_FUNC_CAS(ulong, unsigned long, unsigned long, unsigned long);
CSAN_ATOMIC_FUNC_CAS(ptr, void *, void, void *);

CSAN_ATOMIC_FUNC_SWAP(32, uint32_t, uint32_t, uint32_t);
CSAN_ATOMIC_FUNC_SWAP(64, uint64_t, uint64_t, uint64_t);
CSAN_ATOMIC_FUNC_SWAP(uint, unsigned int, unsigned int, unsigned int);
CSAN_ATOMIC_FUNC_SWAP(ulong, unsigned long, unsigned long, unsigned long);
CSAN_ATOMIC_FUNC_SWAP(ptr, void *, void, void *);

CSAN_ATOMIC_FUNC_DEC(32, uint32_t, uint32_t)
CSAN_ATOMIC_FUNC_DEC(64, uint64_t, uint64_t)
CSAN_ATOMIC_FUNC_DEC(uint, unsigned int, unsigned int);
CSAN_ATOMIC_FUNC_DEC(ulong, unsigned long, unsigned long);
CSAN_ATOMIC_FUNC_DEC(ptr, void *, void);

CSAN_ATOMIC_FUNC_INC(32, uint32_t, uint32_t)
CSAN_ATOMIC_FUNC_INC(64, uint64_t, uint64_t)
CSAN_ATOMIC_FUNC_INC(uint, unsigned int, unsigned int);
CSAN_ATOMIC_FUNC_INC(ulong, unsigned long, unsigned long);
CSAN_ATOMIC_FUNC_INC(ptr, void *, void);

/* -------------------------------------------------------------------------- */

#include <sys/bus.h>

#undef bus_space_read_multi_1
#undef bus_space_read_multi_2
#undef bus_space_read_multi_4
#undef bus_space_read_multi_8
#undef bus_space_read_multi_stream_1
#undef bus_space_read_multi_stream_2
#undef bus_space_read_multi_stream_4
#undef bus_space_read_multi_stream_8
#undef bus_space_read_region_1
#undef bus_space_read_region_2
#undef bus_space_read_region_4
#undef bus_space_read_region_8
#undef bus_space_read_region_stream_1
#undef bus_space_read_region_stream_2
#undef bus_space_read_region_stream_4
#undef bus_space_read_region_stream_8
#undef bus_space_write_multi_1
#undef bus_space_write_multi_2
#undef bus_space_write_multi_4
#undef bus_space_write_multi_8
#undef bus_space_write_multi_stream_1
#undef bus_space_write_multi_stream_2
#undef bus_space_write_multi_stream_4
#undef bus_space_write_multi_stream_8
#undef bus_space_write_region_1
#undef bus_space_write_region_2
#undef bus_space_write_region_4
#undef bus_space_write_region_8
#undef bus_space_write_region_stream_1
#undef bus_space_write_region_stream_2
#undef bus_space_write_region_stream_4
#undef bus_space_write_region_stream_8

#define CSAN_BUS_READ_FUNC(bytes, bits) \
	void bus_space_read_multi_##bytes(bus_space_tag_t, bus_space_handle_t,	\
	    bus_size_t, uint##bits##_t *, bus_size_t);				\
	void kcsan_bus_space_read_multi_##bytes(bus_space_tag_t,		\
	    bus_space_handle_t, bus_size_t, uint##bits##_t *, bus_size_t);	\
	void kcsan_bus_space_read_multi_##bytes(bus_space_tag_t tag,		\
	    bus_space_handle_t hnd, bus_size_t size, uint##bits##_t *buf,	\
	    bus_size_t count)							\
	{									\
		kcsan_access((uintptr_t)buf, sizeof(uint##bits##_t) * count,	\
		    false, false, __RET_ADDR);					\
		bus_space_read_multi_##bytes(tag, hnd, size, buf, count);	\
	}									\
	void bus_space_read_multi_stream_##bytes(bus_space_tag_t,		\
	    bus_space_handle_t, bus_size_t, uint##bits##_t *, bus_size_t);	\
	void kcsan_bus_space_read_multi_stream_##bytes(bus_space_tag_t,		\
	    bus_space_handle_t, bus_size_t, uint##bits##_t *, bus_size_t);	\
	void kcsan_bus_space_read_multi_stream_##bytes(bus_space_tag_t tag,	\
	    bus_space_handle_t hnd, bus_size_t size, uint##bits##_t *buf,	\
	    bus_size_t count)							\
	{									\
		kcsan_access((uintptr_t)buf, sizeof(uint##bits##_t) * count,	\
		    false, false, __RET_ADDR);					\
		bus_space_read_multi_stream_##bytes(tag, hnd, size, buf, count);\
	}									\
	void bus_space_read_region_##bytes(bus_space_tag_t, bus_space_handle_t,	\
	    bus_size_t, uint##bits##_t *, bus_size_t);				\
	void kcsan_bus_space_read_region_##bytes(bus_space_tag_t,		\
	    bus_space_handle_t, bus_size_t, uint##bits##_t *, bus_size_t);	\
	void kcsan_bus_space_read_region_##bytes(bus_space_tag_t tag,		\
	    bus_space_handle_t hnd, bus_size_t size, uint##bits##_t *buf,	\
	    bus_size_t count)							\
	{									\
		kcsan_access((uintptr_t)buf, sizeof(uint##bits##_t) * count,	\
		    false, false, __RET_ADDR);					\
		bus_space_read_region_##bytes(tag, hnd, size, buf, count);	\
	}									\
	void bus_space_read_region_stream_##bytes(bus_space_tag_t,		\
	    bus_space_handle_t, bus_size_t, uint##bits##_t *, bus_size_t);	\
	void kcsan_bus_space_read_region_stream_##bytes(bus_space_tag_t,	\
	    bus_space_handle_t, bus_size_t, uint##bits##_t *, bus_size_t);	\
	void kcsan_bus_space_read_region_stream_##bytes(bus_space_tag_t tag,	\
	    bus_space_handle_t hnd, bus_size_t size, uint##bits##_t *buf,	\
	    bus_size_t count)							\
	{									\
		kcsan_access((uintptr_t)buf, sizeof(uint##bits##_t) * count,	\
		    false, false, __RET_ADDR);					\
		bus_space_read_region_stream_##bytes(tag, hnd, size, buf, count);\
	}

#define CSAN_BUS_WRITE_FUNC(bytes, bits) \
	void bus_space_write_multi_##bytes(bus_space_tag_t, bus_space_handle_t,	\
	    bus_size_t, const uint##bits##_t *, bus_size_t);			\
	void kcsan_bus_space_write_multi_##bytes(bus_space_tag_t,		\
	    bus_space_handle_t, bus_size_t, const uint##bits##_t *, bus_size_t);\
	void kcsan_bus_space_write_multi_##bytes(bus_space_tag_t tag,		\
	    bus_space_handle_t hnd, bus_size_t size, const uint##bits##_t *buf,	\
	    bus_size_t count)							\
	{									\
		kcsan_access((uintptr_t)buf, sizeof(uint##bits##_t) * count,	\
		    true, false, __RET_ADDR);					\
		bus_space_write_multi_##bytes(tag, hnd, size, buf, count);	\
	}									\
	void bus_space_write_multi_stream_##bytes(bus_space_tag_t,		\
	    bus_space_handle_t, bus_size_t, const uint##bits##_t *, bus_size_t);\
	void kcsan_bus_space_write_multi_stream_##bytes(bus_space_tag_t,	\
	    bus_space_handle_t, bus_size_t, const uint##bits##_t *, bus_size_t);\
	void kcsan_bus_space_write_multi_stream_##bytes(bus_space_tag_t tag,	\
	    bus_space_handle_t hnd, bus_size_t size, const uint##bits##_t *buf,	\
	    bus_size_t count)							\
	{									\
		kcsan_access((uintptr_t)buf, sizeof(uint##bits##_t) * count,	\
		    true, false, __RET_ADDR);					\
		bus_space_write_multi_stream_##bytes(tag, hnd, size, buf, count);\
	}									\
	void bus_space_write_region_##bytes(bus_space_tag_t, bus_space_handle_t,\
	    bus_size_t, const uint##bits##_t *, bus_size_t);			\
	void kcsan_bus_space_write_region_##bytes(bus_space_tag_t,		\
	    bus_space_handle_t, bus_size_t, const uint##bits##_t *, bus_size_t);\
	void kcsan_bus_space_write_region_##bytes(bus_space_tag_t tag,		\
	    bus_space_handle_t hnd, bus_size_t size, const uint##bits##_t *buf,	\
	    bus_size_t count)							\
	{									\
		kcsan_access((uintptr_t)buf, sizeof(uint##bits##_t) * count,	\
		    true, false, __RET_ADDR);					\
		bus_space_write_region_##bytes(tag, hnd, size, buf, count);	\
	}									\
	void bus_space_write_region_stream_##bytes(bus_space_tag_t,		\
	    bus_space_handle_t, bus_size_t, const uint##bits##_t *, bus_size_t);\
	void kcsan_bus_space_write_region_stream_##bytes(bus_space_tag_t,	\
	    bus_space_handle_t, bus_size_t, const uint##bits##_t *, bus_size_t);\
	void kcsan_bus_space_write_region_stream_##bytes(bus_space_tag_t tag,	\
	    bus_space_handle_t hnd, bus_size_t size, const uint##bits##_t *buf,	\
	    bus_size_t count)							\
	{									\
		kcsan_access((uintptr_t)buf, sizeof(uint##bits##_t) * count,	\
		    true, false, __RET_ADDR);					\
		bus_space_write_region_stream_##bytes(tag, hnd, size, buf, count);\
	}

CSAN_BUS_READ_FUNC(1, 8)
CSAN_BUS_READ_FUNC(2, 16)
CSAN_BUS_READ_FUNC(4, 32)
CSAN_BUS_READ_FUNC(8, 64)

CSAN_BUS_WRITE_FUNC(1, 8)
CSAN_BUS_WRITE_FUNC(2, 16)
CSAN_BUS_WRITE_FUNC(4, 32)
CSAN_BUS_WRITE_FUNC(8, 64)
