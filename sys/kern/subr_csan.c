/*	$NetBSD: subr_csan.c,v 1.5 2019/11/15 08:11:37 maxv Exp $	*/

/*
 * Copyright (c) 2019 The NetBSD Foundation, Inc.
 * All rights reserved.
 * Copyright (c) 2019 Andrew Turner
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

#define	KCSAN_RUNTIME

#include "opt_ddb.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/cpu.h>
#include <sys/csan.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/systm.h>

#include <ddb/ddb.h>
#include <ddb/db_sym.h>

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

static csan_cpu_t kcsan_cpus[MAXCPU];
static bool kcsan_enabled __read_mostly;

#define __RET_ADDR	(uintptr_t)__builtin_return_address(0)

#define KCSAN_NACCESSES	1024
#define KCSAN_DELAY	10	/* 10 microseconds */

/* -------------------------------------------------------------------------- */

/* The MD code. */
#include <machine/csan.h>

/* -------------------------------------------------------------------------- */

static void
kcsan_enable(void *dummy __unused)
{

	printf("Enabling KCSCAN, expect reduced performance.\n");
	kcsan_enabled = true;
}
SYSINIT(kcsan_enable, SI_SUB_SMP, SI_ORDER_SECOND, kcsan_enable, NULL);

void
kcsan_cpu_init(u_int cpu)
{
	kcsan_cpus[cpu].inited = true;
}

/* -------------------------------------------------------------------------- */

static inline void
kcsan_report(csan_cell_t *new, u_int newcpu, csan_cell_t *old, u_int oldcpu)
{
	const char *newsym, *oldsym;
#ifdef DDB
	c_db_sym_t sym;
	db_expr_t offset;

	sym = db_search_symbol((vm_offset_t)new->pc, DB_STGY_PROC, &offset);
	db_symbol_values(sym, &newsym, NULL);

	sym = db_search_symbol((vm_offset_t)old->pc, DB_STGY_PROC, &offset);
	db_symbol_values(sym, &oldsym, NULL);
#else
	newsym = "";
	oldsym = "";
#endif
	REPORT("CSan: Racy Access "
	    "[Cpu%u %s%s Addr=%p Size=%u PC=%p<%s>] "
	    "[Cpu%u %s%s Addr=%p Size=%u PC=%p<%s>]\n",
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
	if (__predict_false(kcsan_md_unsupported((vm_offset_t)addr)))
		return;
	if (KERNEL_PANICKED())
		return;

	new.addr = addr;
	new.size = size;
	new.write = write;
	new.atomic = atomic;
	new.pc = pc;

	CPU_FOREACH(i) {
		__builtin_memcpy(&old, &kcsan_cpus[i].cell, sizeof(old));

		if (old.addr + old.size <= new.addr)
			continue;
		if (new.addr + new.size <= old.addr)
			continue;
		if (__predict_true(!old.write && !new.write))
			continue;
		if (__predict_true(kcsan_access_is_atomic(&new, &old)))
			continue;

		kcsan_report(&new, PCPU_GET(cpuid), &old, i);
		break;
	}

	if (__predict_false(!kcsan_md_is_avail()))
		return;

	kcsan_md_disable_intrs(&intr);

	cpu = &kcsan_cpus[PCPU_GET(cpuid)];
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
	}								\
	void __tsan_unaligned_read##size(uintptr_t);			\
	void __tsan_unaligned_read##size(uintptr_t addr)		\
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
	}								\
	void __tsan_unaligned_write##size(uintptr_t);			\
	void __tsan_unaligned_write##size(uintptr_t addr)		\
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

#undef copyin
#undef copyin_nofault
#undef copyinstr
#undef copyout
#undef copyout_nofault

int
kcsan_copyin(const void *uaddr, void *kaddr, size_t len)
{
	kcsan_access((uintptr_t)kaddr, len, true, false, __RET_ADDR);
	return copyin(uaddr, kaddr, len);
}

int
kcsan_copyinstr(const void *uaddr, void *kaddr, size_t len, size_t *done)
{
	kcsan_access((uintptr_t)kaddr, len, true, false, __RET_ADDR);
	return copyinstr(uaddr, kaddr, len, done);
}

int
kcsan_copyout(const void *kaddr, void *uaddr, size_t len)
{
	kcsan_access((uintptr_t)kaddr, len, false, false, __RET_ADDR);
	return copyout(kaddr, uaddr, len);
}

/* -------------------------------------------------------------------------- */

#include <machine/atomic.h>
#include <sys/_cscan_atomic.h>

#define	_CSAN_ATOMIC_FUNC_ADD(name, type)				\
	void kcsan_atomic_add_##name(volatile type *ptr, type val)	\
	{								\
		kcsan_access((uintptr_t)ptr, sizeof(type), true, true,	\
		    __RET_ADDR);					\
		atomic_add_##name(ptr, val); 				\
	}

#define	CSAN_ATOMIC_FUNC_ADD(name, type)				\
	_CSAN_ATOMIC_FUNC_ADD(name, type)				\
	_CSAN_ATOMIC_FUNC_ADD(acq_##name, type)				\
	_CSAN_ATOMIC_FUNC_ADD(rel_##name, type)

#define	_CSAN_ATOMIC_FUNC_CLEAR(name, type)				\
	void kcsan_atomic_clear_##name(volatile type *ptr, type val)	\
	{								\
		kcsan_access((uintptr_t)ptr, sizeof(type), true, true,	\
		    __RET_ADDR);					\
		atomic_clear_##name(ptr, val); 				\
	}

#define	CSAN_ATOMIC_FUNC_CLEAR(name, type)				\
	_CSAN_ATOMIC_FUNC_CLEAR(name, type)				\
	_CSAN_ATOMIC_FUNC_CLEAR(acq_##name, type)			\
	_CSAN_ATOMIC_FUNC_CLEAR(rel_##name, type)

#define	_CSAN_ATOMIC_FUNC_CMPSET(name, type)				\
	int kcsan_atomic_cmpset_##name(volatile type *ptr, type val1,	\
	    type val2)							\
	{								\
		kcsan_access((uintptr_t)ptr, sizeof(type), true, true,	\
		    __RET_ADDR);					\
		return (atomic_cmpset_##name(ptr, val1, val2));		\
	}

#define	CSAN_ATOMIC_FUNC_CMPSET(name, type)				\
	_CSAN_ATOMIC_FUNC_CMPSET(name, type)				\
	_CSAN_ATOMIC_FUNC_CMPSET(acq_##name, type)			\
	_CSAN_ATOMIC_FUNC_CMPSET(rel_##name, type)

#define	_CSAN_ATOMIC_FUNC_FCMPSET(name, type)				\
	int kcsan_atomic_fcmpset_##name(volatile type *ptr, type *val1,	\
	    type val2)							\
	{								\
		kcsan_access((uintptr_t)ptr, sizeof(type), true, true,	\
		    __RET_ADDR);					\
		return (atomic_fcmpset_##name(ptr, val1, val2));	\
	}

#define	CSAN_ATOMIC_FUNC_FCMPSET(name, type)				\
	_CSAN_ATOMIC_FUNC_FCMPSET(name, type)				\
	_CSAN_ATOMIC_FUNC_FCMPSET(acq_##name, type)			\
	_CSAN_ATOMIC_FUNC_FCMPSET(rel_##name, type)

#define	CSAN_ATOMIC_FUNC_FETCHADD(name, type)				\
	type kcsan_atomic_fetchadd_##name(volatile type *ptr, type val)	\
	{								\
		kcsan_access((uintptr_t)ptr, sizeof(type), true, true,	\
		    __RET_ADDR);					\
		return (atomic_fetchadd_##name(ptr, val));		\
	}

#define	_CSAN_ATOMIC_FUNC_LOAD(name, type)				\
	type kcsan_atomic_load_##name(volatile type *ptr)		\
	{								\
		kcsan_access((uintptr_t)ptr, sizeof(type), false, true,	\
		    __RET_ADDR);					\
		return (atomic_load_##name(ptr));			\
	}

#define	CSAN_ATOMIC_FUNC_LOAD(name, type)				\
	_CSAN_ATOMIC_FUNC_LOAD(name, type)				\
	_CSAN_ATOMIC_FUNC_LOAD(acq_##name, type)			\

#define	CSAN_ATOMIC_FUNC_READANDCLEAR(name, type)			\
	type kcsan_atomic_readandclear_##name(volatile type *ptr)	\
	{								\
		kcsan_access((uintptr_t)ptr, sizeof(type), true, true,	\
		    __RET_ADDR);					\
		return (atomic_readandclear_##name(ptr));		\
	}

#define	_CSAN_ATOMIC_FUNC_SET(name, type)				\
	void kcsan_atomic_set_##name(volatile type *ptr, type val)	\
	{								\
		kcsan_access((uintptr_t)ptr, sizeof(type), true, true,	\
		    __RET_ADDR);					\
		atomic_set_##name(ptr, val); 				\
	}

#define	CSAN_ATOMIC_FUNC_SET(name, type)				\
	_CSAN_ATOMIC_FUNC_SET(name, type)				\
	_CSAN_ATOMIC_FUNC_SET(acq_##name, type)				\
	_CSAN_ATOMIC_FUNC_SET(rel_##name, type)

#define	_CSAN_ATOMIC_FUNC_SUBTRACT(name, type)				\
	void kcsan_atomic_subtract_##name(volatile type *ptr, type val)	\
	{								\
		kcsan_access((uintptr_t)ptr, sizeof(type), true, true,	\
		    __RET_ADDR);					\
		atomic_subtract_##name(ptr, val); 			\
	}

#define	CSAN_ATOMIC_FUNC_SUBTRACT(name, type)				\
	_CSAN_ATOMIC_FUNC_SUBTRACT(name, type)				\
	_CSAN_ATOMIC_FUNC_SUBTRACT(acq_##name, type)			\
	_CSAN_ATOMIC_FUNC_SUBTRACT(rel_##name, type)

#define	_CSAN_ATOMIC_FUNC_STORE(name, type)				\
	void kcsan_atomic_store_##name(volatile type *ptr, type val)	\
	{								\
		kcsan_access((uintptr_t)ptr, sizeof(type), true, true,	\
		    __RET_ADDR);					\
		atomic_store_##name(ptr, val); 				\
	}

#define	CSAN_ATOMIC_FUNC_STORE(name, type)				\
	_CSAN_ATOMIC_FUNC_STORE(name, type)				\
	_CSAN_ATOMIC_FUNC_STORE(rel_##name, type)

#define	CSAN_ATOMIC_FUNC_SWAP(name, type)				\
	type kcsan_atomic_swap_##name(volatile type *ptr, type val)	\
	{								\
		kcsan_access((uintptr_t)ptr, sizeof(type), true, true,	\
		    __RET_ADDR);					\
		return(atomic_swap_##name(ptr, val)); 			\
	}

#define	CSAN_ATOMIC_FUNC_TESTANDCLEAR(name, type)			\
	int kcsan_atomic_testandclear_##name(volatile type *ptr, u_int val) \
	{								\
		kcsan_access((uintptr_t)ptr, sizeof(type), true, true,	\
		    __RET_ADDR);					\
		return(atomic_testandclear_##name(ptr, val)); 		\
	}

#define	CSAN_ATOMIC_FUNC_TESTANDSET(name, type)				\
	int kcsan_atomic_testandset_##name(volatile type *ptr, u_int val) \
	{								\
		kcsan_access((uintptr_t)ptr, sizeof(type), true, true,	\
		    __RET_ADDR);					\
		return (atomic_testandset_##name(ptr, val)); 		\
	}

CSAN_ATOMIC_FUNC_ADD(8, uint8_t)
CSAN_ATOMIC_FUNC_CLEAR(8, uint8_t)
CSAN_ATOMIC_FUNC_CMPSET(8, uint8_t)
CSAN_ATOMIC_FUNC_FCMPSET(8, uint8_t)
CSAN_ATOMIC_FUNC_LOAD(8, uint8_t)
CSAN_ATOMIC_FUNC_SET(8, uint8_t)
CSAN_ATOMIC_FUNC_SUBTRACT(8, uint8_t)
_CSAN_ATOMIC_FUNC_STORE(8, uint8_t)
#if 0
CSAN_ATOMIC_FUNC_FETCHADD(8, uint8_t)
CSAN_ATOMIC_FUNC_READANDCLEAR(8, uint8_t)
CSAN_ATOMIC_FUNC_SWAP(8, uint8_t)
CSAN_ATOMIC_FUNC_TESTANDCLEAR(8, uint8_t)
CSAN_ATOMIC_FUNC_TESTANDSET(8, uint8_t)
#endif

CSAN_ATOMIC_FUNC_ADD(16, uint16_t)
CSAN_ATOMIC_FUNC_CLEAR(16, uint16_t)
CSAN_ATOMIC_FUNC_CMPSET(16, uint16_t)
CSAN_ATOMIC_FUNC_FCMPSET(16, uint16_t)
CSAN_ATOMIC_FUNC_LOAD(16, uint16_t)
CSAN_ATOMIC_FUNC_SET(16, uint16_t)
CSAN_ATOMIC_FUNC_SUBTRACT(16, uint16_t)
_CSAN_ATOMIC_FUNC_STORE(16, uint16_t)
#if 0
CSAN_ATOMIC_FUNC_FETCHADD(16, uint16_t)
CSAN_ATOMIC_FUNC_READANDCLEAR(16, uint16_t)
CSAN_ATOMIC_FUNC_SWAP(16, uint16_t)
CSAN_ATOMIC_FUNC_TESTANDCLEAR(16, uint16_t)
CSAN_ATOMIC_FUNC_TESTANDSET(16, uint16_t)
#endif

CSAN_ATOMIC_FUNC_ADD(32, uint32_t)
CSAN_ATOMIC_FUNC_CLEAR(32, uint32_t)
CSAN_ATOMIC_FUNC_CMPSET(32, uint32_t)
CSAN_ATOMIC_FUNC_FCMPSET(32, uint32_t)
CSAN_ATOMIC_FUNC_FETCHADD(32, uint32_t)
CSAN_ATOMIC_FUNC_LOAD(32, uint32_t)
CSAN_ATOMIC_FUNC_READANDCLEAR(32, uint32_t)
CSAN_ATOMIC_FUNC_SET(32, uint32_t)
CSAN_ATOMIC_FUNC_SUBTRACT(32, uint32_t)
CSAN_ATOMIC_FUNC_STORE(32, uint32_t)
CSAN_ATOMIC_FUNC_SWAP(32, uint32_t)
#if !defined(__aarch64__)
CSAN_ATOMIC_FUNC_TESTANDCLEAR(32, uint32_t)
CSAN_ATOMIC_FUNC_TESTANDSET(32, uint32_t)
#endif

CSAN_ATOMIC_FUNC_ADD(64, uint64_t)
CSAN_ATOMIC_FUNC_CLEAR(64, uint64_t)
CSAN_ATOMIC_FUNC_CMPSET(64, uint64_t)
CSAN_ATOMIC_FUNC_FCMPSET(64, uint64_t)
CSAN_ATOMIC_FUNC_FETCHADD(64, uint64_t)
CSAN_ATOMIC_FUNC_LOAD(64, uint64_t)
CSAN_ATOMIC_FUNC_READANDCLEAR(64, uint64_t)
CSAN_ATOMIC_FUNC_SET(64, uint64_t)
CSAN_ATOMIC_FUNC_SUBTRACT(64, uint64_t)
CSAN_ATOMIC_FUNC_STORE(64, uint64_t)
CSAN_ATOMIC_FUNC_SWAP(64, uint64_t)
#if !defined(__aarch64__)
CSAN_ATOMIC_FUNC_TESTANDCLEAR(64, uint64_t)
CSAN_ATOMIC_FUNC_TESTANDSET(64, uint64_t)
#endif

CSAN_ATOMIC_FUNC_ADD(char, uint8_t)
CSAN_ATOMIC_FUNC_CLEAR(char, uint8_t)
CSAN_ATOMIC_FUNC_CMPSET(char, uint8_t)
CSAN_ATOMIC_FUNC_FCMPSET(char, uint8_t)
CSAN_ATOMIC_FUNC_LOAD(char, uint8_t)
CSAN_ATOMIC_FUNC_SET(char, uint8_t)
CSAN_ATOMIC_FUNC_SUBTRACT(char, uint8_t)
_CSAN_ATOMIC_FUNC_STORE(char, uint8_t)
#if 0
CSAN_ATOMIC_FUNC_FETCHADD(char, uint8_t)
CSAN_ATOMIC_FUNC_READANDCLEAR(char, uint8_t)
CSAN_ATOMIC_FUNC_SWAP(char, uint8_t)
CSAN_ATOMIC_FUNC_TESTANDCLEAR(char, uint8_t)
CSAN_ATOMIC_FUNC_TESTANDSET(char, uint8_t)
#endif

CSAN_ATOMIC_FUNC_ADD(short, uint16_t)
CSAN_ATOMIC_FUNC_CLEAR(short, uint16_t)
CSAN_ATOMIC_FUNC_CMPSET(short, uint16_t)
CSAN_ATOMIC_FUNC_FCMPSET(short, uint16_t)
CSAN_ATOMIC_FUNC_LOAD(short, uint16_t)
CSAN_ATOMIC_FUNC_SET(short, uint16_t)
CSAN_ATOMIC_FUNC_SUBTRACT(short, uint16_t)
_CSAN_ATOMIC_FUNC_STORE(short, uint16_t)
#if 0
CSAN_ATOMIC_FUNC_FETCHADD(short, uint16_t)
CSAN_ATOMIC_FUNC_READANDCLEAR(short, uint16_t)
CSAN_ATOMIC_FUNC_SWAP(short, uint16_t)
CSAN_ATOMIC_FUNC_TESTANDCLEAR(short, uint16_t)
CSAN_ATOMIC_FUNC_TESTANDSET(short, uint16_t)
#endif

CSAN_ATOMIC_FUNC_ADD(int, u_int)
CSAN_ATOMIC_FUNC_CLEAR(int, u_int)
CSAN_ATOMIC_FUNC_CMPSET(int, u_int)
CSAN_ATOMIC_FUNC_FCMPSET(int, u_int)
CSAN_ATOMIC_FUNC_FETCHADD(int, u_int)
CSAN_ATOMIC_FUNC_LOAD(int, u_int)
CSAN_ATOMIC_FUNC_READANDCLEAR(int, u_int)
CSAN_ATOMIC_FUNC_SET(int, u_int)
CSAN_ATOMIC_FUNC_SUBTRACT(int, u_int)
CSAN_ATOMIC_FUNC_STORE(int, u_int)
CSAN_ATOMIC_FUNC_SWAP(int, u_int)
#if !defined(__aarch64__)
CSAN_ATOMIC_FUNC_TESTANDCLEAR(int, u_int)
CSAN_ATOMIC_FUNC_TESTANDSET(int, u_int)
#endif

CSAN_ATOMIC_FUNC_ADD(long, u_long)
CSAN_ATOMIC_FUNC_CLEAR(long, u_long)
CSAN_ATOMIC_FUNC_CMPSET(long, u_long)
CSAN_ATOMIC_FUNC_FCMPSET(long, u_long)
CSAN_ATOMIC_FUNC_FETCHADD(long, u_long)
CSAN_ATOMIC_FUNC_LOAD(long, u_long)
CSAN_ATOMIC_FUNC_READANDCLEAR(long, u_long)
CSAN_ATOMIC_FUNC_SET(long, u_long)
CSAN_ATOMIC_FUNC_SUBTRACT(long, u_long)
CSAN_ATOMIC_FUNC_STORE(long, u_long)
CSAN_ATOMIC_FUNC_SWAP(long, u_long)
#if !defined(__aarch64__)
CSAN_ATOMIC_FUNC_TESTANDCLEAR(long, u_long)
CSAN_ATOMIC_FUNC_TESTANDSET(long, u_long)
CSAN_ATOMIC_FUNC_TESTANDSET(acq_long, u_long)
#endif

CSAN_ATOMIC_FUNC_ADD(ptr, uintptr_t)
CSAN_ATOMIC_FUNC_CLEAR(ptr, uintptr_t)
CSAN_ATOMIC_FUNC_CMPSET(ptr, uintptr_t)
CSAN_ATOMIC_FUNC_FCMPSET(ptr, uintptr_t)
#if !defined(__amd64__)
CSAN_ATOMIC_FUNC_FETCHADD(ptr, uintptr_t)
#endif
CSAN_ATOMIC_FUNC_LOAD(ptr, uintptr_t)
CSAN_ATOMIC_FUNC_READANDCLEAR(ptr, uintptr_t)
CSAN_ATOMIC_FUNC_SET(ptr, uintptr_t)
CSAN_ATOMIC_FUNC_SUBTRACT(ptr, uintptr_t)
CSAN_ATOMIC_FUNC_STORE(ptr, uintptr_t)
CSAN_ATOMIC_FUNC_SWAP(ptr, uintptr_t)
#if 0
CSAN_ATOMIC_FUNC_TESTANDCLEAR(ptr, uintptr_t)
CSAN_ATOMIC_FUNC_TESTANDSET(ptr, uintptr_t)
#endif

#define	CSAN_ATOMIC_FUNC_THREAD_FENCE(name)				\
	void kcsan_atomic_thread_fence_##name(void)			\
	{								\
		atomic_thread_fence_##name();				\
	}

CSAN_ATOMIC_FUNC_THREAD_FENCE(acq)
CSAN_ATOMIC_FUNC_THREAD_FENCE(acq_rel)
CSAN_ATOMIC_FUNC_THREAD_FENCE(rel)
CSAN_ATOMIC_FUNC_THREAD_FENCE(seq_cst)

/* -------------------------------------------------------------------------- */

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/_cscan_bus.h>

int
kcsan_bus_space_map(bus_space_tag_t tag, bus_addr_t hnd, bus_size_t size,
    int flags, bus_space_handle_t *handlep)
{

	return (bus_space_map(tag, hnd, size, flags, handlep));
}

void
kcsan_bus_space_unmap(bus_space_tag_t tag, bus_space_handle_t hnd,
    bus_size_t size)
{

	bus_space_unmap(tag, hnd, size);
}

int
kcsan_bus_space_subregion(bus_space_tag_t tag, bus_space_handle_t hnd,
    bus_size_t offset, bus_size_t size, bus_space_handle_t *handlep)
{

	return (bus_space_subregion(tag, hnd, offset, size, handlep));
}

#if !defined(__amd64__)
int
kcsan_bus_space_alloc(bus_space_tag_t tag, bus_addr_t reg_start,
    bus_addr_t reg_end, bus_size_t size, bus_size_t alignment,
    bus_size_t boundary, int flags, bus_addr_t *addrp,
    bus_space_handle_t *handlep)
{

	return (bus_space_alloc(tag, reg_start, reg_end, size, alignment,
	    boundary, flags, addrp, handlep));
}
#endif

void
kcsan_bus_space_free(bus_space_tag_t tag, bus_space_handle_t hnd,
    bus_size_t size)
{

	bus_space_free(tag, hnd, size);
}

void
kcsan_bus_space_barrier(bus_space_tag_t tag, bus_space_handle_t hnd,
    bus_size_t offset, bus_size_t size, int flags)
{

	bus_space_barrier(tag, hnd, offset, size, flags);
}

#define CSAN_BUS_READ_FUNC(func, width, type)				\
	type kcsan_bus_space_read##func##_##width(bus_space_tag_t tag,	\
	    bus_space_handle_t hnd, bus_size_t offset)			\
	{								\
		return (bus_space_read##func##_##width(tag, hnd,	\
		    offset));						\
	}								\

#define CSAN_BUS_READ_PTR_FUNC(func, width, type)			\
	void kcsan_bus_space_read_##func##_##width(bus_space_tag_t tag,	\
	    bus_space_handle_t hnd, bus_size_t size, type *buf,		\
	    bus_size_t count)						\
	{								\
		kcsan_access((uintptr_t)buf, sizeof(type) * count,	\
		    false, false, __RET_ADDR);				\
		bus_space_read_##func##_##width(tag, hnd, size, buf, 	\
		    count);						\
	}

CSAN_BUS_READ_FUNC(, 1, uint8_t)
CSAN_BUS_READ_FUNC(_stream, 1, uint8_t)
CSAN_BUS_READ_PTR_FUNC(multi, 1, uint8_t)
CSAN_BUS_READ_PTR_FUNC(multi_stream, 1, uint8_t)
CSAN_BUS_READ_PTR_FUNC(region, 1, uint8_t)
CSAN_BUS_READ_PTR_FUNC(region_stream, 1, uint8_t)

CSAN_BUS_READ_FUNC(, 2, uint16_t)
CSAN_BUS_READ_FUNC(_stream, 2, uint16_t)
CSAN_BUS_READ_PTR_FUNC(multi, 2, uint16_t)
CSAN_BUS_READ_PTR_FUNC(multi_stream, 2, uint16_t)
CSAN_BUS_READ_PTR_FUNC(region, 2, uint16_t)
CSAN_BUS_READ_PTR_FUNC(region_stream, 2, uint16_t)

CSAN_BUS_READ_FUNC(, 4, uint32_t)
CSAN_BUS_READ_FUNC(_stream, 4, uint32_t)
CSAN_BUS_READ_PTR_FUNC(multi, 4, uint32_t)
CSAN_BUS_READ_PTR_FUNC(multi_stream, 4, uint32_t)
CSAN_BUS_READ_PTR_FUNC(region, 4, uint32_t)
CSAN_BUS_READ_PTR_FUNC(region_stream, 4, uint32_t)

CSAN_BUS_READ_FUNC(, 8, uint64_t)
#if defined(__aarch64__)
CSAN_BUS_READ_FUNC(_stream, 8, uint64_t)
CSAN_BUS_READ_PTR_FUNC(multi, 8, uint64_t)
CSAN_BUS_READ_PTR_FUNC(multi_stream, 8, uint64_t)
CSAN_BUS_READ_PTR_FUNC(region, 8, uint64_t)
CSAN_BUS_READ_PTR_FUNC(region_stream, 8, uint64_t)
#endif

#define CSAN_BUS_WRITE_FUNC(func, width, type)				\
	void kcsan_bus_space_write##func##_##width(bus_space_tag_t tag,		\
	    bus_space_handle_t hnd, bus_size_t offset, type value)	\
	{								\
		bus_space_write##func##_##width(tag, hnd, offset, value); \
	}								\

#define CSAN_BUS_WRITE_PTR_FUNC(func, width, type)			\
	void kcsan_bus_space_write_##func##_##width(bus_space_tag_t tag, \
	    bus_space_handle_t hnd, bus_size_t size, const type *buf,	\
	    bus_size_t count)						\
	{								\
		kcsan_access((uintptr_t)buf, sizeof(type) * count,	\
		    true, false, __RET_ADDR);				\
		bus_space_write_##func##_##width(tag, hnd, size, buf, 	\
		    count);						\
	}

CSAN_BUS_WRITE_FUNC(, 1, uint8_t)
CSAN_BUS_WRITE_FUNC(_stream, 1, uint8_t)
CSAN_BUS_WRITE_PTR_FUNC(multi, 1, uint8_t)
CSAN_BUS_WRITE_PTR_FUNC(multi_stream, 1, uint8_t)
CSAN_BUS_WRITE_PTR_FUNC(region, 1, uint8_t)
CSAN_BUS_WRITE_PTR_FUNC(region_stream, 1, uint8_t)

CSAN_BUS_WRITE_FUNC(, 2, uint16_t)
CSAN_BUS_WRITE_FUNC(_stream, 2, uint16_t)
CSAN_BUS_WRITE_PTR_FUNC(multi, 2, uint16_t)
CSAN_BUS_WRITE_PTR_FUNC(multi_stream, 2, uint16_t)
CSAN_BUS_WRITE_PTR_FUNC(region, 2, uint16_t)
CSAN_BUS_WRITE_PTR_FUNC(region_stream, 2, uint16_t)

CSAN_BUS_WRITE_FUNC(, 4, uint32_t)
CSAN_BUS_WRITE_FUNC(_stream, 4, uint32_t)
CSAN_BUS_WRITE_PTR_FUNC(multi, 4, uint32_t)
CSAN_BUS_WRITE_PTR_FUNC(multi_stream, 4, uint32_t)
CSAN_BUS_WRITE_PTR_FUNC(region, 4, uint32_t)
CSAN_BUS_WRITE_PTR_FUNC(region_stream, 4, uint32_t)

CSAN_BUS_WRITE_FUNC(, 8, uint64_t)
#if defined(__aarch64__)
CSAN_BUS_WRITE_FUNC(_stream, 8, uint64_t)
CSAN_BUS_WRITE_PTR_FUNC(multi, 8, uint64_t)
CSAN_BUS_WRITE_PTR_FUNC(multi_stream, 8, uint64_t)
CSAN_BUS_WRITE_PTR_FUNC(region, 8, uint64_t)
CSAN_BUS_WRITE_PTR_FUNC(region_stream, 8, uint64_t)
#endif

#define CSAN_BUS_SET_FUNC(func, width, type)				\
	void kcsan_bus_space_set_##func##_##width(bus_space_tag_t tag,	\
	    bus_space_handle_t hnd, bus_size_t offset, type value,	\
	    bus_size_t count)						\
	{								\
		bus_space_set_##func##_##width(tag, hnd, offset, value,	\
		    count);						\
	}

CSAN_BUS_SET_FUNC(multi, 1, uint8_t)
CSAN_BUS_SET_FUNC(region, 1, uint8_t)
#if !defined(__aarch64__)
CSAN_BUS_SET_FUNC(multi_stream, 1, uint8_t)
CSAN_BUS_SET_FUNC(region_stream, 1, uint8_t)
#endif

CSAN_BUS_SET_FUNC(multi, 2, uint16_t)
CSAN_BUS_SET_FUNC(region, 2, uint16_t)
#if !defined(__aarch64__)
CSAN_BUS_SET_FUNC(multi_stream, 2, uint16_t)
CSAN_BUS_SET_FUNC(region_stream, 2, uint16_t)
#endif

CSAN_BUS_SET_FUNC(multi, 4, uint32_t)
CSAN_BUS_SET_FUNC(region, 4, uint32_t)
#if !defined(__aarch64__)
CSAN_BUS_SET_FUNC(multi_stream, 4, uint32_t)
CSAN_BUS_SET_FUNC(region_stream, 4, uint32_t)
#endif

#if !defined(__amd64__)
CSAN_BUS_SET_FUNC(multi, 8, uint64_t)
CSAN_BUS_SET_FUNC(region, 8, uint64_t)
#if !defined(__aarch64__)
CSAN_BUS_SET_FUNC(multi_stream, 8, uint64_t)
CSAN_BUS_SET_FUNC(region_stream, 8, uint64_t)
#endif
#endif

